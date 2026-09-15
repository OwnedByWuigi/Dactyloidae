"use strict";

// Exercise real channels: a successful import alone does not prove that the
// observer, native wrapper, filters and blocking response are connected.
add_task(function* test_webrequest_backend() {
  let {WebRequest} = Cu.import("resource://gre/modules/WebRequest.jsm", {});
  let server = createHttpServer();
  let hits = 0;
  server.registerPathHandler("/request", (request, response) => {
    ++hits;
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("allowed");
  });
  let url = `http://localhost:${server.identity.primaryPort}/request`;
  function request(system = false) {
    let uri = Services.io.newURI(url, null, null);
    let channel = NetUtil.newChannel({
      uri,
      loadingPrincipal: system ? Services.scriptSecurityManager.getSystemPrincipal()
        : Services.scriptSecurityManager.createCodebasePrincipal(uri, {}),
      securityFlags: Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
      contentPolicyType: Ci.nsIContentPolicy.TYPE_XMLHTTPREQUEST,
    });
    return new Promise(resolve => {
      NetUtil.asyncFetch(channel, (stream, status) => resolve(status));
    });
  }

  let calls = 0;
  let block = data => {
    ++calls;
    equal(data.url, url);
    equal(data.type, "xmlhttprequest");
    equal(typeof data.requestId, "string");
    return {cancel: true};
  };
  let event = WebRequest.onBeforeRequest;
  do_register_cleanup(() => event.removeListener(block));
  event.addListener(block, {urls: ["http://localhost/*"]}, ["blocking"]);
  equal(yield request(), Cr.NS_ERROR_ABORT, "blocking listener cancels the channel");
  equal(calls, 1);
  equal(hits, 0, "cancelled request never reaches the server");
  event.removeListener(block);
  equal(yield request(), Cr.NS_OK, "removing the listener restores loading");

  for (let filter of [
    {urls: ["http://example.org/*"]},
    {urls: ["<all_urls>"], types: ["image"]},
    {urls: ["<all_urls>"], incognito: true},
    {urls: ["<all_urls>"], tabId: 123},
  ]) {
    event.addListener(block, filter, ["blocking"]);
    equal(yield request(), Cr.NS_OK, "nonmatching requests are untouched");
    event.removeListener(block);
  }
  event.addListener(block, {urls: ["<all_urls>"]}, ["blocking"], {
    policy: {id: "backend-test", allowedOrigins: new (Cu.import(
      "resource://gre/modules/MatchPattern.jsm", {}).MatchPattern)(["http://example.org/*"])},
  });
  equal(yield request(), Cr.NS_OK, "host permissions constrain the listener");
  event.removeListener(block);
  event.addListener(block, {urls: ["<all_urls>"]}, ["blocking"]);
  equal(yield request(true), Cr.NS_OK, "system requests are not exposed");
  event.removeListener(block);
  equal(calls, 1, "only the matching content request was dispatched");
});
