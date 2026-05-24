#include "Opcode.h"

EFI_STATUS
FindLoadedImageFromName(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINTN BufferSize = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    SetFilterProtocolForLoadedImageFunction(&FilterProtocol);

    CHAR16 FileName16[0x100] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    //Catch not enough memory here, allocate buffer pool
    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, NULL);
    Handles = AllocateZeroPool(BufferSize);

    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, Handles);
    DEBUG_CODE(Print(L"Found %d Instances\n\r", BufferSize / sizeof(EFI_HANDLE)););

    for (UINTN i = 0; i < BufferSize / sizeof(EFI_HANDLE); i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileNameSREP(*ImageInfo);

            if (String != NULL)
            {
                if (StrCmp(FileName16, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    DEBUG_CODE(
                      Print(L"Found %s at Address 0x%X\n\r", String, (*ImageInfo)->ImageBase);
                    );
                    return EFI_SUCCESS;
                }
            }
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
FindLoadedImageFromGUID(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_GUID GUID = { 0 }, FoundGUID = { 0 };
    EFI_HANDLE *Handles = NULL;
    UINTN BufferSize = 0;
    //-----------------------------------------------------

    SetFilterProtocolForLoadedImageFunction(&FilterProtocol);

    Status = AsciiStrToGuid(FileGuid, &GUID);
    if (EFI_ERROR(Status))
    {
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      UnicodeSPrint(Log, genericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      LogToFile(LogFile, Log);

      return Status;
    }

    //Catch not enough memory here, allocate buffer pool
    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, NULL);
    Handles = AllocateZeroPool(BufferSize);

    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, Handles);

    DEBUG_CODE(
      Print(L"Found %d Instances\n\r", BufferSize / sizeof(EFI_HANDLE));
    );

    for (UINTN i = 0; i < BufferSize / sizeof(EFI_HANDLE); i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle

        if ((Status == EFI_SUCCESS) && (ImageInfo[0]->ImageSize != 0))
        {
            StrToGuid(LoadedImageProtocolDumpFilePath(Handles[i]), &FoundGUID);

            if (CompareGuid(&FoundGUID, &GUID) == 1)
            {
              if (Section_Type == EFI_SECTION_TE) {
                Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_TE_HINT), NULL), ImageInfo[0]->ImageSize);
              }

              Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_FOUND_LOADED_GUID), NULL), ImageInfo[0]->ImageSize);
              Status = EFI_SUCCESS;

              break;
            }
        }
    }

    FreePool(Handles);
    return Status;
}

