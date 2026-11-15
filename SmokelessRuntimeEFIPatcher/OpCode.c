#include "Opcode.h"

EFI_STATUS
FindLoadedImageFromName(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo
)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles = NULL;
  UINTN BufferSize = AsciiStrSize(FileName) * sizeof(UINT16);
  //-----------------------------------------------------

  CHAR16 *FileName16 = AllocateZeroPool(BufferSize);
  if (FileName16 == NULL) {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    LogToFile(LogFile, Log);
    return EFI_OUT_OF_RESOURCES;
  }
  
  UnicodeSPrint(FileName16, BufferSize, L"%a", FileName);

  BufferSize = 0;
  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &BufferSize, NULL);
  Handles = AllocateZeroPool(BufferSize);
  if (Handles == NULL)
  {
    return EFI_OUT_OF_RESOURCES;
  }

  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &BufferSize, Handles);
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
    FreePool(FileName16);
    FreePool(Handles);
  }

  return Status;
}

EFI_STATUS
FindLoadedImageFromGUID(
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo
)
{
  EFI_STATUS Status;
  EFI_GUID GUID = gZeroGuid;
  EFI_HANDLE *Handles = NULL;
  UINTN BufferSize = 0;
  //-----------------------------------------------------

  Status = AsciiStrToGuid(FileGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &BufferSize, NULL);
  Handles = AllocateZeroPool(BufferSize);

  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &BufferSize, Handles);

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Found %d Instances\n\r", BufferSize / sizeof(EFI_HANDLE));
  );

  for (UINTN i = 0; i < BufferSize / sizeof(EFI_HANDLE); i++)
  {
    Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle

    if ((Status == EFI_SUCCESS) && (ImageInfo[0]->ImageSize != 0))
    {
      if (CompareGuid(EfiHandleDumpFilePath(Handles[i]), &GUID))
      {
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
  OUT EFI_HANDLE **HandleBuffer
)
{
  EFI_HANDLE TheHandle;
  //-----------------------------------------------------

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
      return gBS->HandleProtocol(TheHandle, &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo);
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
  UINTN NameBufferSize = AsciiStrSize(FileName) * sizeof(UINT16);
  //-----------------------------------------------------

  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &HandleBufferSize, &Handles);

  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE_WITH_EFISIMPLEFS), NULL));
    return Status;
  }
  DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", HandleBufferSize););

  CHAR16 *FileName16 = AllocateZeroPool(NameBufferSize);
  if (FileName16 == NULL) {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    LogToFile(LogFile, Log);
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeSPrint(FileName16, NameBufferSize, L"%a", FileName);

  for (UINTN Index = 0; Index < HandleBufferSize; Index++)
  {
    Status = gBS->OpenProtocol(
      Handles[Index],
      &gEfiSimpleFileSystemProtocolGuid,
      (VOID **)&BlkIo,
      gImageHandle,
      NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_UNSUPPORTED_PROTOCOL), NULL), Status);
      FreePool(FileName16);
      FreePool(Handles);

      return Status;
    }

    FilePath = FileDevicePath(Handles[Index], FileName16);
    Status = gBS->LoadImage(
      FALSE,
      gImageHandle,
      FilePath,
      (VOID *)NULL,
      0,
      AppImageHandle
    );

    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_FS_LOAD), NULL), Index, Status);
      continue;
    }

    EFI_SHELL_PARAMETERS_PROTOCOL *ParentParams = NULL;
    EFI_SHELL_PARAMETERS_PROTOCOL ChildParams;
    CHAR16 *ChildArgv[] = { FileName16 };

    Status = gBS->OpenProtocol(
      *AppImageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **)ImageInfo,
      gImageHandle,
      (VOID *)NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
    FreePool(FileName16);
    FreePool(Handles);

    Status = gBS->HandleProtocol(
      gImageHandle,
      &gEfiShellParametersProtocolGuid,
      (VOID **)&ParentParams
    );

    ChildParams.Argc = sizeof(ChildArgv);
    ChildParams.Argv = ChildArgv;
    if (!EFI_ERROR(Status) && (ParentParams != NULL)) {
      ChildParams.StdIn = ParentParams->StdIn;
      ChildParams.StdOut = ParentParams->StdOut;
      ChildParams.StdErr = ParentParams->StdErr;
    }
    else {
      ChildParams.StdIn = gST->ConsoleInHandle;
      ChildParams.StdOut = gST->ConsoleOutHandle;
      ChildParams.StdErr = gST->StandardErrorHandle;
    }

    gBS->InstallProtocolInterface(
      AppImageHandle,
      &gEfiShellParametersProtocolGuid,
      EFI_NATIVE_INTERFACE,
      &ChildParams
    );

    PrintConsoleSREP(L"%s%d\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SUCCESSFUL_FS_LOAD), NULL), Index);
    return Status;
  }

  FreePool(FileName16);
  if (Handles != NULL) {
    FreePool(Handles);
  }

  return Status;
}

