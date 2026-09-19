/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "USBBackend.h"

#include "nsError.h"

namespace mozilla {
namespace dom {

#ifndef XP_WIN
nsresult
EnumerateUSBDevices(nsTArray<USBDeviceInfo>& aDevices)
{
  aDevices.Clear();
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<USBDeviceHandle>
CreateUSBDeviceHandle(const nsAString& aDevicePath)
{
  (void)aDevicePath;
  return nullptr;
}
#endif

} // namespace dom
} // namespace mozilla
