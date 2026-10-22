SDDS_REPO = $(firstword $(wildcard ../../SDDS))
ifeq ($(SDDS_REPO),)
  $(error SDDS source code not found. Run 'git clone https://github.com/rtsoliday/SDDS.git' next to the SDDS-EPICS repository)
endif

EPICS_REQUIRED_LIBS := ca Com
ENABLE_OPENMP := 1
BUNDLE_OPENMP := 1
include ../Makefile.rules

READLINE_LIBS = $(EPICS_READLINE_LIBS)

#PROD = rampload ramploadnew setbcorr
PROD = rampload setbcorr
GSL_PROD = rampload ramploadnew setbcorr

CFLAGS += -I$(EPICS_BASE)/include  -I$(SDDS_REPO)/include -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO
override CXXFLAGS += -I$(EPICS_BASE)/include -I$(SDDS_REPO)/include -I$(OBJ_DIR) -DACCESS_SECURITY -D_NO_PROTO

ifeq ($(OS), Linux)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  override CXXFLAGS += -fpermissive -I$(EPICS_BASE)/include/compiler/gcc -I$(EPICS_BASE)/include/os/$(OS)
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) -fopenmp $(LDFLAGS)
  PROD_SYS_LIBS := $(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Darwin)
  CFLAGS += -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  override CXXFLAGS += -fpermissive -Wvla-cxx-extension -I$(EPICS_BASE)/include/compiler/clang -I$(EPICS_BASE)/include/os/$(OS) -Wno-overloaded-virtual -Wno-deprecated-builtins
  LDFLAGS := -L$(SDDS_REPO)/lib/$(OS)-$(ARCH) -L$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH) $(LDFLAGS)
  PROD_SYS_LIBS :=$(EPICS_LIBS) \
    $(LZMA_LIB) $(Z_LIB) $(READLINE_LIBS) $(PROD_SYS_LIBS)
  PROD_LIBS_SDDS = -lmdbcommon -lmatlib -lfftpack -lSDDS1 -lnamelist -lrpnlib -lmdbmth -lmdblib
endif

ifeq ($(OS), Windows)
  CFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  override CXXFLAGS += -wd4101 -wd4244 -wd4250 -wd4267 -wd4275 -w44355 -w44344 -w44251 -I$(EPICS_BASE)/include/compiler/msvc -I$(EPICS_BASE)/include/os/WIN32 -I$(SDDS_REPO)/lzma -permissive -D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING -DEPICS_CALL_DLL
  PROD_LIBS_SDDS = mdbcommon.lib matlib.lib fftpack.lib SDDS1.lib namelist.lib rpnlib.lib mdbmth.lib mdblib.lib lzma.lib z.lib
  PROD_SYS_LIBS += $(EPICS_LIBS) ws2_32.lib
  LIB_LINK_DIRS += -LIBPATH:$(SDDS_REPO)/lib/$(OS)-$(ARCH) -LIBPATH:$(EPICS_BASE)/lib/$(EPICS_HOST)-$(EPICS_ARCH)
endif

# The legacy ramploadnew utility is compiled for the test suite without being
# added to PROD. Overriding PROD would rewrite this component's build
# configuration and rebuild the production programs on the next build. MSVC
# lacks the <sys/time.h> header that ramploadnew includes.
ifneq ($(OS), Windows)
  LEGACY_TEST_PROD = ramploadnew
endif
LEGACY_TEST_TARGETS = $(addprefix $(OBJ_DIR)/,$(addsuffix $(EXEEXT),$(LEGACY_TEST_PROD)))
BUILD_OBJECTS += $(addprefix $(OBJ_DIR)/,$(addsuffix .$(OBJEXT),$(LEGACY_TEST_PROD)))

include ../Makefile.build

ifneq ($(LEGACY_TEST_TARGETS),)
$(LEGACY_TEST_TARGETS): %$(EXEEXT): %.$(OBJEXT) $(PROD_DEPS)
	$(LINKEXE) $(OUTPUTEXE) $< $(LDFLAGS) $(LIB_LINK_DIRS) $(PROD_LIBS) $(PROD_LIBS_SDDS) $(GSL_LIBS) $(PROD_SYS_LIBS)
endif

# Test-only programs stay in OBJ_DIR; they are not installed with PROD.
.PHONY: test-programs
test-programs: $(OBJ_DIR) $(LEGACY_TEST_TARGETS)
	@true