EFI_STATUS
FindLoadedImageFromShellIndex(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *HandleIndex,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE **HandleBuffer,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINTN NumericIndex;
    EFI_HANDLE TheHandle;
    //-----------------------------------------------------
    
    SetFilterProtocolForLoadedImageFunction(&FilterProtocol);

    Status = AsciiStrHexToUintnS(HandleIndex, (CHAR8 **)L"\0", &NumericIndex);
    if (EFI_ERROR(Status))
    {
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), HandleIndex);
      return Status;
    }
    Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HANDLE_INDEX), NULL), NumericIndex);

    EFI_HANDLE *AllHandles = GetHandleListByProtocol(NULL);
    if (AllHandles == NULL)
    {
      Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_HANDLE_LIST_FAIL), NULL));
      return EFI_NOT_FOUND;
    }

    //Save handles to buffer
    *HandleBuffer = AllHandles;

    TheHandle = ConvertHandleIndexToHandle(NumericIndex);
    if (TheHandle != NULL)
    {
      Print(L"\n\r%X: 0x%X\n\r", NumericIndex, TheHandle);
      {
        return gBS->HandleProtocol(TheHandle, &FilterProtocol, (VOID **)ImageInfo);
      }
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS
LoadFromFS(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle
)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles = NULL;
    EFI_BLOCK_IO_PROTOCOL *BlkIo = NULL;
    EFI_DEVICE_PATH_PROTOCOL *FilePath = NULL;
    UINTN HandleBufferSize = 0;
    UINTN Index = 0;
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleBufferSize, &Handles);

    if (EFI_ERROR(Status))
    {
        Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_NO_HANDLE_WITH_EFISIMPLEFS), NULL));
        return Status;
    }
    DEBUG_CODE(Print(L"Found %d Instances\n\r", HandleBufferSize););

    for (Index = 0; Index < HandleBufferSize; Index++)
    {
        Status = gBS->OpenProtocol(
            Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&BlkIo,
            ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(Status))
        {
            Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_UNSUPPORTED_PROTOCOL), NULL), Status);
            return Status;
        }
        CHAR16 FileName16[0x100] = {0};
        UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
        FilePath = FileDevicePath(Handles[Index], FileName16);
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (EFI_ERROR(Status))
        {
            Print(L"%s%d: %r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_COULD_NOT_LOAD), NULL), Index, Status);
            continue;
        }
        else
        {
            Print(L"%s%d\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_SUCCESSFUL_LOAD), NULL), Index);
            Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                       (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            return Status;
        }
    }
    return Status;
}

EFI_STATUS
LoadFromFV(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status = EFI_SUCCESS;

    CHAR16 FileName16[0x100] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromName(FileName16, Section_Type, &Buffer, &BufferSize, FilterProtocol);

    Status = gBS->LoadImage(FALSE, ImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle);

    if (Buffer != NULL) FreePool(Buffer);

    if (!EFI_ERROR(Status))
    {
        Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                   (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

    return Status;
};

EFI_STATUS
LoadGUIDandSavePE(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE_PROTOCOL HandleProtocol;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;;
  EFI_FILE *Root;
  EFI_FILE *DumpFile = NULL;
  //-----------------------------------------------------

  HandleProtocol = SystemTable->BootServices->HandleProtocol;
  HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);

  EFI_GUID GUID = { 0 };
  Status = AsciiStrToGuid(FileGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    UnicodeSPrint(Log, genericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  DEBUG_CODE(
    // Debug
    Print(L"GUID from OpCode.c raw: %s\n\r", FileGuid);
    Print(L"GUID from OpCode.c converted: %g\n\r", GUID);
  );

  UINT8 *Buffer = NULL;
  UINTN BufferSize = 0;
  Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize, FilterProtocol); //Pass the converted guid to Utility

  //Dumping section
  if (Buffer == NULL) {
    Print(L"%s%d\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_NULL_DRIVER_BUFFER), NULL), BufferSize);

    return EFI_NOT_FOUND;
  }

  CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
  UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
  Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
  DumpFile->Write(DumpFile, &BufferSize, (VOID **)Buffer);
  DumpFile->Flush(DumpFile);

  Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_DUMPING), NULL));

  Status = gBS->LoadImage(FALSE, ImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle); //Parse saved buffer to LoadImage BS
  if (Buffer != NULL)
  {
    FreePool(Buffer);
  }
  if (!EFI_ERROR(Status))
  {
    Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                (VOID**)ImageInfo, ImageHandle, (VOID*)NULL,
                                EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  }
  Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

  return Status;
};

EFI_STATUS
LoadGUIDandSaveFreeform(
  IN EFI_HANDLE ImageHandle,
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuid,
  IN CHAR8 *SectionGuid OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_GUID File = { 0 };
    EFI_GUID Section = { 0 };
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN BufferSize;
    UINT32 Authentication;
    UINTN i,j;
    VOID *SectionData = NULL;
    UINTN DataSize;
    EFI_SECTION_TYPE SectionType;
    EFI_FIRMWARE_VOLUME_PROTOCOL *Fv = NULL;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_DEFAULT_FILTER), NULL));
    }

    //Dump related vars
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_FILE *Root;
    EFI_FILE *DumpFile = NULL;
    //-----------------------------------------------------

    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Status = AsciiStrToGuid(FileGuid, &File);
    if (EFI_ERROR(Status))
    {
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      UnicodeSPrint(Log, genericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      LogToFile(LogFile, Log);

      return Status;
    }

    if (SectionGuid != NULL)
    {
      Status = AsciiStrToGuid(SectionGuid, &Section);
      if (EFI_ERROR(Status))
      {
        Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
        UnicodeSPrint(Log, genericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
        LogToFile(LogFile, Log);

        return Status;
      }
    }

    // Locate the Firmware volume protocol
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolume2ProtocolGuid,
        NULL,
        &BufferSize,
        &HandleBuffer
      );
    }
    else
    {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolumeProtocolGuid,
        NULL,
        &BufferSize,
        &HandleBuffer
      );
    }

    if (EFI_ERROR(Status))
    {
      Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);

      return Status;
    }

    DEBUG_CODE(Print(L"Found %d Instances\n\r", BufferSize););

    SectionType = (SectionGuid == NULL) ? EFI_SECTION_RAW : EFI_SECTION_FREEFORM_SUBTYPE_GUID;

    //Find and read raw data files.
    for (i = 0; i < BufferSize; i++) {
      if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
        Status = gBS->HandleProtocol(HandleBuffer[i], &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv2);
      }
      else
      {
        Status = gBS->HandleProtocol(HandleBuffer[i], &gEfiFirmwareVolumeProtocolGuid, (VOID **)&Fv);
      }
        if (EFI_ERROR(Status)) { continue; } //[i]

        for(j = 0; ; j++){
            SectionData = NULL;

            //Read Section From FFS file
            if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
              Status = Fv2->ReadSection(
                Fv2,
                &File,
                SectionType,
                j,
                &SectionData,
                &DataSize,
                &Authentication
              );
            /*
            * At least get PE section if
            * ReadSection failed
            * and
            * SectionGuid was not specified by caller
            */
              if (EFI_ERROR(Status) && SectionGuid == NULL) {
                Status = Fv2->ReadSection(
                  Fv2,
                  &File,
                  EFI_SECTION_PE32,
                  j,
                  &SectionData,
                  &DataSize,
                  &Authentication
                );
              }
            }
            else
            {
              Status = Fv->ReadSection(
                Fv,
                &File,
                SectionType,
                j,
                &SectionData,
                &DataSize,
                &Authentication
              );

              if (EFI_ERROR(Status) && SectionGuid == NULL) {
                Status = Fv->ReadSection(
                  Fv,
                  &File,
                  EFI_SECTION_PE32,
                  j,
                  &SectionData,
                  &DataSize,
                  &Authentication
                );
              }
            }

            //No file found advance to the next FV...
            if (EFI_ERROR(Status)) { break; } //[j]

            //RAW sections don't have guid so we don't need doing file search for them
            if (SectionType == EFI_SECTION_FREEFORM_SUBTYPE_GUID) {
              Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_RETRIEVE_FREEFORM), NULL));
              EFI_GUID *secguid;

              //Compare Section GUID, to find correct one..
              secguid = (EFI_GUID *)SectionData; //We can extract Guid from SectionData

              if (CompareGuid(secguid, &Section) != TRUE) {
                //Free section read, it was not the one we need...
                if (SectionData != NULL) { FreePool(SectionData); }
                continue; //[j]
              }

              //Update
              DataSize -= sizeof(EFI_GUID);
              // ReadSection returns pointer to a section GUID, which caller of this function does not want to see.
              // We could've just returned (EFI_GUID *)AmiData + 1 to the caller,
              // but this pointer can't be used to free the pool (can't be passed to FreePool).
              // Copy section data into a new buffer
              SectionData = AllocatePool(DataSize);
              if (SectionData == NULL) { Status = EFI_OUT_OF_RESOURCES; }
              else { CopyMem(SectionData, secguid + 1, DataSize); }

              FreePool(secguid);
            }

            if (HandleBuffer != NULL) {
              FreePool(HandleBuffer);
            }
            if (!EFI_ERROR(Status)){
                Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_DUMPING), NULL));
                *Pointer = SectionData;
                *Size = DataSize;

                CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
                UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
                Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
                DumpFile->Write(DumpFile, Size, (UINT8 *)SectionData); //(UINT8 *)datum is correct
                DumpFile->Flush(DumpFile);
            }
            return Status;
        }//for [j]
    }//for [i]

    FreePool(HandleBuffer);
    return EFI_NOT_FOUND;
}

EFI_STATUS
Exec(
  IN EFI_HANDLE *AppImageHandle
)
{
    UINTN ExitDataSize;
    EFI_STATUS Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);

    return Status;
}

EFI_STATUS
SendFirstForm(
  IN EFI_HII_HANDLE HiiHandleFromFS
)
{
  EFI_STATUS Status;
  EFI_FORM_BROWSER2_PROTOCOL *FormBrowser2 = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  EFI_BROWSER_ACTION_REQUEST ActionRequest = EFI_BROWSER_ACTION_REQUEST_NONE;
  //-----------------------------------------------------

  Status = gBS->LocateProtocol(&gEfiFormBrowser2ProtocolGuid, NULL, (VOID **)&FormBrowser2);
  if (EFI_ERROR(Status))
  {
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_LOCATE_FB_FAIL), NULL), Status);
    UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_LOCATE_FB_FAIL), NULL), Status);
    LogToFile(LogFile, Log);

    return Status;
  }

  PackageList = GetHandlePackageList(HiiHandleFromFS);

  if((PackageList == NULL) || IsZeroGuid(&PackageList->PackageListGuid))
  {
    Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
    UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
    LogToFile(LogFile, Log);

    return EFI_UNSUPPORTED;
  }

  Status = FormBrowser2->SendForm(
                            FormBrowser2,
                            &HiiHandleFromFS,
                            1,
                            &PackageList->PackageListGuid,
                            0,
                            NULL,
                            &ActionRequest
                            );

  if (ActionRequest == EFI_BROWSER_ACTION_REQUEST_RESET) {
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_ACTION_REQUEST), NULL), Status);
    UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_ACTION_REQUEST), NULL), Status);
    LogToFile(LogFile, Log);

    gBS->Stall(3000000);
    gBS->RaiseTPL(TPL_NOTIFY);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
  }

  Print(L"%r%s%g%s%X%s\n\r",
        Status,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED1), NULL),
        PackageList->PackageListGuid,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED2), NULL),
        HiiHandleFromFS,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED3), NULL)
  );
  UnicodeSPrint(Log, genericBufferSize, u"%r%s%g%s%X%s\n\r",
        Status,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED1), NULL),
        PackageList->PackageListGuid,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED2), NULL),
        HiiHandleFromFS,
        HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SEND_FORM_FAILED3), NULL)
  );
  LogToFile(LogFile, Log);

  return Status;
}

