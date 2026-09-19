/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBOutTransferResult_h
#define mozilla_dom_USBOutTransferResult_h

#include "mozilla/dom/USBBinding.h"
#include "mozilla/dom/USBOutTransferResultBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBOutTransferResult final : public nsISupports, public nsWrapperCache
{
public:
  USBOutTransferResult(nsIGlobalObject*, USBTransferStatus, uint32_t);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBOutTransferResult)
  nsISupports* GetParentObject() const { return mOwner; }
  USBTransferStatus Status() const { return mStatus; }
  uint32_t BytesWritten() const { return mBytesWritten; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBOutTransferResult() = default;
  nsCOMPtr<nsIGlobalObject> mOwner;
  USBTransferStatus mStatus;
  uint32_t mBytesWritten;
};
} }
#endif // mozilla_dom_USBOutTransferResult_h
