/*

This code is issued under a disjunctive tri-license giving you the choice of
one of the three following sets of free software/open source licensing terms:

    * Mozilla Public License, version 1.1
    * GNU General Public License, version 2.0 
    * GNU Lesser General Public License, version 2.1

For users under the Mozilla Public License:

The contents of this file are subject to the Mozilla Public License
Version 1.1 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is the OverbiteFF Gopher addon.

The Initial Developer of the Original Code is Cameron Kaiser.
Portions created by Cameron Kaiser are Copyright (C) 2008-2016
Cameron Kaiser. All Rights Reserved. Copyright (C) 2008-2016 Contributors
to the Overbite Project.

For users under the GNU Public License:

OverbiteFF Gopher/CSO Firefox addon
Copyright (C) 2008-2016 Cameron Kaiser
Copyright (C) 2008-2016 Contributors to the Overbite Project

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
[ or http://www.gnu.org/licenses/gpl-2.0.html ]

For users under the GNU Lesser General Public License:

OverbiteFF Gopher/CSO Firefox addon
Copyright (C) 2008-2016 Cameron Kaiser
Copyright (C) 2008-2016 Contributors to the Overbite Project

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; version 2.1.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
[ or http://www.gnu.org/licenses/lgpl-2.1.html ]

*/
const OBFFVERS = 3.1; 
const OBFFBUILD = 1695;
const OBFFBUILDPREF = "extensions.overbiteff.buildmark";
const OBFFDOTLESSPREF = "extensions.overbiteff.dotless";
const OBFFFIXITYPEPREF = "extensions.overbiteff.fixitype";
const OBFFSCHEME = "gopher";
const OBFFCHROMEURL = "chrome://overbiteff";
const OBFFABOUTURL = (OBFFCHROMEURL + "/content/infobabe.html");
const OBFFIABOUTURL = "about:overbite";
const OBFFRABOUTURL = (OBFFCHROMEURL + "/content/startpage.html");
const OBFFPROT_HANDLER_CONTRACTID = "@mozilla.org/network/protocol;1?name="+OBFFSCHEME;
const OBFFPROT_HANDLER_CID = Components.ID("{977ffc4c-a635-433d-8477-ea575bfb7b19}");

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const nsILoadInfo = Ci.nsILoadInfo;
const nsIFactory = Ci.nsIFactory;
const nsISupports = Ci.nsISupports;
const nsIProtocolHandler = Ci.nsIProtocolHandler;
const nsIProxiedProtocolHandler = Ci.nsIProxiedProtocolHandler;

const nsIRequest = Ci.nsIRequest;
const nsIChannel = Ci.nsIChannel;
const nsIProxiedChannel = Ci.nsIProxiedChannel;
const nsIProgressEventSink = Ci.nsIProgressEventSink;

const nsISocketTransport = Ci.nsISocketTransport;
const nsIStreamConverter = Ci.nsIStreamConverter;
const nsIStreamListener = Ci.nsIStreamListener;
const nsIObserver = Ci.nsIObserver;
const nsIRequestObserver = Ci.nsIRequestObserver;
const nsITimerCallback = Ci.nsITimerCallback;

/* port control */
// 80 is okay because of some hybrid servers that can speak both on one port
var alwayslet = [ 13, 43, 70, 71, 72, 79, 80, 105, 1070, 2347, 3000, 3070,
	3099, 4323, 7055, 7070, 7071, 7072, 7077, 7080, 7777, 27070 ];
//var badports = [ 20,21,22,23,25,53,69,111,115,137,138,139,443,513,514,548 ];

/* cache */
var capscache = new Object();
var nullcaps = {
	PathKeepPreDelimiter : false,
	PathParentDouble : false,
	PathDelimiter : "",
	CapsVersion : 1
};

/* global functions for logging to the error console */
function OverbiteLogAlways(msg, error) {
        var consoleService = Cc["@mozilla.org/consoleservice;1"]
		.getService(Ci.nsIConsoleService);
	msg = "OverbiteFF says: "+msg;
	if (error) {
		consoleService.logStringError(msg);
	} else {
		consoleService.logStringMessage(msg);
	}
}

function OverbiteLog(msg, error) {

	// return Cr.NS_OK; // comment out for logging

	// OverbiteLogAlways(msg, error);
}

/* crap on a stick.
   you mean I have to implement my own NS_QueryNotificationCallbacks?
   so what do I have XPConnect for anyway?!
   rot in hell. */
function OverbiteQNC(one, two, three) {
	var progsink = null;

	if (three)
		return three;
	if (one.notificationCallbacks) {
		progsink = one.notificationCallbacks
			.getInterface(nsIProgressEventSink);
		if (!progsink && two) {
			// try that instead
			var cbs = two.notificationCallbacks;
			if (cbs)
				progsink = cbs.getInterface(
					nsIProgressEventSink);
		}
	}
	return(progsink);
}

