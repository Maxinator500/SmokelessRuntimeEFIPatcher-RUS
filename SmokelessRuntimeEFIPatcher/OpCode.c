#include "Opcode.h"

EFI_STATUS
FindLoadedImageFromName(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINTN BufferSize = 0;
    EFI_HANDLE *Handles = NULL;
    //-----------------------------------------------------

    SetFilterProtocolForLoadedImageFunction(&FilterProtocol);

    CHAR16 FileName16[0x100] = { 0 };
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    //Catch not enough memory here, allocate buffer pool
    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, NULL);
    Handles = AllocateZeroPool(BufferSize);

    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, Handles);
    DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", BufferSize / sizeof(EFI_HANDLE)););

    for (UINTN i = 0; i < BufferSize / sizeof(EFI_HANDLE); i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageBuffer(*ImageInfo, EFI_SECTION_USER_INTERFACE, 0, (VOID *)NULL);

            if (String != NULL)
            {
                if (StrCmp(FileName16, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    DEBUG_CODE(
                      //Debug
                      PrintConsoleSREP(L"Found %s at Address 0x%X\n\r", String, (*ImageInfo)->ImageBase);
                    );

                    Status = EFI_SUCCESS;
                    break;
                }
            }
        }

        Status = EFI_NOT_FOUND;
    }

    if (Handles != NULL) {
      FreePool(Handles);
    }

    return Status;
}

EFI_STATUS
FindLoadedImageFromGUID(
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
      PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      LogToFile(LogFile, Log);

      return Status;
    }

    //Catch not enough memory here, allocate buffer pool
    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, NULL);
    Handles = AllocateZeroPool(BufferSize);

    Status = gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, Handles);

    DEBUG_CODE(
      //Debug
      PrintConsoleSREP(L"Found %d Instances\n\r", BufferSize / sizeof(EFI_HANDLE));
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
                PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_TE_HINT), NULL), ImageInfo[0]->ImageSize);
              }

              PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_LOADED_GUID), NULL), ImageInfo[0]->ImageSize);
              Status = EFI_SUCCESS;

              break;
            }
        }

        Status = EFI_NOT_FOUND;
    }

    if (Handles != NULL) {
      FreePool(Handles);
    }

    return Status;
}

EFI_STATUS
FindLoadedImageFromShellIndex(
  IN UINTN HandleIndex,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE **HandleBuffer,
  IN EFI_GUID FilterProtocol
)
{
    EFI_HANDLE TheHandle;
    //-----------------------------------------------------
    
    SetFilterProtocolForLoadedImageFunction(&FilterProtocol);

    PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HANDLE_INDEX), NULL), HandleIndex);

    EFI_HANDLE *AllHandles = GetHandleListByProtocol(NULL);
    if (AllHandles == NULL)
    {
      PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_GET_HANDLE_LIST_FAIL), NULL));
      return EFI_NOT_FOUND;
    }

    //Save handles to buffer
    *HandleBuffer = AllHandles;

    TheHandle = ConvertHandleIndexToHandle(HandleIndex);
    if (TheHandle != NULL)
    {
      PrintConsoleSREP(L"\n\r%X: 0x%X\n\r", HandleIndex, TheHandle);
      {
        return gBS->HandleProtocol(TheHandle, &FilterProtocol, (VOID **)ImageInfo);
      }
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS
LoadFromFS(
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
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleBufferSize, &Handles);

    if (EFI_ERROR(Status))
    {
        PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE_WITH_EFISIMPLEFS), NULL));
        return Status;
    }
    DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", HandleBufferSize););

    for (UINTN Index = 0; Index < HandleBufferSize; Index++)
    {
        Status = gBS->OpenProtocol(
            Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&BlkIo,
            gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(Status))
        {
            PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_UNSUPPORTED_PROTOCOL), NULL), Status);
            return Status;
        }
        CHAR16 FileName16[0x100] = {0};
        UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
        FilePath = FileDevicePath(Handles[Index], FileName16);
        Status = gBS->LoadImage(FALSE, gImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (EFI_ERROR(Status))
        {
            PrintConsoleSREP(L"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_COULD_NOT_LOAD), NULL), Index, Status);
            continue;
        }
        else
        {
            PrintConsoleSREP(L"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SUCCESSFUL_LOAD), NULL), Index);
            Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                       (VOID **)ImageInfo, gImageHandle, (VOID *)NULL,
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);
            return Status;
        }
    }
    return Status;
}

