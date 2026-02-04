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
    UINTN NumberOfHandles = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    CHAR16 FileName16[0x100] = {0};
    UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(NumberOfHandles * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, Handles);
        DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););
    }

    for (UINTN i = 0; i < NumberOfHandles; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageFileNameSREP(*ImageInfo, FilterProtocol);
            if (String != NULL)
            {
                if (StrCmp(FileName16, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    DEBUG_CODE(Print(L"Found %s at Address 0x%X\n\r", String, (*ImageInfo)->ImageBase););
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
    UINTN NumberOfHandles = 0;
    EFI_HANDLE *Handles;
    //-----------------------------------------------------

    EFI_GUID GUID = { 0 }; //Initialize beforehand
    Status = AsciiStrToGuid(FileGuid, &GUID);
    if (EFI_ERROR(Status))
    {
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      return Status;
    }

    UINT8 *Buffer = NULL;
    UINTN BufferSize = 0;
    Status = LocateAndLoadFvFromGuid(GUID, Section_Type, &Buffer, &BufferSize, FilterProtocol); //Pass the converted guid to Utility to get the target BufferSize

    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, NULL);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        Handles = AllocateZeroPool(NumberOfHandles * sizeof(EFI_HANDLE));
        Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &NumberOfHandles, Handles);
        DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););
    }

    if (Section_Type == EFI_SECTION_TE) {
      if (BufferSize != 0) {
        Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_TE_HINT), NULL), BufferSize);
      }
    }

    for (UINTN i = 0; i < NumberOfHandles; i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo); //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            UINTN FoundSize = FindLoadedImageBufferSize(*ImageInfo, FilterProtocol); //Read ImageInfo, so we get BufferSize from Handles[i]

            DEBUG_CODE(
              //Debug
              Print(L"BufSize from Utility.c: %d\n\r", FoundSize);
              Print(L"Target BufSize: %d\n\r", BufferSize);
            );

            if (FoundSize != 0)
            {
                if (FoundSize == BufferSize)  //Compare the found handle' BufferSize with the value we got from FV by looking for guid specified in cfg
                {
                    Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_LOADED_GUID), NULL), BufferSize);
                    return EFI_SUCCESS;
                }
            }
        }
    }
    return EFI_NOT_FOUND;
}

EFI_STATUS
FindLoadedImageFromShellIndex(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *HandleIndex,
  OUT EFI_LOADED_IMAGE_PROTOCOL **ImageInfo,
  OUT EFI_HANDLE **HandleBuffer
)
{
    EFI_STATUS Status;
    UINTN NumericIndex;
    EFI_HANDLE TheHandle;
    //-----------------------------------------------------

    Status = AsciiStrHexToUintnS(HandleIndex, (CHAR8 **)L"\0", &NumericIndex);
    if (EFI_ERROR(Status))
    {
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), HandleIndex);
      return Status;
    }
    Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_HANDLE_INDEX), NULL), NumericIndex);

    EFI_HANDLE *AllHandles = GetHandleListByProtocol(NULL);
    if (AllHandles == NULL)
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_GET_HANDLE_LIST_FAIL), NULL));
      return EFI_NOT_FOUND;
    }

    //Save handles to buffer
    *HandleBuffer = AllHandles;

    TheHandle = ConvertHandleIndexToHandle(NumericIndex);
    if (TheHandle != NULL)
    {
      Print(L"\n\r%X: 0x%X\n\r", NumericIndex, TheHandle);
      return gBS->HandleProtocol(TheHandle, &gEfiLoadedImageProtocolGuid, (VOID **)ImageInfo);
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
    UINTN NumberOfHandles;
    UINTN Index;
    EFI_HANDLE *SFS_Handles;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo = NULL;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &NumberOfHandles, &SFS_Handles);

    if (EFI_ERROR(Status))
    {
        Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NO_HANDLE_WITH_EFISIMPLEFS), NULL));
        return Status;
    }
    DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););

    for (Index = 0; Index < NumberOfHandles; Index++)
    {
        Status = gBS->OpenProtocol(
            SFS_Handles[Index], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&BlkIo,
            ImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(Status))
        {
            Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_UNSUPPORTED_PROTOCOL), NULL), Status);
            return Status;
        }
        CHAR16 FileName16[0x100] = {0};
        UnicodeSPrint(FileName16, sizeof(FileName16), L"%a", FileName);
        FilePath = FileDevicePath(SFS_Handles[Index], FileName16);
        Status = gBS->LoadImage(FALSE, ImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);

        if (EFI_ERROR(Status))
        {
            Print(L"%s%d: %r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_COULD_NOT_LOAD), NULL), Index, Status);
            continue;
        }
        else
        {
            Print(L"%s%d\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SUCCESSFUL_LOAD), NULL), Index);
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
    if (Buffer != NULL)
        FreePool(Buffer);
    if (!EFI_ERROR(Status))
    {
        Status = gBS->OpenProtocol(*AppImageHandle, &gEfiLoadedImageProtocolGuid,
                                   (VOID **)ImageInfo, ImageHandle, (VOID *)NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    }
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

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
    Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
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
    Print(L"%s%d\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NULL_DRIVER_BUFFER), NULL), BufferSize);
    return EFI_NOT_FOUND;
  }

  CHAR16 DFName[41] = { 0 };                                  //36 chars for guid, 4 for the ".bin" ext and 1 for null-terminator
  UnicodeSPrint(DFName, sizeof(DFName), L"%a.bin", FileGuid); //Append ".bin"
  Root->Open(Root, &DumpFile, DFName, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
  DumpFile->Write(DumpFile, &BufferSize, (VOID **)Buffer);
  DumpFile->Flush(DumpFile);

  Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_DUMPING), NULL));

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
  Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);

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
    UINTN NumberOfHandles;
    UINT32 Authentication;
    UINTN i,j;
    VOID *SectionData = NULL;
    UINTN DataSize;
    EFI_SECTION_TYPE SectionType;
    EFI_FIRMWARE_VOLUME_PROTOCOL *Fv = NULL;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_DEFAULT_FILTER), NULL));
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
      Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
      return Status;
    }

    if (SectionGuid != NULL)
    {
      Status = AsciiStrToGuid(SectionGuid, &Section);
      if (EFI_ERROR(Status))
      {
        Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), FileGuid);
        return Status;
      }
    }

    // Locate the Firmware volume protocol
    if (!CompareGuid(&FilterProtocol, &gEfiFirmwareVolumeProtocolGuid)) {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolume2ProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer
      );
    }
    else
    {
      Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &gEfiFirmwareVolumeProtocolGuid,
        NULL,
        &NumberOfHandles,
        &HandleBuffer
      );
    }

    if (EFI_ERROR(Status))
    {
      Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);

      return Status;
    }

    DEBUG_CODE(Print(L"Found %d Instances\n\r", NumberOfHandles););

    SectionType = (SectionGuid == NULL) ? EFI_SECTION_RAW : EFI_SECTION_FREEFORM_SUBTYPE_GUID;

    //Find and read raw data files.
    for (i = 0; i < NumberOfHandles; i++) {
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
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_RETRIEVE_FREEFORM), NULL));
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
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_DUMPING), NULL));
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
Exec(IN EFI_HANDLE *AppImageHandle)
{
    UINTN ExitDataSize;
    EFI_STATUS Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);

    return Status;
}

