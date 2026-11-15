#ifndef _UTILITY_H_
#define _UTILITY_H_

#include "SmokelessRuntimeEFIPatcher.h"

//Copy from HandleParsingLib
CHAR16 *FindLoadedImageBuffer(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN UINT8 SectionType,
  IN UINTN SectionInstance,
  OUT UINTN *Size
);

EFI_GUID *EfiHandleDumpFilePath(
  IN CONST EFI_HANDLE TheHandle
);

EFI_STATUS LocateAndLoadFromFV(
  IN EFI_GUID *FileGuid OPTIONAL,
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize
);

EFI_STATUS RegexMatch(
  IN UINT8 *DUMP,
  IN CHAR8 *Pattern,
  IN UINTN Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
);

EFI_STATUS GetFormSetPointerByGuid(
  IN EFI_LOADED_IMAGE_PROTOCOL *PImageInfo,
  IN EFI_GUID FormSetGuid,
  OUT UINT8 **FormSetStart
);

EFI_STATUS GetLoadedImagesFromHiiHandles(
  IN EFI_HII_HANDLE *HiiHandles,
  IN UINTN HandlesCount,
  OUT EFI_LOADED_IMAGE_PROTOCOL ***ImageInfo
);

EFI_HII_PACKAGE_LIST_HEADER *GetHandlePackageListAndGuid(
  IN CONST EFI_HII_HANDLE HiiHandle,
  OUT EFI_GUID *FileGuid OPTIONAL
);

UINT8 *PackageBufferOffsetByFormSetGuid(
  IN EFI_GUID *FormSetGuid,
  IN UINT8 *Package,
  IN UINTN PackageListLength
);

EFI_STATUS GetHiiPackagesByTypeAndGuid(
  IN UINT8 PackageType,
  IN EFI_GUID *PackageGuid,
  OUT UINTN *BufferSize,
  OUT EFI_HII_HANDLE **HiiHandles
);

EFI_STRING HiiGetStringSREP(
  IN EFI_STRING_ID StringId,
  IN CONST CHAR8 *Language OPTIONAL
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
VOID LogToFile(
  IN EFI_FILE *LogFile,
  IN CHAR16 *String
);

EFI_STATUS
FindResourceSection(
  IN EFI_LOADED_IMAGE_PROTOCOL *ImageInfo,
  OUT UINT8 **Offset
);

EFI_STATUS GetExt1ImageBufferFromSREP(
  IN UINT32 PackageType,
  IN UINT16 PackageId,
  OUT UINT8 **PackageBuffer,
  OUT UINT32 *PackageSize
);

EFI_STATUS LoadAndRunFileFromBuffer(
  IN CHAR16 *FileName,
  IN UINT8 *Buffer,
  IN UINTN BufferSize
);

EFI_STATUS ExecImageEmbeddedInSREP(
  IN CHAR16 *ReserveFileName,
  IN EFI_IMAGE_ID ImageId
);

BOOLEAN AsciiIsHexaDecimalDigitCharacter(
  IN CHAR8 Char
);

BOOLEAN AsciiIsHexString(
  IN CHAR8 *String
);

BOOLEAN IsValueInSentinelList(
  IN UINT32 Target,
  IN UINT32 First,
  ...
);

#endif
