# Build for the Vulkan project.
# Usage:
#   mingw32-make            debug build -> main.exe (incremental, add -jN for parallel)
#   mingw32-make release    optimized build -> main-release.exe (-O2, NDEBUG, no validation layers)
#   mingw32-make shaders    compile the Slang shaders to SPIR-V
#   mingw32-make clean      remove build artifacts
#
# Object files live in one flat directory per configuration (source path
# separators are replaced with dots) so no nested directories need to be
# created. This keeps the recipes portable across cmd.exe and sh on Windows.

CXX      := g++
CXXSTD   := -std=c++20
DEPFLAGS := -MMD -MP
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wno-missing-field-initializers -Winvalid-pch
INCLUDES := -Isrc -isystem "$(VULKAN_SDK)/Include" -isystem "$(GLFW_PATH)/include" -isystem ./include
LDFLAGS  := -L"$(GLFW_PATH)/lib-mingw-w64" -L"$(VULKAN_SDK)/Lib"
LDLIBS   := -lglfw3 -lvulkan-1 -lgdi32

DEBUG_TARGET  := main.exe
DEBUG_DIR     := build
DEBUG_FLAGS   := -g -O0 -D DEBUG

RELEASE_TARGET := main-release.exe
RELEASE_DIR    := build-release
RELEASE_FLAGS  := -O2 -D NDEBUG

SLANGC  := "$(VULKAN_SDK)/bin/slangc.exe"
SHADERS := slang.spv cull.spv

# Precompiled header. Each configuration builds its own .gch into its object
# directory, which is put on the include path ahead of src/ so that the
# force-included pch.hpp resolves to that configuration's .gch.
PCH_HDR := src/pch.hpp

# Recursively collect every .cpp under src/.
rwildcard = $(foreach d,$(wildcard $(1:=/*)),$(call rwildcard,$d,$2) $(filter $(subst *,%,$2),$d))
SRCS := $(call rwildcard,src,*.cpp)

# src/renderer/gpu/gpu_context.cpp -> <dir>/renderer.gpu.gpu_context.o
obj_of = $2/$(subst /,.,$(patsubst src/%.cpp,%,$1)).o

# Vendored sources are compiled without warnings.
warnings_for = $(if $(filter src/imgui/%,$1),,$(WARNINGS))

DEBUG_OBJS   := $(foreach s,$(SRCS),$(call obj_of,$s,$(DEBUG_DIR)))
RELEASE_OBJS := $(foreach s,$(SRCS),$(call obj_of,$s,$(RELEASE_DIR)))

.PHONY: all release shaders clean
all: $(DEBUG_TARGET) $(SHADERS)
release: $(RELEASE_TARGET) $(SHADERS)
shaders: $(SHADERS)

$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CXX) $(DEBUG_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

$(RELEASE_TARGET): $(RELEASE_OBJS)
	$(CXX) $(RELEASE_OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# $1 = object directory, $2 = configuration flags.
# Any change to pch.hpp (or a header it pulls in) rebuilds the .gch and, in
# turn, every object of that configuration.
define PCH_RULE
$1/pch.hpp.gch: $(PCH_HDR) | $1
	$(CXX) $(CXXSTD) $2 $(DEPFLAGS) $(WARNINGS) $(INCLUDES) -x c++-header $$< -o $$@

$1:
	mkdir $1
endef
$(eval $(call PCH_RULE,$(DEBUG_DIR),$(DEBUG_FLAGS)))
$(eval $(call PCH_RULE,$(RELEASE_DIR),$(RELEASE_FLAGS)))

# One compile rule per source and configuration so the flattened object name
# maps back to its (nested) source file.
# $1 = source, $2 = object directory, $3 = configuration flags.
define COMPILE_RULE
$(call obj_of,$1,$2): $1 $2/pch.hpp.gch | $2
	$(CXX) $(CXXSTD) $3 $(DEPFLAGS) $(call warnings_for,$1) -I$2 $(INCLUDES) -include pch.hpp -c $$< -o $$@
endef
$(foreach s,$(SRCS),$(eval $(call COMPILE_RULE,$s,$(DEBUG_DIR),$(DEBUG_FLAGS))))
$(foreach s,$(SRCS),$(eval $(call COMPILE_RULE,$s,$(RELEASE_DIR),$(RELEASE_FLAGS))))

slang.spv: shaders/shader.slang
	$(SLANGC) $< -target spirv -profile spirv_1_4 -capability nonuniformqualifier -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry entityVertMain -entry fragMain -o $@

cull.spv: shaders/cull.slang
	$(SLANGC) $< -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry cullMain -o $@

# src/pch.hpp.gch and src/pch.hpp.d are left over from older builds that put
# the precompiled header next to its source; remove them too.
# mingw32-make may run recipes through either a POSIX shell (e.g. w64devkit's
# sh) or cmd.exe depending on what is on PATH, so pick the delete syntax based
# on the shell actually in use rather than on the OS.
ifneq (,$(findstring sh,$(SHELL)))
clean:
	rm -rf "$(DEBUG_DIR)" "$(RELEASE_DIR)" "$(DEBUG_TARGET)" "$(RELEASE_TARGET)" $(SHADERS) src/pch.hpp.gch src/pch.hpp.d
else
clean:
	if exist "$(DEBUG_DIR)" rmdir /s /q "$(DEBUG_DIR)"
	if exist "$(RELEASE_DIR)" rmdir /s /q "$(RELEASE_DIR)"
	if exist "$(DEBUG_TARGET)" del /q "$(DEBUG_TARGET)"
	if exist "$(RELEASE_TARGET)" del /q "$(RELEASE_TARGET)"
	if exist slang.spv del /q slang.spv
	if exist cull.spv del /q cull.spv
	if exist src\pch.hpp.gch del /q src\pch.hpp.gch
	if exist src\pch.hpp.d del /q src\pch.hpp.d
endif

-include $(DEBUG_OBJS:.o=.d) $(RELEASE_OBJS:.o=.d)
-include $(DEBUG_DIR)/pch.hpp.d $(RELEASE_DIR)/pch.hpp.d
