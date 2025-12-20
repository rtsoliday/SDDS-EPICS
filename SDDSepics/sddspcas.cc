/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <complex>
#include "mdb.h"
#include "envDefs.h"

#include "sddspcasServer.h"
#include "fdManager.h"
#include <signal.h>
#include "SDDS.h"
#include "scan.h"

#ifndef _WIN32
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  include <fcntl.h>
#  include <errno.h>
#  include <float.h>
#  include <string>
#  include <vector>
#endif

#ifdef USE_RUNCONTROL

#  include "cadef.h"
#  include "libruncontrol.h"
#endif
#include <epicsVersion.h>

#define SET_DEBUGLEVEL 0
#define SET_EXECUTIONTIME 1
#define SET_PVPREFIX 2
#define SET_ALIASCOUNT 3
#define SET_NOISE 4
#define SET_ARRAYSIZE 5
#define SET_SYNCSCAN 6
#define SET_MASTERPVFILE 7
#define SET_PCASPVFILE 8
#define SET_RUNCONTROLPV 9
#define SET_RUNCONTROLDESC 10
#define SET_STANDALONE 11
#define N_OPTIONS 12

char *option[N_OPTIONS] = {
  (char *)"debuglevel", (char *)"executiontime",
  (char *)"pvprefix", (char *)"aliascount",
  (char *)"noise", (char *)"arraysize",
  (char *)"syncscan", (char *)"masterpvfile",
  (char *)"pcaspvfile",
  (char *)"runControlPV", (char *)"runControlDescription",
  (char *)"standalone"};

char *USAGE1 = (char *)"sddspcas <inputfiles> \n\
[-masterPVFile=<filename>] \n\
[-pcasPVFile=<filename>] \n\
[-debugLevel=<debug level>] \n\
[-executionTime=<execution time>] \n\
[-pvPrefix=<PV name prefix>] \n\
[-noise=<rate>] \n\
[-runControlPV=string=<string>,pingTimeout=<value>] \n\
[-runControlDescription=string=<string>] [-standalone]\n\n\
sddspcas is a portable channel access server that is configured\n\
by SDDS input files.\n\
<inputfiles>    SDDS files with a ControlName column that contains\n\
                the names of the process variables that will be created.\n\
                The Hopr and Lopr columns are optional. These are used\n\
                to set the upper and lower limits.\n\
                The ReadbackUnits column is optional. If this column\n\
                does not exist, the PVs do not have any units associated\n\
                with them.\n\
                The ElementCount column is optional. This is used to create\n\
                waveform PVs with various elements.\n\
                The Type column is optional. This is used to specify the\n\
                type of PV to create. The valid values are:\n\
                char, uchar, short, ushort, int, uint, float, double, string, enum.\n\
                The default is double.\n\
                The EnumStrings column is optional, except it is required\n\
                when Type=enum. It is a comma-separated list of state\n\
                strings (1..16). Quotes are not supported and empty states\n\
                are not allowed. Type=enum must have ElementCount=1.\n\
                The Equation column is optional. It will automatically update PV\n\
                values based on other PV values. An example might look like:\n\
                ( ca:FirstPV + ca:SecondPV ) / 100.0\n";
char *USAGE2 = (char *)"-masterPVFile   SDDS file containing all the IOC process variables.\n\
                default: /home/helios/iocinfo/pvdata/all/iocRecNames.sdds\n\
                This file is checked to ensure that no duplicate PVs\n\
                are created.\n\
-pcasPVFile     SDDS file containing all the process variables created by\n\
                the sddspcas program.\n\
                default: /home/helios/iocinfo/oagData/pvdata/iocRecNames.sdds\n\
                This file is checked to ensure that no duplicate PVs\n\
                are created.\n\
-standalone     The masterPVFile and pcasPVFile are not used unless they are\n\
                explicitly given.\n\
-executionTime  The number of seconds the server will run. (default=12 hours, 0=forever)\n\
-pvPrefix       Prefix used on all process variables.\n\
-noise          Random noise is added to the process variables and updated\n\
                at the given rate.\n\
-debugLevel     The debugging level.\n\
-runControlPV   Specifies a runControl PV name.\n\
-runControlDescription\n\
                Specifies a runControl PV description record.\n\n\
SDDS version of program by Robert Soliday\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifndef USE_RUNCONTROL
char *USAGE_WARNING = (char *)"** note ** Program is not compiled with run control.\n";
#else
char *USAGE_WARNING = (char *)"";
#endif

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PVparam, *DescParam;
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;
RUNCONTROL_PARAM rcParam;
#endif

