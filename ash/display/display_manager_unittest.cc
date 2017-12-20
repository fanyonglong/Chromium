// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager.h"

#include "ash/accelerators/accelerator_commands_aura.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_md_test_base.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/mirror_window_test_api.h"
#include "ash/wm/window_state_aura.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "grit/ash_strings.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font_render_params.h"

namespace ash {

using std::vector;
using std::string;

using base::StringPrintf;

namespace {

std::string ToDisplayName(int64_t id) {
  return "x-" + base::Int64ToString(id);
}

}  // namespace

class DisplayManagerTest : public test::AshMDTestBase,
                           public display::DisplayObserver,
                           public aura::WindowObserver {
 public:
  DisplayManagerTest()
      : removed_count_(0U),
        root_window_destroyed_(false),
        changed_metrics_(0U) {}
  ~DisplayManagerTest() override {}

  void SetUp() override {
    AshMDTestBase::SetUp();
    display::Screen::GetScreen()->AddObserver(this);
    Shell::GetPrimaryRootWindow()->AddObserver(this);
  }
  void TearDown() override {
    Shell::GetPrimaryRootWindow()->RemoveObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
    AshMDTestBase::TearDown();
  }

  const vector<display::Display>& changed() const { return changed_; }
  const vector<display::Display>& added() const { return added_; }
  uint32_t changed_metrics() const { return changed_metrics_; }

  string GetCountSummary() const {
    return StringPrintf("%" PRIuS " %" PRIuS " %" PRIuS, changed_.size(),
                        added_.size(), removed_count_);
  }

  void reset() {
    changed_.clear();
    added_.clear();
    removed_count_ = 0U;
    changed_metrics_ = 0U;
    root_window_destroyed_ = false;
  }

  bool root_window_destroyed() const { return root_window_destroyed_; }

  const display::ManagedDisplayInfo& GetDisplayInfo(
      const display::Display& display) {
    return display_manager()->GetDisplayInfo(display.id());
  }

  const display::ManagedDisplayInfo& GetDisplayInfoAt(int index) {
    return GetDisplayInfo(display_manager()->GetDisplayAt(index));
  }

  const display::Display& GetDisplayForId(int64_t id) {
    return display_manager()->GetDisplayForId(id);
  }

  const display::ManagedDisplayInfo& GetDisplayInfoForId(int64_t id) {
    return GetDisplayInfo(display_manager()->GetDisplayForId(id));
  }

  // aura::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    changed_.push_back(display);
    changed_metrics_ |= changed_metrics;
  }
  void OnDisplayAdded(const display::Display& new_display) override {
    added_.push_back(new_display);
  }
  void OnDisplayRemoved(const display::Display& old_display) override {
    ++removed_count_;
  }

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override {
    ASSERT_EQ(Shell::GetPrimaryRootWindow(), window);
    root_window_destroyed_ = true;
  }

 private:
  vector<display::Display> changed_;
  vector<display::Display> added_;
  size_t removed_count_;
  bool root_window_destroyed_;
  uint32_t changed_metrics_;

  DISALLOW_COPY_AND_ASSIGN(DisplayManagerTest);
};

INSTANTIATE_TEST_CASE_P(
    /* prefix intentionally left blank due to only one parameterization */,
    DisplayManagerTest,
    testing::Values(MaterialDesignController::NON_MATERIAL,
                    MaterialDesignController::MATERIAL_NORMAL,
                    MaterialDesignController::MATERIAL_EXPERIMENTAL));