EFI_STATUS
UninstallProtocol(IN CHAR8 *ProtocolGuid, OUT UINTN Indexes)
{
   EFI_STATUS Status;
   EFI_HANDLE *HandlesBuffer;
   UINTN NumberOfHandles = 0;
   VOID *ProtocolInterface;
   //-----------------------------------------------------

   EFI_GUID GUID = { 0 };
   Status = AsciiStrToGuid(ProtocolGuid, &GUID);
   if (EFI_ERROR(Status))
   {
     Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), ProtocolGuid);
     return Status;
   }

   Status = gBS->LocateHandleBuffer(ByProtocol, &GUID, NULL, &NumberOfHandles, &HandlesBuffer);

  if (EFI_ERROR(Status))
  {
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NO_HANDLE), NULL), Status);

    return Status;
  } 
  
  for(UINTN i = 0; i < NumberOfHandles; i += 1)
  {
    ProtocolInterface = NULL;
    Status = gBS->HandleProtocol(HandlesBuffer[i],
                                 &GUID,
                                 (VOID **)&ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      Print(L"%s%d: %r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), i, Status);
      continue;
    }

    Status = gBS->UninstallProtocolInterface(HandlesBuffer[i],
                                             &GUID, 
                                             ProtocolInterface);

    if (EFI_ERROR(Status))
    {
      Print(L"%r - %s%g,\n    Handle %d\n    Pointer 0x%x\n\r", Status, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_UNINSTALLING), NULL), GUID, i, ProtocolInterface);
      continue;
    }
    Indexes += 1;
  }
  return Status;
}

