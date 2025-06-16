/**
 * @file sddstransferscalartowaveform.c
 * @brief Update waveform PVs using scalar values from a file.
 *
 * The program reads an input file describing waveform and scalar PVs and
 * periodically updates the waveform data using the specified scalar PVs.
 * It can run continuously or for a set number of steps and supports
 * optional run control.
 *
 * @section Usage
 * ```
 * sddswput <inputfile>
 *     [-pendIOtime=<seconds>] [-interval=<seconds>]
 *     [-runControl=pv=<string>,pingTimeOut=<value>,pingTimeInterval=<value>,description=<string>]
 *     [-dryRun] [-verbose] [-steps=<integer>] [-infiniteLoop]
 *     [-prefix=<string>] [-suffix=<string>] [-watchInput]
 * ```
 *
 * @section Options
 * | Option        | Description |
 * |---------------|-------------|
 * | `-pendIOtime` | Channel Access timeout in seconds. |
 * | `-interval`   | Delay between updates. |
 * | `-runControl` | Run control PV information. |
 * | `-dryRun`     | Parse inputs but do not update PVs. |
 * | `-verbose`    | Print progress messages. |
 * | `-steps`      | Number of iterations to perform. |
 * | `-infiniteLoop`| Run indefinitely. |
 * | `-prefix`     | Prefix for scalar PV names. |
 * | `-suffix`     | Suffix for scalar PV names. |
 * | `-watchInput` | Reload input when the file changes. |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * M. Borland, H. Shang, R. Soliday
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <time.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
/*#include <chandata.h>*/
#include <signal.h>
#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif
#include <alarm.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#  include <sysLib.h>
#endif

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#if defined(DBAccess)
#  include "dbDefs.h"
#  include "dbAccess.h"
#  include "errlog.h"
#  include "link.h"
#else
#  include <cadef.h>
#endif
#include <epicsVersion.h>

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

void interrupt_handler(int sig);
void interrupt_handler2(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

#define CLO_PENDIOTIME 0
#define CLO_INTERVAL 1
#define CLO_RUNCONTROL 2
#define CLO_DRY_RUN 3
#define CLO_VERBOSE 4
#define CLO_STEPS 5
#define CLO_INFINITELOOP 6
#define CLO_PREFIX 7
#define CLO_SUFFIX 8
#define CLO_WATCHINPUT 9
#define COMMANDLINE_OPTIONS 10
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "pendiotime", "interval", "runControl", "dryRun", "verbose", "steps", "infiniteLoop", "prefix", "suffix", "watchInput"};

static char *USAGE1 = "sddswput <inputfile>\n\
    [-pendIOtime=<seconds>] [-interval=<seconds>] [-runControl=pv=<string>,pingTimeOut=<value>,pingTimeInterval=<value>,description=<string>] \n\
    [-dryRun] [-verbose] [-steps=<integer>] [-infiniteLoop] [-prefix=<string>] [-suffix=<string>] \n\n\
update the FFwaveform from scalar FF pvs..\n\
<inputfile>        A file like that contains WaveformPV name, DeviceName and Index column.\n\
                   and with optional Prefix, Suffix, Suffix1, and IsSum parameters for scalar pvs.\n\
                   the scalar pv name will be obtained as <Prefix><DeviceName><Suffix> \n\
                   If IsSum parameter is 1, then Suffix1 must be provided; the waveform value will be the sum of \n\
                   <Prefix><DeviceName><Suffix> and <Prefix><DeviceName><Suffix1> \n\
                   This is designed for transfer the scalar setpoint+offset to vector adjust waveform \n\
interval           provide the updating interval \n\
runcontrol         provide runcontrol pv information \n\
dryRun             if provided in dryRun mode, the waveform will not be updated.\n\
steps              number of steps to run.\n\
infiniteLoop       if given, it will run endless.\n\
prefix, suffix     the prefix and suffix of the scalar pvs, they overrides the waveform file parameter\n\
                   Prefix and Suffix. \n\
watchInput         watch the input file, if it is modified, reload the input file and restart the loop. \n\
verbose            print out the message \n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  long step, steps, dryRun, verbose;
  double interval, *elapsedTime, *epochTime;
  char *searchPath;
  double pendIOTime;
  char *inputfile;
} LOOP_PARAM;

