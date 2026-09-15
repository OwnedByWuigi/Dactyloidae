/*
 * Adapted from enhanced-h264ify's inject_codec_check.js.
 *
 * The MIT License (MIT)
 * Copyright (c) 2019 alextrv
 * Copyright (c) 2015 erkserkserks
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 */

/* YouTube-only codec selection controls. */
(function () {
  "use strict";

  var settings = window.__dactyloidaeYouTubeVideoSettings || {};
  var originalCanPlayType = HTMLMediaElement.prototype.canPlayType;
  var originalIsTypeSupported = window.MediaSource &&
                                window.MediaSource.isTypeSupported;

  function isVideoType(type) {
    return /(^|\s|;)video\//i.test(type) ||
           /(^|[; ])codecs\s*=\s*["']?(?:avc|vp0|av0)/i.test(type);
  }

  function isBlocked(type) {
    if (!type || !isVideoType(type)) {
      return false;
    }

    // AV1 is always blocked, independently of the preferences.
    if (/av01|av99/i.test(type)) {
      return true;
    }

    if (settings.forceH264 && !/avc1|avc3/i.test(type)) {
      return true;
    }

    if (settings.hideUnaccelerated &&
        !settings.vp9HardwareAccelerated && /vp9|vp09/i.test(type)) {
      return true;
    }

    if (settings.disable60fps) {
      var match = /(?:framerate|fps)\s*=\s*([0-9]+(?:\.[0-9]+)?)/i.exec(type);
      if (match && Number(match[1]) > 30) {
        return true;
      }
    }

    return false;
  }

  HTMLMediaElement.prototype.canPlayType = function (type) {
    if (isBlocked(type)) {
      return "";
    }
    return originalCanPlayType.call(this, type);
  };

  if (window.MediaSource && originalIsTypeSupported) {
    window.MediaSource.isTypeSupported = function (type) {
      if (isBlocked(type)) {
        return false;
      }
      return originalIsTypeSupported.call(this, type);
    };
  }
}());
