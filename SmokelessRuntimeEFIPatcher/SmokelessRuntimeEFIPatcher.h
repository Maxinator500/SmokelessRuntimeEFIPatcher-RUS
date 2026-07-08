#pragma once

#include <Uefi.h>
#include <PiDxe.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemInfo.h>
#include <Guid/GlobalVariable.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiHiiServicesLib.h>           //Needed for strings
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/PeCoffGetEntryPointLib.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/FormBrowser2.h>
#include <Protocol/FormBrowserEx.h>
#include <Protocol/FormBrowserEx2.h>
#include <Protocol/AcpiSystemDescriptionTable.h>
#include <Protocol/DisplayProtocol.h>
#include <Protocol/HiiPopup.h>
#include <Protocol/RegularExpressionProtocol.h>   //Needed for regex
#include <Protocol/HiiPackageList.h>              //       for strings
#include <Library/HiiLib.h>                       //       for fonts and strings
#include <Library/UefiRuntimeServicesTableLib.h>  //       for gRT
#include <Library/ShellLib.h>                     //       for FindNextFile
#include <Library/ShellCommandLib.h>              //       for HandleParsingLib' CatSDumpHex
#include <Library/HandleParsingLib.h>             //       for FindLoadedImageFromShellIndex
#include <Library/TimerLib.h>                     //       for Perf counter

#include "FirmwareVolumeProtocol.h"               //FirmwareVolumeProtocol definition from EDK1

//C2220 suppression due to 'declaration of identifier hiding global declaration'
#pragma warning(disable:4459)

#define APTIO_SETUP_UTILITY_PACKAGE_LIST_GUID { 0x899407D7, 0x99FE, 0x43d8, 0x9A, 0x21, 0x79, 0xEC, 0x32, 0x8C, 0xAC, 0x21 };
#define INSYDE_SETUP_UTILITY_PACKAGE_LIST_GUID { 0xFE3542FE, 0xC1D3, 0x4EF8, 0x65, 0x7c, 0x80, 0x48, 0x60, 0x6f, 0xf6, 0x70 };

EFI_HII_HANDLE HiiHandleSREP;
EFI_GUID gHIIRussianFontGuid;
EFI_GUID gEfiSREPLangVariableGuid = { 0x257ea3f4, 0xc447, 0x4b22, { 0xbb, 0x1c, 0x37, 0x9b, 0x7f, 0xde, 0x28, 0x3e } };
EFI_GUID gEfiGifDecoder2ProtocolGuid = { 0x7ef36949, 0xd78b, 0x47e5, { 0x90, 0x9d, 0x50, 0xf8, 0xb3, 0x47, 0xac, 0x1f } };
const UINTN GenericBufferSize = 0x200;

const UINT8 DevicePathHeaderUint[] = { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, (UINT8)sizeof(VENDOR_DEVICE_PATH), 0x00 };
const UINT8 ifrSetupStringsPointerUint[] = { EFI_HII_PACKAGE_STRINGS, 0x34, 0x00, 0x00, 0x00, 0x34 };
const UINT8 ifrHiiBinPointerHeaderUint[] = { EFI_HII_PACKAGE_FORMS, EFI_IFR_FORM_SET_OP, 0xA7 };

EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *Console = NULL;
EFI_FILE *LogFile = NULL;
CHAR16 Log[0x200];

extern VOID LogToFile(IN EFI_FILE *LogFile, IN CHAR16 *String);
extern UINTN PrintConsoleSREP(IN CONST CHAR16 *Format, ...);
