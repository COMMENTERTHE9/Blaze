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

// Generate PE executable for Windows
void generate_pe_executable(uint8_t* machine_code, uint32_t code_size,
                           const char* output_filename) {
    
    // Align sizes
    uint32_t aligned_code_size = (code_size + 511) & ~511;  // Align to 512
    uint32_t headers_size = sizeof(IMAGE_DOS_HEADER) + sizeof(dos_stub) + 
                           sizeof(uint32_t) + sizeof(IMAGE_FILE_HEADER) + 
                           sizeof(IMAGE_OPTIONAL_HEADER64) + sizeof(IMAGE_SECTION_HEADER);
    headers_size = (headers_size + 511) & ~511;  // Align headers too
    
    // Allocate buffer
    uint8_t pe_buffer[65536];
    uint32_t offset = 0;
    
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
    file_header->NumberOfSections = 1;  // Just .text
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
    opt_header->SizeOfInitializedData = 0;
    opt_header->SizeOfUninitializedData = 0;
    opt_header->AddressOfEntryPoint = 0x1000;  // RVA of entry point
    opt_header->BaseOfCode = 0x1000;
    opt_header->ImageBase = 0x140000000;  // Default for 64-bit
    opt_header->SectionAlignment = 0x1000;
    opt_header->FileAlignment = 0x200;
    opt_header->MajorOperatingSystemVersion = 6;
    opt_header->MinorOperatingSystemVersion = 0;
    opt_header->MajorSubsystemVersion = 6;
    opt_header->MinorSubsystemVersion = 0;
    opt_header->SizeOfImage = 0x2000;  // Must be multiple of SectionAlignment
    opt_header->SizeOfHeaders = headers_size;
    opt_header->Subsystem = IMAGE_SUBSYSTEM_CONSOLE;
    opt_header->DllCharacteristics = 0x160;  // High entropy VA, NX compatible
    opt_header->SizeOfStackReserve = 0x100000;
    opt_header->SizeOfStackCommit = 0x1000;
    opt_header->SizeOfHeapReserve = 0x100000;
    opt_header->SizeOfHeapCommit = 0x1000;
    opt_header->NumberOfRvaAndSizes = 16;
    
    // Zero out data directories
    for (int i = 0; i < 16; i++) {
        opt_header->DataDirectory[i].VirtualAddress = 0;
        opt_header->DataDirectory[i].Size = 0;
    }
    offset += sizeof(IMAGE_OPTIONAL_HEADER64);
    
    // 6. Section Header (.text)
    IMAGE_SECTION_HEADER* text_section = (IMAGE_SECTION_HEADER*)(pe_buffer + offset);
    text_section->Name[0] = '.';
    text_section->Name[1] = 't';
    text_section->Name[2] = 'e';
    text_section->Name[3] = 'x';
    text_section->Name[4] = 't';
    text_section->Name[5] = '\0';
    text_section->VirtualSize = code_size;
    text_section->VirtualAddress = 0x1000;
    text_section->SizeOfRawData = aligned_code_size;
    text_section->PointerToRawData = headers_size;
    text_section->Characteristics = IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_READ;
    offset += sizeof(IMAGE_SECTION_HEADER);
    
    // Pad to headers_size
    while (offset < headers_size) {
        pe_buffer[offset++] = 0;
    }
    
    // 7. Copy machine code with Windows adjustments
    for (uint32_t i = 0; i < code_size; i++) {
        pe_buffer[offset + i] = machine_code[i];
    }
    
    // Add Windows exit if not present
    // Windows x64 calling convention is different!
    // mov rcx, 0     ; exit code
    // mov rax, [gs:0x60]  ; PEB
    // mov rax, [rax+0x18] ; PEB_LDR_DATA  
    // call ExitProcess (would need to import)
    // For now, simple ret
    if (code_size < 1 || machine_code[code_size-1] != 0xC3) {
        pe_buffer[offset + code_size] = 0x48;     // xor rcx, rcx
        pe_buffer[offset + code_size + 1] = 0x31;
        pe_buffer[offset + code_size + 2] = 0xC9;
        pe_buffer[offset + code_size + 3] = 0xC3; // ret
        code_size += 4;
    }
    
    // Pad code section
    while (code_size < aligned_code_size) {
        pe_buffer[offset + code_size++] = 0;
    }
    
    // 8. Write to file
    write_pe_file(output_filename, pe_buffer, headers_size + aligned_code_size);
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