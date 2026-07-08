#include "Utility.h"

CHAR16 *
FindLoadedImageBuffer(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN UINT8 SectionType,
  IN UINTN SectionInstance,
  OUT UINTN *Size
)
{
    EFI_STATUS Status;
    EFI_GUID *NameGuid = NULL;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    VOID *Buffer = NULL;
    UINTN BufferSize = 0;
    UINT32 AuthenticationStatus;
    //-----------------------------------------------------

    if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL))
    {
        return NULL;
    }

    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

    if (NameGuid == NULL)
    {
        return NULL;
    }

    //
    // Get the Protocol of the device handle that this image was loaded from.
    //
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiFirmwareVolume2ProtocolGuid, (VOID **)&Fv2);

    //
    // FirmwareVolume2Protocol is PI, and is not required to be available.
    //
    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    //
    // Read the user interface section of the image.
    //
    Status = Fv2->ReadSection(Fv2, NameGuid, SectionType, SectionInstance, &Buffer, &BufferSize, &AuthenticationStatus);

    if (EFI_ERROR(Status))
    {
        return NULL;
    }

    *Size = BufferSize;

    //
    // ReadSection returns just the section data, without any section header. For
    // a user interface section, the only data is the file name.
    //
    return Buffer;
}

//Copy from HandleParsingLib
CHAR16 *
LoadedImageProtocolDumpFilePath(
  IN EFI_HANDLE TheHandle
)
{
  EFI_STATUS Status;
  CHAR16 *FilePath = AllocateZeroPool(37 * sizeof(CHAR16));
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  //-----------------------------------------------------

  Status = gBS->OpenProtocol(
                  TheHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **)&LoadedImage,
                  gImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );

  if (EFI_ERROR(Status)){
    DEBUG_CODE(
      //Debug
      PrintConsoleSREP(L"%s: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), Status);
      UnicodeSPrint(Log, GenericBufferSize, u"%s: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_GETTING_PROTOCOL_POINTER), NULL), Status);
      LogToFile(LogFile, Log);
    );

    return NULL;
  }

  StrnCpyS(FilePath, 37, ConvertDevicePathToText(LoadedImage->FilePath, TRUE, TRUE) + 7, 36);

  return FilePath;
}

VOID
SetFilterProtocolForLoadedImageFunction(
  IN OUT EFI_GUID *FilterProtocol
)
{
  *FilterProtocol = CompareGuid(FilterProtocol, &gEfiFirmwareVolume2ProtocolGuid) ? gEfiLoadedImageProtocolGuid : *FilterProtocol;
}

//Copy of LocateFvInstanceWithTables from AcpiPlatform.c
EFI_STATUS
LocateAndLoadFvFromName(
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles = NULL;
    UINTN HandleBufferSize = 0;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
    //-----------------------------------------------------

    //
    // Locate protocol.
    //
    Status = gBS->LocateHandleBuffer(
        ByProtocol,
        &FilterProtocol,
        NULL,
        &HandleBufferSize,
        &Handles
    );
    if (EFI_ERROR(Status))
    {
        PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
        UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
        LogToFile(LogFile, Log);

        return Status;
    }

    DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", HandleBufferSize););

    for (UINTN Index = 0; Index < HandleBufferSize; Index++)
    {

        //
        // Get the protocol on this handle
        // This should not fail because of LocateHandleBuffer
        //

        Status = gBS->HandleProtocol(
            Handles[Index],
            &gEfiFirmwareVolume2ProtocolGuid,
            (VOID **)&FvInstance);

        if (EFI_ERROR(Status)) {
          return EFI_NOT_FOUND;
        }

        EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
        EFI_FV_FILE_ATTRIBUTES FileAttributes;
        UINTN FileSize = 0;
        EFI_GUID NameGuid = {0};
        VOID *Keys = AllocateZeroPool(FvInstance->KeySize);

        while (TRUE)
        {
            FileType = EFI_FV_FILETYPE_ALL;
            ZeroMem(&NameGuid, sizeof(EFI_GUID));
            Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);
            if (EFI_ERROR(Status))
            {
              PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
              break;
            }

            VOID *String;
            UINTN StringSize = 0;
            UINT32 AuthenticationStatus;

            String = NULL;
            Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);
            if (StrCmp(Name, String) == 0)
            {
                DEBUG_CODE(PrintConsoleSREP(L"Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType););

                Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0, (VOID **)Buffer, BufferSize, &AuthenticationStatus);

                FreePool(String);
                FreePool(Keys);
                return Status;
            }
            FreePool(String);
        }

        if (Keys != NULL) {
          FreePool(Keys);
        }
    }

    return EFI_NOT_FOUND;
}