EFI_STATUS
LoadFromFV(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;

    CHAR16 FileName16[0x100] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);

    LocateAndLoadFvFromName(FileName16, Section_Type, &Buffer, &BufferSize, FilterProtocol);
    Status = gBS->LoadImage(FALSE, gImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle);
    if (Buffer != NULL){
      FreePool(Buffer);
    }

    if (!EFI_ERROR(Status))
    {
        Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                   (VOID **)ImageInfo, gImageHandle, (VOID *)NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }

    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

    return Status;
};

EFI_STATUS
LoadGUIDandSavePE(
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type,
  IN EFI_GUID FilterProtocol
)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE_PROTOCOL HandleProtocol = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;;
  EFI_FILE *Root, *DumpFile;
  //-----------------------------------------------------

  HandleProtocol = gST->BootServices->HandleProtocol;
  HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);

  EFI_GUID GUID = { 0 };
  Status = AsciiStrToGuid(FileGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  DEBUG_CODE(
    // Debug
    PrintConsoleSREP(L"GUID from OpCode.c raw: %s\n\r", FileGuid);
    PrintConsoleSREP(L"GUID from OpCode.c converted: %g\n\r", GUID);
  );

  UINT8 *Buffer = NULL;
  UINTN BufferSize = 0;
  Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize, FilterProtocol); //Pass the converted guid to Utility

  //Dumping section
  if (Buffer == NULL) {
    PrintConsoleSREP(L"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NULL_DRIVER_BUFFER), NULL), BufferSize);

    return EFI_NOT_FOUND;
  }

  CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
  UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
  Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
  DumpFile->Write(DumpFile, &BufferSize, (VOID **)Buffer);
  DumpFile->Flush(DumpFile);

  PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_DUMPING), NULL));

  Status = gBS->LoadImage(FALSE, gImageHandle, (VOID *)NULL, Buffer, BufferSize, AppImageHandle); //Parse saved buffer to LoadImage BS
  if (Buffer != NULL)
  {
    FreePool(Buffer);
  }
  if (!EFI_ERROR(Status))
  {
    Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                (VOID **)ImageInfo, gImageHandle, (VOID*)NULL,
                                EFI_OPEN_PROTOCOL_GET_PROTOCOL);
  }
  PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

  return Status;
};