typedef struct
{
  double *waveformValue, *waveformValue1;
  char *waveformPV, **DeviceName, *prefix, *suffix, *suffix1;
  int32_t waveformRows;
  chid waveformCID, *scalarCID, *scalarCID1;
  short isSum, *isValid;
} WAVEFORM_DATA;

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PV, *Desc; /* name of run control PV and run control description PV */
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
#  if !defined(vxWorks)
  RUNCONTROL_INFO rcInfo;
#  endif
} RUNCONTROL_PARAM;

RUNCONTROL_PARAM rcParam; /* need to be global because some quantities are used
                             in signal and interrupt handlers */

int runControlPingWhileSleep(double sleepTime, LOOP_PARAM *loopParam);
#endif

volatile int sigint = 0;

#define FPINFO stdout

void InitializeLoopParam(LOOP_PARAM *loopParam);
long readinputfile(char *inputfile, long *waveforms, WAVEFORM_DATA **waveformData, char *prefix, char *suffix, LOOP_PARAM *loopParam);
void free_waveform_memory(long waveforms, WAVEFORM_DATA *waveformData);

int main(int argc, char **argv) {
#ifdef USE_RUNCONTROL
  unsigned long dummyFlags;
#endif
  SCANNED_ARG *scArg;
  long i_arg, i, j;
  LOOP_PARAM loopParam;
  long infinite = 0, waveforms = 0, watchInput = 0;
  WAVEFORM_DATA *waveformData = NULL;
  char *prefix = NULL, *suffix = NULL, *inputlink = NULL;
  double lastRCPingTime = 0.0, timeLeft, targetTime, startTime;
  struct stat input_filestat;

  /*initialize rcParam */
#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif
  signal(SIGINT, sigint_interrupt_handler);
  signal(SIGTERM, sigint_interrupt_handler);
  signal(SIGILL, interrupt_handler);
  signal(SIGABRT, interrupt_handler);
  signal(SIGFPE, interrupt_handler);
  signal(SIGSEGV, interrupt_handler);
#ifndef _WIN32
  signal(SIGHUP, interrupt_handler);
  signal(SIGQUIT, sigint_interrupt_handler);
  signal(SIGTRAP, interrupt_handler);
  signal(SIGBUS, interrupt_handler);
#endif

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }
  InitializeLoopParam(&loopParam);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], "_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &loopParam.pendIOTime) != 1 ||
            loopParam.pendIOTime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_DRY_RUN:
        loopParam.dryRun = 1;
        break;
      case CLO_INTERVAL:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &loopParam.interval) != 1 ||
            loopParam.interval <= 0)
          bomb("invalid -interval syntax\n", NULL);
        break;
      case CLO_RUNCONTROL:
#ifdef USE_RUNCONTROL
        scArg[i_arg].n_items -= 1;
        if ((scArg[i_arg].n_items < 0) ||
            !scanItemList(&dummyFlags, scArg[i_arg].list + 1, &scArg[i_arg].n_items, 0,
                          "pv", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          "pingTimeInterval", SDDS_DOUBLE, &rcParam.pingInterval, 1, 0,
                          "description", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            !rcParam.PV)
          bomb("bad -runControlPV syntax", "-runcontrol=pv=<string>,pingTimeout=<value>,pingInterval=<value>,description=<string>");
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          exit(1);
        }
        scArg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_VERBOSE:
        loopParam.verbose = 1;
        break;
      case CLO_STEPS:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%ld", &loopParam.steps) != 1 ||
            loopParam.steps <= 0)
          bomb("invalid -steps syntax\n", NULL);
        break;
      case CLO_INFINITELOOP:
        infinite = 1;
        break;
      case CLO_PREFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -prefix syntax\n", NULL);
        prefix = scArg[i_arg].list[1];
        break;
      case CLO_SUFFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -suffix syntax\n", NULL);
        suffix = scArg[i_arg].list[1];
        break;
      case CLO_WATCHINPUT:
        watchInput = 1;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!loopParam.inputfile)
        loopParam.inputfile = scArg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }
  if (infinite)
    loopParam.steps = INT_MAX;
  if (!loopParam.inputfile)
    SDDS_Bomb("no input filename");

#ifdef USE_RUNCONTROL
  if (rcParam.PV && !rcParam.Desc) {
    fprintf(stderr, "the runControl description should not be null!");
    exit(1);
  }