EFI_STATUS
UninstallProtocol(
  IN CHAR8 *ProtocolGuid,
  OUT UINTN IndicesCount
)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandlesBuffer;
  UINTN BufferSize = 0;
  VOID *ProtocolInterface;
  EFI_GUID GUID = { 0 };
  //-----------------------------------------------------

  Status = AsciiStrToGuid(ProtocolGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ProtocolGuid);

    return Status;
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, &GUID, NULL, &BufferSize, &HandlesBuffer);
  if (EFI_ERROR(Status))
  {
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
    UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
    LogToFile(LogFile, Log);

    return Status;
  } 
  
  for (UINTN i = 0; i < BufferSize; i += 1)
  {
    ProtocolInterface = NULL;
    Status = gBS->HandleProtocol(HandlesBuffer[i],
                                 &GUID,
                                 (VOID **)&ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      Print(L"%s%d: %r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      UnicodeSPrint(Log, genericBufferSize, u"%s%d: %r\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      LogToFile(LogFile, Log);

      continue;
    }

    Status = gBS->UninstallProtocolInterface(HandlesBuffer[i],
                                             &GUID, 
                                             ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      Print(L"%r - %s%g,\n    Handle %d\n    Pointer 0x%x\n\r", Status, HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      UnicodeSPrint(Log, genericBufferSize, u"%r - %s%g,\n    Handle %d\n    Pointer 0x%x\n\r", Status, HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      LogToFile(LogFile, Log);

      continue;
    }
    IndicesCount += 1;
  }

  return Status;
}

EFI_STATUS
UpdateHandlePackageList(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *PackageGuid,
  IN CHAR8 *PackageSizeOverride OPTIONAL,
  OUT EFI_LOADED_IMAGE_PROTOCOL *ImageInfo
)
{
  #define MAX_UINT28 ((UINT32)0xFFFFFFF)
  EFI_STATUS Status;
  EFI_HANDLE *DriverHandleBuffer = NULL;
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN DriverHandleBufferSize = 0;
  UINTN HiiHandleBufferSize = 0;
  EFI_GUID GUID = { 0 };
  //-----------------------------------------------------

  Status = AsciiStrToGuid(PackageGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), PackageGuid);

    return Status;
  }

  //Catch not enough memory here, allocate buffer pool
  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleBufferSize, NULL);
  DriverHandleBuffer = AllocateZeroPool(DriverHandleBufferSize);

  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleBufferSize, DriverHandleBuffer);
  DEBUG_CODE(
    //Debug
    Print(L"Locate driver handles %X status: %r\n\r", DriverHandleBufferSize / sizeof(EFI_HANDLE), Status);
  );


  //
  //Locate HII database, retrieve all HII packages to HiiHandleBuffer
  //
  Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  DEBUG_CODE(
    //Debug
    Print(L"Locate HiiDb status %r\n\r", Status);
  );

  Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &HiiHandleBufferSize, NULL);

  DEBUG_CODE(
    //Debug
    Print(L"HiiHandle instances: %X\n\r", HiiHandleBufferSize / sizeof(EFI_HII_HANDLE));
  );

  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL)
  {
    Print(L"Failed to list HII handles!\n\r");

    return Status;
  }
  else
  {
    //Catch not enough memory here, allocate buffer pool
    HiiHandleBuffer = AllocateZeroPool(HiiHandleBufferSize);
    if (HiiHandleBuffer == NULL) {

      return EFI_OUT_OF_RESOURCES;
    }

    //Try again after allocation
    Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &HiiHandleBufferSize, HiiHandleBuffer);
  }

  //
  //Try retrieving package list in the HII database by guid
  //
  for (UINTN i = 0; i < HiiHandleBufferSize / sizeof(EFI_HII_HANDLE); i++) {
    PackageList = GetHandlePackageList(HiiHandleBuffer[i]);
    DEBUG_CODE(
      //Debug
      Print(L"Found PackageList: %g of size 0x%X at 0x%X\n\r", PackageList->PackageListGuid, PackageList->PackageLength, PackageList);
    );

    if (CompareGuid(&GUID, &PackageList->PackageListGuid)) {
      //
      // Initialize a pointer to the beginning if the Package List data
      //
      ImageInfo->ImageBase = PackageList + 1; //sizeof(EFI_HII_PACKAGE_LIST_HEADER)
      ImageInfo->ImageSize = PackageList->PackageLength;

      break;
    }
  }

  if (DriverHandleBuffer != NULL) {
    FreePool(DriverHandleBuffer);
    FreePool(HiiHandleBuffer);
  }

  if (ImageInfo->ImageSize == 0) {
    Status = EFI_NOT_FOUND;
  }
  else
  {
    if (PackageSizeOverride != 0) {
      UINTN PackageSizeUintn = 0;
      AsciiStrHexToUintnS(PackageSizeOverride, NULL, &PackageSizeUintn);
      PackageSizeUintn = PackageSizeUintn > MAX_UINT28 ? MAX_UINT28 : PackageSizeUintn;

      ImageInfo->ImageSize = PackageSizeUintn;
    }
  }

  return Status;
}

