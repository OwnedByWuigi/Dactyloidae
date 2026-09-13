/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["PictureInPicture"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Timer.jsm");

// Each content process has its own module instance. Only a weak reference to
// the source crosses between frame scripts; DOM objects never cross processes.
this.PictureInPicture = {
  source: null,
  window: null,
  request: 0,

  register(video, id) {
    if (!video || video.localName != "video" || video.error ||
        !video.videoWidth || !video.videoHeight || video.mediaKeys) {
      throw new Error("This video is not available for Picture-in-Picture");
    }
    this.source = {id, video: Cu.getWeakReference(video)};
    return {id, width: video.videoWidth, height: video.videoHeight};
  },

  takeSource(id) {
    if (!this.source || this.source.id != id) {
      throw new Error("Picture-in-Picture source process is unavailable");
    }
    let video = this.source.video.get();
    this.source = null;
    if (!video || !video.isConnected) {
      throw new Error("Picture-in-Picture source has been removed");
    }
    return video;
  },

  open(browser, video) {
    let id = Cc["@mozilla.org/uuid-generator;1"]
               .getService(Ci.nsIUUIDGenerator).generateUUID().toString();
    let request = ++this.request;
    let mm = browser.messageManager;
    let timer;
    let ready = message => {
      if (message.data.id != id) {
        return;
      }
      mm.removeMessageListener("PictureInPicture:Prepared", ready);
      clearTimeout(timer);
      if (request != this.request || !browser.isConnected) {
        return;
      }
      if (message.data.error) {
        Cu.reportError(message.data.error);
        return;
      }
      this.openWindow(browser, message.data);
    };
    mm.addMessageListener("PictureInPicture:Prepared", ready);
    timer = setTimeout(() => {
      mm.removeMessageListener("PictureInPicture:Prepared", ready);
    }, 10000);
    mm.sendAsyncMessage("ContextMenu:MediaCommand",
                        {command: "pictureinpicture", data: id},
                        {element: video});
  },

  openWindow(browser, data) {
    if (this.window && !this.window.closed) {
      this.window.close();
    }
    let screen = browser.ownerGlobal.screen;
    let width = Math.min(480, screen.availWidth);
    let height = Math.min(Math.round(width * data.height / data.width) + 48,
                          Math.round(screen.availHeight * 0.8));
    this.window = browser.ownerGlobal.openDialog(
      "chrome://browser/content/pictureInPicture.xul", "",
      "chrome,dialog=no,resizable,alwaysRaised,width=" + width +
      ",height=" + height,
      {browser, id: data.id});
    return this.window;
  },
};