#endif
#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    if (loopParam.pendIOTime * 1000 > rcParam.pingTimeout) {
      /* runcontrol will time out if CA connection
         takes longer than its timeout, so the runcontrol ping timeout should be longer
         than the pendIOTime*/
      rcParam.pingTimeout = (loopParam.pendIOTime + 10) * 1000;
    }
    rcParam.handle[0] = (char)0;
    rcParam.Desc = (char *)realloc(rcParam.Desc, 41 * sizeof(char));
    rcParam.PV = (char *)realloc(rcParam.PV, 41 * sizeof(char));

    rcParam.status = runControlInit(rcParam.PV,
                                    rcParam.Desc,
                                    rcParam.pingTimeout,
                                    rcParam.handle,
                                    &(rcParam.rcInfo),
                                    30.0);
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      exit(1);
    }
    runControlPingWhileSleep(0.0, &loopParam);
    lastRCPingTime = getTimeInSecs();
    strcpy(rcParam.message, "Running");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable tp write status message and alarm severity\n");
      exit(1);
    }
  }
#endif
  atexit(rc_interrupt_handler);

  waveforms = 0;
  ca_task_initialize();
  if (loopParam.verbose) {
    fprintf(FPINFO, "reading input file and setup CA connection %s.\n", loopParam.inputfile);
    fflush(FPINFO);
  }
  if (readinputfile(loopParam.inputfile, &waveforms, &waveformData, prefix, suffix, &loopParam)) {
    fprintf(stderr, "Error in reading file/setup CA connections\n");
    free_waveform_memory(waveforms, waveformData);
    exit(1);
  }
  /*********************
   * Iteration loop    *
   *********************/
  if (loopParam.verbose) {
    fprintf(FPINFO, "Starting loop of %ld steps.\n", loopParam.steps);
    fflush(FPINFO);
  }
#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    strcpy(rcParam.message, "Iterations started");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      free_waveform_memory(waveforms, waveformData);
      exit(1);
    }
  }
#endif
  if (watchInput) {
    inputlink = read_last_link_to_file(loopParam.inputfile);
    if (get_file_stat(loopParam.inputfile, inputlink, &input_filestat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", loopParam.inputfile);
      free_waveform_memory(waveforms, waveformData);
      exit(1);
    }
  }

  targetTime = getTimeInSecs();
  startTime = getTimeInSecs();
  for (loopParam.step = 0; loopParam.step < loopParam.steps; loopParam.step++) {
    if (watchInput) {
      if (file_is_modified(loopParam.inputfile, &inputlink, &input_filestat)) {
        /*input file changed, clean memory reload input file */
        if (loopParam.verbose) {
          fprintf(stdout, "step %ld at %f seconds; input file modified, reload input file\n", loopParam.step, getTimeInSecs());
          fflush(stdout);
        }
        free_waveform_memory(waveforms, waveformData);
        readinputfile(loopParam.inputfile, &waveforms, &waveformData, prefix, suffix, &loopParam);
      }
    }
    if (loopParam.verbose) {
      fprintf(stdout, "step %ld at %f seconds\n", loopParam.step, getTimeInSecs() - startTime);
      fflush(stdout);
    }
    /*read scalar pvs at each step */
    for (i = 0; i < waveforms; i++) {
      if (loopParam.verbose) {
        fprintf(stdout, "working on %s, reading scalar pvs...\n", waveformData[i].waveformPV);
        fflush(stdout);
      }
      for (j = 0; j < waveformData[i].waveformRows; j++) {
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          if (getTimeInSecs() >= lastRCPingTime + 2) {
            lastRCPingTime = getTimeInSecs();
            runControlPingWhileSleep(0.0, &loopParam);
          }
        }
#endif
        if (waveformData[i].isValid[j]) {
          if (ca_get(DBR_DOUBLE, waveformData[i].scalarCID[j], &waveformData[i].waveformValue[j]) != ECA_NORMAL) {
            fprintf(stderr, "Error reading %s%s value.\n", waveformData[i].DeviceName[j], waveformData[i].suffix);
            ca_task_exit();
            exit(1);
          }
          if (waveformData[i].isSum) {
            if (ca_get(DBR_DOUBLE, waveformData[i].scalarCID1[j], &waveformData[i].waveformValue1[j]) != ECA_NORMAL) {
              fprintf(stderr, "Error reading %s%s value.\n", waveformData[i].DeviceName[j], waveformData[i].suffix1);
              ca_task_exit();
              exit(1);
            }
          }
        }
      }
    }
    /*ca_pend_io after reading all scalar pv values */
    if (ca_pend_io(loopParam.pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error1: problem doing put for some channels\n");
      exit(1);
    }
    if (!loopParam.dryRun) {
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0, &loopParam);
        }
      }
#endif
    }

    if (loopParam.verbose) {
      fprintf(stdout, "at %lf, writing waveform pvs.\n", getTimeInSecs() - startTime);
      fflush(stdout);
    }
    /*setting waveform pvs */
    for (i = 0; i < waveforms; i++) {
      if (waveformData[i].isSum) {
        for (j = 0; j < waveformData[i].waveformRows; j++)
          waveformData[i].waveformValue[j] += waveformData[i].waveformValue1[j];
      }
      if (!loopParam.dryRun) {
        if (ca_array_put(DBR_DOUBLE, waveformData[i].waveformRows, waveformData[i].waveformCID, waveformData[i].waveformValue) != ECA_NORMAL) {
          fprintf(stderr, "Error setting waveform %s.\n", waveformData[i].waveformPV);
          free_waveform_memory(waveforms, waveformData);
          ca_task_exit();
          exit(1);
        }
      }
    }
    if (!loopParam.dryRun) {
      /*ca_pend_io after setting all waveforms */
      if (ca_pend_io(loopParam.pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "error1: problem doing put for waveforms\n");
        free_waveform_memory(waveforms, waveformData);
        ca_task_exit();
        exit(1);
      }
    }
    targetTime += loopParam.interval;
    timeLeft = targetTime - getTimeInSecs();
    if (timeLeft < 0) {
      /* if the runcontrol PV had been paused for a long time, say, the target
         time is no longer valid, since it is substantially in the past.
         The target time should be reset to now.
      */
      targetTime = getTimeInSecs();
      timeLeft = 0;
    }
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      lastRCPingTime = getTimeInSecs();
      runControlPingWhileSleep(timeLeft, &loopParam);
    } else {
      oag_ca_pend_event(timeLeft, &sigint);
    }
#else
    oag_ca_pend_event(timeLeft, &sigint);
#endif
  }
#ifdef USE_RUNCONTROL
  /* exit runcontrol */
  if (rcParam.PV) {
    switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error exiting run control.\n");
      break;
    default:
      fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
      break;
    }
  }
#endif
  ca_task_exit();
  free_waveform_memory(waveforms, waveformData);
  if (rcParam.PV)
    free(rcParam.PV);
  if (rcParam.Desc)
    free(rcParam.Desc);
  free_scanargs(&scArg, argc);
  return 0;
}

void InitializeLoopParam(LOOP_PARAM *loopParam) {
  loopParam->step = 0;
  loopParam->steps = 1;
  loopParam->verbose = 0;
  loopParam->searchPath = NULL;
  loopParam->interval = 0.5;
  loopParam->dryRun = 0;
  loopParam->elapsedTime = NULL;
  loopParam->epochTime = NULL;
  loopParam->pendIOTime = 30;
  loopParam->inputfile = NULL;
}

void interrupt_handler(int sig) {
  exit(1);
}

void sigint_interrupt_handler(int sig) {
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
  sigint = 1;
  return;
}

void rc_interrupt_handler() {
  int ca = 1;
#ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#endif
  if (ca) {
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
      case RUNCONTROL_OK:
        break;
      case RUNCONTROL_ERROR:
        fprintf(stderr, "Error exiting run control.\n");
        break;
      default:
        fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
        break;
      }
    }
#endif
    ca_task_exit();
  }
}

#ifdef USE_RUNCONTROL
int runControlPingWhileSleep(double sleepTime, LOOP_PARAM *loopParam) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
    switch (rcParam.status) {
    case RUNCONTROL_ABORT:
      if (loopParam->verbose)
        fprintf(stderr, "Run control sddstransferscalarstowaveform aborted.\n");
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      if (loopParam->verbose)
        fprintf(stderr, "Run control sddstransferscalarstowaveform timed out.\n");
      strcpy(rcParam.message, "sddstransferscalarstowaveform timed-out");
      runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
      exit(1);
      break;
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      if (loopParam->verbose)
        fprintf(stderr, "Communications error with runcontrol record.\n");
      return (RUNCONTROL_ERROR);
    default:
      if (loopParam->verbose)
        fprintf(stderr, "Unknown run control error code.\n");
      break;
    }
    oag_ca_pend_event(MIN(timeLeft, rcParam.pingInterval), &sigint);
    if (sigint)
      exit(1);
    timeLeft -= rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void free_waveform_memory(long waveforms, WAVEFORM_DATA *waveformData) {
  long i, j;
  for (i = 0; i < waveforms; i++) {
    free(waveformData[i].waveformPV);
    waveformData[i].waveformPV = NULL;
    for (j = 0; j < waveformData[i].waveformRows; j++)
      free(waveformData[i].DeviceName[j]);
    free(waveformData[i].DeviceName);
    waveformData[i].DeviceName = NULL;
    free(waveformData[i].waveformValue);
    waveformData[i].waveformValue = NULL;
    if (waveformData[i].isSum) {
      free(waveformData[i].waveformValue1);
      waveformData[i].waveformValue1 = NULL;
    }
    if (waveformData[i].prefix)
      free(waveformData[i].prefix);
    if (waveformData[i].suffix)
      free(waveformData[i].suffix);
    if (waveformData[i].suffix1)
      free(waveformData[i].suffix1);
    if (waveformData[i].scalarCID)
      free(waveformData[i].scalarCID);
    if (waveformData[i].scalarCID1)
      free(waveformData[i].scalarCID1);
    if (waveformData[i].isValid)
      free(waveformData[i].isValid);
  }
  free(waveformData);
  waveformData = NULL;
}

long readinputfile(char *inputfile, long *waveforms, WAVEFORM_DATA **waveformData, char *prefix, char *suffix, LOOP_PARAM *loopParam) {
  long i, j, prefixParPresent = 0, suffixParPresent = 0, suffix1ParPresent, isSumParPresent = 0, pages;
  double lastRCPingTime = 0.0, isSum = 0;
  char pvName[256];
  WAVEFORM_DATA *data = NULL;
  SDDS_TABLE SDDSin;

  *waveforms = 0;
  pages = 0;
  *waveformData = NULL;
  if (!SDDS_InitializeInput(&SDDSin, inputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&SDDSin, "WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing parameter Waveform in input file");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDSin, "DeviceName", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing column DeviceName in input file");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (SDDS_CheckParameter(&SDDSin, "Prefix", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OKAY)
    prefixParPresent = 1;
  if (SDDS_CheckParameter(&SDDSin, "Suffix", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OKAY)
    suffixParPresent = 1;
  if (SDDS_CheckParameter(&SDDSin, "Suffix1", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OKAY)
    suffix1ParPresent = 1;
  if (SDDS_CheckParameter(&SDDSin, "IsSum", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) == SDDS_CHECK_OKAY)
    isSumParPresent = 1;

  pages = 0;
  while (SDDS_ReadPage(&SDDSin) > 0) {
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        /* ping once at the beginning */
        if (runControlPingWhileSleep(0.0, loopParam)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          return (1);
        }
      }
    }
#endif
    if (!(data = SDDS_Realloc(data, sizeof(*data) * (pages + 1))))
      SDDS_Bomb("memory allocation failure");
    if (((data)[pages].waveformRows = SDDS_RowCount(&SDDSin)) <= 0)
      continue;
    data[pages].waveformPV = data[pages].prefix = data[pages].suffix = data[pages].suffix1 = NULL;
    data[pages].isValid = NULL;
    data[pages].DeviceName = NULL;
    data[pages].scalarCID = data[pages].scalarCID1 = NULL;
    data[pages].isSum = 0;
    data[pages].waveformValue = NULL;
    data[pages].waveformValue1 = NULL;
    if (!(SDDS_GetParameter(&SDDSin, "WaveformPV", &data[pages].waveformPV)) ||
        !(data[pages].DeviceName = (char **)SDDS_GetColumn(&SDDSin, "DeviceName")))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!prefix && !prefixParPresent)
      SDDS_CopyString(&data[pages].prefix, "SFB:");
    else if (prefix)
      SDDS_CopyString(&data[pages].prefix, prefix);
    else if (prefixParPresent) {
      if (!(SDDS_GetParameter(&SDDSin, "Prefix", &data[pages].prefix)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!suffix && !suffixParPresent)
      SDDS_CopyString(&data[pages].suffix, ":FFCurrentAO");
    else if (suffix)
      SDDS_CopyString(&data[pages].suffix, suffix);
    else if (suffixParPresent) {
      if (!(SDDS_GetParameter(&SDDSin, "Suffix", &data[pages].suffix)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (suffix1ParPresent)
      if (!(SDDS_GetParameter(&SDDSin, "Suffix1", &data[pages].suffix1)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (isSumParPresent) {
      if (!SDDS_GetParameterAsDouble(&SDDSin, "IsSum", &isSum))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      data[pages].isSum = (short)isSum;
    }
    if (data[pages].isSum && !data[pages].suffix1) {
      fprintf(stderr, "Error: Suffix1 is not provided for %s, can not do sum transfer!\n", data[pages].waveformPV);
      exit(1);
    }
    data[pages].waveformValue = malloc(sizeof(double) * data[pages].waveformRows);
    data[pages].scalarCID = malloc(sizeof(chid) * data[pages].waveformRows);
    data[pages].isValid = malloc(sizeof(short) * data[pages].waveformRows);
    if (data[pages].isSum) {
      data[pages].scalarCID1 = malloc(sizeof(chid) * data[pages].waveformRows);
      data[pages].waveformValue1 = malloc(sizeof(double) * data[pages].waveformRows);
    }
    pages++;
  }
  if (!SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (i = 0; i < pages; i++) {
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        runControlPingWhileSleep(0.0, loopParam);
      }
    }
#endif
    if (ca_search(data[i].waveformPV, &(data[i].waveformCID)) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", data[i].waveformPV);
      ca_task_exit();
      exit(1);
    }
    for (j = 0; j < data[i].waveformRows; j++) {
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0, loopParam);
        }
      }
#endif
      if (wild_match(data[i].DeviceName[j], "S*") && !wild_match(data[i].DeviceName[j], "Spare*")) {
        data[i].isValid[j] = 1;
        sprintf(pvName, "%s%s%s", data[i].prefix, data[i].DeviceName[j], data[i].suffix);
        if (ca_search(pvName, &(data[i].scalarCID[j])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvName);
          ca_task_exit();
          exit(1);
        }
        if (data[i].isSum) {
          sprintf(pvName, "%s%s%s", data[i].prefix, data[i].DeviceName[j], data[i].suffix1);
          if (ca_search(pvName, &(data[i].scalarCID1[j])) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for %s\n", pvName);
            ca_task_exit();
            exit(1);
          }
        }
      } else {
        /*FF bpm waveform does not have corresponding bpm, ignore */
        data[i].isValid[j] = 0;
        data[i].waveformValue[j] = 0;
        if (data[i].isSum)
          data[i].waveformValue1[j] = 0;
      }
    }
  }

  /*after connecting all pvs */
  fprintf(stderr, "waiting for CA\n");
  if (ca_pend_io(loopParam->pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < pages; i++) {
      if (ca_state(data[i].waveformCID) != cs_conn)
        fprintf(stderr, "%s not connected\n", data[i].waveformPV);
      for (j = 0; j < data[i].waveformRows; j++) {
        /*remove illegal device names such as 16, Illegal for FF setpoint waveform */
        if (data[i].isValid[j]) {
          sprintf(pvName, "%s%s%s", data[i].prefix, data[i].DeviceName[j], data[i].suffix);
          if (ca_state(data[i].scalarCID[j]) != cs_conn)
            fprintf(stderr, "%s not connected\n", pvName);
          if (data[i].isSum) {
            sprintf(pvName, "%s%s%s", data[i].prefix, data[i].DeviceName[j], data[i].suffix1);
            if (ca_state(data[i].scalarCID1[j]) != cs_conn)
              fprintf(stderr, "%s not connected\n", pvName);
          }
        }
      }
    }
    ca_task_exit();
    return (1);
  }
  *waveformData = data;
  *waveforms = pages;
  fprintf(stderr, "done\n");
  return (0);
}
