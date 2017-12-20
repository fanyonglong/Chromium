// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
#define ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_

#include "base/macros.h"
#include "cc/output/output_surface.h"

namespace android_webview {
class AwRenderThreadContextProvider;

class ParentOutputSurface : NON_EXPORTED_BASE(public cc::OutputSurface) {
 public:
  explicit ParentOutputSurface(
      scoped_refptr<AwRenderThreadContextProvider> context_provider);
  ~ParentOutputSurface() override;

  // OutputSurface overrides.
  void BindToClient(cc::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void BindFramebuffer() override;
  void Reshape(const gfx::Size& size,
               float scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  bool SurfaceIsSuspendForRecycle() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ParentOutputSurface);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_PARENT_OUTPUT_SURFACE_H_
