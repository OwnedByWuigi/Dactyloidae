/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "WebUSB.h"

#include "mozilla/dom/USBBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/USBDevice.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsapi.h"
#include "nsContentPermissionHelper.h"
#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "nsError.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

namespace {

bool
MatchesFilter(const USBDeviceInfo& aDevice, const USBDeviceFilter& aFilter)
{
  if (aFilter.mVendorId.WasPassed() &&
      aFilter.mVendorId.Value() != aDevice.mVendorId) {
    return false;
  }
  if (aFilter.mProductId.WasPassed() &&
      aFilter.mProductId.Value() != aDevice.mProductId) {
    return false;
  }
  if (aFilter.mClassCode.WasPassed() &&
      aFilter.mClassCode.Value() != aDevice.mDeviceClass) {
    return false;
  }
  if (aFilter.mSubclassCode.WasPassed() &&
      aFilter.mSubclassCode.Value() != aDevice.mDeviceSubclass) {
    return false;
  }
  if (aFilter.mProtocolCode.WasPassed() &&
      aFilter.mProtocolCode.Value() != aDevice.mDeviceProtocol) {
    return false;
  }
  if (aFilter.mSerialNumber.WasPassed() &&
      !aFilter.mSerialNumber.Value().Equals(aDevice.mSerialNumber)) {
    return false;
  }
  return true;
}

bool
MatchesRequest(const USBDeviceInfo& aDevice,
               const USBDeviceRequestOptions& aOptions)
{
  // Chromium accepts a chooser request that has no restrictive filters and
  // lets the user select from the enumerated USB devices.  Keep that useful
  // behaviour in UXP as well; an empty filter list must not suppress the
  // permission prompt entirely.
  bool included = aOptions.mFilters.IsEmpty();
  for (const auto& filter : aOptions.mFilters) {
    if (MatchesFilter(aDevice, filter)) {
      included = true;
      break;
    }
  }
  if (!included) {
    return false;
  }

  for (const auto& filter : aOptions.mExclusionFilters) {
    if (MatchesFilter(aDevice, filter)) {
      return false;
    }
  }
  return true;
}

class USBPermissionRequest final : public nsIContentPermissionRequest
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(USBPermissionRequest,
                                           nsIContentPermissionRequest)

  USBPermissionRequest(USB* aUSB,
                       Promise* aPromise,
                       nsPIDOMWindowInner* aWindow,
                       nsTArray<RefPtr<USBDevice>>&& aCandidates)
    : mUSB(aUSB)
    , mPromise(aPromise)
    , mWindow(aWindow)
    , mCandidates(Move(aCandidates))
    , mRequester(new nsContentPermissionRequester(aWindow))
  {
  }

private:
  ~USBPermissionRequest() = default;

