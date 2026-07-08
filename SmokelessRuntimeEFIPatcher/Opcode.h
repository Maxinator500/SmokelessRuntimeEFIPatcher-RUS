#pragma once
#include "Utility.h"

enum SEARCH_TYPE
{
  OFFSET = 1,
  PATTERN,
  REL_NEG_OFFSET,
  REL_POS_OFFSET
};

enum PATCH_TYPE
{
  REGEX,
  FAST,
  FASTEST
};

EFI_STATUS FindLoadedImageFromName(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS FindLoadedImageFromGUID(
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS FindLoadedImageFromShellIndex(
  IN UINTN HandleIndex,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE **HandleBuffer,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadFromFS(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle
);

EFI_STATUS LoadFromFV(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadGUIDandSavePE(
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadGUIDandSaveFreeform(
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuidStr,
  IN CHAR8 *SectionGuidStr OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS Exec(
  IN EFI_HANDLE *AppImageHandle
);

EFI_STATUS SendFirstForm(
  IN EFI_HII_HANDLE HiiHandle
);

EFI_STATUS UninstallProtocol(
  IN CHAR8 *ProtocolGuid,
  OUT UINTN IndicesCount
);

EFI_STATUS UpdateHandlePackageList(
  IN CHAR8 *PackageGuid,
  IN CHAR8 *PackageSizeOverride OPTIONAL,
  OUT EFI_LOADED_IMAGE_PROTOCOL *ImageInfo
);

BOOLEAN DoesFvFileExist(
  IN EFI_GUID GUID16
);

EFI_STATUS AddFirstHiiPackageFromFile(
  IN EFI_LOADED_IMAGE_PROTOCOL *ImageInfo,
  OUT EFI_HII_HANDLE *HiiHandleP
);

EFI_STATUS
FindAndReplace(
  IN EFI_LOADED_IMAGE_PROTOCOL *ImageInfo,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *RegularExpressionProtocol,

  IN UINT64 SearchType,
  IN UINT64 PatchType,
  IN INT64 PatchLimit,

  IN UINT64 SimplePatternBuffer,
  IN UINT64 SimplePatchBuffer,
  IN CHAR8 *RegexPatternBuffer,
  IN CHAR8 *RegexPatchBuffer,

  IN UINT64 PatternLength,
  IN UINT64 PatchLength,

  IN INT64 CurrentOffset,        //Must be initialized with -1 in SREP main
  IN OUT INT64 *BaseOffset,

  OUT BOOLEAN *isOpSkipAllowed,
  OUT EFI_STATUS *Status
);
