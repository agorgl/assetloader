#=- Makefile -=#
# CreateProcess NULL bug
ifeq ($(OS), Windows_NT)
	SHELL = cmd.exe
endif

#---------------------------------------------------------------
# Usage
#---------------------------------------------------------------
# The following generic Makefile is based on a specific project
# structure that makes the following conventions:
#
# The vanilla directory structure is in the form:
# - Root
#   - include [1]
#   - deps    [2]
#   - src     [3]
# Where:
#  [1] exists only for the library projects, and contains the header files
#  [2] exists only for the projects with external dependencies,
#      and each subfolder represents a dependency with a project
#      in the form we are currently documenting
#  [3] contains the source code for the project
#
# A working (built) project can also contain the following
# - Root
#   - bin [5]
#   - lib [6]
#   - tmp [7]
# Where:
#  [5] contains binary results of the linking process (executables, dynamic libraries)
#  [6] contains archive results of the archiving process (static libraries)
#  [7] contains intermediate object files of compiling processes

#---------------------------------------------------------------
# Build variable parameters
#---------------------------------------------------------------
# Variant = (Debug|Release)
VARIANT ?= Debug
QUIET ?= 0

#---------------------------------------------------------------
# Per project configuration
#---------------------------------------------------------------
# Should at least define:
# - PRJTYPE variable (Executable|StaticLib|DynLib)
# - LIBS variable (optional, Executable type only)
# Can optionally define:
# - TARGETNAME variable (project name, defaults to name of the root folder)
# - SRCDIR variable (source directory)
# - BUILDDIR variable (intermediate build directory)
# - SRC variable (list of the source files, defaults to every code file in SRCDIR)
# - SRCEXT variable (list of extensions used to match source files)
# - DEFINES variable (list defines in form of PROPERTY || PROPERTY=VALUE)
# - ADDINCS variable (list with additional include dirs)
# - MOREDEPS variable (list with additional dep dirs)
include Makefile.conf

# Defaults
TARGETNAME ?= $(notdir $(CURDIR))
SRCDIR ?= src
BUILDDIR ?= tmp
SRCEXT = *.c *.cpp *.cc *.cxx
SRC ?= $(call rwildcard, $(SRCDIR), $(SRCEXT))

# Target directory
ifeq ($(PRJTYPE), StaticLib)
	TARGETDIR = lib
else
	TARGETDIR = bin
endif

#---------------------------------------------------------------
# Helpers
#---------------------------------------------------------------
# Recursive wildcard func
rwildcard = $(foreach d, $(wildcard $1*), $(call rwildcard, $d/, $2) $(filter $(subst *, %, $2), $d))

# Suppress command errors
ifeq ($(OS), Windows_NT)
	suppress_out = > nul 2>&1 || (exit 0)
else
	suppress_out => /dev/null 2>&1 || true
endif

# Native paths
ifeq ($(OS), Windows_NT)
	native_path = $(subst /,\,$(1))
else
	native_path = $(1)
endif

# Makedir command
MKDIR_CMD = mkdir
mkdir = -$(if $(wildcard $(1)/.*), , $(MKDIR_CMD) $(call native_path, $(1)) $(suppress_out))

# Rmdir command
RMDIR_CMD = rmdir /s /q
rmdir = $(if $(wildcard $(1)/.*), $(RMDIR_CMD) $(call native_path, $(1)),)

# Lowercase
lc = $(subst A,a,$(subst B,b,$(subst C,c,$(subst D,d,$(subst E,e,$(subst F,f,$(subst G,g,\
	$(subst H,h,$(subst I,i,$(subst J,j,$(subst K,k,$(subst L,l,$(subst M,m,$(subst N,n,\
	$(subst O,o,$(subst P,p,$(subst Q,q,$(subst R,r,$(subst S,s,$(subst T,t,$(subst U,u,\
	$(subst V,v,$(subst W,w,$(subst X,x,$(subst Y,y,$(subst Z,z,$1))))))))))))))))))))))))))

# Quiet execution of command
quiet = $(1) $(suppress_out)

# Os executable extension
ifeq ($(OS), Windows_NT)
	EXECEXT = .exe
endif

#---------------------------------------------------------------
# Generated values
#---------------------------------------------------------------
# Objects
OBJ = $(foreach obj, $(SRC:=.o), $(BUILDDIR)/$(VARIANT)/$(obj))

# Static library extension and prefix
SLIBEXT = .a
SLIBPREF = lib
# Output
ifeq ($(PRJTYPE), StaticLib)
	TARGET = $(SLIBPREF)$(strip $(call lc,$(TARGETNAME)))$(SLIBEXT)
else
	TARGET = $(TARGETNAME)$(EXECEXT)
endif

