//"Pointer/Reference alignment" is right.

//Include sources
#include "Opcode.h"

//Specify app version
#define SREP_VERSION L"0.2.4 RUS"


EFI_BOOT_SERVICES    *_gBS = NULL;
EFI_RUNTIME_SERVICES *_gRS = NULL;

enum ARGS
{
  ENG = BIT0,
  LOG = BIT1,
  NOCON = BIT2
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
  LOAD_HII_FORCE,
  PATCH_FASTEST,
  PATCH_FAST,
  PATCH,
  UNINSTALL_PROTOCOL,
  COMPATIBILITY,
  UPDATE_PACK,
  SKIP,
  EXEC,
  SEND_FORM
};

//Declare data structure for a single operation
struct OP_DATA
{
  enum OPCODE ID;                       //Loaded, LoadFromFV, Patch and etc. See the corresp. OPCODE.

  CHAR8 *Name, *Name2;                  //<FileName>, argument for the OPCODE.
  BOOLEAN Name_Dyn_Alloc, Name2_Dyn_Alloc;

  UINT64 SearchType;                    //Pattern, Offset, Rel...
  BOOLEAN SearchType_Dyn_Alloc;

  INT64 PatchLimit;                     //Batch replacement stop value

  INT64 CurrentOffset;
  BOOLEAN CurrentOffset_Dyn_Alloc;

  UINT64 SimplePatternBuffer;           //Pattern to search for
  BOOLEAN SimplePatternBuffer_Dyn_Alloc;
  UINT64 SimplePatchBuffer;             //FastPatch buffer
  BOOLEAN SimplePatchBuffer_Dyn_Alloc;

  CHAR8 *RegexPatchBuffer;              //Patch buffer with meta-char mask support
  BOOLEAN RegexPatchBuffer_Dyn_Alloc;
  CHAR8 *RegexPatternBuffer;            //RegexPattern Buffer
  BOOLEAN RegexPatternBuffer_Dyn_Alloc;
    
  UINT64 PatchLength;                   //Patch length
  BOOLEAN PatchLength_Dyn_Alloc;
  UINT64 PatternLength;                 //Pattern length
  BOOLEAN PatternLength_Dyn_Alloc;

  struct OP_DATA *next;                 //Links to other lists
  struct OP_DATA *prev;
};

//Copy from UefiPrintLib
UINTN
PrintConsoleSREP(
  IN CONST CHAR16 *Format,
  ...
)
{
  VA_LIST Marker;
  EFI_STATUS Status;
  CHAR16 *Buffer = NULL;
  UINTN BufferSize;
  UINTN Return;

  VA_START(Marker, Format);

  if (Console != NULL)
  {//InternalPrint from UefiPrintLib
    ASSERT(Format != NULL);
    ASSERT(((UINTN)Format & BIT0) == 0);

    BufferSize = GenericBufferSize * sizeof(UINT16) + 1;

    Buffer = (CHAR16 *)AllocatePool(BufferSize);

    if (Buffer == NULL) {
      ASSERT (Buffer != NULL);
      return 0;
    }

    Return = UnicodeVSPrint(Buffer, BufferSize, Format, Marker);

    if (Return > 0) {
      //
      //To be extra safe make sure Console has been initialized
      //
      Status = Console->OutputString(Console, Buffer);
      if (EFI_ERROR(Status)) {
        Return = 0;
      }
    }

    FreePool(Buffer);
  }

  VA_END(Marker);

  return Return;
}

//Collect OpCodes from cfg
static VOID
AddOpCode(
  IN struct OP_DATA *Start,
  IN OUT struct OP_DATA *opCode
)
{
    struct OP_DATA *next = Start;
    while (next->next != NULL)
    {
        next = next->next;
    }
    next->next = opCode;
    opCode->prev = next;
}

//Prints Search Result
static VOID
PrintSearchResult(
  IN EFI_STATUS Status
)
{
    PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SEARCH_RESULT), NULL), Status);
    LogToFile(LogFile, Log);
}

//Prints Case String
static VOID
PrintCase(
  IN CHAR16 *CaseName
)
{
    PrintConsoleSREP(L"%s%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BASIC_CASE), NULL), CaseName);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BASIC_CASE), NULL), CaseName);
    LogToFile(LogFile, Log);
}

//Simple font support function
static UINT8 *
CreateSimpleFontPkg(
  IN EFI_WIDE_GLYPH *WideGlyph,
  IN UINT32 WideGlyphSizeInBytes,
  IN EFI_NARROW_GLYPH *NarrowGlyph,
  IN UINT32 NarrowGlyphSizeInBytes
)
{
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

static VOID
SetLang(
  IN BOOLEAN HasEng
)
{
  EFI_STATUS GetLangVarStatus, SetLangVarStatus;
  CHAR8 *SREPLangData = NULL;
  UINTN SREPLangDataSize;

  GetLangVarStatus = gRT->GetVariable(
    L"SREPLang",
    &gEfiSREPLangVariableGuid,
    NULL,
    &SREPLangDataSize,
    SREPLangData
  );
  DEBUG_CODE(
    //Debug
    Print(L"SREPLangData: %a, Status: %r\n\r", SREPLangData, GetLangVarStatus);
  );

  SREPLangData = AllocatePool(SREPLangDataSize);
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
      Print(L"%s%r\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LANG_VAR_WP), "en-GB"), SetLangVarStatus);
      return;
    }
  }
  FreePool(SREPLangData);

  if (HasEng)
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
      Print(L"%s%r\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LANG_VAR_WP), "en-GB"), SetLangVarStatus);
      gBS->Stall(3000000);
      return;
    }

    Print(HiiGetStringSREP(STRING_TOKEN(STR_ENG_MODE), "en-GB"));
    return;
  }
  
  Print(HiiGetStringSREP(STRING_TOKEN(STR_RUS_MODE), "en-GB"));
  return;
}

static EFI_FILE_INFO *
GetCfg(
  OUT EFI_FILE **File,
  OUT UINT8 *Args
)
{
  EFI_STATUS Status;
  EFI_HANDLE_PROTOCOL HandleProtocol = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
  EFI_FILE *Root = NULL;
  EFI_FILE *ConfigFile = NULL;
  EFI_FILE_INFO *FileInfo = NULL;
  INTN ShellNextFileMemCmp = FALSE;
  BOOLEAN isLastFile = FALSE;

  HandleProtocol = gST->BootServices->HandleProtocol;
  HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);
  if (Root == NULL)
  {
    return NULL;
  }

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
      Print(L"%s%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CFG_FOUND), NULL), FileInfo->FileName);
      UnicodeSPrint(Log, GenericBufferSize, u"%s%s\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CFG_FOUND), NULL), FileInfo->FileName);
      LogToFile(LogFile, Log);
      break;
    }

    DEBUG_CODE(
      //Debug
      CHAR16 ShellNextFileStr[0x100] = { 0 };
      CopyMem(ShellNextFileStr, FileInfo->FileName + StrnLenS(FileInfo->FileName, 0x100) - 0x4, 0x8);
      Print(L"Last 4 chars(extension): \"%s\", and Result: %d\n\r", ShellNextFileStr, ShellNextFileMemCmp);
    );

    Status = ShellFindNextFile(Root, FileInfo, &isLastFile);
  }
  if (FileInfo == NULL || EFI_ERROR(Status))
  {
    Print(HiiGetStringSREP(STRING_TOKEN(STR_CFG_SEARCH_FAIL), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_CFG_SEARCH_FAIL), NULL));
    LogToFile(LogFile, Log);
    gBS->Stall(3000000);
    return NULL;
  }

  //Now open file
  Status = Root->Open(Root, &ConfigFile, FileInfo->FileName, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status))
  {
    Print(HiiGetStringSREP(STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
    LogToFile(LogFile, Log);
    gBS->Stall(3000000);
    return NULL;
  }

  *File = ConfigFile;
  if (StrStr(FileInfo->FileName, L"_ENG")) *Args |= ENG;
  if (StrStr(FileInfo->FileName, L"_LOG")) *Args |= LOG;
  if (StrStr(FileInfo->FileName, L"_NOCON")) *Args |= NOCON;

  return FileInfo;
}

static VOID
GetLog(
  IN BOOLEAN LogEnable
)
{
  EFI_STATUS Status;
  EFI_HANDLE_PROTOCOL HandleProtocol = NULL;
  EFI_LOADED_IMAGE_PROTOCOL *LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *FileSystem = NULL;
  EFI_FILE *Root = NULL;

  HandleProtocol = gST->BootServices->HandleProtocol;
  HandleProtocol(gImageHandle, &gEfiLoadedImageProtocolGuid, (void **)&LoadedImage);
  HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void **)&FileSystem);
  FileSystem->OpenVolume(FileSystem, &Root);

  if (LogEnable)
  {

    Status = Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);

    //File already exists, delete it.
    if (Status == EFI_SUCCESS) {
      LogFile->Delete(LogFile);
      Root->Open(Root, &LogFile, L"SREP.log", EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
      Print(HiiGetStringSREP(STRING_TOKEN(STR_DEL_LOG), NULL));
      UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_DEL_LOG), NULL));
      LogToFile(LogFile, Log);
    }
    else
    {
      Print(L"%s: %r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOG_OPEN_FAIL), NULL), Status);
      gBS->Stall(3000000);
    }
  }
  else
  {
    Print(HiiGetStringSREP(STRING_TOKEN(STR_LOG_CANCEL), NULL));
  }
}

static VOID
InitRegex(
  IN EFI_HANDLE *Handles,
  IN UINTN BufferSize,
  OUT EFI_REGULAR_EXPRESSION_PROTOCOL **RegularExpressionProtocol,
  OUT BOOLEAN *isRegexActive
)
{
  EFI_STATUS Status;

  //Treat an embedded image as app and start it
  Status = ExecImageEmbeddedInSREP(IMAGE_TOKEN(REGEX_DRIVER)); //Produce RegularExpressionProtocol
  if (EFI_ERROR(Status))
  {
    return;
  }

  Status = gBS->LocateHandle(
    ByProtocol,
    &gEfiRegularExpressionProtocolGuid,
    NULL,
    &BufferSize,
    Handles
  );

  if (Status == EFI_NOT_FOUND)
  {
    Print(HiiGetStringSREP(STRING_TOKEN(STR_FAILED_REGEX), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_FAILED_REGEX), NULL));
    LogToFile(LogFile, Log);
  }
  else
  {
    //Catch not enough memory here, get Status from LocateHandle
    if (Status == EFI_BUFFER_TOO_SMALL) {
      Handles = AllocateZeroPool(BufferSize);
      if (Handles == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
      }
    }

    //Try again after allocation
    Status = gBS->LocateHandle(
      ByProtocol,
      &gEfiRegularExpressionProtocolGuid,
      NULL,
      &BufferSize,
      Handles
    );

    for (UINTN Index = 0; Index < BufferSize / sizeof(EFI_HANDLE); Index++) {
      gBS->HandleProtocol(
        Handles[Index],
        &gEfiRegularExpressionProtocolGuid,
        (VOID **)RegularExpressionProtocol
      );
    }

    *isRegexActive = !EFI_ERROR(Status);
  }

  if (Handles != NULL) {
    FreePool(Handles);
  }
}

//Entry point
EFI_STATUS EFIAPI
SREPEntry(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable) {
    EFI_STATUS Status;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInfo = NULL;
    EFI_HII_PACKAGE_LIST_HEADER *PackageList = NULL;

    //Disable watchdog so the system doesn't reboot by timer, set Console output and clear screen
    gBS->SetWatchdogTimer(0, 0, 0, 0);
    Console = gST->ConOut;
    Console->ClearScreen(Console);

    /*-----------------------------------------------------------------------------------*/
    //
    //Timer setup
    //
    UINT64 StartTick = GetPerformanceCounter();

    /*-----------------------------------------------------------------------------------*/
    //
    //Locate HII database, retrieve HII packages from ImageHandle
    //
    Status = gBS->LocateProtocol(&gEfiHiiDatabaseProtocolGuid, NULL, (VOID **)&gHiiDatabase);
    if (EFI_ERROR(Status)) {
      Print(L"Unable to locate HII Database!\n\r");
      return EFI_NOT_STARTED;
    }

    Status = gBS->OpenProtocol(
      ImageHandle,
      &gEfiHiiPackageListProtocolGuid,
      (VOID **)&PackageList,
      ImageHandle, NULL,
      EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
      Print(L"HII Packages not found in PE/COFF resource section!\n\r");
      return Status;
    }

    //Get our strings
    Status = gHiiDatabase->NewPackageList(gHiiDatabase, PackageList, NULL, &HiiHandleSREP);
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
      gSimpleFontNarrowBytes
    );

    EFI_HII_HANDLE FontHandle = HiiAddPackages(
      &gHIIRussianFontGuid,
      NULL,
      FontPackage,
      NULL,
      NULL
    );
    FreePool(FontPackage);

    if (FontHandle == NULL)
    {
      Print(L"Unable to register more HII Package!\n\n\r");
    }


    /*-----------------------------------------------------------------------------------*/
    //
    //Draw welcoming message
    //
    Console->SetCursorPosition(gST->ConOut, 0, 4);
    Print(L"Welcome to Smokeless Runtime EFI Patcher %s\n\r", SREP_VERSION);

    /*-----------------------------------------------------------------------------------*/
    //
    //Get instance of config file, get file name and arguments in it
    //
    EFI_FILE *ConfigFile = NULL;
    UINT8 SREPArgs = 0;
    EFI_FILE_INFO *FileInfo = GetCfg(&ConfigFile, &SREPArgs);
    if (FileInfo == NULL) return EFI_NOT_STARTED;

    if ((BOOLEAN)SREPArgs & NOCON) Console = NULL;

    /*-----------------------------------------------------------------------------------*/
    //
    //Setup lang depending on the presence of argument
    //
    SetLang((BOOLEAN)SREPArgs & ENG);

    /*-----------------------------------------------------------------------------------*/
    //
    //Get log
    //
    GetLog((BOOLEAN)SREPArgs & LOG);

    /*-----------------------------------------------------------------------------------*/
    //
    //Init Regex
    //
    EFI_HANDLE *Handles = NULL;
    UINTN BufferSize = 0;
    EFI_REGULAR_EXPRESSION_PROTOCOL *RegularExpressionProtocol = NULL;
    BOOLEAN isRegexActive = FALSE;

    InitRegex(Handles, BufferSize, &RegularExpressionProtocol, &isRegexActive);

    /*-----------------------------------------------------------------------------------*/
    //
    //Get config data and fill queue
    //
    UINTN ConfigDataSize = FileInfo->FileSize + AsciiStrSize('\0'); //Add Last null Terminator
    Print(L"%s%X\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CFG_SIZE), NULL), ConfigDataSize - 1);
    UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CFG_SIZE), NULL), ConfigDataSize - 1);  //-1 to exclude Last null Terminator from size
    LogToFile(LogFile, Log);

    CHAR8 *ConfigData = AllocateZeroPool(ConfigDataSize);
    FreePool(FileInfo);

    //Now try reading contents to CHAR8 buffer
    Status = ConfigFile->Read(ConfigFile, &ConfigDataSize, ConfigData);
    if (EFI_ERROR(Status))
    {
      Print(HiiGetStringSREP(STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_CFG_OPEN_FAIL), NULL));
      LogToFile(LogFile, Log);
      gBS->Stall(3000000);
      return Status;
    }
    ConfigFile->Close(ConfigFile);

    //Reach here if reading went fine
    Print(HiiGetStringSREP(STRING_TOKEN(STR_PARSING), NULL));
    UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PARSING), NULL));
    LogToFile(LogFile, Log);

    //Stripping NewLine, Carriage and Return
    for (UINTN i = 0; i < ConfigDataSize; i++)
    {
        if (ConfigData[i] == '\n' || ConfigData[i] == '\r' || ConfigData[i] == '\t')
        {
            ConfigData[i] = '\0';
        }
    }

    //Config data parsing vars
    struct OP_DATA *Start = AllocateZeroPool(sizeof(struct OP_DATA));
    struct OP_DATA *Prev_OP;
    Start->ID = 0;
    Start->next = NULL;
    UINTN curr_pos = 0;
    BOOLEAN NullByteSkipped = FALSE;
    CHAR8 *FileName = NULL;

    //Pattern match var
    INTN StrDiff = 0;

    //Fill OP_DATA accroding to ConfigData
    while (curr_pos < ConfigDataSize)
    {
        if (curr_pos != 0 && !NullByteSkipped) { curr_pos += AsciiStrSize(&ConfigData[curr_pos]); }

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
          PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CURRENT_LINE), NULL), &ConfigData[curr_pos]);
          UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CURRENT_LINE), NULL), &ConfigData[curr_pos]);
          LogToFile(LogFile, Log);
        }

        if (AsciiStrStr(&ConfigData[curr_pos], "End"))
        {
          PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OP_END), NULL));
          UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OP_END), NULL));
          LogToFile(LogFile, Log);
          continue;
        }
        if (AsciiStrStr(&ConfigData[curr_pos], "Op"))
        {
            curr_pos += 3;
            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_COMMAND), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_COMMAND), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            if (AsciiStrStr(&ConfigData[curr_pos], "Loaded"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonamePE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_GUID_PE;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "NonameTE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_GUID_TE;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "HandleIndex"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOADED_INDEX;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFS"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_FS;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadFromFV"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_FV;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadGUIDandSavePE"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_GUID_PE;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "LoadGUIDandSaveFreeform"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_GUID_RAWnFREEFORM;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "HiiForceLoad"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = LOAD_HII_FORCE;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "FastestPatch"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = PATCH_FASTEST;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "FastPatch"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = PATCH_FAST;

              Prev_OP->PatchLimit = -1; //-1 meaning unlim.
              if (AsciiStrLen(&ConfigData[curr_pos]) > sizeof("FastPatch")) { //True when "Patch_N" compared to "Patch", size 7 > 6
                StrDiff = AsciiStrCmp(&ConfigData[curr_pos], "FastPatch ");

                Prev_OP->PatchLimit = (BOOLEAN)(StrDiff >= L'0' && StrDiff <= L'9') ? StrDiff - L'0' : Prev_OP->PatchLimit;
              }

              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Patch"))
            {
              Prev_OP->PatchLimit = -1;

              if (!isRegexActive) {
                PrintConsoleSREP(L"Patch%sFastPatch\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_REGEX2), NULL));
                UnicodeSPrint(Log, GenericBufferSize, u"Patch%sFastPatch\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FAILED_REGEX2), NULL));
                LogToFile(LogFile, Log);

                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = PATCH_FAST;

                if (AsciiStrLen(&ConfigData[curr_pos]) > sizeof("FastPatch")) { //True when "Patch_N" compared to "Patch", size 7 > 6
                  StrDiff = AsciiStrCmp(&ConfigData[curr_pos], "FastPatch ");

                  Prev_OP->PatchLimit = (BOOLEAN)(StrDiff >= L'0' && StrDiff <= L'9') ? StrDiff - L'0' : Prev_OP->PatchLimit;
                }
              }
              else
              {
                Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
                Prev_OP->ID = PATCH;

                if (AsciiStrLen(&ConfigData[curr_pos]) > sizeof("Patch")) { //True when "Patch_N" compared to "Patch", size 7 > 6
                  StrDiff = AsciiStrCmp(&ConfigData[curr_pos], "Patch ");

                  Prev_OP->PatchLimit = (BOOLEAN)(StrDiff >= L'0' && StrDiff <= L'9') ? StrDiff - L'0' : Prev_OP->PatchLimit;
                }
              }

              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "UninstallProtocol"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = UNINSTALL_PROTOCOL;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Compatibility"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = COMPATIBILITY;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "UpdateHiiPackage"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = UPDATE_PACK;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Skip"))
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP), NULL));
              LogToFile(LogFile, Log);

              DEBUG_CODE(
                //Debug
                PrintConsoleSREP(L" Prev_OP: %d ", Prev_OP->ID);
              );

              /*
              * Add Op Code if
              * 1 >= ID <= 13
              */
              if (Prev_OP->ID >= LOADED && Prev_OP->ID <= UNINSTALL_PROTOCOL) {
                PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP_SUPPORTED), NULL));
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP_SUPPORTED), NULL));
                LogToFile(LogFile, Log);
              }
              else
              {
                PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP_UNSUPPORTED), NULL));
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OP_SKIP_UNSUPPORTED), NULL));
                LogToFile(LogFile, Log);
                curr_pos += AsciiStrLen(&ConfigData[curr_pos]);
                continue;
              }

              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = SKIP;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "Exec"))
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_OP_EXEC), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_OP_EXEC), NULL));
              LogToFile(LogFile, Log);
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = EXEC;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "SendForm"))
            {
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = SEND_FORM;
              AddOpCode(Start, Prev_OP);
              continue;
            }
            if (AsciiStrStr(&ConfigData[curr_pos], "GetAptioDB"))
            {
              /* Removed
              Prev_OP = AllocateZeroPool(sizeof(struct OP_DATA));
              Prev_OP->ID = GET_AptioDB;
              Add_OP_CODE(Start, Prev_OP);
              */
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_INACTIVE_OP), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_INACTIVE_OP), NULL));
              LogToFile(LogFile, Log);
              continue;
            }

            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_INVALID_OP), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_INVALID_OP), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);
            return EFI_INVALID_PARAMETER;
        }

        if (
            IsValueInSentinelList(Prev_OP->ID, LOADED, LOADED_GUID_PE, LOADED_GUID_TE, LOADED_INDEX, LOAD_FS, LOAD_FV, LOAD_GUID_PE, UNINSTALL_PROTOCOL, COMPATIBILITY, SKIP)
            && (Prev_OP->Name == 0)
           )
        {
            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_NAME_INPUT), NULL), &ConfigData[curr_pos]);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_NAME_INPUT), NULL), &ConfigData[curr_pos]);
            LogToFile(LogFile, Log);

            UINTN FileNameLength = AsciiStrSize(&ConfigData[curr_pos]);
            FileName = AllocateZeroPool(FileNameLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileNameLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            continue;
        }
        if ((Prev_OP->ID == UPDATE_PACK) && (Prev_OP->Name == 0))
        {
            UINTN PackageGuidLength = AsciiStrSize(&ConfigData[curr_pos]);
            FileName = AllocateZeroPool(PackageGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], PackageGuidLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;

            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), Prev_OP->Name);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), Prev_OP->Name);

            //Skip the line with Package Guid and skip again if the next one is not populated
            curr_pos += PackageGuidLength + 1; //+1 is needed
            if (!AsciiIsHexString(&ConfigData[curr_pos])) //Check whether ascii str is a valid number
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PACKAGE_GUID_INPUT), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PACKAGE_GUID_INPUT), NULL));
              LogToFile(LogFile, Log);

              //Get back at the prev pos. so not to break the program sequence
              curr_pos -= PackageGuidLength + 1;
              Prev_OP->Name2 = NULL;
              continue;
            }
            LogToFile(LogFile, Log);

            UINTN CustomPackageSizeStrLength = AsciiStrSize(&ConfigData[curr_pos]);
            FileName = AllocateZeroPool(CustomPackageSizeStrLength);
            CopyMem(FileName, &ConfigData[curr_pos], CustomPackageSizeStrLength);
            Prev_OP->Name2 = FileName;
            Prev_OP->Name2_Dyn_Alloc = TRUE;

            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PACKAGE_SIZE_INPUT), NULL), Prev_OP->Name2);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PACKAGE_SIZE_INPUT), NULL), Prev_OP->Name2);
            LogToFile(LogFile, Log);
            continue;
        }
        if ((Prev_OP->ID == LOAD_GUID_RAWnFREEFORM) && (Prev_OP->Name == 0))
        {
            UINTN FileGuidLength = AsciiStrSize(&ConfigData[curr_pos]);
            FileName = AllocateZeroPool(FileGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], FileGuidLength);
            Prev_OP->Name = FileName;
            Prev_OP->Name_Dyn_Alloc = TRUE;
            EFI_GUID placeholderGUID = { 0 };

            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), Prev_OP->Name);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_FOUND_GUID_INPUT), NULL), Prev_OP->Name);
            LogToFile(LogFile, Log);

            //Skip the line with File Guid and skip again if the next one is not populated
            curr_pos += FileGuidLength + 1; //+1 is needed
            if (AsciiStrToGuid(&ConfigData[curr_pos], &placeholderGUID))
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_RAW_GUID_INPUT), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_RAW_GUID_INPUT), NULL));
              LogToFile(LogFile, Log);

              //Get back at the prev pos. so not to break the program sequence
              curr_pos -= FileGuidLength + 1;
              Prev_OP->Name2 = NULL;
              continue;
            }

            UINTN SectionGuidLength = AsciiStrSize(&ConfigData[curr_pos]);
            FileName = AllocateZeroPool(SectionGuidLength);
            CopyMem(FileName, &ConfigData[curr_pos], SectionGuidLength);
            Prev_OP->Name2 = FileName;
            Prev_OP->Name2_Dyn_Alloc = TRUE;

            PrintConsoleSREP(L"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_GUID_INPUT), NULL), Prev_OP->Name2);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%a\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_GUID_INPUT), NULL), Prev_OP->Name2);
            LogToFile(LogFile, Log);
            continue;
        }
        if (IsValueInSentinelList(Prev_OP->ID, PATCH_FASTEST, PATCH_FAST, PATCH) && (Prev_OP->SearchType == 0))
        {
            if (AsciiStrStr(&ConfigData[curr_pos], "Offset"))
            {
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_ARG_OFFSET), NULL));
                Prev_OP->SearchType = OFFSET;
            }
            else if (AsciiStrStr(&ConfigData[curr_pos], "Pattern"))
            {
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_ARG_PATTERN), NULL));
                Prev_OP->SearchType = PATTERN;
            }
            else if (AsciiStrStr(&ConfigData[curr_pos], "RelNegOffset"))
            {
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_ARG_OFFSET), NULL));
                Prev_OP->SearchType = REL_NEG_OFFSET;
            }
            else if (AsciiStrStr(&ConfigData[curr_pos], "RelPosOffset"))
            {
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_ARG_OFFSET), NULL));
                Prev_OP->SearchType = REL_POS_OFFSET;
            }
            else
            {
                PrintConsoleSREP(L"Position 0x%X - %s%a\n\r", curr_pos, HiiGetStringSREP(STRING_TOKEN(STR_NO_PATCH_TYPE), NULL), &ConfigData[curr_pos]);
                UnicodeSPrint(Log, GenericBufferSize, u"Position 0x%X - %s%a\n\r", curr_pos, HiiGetStringSREP(STRING_TOKEN(STR_NO_PATCH_TYPE), NULL), &ConfigData[curr_pos]);
            }
            LogToFile(LogFile, Log);

            continue;
        }

        //Check which arguments are present, save curr_pos data
        //This is the new itereration, we are just in from the Pattern OPCODE, Prev_OP->CurrentOffset == 0
        if (IsValueInSentinelList(Prev_OP->ID, PATCH_FASTEST, PATCH_FAST, PATCH) && (Prev_OP->SearchType != 0) && (Prev_OP->CurrentOffset == 0))
        {
            if (Prev_OP->SearchType == OFFSET || Prev_OP->SearchType == REL_NEG_OFFSET || Prev_OP->SearchType == REL_POS_OFFSET) //Patch by offset
            {
                PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_ARG_OFFSET_DECODED), NULL));
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_ARG_OFFSET_DECODED), NULL));
                LogToFile(LogFile, Log);
                Prev_OP->CurrentOffset = AsciiStrHexToUint64(&ConfigData[curr_pos]);
            }
            if (Prev_OP->SearchType == PATTERN) //Take offset from Prev_OP if it was PATTERN patch
            {
                Prev_OP->CurrentOffset = -1;
                Prev_OP->PatternLength = AsciiStrLen(&ConfigData[curr_pos]) / sizeof(UINT16);
                PrintConsoleSREP(L"\n%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_SIZE), NULL), Prev_OP->PatternLength);
                UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATTERN_SIZE), NULL), Prev_OP->PatternLength);
                LogToFile(LogFile, Log);

                //Patch with regex impl. 
                UINTN RegexLength = AsciiStrSize(&ConfigData[curr_pos]);
                CHAR8 *RegexPatternBuffer = AllocateZeroPool(RegexLength);
                CopyMem(RegexPatternBuffer, &ConfigData[curr_pos], RegexLength);
                Prev_OP->RegexPatternBuffer = RegexPatternBuffer;
                Prev_OP->RegexPatternBuffer_Dyn_Alloc = TRUE;
                //Patch impl. end

                //FastPatch impl. 
                Prev_OP->SimplePatternBuffer = (UINT64)AllocateZeroPool(Prev_OP->PatternLength);
                RETURN_STATUS FastPatchInputStatus = AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->PatternLength * sizeof(UINT16), (UINT8 *)Prev_OP->SimplePatternBuffer, Prev_OP->PatternLength);
                if (RETURN_ERROR(FastPatchInputStatus) && Prev_OP->ID == PATCH_FASTEST) {
                  PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL), FastPatchInputStatus);
                  UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_NOT_PATCHED), NULL), FastPatchInputStatus);
                  LogToFile(LogFile, Log);
                  gBS->Exit(ImageHandle, 0, 0, 0);
                }
                //FastPatch impl. end
            }
            continue;
        }

        if ((Prev_OP->SearchType != 0) && (Prev_OP->CurrentOffset != 0)) {
            if (IsValueInSentinelList(Prev_OP->ID, PATCH_FAST, PATCH))
            {
              Prev_OP->PatchLength = AsciiStrLen(&ConfigData[curr_pos]);
              Prev_OP->RegexPatchBuffer_Dyn_Alloc = TRUE;
              Prev_OP->RegexPatchBuffer = AllocateZeroPool(Prev_OP->PatchLength + AsciiStrSize('\0')); //String
              AsciiSPrint(Prev_OP->RegexPatchBuffer, Prev_OP->PatchLength + AsciiStrSize('\0'), "%a", &ConfigData[curr_pos]);

              Prev_OP->PatchLength /= sizeof(UINT16); //To bytes count

              PrintConsoleSREP(L"\n%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->PatchLength);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->PatchLength);
              LogToFile(LogFile, Log);

              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATCH_PENDING), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PATCH_PENDING), NULL));
              LogToFile(LogFile, Log);

              PrintDump(Prev_OP->PatchLength, (UINT8 *)Prev_OP->RegexPatchBuffer, TRUE);
            }

            if (Prev_OP->ID == PATCH_FASTEST)
            {
              Prev_OP->PatchLength = AsciiStrLen(&ConfigData[curr_pos]) / sizeof(UINT16);
              Prev_OP->SimplePatchBuffer_Dyn_Alloc = TRUE;
              Prev_OP->SimplePatchBuffer = (UINT64)AllocateZeroPool(Prev_OP->PatchLength); //Unsgined Int
              AsciiStrHexToBytes(&ConfigData[curr_pos], Prev_OP->PatchLength * sizeof(UINT16), (UINT8 *)Prev_OP->SimplePatchBuffer, Prev_OP->PatchLength);

              PrintConsoleSREP(L"\n%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->PatchLength);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PATCH_SIZE), NULL), Prev_OP->PatchLength);
              LogToFile(LogFile, Log);

              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PATCH_PENDING), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PATCH_PENDING), NULL));
              LogToFile(LogFile, Log);

              PrintDump(Prev_OP->PatchLength, (UINT8 *)Prev_OP->SimplePatchBuffer, FALSE);
            }

            UnicodeSPrint(Log, GenericBufferSize, u"\n\r");
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

    //Op Load var
    EFI_HANDLE AppImageHandle = NULL;

    //Op Patch var
    INT64 BaseOffset = 0;

    //Op Compatibility var, don't change
    EFI_GUID FilterProtocol = gEfiFirmwareVolume2ProtocolGuid; //is set to EFI_FIRMWARE_VOLUME2_PROTOCOL for 'FindLoadedImageBuffer' and 'LocateAndLoadFv...'

    //Op UninstallProtocol var
    UINTN UninstallIndices = 0;

    //Op Skip vars
    EFI_STATUS SkipReason = EFI_NOT_FOUND;
    BOOLEAN isOpSkipAllowed = FALSE;
    UINTN skip_pos = 0;

    //Op HiiForceLoad var
    EFI_HII_HANDLE HiiHandleFromForceLoad = NULL;

    for (next = Start; next != NULL; next = next->next)
    {
        switch (next->ID){
        case NOP:
            break;
        case LOADED:
            PrintCase(L"Loaded");

            Status = FindLoadedImageFromName(next->Name, &ImageInfo, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_GUID_PE:
            PrintCase(L"NonamePE");

            Status = FindLoadedImageFromGUID(next->Name, &ImageInfo, EFI_SECTION_PE32, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_GUID_TE:

            PrintCase(L"NonameTE");

            Status = FindLoadedImageFromGUID(next->Name, &ImageInfo, EFI_SECTION_TE, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOADED_INDEX:
            PrintCase(L"HandleIndex");

            UINTN NumericIndex = 0;

            Status = AsciiStrHexToUintnS(next->Name, (CHAR8 **)L"\0", &NumericIndex);
            if (EFI_ERROR(Status))
            {
              PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
              UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
              LogToFile(LogFile, Log);

              break;
            }

            Handles = AllocateZeroPool(MAX_UINT16 * sizeof(EFI_HANDLE));

            Status = FindLoadedImageFromShellIndex(NumericIndex, &ImageInfo, &Handles, FilterProtocol);

            PrintSearchResult(Status);

            if (!EFI_ERROR(Status) && Handles != NULL) {
              if (LogFile != NULL) {
                PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_STANDBY), NULL));
                UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_HANDLE_DUMP), NULL));
                LogToFile(LogFile, Log);

                for (UINT16 i = 0; (Handles[i] != NULL) && (i < MAX_UINT16); i++) {
                  if (LoadedImageProtocolDumpFilePath(Handles[i]) == NULL) { continue; }

                  if (i < NumericIndex) {
                    DisplayUpdateProgressSREP((UINTN)(((float)i / NumericIndex) * 100));
                  }

                  UnicodeSPrint(Log, GenericBufferSize, u"%X:%s;\n\r", ConvertHandleToHandleIndex(Handles[i]), LoadedImageProtocolDumpFilePath(Handles[i]));
                  LogToFile(LogFile, Log);
                }
              }

              if (*Handles != 0)
              {
                DisplayUpdateProgressSREP(100);
              }
              PrintConsoleSREP(L"\n\r");

              FreePool(Handles);
            }

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_FS:
            PrintCase(L"LoadFromFS");

            Status = LoadFromFS(next->Name, &ImageInfo, &AppImageHandle);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_FV:
            PrintCase(L"LoadFromFV");

            Status = LoadFromFV(next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32, FilterProtocol);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_GUID_PE:
            PrintCase(L"LoadGUIDandSavePE");

            Status = LoadGUIDandSavePE(next->Name, &ImageInfo, &AppImageHandle, EFI_SECTION_PE32, FilterProtocol);

            if (EFI_ERROR(Status)) {
              PrintSearchResult(Status);
            }
            else
            {
              PrintConsoleSREP(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              UnicodeSPrint(Log, GenericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_GUID_RAWnFREEFORM:
            PrintCase(L"LoadGUIDandSaveFreeform");

            Status = LoadGUIDandSaveFreeform(&ImageInfo->ImageBase, &ImageInfo->ImageSize, next->Name, next->Name2, SystemTable, FilterProtocol);

            if (EFI_ERROR(Status)) {
              PrintSearchResult(Status);
            }
            else
            {
              PrintConsoleSREP(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              UnicodeSPrint(Log, GenericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINT8 *)ImageInfo->ImageBase, (UINT8 *)ImageInfo->ImageSize);
              LogToFile(LogFile, Log);
            }

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case LOAD_HII_FORCE:
            PrintCase(L"HiiForceLoad");

            Status = AddFirstHiiPackageFromFile(ImageInfo, &HiiHandleFromForceLoad);

            PrintSearchResult(Status);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case PATCH_FASTEST:
            isOpSkipAllowed = FALSE;

            PrintCase(L"FastestPatch");

            if (EFI_ERROR(Status) || ImageInfo->ImageSize == 0) { FreePool(ImageInfo); next->CurrentOffset = 0; }

            PrintConsoleSREP(L"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            FindAndReplace(
                          ImageInfo,
                          RegularExpressionProtocol,
                          next->SearchType,
                          FASTEST,
                          next->PatchLimit,
                          next->SimplePatternBuffer,
                          next->SimplePatchBuffer,
                          next->RegexPatternBuffer,
                          next->RegexPatchBuffer,
                          next->PatternLength,
                          next->PatchLength,
                          next->CurrentOffset,
                          &BaseOffset,
                          &isOpSkipAllowed,
                          &Status
                          );

            break;
        case PATCH_FAST:
            isOpSkipAllowed = FALSE;

            PrintCase(L"FastPatch");

            if (EFI_ERROR(Status) || ImageInfo->ImageSize == 0) { FreePool(ImageInfo); next->CurrentOffset = 0; }

            PrintConsoleSREP(L"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            FindAndReplace(
                          ImageInfo,
                          RegularExpressionProtocol,
                          next->SearchType,
                          FAST,
                          next->PatchLimit,
                          next->SimplePatternBuffer,
                          next->SimplePatchBuffer,
                          next->RegexPatternBuffer,
                          next->RegexPatchBuffer,
                          next->PatternLength,
                          next->PatchLength,
                          next->CurrentOffset,
                          &BaseOffset,
                          &isOpSkipAllowed,
                          &Status
                          );

            break;
        case PATCH:
            isOpSkipAllowed = FALSE;

            PrintCase(L"Patch");

            if (EFI_ERROR(Status) || ImageInfo->ImageSize == 0) { FreePool(ImageInfo); next->CurrentOffset = 0; }

            PrintConsoleSREP(L"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BIN_SIZE), NULL), ImageInfo->ImageSize);
            LogToFile(LogFile, Log);

            FindAndReplace(
                          ImageInfo,
                          RegularExpressionProtocol,
                          next->SearchType,
                          REGEX,
                          next->PatchLimit,
                          next->SimplePatternBuffer,
                          next->SimplePatchBuffer,
                          next->RegexPatternBuffer,
                          next->RegexPatchBuffer,
                          next->PatternLength,
                          next->PatchLength,
                          next->CurrentOffset,
                          &BaseOffset,
                          &isOpSkipAllowed,
                          &Status
                          );
            
            break;
        case UNINSTALL_PROTOCOL:
            PrintCase(L"UninstallProtocol");

            Status = UninstallProtocol(next->Name, UninstallIndices);
            if (EFI_ERROR(Status)) {
              PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_BREAKING_CAUSE), NULL), Status);
            }
            else
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PROTOCOL_UNINSTALLED), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PROTOCOL_UNINSTALLED), NULL));
            }
            LogToFile(LogFile, Log);

            isOpSkipAllowed = !EFI_ERROR(Status);
            break;
        case COMPATIBILITY:
            isOpSkipAllowed = FALSE;

            PrintCase(L"Compatibility");
            if (AsciiStrCmp(next->Name, "DB9A1E3D-45CB-4ABB-853B-E5387FDB2E2D") && AsciiStrCmp(next->Name, "389F751F-1838-4388-8390-CD8154BD27F8"))
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_RECOMMENDED_PROTOCOLS), NULL));
            }

            if (EFI_ERROR(AsciiStrToGuid(next->Name, &FilterProtocol)))
            {
              PrintConsoleSREP(L"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
              UnicodeSPrint(Log, GenericBufferSize, u"%s\"%a\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CONVERTATION_FAILED), NULL), next->Name);
            }
            else
            {
              //Now search with this protocol
              PrintConsoleSREP(L"%s%g\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CURRENT_FILTER), NULL), FilterProtocol);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%g\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CURRENT_FILTER), NULL), FilterProtocol);
            }
            LogToFile(LogFile, Log);

            Status = EFI_NOT_FOUND; //This isn't an Op which loads Image, so it mustn't pass Status from AsciiStrToGuid to patch functions
            break;
        case UPDATE_PACK:
            isOpSkipAllowed = FALSE;

            PrintCase(L"UpdateHiiPackage");

            Status = UpdateHandlePackageList(next->Name, next->Name2, ImageInfo);

            if (Status == EFI_NOT_FOUND)
            {
              PrintConsoleSREP(HiiGetStringSREP(STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
              UnicodeSPrint(Log, GenericBufferSize, HiiGetStringSREP(STRING_TOKEN(STR_PACK_NOT_FOUND), NULL));
            }
            else if (EFI_ERROR(Status))
            {
              PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PACK_FAILED), NULL), Status);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PACK_FAILED), NULL), Status);
            }
            else
            {
              PrintConsoleSREP(L"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINTN *)ImageInfo->ImageBase, ImageInfo->ImageSize);
              UnicodeSPrint(Log, GenericBufferSize, u"%s0x%x / 0x%x\n\r", HiiGetStringSREP(STRING_TOKEN(STR_SECTION_BASENSIZE), NULL), (UINTN *)ImageInfo->ImageBase, ImageInfo->ImageSize);
            }
            LogToFile(LogFile, Log);

            break;
        case SKIP:
            
            //Had to declare a new var because "next->next" will update on assertion
            skip_pos = AsciiStrDecimalToUintn(next->Name);

            if (isOpSkipAllowed == TRUE) {

              PrintConsoleSREP(L"%s%d OP\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CASE_SKIP), NULL), skip_pos);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%d OP\n\r", HiiGetStringSREP(STRING_TOKEN(STR_CASE_SKIP), NULL), skip_pos);

              for (UINT8 i = 0; i < skip_pos - 1; i++) {
                next = next->next;
              }
            }
            else
            {
              SkipReason = (Status == EFI_SUCCESS) ? EFI_NOT_FOUND : Status;
              PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LAST_STATUS), NULL), SkipReason);
              UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LAST_STATUS), NULL), SkipReason);
            }
            LogToFile(LogFile, Log);

            isOpSkipAllowed = FALSE;
            break;
        case EXEC:
            isOpSkipAllowed = FALSE;

            PrintCase(L"Exec");

            gBS->Stall(3000000);
            Status = Exec(&AppImageHandle);

            PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            LogToFile(LogFile, Log);

            break;
        case SEND_FORM:
            isOpSkipAllowed = FALSE;

            PrintCase(L"SendForm");

            if (HiiHandleFromForceLoad != NULL) {
              Status = SendFirstForm(HiiHandleFromForceLoad);
            }
            else
            {
              Status = EFI_NOT_FOUND;
            }

            PrintConsoleSREP(L"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            UnicodeSPrint(Log, GenericBufferSize, u"%s%r\n\r", HiiGetStringSREP(STRING_TOKEN(STR_LOAD_RESULT), NULL), Status);
            LogToFile(LogFile, Log);

            Status = EFI_NOT_FOUND; //This isn't an Op which loads Image, so it mustn't pass Status to patch functions
            break;
        default:
            break;
        }
    }

    //
    //Cleanup
    //
    for (next = Start; next != NULL; next = next->next)
    {
        if (next->Name_Dyn_Alloc)
            FreePool((VOID *)next->Name);
        if (next->Name2_Dyn_Alloc)
            FreePool((VOID *)next->Name2);
        if (next->SearchType_Dyn_Alloc)
            FreePool((VOID *)next->SearchType);
        if (next->CurrentOffset_Dyn_Alloc)
            FreePool((VOID *)next->CurrentOffset);
        if (next->PatchLength_Dyn_Alloc)
            FreePool((VOID *)next->PatchLength);
        if (next->SimplePatchBuffer_Dyn_Alloc)
            FreePool((VOID *)next->SimplePatchBuffer);
        if (next->PatternLength_Dyn_Alloc)
            FreePool((VOID *)next->PatternLength);
        if (next->SimplePatternBuffer_Dyn_Alloc)
            FreePool((VOID *)next->SimplePatternBuffer);
        if (next->RegexPatchBuffer_Dyn_Alloc)
            FreePool((VOID *)next->RegexPatchBuffer);
        if (next->RegexPatternBuffer_Dyn_Alloc)
            FreePool((VOID *)next->RegexPatternBuffer);
    }
    next = Start;
    while (next->next != NULL)
    {
        struct OP_DATA *tmp = next;
        next = next->next;
        FreePool(tmp);
    }

    UINT64 NanoSeconds = GetTimeInNanoSecond(GetPerformanceCounter() - StartTick);
    Print(L"%s%d\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PROGRAM_END), NULL), 1 + (NanoSeconds / 1000000000));
    UnicodeSPrint(Log, GenericBufferSize, u"%s%d\"\n\r", HiiGetStringSREP(STRING_TOKEN(STR_PROGRAM_END), NULL), 1 + (NanoSeconds / 1000000000));
    LogToFile(LogFile, Log);

    return EFI_SUCCESS;
}
