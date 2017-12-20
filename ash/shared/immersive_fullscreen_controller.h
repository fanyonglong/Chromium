// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHARED_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
#define ASH_SHARED_IMMERSIVE_FULLSCREEN_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/shared/immersive_revealed_lock.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/pointer_watcher.h"
#include "ui/views/widget/widget_observer.h"

namespace gfx {
class Point;
class SlideAnimation;
}

namespace ui {
class GestureEvent;
class LocatedEvent;
class MouseEvent;
class TouchEvent;
}

namespace views {
class View;
class Widget;
}

namespace ash {

class ImmersiveFocusWatcher;
class ImmersiveFullscreenControllerDelegate;
class ImmersiveFullscreenControllerTestApi;
class ImmersiveGestureHandler;
class WmWindow;

class ASH_EXPORT ImmersiveFullscreenController
    : public gfx::AnimationDelegate,
      public views::PointerWatcher,
      public views::WidgetObserver,
      public ImmersiveRevealedLock::Delegate {
 public:
  // The enum is used for an enumerated histogram. New items should be only
  // added to the end.
  enum WindowType {
    WINDOW_TYPE_OTHER,
    WINDOW_TYPE_BROWSER,
    WINDOW_TYPE_HOSTED_APP,
    WINDOW_TYPE_PACKAGED_APP,
    WINDOW_TYPE_COUNT
  };

  // How many pixels are reserved for touch-events towards the top of an
  // immersive-fullscreen window.
  static const int kImmersiveFullscreenTopEdgeInset;

  // The height in pixels of the region below the top edge of the display in
  // which the mouse can trigger revealing the top-of-window views. The height
  // must be greater than 1px because the top pixel is used to trigger moving
  // the cursor between displays if the user has a vertical display layout
  // (primary display above/below secondary display).
  static const int kMouseRevealBoundsHeight;

  ImmersiveFullscreenController();
  ~ImmersiveFullscreenController() override;

  // Initializes the controller. Must be called prior to enabling immersive
  // fullscreen via SetEnabled(). |top_container| is used to keep the
  // top-of-window views revealed when a child of |top_container| has focus.
  // |top_container| does not affect which mouse and touch events keep the
  // top-of-window views revealed. |widget| is the widget to make fullscreen.
  void Init(ImmersiveFullscreenControllerDelegate* delegate,
            views::Widget* widget,
            views::View* top_container);

  // Enables or disables immersive fullscreen.
  // |window_type| is the type of window which is put in immersive fullscreen.
  // It is only used for histogramming.
  void SetEnabled(WindowType window_type, bool enable);

  // Returns true if in immersive fullscreen.
  bool IsEnabled() const;

  // Returns true if in immersive fullscreen and the top-of-window views are
  // fully or partially visible.
  bool IsRevealed() const;

  // Returns a lock which will keep the top-of-window views revealed for its
  // lifetime. Several locks can be obtained. When all of the locks are
  // destroyed, if immersive fullscreen is enabled and there is nothing else
  // keeping the top-of-window views revealed, the top-of-window views will be
  // closed. This method always returns a valid lock regardless of whether
  // immersive fullscreen is enabled. The lock's lifetime can span immersive
  // fullscreen being enabled / disabled. If acquiring the lock causes a reveal,
  // the top-of-window views will animate according to |animate_reveal|. The
  // caller takes ownership of the returned lock.
  ImmersiveRevealedLock* GetRevealedLock(AnimateReveal animate_reveal)
      WARN_UNUSED_RESULT;

  views::Widget* widget() { return widget_; }
  views::View* top_container() { return top_container_; }

  // TODO(sky): move OnMouseEvent/OnTouchEvent to private section.
  void OnMouseEvent(const ui::MouseEvent& event,
                    const gfx::Point& location_in_screen,
                    views::Widget* target);
  void OnTouchEvent(const ui::TouchEvent& event,
                    const gfx::Point& location_in_screen);
  // Processes a GestureEvent. This may call SetHandled() on the supplied event.
  void OnGestureEvent(ui::GestureEvent* event,
                      const gfx::Point& location_in_screen);

  // views::PointerWatcher:
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              const gfx::Point& location_in_screen,
                              views::Widget* target) override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;

  // gfx::AnimationDelegate overrides:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  // ash::ImmersiveRevealedLock::Delegate overrides:
  void LockRevealedState(AnimateReveal animate_reveal) override;
  void UnlockRevealedState() override;

 private:
  friend class ImmersiveFullscreenControllerTest;
  friend class ImmersiveFullscreenControllerTestApi;

  enum Animate {
    ANIMATE_NO,
    ANIMATE_SLOW,
    ANIMATE_FAST,
  };
  enum RevealState {
    CLOSED,
    SLIDING_OPEN,
    REVEALED,
    SLIDING_CLOSED,
  };
  enum SwipeType { SWIPE_OPEN, SWIPE_CLOSE, SWIPE_NONE };

