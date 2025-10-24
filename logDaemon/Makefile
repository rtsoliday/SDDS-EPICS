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

ifneq ($(OS), Windows)
PROD = logDaemon logMessage testLogger
LIBRARY = LogD
endif

LIBRARY_SRC = logDaemonLib.c BSlib.c

CFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DSDDS

ifeq ($(OS), Linux)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

ifeq ($(OS), Darwin)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

include ../Makefile.build


.NOTPARALLEL:
