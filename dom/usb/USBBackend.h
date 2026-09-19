/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef mozilla_dom_USBBackend_h
#define mozilla_dom_USBBackend_h

#include <stdint.h>

#include "nsError.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

struct USBDeviceInfo {
  uint16_t mVendorId = 0;
  uint16_t mProductId = 0;
  uint8_t mUSBVersionMajor = 0;
  uint8_t mUSBVersionMinor = 0;
  uint8_t mUSBVersionSubminor = 0;
  uint8_t mDeviceClass = 0;
  uint8_t mDeviceSubclass = 0;
  uint8_t mDeviceProtocol = 0;
  uint8_t mDeviceVersionMajor = 0;
  uint8_t mDeviceVersionMinor = 0;
  uint8_t mDeviceVersionSubminor = 0;
  nsString mManufacturerName;
  nsString mProductName;
  nsString mSerialNumber;
  nsString mDevicePath;
};

struct USBEndpointInfo {
  uint8_t mNumber = 0;
  uint8_t mDirection = 0; // 0 = in, 1 = out
  uint8_t mType = 0;      // 1 = isochronous, 2 = bulk, 3 = interrupt
  uint16_t mPacketSize = 0;
};

struct USBAlternateInterfaceInfo {
  uint8_t mSetting = 0;
  uint8_t mClass = 0;
  uint8_t mSubclass = 0;
  uint8_t mProtocol = 0;
  nsTArray<USBEndpointInfo> mEndpoints;
};

struct USBInterfaceInfo {
  uint8_t mNumber = 0;
  nsTArray<USBAlternateInterfaceInfo> mAlternates;
};

struct USBConfigurationInfo {
  uint8_t mValue = 0;
  nsTArray<USBInterfaceInfo> mInterfaces;
};

struct USBControlTransferInfo {
  uint8_t mRequestType = 0;
  uint8_t mRecipient = 0;
  uint8_t mRequest = 0;
  uint16_t mValue = 0;
  uint16_t mIndex = 0;
};

class USBDeviceHandle
{
public:
  NS_INLINE_DECL_REFCOUNTING(USBDeviceHandle)

  virtual nsresult Open() = 0;
  virtual void Close() = 0;
  virtual nsresult GetConfigurations(
    nsTArray<USBConfigurationInfo>& aConfigurations) = 0;
  virtual nsresult SelectConfiguration(uint8_t aConfigurationValue) = 0;
  virtual nsresult ClaimInterface(uint8_t aInterfaceNumber) = 0;
  virtual nsresult ReleaseInterface(uint8_t aInterfaceNumber) = 0;
  virtual nsresult SelectAlternateInterface(uint8_t aInterfaceNumber,
                                            uint8_t aAlternateSetting) = 0;
  virtual nsresult ClearHalt(bool aIn, uint8_t aEndpointNumber) = 0;
  virtual nsresult Reset() = 0;
  virtual nsresult ControlTransferIn(const USBControlTransferInfo& aSetup,
                                     uint16_t aLength,
                                     nsTArray<uint8_t>& aData) = 0;
  virtual nsresult ControlTransferOut(const USBControlTransferInfo& aSetup,
                                      const nsTArray<uint8_t>& aData,
                                      uint32_t& aWritten) = 0;
  virtual nsresult TransferIn(uint8_t aEndpoint, uint32_t aLength,
                               nsTArray<uint8_t>& aData) = 0;
  virtual nsresult TransferOut(uint8_t aEndpoint,
                               const nsTArray<uint8_t>& aData,
                               uint32_t& aWritten) = 0;

protected:
  virtual ~USBDeviceHandle() = default;
};

// Enumeration returns stable device metadata and a native path. Native
// handles are created only after the caller has obtained permission.
nsresult EnumerateUSBDevices(nsTArray<USBDeviceInfo>& aDevices);

already_AddRefed<USBDeviceHandle>
CreateUSBDeviceHandle(const nsAString& aDevicePath);

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USBBackend_h
