/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

#include "USBBackend.h"

#ifdef XP_WIN

#include <string.h>
#include <windows.h>
#include <setupapi.h>
#include <winusb.h>

#include "mozilla/fallible.h"
#include "nsError.h"

namespace mozilla {
namespace dom {

namespace {

const GUID kUSBDeviceInterface = {
  0xA5DCBF10, 0x6530, 0x11D2,
  { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED }
};

class WinUSBDeviceHandle final : public USBDeviceHandle
{
public:
  explicit WinUSBDeviceHandle(const nsAString& aPath)
    : mPath(aPath)
    , mDevice(INVALID_HANDLE_VALUE)
    , mInterface(nullptr)
  {
  }

  NS_DECL_ISUPPORTS

  nsresult Open() override
  {
    if (mPath.IsEmpty()) {
      return NS_ERROR_INVALID_ARG;
    }

    mDevice = CreateFileW(reinterpret_cast<const wchar_t*>(mPath.get()),
                          GENERIC_READ | GENERIC_WRITE,
                          FILE_SHARE_READ | FILE_SHARE_WRITE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                          nullptr);
    if (mDevice == INVALID_HANDLE_VALUE) {
      return NS_ERROR_FAILURE;
    }

    if (!WinUsb_Initialize(mDevice, &mInterface)) {
      CloseHandle(mDevice);
      mDevice = INVALID_HANDLE_VALUE;
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }

  void Close() override
  {
    if (mInterface) {
      WinUsb_Free(mInterface);
      mInterface = nullptr;
    }
    if (mDevice != INVALID_HANDLE_VALUE) {
      CloseHandle(mDevice);
      mDevice = INVALID_HANDLE_VALUE;
    }
  }

  nsresult GetConfigurations(
    nsTArray<USBConfigurationInfo>& aConfigurations) override
  {
    aConfigurations.Clear();
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    USB_DEVICE_DESCRIPTOR deviceDescriptor;
    ULONG transferred = 0;
    if (!WinUsb_GetDescriptor(mInterface, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                              reinterpret_cast<PUCHAR>(&deviceDescriptor),
                              sizeof(deviceDescriptor), &transferred) ||
        transferred < sizeof(deviceDescriptor)) {
      return NS_ERROR_FAILURE;
    }

    for (uint8_t index = 0; index < deviceDescriptor.bNumConfigurations;
         ++index) {
      uint8_t header[9];
      if (!WinUsb_GetDescriptor(mInterface, USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                index, 0, header, sizeof(header), &transferred) ||
          transferred < sizeof(header)) {
        continue;
      }

      uint16_t totalLength = static_cast<uint16_t>(header[2] |
                                                   (header[3] << 8));
      if (totalLength < sizeof(header)) {
        continue;
      }
      nsTArray<uint8_t> descriptor;
      if (!descriptor.SetLength(totalLength, mozilla::fallible) ||
          !WinUsb_GetDescriptor(mInterface, USB_CONFIGURATION_DESCRIPTOR_TYPE,
                                index, 0, descriptor.Elements(), totalLength,
                                &transferred) ||
          transferred < sizeof(header)) {
        continue;
      }

      USBConfigurationInfo configuration;
      configuration.mValue = descriptor[5];
      USBInterfaceInfo* currentInterface = nullptr;
      USBAlternateInterfaceInfo* currentAlternate = nullptr;
      uint32_t offset = descriptor[0];
      while (offset + 2 <= transferred) {
        uint8_t length = descriptor[offset];
        uint8_t type = descriptor[offset + 1];
        if (!length || offset + length > transferred) {
          break;
        }

        if (type == USB_INTERFACE_DESCRIPTOR_TYPE && length >= 9) {
          USBInterfaceInfo interfaceInfo;
          interfaceInfo.mNumber = descriptor[offset + 2];
          USBAlternateInterfaceInfo alternate;
          alternate.mSetting = descriptor[offset + 3];
          alternate.mClass = descriptor[offset + 5];
          alternate.mSubclass = descriptor[offset + 6];
          alternate.mProtocol = descriptor[offset + 7];
          configuration.mInterfaces.AppendElement(Move(interfaceInfo));
          currentInterface = &configuration.mInterfaces.LastElement();
          currentInterface->mAlternates.AppendElement(Move(alternate));
          currentAlternate = &currentInterface->mAlternates.LastElement();
        } else if (type == USB_ENDPOINT_DESCRIPTOR_TYPE && length >= 7 &&
                   currentAlternate) {
          USBEndpointInfo endpoint;
          uint8_t address = descriptor[offset + 2];
          endpoint.mNumber = address & 0x0f;
          endpoint.mDirection = (address & 0x80) ? 0 : 1;
          endpoint.mType = descriptor[offset + 3] & 0x03;
          endpoint.mPacketSize = static_cast<uint16_t>(
            descriptor[offset + 4] | (descriptor[offset + 5] << 8));
          if (endpoint.mType) {
            currentAlternate->mEndpoints.AppendElement(Move(endpoint));
          }
        }
        offset += length;
      }
      aConfigurations.AppendElement(Move(configuration));
    }
    return NS_OK;
  }

  nsresult SelectConfiguration(uint8_t aConfigurationValue) override
  {
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }

    WINUSB_SETUP_PACKET setup;
    setup.RequestType = 0x00; // Host-to-device, standard, device.
    setup.Request = 9;        // SET_CONFIGURATION.
    setup.Value = aConfigurationValue;
    setup.Index = 0;
    setup.Length = 0;
    ULONG transferred = 0;
    return WinUsb_ControlTransfer(mInterface, setup, nullptr, 0,
                                  &transferred, nullptr)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult ClaimInterface(uint8_t aInterfaceNumber) override
  {
    return mInterface && WinUsb_ClaimInterface(mInterface, aInterfaceNumber)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult ReleaseInterface(uint8_t aInterfaceNumber) override
  {
    return mInterface && WinUsb_ReleaseInterface(mInterface, aInterfaceNumber)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult SelectAlternateInterface(uint8_t aInterfaceNumber,
                                    uint8_t aAlternateSetting) override
  {
    return mInterface &&
           WinUsb_SetCurrentAlternateSetting(mInterface, aInterfaceNumber,
                                             aAlternateSetting)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult ClearHalt(bool aIn, uint8_t aEndpointNumber) override
  {
    uint8_t pipe = aEndpointNumber & 0x0f;
    if (aIn) {
      pipe |= 0x80;
    }
    return mInterface && WinUsb_ResetPipe(mInterface, pipe)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult Reset() override
  {
    return mInterface && WinUsb_ResetDevice(mInterface)
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsresult ControlTransferIn(const USBControlTransferInfo& aSetup,
                             uint16_t aLength,
                             nsTArray<uint8_t>& aData) override
  {
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    aData.Clear();
    if (!aData.SetLength(aLength, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    WINUSB_SETUP_PACKET setup;
    setup.RequestType = static_cast<UCHAR>(0x80 |
      ((aSetup.mRequestType & 0x03) << 5) | (aSetup.mRecipient & 0x1f));
    setup.Request = aSetup.mRequest;
    setup.Value = aSetup.mValue;
    setup.Index = aSetup.mIndex;
    setup.Length = aLength;
    ULONG transferred = 0;
    if (!WinUsb_ControlTransfer(mInterface, setup, aData.Elements(), aLength,
                                &transferred, nullptr)) {
      aData.Clear();
      return NS_ERROR_FAILURE;
    }
    aData.SetLength(transferred);
    return NS_OK;
  }

  nsresult ControlTransferOut(const USBControlTransferInfo& aSetup,
                              const nsTArray<uint8_t>& aData,
                              uint32_t& aWritten) override
  {
    aWritten = 0;
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    WINUSB_SETUP_PACKET setup;
    setup.RequestType = static_cast<UCHAR>(
      ((aSetup.mRequestType & 0x03) << 5) | (aSetup.mRecipient & 0x1f));
    setup.Request = aSetup.mRequest;
    setup.Value = aSetup.mValue;
    setup.Index = aSetup.mIndex;
    setup.Length = static_cast<USHORT>(aData.Length());
    ULONG transferred = 0;
    PUCHAR data = aData.IsEmpty()
      ? nullptr : const_cast<PUCHAR>(aData.Elements());
    if (!WinUsb_ControlTransfer(mInterface, setup, data, aData.Length(),
                                &transferred, nullptr)) {
      return NS_ERROR_FAILURE;
    }
    aWritten = transferred;
    return NS_OK;
  }

  nsresult TransferIn(uint8_t aEndpoint, uint32_t aLength,
                      nsTArray<uint8_t>& aData) override
  {
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    if (!aData.SetLength(aLength, mozilla::fallible)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    ULONG transferred = 0;
    if (!WinUsb_ReadPipe(mInterface, aEndpoint | 0x80, aData.Elements(),
                         aLength, &transferred, nullptr)) {
      aData.Clear();
      return NS_ERROR_FAILURE;
    }
    aData.SetLength(transferred);
    return NS_OK;
  }

  nsresult TransferOut(uint8_t aEndpoint, const nsTArray<uint8_t>& aData,
                       uint32_t& aWritten) override
  {
    aWritten = 0;
    if (!mInterface) {
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    }
    ULONG transferred = 0;
    PUCHAR data = aData.IsEmpty()
      ? nullptr : const_cast<PUCHAR>(aData.Elements());
    if (!WinUsb_WritePipe(mInterface, aEndpoint, data, aData.Length(),
                          &transferred, nullptr)) {
      return NS_ERROR_FAILURE;
    }
    aWritten = transferred;
    return NS_OK;
  }

private:
  ~WinUSBDeviceHandle() override
  {
    Close();
  }

  nsString mPath;
  HANDLE mDevice;
  WINUSB_INTERFACE_HANDLE mInterface;
};

NS_IMPL_ISUPPORTS(WinUSBDeviceHandle, USBDeviceHandle)

bool
ParseHex(const wchar_t* aValue, uint32_t aLength, uint16_t& aResult)
{
  uint32_t value = 0;
  for (uint32_t i = 0; i < aLength; ++i) {
    wchar_t c = aValue[i];
    uint32_t digit;
    if (c >= L'0' && c <= L'9') {
      digit = c - L'0';
    } else if (c >= L'A' && c <= L'F') {
      digit = c - L'A' + 10;
    } else if (c >= L'a' && c <= L'f') {
      digit = c - L'a' + 10;
    } else {
      return false;
    }
    value = (value << 4) | digit;
  }

  if (value > 0xffff) {
    return false;
  }
  aResult = static_cast<uint16_t>(value);
  return true;
}

void
CopyRegistryString(HDEVINFO aDevices,
                   SP_DEVINFO_DATA& aDevice,
                   DWORD aProperty,
                   nsString& aValue)
{
  DWORD type = 0;
  DWORD size = 0;
  if (!SetupDiGetDeviceRegistryPropertyW(aDevices, &aDevice, aProperty,
                                         &type, nullptr, 0, &size) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER ||
      (type != REG_SZ && type != REG_MULTI_SZ)) {
    return;
  }

  nsTArray<wchar_t> buffer;
  if (!buffer.SetLength((size / sizeof(wchar_t)) + 1, mozilla::fallible)) {
    return;
  }

  if (SetupDiGetDeviceRegistryPropertyW(aDevices, &aDevice, aProperty,
                                        &type,
                                        reinterpret_cast<BYTE*>(buffer.Elements()),
                                        size, nullptr)) {
    aValue.Assign(reinterpret_cast<const char16_t*>(buffer.Elements()));
  }
}

void
ParseHardwareId(const nsString& aHardwareId, USBDeviceInfo& aInfo)
{
  const char16_t* id = aHardwareId.BeginReading();
  uint32_t length = aHardwareId.Length();

  for (uint32_t i = 0; i + 8 <= length; ++i) {
    if ((id[i] == 'V' || id[i] == 'v') &&
        (id[i + 1] == 'I' || id[i + 1] == 'i') &&
        (id[i + 2] == 'D' || id[i + 2] == 'd') && id[i + 3] == '_') {
      uint16_t value;
      if (ParseHex(reinterpret_cast<const wchar_t*>(id + i + 4), 4, value)) {
        aInfo.mVendorId = value;
      }
    }

    if ((id[i] == 'P' || id[i] == 'p') &&
        (id[i + 1] == 'I' || id[i + 1] == 'i') &&
        (id[i + 2] == 'D' || id[i + 2] == 'd') && id[i + 3] == '_') {
      uint16_t value;
      if (ParseHex(reinterpret_cast<const wchar_t*>(id + i + 4), 4, value)) {
        aInfo.mProductId = value;
      }
    }
  }
}

} // anonymous namespace

nsresult
EnumerateUSBDevices(nsTArray<USBDeviceInfo>& aDevices)
{
  aDevices.Clear();

  HDEVINFO devices = SetupDiGetClassDevsW(&kUSBDeviceInterface, nullptr,
                                           nullptr,
                                           DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
  if (devices == INVALID_HANDLE_VALUE) {
    return NS_ERROR_FAILURE;
  }

  for (DWORD index = 0; ; ++index) {
    SP_DEVICE_INTERFACE_DATA interfaceData;
    memset(&interfaceData, 0, sizeof(interfaceData));
    interfaceData.cbSize = sizeof(interfaceData);
    if (!SetupDiEnumDeviceInterfaces(devices, nullptr, &kUSBDeviceInterface,
                                     index, &interfaceData)) {
      break;
    }

    DWORD detailSize = 0;
    SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, nullptr, 0,
                                    &detailSize, nullptr);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || !detailSize) {
      continue;
    }

    nsTArray<BYTE> detailBuffer;
    if (!detailBuffer.SetLength(detailSize, mozilla::fallible)) {
      continue;
    }
    SP_DEVICE_INTERFACE_DETAIL_DATA_W* detail =
      reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.Elements());
    detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

    SP_DEVINFO_DATA device;
    memset(&device, 0, sizeof(device));
    device.cbSize = sizeof(device);
    if (!SetupDiGetDeviceInterfaceDetailW(devices, &interfaceData, detail,
                                          detailSize, nullptr, &device)) {
      continue;
    }

    nsString hardwareId;
    CopyRegistryString(devices, device, SPDRP_HARDWAREID, hardwareId);

    USBDeviceInfo info;
    ParseHardwareId(hardwareId, info);
    if (!info.mVendorId && !info.mProductId) {
      continue;
    }

    CopyRegistryString(devices, device, SPDRP_MFG, info.mManufacturerName);
    CopyRegistryString(devices, device, SPDRP_DEVICEDESC, info.mProductName);
    info.mDevicePath.Assign(reinterpret_cast<const char16_t*>(detail->DevicePath));
    aDevices.AppendElement(Move(info));
  }

  SetupDiDestroyDeviceInfoList(devices);
  return NS_OK;
}

already_AddRefed<USBDeviceHandle>
CreateUSBDeviceHandle(const nsAString& aDevicePath)
{
  RefPtr<USBDeviceHandle> handle = new WinUSBDeviceHandle(aDevicePath);
  return handle.forget();
}

} // namespace dom
} // namespace mozilla

#endif // XP_WIN
