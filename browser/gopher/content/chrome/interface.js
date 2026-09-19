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

/* This controls the JavaScript interface. This runs with content privileges,
   not with chrome privileges, by design. It can be blocked by NoScript and
   other such tools. It cannot call back to chrome, so it ought not be
   naughty. Assuming Mozilla did their job. COW's n'at. */

var openstates = new Object();
var opacstates = new Object();
var timestates = new Object();
var localization = new Object();
const outtime = 30;

/* Function names are short because:
   1. I am a lousy programmer
   2. I don't care
   3. To avoid lots of crap */

/* Reviewer note: You'll see lots of simplistic templating below. The
   functions are being called by the HTML generated for gopher menus in
   the main nsIChannel object. The nsIChannel thoroughly sanitizes them.
   They are never directly called by anything that gets unfiltered user
   input, and this script runs with content privileges, not chrome. */

// Store something in localization (passed from chrome bundle)
function l(x, z) {
	localization[x] = decodeURI(z);
}

// Create the disclosure "icon"
function w(x, y, z) {
	document.write('<a href="#" '+
"onClick=\"x('"+x+"','"+y+"', "+z+");return false\" "+
'class="plus" title="['+localization[1]+']"><span id="pl'+z+'">+</span></a>');
}

// Toggle the disclosure icon, and insert the page image or iframe or
// form for [pgI], [0h] or 7. This uses the <div> scaffolding that chrome
// has already set up for us; we just fill it in.
function x(x, y, z) {
	if (!openstates[z]) {
		var h = window.innerHeight / 3;
		openstates[z] = 1;
		document.getElementById("pl"+z).innerHTML = "-";
		if (timestates[z])
			clearTimeout(timestates[z]);
		try {
			document.getElementById("pv"+z).style.opacity = 0.0;
			opacstates[z] = 0.0;
			timestates[z] = setTimeout("fade("+z+")", outtime);
		} catch(e) { alert(e); }
		if (y == "p" || y == "g" || y == "I") {
			document.getElementById("pv"+z).innerHTML =
'<a href="'+x+'"><img style="border: 2px;" src="'+x+'" onload="s(this);"></a>'
		} else if (y == "7") {
			document.getElementById("pv"+z).innerHTML =
'<div class="inlineprompt">'+
'<form method="get" action="#" onSubmit="f('+"'"+x+"',"+z+');return false">'+
'<span class="inlineprompt">'+localization[0]+'</span><br><br>'+
'<input type="text" id="src'+z+'" size="80" maxlength="255"> ' +
		'</form></div>';
			document.getElementById("src"+z).focus();
		} else {
			document.getElementById("pv"+z).innerHTML =
'<iframe src="'+x+'" width="100%" height="'+h+'"></iframe>';
		}
	} else {
		openstates[z] = 0;
		if (timestates[z])
			clearTimeout(timestates[z]);
		timestates[z] = null;
		document.getElementById("pl"+z).innerHTML = "+";
		document.getElementById("pv"+z).innerHTML = "";
	}
	return false;
}

// Resize images (onLoad handler for images)
function s(i) {
	var w = window.innerWidth - 100;
	if (!i.width) return;
	if (i.width > w) {
		i.width = w;
		i.height = w / (i.width / i.height);
	}
}

// Dynamically turn a form into a Gopher item type 7 query
function f(u, r) {
	window.location=u+"%09"+encodeURI(document.getElementById("src"+r).value);
	return false;
}

function fade(z) {
	opacstates[z] += 0.1;
	document.getElementById("pv"+z).style.opacity = opacstates[z];
	if (opacstates[z] >= 1.0) {
		timestates[z] = null;
	} else {
		timestates[z] = setTimeout("fade("+z+")", outtime);
	}
}
// future expansion
function edaf(z) {
	opacstates[z] -= 0.1;
	document.getElementById("pv"+z).style.opacity = opacstates[z];
	if (opacstates[z] <= 0.0) {
		timestates[z] = null;
	} else {
		timestates[z] = setTimeout("edaf("+z+")", outtime);
	}
}