extern "C" void signal_handler(int sig);
#ifndef _WIN32
extern "C" void sigchld_handler(int sig);
#endif
extern "C" void CleanMasterSDDSpcasPVFile();
extern "C" void rc_interrupt_handler();
int runControlPingNoSleep();

char *PVFile2 = NULL;

#ifndef _WIN32
static int gIpcListenFd = -1;
static std::string gIpcSocketPath;
static pid_t gIpcOwnerPid = (pid_t)-1;
static volatile sig_atomic_t gGotSigchld = 0;

static std::string buildIpcSocketPath() {
  const char *runtimeDir = getenv("XDG_RUNTIME_DIR");
  char buf[256];
  if (runtimeDir && runtimeDir[0]) {
    snprintf(buf, sizeof(buf), "%s/sddspcas.%ld.sock", runtimeDir, (long)getuid());
  } else {
    snprintf(buf, sizeof(buf), "/tmp/sddspcas.%ld.sock", (long)getuid());
  }
  return std::string(buf);
}

static void ipcCleanup() {
  if (gIpcListenFd >= 0) {
    close(gIpcListenFd);
    gIpcListenFd = -1;
  }
  if (gIpcOwnerPid != (pid_t)-1 && getpid() != gIpcOwnerPid) {
    return;
  }
  if (!gIpcSocketPath.empty()) {
    unlink(gIpcSocketPath.c_str());
  }
}

static bool writeAll(int fd, const void *buf, size_t n) {
  const char *p = (const char *)buf;
  while (n) {
    ssize_t w = write(fd, p, n);
    if (w < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    p += (size_t)w;
    n -= (size_t)w;
  }
  return true;
}

static bool readAll(int fd, void *buf, size_t n) {
  char *p = (char *)buf;
  while (n) {
    ssize_t r = read(fd, p, n);
    if (r == 0)
      return false;
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return false;
    }
    p += (size_t)r;
    n -= (size_t)r;
  }
  return true;
}

static void setCloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  if (flags >= 0)
    fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

static std::vector<SddspcasPvDef> readPvDefsFromInputFiles(char **input, long inputfiles) {
  std::vector<SddspcasPvDef> defs;
  for (long i = 0; i < inputfiles; i++) {
    SDDS_DATASET SDDS_input;
    int hoprFound = 0, loprFound = 0, unitsFound = 0, elementCountFound = 0, typeFound = 0, enumStringsFound = 0;
    uint32_t rows;

    if (!SDDS_InitializeInput(&SDDS_input, input[i])) {
      fprintf(stderr, "error: Unable to read SDDS input file %s\n", input[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "ControlName") == -1) {
      fprintf(stderr, "error: string column ControlName does not exist in %s\n", input[i]);
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Hopr") >= 0)
      hoprFound = 1;
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Lopr") >= 0)
      loprFound = 1;
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "ReadbackUnits") >= 0)
      unitsFound = 1;
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_INTEGER_TYPE, (char *)"ElementCount") >= 0)
      elementCountFound = 1;
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "Type") >= 0)
      typeFound = 1;
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "EnumStrings") >= 0)
      enumStringsFound = 1;

    if (SDDS_ReadPage(&SDDS_input) != 1) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", input[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if ((rows = (uint32_t)SDDS_RowCount(&SDDS_input)) < 1) {
      fprintf(stderr, "error: No rows found in SDDS file %s\n", input[i]);
      exit(1);
    }

    char **cn = (char **)SDDS_GetColumn(&SDDS_input, (char *)"ControlName");
    char **ru = unitsFound ? (char **)SDDS_GetColumn(&SDDS_input, (char *)"ReadbackUnits") : NULL;
    char **ty = typeFound ? (char **)SDDS_GetColumn(&SDDS_input, (char *)"Type") : NULL;
    char **es = enumStringsFound ? (char **)SDDS_GetColumn(&SDDS_input, (char *)"EnumStrings") : NULL;
    double *ho = hoprFound ? SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Hopr") : NULL;
    double *lo = loprFound ? SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Lopr") : NULL;
    uint32_t *ec = elementCountFound ? (uint32_t *)SDDS_GetColumnInLong(&SDDS_input, (char *)"ElementCount") : NULL;

    if (!cn) {
      fprintf(stderr, "error: Unable to read ControlName column from %s\n", input[i]);
      exit(1);
    }

    for (uint32_t j = 0; j < rows; j++) {
      SddspcasPvDef d;
      d.controlName = cn[j] ? cn[j] : "";
      d.type = (ty && ty[j]) ? ty[j] : "double";
      d.enumStrings = (es && es[j]) ? es[j] : "";
      d.units = (ru && ru[j]) ? ru[j] : "";
      d.hopr = ho ? ho[j] : DBL_MAX;
      d.lopr = lo ? lo[j] : -DBL_MAX;
      d.elementCount = ec ? ec[j] : 1u;
      defs.push_back(d);
    }

    for (uint32_t j = 0; j < rows; j++) {
      if (cn && cn[j])
        free(cn[j]);
      if (ru && ru[j])
        free(ru[j]);
      if (ty && ty[j])
        free(ty[j]);
      if (es && es[j])
        free(es[j]);
    }
    if (cn)
      free(cn);
    if (ru)
      free(ru);
    if (ty)
      free(ty);
    if (es)
      free(es);
    if (ho)
      free(ho);
    if (lo)
      free(lo);
    if (ec)
      free(ec);

    SDDS_Terminate(&SDDS_input);
  }
  return defs;
}

static bool anyInputHasEquationColumn(char **input, long inputfiles) {
  for (long i = 0; i < inputfiles; i++) {
    SDDS_DATASET SDDS_input;
    if (!SDDS_InitializeInput(&SDDS_input, input[i])) {
      fprintf(stderr, "error: Unable to read SDDS input file %s\n", input[i]);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE, SDDS_STRING, "Equation") >= 0) {
      SDDS_Terminate(&SDDS_input);
      return true;
    }
    SDDS_Terminate(&SDDS_input);
  }
  return false;
}

static bool ipcTryForwardAddRequests(const std::string &sockPath, const std::vector<SddspcasPvDef> &defs) {
  if (defs.empty())
    return false;

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return false;
  setCloexec(fd);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (sockPath.size() >= sizeof(addr.sun_path)) {
    close(fd);
    return false;
  }
  strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return false;
  }

  struct timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  const uint32_t magic = 0x53504341u; /* 'SPCA' */
  const uint32_t version = 1u;
  uint32_t count = (uint32_t)defs.size();
  if (!writeAll(fd, &magic, sizeof(magic)) || !writeAll(fd, &version, sizeof(version)) || !writeAll(fd, &count, sizeof(count))) {
    close(fd);
    return false;
  }

  for (size_t i = 0; i < defs.size(); i++) {
    const SddspcasPvDef &d = defs[i];
    uint32_t nameLen = (uint32_t)d.controlName.size();
    uint32_t typeLen = (uint32_t)d.type.size();
    uint32_t unitsLen = (uint32_t)d.units.size();
    uint32_t enumLen = (uint32_t)d.enumStrings.size();
    uint32_t ec = (uint32_t)d.elementCount;
    if (!writeAll(fd, &nameLen, sizeof(nameLen)) || !writeAll(fd, &typeLen, sizeof(typeLen)) ||
        !writeAll(fd, &unitsLen, sizeof(unitsLen)) || !writeAll(fd, &enumLen, sizeof(enumLen)) ||
        !writeAll(fd, &ec, sizeof(ec)) ||
        !writeAll(fd, &d.hopr, sizeof(d.hopr)) || !writeAll(fd, &d.lopr, sizeof(d.lopr))) {
      close(fd);
      return false;
    }
    if (nameLen && !writeAll(fd, d.controlName.data(), nameLen)) {
      close(fd);
      return false;
    }
    if (typeLen && !writeAll(fd, d.type.data(), typeLen)) {
      close(fd);
      return false;
    }
    if (unitsLen && !writeAll(fd, d.units.data(), unitsLen)) {
      close(fd);
      return false;
    }
    if (enumLen && !writeAll(fd, d.enumStrings.data(), enumLen)) {
      close(fd);
      return false;
    }
  }

  close(fd);
  return true;
}

static int ipcSetupListener(const std::string &sockPath) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;
  setCloexec(fd);

  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (sockPath.size() >= sizeof(addr.sun_path)) {
    close(fd);
    return -1;
  }
  strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

  unlink(sockPath.c_str());
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  chmod(sockPath.c_str(), 0600);

  if (listen(fd, 8) != 0) {
    close(fd);
    unlink(sockPath.c_str());
    return -1;
  }
  return fd;
}

static void ipcPump(exServer *pCAS) {
  if (!pCAS || gIpcListenFd < 0)
    return;

  while (true) {
    int cfd = accept(gIpcListenFd, NULL, NULL);
    if (cfd < 0) {
      if (errno == EINTR)
        continue;
      if (errno == EAGAIN || errno == EWOULDBLOCK)
        return;
      return;
    }
    setCloexec(cfd);
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(cfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint32_t magic = 0, version = 0, count = 0;
    if (!readAll(cfd, &magic, sizeof(magic)) || !readAll(cfd, &version, sizeof(version)) || !readAll(cfd, &count, sizeof(count))) {
      close(cfd);
      continue;
    }
    if (magic != 0x53504341u || version != 1u || count > 100000u) {
      close(cfd);
      continue;
    }

    std::vector<SddspcasPvDef> defs;
    defs.reserve(count);
    for (uint32_t i = 0; i < count; i++) {
      uint32_t nameLen = 0, typeLen = 0, unitsLen = 0, enumLen = 0, ec = 0;
      double hopr = DBL_MAX, lopr = -DBL_MAX;
      if (!readAll(cfd, &nameLen, sizeof(nameLen)) || !readAll(cfd, &typeLen, sizeof(typeLen)) ||
          !readAll(cfd, &unitsLen, sizeof(unitsLen)) || !readAll(cfd, &enumLen, sizeof(enumLen)) ||
          !readAll(cfd, &ec, sizeof(ec)) || !readAll(cfd, &hopr, sizeof(hopr)) || !readAll(cfd, &lopr, sizeof(lopr))) {
        defs.clear();
        break;
      }
      if (nameLen > 4096u || typeLen > 128u || unitsLen > 1024u || enumLen > 4096u) {
        defs.clear();
        break;
      }

      SddspcasPvDef d;
      d.hopr = hopr;
      d.lopr = lopr;
      d.elementCount = ec;

      if (nameLen) {
        std::string s(nameLen, '\0');
        if (!readAll(cfd, &s[0], nameLen)) {
          defs.clear();
          break;
        }
        d.controlName = s;
      }
      if (typeLen) {
        std::string s(typeLen, '\0');
        if (!readAll(cfd, &s[0], typeLen)) {
          defs.clear();
          break;
        }
        d.type = s;
      } else {
        d.type = "double";
      }
      if (unitsLen) {
        std::string s(unitsLen, '\0');
        if (!readAll(cfd, &s[0], unitsLen)) {
          defs.clear();
          break;
        }
        d.units = s;
      }
      if (enumLen) {
        std::string s(enumLen, '\0');
        if (!readAll(cfd, &s[0], enumLen)) {
          defs.clear();
          break;
        }
        d.enumStrings = s;
      }
      if (d.elementCount == 0)
        d.elementCount = 1;
      defs.push_back(d);
    }

    if (!defs.empty()) {
      unsigned added = pCAS->addPVs(defs);
      if (added) {
        fprintf(stderr, "sddspcas: added %u PV(s) via IPC\n", added);
      }
    }

    close(cfd);
  }
}
#endif

//
// main()
// (example single threaded ca server tool main loop)
//
extern int main(int argc, const char **argv) {
  epicsTime begin(epicsTime::getCurrent());
  exServer *pCAS;
  uint32_t debugLevel = 0u;
  double executionTime = 43200.0; /* 12 hours */
  char pvPrefix[128] = "";
  uint32_t aliasCount = 0u;
  uint32_t scanOn = false;
  uint32_t syncScan = true;
  char arraySize[64] = "";
  bool forever = false;
  char **input = NULL;
  long inputfiles = 0;
  char *PVFile1 = NULL;
  double rate = -1.0;
  int standalone = 0;
  long i_arg;
  unsigned long dummyFlags;
  SCANNED_ARG *s_arg;
  pid_t pid;

#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = rcParam.PVparam = rcParam.DescParam = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, (char **)argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    fprintf(stderr, "%s", USAGE_WARNING);
    exit(1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_DEBUGLEVEL:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -debugLevel syntax");
        if (sscanf(s_arg[i_arg].list[1], "%u", &debugLevel) != 1)
          SDDS_Bomb((char *)"invalid -debugLevel syntax or value");
        break;
      case SET_EXECUTIONTIME:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -executionTime syntax");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &executionTime) != 1)
          SDDS_Bomb((char *)"invalid -executionTime syntax or value");
        if (executionTime < .5) {
          forever = true;
          executionTime = 0.0;
        }
        break;
      case SET_PVPREFIX:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -pvPrefix syntax");
        if (sscanf(s_arg[i_arg].list[1], "%127s", pvPrefix) != 1)
          SDDS_Bomb((char *)"invalid -pvPrefix syntax or value");
        break;
      case SET_ALIASCOUNT:
        SDDS_Bomb((char *)"error: -aliasCount is not supported in the version of sddspcas");
        /*
                if (s_arg[i_arg].n_items<2)
                SDDS_Bomb((char*)"invalid -aliasCount syntax");
                if (sscanf(s_arg[i_arg].list[1], "%u", &aliasCount)!=1)
                SDDS_Bomb((char*)"invalid -aliasCount syntax or value");
              */
        break;
      case SET_NOISE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -noise syntax");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &rate) != 1)
          SDDS_Bomb((char *)"invalid -noise syntax or value");
        scanOn = true;
        break;
      case SET_ARRAYSIZE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -arraySize syntax");
        if (sscanf(s_arg[i_arg].list[1], "%63s", arraySize) != 1)
          SDDS_Bomb((char *)"invalid -arraySize syntax or value");
        break;
      case SET_SYNCSCAN:
        SDDS_Bomb((char *)"error: -syncScan is not supported in the version of sddspcas");
        /*
                if (s_arg[i_arg].n_items<2)
                SDDS_Bomb((char*)"invalid -syncScan syntax");
                if (sscanf(s_arg[i_arg].list[1], "%u", &syncScan)!=1)
                SDDS_Bomb((char*)"invalid -syncScan syntax or value");
              */
        break;
      case SET_MASTERPVFILE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -masterPVFile syntax");
        SDDS_CopyString(&PVFile1, s_arg[i_arg].list[1]);
        break;
      case SET_PCASPVFILE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -pcasPVFile syntax");
        SDDS_CopyString(&PVFile2, s_arg[i_arg].list[1]);
        break;
      case SET_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PV)) {
          fprintf(stderr, "bad -runControlPV syntax\n");
          exit(1);
        }
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.\n");
          exit(1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case SET_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc)) {
          fprintf(stderr, "bad -runControlDesc syntax\n");
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      case SET_STANDALONE:
        standalone = 1;
        break;
      default:
        fprintf(stderr, "error: unknown switch: %s\n", s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      inputfiles++;
      input = (char **)trealloc(input, inputfiles * sizeof(*input));
      SDDS_CopyString(&(input[inputfiles - 1]), s_arg[i_arg].list[0]);
    }
  }
  if (input == NULL) {
    fprintf(stderr, "error: no input files given\n");
    exit(1);
  }

