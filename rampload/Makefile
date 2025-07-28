SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif
EPICS_BASE = $(firstword $(wildcard /usr/local/oag/base ../../epics-base ../../../../base $(HOME)/epics/base))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the SDDS-EPICS repository)
endif

include ../Makefile.rules

#PROD = rampload ramploadnew setbcorr
PROD = rampload setbcorr

CFLAGS += -I$(EPICS_BASE)/include  -I$(SDDS_REPO)/include -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO
CCFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO

ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS) 
  CCFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins 
  CCFLAGS += -fpermissive -Wvla-cxx-extension -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins 
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS :=$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  CCFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -permissive -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS += gsl.lib gslcblas.lib  ws2_32.lib
  PROD_LIBS_SDDS = mdbcommon.lib matlib.lib fftpack.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib \
                  pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH) -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

include ../Makefile.build



