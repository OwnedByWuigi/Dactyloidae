/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef mozilla_dom_USBDescriptors_h
#define mozilla_dom_USBDescriptors_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/USBBinding.h"
#include "mozilla/dom/USBAlternateInterfaceBinding.h"
#include "mozilla/dom/USBConfigurationBinding.h"
#include "mozilla/dom/USBEndpointBinding.h"
#include "mozilla/dom/USBInterfaceBinding.h"
#include "mozilla/dom/USBInTransferResultBinding.h"
#include "mozilla/dom/USBOutTransferResultBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class USBEndpoint final : public nsISupports, public nsWrapperCache
{
public:
  USBEndpoint(nsIGlobalObject* aOwner, uint8_t aNumber,
              USBDirection aDirection, USBEndpointType aType,
              uint16_t aPacketSize);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBEndpoint)

  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t EndpointNumber() const { return mNumber; }
  USBDirection Direction() const { return mDirection; }
  USBEndpointType Type() const { return mType; }
  uint16_t PacketSize() const { return mPacketSize; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBEndpoint() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mNumber;
  USBDirection mDirection;
  USBEndpointType mType;
  uint16_t mPacketSize;
};

class USBAlternateInterface final : public nsISupports, public nsWrapperCache
{
public:
  USBAlternateInterface(nsIGlobalObject* aOwner, uint8_t aSetting,
                        uint8_t aClass, uint8_t aSubclass, uint8_t aProtocol,
                        nsTArray<RefPtr<USBEndpoint>>&& aEndpoints);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBAlternateInterface)

  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t AlternateSetting() const { return mSetting; }
  uint8_t InterfaceClass() const { return mClass; }
  uint8_t InterfaceSubclass() const { return mSubclass; }
  uint8_t InterfaceProtocol() const { return mProtocol; }
  void GetInterfaceName(Nullable<nsString>& aValue) const
  { aValue = mName; }
  void GetEndpoints(nsTArray<RefPtr<USBEndpoint>>& aValue) const
  { aValue = mEndpoints; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBAlternateInterface() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mSetting;
  uint8_t mClass;
  uint8_t mSubclass;
  uint8_t mProtocol;
  Nullable<nsString> mName;
  nsTArray<RefPtr<USBEndpoint>> mEndpoints;
};

class USBInterface final : public nsISupports, public nsWrapperCache
{
public:
  USBInterface(nsIGlobalObject* aOwner, uint8_t aNumber,
               nsTArray<RefPtr<USBAlternateInterface>>&& aAlternates);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBInterface)

  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t InterfaceNumber() const { return mNumber; }
  USBAlternateInterface* GetAlternate() const { return mAlternate; }
  void SetAlternate(uint8_t aSetting);
  void GetAlternates(nsTArray<RefPtr<USBAlternateInterface>>& aValue) const
  { aValue = mAlternates; }
  bool Claimed() const { return mClaimed; }
  void SetClaimed(bool aClaimed) { mClaimed = aClaimed; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBInterface() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mNumber;
  RefPtr<USBAlternateInterface> mAlternate;
  nsTArray<RefPtr<USBAlternateInterface>> mAlternates;
  bool mClaimed;
};

class USBConfiguration final : public nsISupports, public nsWrapperCache
{
public:
  USBConfiguration(nsIGlobalObject* aOwner, uint8_t aValue,
                   nsTArray<RefPtr<USBInterface>>&& aInterfaces);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBConfiguration)

  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t ConfigurationValue() const { return mValue; }
  void GetConfigurationName(Nullable<nsString>& aValue) const
  { aValue = mName; }
  void GetInterfaces(nsTArray<RefPtr<USBInterface>>& aValue) const
  { aValue = mInterfaces; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBConfiguration() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mValue;
  Nullable<nsString> mName;
  nsTArray<RefPtr<USBInterface>> mInterfaces;
};

class USBInTransferResult final : public nsISupports, public nsWrapperCache
{
public:
  USBInTransferResult(nsIGlobalObject* aOwner, USBTransferStatus aStatus,
                      const nsTArray<uint8_t>& aData);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBInTransferResult)

  nsISupports* GetParentObject() const { return mOwner; }
  USBTransferStatus Status() const { return mStatus; }
  void GetData(JSContext* aCx, JS::MutableHandle<JSObject*> aData,
               ErrorResult& aRv);

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBInTransferResult();

  nsCOMPtr<nsIGlobalObject> mOwner;
  USBTransferStatus mStatus;
  nsTArray<uint8_t> mRawData;
  JS::Heap<JSObject*> mData;
};

class USBOutTransferResult final : public nsISupports, public nsWrapperCache
{
public:
  USBOutTransferResult(nsIGlobalObject* aOwner, USBTransferStatus aStatus,
                       uint32_t aBytesWritten);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBOutTransferResult)

  nsISupports* GetParentObject() const { return mOwner; }
  USBTransferStatus Status() const { return mStatus; }
  uint32_t BytesWritten() const { return mBytesWritten; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USBOutTransferResult() = default;

  nsCOMPtr<nsIGlobalObject> mOwner;
  USBTransferStatus mStatus;
  uint32_t mBytesWritten;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USBDescriptors_h
