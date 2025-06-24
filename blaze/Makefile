# BLAZE COMPILER MAKEFILE
# Ultra low-level build - no dependencies

CC = gcc
# Default to -O0 for stability, -O1 now works with clobber fixes
CFLAGS = -O0 -Wall -Wextra -fno-stack-protector -Iinclude
# For -O1 optimized builds (now working):
# CFLAGS = -O1 -mcmodel=large -fno-section-anchors -fno-pic -Wall -Wextra -fno-stack-protector -Iinclude
LDFLAGS = -nostdlib -static

# NOTE: Optimization status:
# -O0: Fully working (default)
# -O1: Working after syscall clobber fixes (rcx, r11, memory added)
# -O2/-O3: Still crash on startup - needs further investigation

# Target
TARGET = blaze

# Source files
SRCS = src/blaze_compiler_main_clean.c \
       src/lexer/lexer_core.c \
       src/parser/parser_core.c \
       src/parser/symbol_table.c \
       src/parser/symbol_builder.c \
       src/parser/time_travel.c \
       src/codegen/codegen_basic.c \
       src/codegen/codegen_x64.c \
       src/codegen/codegen_x64_sse.c \
       src/codegen/codegen_x64_float_print.c \
       src/codegen/codegen_vars.c \
       src/codegen/codegen_func.c \
       src/codegen/codegen_math.c \
       src/codegen/codegen_solid.c \
       src/codegen/codegen_init_minimal.c \
       src/codegen/codegen_platform.c \
       src/codegen/codegen_output.c \
       src/codegen/scalable_codegen.c \
       src/codegen/elf_generator.c \
       src/codegen/pe_generator.c \
       src/runtime/memory_manager.c \
       src/runtime/temporal_gc.c \
       src/runtime/blaze_stdlib.c \
       src/runtime/blaze_start.c

OBJS = $(SRCS:.c=.o)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJS)
#	strip -s $(TARGET)

%.o: %.c include/blaze_internals.h include/blaze_types.h include/symbol_table_types.h include/scalable_codegen.h
	$(CC) $(CFLAGS) -c $< -o $@

# Test build with standard library (for debugging)
debug: include/blaze_internals.h
	$(CC) -O0 -g -DDEBUG -Iinclude -o $(TARGET)_debug src/blaze_compiler_main.c \
	       src/lexer/lexer_core.c \
	       src/parser/parser_core.c \
	       src/parser/symbol_table.c \
	       src/parser/symbol_builder.c \
	       src/parser/time_travel.c \
	       src/codegen/codegen_basic.c \
	       src/codegen/codegen_x64.c \
	       src/codegen/codegen_x64_sse.c \
	       src/codegen/codegen_x64_float_print.c \
	       src/codegen/codegen_vars.c \
	       src/codegen/codegen_func.c \
	       src/codegen/codegen_math.c \
	       src/codegen/codegen_solid.c \
	       src/codegen/codegen_init_minimal.c \
	       src/codegen/codegen_platform.c \
	       src/codegen/codegen_output.c \
	       src/codegen/elf_generator.c \
	       src/codegen/pe_generator.c \
	       src/runtime/memory_manager.c \
	       src/runtime/blaze_stdlib.c \
	       src/simple_sentry.c \
	       src/sentry_http.c

# Clean
clean:
	rm -f $(TARGET) $(TARGET)_debug $(OBJS)

# Run sample
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run debug