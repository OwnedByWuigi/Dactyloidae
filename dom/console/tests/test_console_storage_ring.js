function run_test() {
  const storage = Components.classes["@mozilla.org/consoleAPI-storage;1"]
                            .getService(Components.interfaces.nsIConsoleAPIStorage);
  Components.utils.import("resource://gre/modules/Services.jsm");
  storage.clearEvents();
  try {
    for (let i = 0; i < 3501; i++) {
      storage.recordEvent("ring-test", "outer", { timeStamp: i });
      if (i === 998 || i === 999 || i === 1000 || i === 1999 || i === 3500) {
        let events = storage.getEvents("ring-test");
        equal(events.length, Math.min(i + 1, 1000));
        for (let j = 0; j < events.length; j++) {
          equal(events[j].timeStamp, i + 1 - events.length + j);
        }
        events.length = 0;
        equal(storage.getEvents("ring-test").length, Math.min(i + 1, 1000));
      }
    }
    storage.recordEvent("other", "outer", { timeStamp: 2500.5 });
    let all = storage.getEvents();
    equal(all.length, 1001);
    equal(all[0].timeStamp, 2500.5);
    equal(all[1].timeStamp, 2501);
    equal(all[1000].timeStamp, 3500);

    let reentered = false;
    let observer = {
      observe(subject, topic, data) {
        if (data === "ring-test" && !reentered) {
          equal(storage.getEvents(data)[999].timeStamp, 3501);
          reentered = true;
          storage.recordEvent("ring-test", "outer", { timeStamp: 3502 });
        }
      }
    };
    Services.obs.addObserver(observer, "console-storage-cache-event", false);
    try {
      storage.recordEvent("ring-test", "outer", { timeStamp: 3501 });
    } finally {
      Services.obs.removeObserver(observer, "console-storage-cache-event");
    }
    equal(reentered, true);
    equal(storage.getEvents("ring-test")[999].timeStamp, 3502);
    storage.clearEvents("ring-test");
    equal(storage.getEvents("ring-test").length, 0);
    equal(storage.getEvents("other").length, 1);
    storage.recordEvent("ring-test", "outer", { timeStamp: 4000 });
    equal(storage.getEvents("ring-test")[0].timeStamp, 4000);
  } finally {
    storage.clearEvents();
  }
  equal(storage.getEvents().length, 0);
}
