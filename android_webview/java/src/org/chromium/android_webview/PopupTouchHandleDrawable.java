// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.SystemClock;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.WindowManager;
import android.view.animation.AnimationUtils;
import android.widget.PopupWindow;

import org.chromium.base.ObserverList;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.content.browser.ContentViewCore;
import org.chromium.content.browser.PositionObserver;
import org.chromium.content.browser.ViewPositionObserver;
import org.chromium.content.browser.input.HandleViewResources;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.ui.touch_selection.TouchHandleOrientation;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * View that displays a selection or insertion handle for text editing.
 *
 * While a HandleView is logically a child of some other view, it does not exist in that View's
 * hierarchy.
 *
 */
@JNINamespace("android_webview")
public class PopupTouchHandleDrawable extends View {
    private final PopupWindow mContainer;
    private final PositionObserver.Listener mParentPositionListener;
    private ContentViewCore mContentViewCore;
    private PositionObserver mParentPositionObserver;
    private Drawable mDrawable;

    // The native side of this object.
    private final long mNativeDrawable;

    // The position of the handle relative to the parent view in DIP.
    private float mOriginXDip;
    private float mOriginYDip;

    // The position of the parent view relative to the application's root view in pixels.
    private int mParentPositionX;
    private int mParentPositionY;

    // The mirror values based on which the handles are inverted about X and Y axes.
    private boolean mMirrorHorizontal;
    private boolean mMirrorVertical;

    private float mAlpha;

    private final int[] mTempScreenCoords = new int[2];

    private int mOrientation = TouchHandleOrientation.UNDEFINED;

    // Length of the delay before fading in after the last page movement.
    private static final int MOVING_FADE_IN_DELAY_MS = 300;
    private static final int FADE_IN_DURATION_MS = 200;
    private Runnable mDeferredHandleFadeInRunnable;
    private long mFadeStartTime;
    private Runnable mTemporarilyHiddenExpiredRunnable;
    private long mTemporarilyHiddenExpireTime;
    private boolean mVisible;
    private boolean mScrolling;
    private boolean mFocused;
    private boolean mTemporarilyHidden;
    private boolean mAttachedToWindow;
    // This should be set only from onVisibilityInputChanged.
    private boolean mWasShowingAllowed;

    // Gesture accounting for handle hiding while scrolling.
    private final GestureStateListener mGestureStateListener;

    // There are no guarantees that the side effects of setting the position of
    // the PopupWindow and the visibility of its content View will be realized
    // in the same frame. Thus, to ensure the PopupWindow is seen in the right
    // location, when the PopupWindow reappears we delay the visibility update
    // by one frame after setting the position.
    private boolean mDelayVisibilityUpdateWAR;

    // Deferred runnable to avoid invalidating outside of frame dispatch,
    // in turn avoiding issues with sync barrier insertion.
    private Runnable mInvalidationRunnable;
    private boolean mHasPendingInvalidate;

    // List of drawables used to inform them of the container view switching.
    private final ObserverList<PopupTouchHandleDrawable> mDrawableObserverList;

    private PopupTouchHandleDrawable(ObserverList<PopupTouchHandleDrawable> drawableObserverList,
            ContentViewCore contentViewCore) {
        super(contentViewCore.getContainerView().getContext());
        mDrawableObserverList = drawableObserverList;
        mDrawableObserverList.addObserver(this);

        mContentViewCore = contentViewCore;
        mContainer = new PopupWindow(mContentViewCore.getWindowAndroid().getContext().get(),
                null, android.R.attr.textSelectHandleWindowStyle);
        mContainer.setSplitTouchEnabled(true);
        mContainer.setClippingEnabled(false);

        // The built-in PopupWindow animation causes jank when transitioning between
        // visibility states. We use a custom fade-in animation when necessary.
        mContainer.setAnimationStyle(0);

        // The SUB_PANEL window layout type improves z-ordering with respect to
        // other popup-based elements.
        setWindowLayoutType(mContainer, WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL);
        mContainer.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mContainer.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);

        mAlpha = 1.f;
        mVisible = getVisibility() == VISIBLE;
        mFocused = mContentViewCore.getContainerView().hasWindowFocus();

