/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "USBDevice.h"

#include "mozilla/dom/Promise.h"
#include "mozilla/dom/USB.h"
#include "mozilla/dom/USBBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/Services.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPermissionManager.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

namespace {

already_AddRefed<Promise>
BackendPromise(nsIGlobalObject* aOwner, nsresult aResult, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(aOwner, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (NS_SUCCEEDED(aResult)) {
    promise->MaybeResolve();
  } else {
    promise->MaybeReject(aResult);
  }
  return promise.forget();
}

USBInterface*
FindInterface(USBConfiguration* aConfiguration, uint8_t aNumber)
{
  if (!aConfiguration) {
    return nullptr;
  }
  nsTArray<RefPtr<USBInterface>> interfaces;
  aConfiguration->GetInterfaces(interfaces);
  for (const auto& interfaceObject : interfaces) {
    if (interfaceObject->InterfaceNumber() == aNumber) {
      return interfaceObject;
    }
  }
  return nullptr;
}

USBControlTransferInfo
MakeControlTransferInfo(const USBControlTransferParameters& aSetup)
{
  USBControlTransferInfo setup;
  setup.mRequestType = static_cast<uint8_t>(aSetup.mRequestType);
  setup.mRecipient = static_cast<uint8_t>(aSetup.mRecipient);
  setup.mRequest = aSetup.mRequest;
  setup.mValue = aSetup.mValue;
  setup.mIndex = aSetup.mIndex;
  return setup;
}

void
CopyTransferData(const ArrayBufferViewOrArrayBuffer& aData,
                 nsTArray<uint8_t>& aOutput)
{
  aOutput.Clear();
  if (aData.IsArrayBuffer()) {
    const ArrayBuffer& buffer = aData.GetAsArrayBuffer();
    buffer.ComputeLengthAndData();
    aOutput.AppendElements(buffer.Data(), buffer.Length());
  } else {
    const ArrayBufferView& view = aData.GetAsArrayBufferView();
    view.ComputeLengthAndData();
    aOutput.AppendElements(view.Data(), view.Length());
  }
}

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(USBDevice, DOMEventTargetHelper,
                                   mHandle, mConfiguration, mConfigurations)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBDevice)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(USBDevice, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(USBDevice, DOMEventTargetHelper)

USBDevice::USBDevice(nsIGlobalObject* aOwner)
  : DOMEventTargetHelper(aOwner)
  , mVendorId(0)
  , mProductId(0)
  , mUSBVersionMajor(0)
  , mUSBVersionMinor(0)
  , mUSBVersionSubminor(0)
  , mDeviceClass(0)
  , mDeviceSubclass(0)
  , mDeviceProtocol(0)
  , mDeviceVersionMajor(0)
  , mDeviceVersionMinor(0)
  , mDeviceVersionSubminor(0)
  , mOpened(false)
{
}

USBDevice::~USBDevice()
{
}

void
USBDevice::SetDeviceInfo(const USBDeviceInfo& aInfo)
{
  mVendorId = aInfo.mVendorId;
  mProductId = aInfo.mProductId;
  mUSBVersionMajor = aInfo.mUSBVersionMajor;
  mUSBVersionMinor = aInfo.mUSBVersionMinor;
  mUSBVersionSubminor = aInfo.mUSBVersionSubminor;
  mDeviceClass = aInfo.mDeviceClass;
  mDeviceSubclass = aInfo.mDeviceSubclass;
  mDeviceProtocol = aInfo.mDeviceProtocol;
  mDeviceVersionMajor = aInfo.mDeviceVersionMajor;
  mDeviceVersionMinor = aInfo.mDeviceVersionMinor;
  mDeviceVersionSubminor = aInfo.mDeviceVersionSubminor;
  mDevicePath = aInfo.mDevicePath;

  if (aInfo.mManufacturerName.IsEmpty()) {
    mManufacturerName.SetNull();
  } else {
    mManufacturerName.SetValue(aInfo.mManufacturerName);
  }
  if (aInfo.mProductName.IsEmpty()) {
    mProductName.SetNull();
  } else {
    mProductName.SetValue(aInfo.mProductName);
  }
  if (aInfo.mSerialNumber.IsEmpty()) {
    mSerialNumber.SetNull();
  } else {
    mSerialNumber.SetValue(aInfo.mSerialNumber);
  }
}

void
USBDevice::GetManufacturerName(DOMString& aValue) const
{
  if (mManufacturerName.IsNull()) {
    aValue.SetNull();
  } else {
    aValue.SetOwnedString(mManufacturerName.Value());
  }
}

void
USBDevice::GetProductName(DOMString& aValue) const
{
  if (mProductName.IsNull()) {
    aValue.SetNull();
  } else {
    aValue.SetOwnedString(mProductName.Value());
  }
}

void
USBDevice::GetSerialNumber(DOMString& aValue) const
{
  if (mSerialNumber.IsNull()) {
    aValue.SetNull();
  } else {
    aValue.SetOwnedString(mSerialNumber.Value());
  }
}

void
USBDevice::SetConfigurations(
  const nsTArray<USBConfigurationInfo>& aConfigurations)
{
  mConfigurations.Clear();
  mConfiguration = nullptr;

  for (const auto& configurationInfo : aConfigurations) {
    nsTArray<RefPtr<USBInterface>> interfaces;
    for (const auto& interfaceInfo : configurationInfo.mInterfaces) {
      nsTArray<RefPtr<USBAlternateInterface>> alternates;
      for (const auto& alternateInfo : interfaceInfo.mAlternates) {
        nsTArray<RefPtr<USBEndpoint>> endpoints;
        for (const auto& endpointInfo : alternateInfo.mEndpoints) {
          USBDirection direction = endpointInfo.mDirection == 0
            ? USBDirection::In : USBDirection::Out;
          USBEndpointType type;
          switch (endpointInfo.mType) {
            case 1:
              type = USBEndpointType::Isochronous;
              break;
            case 3:
              type = USBEndpointType::Interrupt;
              break;
            default:
              type = USBEndpointType::Bulk;
              break;
          }
          endpoints.AppendElement(new USBEndpoint(
            GetOwnerGlobal(), endpointInfo.mNumber, direction, type,
            endpointInfo.mPacketSize));
        }
        alternates.AppendElement(new USBAlternateInterface(
          GetOwnerGlobal(), alternateInfo.mSetting, alternateInfo.mClass,
          alternateInfo.mSubclass, alternateInfo.mProtocol, Move(endpoints)));
      }
      interfaces.AppendElement(new USBInterface(
        GetOwnerGlobal(), interfaceInfo.mNumber, Move(alternates)));
    }
    RefPtr<USBConfiguration> configuration = new USBConfiguration(
      GetOwnerGlobal(), configurationInfo.mValue, Move(interfaces));
    if (!mConfiguration) {
      mConfiguration = configuration;
    }
    mConfigurations.AppendElement(configuration);
  }
}

JSObject*
USBDevice::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USBDeviceBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
USBDevice::Open(ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mOpened) {
    promise->MaybeResolve();
    return promise.forget();
  }

  mHandle = CreateUSBDeviceHandle(mDevicePath);
  if (!mHandle || NS_FAILED(mHandle->Open())) {
    mHandle = nullptr;
    promise->MaybeReject(NS_ERROR_DOM_NOT_SUPPORTED_ERR);
    return promise.forget();
  }

  mOpened = true;
  nsTArray<USBConfigurationInfo> configurations;
  if (NS_SUCCEEDED(mHandle->GetConfigurations(configurations))) {
    SetConfigurations(configurations);
  }
  promise->MaybeResolve();
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::Close(ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mHandle) {
    mHandle->Close();
    mHandle = nullptr;
  }
  mOpened = false;
  promise->MaybeResolve();
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::Forget(ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mHandle) {
    mHandle->Close();
    mHandle = nullptr;
  }
  mOpened = false;
  if (mUSB) {
    mUSB->RemoveAuthorizedDevice(this);
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetOwnerGlobal());
  if (window && window->GetDoc()) {
    nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
    if (permissionManager) {
      permissionManager->RemoveFromPrincipal(window->GetDoc()->NodePrincipal(),
                                             "usb");
    }
  }
  promise->MaybeResolve();
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::SelectConfiguration(uint8_t aValue, ErrorResult& aRv)
{
  if (!mOpened || !mHandle) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  nsresult rv = mHandle->SelectConfiguration(aValue);
  if (NS_SUCCEEDED(rv)) {
    for (const auto& configuration : mConfigurations) {
      if (configuration->ConfigurationValue() == aValue) {
        mConfiguration = configuration;
        break;
      }
    }
  }
  return BackendPromise(GetOwnerGlobal(), rv, aRv);
}

already_AddRefed<Promise>
USBDevice::ClaimInterface(uint8_t aNumber, ErrorResult& aRv)
{
  if (!mOpened || !mHandle || !FindInterface(mConfiguration, aNumber)) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  nsresult rv = mHandle->ClaimInterface(aNumber);
  if (NS_SUCCEEDED(rv)) {
    FindInterface(mConfiguration, aNumber)->SetClaimed(true);
  }
  return BackendPromise(GetOwnerGlobal(), rv, aRv);
}

already_AddRefed<Promise>
USBDevice::ReleaseInterface(uint8_t aNumber, ErrorResult& aRv)
{
  USBInterface* interfaceObject = FindInterface(mConfiguration, aNumber);
  if (!mOpened || !mHandle || !interfaceObject) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  nsresult rv = mHandle->ReleaseInterface(aNumber);
  if (NS_SUCCEEDED(rv)) {
    interfaceObject->SetClaimed(false);
  }
  return BackendPromise(GetOwnerGlobal(), rv, aRv);
}

already_AddRefed<Promise>
USBDevice::SelectAlternateInterface(uint8_t aNumber, uint8_t aSetting,
                                    ErrorResult& aRv)
{
  USBInterface* interfaceObject = FindInterface(mConfiguration, aNumber);
  if (!mOpened || !mHandle || !interfaceObject) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  nsresult rv = mHandle->SelectAlternateInterface(aNumber, aSetting);
  if (NS_SUCCEEDED(rv)) {
    interfaceObject->SetAlternate(aSetting);
  }
  return BackendPromise(GetOwnerGlobal(), rv, aRv);
}

already_AddRefed<Promise>
USBDevice::ClearHalt(USBDirection aDirection, uint8_t aEndpoint,
                     ErrorResult& aRv)
{
  if (!mOpened || !mHandle) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  return BackendPromise(GetOwnerGlobal(),
                        mHandle->ClearHalt(aDirection == USBDirection::In,
                                           aEndpoint), aRv);
}

already_AddRefed<Promise>
USBDevice::Reset(ErrorResult& aRv)
{
  if (!mOpened || !mHandle) {
    return BackendPromise(GetOwnerGlobal(), NS_ERROR_DOM_INVALID_STATE_ERR, aRv);
  }
  nsresult rv = mHandle->Reset();
  if (NS_SUCCEEDED(rv)) {
    nsTArray<RefPtr<USBInterface>> interfaces;
    if (mConfiguration) {
      mConfiguration->GetInterfaces(interfaces);
      for (const auto& interfaceObject : interfaces) {
        interfaceObject->SetClaimed(false);
      }
    }
  }
  return BackendPromise(GetOwnerGlobal(), rv, aRv);
}

already_AddRefed<Promise>
USBDevice::ControlTransferIn(const USBControlTransferParameters& aSetup,
                             uint16_t aLength, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!mOpened || !mHandle) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  nsresult rv = mHandle->ControlTransferIn(
    MakeControlTransferInfo(aSetup), aLength, data);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  RefPtr<USBInTransferResult> result = new USBInTransferResult(
    GetOwnerGlobal(), USBTransferStatus::Ok, data);
  promise->MaybeResolve(result);
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::ControlTransferOut(
  const USBControlTransferParameters& aSetup,
  const Optional<ArrayBufferViewOrArrayBuffer>& aData, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!mOpened || !mHandle) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  if (aData.WasPassed()) {
    CopyTransferData(aData.Value(), data);
  }
  uint32_t written = 0;
  nsresult rv = mHandle->ControlTransferOut(
    MakeControlTransferInfo(aSetup), data, written);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  RefPtr<USBOutTransferResult> result = new USBOutTransferResult(
    GetOwnerGlobal(), USBTransferStatus::Ok, written);
  promise->MaybeResolve(result);
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::TransferIn(uint8_t aEndpoint, uint32_t aLength, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!mOpened || !mHandle) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  nsresult rv = mHandle->TransferIn(aEndpoint, aLength, data);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }
  RefPtr<USBInTransferResult> result = new USBInTransferResult(
    GetOwnerGlobal(), USBTransferStatus::Ok, data);
  promise->MaybeResolve(result);
  return promise.forget();
}

already_AddRefed<Promise>
USBDevice::TransferOut(uint8_t aEndpoint,
                       const ArrayBufferViewOrArrayBuffer& aData,
                       ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  if (!mOpened || !mHandle) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsTArray<uint8_t> data;
  CopyTransferData(aData, data);
  uint32_t written = 0;
  nsresult rv = mHandle->TransferOut(aEndpoint, data, written);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }
  RefPtr<USBOutTransferResult> result = new USBOutTransferResult(
    GetOwnerGlobal(), USBTransferStatus::Ok, written);
  promise->MaybeResolve(result);
  return promise.forget();
}

} // namespace dom
} // namespace mozilla