EFI_STATUS
LoadGUIDandSaveFreeform(
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuidStr,
  IN CHAR8 *SectionGuidStr OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_GUID FileGuid = { 0 };
    EFI_GUID SectionGuid = { 0 };
    EFI_HANDLE *HandleBuffer = NULL;
    EFI_FIRMWARE_VOLUME_PROTOCOL *Fv = NULL;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    VOID *SectionData = NULL;
    EFI_SECTION_TYPE SectionType;
    UINT32 Authentication;
    UINTN DataSize = 0, BufferSize = 0, i, j;
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_DEFAULT_FILTER), NULL));
    }

    //Dump related vars
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_FILE *Root, *DumpFile;
    //-----------------------------------------------------

    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Status = AsciiStrToGuid(FileGuidStr, &FileGuid);
    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      LogToFile(LogFile, Log);

      return Status;
    }

    if (SectionGuidStr != NULL)
    {
      Status = AsciiStrToGuid(SectionGuidStr, &SectionGuid);
      if (EFI_ERROR(Status))
      {
        PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
        UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
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
      PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);

      return Status;
    }

    DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", BufferSize););

    //Only search for freeform when its Section Guid is specified
    SectionType = (SectionGuidStr == NULL) ? EFI_SECTION_RAW : EFI_SECTION_FREEFORM_SUBTYPE_GUID;

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

        for (j = 0; ; j++){
            SectionData = NULL;

            //Read Section From FFS file
            if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
              Status = Fv2->ReadSection(
                Fv2,
                &FileGuid,
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
              if (EFI_ERROR(Status) && SectionGuidStr == NULL) {
                Status = Fv2->ReadSection(
                  Fv2,
                  &FileGuid,
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
                &FileGuid,
                SectionType,
                j,
                &SectionData,
                &DataSize,
                &Authentication
              );

              if (EFI_ERROR(Status) && SectionGuidStr == NULL) {
                Status = Fv->ReadSection(
                  Fv,
                  &FileGuid,
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
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_RETRIEVE_FREEFORM), NULL));
              EFI_GUID *RetrievedSectionGuid;

              //Compare Section GUID, to find correct one..
              RetrievedSectionGuid = (EFI_GUID *)SectionData; //We can extract Guid from SectionData

              if (CompareGuid(RetrievedSectionGuid, &SectionGuid) != TRUE) {
                //Free section read, it was not the one we need...
                if (SectionData != NULL) { FreePool(SectionData); }
                continue; //[j]
              }

              DataSize -= sizeof(EFI_GUID);
              // ReadSection returns pointer to a section GUID, which caller of this function does not want to see.
              // We could've just returned (EFI_GUID *)AmiData + 1 to the caller,
              // but this pointer can't be used to free the pool (can't be passed to FreePool).
              // Copy section data into a new buffer
              SectionData = AllocatePool(DataSize);
              if (SectionData == NULL) { Status = EFI_OUT_OF_RESOURCES; }
              else { CopyMem(SectionData, RetrievedSectionGuid + 1, DataSize); }

              FreePool(RetrievedSectionGuid);
            }

            if (HandleBuffer != NULL) {
              FreePool(HandleBuffer);
            }
            if (!EFI_ERROR(Status)){
                PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_DUMPING), NULL));
                *Pointer = SectionData;
                *Size = DataSize;

                CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
                UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuidStr); //Append ".bin"
                Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
                DumpFile->Write(DumpFile, Size, (UINT8 *)SectionData); //(UINT8 *)datum is correct
                DumpFile->Flush(DumpFile);
            }
            return Status;
        }//for [j]
    }//for [i]

    if (HandleBuffer != NULL) {
      FreePool(HandleBuffer);
    }
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
  IN EFI_HII_HANDLE HiiHandle
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
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_LOCATE_FB_FAIL), NULL), Status);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_LOCATE_FB_FAIL), NULL), Status);
    LogToFile(LogFile, Log);

    return Status;
  }

  PackageList = GetHandlePackageList(HiiHandle);

  if((PackageList == NULL) || IsZeroGuid(&PackageList->PackageListGuid))
  {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
    LogToFile(LogFile, Log);

    return EFI_UNSUPPORTED;
  }

  Status = FormBrowser2->SendForm(
                            FormBrowser2,
                            &HiiHandle,
                            1,
                            &PackageList->PackageListGuid,
                            0,
                            NULL,
                            &ActionRequest
                            );

  if (ActionRequest == EFI_BROWSER_ACTION_REQUEST_RESET) {
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_ACTION_REQUEST), NULL), Status);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_ACTION_REQUEST), NULL), Status);
    LogToFile(LogFile, Log);

    gBS->Stall(3000000);
    gBS->RaiseTPL(TPL_NOTIFY);
    gRT->ResetSystem(EfiResetCold, EFI_SUCCESS, 0, NULL);
  }

  PrintConsoleSREP(L"%r%s%g%s%X%s\n\r",
        Status,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED1), NULL),
        PackageList->PackageListGuid,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED2), NULL),
        HiiHandle,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED3), NULL)
  );
  UnicodeSPrint(Log, GenericBufferSize, u"%r%s%g%s%X%s\n\r",
        Status,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED1), NULL),
        PackageList->PackageListGuid,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED2), NULL),
        HiiHandle,
        HiiGetStringSREP(STRING_TOKEN(STR_HII_SEND_FORM_FAILED3), NULL)
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
  VOID *ProtocolInterface;
  EFI_GUID GUID = { 0 };
  UINTN BufferSize = 0;
  //-----------------------------------------------------

  Status = AsciiStrToGuid(ProtocolGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ProtocolGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ProtocolGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  Status = gBS->LocateHandleBuffer(ByProtocol, &GUID, NULL, &BufferSize, &HandlesBuffer);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
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
      PrintConsoleSREP(L"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      UnicodeSPrint(Log, GenericBufferSize, u"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      LogToFile(LogFile, Log);

      continue;
    }

    Status = gBS->UninstallProtocolInterface(HandlesBuffer[i],
                                             &GUID, 
                                             ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%r - %s%g,\n    Handle %d\n    Pointer 0x%x\n\r", Status, HiiGetStringSREP(STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      UnicodeSPrint(Log, GenericBufferSize, u"%r - %s%g,\n    Handle %d\n    Pointer 0x%x\n\r", Status, HiiGetStringSREP(STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      LogToFile(LogFile, Log);

      continue;
    }
    IndicesCount += 1;
  }

  return Status;
}