EFI_STATUS
LocateAndLoadFvFromGuid(
  IN EFI_GUID GUID16,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize,
  IN EFI_GUID FilterProtocol
)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles;
  UINTN HandleBufferSize = 0;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
  //-----------------------------------------------------

  //
  // Locate protocol.
  //
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &FilterProtocol,
    NULL,
    &HandleBufferSize,
    &Handles);
  if (EFI_ERROR(Status))
  {
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
    LogToFile(LogFile, Log);

    return Status;
  }

  DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", HandleBufferSize););

  for (UINTN Index = 0; Index < HandleBufferSize; Index++)
  {

    //
    // Get the protocol on this handle
    // This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      Handles[Index],
      &FilterProtocol,
      (VOID**)&FvInstance);

    if (EFI_ERROR(Status)) {
      return EFI_NOT_FOUND;
    }

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize = 0;
    EFI_GUID NameGuid = { 0 };
    VOID *Keys = AllocateZeroPool(FvInstance->KeySize);

    while (TRUE)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      ZeroMem(&NameGuid, sizeof(EFI_GUID));
      Status = FvInstance->GetNextFile(FvInstance, Keys, &FileType, &NameGuid, &FileAttributes, &FileSize);

      if (EFI_ERROR(Status))
      {
        PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
        break;
      }

      VOID *String;
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      Status = FvInstance->ReadSection(FvInstance, &NameGuid, EFI_SECTION_USER_INTERFACE, 0, &String, &StringSize, &AuthenticationStatus);

      DEBUG_CODE(
        //Debug
        PrintConsoleSREP(L"Current GUID: %g\n\r", NameGuid); //Current processing guid per While iteration
      );

      if (CompareGuid(&GUID16, &NameGuid) == 1) //I can do via "&"
      {

        DEBUG_CODE(
          //Debug
          PrintConsoleSREP(L"Found Guid: %g, FileSize: %d, Name: %s, Type: %d\n\r", NameGuid, FileSize, String, FileType);
        );

        Status = FvInstance->ReadSection(FvInstance, &NameGuid, Section_Type, 0, (VOID **)Buffer, BufferSize, &AuthenticationStatus);

        FreePool(String);
        FreePool(Keys);
        return Status;
      }
      FreePool(String);
    }

    if (Keys != NULL) {
      FreePool(Keys);
    }
  }
  return EFI_NOT_FOUND;
}

EFI_STATUS
LoadandRunImage(
  IN EFI_SYSTEM_TABLE *SystemTable,
  IN CHAR16 *FileName,
  OUT EFI_HANDLE *AppImageHandle
)
{
    EFI_HANDLE *Handles = NULL;
    EFI_STATUS Status = EFI_SUCCESS;
    EFI_BLOCK_IO_PROTOCOL *BlkIo = NULL;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    UINTN BufferSize = 0, ExitDataSize;
    //-----------------------------------------------------

    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiSimpleFileSystemProtocolGuid, NULL, &BufferSize, &Handles);

    if (EFI_ERROR(Status))
    {
        return Status;
    }

    for (UINTN Index = 0; Index < BufferSize; Index++)
    {
        Status = gBS->OpenProtocol(
          Handles[Index],
          &gEfiSimpleFileSystemProtocolGuid,
          (VOID **)&BlkIo,
          gImageHandle, NULL,
          EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(Status))
        {
            return Status;
        }

        FilePath = FileDevicePath(Handles[Index], FileName);
        Status = gBS->LoadImage(FALSE, gImageHandle, FilePath, (VOID *)NULL, 0, AppImageHandle);
        if (FilePath != NULL) {
          FreePool(FilePath);
        }

        if (EFI_ERROR(Status))
        {
            continue;
        }

        Status = gBS->StartImage(*AppImageHandle, &ExitDataSize, (CHAR16 **)NULL);
        if (EFI_ERROR(Status))
        {
            return Status;
        }
        break;
    }

    return Status;
}