TEST_P(DisplayManagerTest, UpdateDisplayTest) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Update primary and add seconary.
  UpdateDisplay("100+0-500x500,0+501-400x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            display_manager()->GetDisplayAt(0).bounds().ToString());

  EXPECT_EQ("2 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[1].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  EXPECT_EQ("500,0 400x400", changed()[0].bounds().ToString());
  EXPECT_EQ("0,0 500x500", changed()[1].bounds().ToString());
  // Secondary display is on right.
  EXPECT_EQ("500,0 400x400", added()[0].bounds().ToString());
  EXPECT_EQ("0,501 400x400",
            GetDisplayInfo(added()[0]).bounds_in_native().ToString());
  reset();

  // Delete secondary.
  UpdateDisplay("100+0-500x500");
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  // Change primary.
  UpdateDisplay("1+1-1000x600");
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ("0,0 1000x600", changed()[0].bounds().ToString());
  reset();

  // Add secondary.
  UpdateDisplay("1+1-1000x600,1002+0-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 1 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  // Secondary display is on right.
  EXPECT_EQ("1000,0 600x400", added()[0].bounds().ToString());
  EXPECT_EQ("1002,0 600x400",
            GetDisplayInfo(added()[0]).bounds_in_native().ToString());
  reset();

  // Secondary removed, primary changed.
  UpdateDisplay("1+1-800x300");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ("0,0 800x300", changed()[0].bounds().ToString());
  reset();

  // # of display can go to zero when screen is off.
  const vector<display::ManagedDisplayInfo> empty;
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  // Display configuration stays the same
  EXPECT_EQ("0,0 800x300",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  reset();

  // Connect to display again
  UpdateDisplay("100+100-500x400");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 0 0", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  EXPECT_EQ("0,0 500x400", changed()[0].bounds().ToString());
  EXPECT_EQ("100,100 500x400",
            GetDisplayInfo(changed()[0]).bounds_in_native().ToString());
  reset();

  // Go back to zero and wake up with multiple displays.
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(root_window_destroyed());
  reset();

  // Add secondary.
  UpdateDisplay("0+0-1000x600,1000+1000-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 1000x600",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  // Secondary display is on right.
  EXPECT_EQ("1000,0 600x400",
            display_manager()->GetDisplayAt(1).bounds().ToString());
  EXPECT_EQ("1000,1000 600x400",
            GetDisplayInfoAt(1).bounds_in_native().ToString());
  reset();

  // Changing primary will update secondary as well.
  UpdateDisplay("0+0-800x600,1000+1000-600x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();
  EXPECT_EQ("0,0 800x600",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ("800,0 600x400",
            display_manager()->GetDisplayAt(1).bounds().ToString());
}

TEST_P(DisplayManagerTest, ScaleOnlyChange) {
  if (!SupportsMultipleDisplays())
    return;
  display_manager()->ToggleDisplayScaleFactor();
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
}

// Test in emulation mode (use_fullscreen_host_window=false)
TEST_P(DisplayManagerTest, EmulatorTest) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  display_manager()->AddRemoveDisplay();
  // Update primary and add seconary.
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 1 0", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0 0 1", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1 1 0", GetCountSummary());
}

// Tests support for 3 displays.
TEST_P(DisplayManagerTest, UpdateThreeDisplaysWithDefaultLayout) {
  if (!SupportsMultipleDisplays())
    return;

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Test with three displays. Native origin will not affect ash
  // display layout.
  UpdateDisplay("0+0-640x480,1000+0-320x200,2000+0-400x300");

  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 640x480",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ("640,0 320x200",
            display_manager()->GetDisplayAt(1).bounds().ToString());
  EXPECT_EQ("960,0 400x300",
            display_manager()->GetDisplayAt(2).bounds().ToString());

  EXPECT_EQ("3 2 0", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(2).id(), changed()[1].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[2].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(2).id(), added()[1].id());
  EXPECT_EQ("640,0 320x200", changed()[0].bounds().ToString());
  EXPECT_EQ("960,0 400x300", changed()[1].bounds().ToString());
  EXPECT_EQ("0,0 640x480", changed()[2].bounds().ToString());
  // Secondary and terniary displays are on right.
  EXPECT_EQ("640,0 320x200", added()[0].bounds().ToString());
  EXPECT_EQ("1000,0 320x200",
            GetDisplayInfo(added()[0]).bounds_in_native().ToString());
  EXPECT_EQ("960,0 400x300", added()[1].bounds().ToString());
  EXPECT_EQ("2000,0 400x300",
            GetDisplayInfo(added()[1]).bounds_in_native().ToString());

  // Verify calling ReconfigureDisplays doesn't change anything.
  display_manager()->ReconfigureDisplays();
  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 640x480",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ("640,0 320x200",
            display_manager()->GetDisplayAt(1).bounds().ToString());
  EXPECT_EQ("960,0 400x300",
            display_manager()->GetDisplayAt(2).bounds().ToString());

  display::DisplayPlacement default_placement(display::DisplayPlacement::BOTTOM,
                                              10);
  display_manager()->layout_store()->SetDefaultDisplayPlacement(
      default_placement);

  // Test with new displays.
  UpdateDisplay("640x480");
  UpdateDisplay("640x480,320x200,400x300");

  EXPECT_EQ("0,0 640x480",
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ("10,480 320x200",
            display_manager()->GetDisplayAt(1).bounds().ToString());
  EXPECT_EQ("20,680 400x300",
            display_manager()->GetDisplayAt(2).bounds().ToString());
}

TEST_P(DisplayManagerTest, LayoutMorethanThreeDisplaysTest) {
  if (!SupportsMultipleDisplays())
    return;

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list = display::test::CreateDisplayIdListN(
      3, primary_id, primary_id + 1, primary_id + 2);
  {
    // Layout: [2]
    //         [1][P]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::LEFT, 10);
    builder.AddDisplayPlacement(list[2], list[1],
                                display::DisplayPlacement::TOP, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ("0,0 640x480",
              display_manager()->GetDisplayAt(0).bounds().ToString());
    EXPECT_EQ("-320,10 320x200",
              display_manager()->GetDisplayAt(1).bounds().ToString());
    EXPECT_EQ("-310,-290 400x300",
              display_manager()->GetDisplayAt(2).bounds().ToString());
  }
  {
    // Layout: [1]
    //         [P][2]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::TOP, 10);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::RIGHT, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ("0,0 640x480",
              display_manager()->GetDisplayAt(0).bounds().ToString());
    EXPECT_EQ("10,-200 320x200",
              display_manager()->GetDisplayAt(1).bounds().ToString());
    EXPECT_EQ("640,10 400x300",
              display_manager()->GetDisplayAt(2).bounds().ToString());
  }
  {
    // Layout: [P]
    //         [2]
    //         [1]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], list[2],
                                display::DisplayPlacement::BOTTOM, 10);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::BOTTOM, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ("0,0 640x480",
              display_manager()->GetDisplayAt(0).bounds().ToString());
    EXPECT_EQ("20,780 320x200",
              display_manager()->GetDisplayAt(1).bounds().ToString());
    EXPECT_EQ("10,480 400x300",
              display_manager()->GetDisplayAt(2).bounds().ToString());
  }

  {
    list = display::test::CreateDisplayIdListN(5, primary_id, primary_id + 1,
                                               primary_id + 2, primary_id + 3,
                                               primary_id + 4);
    // Layout: [P][2]
    //      [3][4]
    //      [1]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::RIGHT, 10);
    builder.AddDisplayPlacement(list[1], list[3],
                                display::DisplayPlacement::BOTTOM, 10);
    builder.AddDisplayPlacement(list[3], list[4],
                                display::DisplayPlacement::LEFT, 10);
    builder.AddDisplayPlacement(list[4], primary_id,
                                display::DisplayPlacement::BOTTOM, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300,300x200,200x100");

    EXPECT_EQ(5U, display_manager()->GetNumDisplays());

    EXPECT_EQ("0,0 640x480",
              display_manager()->GetDisplayAt(0).bounds().ToString());
    // 2nd is right of the primary.
    EXPECT_EQ("640,10 400x300",
              display_manager()->GetDisplayAt(2).bounds().ToString());
    // 4th is bottom of the primary.
    EXPECT_EQ("10,480 200x100",
              display_manager()->GetDisplayAt(4).bounds().ToString());
    // 3rd is the left of 4th.
    EXPECT_EQ("-290,480 300x200",
              display_manager()->GetDisplayAt(3).bounds().ToString());
    // 1st is the bottom of 3rd.
    EXPECT_EQ("-280,680 320x200",
              display_manager()->GetDisplayAt(1).bounds().ToString());
  }
}

TEST_P(DisplayManagerTest, NoMirrorInThreeDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("640x480,320x200,400x300");
  ash::Shell::GetInstance()->display_configuration_controller()->SetMirrorMode(
      true, true);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(3u, display_manager()->GetNumDisplays());
#if defined(OS_CHROMEOS)
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_DISPLAY_MIRRORING_NOT_SUPPORTED),
            GetDisplayErrorNotificationMessageForTest());
#endif
}

TEST_P(DisplayManagerTest, OverscanInsetsTest) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("0+0-500x500,0+501-400x400");
  reset();
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  const display::ManagedDisplayInfo& display_info1 = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo& display_info2 = GetDisplayInfoAt(1);
  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets(13, 12, 11, 10));

  std::vector<display::Display> changed_displays = changed();
  EXPECT_EQ(1u, changed_displays.size());
  EXPECT_EQ(display_info2.id(), changed_displays[0].id());
  EXPECT_EQ("0,0 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  display::ManagedDisplayInfo updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("378x376", updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("13,12,11,10",
            updated_display_info2.overscan_insets_in_dip().ToString());
  EXPECT_EQ("500,0 378x376",
            display_manager()->GetSecondaryDisplay().bounds().ToString());

  // Make sure that SetOverscanInsets() is idempotent.
  display_manager()->SetOverscanInsets(display_info1.id(), gfx::Insets());
  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets(13, 12, 11, 10));
  EXPECT_EQ("0,0 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("378x376", updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("13,12,11,10",
            updated_display_info2.overscan_insets_in_dip().ToString());

  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets(10, 11, 12, 13));
  EXPECT_EQ("0,0 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("376x378", GetDisplayInfoAt(1).size_in_pixel().ToString());
  EXPECT_EQ("10,11,12,13",
            GetDisplayInfoAt(1).overscan_insets_in_dip().ToString());

  // Recreate a new 2nd display. It won't apply the overscan inset because the
  // new display has a different ID.
  UpdateDisplay("0+0-500x500");
  UpdateDisplay("0+0-500x500,0+501-400x400");
  EXPECT_EQ("0,0 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("0,501 400x400", GetDisplayInfoAt(1).bounds_in_native().ToString());

  // Recreate the displays with the same ID.  It should apply the overscan
  // inset.
  UpdateDisplay("0+0-500x500");
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ("1,1 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("376x378", updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("10,11,12,13",
            updated_display_info2.overscan_insets_in_dip().ToString());

  // HiDPI but overscan display. The specified insets size should be doubled.
  UpdateDisplay("0+0-500x500,0+501-400x400*2");
  display_manager()->SetOverscanInsets(display_manager()->GetDisplayAt(1).id(),
                                       gfx::Insets(4, 5, 6, 7));
  EXPECT_EQ("0,0 500x500", GetDisplayInfoAt(0).bounds_in_native().ToString());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ("0,501 400x400",
            updated_display_info2.bounds_in_native().ToString());
  EXPECT_EQ("376x380", updated_display_info2.size_in_pixel().ToString());
  EXPECT_EQ("4,5,6,7",
            updated_display_info2.overscan_insets_in_dip().ToString());
  EXPECT_EQ("8,10,12,14",
            updated_display_info2.GetOverscanInsetsInPixel().ToString());

  // Make sure switching primary display applies the overscan offset only once.
  ash::Shell::GetInstance()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetSecondaryDisplay().id());
  EXPECT_EQ("-500,0 500x500",
            display_manager()->GetSecondaryDisplay().bounds().ToString());
  EXPECT_EQ("0,0 500x500",
            GetDisplayInfo(display_manager()->GetSecondaryDisplay())
                .bounds_in_native()
                .ToString());
  EXPECT_EQ("0,501 400x400",
            GetDisplayInfo(display::Screen::GetScreen()->GetPrimaryDisplay())
                .bounds_in_native()
                .ToString());
  EXPECT_EQ(
      "0,0 188x190",
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().ToString());

  // Make sure just moving the overscan area should property notify observers.
  UpdateDisplay("0+0-500x500");
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager()->SetOverscanInsets(primary_id, gfx::Insets(0, 0, 20, 20));
  EXPECT_EQ(
      "0,0 480x480",
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().ToString());
  reset();
  display_manager()->SetOverscanInsets(primary_id, gfx::Insets(10, 10, 10, 10));
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_EQ(
      "0,0 480x480",
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().ToString());
  reset();
  display_manager()->SetOverscanInsets(primary_id, gfx::Insets(0, 0, 0, 0));
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_EQ(
      "0,0 500x500",
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().ToString());
}

