/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBEndpoint_h
#define mozilla_dom_USBEndpoint_h

#include "mozilla/dom/USBBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBEndpoint final : public nsISupports, public nsWrapperCache
{
public:
  USBEndpoint(nsIGlobalObject*, uint8_t, USBDirection, USBEndpointType, uint16_t);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBEndpoint)
  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t EndpointNumber() const { return mNumber; }
  USBDirection Direction() const { return mDirection; }
  USBEndpointType Type() const { return mType; }
  uint16_t PacketSize() const { return mPacketSize; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBEndpoint() = default;
  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mNumber;
  USBDirection mDirection;
  USBEndpointType mType;
  uint16_t mPacketSize;
};
} }
#endif // mozilla_dom_USBEndpoint_h
