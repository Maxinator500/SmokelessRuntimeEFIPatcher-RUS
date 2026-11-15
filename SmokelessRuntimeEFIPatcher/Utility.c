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
  //Get the Protocol of the device handle that this image was loaded from.
  //
  Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &FvProtocolGuid, (VOID **)&Fv2);

  //
  //FirmwareVolume2Protocol is PI, and is not required to be available.
  //
  if (EFI_ERROR(Status))
  {
    return NULL;
  }

  //
  //Read the user interface section of the image.
  //
  Status = Fv2->ReadSection(Fv2, NameGuid, SectionType, SectionInstance, &Buffer, &BufferSize, &AuthenticationStatus);

  if (EFI_ERROR(Status))
  {
    return NULL;
  }

  *Size = BufferSize;

  //
  //ReadSection returns just the section data, without any section header. For
  //a user interface section, the only data is the file name.
  //
  return Buffer;
}

//Copy from HandleParsingLib
EFI_GUID *
EfiHandleDumpFilePath(
  IN CONST EFI_HANDLE TheHandle
)
{
  EFI_STATUS Status;
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

  return EfiGetNameGuidFromFwVolDevicePathNode((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)LoadedImage->FilePath);
}

//Copy of LocateFvInstanceWithTables from AcpiPlatform.c
EFI_STATUS
LocateAndLoadFromFV(
  IN EFI_GUID *FileGuid OPTIONAL,
  IN CHAR16 *Name,
  IN EFI_SECTION_TYPE Section_Type,
  OUT UINT8 **Buffer,
  OUT UINTN *BufferSize
)
{
  EFI_STATUS Status;
  EFI_HANDLE *Handles = NULL;
  EFI_FIRMWARE_VOLUME2_PROTOCOL *FvInstance = NULL;
  UINTN HandleBufferSize = 0;
  UINT8 VolIndex = 0;
  //-----------------------------------------------------

  //
  //Locate protocol.
  //
  Status = gBS->LocateHandleBuffer(
    ByProtocol,
    &FvProtocolGuid,
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

  DEBUG_CODE(PrintConsoleSREP(L"Found %d FVs\n\r", HandleBufferSize););

  for (UINTN Index = 0; Index < HandleBufferSize; Index++)
  {

    //
    //Get the protocol on this handle
    //This should not fail because of LocateHandleBuffer
    //

    Status = gBS->HandleProtocol(
      Handles[Index],
      &FvProtocolGuid,
      (VOID **)&FvInstance
    );

    if (EFI_ERROR(Status)) {
      FreePool(Handles);
      return EFI_NOT_FOUND;
    }

    EFI_FV_FILETYPE FileType = EFI_FV_FILETYPE_ALL;
    EFI_FV_FILE_ATTRIBUTES FileAttributes;
    UINTN FileSize;
    EFI_GUID NameGuid = gZeroGuid;
    VOID *Keys = AllocateZeroPool(FvInstance->KeySize);
    if (Keys == NULL) {
      FreePool(Handles);
      return EFI_OUT_OF_RESOURCES;
    }

    while (TRUE)
    {
      FileType = EFI_FV_FILETYPE_ALL;
      NameGuid = gZeroGuid;
      Status = FvInstance->GetNextFile(
        FvInstance,
        Keys,
        &FileType,
        &NameGuid,
        &FileAttributes,
        &FileSize
      );
      if (EFI_ERROR(Status))
      {
        PrintConsoleSREP(L"%s%d\r", HiiGetStringSREP(STRING_TOKEN(STR_PROCESSED_VOLUMES), NULL), ++VolIndex);

        break;
      }

      VOID *String;
      UINTN StringSize = 0;
      UINT32 AuthenticationStatus;
      String = NULL;
      FvInstance->ReadSection(
        FvInstance,
        &NameGuid,
        EFI_SECTION_USER_INTERFACE,
        0,
        &String,
        &StringSize,
        &AuthenticationStatus
      );

      if (FileGuid != NULL) {
        if (CompareGuid(FileGuid, &NameGuid))
        {
          Name = (CHAR16 *)String;
          goto MATCH;
        }
        continue;
      }

      MATCH:
      if (StrCmp(String, Name) == 0)
      {
        PrintConsoleSREP(L"%s%g / 0x%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_IMAGE_GUIDNSIZE), NULL), NameGuid, FileSize);
        UnicodeSPrint(Log, GenericBufferSize, u"%s%g / 0x%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_IMAGE_GUIDNSIZE), NULL), NameGuid, FileSize);
        LogToFile(LogFile, Log);

        Status = FvInstance->ReadSection(
          FvInstance,
          &NameGuid,
          Section_Type,
          0,
          (VOID **)Buffer,
          BufferSize,
          &AuthenticationStatus
        );

        FreePool(Handles);
        FreePool(String);
        FreePool(Keys);
        return Status;
      }
      if (String != NULL) {
        FreePool(String);
      }
    }
    FreePool(Keys); 
  }

  FreePool(Handles);
  return EFI_NOT_FOUND;
}

EFI_STATUS
RegexMatch(
  IN UINT8 *Dump,
  IN CHAR8 *Pattern,
  IN UINTN Size,
  IN EFI_REGULAR_EXPRESSION_PROTOCOL *Oniguruma,
  OUT BOOLEAN *CResult
)
{
  EFI_STATUS Status = EFI_SUCCESS;
  CHAR16 tmp[3];
  CHAR16 StringifiedHexDump16[SREP_HEX_BUFFER_CHARS];
  CHAR16 Pattern16[SREP_HEX_BUFFER_CHARS];
  UINTN CapturesCount = 0;
  //-----------------------------------------------------

  Size = Size > GenericBufferSize ? GenericBufferSize : Size;
  StringifiedHexDump16[0] = L'\0';

  for (UINTN i = 0; i < Size; i++) //Get string from UINT buffer
  {
    UnicodeSPrint(tmp, sizeof(tmp), u"%02x", Dump[i]);
    Status = StrCatS(StringifiedHexDump16, ARRAY_SIZE(StringifiedHexDump16), tmp); //DestMax counts CHAR16, not bytes
  }
  UnicodeSPrint(Pattern16, sizeof(Pattern16), L"%a", Pattern);  //Convert Pattern string

  DEBUG_CODE(
    //Debug
    PrintConsoleSREP(L"Append result: %r\n\r", Status);
    PrintConsoleSREP(L"Strings Dump/Pattern %s / %s\n\r", &StringifiedHexDump16, &Pattern16);
    INTN Cmp = StrCmp(StringifiedHexDump16, Pattern16);
    PrintConsoleSREP(L"Strings match? : %a\n\r", Cmp ? L"False" : L"True"); //Cmp = 0 is yes, any other means they dont
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

EFI_STATUS
GetLoadedImagesFromHiiHandles(
  IN EFI_HII_HANDLE *HiiHandles,
  IN UINTN HandlesCount,
  OUT EFI_LOADED_IMAGE_PROTOCOL ***ImageInfo
)
{
  EFI_STATUS Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    return EFI_UNSUPPORTED;
  }

  EFI_LOADED_IMAGE_PROTOCOL **TempImageInfo = AllocateZeroPool(HandlesCount * sizeof(UINTN));
  UINT8 **DevicePathInfo = AllocateZeroPool(HandlesCount * sizeof(UINTN));
  if (TempImageInfo == NULL) { return EFI_OUT_OF_RESOURCES; }

  EFI_HANDLE DriverHandle;
  for (UINTN i = 0; i < HandlesCount; i++) {
    Status = gHiiDatabase->GetPackageListHandle(
      gHiiDatabase,
      HiiHandles[i],
      &DriverHandle
    );
    if (EFI_ERROR(Status)) {
      DEBUG_CODE(
        //Debug
        Print(L"GetPackageListHandle status %r\n\n\r", Status);
      );
      break;
    }

    Status = gBS->HandleProtocol(
      DriverHandle,
      &gEfiDevicePathProtocolGuid,
      (VOID **)&DevicePathInfo[i]
    );
    if (EFI_ERROR(Status)) {
      DEBUG_CODE(
        //Debug
        Print(L"\nHandleProtocol status %r\n\n\r", Status);
      );
    }


    EFI_GUID FvFileName;
    //FvFileName = *(EFI_GUID *)(DevicePathInfo[i] + sizeof(EFI_DEVICE_PATH_PROTOCOL) + sizeof(UINT32) + sizeof(EFI_GUID));
    //First comes FW_VOL_DEVICE_PATH, then FW_VOL_FILEPATH_DEVICE_PATH, then VENDOR_DEVICE_PATH
    //Print(GetProtocolInformationDump(DriverHandle, &gEfiDevicePathProtocolGuid, TRUE));

    //Better impl to get FvFileName?
    VOID *Node = DevicePathInfo[i];
    FvFileName = gZeroGuid;
    while (!IsDevicePathEnd(Node)) {
      if (DevicePathType(Node) == MEDIA_DEVICE_PATH &&
        DevicePathSubType(Node) == MEDIA_PIWG_FW_FILE_DP) {
        FvFileName = ((MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node)->FvFileName;
        break;
      }
      Node = NextDevicePathNode(Node);
    }

    if (CompareGuid(&FvFileName, &gZeroGuid)) {
      DEBUG_CODE(
        //Debug
        Print(L"HII handle %X doesn't have parent Driver handle, nothing to patch\n\r", HiiHandles[i]);
      );
      continue;
    }
    DEBUG_CODE(
      //Debug
      Print(L"HII handle %X has parent Driver handle %X\nwith GUID: %g\n\r", HiiHandles[i], DriverHandle, FvFileName);
    );

    CHAR8 Str[37]; //Guid sized
    AsciiSPrint(Str, sizeof(Str), "%g", &FvFileName);
    Status = FindLoadedImageFromGUID(Str, &TempImageInfo[i]);
    if (EFI_ERROR(Status)) {
      DEBUG_CODE(
        //Debug
        Print(L"\nFindLoadedImageFromGUID status %r\n\n\r", Status);
      );
    }
    DEBUG_CODE(
      //Debug
      Print(L"LOADED IMAGE INTERFACE at: 0x%X, PE base: 0x%X\n\n\r", TempImageInfo[i], TempImageInfo[i]->ImageBase);
    );
  }

  if (DevicePathInfo != NULL) { FreePool(DevicePathInfo); }

  *ImageInfo = TempImageInfo;
  return EFI_SUCCESS;
}

EFI_STATUS
GetFormSetPointerByGuid(
  IN EFI_LOADED_IMAGE_PROTOCOL *PImageInfo,
  IN EFI_GUID FormSetGuid,
  OUT UINT8 **FormSetStart
)
{
  EFI_STATUS Status = EFI_NOT_FOUND;
  *FormSetStart = NULL;

  for (UINTN i = 0; i < PImageInfo->ImageSize; i++)
  {
    UINTN Offset = (UINTN)PImageInfo->ImageBase + (UINTN)i;

    if (CompareMem((UINT8 *)Offset, (UINT8 *)Edk2ifrHiiBinPointerHeaderUint, sizeof(Edk2ifrHiiBinPointerHeaderUint)) == 0)
    {
      if (CompareGuid(&FormSetGuid, (EFI_GUID *)(Offset + sizeof(Edk2ifrHiiBinPointerHeaderUint)))) {
        *FormSetStart = (UINT8 *)(Offset - (sizeof(UINT64) - 1));
        break;
      }
    }
  }

  if (*FormSetStart) {
    Status = EFI_SUCCESS;
  }
  return Status;
}

EFI_HII_PACKAGE_LIST_HEADER *
GetHandlePackageListAndGuid(
  IN CONST EFI_HII_HANDLE HiiHandle,
  OUT EFI_GUID *FileGuid OPTIONAL
)
{
  EFI_STATUS Status;
  EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;
  UINTN BufferSize = 0;
  //-----------------------------------------------------

  Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
  if (EFI_ERROR(Status)) {
    DEBUG_CODE(
      //Debug
      PrintConsoleSREP(L"Unable to locate HII Database!\n\r");
    );
    return NULL;
  }

  Status = gHiiDatabase->ExportPackageLists(gHiiDatabase, HiiHandle, &BufferSize, PackageList);
  if (EFI_ERROR(Status))
  {
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
    if (FileGuid != NULL) {
      EFI_HANDLE DriverHandle;
      UINT8 *DevicePathInfo = AllocateZeroPool(sizeof(UINTN));

      gHiiDatabase->GetPackageListHandle(
        gHiiDatabase,
        HiiHandle,
        &DriverHandle
      );
      if (!EFI_ERROR(Status))
      {

        //Handles got from gBS->OpenProtocol or gBS->HandleProtocol point to HII CONFIG ACCESS, but we need parent
        Status = gBS->OpenProtocol(
          DriverHandle,
          &gEfiDevicePathProtocolGuid,
          (VOID **)&DevicePathInfo,
          gImageHandle,
          NULL,
          EFI_OPEN_PROTOCOL_GET_PROTOCOL
        );
        if (EFI_ERROR(Status)) {
          DEBUG_CODE(
            //Debug
            PrintConsoleSREP(L"\nHandleProtocol status %r\n\n\r", Status);
          );
        }
        else
        {
          //First comes FW_VOL_DEVICE_PATH, then FW_VOL_FILEPATH_DEVICE_PATH, then VENDOR_DEVICE_PATH
          *FileGuid = *(EFI_GUID *)(DevicePathInfo + sizeof(EFI_DEVICE_PATH_PROTOCOL) + sizeof(UINT32) + sizeof(EFI_GUID));
        }
      }
    }

    return PackageList;
  }
}

UINT8 *
PackageBufferOffsetByFormSetGuid(
  IN EFI_GUID *FormSetGuid,
  IN UINT8 *Package,
  IN UINTN PackageListLength
)
{
  EFI_GUID *Guid = NULL;
  EFI_HII_PACKAGE_HEADER *PackageHeader = NULL;
  UINT32 Offset = 0;
  UINT8 *OpCodeData = NULL;

  if (FormSetGuid == NULL || Package == NULL) {
    return NULL;
  }

  while (Offset < PackageListLength && Offset < MAX_UINT32) {
    PackageHeader = (EFI_HII_PACKAGE_HEADER *)Package + Offset;
    if (PackageHeader->Type != EFI_HII_PACKAGE_FORMS) {
      Offset += PackageHeader->Length;
      continue;
    }

    OpCodeData = Package + Offset + sizeof(EFI_HII_PACKAGE_HEADER);
    if (((EFI_IFR_OP_HEADER *)OpCodeData)->OpCode == EFI_IFR_FORM_SET_OP &&
      ((EFI_IFR_OP_HEADER *)OpCodeData)->Length > OFFSET_OF(EFI_IFR_FORM_SET, Flags)) {
      Guid = (EFI_GUID *)(VOID *)(OpCodeData + sizeof(EFI_IFR_OP_HEADER));

      if (CompareGuid(Guid, FormSetGuid)) {
        return (UINT8 *)Guid - sizeof(EFI_IFR_OP_HEADER) - sizeof(UINT32);
      }
    }

    Offset += PackageHeader->Length;
  }

  return NULL;
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

  if (!EFI_ERROR(Status) || Status == EFI_BUFFER_TOO_SMALL)
  {
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
  //Get the languages that the package specified by HiiHandle supports
  //
  SupportedLanguages = HiiGetSupportedLanguages(HiiHandleSREP);
  if (SupportedLanguages == NULL) {
    goto Error;
  }

  //
  //Get the current language setting
  //
  gRT->GetVariable(
    L"SREPLang",
    &SREPLangVariableGuid,
    NULL,
    &SREPLangDataSize,
    PlatformLanguage
  );

  //
  //If Languag is NULL, then set it to an empty string, so it will be
  //skipped by GetBestLanguage() as it won't match with Hii Handle SupportedLanguages
  //
  if (Language == NULL) {
    Language = "";

    //
    //Get the best matching language from SupportedLanguages
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
  //Retrieve the size of the string in the string package for the BestLanguage
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
  //If GetString() returns EFI_SUCCESS for a zero size,
  //then there are no supported languages registered for HiiHandle.  If GetString()
  //returns an error other than EFI_BUFFER_TOO_SMALL, then HiiHandle is not present
  //in the HII Database
  //
  if (Status != EFI_BUFFER_TOO_SMALL) {
    goto Error;
  }

  //
  //Allocate a buffer for the return string
  //
  String = AllocateZeroPool(StringSize);
  if (String == NULL) {
    goto Error;
  }

  //
  //Retrieve the string from the string package
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
  if (EFI_ERROR(Status)) {
    //
    //Free the buffer and return NULL if the supported languages can not be retrieved.
    //
    FreePool(String);
    String = NULL;
  }
  
Error:
  //
  //Free allocated buffers
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
  //Return the Null-terminated Unicode string
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
  //Check range
  //
  if (Completion > 100) {
    return EFI_INVALID_PARAMETER;
  }

  //
  //Print progress percentage
  //
  PrintConsoleSREP(L"\r %3d%% ", Completion);

  //
  //Set progress bar color
  //
  gST->ConOut->SetAttribute (
    gST->ConOut,
    EFI_TEXT_ATTR(EFI_WHITE, EFI_BLACK)
  );

  //
  //Print completed portion of progress bar
  //
  for (; Index < Completion / 2; Index++){
    PrintConsoleSREP(L"%c", BLOCKELEMENT_FULL_BLOCK);
  }

  //
  //Restore text color
  //
  gST->ConOut->SetAttribute(gST->ConOut, CurrentAttribute);

  //
  //Print remaining portion of progress bar
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

  UINTN MaskLengthChars = (MaskLengthBytes * sizeof(UINT16)) + AsciiStrSize('\0');
  UINTN tmpSize = AsciiStrSize("XX");
  CHAR8 *tmp = AllocatePool(tmpSize);
  CHAR8 *StringifiedHexDump = AllocateZeroPool(MaskLengthChars);
  CHAR8 *Result = AllocateZeroPool(MaskLengthChars);
  if (tmp == NULL || StringifiedHexDump == NULL || Result == NULL) {
    PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OUT_OF_RESOURCES), NULL));
    LogToFile(LogFile, Log);

    return NULL;
  }

  for (UINT16 i = 0; i < MaskLengthBytes; i++) //Get string from UINT buffer
  {
    AsciiSPrint(tmp, tmpSize, "%02X", SourceBuffer[i]);
    AsciiStrCatS(StringifiedHexDump, MaskLengthChars, tmp);
  }

  for (UINTN i = 0; i < MaskLengthChars - AsciiStrSize('\0'); i++)
  {
    if (MaskBuffer[i] == Metacharacter) //e.g. 0x2E is (CHAR8)'.'
    {
      Result[i] = StringifiedHexDump[i];
      continue;
    }

    Result[i] = MaskBuffer[i];
  }

  if (StringifiedHexDump != NULL) {
    FreePool(tmp);
    FreePool(StringifiedHexDump);
  }

  return Result;
}

//Prints those 16 symbols wide dumps
VOID
PrintDump(
  IN UINT64 Size,
  IN UINT8 *Dump,
  IN BOOLEAN isUnformattedAscii
)
{
  if (isUnformattedAscii)
  {
    for (UINT64 i = 0; i < Size * 2; i++)
    {
      if (i % 0x20 == 0)
      {
        UnicodeSPrint(Log, GenericBufferSize, u"%a", "\n\t");
        LogToFile(LogFile, Log);
      }
      if (i % 2 == 0)
      {
        UnicodeSPrint(Log, GenericBufferSize, u"%a", " ");
        LogToFile(LogFile, Log);
      }
      UnicodeSPrint(Log, GenericBufferSize, u"%c", Dump[i]);
      LogToFile(LogFile, Log);
    }
    UnicodeSPrint(Log, GenericBufferSize, u"%a", "\n\t");
    LogToFile(LogFile, Log);

    return;
  }

  for (UINT64 i = 0; i < Size; i++)
  {
      if (i % 0x10 == 0)
      {
          UnicodeSPrint(Log, GenericBufferSize, u"%a", "\n\t");
          LogToFile(LogFile, Log);
      }
      UnicodeSPrint(Log, GenericBufferSize, u"%02x ", Dump[i]);
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
FindResourceSection(
  IN EFI_LOADED_IMAGE_PROTOCOL *ImageInfo,
  OUT UINT8 **Offset
)
{
  UINT8 *ImageBase = NULL;
  UINT64 ImageSize;
  EFI_IMAGE_DOS_HEADER *DosHeader = NULL;
  UINT8 *NtHeaders = NULL;
  EFI_IMAGE_FILE_HEADER *FileHeader = NULL;
  EFI_IMAGE_SECTION_HEADER *Section = NULL;
  UINT16 NumberOfSections;
  UINT16 OptionalHeaderSize;
  UINT32 SectionTableOffset;
  UINT32 SectionVirtualSize;
  UINT32 SectionRawSize;
  UINT32 SectionSize;
  UINTN Index;

  if (ImageInfo == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ImageBase = (UINT8 *)ImageInfo->ImageBase;
  ImageSize = ImageInfo->ImageSize;

  //
  //DOS header
  //
  DosHeader = (EFI_IMAGE_DOS_HEADER *)ImageBase;

  if (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }


  //
  //Offset to PE header
  //
  if ((DosHeader->e_lfanew < sizeof(EFI_IMAGE_DOS_HEADER)) ||
    ((UINT64)DosHeader->e_lfanew + sizeof(UINT32) +
      sizeof(EFI_IMAGE_FILE_HEADER) > ImageSize))
  {
    return EFI_INVALID_PARAMETER;
  }

  NtHeaders = ImageBase + DosHeader->e_lfanew;

  //
  //Parse PE
  //
  if (*(UINT32 *)NtHeaders != EFI_IMAGE_NT_SIGNATURE) {
    return EFI_INVALID_PARAMETER;
  }

  FileHeader = (EFI_IMAGE_FILE_HEADER *)(NtHeaders + sizeof(UINT32));

  NumberOfSections = FileHeader->NumberOfSections;
  OptionalHeaderSize = FileHeader->SizeOfOptionalHeader;

  if (NumberOfSections == 0) {
    return EFI_NOT_FOUND;
  }

  //
  //Sections table starts after:
  //Signature + IMAGE_FILE_HEADER + OptionalHeader.
  //
  SectionTableOffset =
    DosHeader->e_lfanew +
    sizeof(UINT32) +
    sizeof(EFI_IMAGE_FILE_HEADER) +
    OptionalHeaderSize;

  if ((UINT64)SectionTableOffset +
    ((UINT64)NumberOfSections * sizeof(EFI_IMAGE_SECTION_HEADER)) > ImageSize)
  {
    return EFI_INVALID_PARAMETER;
  }

  Section = (EFI_IMAGE_SECTION_HEADER *)(ImageBase + SectionTableOffset);

  for (Index = 0; Index < NumberOfSections; ++Index) {
    //
    //IMAGE_SECTION_HEADER.Name is 8 bytes and doesnt have to be NULL
    //
    if (CompareMem(Section[Index].Name, ".rsrc", 5) != 0) {
      continue;
    }

    //
    //Use SizeOfRawData as a fallback
    //
    SectionVirtualSize = Section[Index].Misc.VirtualSize;
    SectionRawSize = Section[Index].SizeOfRawData;

    SectionSize = SectionVirtualSize;
    if (SectionSize == 0) {
      SectionSize = SectionRawSize;
    }

    //
    //RVA of .rsrc is inbounds
    //
    if ((UINT64)Section[Index].VirtualAddress >= ImageSize) {
      return EFI_INVALID_PARAMETER;
    }

    if ((UINT64)Section[Index].VirtualAddress + SectionSize > ImageSize) {
      SectionSize = (UINT32)(
        ImageSize - Section[Index].VirtualAddress
      );
    }

    if (SectionSize == 0) {
      return EFI_INVALID_PARAMETER;
    }

    *Offset = ImageBase + Section[Index].VirtualAddress + 0x60;

    return EFI_SUCCESS;
  }

  return EFI_NOT_FOUND;
}

EFI_STATUS
GetExt1ImageBufferFromSREP(
  IN UINT32 PackageType,
  IN UINT16 PackageId,
  OUT UINT8 **PackageBuffer,
  OUT UINT32 *PackageSize
)
{
  EFI_STATUS Status;
  EFI_HII_PACKAGE_HEADER *PackageHeader = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  EFI_HII_PACKAGE_LIST_HEADER *HiiPackageList = NULL;
  UINT8 *Package = NULL;
  //-----------------------------------------------------

  Status = gBS->HandleProtocol(
    gImageHandle,
    &gEfiLoadedImageProtocolGuid,
    (VOID **)&LoadedImage
  );
  if (EFI_ERROR(Status)) {
    return Status;
  }

  Status = FindResourceSection(LoadedImage, (UINT8 **)&HiiPackageList);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  DEBUG_CODE(
    //Debug
    Print(L".rsrc section offset: 0x%X\n\r", HiiPackageList);
  );

  //
  //In HII package list, find IMAGES package
  //Dont do FreePool(HiiPackageList) yet!
  //
  UINT8 ID = 1;
  UINT32 Offset = sizeof(EFI_HII_PACKAGE_LIST_HEADER); // + 0x14
  UINT32 PackageListLength = HiiPackageList->PackageLength;
  while (Offset < PackageListLength) {
    Package = (UINT8 *)HiiPackageList + Offset;
    PackageHeader = (EFI_HII_PACKAGE_HEADER *)Package;

    if (PackageHeader->Type == PackageType){
      break;
    }

    Offset += PackageHeader->Length;
  }

  //
  //Return if IMAGES package not found
  //
  if (PackageHeader->Type != PackageType)
  {
    return EFI_NOT_FOUND;
  }

  //
  //Go through all images in package, get pointer to image with index bigger than 1
  //
  Package += sizeof(EFI_HII_IMAGE_PACKAGE_HDR) + sizeof(EFI_HII_IMAGE_BLOCK) + sizeof(UINT32); //0xC + 0x1 + 0x4
  while ((ID != PackageId) && (ID < BIT4)) {
    if (*(Package + ReadUnaligned32((UINT32 *)Package - 1)) != EFI_HII_GIBT_EXT1) break;

    Package = Package + ReadUnaligned32((UINT32 *)Package - 1) + sizeof(EFI_HII_IMAGE_BLOCK) + sizeof(UINT32);

    ID++;
  }

  //Save data of a single package, or "*PackageBuffer = Package" and dont free HiiPackageList
  if (ID == PackageId) {
    *PackageSize = ReadUnaligned32((UINT32 *)Package - 1);

    *PackageBuffer = AllocatePool(*PackageSize);
    CopyMem(*PackageBuffer, Package, *PackageSize);

    Status = EFI_SUCCESS;
  }
  else
  {
    Status = EFI_NOT_FOUND;
  }

  return Status;
}

EFI_STATUS
LoadAndRunFileFromBuffer(
  IN CHAR16 *FileName,
  IN UINT8 *Buffer,
  IN UINTN BufferSize
)
{
  EFI_STATUS Status;
  EFI_HANDLE_PROTOCOL HandleProtocol = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
  EFI_FILE_PROTOCOL *Root = NULL;
  EFI_FILE_PROTOCOL *File = NULL;
  EFI_HANDLE NewImageHandle = NULL;
  UINTN WrittenSize;

  HandleProtocol = gST->BootServices->HandleProtocol;
  HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);
  if (Root == NULL)
  {
    return EFI_NO_MEDIA;
  }

  //Create empty file and write Buffer
  Status = Root->Open(
    Root,
    &File,
    FileName,
    EFI_FILE_MODE_CREATE | EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
    0
  );
  if (EFI_ERROR(Status)) {
    Root->Close(Root);
    return Status;
  }

  WrittenSize = BufferSize;
  Status = File->Write(File, &WrittenSize, Buffer);
  File->Close(File);
  if (EFI_ERROR(Status) || WrittenSize != BufferSize) {
    DEBUG_CODE(
      //Debug
      Print(L"Write failed: %r\n\r", Status);
    );
    Root->Close(Root);
    return EFI_ERROR(Status) ? Status : EFI_DEVICE_ERROR;
  }

  UINTN Str8Size = StrLen(FileName) + 1;
  CHAR8 *Str = AllocateZeroPool(Str8Size);
  AsciiSPrint(Str, Str8Size, "%s", FileName);
  Status = LoadFromFS(Str, &LoadedImage, &NewImageHandle);
  if (!EFI_ERROR(Status)) {
    Status = gBS->StartImage(NewImageHandle, (UINTN *)NULL, (CHAR16 **)NULL);
  }

  Root->Close(Root);
  return Status;
}

EFI_STATUS
ExecImageEmbeddedInSREP(
  IN CHAR16 *ReserveFileName,
  IN EFI_IMAGE_ID ImageId
)
{
  EFI_STATUS Status;
  EFI_HANDLE NewImageHandle = NULL;
  UINT8 *Package = NULL;
  UINT32 PackageSize = 0;
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

  DevicePath = AllocateZeroPool(sizeof(MEMMAP_DEVICE_PATH) + sizeof(EFI_DEVICE_PATH_PROTOCOL));
  if (DevicePath == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  MemMapPath = (MEMMAP_DEVICE_PATH *)DevicePath;
  MemMapPath->Header.Type = HARDWARE_DEVICE_PATH;
  MemMapPath->Header.SubType = HW_MEMMAP_DP;
  SetDevicePathNodeLength(&MemMapPath->Header, sizeof(MEMMAP_DEVICE_PATH));
  MemMapPath->MemoryType = EfiBootServicesData;
  MemMapPath->StartingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Package;
  MemMapPath->EndingAddress = (EFI_PHYSICAL_ADDRESS)(UINTN)Package + PackageSize - 1;
  SetDevicePathEndNode(NextDevicePathNode(&MemMapPath->Header));

  Status = gBS->LoadImage(FALSE, gImageHandle, DevicePath, Package, PackageSize, &NewImageHandle);
  FreePool(DevicePath);

  if (Status == EFI_UNSUPPORTED)
  {
    Status = LoadAndRunFileFromBuffer(ReserveFileName, Package, PackageSize);
  }
  else
  {
    Status = gBS->StartImage(NewImageHandle, (UINTN *)NULL, (CHAR16 **)NULL);
  }

  if (Package != NULL){
    FreePool(Package);
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

BOOLEAN
IsValueInSentinelList(
  IN UINT32 Target,
  IN UINT32 First,
  ...
)
{
  VA_LIST Marker;
  UINTN CurrentValue;
  BOOLEAN Found = FALSE;

  //If the very first argument is our terminator (NOP), the list is empty
  if (First == 0) {
    return FALSE;
  }

  //Check the first mandatory argument
  if (First == Target) {
    return TRUE;
  }

  //Initialize the variable argument list
  VA_START(Marker, First);

  //Loop until the sentinel value (0) is encountered
  while (TRUE) {
    CurrentValue = VA_ARG(Marker, UINTN);
    if (CurrentValue == 0) {
      break;
    }

    if (CurrentValue == Target) {
      Found = TRUE;
      break;
    }
  }

  //Clean up the variable argument list
  VA_END(Marker);

  return Found;
}
