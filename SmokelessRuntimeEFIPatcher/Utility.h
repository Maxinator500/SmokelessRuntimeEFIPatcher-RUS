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
#include <Protocol/ShellParameters.h>             //Needed for GetArgs
#include <Protocol/RegularExpressionProtocol.h>   //       for regex
#include <Protocol/HiiPackageList.h>              //       for strings
#include <Library/HiiLib.h>                       //       for fonts and strings
#include <Library/UefiRuntimeServicesTableLib.h>  //       for gRT
#include <Library/ShellLib.h>                     //       for FindNextFile
#include <Library/ShellCommandLib.h>              //       for HandleParsingLib' CatSDumpHex
#include <Library/HandleParsingLib.h>             //       for FindLoadedImageFromShellIndex

#include "FirmwareVolumeProtocol.h"               //FirmwareVolumeProtocol definition from EDK1

//C2220 suppression due to 'declaration of identifier hiding global declaration'
#pragma warning(disable:4459)

#define APTIO_SETUP_UTILITY_PACKAGE_LIST_GUID { 0x899407D7, 0x99FE, 0x43d8, 0x9A, 0x21, 0x79, 0xEC, 0x32, 0x8C, 0xAC, 0x21 };
#define INSYDE_SETUP_UTILITY_PACKAGE_LIST_GUID { 0xFE3542FE, 0xC1D3, 0x4EF8, 0x65, 0x7c, 0x80, 0x48, 0x60, 0x6f, 0xf6, 0x70 };

EFI_HII_HANDLE HiiHandleSREP;
EFI_GUID gHIIRussianFontGuid;
EFI_GUID gEfiSREPLangVariableGuid = { 0x257ea3f4, 0xc447, 0x4b22, { 0xbb, 0x1c, 0x37, 0x9b, 0x7f, 0xde, 0x28, 0x3e } };
const UINTN genericBufferSize = 0x200;

const UINT8 DevicePathHeaderUint[] = { 0x01, 0x04, 0x14, 0x00 };
const UINT8 ifrSetupStringsPointerUint[] = { 0x04, 0x34, 0x00, 0x00, 0x00, 0x34 };
const UINT8 ifrHiiBinPointerHeaderUint[] = { 0x02, 0x0E, 0xA7 };

EFI_FILE *LogFile = NULL;
CHAR16 Log[0x200];
extern VOID LogToFile(IN EFI_FILE *LogFile, IN CHAR16 *String);

//Copy from HandleParsingLib
CHAR16 *FindLoadedImageFileNameSREP(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage
);

//Copy from HandleParsingLib
CHAR16 *LoadedImageProtocolDumpFilePath(
  IN CONST EFI_HANDLE TheHandle
);

VOID SetFilterProtocolForLoadedImageFunction(
  IN OUT EFI_GUID *FilterProtocol
);

EFI_STATUS LocateAndLoadFvFromName(
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LocateAndLoadFvFromGuid(
  IN EFI_GUID GUID16,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadandRunImage(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN CHAR16 *FileName,
  OUT EFI_HANDLE *AppImageHandle
);

EFI_STATUS RegexMatch(
  IN UINT8 *DUMP,
  IN CHAR8 *Pattern,
  IN UINT16 Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
);

EFI_HII_PACKAGE_LIST_HEADER *GetHandlePackageList(
  IN CONST EFI_HII_HANDLE ImageHandle
);

EFI_HII_HANDLE HiiConstructAddPackagesSREP(
  IN CONST EFI_GUID *PackageListGuid,
  IN EFI_HANDLE DeviceHandle OPTIONAL,
  IN UINT8 *UniBin,
  IN UINT8 *VfrBin
);

EFI_STATUS GetSetupPointers(
  IN EFI_LOADED_IMAGE_PROTOCOL *PImageInfo,
  OUT UINTN **DevicePathPointer,
  OUT UINTN **ifrSetupStringsPointer,
  OUT UINTN **ifrHiiBinPointer
);

EFI_STRING HiiGetStringSREP(
  IN EFI_HII_HANDLE HiiHandle,
  IN EFI_STRING_ID StringId,
  IN CONST CHAR8 *Language  OPTIONAL
);

//Copy from DisplayUpdateProgressLib
EFI_STATUS DisplayUpdateProgressSREP(
  IN UINTN Completion
);

//Unused
UINT8 *FindBaseAddressFromName(
  IN CONST CHAR16 *Name,
  IN EFI_GUID FilterProtocol
);

//Unused
UINTN FindFvImageBufferSizeFromLoadedImage(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);
