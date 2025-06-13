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
                type of PV to create. The value values are:\n\
                char, uchar, short, ushort, int, uint, float, double, string.\n\
                The default is double.\n\
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
Link date: "__DATE__
                       " "__TIME__
                       ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

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
extern "C" void CleanMasterSDDSpcasPVFile();
extern "C" void rc_interrupt_handler();
int runControlPingNoSleep();

char *PVFile2 = NULL;

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
  signal(SIGHUP, signal_handler);
  signal(SIGQUIT, signal_handler);
  signal(SIGTRAP, signal_handler);
  signal(SIGBUS, signal_handler);
#endif
  if (PVFile2 != NULL) {
    atexit(CleanMasterSDDSpcasPVFile);
  }

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

  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Error forking process\n");
    exit(1);
  } else if (pid == 0) {
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
    exit(1);
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
