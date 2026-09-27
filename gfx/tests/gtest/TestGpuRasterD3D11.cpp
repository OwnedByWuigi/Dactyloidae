/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NOMINMAX
# define NOMINMAX
#endif
#include "d3d11/GpuRasterD3D11.h"

#include <stdint.h>
#include <stdio.h>
#include <limits>

#ifndef GPU_RASTER_STANDALONE
# include "gtest/gtest.h"
#endif

#define CHECK_GPU(condition) do { \
  if (!(condition)) { fprintf(stderr, "GPU rectangle test failed at line %d\n", __LINE__); \
    return false; } \
} while (0)

static bool TestGpuRectangleClipping()
{
  D3D11_RECT clip = { 2, 3, 12, 13 }, output;
  GpuRasterRect rect = { -4, -5, 20, 21 };
  CHECK_GPU(GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  CHECK_GPU(output.left == 2 && output.top == 3 && output.right == 10 && output.bottom == 10);
  rect = { 20, 20, 25, 25 };
  CHECK_GPU(GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  CHECK_GPU(output.left == output.right && output.top == output.bottom);
  rect = { 1.5, 2, 4, 5 };
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  rect.left = std::numeric_limits<double>::quiet_NaN();
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  rect.left = std::numeric_limits<double>::infinity();
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  rect.left = 2147483648.0;
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  rect = { 6, 2, 4, 5 };
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 10, 10, &output));
  CHECK_GPU(!GpuRasterD3D11ClipRect(nullptr, &clip, 10, 10, &output));
  CHECK_GPU(!GpuRasterD3D11ClipRect(&rect, &clip, 0, 10, &output));
  return true;
}

#ifdef GPU_RASTER_STANDALONE
struct GpuRasterTestDevice {
  ID3D11Device* device = nullptr;
  ID3D11DeviceContext* context = nullptr;
  ID3D11DeviceContext1* context1 = nullptr;
  ID3D11Texture2D* texture = nullptr;
  ID3D11Texture2D* readback = nullptr;
  ID3D11RenderTargetView* view = nullptr;

  ~GpuRasterTestDevice() {
    if (view) view->Release();
    if (readback) readback->Release();
    if (texture) texture->Release();
    if (context1) context1->Release();
    if (context) context->Release();
    if (device) device->Release();
  }
};

static bool TestGpuRectanglePixels(DXGI_FORMAT format)
{
  GpuRasterTestDevice gpu;
  CHECK_GPU(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                       0, nullptr, 0, D3D11_SDK_VERSION,
                                       &gpu.device, nullptr, &gpu.context)));
  CHECK_GPU(SUCCEEDED(gpu.context->QueryInterface(__uuidof(ID3D11DeviceContext1),
                                                  (void**)&gpu.context1)));
  D3D11_FEATURE_DATA_D3D11_OPTIONS options = {};
  CHECK_GPU(SUCCEEDED(gpu.device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS,
                                                       &options, sizeof(options))));
  CHECK_GPU(options.ClearView);
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = desc.Height = 16;
  desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
  desc.Format = format;
  desc.BindFlags = D3D11_BIND_RENDER_TARGET;
  CHECK_GPU(SUCCEEDED(gpu.device->CreateTexture2D(&desc, nullptr, &gpu.texture)));
  CHECK_GPU(SUCCEEDED(gpu.device->CreateRenderTargetView(gpu.texture, nullptr, &gpu.view)));
  desc.Usage = D3D11_USAGE_STAGING;
  desc.BindFlags = 0;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  CHECK_GPU(SUCCEEDED(gpu.device->CreateTexture2D(&desc, nullptr, &gpu.readback)));

  const float blue[4] = { 0, 0, 1, 1 }, red[4] = { 1, 0, 0, 1 }, clear[4] = { 0, 0, 0, 0 };
  gpu.context->ClearRenderTargetView(gpu.view, blue);
  GpuRasterRect rect = { -3, 2, 12, 20 };
  D3D11_RECT clip = { 3, 4, 10, 11 };
  // ClearView must use the explicit clip, not the context's scissor state.
  D3D11_RECT scissor = { 0, 0, 1, 1 };
  gpu.context->RSSetScissorRects(1, &scissor);
  gpu.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  CHECK_GPU(GpuRasterD3D11FillRect(gpu.context1, gpu.view, 16, 16, &rect, &clip, red));
  rect = { 5, 5, 7, 7 };
  clip = { 0, 0, 16, 16 };
  CHECK_GPU(GpuRasterD3D11FillRect(gpu.context1, gpu.view, 16, 16, &rect, &clip, clear));
  rect = { 50, 50, 60, 60 };
  CHECK_GPU(GpuRasterD3D11FillRect(gpu.context1, gpu.view, 16, 16, &rect, &clip, clear));
  rect = { 0.5, 0, 16, 16 };
  CHECK_GPU(!GpuRasterD3D11FillRect(gpu.context1, gpu.view, 16, 16, &rect, &clip, clear));
  CHECK_GPU(!GpuRasterD3D11FillRect(nullptr, gpu.view, 16, 16, &rect, &clip, clear));

  D3D11_PRIMITIVE_TOPOLOGY topology;
  gpu.context->IAGetPrimitiveTopology(&topology);
  CHECK_GPU(topology == D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
  UINT count = 1;
  gpu.context->RSGetScissorRects(&count, &scissor);
  CHECK_GPU(count == 1 && scissor.left == 0 && scissor.top == 0 &&
            scissor.right == 1 && scissor.bottom == 1);

  gpu.context->CopyResource(gpu.readback, gpu.texture);
  D3D11_MAPPED_SUBRESOURCE map;
  CHECK_GPU(SUCCEEDED(gpu.context->Map(gpu.readback, 0, D3D11_MAP_READ, 0, &map)));
  bool equal = true;
  for (unsigned y = 0; y < 16; ++y) {
    const uint32_t* row = reinterpret_cast<const uint32_t*>(
      static_cast<const uint8_t*>(map.pData) + y * map.RowPitch);
    for (unsigned x = 0; x < 16; ++x) {
      bool isRed = x >= 3 && x < 10 && y >= 4 && y < 11;
      uint32_t expected = (isRed == (format == DXGI_FORMAT_R8G8B8A8_UNORM))
                          ? 0xff0000ff : 0xffff0000;
      if (x >= 5 && x < 7 && y >= 5 && y < 7) expected = 0;
      equal = equal && row[x] == expected;
    }
  }
  gpu.context->Unmap(gpu.readback, 0);
  CHECK_GPU(equal);
  return true;
}

int main()
{
  return TestGpuRectangleClipping() &&
         TestGpuRectanglePixels(DXGI_FORMAT_R8G8B8A8_UNORM) &&
         TestGpuRectanglePixels(DXGI_FORMAT_B8G8R8A8_UNORM) ? 0 : 1;
}
#else
TEST(GpuRasterD3D11, ClipRect) { EXPECT_TRUE(TestGpuRectangleClipping()); }
// WARP/Context1 is not available on every supported Windows installation.
// Run the standalone pixel tests on Windows 8+ with a D3D11.1 runtime.
#endif

#undef CHECK_GPU