TEST_P(DisplayManagerTest, ZeroOverscanInsets) {
  if (!SupportsMultipleDisplays())
    return;

  // Make sure the display change events is emitted for overscan inset changes.
  UpdateDisplay("0+0-500x500,0+501-400x400");
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  int64_t display2_id = display_manager()->GetDisplayAt(1).id();

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(0, 0, 0, 0));
  EXPECT_EQ(0u, changed().size());

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(1, 0, 0, 0));
  EXPECT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets(0, 0, 0, 0));
  EXPECT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());
}

#if !defined(OS_WIN)
// Disabled on windows because of http://crbug.com/650326.
TEST_P(DisplayManagerTest, TestDeviceScaleOnlyChange) {
  UpdateDisplay("1000x600");
  aura::WindowTreeHost* host = Shell::GetPrimaryRootWindow()->GetHost();
  EXPECT_EQ(1, host->compositor()->device_scale_factor());
  EXPECT_EQ("1000x600",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  EXPECT_EQ("1 0 0", GetCountSummary());

  UpdateDisplay("1000x600*2");
  EXPECT_EQ(2, host->compositor()->device_scale_factor());
  EXPECT_EQ("2 0 0", GetCountSummary());
  EXPECT_EQ("500x300",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
}
#endif

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds) {
  display::ManagedDisplayInfo info(id, ToDisplayName(id), false);
  info.SetBounds(bounds);
  return info;
}

TEST_P(DisplayManagerTest, TestNativeDisplaysChanged) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const int external_id = 10;
  const int mirror_id = 11;
  const int64_t invalid_id = display::Display::kInvalidDisplayID;
  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 100, 100));
  const display::ManagedDisplayInfo mirroring_display_info =
      CreateDisplayInfo(mirror_id, gfx::Rect(0, 0, 500, 500));

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  std::string default_bounds =
      display_manager()->GetDisplayAt(0).bounds().ToString();

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Primary disconnected.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(default_bounds,
            display_manager()->GetDisplayAt(0).bounds().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  if (!SupportsMultipleDisplays())
    return;

  // External connected while primary was disconnected.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(external_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(external_id,
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  EXPECT_EQ(internal_display_id, display::Display::InternalDisplayId());

  // Primary connected, with different bounds.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(internal_display_id,
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // This combinatino is new, so internal display becomes primary.
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // Emulate suspend.
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // External display has disconnected then resumed.
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // External display was changed during suspend.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // suspend...
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // and resume with different external display.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(CreateDisplayInfo(12, gfx::Rect(1, 1, 100, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // mirrored...
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(mirroring_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(11U, display_manager()->mirroring_display_id());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Test display name.
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));
  EXPECT_EQ("x-10", display_manager()->GetDisplayNameForId(10));
  EXPECT_EQ("x-11", display_manager()->GetDisplayNameForId(11));
  EXPECT_EQ("x-12", display_manager()->GetDisplayNameForId(12));
  // Default name for the id that doesn't exist.
  EXPECT_EQ("Display 100", display_manager()->GetDisplayNameForId(100));

  // and exit mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("500,0 100x100", GetDisplayForId(10).bounds().ToString());

  // Turn off internal
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(external_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Switched to another display
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(
      "0,0 500x500",
      GetDisplayInfoForId(internal_display_id).bounds_in_native().ToString());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
}

// Make sure crash does not happen if add and remove happens at the same time.
// See: crbug.com/414394
TEST_P(DisplayManagerTest, DisplayAddRemoveAtTheSameTime) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100+0-500x500,0+501-400x400");

  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id = display_manager()->GetSecondaryDisplay().id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // An id which is different from primary and secondary.
  const int64_t third_id = secondary_id + 1;

  display::ManagedDisplayInfo third_info =
      CreateDisplayInfo(third_id, gfx::Rect(0, 0, 600, 600));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(third_info);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Secondary seconary_id becomes the primary as it has smaller output index.
  EXPECT_EQ(secondary_id, WindowTreeHostManager::GetPrimaryDisplayId());
  EXPECT_EQ(third_id, display_manager()->GetSecondaryDisplay().id());
  EXPECT_EQ("600x600", GetDisplayForId(third_id).size().ToString());
}

// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, TestNativeDisplaysChangedNoInternal) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Don't change the display info if all displays are disconnected.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Connect another display which will become primary.
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 100, 100));
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("1,1 100x100",
            GetDisplayInfoForId(10).bounds_in_native().ToString());
  EXPECT_EQ("100x100", ash::Shell::GetPrimaryRootWindow()
                           ->GetHost()
                           ->GetBounds()
                           .size()
                           .ToString());
}
#endif  // defined(OS_CHROMEOS)

TEST_P(DisplayManagerTest, NativeDisplaysChangedAfterPrimaryChange) {
  if (!SupportsMultipleDisplays())
    return;

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo secondary_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 100, 100));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_info_list.push_back(secondary_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("0,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("500,0 100x100", GetDisplayForId(10).bounds().ToString());

  ash::Shell::GetInstance()->window_tree_host_manager()->SetPrimaryDisplayId(
      secondary_display_info.id());
  EXPECT_EQ("-500,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("0,0 100x100", GetDisplayForId(10).bounds().ToString());

  // OnNativeDisplaysChanged may change the display bounds.  Here makes sure
  // nothing changed if the exactly same displays are specified.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ("-500,0 500x500",
            GetDisplayForId(internal_display_id).bounds().ToString());
  EXPECT_EQ("0,0 100x100", GetDisplayForId(10).bounds().ToString());
}

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, DontRememberBestResolution) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(1000, 500), 58.0f, false, true)));
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(800, 300), 59.0f, false, false)));
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(400, 500), 60.0f, false, false)));

  native_display_info.SetManagedDisplayModes(display_modes);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  scoped_refptr<display::ManagedDisplayMode> mode;
  scoped_refptr<display::ManagedDisplayMode> expected_mode(
      new display::ManagedDisplayMode(gfx::Size(1000, 500), 0.0f, false,
                                      false));

  mode = display_manager()->GetSelectedModeForDisplayId(display_id);
  EXPECT_FALSE(!!mode);
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Unsupported resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(800, 4000));
  mode = display_manager()->GetSelectedModeForDisplayId(display_id);
  EXPECT_FALSE(!!mode);
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Supported resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(800, 300));
  mode = display_manager()->GetSelectedModeForDisplayId(display_id);
  EXPECT_TRUE(!!mode);
  EXPECT_EQ("800x300", mode->size().ToString());
  EXPECT_EQ(59.0f, mode->refresh_rate());
  EXPECT_FALSE(mode->native());

  expected_mode =
      new display::ManagedDisplayMode(gfx::Size(800, 300), 0.0f, false, false);

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  // Best resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(1000, 500));
  mode = display_manager()->GetSelectedModeForDisplayId(display_id);
  EXPECT_TRUE(!!mode);
  EXPECT_EQ("1000x500", mode->size().ToString());
  EXPECT_EQ(58.0f, mode->refresh_rate());
  EXPECT_TRUE(mode->native());

  expected_mode =
      new display::ManagedDisplayMode(gfx::Size(1000, 500), 0.0f, false, false);

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
}
#endif  // defined(OS_CHROMEOS)

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, ResolutionFallback) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(1000, 500), 58.0f, false, true)));
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(800, 300), 59.0f, false, false)));
  display_modes.push_back(make_scoped_refptr(new display::ManagedDisplayMode(
      gfx::Size(400, 500), 60.0f, false, false)));

  display::ManagedDisplayInfo::ManagedDisplayModeList copy = display_modes;
  native_display_info.SetManagedDisplayModes(copy);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  {
    display::test::SetDisplayResolution(display_manager(), display_id,
                                        gfx::Size(800, 300));
    display::ManagedDisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 400, 500));
    copy = display_modes;
    new_native_display_info.SetManagedDisplayModes(copy);
    std::vector<display::ManagedDisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    scoped_refptr<display::ManagedDisplayMode> mode =
        display_manager()->GetSelectedModeForDisplayId(display_id);
    EXPECT_TRUE(!!mode);
    EXPECT_EQ("400x500", mode->size().ToString());
    EXPECT_EQ(60.0f, mode->refresh_rate());
    EXPECT_FALSE(mode->native());
  }
  {
    // Best resolution should find itself on the resolutions list.
    display::test::SetDisplayResolution(display_manager(), display_id,
                                        gfx::Size(800, 300));
    display::ManagedDisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
    display::ManagedDisplayInfo::ManagedDisplayModeList copy = display_modes;
    new_native_display_info.SetManagedDisplayModes(copy);
    std::vector<display::ManagedDisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    scoped_refptr<display::ManagedDisplayMode> mode =
        display_manager()->GetSelectedModeForDisplayId(display_id);
    EXPECT_TRUE(!!mode);
    EXPECT_EQ("1000x500", mode->size().ToString());
    EXPECT_EQ(58.0f, mode->refresh_rate());
    EXPECT_TRUE(mode->native());
  }
}
#endif  // defined(OS_CHROMEOS)

