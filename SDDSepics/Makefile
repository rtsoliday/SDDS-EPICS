SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif
EPICS_BASE = $(firstword $(wildcard /usr/local/oag/base ../../epics-base $(HOME)/epics/base))
ifeq ($(EPICS_BASE),)
  $(error EPICS base not found. Run 'git clone --recursive -b 7.0 https://github.com/epics-base/epics-base.git' next to the SDDS-EPICS repository)
endif

include ../Makefile.rules

PROD = MV20image2sdds sddssnapshot sddscontrollaw sddscaramp sddsgencontrolbits sddslogTurnByTurnDAQ \
       degauss standardize \
       sddssynchlog sddsmonitor sddsstatmon sddsvmonitor sddswmonitor BPS16monitor squishPVs \
       sddsalarmlog toggle sddslogger sddsfeedforward sdds2dfeedforward sddsglitchlogger sddsoptimize \
       sddspvtest sddslogonchange  sddswput sddsbcontrol sddscasr sddsimagemonitor makesrbump \
       sddsexperiment sddsvexperiment sddspvalogger sddswget sddspvasaverestore sddspermissive \
       sddspvaglitchlogger sddscainfo sddspvacontrollaw

sddssynchlog_SRC = sddssynchlog.c SDDSepics.c
sddsmonitor_SRC = sddsmonitor.c SDDSepics.c
sddsstatmon_SRC = sddsstatmon.c SDDSepics.c
sddsvmonitor_SRC = sddsvmonitor.c SDDSepics.c
sddswmonitor_SRC = sddswmonitor.cc SDDSepics.c pvaSDDS.cc
BPS16monitor_SRC = BPS16monitor.c SDDSepics.c
squishPVs_SRC = squishPVs.c SDDSepics.c
sddsalarmlog_SRC = sddsalarmlog.c SDDSepics.c
toggle_SRC = toggle.c SDDSepics.c
sddslogger_SRC = sddslogger.c SDDSepics.c
sddsfeedforward_SRC = sddsfeedforward.c SDDSepics.c
sdds2dfeedforward_SRC = sdds2dfeedforward.c SDDSepics.c
sddsglitchlogger_SRC = sddsglitchlogger.c SDDSepics.c
sddsoptimize_SRC = sddsoptimize.c SDDSepics.c
sddspvtest_SRC = sddspvtest.c SDDSepics.c
sddslogonchange_SRC = sddslogonchange.c SDDSepics.c
sddswput_SRC = sddswput.c SDDSepics.c
sddsbcontrol_SRC = sddsbcontrol.c SDDSepics.c
sddscasr_SRC = sddscasr.c SDDSepics.c
sddsimagemonitor_SRC = sddsimagemonitor.c SDDSepics.c
makesrbump_SRC = makesrbump.c SDDSepics.c
sddsexperiment_SRC = sddsexperiment.c SDDSepics.c
sddsvexperiment_SRC = sddsvexperiment.c SDDSepics.c
sddspvalogger_SRC = sddspvalogger.cc SDDSepics.c pvaSDDS.cc
sddswget_SRC = sddswget.cc SDDSepics.c pvaSDDS.cc
sddspvasaverestore_SRC = sddspvasaverestore.cc SDDSepics.c pvaSDDS.cc
sddspvaglitchlogger_SRC = sddspvaglitchlogger.cc SDDSepics.c pvaSDDS.cc
sddscainfo_SRC = sddscainfo.cc pvaSDDS.cc
sddspermissive_SRC = sddspermissive.cc pvaSDDS.cc
sddspvacontrollaw_SRC = sddspvacontrollaw.cc pvaSDDS.cc

ifneq ($(wildcard $(DD)../.svn),)
  SVN_VERSION=$(shell svnversion .)
  ifeq ($(SVN_VERSION),)
    SVN_VERSION=unknown
  endif
else
  SVN_VERSION=GIT_$(shell git log --oneline | wc -l | sed 's/^[[:space:]]*//')
endif