        mParentPositionObserver = new ViewPositionObserver(mContentViewCore.getContainerView());
        mParentPositionListener = new PositionObserver.Listener() {
            @Override
            public void onPositionChanged(int x, int y) {
                updateParentPosition(x, y);
            }
        };
        mGestureStateListener = new GestureStateListener() {
            @Override
            public void onScrollStarted(int scrollOffsetX, int scrollOffsetY) {
                setIsScrolling(true);
            }
            @Override
            public void onScrollEnded(int scrollOffsetX, int scrollOffsetY) {
                setIsScrolling(false);
            }
            @Override
            public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
                // Fling accounting is unreliable in WebView, as the embedder
                // can override onScroll() and suppress fling ticking. At best
                // we have to rely on the scroll offset changing to temporarily
                // and repeatedly keep the handles hidden.
                setIsScrolling(false);
            }
            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                temporarilyHide();
            }
            @Override
            public void onWindowFocusChanged(boolean hasWindowFocus) {
                setIsFocused(hasWindowFocus);
            }
            @Override
            public void onDestroyed() {
                destroy();
            }
        };
        mContentViewCore.addGestureStateListener(mGestureStateListener);
        mNativeDrawable = nativeInit(HandleViewResources.getHandleHorizontalPaddingRatio());
    }

    public static PopupTouchHandleDrawable create(
            ObserverList<PopupTouchHandleDrawable> drawableObserverList,
            ContentViewCore contentViewCore) {
        return new PopupTouchHandleDrawable(drawableObserverList, contentViewCore);
    }

    public long getNativeDrawable() {
        return mNativeDrawable;
    }

    private static void setWindowLayoutType(PopupWindow window, int layoutType) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            window.setWindowLayoutType(layoutType);
            return;
        }

        try {
            Method setWindowLayoutTypeMethod =
                    PopupWindow.class.getMethod("setWindowLayoutType", int.class);
            setWindowLayoutTypeMethod.invoke(window, layoutType);
        } catch (NoSuchMethodException | IllegalAccessException | InvocationTargetException
                | RuntimeException e) {
        }
    }

    private static Drawable getHandleDrawable(Context context, int orientation) {
        switch (orientation) {
            case TouchHandleOrientation.LEFT: {
                return HandleViewResources.getLeftHandleDrawable(context);
            }

            case TouchHandleOrientation.RIGHT: {
                return HandleViewResources.getRightHandleDrawable(context);
            }

            case TouchHandleOrientation.CENTER: {
                return HandleViewResources.getCenterHandleDrawable(context);
            }

            case TouchHandleOrientation.UNDEFINED:
            default:
                assert false;
                return HandleViewResources.getCenterHandleDrawable(context);
        }
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mContentViewCore == null) return false;
        // Convert from PopupWindow local coordinates to
        // parent view local coordinates prior to forwarding.
        mContentViewCore.getContainerView().getLocationOnScreen(mTempScreenCoords);
        final float offsetX = event.getRawX() - event.getX() - mTempScreenCoords[0];
        final float offsetY = event.getRawY() - event.getY() - mTempScreenCoords[1];
        final MotionEvent offsetEvent = MotionEvent.obtainNoHistory(event);
        offsetEvent.offsetLocation(offsetX, offsetY);
        final boolean handled = mContentViewCore.onTouchHandleEvent(offsetEvent);
        offsetEvent.recycle();
        return handled;
    }

    @CalledByNative
    private void setOrientation(int orientation, boolean mirrorVertical, boolean mirrorHorizontal) {
        assert (orientation >= TouchHandleOrientation.LEFT
                && orientation <= TouchHandleOrientation.UNDEFINED);

        final boolean orientationChanged = mOrientation != orientation;
        final boolean mirroringChanged =
                mMirrorHorizontal != mirrorHorizontal || mMirrorVertical != mirrorVertical;
        mOrientation = orientation;
        mMirrorHorizontal = mirrorHorizontal;
        mMirrorVertical = mirrorVertical;

        // Create new InvertedDrawable only if orientation has changed.
        // Otherwise, change the mirror values to scale canvas on draw() calls.
        if (orientationChanged) mDrawable = getHandleDrawable(getContext(), mOrientation);

        if (mDrawable != null) mDrawable.setAlpha((int) (255 * mAlpha));

        if (orientationChanged || mirroringChanged) scheduleInvalidate();
    }

    private void updateParentPosition(int parentPositionX, int parentPositionY) {
        if (mParentPositionX == parentPositionX && mParentPositionY == parentPositionY) return;
        mParentPositionX = parentPositionX;
        mParentPositionY = parentPositionY;
        temporarilyHide();
    }

    private int getContainerPositionX() {
        final float deviceScale = mContentViewCore.getDeviceScaleFactor();
        return mParentPositionX + (int) (mOriginXDip * deviceScale);
    }

    private int getContainerPositionY() {
        final float deviceScale = mContentViewCore.getDeviceScaleFactor();
        return mParentPositionY + (int) (mOriginYDip * deviceScale);
    }

    private void updatePosition() {
        mContainer.update(getContainerPositionX(), getContainerPositionY(),
                getRight() - getLeft(), getBottom() - getTop());
    }

    private boolean isShowingAllowed() {
        final float deviceScale = mContentViewCore.getDeviceScaleFactor();
        // mOriginX/YDip * deviceScale is the distance of the handler drawable from the top left of
        // the containerView (the WebView).
        return mAttachedToWindow && mVisible && mFocused && !mScrolling && !mTemporarilyHidden
                && isPositionVisible(mOriginXDip * deviceScale, mOriginYDip * deviceScale);
    }

    // Adapted from android.widget.Editor#isPositionVisible.
    private boolean isPositionVisible(final float positionX, final float positionY) {
        final float[] position = new float[2];
        position[0] = positionX;
        position[1] = positionY;
        View view = mContentViewCore.getContainerView();

        while (view != null) {
            if (view != mContentViewCore.getContainerView()) {
                // Local scroll is already taken into account in positionX/Y
                position[0] -= view.getScrollX();
                position[1] -= view.getScrollY();
            }

            final float drawableWidth = mDrawable.getIntrinsicWidth();
            final float drawableHeight = mDrawable.getIntrinsicHeight();

            if (position[0] + drawableWidth < 0 || position[1] + drawableHeight < 0
                    || position[0] > view.getWidth() || position[1] > view.getHeight()) {
                return false;
            }

            if (!view.getMatrix().isIdentity()) {
                view.getMatrix().mapPoints(position);
            }

            position[0] += view.getLeft();
            position[1] += view.getTop();

            final ViewParent parent = view.getParent();
            if (parent instanceof View) {
                view = (View) parent;
            } else {
                // We've reached the ViewRoot, stop iterating
                view = null;
            }
        }

        // We've been able to walk up the view hierarchy and the position was never clipped
        return true;
    }

    private void updateVisibility() {
        int newVisibility = isShowingAllowed() ? VISIBLE : INVISIBLE;

        // When regaining visibility, delay the visibility update by one frame
        // to ensure the PopupWindow has first been positioned properly.
        if (newVisibility == VISIBLE && getVisibility() != VISIBLE) {
            if (!mDelayVisibilityUpdateWAR) {
                mDelayVisibilityUpdateWAR = true;
                scheduleInvalidate();
                return;
            }
        }
        mDelayVisibilityUpdateWAR = false;

        setVisibility(newVisibility);
    }

    private void setIsScrolling(boolean scrolling) {
        if (mScrolling == scrolling) return;
        mScrolling = scrolling;
        onVisibilityInputChanged();
    }

    private void setIsFocused(boolean focused) {
        if (mFocused == focused) return;
        mFocused = focused;
        onVisibilityInputChanged();
    }

    private void setTemporarilyHidden(boolean hidden) {
        if (mTemporarilyHidden == hidden) return;

        mTemporarilyHidden = hidden;
        if (mTemporarilyHidden) {
            if (mTemporarilyHiddenExpiredRunnable == null) {
                mTemporarilyHiddenExpiredRunnable = new Runnable() {
                    @Override
                    public void run() {
                        setTemporarilyHidden(false);
                    }
                };
            }
            removeCallbacks(mTemporarilyHiddenExpiredRunnable);
            long now = SystemClock.uptimeMillis();
            long delay = Math.max(0, mTemporarilyHiddenExpireTime - now);
            postDelayed(mTemporarilyHiddenExpiredRunnable, delay);
        } else if (mTemporarilyHiddenExpiredRunnable != null) {
            removeCallbacks(mTemporarilyHiddenExpiredRunnable);
        }
        onVisibilityInputChanged();
    }

    private void onVisibilityInputChanged() {
        if (!mContainer.isShowing()) return;
        boolean allowed = isShowingAllowed();
        if (mWasShowingAllowed == allowed) return;
        mWasShowingAllowed = allowed;
        cancelFadeIn();
        if (allowed) {
            if (mDeferredHandleFadeInRunnable == null) {
                mDeferredHandleFadeInRunnable = new Runnable() {
                    @Override
                    public void run() {
                        beginFadeIn();
                    }
                };
            }
            postOnAnimation(mDeferredHandleFadeInRunnable);
        } else {
            updateVisibility();
        }
    }

    private void updateAlpha() {
        if (mAlpha == 1.f) return;
        long currentTimeMillis = AnimationUtils.currentAnimationTimeMillis();
        mAlpha = Math.min(1.f, (float) (currentTimeMillis - mFadeStartTime) / FADE_IN_DURATION_MS);
        mDrawable.setAlpha((int) (255 * mAlpha));
        scheduleInvalidate();
    }

    private void temporarilyHide() {
        if (!mContainer.isShowing()) return;
        mTemporarilyHiddenExpireTime = SystemClock.uptimeMillis() + MOVING_FADE_IN_DELAY_MS;
        setTemporarilyHidden(true);
    }

    private void doInvalidate() {
        if (!mContainer.isShowing()) return;
        updateVisibility();
        updatePosition();
        invalidate();
    }

    private void scheduleInvalidate() {
        if (mInvalidationRunnable == null) {
            mInvalidationRunnable = new Runnable() {
                @Override
                public void run() {
                    mHasPendingInvalidate = false;
                    doInvalidate();
                }
            };
        }

        if (mHasPendingInvalidate) return;
        mHasPendingInvalidate = true;
        postOnAnimation(mInvalidationRunnable);
    }

    private void cancelFadeIn() {
        if (mDeferredHandleFadeInRunnable == null) return;
        removeCallbacks(mDeferredHandleFadeInRunnable);
    }

    private void beginFadeIn() {
        if (getVisibility() == VISIBLE) return;
        mAlpha = 0.f;
        mFadeStartTime = AnimationUtils.currentAnimationTimeMillis();
        doInvalidate();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDrawable == null) {
            setMeasuredDimension(0, 0);
            return;
        }
        setMeasuredDimension(mDrawable.getIntrinsicWidth(), mDrawable.getIntrinsicHeight());
    }

    @Override
    protected void onDraw(Canvas c) {
        if (mDrawable == null) return;
        final boolean needsMirror = mMirrorHorizontal || mMirrorVertical;
        if (needsMirror) {
            c.save();
            float scaleX = mMirrorHorizontal ? -1.f : 1.f;
            float scaleY = mMirrorVertical ? -1.f : 1.f;
            c.scale(scaleX, scaleY, getWidth() / 2.0f, getHeight() / 2.0f);
        }
        updateAlpha();
        mDrawable.setBounds(0, 0, getRight() - getLeft(), getBottom() - getTop());
        mDrawable.draw(c);
        if (needsMirror) c.restore();
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mAttachedToWindow = true;
        onVisibilityInputChanged();
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        mAttachedToWindow = false;
        onVisibilityInputChanged();
    }

    @CalledByNative
    private void destroy() {
        mDrawableObserverList.removeObserver(this);
        if (mContentViewCore == null) return;
        hide();
        mContentViewCore.removeGestureStateListener(mGestureStateListener);
        mContentViewCore = null;
    }

    @CalledByNative
    private void show() {
        if (mContentViewCore == null) return;
        if (mContainer.isShowing()) return;

        // While hidden, the parent position may have become stale. It must be updated before
        // checking isPositionVisible().
        updateParentPosition(mParentPositionObserver.getPositionX(),
                mParentPositionObserver.getPositionY());
        mParentPositionObserver.addListener(mParentPositionListener);
        mContainer.setContentView(this);
        try {
            mContainer.showAtLocation(mContentViewCore.getContainerView(), Gravity.NO_GRAVITY,
                    getContainerPositionX(), getContainerPositionY());
        } catch (WindowManager.BadTokenException e) {
            hide();
        }
    }

    @CalledByNative
    private void hide() {
        mTemporarilyHiddenExpireTime = 0;
        setTemporarilyHidden(false);
        mAlpha = 1.0f;
        if (mContainer.isShowing()) {
            try {
                mContainer.dismiss();
            } catch (IllegalArgumentException e) {
                // Intentionally swallowed due to bad Android implemention. See crbug.com/633224.
            }
        }
        mParentPositionObserver.clearListener();
    }

    @CalledByNative
    private void setOrigin(float originXDip, float originYDip) {
        if (mOriginXDip == originXDip && mOriginYDip == originYDip) return;
        mOriginXDip = originXDip;
        mOriginYDip = originYDip;
        if (getVisibility() == VISIBLE) scheduleInvalidate();
    }

    @CalledByNative
    private void setVisible(boolean visible) {
        if (mVisible == visible) return;
        mVisible = visible;
        onVisibilityInputChanged();
    }

    @CalledByNative
    private float getOriginXDip() {
        return mOriginXDip;
    }

    @CalledByNative
    private float getOriginYDip() {
        return mOriginYDip;
    }

    @CalledByNative
    private float getVisibleWidthDip() {
        if (mDrawable == null) return 0;
        if (mContentViewCore == null) return 0;
        return mDrawable.getIntrinsicWidth() / mContentViewCore.getDeviceScaleFactor();
    }

    @CalledByNative
    private float getVisibleHeightDip() {
        if (mDrawable == null) return 0;
        if (mContentViewCore == null) return 0;
        return mDrawable.getIntrinsicHeight() / mContentViewCore.getDeviceScaleFactor();
    }

    public void onContainerViewChanged(ViewGroup newContainerView) {
        // If the parent View ever changes, the parent position observer
        // must be updated accordingly.
        mParentPositionObserver.clearListener();
        mParentPositionObserver = new ViewPositionObserver(newContainerView);
        if (mContainer.isShowing()) {
            mParentPositionObserver.addListener(mParentPositionListener);
        }
    }

    private native long nativeInit(float horizontalPaddingRatio);
}
