SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif
EPICS_BASE = $(firstword $(wildcard /usr/local/oag/base ../../epics-base ../../../../base $(HOME)/epics/base))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the SDDS-EPICS repository)
endif

include ../Makefile.rules

ifneq ($(OS), Windows)
PROD = logDaemon logMessage testLogger
LIBRARY = LogD
endif

LIBRARY_SRC = logDaemonLib.c BSlib.c

CFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DSDDS

ifeq ($(OS), Linux)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

ifeq ($(OS), Darwin)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lreadline $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

include ../Makefile.build


.NOTPARALLEL:
