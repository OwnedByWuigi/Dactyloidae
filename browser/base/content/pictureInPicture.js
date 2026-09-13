/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var Player = {
  browser: null,
  source: null,
  sourceTab: null,

  init() {
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
    mm.addMessageListener("PictureInPicture:Ready", () => {
      mm.sendAsyncMessage("PictureInPicture:Init", {id: args.id});
    });
    mm.addMessageListener("PictureInPicture:Close", this.close);
    mm.addMessageListener("PictureInPicture:State", message => {
      document.getElementById("play").label = message.data.paused ?
        this.playLabel : document.getElementById("pause-label").value;
      document.getElementById("mute").label = message.data.muted ?
        document.getElementById("unmute-label").value : this.muteLabel;
    });
    browser.addEventListener("oop-browser-crashed", this.close);
    mm.loadFrameScript("chrome://browser/content/pictureInPictureContent.js", false);
  },

  command(command) {
    if (this.browser) {
      this.browser.messageManager.sendAsyncMessage("PictureInPicture:Command", {command});
    }
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

window.addEventListener("load", () => Player.init(), {once: true});
window.addEventListener("unload", () => Player.destroy(), {once: true});
window.addEventListener("keydown", event => {
  if (event.key == "Escape") {
    window.close();
  } else if (event.key == " " && event.target.localName != "button") {
    event.preventDefault();
    Player.command("playpause");
  }
});
