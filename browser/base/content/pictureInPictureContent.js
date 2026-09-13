/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var {PictureInPicture} = Components.utils.import(
  "resource:///modules/PictureInPicture.jsm", {});
var source = null;
var stream = null;
var output = null;
var still = null;
var observer = null;
var sourceWindow = null;
var initialized = false;
var events = ["play", "pause", "volumechange", "ended", "seeking", "seeked",
              "resize", "timeupdate", "durationchange", "progress"];

function update() {
  if (!source) {
    return;
  }
  // Capture streams do not produce frames while their source is paused.
  // Keep a snapshot for that case, without changing the source's play state.
  still.hidden = !source.paused && !source.ended;
  if (!still.hidden && source.readyState >= 2) {
    still.width = source.videoWidth;
    still.height = source.videoHeight;
    still.getContext("2d").drawImage(source, 0, 0, still.width, still.height);
  }
  let ranges = source.seekable;
  sendAsyncMessage("PictureInPicture:State", {
    paused: source.paused || source.ended,
    muted: source.muted,
    currentTime: source.currentTime,
    duration: source.duration,
    seekableStart: ranges.length ? ranges.start(0) : 0,
    seekableEnd: ranges.length ? ranges.end(ranges.length - 1) : 0,
  });
}

function seekTo(time) {
  if (typeof time != "number" || !Number.isFinite(time)) {
    return;
  }
  let ranges = source.seekable;
  // Live streams may have no seekable range, or a moving DVR window.
  // Pick the nearest valid point, including when there are gaps.
  let position = null;
  let distance = Infinity;
  for (let i = 0; i < ranges.length; i++) {
    let candidate = Math.max(ranges.start(i), Math.min(time, ranges.end(i)));
    if (Math.abs(candidate - time) < distance) {
      distance = Math.abs(candidate - time);
      position = candidate;
    }
  }
  if (position !== null) {
    try {
      source.currentTime = position;
    } catch (error) {
      Components.utils.reportError(error);
    }
  }
}

function cleanup() {
  if (observer) {
    observer.disconnect();
    observer = null;
  }
  if (source) {
    for (let name of events) {
      source.removeEventListener(name, update);
    }
    source.removeEventListener("emptied", closePlayer);
    source.removeEventListener("error", closePlayer);
    sourceWindow.removeEventListener("pagehide", closePlayer);
    source.mozPictureInPicture = false;
    source = sourceWindow = null;
  }
  if (output) {
    output.srcObject = null;
    output = null;
  }
  if (stream) {
    for (let track of stream.getTracks()) {
      track.stop();
    }
    stream = null;
  }
  still = null;
}

function closePlayer() {
  cleanup();
  sendAsyncMessage("PictureInPicture:Close");
}

addMessageListener("PictureInPicture:Init", message => {
  // takeSource consumes the handoff. A duplicate initialization must not
  // consume it again or close an already playing window.
  if (initialized) {
    return;
  }
  initialized = true;
  try {
    source = PictureInPicture.takeSource(message.data.id);
    sourceWindow = source.ownerGlobal;
    let doc = content.document;
    doc.documentElement.style.cssText = "height:100%;background:black";
    doc.body.style.cssText = "margin:0;height:100%;overflow:hidden";
    output = doc.createElement("video");
    still = doc.createElement("canvas");
    for (let element of [output, still]) {
      element.style.cssText = "position:absolute;width:100%;height:100%;object-fit:contain";
      doc.body.appendChild(element);
    }
    // Decoder capture redirects audio to the captured stream. Its samples
    // already include the source's volume/mute settings. MediaStream sources
    // keep their own audio output, so mute only that case to avoid an echo.
    output.muted = !!source.srcObject;
    output.volume = 1;
    source.mozPictureInPicture = true;
    stream = source.mozCaptureStream();
    output.srcObject = stream;
    output.play().catch(error => {
      Components.utils.reportError(error);
      closePlayer();
    });
    for (let name of events) {
      source.addEventListener(name, update);
    }
    source.addEventListener("emptied", closePlayer);
    source.addEventListener("error", closePlayer);
    sourceWindow.addEventListener("pagehide", closePlayer);
    observer = new sourceWindow.MutationObserver(() => {
      if (source && !source.isConnected) {
        closePlayer();
      }
    });
    observer.observe(source.ownerDocument, {childList: true, subtree: true});
    update();
  } catch (error) {
    Components.utils.reportError(error);
    closePlayer();
  }
});

addMessageListener("PictureInPicture:Command", message => {
  if (!source) {
    return;
  }
  switch (message.data.command) {
    case "playpause":
      if (source.paused || source.ended) {
        source.play().catch(Components.utils.reportError);
      } else {
        source.pause();
      }
      break;
    case "mute":
      source.muted = !source.muted;
      break;
    case "seek":
      seekTo(message.data.time);
      break;
    case "close":
      cleanup();
      break;
  }
});

// Frame-script unload also covers a parent window closing before an async
// close command can be delivered.
addEventListener("unload", cleanup);
sendAsyncMessage("PictureInPicture:Ready");