TEST_P(DisplayManagerTest, Rotate) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("100x200/r,300x400/l");
  EXPECT_EQ("1,1 100x200", GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("200x100", GetDisplayInfoAt(0).size_in_pixel().ToString());

  EXPECT_EQ("1,201 300x400", GetDisplayInfoAt(1).bounds_in_native().ToString());
  EXPECT_EQ("400x300", GetDisplayInfoAt(1).size_in_pixel().ToString());
  reset();
  UpdateDisplay("100x200/b,300x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();

  EXPECT_EQ("1,1 100x200", GetDisplayInfoAt(0).bounds_in_native().ToString());
  EXPECT_EQ("100x200", GetDisplayInfoAt(0).size_in_pixel().ToString());

  EXPECT_EQ("1,201 300x400", GetDisplayInfoAt(1).bounds_in_native().ToString());
  EXPECT_EQ("300x400", GetDisplayInfoAt(1).size_in_pixel().ToString());

  // Just Rotating display will change the bounds on both display.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("2 0 0", GetCountSummary());
  reset();

  // Updating to the same configuration should report no changes.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("0 0 0", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("100x200/r,300x400");
  EXPECT_EQ("1 0 0", GetCountSummary());
  reset();

  UpdateDisplay("200x200");
  EXPECT_EQ("1 0 1", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("200x200/u");
  EXPECT_EQ("1 0 0", GetCountSummary());
  reset();

  UpdateDisplay("200x200/l");
  EXPECT_EQ("1 0 0", GetCountSummary());

  // Having the internal display deactivated should restore user rotation. Newly
  // set rotations should be applied.
  UpdateDisplay("200x200, 200x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display_manager()->SetDisplayRotation(internal_display_id,
                                        display::Display::ROTATE_90,
                                        display::Display::ROTATION_SOURCE_USER);
  display_manager()->SetDisplayRotation(
      internal_display_id, display::Display::ROTATE_0,
      display::Display::ROTATION_SOURCE_ACTIVE);

  const display::ManagedDisplayInfo info =
      GetDisplayInfoForId(internal_display_id);
  EXPECT_EQ(display::Display::ROTATE_0, info.GetActiveRotation());

  // Deactivate internal display to simulate Docked Mode.
  vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(GetDisplayInfoAt(1));
  display_manager()->OnNativeDisplaysChanged(secondary_only);

  const display::ManagedDisplayInfo& post_removal_info =
      display::test::DisplayManagerTestApi(display_manager())
          .GetInternalManagedDisplayInfo(internal_display_id);
  EXPECT_NE(info.GetActiveRotation(), post_removal_info.GetActiveRotation());
  EXPECT_EQ(display::Display::ROTATE_90, post_removal_info.GetActiveRotation());

  display_manager()->SetDisplayRotation(
      internal_display_id, display::Display::ROTATE_180,
      display::Display::ROTATION_SOURCE_ACTIVE);
  const display::ManagedDisplayInfo& post_rotation_info =
      display::test::DisplayManagerTestApi(display_manager())
          .GetInternalManagedDisplayInfo(internal_display_id);
  EXPECT_NE(info.GetActiveRotation(), post_rotation_info.GetActiveRotation());
  EXPECT_EQ(display::Display::ROTATE_180,
            post_rotation_info.GetActiveRotation());
}

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, UIScale) {
  display::test::ScopedDisable125DSFForUIScaling disable;

  UpdateDisplay("1280x800");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.0, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  UpdateDisplay("1366x768");
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.75f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.6f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.6f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  UpdateDisplay("1280x850*2");
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.5f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.0f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(2.0f, display.device_scale_factor());
  EXPECT_EQ("640x425", display.bounds().size().ToString());

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 2.0f);
  EXPECT_EQ(2.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(1.0f, display.device_scale_factor());
  EXPECT_EQ("1280x850", display.bounds().size().ToString());

  // 1.25 ui scaling on 1.25 DSF device should use 1.0 DSF
  // on screen.
  UpdateDisplay("1280x850*1.25");
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(1.0f, display.device_scale_factor());
  EXPECT_EQ("1280x850", display.bounds().size().ToString());
}
#endif  // defined(OS_CHROMEOS)

TEST_P(DisplayManagerTest, UIScaleWithDisplayMode) {
  int display_id = 1000;

  // Setup the display modes with UI-scale.
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1280, 800));
  const scoped_refptr<display::ManagedDisplayMode>& base_mode(
      new display::ManagedDisplayMode(gfx::Size(1280, 800), 60.0f, false,
                                      false));
  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list =
      CreateInternalManagedDisplayModeList(base_mode);
  native_display_info.SetManagedDisplayModes(mode_list);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  scoped_refptr<display::ManagedDisplayMode> expected_mode = base_mode;
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.125f);
  EXPECT_EQ(1.125f, GetDisplayInfoAt(0).configured_ui_scale());

  expected_mode = new display::ManagedDisplayMode(
      expected_mode->size(), expected_mode->refresh_rate(),
      expected_mode->is_interlaced(), expected_mode->native(),
      1.125f /* ui_scale */, expected_mode->device_scale_factor());

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());

  expected_mode = new display::ManagedDisplayMode(
      expected_mode->size(), expected_mode->refresh_rate(),
      expected_mode->is_interlaced(), expected_mode->native(),
      0.8f /* ui_scale */, expected_mode->device_scale_factor());

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.75f);
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.625f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());

  expected_mode = new display::ManagedDisplayMode(
      expected_mode->size(), expected_mode->refresh_rate(),
      expected_mode->is_interlaced(), expected_mode->native(),
      0.625f /* ui_scale */, expected_mode->device_scale_factor());

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.6f);
  EXPECT_EQ(0.625f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());

  expected_mode = new display::ManagedDisplayMode(
      expected_mode->size(), expected_mode->refresh_rate(),
      expected_mode->is_interlaced(), expected_mode->native(),
      0.5f /* ui_scale */, expected_mode->device_scale_factor());

  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));
}

// Tests that ResetInternalDisplayZoom() resets to the default 0.8f UI scale
// defined for the 1.25x displays.
TEST_P(DisplayManagerTest, ResetInternalDisplayZoomFor1_25x) {
  // Setup the display modes with UI-scale.
  const scoped_refptr<display::ManagedDisplayMode> base_mode(
      new display::ManagedDisplayMode(gfx::Size(1920, 1080), 60.0f,
                                      false /* is_interlaced */,
                                      true /* native */, 1.0f /* ui_scale */,
                                      1.25f /* device_scale_factor */));
  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list =
      CreateInternalManagedDisplayModeList(base_mode);

  const int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1920, 1080));
  native_display_info.set_device_scale_factor(1.25f);
  native_display_info.SetManagedDisplayModes(mode_list);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  scoped_refptr<display::ManagedDisplayMode> expected_mode = base_mode;
  EXPECT_TRUE(expected_mode->IsEquivalent(
      display_manager()->GetActiveModeForDisplayId(display_id)));

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ("960x540", GetDisplayForId(display_id).size().ToString());

  // Reset the internal display zoom and expect the UI scale to go to the
  // default 0.8f.
  display_manager()->ResetInternalDisplayZoom();
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ(0.8f, GetDisplayInfoAt(0).configured_ui_scale());
  EXPECT_EQ("1536x864", GetDisplayForId(display_id).size().ToString());
}

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, Use125DSFForUIScaling) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);
  UpdateDisplay("1920x1080*1.25");
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.8f);
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("1536x864", GetDisplayForId(display_id).size().ToString());

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 0.5f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(0.5f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("960x540", GetDisplayForId(display_id).size().ToString());

  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(display_id, 1.25f);
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveUIScale());
  EXPECT_EQ("2400x1350", GetDisplayForId(display_id).size().ToString());
}
#endif  // defined(OS_CHROMEOS)

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, FHD125DefaultsTo08UIScaling) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display_id++;
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  // Setup the display modes with UI-scale.
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1920, 1080));
  native_display_info.set_device_scale_factor(1.25);

  const scoped_refptr<display::ManagedDisplayMode>& base_mode(
      new display::ManagedDisplayMode(gfx::Size(1920, 1080), 60.0f, false,
                                      false));
  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list =
      CreateInternalManagedDisplayModeList(base_mode);
  native_display_info.SetManagedDisplayModes(mode_list);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);

  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(1.25f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
}
#endif  // defined(OS_CHROMEOS)

// TODO(msw): Broken on Windows. http://crbug.com/584038
#if defined(OS_CHROMEOS)
// Don't default to 1.25 DSF if the user already has a prefrence stored for
// the internal display.
TEST_P(DisplayManagerTest, FHD125DefaultsTo08UIScalingNoOverride) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display_id++;
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);
  const gfx::Insets dummy_overscan_insets;
  display_manager()->RegisterDisplayProperty(
      display_id, display::Display::ROTATE_0, 1.0f, &dummy_overscan_insets,
      gfx::Size(), 1.0f, ui::ColorCalibrationProfile());

  // Setup the display modes with UI-scale.
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1920, 1080));
  native_display_info.set_device_scale_factor(1.25);

  const scoped_refptr<display::ManagedDisplayMode>& base_mode(
      new display::ManagedDisplayMode(gfx::Size(1920, 1080), 60.0f, false,
                                      false));
  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list =
      CreateInternalManagedDisplayModeList(base_mode);
  native_display_info.SetManagedDisplayModes(mode_list);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);

  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveDeviceScaleFactor());
  EXPECT_EQ(1.0f, GetDisplayInfoAt(0).GetEffectiveUIScale());
}
#endif  // defined(OS_CHROMEOS)

TEST_P(DisplayManagerTest, ResolutionChangeInUnifiedMode) {
  if (!SupportsMultipleDisplays())
    return;
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("200x200, 400x400");

  int64_t unified_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::ManagedDisplayInfo info =
      display_manager()->GetDisplayInfo(unified_id);
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("400x200", info.display_modes()[0]->size().ToString());
  EXPECT_TRUE(info.display_modes()[0]->native());
  EXPECT_EQ("800x400", info.display_modes()[1]->size().ToString());
  EXPECT_FALSE(info.display_modes()[1]->native());
  EXPECT_EQ(
      "400x200",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());
  scoped_refptr<display::ManagedDisplayMode> active_mode =
      display_manager()->GetActiveModeForDisplayId(unified_id);
  EXPECT_EQ(1.0f, active_mode->ui_scale());
  EXPECT_EQ("400x200", active_mode->size().ToString());

  EXPECT_TRUE(display::test::SetDisplayResolution(display_manager(), unified_id,
                                                  gfx::Size(800, 400)));
  EXPECT_EQ(
      "800x400",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());

  active_mode = display_manager()->GetActiveModeForDisplayId(unified_id);
  EXPECT_EQ(1.0f, active_mode->ui_scale());
  EXPECT_EQ("800x400", active_mode->size().ToString());

  // resolution change will not persist in unified desktop mode.
  UpdateDisplay("600x600, 200x200");
  EXPECT_EQ(
      "1200x600",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());
  active_mode = display_manager()->GetActiveModeForDisplayId(unified_id);
  EXPECT_EQ(1.0f, active_mode->ui_scale());
  EXPECT_TRUE(active_mode->native());
  EXPECT_EQ("1200x600", active_mode->size().ToString());
}

// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, UpdateMouseCursorAfterRotateZoom) {
  // Make sure just rotating will not change native location.
  UpdateDisplay("300x200,200x150");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  // Test on 1st display.
  generator1.MoveMouseToInHost(150, 50);
  EXPECT_EQ("150,50", env->last_mouse_location().ToString());
  UpdateDisplay("300x200/r,200x150");
  EXPECT_EQ("50,149", env->last_mouse_location().ToString());

  // Test on 2nd display.
  generator2.MoveMouseToInHost(50, 100);
  EXPECT_EQ("250,100", env->last_mouse_location().ToString());
  UpdateDisplay("300x200/r,200x150/l");
  EXPECT_EQ("249,50", env->last_mouse_location().ToString());

  // The native location is now outside, so move to the center
  // of closest display.
  UpdateDisplay("300x200/r,100x50/l");
  EXPECT_EQ("225,50", env->last_mouse_location().ToString());

  // Make sure just zooming will not change native location.
  UpdateDisplay("600x400*2,400x300");

  // Test on 1st display.
  generator1.MoveMouseToInHost(200, 300);
  EXPECT_EQ("100,150", env->last_mouse_location().ToString());
  UpdateDisplay("600x400*2@1.5,400x300");
  EXPECT_EQ("150,225", env->last_mouse_location().ToString());

  // Test on 2nd display.
  UpdateDisplay("600x400,400x300*2");
  generator2.MoveMouseToInHost(200, 250);
  EXPECT_EQ("700,125", env->last_mouse_location().ToString());
  UpdateDisplay("600x400,400x300*2@1.5");
  EXPECT_EQ("750,187", env->last_mouse_location().ToString());

  // The native location is now outside, so move to the
  // center of closest display.
  UpdateDisplay("600x400,400x200*2@1.5");
  EXPECT_EQ("750,75", env->last_mouse_location().ToString());
}
#endif  // defined(OS_CHROMEOS)

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() : changed_(false) {}
  ~TestDisplayObserver() override {}

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display&, uint32_t) override {}
  void OnDisplayAdded(const display::Display& new_display) override {
    // Mirror window should already be delete before restoring
    // the external display.
    EXPECT_FALSE(test_api.GetHost());
    changed_ = true;
  }
  void OnDisplayRemoved(const display::Display& old_display) override {
    // Mirror window should not be created until the external display
    // is removed.
    EXPECT_FALSE(test_api.GetHost());
    changed_ = true;
  }

  bool changed_and_reset() {
    bool changed = changed_;
    changed_ = false;
    return changed;
  }

 private:
  test::MirrorWindowTestApi test_api;
  bool changed_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

