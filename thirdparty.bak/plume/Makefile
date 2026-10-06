# plume — PlayStation 5 (__PROSPERO__) static library build
#
# Usage:
#   make                        # Auto-detects or bootstraps PS5_PayloadSDK and builds build/libplume_ps5.a
#   make PS5_PAYLOAD_SDK=/path  # Uses an explicit PS5_PayloadSDK install prefix
#   make clean

SHELL := /bin/bash
.DEFAULT_GOAL := all

CLANG   := $(shell command -v clang   || command -v clang-18   || command -v clang-19   || command -v clang-17)
CLANGXX := $(shell command -v clang++ || command -v clang++-18 || command -v clang++-19 || command -v clang++-17)
LLVM_AR := $(shell command -v llvm-ar || command -v llvm-ar-18 || command -v llvm-ar-19 || command -v ar)

# Auto-detect PS5_PayloadSDK across common sibling directories if not explicitly set
ifeq ($(strip $(PS5_PAYLOAD_SDK)),)
  ifneq ($(wildcard $(CURDIR)/.deps/native/ps5-payload-sdk/target/include/ps5/kernel.h),)
    PS5_PAYLOAD_SDK := $(CURDIR)/.deps/native/ps5-payload-sdk
  else ifneq ($(wildcard $(CURDIR)/../ps5-plume-triangle/.deps/native/ps5-payload-sdk/target/include/ps5/kernel.h),)
    PS5_PAYLOAD_SDK := $(abspath $(CURDIR)/../ps5-plume-triangle/.deps/native/ps5-payload-sdk)
  else ifneq ($(wildcard $(CURDIR)/../UnleashedRecomp-Prospero/.deps/native/ps5-payload-sdk/target/include/ps5/kernel.h),)
    PS5_PAYLOAD_SDK := $(abspath $(CURDIR)/../UnleashedRecomp-Prospero/.deps/native/ps5-payload-sdk)
  else ifneq ($(wildcard $(CURDIR)/../UnleashedRecomp/.deps/native/ps5-payload-sdk/target/include/ps5/kernel.h),)
    PS5_PAYLOAD_SDK := $(abspath $(CURDIR)/../UnleashedRecomp/.deps/native/ps5-payload-sdk)
  else
    PS5_PAYLOAD_SDK := $(CURDIR)/.deps/native/ps5-payload-sdk
  endif
endif

SDK_REVISION := 95c08f27386fc698f6bbe21dde3030140a41d10b
SDK_REPO_URL := https://github.com/mihawk-99/PS5_PayloadSDK.git

COMMON_FLAGS := \
	-target x86_64-sie-ps5 \
	-fvisibility-nodllstorageclass=default \
	-O2 \
	-fPIC \
	-fno-plt \
	-fno-stack-protector \
	-femulated-tls \
	-fdenormal-fp-math=ieee \
	-msse4.1 \
	-mssse3 \
	-mcx16 \
	-isysroot "$(PS5_PAYLOAD_SDK)" \
	-isystem "$(PS5_PAYLOAD_SDK)/target/include/c++/v1" \
	-isystem "$(PS5_PAYLOAD_SDK)/target/include" \
	-I"$(CURDIR)" \
	-I"$(CURDIR)/contrib/Vulkan-Headers/include" \
	-I"$(CURDIR)/contrib/VulkanMemoryAllocator/include" \
	-I"$(CURDIR)/contrib/volk" \
	-I"$(CURDIR)/contrib/SPIRV-Reflect"

CFLAGS   := $(COMMON_FLAGS) -std=c11
CXXFLAGS := $(COMMON_FLAGS) -std=c++17 -DNDEBUG

BUILD_DIR := build
LIB_OUT   := $(BUILD_DIR)/libplume_ps5.a
OBJS      := \
	$(BUILD_DIR)/plume_vulkan.o \
	$(BUILD_DIR)/volk.o \
	$(BUILD_DIR)/spirv_reflect.o

.PHONY: all sdk clean

all: $(LIB_OUT) plume_vulkan.o

sdk:
	@if [[ ! -f "$(PS5_PAYLOAD_SDK)/target/include/ps5/kernel.h" ]]; then \
		echo "==> [sdk] Bootstrapping PS5_PayloadSDK into $(PS5_PAYLOAD_SDK)..."; \
		mkdir -p "$(CURDIR)/.deps/src" "$(dir $(PS5_PAYLOAD_SDK))"; \
		if [[ ! -d "$(CURDIR)/.deps/src/PS5_PayloadSDK/.git" ]]; then \
			git clone "$(SDK_REPO_URL)" "$(CURDIR)/.deps/src/PS5_PayloadSDK"; \
		fi; \
		git -C "$(CURDIR)/.deps/src/PS5_PayloadSDK" checkout -q "$(SDK_REVISION)"; \
		$(MAKE) -C "$(CURDIR)/.deps/src/PS5_PayloadSDK" DESTDIR="$(PS5_PAYLOAD_SDK)" install >/dev/null; \
	fi

$(BUILD_DIR)/plume_vulkan.o: plume_vulkan.cpp plume_vulkan.h plume_render_interface.h | sdk
	@mkdir -p "$(BUILD_DIR)"
	"$(CLANGXX)" $(CXXFLAGS) -c $< -o $@

plume_vulkan.o: $(BUILD_DIR)/plume_vulkan.o
	@cp -f $< $@

$(BUILD_DIR)/volk.o: contrib/volk/volk.c | sdk
	@mkdir -p "$(BUILD_DIR)"
	"$(CLANG)" $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/spirv_reflect.o: contrib/SPIRV-Reflect/spirv_reflect.c | sdk
	@mkdir -p "$(BUILD_DIR)"
	"$(CLANG)" $(CFLAGS) -c $< -o $@

$(LIB_OUT): $(OBJS)
	"$(LLVM_AR)" rcs $@ $(OBJS)
	@echo "==> Built $@ and plume_vulkan.o (using PS5_PAYLOAD_SDK=$(PS5_PAYLOAD_SDK))"

clean:
	rm -rf "$(BUILD_DIR)" plume_vulkan.o