EFI_STATUS
LoadFromFV(
  IN CHAR8 *FileName,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type
)
{
  EFI_STATUS Status;
  EFI_HANDLE NewHandle = NULL;
  UINT8 *Buffer = NULL;
  UINTN BufferSize = AsciiStrSize(FileName) * sizeof(UINT16);

  CHAR16 *FileName16 = AllocateZeroPool(BufferSize);
  if (FileName16 == NULL) {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    LogToFile(LogFile, Log);
    return EFI_OUT_OF_RESOURCES;
  }

  UnicodeSPrint(FileName16, BufferSize, L"%a", FileName);

  BufferSize = 0;
  LocateAndLoadFromFV(NULL, FileName16, Section_Type, &Buffer, &BufferSize);

  Status = gBS->LoadImage(
    FALSE,
    gImageHandle,
    (VOID *)NULL,
    Buffer,
    BufferSize,
    &NewHandle
  );
  if (Buffer != NULL){
    FreePool(FileName16);
    FreePool(Buffer);
  }

  if (!EFI_ERROR(Status))
  {
    Status = gBS->OpenProtocol(
      NewHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **)ImageInfo,
      gImageHandle,
      (VOID *)NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );

    *AppImageHandle = NewHandle;
  }
  else
  {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_LOADIMAGE_ERROR), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_LOADIMAGE_ERROR), NULL));
    LogToFile(LogFile, Log);
  }

  return Status;
};

EFI_STATUS
LoadGUIDandSavePE(
  IN CHAR8 *FileGuid,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE *AppImageHandle,
  IN EFI_SECTION_TYPE Section_Type
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

  EFI_GUID GUID = gZeroGuid;
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
  Status = LocateAndLoadFromFV(&GUID, L"DEADBEEF", Section_Type, &Buffer, &BufferSize); //Pass the converted guid to Utility

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
    Status = gBS->OpenProtocol(
      *AppImageHandle,
      &gEfiLoadedImageProtocolGuid,
      (VOID **)ImageInfo,
      gImageHandle,
      (VOID *)NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL
    );
  }

  return Status;
}