# Dependencies
DEPSDIR = deps
DEPS = $(strip $(sort $(dir $(wildcard $(DEPSDIR)/*/)))) $(MOREDEPS)
DEPNAMES = $(strip $(foreach d, $(DEPS), $(lastword $(subst /, , $d))))
# Include directories (implicit)
INCDIR = $(strip -Iinclude $(foreach dep, $(DEPS), -I$(dep)include))
# Include directories (explicit)
INCDIR += $(strip $(foreach addinc, $(ADDINCS), -I$(addinc)))
# Library flags
LIBFLAGS = $(strip $(foreach lib, $(LIBS), -l$(lib)))

#---------------------------------------------------------------
# Toolchain dependent values
#---------------------------------------------------------------
# Compiler
CC = gcc
CXX = g++
CPPFLAGS = $(strip $(foreach define, $(DEFINES), -D$(define)))
# Archiver
AR = ar
ARFLAGS = rcs
# Linker
LD = gcc
LDFLAGS = -static -static-libgcc

#---------------------------------------------------------------
# Variant dependent values
#---------------------------------------------------------------
# Library directories
libdir = $(strip $(foreach dep, $(DEPS), -L$(dep)lib/$(strip $(1))))
# Compiler flags
compiler_flags = $(strip $(if $(filter $(1), Debug), -g -O0, -O2) -Wall -Wextra)

#---------------------------------------------------------------
# Command generator functions
#---------------------------------------------------------------
ccompile = $(CC) $$(CFLAGS) $$(CPPFLAGS) $$(INCDIR) -c $$< -o $$@
cxxcompile = $(CXX) $$(CFLAGS) $$(CPPFLAGS) $$(INCDIR) -c $$< -o $$@
link = $(LD) $(LDFLAGS) $(LIBDIR) -o $@ $^ $(LIBFLAGS)
archive = $(AR) $(ARFLAGS) -o $@ $?

#---------------------------------------------------------------
# Rules
#---------------------------------------------------------------
# Disable builtin rules
.SUFFIXES:

# Main build rule
build: variables $(OBJ) $(TARGETDIR)/$(VARIANT)/$(TARGET)

# Full build rule
all: deps build

# Executes target
run: build
	$(eval exec = $(TARGETDIR)/$(VARIANT)/$(TARGET))
	@echo Executing $(exec) ...
	@$(exec)

# Set variables for current build execution
variables:
	@echo Making $(VARIANT) build...
	$(eval CFLAGS = $(call compiler_flags, $(VARIANT)))
	$(eval LIBDIR = $(call libdir, $(VARIANT)))

# Print build debug info
showvars: variables
	@echo DEPS: $(DEPNAMES)
	@echo CFLAGS: $(CFLAGS)
	@echo LIBDIR: $(LIBDIR)

# Link rule
%$(EXECEXT): $(OBJ)
	@echo [+] Linking $@
	@$(call mkdir, $(@D))
	$(eval lcommand = $(link))
	@$(lcommand)

# Archive rule
%$(SLIBEXT): $(OBJ)
	@echo [+] Archiving $@
	@$(call mkdir, $(@D))
	$(eval lcommand = $(archive))
	@$(lcommand)

# $(eval lcommand = $(call archive, $<, $@))
#
# Compile rules
#
define compile-rule
$(BUILDDIR)/$(VARIANT)/%.$(strip $(1)).o: %.$(strip $(1))
	@echo [^>] Compiling $$<
	@$$(call mkdir, $$(@D))
	@$(2)
endef
# Generate compile rules
$(eval $(call compile-rule, c, $(ccompile)))
$(foreach ext, cpp cxx cc, $(eval $(call compile-rule, $(ext), $(cxxcompile))))

# Cleanup rule
clean:
	@echo Cleaning...
	@$(call rmdir, $(BUILDDIR))

# Build rule template (1=Name, 2=Dir)
define build-rule
build-$(strip $(1)):
	@echo ===================================
	@echo Building $(strip $(1))
	@echo ===================================
	@$(MAKE) -C $(2)/$(strip $(1)) -f ../../$(firstword $(MAKEFILE_LIST)) all
endef

# Generate dependency build rules
$(foreach dep, $(DEPNAMES), $(eval $(call build-rule, $(dep), $(DEPSDIR))))
# Dependencies build rule
deps: $(foreach dep, $(DEPNAMES), build-$(dep))

# Clean rule template (1=Name, 2=Dir)
define clean-rule
clean-$(strip $(1)):
	@echo Cleaning $(strip $(1))
	@$(MAKE) -C $(2)/$(strip $(1)) -f ../../$(firstword $(MAKEFILE_LIST)) clean
endef

# Generate dependency clean rules
$(foreach dep, $(DEPNAMES), $(eval $(call clean-rule, $(dep), $(DEPSDIR))))
# Depencencies clean rule
depsclean: $(foreach dep, $(DEPNAMES), clean-$(dep))

# Non file targets
.PHONY: all \
		build \
		run \
		variables \
		showvars \
		clean \
		depsclean \
		deps \
		$(foreach dep, $(DEPNAMES), build-$(dep) clean-$(dep)) \