TEST_P(DisplayManagerTest, SoftwareMirroring) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");

  test::MirrorWindowTestApi test_api;
  EXPECT_EQ(nullptr, test_api.GetHost());

  TestDisplayObserver display_observer;
  display::Screen::GetScreen()->AddObserver(&display_observer);

  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager()->UpdateDisplays();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(
      "0,0 300x400",
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().ToString());
  EXPECT_EQ("400x500", test_api.GetHost()->GetBounds().size().ToString());
  EXPECT_EQ("300x400",
            test_api.GetHost()->window()->bounds().size().ToString());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  display_manager()->SetMirrorMode(false);
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(nullptr, test_api.GetHost());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Make sure the mirror window has the pixel size of the
  // source display.
  display_manager()->SetMirrorMode(true);
  EXPECT_TRUE(display_observer.changed_and_reset());

  UpdateDisplay("300x400@0.5,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("300x400",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("310x410*2,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("310x410",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("320x420/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("320x420",
            test_api.GetHost()->window()->bounds().size().ToString());

  UpdateDisplay("330x440/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("330x440",
            test_api.GetHost()->window()->bounds().size().ToString());

  // Overscan insets are ignored.
  UpdateDisplay("400x600/o,600x800/o");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ("400x600",
            test_api.GetHost()->window()->bounds().size().ToString());

  display::Screen::GetScreen()->RemoveObserver(&display_observer);
}

TEST_P(DisplayManagerTest, RotateInSoftwareMirroring) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("600x400,500x300");
  display_manager()->SetMirrorMode(true);

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager()->SetDisplayRotation(
      primary_id, display::Display::ROTATE_180,
      display::Display::ROTATION_SOURCE_ACTIVE);
  display_manager()->SetMirrorMode(false);
}

TEST_P(DisplayManagerTest, SingleDisplayToSoftwareMirroring) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("600x400");

  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  UpdateDisplay("600x400,600x400");

  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  WindowTreeHostManager* window_tree_host_manager =
      ash::Shell::GetInstance()->window_tree_host_manager();
  EXPECT_TRUE(
      window_tree_host_manager->mirror_window_controller()->GetWindow());

  UpdateDisplay("600x400");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(
      window_tree_host_manager->mirror_window_controller()->GetWindow());
}

#if defined(OS_CHROMEOS)
// Make sure this does not cause any crashes. See http://crbug.com/412910
// This test is limited to OS_CHROMEOS because CursorCompositingEnabled is only
// for ChromeOS.
TEST_P(DisplayManagerTest, SoftwareMirroringWithCompositingCursor) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("300x400,400x500");

  test::MirrorWindowTestApi test_api;
  EXPECT_EQ(nullptr, test_api.GetHost());

  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(
          display_manager()->GetSecondaryDisplay().id());

  display_manager()->SetSoftwareMirroring(true);
  display_manager()->UpdateDisplays();

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_FALSE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  Shell::GetInstance()->SetCursorCompositingEnabled(true);

  EXPECT_TRUE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  // Removes the first display and keeps the second one.
  display_manager()->SetSoftwareMirroring(false);
  std::vector<display::ManagedDisplayInfo> new_info_list;
  new_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(new_info_list);

  root_windows = Shell::GetAllRootWindows();
  EXPECT_TRUE(root_windows[0]->Contains(test_api.GetCursorWindow()));

  Shell::GetInstance()->SetCursorCompositingEnabled(false);
}
#endif  // OS_CHROMEOS

TEST_P(DisplayManagerTest, MirroredLayout) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("500x500,400x400");
  EXPECT_FALSE(display_manager()->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());

  UpdateDisplay("1+0-500x500,1+0-500x500");
  EXPECT_TRUE(display_manager()->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());

  UpdateDisplay("500x500,500x500");
  EXPECT_FALSE(display_manager()->GetCurrentDisplayLayout().mirrored);
  EXPECT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
}

TEST_P(DisplayManagerTest, InvertLayout) {
  EXPECT_EQ("left, 0",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("left, -100",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, 100)
                .Swap()
                .ToString());
  EXPECT_EQ("left, 50",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, -50)
                .Swap()
                .ToString());

  EXPECT_EQ("right, 0",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("right, -90",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, 90)
                .Swap()
                .ToString());
  EXPECT_EQ("right, 60",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, -60)
                .Swap()
                .ToString());

  EXPECT_EQ("bottom, 0",
            display::DisplayPlacement(display::DisplayPlacement::TOP, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("bottom, -80",
            display::DisplayPlacement(display::DisplayPlacement::TOP, 80)
                .Swap()
                .ToString());
  EXPECT_EQ("bottom, 70",
            display::DisplayPlacement(display::DisplayPlacement::TOP, -70)
                .Swap()
                .ToString());

  EXPECT_EQ("top, 0",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("top, -70",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, 70)
                .Swap()
                .ToString());
  EXPECT_EQ("top, 80",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, -80)
                .Swap()
                .ToString());
}

TEST_P(DisplayManagerTest, NotifyPrimaryChange) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("500x500,500x500");
  SwapPrimaryDisplay();
  reset();
  UpdateDisplay("500x500");
  EXPECT_FALSE(changed_metrics() &
               display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_FALSE(changed_metrics() &
               display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);

  UpdateDisplay("500x500,500x500");
  SwapPrimaryDisplay();
  UpdateDisplay("500x400");
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);
}

TEST_P(DisplayManagerTest, NotifyPrimaryChangeUndock) {
  if (!SupportsMultipleDisplays())
    return;
  // Assume the default display is an external display, and
  // emulates undocking by switching to another display.
  display::ManagedDisplayInfo another_display_info =
      CreateDisplayInfo(1, gfx::Rect(0, 0, 1280, 800));
  std::vector<display::ManagedDisplayInfo> info_list;
  info_list.push_back(another_display_info);
  reset();
  display_manager()->OnNativeDisplaysChanged(info_list);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);
}

