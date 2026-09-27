"use strict";

add_task(function* test_explicit_webextension_schemes() {
  let {MatchPattern} = Cu.import("resource://gre/modules/MatchPattern.jsm", {});
  let patterns = [
    ["moz-extension://6e20d047-ef47-41c0-a95f-93aa35f1798d/web_accessible_resources/*",
     "moz-extension://6e20d047-ef47-41c0-a95f-93aa35f1798d/web_accessible_resources/file.js"],
    ["ws://*/*", "ws://example.com/socket"],
    ["wss://*/*", "wss://example.com/socket"],
  ];

  for (let [pattern, url] of patterns) {
    let matcher = new MatchPattern(pattern);
    ok(matcher.matches(Services.io.newURI(url, null, null)),
       `explicit scheme is accepted: ${pattern}`);
  }
});

add_task(function* test_wildcard_remains_web_only() {
  let {MatchPattern} = Cu.import("resource://gre/modules/MatchPattern.jsm", {});
  let matcher = new MatchPattern("*://*/*");
  ok(!matcher.matches(Services.io.newURI("ws://example.com/socket", null, null)),
     "scheme wildcard does not implicitly include WebSockets");
  ok(!matcher.matches(Services.io.newURI("moz-extension://example/", null, null)),
     "scheme wildcard does not include extension URLs");
});