BOOLEAN
DoesFvFileExist(
  IN EFI_GUID GUID16
)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer;
  UINTN BufferSize;
  UINT8 Index = 0;
  UINT8 j = 0;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
  VOID *String;
  //-----------------------------------------------------

  //
  // Locate protocol.
  //
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &gEfiFirmwareVolume2ProtocolGuid,
    NULL,
    &BufferSize,
    &HandleBuffer);
  if (EFI_ERROR(Status))
  {
    return FALSE;
  }

  DEBUG_CODE(
    AsciiPrint("\nFound %d Instances\n\r", BufferSize);
  );

  for (Index = 0; Index < BufferSize; Index++)
  {

    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      HandleBuffer[Index],
      &gEfiFirmwareVolume2ProtocolGuid,
      (VOID**)&FvInstance);
    ASSERT_EFI_ERROR(Status);

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize;
    EFI_GUID NameGuid = { 0 };
    VOID *Keys = AllocateZeroPool(FvInstance->KeySize);

    while (j < BufferSize)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      ZeroMem(&NameGuid, sizeof(EFI_GUID));
      Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
      if (EFI_ERROR(Status))
      {
        //AsciiPrint("\nBreaking cause: %r\n\r", Status);

        j++;
        break;
      }
      
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);

      DEBUG_CODE(
        // Debug
        AsciiPrint("\nCurrent GUID: %g\n\r", NameGuid); //Current processing guid per While iteration
      );

      if (CompareGuid(&GUID16, &NameGuid) == 1)
      {
        DEBUG_CODE(
          // Debug
          AsciiPrint("\nFound Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType);
        );

        FreePool(String);
        return TRUE;
      }
    }
  }
  FreePool(String);
  return FALSE;
}