#ifndef _WIN32
  gIpcSocketPath = buildIpcSocketPath();
  {
    std::vector<SddspcasPvDef> defs = readPvDefsFromInputFiles(input, inputfiles);
    if (ipcTryForwardAddRequests(gIpcSocketPath, defs)) {
      fprintf(stderr, "sddspcas: forwarded %lu PV(s) to running instance; exiting\n", (unsigned long)defs.size());
      free_scanargs(&s_arg, argc);
      return 0;
    }
  }
#endif
  if ((PVFile1 == NULL) && (standalone == 0)) {
    SDDS_CopyString(&PVFile1, (char *)"/home/helios/iocinfo/pvdata/all/iocRecNames.sdds");
  }
  if ((PVFile2 == NULL) && (standalone == 0)) {
    SDDS_CopyString(&PVFile2, (char *)"/home/helios/iocinfo/oagData/pvdata/iocRecNames.sdds");
  }

  if (arraySize[0] != '\0') {
    epicsEnvSet("EPICS_CA_MAX_ARRAY_BYTES", arraySize);
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    rcParam.handle[0] = (char)0;
    rcParam.Desc = (char *)realloc(rcParam.Desc, 41 * sizeof(char));
    rcParam.PV = (char *)realloc(rcParam.PV, 41 * sizeof(char));
    rcParam.status = runControlInit(rcParam.PV,
                                    rcParam.Desc,
                                    rcParam.pingTimeout,
                                    rcParam.handle,
                                    &(rcParam.rcInfo),
                                    10.0);
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      exit(1);
    }
    atexit(rc_interrupt_handler);
  }
  if (rcParam.PV) {
    /* ping once at the beginning */
    if (runControlPingNoSleep()) {
      fprintf(stderr, "Problem pinging the run control record.\n");
      exit(1);
    }
  }
  if (rcParam.PV) {
    strcpy(rcParam.message, "Starting");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      exit(1);
    }
  }

