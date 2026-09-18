/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ColorLayerComposite.h"
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/gfx/Types.h"          // for Color
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#include "mozilla/layers/CompositorTypes.h"  // for DiagnosticFlags::COLOR
#include "mozilla/layers/Effects.h"     // for Effect, EffectChain, etc
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "gfxPrefs.h"                   // for gfxPrefs::WebRenderEnabled

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
ColorLayerComposite::RenderLayer(const IntRect& aClipRect)
{
  Rect rect(GetBounds());
  const Matrix4x4& transform = GetEffectiveTransform();

  RenderWithAllMasks(this, mCompositor, aClipRect,
                     [&](EffectChain& effectChain, const IntRect& clipRect) {
    GenEffectChain(effectChain);

    // Route simple 2D color primitives through the retained C display list.
    // Masked or 3D layers continue through the established effect path until
    // equivalent WebRender primitives are available for those cases.
    CompositorOGL* compositorOGL = mCompositor->AsCompositorOGL();
    if (gfxPrefs::WebRenderEnabled() && compositorOGL &&
        !effectChain.mSecondaryEffects[EffectTypes::MASK] &&
        GetEffectiveMixBlendMode() == CompositionOp::OP_OVER &&
        transform.Is2D() && !clipRect.IsEmpty()) {
      if (compositorOGL->DrawWebRenderRect(rect, GetColor(),
                                           GetEffectiveOpacity(), transform,
                                           clipRect)) {
        return;
      }
    }

    mCompositor->DrawQuad(rect, clipRect, effectChain, GetEffectiveOpacity(),
                          transform);
  });

  mCompositor->DrawDiagnostics(DiagnosticFlags::COLOR, rect, aClipRect,
                               transform);
}

void
ColorLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = new EffectSolidColor(GetColor());
}

} // namespace layers
} // namespace mozilla
