/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
#ifndef mozilla_dom_USBConfiguration_h
#define mozilla_dom_USBConfiguration_h

#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/USBConfigurationBinding.h"
#include "mozilla/dom/USBInterface.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWrapperCache.h"

namespace mozilla { namespace dom {
class USBConfiguration final : public nsISupports, public nsWrapperCache
{
public:
  USBConfiguration(nsIGlobalObject*, uint8_t,
                   nsTArray<RefPtr<USBInterface>>&&);
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(USBConfiguration)
  nsISupports* GetParentObject() const { return mOwner; }
  uint8_t ConfigurationValue() const { return mValue; }
  void GetConfigurationName(Nullable<nsString>& aValue) const { aValue = mName; }
  void GetInterfaces(nsTArray<RefPtr<USBInterface>>& aValue) const { aValue = mInterfaces; }
  JSObject* WrapObject(JSContext*, JS::Handle<JSObject*>) override;
private:
  ~USBConfiguration() = default;
  nsCOMPtr<nsIGlobalObject> mOwner;
  uint8_t mValue;
  Nullable<nsString> mName;
  nsTArray<RefPtr<USBInterface>> mInterfaces;
};
} }
#endif // mozilla_dom_USBConfiguration_h
