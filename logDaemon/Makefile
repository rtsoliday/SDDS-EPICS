SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif

EPICS_REQUIRED_LIBS := Com
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../Makefile.rules

READLINE_LIBS = $(EPICS_READLINE_LIBS)

ifneq ($(OS), Windows)
PROD = logDaemon logMessage testLogger
LIBRARY = LogD
endif

GSL_PROD = logDaemon

LIBRARY_SRC = logDaemonLib.c BSlib.c

CFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DSDDS

ifeq ($(OS), Linux)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_LIBS) $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

ifeq ($(OS), Darwin)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_LIBS) $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

include ../Makefile.build


.NOTPARALLEL:
