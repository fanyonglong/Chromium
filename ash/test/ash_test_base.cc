// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm/window_positioner.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/display/extended_mouse_warp_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/unified_mouse_warp_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/ime/input_method_event_handler.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/env.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/core/coordinate_conversion.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#include "ui/platform_window/win/win_window.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"  // nogncheck
#endif

namespace ash {
namespace test {
namespace {

class AshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  AshEventGeneratorDelegate() {}
  ~AshEventGeneratorDelegate() override {}

  // aura::test::EventGeneratorDelegateAura overrides:
  aura::WindowTreeHost* GetHostAt(
      const gfx::Point& point_in_screen) const override {
    display::Screen* screen = display::Screen::GetScreen();
    display::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    return Shell::GetInstance()
        ->window_tree_host_manager()
        ->GetRootWindowForDisplayId(display.id())
        ->GetHost();
  }

  aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const override {
    return aura::client::GetScreenPositionClient(window->GetRootWindow());
  }

  void DispatchKeyEventToIME(ui::EventTarget* target,
                             ui::KeyEvent* event) override {
    // In Ash environment, the key event will be processed by event rewriters
    // first.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshEventGeneratorDelegate);
};

}  // namespace

/////////////////////////////////////////////////////////////////////////////

AshTestBase::AshTestBase()
    : setup_called_(false),
      teardown_called_(false),
      start_session_(true),
      material_mode_(MaterialDesignController::Mode::UNINITIALIZED) {
#if defined(USE_X11)
  // This is needed for tests which use this base class but are run in browser
  // test binaries so don't get the default initialization in the unit test
  // suite.
  gfx::InitializeThreadedX11();
#endif

  ash_test_environment_ = AshTestEnvironment::Create();

  // Must initialize |ash_test_helper_| here because some tests rely on
  // AshTestBase methods before they call AshTestBase::SetUp().
  ash_test_helper_.reset(new AshTestHelper(ash_test_environment_.get()));
}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  setup_called_ = true;

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(::switches::kHostWindowBounds)) {
    command_line->AppendSwitchASCII(::switches::kHostWindowBounds,
                                    "1+1-800x600");
  }
#if defined(OS_WIN)
  ui::test::SetUsePopupAsRootWindowForTest(true);
#endif

  ash_test_helper_->SetUp(start_session_, material_mode_);

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  Shell::GetInstance()->cursor_manager()->EnableMouseEvents();

  // Changing GestureConfiguration shouldn't make tests fail. These values
  // prevent unexpected events from being generated during tests. Such as
  // delayed events which create race conditions on slower tests.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);
}

void AshTestBase::TearDown() {
  teardown_called_ = true;
  Shell::GetInstance()->OnAppTerminating();
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  ash_test_helper_->TearDown();
#if defined(OS_WIN)
  ui::test::SetUsePopupAsRootWindowForTest(false);
#endif

  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  display::Display::SetInternalDisplayId(display::Display::kInvalidDisplayID);
}

// static
WmShelf* AshTestBase::GetPrimaryShelf() {
  return WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->GetShelf();
}

// static
SystemTray* AshTestBase::GetPrimarySystemTray() {
  return Shell::GetInstance()->GetPrimarySystemTray();
}

ui::test::EventGenerator& AshTestBase::GetEventGenerator() {
  if (!event_generator_) {
    event_generator_.reset(
        new ui::test::EventGenerator(new AshEventGeneratorDelegate()));
  }
  return *event_generator_.get();
}

// static
display::Display::Rotation AshTestBase::GetActiveDisplayRotation(int64_t id) {
  return Shell::GetInstance()
      ->display_manager()
      ->GetDisplayInfo(id)
      .GetActiveRotation();
}

// static
display::Display::Rotation AshTestBase::GetCurrentInternalDisplayRotation() {
  return GetActiveDisplayRotation(display::Display::InternalDisplayId());
}

// static
bool AshTestBase::SupportsMultipleDisplays() {
  return AshTestHelper::SupportsMultipleDisplays();
}

// static
void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  display::test::DisplayManagerTestApi(Shell::GetInstance()->display_manager())
      .UpdateDisplay(display_specs);
}

aura::Window* AshTestBase::CurrentContext() {
  return ash_test_helper_->CurrentContext();
}

// static
std::unique_ptr<views::Widget> AshTestBase::CreateTestWidget(
    views::WidgetDelegate* delegate,
    int container_id,
    const gfx::Rect& bounds) {
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.delegate = delegate;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = bounds;
  WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(widget.get(), container_id,
                                              &params);
  widget->Init(params);
  widget->Show();
  return widget;
}

aura::Window* AshTestBase::CreateTestWindowInShellWithId(int id) {
  return CreateTestWindowInShellWithDelegate(NULL, id, gfx::Rect());
}

aura::Window* AshTestBase::CreateTestWindowInShellWithBounds(
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(NULL, 0, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShell(SkColor color,
                                                   int id,
                                                   const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(
      new aura::test::ColorTestWindowDelegate(color), id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate, ui::wm::WINDOW_TYPE_NORMAL, id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegateAndType(
    aura::WindowDelegate* delegate,
    ui::wm::WindowType type,
    int id,
    const gfx::Rect& bounds) {
  aura::Window* window = new aura::Window(delegate);
  window->set_id(id);
  window->SetType(type);
  window->Init(ui::LAYER_TEXTURED);
  window->Show();

  if (bounds.IsEmpty()) {
    ParentWindowInPrimaryRootWindow(window);
  } else {
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    aura::Window* root = Shell::GetInstance()
                             ->window_tree_host_manager()
                             ->GetRootWindowForDisplayId(display.id());
    gfx::Point origin = bounds.origin();
    ::wm::ConvertPointFromScreen(root, &origin);
    window->SetBounds(gfx::Rect(origin, bounds.size()));
    aura::client::ParentWindowWithContext(window, root, bounds);
  }
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  window->SetProperty(aura::client::kCanMinimizeKey, true);
  return window;
}

void AshTestBase::ParentWindowInPrimaryRootWindow(aura::Window* window) {
  aura::client::ParentWindowWithContext(window, Shell::GetPrimaryRootWindow(),
                                        gfx::Rect());
}

void AshTestBase::RunAllPendingInMessageLoop() {
  ash_test_helper_->RunAllPendingInMessageLoop();
}

TestScreenshotDelegate* AshTestBase::GetScreenshotDelegate() {
  return ash_test_helper_->test_screenshot_delegate();
}

TestSystemTrayDelegate* AshTestBase::GetSystemTrayDelegate() {
  return static_cast<TestSystemTrayDelegate*>(
      WmShell::Get()->system_tray_delegate());
}

void AshTestBase::SetSessionStarted(bool session_started) {
  AshTestHelper::GetTestSessionStateDelegate()->SetActiveUserSessionStarted(
      session_started);
}

void AshTestBase::SetSessionStarting() {
  AshTestHelper::GetTestSessionStateDelegate()->set_session_state(
      session_manager::SessionState::ACTIVE);
}

void AshTestBase::SetUserLoggedIn(bool user_logged_in) {
  AshTestHelper::GetTestSessionStateDelegate()->SetHasActiveUser(
      user_logged_in);
}

void AshTestBase::SetShouldLockScreenAutomatically(bool should_lock) {
  AshTestHelper::GetTestSessionStateDelegate()
      ->SetShouldLockScreenAutomatically(should_lock);
}

void AshTestBase::SetUserAddingScreenRunning(bool user_adding_screen_running) {
  AshTestHelper::GetTestSessionStateDelegate()->SetUserAddingScreenRunning(
      user_adding_screen_running);
}

void AshTestBase::BlockUserSession(UserSessionBlockReason block_reason) {
  switch (block_reason) {
    case BLOCKED_BY_LOCK_SCREEN:
      SetSessionStarted(true);
      SetUserAddingScreenRunning(false);
      WmShell::Get()->GetSessionStateDelegate()->LockScreen();
      Shell::GetInstance()->OnLockStateChanged(true);
      break;
    case BLOCKED_BY_LOGIN_SCREEN:
      SetUserAddingScreenRunning(false);
      SetSessionStarted(false);
      break;
    case BLOCKED_BY_USER_ADDING_SCREEN:
      SetUserAddingScreenRunning(true);
      SetSessionStarted(true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AshTestBase::UnblockUserSession() {
  WmShell::Get()->GetSessionStateDelegate()->UnlockScreen();
  SetSessionStarted(true);
  SetUserAddingScreenRunning(false);
}

void AshTestBase::DisableIME() {
  Shell::GetInstance()->RemovePreTargetHandler(
      Shell::GetInstance()
          ->window_tree_host_manager()
          ->input_method_event_handler());
}

display::DisplayManager* AshTestBase::display_manager() {
  return Shell::GetInstance()->display_manager();
}

bool AshTestBase::TestIfMouseWarpsAt(ui::test::EventGenerator& event_generator,
                                     const gfx::Point& point_in_screen) {
  DCHECK(!Shell::GetInstance()->display_manager()->IsInUnifiedMode());
  static_cast<ExtendedMouseWarpController*>(
      Shell::GetInstance()
          ->mouse_cursor_filter()
          ->mouse_warp_controller_for_test())
      ->allow_non_native_event_for_test();
  display::Screen* screen = display::Screen::GetScreen();
  display::Display original_display =
      screen->GetDisplayNearestPoint(point_in_screen);
  event_generator.MoveMouseTo(point_in_screen);
  return original_display.id() !=
         screen
             ->GetDisplayNearestPoint(
                 aura::Env::GetInstance()->last_mouse_location())
             .id();
}

void AshTestBase::SwapPrimaryDisplay() {
  if (display::Screen::GetScreen()->GetNumDisplays() <= 1)
    return;
  Shell::GetInstance()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager()->GetSecondaryDisplay().id());
}

}  // namespace test
}  // namespace ash