EFI_STATUS
AddFirstHiiPackageFromFile(
  IN EFI_HANDLE This,
  IN EFI_LOADED_IMAGE_PROTOCOL *ImageInfo,
  OUT EFI_HII_HANDLE *HiiHandleP
)
{

  ///
  /// HII specific Vendor Device Path definition.
  ///
  ///
  /*
  typedef struct {
    VENDOR_DEVICE_PATH                VendorDevicePath;
    EFI_DEVICE_PATH_PROTOCOL          End;
  } HII_VENDOR_DEVICE_PATH;


  EFI_GUID PackageListGuid = {
    0x12345678, 0xabcd, 0xef01, {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x00}
  };

  HII_VENDOR_DEVICE_PATH mHiiVendorDevicePath0 = {
    {
      { //DevicePathHeaderUint (see utility.h)
        HARDWARE_DEVICE_PATH,
        HW_VENDOR_DP,
        {
          (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
          (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
        }
      },//DevicePathHeaderUint end
      PackageListGuid
    },
    {
      END_DEVICE_PATH_TYPE,
      END_ENTIRE_DEVICE_PATH_SUBTYPE,
      {
        (UINT8)(END_DEVICE_PATH_LENGTH),
        (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
      }
    }
  };
  */

  EFI_STATUS Status;
  EFI_GUID PackageListGuid = { 0 };
  EFI_GUID AptioPackageListGuid = APTIO_SETUP_UTILITY_PACKAGE_LIST_GUID;
  EFI_GUID InsydePackageListGuid = INSYDE_SETUP_UTILITY_PACKAGE_LIST_GUID;
  EFI_HII_HANDLE HiiHandle = NULL;
  //-----------------------------------------------------

  //
  //Locate HII database, retrieve all HII packages to HiiHandleBuffer
  //
  Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    DEBUG_CODE(
      //Debug
      Print(L"Unable to locate HII Database!\n\r");
    );
    return EFI_UNSUPPORTED;
  }

  //
  // Get Pointers part
  //
  UINTN *DevicePathPointer = NULL;
  UINTN *ifrSetupStringsPointer = NULL;
  UINTN *ifrHiiBinPointer = NULL;

  if (ImageInfo != NULL) {
    Status = GetSetupPointers(
      ImageInfo,
      &DevicePathPointer,
      &ifrSetupStringsPointer,
      &ifrHiiBinPointer
    );
  }
  else
  {
    Status = EFI_NOT_STARTED;
  }

  if ((Status != EFI_NOT_STARTED) && DevicePathPointer) {
    DEBUG_CODE(
      //Debug
      Print(L"Pointers %X, %X, %X\n\r", DevicePathPointer, ifrSetupStringsPointer, ifrHiiBinPointer);
    );

    //
    // Get PackageListGuid from image
    //
    gBS->CopyMem(&PackageListGuid.Data1, (UINT8 *)DevicePathPointer, sizeof(UINT32));
    gBS->CopyMem(&PackageListGuid.Data2, (UINT8 *)DevicePathPointer + sizeof(UINT32), sizeof(UINT16));
    gBS->CopyMem(&PackageListGuid.Data3, (UINT8 *)DevicePathPointer + sizeof(UINT32) + sizeof(UINT16), sizeof(UINT16));
    gBS->CopyMem(&PackageListGuid.Data4, (UINT8 *)DevicePathPointer + sizeof(UINT32) + sizeof(UINT16) + sizeof(UINT16), sizeof(UINT64));

    //PackageListGuid.Data1 = SwapBytes32(PackageListGuid.Data1);
    //PackageListGuid.Data2 = SwapBytes16(PackageListGuid.Data2);
    //PackageListGuid.Data3 = SwapBytes16(PackageListGuid.Data3);
  }
  else if (ifrSetupStringsPointer && ifrHiiBinPointer)
  {
    //
    // DevicePath doesn't exist, generate one
    //
    if (DoesFvFileExist(AptioPackageListGuid)) {
      PackageListGuid = AptioPackageListGuid;
    }
    else if (DoesFvFileExist(InsydePackageListGuid)) {
      PackageListGuid = InsydePackageListGuid;
    }
    else
    {
      //Failed to determine UEFI vendor
      Print(HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_UNK_UEFI_VENDOR), NULL));
      UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_UNK_UEFI_VENDOR), NULL));
      LogToFile(LogFile, Log);

      return EFI_UNSUPPORTED;
    }

    Print(L"%s%X -> %g\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_DEV_PATH_NOT_FOUND), NULL), DevicePathPointer, &PackageListGuid);
    UnicodeSPrint(Log, genericBufferSize, u"%s%X -> %g\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_DEV_PATH_NOT_FOUND), NULL), DevicePathPointer, &PackageListGuid);
    LogToFile(LogFile, Log);
  }
  else
  {
    //
    // GetSetupPointers failed, return status
    //
    Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SETUP_POINTERS_NOT_FOUND), NULL), ImageInfo->ImageBase);
    UnicodeSPrint(Log, genericBufferSize, u"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_SETUP_POINTERS_NOT_FOUND), NULL), ImageInfo->ImageBase);
    LogToFile(LogFile, Log);

    return Status;
  }

  //
  // Publish resources part
  //
  //if DeviceHandle = NULL, handle will be assigned.
  // 
  //mHiiVendorDevicePath0 must be a pointer to dev. path in EFI setup binary. Dev. path is DevicePathHeader + Guid

  DEBUG_CODE(
    //Debug
    Print(L"Printing 4 bytes for Vfr 0x%X and Uni 0x%X...\n", *ifrHiiBinPointer, *ifrSetupStringsPointer);
  );

  // HiiAddPackages here makes PC hang when entering bios setup
  HiiHandle = HiiConstructAddPackagesSREP(
    &PackageListGuid,
    ImageInfo->DeviceHandle,
    (UINT8 *)ifrHiiBinPointer,
    (UINT8 *)ifrSetupStringsPointer
  );
  DEBUG_CODE(
    //Debug
    Print(L"HiiAddPackages done. Handle - %X\n", HiiHandle);
  );


  //
  // HiiHandle checks part
  //
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN BufferSize = 0;

  Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, NULL);

  DEBUG_CODE(
    //Debug
    Print(L"HiiHandle instances: %X\n\r", BufferSize / sizeof(EFI_HII_HANDLE));
  );


  //Catch not enough memory here, allocate buffer pool
  HiiHandleBuffer = AllocateZeroPool(BufferSize);
  if (HiiHandleBuffer != NULL) {
    Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, HiiHandleBuffer);
  }
  else
  {
    Status = EFI_OUT_OF_RESOURCES;
  }

  if (EFI_ERROR(Status))
  {
    Print(L"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_FAIL), NULL));
    UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_FAIL), NULL));
    LogToFile(LogFile, Log);
  }
  else
  {
    //
    //Try retrieving package list in the HII database by guid
    //
    Status = EFI_NOT_FOUND;

    for (UINTN i = 0; i < BufferSize / sizeof(EFI_HII_HANDLE); i++) {
      PackageList = GetHandlePackageList(HiiHandleBuffer[i]);

      if (CompareGuid(&PackageListGuid, &PackageList->PackageListGuid)) {
        HiiHandle = HiiHandleBuffer[i];

        Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_SUCCESS), NULL), HiiHandle);
        UnicodeSPrint(Log, genericBufferSize, u"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_SUCCESS), NULL), HiiHandle);
        LogToFile(LogFile, Log);
        Status = EFI_SUCCESS;
        break;
      }
    }
  }

  EFI_HANDLE RetrievedHandleFromHii;
  if (HiiHandle != NULL && Status != EFI_SUCCESS) {
    Status = gHiiDatabase->GetPackageListHandle(gHiiDatabase, HiiHandle, &RetrievedHandleFromHii); //Find what handle did the last function create and save to RetrievedHandleFromHii
    if (EFI_ERROR(Status)) {
      Print(L"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_FAIL), NULL));
      UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_FAIL), NULL));
      LogToFile(LogFile, Log);
    }
    else
    {
      Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_SUCCESS), NULL), RetrievedHandleFromHii);
      UnicodeSPrint(Log, genericBufferSize, u"%s%X\n\r", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_SUCCESS), NULL), RetrievedHandleFromHii);
      LogToFile(LogFile, Log);

      // Get the HiiPackageList from the Image's Handle
      Status = gBS->HandleProtocol(RetrievedHandleFromHii, &gEfiHiiPackageListProtocolGuid, (VOID **)&PackageList);
      if (EFI_ERROR(Status)) {
        DEBUG_CODE(
          //Debug
          Print(L"HandleProtocol error, getting HII Package List via GetHandlePackageList...\n\r");
        );
        PackageList = GetHandlePackageList(HiiHandle);
      }

      Status = EFI_SUCCESS;
    }
  }

  //
  // Result print part
  //
  if (HiiHandle == NULL || Status != EFI_SUCCESS)
  {
    Print(L"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_LOAD_NOT_CONFIRMED), NULL));
    UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_LOAD_NOT_CONFIRMED), NULL));
    LogToFile(LogFile, Log);

    *HiiHandleP = NULL;
  }
  else
  {
    Print(L"%s%g%s%X%s%X\n\r",
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_GUID), NULL),
          GetHandlePackageList(HiiHandle)->PackageListGuid,
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_SIZE), NULL),
          GetHandlePackageList(HiiHandle)->PackageLength,
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_AT), NULL),
          GetHandlePackageList(HiiHandle)
    );
    UnicodeSPrint(Log, genericBufferSize, u"%s%g%s%X%s%X\n\r",
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_GUID), NULL),
          GetHandlePackageList(HiiHandle)->PackageListGuid,
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_SIZE), NULL),
          GetHandlePackageList(HiiHandle)->PackageLength,
          HiiGetStringSREP(HiiHandleSREP, STRING_TOKEN(STR_HII_PACKAGE_LIST_AT), NULL),
          GetHandlePackageList(HiiHandle)
    );
    LogToFile(LogFile, Log);

    *HiiHandleP = HiiHandle;
  }

  return Status;
}