EFI_STATUS
RegexMatch(
  IN UINT8 *Dump,
  IN CHAR8 *Pattern,
  IN UINT16 Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR16 tmp[0x6] = { 0 };
  CHAR16 StringifiedHexDump16[0x100] = { 0 };
  CHAR16 Pattern16[0x100] = { 0 };
  UINTN CapturesCount = 0;
  //-----------------------------------------------------

  if (Size > GenericBufferSize / sizeof(UINT16))
  {
    Size = (UINT16)GenericBufferSize / sizeof(UINT16);
  }

  for (UINT16 i = 0; i < Size; i++) //Get string from UINT buffer
  {
    UnicodeSPrint(tmp, StrSize(u"XX"), u"%02x", Dump[i]);
    StrCatS(StringifiedHexDump16, sizeof(StringifiedHexDump16) + StrSize(u"\0"), tmp); //+ sizeof(L"\0")?
  }
  UnicodeSPrint(Pattern16, sizeof(Pattern16), L"%a", Pattern);  //Convert Pattern string

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Append result: %r\n\r", Status);
    PrintConsoleSREP(L"Strings Dump/Pattern %s / %s\n\r", &StringifiedHexDump16, &Pattern16);
    INTN m = StrCmp(StringifiedHexDump16, Pattern16);
    PrintConsoleSREP(L"Strings match? : %a\n\r", m ? L"False" : L"True"); //INT m = 0 is yes, any other means they dont
    PrintConsoleSREP(L"Dump16 : %d, Pattern16 : %d\n\r", StrLen(StringifiedHexDump16), StrLen(Pattern16));
  );

  Status = Oniguruma->MatchString(
    Oniguruma,
    StringifiedHexDump16,
    Pattern16,
    NULL,
    CResult,
    NULL,
    &CapturesCount
  );

  return Status;
}

EFI_HII_PACKAGE_LIST_HEADER *
GetHandlePackageList(
  IN CONST EFI_HII_HANDLE HiiHandle
)
{
  EFI_STATUS Status;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN BufferSize = 0;
  //-----------------------------------------------------

  Status = gHiiDatabase->ExportPackageLists(gHiiDatabase, HiiHandle, &BufferSize, PackageList);
  if (EFI_ERROR(Status))
  {
    //Catch not enough memory here, allocate buffer pool
    PackageList = AllocateZeroPool(BufferSize);
    if (PackageList == NULL) {
      return NULL;
    }

    //Try again after allocation
    Status = gHiiDatabase->ExportPackageLists(gHiiDatabase, HiiHandle, &BufferSize, PackageList);
  }

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"HII handle %X ExportPackageLists BufferSize: %X, PackageListGuid: %g, status: %r\n\r", HiiHandle, BufferSize, PackageList->PackageListGuid, Status);
  );

  if (EFI_ERROR(Status))
  {
    if (PackageList != NULL) {
      FreePool(PackageList);
    }
    return NULL;
  }
  else
  {
    return PackageList;
  }
}

EFI_STATUS
GetHiiPackagesByTypeAndGuid(
  IN UINT8 PackageType,
  IN EFI_GUID *PackageGuid,
  OUT UINTN *BufferSize,
  OUT EFI_HII_HANDLE **HiiHandles
)
{
  EFI_STATUS Status;
  EFI_HII_HANDLE *HiiHandleBuffer = NULL;
  UINTN HiiHandleBufferSize = 0;

  //
  //Locate HII database, retrieve all HII packages to HiiHandleBuffer
  //
  Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Locate HiiDb status %r\n\r", Status);
  );

  Status = gHiiDatabase->ListPackageLists(gHiiDatabase, PackageType, PackageGuid, &HiiHandleBufferSize, NULL);

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"HiiHandle instances: %X\n\r", HiiHandleBufferSize / sizeof(EFI_HII_HANDLE));
  );

  if (EFI_ERROR(Status) && Status != EFI_BUFFER_TOO_SMALL)
  {
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NO_HANDLE), NULL), Status);
  }
  else
  {
    //Catch not enough memory here, allocate buffer pool
    HiiHandleBuffer = AllocateZeroPool(HiiHandleBufferSize);
    if (HiiHandleBuffer == NULL) {

      return EFI_OUT_OF_RESOURCES;
    }

    //Try again after allocation
    Status = gHiiDatabase->ListPackageLists(gHiiDatabase, PackageType, PackageGuid, &HiiHandleBufferSize, HiiHandleBuffer);
  }

  *BufferSize = HiiHandleBufferSize;
  *HiiHandles = HiiHandleBuffer;

  return Status;
}