  // Enables or disables observers for mouse, touch, focus, and activation.
  void EnableWindowObservers(bool enable);

  // Updates |top_edge_hover_timer_| based on a mouse |event|. If the mouse is
  // hovered at the top of the screen the timer is started. If the mouse moves
  // away from the top edge, or moves too much in the x direction, the timer is
  // stopped.
  void UpdateTopEdgeHoverTimer(const ui::MouseEvent& event,
                               const gfx::Point& location_in_screen,
                               views::Widget* target);

  // Updates |located_event_revealed_lock_| based on the current mouse state and
  // the current touch state.
  // |event| is NULL if the source event is not known.
  void UpdateLocatedEventRevealedLock(const ui::LocatedEvent* event,
                                      const gfx::Point& location_in_screen);

  // Convenience for calling two argument version with a null event and looking
  // up the location from the last mouse location.
  void UpdateLocatedEventRevealedLock();

  // Acquires |located_event_revealed_lock_| if it is not already held.
  void AcquireLocatedEventRevealedLock();

  // Updates |focus_revealed_lock_| based on the currently active view and the
  // currently active widget.
  void UpdateFocusRevealedLock();

  // Update |located_event_revealed_lock_| and |focus_revealed_lock_| as a
  // result of a gesture of |swipe_type|. Returns true if any locks were
  // acquired or released.
  bool UpdateRevealedLocksForSwipe(SwipeType swipe_type);

  // Returns the animation duration given |animate|.
  int GetAnimationDuration(Animate animate) const;

  // Temporarily reveals the top-of-window views while in immersive mode,
  // hiding them when the cursor exits the area of the top views. If |animate|
  // is not ANIMATE_NO, slides in the view, otherwise shows it immediately.
  void MaybeStartReveal(Animate animate);

  // Called when the animation to slide open the top-of-window views has
  // completed.
  void OnSlideOpenAnimationCompleted();

  // Hides the top-of-window views if immersive mode is enabled and nothing is
  // keeping them revealed. Optionally animates.
  void MaybeEndReveal(Animate animate);

  // Called when the animation to slide out the top-of-window views has
  // completed.
  void OnSlideClosedAnimationCompleted();

  // Returns the type of swipe given |event|.
  SwipeType GetSwipeType(const ui::GestureEvent& event) const;

  // Returns true if a mouse event at |location_in_screen| should be ignored.
  // Ignored mouse events should not contribute to revealing or unrevealing the
  // top-of-window views.
  bool ShouldIgnoreMouseEventAtLocation(
      const gfx::Point& location_in_screen) const;

  // True when |location| is "near" to the top container. When the top container
  // is not closed "near" means within the displayed bounds or above it. When
  // the top container is closed "near" means either within the displayed
  // bounds, above it, or within a few pixels below it. This allow the container
  // to steal enough pixels to detect a swipe in and handles the case that there
  // is a bezel sensor above the top container.
  bool ShouldHandleGestureEvent(const gfx::Point& location) const;

  // Returns the display bounds of the screen |widget_| is on.
  gfx::Rect GetDisplayBoundsInScreen() const;

  // Not owned.
  ImmersiveFullscreenControllerDelegate* delegate_;
  views::View* top_container_;
  views::Widget* widget_;
  // The WmWindow for |widget_|.
  WmWindow* widget_window_ = nullptr;

  // True if the observers have been enabled.
  bool observers_enabled_;

  // True when in immersive fullscreen.
  bool enabled_;

  // State machine for the revealed/closed animations.
  RevealState reveal_state_;

  int revealed_lock_count_;

  // Timer to track cursor being held at the top edge of the screen.
  base::OneShotTimer top_edge_hover_timer_;

  // The cursor x position in screen coordinates when the cursor first hit the
  // top edge of the screen.
  int mouse_x_when_hit_top_in_screen_;

  // Tracks if the controller has seen a ET_GESTURE_SCROLL_BEGIN, without the
  // following events.
  bool gesture_begun_;

  // Lock which keeps the top-of-window views revealed based on the current
  // mouse state and the current touch state. Acquiring the lock is used to
  // trigger a reveal when the user moves the mouse to the top of the screen
  // and when the user does a SWIPE_OPEN edge gesture.
  std::unique_ptr<ImmersiveRevealedLock> located_event_revealed_lock_;

  // The animation which controls sliding the top-of-window views in and out.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Whether the animations are disabled for testing.
  bool animations_disabled_for_test_;

  std::unique_ptr<ImmersiveFocusWatcher> immersive_focus_watcher_;
  std::unique_ptr<ImmersiveGestureHandler> immersive_gesture_handler_;

  base::WeakPtrFactory<ImmersiveFullscreenController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFullscreenController);
};

}  // namespace ash

#endif  // ASH_SHARED_IMMERSIVE_FULLSCREEN_CONTROLLER_H_