EFI_STATUS
UpdateHandlePackageList(
  IN CHAR8 *PackageGuid,
  IN CHAR8 *PackageSizeOverride OPTIONAL,
  OUT EFI_LOADED_IMAGE_PROTOCOL *ImageInfo
)
{
  #define MAX_UINT28 ((UINT32)0xFFFFFFF)
  ImageInfo->ImageBase = NULL;
  ImageInfo->ImageSize = 0;

  EFI_STATUS Status;
  EFI_HANDLE *DriverHandleBuffer = NULL;
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  EFI_GUID GUID = { 0 };
  UINTN DriverHandleBufferSize = 0, HiiHandleBufferSize = 0;
  //-----------------------------------------------------

  Status = AsciiStrToGuid(PackageGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), PackageGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), PackageGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  //Catch not enough memory here, allocate buffer pool
  gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleBufferSize, NULL);
  DriverHandleBuffer = AllocateZeroPool(DriverHandleBufferSize);
  if (DriverHandleBuffer == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleBufferSize, DriverHandleBuffer);
  if (EFI_ERROR(Status))
  {
    return Status;
  }
  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Locate driver handles %X status: %r\n\r", DriverHandleBufferSize / sizeof(EFI_HANDLE), Status);
  );


  Status = GetHiiPackagesByTypeAndGuid(EFI_HII_PACKAGE_TYPE_ALL, NULL, &HiiHandleBufferSize, &HiiHandleBuffer);
  if (EFI_ERROR(Status))
  {
    return Status;
  }

  //
  //Try retrieving package list in the HII database by guid
  //
  UINT32 OnePackageLength = MAX_UINT28;
  for (UINTN i = 0; i < HiiHandleBufferSize / sizeof(EFI_HII_HANDLE); i++)
  {
    PackageList = GetHandlePackageList(HiiHandleBuffer[i]);
    DEBUG_CODE(
      //Debug
      PrintConsoleSREP(L"Found PackageList: %g of size 0x%X at 0x%X\n\r", PackageList->PackageListGuid, PackageList->PackageLength, PackageList);
    );

    if (CompareGuid(&GUID, &PackageList->PackageListGuid) || (PackageList->PackageLength == OnePackageLength)) {
      //
      // Initialize a pointer to the beginning if the Package List data
      //
      if (ImageInfo->ImageBase == NULL) {
        ImageInfo->ImageBase = PackageList + 1; //sizeof(EFI_HII_PACKAGE_LIST_HEADER)
        OnePackageLength = PackageList->PackageLength;
        ImageInfo->ImageSize = PackageList->PackageLength;

        continue;
      }

      ImageInfo->ImageSize = ((UINTN*)PackageList - (UINTN*)ImageInfo->ImageBase) + PackageList->PackageLength;
    }
  }

  if (DriverHandleBuffer != NULL) {
    FreePool(DriverHandleBuffer);
    FreePool(HiiHandleBuffer);
  }

  if (ImageInfo->ImageSize == 0) {
    Status = EFI_NOT_FOUND;
  }
  else if (AsciiIsHexString(PackageSizeOverride))
  {
    if (AsciiStrHexToUintnS(PackageSizeOverride, NULL, &ImageInfo->ImageSize))
    {
      PrintConsoleSREP(L"%s\"%X\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ImageInfo->ImageSize);
      UnicodeSPrint(Log, GenericBufferSize, u"%s\"%X\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ImageInfo->ImageSize);
      LogToFile(LogFile, Log);

      Status = EFI_INVALID_PARAMETER;
    }
  }

  return EFI_SUCCESS;
}

BOOLEAN
DoesFvFileExist(
  IN EFI_GUID GUID16
)
{
  EFI_STATUS Status;
  EFI_HANDLE *HandleBuffer = NULL;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
  VOID *String;
  UINTN BufferSize;
  UINT8 Index = 0;
  UINT8 j = 0;
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

  DEBUG_CODE(AsciiPrint("\nFound %d Instances\n\r", BufferSize););

  for (Index = 0; Index < BufferSize; Index++)
  {

    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      HandleBuffer[Index],
      &gEfiFirmwareVolume2ProtocolGuid,
      (VOID **)&FvInstance);
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
        FreePool(Keys);
        return TRUE;
      }
      FreePool(String);
    }

    if (Keys != NULL) {
      FreePool(Keys);
    }
  }
  return FALSE;
}

