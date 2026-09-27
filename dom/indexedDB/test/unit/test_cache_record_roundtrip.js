/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps() {
  const name = "cache-record-roundtrip";
  let request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onupgradeneeded = event => {
    event.target.result.createObjectStore("cache", {keyPath: "key"});
  };
  request.onsuccess = grabEventAndContinueHandler;
  let event = yield undefined;
  let db = event.target.result;
  let transaction = db.transaction("cache", "readwrite");
  transaction.onabort = errorHandler;
  transaction.oncomplete = grabEventAndContinueHandler;
  let store = transaction.objectStore("cache");
  store.put({key: "text", value: "compiled filter data"});
  store.put({key: "bytes", value: new Uint8Array([0, 1, 127, 255])});
  store.put({key: "empty", value: new ArrayBuffer(0)});
  yield undefined;
  db.close();

  // Reopen after committing: test the serialized database records, rather than
  // just observing successful writes or values still held by the caller.
  request = indexedDB.open(name, 1);
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  db = event.target.result;
  request = db.transaction("cache").objectStore("cache").getAll();
  request.onerror = errorHandler;
  request.onsuccess = grabEventAndContinueHandler;
  event = yield undefined;
  let records = event.target.result;
  is(records.length, 3, "All committed records are readable");
  is(records[0].key, "bytes", "Records retain their keys");
  ok(records[0].value instanceof Uint8Array, "Typed array type survives storage");
  is(Array.from(records[0].value).join(), "0,1,127,255", "Bytes survive storage");
  is(records[1].value.byteLength, 0, "Empty buffers survive storage");
  is(records[2].value, "compiled filter data", "Strings survive storage");
  db.close();
  finishTest();
  yield undefined;
}
