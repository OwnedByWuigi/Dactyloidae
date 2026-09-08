/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DirectShowReader.h"
#include "MediaDecoderReader.h"
#include "mozilla/RefPtr.h"
#include "DirectShowUtils.h"
#include "AudioSinkFilter.h"
#include "SourceFilter.h"
#include "SampleSink.h"
#include "VideoUtils.h"

using namespace mozilla::media;

namespace mozilla {

// Windows XP's MP3 decoder filter. This is available on XP only, on Vista
// and later we can use the DMO Wrapper filter and MP3 decoder DMO.
const GUID DirectShowReader::CLSID_MPEG_LAYER_3_DECODER_FILTER =
{ 0x38BE3000, 0xDBF4, 0x11D0, {0x86, 0x0E, 0x00, 0xA0, 0x24, 0xCF, 0xEF, 0x6D} };


static LazyLogModule gDirectShowLog("DirectShowDecoder");
#define LOG(...) MOZ_LOG(gDirectShowLog, mozilla::LogLevel::Debug, (__VA_ARGS__))

DirectShowReader::DirectShowReader(AbstractMediaDecoder* aDecoder)
  : MediaDecoderReader(aDecoder),
    mMP3FrameParser(aDecoder->GetResource()->GetLength()),
#ifdef DIRECTSHOW_REGISTER_GRAPH
    mRotRegister(0),
#endif
    mNumChannels(0),
    mAudioRate(0),
    mBytesPerSample(0),
    mIsH264(aDecoder->GetResource()->GetContentType().EqualsLiteral("video/h264") ||
            aDecoder->GetResource()->GetContentType().EqualsLiteral("video/avc"))
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_CTOR(DirectShowReader);
}

DirectShowReader::~DirectShowReader()
{
  MOZ_ASSERT(NS_IsMainThread(), "Must be on main thread.");
  MOZ_COUNT_DTOR(DirectShowReader);
#ifdef DIRECTSHOW_REGISTER_GRAPH
  if (mRotRegister) {
    RemoveGraphFromRunningObjectTable(mRotRegister);
  }
#endif
}

// Try to parse the MP3 stream to make sure this is indeed an MP3, get the
// estimated duration of the stream, and find the offset of the actual MP3
// frames in the stream, as DirectShow doesn't like large ID3 sections.
static nsresult
ParseMP3Headers(MP3FrameParser *aParser, MediaResource *aResource)
{
  const uint32_t MAX_READ_SIZE = 4096;

  uint64_t offset = 0;
  while (aParser->NeedsData() && !aParser->ParsedHeaders()) {
    uint32_t bytesRead;
    char buffer[MAX_READ_SIZE];
    nsresult rv = aResource->ReadAt(offset, buffer,
                                    MAX_READ_SIZE, &bytesRead);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!bytesRead) {
      // End of stream.
      return NS_ERROR_FAILURE;
    }

    aParser->Parse(reinterpret_cast<uint8_t*>(buffer), bytesRead, offset);
    offset += bytesRead;
  }

  return aParser->IsMP3() ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
DirectShowReader::ReadMetadata(MediaInfo* aInfo,
                               MetadataTags** aTags)
{
  MOZ_ASSERT(OnTaskQueue());
  HRESULT hr;
  nsresult rv;

  // Create the filter graph, reference it by the GraphBuilder interface,
  // to make graph building more convenient.
  hr = CoCreateInstance(CLSID_FilterGraph,
                        nullptr,
                        CLSCTX_INPROC_SERVER,
                        IID_IGraphBuilder,
                        reinterpret_cast<void**>(static_cast<IGraphBuilder**>(getter_AddRefs(mGraph))));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mGraph, NS_ERROR_FAILURE);

#ifdef DIRECTSHOW_REGISTER_GRAPH
  hr = AddGraphToRunningObjectTable(mGraph, &mRotRegister);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  #endif

  // Extract the interface pointers we'll need from the filter graph.
  hr = mGraph->QueryInterface(static_cast<IMediaControl**>(getter_AddRefs(mControl)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mControl, NS_ERROR_FAILURE);

  hr = mGraph->QueryInterface(static_cast<IMediaSeeking**>(getter_AddRefs(mMediaSeeking)));
  NS_ENSURE_TRUE(SUCCEEDED(hr) && mMediaSeeking, NS_ERROR_FAILURE);

  if (mIsH264) {
    return ReadH264Metadata(aInfo, aTags);
  }

  rv = ParseMP3Headers(&mMP3FrameParser, mDecoder->GetResource());
  NS_ENSURE_SUCCESS(rv, rv);

  // Build the graph. Create the filters we need, and connect them. We
  // build the entire graph ourselves to prevent other decoders installed
  // on the system being created and used.

  // Our source filters, wraps the MediaResource.
  mSourceFilter = new SourceFilter(MEDIATYPE_Stream, MEDIASUBTYPE_MPEG1Audio);
  NS_ENSURE_TRUE(mSourceFilter, NS_ERROR_FAILURE);

  rv = mSourceFilter->Init(mDecoder->GetResource(), mMP3FrameParser.GetMP3Offset());
  NS_ENSURE_SUCCESS(rv, rv);

  hr = mGraph->AddFilter(mSourceFilter, L"MozillaDirectShowSource");
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // The MPEG demuxer.
  RefPtr<IBaseFilter> demuxer;
  hr = CreateAndAddFilter(mGraph,
                          CLSID_MPEG1Splitter,
                          L"MPEG1Splitter",
                          getter_AddRefs(demuxer));
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // Platform MP3 decoder.
  RefPtr<IBaseFilter> decoder;
  // Firstly try to create the MP3 decoder filter that ships with WinXP
  // directly. This filter doesn't normally exist on later versions of
  // Windows.
  hr = CreateAndAddFilter(mGraph,
                          CLSID_MPEG_LAYER_3_DECODER_FILTER,
                          L"MPEG Layer 3 Decoder",
                          getter_AddRefs(decoder));
  if (FAILED(hr)) {
    // Failed to create MP3 decoder filter. Try to instantiate
    // the MP3 decoder DMO.
    hr = AddMP3DMOWrapperFilter(mGraph, getter_AddRefs(decoder));
    NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  }

  // Sink, captures audio samples and inserts them into our pipeline.
  static const wchar_t* AudioSinkFilterName = L"MozAudioSinkFilter";
  mAudioSinkFilter = new AudioSinkFilter(AudioSinkFilterName, &hr);
  NS_ENSURE_TRUE(mAudioSinkFilter && SUCCEEDED(hr), NS_ERROR_FAILURE);
  hr = mGraph->AddFilter(mAudioSinkFilter, AudioSinkFilterName);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  // Join the filters.
  hr = ConnectFilters(mGraph, mSourceFilter, demuxer);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConnectFilters(mGraph, demuxer, decoder);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = ConnectFilters(mGraph, decoder, mAudioSinkFilter);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  WAVEFORMATEX format;
  mAudioSinkFilter->GetSampleSink()->GetAudioFormat(&format);
  NS_ENSURE_TRUE(format.wFormatTag == WAVE_FORMAT_PCM, NS_ERROR_FAILURE);

  mInfo.mAudio.mChannels = mNumChannels = format.nChannels;
  mInfo.mAudio.mRate = mAudioRate = format.nSamplesPerSec;
  mInfo.mAudio.mBitDepth = format.wBitsPerSample;
  mBytesPerSample = format.wBitsPerSample / 8;

  // Begin decoding!
  hr = mControl->Run();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  DWORD seekCaps = 0;
  hr = mMediaSeeking->GetCapabilities(&seekCaps);
  mInfo.mMediaSeekable = SUCCEEDED(hr) && (AM_SEEKING_CanSeekAbsolute & seekCaps);

  int64_t duration = mMP3FrameParser.GetDuration();
  if (SUCCEEDED(hr)) {
    mInfo.mMetadataDuration.emplace(TimeUnit::FromMicroseconds(duration));
  }

  LOG("Successfully initialized DirectShow MP3 decoder.");
  LOG("Channels=%u Hz=%u duration=%lld bytesPerSample=%d",
      mInfo.mAudio.mChannels,
      mInfo.mAudio.mRate,
      RefTimeToUsecs(duration),
      mBytesPerSample);

  *aInfo = mInfo;
  // Note: The SourceFilter strips ID3v2 tags out of the stream.
  *aTags = nullptr;

  return NS_OK;
}

nsresult
DirectShowReader::ReadH264Metadata(MediaInfo* aInfo, MetadataTags** aTags)
{
  HRESULT hr;
  mSourceFilter = new SourceFilter(MEDIATYPE_Stream, MEDIASUBTYPE_H264);
  NS_ENSURE_TRUE(mSourceFilter, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = mSourceFilter->Init(mDecoder->GetResource(), 0);
  NS_ENSURE_SUCCESS(rv, rv);
  hr = mGraph->AddFilter(mSourceFilter, L"Mozilla H.264 source");
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  RefPtr<IBaseFilter> decoder;
  hr = AddH264DecoderFilter(mGraph, getter_AddRefs(decoder));
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  mAudioSinkFilter = new AudioSinkFilter(L"Mozilla H.264 video sink", &hr);
  NS_ENSURE_TRUE(mAudioSinkFilter && SUCCEEDED(hr), NS_ERROR_FAILURE);
  hr = mGraph->AddFilter(mAudioSinkFilter, L"Mozilla H.264 video sink");
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  hr = ConnectFilters(mGraph, mSourceFilter, decoder);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  hr = ConnectFilters(mGraph, decoder, mAudioSinkFilter);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  const VIDEOINFOHEADER* vih = mAudioSinkFilter->GetVideoInfo();
  int32_t width = abs(vih->bmiHeader.biWidth);
  int32_t height = abs(vih->bmiHeader.biHeight);
  NS_ENSURE_TRUE(width > 0 && height > 0, NS_ERROR_FAILURE);
  mInfo.mVideo = VideoInfo(width, height);
  mInfo.mVideo.mMimeType = "video/h264";
  hr = mControl->Run();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);
  *aInfo = mInfo;
  *aTags = nullptr;
  return NS_OK;
}

inline float
UnsignedByteToAudioSample(uint8_t aValue)
{
  return aValue * (2.0f / UINT8_MAX) - 1.0f;
}

bool
DirectShowReader::Finish(HRESULT aStatus)
{
  MOZ_ASSERT(OnTaskQueue());

  LOG("DirectShowReader::Finish(0x%x)", aStatus);
  // Notify the filter graph of end of stream.
  RefPtr<IMediaEventSink> eventSink;
  HRESULT hr = mGraph->QueryInterface(static_cast<IMediaEventSink**>(getter_AddRefs(eventSink)));
  if (SUCCEEDED(hr) && eventSink) {
    eventSink->Notify(EC_COMPLETE, aStatus, 0);
  }
  return false;
}

class DirectShowCopy
{
public:
  DirectShowCopy(uint8_t *aSource, uint32_t aBytesPerSample,
                 uint32_t aSamples, uint32_t aChannels)
    : mSource(aSource)
    , mBytesPerSample(aBytesPerSample)
    , mSamples(aSamples)
    , mChannels(aChannels)
    , mNextSample(0)
  { }

  uint32_t operator()(AudioDataValue *aBuffer, uint32_t aSamples)
  {
    uint32_t maxSamples = std::min(aSamples, mSamples - mNextSample);
    uint32_t frames = maxSamples / mChannels;
    size_t byteOffset = mNextSample * mBytesPerSample;
    if (mBytesPerSample == 1) {
      for (uint32_t i = 0; i < maxSamples; ++i) {
        uint8_t *sample = mSource + byteOffset;
        aBuffer[i] = UnsignedByteToAudioSample(*sample);
        byteOffset += mBytesPerSample;
      }
    } else if (mBytesPerSample == 2) {
      for (uint32_t i = 0; i < maxSamples; ++i) {
        int16_t *sample = reinterpret_cast<int16_t *>(mSource + byteOffset);
        aBuffer[i] = AudioSampleToFloat(*sample);
        byteOffset += mBytesPerSample;
      }
    }
    mNextSample += maxSamples;
    return frames;
  }

private:
  uint8_t * const mSource;
  const uint32_t mBytesPerSample;
  const uint32_t mSamples;
  const uint32_t mChannels;
  uint32_t mNextSample;
};

bool
DirectShowReader::DecodeAudioData()
{
  MOZ_ASSERT(OnTaskQueue());
  HRESULT hr;

  SampleSink* sink = mAudioSinkFilter->GetSampleSink();
  if (sink->AtEOS()) {
    // End of stream.
    return Finish(S_OK);
  }

  // Get the next chunk of audio samples. This blocks until the sample
  // arrives, or an error occurs (like the stream is shutdown).
  RefPtr<IMediaSample> sample;
  hr = sink->Extract(sample);
  if (FAILED(hr) || hr == S_FALSE) {
    return Finish(hr);
  }

  int64_t start = 0, end = 0;
  sample->GetMediaTime(&start, &end);
  LOG("DirectShowReader::DecodeAudioData [%4.2lf-%4.2lf]",
      RefTimeToSeconds(start),
      RefTimeToSeconds(end));

  LONG length = sample->GetActualDataLength();
  LONG numSamples = length / mBytesPerSample;
  LONG numFrames = length / mBytesPerSample / mNumChannels;

  BYTE* data = nullptr;
  hr = sample->GetPointer(&data);
  NS_ENSURE_TRUE(SUCCEEDED(hr), Finish(hr));

  mAudioCompactor.Push(mDecoder->GetResource()->Tell(),
                       RefTimeToUsecs(start),
                       mInfo.mAudio.mRate,
                       numFrames,
                       mNumChannels,
                       DirectShowCopy(reinterpret_cast<uint8_t *>(data),
                                      mBytesPerSample,
                                      numSamples,
                                      mNumChannels));
  return true;
}

bool
DirectShowReader::DecodeVideoFrame(bool &aKeyframeSkip,
                                   int64_t aTimeThreshold)
{
  MOZ_ASSERT(OnTaskQueue());
  if (!mIsH264) {
    return false;
  }
  RefPtr<IMediaSample> sample;
  HRESULT hr = mAudioSinkFilter->GetSampleSink()->Extract(sample);
  if (FAILED(hr) || hr == S_FALSE) {
    return Finish(hr);
  }
  REFERENCE_TIME start = 0, end = 0;
  sample->GetMediaTime(&start, &end);
  BYTE* data = nullptr;
  NS_ENSURE_TRUE(SUCCEEDED(sample->GetPointer(&data)), Finish(E_FAIL));
  LONG length = sample->GetActualDataLength();
  int32_t width = mInfo.mVideo.mImage.width;
  int32_t height = mInfo.mVideo.mImage.height;
  int32_t stride = width * 2;
  NS_ENSURE_TRUE(length >= stride * height, Finish(E_FAIL));
  nsTArray<uint8_t> y;
  nsTArray<uint8_t> u;
  nsTArray<uint8_t> v;
  NS_ENSURE_TRUE(y.SetLength(width * height) &&
                 u.SetLength((width / 2) * (height / 2)) &&
                 v.SetLength((width / 2) * (height / 2)),
                 Finish(E_OUTOFMEMORY));
  for (int32_t row = 0; row < height; ++row) {
    const uint8_t* src = data + row * stride;
    for (int32_t x = 0; x < width; x += 2) {
      int32_t p = row * width + x;
      y[p] = src[0]; y[p + 1] = src[2];
      if (!(row & 1)) {
        u[(row / 2) * (width / 2) + x / 2] = src[1];
        v[(row / 2) * (width / 2) + x / 2] = src[3];
      }
      src += 4;
    }
  }
  VideoData::YCbCrBuffer buffer;
  buffer.mPlanes[0] = { y.Elements(), uint32_t(width), uint32_t(height), uint32_t(width), 0, 1 };
  buffer.mPlanes[1] = { u.Elements(), uint32_t(width / 2), uint32_t(height / 2), uint32_t(width / 2), 0, 1 };
  buffer.mPlanes[2] = { v.Elements(), uint32_t(width / 2), uint32_t(height / 2), uint32_t(width / 2), 0, 1 };
  RefPtr<VideoData> video = VideoData::CreateAndCopyData(
    mInfo.mVideo, mDecoder->GetImageContainer(), 0, RefTimeToUsecs(start),
    RefTimeToUsecs(end - start), buffer, true, -1,
    gfx::IntRect(0, 0, width, height));
  NS_ENSURE_TRUE(video, Finish(E_OUTOFMEMORY));
  VideoQueue().Push(video);
  return true;
}

RefPtr<MediaDecoderReader::SeekPromise>
DirectShowReader::Seek(SeekTarget aTarget, int64_t aEndTime)
{
  nsresult res = SeekInternal(aTarget.GetTime().ToMicroseconds());
  if (NS_FAILED(res)) {
    return SeekPromise::CreateAndReject(res, __func__);
  } else {
    return SeekPromise::CreateAndResolve(aTarget.GetTime(), __func__);
  }
}

nsresult
DirectShowReader::SeekInternal(int64_t aTargetUs)
{
  HRESULT hr;
  MOZ_ASSERT(OnTaskQueue());

  LOG("DirectShowReader::Seek() target=%lld", aTargetUs);

  hr = mControl->Pause();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  nsresult rv = ResetDecode();
  NS_ENSURE_SUCCESS(rv, rv);

  LONGLONG seekPosition = UsecsToRefTime(aTargetUs);
  hr = mMediaSeeking->SetPositions(&seekPosition,
                                   AM_SEEKING_AbsolutePositioning,
                                   nullptr,
                                   AM_SEEKING_NoPositioning);
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  hr = mControl->Run();
  NS_ENSURE_TRUE(SUCCEEDED(hr), NS_ERROR_FAILURE);

  return NS_OK;
}

} // namespace mozilla
