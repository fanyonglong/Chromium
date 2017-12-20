// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_palette_delegate.h"

namespace ash {

TestPaletteDelegate::TestPaletteDelegate() {}

TestPaletteDelegate::~TestPaletteDelegate() {}

std::unique_ptr<PaletteDelegate::EnableListenerSubscription>
TestPaletteDelegate::AddPaletteEnableListener(
    const EnableListener& on_state_changed) {
  return nullptr;
}

void TestPaletteDelegate::CreateNote() {
  ++create_note_count_;
}

bool TestPaletteDelegate::HasNoteApp() {
  ++has_note_app_count_;
  return has_note_app_;
}

void TestPaletteDelegate::SetPartialMagnifierState(bool enabled) {
  partial_magnifier_state_ = enabled;
}

void TestPaletteDelegate::SetStylusStateChangedCallback(
    const PaletteDelegate::OnStylusStateChangedCallback&
        on_stylus_state_changed) {}

bool TestPaletteDelegate::ShouldAutoOpenPalette() {
  return should_auto_open_palette_;
}

bool TestPaletteDelegate::ShouldShowPalette() {
  return should_show_palette_;
}

void TestPaletteDelegate::TakeScreenshot() {
  ++take_screenshot_count_;
}

void TestPaletteDelegate::TakePartialScreenshot(const base::Closure& done) {
  ++take_partial_screenshot_count_;
  partial_screenshot_done_ = done;
}

void TestPaletteDelegate::CancelPartialScreenshot() {}

}  // namespace ash
