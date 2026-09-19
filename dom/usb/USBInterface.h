/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBInterface_h
#define mozilla_dom_USBInterface_h

#include "mozilla/dom/USBInterfaceBinding.h"
#include "mozilla/dom/USBAlternateInterface.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBInterface final : public nsISupports, public nsWrapperCache
{
public:
  USBInterface(nsIGlobalObject*, uint8_t,
               nsTArray<RefPtr<USBAlternateInterface>>&&);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBInterface)
  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t InterfaceNumber() const { return mNumber; }
  USBAlternateInterface* GetAlternate() const { return mAlternate; }
  void SetAlternate(uint8_t);
  void GetAlternates(nsTArray<RefPtr<USBAlternateInterface>>& aValue) const { aValue = mAlternates; }
  bool Claimed() const { return mClaimed; }
  void SetClaimed(bool aClaimed) { mClaimed = aClaimed; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBInterface() = default;
  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mNumber;
  RefPtr<USBAlternateInterface> mAlternate;
  nsTArray<RefPtr<USBAlternateInterface>> mAlternates;
  bool mClaimed;
};
} }
#endif // mozilla_dom_USBInterface_h
