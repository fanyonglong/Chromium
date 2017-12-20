// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_wk_script_message_router.h"

#import "base/logging.h"
#import "base/mac/scoped_nsobject.h"

@interface CRWWKScriptMessageRouter ()<WKScriptMessageHandler>

// Removes a specific message handler. Does nothing if handler does not exist.
- (void)tryRemoveScriptMessageHandlerForName:(NSString*)messageName
                                     webView:(WKWebView*)webView;

@end

@implementation CRWWKScriptMessageRouter {
  // Two level map of registed message handlers. Keys are message names and
  // values are more maps (where keys are web views and values are handlers).
  base::scoped_nsobject<NSMutableDictionary> _handlers;
  // Wrapped WKUserContentController.
  base::scoped_nsobject<WKUserContentController> _userContentController;
}

#pragma mark -
#pragma mark Interface

- (WKUserContentController*)userContentController {
  return _userContentController.get();
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithUserContentController:
    (WKUserContentController*)userContentController {
  DCHECK(userContentController);
  if ((self = [super init])) {
    _handlers.reset([[NSMutableDictionary alloc] init]);
    _userContentController.reset([userContentController retain]);
  }
  return self;
}

- (void)setScriptMessageHandler:(void (^)(WKScriptMessage*))handler
                           name:(NSString*)messageName
                        webView:(WKWebView*)webView {
  DCHECK(handler);
  DCHECK(messageName);
  DCHECK(webView);

  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:messageName];
  if (!webViewToHandlerMap) {
    webViewToHandlerMap =
        [NSMapTable mapTableWithKeyOptions:NSPointerFunctionsStrongMemory
                              valueOptions:NSPointerFunctionsCopyIn];
    [_handlers setObject:webViewToHandlerMap forKey:messageName];
    [_userContentController addScriptMessageHandler:self name:messageName];
  }
  DCHECK(![webViewToHandlerMap objectForKey:webView]);
  [webViewToHandlerMap setObject:handler forKey:webView];
}

- (void)removeScriptMessageHandlerForName:(NSString*)messageName
                                  webView:(WKWebView*)webView {
  DCHECK(messageName);
  DCHECK(webView);
  DCHECK([[_handlers objectForKey:messageName] objectForKey:webView]);
  [self tryRemoveScriptMessageHandlerForName:messageName webView:webView];
}

- (void)removeAllScriptMessageHandlersForWebView:(WKWebView*)webView {
  DCHECK(webView);
  for (NSString* messageName in [_handlers allKeys]) {
    [self tryRemoveScriptMessageHandlerForName:messageName webView:webView];
  }
}

#pragma mark -
#pragma mark WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:message.name];
  DCHECK(webViewToHandlerMap);
  id handler = [webViewToHandlerMap objectForKey:message.webView];
  if (handler) {
    // Web process can send messages even if web view was reset and
    // script message handler has been removed from the router.
    ((void (^)(WKScriptMessage*))handler)(message);
  }
}

#pragma mark -
#pragma mark Implementation

- (void)tryRemoveScriptMessageHandlerForName:(NSString*)messageName
                                     webView:(WKWebView*)webView {
  NSMapTable* webViewToHandlerMap = [_handlers objectForKey:messageName];
  id handler = [webViewToHandlerMap objectForKey:webView];
  if (!handler)
    return;
  // Extend the lifetime of |handler| so removeScriptMessageHandlerForName: can
  // be called from inside of |handler|.
  [[handler retain] autorelease];
  if (webViewToHandlerMap.count == 1) {
    [_handlers removeObjectForKey:messageName];
    [_userContentController removeScriptMessageHandlerForName:messageName];
  } else {
    [webViewToHandlerMap removeObjectForKey:webView];
  }
}

@end