/* global functions for processing caps */
function OverbiteCapsKey(host, port) {
	var nport = (port < 1) ? 70 : port;
	return host.toLowerCase()+":"+nport;
}
function OverbiteCapsPro(caps) {
	if (caps.substr(0, 4) != "CAPS")
		return nullcaps;

	var capsarr = caps.substr(4).split(/[\r\n]{1,2}/);
	var ncapsarr = nullcaps;
	var i;

	try {
	for (i=0; i<capsarr.length; i++) {
		var m = capsarr[i].indexOf("=");
		if (!capsarr[i].match(/^#/) && m != -1) {
			var k = capsarr[i].substr(0, m);
			var v = capsarr[i].substr(m+1);

			// convert booleans
			if (k == "PathParentDouble" ||
					k == "ServerSupportsStdinScripts" ||
					k == "PathKeepPreDelimiter" ||
					k == "PathKeepPreDelimeter")
				v = (v == "true" || v == "TRUE") ? true:false;

			// convert spelling
			if (k == "PathKeepPreDelimeter")
				k = "PathKeepPreDelimiter";
			if (k == "PathDelimeter")
				k = "PathDelimiter";
			ncapsarr[k] = v;
		}
	}
	} catch(e) { ncapsarr = nullcaps; }
	return ncapsarr;
}
function OverbiteCapsBackPathFromSel(caps, sel) {
	/* given a caps entry and a sel, lop off anything up to the last
	   delimiter. do not assume that the delimeter is one character
	   long. */

	var d = caps["PathDelimiter"];
	if (!d || !d.length) return ""; // no delimiter => opaque, jump to root
	var i = sel.lastIndexOf(d);
	if (i == -1) return ""; // no delimeter in sel => jump to root

	// putative
	var s = sel.substr(0, i);

	// handle the situation where the delimiter was on the end with nothing
	// after it
	if ((s + d) == sel) { // grrr
		i = s.lastIndexOf(d);
		if (i == -1) return ""; // no delimiter in sel => jump to root
		s = s.substr(0, i);
	}

	return s;
}
function OverbiteCapsUniversalBackURL(host, port, itype, sel) {
	// based on the caps, return either a .caps pseudo URL, a real URL or
	// nothing. used for the direct links. the capupurl is only valid iff:
	//
	// 1. we have a cached caps *and* it supports path delimeters
	// or
	// 2. we have no cached caps, in which case we hope for the best
	//
	// I thought about having this suppress the capupurl if it equals the
	// root, but this is inconsistent, especially with new hosts.

	var capupurl = '';
	var caps = capscache[OverbiteCapsKey(host, port)];

	// the itype could be in one of two places: either itself, or the
	// first character of the sel. if itype == '' then get it from there.
	if (itype == '') {
		itype = sel.substr(0, 1);
		sel = sel.substr(1);
	}
	// by definition backing up only can *ever* go up to a menu.
	itype = '1';

	if (!caps) { // no cache; construct pseudo URL
		capupurl = OverbiteURLFromSel(
			host + '.caps', port, itype, sel);
	} else {
		var kk = caps["PathDelimiter"];
		// if we have a path delimeter, use it
		if (kk && kk.length) 
			capupurl = OverbiteURLFromSel(
				host, port, itype,
				OverbiteCapsBackPathFromSel(caps, sel));
	}
	return capupurl;
}

// utility function, used by lots of things
function OverbiteURLFromSel(host, port, itype, sel) {
	var nport = (!port || port < 1) ? 70 : port;
	var suburl = OBFFSCHEME + '://' + host
		+ ((nport != 70)?(':'+nport): '')
		+ '/' + encodeURI(itype + sel);
	;
	return suburl;
}

/*
 * HACK!
 * the OverbitePrompter "debounces" back to back alerts and acts
 * effectively as the front end mutex. Needed since our channels
 * can get multiplexed even in single-process Firefox and our
 * channel object does a certain amount of front-end work currently.
 *
 * TODO: For e10s, remove all use of the prompter and use content
 * controls (a form for searches, HTML error messages, etc.)
*/

var lastPromptKey = null;
var lastCachedResponse = null;
// These need to be globals so that responses cache properly.
var chequestub = { value : false };
var query = { value : '' };
function OverbitePrompter(newkey, response) {
	if (lastPromptKey == newkey) {
OverbiteLog("prompter key "+lastPromptKey+" returning cached "+lastCachedResponse);
		lastPromptKey = null;
		return lastCachedResponse;
	}
	query = { value : '' };
	lastPromptKey = newkey;
	lastCachedResponse = response();
OverbiteLog("caching prompt "+newkey+" responded "+lastCachedResponse);
	return lastCachedResponse;
}

/*
 * the OverbiteCaps object is a pseudo-object that controls browser navigation
 * and stores caps data in the cache. it is mostly the same as Dotless and
 * implements the same interfaces.
 *
 * future expansion: have the caps display a proper info page.
*/

function OverbiteCaps() { }
OverbiteCaps.prototype = {

	// my stuff
	_contentType: null,
	_listener : null,
	_context : null,
	_buf : '',
	_sstream : null,
	_browser_callback : null,
	_host : '',
	_port : -1,
	_itemtype : '',
	_selector : '',

	// feed the beast (i.e., the listener on the other end)
	_feedBeast : function(request, listener, context, what) {
		// create a new instance each time (instead of reusing one)
		// just to make sure the previous instance has time to finish
		var stringstream = Cc["@mozilla.org/io/string-input-stream;1"]
			.createInstance(Ci.nsIStringInputStream);
		stringstream.setData(what, what.length);
		listener.onDataAvailable(request, context,
			stringstream, 0, what.length);
	},

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
						nsIStreamListener,
						nsIStreamConverter,
						nsIRequestObserver]),

	// nsIRequestObserver
	onStartRequest : function(request, context) {
		// init the channel with data and content type
		// this is almost always text/plain but we are flexible

		this._context = context;
		this._buf = '';

		var chan = request.QueryInterface(nsIChannel);
		if (chan)
			chan.contentType = "text/html";
		this._listener.onStartRequest(request, context);
	},

	/* when the request terminates, we then parse the data (using the
	   global functions above) and manipulate the browser. the other
	   end gets a StopRequest but we don't pass it any data. */

	onStopRequest : function(request, context, status) {
		// stop the stream and destroy it
		if (this._sstream)
			this._sstream.close();
		this._sstream = null;

		// process it here before we tear down everything
		var w = OverbiteCapsPro(this._buf);
		var k = OverbiteCapsKey(this._host, this._port);
		capscache[k] = w;
		OverbiteLog("cached caps entry created for "+k);

		// if there is a callback, send NO data. manipulate the URL
		// and we are done!
		if (this._browser_callback) {
			// compute the new URL ...
			var s = OverbiteCapsBackPathFromSel(w, this._selector);
			s = OverbiteURLFromSel(this._host, this._port,
				this._itemtype, s);
			OverbiteLog("backing up to "+s);

			// briefly stop the load group before we retarget ...
			this._listener.onStopRequest(request, this._context, status);
			this._listener = null;
			this._context = null;

			// ... and force the browser to the new URL.
			// we need to biff the current history entry so that
			// the caps redirect is seamless. 
			this._browser_callback.webNavigation.loadURI(s,
				128, // LOAD_FLAGS_REPLACE_HISTORY
				null, null, null);
			return;
		}

		// we are being asked to display the caps, so squirt it out
		// to the _listener, and conclude.
		var x = "";
		for (var i in w) {
			if (w.hasOwnProperty(i)) {
				var v = w[i];
				if (v == true)
					v = "true";
				if (v == false)
					v = "false";
				x += i + " ==> " +w[i] + "<br>\n";
			}
		}
		this._feedBeast(request, this._listener, this._context,
			x);
		this._listener.onStopRequest(request, this._context, status);
		this._listener = null;
		this._context = null;
	},

	// nsIStreamListener
	onDataAvailable : function(request, context, stream, offset, count) {
		var nbuf;

		if (!this._sstream) {
			// create (and cache) our scriptable input stream
			// note: this is NOT BINARY SAFE (but Gopher is
			//	mostly 7-bit)
			this._sstream = Cc["@mozilla.org/scriptableinputstream;1"]
				.createInstance(Ci.nsIScriptableInputStream);
			this._sstream.init(stream);
		}
		nbuf = this._sstream.read(count);
		this._buf += nbuf;
	},

	// nsIStreamConverter
	convert : function(from, to, listener, context) {
		// synchronous conversion will not be supported w/o good reason
		throw Cr.NS_ERROR_NOT_IMPLEMENTED;
	},
	asyncConvertData : function(from, to, listener, context) {
		// if this were a real translator, we'd uncomment this
//		if (from != ... || to != ...)
//			throw Cr.NS_ERROR_NOT_IMPLEMENTED;
		this._contentType = to;
		this._listener = listener;
		this._context = context;
	}
}

/*
 * the dotless object handles things like svg and xml that barf on the
 * terminal period that certain servers emit, and is also nice for
 * text/plain. it is similar to Caps and implements the same interfaces.
*/

function OverbiteDotless() { }
OverbiteDotless.prototype = {

	// my stuff
	_contentType: null,
	_listener : null,
	_context : null,
	_buf : '',
	_sstream : null,

	// useful internal functions

	// feed the beast (i.e., the listener on the other end)
	_feedBeast : function(request, listener, context, what) {
		// create a new instance each time (instead of reusing one)
		// just to make sure the previous instance has time to finish
		var stringstream = Cc["@mozilla.org/io/string-input-stream;1"]
			.createInstance(Ci.nsIStringInputStream);
		stringstream.setData(what, what.length);
		listener.onDataAvailable(request, context,
			stringstream, 0, what.length);
	},
	// charAtEnd allows easier snarfing of trailing characters
	_charAtEnd : function(index) {
		return this._buf.charAt(this._buf.length - index);
	},
	_lopOff : function(index) {
		OverbiteLog("dotless removed "+index+" characters");
		this._buf = this._buf.substr(0, this._buf.length - index);
		OverbiteLog("now "+this._buf.length+" long");
	},

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
						nsIStreamListener,
						nsIStreamConverter,
						nsIRequestObserver]),

	// nsIRequestObserver
	onStartRequest : function(request, context) {
		// init the channel with data and content type

		var chan = request.QueryInterface(nsIChannel);
		if (chan)
			chan.contentType = this._contentType;
				// from asyncConvertData
		this._context = context;
		this._buf = '';
		this._listener.onStartRequest(request, context);
	},

	/* this is a very simple construct with a rolling buffer. essentially
	   we start by priming the pump with a small one character output,
	   then delaying and buffering data until the last pulse of data and
	   onStopRequest gets called, which if there is a final dot, should
	   be in the leftover buffer. */

	onStopRequest : function(request, context, status) {
		// trim the end of _buf
		if (this._buf.indexOf(".")) { // don't bother if there's no dot
			// check for very specific sequences
			var l = this._buf.length;
			if (l == 1)
				this._buf = ""; // must be a bare dot
			else if (this._buf == ".\r\n" || this._buf == ".\n")
				this._buf = "";
			else if (this._buf == "\n." || this._buf == "\r\n.")
				this._buf = "";
			else if (this._charAtEnd(1) == "." &&
					this._charAtEnd(2) == "\n"){
				this._lopOff(1);
			} else if (this._charAtEnd(1) == "\n") {
				if (l > 3 && this._charAtEnd(2) == "\r" &&
						this._charAtEnd(3) == "." &&
						this._charAtEnd(4) == "\n") {
					this._lopOff(3);
				} else if (l > 2 && this._charAtEnd(2) == "."
						&& this._charAtEnd(3) == "\n") {
					this._lopOff(2);
				}
			}
			// otherwise must be some other construction
		}
		this._feedBeast(request, this._listener,
			this._context, this._buf)
				if (this._buf.length);	
		this._buf = '';
		this._listener.onStopRequest(request, this._context, status);
		this._listener = null;
		this._context = null;
		if (this._sstream)
			this._sstream.close();
		this._sstream = null;
	},

	// nsIStreamListener
	onDataAvailable : function(request, context, stream, offset, count) {
		var nbuf;

		if (!this._sstream) {
			// create (and cache) our scriptable input stream
			// note: this is NOT BINARY SAFE (but Gopher is
			//	mostly 7-bit)
			this._sstream = Cc["@mozilla.org/scriptableinputstream;1"]
				.createInstance(Ci.nsIScriptableInputStream);
			this._sstream.init(stream);
		}
		nbuf = this._sstream.read(count);
		if (this._buf.length) {
			this._feedBeast(request, this._listener, this._context,
				this._buf);
			this._buf = nbuf;
		} else {
			if (count == 1) { // degenerate case
				this._feedBeast(request, this._listener,
					this._context, nbuf);
				// come back for more
			} else {
				this._buf = nbuf.substr(1);
				this._feedBeast(request, this._listener,
					this._context, nbuf.substr(0,1));
			}
		}
	},

	// nsIStreamConverter
	convert : function(from, to, listener, context) {
		// synchronous conversion will not be supported w/o good reason
		throw Cr.NS_ERROR_NOT_IMPLEMENTED;
	},
	asyncConvertData : function(from, to, listener, context) {
		// if this were a real translator, we'd uncomment this
//		if (from != ... || to != ...)
//			throw Cr.NS_ERROR_NOT_IMPLEMENTED;
		this._contentType = to;
		this._listener = listener;
		this._context = context;
	}
}

