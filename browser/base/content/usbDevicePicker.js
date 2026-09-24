/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var USBDevicePicker = {
  args: null,

  init() {
    this.args = window.arguments[0];
    document.title = this.args.title;
    document.getElementById("message").value = this.args.message;
    document.getElementById("select").label = this.args.selectLabel;
    document.getElementById("cancel").label = this.args.cancelLabel;

    let list = document.getElementById("devices");
    for (let choice of this.args.choices) {
      list.appendItem(choice, choice);
    }
    list.selectedIndex = 0;
    list.focus();
  },

  select() {
    let list = document.getElementById("devices");
    this.args.prompt.select(list.selectedIndex);
  },

  cancel() {
    this.args.prompt.cancel();
  },

  destroy() {
    if (this.args && !this.args.prompt.completed) {
      // The window manager can close this window directly. Detach it first
      // so cancel() does not try to close an already-unloading window.
      this.args.prompt.window = null;
      this.args.prompt.cancel();
    }
    this.args = null;
  },
};

window.addEventListener("keydown", event => {
  if (event.key == "Escape") {
    USBDevicePicker.cancel();
  } else if (event.key == "Enter") {
    USBDevicePicker.select();
  }
});
