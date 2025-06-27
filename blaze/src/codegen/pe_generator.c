// PE EXECUTABLE GENERATOR FOR WINDOWS
// Builds 64-bit PE executables from raw machine code

#include "blaze_internals.h"

// Forward declarations
void write_pe_file(const char* filename, uint8_t* data, uint32_t size);
ssize_t write(int fd, const void* buf, size_t count);

// PE data types
typedef struct {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} IMAGE_FILE_HEADER;

typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} IMAGE_DATA_DIRECTORY;

typedef struct {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
} IMAGE_OPTIONAL_HEADER64;

// Import table structures
typedef struct {
    uint32_t OriginalFirstThunk;    // RVA to INT (Import Name Table)
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;                  // RVA to DLL name string
    uint32_t FirstThunk;            // RVA to IAT (Import Address Table)
} IMAGE_IMPORT_DESCRIPTOR;

typedef struct {
    union {
        uint64_t ForwarderString;
        uint64_t Function;
        uint64_t Ordinal;
        uint64_t AddressOfData;
    } u1;
} IMAGE_THUNK_DATA64;

typedef struct {
    uint16_t Hint;
    char Name[1];  // Variable length, null-terminated
} IMAGE_IMPORT_BY_NAME;

typedef struct {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} IMAGE_SECTION_HEADER;

// DOS header for PE files
typedef struct {
    uint16_t e_magic;    // Magic number "MZ"
    uint16_t e_cblp;     // Bytes on last page
    uint16_t e_cp;       // Pages in file
    uint16_t e_crlc;     // Relocations
    uint16_t e_cparhdr;  // Size of header in paragraphs
    uint16_t e_minalloc; // Minimum extra paragraphs needed
    uint16_t e_maxalloc; // Maximum extra paragraphs needed
    uint16_t e_ss;       // Initial SS value
    uint16_t e_sp;       // Initial SP value
    uint16_t e_csum;     // Checksum
    uint16_t e_ip;       // Initial IP value
    uint16_t e_cs;       // Initial CS value
    uint16_t e_lfarlc;   // Address of relocation table
    uint16_t e_ovno;     // Overlay number
    uint16_t e_res[4];   // Reserved
    uint16_t e_oemid;    // OEM identifier
    uint16_t e_oeminfo;  // OEM information
    uint16_t e_res2[10]; // Reserved
    uint32_t e_lfanew;   // Offset to PE header
} IMAGE_DOS_HEADER;

// PE constants
#define IMAGE_DOS_SIGNATURE    0x5A4D      // MZ
#define IMAGE_NT_SIGNATURE     0x00004550  // PE\0\0
#define IMAGE_FILE_MACHINE_AMD64 0x8664
#define IMAGE_SUBSYSTEM_CONSOLE  3
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002
#define IMAGE_FILE_LARGE_ADDRESS_AWARE 0x0020
#define IMAGE_SCN_CNT_CODE     0x00000020
#define IMAGE_SCN_MEM_EXECUTE  0x20000000
#define IMAGE_SCN_MEM_READ     0x40000000
#define IMAGE_SCN_MEM_WRITE    0x80000000
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040

// Data directory indices
#define IMAGE_DIRECTORY_ENTRY_IMPORT     1
#define IMAGE_DIRECTORY_ENTRY_IAT       12

// DOS stub program
const uint8_t dos_stub[] = {
    0x0E, 0x1F, 0xBA, 0x0E, 0x00, 0xB4, 0x09, 0xCD,
    0x21, 0xB8, 0x01, 0x4C, 0xCD, 0x21, 0x54, 0x68,
    0x69, 0x73, 0x20, 0x70, 0x72, 0x6F, 0x67, 0x72,
    0x61, 0x6D, 0x20, 0x63, 0x61, 0x6E, 0x6E, 0x6F,
    0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6E,
    0x20, 0x69, 0x6E, 0x20, 0x44, 0x4F, 0x53, 0x20,
    0x6D, 0x6F, 0x64, 0x65, 0x2E, 0x0D, 0x0D, 0x0A,
    0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Generate PE executable for Windows with import table
void generate_pe_executable(uint8_t* machine_code, uint32_t code_size,
                           const char* output_filename) {
    
    // Calculate sizes for sections
    uint32_t aligned_code_size = (code_size + 511) & ~511;  // Align to 512
    
    // Import data size calculation
    uint32_t import_data_size = sizeof(IMAGE_IMPORT_DESCRIPTOR) * 2 +  // 2 descriptors (kernel32 + null)
                               80 +  // Space for function names and hints
                               20 +  // "kernel32.dll\0"
                               sizeof(IMAGE_THUNK_DATA64) * 8;  // 4 functions * 2 (INT + IAT)
    uint32_t aligned_import_size = (import_data_size + 511) & ~511;
    
    // Headers size
    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + 
                           sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER) + 
                           sizeof(IMAGE_OPTIONAL_HEADER64) + 
                           sizeof(IMAGE_SECTION_HEADER) * 2;  // 2 sections now
    headers_size = (headers_size + 511) & ~511;
    
    // Virtual addresses
    uint32_t text_rva = 0x1000;
    uint32_t idata_rva = 0x2000;
    uint32_t image_size = 0x3000;
    
    // Allocate buffer
    uint8_t pe_buffer[65536];
    uint32_t offset = 0;
    
    // Clear buffer
    for (int i = 0; i < 65536; i++) pe_buffer[i] = 0;
    
    // 1. DOS Header
    IMAGE_DOS_HEADER* dos_header = (IMAGE_DOS_HEADER*)pe_buffer;
    dos_header->e_magic = IMAGE_DOS_SIGNATURE;
    dos_header->e_cblp = 0x90;
    dos_header->e_cp = 0x03;
    dos_header->e_cparhdr = 0x04;
    dos_header->e_minalloc = 0x00;
    dos_header->e_maxalloc = 0xFFFF;
    dos_header->e_sp = 0xB8;
    dos_header->e_lfarlc = 0x40;
    dos_header->e_lfanew = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub);
    offset += sizeof(IMAGE_DOS_HEADER);
    
    // 2. DOS Stub
    for (uint32_t i = 0; i < sizeof(dos_stub); i++) {
        pe_buffer[offset++] = dos_stub[i];
    }
    
    // 3. PE Signature
    *(uint32_t*)(pe_buffer + offset) = IMAGE_NT_SIGNATURE;
    offset += sizeof(uint32_t);
    
    // 4. File Header
    IMAGE_FILE_HEADER* file_header = (IMAGE_FILE_HEADER*)(pe_buffer + offset);
    file_header->Machine = IMAGE_FILE_MACHINE_AMD64;
    file_header->NumberOfSections = 2;  // .text and .idata
    file_header->TimeDateStamp = 0;
    file_header->PointerToSymbolTable = 0;
    file_header->NumberOfSymbols = 0;
    file_header->SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    file_header->Characteristics = IMAGE_FILE_EXECUTABLE_IMAGE | IMAGE_FILE_LARGE_ADDRESS_AWARE;
    offset += sizeof(IMAGE_FILE_HEADER);
    
    // 5. Optional Header
    IMAGE_OPTIONAL_HEADER64* opt_header = (IMAGE_OPTIONAL_HEADER64*)(pe_buffer + offset);
    opt_header->Magic = 0x20B;  // PE32+
    opt_header->MajorLinkerVersion = 14;
    opt_header->MinorLinkerVersion = 0;
    opt_header->SizeOfCode = aligned_code_size;
    opt_header->SizeOfInitializedData = aligned_import_size;
    opt_header->SizeOfUninitializedData = 0;
    opt_header->AddressOfEntryPoint = text_rva;  // Entry point in .text
    opt_header->BaseOfCode = text_rva;
    opt_header->ImageBase = 0x140000000;  // Default for 64-bit
    opt_header->SectionAlignment = 0x1000;
    opt_header->FileAlignment = 0x200;
    opt_header->MajorOperatingSystemVersion = 6;
    opt_header->MinorOperatingSystemVersion = 0;
    opt_header->MajorSubsystemVersion = 6;
    opt_header->MinorSubsystemVersion = 0;
    opt_header->SizeOfImage = image_size;
    opt_header->SizeOfHeaders = headers_size;
    opt_header->Subsystem = IMAGE_SUBSYSTEM_CONSOLE;
    opt_header->DllCharacteristics = 0x160;  // High entropy VA, NX compatible
    opt_header->SizeOfStackReserve = 0x100000;
    opt_header->SizeOfStackCommit = 0x1000;
    opt_header->SizeOfHeapReserve = 0x100000;
    opt_header->SizeOfHeapCommit = 0x1000;
    opt_header->NumberOfRvaAndSizes = 16;
    
    // Zero out data directories first
    for (int i = 0; i < 16; i++) {
        opt_header->DataDirectory[i].VirtualAddress = 0;
        opt_header->DataDirectory[i].Size = 0;
    }
    
    // Set import directory
    opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress = idata_rva;
    opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size = sizeof(IMAGE_IMPORT_DESCRIPTOR) * 2;
    
    // Set IAT directory
    opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].VirtualAddress = idata_rva + 0x40;
    opt_header->DataDirectory[IMAGE_DIRECTORY_ENTRY_IAT].Size = sizeof(IMAGE_THUNK_DATA64) * 4;
    
    offset += sizeof(IMAGE_OPTIONAL_HEADER64);
    
    // 6. Section Headers
    // .text section
    IMAGE_SECTION_HEADER* text_section = (IMAGE_SECTION_HEADER*)(pe_buffer + offset);
    text_section->Name[0] = '.';
    text_section->Name[1] = 't';
    text_section->Name[2] = 'e';
    text_section->Name[3] = 'x';
    text_section->Name[4] = 't';
    text_section->VirtualSize = code_size;
    text_section->VirtualAddress = text_rva;
    text_section->SizeOfRawData = aligned_code_size;
    text_section->PointerToRawData = headers_size;
    text_section->Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    offset += sizeof(IMAGE_SECTION_HEADER);
    
    // .idata section
    IMAGE_SECTION_HEADER* idata_section = (IMAGE_SECTION_HEADER*)(pe_buffer + offset);
    idata_section->Name[0] = '.';
    idata_section->Name[1] = 'i';
    idata_section->Name[2] = 'd';
    idata_section->Name[3] = 'a';
    idata_section->Name[4] = 't';
    idata_section->Name[5] = 'a';
    idata_section->VirtualSize = import_data_size;
    idata_section->VirtualAddress = idata_rva;
    idata_section->SizeOfRawData = aligned_import_size;
    idata_section->PointerToRawData = headers_size + aligned_code_size;
    idata_section->Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    offset += sizeof(IMAGE_SECTION_HEADER);
    
    // Pad headers
    while (offset < headers_size) {
        pe_buffer[offset++] = 0;
    }
    
    // 7. Code section (.text)
    // First, we need to patch the code to use IAT addresses
    // Copy the original code
    for (uint32_t i = 0; i < code_size; i++) {
        pe_buffer[offset + i] = machine_code[i];
    }
    
    // Pad code section
    uint32_t code_end = offset + code_size;
    while (offset + code_size < headers_size + aligned_code_size) {
        pe_buffer[offset + code_size++] = 0;
    }
    offset = headers_size + aligned_code_size;
    
    // 8. Import section (.idata)
    uint32_t import_base = offset;
    
    // Import descriptors
    IMAGE_IMPORT_DESCRIPTOR* import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(pe_buffer + offset);
    
    // kernel32.dll descriptor
    import_desc[0].OriginalFirstThunk = idata_rva + 0x40;  // INT RVA
    import_desc[0].Name = idata_rva + 0x80;                // DLL name RVA
    import_desc[0].FirstThunk = idata_rva + 0x60;          // IAT RVA
    
    // Null terminator
    import_desc[1].OriginalFirstThunk = 0;
    import_desc[1].Name = 0;
    import_desc[1].FirstThunk = 0;
    
    // Import Name Table (INT) at offset 0x40
    IMAGE_THUNK_DATA64* int_table = (IMAGE_THUNK_DATA64*)(pe_buffer + import_base + 0x40);
    int_table[0].u1.AddressOfData = idata_rva + 0x90;  // GetStdHandle
    int_table[1].u1.AddressOfData = idata_rva + 0xA0;  // WriteConsoleA
    int_table[2].u1.AddressOfData = idata_rva + 0xB0;  // ExitProcess
    int_table[3].u1.AddressOfData = 0;                 // Null terminator
    
    // Import Address Table (IAT) at offset 0x60
    IMAGE_THUNK_DATA64* iat_table = (IMAGE_THUNK_DATA64*)(pe_buffer + import_base + 0x60);
    iat_table[0].u1.AddressOfData = idata_rva + 0x90;  // GetStdHandle
    iat_table[1].u1.AddressOfData = idata_rva + 0xA0;  // WriteConsoleA
    iat_table[2].u1.AddressOfData = idata_rva + 0xB0;  // ExitProcess
    iat_table[3].u1.AddressOfData = 0;                 // Null terminator
    
    // DLL name at offset 0x80
    const char* dll_name = "kernel32.dll";
    for (int i = 0; dll_name[i]; i++) {
        pe_buffer[import_base + 0x80 + i] = dll_name[i];
    }
    
    // Function names with hints at offset 0x90
    // GetStdHandle
    IMAGE_IMPORT_BY_NAME* func1 = (IMAGE_IMPORT_BY_NAME*)(pe_buffer + import_base + 0x90);
    func1->Hint = 0;
    const char* name1 = "GetStdHandle";
    for (int i = 0; name1[i]; i++) {
        func1->Name[i] = name1[i];
    }
    
    // WriteConsoleA at offset 0xA0
    IMAGE_IMPORT_BY_NAME* func2 = (IMAGE_IMPORT_BY_NAME*)(pe_buffer + import_base + 0xA0);
    func2->Hint = 0;
    const char* name2 = "WriteConsoleA";
    for (int i = 0; name2[i]; i++) {
        func2->Name[i] = name2[i];
    }
    
    // ExitProcess at offset 0xB0
    IMAGE_IMPORT_BY_NAME* func3 = (IMAGE_IMPORT_BY_NAME*)(pe_buffer + import_base + 0xB0);
    func3->Hint = 0;
    const char* name3 = "ExitProcess";
    for (int i = 0; name3[i]; i++) {
        func3->Name[i] = name3[i];
    }
    
    // Calculate total size
    uint32_t total_size = headers_size + aligned_code_size + aligned_import_size;
    
    // 9. Write to file
    write_pe_file(output_filename, pe_buffer, total_size);
}

// Write PE file (platform-specific implementation needed)
void write_pe_file(const char* filename, uint8_t* data, uint32_t size) {
    // This would need platform-specific file I/O
    // For now, we'll use the syscall wrappers from ELF
    extern int syscall_open(const char* filename, int flags, int mode);
    extern int syscall_write(int fd, const void* buf, size_t count);
    extern int syscall_close(int fd);
    
    int fd = syscall_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (fd >= 0) {
        syscall_write(fd, data, size);
        syscall_close(fd);
        
        write(1, "PE executable generated: ", 25);
        write(1, filename, str_len(filename));
        write(1, "\n", 1);
    }
}

// Reuse string length from ELF
extern uint32_t str_len(const char* s);