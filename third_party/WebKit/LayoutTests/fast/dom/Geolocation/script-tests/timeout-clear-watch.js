description("Tests that when a watch times out and is cleared from the error callback, there is no crash. This a regression test for https://bugs.webkit.org/show_bug.cgi?id=32111.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var error;

geolocationServiceMock.then(mock => {
    mock.setGeolocationPosition(51.478, -0.166, 100.0);

    var watchId = navigator.geolocation.watchPosition(function() {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'error.TIMEOUT');
        shouldBe('error.message', '"Timeout expired"');
        navigator.geolocation.clearWatch(watchId);
        window.setTimeout(finishJSTest, 0);
    }, {
        timeout: 0
    });
});

window.jsTestIsAsync = true;