// TODO(scottmg): RootWindow doesn't get resized on Windows
// Ash. http://crbug.com/247916.
#if defined(OS_CHROMEOS)
TEST_P(DisplayManagerTest, UpdateDisplayWithHostOrigin) {
  UpdateDisplay("100x200,300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  aura::Window::Windows root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  aura::WindowTreeHost* host0 = root_windows[0]->GetHost();
  aura::WindowTreeHost* host1 = root_windows[1]->GetHost();

  EXPECT_EQ("1,1", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  // UpdateDisplay set the origin if it's not set.
  EXPECT_NE("1,1", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("100x200,200+300-300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ("0,0", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  EXPECT_EQ("200,300", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("400+500-200x300,300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ("400,500", host0->GetBounds().origin().ToString());
  EXPECT_EQ("200x300", host0->GetBounds().size().ToString());
  EXPECT_EQ("0,0", host1->GetBounds().origin().ToString());
  EXPECT_EQ("300x400", host1->GetBounds().size().ToString());

  UpdateDisplay("100+200-100x200,300+500-200x300");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ("100,200", host0->GetBounds().origin().ToString());
  EXPECT_EQ("100x200", host0->GetBounds().size().ToString());
  EXPECT_EQ("300,500", host1->GetBounds().origin().ToString());
  EXPECT_EQ("200x300", host1->GetBounds().size().ToString());
}
#endif  // defined(OS_CHROMEOS)

TEST_P(DisplayManagerTest, UnifiedDesktopBasic) {
  if (!SupportsMultipleDisplays())
    return;

  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");

  // Enable after extended mode.
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Defaults to the unified desktop.
  display::Screen* screen = display::Screen::GetScreen();
  // The 2nd display is scaled so that it has the same height as 1st display.
  // 300 * 500 / 200  + 400 = 1150.
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

  display_manager()->SetMirrorMode(true);
  EXPECT_EQ(gfx::Size(400, 500), screen->GetPrimaryDisplay().size());

  display_manager()->SetMirrorMode(false);
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

  // Switch to single desktop.
  UpdateDisplay("500x300");
  EXPECT_EQ(gfx::Size(500, 300), screen->GetPrimaryDisplay().size());

  // Switch to unified desktop.
  UpdateDisplay("500x300,400x500");
  // 400 * 300 / 500 + 500 ~= 739.
  EXPECT_EQ(gfx::Size(739, 300), screen->GetPrimaryDisplay().size());

  // The default should fit to the internal display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(CreateDisplayInfo(10, gfx::Rect(0, 0, 500, 300)));
  display_info_list.push_back(
      CreateDisplayInfo(11, gfx::Rect(500, 0, 400, 500)));
  {
    display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                           11);
    display_manager()->OnNativeDisplaysChanged(display_info_list);
    // 500 * 500 / 300 + 400 ~= 1233.
    EXPECT_EQ(gfx::Size(1233, 500), screen->GetPrimaryDisplay().size());
  }

  // Switch to 3 displays.
  UpdateDisplay("500x300,400x500,500x300");
  EXPECT_EQ(gfx::Size(1239, 300), screen->GetPrimaryDisplay().size());

  // Switch back to extended desktop.
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_EQ(gfx::Size(500, 300), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(400, 500),
            display_manager()->GetSecondaryDisplay().size());
  EXPECT_EQ(
      gfx::Size(500, 300),
      display_manager()
          ->GetDisplayForId(display_manager()->GetSecondaryDisplay().id() + 1)
          .size());
}

TEST_P(DisplayManagerTest, UnifiedDesktopWithHardwareMirroring) {
  if (!SupportsMultipleDisplays())
    return;
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  // Enter to hardware mirroring.
  display::ManagedDisplayInfo d1(1, "", false);
  d1.SetBounds(gfx::Rect(0, 0, 500, 500));
  display::ManagedDisplayInfo d2(2, "", false);
  d2.SetBounds(gfx::Rect(0, 0, 500, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(d1);
  display_info_list.push_back(d2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  ASSERT_TRUE(display_manager()->IsInMirrorMode());
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // The display manager automaticaclly switches to software mirroring
  // if the displays are configured to use mirroring when running on desktop.
  // This is a workdaround to force the display manager to forget
  // the mirroing layout.
  display::DisplayIdList list = display::test::CreateDisplayIdList2(1, 2);
  display::DisplayLayoutBuilder builder(
      display_manager()->layout_store()->GetRegisteredDisplayLayout(list));
  builder.SetMirrored(false);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());

  // Exit from hardware mirroring.
  d2.SetBounds(gfx::Rect(0, 500, 500, 500));
  display_info_list.clear();
  display_info_list.push_back(d1);
  display_info_list.push_back(d2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());
}

TEST_P(DisplayManagerTest, UnifiedDesktopEnabledWithExtended) {
  if (!SupportsMultipleDisplays())
    return;
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");
  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  display::DisplayLayoutBuilder builder(
      display_manager()->layout_store()->GetRegisteredDisplayLayout(list));
  builder.SetDefaultUnified(false);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_P(DisplayManagerTest, UnifiedDesktopWith2xDSF) {
  if (!SupportsMultipleDisplays())
    return;
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  // 2nd display is 2x.
  UpdateDisplay("400x500,1000x800*2");
  display::ManagedDisplayInfo info =
      display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  EXPECT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("1640x800", info.display_modes()[0]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[0]->device_scale_factor());
  EXPECT_EQ("1025x500", info.display_modes()[1]->size().ToString());
  EXPECT_EQ(1.0f, info.display_modes()[1]->device_scale_factor());

  // For 1x, 400 + 500 / 800 * 100 = 1025.
  EXPECT_EQ("1025x500", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1025x500",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  accelerators::ZoomInternalDisplay(false);
  // (800 / 500 * 400 + 500) /2 = 820
  EXPECT_EQ("820x400", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("820x400",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());

  // 1st display is 2x.
  UpdateDisplay("1200x800*2,1000x1000");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  EXPECT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("2000x800", info.display_modes()[0]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[0]->device_scale_factor());
  EXPECT_EQ("2500x1000", info.display_modes()[1]->size().ToString());
  EXPECT_EQ(1.0f, info.display_modes()[1]->device_scale_factor());

  // For 2x, (800 / 1000 * 1000 + 1200) / 2 = 1000
  EXPECT_EQ("1000x400", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1000x400",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  accelerators::ZoomInternalDisplay(true);
  // 1000 / 800 * 1200 + 1000 = 2500
  EXPECT_EQ("2500x1000", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("2500x1000",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());

  // Both displays are 2x.
  // 1st display is 2x.
  UpdateDisplay("1200x800*2,1000x1000*2");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  EXPECT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("2000x800", info.display_modes()[0]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[0]->device_scale_factor());
  EXPECT_EQ("2500x1000", info.display_modes()[1]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[1]->device_scale_factor());

  EXPECT_EQ("1000x400", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1000x400",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  accelerators::ZoomInternalDisplay(true);
  EXPECT_EQ("1250x500", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1250x500",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());

  // Both displays have the same physical height, with the first display
  // being 2x.
  UpdateDisplay("1000x800*2,300x800");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  EXPECT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("1300x800", info.display_modes()[0]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[0]->device_scale_factor());
  EXPECT_EQ("1300x800", info.display_modes()[1]->size().ToString());
  EXPECT_EQ(1.0f, info.display_modes()[1]->device_scale_factor());

  EXPECT_EQ("650x400", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("650x400",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  accelerators::ZoomInternalDisplay(true);
  EXPECT_EQ("1300x800", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1300x800",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());

  // Both displays have the same physical height, with the second display
  // being 2x.
  UpdateDisplay("1000x800,300x800*2");
  EXPECT_EQ(2u, info.display_modes().size());
  EXPECT_EQ("1300x800", info.display_modes()[0]->size().ToString());
  EXPECT_EQ(2.0f, info.display_modes()[0]->device_scale_factor());
  EXPECT_EQ("1300x800", info.display_modes()[1]->size().ToString());
  EXPECT_EQ(1.0f, info.display_modes()[1]->device_scale_factor());

  EXPECT_EQ("1300x800", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("1300x800",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
  accelerators::ZoomInternalDisplay(false);
  EXPECT_EQ("650x400", screen->GetPrimaryDisplay().size().ToString());
  EXPECT_EQ("650x400",
            Shell::GetPrimaryRootWindow()->bounds().size().ToString());
}

// Updating displays again in unified desktop mode should not crash.
// crbug.com/491094.
TEST_P(DisplayManagerTest, ConfigureUnifiedTwice) {
  if (!SupportsMultipleDisplays())
    return;
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("300x200,400x500");
  // Mirror windows are created in a posted task.
  RunAllPendingInMessageLoop();

  UpdateDisplay("300x250,400x550");
  RunAllPendingInMessageLoop();
}

TEST_P(DisplayManagerTest, NoRotateUnifiedDesktop) {
  if (!SupportsMultipleDisplays())
    return;
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  EXPECT_EQ("1150x500", display.size().ToString());
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ("1150x500", screen->GetPrimaryDisplay().size().ToString());
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::ROTATION_SOURCE_ACTIVE);
  EXPECT_EQ("1150x500", screen->GetPrimaryDisplay().size().ToString());

  UpdateDisplay("400x500");
  EXPECT_EQ("400x500", screen->GetPrimaryDisplay().size().ToString());
}

// Makes sure the transition from unified to single won't crash
// with docked windows.
TEST_P(DisplayManagerTest, UnifiedWithDockWindows) {
  if (!SupportsMultipleDisplays())
    return;
  const int height_offset = GetMdMaximizedWindowHeightOffset();
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");

  std::unique_ptr<aura::Window> docked(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 10, 50, 50)));
  docked->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_DOCKED);
  ASSERT_TRUE(wm::GetWindowState(docked.get())->IsDocked());
  // 47 pixels reserved for launcher shelf height in non-material design, and
  // 48 pixels reserved in material design.
  EXPECT_EQ(gfx::Rect(0, 0, 250, 453 + height_offset).ToString(),
            docked->bounds().ToString());
  UpdateDisplay("300x300");
  // Make sure the window is still docked.
  EXPECT_TRUE(wm::GetWindowState(docked.get())->IsDocked());
  EXPECT_EQ(gfx::Rect(0, 0, 250, 253 + height_offset).ToString(),
            docked->bounds().ToString());
}

TEST_P(DisplayManagerTest, DockMode) {
  if (!SupportsMultipleDisplays())
    return;
  const int64_t internal_id = 1;
  const int64_t external_id = 2;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, gfx::Rect(0, 0, 500, 500));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 100, 100));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // software mirroring.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);

  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->active_display_list().size());

  EXPECT_TRUE(display_manager()->IsActiveDisplayId(external_id));
  EXPECT_FALSE(display_manager()->IsActiveDisplayId(internal_id));

  EXPECT_FALSE(display_manager()->ZoomInternalDisplay(true));
  EXPECT_FALSE(display_manager()->ZoomInternalDisplay(false));
  EXPECT_FALSE(display::test::DisplayManagerTestApi(display_manager())
                   .SetDisplayUIScale(internal_id, 1.0f));
}

// Make sure that bad layout information is ignored and does not crash.
TEST_P(DisplayManagerTest, DontRegisterBadConfig) {
  if (!SupportsMultipleDisplays())
    return;
  display::DisplayIdList list = display::test::CreateDisplayIdList2(1, 2);
  display::DisplayLayoutBuilder builder(1);
  builder.AddDisplayPlacement(2, 1, display::DisplayPlacement::LEFT, 0);
  builder.AddDisplayPlacement(3, 1, display::DisplayPlacement::BOTTOM, 0);

  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
}

class ScreenShutdownTest : public test::AshTestBase {
 public:
  ScreenShutdownTest() {}
  ~ScreenShutdownTest() override {}

  void TearDown() override {
    display::Screen* orig_screen = display::Screen::GetScreen();
    AshTestBase::TearDown();
    if (!SupportsMultipleDisplays())
      return;
    display::Screen* screen = display::Screen::GetScreen();
    EXPECT_NE(orig_screen, screen);
    EXPECT_EQ(2, screen->GetNumDisplays());
    EXPECT_EQ("500x300", screen->GetPrimaryDisplay().size().ToString());
    std::vector<display::Display> all = screen->GetAllDisplays();
    EXPECT_EQ("500x300", all[0].size().ToString());
    EXPECT_EQ("800x400", all[1].size().ToString());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenShutdownTest);
};

TEST_F(ScreenShutdownTest, ScreenAfterShutdown) {
  if (!SupportsMultipleDisplays())
    return;
  UpdateDisplay("500x300,800x400");
}

#if defined(OS_CHROMEOS)
namespace {

// A helper class that sets the display configuration and starts ash.
// This is to make sure the font configuration happens during ash
// initialization process.
class FontTestHelper : public test::AshTestBase {
 public:
  enum DisplayType { INTERNAL, EXTERNAL };

  FontTestHelper(float scale, DisplayType display_type) {
    gfx::ClearFontRenderParamsCacheForTest();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (display_type == INTERNAL)
      command_line->AppendSwitch(::switches::kUseFirstDisplayAsInternal);
    command_line->AppendSwitchASCII(::switches::kHostWindowBounds,
                                    StringPrintf("1000x800*%f", scale));
    SetUp();
  }

  ~FontTestHelper() override { TearDown(); }

  // test::AshTestBase:
  void TestBody() override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FontTestHelper);
};

bool IsTextSubpixelPositioningEnabled() {
  gfx::FontRenderParams params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  return params.subpixel_positioning;
}

gfx::FontRenderParams::Hinting GetFontHintingParams() {
  gfx::FontRenderParams params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  return params.hinting;
}

}  // namespace

using DisplayManagerFontTest = testing::Test;

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100Internal) {
  FontTestHelper helper(1.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf125Internal) {
  display::test::ScopedDisable125DSFForUIScaling disable;
  FontTestHelper helper(1.25f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.25f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200Internal) {
  FontTestHelper helper(2.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());

  display::test::DisplayManagerTestApi(helper.display_manager())
      .SetDisplayUIScale(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                         2.0f);

  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100External) {
  FontTestHelper helper(1.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf125External) {
  FontTestHelper helper(1.25f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.25f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200External) {
  FontTestHelper helper(2.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest,
       TextSubpixelPositioningWithDsf125InternalWithScaling) {
  FontTestHelper helper(1.25f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());

  display::test::DisplayManagerTestApi(helper.display_manager())
      .SetDisplayUIScale(display::Screen::GetScreen()->GetPrimaryDisplay().id(),
                         0.8f);

  ASSERT_DOUBLE_EQ(
      1.25f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_P(DisplayManagerTest, CheckInitializationOfRotationProperty) {
  int64_t id = display_manager()->GetDisplayAt(0).id();
  display_manager()->RegisterDisplayProperty(id, display::Display::ROTATE_90,
                                             1.0f, nullptr, gfx::Size(), 1.0f,
                                             ui::COLOR_PROFILE_STANDARD);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(id);

  EXPECT_EQ(display::Display::ROTATE_90,
            info.GetRotation(display::Display::ROTATION_SOURCE_USER));
  EXPECT_EQ(display::Display::ROTATE_90,
            info.GetRotation(display::Display::ROTATION_SOURCE_ACTIVE));
}

TEST_P(DisplayManagerTest, RejectInvalidLayoutData) {
  display::DisplayLayoutStore* layout_store = display_manager()->layout_store();
  int64_t id1 = 10001;
  int64_t id2 = 10002;
  ASSERT_TRUE(display::CompareDisplayIds(id1, id2));
  display::DisplayLayoutBuilder good_builder(id1);
  good_builder.SetSecondaryPlacement(id2, display::DisplayPlacement::LEFT, 0);
  std::unique_ptr<display::DisplayLayout> good(good_builder.Build());

  display::DisplayIdList good_list =
      display::test::CreateDisplayIdList2(id1, id2);
  layout_store->RegisterLayoutForDisplayIdList(good_list, good->Copy());

  display::DisplayLayoutBuilder bad(id1);
  bad.SetSecondaryPlacement(id2, display::DisplayPlacement::BOTTOM, 0);

  display::DisplayIdList bad_list(2);
  bad_list[0] = id2;
  bad_list[1] = id1;
  layout_store->RegisterLayoutForDisplayIdList(bad_list, bad.Build());

  EXPECT_EQ(good->ToString(),
            layout_store->GetRegisteredDisplayLayout(good_list).ToString());
}

TEST_P(DisplayManagerTest, GuessDisplayIdFieldsInDisplayLayout) {
  int64_t id1 = 10001;
  int64_t id2 = 10002;

  std::unique_ptr<display::DisplayLayout> old_layout(
      new display::DisplayLayout);
  old_layout->placement_list.emplace_back(display::DisplayPlacement::BOTTOM, 0);
  old_layout->primary_id = id1;

  display::DisplayLayoutStore* layout_store = display_manager()->layout_store();
  display::DisplayIdList list = display::test::CreateDisplayIdList2(id1, id2);
  layout_store->RegisterLayoutForDisplayIdList(list, std::move(old_layout));
  const display::DisplayLayout& stored =
      layout_store->GetRegisteredDisplayLayout(list);

  EXPECT_EQ(id1, stored.placement_list[0].parent_display_id);
  EXPECT_EQ(id2, stored.placement_list[0].display_id);
}

#endif  // OS_CHROMEOS

}  // namespace ash