EFI_HII_HANDLE
HiiConstructAddPackagesSREP(
  IN CONST EFI_GUID *PackageListGuid,
  IN EFI_HANDLE DeviceHandle OPTIONAL,
  IN UINT8 *UniBin,
  IN UINT8 *VfrBin
)
{
  CONST EFI_HII_PACKAGE_HEADER mEndOfPackageList = {
    sizeof(EFI_HII_PACKAGE_HEADER),
    EFI_HII_PACKAGE_END
  };

  EFI_STATUS Status;
  EFI_HII_PACKAGE_LIST_HEADER *PackageListHeader = NULL;
  EFI_HII_HANDLE HiiHandle = NULL;
  UINT32 DataLength = 0;
  UINT8 *Data = NULL;
  //-----------------------------------------------------

  ASSERT(PackageListGuid != NULL);

  //
  // Calculate the length of the packages
  //

  //Get PrependedArrSize and substract UINT32 from the value
  DataLength += (ReadUnaligned32((UINT32 *)UniBin) - sizeof (UINT32));
  DataLength += (ReadUnaligned32((UINT32 *)VfrBin) - sizeof (UINT32));
  DEBUG_CODE(
    //Debug
    AsciiPrint("ReadUnaligned Vfr from HiiAddPackages %X\n\r", ReadUnaligned32((UINT32 *)VfrBin) - sizeof(UINT32));
  );

  //
  // If there are no packages or all the packages
  // are empty, then return a NULL HII Handle
  //
  if (DataLength == 0) {
    DEBUG_CODE(
      //Debug
      AsciiPrint("There are no packages or all the packages are empty\n\r");
    );
    return NULL;
  }

  //
  // Add the length of the Package List Header and the terminating Package Header
  //
  DataLength += sizeof(EFI_HII_PACKAGE_LIST_HEADER) + sizeof(EFI_HII_PACKAGE_HEADER);

  //
  // Allocate the storage for the entire Package List
  //
  PackageListHeader = AllocateZeroPool(DataLength);

  //
  // If the Package List can not be allocated, then return a NULL HII Handle
  //
  if (PackageListHeader == NULL) {
    DEBUG_CODE(
      //Debug
      AsciiPrint("The Package List can not be allocated\n");
    );
    return NULL;
  }

  //
  // Fill in the GUID and Length of the Package List Header
  //
  CopyGuid(&PackageListHeader->PackageListGuid, PackageListGuid);
  PackageListHeader->PackageLength = DataLength;

  //
  // Initialize a pointer to the beginning of the Package List data
  //
  Data = (UINT8 *)(PackageListHeader + 1);

  //
  // Copy the data from EACH package
  //
  //Uni
  DataLength = (ReadUnaligned32((UINT32 *)UniBin) - sizeof (UINT32));
  CopyMem(Data, (UINT32 *)UniBin + 1, DataLength);
  //Update pointer
  Data += DataLength;

  //Vfr
  DataLength = (ReadUnaligned32((UINT32 *)VfrBin) - sizeof (UINT32));
  CopyMem(Data, (UINT32 *)VfrBin + 1, DataLength); //Pointer math (UINT32 + 1 = sizeof(UINT32) + sizeof(UINT32))
  //Update pointer
  Data += DataLength;

  DEBUG_CODE(
    //Debug
    AsciiPrint("HiiAddPackages copied %X bytes of Uni, Vfr and Uni must be: 0x%X / 0x%X\n", ReadUnaligned16((UINT16 *)(PackageListHeader + 1)), ReadUnaligned32((UINT32 *)VfrBin), ReadUnaligned32((UINT32 *)UniBin));
  );

  //
  // Append a package of type EFI_HII_PACKAGE_END to mark the end of the package list
  //
  CopyMem(Data, &mEndOfPackageList, sizeof(mEndOfPackageList));

  //
  // Register the package list with the HII Database
  //
  Status = gHiiDatabase->NewPackageList (
                           IN gHiiDatabase,
                           IN PackageListHeader, //PackageListHeader consists of Header + Data
                           IN DeviceHandle,
                           OUT &HiiHandle
                           );

  if (EFI_ERROR(Status)) {
    DEBUG_CODE(
      //Debug
      AsciiPrint("The Package List is null or DeviceHandle is null or %r\n", Status);
    );
    HiiHandle = NULL;
  }

  //
  // Free the allocated package list
  //
  FreePool(PackageListHeader);

  //
  // Return the new HII Handle
  //
  return HiiHandle;
}