/*
 * the converter object for turning a gopher directory into HTML. we no
 * longer use HTTP_INDEX because frankly it can't handle all that gopher
 * offers.
*/

function OverbiteDirconv() { }
OverbiteDirconv.prototype = {

	// my stuff
	_listener : null,
	_context : null,
	_buf : '',
	_pbuf : '',
	_sstream : null,
	_divlevel : 0,

	/* l10n/i18n support and string bundles */

	// for strings that are already Unicode (like our localization)
	_unicodeEntityFix : function(what) {
		// needed to ampersand-encode ALT text and other stuff
		return "&#" + what.charCodeAt(0) + ";";
	},
	_unicodeStringFix : function(what) {
		var q;
		q = what;
		q = q.replace(/([\u0080-\uFFFF])/g, this._unicodeEntityFix);
		return q;
	},

	// we load both itypes and obff because we use both of them now.
	_bundle : Cc["@mozilla.org/intl/stringbundle;1"]
		.getService(Ci.nsIStringBundleService)
		.createBundle(OBFFCHROMEURL + "/locale/itypes.properties"),
	_getL10NString : function(msg, args) {
		var q;
		OverbiteLog("string query: "+msg);

		if (args) 
			return this._unicodeStringFix(
				this._bundle.formatStringFromName(msg, args,
					args.length));
		else
			return this._unicodeStringFix(
				this._bundle.GetStringFromName(msg));
	},
	_obundle : Cc["@mozilla.org/intl/stringbundle;1"]
		.getService(Ci.nsIStringBundleService)
		.createBundle(OBFFCHROMEURL + "/locale/obff.properties"),
	_ogetL10NString : function(msg, args) {
		if (args) 
			return this._obundle.formatStringFromName(msg, args,
				args.length);
		else
			return this._obundle.GetStringFromName(msg);
	},

	_itypes :  [ '0', '1', '2', '3', '4', '5', '6',
			'7', '8', '9', 'g', 'I', 's', 'h', ';',
			'p', 'd', 'T' ]  ,

	// useful internal functions

	// feed the beast (i.e., the listener on the other end)
	_feedBeast : function(request, listener, context, what) {
		// create a new instance each time (instead of reusing one)
		// just to make sure the previous instance has time to finish
		var stringstream = Cc["@mozilla.org/io/string-input-stream;1"]
			.createInstance(Ci.nsIStringInputStream);
		stringstream.setData(what, what.length);
		listener.onDataAvailable(request, context,
			stringstream, 0, what.length);
	},
	// mungers and twisters to convert RFC-1436 cdata into valid SGML
	_dsSpaceFix : function(what) {
		var wout = what;
		wout = wout.replace(/  /g,		"&nbsp;&nbsp;");
		wout = wout.replace(/ \&nbsp;/g,	"&nbsp;&nbsp;");
		wout = wout.replace(/\&nbsp; /g,	"&nbsp;&nbsp;");
		wout = wout.replace(/^ /,		"&nbsp;");
		return wout;
	},
	_entityFix : function(what) {
		var wout = what;
		wout = wout.replace(/\&/g, "&amp;");
		wout = wout.replace(/>/g, "&gt;");
		wout = wout.replace(/</g, "&lt;");
		return wout;
	},

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
						nsIStreamListener,
						nsIStreamConverter,
						nsIRequestObserver]),

	// nsIRequestObserver
	onStartRequest : function(request, context) {
		var whoami = request.name;
		var twhoami = '';
		var rootbutt = '';
		this._divlevel = 0;

		if (whoami.asciiSpec) { // is this actually an nsIURI? YES!!
			if (whoami.path != "/" &&
					whoami.path != "" &&
					whoami.path != "1" &&
					whoami.path != "/1" &&
					whoami.path != "/1/") {
				var rooturl = OverbiteURLFromSel(whoami.host,
					whoami.port, '1', '');
				var capupurl = OverbiteCapsUniversalBackURL(
					whoami.host, whoami.port, '',
					whoami.path.substr(1));
				if (capupurl.length) capupurl =
'<a href="' + capupurl + 
'"><img class="gicon" src="gopher:///internal-up.png" '+
'alt="[' + this._getL10NString('uplev') + ']" '+
'title="[' + this._getL10NString('uplev') + ']"></a>' ;

				rootbutt =
'<div id = "buttonarea"><nobr>' + capupurl +
'<a href = "' + rooturl + '">' +
'<img class = "gicon" src = "gopher:///internal-root.png" '+
'alt="[' + this._getL10NString('backpath') + ']" '+
'title="[' + this._getL10NString('backpath') + ']"></a></nobr></div>' +
"\n";
			}
			whoami = whoami.asciiSpec;
		}
		if (whoami && whoami.length) {
			twhoami = ": "+whoami;
			if (whoami.indexOf("?") > -1)
				whoami = ((whoami.split("?"))[0]) + "?...";
			else if (whoami.indexOf("%09") > -1)
				whoami = ((whoami.split("%09"))[0]) + "?...";
		}
		if (!whoami)
			whoami = '';
			
		var ibuf = 
'<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN"' + "\n" +
' "http://www.w3.org/TR/html4/strict.dtd">' + "\n" +
"<html>\n"+
"<head>\n"+
'<link rel="stylesheet" href = "gopher:///internal-gopherchrome.css" ' +
	'type="text/css"/>' + "\n" +
'<link rel="icon" href = "gopher:///internal-favicon.png" '+
	'type="image/png"/>' + "\n" +
"<title>Gopher document" + twhoami + "</title>\n"+
'<script type="text/javascript" src="gopher:///internal-interface.js"></script>'+
'<script type="text/javascript">'+
'l(0, "'+encodeURI(this._ogetL10NString("search"))+'");' +
'l(1, "'+encodeURI(this._getL10NString("preview"))+'");' +
"</script></head>\n"+
"<body>\n"+
'<div id = "everything">'+
'<div id = "topbar">'+
'<div id = "urlparent">'+
'<div id = "urlarea"><span class = "purl">' + whoami + '</span></div></div>'
	+ "\n" +
rootbutt +
'</div>'+
'<div id = "contentarea">' + "\n" +
'<table>' + "\n";

		// init the channel with data and content type
		var chan = request.QueryInterface(nsIChannel);
		if (chan)
			chan.contentType = 'text/html';
		this._context = context;
		this._listener.onStartRequest(request, context);
		this._feedBeast(request, this._listener, context, ibuf);

	},

	onStopRequest : function(request, context, status) {
		this._buf +=
"</table>\n"+
"</div>\n"+
"</div>\n"+
"</body>\n"+
"</html>\n";

		this._feedBeast(request, this._listener,
			this._context, this._buf);
		this._buf = '';
		this._listener.onStopRequest(request, this._context, status);
		this._listener = null;
		this._context = null;
		if (this._sstream)
			this._sstream.close();
		this._sstream = null;
	},

	// nsIStreamListener
	onDataAvailable : function(request, context, stream, offset, count) {
		var i;
		var obuf = '';

		if (!this._sstream) {
			// create (and cache) our scriptable input stream
			// note: this is NOT BINARY SAFE (but Gopher is
			//	mostly 7-bit)
			this._sstream = Cc["@mozilla.org/scriptableinputstream;1"]
				.createInstance(Ci.nsIScriptableInputStream);
			this._sstream.init(stream);
		}
		this._pbuf += this._sstream.read(count);
		while((i = this._pbuf.indexOf("\n")) > -1) {
			// pull the next tab-delimited string off the buffer
			var w = this._pbuf.substr(0, i);
			if (i < this._pbuf.length)
				this._pbuf = this._pbuf.substr(i+1);
			else
				this._pbuf = '';

			w = w.replace("\r", "");
			w = w.replace("\n", "");

			var itype = w.substr(0,1);

			var attribs = w.substr(1).split("\t");
			var ds = this._dsSpaceFix(this._entityFix(
				attribs.shift()));
			if (!ds.length)
				ds = "&nbsp;";

			var sel = attribs.shift();
			if (!sel)
				sel = '';

			var host = attribs.shift();
			if (host && host.length)
				host = encodeURI(host);
			var port = parseInt(attribs.shift());
			if (isNaN(port))
				port = 0; // falls through to bogosity filter
			var icalt = (this._itypes.indexOf(itype) > -1)
				? this._getL10NString(itype)
				: this._getL10NString('unknown');
			var iconbase = "gopher:///internal-";

			if (itype == "'" || itype == '"') {
				// these are just going to cause all kinds of
				// problems, so they are simply suppressed
				obuf +=
'<!-- suppressed problematic item type '+escape(itype)+" -->\n";
			} else if (itype == 'i' || itype == '3') {
				var lclass = (itype == '3') ? 'erroritem'
					: 'infoitem';
				if (itype == '3') {
					obuf += 
				'<tr><td class = "gicon">'
				+ '<img src = "' + iconbase + 'icn3.png" '
				+ 'alt = "[' + icalt + ']" '
				+ 'title = "[' + icalt + ']" '
				+ 'class = "gicon"></a></td>';
				//+ 'border = "0"></a></td>';
				} else
					obuf += "<tr><td></td>";
				obuf += '<td class = "ds">'
					+ '<span class = "' + lclass + '">'
					+ ds + "</span></td></tr>\n";
			} else if (host && host.length && port > 1) {
				var suburl;
				var icon = "icn.png";
				var lclass = "fileitem";
				var spannable = 0;

				if (itype == '8' || itype == 'T') {
					// don't let them inject
					// arbitrary HTML with "
					sel = sel.replace(/["']/g, "");
					suburl = "telnet://" + host + ":" +
						port + "/" + sel;
					var icon = "icn" + itype + ".png";
					var lclass = "telnetitem";
					icalt = this._getL10NString('telnet');
				} else if (itype == 'h' &&
					(sel.substr(0,4) == "URL:" ||
						sel.substr(0,5) == "/URL:")) {
					var subn = (sel.substr(0,1) == "/")
						? 5 : 4;
					suburl = encodeURI(sel.substr(subn));
					if (suburl.match(/^javascript:/) ||
						suburl.match(/^data:/)) {
						suburl = "";
						ds +=
			' <b>(' + this._getL10NString('unsafeurl') + ')</b>';
					}
					icon = "icnhURL.png";
					lclass = "urlitem";
					icalt = this._getL10NString('exturl');
				} else if (port >= 80 && itype == 'h' &&
						sel.match(/^[A-Z]+(%20| )/)) {
					sel = sel.replace(/^[A-Z]+(%20| )/,'');
					suburl = "http://"+host+":"+port+
						(sel.substr(0,1) == "/" ?
							"" : "/") +
						encodeURI(unescape(sel));
					icon = "icnhURL.png";
					lclass = "urlitem";
					icalt = this._getL10NString('exturl');
				} else {
					suburl = OverbiteURLFromSel(host, port,
						itype, sel);

					if (itype == "g"
						|| itype == "h"
						|| itype == "p"
						|| itype == "I"
						|| itype == "7"
						|| itype == "0") spannable = 1;
					this._divlevel += spannable;
					var dl = this._divlevel;

					// attempt to escape weird itypes
					var eitype = escape(itype);
					if (eitype.length > 1) // it was
						eitype = eitype.substr(1)
							.toLowerCase();
					icon = (this._itypes.indexOf(itype)>-1)
						? ("icn"+eitype+".png")
						: "icn.png";
					lclass = 
						(itype == '7' || itype == '2')
							? 'searchitem':
						(itype == '1') ? 'diritem' :
							'fileitem';
				}
					obuf +=
				'<tr><td class = "gicon"><nobr>';
					if (spannable) obuf +=
'<script type="text/javascript">w("'+suburl+'","'+itype+'",'+dl+')</script>';
					obuf +=
				'<a href = "'+ suburl +'">'
				+ '<img src = "' + iconbase + icon +'" '
				+ 'alt = "[' + icalt + ']" '
				+ 'title="'+icalt+'" '
				+ 'class="gicon"></a></nobr></td>'
				+ '<td class = "ds">'
				+ '<a href = "'+suburl
+ ((itype == '7' || itype == '2') ? ('" onClick="return x('+"'"+suburl+"','"+itype+"',"+dl+')') : '')
				+ '" title="'+icalt+'">'
				+ '<span class = "' + lclass + '">'
				+ ds + "</span></a></td></tr>\n";
					if (spannable) obuf +=
				'<tr><td class="gicon"></td><td class="ds">'+
				'<div id="pv'+dl+'"></div></td></tr>'+"\n";

			} else {
				obuf += // no point in localizing this, I think
				"<!-- bogus element suppressed -->\n";
			}
		}
		this._feedBeast(request, this._listener, this._context, obuf);
	},

	// nsIStreamConverter
	convert : function(from, to, listener, context) {
		// synchronous conversion will not be supported w/o good reason
		throw Cr.NS_ERROR_NOT_IMPLEMENTED;
	},
	asyncConvertData : function(from, to, listener, context) {
		// if this were a real translator, we'd uncomment this
//		if (from != "text/x-overbite-gopher-dir" || to != "text/html")
//			throw Cr.NS_ERROR_NOT_IMPLEMENTED;
		this._listener = listener;
		this._context = context;
	}
	
};

/*
 * the channel object that actually does the protocol negotiation.
*/

function OverbiteChannel(input_uri, proxyinfo, loadinfo) {
	this.init(input_uri, proxyinfo, loadinfo);
}

OverbiteChannel.prototype = {

	// my stuff
	_itemtype : null,
	_selector : null,
	_host : null,
	_port : null,
	_transport : null,
	_proxyinfo : null,
	_pump : null,
	_listener : null,
	_context : null,
	_progsink : null,
	_browser_callback : null,
	_is_caps : false,

	// for bug 769764
	_proxyresolveflags : null,
	_proxy_uri : null,

	csoargs : '',
	queryargs : '',
	transreq : null, // actual transmitted request (see sendRequest)

	// we only need itypes for the notification helper.
	_bundle : Cc["@mozilla.org/intl/stringbundle;1"]
		.getService(Ci.nsIStringBundleService)
		.createBundle(OBFFCHROMEURL + "/locale/itypes.properties"),
	_getL10NString : function(msg, args) {
		var q;
		OverbiteLog("string query: "+msg);

		if (args) 
			return this._bundle.formatStringFromName(msg, args,
					args.length);
		else
			return this._bundle.GetStringFromName(msg);
	},
	// for strings that are already Unicode (like our localization)
	_unicodeEntityFix : function(what) {
		// needed to ampersand-encode ALT text and other stuff
		return "&#" + what.charCodeAt(0) + ";";
	},
	_unicodeStringFix : function(what) {
		var q;
		q = what;
		q = q.replace(/([\u0080-\uFFFF])/g, this._unicodeEntityFix);
		return q;
	},

	// nsISupports
	// we can simplify this later; see nsIProxiedProtocolHandler
	QueryInterface : XPCOMUtils.generateQI(((Ci.nsIProtocolProxyCallback)
	? [nsISupports, nsIChannel, nsIRequest, Ci.nsIProtocolProxyCallback]
	: [nsISupports, nsIChannel, nsIRequest])),

	// nsIRequest
	loadFlags : 0,
	name : null,
	_pending : false,
	isPending : function() { return this._pending; },
	_status : Cr.NS_OK,
	_loadGroup : null,

	get status() {
		if (this._pump)
			return (this._pump.status);
		else
			return this._status;
	},
	set status(status) { this._status = status; },
	cancel : function(status) {
		if (this._pump)
			return (this._pump.cancel(status));
		else
			return Cr.NS_OK;
	},
	suspend : function() {
		if (this._pump)
			return (this._pump.suspend());
		else
			return Cr.NS_OK;
	},
	resume : function() {
		if (this._pump)
			return (this._pump.resume());
		else
			return Cr.NS_OK;
	},

	// nsIChannel
	loadInfo : null,
	loadAttributes : null,
	contentCharset : null,
	contentLength : -1,
	contentType : null,
	_notificationCallbacks : null ,
	originalURI : null,
	owner : null,
	URI : null,
	init : function(input_uri, proxyinfo, loadinfo) {
		// constructor
		this.URI = input_uri;
		this.originalURI = input_uri;
		this.name = input_uri;
		if (proxyinfo)
			this._proxyinfo = proxyinfo;

		// process item type and set contentType
		var sel = decodeURI(input_uri.path);
		if (sel == null || !sel.length || sel == '/') {
			this._itemtype = '1';
			this._selector = '';
		} else if (sel.length == 1) {
			this._itemtype = sel;
			this._selector = '';
		} else {
			this._itemtype = sel.substr(1,1);
			this._selector = sel.substr(2);
		}

		if (!input_uri.host || !input_uri.host.length)
			throw Cr.NS_ERROR_MALFORMED_URI;
		else
			this._host = input_uri.host;
		if (!input_uri.port || input_uri.port < 1)
			this._port = 70;
		else
			this._port = input_uri.port;

		// force our itemtype. realistically, the old Gopher let people
		// slide a lot with content sniffing, but that's not going to
		// happen anymore. this is written to be as rigid as possible
		// for a particular itemtype (but see type I, sigh).
		var c;
		switch(this._itemtype) {
			case '1' :
			case '7' :
				c = 'application/x-overbite-gopher-dir';
				// this lets us override 1.9's gopher support
				break;

			case 'c' :
				c = 'text/css';
				break;

			case 'x' :
				c = 'application/xml'; // I miss text/xml
				break;

			case '0' :
			case '2' :
				c = 'text/plain';
				break;

			case 'g' :
				c = 'image/gif' ;
				break;

			case 'I' :
				// oh, man, this is gross -- designed to
				// support both the common use of I for
				// JPEG images, and the official spec that
				// I is a 'general image type' (we only
				// support the ones Mozilla will display)
				//
				// taken from netwerk/mime/public/nsMimeTypes.h
				if (this._selector.match(/\.jpe?g$/i))
					c = 'image/jpeg' ;
				else if (this._selector.match(/\.gif$/i))
					c = 'image/gif' ; // grrRRR! use 'g'!!
				else if (this._selector.match(/\.xbm$/i))
					c = 'image/x-xbitmap' ;
				else if (this._selector.match(/\.png$/i))
					c = 'image/png' ;
				else if (this._selector.match(/\.svg$/i))
					c = 'image/svg+xml' ;
				else if (this._selector.match(/\.bmp$/i))
					c = 'image/bmp' ;
				else if (this._selector.match(/\.icon?$/i))
					c = 'image/x-icon' ;
				else if (this._selector.match(/\.tiff?$/i))
					c = 'image/tiff' ;
				else
					c = 'image/jpeg' ;
					// this broke too many things
					//c = 'application/octet-stream';
				break;

			case 'h' :
				c = 'text/html';
				break;

			case 'p' :
				c = 'image/png';
				break;

			case 'd' :
				c = 'application/pdf';
				break;

			case '8' :
			case 'T' :
				throw Cr.NS_ERROR_NOT_IMPLEMENTED;
				break;

			default :
				c = 'application/octet-stream';
				break;
		}
		this.contentType = c;
		this.loadInfo = loadinfo;

		OverbiteLog(("channel initialized: "+
			this._host + " " +
			this._port + " " +
			this._itemtype + " " +
			this._selector + " " +
			""));
		return Cr.NS_OK;
	},
			
	/* open is not being implemented */
	open : function() {
		throw Cr.NS_ERROR_NOT_IMPLEMENTED;
	},

	asyncOpen2 : function (listener) {
		return asyncOpen(listener, null);
	},

	asyncOpen : function (listener, context) {
		this._listener = listener;
		this._context = context;

		// we can simplify this later; see nsIProxiedProtocolHandler
		if (Ci.nsIProtocolProxyCallback) {
			// if we can do asynchronous proxy resolution, ask
			// the proxy service what to do
			var nsPPS = Cc["@mozilla.org/network/protocol-proxy-service;1"]
				.getService(Ci.nsIProtocolProxyService);
			OverbiteLog(("async proxy resolution in progress"));
			nsPPS.asyncResolve(this.URI, this._proxyresolveflags,
				this);
			OverbiteLog(("waiting for proxy"));
		} else {
			// otherwise assume already synchronously resolved
			OverbiteLog(("no proxy callback needed"));
			this.onProxyAvailable(null, null, this._proxyinfo,
				Cr.NS_OK);
		}
	},

	onProxyAvailable : function(request, uri, proxyinfo, status) {
		if (proxyinfo)
			OverbiteLog(("proxyinfo: "+proxyinfo));

		this._proxyinfo = proxyinfo;
		OverbiteLog(("proxyinfo on; trying to initialize transport"));

		var transportService = Cc["@mozilla.org/network/socket-transport-service;1"]
			.getService(Ci.nsISocketTransportService);
		this._transport = transportService
			.createTransport(null, 0,
				this._host,
				this._port,
				this._proxyinfo);
		if (!(this.loadFlags & nsIRequest.LOAD_BACKGROUND)) {
			// hook up our event sink to the current UI thread
			var cq = Cc["@mozilla.org/thread-manager;1"]
				.getService().currentThread;
			OverbiteLog("yes, we have sink "+cq);
			this._transport.setEventSink(this, cq);
		}
		// open and initialize the data pump to read from the socket
		var sinput =
			this._transport.openInputStream(0,0,0);
		this.sendRequest();
		this._pump = Cc["@mozilla.org/network/input-stream-pump;1"].
			createInstance(Ci.nsIInputStreamPump);
		this._pump.init(sinput, -1, -1, 0, 0, true);
		this._pump.asyncRead(this, null);
		if (this._loadGroup) {
			this._loadGroup.addRequest(this, null);
			OverbiteLog("load group added");
		}
		this._pending = true;

		// if this is caps, start it and abort now
		if (this._is_caps) {
			OverbiteLog("caps request in progress");
			var caps = new OverbiteCaps();
			caps._host = this._host;
			caps._port = this._port;
			caps._itemtype = this._itemtype;
			caps._selector = this._selector;
			caps.asyncConvertData(
				'generic/caps',
				this.contentType,
				this._listener,
				this._context);
			this._listener = caps;
			this._context = null;
			caps._browser_callback = this._browser_callback;
			OverbiteLog(("now with caps: "+caps));
			return Cr.NS_OK;
		}

		// push on another content listener (in this case us) for
		// those itemtypes requiring translation to something else
		var transitives = [ '1', '7' ]; // item types for translation
		if (transitives.indexOf(this._itemtype) > -1) {
			OverbiteLog(("this type requires translation"));
			var dirconv = new OverbiteDirconv();
			dirconv.asyncConvertData(
				'application/x-overbite-gopher-dir',
				'text/html',
				this._listener,
				this._context);
			this._listener = dirconv;
			this._context = null;
			OverbiteLog(("now with dirconv: "+dirconv));
		}	
		// dotless conversion
		var prefs = Cc["@mozilla.org/preferences-service;1"]
			.getService(Ci.nsIPrefBranch);
		var do_dotless = true;
		// first load hidden preference for dotless
		if (prefs.getPrefType(OBFFDOTLESSPREF) == prefs.PREF_BOOL) {
			do_dotless = prefs.getBoolPref(OBFFDOTLESSPREF);
			OverbiteLog(("dotless has been set to "+do_dotless));
			// otherwise default is true
		} else {
			OverbiteLog(("no "+OBFFDOTLESSPREF+" not bool"));
		}
		if (do_dotless && (
				this.contentType.match(/^text\//) ||
				this.contentType == 'application/xml' ||
				this.contentType == 'image/svg+xml' ||
			0)) {
			OverbiteLog("this type needs to be dotless");
			var dotless = new OverbiteDotless();
			dotless.asyncConvertData(
				'generic/dotless-type',
				this.contentType,
				this._listener,
				this._context);
			this._listener = dotless;
			this._context = null;
			OverbiteLog(("now with dotless: "+dotless));
		}
		OverbiteLog(("transport service for "+
			this._host + " initialized"));
		return Cr.NS_OK;
	},

	get loadGroup() { return this._loadGroup; },
	set loadGroup(loadGroup) {
		this._loadGroup = loadGroup;
		this._progsink = null;
	},
	get notificationCallbacks() {
		return this._notificationCallbacks;
	},
	set notificationCallbacks(nc) {
		this._notificationCallbacks = nc;
		this._progsink = null;
	},
	get securityInfo() {
		if (this._transport)
			return this._transport.securityInfo;
		//throw Cr.NS_ERROR_NOT_AVAILABLE;
		return null;
	},
	// set securityInfo? bwahahaha
	set securityInfo(foo) {
		throw Cr.NS_ERROR_NOT_IMPLEMENTED;
		return null;
	},

	
	onStartRequest : function(request, context) {
		if (this._listener)
			this._listener.onStartRequest(this,
				this._context);
		OverbiteLog(("onStartRequest"+this._listener));
	},
	onStopRequest : function(request, context, status) {
		OverbiteLog(("onStopRequest: "+status));
		if(Components.isSuccessCode(status))
			this.status = status;

		if (this._listener) {
			this._listener.onStopRequest(this,
					this._context,
					this.status);
			OverbiteLog("listener stopped");
			this._listener = null;
			this._context = null;
		}
		if (this._loadGroup) {
			this._loadGroup.removeRequest(this,
				context, // null,
				this.status);
			OverbiteLog("load group stopped");
		}

		this._pump = null;
		this._transport.close(this.status);
		this._transport = null;
		this.notificationCallbacks = null; // use our own getter/setter
		this._progsink = null;
		this._pending = false;

		OverbiteLog("end of request");

		// Attach a notification box for navigation.
		// Attach only iff:
		// - the tab URL matches ours. (We could be in an iframe.)
		// - there is no notification box there already.
		// - this is not a gopher menu. We do this in the HTML instead.
		// Backing up to a menu doesn't restore this box, so we should
		// only do this to a terminal document.
		var transitives = [ '1', '7' ]; // item types that are menus
		if (transitives.indexOf(this._itemtype) == -1) {
		try { // no meaning if this fails.
			OverbiteLog("attempting to load nav bar "+this.originalURI.spec);
			// XXX try selected tab first for speed?

			// Iterate over all windows and tabs to find us.
			// If there are multiple instances, add the box; it
			// won't hurt (as long as we didn't before).

			var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
				.getService(Ci.nsIWindowMediator);
			var be = wm.getEnumerator("navigator:browser");
			while(be.hasMoreElements()) {
				var win = be.getNext();
				var tabb = win.gBrowser;

				// check each tab
				var tabs = tabb.browsers.length;
				for (var i=0; i<tabs; i++) {
					var cb = tabb.getBrowserAtIndex(i);
					OverbiteLog("found "+cb.currentURI.spec);
					var notifo =
						tabb.getNotificationBox(cb);
			if (this.originalURI.spec == cb.currentURI.spec &&
			notifo &&
			!notifo.getNotificationWithValue('gopher-nav')) {
						// got one!
						OverbiteLog("found tab");
						var homeURL = "gopher://"+
		this._host + (this._port != "70" ? ":"+this._port : '') + "/";
						var backURL =
	OverbiteCapsUniversalBackURL(this._host, this._port, this._itemtype,
							this._selector);

						var tb = cb;
						var set_to_home =
						function(n, b) {
			tb.webNavigation.loadURI(homeURL, 0, null, null, null);
						};
						var set_to_up = function(n, b){
			tb.webNavigation.loadURI(backURL, 0, null, null, null);
						};
						var buttons = [ {
							label:
						this._getL10NString('backpath'),
							accessKey: 'B',
							popup: null,
							callback: set_to_home
						} ];
						if (backURL && backURL.length)
						buttons.push( {
							label:
						this._getL10NString('uplev'),
							accessKey: 'U',
							popup: null,
							callback: set_to_up
						} );
						notifo.appendNotification(
this._host + (this._port != "70" ? ":"+this._port : '') +
" - " +
this._getL10NString(this._itemtype), // oh, master, I hope you set this right!
							"gopher-nav",
							null,
						notifo.PRIORITY_INFO_LOW,
							buttons);
					}
				}
			}
			OverbiteLog("done iterating for nav bar");
		} catch(e) { OverbiteLog(e); }
		}

		// lookit nsNetError.h
		return Cr.NS_OK;
	},
	onDataAvailable : function(request, context, inputStream, offset,
			count) {
		OverbiteLog(("data event"));
		if (this._listener) {
			this._listener.onDataAvailable(this,
					this._context,
					inputStream,
					offset,
					count);
		}
		OverbiteLog(("data available: "+count+" bytes"));
	},
	sendRequest : function() {
		var transtring = this._selector;

		// the original version put up the itemtype 7 dialogue
		// at the channel, but I like throwing ABORTs early,
		// so we're doing that at the protocol handler level.
		// ditto for CSO/ph searches.
		if (this.csoargs && this.csoargs.length) {
			// completely replace selector with CSO/ph query
			// this eliminates a lot of headaches!!
			if (!this.csoargs.match(/^query /i)) // already query?
				transtring="query "+this.csoargs+" return all";
			else
				transtring=this.csoargs;
		} else if (this.queryargs && this.queryargs.length)
			transtring += "\t" + this.queryargs;
		transtring += "\r\n";
		// add terminating quit command to our query just in case
		// if this is CSO/ph
		if (this._itemtype == "2")
			transtring += "quit\r\n";

		// if this is caps, send something totally different
		if (this._is_caps)
			transtring = "caps.txt\r\n";

		// send the data
		var outstream = this._transport
			.openOutputStream(0,0,0);
		outstream.write(transtring, transtring.length);
		this.transreq = transtring; // for debugging
		outstream.close();
		OverbiteLog("selector sent: "+escape(transtring));
	},
	onTransportStatus : function(trans, status, prog, progmax) {
		this._progsink = OverbiteQNC(this, this._loadGroup,
				this._progsink);
		if (!this._progsink)
			OverbiteLog(("crap: no progsink"));
		OverbiteLog(("status changed: "+status+this._loadGroup+
			this.notificationCallbacks+this._progsink));
		if (this._progsink &&
		// wtf?! this doesn't work, so I'm commenting it out
		//		Components.isSuccessCode(status) &&
		//		this._pump &&
			!(this.loadFlags & nsIRequest.LOAD_BACKGROUND)){
			this._progsink.onStatus(this, 
				this._context,
				status,
				this.URI.asciiHost);
			OverbiteLog(("onStatus"));
			
			if (status == nsISocketTransport.STATUS_RECEIVING_FROM
				||
				status == nsISocketTransport.STATUS_SENDING_TO
				) {
				this._progsink.onProgress(this,
					this._context,
					prog, -1);
				OverbiteLog(("onProgress"));
			}
		}
		return Cr.NS_OK;
	}

};
	
function OverbiteProtocol() {
}

OverbiteProtocol.prototype = {

	// our stuff

	// l10n/i18n support -- load our string bundles
	_bundle : Cc["@mozilla.org/intl/stringbundle;1"]
		.getService(Ci.nsIStringBundleService)
		.createBundle(OBFFCHROMEURL + "/locale/obff.properties"),
	_getL10NString : function(msg, args) {
		if (args) 
			return this._bundle.formatStringFromName(msg, args,
				args.length);
		else
			return this._bundle.GetStringFromName(msg);
	},

	// return the current browser instance for manipulation
	_getBrowser : function() {
		return Cc["@mozilla.org/appshell/window-mediator;1"]
				.getService(Ci.nsIWindowMediator)
				.getMostRecentWindow('navigator:browser')
				.getBrowser();
	},

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
                                                nsIObserver,
						nsIFactory,
						nsIProtocolHandler,
						nsIProxiedProtocolHandler]),

	classDescription : "Gopher protocol handler",
	contractID : OBFFPROT_HANDLER_CONTRACTID,
	classID : OBFFPROT_HANDLER_CID,

	// nsIFactory (singleton)
	createInstance : function(outer, iid) {
		if (outer) return Cr.NS_ERROR_NO_AGGREGATION;
		return this.QueryInterface(iid);
	},

	// nsIObserver
	observe : function(subject, topic, data) {
		// nothing yet
	},

	// nsIProtocolHandler
	scheme: OBFFSCHEME,
	defaultPort: 70,
	protocolFlags: 0
			| nsIProtocolHandler.ALLOWS_PROXY
			| nsIProtocolHandler.ALLOWS_HTTP_PROXY
			| nsIProtocolHandler.URI_LOADABLE_BY_ANYONE
		,

	allowPort : function(port, scheme) {
		// explicitly overridden -- these are common
		// and should never be blacklisted
		// we also include whois, finger and CSO/ph since gopher
		// necessarily subsumes all of those protocols very easily
		return (alwayslet.indexOf(port) != -1);
	},

	newURI : function(spec, charset, baseURI) {
		var uri = Cc["@mozilla.org/network/standard-url;1"]
			.createInstance(Ci.nsIURI);
		uri.spec = spec;
		return uri;
	},

	newChannel : function(input_uri) {
		OverbiteLog("new request for "+input_uri.asciiSpec);
		return this.newProxiedChannel(input_uri,null,null, null, null);
	},

	newChannel2 : function(input_uri, loadinfo) {
		OverbiteLog("new request2 for "+input_uri.asciiSpec);
		return this.newProxiedChannel(input_uri,null,null, null, null);
	},

	newChannelFromURI : function(input_uri) {
		OverbiteLog("new channel from URI "+input_uri.asciiSpec);
		return this.newProxiedChannel(input_uri,null,null, null, null);
	},

	newChannelFromURI2 : function(input_uri, loadnode, loadprin,
			trigprin, secflags, content_policy_type) {
		OverbiteLog("new channel2 from URI "+input_uri.asciiSpec);
		return this.newProxiedChannel(input_uri,null,null, null, null);
	},

	// nsIProxiedProtocolHandler 
	newProxiedChannel : function(input_uri, proxyinfo, proxyresolveflags,
			proxy_uri, loadinfo) {
		// see bug 769764. When we support only 18+, we can assume
		// that there is always an nsIProtocolProxyCallback and
		// then move proxyresolveflags and proxy_uri into the
		// ctor for OverbiteChannel. in practice, this should be
		// supported as far back as Gecko 1.8/Firefox 2.

		var ioService = Cc["@mozilla.org/network/io-service;1"]
			.getService(Ci.nsIIOService);
		var prompter = Cc["@mozilla.org/embedcomp/prompt-service;1"]
			.getService(Ci.nsIPromptService);
		var self = this;

		OverbiteLog("new proxied request for "+input_uri.asciiSpec);

		if (proxyinfo)
			OverbiteLog("proxy is: "+proxyinfo);

		// the CAPS pseudo-handler is triggered by the .caps
		// pseudo-domain. if we find that in the host, check the
		// cache. if cached, behave appropriately. if not, launch
		// a channel but stick this browser object in it so it knows
		// to call back.
		if (input_uri.host.match(/\.caps$/)) {
			// the cache key is the host + port, canonicalized
			// remove the caps part though!
			var IURL = input_uri.clone();
			IURL.host = IURL.host.substr(0, IURL.host.length - 5);

			var k = OverbiteCapsKey(IURL.host, IURL.port);
			var m = capscache[k];
			if (m) {
				// cache hit! we can redirect right now!
				OverbiteLogAlways("cache hit for caps: "+k);

				var newuri = OverbiteURLFromSel(
					IURL.host,
					IURL.port,
					'',
					OverbiteCapsBackPathFromSel(m,
						IURL.path.substr(1)));
				OverbiteLog("backing up to "+newuri);
				this._getBrowser().webNavigation.loadURI(
						newuri, 0, null, null, null);
				// this is a bit cheap, but we don't want
				// Fx freaking that we didn't give it a channel
				throw Cr.NS_ERROR_ABORT;
				return null; // above will do redirect
			} 
				

			// not cached. start a fetch and redirect.
			OverbiteLogAlways("no cache entry for "+k
				+", executing caps fetch for "+IURL.host);
			var ob = new OverbiteChannel(IURL, proxyinfo,
				loadinfo);
			ob._proxyresolveflags = proxyresolveflags;
			ob._proxy_uri = proxy_uri;
			ob._is_caps = true;

			// if the path ends in caps.txt, assume it is a caps
			// meta-fetch. otherwise, process as a backpath and
			// send this browser to it so it knows what to change.
			if (!input_uri.path.match(/caps\.txt$/))
				ob._browser_callback = this._getBrowser();
			return ob;
		}

		// handle hURLs directly here (and reject Javascript
			// and data:)
		if (input_uri.path.match(/^\/?h\/?URL:.+/)) {
			var newuri = input_uri.path.replace(
				/^\/?h\/?URL:\/?/, "");
			OverbiteLog("URL REDIRECT: "+newuri);

			// reject unsafe destination schemes
			if (newuri.match(/^javascript:/) ||
					newuri.match(/^data:/)) {
				OverbitePrompter("hurl", function() {
				prompter.alert(null,
					self._getL10NString('hurl.error.title',
						[newuri]),
					self._getL10NString('hurl.error'))
				});
				throw Cr.NS_ERROR_ABORT;
				return null;
			}

			var rv = OverbitePrompter("hurlwarning", function() {
			return prompter.confirm(null,
				self._getL10NString('hurl.warning.title'),
				self._getL10NString('hurl.warning', [newuri]))
			});
			if (rv) {
				this._getBrowser().webNavigation.loadURI(
						newuri, 0, null, null, null);
				// this is a bit cheap, but we don't want
				// Fx freaking that we didn't give it a channel
				throw Cr.NS_ERROR_ABORT;
				return null; // above will do redirect
			} else {
				throw Cr.NS_ERROR_ABORT;
				return null;
			}
		}

		// help clueless users who try to use anything but itemtype 1,
		// 2 or 7 for something that ends in a /. however, make sure
		// we aren't querying something.
		if (input_uri.path.substr(0,2) != "/1" &&
			input_uri.path.substr(0,2) != "/2" &&
			input_uri.path.substr(0,2) != "/7" &&
				input_uri.path.indexOf("?") == -1 &&
				input_uri.path.indexOf("%09") == -1 &&
					input_uri.path.length > 1 &&
		(input_uri.path.charAt(input_uri.path.length-1) == "/" ||
				input_uri.path.match(/^\/[^1]$/))) {
			var prefs = Cc["@mozilla.org/preferences-service;1"]
				.getService(Ci.nsIPrefBranch);
			// first load hidden preference for cluefulness
			if (prefs.getPrefType(OBFFFIXITYPEPREF) ==
					prefs.PREF_BOOL) {
				rv = prefs.getBoolPref(OBFFFIXITYPEPREF);
				// this is our default answer
			} else {
				rv = OverbitePrompter("swapitype", function() {
				return prompter.confirm(null,
					self._getL10NString('swapitype.title'),
					self._getL10NString('swapitype'))
				});
			}
			if (rv) {
				var newuri = "gopher://" + input_uri.host +
					((input_uri.port == 70 ||
					  input_uri.port == -1) ? "" :
						(":"+input_uri.port)) +
					"/1" + input_uri.path.substr(2);
				this._getBrowser().webNavigation.loadURI(
						newuri, 0, null, null, null);
				// this is a bit cheap, but we don't want
				// Fx freaking that we didn't give it a channel
				throw Cr.NS_ERROR_ABORT;
				return null; // above will do redirect
			} else {
				// INSERT COINS AND CONTINUE
			}
		}

		// handle itemtype 7 at this stage and turn into a channel
		// for itemtype 1 instead, except if this particular URL has
		// arguments (but wouldn't you want to do that as itype 1?).
		if (input_uri.path.substr(0,2) == "/7" &&
				input_uri.path.indexOf("?") == -1 &&
				input_uri.path.indexOf("%09") == -1) {
			// we will accept "blank" responses -- could be valid
			// use the global prompter objects
			var rv = OverbitePrompter("search", function() {
			return prompter.prompt(null,
				self._getL10NString('search.title'),
				self._getL10NString('search'),
				query, null, chequestub)
			});
			if (!rv) {
				throw Cr.NS_ERROR_ABORT;
				return null;
			}
			// stuff query into channel query args rather than
			// kludging it into a URL
			var ob = new OverbiteChannel(input_uri, proxyinfo,
				loadinfo);
			ob._proxyresolveflags = proxyresolveflags;
			ob._proxy_uri = proxy_uri;
			ob.queryargs = query.value;
			return ob;
		}

		// similarly handle itemtype 2
		// if it's /2....fjhgkrjgh then pass that on to the CSO server
		// (it's a fully qualified query that requires no parsing)
		if (input_uri.path == "/2" || input_uri.path == "/2/") {
			// unlike itype 7 we MUST enter a query for this
			var rv = OverbitePrompter("cso", function() {
				return prompter.prompt(null,
				self._getL10NString('cso.title'),
				self._getL10NString('cso'),
				query, null, chequestub)
			});
			if (rv && !query.value.length) // blank query
				OverbitePrompter("csobogus", function() {
				prompter.alert(null,
					self._getL10NString('csobogus.title'),
					self._getL10NString('csobogus'))
				});
			if (!rv || !query.value.length) {
				throw Cr.NS_ERROR_ABORT;
				return null;
			}
			// stuff query into channel csoargs
			var ob = new OverbiteChannel(input_uri, proxyinfo,
				loadinfo);
			ob._proxyresolveflags = proxyresolveflags;
			ob._proxy_uri = proxy_uri;
			ob.csoargs = query.value;
			return ob;
		}
			
		// make chrome channel either to images or CSS if
		// input_uri is "gopher:///internal-" and no / and
		// extension is .png, .css or .js
		if (!input_uri.host.length &&
				// PARANOIA STRIKES DEEP IN THE HEARTLAND!!!1
				!input_uri.path.substr(1).match(/\//) &&
		input_uri.path.match(/^\/internal-[^/ ]+\.(css|png|js)$/)) {
			OverbiteLog("handling internal chrome: "
				+input_uri.asciiSpec);
			var IURL;
			var mpath = input_uri.path.substr(1)
				.replace(/^internal-/, "");

			// try profile directory
			var dirService = Cc["@mozilla.org/file/directory_service;1"]
				.getService(Ci.nsIProperties);

			var uprof = dirService.get("ProfD", Ci.nsIFile);
			var basedir = "gopherchrome";
			//uprof.appendRelativePath(basedir);
			uprof.append(basedir);

			if (uprof.exists() && uprof.isDirectory()) {
				// use the user's gopherchrome directory for
				// CSS, icons and content level javascript
				OverbiteLog("trying user directory");
				var fileProServ = Cc["@mozilla.org/network/io-service;1"]
					.getService(Ci.nsIIOService)
					.getProtocolHandler("file")
					.QueryInterface(Ci
						.nsIFileProtocolHandler);

				uprof.append(mpath);
				if (uprof.exists() && uprof.isFile())
					IURL = fileProServ.getURLSpecFromFile(
						uprof);
			}
			if (!IURL) {
				OverbiteLog("trying internal chrome dir");
				IURL = OBFFCHROMEURL+ "/content/chrome/"
					+mpath;
			}
			OverbiteLog("resulting URL: "+IURL);
			return ioService.newChannel(IURL, null, null);
		}

		// otherwise
		// make chrome channel to about page if
			// input_uri lacks a hostname
		if (!input_uri.host.length) {
			OverbiteLog("internal about page served up instead");
			OverbiteSetPrefs(); // sigh

			// SEKRIT FUNKTION. gopher:/// clears the caps cache
			capscache = new Object();
			OverbiteLogAlways("caps cache is cleared");

			return ioService.newChannel(
				OBFFABOUTURL,
				null, null);
		}

		// otherwise
		// immediately reject "pseudo" item types we don't handle
		// do this here because it traps internal URLs
		var wontshow = ['/i', '/3', '/8', '/T'];
		if (input_uri.path && input_uri.path.length > 1 &&
			wontshow.indexOf(input_uri.path.substr(0,2)) > -1) {
			OverbitePrompter("baditype", function() {
			prompter.alert(null,
				self._getL10NString('baditype.title'),
				self._getL10NString('baditype'))
			});
			throw Cr.NS_ERROR_ABORT;
			return null;
		}

		// silently reject port numbers we will never allow
		//if (badports.indexOf(input_uri.port) > -1) {
		if (input_uri.port && input_uri.port >= 0
				&& alwayslet.indexOf(input_uri.port)== -1) {
			OverbiteLog("illegal port: "+input_uri.port);
			throw Cr.NS_ERROR_PORT_ACCESS_NOT_ALLOWED;
			return null;
		}

		// else it's a legit gopher request
		// make our channel and gopher it
		ob = new OverbiteChannel(input_uri, proxyinfo, loadinfo);
		ob._proxyresolveflags = proxyresolveflags;
		ob._proxy_uri = proxy_uri;
		return ob;
	}
};

/* Startup code */

var components = [ OverbiteProtocol ];
function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
/* for Gecko 2 (deprecated) */
if (XPCOMUtils.generateNSGetFactory)
	var NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

function OverbiteSetPrefs() {
	var prefs = Cc["@mozilla.org/preferences-service;1"];
	var prefserv = prefs.getService(Ci.nsIPrefService);
	prefs = prefs.getService(Ci.nsIPrefBranch);
	prefs.setIntPref(OBFFBUILDPREF, OBFFBUILD);
	prefserv.savePrefFile(null);
}

OverbiteLog("startup with version "+OBFFVERS+" build "+OBFFBUILD);

var prefs = Cc["@mozilla.org/preferences-service;1"];
var prefserv = prefs.getService(Ci.nsIPrefService);
prefs = prefs.getService(Ci.nsIPrefBranch);

if (prefs.getPrefType(OBFFBUILDPREF) != prefs.PREF_INT ||
		prefs.getIntPref(OBFFBUILDPREF) < OBFFBUILD) {
	var obs = Cc["@mozilla.org/observer-service;1"]
		.getService(Ci.nsIObserverService);

	// create an observer to wait for the top level window
	// and then take it over for startup
	var listeno = {
		_timer : null,
		_window : null,
/* TEMPORARY HACK: Complain in the brag screen if this is an e10s window. */
		_e10s : false,

		// nsISupports
		QueryInterface : XPCOMUtils.generateQI([nsISupports,
							nsIObserver,
							nsITimerCallback]),

		// nsITimerCallback
		notify : function(timer) {
			OverbiteLog("timer tripped");
			this._timer = null;
			this._window.focus();
			this._window.getBrowser().selectedTab =
				this._window.getBrowser().addTab(OBFFIABOUTURL+
/* TEMPORARY HACK: Complain in the brag screen if this is an e10s window. */
					((this._e10s) ? "#e10s" : ""));
			this._window = null;
		},

		// nsIObserver
		observe : function(subject, topic, data) {
			// we're not picky about topic, since it
			// is designed to respond to *at least* two.
			OverbiteLog("observer tripped by topic: "+topic);

/* TEMPORARY HACK: Complain in the brag screen if this is an e10s window. */
			if (subject && subject.gMultiProcessBrowser)
				this._e10s = true; // damn
				
			var obs = Cc["@mozilla.org/observer-service;1"]
				.getService(Ci.nsIObserverService);
			var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
				.getService(Ci.nsIWindowMediator);

			var w = wm.getMostRecentWindow('navigator:browser');
			if (!w)
					return; // and live to hook another day
			OverbiteLog("trying window");
			this._window = w;

		// for some reason this doesn't work until the window appears
		// so we do it at the window opening level
			var prefs = Cc["@mozilla.org/preferences-service;1"];
			var prefserv = prefs.getService(Ci.nsIPrefService);
			prefs = prefs.getService(Ci.nsIPrefBranch);

			if (prefs.getPrefType(OBFFBUILDPREF) !=
					prefs.PREF_INT ||
				prefs.getIntPref(OBFFBUILDPREF) <
					 OBFFBUILD) {
				// save even if the tab fails -- we
				// only want to try this the first time
				prefs.setIntPref(OBFFBUILDPREF, OBFFBUILD);
				prefserv.savePrefFile(null);
				OverbiteLog("prefs set");

				this._timer = Cc["@mozilla.org/timer;1"]
					.createInstance(Ci.nsITimer);
				this._timer.initWithCallback(this, 2000, 0);
			}
			obs.removeObserver(this, topic);
			return;
		}
	};

	obs.addObserver(listeno, "sessionstore-windows-restored", false);
	obs.addObserver(listeno, "browser-delayed-startup-finished", false);
	OverbiteLog("no window yet, listener installed");
} 