//Unused
UINTN
GetAptioHiiDB(IN BOOLEAN BufferSizeOrPointer)
{
  typedef struct {
    UINT32 DataSize;
    UINT32 DataPointer;
  } HiiDbBlock_DATA;

  EFI_STATUS Status = EFI_SUCCESS;
  EFI_GUID ExportDatabaseGuid = { 0x1b838190, 0x4625, 0x4ead, {0xab, 0xc9, 0xcd, 0x5e, 0x6a, 0xf1, 0x8f, 0xe0} };
  UINTN Size = 8;
  HiiDbBlock_DATA HiiDB; //The whole var is 8 bytes, I save it into 2 by 4
  UINTN BufferSize = 0, Pointer = 0, Result = 0;
  //-----------------------------------------------------

  Status = gRT->GetVariable(
      L"HiiDB",
      &ExportDatabaseGuid,
      NULL,
      &Size,
      &HiiDB);

  if (Status == EFI_SUCCESS && Size != 0)
  {
    //Get buffers from pointer, source called with &
    CopyMem(&BufferSize, &HiiDB.DataSize, Size / 2);
    CopyMem(&Pointer, &HiiDB.DataPointer, Size / 2);

    DEBUG_CODE(
      //Debug
      Print(L"\nPart1:\n\r");
      Print(L"%x\n\r", BufferSize);
      Print(L"\nPart2:\n\r");
      Print(L"%x\n\r", Pointer);
    );

    BufferSizeOrPointer ? (Result = Pointer) : (Result = BufferSize);
    return Result;
  }
  return 0;
}

EFI_STATUS
UpdateHandlePackageList(
  IN EFI_HANDLE ImageHandle,
  IN CHAR8 *PackageGuid,
  IN CHAR8 *PackageSize OPTIONAL,
  OUT EFI_LOADED_IMAGE_PROTOCOL *ImageInfo
)
{
  #define MAX_UINT28 ((UINT32)0xFFFFFFF)
  EFI_STATUS Status;
  EFI_HANDLE *DriverHandleBuffer = NULL;
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN BufferSize = 0;
  UINTN DriverHandleNumber = 0;

  EFI_GUID GUID = { 0 };
  Status = AsciiStrToGuid(PackageGuid, &GUID);
  if (EFI_ERROR(Status))
  {
    Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), PackageGuid);
    return Status;
  }

  //
  //Find the required Driver Handle out of all by comparing ImageInfo
  //
  Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleNumber, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL)
  {
    DriverHandleBuffer = AllocateZeroPool(DriverHandleNumber * sizeof(EFI_HANDLE));
    Status = gBS->LocateHandle(ByProtocol, &gEfiLoadedImageProtocolGuid, NULL, &DriverHandleNumber, DriverHandleBuffer);
  }

  DEBUG_CODE(
    //Debug
    Print(L"Locate driver handles %X status: %r\n\r", DriverHandleNumber, Status);
  );


  //
  //Locate HII database, retrieve all HII packages to HiiHandleBuffer
  //
  Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    Print(L"Unable to locate HII Database!\n\r");
    return EFI_NOT_STARTED;
  }

  DEBUG_CODE(
    //Debug
    Print(L"Locate HiiDb status %r\n\r", Status);
  );

  Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, HiiHandleBuffer);

  DEBUG_CODE(
    //Debug
    Print(L"HiiHandle instances: %X\n\r", BufferSize / sizeof(EFI_HII_HANDLE));
  );

  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL)
  {
    Print(L"Failed to list HII handles!\n\r");
    return Status;
  }
  else
  {
    //Catch not enough memory here, allocate buffer pool
    HiiHandleBuffer = AllocateZeroPool(BufferSize);
    if (HiiHandleBuffer == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    //Try again after allocation
    Status = gHiiDatabase->ListPackageLists(gHiiDatabase, EFI_HII_PACKAGE_TYPE_ALL, NULL, &BufferSize, HiiHandleBuffer);
  }
  if (EFI_ERROR(Status))
  {
    return Status;
  }
  DEBUG_CODE(
    //Debug
    Print(L"ListPackageLists status: %r\n\r", Status);
  );

  //
  //Try retrieving package list in the HII database by guid
  //
  for (UINTN i = 0; i < BufferSize / sizeof(EFI_HII_HANDLE); i++) {
    PackageList = GetHandlePackageList(HiiHandleBuffer[i]);
    DEBUG_CODE(
      //Debug
      Print(L"Found PackageList: %g of size 0x%X at 0x%X\n\r", PackageList->PackageListGuid, PackageList->PackageLength, PackageList);
     );

    if (CompareGuid(&GUID, &PackageList->PackageListGuid)) {
      ImageInfo->ImageBase = PackageList + (UINTN)0x14; //sizeof(EFI_HII_PACKAGE_LIST_HEADER)
      ImageInfo->ImageSize = PackageList->PackageLength;
    }
  }

  if (ImageInfo->ImageBase == 0) {
    return EFI_NOT_FOUND;
  }
  else
  {
    if (PackageSize != 0) {
      UINTN PackageSizeUintn = 0;
      AsciiStrHexToUintnS(PackageSize, NULL, &PackageSizeUintn);
      PackageSizeUintn = PackageSizeUintn > MAX_UINT28 ? MAX_UINT28 : PackageSizeUintn;

      ImageInfo->ImageSize = PackageSizeUintn;
    }
  }
  
  if (DriverHandleBuffer != NULL) {
    FreePool(DriverHandleBuffer);
    FreePool(HiiHandleBuffer);
  }

  return EFI_SUCCESS;
}
