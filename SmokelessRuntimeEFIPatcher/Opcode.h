#pragma once
#include "Utility.h"

EFI_STATUS FindLoadedImageFromName(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS FindLoadedImageFromGUID(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS FindLoadedImageFromShellIndex(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *HandleIndex,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE **HandleBuffer
);

EFI_STATUS LoadFromFS(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle
);

EFI_STATUS LoadFromFV(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadGUIDandSavePE(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS LoadGUIDandSaveFreeform(
  IN EFI_HANDLE ImageHandle,
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuid,
  IN CHAR8 *SectionGuid OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
);

EFI_STATUS Exec(
  IN EFI_HANDLE *AppImageHandle
);

EFI_STATUS UninstallProtocol(
  IN CHAR8 *ProtocolGuid,
  OUT UINTN Indexes
);

//Unused
UINTN GetAptioHiiDB(
  IN BOOLEAN BufferSizeOrPointer
);