#endif

  try {
    pCAS = new exServer(pvPrefix, aliasCount,
                        scanOn != 0, syncScan == 0,
                        inputfiles, input, PVFile1, PVFile2, rate);
  } catch (...) {
    errlogPrintf("Server initialization error\n");
    errlogFlush();
    return (-1);
  }

  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGILL, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGFPE, signal_handler);
  signal(SIGSEGV, signal_handler);
#ifndef _WIN32
  signal(SIGCHLD, sigchld_handler);
  signal(SIGHUP, signal_handler);
  signal(SIGQUIT, signal_handler);
  signal(SIGTRAP, signal_handler);
  signal(SIGBUS, signal_handler);
#endif
  if (PVFile2 != NULL) {
    atexit(CleanMasterSDDSpcasPVFile);
  }

#ifndef _WIN32
  gIpcListenFd = ipcSetupListener(gIpcSocketPath);
  if (gIpcListenFd >= 0) {
    gIpcOwnerPid = getpid();
    int flags = fcntl(gIpcListenFd, F_GETFL);
    if (flags >= 0)
      fcntl(gIpcListenFd, F_SETFL, flags | O_NONBLOCK);
    atexit(ipcCleanup);
  } else {
    /* Non-fatal: server still runs; dynamic add via IPC disabled. */
    fprintf(stderr, "warning: unable to create sddspcas IPC socket %s\n", gIpcSocketPath.c_str());
  }
#endif

  pCAS->setDebugLevel(debugLevel);

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    atexit(rc_interrupt_handler);
    strcpy(rcParam.message, "Running");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      exit(1);
    }
  }
