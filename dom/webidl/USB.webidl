/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

[SecureContext, Pref="dom.usb.enabled", Exposed=Window]
interface USB : EventTarget {
  [NewObject]
  Promise<sequence<USBDevice>> getDevices();

  [NewObject]
  Promise<USBDevice> requestDevice(optional USBDeviceRequestOptions options);

  attribute EventHandler onconnect;
  attribute EventHandler ondisconnect;
};

dictionary USBDeviceRequestOptions {
  required sequence<USBDeviceFilter> filters;
  sequence<USBDeviceFilter> exclusionFilters = [];
};

dictionary USBDeviceFilter {
  unsigned short vendorId;
  unsigned short productId;
  octet classCode;
  octet subclassCode;
  octet protocolCode;
  DOMString serialNumber;
};

enum USBDirection {
  "in",
  "out"
};

enum USBEndpointType {
  "bulk",
  "interrupt",
  "isochronous"
};

enum USBTransferStatus {
  "ok",
  "stall",
  "babble"
};

enum USBRequestType {
  "standard",
  "class",
  "vendor"
};

enum USBRecipient {
  "device",
  "interface",
  "endpoint",
  "other"
};

dictionary USBControlTransferParameters {
  required USBRequestType requestType;
  required USBRecipient recipient;
  required octet request;
  required unsigned short value;
  required unsigned short index;
};

[SecureContext, Pref="dom.usb.enabled", Exposed=Window]
interface USBDevice : EventTarget {
  readonly attribute octet usbVersionMajor;
  readonly attribute octet usbVersionMinor;
  readonly attribute octet usbVersionSubminor;
  readonly attribute octet deviceClass;
  readonly attribute octet deviceSubclass;
  readonly attribute octet deviceProtocol;
  readonly attribute unsigned short vendorId;
  readonly attribute unsigned short productId;
  readonly attribute octet deviceVersionMajor;
  readonly attribute octet deviceVersionMinor;
  readonly attribute octet deviceVersionSubminor;
  readonly attribute DOMString? manufacturerName;
  readonly attribute DOMString? productName;
  readonly attribute DOMString? serialNumber;
  readonly attribute USBConfiguration? configuration;
  [Cached] readonly attribute sequence<USBConfiguration> configurations;
  readonly attribute boolean opened;

  [NewObject] Promise<void> open();
  [NewObject] Promise<void> close();
  [NewObject] Promise<void> forget();
  [NewObject] Promise<void> selectConfiguration(octet configurationValue);
  [NewObject] Promise<void> claimInterface(octet interfaceNumber);
  [NewObject] Promise<void> releaseInterface(octet interfaceNumber);
  [NewObject] Promise<void> selectAlternateInterface(octet interfaceNumber,
                                                     octet alternateSetting);
  [NewObject] Promise<void> clearHalt(USBDirection direction,
                                       octet endpointNumber);
  [NewObject] Promise<void> reset();
  [NewObject] Promise<USBInTransferResult> controlTransferIn(
      USBControlTransferParameters setup, unsigned short length);
  [NewObject] Promise<USBOutTransferResult> controlTransferOut(
      USBControlTransferParameters setup, optional BufferSource data);
  [NewObject] Promise<USBInTransferResult> transferIn(octet endpointNumber,
                                                       unsigned long length);
  [NewObject] Promise<USBOutTransferResult> transferOut(
      octet endpointNumber, BufferSource data);
};

[SecureContext, Exposed=Window]
interface USBConfiguration {
  readonly attribute octet configurationValue;
  readonly attribute DOMString? configurationName;
  [Cached] readonly attribute sequence<USBInterface> interfaces;
};

[SecureContext, Exposed=Window]
interface USBInterface {
  readonly attribute octet interfaceNumber;
  readonly attribute USBAlternateInterface alternate;
  [Cached] readonly attribute sequence<USBAlternateInterface> alternates;
  readonly attribute boolean claimed;
};

[SecureContext, Exposed=Window]
interface USBAlternateInterface {
  readonly attribute octet alternateSetting;
  readonly attribute octet interfaceClass;
  readonly attribute octet interfaceSubclass;
  readonly attribute octet interfaceProtocol;
  readonly attribute DOMString? interfaceName;
  [Cached] readonly attribute sequence<USBEndpoint> endpoints;
};

[SecureContext, Exposed=Window]
interface USBEndpoint {
  readonly attribute octet endpointNumber;
  readonly attribute USBDirection direction;
  readonly attribute USBEndpointType type;
  readonly attribute unsigned short packetSize;
};

[SecureContext, Exposed=Window]
interface USBInTransferResult {
  readonly attribute USBTransferStatus status;
  // UXP's binding generator does not expose DataView as a WebIDL type;
  // transfer data is returned as an ArrayBuffer until that binding exists.
  readonly attribute ArrayBuffer? data;
};

[SecureContext, Exposed=Window]
interface USBOutTransferResult {
  readonly attribute USBTransferStatus status;
  readonly attribute unsigned long bytesWritten;
};
