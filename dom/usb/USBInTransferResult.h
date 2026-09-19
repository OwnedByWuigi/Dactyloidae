/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBInTransferResult_h
#define mozilla_dom_USBInTransferResult_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/USBBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBInTransferResult final : public nsISupports, public nsWrapperCache
{
public:
  USBInTransferResult(nsIGlobalObject*, USBTransferStatus,
                      const nsTArray<uint8_t>&);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBInTransferResult)
  nsISupports* GetParentObject() const { return mOwner; }
  USBTransferStatus Status() const { return mStatus; }
  void GetData(JSContext*, JS::MutableHandle<JSObject*>, ErrorResult&);
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBInTransferResult();
  nsCOMPtr<nsIGlobalObject> mOwner;
  USBTransferStatus mStatus;
  nsTArray<uint8_t> mRawData;
  JS::Heap<JSObject*> mData;
};
} }
#endif // mozilla_dom_USBInTransferResult_h
