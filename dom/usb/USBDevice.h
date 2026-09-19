/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef mozilla_dom_USBDevice_h
#define mozilla_dom_USBDevice_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/USBBackend.h"
#include "mozilla/dom/USBDescriptors.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/Nullable.h"
#include "nsString.h"
#include <stdint.h>

namespace mozilla {
namespace dom {

class Promise;
class USB;
class ArrayBufferViewOrArrayBuffer;
struct USBControlTransferParameters;

class USBDevice final : public DOMEventTargetHelper
{
  uint16_t mVendorId;
  uint16_t mProductId;
  uint8_t mUSBVersionMajor;
  uint8_t mUSBVersionMinor;
  uint8_t mUSBVersionSubminor;
  uint8_t mDeviceClass;
  uint8_t mDeviceSubclass;
  uint8_t mDeviceProtocol;
  uint8_t mDeviceVersionMajor;
  uint8_t mDeviceVersionMinor;
  uint8_t mDeviceVersionSubminor;
  Nullable<nsString> mManufacturerName;
  Nullable<nsString> mProductName;
  Nullable<nsString> mSerialNumber;
  nsString mDevicePath;
  RefPtr<USBDeviceHandle> mHandle;
  RefPtr<USBConfiguration> mConfiguration;
  nsTArray<RefPtr<USBConfiguration>> mConfigurations;
  USB* mUSB = nullptr;
  bool mOpened;

public:
  explicit USBDevice(nsIGlobalObject* aOwner);

  void SetDeviceInfo(const USBDeviceInfo& aInfo);
  void SetConfigurations(const nsTArray<USBConfigurationInfo>& aConfigurations);
  void SetUSB(USB* aUSB) { mUSB = aUSB; }
  void ClearUSB() { mUSB = nullptr; }

  uint16_t VendorId() const { return mVendorId; }
  uint16_t ProductId() const { return mProductId; }
  uint8_t UsbVersionMajor() const { return mUSBVersionMajor; }
  uint8_t UsbVersionMinor() const { return mUSBVersionMinor; }
  uint8_t UsbVersionSubminor() const { return mUSBVersionSubminor; }
  uint8_t DeviceClass() const { return mDeviceClass; }
  uint8_t DeviceSubclass() const { return mDeviceSubclass; }
  uint8_t DeviceProtocol() const { return mDeviceProtocol; }
  uint8_t DeviceVersionMajor() const { return mDeviceVersionMajor; }
  uint8_t DeviceVersionMinor() const { return mDeviceVersionMinor; }
  uint8_t DeviceVersionSubminor() const { return mDeviceVersionSubminor; }
  void GetManufacturerName(DOMString& aValue) const;
  void GetProductName(DOMString& aValue) const;
  void GetSerialNumber(DOMString& aValue) const;
  bool Opened() const { return mOpened; }
  USBConfiguration* Configuration() const { return mConfiguration; }
  void GetConfigurations(nsTArray<RefPtr<USBConfiguration>>& aValue) const
  { aValue = mConfigurations; }

  already_AddRefed<Promise> Open(ErrorResult& aRv);
  already_AddRefed<Promise> Close(ErrorResult& aRv);
  already_AddRefed<Promise> Forget(ErrorResult& aRv);
  already_AddRefed<Promise> SelectConfiguration(uint8_t aValue,
                                                ErrorResult& aRv);
  already_AddRefed<Promise> ClaimInterface(uint8_t aNumber, ErrorResult& aRv);
  already_AddRefed<Promise> ReleaseInterface(uint8_t aNumber,
                                             ErrorResult& aRv);
  already_AddRefed<Promise> SelectAlternateInterface(uint8_t aNumber,
                                                     uint8_t aSetting,
                                                     ErrorResult& aRv);
  already_AddRefed<Promise> ClearHalt(USBDirection aDirection,
                                     uint8_t aEndpoint, ErrorResult& aRv);
  already_AddRefed<Promise> Reset(ErrorResult& aRv);
  already_AddRefed<Promise> ControlTransferIn(
    const USBControlTransferParameters& aSetup, uint16_t aLength,
    ErrorResult& aRv);
  already_AddRefed<Promise> ControlTransferOut(
    const USBControlTransferParameters& aSetup,
    const Optional<ArrayBufferViewOrArrayBuffer>& aData, ErrorResult& aRv);
  already_AddRefed<Promise> TransferIn(uint8_t aEndpoint, uint32_t aLength,
                                        ErrorResult& aRv);
  already_AddRefed<Promise> TransferOut(
    uint8_t aEndpoint, const ArrayBufferViewOrArrayBuffer& aData,
    ErrorResult& aRv);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(USBDevice, DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBDevice();
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USBDevice_h
