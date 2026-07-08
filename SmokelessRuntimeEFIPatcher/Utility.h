#pragma once
#include "SmokelessRuntimeEFIPatcher.h"

//Copy from HandleParsingLib
CHAR16 *FindLoadedImageBuffer(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN UINT8 SectionType,
  IN UINTN SectionInstance,
  OUT UINTN *Size
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
  IN CONST EFI_HII_HANDLE HiiHandle
);

EFI_STATUS
GetHiiPackagesByTypeAndGuid(
  IN UINT8 PackageType,
  IN EFI_GUID *PackageGuid,
  OUT UINTN *BufferSize,
  OUT EFI_HII_HANDLE **HiiHandles
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
  IN EFI_STRING_ID StringId,
  IN CONST CHAR8 *Language  OPTIONAL
);

//Copy from DisplayUpdateProgressLib
EFI_STATUS DisplayUpdateProgressSREP(
  IN UINTN Completion
);

CHAR8 *ApplySimpleMask(
  IN UINT8 *SourceBuffer, //Accepts UINT8 byte buffer only
  IN CHAR8 *MaskBuffer,
  IN UINTN MaskLengthBytes,
  IN CHAR8 Metacharacter
);

VOID PrintDump(
  IN UINT64 Size,
  IN UINT8* Dump,
  IN BOOLEAN isUnformattedAscii
);

//Writes string from the buffer to SREP.log
VOID
LogToFile(
  IN EFI_FILE *LogFile,
  IN CHAR16 *String
);

EFI_STATUS
GetExt1ImageBufferFromSREP(
  IN UINT32 PackageType,
  IN UINT16 PackageId,
  OUT UINT8 **PackageBuffer,
  OUT UINT32 **PackageSize
);

EFI_STATUS
ExecImageEmbeddedInSREP(
  IN EFI_IMAGE_ID ImageId
);

BOOLEAN
AsciiIsHexaDecimalDigitCharacter(
  IN CHAR8 Char
);

BOOLEAN
AsciiIsHexString(
  IN CHAR8 *String
);

BOOLEAN
EFIAPI
IsValueInSentinelList(
  IN UINT32 Target,
  IN UINT32 First,
  ...
);

//Unused
UINT8 *FindBaseAddressFromName(
  IN CONST CHAR16 *Name,
  IN EFI_GUID FilterProtocol
);

//Unused, unfinished
UINTN FindFvImageBufferSizeFromLoadedImage(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
);
