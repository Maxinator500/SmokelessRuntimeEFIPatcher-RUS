//
// Firmware Volume Protocol GUID definition
//
#define EFI_FIRMWARE_VOLUME_PROTOCOL_GUID \
  { \
    0x389F751F, 0x1838, 0x4388, {0x83, 0x90, 0xCD, 0x81, 0x54, 0xBD, 0x27, 0xF8 } \
  }

EFI_GUID gEfiFirmwareVolumeProtocolGuid = EFI_FIRMWARE_VOLUME_PROTOCOL_GUID;

typedef struct _EFI_FIRMWARE_VOLUME_PROTOCOL  EFI_FIRMWARE_VOLUME_PROTOCOL;

//
// FRAMEWORK_EFI_FV_ATTRIBUTES bit definitions
//
typedef UINT64  FRAMEWORK_EFI_FV_ATTRIBUTES;

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_GET_ATTRIBUTES)(
  IN  EFI_FIRMWARE_VOLUME_PROTOCOL            *This,
  OUT FRAMEWORK_EFI_FV_ATTRIBUTES             *Attributes
);

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_SET_ATTRIBUTES)(
  IN EFI_FIRMWARE_VOLUME_PROTOCOL       *This,
  IN OUT FRAMEWORK_EFI_FV_ATTRIBUTES    *Attributes
);

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_READ_FILE)(
  IN EFI_FIRMWARE_VOLUME_PROTOCOL   *This,
  IN EFI_GUID                       *NameGuid,
  IN OUT VOID                       **Buffer,
  IN OUT UINTN                      *BufferSize,
  OUT EFI_FV_FILETYPE               *FoundType,
  OUT EFI_FV_FILE_ATTRIBUTES        *FileAttributes,
  OUT UINT32                        *AuthenticationStatus
);

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_READ_SECTION)(
  IN EFI_FIRMWARE_VOLUME_PROTOCOL   *This,
  IN EFI_GUID                       *NameGuid,
  IN EFI_SECTION_TYPE               SectionType,
  IN UINTN                          SectionInstance,
  IN OUT VOID                       **Buffer,
  IN OUT UINTN                      *BufferSize,
  OUT UINT32                        *AuthenticationStatus
);

typedef UINT32  FRAMEWORK_EFI_FV_WRITE_POLICY;

typedef struct {
  EFI_GUID                *NameGuid;
  EFI_FV_FILETYPE         Type;
  EFI_FV_FILE_ATTRIBUTES  FileAttributes;
  VOID                    *Buffer;
  UINT32                  BufferSize;
} FRAMEWORK_EFI_FV_WRITE_FILE_DATA;

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_WRITE_FILE)(
  IN EFI_FIRMWARE_VOLUME_PROTOCOL             *This,
  IN UINT32                                   NumberOfFiles,
  IN FRAMEWORK_EFI_FV_WRITE_POLICY            WritePolicy,
  IN FRAMEWORK_EFI_FV_WRITE_FILE_DATA         *FileData
);

typedef
EFI_STATUS
(EFIAPI *FRAMEWORK_EFI_FV_GET_NEXT_FILE)(
  IN EFI_FIRMWARE_VOLUME_PROTOCOL   *This,
  IN OUT VOID                       *Key,
  IN OUT EFI_FV_FILETYPE            *FileType,
  OUT EFI_GUID                      *NameGuid,
  OUT EFI_FV_FILE_ATTRIBUTES        *Attributes,
  OUT UINTN                         *Size
);

//
// Protocol interface structure
//
struct _EFI_FIRMWARE_VOLUME_PROTOCOL {
  ///
  /// Retrieves volume capabilities and current settings.
  ///
  FRAMEWORK_EFI_FV_GET_ATTRIBUTES GetVolumeAttributes;

  ///
  /// Modifies the current settings of the firmware volume.
  ///
  FRAMEWORK_EFI_FV_SET_ATTRIBUTES SetVolumeAttributes;

  ///
  /// Reads an entire file from the firmware volume.
  ///
  FRAMEWORK_EFI_FV_READ_FILE      ReadFile;

  ///
  /// Reads a single section from a file into a buffer.
  ///
  FRAMEWORK_EFI_FV_READ_SECTION   ReadSection;

  ///
  /// Writes an entire file into the firmware volume.
  ///
  FRAMEWORK_EFI_FV_WRITE_FILE     WriteFile;

  ///
  /// Provides service to allow searching the firmware volume.
  ///
  FRAMEWORK_EFI_FV_GET_NEXT_FILE  GetNextFile;

  ///
  ///  Data field that indicates the size in bytes of the Key input buffer for
  ///  the GetNextFile() API.
  ///
  UINT32                KeySize;

  ///
  ///  Handle of the parent firmware volume.
  ///
  EFI_HANDLE            ParentHandle;
};
