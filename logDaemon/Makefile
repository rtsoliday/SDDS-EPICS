SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif
EPICS_BASE = $(firstword $(wildcard ../../epics-base $(USER)/epics/base))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the SDDS-EPICS repository)
endif

include ../Makefile.rules

PROD = logDaemon logMessage testLogger
LIBRARY = LogD
LIBRARY_SRC = logDaemonLib.c BSlib.c

CFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I. -I$(OBJ_DIR) -DSDDS

ifeq ($(OS), Linux)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lreadline $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

ifeq ($(OS), Darwin)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lreadline $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
  PROD_LIBS = -lLogD
endif

ifeq ($(OS), Windows)
  LIBRARY_LIBS += Com.lib gsl.lib gslcblas.lib
  LIB_LINK_DIRS += -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
  PROD_LIBS_SDDS = mdbcommon.lib matlib.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib \
                  Com.lib
  PROD_LIBS = LogD.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH) -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

include ../Makefile.build


.NOTPARALLEL:
