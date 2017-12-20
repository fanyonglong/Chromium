// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Chrome window that hosts UI. Only one window is allowed.
 * @type {chrome.app.window.AppWindow}
 */
var appWindow = null;

/**
 * Contains Web content provided by Google authorization server.
 * @type {WebView}
 */
var lsoView = null;

/** @type {TermsOfServicePage} */
var termsPage = null;

/**
 * Used for bidirectional communication with native code.
 * @type {chrome.runtime.Port}
 */
var port = null;

/**
 * Stores current device id.
 * @type {string}
 */
var currentDeviceId = null;

/**
 * Host window inner default width.
 * @const {number}
 */
var INNER_WIDTH = 960;

/**
 * Host window inner default height.
 * @const {number}
 */
var INNER_HEIGHT = 688;


/**
 * Sends a native message to ArcSupportHost.
 * @param {string} event The event type in message.
 * @param {Object=} opt_props Extra properties for the message.
 */
function sendNativeMessage(event, opt_props) {
  var message = Object.assign({'event': event}, opt_props);
  port.postMessage(message);
}

/**
 * Class to handle checkbox corresponding to a preference.
 */
class PreferenceCheckbox {

  /**
   * Creates a Checkbox which handles the corresponding preference update.
   * @param {Element} container The container this checkbox corresponds to.
   *     The element must have <input type="checkbox" class="checkbox-option">
   *     for the checkbox itself, and <p class="checkbox-text"> for its label.
   * @param {string} learnMoreContent I18n content which is shown when "Learn
   *     More" link is clicked.
   * @param {string?} learnMoreLinkId The ID for the "Learn More" link element.
   *     TODO: Get rid of this. The element can have class so that it can be
   *     identified easily. Also, it'd be better to extract the link element
   *     (tag) from the i18n text, and let i18n focus on the content.
   * @param {string?} policyText The content of the policy indicator.
   */
  constructor(container, learnMoreContent, learnMoreLinkId, policyText) {
    this.container_ = container;
    this.learnMoreContent_ = learnMoreContent;

    this.checkbox_ = container.querySelector('.checkbox-option');
    this.label_ = container.querySelector('.checkbox-text');

    var learnMoreLink = this.label_.querySelector(learnMoreLinkId);
    if (learnMoreLink) {
      learnMoreLink.addEventListener(
          'click', (event) => this.onLearnMoreLinkClicked(event));
    }

    // Create controlled indicator for policy if necessary.
    if (policyText) {
      this.policyIndicator_ =
          new appWindow.contentWindow.cr.ui.ControlledIndicator();
      this.policyIndicator_.setAttribute('textpolicy', policyText);
      // TODO: better to have a dedicated element for this place.
      this.label_.insertBefore(this.policyIndicator_, learnMoreLink);
    } else {
      this.policyIndicator_ = null;
    }
  }

  /**
   * Returns if the checkbox is checked or not. Note that this *may* be
   * different from the preference value, because the user's check is
   * not propagated to the preference until the user clicks "AGREE" button.
   */
  isChecked() { return this.checkbox_.checked; }

  /**
   * Called when the preference value in native code is updated.
   */
  onPreferenceChanged(isEnabled, isManaged) {
    this.checkbox_.checked = isEnabled;
    this.checkbox_.disabled = isManaged;
    this.label_.disabled = isManaged;

    if (this.policyIndicator_) {
      if (isManaged) {
        this.policyIndicator_.setAttribute('controlled-by', 'policy');
      } else {
        this.policyIndicator_.removeAttribute('controlled-by');
      }
    }
  }

  /**
   * Called when the "Learn More" link is clicked.
   */
  onLearnMoreLinkClicked() {
    showTextOverlay(this.learnMoreContent_);
  }
};

/**
 * Handles the checkbox action of metrics preference.
 * This has special customization e.g. show/hide the checkbox based on
 * the native preference.
 */
class MetricsPreferenceCheckbox extends PreferenceCheckbox {
  constructor(
      container, learnMoreContent, learnMoreLinkId, isOwner,
      textDisabled, textEnabled, textManagedDisabled, textManagedEnabled) {
    // Do not use policy indicator.
    // Learn More link handling is done by this class.
    // So pass |null| intentionally.
    super(container, learnMoreContent, null, null);

    this.learnMoreLinkId_ = learnMoreLinkId;
    this.isOwner_ = isOwner;

    // Two dimensional array. First dimension is whether it is managed or not,
    // the second one is whether it is enabled or not.
    this.texts_ = [
        [textDisabled, textEnabled],
        [textManagedDisabled, textManagedEnabled]
    ];
  }

  onPreferenceChanged(isEnabled, isManaged) {
    isManaged = isManaged || !this.isOwner_;
    super.onPreferenceChanged(isEnabled, isManaged);

    // Hide the checkbox if it is not allowed to (re-)enable.
    var canEnable = !isEnabled && !isManaged;
    this.checkbox_.hidden = !canEnable;

    // Update the label.
    this.label_.innerHTML = this.texts_[isManaged ? 1 : 0][isEnabled ? 1 : 0];

    // Work around for the current translation text.
    // The translation text has tags for following links, although those
    // tags are not the target of the translation (but those content text is
    // the translation target).
    // So, meanwhile, we set the link everytime we update the text.
    // TODO: fix the translation text, and main html.
    var learnMoreLink = this.label_.querySelector(this.learnMoreLinkId_);
    learnMoreLink.addEventListener(
        'click', (event) => this.onLearnMoreLinkClicked(event));
    var settingsLink = this.label_.querySelector('#settings-link');
    settingsLink.addEventListener(
        'click', (event) => this.onSettingsLinkClicked(event));
  }

  /** Called when "settings" link is clicked. */
  onSettingsLinkClicked(event) {
    chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
    event.preventDefault();
  }
};

/**
 * Represents the page loading state.
 * @enum {number}
 */
var LoadState = {
  UNLOADED: 0,
  LOADING: 1,
  ABORTED: 2,
  LOADED: 3,
};

/**
 * Handles events for Terms-Of-Service page. Also this implements the async
 * loading of Terms-Of-Service content.
 */
class TermsOfServicePage {

  /**
   * @param {Element} container The container of the page.
   * @param {boolean} isManaged Set true if ARC is managed.
   * @param {string} countryCode The country code for the terms of service.
   * @param {MetricsPreferenceCheckbox} metricsCheckbox. The checkbox for the
   *     metrics preference.
   * @param {PreferenceCheckbox} backupRestoreCheckbox The checkbox for the
   *     backup-restore preference.
   * @param {PreferenceCheckbox} locationServiceCheckbox The checkbox for the
   *     location service.
   */
  constructor(
      container, isManaged, countryCode,
      metricsCheckbox, backupRestoreCheckbox, locationServiceCheckbox) {
    this.loadingContainer_ =
        container.querySelector('#terms-of-service-loading');
    this.contentContainer_ =
        container.querySelector('#terms-of-service-content');

    this.metricsCheckbox_ = metricsCheckbox;
    this.backupRestoreCheckbox_ = backupRestoreCheckbox;
    this.locationServiceCheckbox_ = locationServiceCheckbox;

    this.isManaged_ = isManaged;

    // Set event listener for webview loading.
    this.termsView_ = container.querySelector('#terms-view');
    this.termsView_.addEventListener(
        'loadstart', () => this.onTermsViewLoadStarted_());
    this.termsView_.addEventListener(
        'contentload', () => this.onTermsViewLoaded_());
    this.termsView_.addEventListener(
        'loadabort', (event) => this.onTermsViewLoadAborted_(event.reason));

    var scriptSetCountryCode =
        'document.countryCode = \'' + countryCode.toLowerCase() + '\';';
    this.termsView_.addContentScripts([
      { name: 'preProcess',
        matches: ['https://play.google.com/*'],
        js: { code: scriptSetCountryCode },
        run_at: 'document_start'
      },
      { name: 'postProcess',
        matches: ['https://play.google.com/*'],
        css: { files: ['playstore.css'] },
        js: { files: ['playstore.js'] },
        run_at: 'document_end'
      }]);

    // webview is not allowed to open links in the new window. Hook these
    // events and open links in overlay dialog.
    this.termsView_.addEventListener('newwindow', function(event) {
      event.preventDefault();
      showURLOverlay(event.targetUrl);
    });
    this.state_ = LoadState.UNLOADED;

    // On managed case, do not show TermsOfService section. Note that the
    // checkbox for the prefereces are still visible.
    var visibility = isManaged ? 'hidden' : 'visible';
    container.querySelector('#terms-title').style.visibility = visibility;
    container.querySelector('#terms-container').style.visibility = visibility;

    // Set event handler for buttons.
    container.querySelector('#button-agree')
        .addEventListener('click', () => this.onAgree());
    container.querySelector('#button-cancel')
        .addEventListener('click', () => this.onCancel_());
  }

  /** Called when the TermsOfService page is shown. */
  onShow() {
    if (this.isManaged_ || this.state_ == LoadState.LOADED) {
      // Note: in managed case, because it does not show the contents of terms
      // of service, it is ok to show the content container immediately.
      this.showContent_();
    } else {
      this.startTermsViewLoading_();
    }
  }

  /** Shows the loaded terms-of-service content. */
  showContent_() {
    this.loadingContainer_.hidden = true;
    this.contentContainer_.hidden = false;
    this.updateTermsHeight_();
  }

  /**
   * Updates terms view height manually because webview is not automatically
   * resized in case parent div element gets resized.
   */
  updateTermsHeight_() {
    // Update the height in next cycle to prevent webview animation and
    // wrong layout caused by whole-page layout change.
    setTimeout(function() {
      var doc = appWindow.contentWindow.document;
      // Reset terms-view height in order to stabilize style computation. For
      // some reason, child webview affects final result.
      this.termsView_.style.height = '0px';
      var termsContainer =
          this.contentContainer_.querySelector('#terms-container');
      var style = window.getComputedStyle(termsContainer, null);
      this.termsView_.style.height = style.getPropertyValue('height');
    }.bind(this), 0);
  }

  /** Starts to load the terms of service webview content. */
  startTermsViewLoading_() {
    if (this.state_ == LoadState.LOADING) {
      // If there already is inflight loading task, do nothing.
      return;
    }

    if (this.termsView_.src) {
      // This is reloading the page, typically clicked RETRY on error page.
      this.termsView_.reload();
    } else {
      // This is first loading case so set the URL explicitly.
      this.termsView_.src = 'https://play.google.com/about/play-terms.html';
    }
  }

  /** Called when the terms-view starts to be loaded. */
  onTermsViewLoadStarted_() {
    // Note: Reloading can be triggered by user action. E.g., user may select
    // their language by selection at the bottom of the Terms Of Service
    // content.
    this.state_ = LoadState.LOADING;
    // Show loading page.
    this.loadingContainer_.hidden = false;
    this.contentContainer_.hidden = true;
  }

  /** Called when the terms-view is loaded. */
  onTermsViewLoaded_() {
    // This is called also when the loading is failed.
    // In such a case, onTermsViewLoadAborted_() is called in advance, and
    // state_ is set to ABORTED. Here, switch the view only for the
    // successful loading case.
    if (this.state_ == LoadState.LOADING) {
      this.state_ = LoadState.LOADED;
      this.showContent_();
    }
  }

  /** Called when the terms-view loading is aborted. */
  onTermsViewLoadAborted_(reason) {
    console.error('TermsView loading is aborted: ' + reason);
    // Mark ABORTED so that onTermsViewLoaded_() won't show the content view.
    this.state_ = LoadState.ABORTED;
    showErrorPage(
        appWindow.contentWindow.loadTimeData.getString('serverError'));
  }

  /** Called when "AGREE" button is clicked. */
  onAgree() {
    sendNativeMessage('onAgreed', {
      isMetricsEnabled: this.metricsCheckbox_.isChecked(),
      isBackupRestoreEnabled: this.backupRestoreCheckbox_.isChecked(),
      isLocationServiceEnabled: this.locationServiceCheckbox_.isChecked()
    });
  }

  /** Called when "CANCEL" button is clicked. */
  onCancel_() {
    if (appWindow) {
      appWindow.close();
    }
  }

  /** Called when metrics preference is updated. */
  onMetricxPreferenceChanged(isEnabled, isManaged) {
    this.metricsCheckbox_.onPreferenceChanged(isEnabled, isManaged);

    // Applying metrics mode may change page layout, update terms height.
    this.updateTermsHeight_();
  }

  /** Called when backup-restore preference is updated. */
  onBackupRestorePreferenceChanged(isEnabled, isManaged) {
    this.backupRestoreCheckbox_.onPreferenceChanged(isEnabled, isManaged);
  }

  /** Called when location service preference is updated. */
  onLocationServicePreferenceChanged(isEnabled, isManaged) {
    this.locationServiceCheckbox_.onPreferenceChanged(isEnabled, isManaged);
  }
};

/**
 * Applies localization for html content and sets terms webview.
 * @param {!Object} data Localized strings and relevant information.
 * @param {string} deviceId Current device id.
 */
function initialize(data, deviceId) {
  currentDeviceId = deviceId;
  var doc = appWindow.contentWindow.document;
  var loadTimeData = appWindow.contentWindow.loadTimeData;
  loadTimeData.data = data;
  appWindow.contentWindow.i18nTemplate.process(doc, loadTimeData);

  // Initialize preference connected checkboxes in terms of service page.
  termsPage = new TermsOfServicePage(
      doc.getElementById('terms'),
      data.arcManaged,
      data.countryCode,
      new MetricsPreferenceCheckbox(
          doc.getElementById('metrics-preference'),
          data.learnMoreStatistics,
          '#learn-more-link-metrics',
          data.isOwnerProfile,
          data.textMetricsDisabled,
          data.textMetricsEnabled,
          data.textMetricsManagedDisabled,
          data.textMetricsManagedEnabled),
      new PreferenceCheckbox(
          doc.getElementById('backup-restore-preference'),
          data.learnMoreBackupAndRestore,
          '#learn-more-link-backup-restore',
          data.controlledByPolicy),
      new PreferenceCheckbox(
          doc.getElementById('location-service-preference'),
          data.learnMoreLocationServices,
          '#learn-more-link-location-service',
          data.controlledByPolicy));
}

/**
 * Handles native messages received from ArcSupportHost.
 * @param {!Object} message The message received.
 */
function onNativeMessage(message) {
  if (!message.action) {
    return;
  }

  if (!appWindow) {
    console.warn('Received native message when window is not available.');
    return;
  }

  if (message.action == 'initialize') {
    initialize(message.data, message.deviceId);
  } else if (message.action == 'setMetricsMode') {
    termsPage.onMetricxPreferenceChanged(message.enabled, message.managed);
  } else if (message.action == 'setBackupAndRestoreMode') {
    termsPage.onBackupRestorePreferenceChanged(
        message.enabled, message.managed);
  } else if (message.action == 'setLocationServiceMode') {
    termsPage.onLocationServicePreferenceChanged(
        message.enabled, message.managed);
  } else if (message.action == 'closeWindow') {
    if (appWindow) {
      appWindow.close();
    }
  } else if (message.action == 'showPage') {
    showPage(message.page);
  } else if (message.action == 'showErrorPage') {
    showErrorPage(message.errorMessage, message.shouldShowSendFeedback);
  } else if (message.action == 'setWindowBounds') {
    setWindowBounds();
  }
}

/**
 * Connects to ArcSupportHost.
 */
function connectPort() {
  var hostName = 'com.google.arc_support';
  port = chrome.runtime.connectNative(hostName);
  port.onMessage.addListener(onNativeMessage);
}

/**
 * Shows requested page and hide others. Show appWindow if it was hidden before.
 * 'none' hides all views.
 * @param {string} pageDivId id of divider of the page to show.
 */
