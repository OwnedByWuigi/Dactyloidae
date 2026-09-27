/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// The browser XBL bindings resolve these names in their owning window.
const {utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/Services.jsm");

var Player = {
  browser: null,
  source: null,
  sourceTab: null,
  seeking: false,
  state: null,
  drag: null,

  startDrag(event) {
    if (event.button != 0 ||
        event.target.closest("button, input, #playback-controls")) {
      return;
    }
    let stack = document.getElementById("player-stack");
    if (!stack.contains(event.target)) {
      return;
    }
    event.preventDefault();
    this.drag = {
      direction: event.target.getAttribute("data-resize") || "",
      pointerX: event.screenX,
      pointerY: event.screenY,
      x: window.screenX,
      y: window.screenY,
      width: window.outerWidth,
      height: window.outerHeight,
    };
    // Capture keeps the gesture working after the pointer leaves the window.
    stack.setCapture(true);
    stack.setAttribute("dragging", "true");
  },

  moveDrag(event) {
    let drag = this.drag;
    if (!drag) {
      return;
    }
    if (!(event.buttons & 1)) {
      this.endDrag();
      return;
    }
    event.preventDefault();
    let dx = event.screenX - drag.pointerX;
    let dy = event.screenY - drag.pointerY;
    if (!drag.direction) {
      window.moveTo(drag.x + dx, drag.y + dy);
      return;
    }
    let west = drag.direction.includes("w");
    let east = drag.direction.includes("e");
    let north = drag.direction.includes("n");
    let south = drag.direction.includes("s");
    let width = Math.max(300, drag.width + (west ? -dx : east ? dx : 0));
    let height = Math.max(170, drag.height + (north ? -dy : south ? dy : 0));
    window.resizeTo(width, height);
    // Anchor the opposite edge, using the actual size in case the platform
    // imposed a constraint (for example, at a monitor boundary).
    if (west || north) {
      window.moveTo(west ? drag.x + drag.width - window.outerWidth : drag.x,
                    north ? drag.y + drag.height - window.outerHeight : drag.y);
    }
  },

  endDrag() {
    if (this.drag) {
      this.drag = null;
      let stack = document.getElementById("player-stack");
      stack.releaseCapture();
      stack.removeAttribute("dragging");
    }
  },

  init() {
    if (this.browser) {
      return;
    }
    let args = window.arguments[0];
    this.source = args.browser;
    if (!this.source || !this.source.isConnected) {
      window.close();
      return;
    }
    this.sourceTab = this.source.ownerGlobal.gBrowser.getTabForBrowser(this.source);
    this.close = () => window.close();
    this.sourceTab.addEventListener("TabClose", this.close);
    this.source.addEventListener("oop-browser-crashed", this.close);

    this.playLabel = document.getElementById("play").label;
    this.muteLabel = document.getElementById("mute").label;
    let browser = this.browser = document.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("flex", "1");
    browser.setAttribute("disablehistory", "true");
    browser.setAttribute("remote", this.source.isRemoteBrowser ? "true" : "false");
    if (this.source.isRemoteBrowser) {
      browser.setAttribute("remoteType", this.source.getAttribute("remoteType"));
    }
    // The process-local source reference requires this process affinity.
    browser.relatedBrowser = this.source;
    document.getElementById("player-container").appendChild(browser);
    let mm = browser.messageManager;
    let initialized = false;
    let ready = () => {
      if (initialized) {
        return;
      }
      initialized = true;
      mm.removeMessageListener("PictureInPicture:Ready", ready);
      mm.sendAsyncMessage("PictureInPicture:Init", {id: args.id});
    };
    mm.addMessageListener("PictureInPicture:Ready", ready);
    mm.addMessageListener("PictureInPicture:Close", this.close);
    mm.addMessageListener("PictureInPicture:State", message => {
      this.updateState(message.data);
    });
    browser.addEventListener("oop-browser-crashed", this.close);
    mm.loadFrameScript("chrome://browser/content/pictureInPictureContent.js", false);
  },

  command(command, data = {}) {
    if (this.browser) {
      this.browser.messageManager.sendAsyncMessage("PictureInPicture:Command",
                                                   Object.assign({command}, data));
    }
  },

  formatTime(seconds) {
    if (!Number.isFinite(seconds) || seconds < 0) {
      return "0:00";
    }
    seconds = Math.floor(seconds);
    let hours = Math.floor(seconds / 3600);
    let minutes = Math.floor(seconds / 60) % 60;
    let pad = value => value < 10 ? "0" + value : String(value);
    return (hours ? hours + ":" + pad(minutes) : String(minutes)) +
           ":" + pad(seconds % 60);
  },

  updateTime(position) {
    let duration = this.state && this.state.duration;
    let total = Number.isFinite(duration) ? this.formatTime(duration) :
      document.getElementById("live-label").value;
    document.getElementById("time").value = this.formatTime(position) + " / " + total;
  },

  updateState(state) {
    this.state = state;
    document.getElementById("play").label = state.paused ?
      this.playLabel : document.getElementById("pause-label").value;
    document.getElementById("mute").label = state.muted ?
      document.getElementById("unmute-label").value : this.muteLabel;
    if (!this.seeking) {
      let seek = document.getElementById("seek");
      seek.disabled = !(state.seekableEnd > state.seekableStart);
      seek.min = state.seekableStart;
      seek.max = state.seekableEnd || 1;
      seek.value = state.currentTime;
      this.updateTime(state.currentTime);
    }
  },

  seek(value) {
    let position = Number(value);
    if (!Number.isFinite(position) || document.getElementById("seek").disabled) {
      return;
    }
    this.seeking = true;
    document.getElementById("player-overlay").setAttribute("seeking", "true");
    this.updateTime(position);
    this.command("seek", {time: position});
  },

  finishSeeking() {
    this.seeking = false;
    document.getElementById("player-overlay").removeAttribute("seeking");
  },

  returnToTab() {
    if (this.source && this.source.isConnected) {
      let win = this.source.ownerGlobal;
      win.gBrowser.selectedTab = this.sourceTab;
      win.focus();
    }
    window.close();
  },

  destroy() {
    this.endDrag();
    if (this.sourceTab) {
      this.sourceTab.removeEventListener("TabClose", this.close);
      this.source.removeEventListener("oop-browser-crashed", this.close);
    }
    if (this.browser) {
      this.command("close");
    }
    this.source = this.sourceTab = null;
  },
};

// Only consume the listener for the player document's own load, rather than
// a load from the embedded browser.
window.addEventListener("load", function onLoad(event) {
  if (event.target == document) {
    window.removeEventListener("load", onLoad);
    Player.init();
  }
});
window.addEventListener("unload", function onUnload(event) {
  if (event.target == document) {
    window.removeEventListener("unload", onUnload);
    Player.destroy();
  }
});
window.addEventListener("keydown", event => {
  document.getElementById("player-overlay").setAttribute("keyboard", "true");
  if (event.key == "Escape") {
    window.close();
  } else if (event.target.localName == "input" || event.target.localName == "button") {
    return;
  } else if (event.key == " ") {
    event.preventDefault();
    Player.command("playpause");
  } else if (event.key == "ArrowLeft" || event.key == "ArrowRight") {
    event.preventDefault();
    if (Player.state) {
      Player.command("seek", {time: Player.state.currentTime +
                                    (event.key == "ArrowLeft" ? -5 : 5)});
    }
  }
});
window.addEventListener("mousedown", event => Player.startDrag(event));
window.addEventListener("mousemove", event => {
  Player.moveDrag(event);
  document.getElementById("player-overlay").removeAttribute("keyboard");
});
window.addEventListener("mouseup", () => Player.endDrag());
window.addEventListener("blur", event => {
  if (event.target == window) {
    Player.endDrag();
  }
});