CFLAGS += -I$(EPICS_BASE)/include  -I$(SDDS_REPO)/include -DUSE_RUNCONTROL -I../runcontrol -I$(OBJ_DIR) -DSVN_VERSION=\"$(SVN_VERSION)\" -DACCESS_SECURITY -D_NO_PROTO
CCFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -DUSE_RUNCONTROL -I../runcontrol -I$(OBJ_DIR) -DSVN_VERSION=\"$(SVN_VERSION)\" -DACCESS_SECURITY -D_NO_PROTO

ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS) -DUSE_LOGDAEMON -I../logDaemon
  CCFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS) -DUSE_LOGDAEMON -I../logDaemon
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := -lrunControl -lLogD $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libnt.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccessCA.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvAccess.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvData.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libca.a \
    $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libCom.a \
    $(LZMA_LIB) $(GSL_LIB) $(GSLCBLAS_LIB) $(Z_LIB) -lreadline $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins -DUSE_LOGDAEMON -I../logDaemon
  CCFLAGS += -fpermissive -Wvla-cxx-extension -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins -DUSE_LOGDAEMON -I../logDaemon
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS := -lrunControl -lLogD $(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)/libpvaClient.a \
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
  PROD_LIBS += runControl.lib gsl.lib gslcblas.lib  ws2_32.lib
  PROD_LIBS_SDDS = mdbcommon.lib matlib.lib fftpack.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib \
                  pvaClient.lib nt.lib pvAccessCA.lib pvAccess.lib pvData.lib pvDatabase.lib ca.lib Com.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH) -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

include ../Makefile.build

$(OBJ_DIR)/sddssynchlog$(EXEEXT): $(sddssynchlog_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddssynchlog_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsmonitor$(EXEEXT): $(sddsmonitor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsmonitor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsstatmon$(EXEEXT): $(sddsstatmon_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsstatmon_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsvmonitor$(EXEEXT): $(sddsvmonitor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsvmonitor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddswmonitor$(EXEEXT): $(sddswmonitor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddswmonitor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/BPS16monitor$(EXEEXT): $(BPS16monitor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(BPS16monitor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/squishPVs$(EXEEXT): $(squishPVs_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(squishPVs_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsalarmlog$(EXEEXT): $(sddsalarmlog_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsalarmlog_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/toggle$(EXEEXT): $(toggle_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(toggle_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddslogger$(EXEEXT): $(sddslogger_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddslogger_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsfeedforward$(EXEEXT): $(sddsfeedforward_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsfeedforward_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sdds2dfeedforward$(EXEEXT): $(sdds2dfeedforward_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sdds2dfeedforward_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsglitchlogger$(EXEEXT): $(sddsglitchlogger_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsglitchlogger_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsoptimize$(EXEEXT): $(sddsoptimize_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsoptimize_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddspvtest$(EXEEXT): $(sddspvtest_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspvtest_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddslogonchange$(EXEEXT): $(sddslogonchange_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddslogonchange_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddswput$(EXEEXT): $(sddswput_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddswput_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsbcontrol$(EXEEXT): $(sddsbcontrol_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsbcontrol_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddscasr$(EXEEXT): $(sddscasr_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddscasr_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsimagemonitor$(EXEEXT): $(sddsimagemonitor_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsimagemonitor_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/makesrbump$(EXEEXT): $(makesrbump_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(makesrbump_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsexperiment$(EXEEXT): $(OBJ_DIR)/sddsexperiment.h $(sddsexperiment_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsexperiment_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsexperiment.h: sddsexperiment.nl
	$(SDDS_REPO)/bin/$(OS)-$(ARCH)/nlpp sddsexperiment.nl $(OBJ_DIR)/sddsexperiment.h

$(OBJ_DIR)/sddsvexperiment$(EXEEXT): $(OBJ_DIR)/sddsvexperiment.h $(sddsvexperiment_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddsvexperiment_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddsvexperiment.h: sddsvexperiment.nl
	$(SDDS_REPO)/bin/$(OS)-$(ARCH)/nlpp sddsvexperiment.nl $(OBJ_DIR)/sddsvexperiment.h

$(OBJ_DIR)/sddspvalogger$(EXEEXT): $(sddspvalogger_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspvalogger_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddswget$(EXEEXT): $(sddswget_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddswget_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddspvasaverestore$(EXEEXT): $(sddspvasaverestore_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspvasaverestore_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddspvaglitchlogger$(EXEEXT): $(sddspvaglitchlogger_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspvaglitchlogger_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi

$(OBJ_DIR)/sddscainfo$(EXEEXT): $(sddscainfo_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddscainfo_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
 
$(OBJ_DIR)/sddspermissive$(EXEEXT): $(sddspermissive_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspermissive_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
 
$(OBJ_DIR)/sddspvacontrollaw$(EXEEXT): $(sddspvacontrollaw_OBJS) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $(sddspvacontrollaw_OBJS) $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(PROD_SYS_LIBS)
	cp -f $@ $(BIN_DIR)/
	@if [ -n "$(EPICS_BIN_DIR)" ]; then echo cp -f $@ $(EPICS_BIN_DIR)/; fi
	@if [ -n "$(EPICS_BIN_DIR)" ]; then cp -f $@ $(EPICS_BIN_DIR)/; fi