function showPage(pageDivId) {
  if (!appWindow) {
    return;
  }

  hideOverlay();
  var doc = appWindow.contentWindow.document;
  // If the request is lso-loading and arc-loading page is currently shown,
  // then we do not switch the view. This is because both pages are saying
  // "operation in progress", and switching the page looks unwanted message
  // change from users' point of view.
  if (pageDivId != 'lso-loading' || doc.getElementById('arc-loading').hidden) {
    var pages = doc.getElementsByClassName('section');
    for (var i = 0; i < pages.length; i++) {
      pages[i].hidden = pages[i].id != pageDivId;
    }
  }

  if (pageDivId == 'lso-loading') {
    lsoView.src = 'https://accounts.google.com/o/oauth2/v2/auth?client_id=' +
                  '1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.' +
                  'googleusercontent.com&response_type=code&redirect_uri=oob&' +
                  'scope=https://www.google.com/accounts/OAuthLogin&' +
                  'device_type=arc_plus_plus&device_id=' + currentDeviceId +
                  '&hl=' + navigator.language;
  }
  appWindow.show();
  if (pageDivId == 'terms') {
    termsPage.onShow();
  }
}

/**
 * Shows an error page, with given errorMessage.
 *
 * @param {string} errorMessage Localized error message text.
 * @param {?boolean} opt_shouldShowSendFeedback If set to true, show "Send
 *     feedback" button.
 */
function showErrorPage(errorMessage, opt_shouldShowSendFeedback) {
  if (!appWindow) {
    return;
  }

  var doc = appWindow.contentWindow.document;
  var messageElement = doc.getElementById('error-message');
  messageElement.innerText = errorMessage;

  var sendFeedbackElement = doc.getElementById('button-send-feedback');
  sendFeedbackElement.hidden = !opt_shouldShowSendFeedback;

  showPage('error');
}

/**
 * Shows overlay dialog and required content.
 * @param {string} overlayClass Defines which content to show, 'overlay-url' for
 *                              webview based content and 'overlay-text' for
 *                              simple text view.
 */
function showOverlay(overlayClass) {
  var doc = appWindow.contentWindow.document;
  var overlayContainer = doc.getElementById('overlay-container');
  overlayContainer.className = 'overlay ' + overlayClass;
  overlayContainer.hidden = false;
}

/**
 * Opens overlay dialog and shows formatted text content there.
 * @param {string} content HTML formatted text to show.
 */
function showTextOverlay(content) {
  var doc = appWindow.contentWindow.document;
  var textContent = doc.getElementById('overlay-text-content');
  textContent.innerHTML = content;
  showOverlay('overlay-text');
}

/**
 * Opens overlay dialog and shows external URL there.
 * @param {string} url Target URL to open in overlay dialog.
 */
function showURLOverlay(url) {
  var doc = appWindow.contentWindow.document;
  var overlayWebview = doc.getElementById('overlay-url');
  overlayWebview.src = url;
  showOverlay('overlay-url');
}

/**
 * Shows Google Privacy Policy in overlay dialog. Policy link is detected from
 * the content of terms view.
 */
function showPrivacyPolicyOverlay() {
  var details = {code: 'getPrivacyPolicyLink();'};
  termsPage.termsView_.executeScript(details, function(results) {
    if (results && results.length == 1 && typeof results[0] == 'string') {
      showURLOverlay(results[0]);
    } else {
      showURLOverlay('https://www.google.com/policies/privacy/');
    }
  });
}

/**
 * Hides overlay dialog.
 */
function hideOverlay() {
  var doc = appWindow.contentWindow.document;
  var overlayContainer = doc.getElementById('overlay-container');
  overlayContainer.hidden = true;
}

function setWindowBounds() {
  if (!appWindow) {
    return;
  }

  var decorationWidth = appWindow.outerBounds.width -
      appWindow.innerBounds.width;
  var decorationHeight = appWindow.outerBounds.height -
      appWindow.innerBounds.height;

  var outerWidth = INNER_WIDTH + decorationWidth;
  var outerHeight = INNER_HEIGHT + decorationHeight;
  if (outerWidth > screen.availWidth) {
    outerWidth = screen.availWidth;
  }
  if (outerHeight > screen.availHeight) {
    outerHeight = screen.availHeight;
  }
  if (appWindow.outerBounds.width == outerWidth &&
      appWindow.outerBounds.height == outerHeight) {
    return;
  }

  appWindow.outerBounds.width = outerWidth;
  appWindow.outerBounds.height = outerHeight;
  appWindow.outerBounds.left = Math.ceil((screen.availWidth - outerWidth) / 2);
  appWindow.outerBounds.top =
    Math.ceil((screen.availHeight - outerHeight) / 2);
}

chrome.app.runtime.onLaunched.addListener(function() {
  var onAppContentLoad = function() {
    var doc = appWindow.contentWindow.document;
    lsoView = doc.getElementById('arc-support');
    lsoView.addContentScripts([
        { name: 'postProcess',
          matches: ['https://accounts.google.com/*'],
          css: { files: ['lso.css'] },
          run_at: 'document_end'
        }]);

    var isApprovalResponse = function(url) {
      var resultUrlPrefix = 'https://accounts.google.com/o/oauth2/approval?';
      return url.substring(0, resultUrlPrefix.length) == resultUrlPrefix;
    };

    var lsoError = false;
    var onLsoViewRequestResponseStarted = function(details) {
      if (isApprovalResponse(details.url)) {
        showPage('arc-loading');
      }
      lsoError = false;
    };

    var onLsoViewErrorOccurred = function(details) {
      showErrorPage(
          appWindow.contentWindow.loadTimeData.getString('serverError'));
      lsoError = true;
    };

    var onLsoViewContentLoad = function() {
      if (lsoError) {
        return;
      }

      if (!isApprovalResponse(lsoView.src)) {
        // Show LSO page when its content is ready.
        showPage('lso');
        // We have fixed width for LSO page in css file in order to prevent
        // unwanted webview resize animation when it is shown first time. Now
        // it safe to make it up to window width.
        lsoView.style.width = '100%';
        return;
      }

      lsoView.executeScript({code: 'document.title;'}, function(results) {
        var authCodePrefix = 'Success code=';
        if (results && results.length == 1 && typeof results[0] == 'string' &&
            results[0].substring(0, authCodePrefix.length) == authCodePrefix) {
          var authCode = results[0].substring(authCodePrefix.length);
          sendNativeMessage('onAuthSucceeded', {code: authCode});
        } else {
          sendNativeMessage('onAuthFailed');
          showErrorPage(
              appWindow.contentWindow.loadTimeData.getString(
                  'authorizationFailed'));
        }
      });
    };

    var requestFilter = {
      urls: ['<all_urls>'],
      types: ['main_frame']
    };

    lsoView.request.onResponseStarted.addListener(
        onLsoViewRequestResponseStarted, requestFilter);
    lsoView.request.onErrorOccurred.addListener(
        onLsoViewErrorOccurred, requestFilter);
    lsoView.addEventListener('contentload', onLsoViewContentLoad);

    var onRetry = function() {
      sendNativeMessage('onRetryClicked');
    };

    var onSendFeedback = function() {
      sendNativeMessage('onSendFeedbackClicked');
    };

    doc.getElementById('button-retry').addEventListener('click', onRetry);
    doc.getElementById('button-send-feedback')
        .addEventListener('click', onSendFeedback);
    doc.getElementById('overlay-close').addEventListener('click', hideOverlay);
    doc.getElementById('privacy-policy-link').addEventListener(
        'click', showPrivacyPolicyOverlay);

    var overlay = doc.getElementById('overlay-container');
    appWindow.contentWindow.cr.ui.overlay.setupOverlay(overlay);
    appWindow.contentWindow.cr.ui.overlay.globalInitialization();
    overlay.addEventListener('cancelOverlay', hideOverlay);

    connectPort();
  };

  var onWindowCreated = function(createdWindow) {
    appWindow = createdWindow;
    appWindow.contentWindow.onload = onAppContentLoad;
    appWindow.onClosed.addListener(onWindowClosed);

    setWindowBounds();
  };

  var onWindowClosed = function() {
    appWindow = null;

    // Notify to Chrome.
    sendNativeMessage('onWindowClosed');

    // On window closed, then dispose the extension. So, close the port
    // otherwise the background page would be kept alive so that the extension
    // would not be unloaded.
    port.disconnect();
    port = null;
  };

  var options = {
    'id': 'play_store_wnd',
    'resizable': false,
    'hidden': true,
    'frame': {
      type: 'chrome',
      color: '#ffffff'
    },
    'innerBounds': {
      'width': INNER_WIDTH,
      'height': INNER_HEIGHT
    }
  };
  chrome.app.window.create('main.html', options, onWindowCreated);
});