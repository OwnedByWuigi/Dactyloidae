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
var events = ["play", "pause", "volumechange", "ended", "seeked", "resize"];

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
  sendAsyncMessage("PictureInPicture:State",
                   {paused: source.paused || source.ended, muted: source.muted});
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
    output.muted = true; // Audio continues to come from the originating video.
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
    case "close":
      cleanup();
      break;
  }
});

// Frame-script unload also covers a parent window closing before an async
// close command can be delivered.
addEventListener("unload", cleanup);
sendAsyncMessage("PictureInPicture:Ready");
