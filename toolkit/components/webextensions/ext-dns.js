"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function dnsResolve(hostname, flags = []) {
  let mask = 0;
  for (let flag of flags) {
    switch (flag) {
      case "bypass_cache": mask |= Ci.nsIDNSService.RESOLVE_BYPASS_CACHE; break;
      case "canonical_name": mask |= Ci.nsIDNSService.RESOLVE_CANONICAL_NAME; break;
      case "disable_ipv4": mask |= Ci.nsIDNSService.RESOLVE_DISABLE_IPV4; break;
      case "disable_ipv6": mask |= Ci.nsIDNSService.RESOLVE_DISABLE_IPV6; break;
      case "offline": mask |= Ci.nsIDNSService.RESOLVE_OFFLINE; break;
      case "priority_low": mask |= Ci.nsIDNSService.RESOLVE_PRIORITY_LOW; break;
      case "priority_medium": mask |= Ci.nsIDNSService.RESOLVE_PRIORITY_MEDIUM; break;
      case "allow_name_collisions": mask |= Ci.nsIDNSService.RESOLVE_ALLOW_NAME_COLLISION; break;
    }
  }
  let dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
  return new Promise((resolve, reject) => {
    let listener = {
      onLookupComplete(request, record, status) {
        if (Components.isSuccessCode(status)) {
          let addresses = [];
          try { record.rewind(); while (record.hasMore()) addresses.push(record.getNextAddrAsString()); } catch (e) {}
          resolve({addresses, canonicalName: record.canonicalName || undefined, isTRR: false});
        } else {
          reject(Components.Exception("DNS resolution failed", status));
        }
      },
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSListener]),
    };
    dns.asyncResolve(hostname, mask, listener, Services.tm.mainThread);
  });
}

extensions.registerSchemaAPI("dns", "addon_parent", () => ({
  dns: { resolve: dnsResolve },
}));
