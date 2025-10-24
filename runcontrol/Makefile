SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif
EPICS_BASE = $(firstword $(wildcard /usr/local/oag/base ../../epics-base ../../../../base $(HOME)/epics/base))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the SDDS-EPICS repository)
endif

include ../Makefile.rules

# Check if libCom was built with readline support by examining the library
# Only on Unix-like systems (Linux, Darwin) - Windows doesn't use readline
ifneq ($(OS),Windows)
  # First, try libCom.so/dylib with ldd/otool, then fall back to checking libCom.a with nm
  ifeq ($(OS),Darwin)
    LIBCOM_SO = $(wildcard $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.dylib)
  else
    LIBCOM_SO = $(wildcard $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.so)
  endif
  LIBCOM_A = $(wildcard $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a)

  ifneq ($(LIBCOM_SO),)
    # Check if libCom.so/dylib depends on libreadline
    # Darwin uses otool -L, Linux uses ldd
    ifeq ($(OS),Darwin)
      NEEDS_READLINE := $(shell otool -L $(LIBCOM_SO) 2>/dev/null | grep -q libreadline && echo yes)
    else
      NEEDS_READLINE := $(shell ldd $(LIBCOM_SO) 2>/dev/null | grep -q libreadline && echo yes)
    endif
  else ifneq ($(LIBCOM_A),)
    # Check if libCom.a has undefined readline symbols (rl_readline_name is a common one)
    # nm works on both Darwin and Linux
    NEEDS_READLINE := $(shell nm $(LIBCOM_A) 2>/dev/null | grep -q "U rl_readline_name" && echo yes)
  endif

  ifeq ($(NEEDS_READLINE),yes)
    READLINE_LIBS = -lreadline
  endif
endif

PROD = rctest
LIBRARY = runControl
LIBRARY_SRC = libruncontrol.cc pvaSDDS.cc

CCFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO

ifeq ($(OS), Linux)
  CCFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  LIBRARY_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrunControl
endif

ifeq ($(OS), Darwin)
  CCFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  LIBRARY_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS = -lrunControl
endif

ifeq ($(OS), Windows)
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(SDDS_REPO)/lzma -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -permissive -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  LIBRARY_LIBS += pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib gsl.lib gslcblas.lib  ws2_32.lib
  PROD_LIBS += runControl.lib pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib  ws2_32.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -LIBPATH:../lib/$(OS)-$(ARCH)
  NO_DLL = 1
endif

include ../Makefile.build


.NOTPARALLEL:
