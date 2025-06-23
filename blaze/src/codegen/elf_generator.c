// ELF EXECUTABLE GENERATOR FOR LINUX
// Builds 64-bit ELF executables from raw machine code

#include "blaze_internals.h"

// Forward declaration
ssize_t write(int fd, const void* buf, size_t count);

// ELF data types
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

// ELF Header
typedef struct {
    unsigned char e_ident[16];  // Magic number and other info
    Elf64_Half    e_type;       // Object file type
    Elf64_Half    e_machine;    // Architecture
    Elf64_Word    e_version;    // Object file version
    Elf64_Addr    e_entry;      // Entry point virtual address
    Elf64_Off     e_phoff;      // Program header table file offset
    Elf64_Off     e_shoff;      // Section header table file offset
    Elf64_Word    e_flags;      // Processor-specific flags
    Elf64_Half    e_ehsize;     // ELF header size in bytes
    Elf64_Half    e_phentsize;  // Program header table entry size
    Elf64_Half    e_phnum;      // Program header table entry count
    Elf64_Half    e_shentsize;  // Section header table entry size
    Elf64_Half    e_shnum;      // Section header table entry count
    Elf64_Half    e_shstrndx;   // Section header string table index
} Elf64_Ehdr;

// Program header
typedef struct {
    Elf64_Word  p_type;     // Segment type
    Elf64_Word  p_flags;    // Segment flags
    Elf64_Off   p_offset;   // Segment file offset
    Elf64_Addr  p_vaddr;    // Segment virtual address
    Elf64_Addr  p_paddr;    // Segment physical address
    Elf64_Xword p_filesz;   // Segment size in file
    Elf64_Xword p_memsz;    // Segment size in memory
    Elf64_Xword p_align;    // Segment alignment
} Elf64_Phdr;

// ELF constants
#define EI_MAG0     0
#define EI_MAG1     1
#define EI_MAG2     2
#define EI_MAG3     3
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7
#define EI_PAD      8

#define ELFMAG0     0x7f
#define ELFMAG1     'E'
#define ELFMAG2     'L'
#define ELFMAG3     'F'

#define ELFCLASS64  2       // 64-bit
#define ELFDATA2LSB 1       // Little endian
#define EV_CURRENT  1       // Current version

#define ET_EXEC     2       // Executable file
#define EM_X86_64   62      // AMD x86-64 architecture

#define PT_LOAD     1       // Loadable segment
#define PF_X        1       // Execute
#define PF_W        2       // Write
#define PF_R        4       // Read

// Virtual address where we'll load our program
#define LOAD_ADDRESS 0x400000
#define PAGE_SIZE    0x1000

// Generate minimal ELF executable
void generate_elf_executable(uint8_t* machine_code, uint32_t code_size, 
                            const char* output_filename) {
    
    
    // Calculate alignments
    uint32_t code_pages = (code_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t file_size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) + code_size;
    
    // Use static buffer to avoid stack allocation
    static volatile uint8_t elf_buffer[65536];
    uint32_t offset = 0;
    
    
    // 1. Build ELF header
    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)elf_buffer;
    
    // Magic number
    ehdr->e_ident[EI_MAG0] = ELFMAG0;
    ehdr->e_ident[EI_MAG1] = ELFMAG1;
    ehdr->e_ident[EI_MAG2] = ELFMAG2;
    ehdr->e_ident[EI_MAG3] = ELFMAG3;
    
    // 64-bit, little endian, System V ABI
    ehdr->e_ident[EI_CLASS] = ELFCLASS64;
    ehdr->e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr->e_ident[EI_VERSION] = EV_CURRENT;
    ehdr->e_ident[EI_OSABI] = 0;  // System V
    
    // Zero padding
    for (int i = EI_PAD; i < 16; i++) {
        ehdr->e_ident[i] = 0;
    }
    
    ehdr->e_type = ET_EXEC;
    ehdr->e_machine = EM_X86_64;
    ehdr->e_version = EV_CURRENT;
    ehdr->e_entry = LOAD_ADDRESS + sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    ehdr->e_phoff = sizeof(Elf64_Ehdr);
    ehdr->e_shoff = 0;  // No section headers (minimal ELF)
    ehdr->e_flags = 0;
    ehdr->e_ehsize = sizeof(Elf64_Ehdr);
    ehdr->e_phentsize = sizeof(Elf64_Phdr);
    ehdr->e_phnum = 1;  // One program header
    ehdr->e_shentsize = 0;
    ehdr->e_shnum = 0;
    ehdr->e_shstrndx = 0;
    
    offset += sizeof(Elf64_Ehdr);
    
    // 2. Build program header
    Elf64_Phdr* phdr = (Elf64_Phdr*)(elf_buffer + offset);
    
    phdr->p_type = PT_LOAD;
    phdr->p_flags = PF_X | PF_R;  // Execute + Read
    phdr->p_offset = 0;
    phdr->p_vaddr = LOAD_ADDRESS;
    phdr->p_paddr = LOAD_ADDRESS;
    phdr->p_filesz = file_size;
    phdr->p_memsz = file_size;
    phdr->p_align = PAGE_SIZE;
    
    offset += sizeof(Elf64_Phdr);
    
    // 3. Copy machine code
    for (uint32_t i = 0; i < code_size; i++) {
        elf_buffer[offset + i] = machine_code[i];
    }
    
    // 4. Add exit syscall if not present
    if (code_size < 10 || machine_code[code_size-2] != 0x0F || 
        machine_code[code_size-1] != 0x05) {
        // mov rax, 60  ; exit syscall
        elf_buffer[offset + code_size] = 0xB8;
        elf_buffer[offset + code_size + 1] = 0x3C;
        elf_buffer[offset + code_size + 2] = 0x00;
        elf_buffer[offset + code_size + 3] = 0x00;
        elf_buffer[offset + code_size + 4] = 0x00;
        // xor edi, edi (32-bit xor to save space)
        elf_buffer[offset + code_size + 5] = 0x31;
        elf_buffer[offset + code_size + 6] = 0xFF;
        // syscall
        elf_buffer[offset + code_size + 7] = 0x0F;
        elf_buffer[offset + code_size + 8] = 0x05;
        file_size += 9;
    }
    
    // 5. Write to file using direct syscalls
    int fd = syscall_open(output_filename, O_CREAT | O_WRONLY | O_TRUNC, 0755);
    if (fd < 0) {
        write(2, "Error: Cannot create output file\n", 33);
        return;
    }
    
    syscall_write(fd, elf_buffer, file_size);
    syscall_close(fd);
    
}

// System call wrappers
int syscall_open(const char* filename, int flags, int mode) {
    int result;
    __asm__ volatile (
        "movq $2, %%rax\n"     // open syscall
        "syscall\n"
        : "=a" (result)
        : "D" (filename), "S" (flags), "d" (mode)
        : "rcx", "r11", "memory"
    );
    return result;
}

int syscall_write(int fd, const void* buf, size_t count) {
    int result;
    __asm__ volatile (
        "movq $1, %%rax\n"     // write syscall
        "syscall\n"
        : "=a" (result)
        : "D" (fd), "S" (buf), "d" (count)
        : "rcx", "r11", "memory"
    );
    return result;
}

int syscall_close(int fd) {
    int result;
    __asm__ volatile (
        "movq $3, %%rax\n"     // close syscall
        "syscall\n"
        : "=a" (result)
        : "D" (fd)
        : "rcx", "r11", "memory"
    );
    return result;
}

// String length
uint32_t str_len(const char* s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

// Flags for open()
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0100
#define O_TRUNC     01000