EFI_STATUS
LoadGUIDandSaveFreeform(
  OUT VOID **Pointer,
  OUT UINT64 *Size,
  IN CHAR8 *FileGuidStr,
  IN CHAR8 *SectionGuidStr OPTIONAL,
  IN EFI_SYSTEM_TABLE *SystemTable
)
{
  EFI_STATUS Status;
  EFI_GUID FileGuid = gZeroGuid;
  EFI_GUID SectionGuid = gZeroGuid;
  EFI_HANDLE *HandleBuffer = NULL;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
  VOID *SectionData = NULL;
  EFI_SECTION_TYPE SectionType;
  UINT32 Authentication;
  UINTN DataSize = 0, BufferSize = 0, i, j;
  PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_DEFAULT_FILTER), NULL));

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

  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &FvProtocolGuid,
    NULL,
    &BufferSize,
    &HandleBuffer
  );

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

    Status = gBS->HandleProtocol(HandleBuffer[i], &FvProtocolGuid, (VOID **)&Fv2);
    if (EFI_ERROR(Status)) { continue; } //[i]

    for (j = 0; ; j++) {
      SectionData = NULL;

      //Read Section From FFS file
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
        //ReadSection returns pointer to a section GUID, which caller of this function does not want to see.
        //We could've just returned (EFI_GUID *)AmiData + 1 to the caller,
        //but this pointer can't be used to free the pool (can't be passed to FreePool).
        //Copy section data into a new buffer
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

  PackageList = GetHandlePackageListAndGuid(HiiHandle, NULL);

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
  EFI_GUID GUID;
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
    Status = gBS->HandleProtocol(
      HandlesBuffer[i],
      &GUID,
      (VOID **)&ProtocolInterface
    );

    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      UnicodeSPrint(Log, GenericBufferSize, u"%s%d: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      LogToFile(LogFile, Log);

      continue;
    }

    Status = gBS->UninstallProtocolInterface(
      HandlesBuffer[i],
      &GUID, 
      ProtocolInterface
    );

    if (EFI_ERROR(Status))
    {
      PrintConsoleSREP(L"%r - %s%g,\n    Handle %d\n    Pointer 0x%X\n\r", Status, HiiGetStringSREP(STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      UnicodeSPrint(Log, GenericBufferSize, u"%r - %s%g,\n    Handle %d\n    Pointer 0x%X\n\r", Status, HiiGetStringSREP(STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      LogToFile(LogFile, Log);

      continue;
    }
    IndicesCount += 1;
  }

  return Status;
}

EFI_STATUS
UpdateHandlePackageList(
  IN CHAR8 *FileGuid
)
{
  EFI_STATUS Status;
  EFI_GUID FormSetGuid;
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  EFI_LOADED_IMAGE_PROTOCOL **ImageInfo = NULL;
  UINT8 *PackageFormSetP = NULL;
  UINT8 *IfrHiiBinFormSetP = NULL;
  UINTN HiiHandleBufferSize;
  UINT8 HiiHandlesCount;
  //-----------------------------------------------------

  Status = AsciiStrToGuid(FileGuid, &FormSetGuid);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
    LogToFile(LogFile, Log);

    return Status;
  }

  //Get all form containing handles to HiiHandleBuffer
  Status = GetHiiPackagesByTypeAndGuid(EFI_HII_PACKAGE_FORMS, NULL, &HiiHandleBufferSize, &HiiHandleBuffer);
  if (EFI_ERROR(Status))
  {
    return Status;
  }

  //Get images of the handles
  HiiHandlesCount = (UINT8)(HiiHandleBufferSize / sizeof(EFI_HII_HANDLE));
  Status = GetLoadedImagesFromHiiHandles(HiiHandleBuffer, HiiHandlesCount, &ImageInfo);
  if (EFI_ERROR(Status))
  {
    return Status;
  }

  //
  //For the list of HII Handles, try retrieving Package List and find forms in parent EFI image
  //
  Status = EFI_NOT_FOUND;
  for (UINTN i = 0; i < HiiHandlesCount; i++)
  {
    UINTN BufferSize = 0;

    //PackageList
    Status = gHiiDatabase->ExportPackageLists(gHiiDatabase, HiiHandleBuffer[i], &BufferSize, PackageList);
    if (EFI_ERROR(Status))
    {
      PackageList = AllocateZeroPool(BufferSize);
      if (PackageList == NULL) {
        PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
        UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
        LogToFile(LogFile, Log);
        return EFI_OUT_OF_RESOURCES;
      }

      Status = gHiiDatabase->ExportPackageLists(gHiiDatabase, HiiHandleBuffer[i], &BufferSize, PackageList);
    }
    DEBUG_CODE(
      //Debug
      PrintConsoleSREP(L"Parsing PackageList: %g\n\r", PackageList->PackageListGuid);
    );

    if (PackageList->PackageLength > 0)
    {
      //Forms
      Status = GetFormSetPointerByGuid(ImageInfo[i], FormSetGuid, &IfrHiiBinFormSetP);
      if (EFI_ERROR(Status))
      {
        continue;
      }

      //Since list can contain multiple EFI_IFR_FORM_SET_OP, make sure we pick the correct and not simply the first one
      PackageFormSetP = PackageBufferOffsetByFormSetGuid(&FormSetGuid, (UINT8 *)(PackageList + 1), PackageList->PackageLength);

      //Continue if not found
      if (IfrHiiBinFormSetP == NULL || PackageFormSetP == NULL) { continue; }

      DEBUG_CODE(
        //Debug
        PrintConsoleSREP(L"PackageList 0x%X IfrHiiBinPointer 0x%X\nLength 0x%X and 0x%X\n\r", (UINT8 *)PackageFormSetP, (IfrHiiBinFormSetP + sizeof(UINT32)), ((EFI_HII_PACKAGE_HEADER *)PackageFormSetP)->Length, ((EFI_HII_PACKAGE_HEADER *)(IfrHiiBinFormSetP + sizeof(UINT32)))->Length);
      );

      //Copy data from loaded EFI image to PackageList at the corresponding offsets(IfrHiiBinPointer to PackageList)
      CopyMem(PackageFormSetP, (IfrHiiBinFormSetP + sizeof(UINT32)), ((EFI_HII_PACKAGE_HEADER *)(IfrHiiBinFormSetP + sizeof(UINT32)))->Length);

      //Real package List require update using patched export...
      Status = gHiiDatabase->UpdatePackageList(
        gHiiDatabase,
        HiiHandleBuffer[i],
        PackageList
      );

      if (!EFI_ERROR(Status)) {
        PrintConsoleSREP(L"%g%s", PackageList->PackageListGuid, HiiGetStringSREP(STRING_TOKEN(STR_PACK_UPDATE_SUCCESS), NULL));
        UnicodeSPrint(Log, GenericBufferSize, u"%g%s", PackageList->PackageListGuid, HiiGetStringSREP(STRING_TOKEN(STR_PACK_UPDATE_SUCCESS), NULL));
      }
      break;
    }
  }

  if (ImageInfo != NULL) {
    FreePool(ImageInfo);
  }
  if (PackageList != NULL) {
    FreePool(PackageList);
  }
  if (HiiHandleBuffer != NULL) {
    FreePool(HiiHandleBuffer);
  }

  return Status;
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
    &FvProtocolGuid,
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
      &FvProtocolGuid,
      (VOID **)&FvInstance);
    ASSERT_EFI_ERROR(Status);

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize;
    EFI_GUID NameGuid = gZeroGuid;
    VOID *Keys = AllocateZeroPool(FvInstance->KeySize);

    while (j < BufferSize)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      ZeroMem(&NameGuid, sizeof(EFI_GUID));
      Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
      if (EFI_ERROR(Status))
      {
        //PrintConsoleSREP(L"\nBreaking cause: %r\n\r", Status);

        j++;
        break;
      }
      
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);

      DEBUG_CODE(
        // Debug
        PrintConsoleSREP(L"\nCurrent GUID: %g\n\r", NameGuid); //Current processing guid per While iteration
      );

      if (CompareGuid(&GUID16, &NameGuid))
      {
        DEBUG_CODE(
          // Debug
          PrintConsoleSREP(L"\nFound Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType);
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
  UINT64 *Captures = AllocateZeroPool(SREP_MAX_MATCHES * sizeof(UINT64)); //Represents found offsets, each offset can be up to 8 bytes
  if (Captures == NULL) {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    LogToFile(LogFile, Log);

    return EFI_OUT_OF_RESOURCES;
  }

  switch (PatchType) {
    case FASTEST:

    if (SearchType == PATTERN && CurrentOffset)
    {
      PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_SEARCH), NULL));

      for (UINTN i = 0; i + PatternLength <= ImageInfo->ImageSize; i++)
      {
        if (!(i % 0x10000)) {
          //PrintConsoleSREP(L"\r0x%X out of 0x%X", &ImageInfo->ImageBase + i, &ImageInfo->ImageBase + ImageInfo->ImageSize);
          DisplayUpdateProgressSREP((UINTN)(((float)i / ImageInfo->ImageSize) * 100) + 1);
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
      if ((PatternLength > GenericBufferSize / sizeof(UINT16) - 1) || (PatchLength > GenericBufferSize / sizeof(UINT16) - 1)) {
        PrintConsoleSREP(L"%s%X%s\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)PatternLength, L"HALT!");
        UnicodeSPrint(Log, GenericBufferSize, u"%s%X%s\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)PatternLength, L"HALT!");
        LogToFile(LogFile, Log);

        FreePool(Captures);
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

      for (UINTN i = 0; (i + PatternLength <= ImageInfo->ImageSize) && PatchLimit; i++)
      {
        BOOLEAN CResult = FALSE; //Reset result to keep RegexMatch running

        if (!(i % 0x10000)) {
          //PrintConsoleSREP(L"\r0x%X out of 0x%X", &ImageInfo->ImageBase + i, &ImageInfo->ImageBase + ImageInfo->ImageSize);
          DisplayUpdateProgressSREP((UINTN)(((float)i / ImageInfo->ImageSize) * 100) + 1);
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
          RegexMatch((UINT8 *)ImageInfo->ImageBase + i, (CHAR8 *)RegexPatternBuffer, (UINT16)PatternLength, RegularExpressionProtocol, &CResult);
        }
        else
        {
          ProcessedPattern = ApplySimpleMask((UINT8 *)ImageInfo->ImageBase + i, RegexPatternBuffer, PatternLength, '.');

          if (RETURN_ERROR(AsciiStrHexToBytes(ProcessedPattern, PatternLength * sizeof(UINT16), HexPattern, PatternLength)))
          {
            FreePool(ProcessedPattern);
            break;
          }

          CResult = (BOOLEAN)!CompareMem((UINT8 *)ImageInfo->ImageBase + i, (UINT8 *)HexPattern, PatternLength);
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

          if (MatchesCount == SREP_MAX_MATCHES)
          {
            Print(HiiGetStringSREP(STRING_TOKEN(STR_MATCH_LIMIT_REACHED), NULL));
            UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_MATCH_LIMIT_REACHED), NULL));
            LogToFile(LogFile, Log);

            break;
          }
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
      CurrentOffset = Captures[MatchesCount - 1];

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
          FreePool(Captures);

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

    if (Captures != NULL) ZeroMem(Captures, MatchesCount * sizeof(UINT64));
    MatchesCount = 0;

    break;
  }

  if (Captures != NULL) { FreePool(Captures); }
  return *Status;
}
