SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif

EPICS_REQUIRED_LIBS := pvaClient nt pvAccessCA pvAccess pvData ca Com
# Retain the compiler-default C++ mode for legacy Windows builds.
WINDOWS_CXX_STD_FLAGS :=
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../Makefile.rules

READLINE_LIBS = $(EPICS_READLINE_LIBS)

PROD = rctest
LIBRARY = runControl
LIBRARY_SRC = libruncontrol.cc pvaSDDS.cc

override CXXFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO

ifeq ($(OS), Linux)
  override CXXFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  LIBRARY_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrunControl
endif

ifeq ($(OS), Darwin)
  override CXXFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  LIBRARY_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrunControl
endif

ifeq ($(OS), Windows)
  override CXXFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(SDDS_REPO)/lzma -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -permissive -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += runControl.lib
  PROD_SYS_LIBS += $(EPICS_LIBS) ws2_32.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -LIBPATH:../lib/$(OS)-$(ARCH)
  NO_DLL = 1
endif

include ../Makefile.build


.NOTPARALLEL:
