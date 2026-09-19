/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#ifndef mozilla_dom_USB_h
#define mozilla_dom_USB_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/USBBackend.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class Promise;
class USBDevice;
struct USBDeviceRequestOptions;

class USB final : public DOMEventTargetHelper
{
public:
  explicit USB(nsIGlobalObject* aOwner);

  IMPL_EVENT_HANDLER(connect)
  IMPL_EVENT_HANDLER(disconnect)

  already_AddRefed<Promise> GetDevices(ErrorResult& aRv);
  already_AddRefed<Promise> RequestDevice(
    const USBDeviceRequestOptions& aOptions, ErrorResult& aRv);

  void AddAuthorizedDevice(USBDevice* aDevice);
  void RemoveAuthorizedDevice(USBDevice* aDevice);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(USB, DOMEventTargetHelper)

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

private:
  ~USB();

  nsTArray<RefPtr<USBDevice>> mAuthorizedDevices;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_USB_h