EFI_STATUS
GetSetupPointers(
  IN EFI_LOADED_IMAGE_PROTOCOL *PImageInfo,
  OUT UINTN **DevicePathPointer,
  OUT UINTN **ifrSetupStringsPointer,
  OUT UINTN **ifrHiiBinPointer
)
{
  for (UINTN i = 0; i < PImageInfo->ImageSize; i += 1)
  {
    UINTN Offset = (UINTN)PImageInfo->ImageBase + (UINTN)i;

    if (CompareMem((UINT8 *)Offset, (UINT8 *)DevicePathHeaderUint, sizeof(DevicePathHeaderUint)) == 0
        && !(*DevicePathPointer > (UINTN *)0))
    {
      *DevicePathPointer = (UINTN *)(Offset + sizeof(DevicePathHeaderUint));
    }
    if (CompareMem((UINT8 *)Offset, (UINT8 *)ifrSetupStringsPointerUint, sizeof(ifrSetupStringsPointerUint)) == 0
        && !(*ifrSetupStringsPointer > (UINTN *)0))
    {
      *ifrSetupStringsPointer = (UINTN *)(Offset - (sizeof(UINT64) - 1));
    }
    if (CompareMem((UINT8 *)Offset, (UINT8 *)ifrHiiBinPointerHeaderUint, sizeof(ifrHiiBinPointerHeaderUint)) == 0
        && !(*ifrHiiBinPointer > (UINTN *)0))
    {
      *ifrHiiBinPointer = (UINTN *)(Offset - (sizeof(UINT64) - 1));
    }

    if (*DevicePathPointer && *ifrSetupStringsPointer && *ifrHiiBinPointer)
    {
      return EFI_SUCCESS;
    }
  }

  return EFI_NOT_FOUND;
}

EFI_STRING
HiiGetStringSREP(
  IN EFI_STRING_ID StringId,
  IN CONST CHAR8 *Language  OPTIONAL
)
{
  EFI_STATUS Status;
  EFI_STRING String = NULL;
  UINTN SREPLangDataSize = 0x6;
  CHAR8 *PlatformLanguage = AllocatePool(SREPLangDataSize);
  CHAR8 *SupportedLanguages = NULL;
  CHAR8 *BestLanguage = NULL;
  UINTN StringSize = 0;
  CHAR16 TempString;
  //-----------------------------------------------------

  //
  // Get the languages that the package specified by HiiHandle supports
  //
  SupportedLanguages = HiiGetSupportedLanguages(HiiHandleSREP);
  if (SupportedLanguages == NULL) {
    goto Error;
  }

  //
  // Get the current language setting
  //
  Status = gRT->GetVariable(
    L"SREPLang",
    &gEfiSREPLangVariableGuid,
    NULL,
    &SREPLangDataSize,
    PlatformLanguage
  );

  //
  // If Languag is NULL, then set it to an empty string, so it will be
  // skipped by GetBestLanguage() as it won't match with Hii Handle SupportedLanguages
  //
  if (Language == NULL) {
    Language = "";

    //
    // Get the best matching language from SupportedLanguages
    //
    BestLanguage = GetBestLanguage (
                     SupportedLanguages,
                     FALSE,                                             // RFC 4646 mode
                     Language,                                          // Highest priority
                     PlatformLanguage != NULL ? PlatformLanguage : "",  // Next highest priority
                     SupportedLanguages,                                // Lowest priority
                     NULL
                     );
    if (BestLanguage == NULL) {
      //HiiGetSupportedLanguages failed, meaning there's no way we getting resources from Hii Handle
      goto Error;
    }
  }
  else
  {
    BestLanguage = (CHAR8 *)Language;
  }

  //
  // Retrieve the size of the string in the string package for the BestLanguage
  //
  Status = gHiiString->GetString (
            gHiiString,
            BestLanguage,
            HiiHandleSREP,
            StringId,
            &TempString,
            &StringSize,
            NULL
            );
  //
  // If GetString() returns EFI_SUCCESS for a zero size,
  // then there are no supported languages registered for HiiHandle.  If GetString()
  // returns an error other than EFI_BUFFER_TOO_SMALL, then HiiHandle is not present
  // in the HII Database
  //
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Error;
  }

  //
  // Allocate a buffer for the return string
  //
  String = AllocateZeroPool (StringSize);
  if (String == NULL) {
    goto Error;
  }

  //
  // Retrieve the string from the string package
  //
  Status = gHiiString->GetString (
            gHiiString,
            BestLanguage,
            HiiHandleSREP,
            StringId,
            String,
            &StringSize,
            NULL
            );
  if (EFI_ERROR (Status)) {
    //
    // Free the buffer and return NULL if the supported languages can not be retrieved.
    //
    FreePool (String);
    String = NULL;
  }
  
