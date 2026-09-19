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
Portions created by Cameron Kaiser are Copyright (C) 2008-2011
Cameron Kaiser. All Rights Reserved. Copyright (C) 2008-2011 Contributors
to the Overbite Project.

For users under the GNU Public License:

OverbiteFF Gopher/CSO Firefox addon
Copyright (C) 2008-2011 Cameron Kaiser
Copyright (C) 2008-2011 Contributors to the Overbite Project

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
Copyright (C) 2008-2011 Cameron Kaiser
Copyright (C) 2008-2011 Contributors to the Overbite Project

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

const OBFFABOUT = "chrome://overbiteff/content/infobabe.html";
// register this to three contract IDs
const OBFFABOUT_CONTRACTID1 = "@mozilla.org/network/protocol/about;1?what=gopher";
const OBFFABOUT_CONTRACTID2 = "@mozilla.org/network/protocol/about;1?what=overbite";
//const OBFFABOUT_CONTRACTID3 = "@mozilla.org/network/protocol/about;1?what=overbiteff";

const OBFFABOUT_CID = Components.ID("{80d3faa0-de53-11dc-95ff-0800200c9a66}");

const nsISupports = Components.interfaces.nsISupports;
const nsIFactory = Components.interfaces.nsIFactory;
const nsIModule = Components.interfaces.nsIModule;
const nsIAboutModule = Components.interfaces.nsIAboutModule;
const nsIIOService = Components.interfaces.nsIIOService;
const nsIObserver = Components.interfaces.nsIObserver;
const nsIComponentRegistrar = Components.interfaces.nsIComponentRegistrar;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

var OverbiteAbout = {

	classID: OBFFABOUT_CID,

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
                                                nsIObserver,
						nsIFactory,
						nsIAboutModule]),

	// nsIObserver
	observe : function(subject, topic, data) {
		// nothing yet
	},

	// nsIAboutModule
	newChannel : function(uri) {
		var chan = Components
			.classes["@mozilla.org/network/io-service;1"]
			.getService(nsIIOService)
			.newChannel(OBFFABOUT, null, null);
		chan.originalURI = uri;
		return chan;
	},

	getURIFlags : function(uri) { return 0; },

	// nsIFactory
	createInstance : function(outer, iid) {
		if (outer != null)
			throw Components.results.NS_ERROR_NO_AGGREGATION;
		return this.QueryInterface(iid);
	},
	
	lockFactory : function(lock) { // ignored
	}
};

var Module = {

	// nsISupports
	QueryInterface : XPCOMUtils.generateQI([nsISupports,
						nsIModule]),

	// nsIModule
	getClassObject : function(compMgr, cid, iid) {
		if (cid.equals(OBFFABOUT_CID)) {
			return OverbiteAbout.QueryInterface(iid);
		}

		throw Components.results.NS_ERROR_FACTORY_NOT_REGISTERED;
	},
	registerSelf : function(compMgr, fileSpec, location, type) {
		var compReg = compMgr.QueryInterface(nsIComponentRegistrar);

		// do the treble
		compReg.registerFactoryLocation(OBFFABOUT_CID,
						"about:gopher",
						OBFFABOUT_CONTRACTID1,
						fileSpec,
						location,
						type);
		compReg.registerFactoryLocation(OBFFABOUT_CID,
						"about:overbite",
						OBFFABOUT_CONTRACTID2,
						fileSpec,
						location,
						type);
/* sigh. it must not like the length.
		compReg.registerFactoryLocation(OBFFABOUT_CID,
						"about:overbiteff",
						OBFFABOUT_CONTRACTID3,
						fileSpec,
						location,
						type);
*/
	},
	unregisterSelf : function(compMgr, location, type) {
		var compReg = compMgr.QueryInterface(nsIComponentRegistrar);

		// undo the treble
		compReg.unregisterFactoryLocation(OBFFABOUT_CID, location);
	},
	canUnload : function(compMgr) { return true; }
};

function NSGetModule(compMgr, fileSpec) { return Module; }
function NSGetFactory(component) { return OverbiteAbout; }
