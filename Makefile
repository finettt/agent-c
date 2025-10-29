CC = gcc
TARGET = agent-c
RAG_ENABLED ?= 1

# Source files with conditional RAG support
SOURCES = src/main.c src/json.c src/agent.c src/cli.c src/utils.c src/args.c
ifneq ($(RAG_ENABLED),0)
SOURCES += src/rag.c
endif

# Object files in build directory
OBJECTS = $(addprefix build/, $(notdir $(SOURCES:.c=.o)))

# Detect OS - Windows compatible
UNAME := $(shell uname -s 2>/dev/null || echo Windows)
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
UNAME := Windows
endif
ifeq ($(findstring CYGWIN,$(UNAME)),CYGWIN)
UNAME := Windows
endif

# Optimized build flags for smallest size
CFLAGS_OPT = -std=c99 -D_POSIX_C_SOURCE=200809L -DRAG_ENABLED=$(RAG_ENABLED) -Oz -DNDEBUG \
             -ffunction-sections -fdata-sections \
             -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
             -fno-math-errno -ffast-math -fmerge-all-constants -flto \
             -fomit-frame-pointer -fno-ident -fno-stack-check \
             -fvisibility=hidden -fno-builtin

# Linker flags per-OS
ifeq ($(UNAME),Windows)
LDFLAGS_OPT = -Wl,--gc-sections -Wl,-s
else
LDFLAGS_OPT = -Wl,-dead_strip -Wl,-x -Wl,-S -Wl,-s
endif

# Auto-detect platform and build accordingly
all: $(OBJECTS)
	@echo "Building for $(UNAME)..."
	@$(MAKE) $(UNAME)

# Create build directory
build/%.o: src/%.c | build
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS_OPT) -c $< -o $@

build:
	@mkdir -p build

# Windows build (with UPX compression)
Windows: $(OBJECTS)
	@echo "Building optimized binary for Windows..."
	$(CC) $(CFLAGS_OPT) -o $(TARGET).exe $(OBJECTS) $(LDFLAGS_OPT)
	strip --strip-all $(TARGET).exe 2>/dev/null || strip $(TARGET).exe
	@echo "Applying UPX compression..."
	@which upx >/dev/null 2>&1 && upx --best $(TARGET).exe || echo "⚠️ UPX not found, binary uncompressed"
	@echo "✅ Windows build complete: $$(ls -lh $(TARGET).exe | awk '{print $$5}')"

# Linux build (with UPX compression)
Linux: $(OBJECTS)
	@echo "Building optimized binary for Linux..."
	$(CC) $(CFLAGS_OPT) -o $(TARGET) $(OBJECTS) $(LDFLAGS_OPT)
	strip --strip-all $(TARGET) 2>/dev/null || strip $(TARGET)
	@echo "Applying UPX compression..."
	@which upx >/dev/null 2>&1 && upx --best $(TARGET) || echo "⚠️ UPX not found, binary uncompressed"
	@echo "✅ Linux build complete: $$(ls -lh $(TARGET) | awk '{print $$5}')"

# macOS build (with GZEXE compression)
Darwin: $(OBJECTS)
	@echo "Building optimized binary for macOS..."
	$(CC) $(CFLAGS_OPT) -o $(TARGET) $(OBJECTS) $(LDFLAGS_OPT)
	strip -S -x $(TARGET) 2>/dev/null || strip $(TARGET)
	@echo "Applying GZEXE compression..."
	gzexe $(TARGET)
	@echo "✅ macOS build complete: $$(ls -lh $(TARGET) | awk '{print $$5}')"

clean:
	rm -rf build/ $(TARGET) $(TARGET)~ *~ $(TARGET).exe $(TARGET).exe~

install: all
	cp $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Show help information
help:
	@echo "🚀 Agent-C - A lightweight AI agent written in C"
	@echo ""
	@echo "📋 SIMPLE BUILD COMMANDS:"
	@echo "   make          Auto-detects platform and builds optimally"
	@echo "   make Windows  Windows build with UPX compression"
	@echo "   make Linux    Linux build with UPX compression"
	@echo "   make Darwin   macOS build with GZEXE compression"
	@echo "   make clean    Clean all build files"
	@echo "   make install  Install to /usr/local/bin"
	@echo "   make help     Show this help"
	@echo ""
	@echo "🔧 ADVANCED OPTIONS:"
	@echo "   make RAG_ENABLED=0  Build without RAG feature (smaller binary)"
	@echo "   make CFLAGS_OPT=\"...\"  Override compiler flags"

.PHONY: all Windows Linux Darwin clean install uninstall help
