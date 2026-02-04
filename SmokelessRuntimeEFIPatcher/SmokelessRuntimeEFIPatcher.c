//"Pointer/Reference alignment" is right.

//Include sources
#include "Opcode.h"

//Specify app version
#define SREP_VERSION L"0.2.2 RUS"


EFI_BOOT_SERVICES *_gBS = NULL;
EFI_RUNTIME_SERVICES *_gRS = NULL;

EFI_FILE *LogFile = NULL;
CHAR16 Log[0x200];

//Declare two sets of named constants
enum
{
    OFFSET = 1,
    PATTERN,
    REL_NEG_OFFSET,
    REL_POS_OFFSET
};

enum OPCODE
{
    NOP,
    LOADED,
    LOADED_GUID_PE,
    LOADED_GUID_TE,
    LOADED_INDEX,
    LOAD_FS,
    LOAD_FV,
    LOAD_GUID_PE,
    LOAD_GUID_RAWnFREEFORM,
    PATCH_FAST,
    PATCH,
    UNINSTALL_PROTOCOL,
    COMPATIBILITY,
    UPDATE_PACK,
    SKIP,
    GET_DB,
    EXEC
};

//Declare data structure for a single operation
struct OP_DATA
{
    enum OPCODE ID;               //Loaded, LoadFromFV, Patch and etc. See the corresp. OPCODE.
    CHAR8 *Name, *Name2;          //<FileName>, argument for the OPCODE.
    BOOLEAN Name_Dyn_Alloc, Name2_Dyn_Alloc;
    UINT64 PatchType;             //Pattern, Offset, Rel...
    BOOLEAN PatchType_Dyn_Alloc;
    INT64 ARG3;                   //Offset
    BOOLEAN ARG3_Dyn_Alloc;
    UINT64 ARG4;                  //Patch length
    BOOLEAN ARG4_Dyn_Alloc;
    UINT64 ARG5;                  //Patch buffer
    BOOLEAN ARG5_Dyn_Alloc;
    UINT64 ARG6;                  //Pattern length
    BOOLEAN ARG6_Dyn_Alloc;
    UINT64 ARG7;                  //Pattern to search for
    BOOLEAN ARG7_Dyn_Alloc;
    CHAR8 *RegexChar;
    BOOLEAN Regex_Dyn_Alloc;
    struct OP_DATA *next;
    struct OP_DATA *prev;
};

typedef struct {
  VOID *BaseAddress;
  UINTN BufferSize;
  UINTN Width;
  UINTN Height;
  UINTN PixelsPerScanLine;
} FRAME_BUFFER;

//Writes string from the buffer to SREP.log
VOID
LogToFile(IN EFI_FILE *LogFile, IN CHAR16 *String)
{
    if (LogFile == NULL) { return; }
    UINTN Size = StrLen(String) * 2;      //Size variable equals to the string size, multiplies by 2 cuz Unicode is CHAR16
    LogFile->Write(LogFile,&Size,String); //EFI_FILE_WRITE (Filename, size, the actual string to write)
    LogFile->Flush(LogFile);              //Flushes all modified data associated with a file to a device
}

//Collect OPCODEs from cfg
static VOID
Add_OP_CODE(IN struct OP_DATA *Start, IN OUT struct OP_DATA *opCode)
{
    struct OP_DATA *next = Start;
    while (next->next != NULL)
    {
        next = next->next;
    }
    next->next = opCode;
    opCode->prev = next;
}

//Prints those 16 symbols wide dumps
static VOID
PrintDump(IN UINT64 Size, IN UINT8 *Dump)
{
    for (UINT64 i = 0; i < Size; i++)
    {
        if (i % 0x10 == 0)
        {
            UnicodeSPrint(Log,genericBufferSize,u"%a","\n\t");
            LogToFile(LogFile,Log);
        }
        UnicodeSPrint(Log,genericBufferSize,u"%02x", Dump[i]);
        LogToFile(LogFile,Log);
    }
    UnicodeSPrint(Log,genericBufferSize,u"%a","\n\t");
    LogToFile(LogFile,Log);
}

//Prints Search Result
static VOID
PrintSearchResult(IN EFI_STATUS Status)
{
    Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);
    UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);
    LogToFile(LogFile, Log);
}

//Prints Case String
static VOID
PrintCase(IN CHAR16 *CaseName)
{
    Print(L"%s%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BASIC_CASE), NULL), CaseName);
    UnicodeSPrint(Log, genericBufferSize, u"%s%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BASIC_CASE), NULL), CaseName);
    LogToFile(LogFile, Log);
}

//Simple font support function
static UINT8 *
CreateSimpleFontPkg(
  IN EFI_WIDE_GLYPH *WideGlyph,
  IN UINT32 WideGlyphSizeInBytes,
  IN EFI_NARROW_GLYPH *NarrowGlyph,
  IN UINT32 NarrowGlyphSizeInBytes
){
  UINT32 PackageLen = sizeof(EFI_HII_SIMPLE_FONT_PACKAGE_HDR) + WideGlyphSizeInBytes + NarrowGlyphSizeInBytes + 4;
  UINT8 *FontPackage = (UINT8 *)AllocateZeroPool(PackageLen);
  *(UINT32 *)FontPackage = PackageLen;

  EFI_HII_SIMPLE_FONT_PACKAGE_HDR *SimpleFont;
  SimpleFont = (EFI_HII_SIMPLE_FONT_PACKAGE_HDR*)(FontPackage + 4);
  SimpleFont->Header.Length = (UINT32)(PackageLen - 4);
  SimpleFont->Header.Type = EFI_HII_PACKAGE_SIMPLE_FONTS;
  SimpleFont->NumberOfNarrowGlyphs = (UINT16)(NarrowGlyphSizeInBytes / sizeof(EFI_NARROW_GLYPH));
  SimpleFont->NumberOfWideGlyphs = (UINT16)(WideGlyphSizeInBytes / sizeof(EFI_WIDE_GLYPH));

  UINT8 *Location = (UINT8*)(&SimpleFont->NumberOfWideGlyphs + 1);
  CopyMem(Location, NarrowGlyph, NarrowGlyphSizeInBytes);
  CopyMem(Location + NarrowGlyphSizeInBytes, WideGlyph, WideGlyphSizeInBytes);

  return FontPackage;
}

static EFI_STATUS
CheckArgs(IN EFI_HANDLE ImageHandle) {
  EFI_STATUS Status, GetLangVarStatus, SetLangVarStatus;
  UINTN SREPLangDataSize = 0x6;
  CHAR8 *SREPLangData = AllocatePool(SREPLangDataSize);
  EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters = NULL;
  Status = gBS->HandleProtocol(
    ImageHandle,
    &gEfiShellParametersProtocolGuid,
    (VOID **)&ShellParameters
  );

  Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SHELL_STATUS), "en-GB"), Status);
  UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SHELL_STATUS), "en-GB"), Status);

  GetLangVarStatus = gRT->GetVariable(
    L"SREPLang",
    &gEfiSREPLangVariableGuid,
    NULL,
    &SREPLangDataSize,
    SREPLangData
  );
  DEBUG_CODE(
    Print(L"SREPLangData: %a, Status: %r\n\r", SREPLangData, GetLangVarStatus);
  );

  if (GetLangVarStatus == EFI_NOT_FOUND || GetLangVarStatus == EFI_BUFFER_TOO_SMALL || AsciiStrCmp(SREPLangData, "ru-RU")) {
    SetLangVarStatus = gRT->SetVariable(
      L"SREPLang",
      &gEfiSREPLangVariableGuid,
      0x7,
      0x6,
      "ru-RU"
    );

    if (EFI_ERROR(SetLangVarStatus))
    {
      Print(L"%s%r\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LANG_VAR_WP), "en-GB"), SetLangVarStatus);
      return SetLangVarStatus;
    }
  }

  //ShellParametersProtocol found and at least 1 arg present
  if (Status == EFI_SUCCESS && ShellParameters->Argc > 1) {
    if (!StrCmp(ShellParameters->Argv[1], L"ENG")) //Check for ENG arg, Argv[0] is app name
    {

      SetLangVarStatus = gRT->SetVariable(
        L"SREPLang",
        &gEfiSREPLangVariableGuid,
        0x7,
        0x6,
        "en-GB"
      );

      if (EFI_ERROR(SetLangVarStatus))
      {
        Print(L"%s%r\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LANG_VAR_WP), "en-GB"), SetLangVarStatus);
        gBS->Stall(3000000);
        return SetLangVarStatus;
      }

      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ENG_MODE), "en-GB"));
      return EFI_SUCCESS;
    }
    else
    {
      //Reach this if arg is not "ENG" and there's more than 1
      Print(L"%s%s\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_INVALID_LAUNCH_PARM), "en-GB"), ShellParameters->Argv[1]);
      gBS->Stall(3000000);
      gBS->Exit(ImageHandle, 0, 0, 0);
    }
  }
  else
  {
    if (EFI_ERROR(Status)) {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LANG_SWITCH_DISABLED), "en-GB"));
    };
  }
  
  Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_RUS_MODE), "en-GB"));
  return Status;
}

//Entry point
EFI_STATUS EFIAPI
SREPEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_STATUS SkipReason = EFI_NOT_FOUND;
    EFI_HANDLE AppImageHandle;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    EFI_REGULAR_EXPRESSION_PROTOCOL *RegularExpressionProtocol = NULL;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
    EFI_FILE *Root;
    EFI_FILE *ConfigFile;
    EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;

    gBS->SetWatchdogTimer(0, 0, 0, 0); //Disable watchdog so the system doesn't reboot by timer

    /*-----------------------------------------------------------------------------------*/
    //
    //Locate HII database, retrieve HII packages from ImageHandle
    //
    Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
    if (EFI_ERROR(Status)) {
        Print(L"Unable to locate HII Database!\n");
        return EFI_NOT_STARTED;
    }

    Status = gBS->OpenProtocol(
      ImageHandle,
      &gEfiHiiPackageListProtocolGuid,
      (VOID **)&PackageList,
      ImageHandle, NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
        Print(L"HII Packages not found in PE/COFF resource section!\n");
        return Status;
    }

    //Get our strings
    Status = gHiiDatabase->NewPackageList(gHiiDatabase, PackageList, NULL, &HiiHandle);
    if (EFI_ERROR(Status))
    {
      Print(L"Unable to register more HII Package!\n\n\r");
      return Status;
    }

    /*-----------------------------------------------------------------------------------*/
    //
    //Create font
    //
    extern EFI_WIDE_GLYPH gSimpleFontWideGlyphData[];
    extern UINT32 gSimpleFontWideBytes;
    extern EFI_NARROW_GLYPH gSimpleFontNarrowGlyphData[];
    extern UINT32 gSimpleFontNarrowBytes;

    UINT8 *FontPackage = CreateSimpleFontPkg(
      gSimpleFontWideGlyphData,
      gSimpleFontWideBytes,
      gSimpleFontNarrowGlyphData,
      gSimpleFontNarrowBytes);

    EFI_HII_HANDLE FontHandle = HiiAddPackages(
      &gHIIRussianFontGuid, //This is OK
      NULL,
      FontPackage,
      NULL,
      NULL);

    FreePool(FontPackage);

    if (FontHandle == NULL)
    {
      Print(L"Error! Unable to add any more font packages!\n\n\r");
    }

    /*-----------------------------------------------------------------------------------*/
    //
    //Draw welcoming message
    //
    gST->ConOut->SetCursorPosition(gST->ConOut, 0, 4);
    Print(L"Welcome to Smokeless Runtime EFI Patcher %s\n\r", SREP_VERSION);

    /*-----------------------------------------------------------------------------------*/
    //
    //Setup the program depending on the presence of arguments
    //
    CheckArgs(ImageHandle);

    /*-----------------------------------------------------------------------------------*/
    //
    //Get log
    //
    EFI_INPUT_KEY Key;
    UINT8 Delay;

    HandleProtocol = SystemTable->BootServices->HandleProtocol;
    HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
    HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
    FileSystem->OpenVolume(FileSystem, &Root);

    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PRESS_L), NULL));
    for ( Status = EFI_UNSUPPORTED, Delay = 0x4; Delay != 0 && EFI_ERROR(Status); Delay--)
    {
      gBS->Stall(1000000);
      Status = gST->ConIn->ReadKeyStroke(gST->ConIn, &Key);
    }
    if ((Status == EFI_SUCCESS) && (Key.UnicodeChar == L'l') && (Key.ScanCode == SCAN_NULL)) {

      Status = Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);

      //File already exists, delete it.
      if (Status == EFI_SUCCESS) {
        LogFile->Delete(LogFile);
        Status = Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
        Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_DEL_LOG), NULL));
        UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_DEL_LOG), NULL));
        LogToFile(LogFile, Log);
      }
      else
      {
        Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOG_OPEN_FAIL), NULL));
        gBS->Stall(3000000);
        return Status;
      }
    }
    else
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOG_CANCEL), NULL));
    }

    /*-----------------------------------------------------------------------------------*/
    //
    //Init Regex
    //
    EFI_HANDLE *HandleBuffer = NULL;
    UINTN BufferSize = 0;
    BOOLEAN isRegexActive = FALSE;

    LoadandRunImage(ImageHandle, SystemTable, L"Oniguruma.efi", &AppImageHandle); //Produce RegularExpressionProtocol

    Status = gBS->LocateHandle(
      ByProtocol,
      &gEfiRegularExpressionProtocolGuid,
      NULL,
      &BufferSize,
      HandleBuffer
    );

    if (Status == EFI_NOT_FOUND)
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_REGEX), NULL));
      UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_REGEX), NULL));
      LogToFile(LogFile, Log);
    }
    else
    {
      //Catch not enough memory here, get Status from LocateHandle
      if (Status == EFI_BUFFER_TOO_SMALL) {
        HandleBuffer = AllocateZeroPool(BufferSize);
        if (HandleBuffer == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
        }
      }

      //Try again after allocation
      Status = gBS->LocateHandle(
        ByProtocol,
        &gEfiRegularExpressionProtocolGuid,
        NULL,
        &BufferSize,
        HandleBuffer
      );

      for (UINTN Index = 0; Index < BufferSize / sizeof(EFI_HANDLE); Index++) {
        gBS->HandleProtocol(
          HandleBuffer[Index],
          &gEfiRegularExpressionProtocolGuid,
          (VOID **)&RegularExpressionProtocol
        );
      }

      isRegexActive = !EFI_ERROR(Status);
    }

    if (HandleBuffer != NULL) {
      FreePool(HandleBuffer);
    }

    /*-----------------------------------------------------------------------------------*/
    //
    //Process cfg file
    //
    EFI_FILE_INFO *FileInfo = NULL;
    BOOLEAN isLastFile = FALSE;
    INTN ShellNextFileMemCmp = FALSE;

    Status = ShellFindFirstFile(Root, &FileInfo);

    DEBUG_CODE(
      //Debug
      Print(L"First file in the root dir: %s\n\r", FileInfo->FileName);
    );

    //File search cycle
    while (!EFI_ERROR(Status) && !isLastFile)
    {
      //Check whether file has .cfg extension
      ShellNextFileMemCmp = CompareMem((VOID *)u".cfg", FileInfo->FileName + StrnLenS(FileInfo->FileName, 0x100) - 0x4, 0x8);
      if (!ShellNextFileMemCmp)
      {
        Print(L"%s%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_FOUND), NULL), FileInfo->FileName);
        UnicodeSPrint(Log, genericBufferSize, u"%s%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_FOUND), NULL), FileInfo->FileName);
        LogToFile(LogFile, Log);
        break;
      }

      DEBUG_CODE(
        //Debug
        CHAR16 ShellNextFileStr[0x100] = { 0 };
        CopyMem(ShellNextFileStr, FileInfo->FileName + StrnLenS(FileInfo->FileName, 0x100) - 0x4, 0x8);
        Print(u"Last 4 chars(extension): \"%s\", and Result: %d\n\r", ShellNextFileStr, ShellNextFileMemCmp);
      );

      Status = ShellFindNextFile(Root, FileInfo, &isLastFile);
    }
    if (FileInfo == NULL || EFI_ERROR(Status))
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_SEARCH_FAIL), NULL));
      UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_SEARCH_FAIL), NULL));
      LogToFile(LogFile, Log);
      gBS->Stall(3000000);
      return Status;
    }

    //Now open file
    Status = Root->Open(Root, &ConfigFile, FileInfo->FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status))
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      LogToFile(LogFile, Log);
      gBS->Stall(3000000);
      return Status;
    }


    //Get config info
    EFI_GUID gFileInfo = EFI_FILE_INFO_ID;
    UINTN FileInfoSize = 0;
    Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, &FileInfo);
    if (Status == EFI_BUFFER_TOO_SMALL)
    {
        FileInfo = AllocatePool(FileInfoSize);
        Status = ConfigFile->GetInfo(ConfigFile, &gFileInfo, &FileInfoSize, FileInfo);
        if (EFI_ERROR(Status))
        {
            Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
            UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
            LogToFile(LogFile, Log);
            gBS->Stall(3000000);
            return Status;
        }
    }
    UINTN ConfigDataSize = FileInfo->FileSize + 1; //Add Last null Terminator
    Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_SIZE), NULL), ConfigDataSize - 1);
    UnicodeSPrint(Log, genericBufferSize, u"%s%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_SIZE), NULL), ConfigDataSize - 1);  //-1 to exclude Last null Terminator from size
    LogToFile(LogFile, Log);

    CHAR8 *ConfigData = AllocateZeroPool(ConfigDataSize);
    FreePool(FileInfo);

    //Now try reading file
    Status = ConfigFile->Read(ConfigFile, &ConfigDataSize, ConfigData);
    if (EFI_ERROR(Status))
    {
      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      LogToFile(LogFile, Log);
      gBS->Stall(3000000);
      return Status;
    }
    ConfigFile->Close(ConfigFile);

    //Reach here if reading went fine
    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PARSING), NULL));
    UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PARSING), NULL));
    LogToFile(LogFile, Log);

    //Stripping NewLine, Carriage and Return
    for (UINTN i = 0; i < ConfigDataSize; i++)
    {
        if (ConfigData[i] == '\n' || ConfigData[i] == '\r' || ConfigData[i] == '\t')
        {
            ConfigData[i] = '\0';
        }
    }
    UINTN curr_pos = 0;

    //Config data parsing vars
    struct OP_DATA *Start = AllocateZeroPool(sizeof(struct OP_DATA));
    struct OP_DATA *Prev_OP;
    Start->ID = 0;
    Start->next = NULL;
    BOOLEAN NullByteSkipped = FALSE;

    //Regex match var
    CHAR16 *Pattern16 = NULL;

    //Fill OP_DATA accroding to ConfigData
    while (curr_pos < ConfigDataSize)
    {

        if (curr_pos != 0 && !NullByteSkipped)
            curr_pos += AsciiStrLen(&ConfigData[curr_pos]);
        if (ConfigData[curr_pos] == '\0')
        {
            curr_pos += 1;
            NullByteSkipped = TRUE;
            continue;
        }
        NullByteSkipped = FALSE;
        if (AsciiStrStr(&ConfigData[curr_pos], "#"))
        {
          curr_pos += AsciiStrLen(&ConfigData[curr_pos]); //Skip the whole line
          continue;
        }
        else
        {
          Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CURRENT_LINE), NULL), &ConfigData[curr_pos]);
          UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CURRENT_LINE), NULL), &ConfigData[curr_pos]);
          LogToFile(LogFile, Log);
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "End"))
        {
          Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_END), NULL));
          UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_END), NULL));
          LogToFile(LogFile, Log);
          continue;
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "Op"))
        {
            curr_pos += 3;
            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_COMMAND), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_COMMAND), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFS"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_FS;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFV"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_FV;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadGUIDandSavePE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_GUID_PE;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadGUIDandSaveFreeform"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_GUID_RAWnFREEFORM;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Compatibility"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = COMPATIBILITY;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Loaded"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonamePE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_GUID_PE;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonameTE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_GUID_TE;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "HandleIndex"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_INDEX;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "FastPatch"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = PATCH_FAST;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Patch"))
            {
              if (!isRegexActive) {
                Print(L"Patch%sFastPatch\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_REGEX2), NULL));
                UnicodeSPrint(Log, genericBufferSize, u"Patch%sFastPatch\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FAILED_REGEX2), NULL));
                LogToFile(LogFile, Log);

                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = PATCH_FAST;
                Add_OP_CODE(Start, Prev_OP);
                continue;
              }

              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = PATCH;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Exec"))
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_EXEC), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_EXEC), NULL));
              LogToFile(LogFile, Log);
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = EXEC;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Skip"))
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP), NULL));
              LogToFile(LogFile, Log);

              DEBUG_CODE(
                //Debug
                Print(L" Prev_OP: %d ", Prev_OP->ID);
              );

              /*
              * Add Op Code if
              * 1 >= ID <= 11
              */
              if (Prev_OP->ID >= 1 && Prev_OP->ID <= 11) {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP_SUPPORTED), NULL));
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP_SUPPORTED), NULL));
                LogToFile(LogFile, Log);
              }
              else
              {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP_UNSUPPORTED), NULL));
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_OP_SKIP_UNSUPPORTED), NULL));
                LogToFile(LogFile, Log);
                curr_pos += AsciiStrLen(&ConfigData[curr_pos]);
                continue;
              }

              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = SKIP;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "UninstallProtocol"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = UNINSTALL_PROTOCOL;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "UpdateHiiPackage"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = UPDATE_PACK;
              Add_OP_CODE(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "GetAptioDB"))
            {
              /*
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = GET_AptioDB;
              Add_OP_CODE(Start, Prev_OP);
              */
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_INACTIVE_OP), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_INACTIVE_OP), NULL));
              LogToFile(LogFile, Log);
              continue;
            }

            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_INVALID_OP), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_INVALID_OP), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);
            return EFI_INVALID_PARAMETER;
        }

        if (
             (
               Prev_OP->ID == LOADED || Prev_OP->ID == LOADED_GUID_PE || Prev_OP->ID == LOADED_GUID_TE || Prev_OP->ID == LOADED_INDEX || Prev_OP->ID == LOAD_FS || Prev_OP->ID == LOAD_FV
               || Prev_OP->ID == LOAD_GUID_PE || Prev_OP->ID == UNINSTALL_PROTOCOL || Prev_OP->ID == COMPATIBILITY || Prev_OP->ID == SKIP || Prev_OP->ID == GET_DB
             )
             && Prev_OP->Name == 0
           )
        {
            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_NAME_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_NAME_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            UINTN FileNameLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            CHAR8 *FileName = AllocateZeroPool(FileNameLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileNameLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            continue;
        }
        if ((Prev_OP->ID == UPDATE_PACK) && (Prev_OP->Name == 0))
        {
            UINTN PackageGuidLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            CHAR8 *FileName = AllocateZeroPool(PackageGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], PackageGuidLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            UINTN placeholderUintn = { 0 };

            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            //Skip the line with Package Guid and skip again if the next one is not populated
            curr_pos += PackageGuidLength + 1; //+1 is needed
            if (RETURN_ERROR(AsciiStrHexToUintnS(&ConfigData[curr_pos], NULL, &placeholderUintn)))
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACKAGE_GUID_INPUT), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACKAGE_GUID_INPUT), NULL));
              LogToFile(LogFile, Log);

              //Get back at the prev pos. so not to break the program sequence
              curr_pos -= PackageGuidLength + 1;
              Prev_OP->Name2 = NULL;
              continue;
            }

            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACKAGE_SIZE_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACKAGE_SIZE_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            UINTN CustomPackageLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            FileName = AllocateZeroPool(CustomPackageLength);
            CopyMem(FileName, &ConfigData[curr_pos], CustomPackageLength);
            Prev_OP->Name2 = FileName;
            Prev_OP->Name2_Dyn_Alloc = TRUE;
            continue;
        }
        if ((Prev_OP->ID == LOAD_GUID_RAWnFREEFORM) && (Prev_OP->Name == 0))
        {
            UINTN FileGuidLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            CHAR8 *FileName = AllocateZeroPool(FileGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileGuidLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            EFI_GUID placeholderGuid = { 0 };

            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            //Skip the line with File Guid and skip again if the next one is not populated
            curr_pos += FileGuidLength + 1; //+1 is needed
            if (RETURN_ERROR(AsciiStrToGuid(&ConfigData[curr_pos], &placeholderGuid)))
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_RAW_GUID_INPUT), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_RAW_GUID_INPUT), NULL));
              LogToFile(LogFile, Log);

              //Get back at the prev pos. so not to break the program sequence
              curr_pos -= FileGuidLength + 1;
              Prev_OP->Name2 = NULL;
              continue;
            }

            Print(L"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, genericBufferSize, u"%s%a\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_GUID_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            UINTN SectionGuidLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
            FileName = AllocateZeroPool(SectionGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], SectionGuidLength);
            Prev_OP->Name2 = FileName;
            Prev_OP->Name2_Dyn_Alloc = TRUE;
            continue;
        }
        if ((Prev_OP->ID == PATCH_FAST || Prev_OP->ID == PATCH) && Prev_OP->PatchType == 0)
        {
            if (AsciiStrStr(&ConfigData[curr_pos], "Offset"))
            {
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_OFFSET), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->PatchType = OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Pattern"))
            {
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_PATTERN), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->PatchType = PATTERN;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelNegOffset"))
            {
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_OFFSET), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->PatchType = REL_NEG_OFFSET;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "RelPosOffset"))
            {
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_OFFSET), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->PatchType = REL_POS_OFFSET;
            }
            continue;
        }

        //Check which arguments are present, save curr_pos data
        //This is the new itereration, we are just in from the Pattern OPCODE, Prev_OP->ARG3 == 0
        //ARG3 is Offset, ARG6 is Pattern Length, ARG7 is Pattern Buffer, Regex is raw pattern string
        if ((Prev_OP->ID == PATCH_FAST || Prev_OP->ID == PATCH) && Prev_OP->PatchType != 0 && Prev_OP->ARG3 == 0)
        {
            if (Prev_OP->PatchType == OFFSET || Prev_OP->PatchType == REL_NEG_OFFSET || Prev_OP->PatchType == REL_POS_OFFSET) //Patch by offset
            {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_OFFSET_DECODED), NULL));
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ARG_OFFSET_DECODED), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->ARG3 = AsciiStrHexToUint64(&ConfigData[curr_pos]);
            }
            if (Prev_OP->PatchType == PATTERN) //Take offset from Prev_OP if it was PATTERN patch
            {
                Prev_OP->ARG3 = MAX_UINT32;
                Prev_OP->ARG6 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
                Print(L"\n%s%x\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_SIZE), NULL), Prev_OP->ARG6);
                UnicodeSPrint(Log, genericBufferSize, u"%s%x\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_SIZE), NULL), Prev_OP->ARG6);
                LogToFile(LogFile, Log);

                //Patch with regex impl. 
                UINTN RegexLength = AsciiStrLen(&ConfigData[curr_pos]) + 1;
                CHAR8 *RegexChar = AllocateZeroPool(RegexLength);
                CopyMem(RegexChar, &ConfigData[curr_pos], RegexLength);
                Prev_OP->RegexChar = RegexChar;
                Prev_OP->Regex_Dyn_Alloc = TRUE;
                //Patch impl. end

                //FastPatch impl. 
                Prev_OP->ARG7 = (UINT64)AllocateZeroPool(Prev_OP->ARG6);
                RETURN_STATUS FastPatchInputStatus = AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG6 * 2, (UINT8 *)Prev_OP->ARG7, Prev_OP->ARG6);
                if (RETURN_ERROR(FastPatchInputStatus) && Prev_OP->ID == PATCH_FAST) {
                  Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL), FastPatchInputStatus);
                  UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL), FastPatchInputStatus);
                  LogToFile(LogFile, Log);
                  gBS->Exit(ImageHandle, 0, 0, 0);
                }
                //FastPatch impl. end
            }
            continue;
        }

        //Prev_OP->ARG3 != 0
        //ARG4 is Patch Length, ARG5 is Patch Buffer
        if ((Prev_OP->ID == PATCH_FAST || Prev_OP->ID == PATCH) && Prev_OP->PatchType != 0 && Prev_OP->ARG3 != 0)
        {

            Prev_OP->ARG4 = AsciiStrLen(&ConfigData[curr_pos]) / 2;
            Print(L"\n%s%x\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->ARG4);
            UnicodeSPrint(Log, genericBufferSize, u"%s%x\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->ARG4);
            LogToFile(LogFile, Log);

            Prev_OP->ARG5_Dyn_Alloc = TRUE;
            Prev_OP->ARG5 = (UINT64)AllocateZeroPool(Prev_OP->ARG4);
            AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->ARG4 * 2, (UINT8 *)Prev_OP->ARG5, Prev_OP->ARG4);
            
            AsciiStrToUnicodeStrS(&ConfigData[curr_pos], Pattern16, Prev_OP->ARG4 * 2);
            Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCH_PENDING), NULL));
            UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCH_PENDING), NULL));
            LogToFile(LogFile, Log);

            PrintDump(Prev_OP->ARG4, (UINT8 *)Prev_OP->ARG5);
            UnicodeSPrint(Log, genericBufferSize, u"\n\r");
            LogToFile(LogFile, Log);
            continue;
        }
    }
    FreePool(ConfigData);


    /*-----------------------------------------------------------------------------------*/
    //
    //Start of the actual execution
    //

    struct OP_DATA *next;

    //Op Patch vars
    INT64 BaseOffset = 0; //REL_POS and REL_NEG args rely on this
    UINT64 *Captures = { 0 }; //Represents found offsets, each offset can be up to 8 bytes
    UINTN j = 0; //Stores Captures index

    //Op GetAptioDB vars
    BOOLEAN isDBThere = FALSE;
    UINTN DBSize = 0;
    UINTN DBPointer = 0;

    //Op Compatibility var
    EFI_GUID FilterProtocol = gEfiFirmwareVolume2ProtocolGuid;

    //Op UninstallProtocol var
    UINTN UninstallIndexes = 0;

    //Op Skip vars
    BOOLEAN isOpSkipAllowed = FALSE;
    UINTN skip_pos = 0;

    for (next = Start; next != NULL; next = next->next)
    {
        switch (next->ID){
        case NOP:
            break;
        case LOADED:
            PrintCase(L"Loaded");

            Status = FindLoadedImageFromName(ImageHandle, next->Name, &ImageInfo, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_GUID_PE:
            PrintCase(L"NonamePE");

            Status = FindLoadedImageFromGUID(ImageHandle, next->Name, &ImageInfo, EFI_SECTION_PE32, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_GUID_TE:

            PrintCase(L"NonameTE");

            Status = FindLoadedImageFromGUID(ImageHandle, next->Name, &ImageInfo, EFI_SECTION_TE, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_INDEX:
            PrintCase(L"HandleIndex");

            HandleBuffer = AllocateZeroPool(MAX_UINT16 * sizeof(EFI_HANDLE));

            Status = FindLoadedImageFromShellIndex(ImageHandle, next->Name, &ImageInfo, &HandleBuffer);

            PrintSearchResult(Status);

            if (!EFI_ERROR(Status) && HandleBuffer != NULL) {
              if (LogFile != NULL) {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_STANDBY), NULL));
                UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_HANDLE_DUMP), NULL));
                LogToFile(LogFile, Log);

                for (UINT16 i = 0; HandleBuffer[i] != NULL; i++) {
                  if (LoadedImageProtocolDumpFilePath(HandleBuffer[i]) == NULL) { continue; }

                  UnicodeSPrint(Log, genericBufferSize, u"%X:%s;\n\r", ConvertHandleToHandleIndex(HandleBuffer[i]), LoadedImageProtocolDumpFilePath(HandleBuffer[i]));
                  LogToFile(LogFile, Log);
                }
              }

              FreePool(HandleBuffer);
            }

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_FS:
            PrintCase(L"LoadFromFS");

            Status = LoadFromFS(ImageHandle, next->Name, &ImageInfo, &AppImageHandle);

            PrintSearchResult(Status);

            //UnicodeSPrint(Log,512,u"\t FileName %a\n\r", next->ARG2); //A leftover from Smokeless

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_FV:
            PrintCase(L"LoadFromFV");

            Status = LoadFromFV(ImageHandle, next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_GUID_PE:
            PrintCase(L"LoadGUIDandSavePE");

            Status = LoadGUIDandSavePE(ImageHandle, next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32, SystemTable, FilterProtocol);

            if (EFI_ERROR(Status)) {
              PrintSearchResult(Status);
              isOpSkipAllowed = FALSE;
              break;
            };

            Print(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
            UnicodeSPrint(Log, genericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_GUID_RAWnFREEFORM:
            PrintCase(L"LoadGUIDandSaveFreeform");

            Status = LoadGUIDandSaveFreeform(ImageHandle, &ImageInfo->ImageBase, &ImageInfo->ImageSize, next->Name, next->Name2, SystemTable, FilterProtocol);

            if (EFI_ERROR(Status)) {
              PrintSearchResult(Status);
              isOpSkipAllowed = FALSE;
              break;
            };

            Print(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
            UnicodeSPrint(Log, genericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case PATCH_FAST:
            isOpSkipAllowed = FALSE;

            PrintCase(L"FastPatch");

            /*
            * Reset to patch by ImageInfo if
            * Op has changed from GetAptioDB with success
            */
            if(Status == EFI_SUCCESS){
              isDBThere = FALSE;
              DBPointer = 0;
            }

            /*
            * Reset ImageInfo if
            * Prev Op has not found an Image
            * Prev Op is not GetAptioDB
            */
            if (EFI_ERROR(Status) && isDBThere == FALSE){ FreePool(ImageInfo); goto PatchFail; }

            //Reach here if image found (Status = SUCCESS)
            if (!isDBThere) {
              Print(L"%s%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
              UnicodeSPrint(Log, genericBufferSize, u"%s%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }

              if (next->PatchType == PATTERN)
              {
                  Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_SEARCH), NULL));

                  if (isDBThere) {
                    for (UINTN i = 0; i < DBSize - next->ARG6; i += 1)
                    {
                      next->ARG3 = DBPointer + i;
                      //Since DBPointer is not (UINT8 *), have to use ARG3 as DestinBuf, which slows the cycle
                      if (CompareMem((UINT8 *)next->ARG3, (UINT8 *)next->ARG7, next->ARG6) == 0)
                      {
                        break;
                      }
                    }

                    DEBUG_CODE(
                      //Debug
                      Print(L"Base: %x\n\r", DBPointer);
                      Print(L"ARG3: %x\n\r", (UINT8 *)next->ARG3);
                    );
                  }
                  else
                  {
                    for (UINTN i = 0; i < ImageInfo->ImageSize - next->ARG6; i += 1)
                    {
                      //Old imp, no regex
                      if (CompareMem(((UINT8 *)ImageInfo->ImageBase) + i, (UINT8 *)next->ARG7, next->ARG6) == 0)
                      {
                        next->ARG3 = i;
                        break;
                      }
                    }
                  }
                  if (next->ARG3 == MAX_UINT32) //Stopped near overflow
                  {
                    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
                    UnicodeSPrint(Log, genericBufferSize, u"%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
                    LogToFile(LogFile, Log);

                    next->ARG3 = 0;
                  }
              }
              if (next->PatchType == REL_POS_OFFSET && next->ARG3 != 0)
              {
                next->ARG3 = BaseOffset + next->ARG3;
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_REL_POS_OFFSET), NULL));
              }
              if (next->PatchType == REL_NEG_OFFSET && next->ARG3 != 0)
              {
                next->ARG3 = BaseOffset - next->ARG3;
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_REL_NEG_OFFSET), NULL));
              }
              BaseOffset = next->ARG3;

              Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_MATCH_OFFSET), NULL), next->ARG3);
              UnicodeSPrint(Log, genericBufferSize, u"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_MATCH_OFFSET), NULL), next->ARG3);
              LogToFile(LogFile, Log);

              if (next->ARG3 != 0) {
                isOpSkipAllowed = TRUE;

                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCHED), NULL));
                UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCHED), NULL));
                LogToFile(LogFile, Log);

                if (!isDBThere) {
                  CopyMem((UINT8 *)ImageInfo->ImageBase + next->ARG3, (UINT8 *)next->ARG5, next->ARG4);
                }
                else
                {
                  CopyMem((UINT8 *)next->ARG3, (UINT8 *)next->ARG5, next->ARG4);
                }
                Status = EFI_SUCCESS;
              }
              else
              {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL));
                UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL));
                LogToFile(LogFile, Log);
              }

              PatchFail:
              next->ARG3 = 0;
              break;
        case PATCH:
            isOpSkipAllowed = FALSE;
            /*
            * Reset ImageInfo if
            * Prev Op has not found an Image
            */
            if (EFI_ERROR(Status)) { FreePool(ImageInfo); goto PatchFail; }

            PrintCase(L"Patch");
            Print(L"%s%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            UnicodeSPrint(Log, genericBufferSize, u"%s%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            if (next->PatchType == PATTERN)
            {
              if ((UINT64)next->ARG6 > 0x199) {
                Print(L"%s%X%s\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)next->ARG6, L"HALT!");
                UnicodeSPrint(Log, genericBufferSize, u"%s%X%s\n\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_TOO_BIG), NULL), (UINT64)next->ARG6, L"HALT!");
                LogToFile(LogFile, Log);
                return EFI_UNSUPPORTED;
              };
                
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_SEARCH), NULL));

                Print(L"\n%a\n\n\r", (CHAR8 *)next->RegexChar); //PrintDump ascii to screen
                UnicodeSPrint(Log, genericBufferSize, u"%a%a%a", "\n\t", (CHAR8 *)next->RegexChar, "\n\t"); //PrintDump unicode to log
                LogToFile(LogFile, Log);

                BOOLEAN CResult = FALSE, CResult2 = FALSE; //Comparison Result
                for (UINTN i = 0; i < ImageInfo->ImageSize - next->ARG6; i += 1)
                {
                  Status = EFI_WARN_RESET_REQUIRED; //Reset status to stop saving offset to ARG3
                  CResult2 = FALSE; //Reset result to keep RegexMatch n2 running

                  if (CResult == FALSE) {
                    //Regex match
                    Status = RegexMatch(((UINT8 *)ImageInfo->ImageBase) + i, (CHAR8 *)next->RegexChar, (UINT8)next->ARG6, RegularExpressionProtocol, &CResult);
                    //Status now SUCCESS
                  }

                  /*
                  * Save offset to ARG3 if
                  * 1. The 1st RegexMatch finished with success
                  * 2. It happened this iteration
                  */
                  if (CResult != FALSE && Status == EFI_SUCCESS)
                  {
                    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_STANDBY), NULL));

                    UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_FOUND_INSTANCES), NULL));
                    LogToFile(LogFile, Log);

                    isOpSkipAllowed = TRUE;
                    next->ARG3 = i;
                  }

                  if ((UINTN)next->ARG3 == i) {
                    PrintDump(next->ARG6, (UINT8 *)ImageInfo->ImageBase + next->ARG3); //The cause of PrintDump call edit: winraid.level1techs.com/t/89351/6
                  }

                  //Regex match with batch replacement. ARG 3 is used too widely, so I have to use RegexMatch again to fill Captures. CopyMem will be used again either.
                  /*
                  * Begin matching if
                  * 1. This iteration is new
                  * 2. The 1st RegexMatch finished with success
                  */
                  if ((UINTN)next->ARG3 != i && CResult != FALSE) {
                    RegexMatch(((UINT8 *)ImageInfo->ImageBase) + i, (CHAR8 *)next->RegexChar, (UINT8)next->ARG6, RegularExpressionProtocol, &CResult2);
                    if (CResult2 != FALSE) {

                      Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_ANOTHER_MATCH), NULL));

                      //Fill Captures[j]
                      Captures[j] = i;
                      j += 1;

                      PrintDump(next->ARG6, (UINT8 *)ImageInfo->ImageBase + i);
                    }
                  }
                }
                if (next->ARG3 == MAX_UINT32) //Stopped near overflow
                {
                    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
                    UnicodeSPrint(Log, genericBufferSize, u"%s\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATTERN_NOT_FOUND), NULL));
                    LogToFile(LogFile, Log);

                    next->ARG3 = 0;
                }
            }
            if (next->PatchType == REL_POS_OFFSET && next->ARG3 != 0)
            {
                next->ARG3 = BaseOffset + next->ARG3;
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_REL_POS_OFFSET), NULL));
            }
            if (next->PatchType == REL_NEG_OFFSET && next->ARG3 != 0)
            {
                next->ARG3 = BaseOffset - next->ARG3;
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_REL_NEG_OFFSET), NULL));
            }
            BaseOffset = next->ARG3;

            Print(L"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_MATCH_OFFSET), NULL), next->ARG3);
            UnicodeSPrint(Log, genericBufferSize, u"%s%X\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_MATCH_OFFSET), NULL), next->ARG3);
            LogToFile(LogFile, Log);
            //PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 ); //A leftover from Smokeless

            if (next->ARG3 != 0) {
              isOpSkipAllowed = TRUE;

              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCHED), NULL));
              UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PATCHED), NULL));
              LogToFile(LogFile, Log);

              //Patch first found instance only
              CopyMem((UINT8*)ImageInfo->ImageBase + next->ARG3, (UINT8*)next->ARG5, next->ARG4);
            }
            else
            {
                Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL));
                UnicodeSPrint(Log, genericBufferSize, u"%s", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NOT_PATCHED), NULL));
                LogToFile(LogFile, Log);
            }

            //Patch every instance from Captures (it includes all except the first one)
            if (j != 0) {

              Print(L"%s%d\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_TOTAL_MATCHES), NULL), j);
              UnicodeSPrint(Log, genericBufferSize, u"%s%d\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_TOTAL_MATCHES), NULL), j);
              LogToFile(LogFile, Log);

              for (UINTN i = 0; i < j; i += 1) {
                Print(L"%d%s%x\n\r", i + 1, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NEXT_OFFSET), NULL), (UINT8 *)ImageInfo->ImageBase + Captures[i]);
                UnicodeSPrint(Log, genericBufferSize, u"%d%s%x\n\r", i + 1, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_NEXT_OFFSET), NULL), (UINT8 *)ImageInfo->ImageBase + Captures[i]);
                LogToFile(LogFile, Log);

                CopyMem((UINT8 *)ImageInfo->ImageBase + Captures[i], (UINT8 *)next->ARG5, next->ARG4);
              }
            }

            SetMem(Captures, j, 0);
            j = 0;
            next->ARG3 = 0;
            Status = EFI_SUCCESS;
            //PrintDump(next->ARG4+10,ImageInfo->ImageBase + next->ARG3 -5 ); //A leftover from Smokeless
            break;
        case UNINSTALL_PROTOCOL:
            PrintCase(L"UninstallProtocol");

            Status = UninstallProtocol(next->Name, UninstallIndexes);
            if (EFI_ERROR(Status)) {
              Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
              UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
              LogToFile(LogFile, Log);
              
              isOpSkipAllowed = FALSE;
              break;
            };

            Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PROTOCOL_UNINSTALLED), NULL));
            UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PROTOCOL_UNINSTALLED), NULL));
            LogToFile(LogFile, Log);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case COMPATIBILITY:
            isOpSkipAllowed = FALSE;

            PrintCase(L"Compatibility");
            if (AsciiStrCmp(next->Name, "DB9A1E3D-45CB-4ABB-853B-E5387FDB2E2D") && AsciiStrCmp(next->Name, "389F751F-1838-4388-8390-CD8154BD27F8"))
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_RECOMMENDED_PROTOCOLS), NULL));
            }

            Status = AsciiStrToGuid(next->Name, &FilterProtocol); //Now search for everything with this protocol
            if (EFI_ERROR(Status))
            {
              Print(L"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
              UnicodeSPrint(Log, genericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
              LogToFile(LogFile, Log);

              break;
            }

            Print(L"%s%g\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CURRENT_FILTER), NULL), FilterProtocol);
            UnicodeSPrint(Log, genericBufferSize, u"%s%g\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CURRENT_FILTER), NULL), FilterProtocol);
            LogToFile(LogFile, Log);

            Status = EFI_NOT_FOUND; //This isn't an Op which loads Image, so it mustn't pass Status from AsciiStrToGuid to patch functions
            break;
        case UPDATE_PACK:
            isOpSkipAllowed = FALSE;

            PrintCase(L"UpdateHiiPackage");

            Status = UpdateHandlePackageList(ImageHandle, next->Name, next->Name2, ImageInfo);

            if (Status == EFI_NOT_FOUND)
            {
              Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
              UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
              LogToFile(LogFile, Log);
            }
            else if (EFI_ERROR(Status))
            {
              Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACK_FAILED), NULL), Status);
              UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PACK_FAILED), NULL), Status);
              LogToFile(LogFile, Log);
            }
            else
            {
              Print(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              UnicodeSPrint(Log, genericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }

            break;
        case SKIP:
            
            //Had to declare a new var because "next = next->next" will update on assertion
            skip_pos = AsciiStrDecimalToUintn(next->Name);

            if (isOpSkipAllowed == TRUE) {

              Print(L"%s%d OP\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CASE_SKIP), NULL), skip_pos);
              UnicodeSPrint(Log, genericBufferSize, u"%s%d OP\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_CASE_SKIP), NULL), skip_pos);
              LogToFile(LogFile, Log);

              for (UINT8 i = 0; i < skip_pos - 1; i++) {
                next = next->next;
              }
            }
            else
            {
              SkipReason = (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
              Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LAST_STATUS), NULL), SkipReason);
              UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LAST_STATUS), NULL), SkipReason);
              LogToFile(LogFile, Log);
            }

            isOpSkipAllowed = FALSE;
            break;
        case GET_DB:  //Unused
            isOpSkipAllowed = FALSE;

            PrintCase(L"GetAptioDB");

            DBSize = GetAptioHiiDB( 0 ); DBPointer = GetAptioHiiDB( 1 );

            DEBUG_CODE(
              //Debug
              Print(L"Size: %x, Pointer: %x\n\r", DBSize, DBPointer);
            );

            if (DBSize == 0 || DBPointer == 0) {
              Print(L"HiiDB not found\n\r");
              UnicodeSPrint(Log, genericBufferSize, u"HiiDB not found\n\r");
              LogToFile(LogFile, Log);

              isDBThere = FALSE;
              Status = EFI_NOT_FOUND;
              break;
            }
            Print(L"HiiDB found\n\r");
            UnicodeSPrint(Log, genericBufferSize, u"Size: %x, Pointer: %x\n\r", DBSize, DBPointer);
            LogToFile(LogFile, Log);

            isDBThere = TRUE;
            Status = EFI_NOT_FOUND; //This is ok
            break;
        case EXEC:
            isOpSkipAllowed = FALSE;

            PrintCase(L"Exec");

            gBS->Stall(3000000);
            Status = Exec(&AppImageHandle);

            Print(L"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            UnicodeSPrint(Log, genericBufferSize, u"%s%r\n\r", HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            LogToFile(LogFile, Log);

            break;
        default:
            break;
        }
    }

    for (next = Start; next != NULL; next = next->next)
    {
        if (next->Name_Dyn_Alloc)
            FreePool((VOID *)next->Name);
        if (next->Name2_Dyn_Alloc)
            FreePool((VOID *)next->Name2);
        if (next->PatchType_Dyn_Alloc)
            FreePool((VOID *)next->PatchType);
        if (next->ARG3_Dyn_Alloc)
            FreePool((VOID *)next->ARG3);
        if (next->ARG4_Dyn_Alloc)
            FreePool((VOID *)next->ARG4);
        if (next->ARG5_Dyn_Alloc)
            FreePool((VOID *)next->ARG5);
        if (next->ARG6_Dyn_Alloc)
            FreePool((VOID *)next->ARG6);
        if (next->ARG7_Dyn_Alloc)
            FreePool((VOID *)next->ARG7);
        if (next->Regex_Dyn_Alloc)
            FreePool((VOID *)next->RegexChar);
    }
    next = Start;
    while (next->next != NULL)
    {
        struct OP_DATA *tmp = next;
        next = next->next;
        FreePool(tmp);
    }

    Print(HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PROGRAM_END), NULL));
    UnicodeSPrint(Log, genericBufferSize, HiiGetStringSREP(HiiHandle, STRING_TOKEN(STR_PROGRAM_END), NULL));
    LogToFile(LogFile, Log);

    return EFI_SUCCESS;
}
