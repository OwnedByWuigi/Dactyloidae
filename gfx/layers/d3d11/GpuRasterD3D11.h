/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_GPU_RASTER_D3D11_H
#define GFX_GPU_RASTER_D3D11_H

#include <d3d11_1.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Device-space edges. Fractional or non-finite edges require normal drawing. */
typedef struct GpuRasterRect {
  double left, top, right, bottom;
} GpuRasterRect;

/* FALSE means unsupported input; TRUE may produce an empty clipped rectangle. */
BOOL GpuRasterD3D11ClipRect(const GpuRasterRect* rect, const D3D11_RECT* clip,
                          LONG width, LONG height, D3D11_RECT* result);

/* Source replacement, not alpha blending. The caller must check ClearView
 * device support and supply the dimensions of the render target. No pipeline
 * bindings are changed. FALSE leaves the target untouched for shader fallback. */
BOOL GpuRasterD3D11FillRect(ID3D11DeviceContext1* context,
                          ID3D11RenderTargetView* target,
                          LONG width, LONG height,
                          const GpuRasterRect* rect, const D3D11_RECT* clip,
                          const float color[4]);

#ifdef __cplusplus
}
#endif

#endif