#endif

  bool launchEquations = anyInputHasEquationColumn(input, inputfiles);
  if (!launchEquations) {
    pid = 1;
  } else {
    pid = fork();
  }
  if (launchEquations && pid < 0) {
    fprintf(stderr, "Error forking process\n");
    exit(1);
  } else if (launchEquations && pid == 0) {
    // Child process
    std::string fileNames;
  
    for (int i = 0; i < inputfiles; i++) {
      fileNames += input[i];
      if (i < inputfiles - 1) {
        fileNames += ",";
      }
    }
    if (debugLevel > 0) {
      execlp("sddspcasEquations", "sddspcasEquations", "-debug", "1", "-input", fileNames.c_str(), NULL);
    } else {
      execlp("sddspcasEquations", "sddspcasEquations", "-input", fileNames.c_str(), NULL);
    }
    fprintf(stderr, "Error executing sddspcasEquations script\n");
    _exit(1);
  } else {
    if (forever) {
      //
      // loop here forever
      //
      while (true) {
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          runControlPingNoSleep();
        }
#endif
        fileDescriptorManager.process(1000.0);
      #ifndef _WIN32
        if (gGotSigchld) {
          int status;
          gGotSigchld = 0;
          while (waitpid(-1, &status, WNOHANG) > 0) {
          }
        }
        ipcPump(pCAS);
      #endif
      }
    } else {
      double delay = epicsTime::getCurrent() - begin;
      //
      // loop here until the specified execution time
      // expires
      //
      while (delay < executionTime) {
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          runControlPingNoSleep();
        }
#endif
        fileDescriptorManager.process(1000.0);
      #ifndef _WIN32
        if (gGotSigchld) {
          int status;
          gGotSigchld = 0;
          while (waitpid(-1, &status, WNOHANG) > 0) {
          }
        }
        ipcPump(pCAS);
      #endif
        delay = epicsTime::getCurrent() - begin;
      }
    }
    
    pCAS->show(2u);
    delete pCAS;
    errlogFlush();
    
    free_scanargs(&s_arg, argc);
  }
  return (0);
}

extern "C" void signal_handler(int sig) {
  exit(1);
}

#ifndef _WIN32
extern "C" void sigchld_handler(int sig) {
  (void)sig;
  gGotSigchld = 1;
}
#endif

extern "C" void CleanMasterSDDSpcasPVFile() {
  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL, **host_name = NULL;
  char hostname[255];

  gethostname(hostname, 255);

  if (!SDDS_InitializeInput(&SDDS_masterlist, PVFile2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", PVFile2);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  rows = SDDS_SetRowsOfInterest(&SDDS_masterlist, (char *)"host_name", SDDS_MATCH_STRING, hostname, SDDS_NEGATE_MATCH | SDDS_AND);
  if (rows == -1) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }

  if (rows > 0) {
    if (!(rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", PVFile2);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
    if (!(host_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"host_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", PVFile2);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
  }
  SDDS_Terminate(&SDDS_masterlist);

  if (!SDDS_InitializeOutput(&SDDS_masterlist, SDDS_BINARY, 1, NULL, NULL, PVFile2)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_DefineSimpleColumn(&SDDS_masterlist, (char *)"host_name", NULL, SDDS_STRING)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_DefineSimpleColumn(&SDDS_masterlist, (char *)"rec_name", NULL, SDDS_STRING)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_WriteLayout(&SDDS_masterlist)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_StartTable(&SDDS_masterlist, rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (rows > 0) {
    if (!SDDS_SetColumn(&SDDS_masterlist, SDDS_BY_NAME, host_name, rows, (char *)"host_name")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
    if (!SDDS_SetColumn(&SDDS_masterlist, SDDS_BY_NAME, rec_name, rows, (char *)"rec_name")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return;
    }
  }
  if (!SDDS_WriteTable(&SDDS_masterlist)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  for (int m = 0; m < rows; m++) {
    free(rec_name[m]);
  }
  if (rec_name)
    free(rec_name);
  for (int m = 0; m < rows; m++) {
    free(host_name[m]);
  }
  if (host_name)
    free(host_name);

  return;
}

#ifdef USE_RUNCONTROL
void rc_interrupt_handler() {
  if (rcParam.PV) {
    rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error during exiting run control.\n");
      return;
    }
    rcParam.PV = NULL;
  }
  return;
}

/* returns value from same list of statuses as other runcontrol calls */
int runControlPingNoSleep() {
  rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
  switch (rcParam.status) {
  case RUNCONTROL_ABORT:
    fprintf(stderr, "Run control application aborted.\n");
    exit(1);
    break;
  case RUNCONTROL_TIMEOUT:
    fprintf(stderr, "Run control application timed out.\n");
    strcpy(rcParam.message, "Application timed-out");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    exit(1);
    break;
  case RUNCONTROL_OK:
    break;
  case RUNCONTROL_ERROR:
    fprintf(stderr, "Communications error with runcontrol record.\n");
    return (RUNCONTROL_ERROR);
  default:
    fprintf(stderr, "Unknown run control error code.\n");
    break;
  }
  return (RUNCONTROL_OK);
}
#endif