EFI_STATUS
AddFirstHiiPackageFromFile(
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
      PrintConsoleSREP(L"Unable to locate HII Database!\n\r");
    );
    return EFI_UNSUPPORTED;
  }

  //
  //Get Pointers part
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
      PrintConsoleSREP(L"Pointers %X, %X, %X\n\r", DevicePathPointer, ifrSetupStringsPointer, ifrHiiBinPointer);
    );

    //
    //Get PackageListGuid from image
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
    //DevicePath doesn't exist, generate one
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
      PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_HII_UNK_UEFI_VENDOR), NULL));
      UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_HII_UNK_UEFI_VENDOR), NULL));
      LogToFile(LogFile, Log);

      return EFI_UNSUPPORTED;
    }

    PrintConsoleSREP(L"%s%X -> %g\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_DEV_PATH_NOT_FOUND), NULL), DevicePathPointer, &PackageListGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%X -> %g\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_DEV_PATH_NOT_FOUND), NULL), DevicePathPointer, &PackageListGuid);
    LogToFile(LogFile, Log);
  }
  else
  {
    //
    //GetSetupPointers failed, return status
    //
    PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_SETUP_POINTERS_NOT_FOUND), NULL), ImageInfo->ImageBase);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_HII_SETUP_POINTERS_NOT_FOUND), NULL), ImageInfo->ImageBase);
    LogToFile(LogFile, Log);

    return Status;
  }

  //
  //Publish resources part
  //
  //if DeviceHandle = NULL, handle will be assigned.
  // 
  //mHiiVendorDevicePath0 must be a pointer to dev. path in EFI setup binary. Dev. path is DevicePathHeader + Guid

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Printing 4 bytes for Vfr 0x%X and Uni 0x%X...\n", *ifrHiiBinPointer, *ifrSetupStringsPointer);
  );

  //EDK2' HiiAddPackages didnt work.
  HiiHandle = HiiConstructAddPackagesSREP(
    &PackageListGuid,
    ImageInfo->DeviceHandle,
    (UINT8 *)ifrHiiBinPointer,
    (UINT8 *)ifrSetupStringsPointer
  );
  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"HiiAddPackages done. Handle - %X\n", HiiHandle);
  );


  //
  //HiiHandle checks part
  //
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN BufferSize;

  Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, NULL);

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"HiiHandle instances: %X\n\r", BufferSize / sizeof(EFI_HII_HANDLE));
  );


  gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, HiiHandleBuffer);
  HiiHandleBuffer = AllocateZeroPool(BufferSize);
  if (HiiHandleBuffer == NULL) {
    EFI_OUT_OF_RESOURCES;
  }

  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s", HiiGetStringSREP(STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_FAIL), NULL));
    UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_FAIL), NULL));
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

        PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_SUCCESS), NULL), HiiHandle);
        UnicodeSPrint(Log, GenericBufferSize, u"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_GET_HANDLE_PACKAGE_LIST_SUCCESS), NULL), HiiHandle);
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
      PrintConsoleSREP(L"%s", HiiGetStringSREP(STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_FAIL), NULL));
      UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_FAIL), NULL));
      LogToFile(LogFile, Log);
    }
    else
    {
      PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_SUCCESS), NULL), RetrievedHandleFromHii);
      UnicodeSPrint(Log, GenericBufferSize, u"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_GET_PACKAGE_LIST_HANDLE_SUCCESS), NULL), RetrievedHandleFromHii);
      LogToFile(LogFile, Log);

      //Get the HiiPackageList from the Image's Handle
      Status = gBS->HandleProtocol(RetrievedHandleFromHii, &gEfiHiiPackageListProtocolGuid, (VOID **)&PackageList);
      if (EFI_ERROR(Status)) {
        DEBUG_CODE(
          //Debug
          PrintConsoleSREP(L"HandleProtocol error, getting HII Package List via GetHandlePackageList...\n\r");
        );
        PackageList = GetHandlePackageList(HiiHandle);
      }

      Status = EFI_SUCCESS;
    }
  }

  //
  //Result print part
  //
  if (HiiHandle == NULL || Status != EFI_SUCCESS)
  {
    PrintConsoleSREP(L"%s", HiiGetStringSREP(STRING_TOKEN(STR_HII_LOAD_NOT_CONFIRMED), NULL));
    UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_HII_LOAD_NOT_CONFIRMED), NULL));
    LogToFile(LogFile, Log);

    *HiiHandleP = NULL;
  }
  else
  {
    PrintConsoleSREP(L"%s%g%s%X%s%X\n\r",
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_GUID), NULL),
          GetHandlePackageList(HiiHandle)->PackageListGuid,
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_SIZE), NULL),
          GetHandlePackageList(HiiHandle)->PackageLength,
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_AT), NULL),
          GetHandlePackageList(HiiHandle)
    );
    UnicodeSPrint(Log, GenericBufferSize, u"%s%g%s%X%s%X\n\r",
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_GUID), NULL),
          GetHandlePackageList(HiiHandle)->PackageListGuid,
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_SIZE), NULL),
          GetHandlePackageList(HiiHandle)->PackageLength,
          HiiGetStringSREP(STRING_TOKEN(STR_HII_PACKAGE_LIST_AT), NULL),
          GetHandlePackageList(HiiHandle)
    );
    LogToFile(LogFile, Log);

    *HiiHandleP = HiiHandle;
  }

  if (HiiHandleBuffer != NULL) {
    FreePool(HiiHandleBuffer);
  }

  return Status;
}

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

  IN INT64 CurrentOffset,
  IN OUT INT64 *BaseOffset,

  OUT BOOLEAN *isOpSkipAllowed,
  OUT EFI_STATUS *Status
)
{
  UINTN MatchesCount = 0;
  UINT64 *Captures = { 0 }; //Represents found offsets, each offset can be up to 8 bytes

  switch (PatchType) {
    case FASTEST:

      if (SearchType == PATTERN && CurrentOffset)
      {
          PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_SEARCH), NULL));

          for (UINTN i = 0; i < ImageInfo->ImageSize - PatternLength; i++)
          {
            if (!(i % 0x10000)) {
              //PrintConsoleSREP(L"\r0x%X out of 0x%X", &ImageInfo->ImageBase + i, &ImageInfo->ImageBase + ImageInfo->ImageSize);
              DisplayUpdateProgressSREP((UINTN)(((float)i / ImageInfo->ImageSize) * 100));
            }

            if (i >= MAX_UINT32)
            {
              DisplayUpdateProgressSREP(100);
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
              UnicodeSPrint(Log, GenericBufferSize, u"%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
              LogToFile(LogFile, Log);

              CurrentOffset = 0;
              break;
            }

            //Old imp, no regex
            if (CompareMem(((UINT8 *)ImageInfo->ImageBase) + i, (UINT8 *)SimplePatternBuffer, PatternLength) == 0)
            {
              CurrentOffset = i;
              break;
            }
          }
          PrintConsoleSREP(L"\n");
      }
      if (SearchType == REL_POS_OFFSET && CurrentOffset)
      {
        CurrentOffset = *BaseOffset + CurrentOffset;
        PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_REL_POS_OFFSET), NULL));
      }
      if (SearchType == REL_NEG_OFFSET && CurrentOffset)
      {
        CurrentOffset = *BaseOffset - CurrentOffset;
        PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_REL_NEG_OFFSET), NULL));
      }
      *BaseOffset = CurrentOffset;

      PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_MATCH_OFFSET), NULL), CurrentOffset);
      UnicodeSPrint(Log, GenericBufferSize, u"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_MATCH_OFFSET), NULL), CurrentOffset);

      if (CurrentOffset > 0) {
        *isOpSkipAllowed = TRUE;

        CopyMem((UINT8 *)ImageInfo->ImageBase + CurrentOffset, (UINT8 *)SimplePatchBuffer, PatchLength);

        PrintConsoleSREP(L"\n%s", HiiGetStringSREP(STRING_TOKEN(STR_PATCHED), NULL));
        UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_PATCHED), NULL));

        *Status = EFI_SUCCESS;
      }
      else
      {
        PrintConsoleSREP(L"\n%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));
        UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));\
      }
      LogToFile(LogFile, Log);

      break;

    default:
      if ((SearchType == PATTERN) && CurrentOffset)
      {
        if ((PatternLength > GenericBufferSize - 1) || (PatchLength > GenericBufferSize - 1)) {
          PrintConsoleSREP(L"%s%X%s\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)PatternLength, L"HALT!");
          UnicodeSPrint(Log, GenericBufferSize, u"%s%X%s\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)PatternLength, L"HALT!");
          LogToFile(LogFile, Log);
          return EFI_UNSUPPORTED;
        };

        PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_SEARCH), NULL));
        PrintConsoleSREP(L"\n%a\n\n\r", (CHAR8 *)RegexPatternBuffer); //PrintDump ascii to screen
        PrintDump(PatternLength, (UINT8 *)RegexPatternBuffer, TRUE); //PrintDump unicode to log

        if (PatchLimit >= 0) {
          PrintConsoleSREP(L"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_LIMIT), NULL), PatchLimit);
          UnicodeSPrint(Log, GenericBufferSize, u"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_LIMIT), NULL), PatchLimit);
          LogToFile(LogFile, Log);
        }

        //
        //Get pattern locations
        //
        UINT8 *HexPattern = AllocateZeroPool(PatternLength);
        CHAR8 *ProcessedPattern = NULL;

        for (UINTN i = 0; (i < ImageInfo->ImageSize - PatternLength) && PatchLimit; i++)
        {
          BOOLEAN CResult = FALSE; //Reset result to keep RegexMatch running

          if (!(i % 0x10000)) {
            //PrintConsoleSREP(L"\r0x%X out of 0x%X", &ImageInfo->ImageBase + i, &ImageInfo->ImageBase + ImageInfo->ImageSize);
            DisplayUpdateProgressSREP((UINTN)(((float)i / ImageInfo->ImageSize) * 100));
          }

          if (i >= MAX_UINT32)
          {
            DisplayUpdateProgressSREP(100);
            PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
            UnicodeSPrint(Log, GenericBufferSize, u"%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
            LogToFile(LogFile, Log);

            CurrentOffset = 0;
            break;
          }

          if (PatchType == REGEX) {
            RegexMatch(((UINT8 *)ImageInfo->ImageBase) + i, (CHAR8 *)RegexPatternBuffer, (UINT16)PatternLength, RegularExpressionProtocol, &CResult);
          }
          else
          {
            ProcessedPattern = ApplySimpleMask((UINT8 *)ImageInfo->ImageBase + i, RegexPatternBuffer, PatternLength, '.');
            if (RETURN_ERROR(AsciiStrHexToBytes(ProcessedPattern, PatternLength * sizeof(UINT16), HexPattern, PatternLength)))
            {
              FreePool(ProcessedPattern);
              break;
            }

            CResult = (BOOLEAN)!CompareMem(((UINT8 *)ImageInfo->ImageBase) + i, (UINT8 *)HexPattern, PatternLength);
            FreePool(ProcessedPattern);
          }

          if (CResult)
          {
            //Fill Captures[MatchesCount]
            Captures[MatchesCount] = i;
            MatchesCount++;

            PatchLimit--;

            PrintDump(PatternLength, (UINT8 *)ImageInfo->ImageBase + i, FALSE);
            UnicodeSPrint(Log, GenericBufferSize, u"\n\r");
            LogToFile(LogFile, Log);
          }
        }

        PrintConsoleSREP(L"\n");
        if (HexPattern != NULL) {
          FreePool(HexPattern);
        }
      }

      //
      //Replace pattern locations
      //
      if (MatchesCount) {

        *isOpSkipAllowed = TRUE;

        //Patch mask + memory dump
        CHAR8 *ProcessedPatch = NULL;

        //ProcessedPatch converted to byte array
        UINT8 *HexPatch = AllocateZeroPool(PatchLength);

        //Save offset of the last match for future use
        CurrentOffset = Captures[MatchesCount - sizeof(UINT8)];

        PrintConsoleSREP(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_MATCH_OFFSET), NULL), CurrentOffset);
        UnicodeSPrint(Log, GenericBufferSize, u"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_MATCH_OFFSET), NULL), CurrentOffset);

        PrintConsoleSREP(L"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_TOTAL_MATCHES), NULL), MatchesCount);
        UnicodeSPrint(Log, GenericBufferSize, u"%s%d\n\t", HiiGetStringSREP(STRING_TOKEN(STR_TOTAL_MATCHES), NULL), MatchesCount);
        LogToFile(LogFile, Log);

        for (UINTN i = 0; i < MatchesCount; i++) {

          //Regex support for hex sequence patch
          ProcessedPatch = ApplySimpleMask((UINT8 *)ImageInfo->ImageBase + Captures[i], RegexPatchBuffer, PatchLength, '.');

          if (RETURN_ERROR(AsciiStrHexToBytes(ProcessedPatch, PatchLength * sizeof(UINT16), HexPatch, PatchLength)))
          {
            FreePool(ProcessedPatch);
            FreePool(HexPatch);

            return EFI_UNSUPPORTED;
          }

          CopyMem((UINT8 *)ImageInfo->ImageBase + Captures[i], HexPatch, PatchLength);

          PrintConsoleSREP(L"\n%d%s%x", i + 1, HiiGetStringSREP(STRING_TOKEN(STR_NEXT_OFFSET), NULL), (UINT8 *)ImageInfo->ImageBase + Captures[i]);
          UnicodeSPrint(Log, GenericBufferSize, u"\n\t%d%s%x", i + 1, HiiGetStringSREP(STRING_TOKEN(STR_NEXT_OFFSET), NULL), (UINT8 *)ImageInfo->ImageBase + Captures[i]);
          LogToFile(LogFile, Log);

          PrintDump(PatternLength, (UINT8 *)ImageInfo->ImageBase + Captures[i], FALSE);
        }

        //MatchesCount is unsigned and we reach here if MatchesCount != 0, so shouldn't get the Captures array index out of bounds error when substracting 1
        if (!CompareMem((UINT8 *)ImageInfo->ImageBase + Captures[MatchesCount - 1], HexPatch, PatchLength)) {
          *Status = EFI_SUCCESS;
          FreePool(ProcessedPatch);
          FreePool(HexPatch);

          PrintConsoleSREP(L"\n%s", HiiGetStringSREP(STRING_TOKEN(STR_PATCHED), NULL));
          UnicodeSPrint(Log, GenericBufferSize, u"\r%s", HiiGetStringSREP(STRING_TOKEN(STR_PATCHED), NULL));
        }
        else
        {
          PrintConsoleSREP(L"\n%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));
          UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));
        }
      }
      else
      {
        PrintConsoleSREP(L"\n%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));
        UnicodeSPrint(Log, GenericBufferSize, u"%s", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL));
      }
      LogToFile(LogFile, Log);

      ZeroMem(Captures, MatchesCount * sizeof(UINT64));
      MatchesCount = 0;

      break;
  }

  return *Status;
}
