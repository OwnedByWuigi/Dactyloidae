/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define COBJMACROS
#include "GpuRasterD3D11.h"

#include <limits.h>

static BOOL
IntegerEdge(double value)
{
  /* Ordered comparisons reject NaNs and infinities before integer conversion. */
  return value >= LONG_MIN && value <= LONG_MAX && value == (LONG)value;
}

BOOL
GpuRasterD3D11ClipRect(const GpuRasterRect* rect, const D3D11_RECT* clip,
                     LONG width, LONG height, D3D11_RECT* result)
{
  LONG left, top, right, bottom;
  if (!rect || !clip || !result || width <= 0 || height <= 0 ||
      !IntegerEdge(rect->left) || !IntegerEdge(rect->top) ||
      !IntegerEdge(rect->right) || !IntegerEdge(rect->bottom) ||
      rect->right < rect->left || rect->bottom < rect->top) {
    return FALSE;
  }

  left = (LONG)rect->left;
  top = (LONG)rect->top;
  right = (LONG)rect->right;
  bottom = (LONG)rect->bottom;
  if (left < clip->left) left = clip->left;
  if (top < clip->top) top = clip->top;
  if (right > clip->right) right = clip->right;
  if (bottom > clip->bottom) bottom = clip->bottom;
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right > width) right = width;
  if (bottom > height) bottom = height;

  if (right <= left || bottom <= top) {
    result->left = result->top = result->right = result->bottom = 0;
  } else {
    result->left = left;
    result->top = top;
    result->right = right;
    result->bottom = bottom;
  }
  return TRUE;
}

BOOL
GpuRasterD3D11FillRect(ID3D11DeviceContext1* context,
                     ID3D11RenderTargetView* target,
                     LONG width, LONG height,
                     const GpuRasterRect* rect, const D3D11_RECT* clip,
                     const float color[4])
{
  D3D11_RECT clipped;
  D3D11_RENDER_TARGET_VIEW_DESC desc;
  unsigned i;
  if (!context || !target || !color ||
      !GpuRasterD3D11ClipRect(rect, clip, width, height, &clipped)) {
    return FALSE;
  }
  for (i = 0; i < 4; ++i) {
    if (!(color[i] >= 0.0f && color[i] <= 1.0f)) {
      return FALSE;
    }
  }

  ID3D11RenderTargetView_GetDesc(target, &desc);
  if ((desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
       desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM) ||
      desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D ||
      desc.Texture2D.MipSlice != 0) {
    return FALSE;
  }
  if (clipped.right != clipped.left && clipped.bottom != clipped.top) {
    ID3D11DeviceContext1_ClearView(context, (ID3D11View*)target, color, &clipped, 1);
  }
  return TRUE;
}
