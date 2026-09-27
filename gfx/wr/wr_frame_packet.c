#include "webrender.h"

#include <stdint.h>
#include <string.h>

static int
wr_packet_size_mul_overflow(size_t a, size_t b)
{
  return b != 0 && a > SIZE_MAX / b;
}

size_t
wr_frame_packet_size(const wr_frame* frame)
{
  size_t size;

  if (!frame || frame->quad_count > UINT32_MAX ||
      frame->batch_count > UINT32_MAX ||
      (frame->quad_count && !frame->quads) ||
      (frame->batch_count && !frame->batches))
    return 0;

  size = sizeof(wr_frame_packet_header);
  if (wr_packet_size_mul_overflow(frame->quad_count, sizeof(*frame->quads)) ||
      wr_packet_size_mul_overflow(frame->batch_count, sizeof(*frame->batches)))
    return 0;
  if (size > SIZE_MAX - frame->quad_count * sizeof(*frame->quads))
    return 0;
  size += frame->quad_count * sizeof(*frame->quads);
  if (size > SIZE_MAX - frame->batch_count * sizeof(*frame->batches))
    return 0;
  return size + frame->batch_count * sizeof(*frame->batches);
}

int
wr_frame_serialize(const wr_frame* frame, void* buffer, size_t buffer_size)
{
  wr_frame_packet_header header;
  size_t required = wr_frame_packet_size(frame);
  unsigned char* output = (unsigned char*)buffer;

  if (!required || !buffer || buffer_size < required)
    return 0;
  header.version = WR_FRAME_PACKET_VERSION;
  header.quad_count = (uint32_t)frame->quad_count;
  header.batch_count = (uint32_t)frame->batch_count;
  memcpy(output, &header, sizeof(header));
  output += sizeof(header);
  if (frame->quad_count) {
    memcpy(output, frame->quads,
           frame->quad_count * sizeof(*frame->quads));
    output += frame->quad_count * sizeof(*frame->quads);
  }
  if (frame->batch_count) {
    memcpy(output, frame->batches,
           frame->batch_count * sizeof(*frame->batches));
  }
  return 1;
}