  RefPtr<USB> mUSB;
  RefPtr<Promise> mPromise;
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsTArray<RefPtr<USBDevice>> mCandidates;
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USBPermissionRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentPermissionRequest)
  NS_INTERFACE_MAP_ENTRY(nsIContentPermissionRequest)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(USBPermissionRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(USBPermissionRequest)
NS_IMPL_CYCLE_COLLECTION(USBPermissionRequest, mUSB, mPromise, mWindow,
                         mCandidates, mRequester)

NS_IMETHODIMP
USBPermissionRequest::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_ENSURE_ARG_POINTER(aPrincipal);
  *aPrincipal = nullptr;
  if (!mWindow || !mWindow->GetDoc()) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPrincipal> principal = mWindow->GetDoc()->NodePrincipal();
  principal.forget(aPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
USBPermissionRequest::GetTypes(nsIArray** aTypes)
{
  nsTArray<nsString> options;
  for (const auto& device : mCandidates) {
    DOMString label;
    device->GetProductName(label);
    nsAutoString labelString;
    label.ToString(labelString);
    if (label.IsNull() || labelString.IsEmpty()) {
      options.AppendElement(NS_LITERAL_STRING("USB device"));
    } else {
      options.AppendElement(labelString);
    }
  }
  return nsContentPermissionUtils::CreatePermissionArray(
    NS_LITERAL_CSTRING("usb"), NS_LITERAL_CSTRING("device"), options, aTypes);
}

NS_IMETHODIMP
USBPermissionRequest::GetWindow(mozIDOMWindow** aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);
  nsCOMPtr<mozIDOMWindow> window = do_QueryInterface(mWindow);
  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
USBPermissionRequest::GetElement(nsIDOMElement** aElement)
{
  NS_ENSURE_ARG_POINTER(aElement);
  *aElement = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
USBPermissionRequest::GetRequester(nsIContentPermissionRequester** aRequester)
{
  NS_ENSURE_ARG_POINTER(aRequester);
  nsCOMPtr<nsIContentPermissionRequester> requester = mRequester;
  requester.forget(aRequester);
  return NS_OK;
}

NS_IMETHODIMP
USBPermissionRequest::Cancel()
{
  mPromise->MaybeReject(NS_ERROR_DOM_NOT_ALLOWED_ERR);
  return NS_OK;
}

NS_IMETHODIMP
USBPermissionRequest::Allow(JS::HandleValue aChoices)
{
  if (mCandidates.IsEmpty()) {
    mPromise->MaybeReject(NS_ERROR_DOM_NOT_FOUND_ERR);
    return NS_OK;
  }

  uint32_t selected = 0;
  if (aChoices.isObject()) {
    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    if (cx) {
      JS::RootedObject object(cx, &aChoices.toObject());
      JSAutoCompartment ac(cx, object);
      JS::RootedValue value(cx);
      if (JS_GetProperty(cx, object, "usb", &value) && value.isString()) {
        nsAutoJSString choice;
        if (choice.init(cx, value)) {
          for (uint32_t i = 0; i < mCandidates.Length(); ++i) {
            DOMString label;
            mCandidates[i]->GetProductName(label);
            nsAutoString labelString;
            label.ToString(labelString);
            if ((label.IsNull() || labelString.IsEmpty()) &&
                choice.EqualsLiteral("USB device")) {
              selected = i;
              break;
            }
            if (!label.IsNull() && labelString.Equals(choice)) {
              selected = i;
              break;
            }
          }
        }
      } else {
        JS_ClearPendingException(cx);
      }
    }
  }

  RefPtr<USBDevice> device = mCandidates[selected];
  mUSB->AddAuthorizedDevice(device);
  mPromise->MaybeResolve(device.get());
  return NS_OK;
}

} // anonymous namespace

NS_IMPL_CYCLE_COLLECTION_INHERITED(USB, DOMEventTargetHelper,
                                   mAuthorizedDevices)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(USB)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(USB, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(USB, DOMEventTargetHelper)

USB::USB(nsIGlobalObject* aOwner)
  : DOMEventTargetHelper(aOwner)
{
}

USB::~USB()
{
  for (const auto& device : mAuthorizedDevices) {
    device->ClearUSB();
  }
}

void
USB::AddAuthorizedDevice(USBDevice* aDevice)
{
  if (!aDevice || mAuthorizedDevices.Contains(aDevice)) {
    return;
  }
  aDevice->SetUSB(this);
  mAuthorizedDevices.AppendElement(aDevice);
}

void
USB::RemoveAuthorizedDevice(USBDevice* aDevice)
{
  if (!aDevice) {
    return;
  }
  aDevice->ClearUSB();
  mAuthorizedDevices.RemoveElement(aDevice);
}

JSObject*
USB::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return USBBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise>
USB::GetDevices(ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  promise->MaybeResolve(mAuthorizedDevices);
  return promise.forget();
}

already_AddRefed<Promise>
USB::RequestDevice(const USBDeviceRequestOptions& aOptions, ErrorResult& aRv)
{
  RefPtr<Promise> promise = Promise::Create(GetOwnerGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  nsTArray<USBDeviceInfo> infos;
  nsresult rv = EnumerateUSBDevices(infos);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  nsTArray<RefPtr<USBDevice>> candidates;
  for (const auto& info : infos) {
    if (!MatchesRequest(info, aOptions)) {
      continue;
    }
    RefPtr<USBDevice> device = new USBDevice(GetOwnerGlobal());
    device->SetDeviceInfo(info);
    candidates.AppendElement(device);
  }

  if (candidates.IsEmpty()) {
    promise->MaybeReject(NS_ERROR_DOM_NOT_FOUND_ERR);
    return promise.forget();
  }

  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetOwnerGlobal());
  if (!window) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  RefPtr<USBPermissionRequest> request =
    new USBPermissionRequest(this, promise, window, Move(candidates));
  rv = nsContentPermissionUtils::AskPermission(request, window);
  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
  }
  return promise.forget();
}

} // namespace dom
} // namespace mozilla