Error:
  //
  // Free allocated buffers
  //
  if (SupportedLanguages != NULL) {
    FreePool(SupportedLanguages);
  }
  if (PlatformLanguage != NULL) {
    FreePool(PlatformLanguage);
  }
  if (BestLanguage != NULL) {
    FreePool(BestLanguage);
  }

  //
  // Return the Null-terminated Unicode string
  //
  return String;
}

//Copy from DisplayUpdateProgressLib
EFI_STATUS
DisplayUpdateProgressSREP(
  IN UINTN Completion
)
{
  UINTN Index = 0;
  UINTN CurrentAttribute = (UINTN)gST->ConOut->Mode->Attribute;;

  //
  // Check range
  //
  if (Completion > 100) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Print progress percentage
  //
  PrintConsoleSREP(L"\r %3d%% ", Completion);

  //
  // Set progress bar color
  //
  gST->ConOut->SetAttribute (
                 gST->ConOut,
                 EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK)
                 );

  //
  // Print completed portion of progress bar
  //
  for (; Index < Completion / 2; Index++){
    PrintConsoleSREP(L"%c", BLOCKELEMENT_FULL_BLOCK);
  }

  //
  // Restore text color
  //
  gST->ConOut->SetAttribute(gST->ConOut, CurrentAttribute);

  //
  // Print remaining portion of progress bar
  //
  for (; Index < 50; Index++) {
    PrintConsoleSREP(L"%c", BLOCKELEMENT_LIGHT_SHADE);
  }

  return EFI_SUCCESS;
}

CHAR8 *
ApplySimpleMask(
  IN UINT8 *SourceBuffer, //Accepts UINT8 byte buffer only
  IN CHAR8 *MaskBuffer,
  IN UINTN MaskLengthBytes,
  IN CHAR8 Metacharacter
)
{
  if ((SourceBuffer == NULL) || (MaskBuffer == NULL)) {
    return NULL;
  }

  CHAR8 tmp[0x3] = { 0 };
  CHAR8 StringifiedHexDump[0x200] = { 0 };
  CHAR8 *Result = AllocateZeroPool(MaskLengthBytes);

  for (UINT16 i = 0; i < MaskLengthBytes; i++) //Get string from UINT buffer
  {
    AsciiSPrint(tmp, AsciiStrSize("XX"), "%02x", SourceBuffer[i]);
    AsciiStrCatS(StringifiedHexDump, sizeof(StringifiedHexDump) + AsciiStrSize('\0'), tmp);
  }

  for (UINTN i = 0; i < MaskLengthBytes * sizeof(UINT16); i++)
  {
    if (MaskBuffer[i] == Metacharacter) //e.g. 0x2E is (CHAR8)'.'
    {
      Result[i] = StringifiedHexDump[i];
      continue;
    }

    Result[i] = MaskBuffer[i];
  }

  if (StringifiedHexDump != NULL) {
    FreePool(StringifiedHexDump);
  }

  return Result;
}

//Prints those 16 symbols wide dumps
VOID
PrintDump(
  IN UINT64 Size,
  IN UINT8 *Dump
)
{
    for (UINT64 i = 0; i < Size; i++)
    {
        if (i % 0x10 == 0)
        {
            UnicodeSPrint(Log, GenericBufferSize, u"%a", "\n\t");
            LogToFile(LogFile, Log);
        }
        UnicodeSPrint(Log, GenericBufferSize, u"%02x", Dump[i]);
        LogToFile(LogFile, Log);
    }
    UnicodeSPrint(Log, GenericBufferSize, u"%a", "\n\t");
    LogToFile(LogFile, Log);
}

