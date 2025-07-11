# BLAZE COMPILER MAKEFILE
# Ultra low-level build - no dependencies

CC = gcc
# Using -O3 for maximum performance (now working after syscall fixes)
CFLAGS = -O3 -Wall -Wextra -fno-stack-protector -Iinclude -Isrc
# For -O1 optimized builds (now working):
# CFLAGS = -O1 -mcmodel=large -fno-section-anchors -fno-pic -Wall -Wextra -fno-stack-protector -Iinclude
LDFLAGS = -lm

# NOTE: Optimization status:
# -O0: Fully working (debug builds)
# -O1: Working after syscall clobber fixes (rcx, r11, memory added)
# -O2: Working after syscall clobber fixes
# -O3: Working after syscall clobber fixes - MAXIMUM PERFORMANCE (DEFAULT)

# Target
TARGET = blaze

# Source files
SRCS = src/blaze_compiler_main_clean.c \
       src/lexer/lexer_blaze_v2.c \
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
       src/codegen/codegen_platform_print.c \
       src/codegen/codegen_windows_console.c \
       src/codegen/codegen_output.c \
       src/codegen/scalable_codegen.c \
       src/codegen/elf_generator.c \
       src/codegen/pe_generator.c \
       src/runtime/memory_manager.c \
       src/runtime/temporal_gc.c \
       src/runtime/blaze_stdlib.c \
       src/runtime/blaze_start.c \
       src/gggx.c \
       src/gggx_integration.c

OBJS = $(SRCS:.c=.o)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LDFLAGS)
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
	       src/codegen/codegen_platform_print.c \
	       src/codegen/codegen_windows_console.c \
	       src/codegen/codegen_output.c \
	       src/codegen/elf_generator.c \
	       src/codegen/pe_generator.c \
	       src/runtime/memory_manager.c \
	       src/runtime/temporal_gc.c \
	       src/runtime/blaze_stdlib.c \
	       src/simple_sentry.c \
	       src/sentry_http.c

# Clean
clean:
	rm -f $(TARGET) $(TARGET)_debug $(OBJS)

# Run sample
run: $(TARGET)
	./$(TARGET)

# Test target
test: $(TARGET)
	@echo "Running Blaze compiler tests..."
	@echo "Test 1: Basic print"
	@echo 'print/ "Hello from test!" \' > test_basic.blaze
	@./$(TARGET) test_basic.blaze test_basic_out
	@./test_basic_out
	@echo "Test 1: Basic print completed."

# GGGX test target
gggx_test: test_gggx_simple.c
	$(CC) -O0 -g -o test_gggx_simple test_gggx_simple.c -lm
	./test_gggx_simple

# GGGX fine-grained control test
gggx_fine_test: test_gggx_fine_grained.c
	$(CC) -O0 -g -Iinclude -o test_gggx_fine_grained test_gggx_fine_grained.c src/gggx.c src/gggx_integration.c -lm
	./test_gggx_fine_grained

# Installation directories
PREFIX ?= /usr/local
DESTDIR ?=

# .PHONY update
.PHONY: all clean run debug test gggx_test gggx_fine_test install

install: $(TARGET)
	# Create directories if they don't exist and copy files
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/
	install -d $(DESTDIR)$(PREFIX)/include/blaze
	cp -r include/* $(DESTDIR)$(PREFIX)/include/blaze/
	install -d $(DESTDIR)$(PREFIX)/share/doc/blaze
	cp -r docs $(DESTDIR)$(PREFIX)/share/doc/blaze/