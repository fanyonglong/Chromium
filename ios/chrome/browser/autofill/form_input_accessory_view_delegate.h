// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_DELEGATE_H_

#import <Foundation/Foundation.h>

// Handles user interaction with a FormInputAccessoryView.
@protocol FormInputAccessoryViewDelegate<NSObject>

// Called when the close button is pressed by the user.
- (void)closeKeyboardWithButtonPress;
// Called to close the keyboard when the user did not press the close button
// directly.
- (void)closeKeyboardWithoutButtonPress;

// Called when the previous button is pressed by the user.
- (void)selectPreviousElementWithButtonPress;
// Called to select the previous element when the user did not press the
// previous button directly.
- (void)selectPreviousElementWithoutButtonPress;

// Called when the next button is pressed by the user.
- (void)selectNextElementWithButtonPress;
// Called to select the next element when the user did not press the next button
// directly.
- (void)selectNextElementWithoutButtonPress;

// Called when updating the keyboard view. Checks if the page contains a next
// and a previous element.
// |completionHandler| is called with 2 BOOLs, the first indicating if a
// previous element was found, and the second indicating if a next element was
// found. |completionHandler| cannot be nil.
- (void)fetchPreviousAndNextElementsPresenceWithCompletionHandler:
        (void (^)(BOOL, BOOL))completionHandler;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_INPUT_ACCESSORY_VIEW_DELEGATE_H_