//Writes string from the buffer to SREP.log
VOID
LogToFile(
  IN EFI_FILE *LogFile,
  IN CHAR16 *String
)
{
    if (LogFile == NULL) { return; }

    UINTN Size = StrLen(String) * 2;        //Size variable equals to the string size, multiplies by 2 cuz Unicode is CHAR16
    LogFile->Write(LogFile, &Size, String); //EFI_FILE_WRITE (Filename, size, the actual string to write)
    LogFile->Flush(LogFile);                //Flushes all modified data associated with a file to a device
}

EFI_STATUS
GetExt1ImageBufferFromSREP(
  IN UINT32 PackageType,
  IN UINT16 PackageId,
  OUT UINT8 **PackageBuffer,
  OUT UINT32 **PackageSize
)
{
  EFI_STATUS Status;
  EFI_HII_PACKAGE_HEADER *PackageHeader = NULL;
  UINT8 *Package = NULL;
  //-----------------------------------------------------


  EFI_HII_PACKAGE_LIST_HEADER *HiiPackageList = GetHandlePackageList(HiiHandleSREP);
  if (HiiPackageList == NULL) {
    return EFI_NOT_FOUND;
  }

  //
  //In HII package list, find IMAGES package
  //
  UINT8 ID = 1;
  UINT32 Offset = sizeof(EFI_HII_PACKAGE_LIST_HEADER); // + 0x14
  UINT32 PackageListLength = ReadUnaligned32(&HiiPackageList->PackageLength);
  while (Offset < PackageListLength) {
    Package = (UINT8 *)HiiPackageList + Offset;
    PackageHeader = (EFI_HII_PACKAGE_HEADER *)Package;

    if (PackageHeader->Type == EFI_HII_PACKAGE_IMAGES){
      break;
    }

    Offset += PackageHeader->Length;
  }
  FreePool(HiiPackageList);

  //
  //Return if IMAGES package not found
  //
  if (PackageHeader->Type != EFI_HII_PACKAGE_IMAGES)
  {
    *PackageBuffer = NULL;

    return EFI_NOT_FOUND;
  }

  //
  //Go through all images, get pointer to image with index bigger than 1
  //
  Package += sizeof(EFI_HII_IMAGE_PACKAGE_HDR) + sizeof(EFI_HII_IMAGE_BLOCK) + sizeof(UINT32); //0xC + 0x1 + 0x4
  while ((ID != PackageId) && (ID < BIT4)) {
    if (*(Package + ReadUnaligned32((UINT32 *)Package - 1)) != EFI_HII_GIBT_EXT1) break;

    Package = Package + ReadUnaligned32((UINT32 *)Package - 1) + sizeof(EFI_HII_IMAGE_BLOCK) + sizeof(UINT32);

    ID++;
  }

  //Save data and return
  if (ID == PackageId) {
    *PackageBuffer = Package;
    *PackageSize = (UINT32 *)Package - 1;

    DEBUG_CODE(
      //Debug
      Print(L"Start byte / size: %X / 0x%X\n\r", *Package, *((UINT32 *)Package - 1));
    );
    Status = EFI_SUCCESS;
  }
  else
  {
    *PackageBuffer = NULL;

    Status = EFI_NOT_FOUND;
  }

  return Status;
}

EFI_STATUS
ExecImageEmbeddedInSREP(
  IN EFI_IMAGE_ID ImageId
)
{
  EFI_STATUS Status;
  EFI_HANDLE NewImageHandle = NULL;
  UINT8 *Package = NULL;
  UINT32 *PackageSize = NULL;
  //-----------------------------------------------------

  Status = GetExt1ImageBufferFromSREP(EFI_HII_PACKAGE_IMAGES, ImageId, &Package, &PackageSize);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  //
  //Prepare Device Path
  //
  MEMMAP_DEVICE_PATH *MemMapPath;
  EFI_DEVICE_PATH_PROTOCOL *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL *PathWithEndNode;

  DevicePath = AllocateZeroPool(sizeof(MEMMAP_DEVICE_PATH) + sizeof(EFI_DEVICE_PATH_PROTOCOL));
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  {
    MemMapPath = (MEMMAP_DEVICE_PATH *)DevicePath;
    MemMapPath->Header.Type = HARDWARE_DEVICE_PATH;
    MemMapPath->Header.SubType = HW_MEMMAP_DP;
    SetDevicePathNodeLength(&MemMapPath->Header, sizeof(MEMMAP_DEVICE_PATH));
    MemMapPath->MemoryType = EfiBootServicesData;
    MemMapPath->StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Package;
    MemMapPath->EndingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Package + *PackageSize;
  }

  //Append Device Path End node
  PathWithEndNode = AppendDevicePathNode(DevicePath, NULL);
  FreePool(DevicePath);

  DevicePath = PathWithEndNode;
  Status = gBS->LoadImage(FALSE, gImageHandle, DevicePath, Package, *PackageSize, &NewImageHandle);
  FreePool(PathWithEndNode);
  
  if (!EFI_ERROR(Status))
  {
    Status = gBS->StartImage(NewImageHandle, (UINTN *)NULL, (CHAR16 **)NULL);
  }

  return Status;
}

