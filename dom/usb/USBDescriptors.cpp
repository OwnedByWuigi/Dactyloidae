/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "USBDescriptors.h"

#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ArrayBuffer.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(USBEndpoint)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBEndpoint)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBEndpoint)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBEndpoint)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBEndpoint::USBEndpoint(nsIGlobalObject* aOwner, uint8_t aNumber,
                         USBDirection aDirection, USBEndpointType aType,
                         uint16_t aPacketSize)
  : mOwner(aOwner)
  , mNumber(aNumber)
  , mDirection(aDirection)
  , mType(aType)
  , mPacketSize(aPacketSize)
{
}

JSObject*
USBEndpoint::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USBEndpointBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USBAlternateInterface, mEndpoints)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBAlternateInterface)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBAlternateInterface)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBAlternateInterface)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBAlternateInterface::USBAlternateInterface(
  nsIGlobalObject* aOwner, uint8_t aSetting, uint8_t aClass,
  uint8_t aSubclass, uint8_t aProtocol,
  nsTArray<RefPtr<USBEndpoint>>&& aEndpoints)
  : mOwner(aOwner)
  , mSetting(aSetting)
  , mClass(aClass)
  , mSubclass(aSubclass)
  , mProtocol(aProtocol)
  , mEndpoints(Move(aEndpoints))
{
}

JSObject*
USBAlternateInterface::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  return USBAlternateInterfaceBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USBInterface, mAlternate, mAlternates)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBInterface)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBInterface)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBInterface)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBInterface::USBInterface(
  nsIGlobalObject* aOwner, uint8_t aNumber,
  nsTArray<RefPtr<USBAlternateInterface>>&& aAlternates)
  : mOwner(aOwner)
  , mNumber(aNumber)
  , mAlternates(Move(aAlternates))
  , mClaimed(false)
{
  if (!mAlternates.IsEmpty()) {
    mAlternate = mAlternates[0];
  }
}

void
USBInterface::SetAlternate(uint8_t aSetting)
{
  for (const auto& alternate : mAlternates) {
    if (alternate->AlternateSetting() == aSetting) {
      mAlternate = alternate;
      return;
    }
  }
}

JSObject*
USBInterface::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USBInterfaceBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USBConfiguration, mInterfaces)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBConfiguration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBConfiguration)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBConfiguration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBConfiguration::USBConfiguration(
  nsIGlobalObject* aOwner, uint8_t aValue,
  nsTArray<RefPtr<USBInterface>>&& aInterfaces)
  : mOwner(aOwner)
  , mValue(aValue)
  , mInterfaces(Move(aInterfaces))
{
}

JSObject*
USBConfiguration::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USBConfigurationBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(USBInTransferResult)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(USBInTransferResult)
  tmp->mData = nullptr;
  mozilla::DropJSObjects(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(USBInTransferResult)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOwner)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(USBInTransferResult)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mData)
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBInTransferResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBInTransferResult)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBInTransferResult)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBInTransferResult::USBInTransferResult(
  nsIGlobalObject* aOwner, USBTransferStatus aStatus,
  const nsTArray<uint8_t>& aData)
  : mOwner(aOwner)
  , mStatus(aStatus)
  , mRawData(aData)
  , mData(nullptr)
{
  mozilla::HoldJSObjects(this);
}

USBInTransferResult::~USBInTransferResult()
{
  mData = nullptr;
  mozilla::DropJSObjects(this);
}

void
USBInTransferResult::GetData(JSContext* aCx,
                             JS::MutableHandle<JSObject*> aData)
{
  if (!mData) {
    mData = ArrayBuffer::Create(aCx, this, mRawData.Length(),
                                mRawData.Elements());
    if (!mData) {
      aData.set(nullptr);
      return;
    }
    mRawData.Clear();
  }
  aData.set(mData);
}

JSObject*
USBInTransferResult::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto)
{
  return USBInTransferResultBinding::Wrap(aCx, this, aGivenProto);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(USBOutTransferResult, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(USBOutTransferResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBOutTransferResult)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBOutTransferResult)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

USBOutTransferResult::USBOutTransferResult(nsIGlobalObject* aOwner,
                                           USBTransferStatus aStatus,
                                           uint32_t aBytesWritten)
  : mOwner(aOwner)
  , mStatus(aStatus)
  , mBytesWritten(aBytesWritten)
{
}

JSObject*
USBOutTransferResult::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return USBOutTransferResultBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
