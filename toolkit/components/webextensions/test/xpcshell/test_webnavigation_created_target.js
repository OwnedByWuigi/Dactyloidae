"use strict";

add_task(function* test_created_navigation_target_dispatch() {
  let scope = {};
  Services.scriptloader.loadSubScript("resource://gre/modules/WebNavigation.jsm", scope);
  let {Manager, WebNavigation} = scope;
  let event = WebNavigation.onCreatedNavigationTarget;
  let source = {}, target = {};
  let received = [];
  let listener = data => received.push(data);
  event.addListener(listener, {matches: url => url == "https://example.com/target"});
  do_register_cleanup(() => event.removeListener(listener));

  for (let sourceFirst of [true, false]) {
    let data = {
      url: "https://example.com/target", sourceWindowId: 123,
      createdWindowId: sourceFirst ? 456 : 789,
    };
    Manager.onCreatedNavigationTarget(sourceFirst ? source : target,
      Object.assign({isSourceTab: sourceFirst}, data));
    equal(received.length, 0, "wait for both browsers");
    Manager.onCreatedNavigationTarget(sourceFirst ? target : source,
      Object.assign({isSourceTab: !sourceFirst}, data));
    equal(received.length, 1, "dispatch once, in either message order");
    equal(received[0].browser, target);
    equal(received[0].sourceTabBrowser, source);
    equal(received[0].sourceWindowId, 123);
    received.length = 0;
  }

  function notify(url) {
    Services.obs.notifyObservers({wrappedJSObject: {
      url, sourceTabBrowser: source, createdTabBrowser: target,
      sourceFrameOuterWindowID: 123,
    }}, "webNavigation-createdNavigationTarget", null);
  }
  notify("https://other.example/target");
  equal(received.length, 0, "filter applies to chrome-created targets too");
  notify("https://example.com/target");
  equal(received.length, 1);
  Manager.onCreatedNavigationTarget(source, {
    isSourceTab: true, createdWindowId: 1000, sourceWindowId: 123,
    url: "https://example.com/target",
  });
  event.removeListener(listener);
  equal(Manager.createdNavigationTargetByOuterWindowId.size, 0,
        "unregistering releases unmatched messages and timers");
  notify("https://example.com/target");
  equal(received.length, 1, "removed listeners are not invoked");
});
