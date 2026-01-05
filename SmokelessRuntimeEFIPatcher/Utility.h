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

EFI_GUID gHIIRussianFontGuid;
EFI_GUID gEfiSREPLangVariableGuid = { 0x257ea3f4, 0xc447, 0x4b22, { 0xbb, 0x1c, 0x37, 0x9b, 0x7f, 0xde, 0x28, 0x3e } };
EFI_HII_HANDLE HiiHandle;
const UINTN genericBufferSize = 0x200;

//Initizalize log function
VOID LogToFile(
  IN EFI_FILE *LogFile,
  IN CHAR16 *String
);

//Copy from HandleParsingLib
CHAR16 *FindLoadedImageFileNameSREP(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);

UINTN FindLoadedImageBufferSize(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadandRunImage(
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN CHAR16 *FileName,
  OUT EFI_HANDLE *AppImageHandle
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

EFI_STATUS RegexMatch(
  IN UINT8 *DUMP,
  IN CHAR8 *Pattern,
  IN UINT16 Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
);

//Copy from HandleParsingLib
EFI_STRING HiiGetStringSREP(
  IN EFI_HII_HANDLE  HiiHandle,
  IN EFI_STRING_ID   StringId,
  IN CONST CHAR8 *Language  OPTIONAL
);

CHAR16 *
LoadedImageProtocolDumpFilePath(
  IN CONST EFI_HANDLE  TheHandle
);

UINT8 *FindBaseAddressFromName(
  IN const CHAR16 *Name,
  IN EFI_GUID FilterProtocol
);
