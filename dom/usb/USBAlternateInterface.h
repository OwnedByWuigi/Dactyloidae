/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBAlternateInterface_h
#define mozilla_dom_USBAlternateInterface_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/DOMString.h"
#include "mozilla/dom/USBEndpoint.h"
#include "mozilla/dom/USBBinding.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBAlternateInterface final : public nsISupports, public nsWrapperCache
{
public:
  USBAlternateInterface(nsIGlobalObject*, uint8_t, uint8_t, uint8_t, uint8_t,
                        nsTArray<RefPtr<USBEndpoint>>&&);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBAlternateInterface)
  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t AlternateSetting() const { return mSetting; }
  uint8_t InterfaceClass() const { return mClass; }
  uint8_t InterfaceSubclass() const { return mSubclass; }
  uint8_t InterfaceProtocol() const { return mProtocol; }
  void GetInterfaceName(DOMString& aValue) const
  {
    if (mName.IsNull()) aValue.SetNull();
    else aValue.SetOwnedString(mName.Value());
  }
  void GetEndpoints(nsTArray<RefPtr<USBEndpoint>>& aValue) const { aValue = mEndpoints; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBAlternateInterface() = default;
  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mSetting, mClass, mSubclass, mProtocol;
  Nullable<nsString> mName;
  nsTArray<RefPtr<USBEndpoint>> mEndpoints;
};
} }
#endif // mozilla_dom_USBAlternateInterface_h