//Copy from String
BOOLEAN
AsciiIsHexaDecimalDigitCharacter(
  IN CHAR8 Char
)
{
  return (BOOLEAN)((Char >= '0' && Char <= '9') ||
                   (Char >= 'A' && Char <= 'F') ||
                   (Char >= 'a' && Char <= 'f'));
}

BOOLEAN
AsciiIsHexString(
  IN CHAR8 *String
)
{
  if ((String == NULL) || (*String == '\0')) {
    return FALSE;
  }

  UINTN Index = 0;
  for (; (String[Index] != '\0') && (Index < MAX_UINT8); Index++) {
    if (AsciiIsHexaDecimalDigitCharacter(String[Index])){
      continue;
    }
    return FALSE;
  }

  return (BOOLEAN)Index > 0;
}

//Unused
UINT8 *
FindBaseAddressFromName(
  IN const CHAR16 *Name,
  IN EFI_GUID FilterProtocol
)
{
    EFI_STATUS Status;
    EFI_HANDLE *Handles = NULL;
    UINTN BufferSize = 0;
    //-----------------------------------------------------

    gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, NULL);
    Handles = AllocateZeroPool(BufferSize);

    gBS->LocateHandle(ByProtocol, &FilterProtocol, NULL, &BufferSize, Handles);
    DEBUG_CODE(PrintConsoleSREP(L"Found %d Instances\n\r", BufferSize););

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImageProtocol = NULL;
    for (UINTN i = 0; i < BufferSize / sizeof(EFI_HANDLE); i++)
    {
        Status = gBS->HandleProtocol(Handles[i], &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImageProtocol);  //Process every found handle
        if (Status == EFI_SUCCESS)
        {
            CHAR16 *String = FindLoadedImageBuffer(LoadedImageProtocol, EFI_SECTION_USER_INTERFACE, 0, (VOID *)NULL);
            if (String != NULL)
            {
                if (StrCmp(Name, String) == 0)  //If SUCCESS, compare the handle' name with the one specified in cfg
                {
                    DEBUG_CODE(PrintConsoleSREP(L"Found %s at Address 0x%X\n\r", String, LoadedImageProtocol->ImageBase););
                    return LoadedImageProtocol->ImageBase;
                }
            }
        }
    }
    return NULL;
}

//Unused, unfinished
UINTN
FindFvImageBufferSizeFromLoadedImage(
  IN EFI_LOADED_IMAGE_PROTOCOL *LoadedImage,
  IN EFI_GUID FilterProtocol
)
{
    EFI_GUID *NameGuid;
    EFI_STATUS Status;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *Fv2 = NULL;
    VOID *Buffer;
    UINTN BufferSize;
    UINT32 AuthenticationStatus;
    //-----------------------------------------------------

    if ((LoadedImage == NULL) || (LoadedImage->FilePath == NULL))
    {
        return 0;
    }

    NameGuid = EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);

    if (NameGuid == NULL)
    {
        return 0;
    }

    //
    // Get the FirmwareVolume2Protocol of the device handle that this image was loaded from.
    //
    Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &FilterProtocol, (VOID **)&Fv2);

    //
    // FirmwareVolume2Protocol is PI, and is not required to be available.
    //
    if (EFI_ERROR(Status))
    {
        return 0;
    }

    //
    // Read the user interface section of the image.
    //
    Buffer = NULL;
    Status = Fv2->ReadSection(Fv2, NameGuid, EFI_SECTION_PE32, 0, &Buffer, &BufferSize, &AuthenticationStatus);

    if (EFI_ERROR(Status))
    {
        return 0;
    }

    //
    // ReadSection returns just the section data, without any section header. For
    // a user interface section, the only data is the file name.
    //
    return BufferSize;
}
