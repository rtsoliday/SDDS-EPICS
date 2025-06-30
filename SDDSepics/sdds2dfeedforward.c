/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* sdds2dfeedforward
 * A generalized SDDS-compliant 2d feed-forward program for EPICS
 *
 * The structure of the main input file is:
 * Parameters:
 *    Readback1Name   PV name for the first readback
 *    Readback2Name   PV name for the first readback
 *    ActuatorName    PV name for the actuator
 *    ReadbackChangeThreshold  
 *                   (Optional) value by which readback must change
 *                   before any action will be taken for the named actuator.
 *    ActuatorChangeLimit
 *                   (Optional) maximum change that will be performed
 *                   in one step for the named actuator.
 * Columns:
 *    Readback1Value  values of the readback
 *    Readback2Value  values of the readback
 *    ActuatorValue   values of the actuator
 *
 * Each page must have a different actuator but any number may have the
 * same readback.
 *
 * Based on sddsfeedforward.
 * Michael Borland, June 2025
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSepics.h"
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
/*#include <chandata.h>*/
#include <signal.h>
#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif
#include <alarm.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

#define CLO_TIME_INTERVAL 0
#define CLO_STEPS 1
#define CLO_VERBOSE 2
#define CLO_DRY_RUN 3
#define CLO_TEST_VALUES 4
#define CLO_EZCATIMING 5
#define CLO_RUNCONTROLPV 6
#define CLO_RUNCONTROLDESC 7
#define CLO_AVERAGE 8
#define CLO_TESTCASECURITY 9
#define CLO_OFFSETONLY 10
#define CLO_ADVANCE 11
#define CLO_PENDIOTIME 12
#define CLO_INFINITELOOP 13
#define CLO_CONTROLLOG 14
#define COMMANDLINE_OPTIONS 15

#define DEFAULT_AVEINTERVAL 1 /* one second default time interval */

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_TIME_ADVANCE 0.0
#define DEFAULT_STEPS 100
#define DEFAULT_TIMEOUT_ERRORS 5

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
#endif

/* parameters of the main feed-forward iteration loop */
typedef struct
{
  long step, steps;              /* present step, total number of steps to do */
  long dryRun;                   /* for testing */
  double interval;               /* interval in seconds between correction */
  double elapsedTime, epochTime; /* in seconds */
  long verbose, offsetOnly;
  double advance;
  char *controlLogRootname;
  long logOnChange;
} LOOP_PARAM;

/* data for feedforward algorithm for one readback and N actuators */
typedef struct
{
  /* name of readbacks and actuator they drive */
  char *readback1, *readback2, **actuator;
  long actuators;
  /* value by which readback must change before any action will be taken */
  double readbackChangeThreshold;
  short thresholdExceeded;
  /* maximum change that will be performed in one step for each actuator */
  double *actuatorDeltaLimit;
  /* the lower limit and upper limit of actutator setting values */
  double *actuatorLowerLimit, *actuatorUpperLimit;
  /* actuatorData[i] and readbackData[i] contain data arrays for
   * feedfoward on actuator[i] using readback.  Arrays have nData[i]
   * elements. */
  double **actuatorData, **readback1Data, **readback2Data;
  long *n1Data, *n2Data;
  /* indices of actuator and each readback in the arrays in 
   * CONTROL_DATA structures */
  long *actuatorIndex, readbackIndex;
  /* index of the enable PV in the enable PV list.  -1 if no enable PV */
  long *enablePVIndex;
  double *ffValue; /*computed ffValue for each actuator in this group. */
} FEEDFORWARD_GROUP;

/* This structure is used for CA operations to read/write many values at once.  */
typedef struct
{
  char **controlName;
  long controlNames;
  double *value, *lastValue, *initialValue;
  double *referenceValue; /* used to remember value at the time of the last change */
  double *computeValue;
  short *flag;    /* used to indicate which to write*/
  short *updated; /*indicated if new FF value is computed */
  chid *cid;
} CONTROL_DATA;

/* This structure is used for CA operations to read/write many values at once.  */
/* It's been adapted for use with enablePV's by adding some fields and deleting others. */
typedef struct
{
  char **controlName, **controlName2, **test_pvName;
  long controlNames;
  double *value, *value2, *test_value;
  chid *cid, *cid2, *test_id;
  double *upperLimit, *lowerLimit, *disabledOutputValue, *upperLimit2, *lowerLimit2, *test_lowerLimit, *test_upperLimit;
  long *outOfLimit, *outOfLimit2, *outOfTest;
  short *flag, *flag2, *test_flag; /*indicate whether write to log file or not */
} ENABLE_DATA;

/* data for feedforward algorithm */
typedef struct
{
  unsigned long flags;
#define READBACK_CHANGE_THRESHOLD 0x0001UL
#define ACTUATOR_CHANGE_LIMIT 0x0002UL
#define ACTUATOR_LOWER_LIMIT 0x0004UL
#define ACTUATOR_UPPER_LIMIT 0x0008UL
  char *file; /* file from which data is obtained */
  FEEDFORWARD_GROUP *group;
  long groups; /* also the number of actuators, since there is one group formed per actuator  */
} FEEDFORWARD_DATA;

/* data for PV tests that may suspend the feedforward */
typedef struct
{
  char *file;          /* file from which data is obtained */
  char **controlName;  /* PV names */
  long n, *outOfRange; /* number of tests, out of range flags */
  /* latest readings, min/max limits, sleep seconds (after a PV goes
   * out of range), reset seconds (after it comes back in range) 
   */
  short *exitOnFailure; /* terminate the program if the test failed and its exitOnFailure is 1.*/
  double *value, *min, *max, *sleep, *reset, longestSleep, longestReset;
  chid *cid;
} TESTS;

/* The program reports when PVs fail the tests.  It "backs off" from 
 * reporting too frequently if the tests repeatedly fail. */
typedef struct
{
  long level, counter;
} BACKOFF;

/* specification of how to average the readbacks for input to
 * the feedforward equation 
 */
typedef struct
{
  long n;
  double interval;
} AVERAGE_PARAM;

long sdds2dfeedforward_parseArguments(char ***argv, int *argc, FEEDFORWARD_DATA *feedforwardData,
                                    LOOP_PARAM *loopParam, AVERAGE_PARAM *aveParam,
                                    TESTS *testConditions, long *testCASecurity,
                                    char *commandline_option[COMMANDLINE_OPTIONS], double *pendIOtime);
void sdds2dfeedforward_interrupt_handler(int sig);
void sdds2dfeedforward_sigint_interrupt_handler(int sig);
int runControlPingWhileSleep(double sleepTime);
void getControlDataValues(CONTROL_DATA *readback, AVERAGE_PARAM *aveParam,
                          long initialValues, LOOP_PARAM *loopParam, double pendIOtime);
long ReadEnablePVValues(ENABLE_DATA *enablePVData, AVERAGE_PARAM *aveParam,
                        long initialValues, LOOP_PARAM *loopParam, double pendIOtime);
void sdds2dfeedforward_initializeData(FEEDFORWARD_DATA *feedforwardData,
                                    LOOP_PARAM *loopParam, AVERAGE_PARAM *aveParam, TESTS *test);
void sdds2dfeedforward_setupData(FEEDFORWARD_DATA *feedforwardData, LOOP_PARAM *loopParam,
                                 TESTS *test, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data,
                                 CONTROL_DATA *actuatorData,
                                 ENABLE_DATA *enablePVData);
void setupControlLogFile(char *logRootname, SDDS_DATASET *logDataSet);

void readTestsFile(TESTS *test, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, CONTROL_DATA *actuatorData,
                   LOOP_PARAM *loopParam);
void readFeedForwardFile(FEEDFORWARD_DATA *feedforwardData, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data,
                         CONTROL_DATA *actuatorData, ENABLE_DATA *enablePVData, LOOP_PARAM *loopParam);
long checkActionThreshold(CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, FEEDFORWARD_DATA *feedforwardData, LOOP_PARAM *loopParam);
long sdds2dfeedforward_checkOutOfRange(TESTS *test, BACKOFF *backoffParam, LOOP_PARAM *loopParam, double timeOfDay, double pendIOtime);
long ComputeNewActuatorValues(FEEDFORWARD_DATA *feedforwardData, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, ENABLE_DATA *enablePVData,
                              CONTROL_DATA *actuatorData, LOOP_PARAM *loopParam, long offsetMode,
                              double *offsetValue, long *settingOutOfRange);
double ffInterpolate(double *f, double *x1, long n1, double *x2, long n2,
                     double x1o, double x2o, long order, long *returnCode);
void operate_runcontrol(long prevValue, long currentValue, char *message, long caWriteError,
                        LOOP_PARAM *loopParam, BACKOFF *backoffParam);

/* this is the file pointer to which informational printouts
 * will be sent 
 * changed to stdout since stderr caused problem in APSExecLog window because it waited for
 * incoming text and tried to display all the text in one time (March 2008, Shang)
 */
#define FPINFO stdout

#if defined(vxWorks)
void sdds2dfeedforwardDeleteHook(WIND_TCB *pTcb);
#else
void sdds2dfeedforwardDeleteHook();
#endif

/* need to be global because some quantities are used
 * in signal and interrupt handlers */
typedef struct
{
  FEEDFORWARD_DATA feedforwardData;
  CONTROL_DATA readback1Data;
  CONTROL_DATA readback2Data;
  CONTROL_DATA actuatorData;
  ENABLE_DATA enablePVData;
#ifdef USE_RUNCONTROL
  RUNCONTROL_PARAM rcParam;
#endif
  TESTS testConditions;
  double *actuatorValueOffset;
#ifdef USE_LOGDAEMON
  LOGHANDLE logHandle;
  long useLogDaemon;
#endif
  int argc;
  volatile int sigint;
  char **argv;
} SDDS2DFEEDFORWARD_GLOBAL;
SDDS2DFEEDFORWARD_GLOBAL *sdds2dfeedforwardGlobal;

#if defined(vxWorks)
int sddsIocFeedforward(char *name, char *options)
#else
int main(int argc, char **argv)
#endif
{
#if defined(vxWorks)
  int argc;
  char **argv = NULL;
  char *buffer = NULL;
#endif
  SCANNED_ARG *s_arg = NULL;
  SDDS_DATASET controlLogDataSet;
  double hourNow = 0, lastHour;
  long outputRow = 0, i_var = 0;
  long skipIteration, outOfRange, prevOutOfRange, prevSettingOutOfRange, settingOutOfRange;
  char *timeStamp;
  double startTime, startHour, timeOfDay, epochTime;
  LOOP_PARAM loopParam;
  /*  TESTS testConditions;*/
  BACKOFF backoffParam;
  AVERAGE_PARAM aveParam;
  /*  FEEDFORWARD_DATA feedforwardData;*/
  /* CONTROL_DATA readbackData;*/
  /* CONTROL_DATA actuatorData;*/
  long testCASecurity, caWriteError = 0;
  long firstPass = 1;
  /*  double *actuatorValueOffset = NULL;*/
  double pendIOtime = 10.0;
  double interval;
  long actuatorValueChange = 0;
  char *commandline_option[COMMANDLINE_OPTIONS] = {
    "interval",
    "steps",
    "verbose",
    "dryrun",
    "testvalues",
    "ezca",
    "runcontrolpv",
    "runcontroldescription",
    "averageof",
    "casecuritytest",
    "offsetonly",
    "advance",
    "pendiotime",
    "infiniteLoop",
    "controllog",
  };
#if !defined(vxWorks)
  char *USAGE1 = "sdds2dfeedforward <inputfile> \n\
       [-controlLog=<rootname>] \n\
       [-interval=<real-value>] [-steps=<integer=value>]\n\
       [-verbose] [-dryRun] [-offsetOnly]\n\
       [-averageOf=<number>[,interval=<seconds>]] \n\
       [-advance=<seconds>] \n\
       [-testValues=<SDDSfile>]\n\
       [-runControlPV==string=<string>[,pingTimeout=<value]] \n\
       [-runControlDescription=<string>]\n\
       [-CASecurityTest] \n\
       [-pendIOtime=<seconds>] [-infiniteLoop] \n\n\
Perform simple 2D feedforward on APS control system process variables using CA calls.\n\
<inputfile>    feedforward data in sdds format\n\
controlLog     optional, log the input and output data in an SDDS file.\n\
               the file is named as <rootname>-YYYY-JJJ-MMDD.<N>. A new daily file \n\
               will be started at the midnight.\n\
interval       time interval between each correction.\n\
steps          total number of corrections.\n\
verbose        prints extra information.\n\
dryRun         readback variables are read, but the control variables\n\
               aren't changed.\n";
  char *USAGE2 = "offsetOnly     reads the initial value of the Readbacks and computes\n\
               the initial value of the Actuators.  Each actuator table\n\
               is offset by this initial value so that there is no change\n\
               made until the Readback changes.\n\
average        number of readbacks to average before making a correction.\n\
               Default interval is one second. Units of the specified \n\
               interval is seconds. The total time for averaging will\n\
               not add to the time between corrections.\n\
advance        number of seconds to advance the readback value based\n\
               on the observed ramp of the readback value.I.e.,\n\
               value <- value + (value-valueOld)/interval*advance\n\
infiniteLoop   run with steps of the biggest integer. \n\
testValues     sdds format file containing minimum and maximum values\n\
               of PV's specifying a range outside of which the feedback\n\
               is temporarily suspended. Column names are ControlName,\n\
               MinimumValue, MaximumValue. Optional column names are \n\
               SleepTime and ResetTime.\n\
runControlPV   specifies a string whose value is a runControl PV name. \n\
               and/or pingTimeout value. \n";
  char *USAGE3 = "runControlDescription\n\
               specifies a runControl PV description record.\n\
CASecurityTest checks the channel access write permission of the control PVs.\n\
               If at least one PV has a write restriction then the program suspends\n\
               the runcontrol record.\n\
Program by Michael Borland, ANL\n\
Link date: "__DATE__
                 " "__TIME__
                 ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#  ifndef USE_RUNCONTROL
  char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#  else
  static char *USAGE_WARNING = "";
#  endif
#endif

  signal(SIGINT, sdds2dfeedforward_sigint_interrupt_handler);
  signal(SIGTERM, sdds2dfeedforward_sigint_interrupt_handler);
  signal(SIGILL, sdds2dfeedforward_interrupt_handler);
  signal(SIGABRT, sdds2dfeedforward_interrupt_handler);
  signal(SIGFPE, sdds2dfeedforward_interrupt_handler);
  signal(SIGSEGV, sdds2dfeedforward_interrupt_handler);
#ifndef _WIN32
  signal(SIGHUP, sdds2dfeedforward_interrupt_handler);
  signal(SIGQUIT, sdds2dfeedforward_sigint_interrupt_handler);
  signal(SIGTRAP, sdds2dfeedforward_interrupt_handler);
  signal(SIGBUS, sdds2dfeedforward_interrupt_handler);
#endif
#if !defined(vxWorks)
  SDDS_RegisterProgramName("sdds2dfeedforward");
#endif

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif
  ca_add_exception_event(oag_ca_exception_handler, NULL);

#if defined(vxWorks)
  buffer = malloc(sizeof(char) * (strlen(options) + 1 + 16));
  sprintf(buffer, "sdds2dfeedforward %s", options);
  argc = parse_string(&argv, buffer);
  if (buffer)
    free(buffer);
  buffer = NULL;

  if (taskVarAdd(0, (int *)&sdds2dfeedforwardGlobal) != OK) {
    fprintf(stderr, "can't taskVarAdd sdds2dfeedforwardGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
  if ((sdds2dfeedforwardGlobal = (SDDS2DFEEDFORWARD_GLOBAL *)calloc(1, sizeof(SDDS2DFEEDFORWARD_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sdds2dfeedforwardGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
#else
  if ((sdds2dfeedforwardGlobal = (SDDS2DFEEDFORWARD_GLOBAL *)calloc(1, sizeof(SDDS2DFEEDFORWARD_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sdds2dfeedforwardGlobal\n");
    return (1);
  }
#endif

#if defined(vxWorks)
  sdds2dfeedforwardGlobal->argc = argc;
  sdds2dfeedforwardGlobal->argv = argv;
  taskDeleteHookAdd((FUNCPTR)sdds2dfeedforwardDeleteHook);
#else
  atexit(sdds2dfeedforwardDeleteHook);
#endif
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
#if defined(vxWorks)
    fprintf(stderr, "invalid sddsfeedfoward command line\n");
#else
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    fprintf(stderr, "%s", USAGE_WARNING);
#endif
    free_scanargs(&s_arg, argc);
    return (1);
  }
  free_scanargs(&s_arg, argc);
  if (sdds2dfeedforward_parseArguments(&argv, &argc, &sdds2dfeedforwardGlobal->feedforwardData, &loopParam, &aveParam,
                                     &sdds2dfeedforwardGlobal->testConditions, &testCASecurity, commandline_option, &pendIOtime)) {
    fprintf(stderr, "Problem with parsing arguments.\n");
    return (1);
  }

#ifdef USE_RUNCONTROL
  if (sdds2dfeedforwardGlobal->rcParam.PV) {
    sdds2dfeedforwardGlobal->rcParam.handle[0] = (char)0;
    sdds2dfeedforwardGlobal->rcParam.Desc = (char *)realloc(sdds2dfeedforwardGlobal->rcParam.Desc, 41 * sizeof(char));
    sdds2dfeedforwardGlobal->rcParam.PV = (char *)realloc(sdds2dfeedforwardGlobal->rcParam.PV, 41 * sizeof(char));
    sdds2dfeedforwardGlobal->rcParam.status = runControlInit(sdds2dfeedforwardGlobal->rcParam.PV,
                                                           sdds2dfeedforwardGlobal->rcParam.Desc,
                                                           sdds2dfeedforwardGlobal->rcParam.pingTimeout,
                                                           sdds2dfeedforwardGlobal->rcParam.handle,
                                                           &(sdds2dfeedforwardGlobal->rcParam.rcInfo),
                                                           pendIOtime);

    if (sdds2dfeedforwardGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (sdds2dfeedforwardGlobal->rcParam.status == RUNCONTROL_DENIED)
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      return (1);
    }
  }
#endif

  sdds2dfeedforward_setupData(&sdds2dfeedforwardGlobal->feedforwardData, &loopParam,
                              &sdds2dfeedforwardGlobal->testConditions, &sdds2dfeedforwardGlobal->readback1Data,
                              &sdds2dfeedforwardGlobal->readback2Data,
                              &sdds2dfeedforwardGlobal->actuatorData, &sdds2dfeedforwardGlobal->enablePVData);

  if (!sdds2dfeedforwardGlobal->feedforwardData.file) {
    fprintf(stderr, "input filename not given");
    return (1);
  }
  if (aveParam.n > 1) {
    if (loopParam.interval < (aveParam.n - 1) * aveParam.interval) {
      fprintf(stderr, "Time interval between corrections (%e) is too short for the averaging requested (%ldx%e).\n", loopParam.interval, (aveParam.n - 1), aveParam.interval);
      return (1);
    }
    loopParam.interval -= (aveParam.n - 1) * aveParam.interval;
  }

#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  sdds2dfeedforwardGlobal->useLogDaemon = 0;
  if (sdds2dfeedforwardGlobal->rcParam.PV) {
    switch (logOpen(&sdds2dfeedforwardGlobal->logHandle, NULL, "controllawAudit", "Instance Action")) {
    case LOG_OK:
      sdds2dfeedforwardGlobal->useLogDaemon = 1;
      break;
    case LOG_INVALID:
      fprintf(FPINFO,
              "warning: logDaemon rejected given sourceId and tagList. logDaemon messages not logged.\n");
      fflush(FPINFO);
      break;
    case LOG_ERROR:
      fprintf(FPINFO,
              "warning: unable to open connection to logDaemon. logDaemon messages not logged.\n");
      fflush(FPINFO);
      break;
    case LOG_TOOBIG:
      fprintf(FPINFO,
              "warning: one of the string supplied is too long. logDaemon messages not logged.\n");
      fflush(FPINFO);
      break;
    default:
      fprintf(FPINFO, "warning: logOpen failed for unknown reason.\n");
      fflush(FPINFO);
      break;
    }
  }
#  endif
#endif

  getTimeBreakdown(&startTime, NULL, &startHour, NULL, NULL, NULL, &timeStamp);
  if (loopParam.verbose) {
    backoffParam.level = 1;
    backoffParam.counter = 0;
  }

  /*********************
   * Iteration loop    *
   *********************/
  if (loopParam.verbose) {
    fprintf(FPINFO, "Starting loop of %ld steps.\n", loopParam.steps);
    fflush(FPINFO);
  }
  outOfRange = 0;
  settingOutOfRange = 0;
  prevSettingOutOfRange = 0;

  if (loopParam.controlLogRootname) {
    setupControlLogFile(loopParam.controlLogRootname, &controlLogDataSet);
    hourNow = getHourOfDay();
  }
  for (loopParam.step = 0; loopParam.step < loopParam.steps; loopParam.step++) {
    lastHour = hourNow;
    hourNow = getHourOfDay();
    if (hourNow < lastHour && loopParam.controlLogRootname) {
      if (!SDDS_Terminate(&controlLogDataSet))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      outputRow = 0;
      setupControlLogFile(loopParam.controlLogRootname, &controlLogDataSet);
    }
    if (sdds2dfeedforwardGlobal->sigint) {
      return (1);
    }
#ifdef USE_RUNCONTROL
    if (sdds2dfeedforwardGlobal->rcParam.PV) {
      /* ping once at the beginning */
      if (runControlPingWhileSleep(0.1)) {
        return (1);
      }
    }
#endif

    loopParam.elapsedTime = (loopParam.epochTime = getTimeInSecs()) - startTime;
    if (ReadEnablePVValues(&sdds2dfeedforwardGlobal->enablePVData, &aveParam, !loopParam.step, &loopParam, pendIOtime))
      SDDS_Bomb("Error in reading values of feedforward enable pvs.!");
    getControlDataValues(&sdds2dfeedforwardGlobal->readback1Data, &aveParam, !loopParam.step, &loopParam, pendIOtime);
    getControlDataValues(&sdds2dfeedforwardGlobal->readback2Data, &aveParam, !loopParam.step, &loopParam, pendIOtime);
    getControlDataValues(&sdds2dfeedforwardGlobal->actuatorData, NULL, !loopParam.step, &loopParam, pendIOtime);

    epochTime = getTimeInSecs();
    loopParam.elapsedTime = epochTime - startTime;
    timeOfDay = startHour + loopParam.elapsedTime / 3600.0;
    if ((skipIteration = !checkActionThreshold(&sdds2dfeedforwardGlobal->readback1Data, &sdds2dfeedforwardGlobal->readback2Data,
                                               &sdds2dfeedforwardGlobal->feedforwardData, &loopParam)) != 0) {
      if (loopParam.verbose) {
        fprintf(FPINFO, "Skipping iteration due to lack of change in readbacks\n");
        fflush(FPINFO);
      }
    } else {
      prevSettingOutOfRange = settingOutOfRange;
      if (sdds2dfeedforwardGlobal->testConditions.file) {
        /**********************
               * test values        *
               **********************/
        loopParam.elapsedTime = (loopParam.epochTime = getTimeInSecs()) - startTime;

        prevOutOfRange = outOfRange;

        outOfRange = sdds2dfeedforward_checkOutOfRange(&sdds2dfeedforwardGlobal->testConditions, &backoffParam, &loopParam, timeOfDay, pendIOtime);
        operate_runcontrol(outOfRange, prevOutOfRange, "Out-of-range test variables(s)", caWriteError,
                           &loopParam, &backoffParam);
      }
      if (!outOfRange) {
        /* compute new values for actuators */
        if (loopParam.offsetOnly) {
          /* In this mode, the values sent out are offsets from the initial computed values.
                   * Hence, nothing is changed until one of the readbacks changes.
                   */
          long iActuator;
          if (firstPass) {
            if (!(sdds2dfeedforwardGlobal->actuatorValueOffset = SDDS_Malloc(sizeof(double) *
                                                                           sdds2dfeedforwardGlobal->actuatorData.controlNames))) {
              fprintf(FPINFO, "memory allocation failure1");
              fflush(FPINFO);
              return (1);
            }
            getControlDataValues(&sdds2dfeedforwardGlobal->actuatorData, NULL, 1, &loopParam, pendIOtime);
            actuatorValueChange = ComputeNewActuatorValues(&sdds2dfeedforwardGlobal->feedforwardData,
                                                           &sdds2dfeedforwardGlobal->readback1Data,
                                                           &sdds2dfeedforwardGlobal->readback2Data,
                                                           &sdds2dfeedforwardGlobal->enablePVData,
                                                           &sdds2dfeedforwardGlobal->actuatorData, &loopParam, -1, NULL, &settingOutOfRange);
            for (iActuator = 0; iActuator < sdds2dfeedforwardGlobal->actuatorData.controlNames; iActuator++) {
              sdds2dfeedforwardGlobal->actuatorValueOffset[iActuator] = sdds2dfeedforwardGlobal->actuatorData.value[iActuator] - sdds2dfeedforwardGlobal->actuatorData.initialValue[iActuator];
              sdds2dfeedforwardGlobal->actuatorData.value[iActuator] = sdds2dfeedforwardGlobal->actuatorData.initialValue[iActuator];
            }
            if (loopParam.verbose) {
              fprintf(FPINFO, "Offsets for actuators:\n");
              fflush(FPINFO);
              for (iActuator = 0; iActuator < sdds2dfeedforwardGlobal->actuatorData.controlNames; iActuator++) {
                fprintf(FPINFO, "%s = %e\n", sdds2dfeedforwardGlobal->actuatorData.controlName[iActuator],
                        sdds2dfeedforwardGlobal->actuatorValueOffset[iActuator]);
                fflush(FPINFO);
              }
            }
            firstPass = 0;
          }
          actuatorValueChange = ComputeNewActuatorValues(&sdds2dfeedforwardGlobal->feedforwardData,
                                                         &sdds2dfeedforwardGlobal->readback1Data,
                                                         &sdds2dfeedforwardGlobal->readback2Data,
                                                         &sdds2dfeedforwardGlobal->enablePVData,
                                                         &sdds2dfeedforwardGlobal->actuatorData, &loopParam, 1,
                                                         sdds2dfeedforwardGlobal->actuatorValueOffset, &settingOutOfRange);
        } else
          actuatorValueChange = ComputeNewActuatorValues(&sdds2dfeedforwardGlobal->feedforwardData,
                                                         &sdds2dfeedforwardGlobal->readback1Data,
                                                         &sdds2dfeedforwardGlobal->readback2Data,
                                                         &sdds2dfeedforwardGlobal->enablePVData,
                                                         &sdds2dfeedforwardGlobal->actuatorData, &loopParam, 0, NULL, &settingOutOfRange);

        if (!loopParam.dryRun) {
          if (settingOutOfRange || prevSettingOutOfRange) {
            operate_runcontrol(prevSettingOutOfRange, settingOutOfRange, "Actuator setting out-of-range",
                               caWriteError, &loopParam, &backoffParam);
          } else {
#ifdef USE_RUNCONTROL
            if (sdds2dfeedforwardGlobal->rcParam.PV) {
              strcpy(sdds2dfeedforwardGlobal->rcParam.message, "Running");
              sdds2dfeedforwardGlobal->rcParam.status = runControlLogMessage(sdds2dfeedforwardGlobal->rcParam.handle,
                                                                           sdds2dfeedforwardGlobal->rcParam.message,
                                                                           NO_ALARM,
                                                                           &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
              if (sdds2dfeedforwardGlobal->rcParam.status != RUNCONTROL_OK) {
                fprintf(FPINFO, "Unable to write status message and alarm severity\n");
                fflush(FPINFO);
                return (1);
              }
            }
#endif /* USE_RUNCONTROL */
          }
          if (!settingOutOfRange) {
            loopParam.elapsedTime = (loopParam.epochTime = getTimeInSecs()) - startTime;
            if (loopParam.verbose) {
              fprintf(FPINFO, "Setting devices at %f seconds, time of day: %f hours.\n",
                      loopParam.elapsedTime, timeOfDay);
              fflush(FPINFO);
            }
            caWriteError = 0;
            if (testCASecurity && (caWriteError =
                                     CheckCAWritePermission(sdds2dfeedforwardGlobal->actuatorData.controlName,
                                                            sdds2dfeedforwardGlobal->actuatorData.controlNames,
                                                            sdds2dfeedforwardGlobal->actuatorData.cid))) {
              fprintf(FPINFO, "Write access denied to at least one PV.\n");
              fflush(FPINFO);
#ifdef USE_RUNCONTROL
              if (sdds2dfeedforwardGlobal->rcParam.PV) {
                /* This messages must be re-written at every iteration
                                 because the runcontrolMessage can be written by another process
                                 such as the tcl interface that runs sddscontrollaw */
                strcpy(sdds2dfeedforwardGlobal->rcParam.message, "CA Security denies access.");
                sdds2dfeedforwardGlobal->rcParam.status = runControlLogMessage(sdds2dfeedforwardGlobal->rcParam.handle,
                                                                             sdds2dfeedforwardGlobal->rcParam.message,
                                                                             MAJOR_ALARM,
                                                                             &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
                if (sdds2dfeedforwardGlobal->rcParam.status != RUNCONTROL_OK) {
                  fprintf(FPINFO, "Unable to write status message and alarm severity\n");
                  fflush(FPINFO);
                  return (1);
                }
#  ifdef USE_LOGDAEMON
                if (sdds2dfeedforwardGlobal->useLogDaemon && sdds2dfeedforwardGlobal->rcParam.PV) {
                  switch (logArguments(sdds2dfeedforwardGlobal->logHandle, sdds2dfeedforwardGlobal->rcParam.PV, "CA denied", NULL)) {
                  case LOG_OK:
                    break;
                  case LOG_ERROR:
                  case LOG_TOOBIG:
                  default:
                    fprintf(FPINFO, "warning: something wrong in calling logArguments.\n");
                    fflush(FPINFO);
                    break;
                  }
                }
#  endif /* USE_LOGDAEMON */
              }
#endif        /* USE_RUNCONTROL */
            } /* end of branch for ca write error, if test performed and error found */
            if (!caWriteError) {
              if (loopParam.dryRun) {
                fprintf(FPINFO, "This should never happen"); /* just for paranoia's sake */
                fflush(FPINFO);
                return (1);
              }
              if (actuatorValueChange) {
                if (WriteScalarValues(sdds2dfeedforwardGlobal->actuatorData.controlName,
                                      sdds2dfeedforwardGlobal->actuatorData.value,
                                      sdds2dfeedforwardGlobal->actuatorData.flag,
                                      sdds2dfeedforwardGlobal->actuatorData.controlNames,
                                      sdds2dfeedforwardGlobal->actuatorData.cid,
                                      pendIOtime)) {
                  fprintf(FPINFO, "Error in setting PVs of the control variables.\n");
                  fflush(FPINFO);
                }
              }
              if (loopParam.controlLogRootname) {
                if (!(loopParam.logOnChange) || (loopParam.logOnChange && actuatorValueChange)) {
                  if (!SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                         "Step", loopParam.step, "Time", epochTime,
                                         "TimeOfDay", (float)timeOfDay, NULL))
                    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  for (i_var = 0; i_var < sdds2dfeedforwardGlobal->readback1Data.controlNames; i_var++) {
                    if (sdds2dfeedforwardGlobal->readback1Data.flag[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->readback1Data.controlName[i_var],
                                           sdds2dfeedforwardGlobal->readback1Data.value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  }
                  for (i_var = 0; i_var < sdds2dfeedforwardGlobal->readback2Data.controlNames; i_var++) {
                    if (sdds2dfeedforwardGlobal->readback2Data.flag[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->readback2Data.controlName[i_var],
                                           sdds2dfeedforwardGlobal->readback2Data.value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  }
                  for (i_var = 0; i_var < sdds2dfeedforwardGlobal->readback2Data.controlNames; i_var++) {
                    if (sdds2dfeedforwardGlobal->readback2Data.flag[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->readback2Data.controlName[i_var],
                                           sdds2dfeedforwardGlobal->readback2Data.value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  }
                  for (i_var = 0; i_var < sdds2dfeedforwardGlobal->actuatorData.controlNames; i_var++) {
                    if (!SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->actuatorData.controlName[i_var],
                                           sdds2dfeedforwardGlobal->actuatorData.value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  }
                  for (i_var = 0; i_var < sdds2dfeedforwardGlobal->enablePVData.controlNames; i_var++) {
                    if (sdds2dfeedforwardGlobal->enablePVData.flag[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->enablePVData.controlName[i_var],
                                           sdds2dfeedforwardGlobal->enablePVData.value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                    if (sdds2dfeedforwardGlobal->enablePVData.controlName2 && sdds2dfeedforwardGlobal->enablePVData.flag2[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->enablePVData.controlName2[i_var],
                                           sdds2dfeedforwardGlobal->enablePVData.value2[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                    if (sdds2dfeedforwardGlobal->enablePVData.test_pvName && sdds2dfeedforwardGlobal->enablePVData.test_flag[i_var] &&
                        !SDDS_SetRowValues(&controlLogDataSet, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                                           sdds2dfeedforwardGlobal->enablePVData.test_pvName[i_var],
                                           sdds2dfeedforwardGlobal->enablePVData.test_value[i_var],
                                           NULL))
                      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                  }
                  outputRow++;
                  if (!SDDS_UpdatePage(&controlLogDataSet, FLUSH_TABLE))
                    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } /*end of write control log file */
            }   /* end of if (!caWriteError) */
          }     /*end of if (!settingOutOfRange) */
        }       /* end of branch for nondry run mode */
        if (loopParam.verbose) {
          long i;
          fprintf(FPINFO, "New actuator values: \n");
          fflush(FPINFO);
          for (i = 0; i < sdds2dfeedforwardGlobal->actuatorData.controlNames; i++) {
            if (sdds2dfeedforwardGlobal->actuatorData.flag[i])
              fprintf(FPINFO, "%s -> %e\n", sdds2dfeedforwardGlobal->actuatorData.controlName[i], sdds2dfeedforwardGlobal->actuatorData.value[i]);
            else
              fprintf(FPINFO, "%s will not be updated due to test failure.\n", sdds2dfeedforwardGlobal->actuatorData.controlName[i]);
            fflush(FPINFO);
          }
        }
#ifdef USE_RUNCONTROL
        if (sdds2dfeedforwardGlobal->rcParam.PV) {
          runControlPingWhileSleep(0.1);
        }
#endif
      } /* end of branch for no out of range PVs */
    }   /* end of branch for skipIteration=0 */

    /********************************
       * pause for iteration interval *
       ********************************/
#ifdef USE_RUNCONTROL
    if (sdds2dfeedforwardGlobal->rcParam.PV) {
      runControlPingWhileSleep(outOfRange
                                 ? MAX(sdds2dfeedforwardGlobal->testConditions.longestSleep, loopParam.interval)
                                 : loopParam.interval);
    } else {
      interval = outOfRange ? MAX(sdds2dfeedforwardGlobal->testConditions.longestSleep, loopParam.interval) : loopParam.interval;
      oag_ca_pend_event(interval, &(sdds2dfeedforwardGlobal->sigint));
    }
#else
    interval = outOfRange ? MAX(sdds2dfeedforwardGlobal->testConditions.longestSleep, loopParam.interval) : loopParam.interval;
    oag_ca_pend_event(interval, &(sdds2dfeedforwardGlobal->sigint));
#endif
    if (loopParam.verbose) {
      fprintf(FPINFO, "\n");
      fflush(FPINFO);
    }
    fflush(FPINFO);
  } /* end of main iteration loop */

  /************
   * cleanup  *
   ************/
  if (loopParam.controlLogRootname) {
    if (!SDDS_UpdatePage(&controlLogDataSet, 0) || !SDDS_Terminate(&controlLogDataSet))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    free(loopParam.controlLogRootname);
  }
  return (0);
}

long sdds2dfeedforward_parseArguments(char ***argv, int *argc, FEEDFORWARD_DATA *feedforwardData,
                                    LOOP_PARAM *loopParam, AVERAGE_PARAM *aveParam,
                                    TESTS *testConditions, long *testCASecurity, char *commandline_option[COMMANDLINE_OPTIONS], double *pendIOtime) {
  SCANNED_ARG *s_arg;
  long i_arg, infinite = 0;
  long unsigned dummyFlag;
  double timeout;
  long retries;

  *argc = scanargs(&s_arg, *argc, *argv);

  sdds2dfeedforward_initializeData(feedforwardData, loopParam, aveParam, testConditions);
  *testCASecurity = 0;

  loopParam->offsetOnly = 0;
  for (i_arg = 1; i_arg < *argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_TIME_INTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->interval) != 1) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("no number given for option -timeInterval.\n");
        }
        break;
      case CLO_INFINITELOOP:
        infinite = 1;
        break;
      case CLO_ADVANCE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->advance) != 1 ||
            loopParam->advance < 0) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("no (valid) number given for option -advance.\n");
        }
        break;
      case CLO_CONTROLLOG:
        if (s_arg[i_arg].n_items < 2) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("invalid -controllog syntax.\n");
        }
        if (!SDDS_CopyString(&loopParam->controlLogRootname, s_arg[i_arg].list[1])) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("Problem -controlLog syntax.\n");
        }
        if (s_arg[i_arg].n_items == 3 && strncmp(s_arg[i_arg].list[2], "logonchange",
                                                 strlen(s_arg[i_arg].list[1])) == 0)
          loopParam->logOnChange = 1;
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &loopParam->steps) != 1) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("no number given for option -steps.\n");
        }
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("wrong number of items for -ezcaTiming");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || sdds2dfeedforwardGlobal->rcParam.pingTimeout < 0)
          bomb("invalid -ezca values", NULL);
        *pendIOtime = timeout * (retries + 1);
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", pendIOtime) != 1 ||
            *pendIOtime <= 0) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("invalid -pendIOtime syntax\n");
        }
        break;
      case CLO_DRY_RUN:
        loopParam->dryRun = 1;
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &sdds2dfeedforwardGlobal->rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &sdds2dfeedforwardGlobal->rcParam.pingTimeout, 1, 0,
                          NULL) ||
            !sdds2dfeedforwardGlobal->rcParam.PV) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          bomb("bad -runControlPV syntax", "-runControlPV={string=<string>[,pingTimeOut=<value>}\n");
        }
        sdds2dfeedforwardGlobal->rcParam.pingTimeout *= 1000.0;
#else
        fprintf(FPINFO, "runControl is not available. Option -runControlPV ignored.\n");
        fflush(FPINFO);
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if (s_arg[i_arg].n_items != 2 ||
            !SDDS_CopyString(&sdds2dfeedforwardGlobal->rcParam.Desc, s_arg[i_arg].list[1])) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("bad -runControlDesc syntax");
        }
#else
        fprintf(FPINFO, "runControl is not available. Option -runControlDescription ignored.\n");
        fflush(FPINFO);
#endif
        break;
      case CLO_VERBOSE:
        loopParam->verbose = 1;
        fprintf(FPINFO, "Link date: %s %s\n", __DATE__, __TIME__);
        fflush(FPINFO);
        break;
      case CLO_TESTCASECURITY:
        *testCASecurity = 1;
        break;
      case CLO_TEST_VALUES:
        if (s_arg[i_arg].n_items != 2 ||
            !SDDS_CopyString(&testConditions->file, s_arg[i_arg].list[1])) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("No file given in option -testValues.\n");
        }
        break;
      case CLO_AVERAGE:
        if ((s_arg[i_arg].n_items -= 2) < 0 ||
            !scanItemList(&dummyFlag, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "interval", SDDS_DOUBLE, &aveParam->interval, 1, 0, NULL) ||
            aveParam->interval <= 0) {
          s_arg[i_arg].n_items += 2;
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("bad -average syntax");
        }
        s_arg[i_arg].n_items += 2;
        if (sscanf(s_arg[i_arg].list[1], "%ld", &aveParam->n) != 1 ||
            aveParam->n <= 0) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("bad -average syntax.");
        }
        break;
      case CLO_OFFSETONLY:
        loopParam->offsetOnly = 1;
        break;
      default:
        fprintf(FPINFO, "Unrecognized option %s given.\n", s_arg[i_arg].list[0]);
        free_scanargs(&s_arg, *argc);
        exit(1);
      }
    } else {
      if (!feedforwardData->file) {
        if (!SDDS_CopyString(&feedforwardData->file, s_arg[i_arg].list[0])) {
          free_scanargs(&s_arg, *argc);
          SDDS_Bomb("Problem with input filename.\n");
        }
      } else {
        free_scanargs(&s_arg, *argc);
        SDDS_Bomb("Too many filenames given");
      }
    }
  }
  if (infinite)
    loopParam->steps = INT_MAX;
  free_scanargs(&s_arg, *argc);
  return (0);
}

void sdds2dfeedforward_interrupt_handler(int sig) {
  exit(1);
}

void sdds2dfeedforward_sigint_interrupt_handler(int sig) {
  signal(SIGINT, sdds2dfeedforward_interrupt_handler);
  signal(SIGTERM, sdds2dfeedforward_interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, sdds2dfeedforward_interrupt_handler);
#endif
  sdds2dfeedforwardGlobal->sigint = 1;
  return;
}

#ifdef USE_RUNCONTROL

/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    sdds2dfeedforwardGlobal->rcParam.status = runControlPing(sdds2dfeedforwardGlobal->rcParam.handle, &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
    switch (sdds2dfeedforwardGlobal->rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(FPINFO, "Run control application aborted.\n");
      fflush(FPINFO);
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(FPINFO, "Run control application timed out.\n");
      fflush(FPINFO);
      strcpy(sdds2dfeedforwardGlobal->rcParam.message, "Application timed-out");
      runControlLogMessage(sdds2dfeedforwardGlobal->rcParam.handle, sdds2dfeedforwardGlobal->rcParam.message, MAJOR_ALARM, &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
      exit(1);
      break;
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(FPINFO, "Communications error with runcontrol record.\n");
      fflush(FPINFO);
      return (RUNCONTROL_ERROR);
    default:
      fprintf(FPINFO, "Unknown run control error code.\n");
      fflush(FPINFO);
      break;
    }
    oag_ca_pend_event(MIN(timeLeft, sdds2dfeedforwardGlobal->rcParam.pingInterval), &(sdds2dfeedforwardGlobal->sigint));
    if (sdds2dfeedforwardGlobal->sigint)
      exit(1);
    timeLeft -= sdds2dfeedforwardGlobal->rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void getControlDataValues(CONTROL_DATA *readback, AVERAGE_PARAM *aveParam,
                          long initialValues, LOOP_PARAM *loopParam, double pendIOtime) {
  long i;
  double *ptr;

  if (loopParam->verbose) {
    if (initialValues)
      fprintf(FPINFO, "Reading initial values...");
    else
      fprintf(FPINFO, "Reading values at %f seconds...", loopParam->elapsedTime);
    fflush(FPINFO);
  }

  if (!initialValues) {
    /* swap last and new value arrays instead of copying */
    ptr = readback->value;
    readback->value = readback->lastValue;
    readback->lastValue = ptr;
  }

  if (aveParam && aveParam->n > 1) {
    if (ReadAverageScalarValues(readback->controlName, NULL, NULL,
                                readback->value, readback->controlNames,
                                aveParam->n, aveParam->interval,
                                readback->cid, pendIOtime)) {
      SDDS_Bomb("Error in reading values of PVs.\n");
    }
  } else {
    if (ReadScalarValues(readback->controlName, NULL, NULL,
                         readback->value, readback->controlNames,
                         readback->cid, pendIOtime)) {
      SDDS_Bomb("Error in reading values of PVs.\n");
    }
  }

  if (initialValues)
    for (i = 0; i < readback->controlNames; i++) {
      readback->lastValue[i] = readback->initialValue[i] = readback->value[i];
      readback->referenceValue[i] = DBL_MAX / 10.0;
    }
  if (loopParam->verbose) {
    fprintf(FPINFO, "done.\n");
    fflush(FPINFO);
  }
}

long ReadEnablePVValues(ENABLE_DATA *enablePVData, AVERAGE_PARAM *aveParam,
                        long initialValues, LOOP_PARAM *loopParam, double pendIOtime) {
  long i, average = 1, j, pend = 0, caErrors = 0;
  double interval = 0.0, *sum = NULL, *sum2 = NULL, *sum3 = NULL;

  /*do nothing if no feedforward enable pvs provided */
  if (!enablePVData->controlNames)
    return 0;
  if (loopParam->verbose) {
    if (initialValues)
      fprintf(FPINFO, "Reading initial values of enable PVs...\n");
    else
      fprintf(FPINFO, "Reading values of enable PVs at %f seconds...\n", loopParam->elapsedTime);
    fflush(FPINFO);
  }
  if (aveParam && aveParam->n > 1) {
    average = aveParam->n;
    interval = aveParam->interval;
  }
  if (average > 1) {
    sum = (double *)calloc(enablePVData->controlNames, sizeof(*sum));
    if (enablePVData->controlName2)
      sum2 = (double *)calloc(enablePVData->controlNames, sizeof(*sum2));
    if (enablePVData->test_pvName)
      sum3 = (double *)calloc(enablePVData->controlNames, sizeof(*sum3));
  }

  for (i = 0; i < average; i++) {
    pend = 0;
    for (j = 0; j < enablePVData->controlNames; j++) {
      if (enablePVData->controlName && enablePVData->controlName[j] && strlen(enablePVData->controlName[j])) {
        if (!enablePVData->cid[j]) {
          pend = 1;
          if (ca_search(enablePVData->controlName[j], &(enablePVData->cid[j])) != ECA_NORMAL) {
            fprintf(FPINFO, "error: problem doing search for %s\n", enablePVData->controlName[j]);
            fflush(FPINFO);
            return -1;
          }
        }
      }
      if (enablePVData->controlName2 && enablePVData->controlName2[j] && strlen(enablePVData->controlName2[j])) {
        if (!enablePVData->cid2[j]) {
          pend = 1;
          if (ca_search(enablePVData->controlName2[j], &(enablePVData->cid2[j])) != ECA_NORMAL) {
            fprintf(FPINFO, "error: problem doing search for %s\n", enablePVData->controlName2[j]);
            fflush(FPINFO);
            return -1;
          }
        }
      }
      if (enablePVData->test_pvName && enablePVData->test_pvName[j] && strlen(enablePVData->test_pvName[j])) {
        if (!enablePVData->test_id[j]) {
          pend = 1;
          if (ca_search(enablePVData->test_pvName[j], &(enablePVData->test_id[j])) != ECA_NORMAL) {
            fprintf(FPINFO, "error: problem doing search for %s\n", enablePVData->test_pvName[j]);
            fflush(FPINFO);
            return -1;
          }
        }
      }
    }
    if (pend) {
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(FPINFO, "error: problem doing search for some enable pv channels\n");
        fflush(FPINFO);
        for (j = 0; j < enablePVData->controlNames; j++) {
          if (strlen(enablePVData->controlName[j]) && ca_state(enablePVData->cid[j]) != cs_conn)
            fprintf(FPINFO, "%s not connected\n", enablePVData->controlName[j]);
          if (enablePVData->controlName2 && strlen(enablePVData->controlName2[j]) && ca_state(enablePVData->cid2[j]) != cs_conn)
            fprintf(FPINFO, "%s not connected\n", enablePVData->controlName2[j]);
          if (strlen(enablePVData->test_pvName[j]) && ca_state(enablePVData->test_id[j]) != cs_conn)
            fprintf(FPINFO, "%s not connected\n", enablePVData->test_pvName[j]);
          fflush(FPINFO);
        }
        return -1;
      }
    }

    for (j = 0; j < enablePVData->controlNames; j++) {
      if (enablePVData->controlName[j] && strlen(enablePVData->controlName[j])) {
        if (ca_state(enablePVData->cid[j]) == cs_conn) {
          if (ca_get(DBR_DOUBLE, enablePVData->cid[j], enablePVData->value + j) != ECA_NORMAL) {
            enablePVData->value[j] = 0;
            caErrors++;
            fprintf(FPINFO, "error: problem reading %s\n", enablePVData->controlName[j]);
            fflush(FPINFO);
          }
        } else {
          fprintf(FPINFO, "PV %s is not connected\n", enablePVData->controlName[j]);
          fflush(FPINFO);
          enablePVData->value[j] = 0;
          caErrors++;
        }
      }
      if (enablePVData->controlName2 && enablePVData->controlName2[j] && strlen(enablePVData->controlName2[j])) {
        if (ca_state(enablePVData->cid2[j]) == cs_conn) {
          if (ca_get(DBR_DOUBLE, enablePVData->cid2[j], enablePVData->value2 + j) != ECA_NORMAL) {
            enablePVData->value2[j] = 0;
            caErrors++;
            fprintf(FPINFO, "error: problem reading %s\n", enablePVData->controlName2[j]);
            fflush(FPINFO);
          }
        } else {
          fprintf(FPINFO, "PV %s is not connected\n", enablePVData->controlName2[j]);
          fflush(FPINFO);
          enablePVData->value2[j] = 0;
          caErrors++;
        }
      }
      if (enablePVData->test_pvName && enablePVData->test_pvName[j] && strlen(enablePVData->test_pvName[j])) {
        if (ca_state(enablePVData->test_id[j]) == cs_conn) {
          if (ca_get(DBR_DOUBLE, enablePVData->test_id[j], enablePVData->test_value + j) != ECA_NORMAL) {
            enablePVData->test_value[j] = 0;
            caErrors++;
            fprintf(FPINFO, "error: problem reading %s\n", enablePVData->test_pvName[j]);
            fflush(FPINFO);
          }
        } else {
          fprintf(FPINFO, "PV %s is not connected\n", enablePVData->test_pvName[j]);
          fflush(FPINFO);
          enablePVData->test_value[j] = 0;
          caErrors++;
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(FPINFO, "error: problem reading some channels\n");
      fflush(FPINFO);
      return -1;
    }
    if (average > 1) {
      for (j = 0; j < enablePVData->controlNames; j++) {
        if (enablePVData->controlName[j] && strlen(enablePVData->controlName[j]))
          sum[j] += enablePVData->value[j];
        if (enablePVData->controlName2 && enablePVData->controlName2[j] && strlen(enablePVData->controlName2[j]))
          sum2[j] += enablePVData->value2[j];
        if (enablePVData->test_pvName && enablePVData->test_pvName[j] && strlen(enablePVData->test_pvName[j]))
          sum3[j] += enablePVData->test_value[j];
      }
      if (i) {
        oag_ca_pend_event(interval, &(sdds2dfeedforwardGlobal->sigint));
      }
    }
  }

  if (average > 1) {
    for (j = 0; j < enablePVData->controlNames; j++) {
      if (enablePVData->controlName[j] && strlen(enablePVData->controlName[j]))
        enablePVData->value[j] = sum[j] / average;
      if (enablePVData->controlName2 && enablePVData->controlName2[j] && strlen(enablePVData->controlName2[j]))
        enablePVData->value2[j] = sum2[j] / average;
      if (enablePVData->test_pvName && enablePVData->test_pvName[j] && strlen(enablePVData->test_pvName[j]))
        enablePVData->test_value[j] = sum3[j] / average;
    }
    free(sum);
    if (enablePVData->controlName2)
      free(sum2);
    if (enablePVData->test_pvName)
      free(sum3);
  }

  for (j = 0; j < enablePVData->controlNames; j++) {
    enablePVData->outOfLimit[j] = 0;
    if (enablePVData->outOfLimit2)
      enablePVData->outOfLimit2[j] = 0;
    if (enablePVData->outOfTest)
      enablePVData->outOfTest[j] = 0;
    if (enablePVData->test_pvName && enablePVData->test_pvName[j] && strlen(enablePVData->test_pvName[j])) {
      if (enablePVData->test_value[j] > enablePVData->test_upperLimit[j] ||
          enablePVData->test_value[j] < enablePVData->test_lowerLimit[j])
        enablePVData->outOfTest[j] = 1;
      if (loopParam->verbose) {
        fprintf(FPINFO, "Test PV %s value is %f compared to [%e, %e]--- %s. If out of range, no action will be taken\n",
                enablePVData->test_pvName[j], enablePVData->test_value[j],
                enablePVData->test_lowerLimit[j], enablePVData->test_upperLimit[j],
                enablePVData->outOfTest[j] ? "out of range" : "in range");
        fflush(FPINFO);
      }
    }
    if (enablePVData->controlName && enablePVData->controlName[j] && strlen(enablePVData->controlName[j])) {
      if (enablePVData->value[j] > enablePVData->upperLimit[j] ||
          enablePVData->value[j] < enablePVData->lowerLimit[j])
        enablePVData->outOfLimit[j] = 1;
      if (loopParam->verbose) {
        fprintf(FPINFO, "Enable PV %s value is %f compared to [%e, %e]--- %s\n",
                enablePVData->controlName[j], enablePVData->value[j],
                enablePVData->lowerLimit[j], enablePVData->upperLimit[j],
                enablePVData->outOfLimit[j] ? "out of range" : "in range");
        fflush(FPINFO);
      }
    }
    if (enablePVData->controlName2 && enablePVData->controlName2[j] && strlen(enablePVData->controlName2[j])) {
      if (enablePVData->value2[j] > enablePVData->upperLimit2[j] ||
          enablePVData->value2[j] < enablePVData->lowerLimit2[j])
        enablePVData->outOfLimit2[j] = 1;
      if (loopParam->verbose) {
        fprintf(FPINFO, "Enable2 PV %s value is %f compared to [%e, %e]--- %s\n",
                enablePVData->controlName2[j], enablePVData->value2[j],
                enablePVData->lowerLimit2[j], enablePVData->upperLimit2[j],
                enablePVData->outOfLimit2[j] ? "out of range" : "in range");
        fflush(FPINFO);
      }
    }
  }
  return caErrors;
}

void sdds2dfeedforward_initializeData(FEEDFORWARD_DATA *feedforwardData,
                                    LOOP_PARAM *loopParam,
                                    AVERAGE_PARAM *aveParam,
                                    TESTS *test) {
  feedforwardData->file = NULL;
  feedforwardData->group = NULL;
  feedforwardData->groups = 0;
  feedforwardData->flags = 0;

  loopParam->step = -1;
  loopParam->steps = DEFAULT_STEPS;
  loopParam->dryRun = 0;
  loopParam->interval = DEFAULT_TIME_INTERVAL;
  loopParam->advance = DEFAULT_TIME_ADVANCE;
  loopParam->verbose = 0;
  loopParam->controlLogRootname = NULL;
  loopParam->logOnChange = 0;

  aveParam->n = 1;
  aveParam->interval = DEFAULT_AVEINTERVAL;

  test->file = NULL;
  test->value = test->min = test->sleep = test->reset = NULL;
  test->controlName = NULL;
  test->exitOnFailure = NULL;

#ifdef USE_RUNCONTROL
  sdds2dfeedforwardGlobal->rcParam.PV = sdds2dfeedforwardGlobal->rcParam.Desc = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  sdds2dfeedforwardGlobal->rcParam.pingInterval = 2;
  sdds2dfeedforwardGlobal->rcParam.pingTimeout = 10;
  sdds2dfeedforwardGlobal->rcParam.alarmSeverity = NO_ALARM;
#endif
  return;
}

void sdds2dfeedforward_setupData(FEEDFORWARD_DATA *feedforwardData,
                               LOOP_PARAM *loopParam,
                               TESTS *test,
                               CONTROL_DATA *readback1Data,
                               CONTROL_DATA *readback2Data,
                               CONTROL_DATA *actuatorData,
                               ENABLE_DATA *enablePVData) {
  readFeedForwardFile(feedforwardData, readback1Data, readback2Data, actuatorData, enablePVData,
                      loopParam);

  readTestsFile(test, readback1Data, readback2Data, actuatorData, loopParam);

#ifdef USE_RUNCONTROL
  if (sdds2dfeedforwardGlobal->rcParam.pingTimeout == 0.0) {
    sdds2dfeedforwardGlobal->rcParam.pingTimeout = 1000 * 2 *
                                                 MAX(loopParam->interval, sdds2dfeedforwardGlobal->rcParam.pingInterval);
  }

#endif
  return;
}

void setupControlLogFile(char *logRootname, SDDS_DATASET *logDataSet) {
  long i;
  char *filename = NULL;
  double StartTime, StartDay, StartHour, StartJulianDay, StartMonth, StartYear;
  char *TimeStamp;

  getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                   &StartYear, &TimeStamp);
  filename = MakeDailyGenerationFilename(logRootname, 4, ".", 0);
  if (!SDDS_InitializeOutput(logDataSet, SDDS_BINARY, 1, NULL, NULL, filename) ||
      !DefineLoggingTimeParameters(logDataSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_DefineSimpleColumn(logDataSet, "Step", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(logDataSet, "Time", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleColumn(logDataSet, "TimeOfDay", NULL, SDDS_FLOAT))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_DefineSimpleColumns(logDataSet, sdds2dfeedforwardGlobal->actuatorData.controlNames,
                                sdds2dfeedforwardGlobal->actuatorData.controlName, NULL, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < sdds2dfeedforwardGlobal->readback1Data.controlNames; i++) {
    if (sdds2dfeedforwardGlobal->readback1Data.flag[i]) {
      if (SDDS_DefineColumn(logDataSet, sdds2dfeedforwardGlobal->readback1Data.controlName[i], NULL,
                            NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  for (i = 0; i < sdds2dfeedforwardGlobal->readback2Data.controlNames; i++) {
    if (sdds2dfeedforwardGlobal->readback2Data.flag[i]) {
      if (SDDS_DefineColumn(logDataSet, sdds2dfeedforwardGlobal->readback2Data.controlName[i], NULL,
                            NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  for (i = 0; i < sdds2dfeedforwardGlobal->enablePVData.controlNames; i++) {
    if (sdds2dfeedforwardGlobal->enablePVData.flag[i]) {
      if (SDDS_DefineColumn(logDataSet, sdds2dfeedforwardGlobal->enablePVData.controlName[i], NULL,
                            NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (sdds2dfeedforwardGlobal->enablePVData.controlName2 && sdds2dfeedforwardGlobal->enablePVData.flag2[i]) {
      if (SDDS_DefineColumn(logDataSet, sdds2dfeedforwardGlobal->enablePVData.controlName2[i], NULL,
                            NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (sdds2dfeedforwardGlobal->enablePVData.test_pvName && sdds2dfeedforwardGlobal->enablePVData.test_flag[i]) {
      if (SDDS_DefineColumn(logDataSet, sdds2dfeedforwardGlobal->enablePVData.test_pvName[i], NULL,
                            NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_DefineSimpleParameters(logDataSet, 0, NULL, NULL, SDDS_STRING) ||
      !SDDS_WriteLayout(logDataSet) ||
      !SDDS_StartPage(logDataSet, 0))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SetParameters(logDataSet, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "TimeStamp", TimeStamp, "PageTimeStamp", TimeStamp,
                          "StartTime", StartTime, "StartYear", (short)StartYear,
                          "YearStartTime", computeYearStartTime(StartTime),
                          "StartJulianDay", (short)StartJulianDay, "StartMonth", (short)StartMonth,
                          "StartDayOfMonth", (short)StartDay, "StartHour", (float)StartHour,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(filename);
}

void readTestsFile(TESTS *test, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, CONTROL_DATA *actuatorData,
                   LOOP_PARAM *loopParam) {
  long itest;
  SDDS_TABLE testValuesPage;
  long sleepTimeColumnPresent = 0, resetTimeColumnPresent = 0;

  if (!test->file)
    return;
  if (!SDDS_InitializeInput(&testValuesPage, test->file))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  switch (SDDS_CheckColumn(&testValuesPage, "ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", test->file);
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("");
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MinimumValue", NULL, SDDS_ANY_FLOATING_TYPE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MinimumValue", test->file);
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("");
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MaximumValue", NULL, SDDS_ANY_FLOATING_TYPE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(FPINFO, "Something wrong with column %s in file %s.\n",
            "MaximumValue", test->file);
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("");
  }
  switch (SDDS_CheckColumn(&testValuesPage, "SleepTime", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepTimeColumnPresent = 0;
    break;
  default:
    fprintf(FPINFO, "Something wrong with column %s in file %s.\n",
            "SleepTime", test->file);
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("");
  }
  switch (SDDS_CheckColumn(&testValuesPage, "ResetTime", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    resetTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    resetTimeColumnPresent = 0;
    break;
  default:
    fprintf(FPINFO, "Something wrong with column %s in file %s.\n",
            "ResetTime", test->file);
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("");
  }
  if (SDDS_ReadPage(&testValuesPage) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&testValuesPage);
    exit(1);
  }
  if (!(test->n = SDDS_RowCount(&testValuesPage))) {
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("test data file is empty");
  }
  if (!(test->value = (double *)malloc(sizeof(double) * (test->n))) ||
      !(test->outOfRange = (long *)malloc(sizeof(long) * (test->n)))) {
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("Memory allocation failure2");
  }
  if (loopParam->verbose) {
    fprintf(FPINFO, "number of test values in file %s is %ld\n", test->file, test->n);
    fflush(FPINFO);
  }
  if (!(test->controlName = (char **)SDDS_GetColumn(&testValuesPage, "ControlName")) ||
      !(test->min = SDDS_GetColumnInDoubles(&testValuesPage, "MinimumValue")) ||
      !(test->max = SDDS_GetColumnInDoubles(&testValuesPage, "MaximumValue")) ||
      (sleepTimeColumnPresent &&
       !(test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, "SleepTime"))) ||
      (resetTimeColumnPresent &&
       !(test->reset = SDDS_GetColumnInDoubles(&testValuesPage, "ResetTime")))) {
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("Problem getting data columns from test data file.");
  }
  if (SDDS_CheckColumn(&testValuesPage, "ExitOnFailure", NULL, SDDS_ANY_INTEGER_TYPE, NULL) == SDDS_CHECK_OKAY) {
    if (!(test->exitOnFailure = (short *)SDDS_GetColumnInShort(&testValuesPage, "ExitOnFailure"))) {
      SDDS_Terminate(&testValuesPage);
      SDDS_Bomb("Problem getting data columns from test data file.");
    }
  } else {
    if (!(test->exitOnFailure = (short *)calloc(test->n, sizeof(*(test->exitOnFailure))))) {
      SDDS_Terminate(&testValuesPage);
      SDDS_Bomb("Memory allocation failure3");
    }
  }
  if (!(test->cid = SDDS_Calloc(1, sizeof(*(test->cid)) * test->n))) {
    SDDS_Terminate(&testValuesPage);
    SDDS_Bomb("Memory allocation failure4");
  }
  if (!SDDS_Terminate(&testValuesPage))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (loopParam->verbose) {
    fprintf(FPINFO, " # %20s\t%10s\t%10s",
            "ControlName", "MinLimit", "MaxLimit");
    fflush(FPINFO);
    if (sleepTimeColumnPresent)
      fprintf(FPINFO, "\t%10s", "Sleep(s)");
    if (resetTimeColumnPresent)
      fprintf(FPINFO, "\t%10s", "Reset(s)");
    fputc('\n', FPINFO);
    fflush(FPINFO);
    for (itest = 0; itest < test->n; itest++) {
      fprintf(FPINFO, "%3ld%20s\t%10.6g\t%10.6g", itest, test->controlName[itest],
              test->min[itest], test->max[itest]);
      if (sleepTimeColumnPresent)
        fprintf(FPINFO, "\t%10.6f", test->sleep[itest]);
      if (resetTimeColumnPresent)
        fprintf(FPINFO, "\t%10.6f", test->reset[itest]);
      fprintf(FPINFO, "\n");
      fflush(FPINFO);
    }
  }
}

void gridifyData(char *var1Name, char *var2Name, double *var1Value, double *var2Value, long gridPoints, long *n1, long *n2)
{
  long i, j;
  char s[256], *gridVariable[2];
  double *gridValue[2];
  long ng[2];
  long i1, i2;
  
  gridVariable[0] = var1Name;
  gridVariable[1] = var2Name;
  
  gridValue[0] = var1Value;
  gridValue[1] = var2Value;
  
  for (i = 0; i < 2; i++) {
    double *copy = tmalloc(sizeof(double) * gridPoints);
    memcpy(copy, gridValue[i], gridPoints * sizeof(double));
    qsort((void *)copy, gridPoints, sizeof(double), double_cmpasc);
    ng[i] = 1;
    for (j = 1; j < gridPoints; j++) {
      if (copy[j - 1] != copy[j]) {
        ng[i] += 1;
      }
    }
    free(copy);
    if (ng[i] == gridPoints) {
      snprintf(s, sizeof(s), "Grid variable %s has only unique values.\n", gridVariable[i]);
      SDDS_Bomb(s);
    }
    if (ng[i] == 1) {
      snprintf(s, sizeof(s), "Grid variable %s has only one unique value.\n", gridVariable[i]);
      SDDS_Bomb(s);
    }
  }
  if (ng[0] * ng[1] != gridPoints) {
    snprintf(s, sizeof(s), "Input data does not form a grid for %s and %s (nx = %ld, ny=%ld, rows=%ld)\n",
             var1Name, var2Name, ng[0], ng[1], gridPoints);
    SDDS_Bomb(s);
  }
  /*  printf("File has %ld rows, %ld x %ld\n", gridPoints, ng[0], ng[1]); */
  
  /* check sorting */
  for (i1=0; i1<ng[0]; i1++) {
    for (i2=1; i2<ng[1]; i2++) {
      long index1, index2;
      if ((index1 = i1*ng[1] + i2 - 1)<0)
        continue;
      if ((index2 = index1 + 1)>=gridPoints)
        continue;
      if (var2Value[index2]<=var2Value[index1])
        SDDS_Bomb("Input data not properly sorted (1). Use\nsddssort -column=Readback1Value -column=Readback2Value <filename>\n");
    }
  }
  for (i2=0; i2<ng[1]; i2++) {
    for (i1=0; i1<ng[0]; i1++) {
      long index1, index2;
      if ((index1 = i1*ng[1] + i2 - 1)<0)
        continue;
      if ((index2 = index1 + ng[1])>=gridPoints)
        continue;
      if (var1Value[index2]<=var1Value[index1])
        SDDS_Bomb("Input data not properly sorted (2). Use\nsddssort -column=Readback1Value -column=Readback2Value <filename>\n");
    }
  }
  *n1 = ng[0];
  *n2 = ng[1];
}

void readFeedForwardFile(FEEDFORWARD_DATA *feedforwardData, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, CONTROL_DATA *actuatorData,
                         ENABLE_DATA *enablePVData, LOOP_PARAM *loopParam) {
  SDDS_DATASET FFdataset;
  long code;
  char *readback1Name = NULL, *readback2Name = NULL, *actuatorName = NULL;
  double changeThreshold, changeLimit, lowerLimit, upperLimit;
  long thresholdPresent, limitPresent, enablePVPresent, enableLowerLimitPresent, lowerLimitPresent, upperLimitPresent;
  long enable2PVPresent, enable2LowerLimitPresent, enable2UpperLimitPresent, testPVPresent, testLowerLimitPresent, testUpperLimitPresent;
  long enableUpperLimitPresent, disabledOutputPresent;
  long iGroup, iActuator, nActuators;
  FEEDFORWARD_GROUP *ffGroup;
  char *TestPV = NULL;
  long index, nData, n1, n2, i;

  if (!feedforwardData->file)
    SDDS_Bomb("input filename not given");
  if (!SDDS_InitializeInput(&FFdataset, feedforwardData->file))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (loopParam->verbose) {
    fprintf(FPINFO, "Reading feedforward data file %s\n", feedforwardData->file);
    fflush(FPINFO);
  }
  if (SDDS_CheckParameter(&FFdataset, "Readback1Name", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY ||
      SDDS_CheckParameter(&FFdataset, "Readback2Name", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY ||      
      SDDS_CheckParameter(&FFdataset, "ActuatorName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&FFdataset, "Readback1Value", NULL, SDDS_ANY_FLOATING_TYPE, stderr) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&FFdataset, "Readback2Value", NULL, SDDS_ANY_FLOATING_TYPE, stderr) != SDDS_CHECK_OKAY ||
      SDDS_CheckColumn(&FFdataset, "ActuatorValue", NULL, SDDS_ANY_FLOATING_TYPE, stderr) != SDDS_CHECK_OKAY) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
  }

  thresholdPresent = limitPresent = lowerLimitPresent = upperLimitPresent = 0;
  enablePVPresent = enableLowerLimitPresent = enableUpperLimitPresent = disabledOutputPresent = 0;
  enable2PVPresent = enable2LowerLimitPresent = enable2UpperLimitPresent = testPVPresent = testLowerLimitPresent = testUpperLimitPresent = 0;
  switch (SDDS_CheckParameter(&FFdataset, "ReadbackChangeThreshold", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    thresholdPresent = 1;
    feedforwardData->flags += READBACK_CHANGE_THRESHOLD;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "ActuatorChangeLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    limitPresent = 1;
    feedforwardData->flags += ACTUATOR_CHANGE_LIMIT;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  switch (SDDS_CheckParameter(&FFdataset, "ActuatorLowerLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    lowerLimitPresent = 1;
    feedforwardData->flags += ACTUATOR_LOWER_LIMIT;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  switch (SDDS_CheckParameter(&FFdataset, "ActuatorUpperLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    upperLimitPresent = 1;
    feedforwardData->flags += ACTUATOR_UPPER_LIMIT;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  if (limitPresent && thresholdPresent) {
    fprintf(FPINFO, "Error: can't use ReadbackChangeThreshold and ActuatorChangeLimit together.\n");
    fflush(FPINFO);
    exit(1);
  }

  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnablePV", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    enablePVPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnableLowerLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    enableLowerLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnableUpperLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    enableUpperLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnable2PV", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    enable2PVPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnable2LowerLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    enable2LowerLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardEnable2UpperLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    enable2UpperLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardTestPV", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    testPVPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardTestLowerLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    testLowerLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardTestUpperLimit", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    testUpperLimitPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }

  switch (SDDS_CheckParameter(&FFdataset, "FeedforwardDisabledOutputValue", NULL, SDDS_ANY_FLOATING_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    disabledOutputPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&FFdataset);
    exit(1);
    break;
  }
  if (enablePVPresent) {
    if (!enableLowerLimitPresent || !enableUpperLimitPresent || !disabledOutputPresent) {
      if (loopParam->verbose) {
        fprintf(FPINFO, "Warning: The feedforward enable pv will be ignore since its lower limit or upper limit or the disabled output value is not provided.\n");
        fflush(FPINFO);
        enablePVPresent = 0;
      }
    }
  }
  if (enable2PVPresent) {
    if (!enable2LowerLimitPresent || !enable2UpperLimitPresent || !disabledOutputPresent || !enablePVPresent) {
      if (loopParam->verbose) {
        fprintf(FPINFO, "Warning: The feedforward enable2 pv will be ignore since its lower limit or upper limit or the disabled output value or enable PV is not provided.\n");
        fflush(FPINFO);
        enable2PVPresent = 0;
      }
    }
  }
  if (testPVPresent) {
    if (!testLowerLimitPresent || !testUpperLimitPresent) {
      if (loopParam->verbose) {
        fprintf(FPINFO, "Warning: The feedforward test pv will be ignore since its lower limit or upper limit is not provided.\n");
        fflush(FPINFO);
        testPVPresent = 0;
      }
    }
  }
  enablePVData->controlName2 = enablePVData->test_pvName = NULL;
  nActuators = 0;
  while ((code = SDDS_ReadPage(&FFdataset)) > 0) {
    if (!SDDS_GetParameter(&FFdataset, "Readback1Name", &readback1Name) ||
        !SDDS_GetParameter(&FFdataset, "Readback2Name", &readback2Name) ||
        !SDDS_GetParameter(&FFdataset, "ActuatorName", &actuatorName) ||
        (thresholdPresent &&
         !SDDS_GetParameterAsDouble(&FFdataset, "ReadbackChangeThreshold", &changeThreshold)) ||
        (limitPresent &&
         !SDDS_GetParameterAsDouble(&FFdataset, "ActuatorChangeLimit", &changeLimit)) ||
        (lowerLimitPresent &&
         !SDDS_GetParameterAsDouble(&FFdataset, "ActuatorLowerLimit", &lowerLimit)) ||
        (upperLimitPresent &&
         !SDDS_GetParameterAsDouble(&FFdataset, "ActuatorUpperLimit", &upperLimit))) {
      if (readback1Name)
        free(readback1Name);
      if (readback2Name)
        free(readback2Name);
      if (actuatorName)
        free(actuatorName);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&FFdataset);
      exit(1);
    }
    if (testPVPresent && !SDDS_GetParameter(&FFdataset, "FeedforwardTestPV", &TestPV)) {
      if (TestPV)
        free(TestPV);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&FFdataset);
      exit(1);
    }
    if (TestPV)
      free(TestPV);
    if (loopParam->verbose) {
      fprintf(FPINFO, "Readback = (%s, %s), Actuator = %s\n", readback1Name, readback2Name, actuatorName);
      fflush(FPINFO);
    }
    /* see if this actuator is already in use */
    /*allow one actuator has multiple FF table, so ignore this check. 3/7/2017 Shang */

    nActuators++;
    /* search for group that uses the same readback as new group */
    for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++) {
      if (strcmp(feedforwardData->group[iGroup].readback1, readback1Name) == 0 &&
          strcmp(feedforwardData->group[iGroup].readback2, readback2Name) == 0)
        break;
    }
    if (iGroup == feedforwardData->groups) {
      /* create a new group */
      if (!(feedforwardData->group = SDDS_Realloc(feedforwardData->group, (iGroup + 1) * sizeof(*(feedforwardData->group))))) {
        if (readback1Name)
          free(readback1Name);
        if (readback2Name)
          free(readback2Name);
        if (actuatorName)
          free(actuatorName);
        SDDS_Terminate(&FFdataset);
        SDDS_Bomb("Memory allocation failure5");
      }
      feedforwardData->groups = iGroup + 1;
      ffGroup = feedforwardData->group + iGroup;
      ffGroup->readback1 = readback1Name;
      ffGroup->readback2 = readback2Name;
      ffGroup->actuator = NULL;
      ffGroup->actuators = 0;
      ffGroup->readbackChangeThreshold = (thresholdPresent ? changeThreshold : 0);
      ffGroup->actuatorDeltaLimit = ffGroup->actuatorLowerLimit = ffGroup->actuatorUpperLimit = NULL;
      ffGroup->actuatorData = ffGroup->readback1Data = ffGroup->readback2Data = NULL;
      ffGroup->enablePVIndex = NULL;
      ffGroup->n1Data = ffGroup->n2Data = NULL;
      ffGroup->actuatorIndex = NULL;
      ffGroup->readbackIndex = iGroup;
      ffGroup->ffValue = NULL;
    } else {
      if (readback1Name)
        free(readback1Name);
      if (readback2Name)
        free(readback2Name);
      ffGroup = feedforwardData->group + iGroup;
      if (thresholdPresent && changeThreshold < ffGroup->readbackChangeThreshold)
        ffGroup->readbackChangeThreshold = changeThreshold;
    }
    if (!(ffGroup->actuator = SDDS_Realloc(ffGroup->actuator, sizeof(*(ffGroup->actuator)) * (ffGroup->actuators + 1))) ||
        !(ffGroup->ffValue = SDDS_Realloc(ffGroup->ffValue, sizeof(*(ffGroup->ffValue)) * (ffGroup->actuators + 1))) ||
        !(ffGroup->actuatorDeltaLimit = SDDS_Realloc(ffGroup->actuatorDeltaLimit, sizeof(*(ffGroup->actuatorDeltaLimit)) *
                                                                                    (ffGroup->actuators + 1))) ||
        !(ffGroup->actuatorLowerLimit = SDDS_Realloc(ffGroup->actuatorLowerLimit, sizeof(*(ffGroup->actuatorLowerLimit)) *
                                                                                    (ffGroup->actuators + 1))) ||
        !(ffGroup->actuatorUpperLimit = SDDS_Realloc(ffGroup->actuatorUpperLimit, sizeof(*(ffGroup->actuatorUpperLimit)) *
                                                                                    (ffGroup->actuators + 1))) ||
        !(ffGroup->actuatorData = SDDS_Realloc(ffGroup->actuatorData, sizeof(*(ffGroup->actuatorData)) *
                                                                        (ffGroup->actuators + 1))) ||
        !(ffGroup->readback1Data = SDDS_Realloc(ffGroup->readback1Data, sizeof(*(ffGroup->readback1Data)) *
                                                                        (ffGroup->actuators + 1))) ||
        !(ffGroup->readback2Data = SDDS_Realloc(ffGroup->readback2Data, sizeof(*(ffGroup->readback2Data)) *
                                                                        (ffGroup->actuators + 1))) ||
        !(ffGroup->n1Data = SDDS_Realloc(ffGroup->n1Data, sizeof(*(ffGroup->n1Data)) *
                                         (ffGroup->actuators + 1))) ||
        !(ffGroup->n2Data = SDDS_Realloc(ffGroup->n2Data, sizeof(*(ffGroup->n2Data)) *
                                         (ffGroup->actuators + 1))) ||
        !(ffGroup->enablePVIndex = SDDS_Realloc(ffGroup->enablePVIndex,
                                                sizeof(*(ffGroup->enablePVIndex)) * (ffGroup->actuators + 1)))) {
      if (actuatorName)
        free(actuatorName);
      SDDS_Terminate(&FFdataset);
      SDDS_Bomb("Memory allocation failure6");
    }

    ffGroup->enablePVIndex[ffGroup->actuators] = -1;
    if (enablePVPresent) {
      /* store enable PV data */
      ffGroup->enablePVIndex[ffGroup->actuators] = nActuators - 1;
      enablePVData->controlNames = nActuators;
      if (!(enablePVData->controlName = SDDS_Realloc(enablePVData->controlName,
                                                     sizeof(*(enablePVData->controlName)) *
                                                       nActuators)) ||
          !(enablePVData->upperLimit = SDDS_Realloc(enablePVData->upperLimit,
                                                    sizeof(*(enablePVData->upperLimit)) *
                                                      nActuators)) ||
          !(enablePVData->lowerLimit = SDDS_Realloc(enablePVData->lowerLimit,
                                                    sizeof(*(enablePVData->lowerLimit)) *
                                                      nActuators)) ||
          !(enablePVData->disabledOutputValue = SDDS_Realloc(enablePVData->disabledOutputValue,
                                                             sizeof(*(enablePVData->disabledOutputValue)) *
                                                               nActuators)) ||
          !(enablePVData->outOfLimit = SDDS_Realloc(enablePVData->outOfLimit,
                                                    sizeof(*(enablePVData->outOfLimit)) *
                                                      nActuators)) ||
          !(enablePVData->outOfLimit2 = SDDS_Realloc(enablePVData->outOfLimit2,
                                                     sizeof(*(enablePVData->outOfLimit2)) *
                                                       nActuators)) ||
          !(enablePVData->cid = SDDS_Realloc(enablePVData->cid,
                                             sizeof(*(enablePVData->cid)) *
                                               nActuators)) ||
          !(enablePVData->flag = SDDS_Realloc(enablePVData->flag,
                                              sizeof(*(enablePVData->flag)) *
                                                nActuators)) ||
          !(enablePVData->value = SDDS_Realloc(enablePVData->value,
                                               sizeof(*(enablePVData->value)) *
                                                 nActuators))) {
        SDDS_Terminate(&FFdataset);
        SDDS_Bomb("Memory allocation failure7");
      }
      enablePVData->cid[nActuators - 1] = NULL;
      if (!SDDS_GetParameter(&FFdataset, "FeedforwardEnablePV", enablePVData->controlName + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardEnableLowerLimit",
                                     enablePVData->lowerLimit + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardEnableUpperLimit",
                                     enablePVData->upperLimit + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardDisabledOutputValue",
                                     enablePVData->disabledOutputValue + nActuators - 1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Terminate(&FFdataset);
        exit(1);
      }
    }

    if (enable2PVPresent) {
      /* store enable2 PV data */
      ffGroup->enablePVIndex[ffGroup->actuators] = nActuators - 1;
      enablePVData->controlNames = nActuators;
      if (!(enablePVData->controlName2 = SDDS_Realloc(enablePVData->controlName2,
                                                      sizeof(*(enablePVData->controlName)) *
                                                        nActuators)) ||
          !(enablePVData->upperLimit2 = SDDS_Realloc(enablePVData->upperLimit2,
                                                     sizeof(*(enablePVData->upperLimit)) *
                                                       nActuators)) ||
          !(enablePVData->lowerLimit2 = SDDS_Realloc(enablePVData->lowerLimit2,
                                                     sizeof(*(enablePVData->lowerLimit)) *
                                                       nActuators)) ||
          !(enablePVData->cid2 = SDDS_Realloc(enablePVData->cid2,
                                              sizeof(*(enablePVData->cid2)) *
                                                nActuators)) ||
          !(enablePVData->flag2 = SDDS_Realloc(enablePVData->flag2,
                                               sizeof(*(enablePVData->flag2)) *
                                                 nActuators)) ||
          !(enablePVData->value2 = SDDS_Realloc(enablePVData->value2,
                                                sizeof(*(enablePVData->value)) *
                                                  nActuators))) {
        SDDS_Terminate(&FFdataset);
        SDDS_Bomb("Memory allocation failure6");
      }
      enablePVData->cid2[nActuators - 1] = NULL;
      if (!SDDS_GetParameter(&FFdataset, "FeedforwardEnable2PV", enablePVData->controlName2 + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardEnable2LowerLimit",
                                     enablePVData->lowerLimit2 + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardEnable2UpperLimit",
                                     enablePVData->upperLimit2 + nActuators - 1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Terminate(&FFdataset);
        exit(1);
      }
      /*remove the quotes of enable2 pv */
      if (enablePVData->controlName2[nActuators - 1][0] == '\"' &&
          enablePVData->controlName2[nActuators - 1][strlen(enablePVData->controlName2[nActuators - 1]) - 1] == '\"') {
        strslide(enablePVData->controlName2[nActuators - 1], -1);
        enablePVData->controlName2[nActuators - 1][strlen(enablePVData->controlName2[nActuators - 1]) - 1] = '\0';
      }
    }

    if (testPVPresent) {
      /* store enable2 PV data */
      ffGroup->enablePVIndex[ffGroup->actuators] = nActuators - 1;
      enablePVData->controlNames = nActuators;
      if (!(enablePVData->test_pvName = SDDS_Realloc(enablePVData->test_pvName,
                                                     sizeof(*(enablePVData->test_pvName)) *
                                                       nActuators)) ||
          !(enablePVData->test_upperLimit = SDDS_Realloc(enablePVData->test_upperLimit,
                                                         sizeof(*(enablePVData->test_upperLimit)) *
                                                           nActuators)) ||
          !(enablePVData->test_lowerLimit = SDDS_Realloc(enablePVData->test_lowerLimit,
                                                         sizeof(*(enablePVData->test_lowerLimit)) *
                                                           nActuators)) ||
          !(enablePVData->test_id = SDDS_Realloc(enablePVData->test_id,
                                                 sizeof(*(enablePVData->test_id)) *
                                                   nActuators)) ||
          !(enablePVData->test_flag = SDDS_Realloc(enablePVData->test_flag,
                                                   sizeof(*(enablePVData->test_flag)) *
                                                     nActuators)) ||
          !(enablePVData->outOfTest = SDDS_Realloc(enablePVData->outOfTest,
                                                   sizeof(*(enablePVData->outOfTest)) *
                                                     nActuators)) ||
          !(enablePVData->test_value = SDDS_Realloc(enablePVData->test_value,
                                                    sizeof(*(enablePVData->test_value)) *
                                                      nActuators))) {
        SDDS_Terminate(&FFdataset);
        SDDS_Bomb("Memory allocation failure8");
      }
      enablePVData->test_id[nActuators - 1] = NULL;
      if (!SDDS_GetParameter(&FFdataset, "FeedforwardTestPV", enablePVData->test_pvName + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardTestLowerLimit",
                                     enablePVData->test_lowerLimit + nActuators - 1) ||
          !SDDS_GetParameterAsDouble(&FFdataset, "FeedforwardTestUpperLimit",
                                     enablePVData->test_upperLimit + nActuators - 1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Terminate(&FFdataset);
        exit(1);
      }
      /*remove the quotes of enable2 pv */
      if (enablePVData->test_pvName[nActuators - 1][0] == '\"' &&
          enablePVData->test_pvName[nActuators - 1][strlen(enablePVData->test_pvName[nActuators - 1]) - 1] == '\"') {
        strslide(enablePVData->test_pvName[nActuators - 1], -1);
        enablePVData->test_pvName[nActuators - 1][strlen(enablePVData->test_pvName[nActuators - 1]) - 1] = '\0';
      }
    }

    ffGroup->actuator[ffGroup->actuators] = actuatorName;
    if (limitPresent) {
      ffGroup->actuatorDeltaLimit[ffGroup->actuators] = changeLimit;
    }
    if (lowerLimitPresent)
      ffGroup->actuatorLowerLimit[ffGroup->actuators] = lowerLimit;
    if (upperLimitPresent)
      ffGroup->actuatorUpperLimit[ffGroup->actuators] = upperLimit;
    if ((nData = SDDS_RowCount(&FFdataset)) <= 0) {
      SDDS_Terminate(&FFdataset);
      SDDS_Bomb("Empty data table in feedforward data set.");
    }
    if (!(ffGroup->actuatorData[ffGroup->actuators] = SDDS_GetColumnInDoubles(&FFdataset, "ActuatorValue")) ||
        !(ffGroup->readback1Data[ffGroup->actuators] = SDDS_GetColumnInDoubles(&FFdataset, "Readback1Value")) ||
        !(ffGroup->readback2Data[ffGroup->actuators] = SDDS_GetColumnInDoubles(&FFdataset, "Readback2Value"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&FFdataset);
      SDDS_Bomb("Unable to read data arrays for feedforward.");
    }

    gridifyData(readback1Name, readback2Name,
                ffGroup->readback1Data[ffGroup->actuators], ffGroup->readback2Data[ffGroup->actuators], nData, &n1, &n2);
    ffGroup->n1Data[ffGroup->actuators] = n1;
    ffGroup->n2Data[ffGroup->actuators] = n2;
    
    ffGroup->actuators += 1;
  }
  if (!SDDS_Terminate(&FFdataset) || code == 0)
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (feedforwardData->groups == 0)
    SDDS_Bomb("No data in feedforward data file.");
  for (i = 0; i < enablePVData->controlNames; i++) {
    if (enablePVData->controlName[i] && strlen(enablePVData->controlName[i])) {
      if (!i || match_string(enablePVData->controlName[i], enablePVData->controlName, i, EXACT_MATCH) == -1)
        enablePVData->flag[i] = 1;
      else
        enablePVData->flag[i] = 0;
    } else
      enablePVData->flag[i] = 0;
    if (enable2PVPresent) {
      if (enablePVData->controlName2[i] && strlen(enablePVData->controlName2[i])) {
        if (match_string(enablePVData->controlName2[i], enablePVData->controlName, enablePVData->controlNames, EXACT_MATCH) == -1) {
          if (!i)
            enablePVData->flag2[i] = 1;
          else if (match_string(enablePVData->controlName2[i], enablePVData->controlName2, i, EXACT_MATCH) == -1)
            enablePVData->flag2[i] = 1;
        } else
          enablePVData->flag2[i] = 0;
      } else
        enablePVData->flag2[i] = 0;
    }
    if (testPVPresent) {
      if (enablePVData->test_pvName[i] && strlen(enablePVData->test_pvName[i])) {
        if (match_string(enablePVData->test_pvName[i], enablePVData->controlName, enablePVData->controlNames, EXACT_MATCH) == -1 &&
            match_string(enablePVData->test_pvName[i], enablePVData->controlName2, enablePVData->controlNames, EXACT_MATCH) == -1) {
          if (!i)
            enablePVData->test_flag[i] = 1;
          else if (match_string(enablePVData->test_pvName[i], enablePVData->test_pvName, i, EXACT_MATCH) == -1)
            enablePVData->test_flag[i] = 1;
        } else
          enablePVData->test_flag[i] = 0;
      } else
        enablePVData->test_flag[i] = 0;
    }
  }
  if (loopParam->verbose)
    for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++) {
      ffGroup = feedforwardData->group + iGroup;
      fprintf(FPINFO, "** Group %ld has %ld actuators\n", iGroup, ffGroup->actuators);
      fflush(FPINFO);
      for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
        fprintf(FPINFO, "Readback: (%s, %s)    Actuator: %s\n",
                ffGroup->readback1, ffGroup->readback2, ffGroup->actuator[iActuator]);
        fprintf(FPINFO, "  %ld x %ld rows of data\n", ffGroup->n1Data[iActuator], ffGroup->n2Data[iActuator]);
        fflush(FPINFO);
      }
    }

  actuatorData->controlName = SDDS_Malloc(sizeof(*(actuatorData->controlName)) * nActuators);
  /*remove duplicate actuators */
  nActuators = 0;
  for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++) {
    ffGroup = feedforwardData->group + iGroup;
    ffGroup->actuatorIndex = SDDS_Malloc(sizeof(*(ffGroup->actuatorIndex)) * ffGroup->actuators);
    if (iGroup == 0) {
      for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
        SDDS_CopyString(&(actuatorData->controlName[iActuator]), ffGroup->actuator[iActuator]);
        ffGroup->actuatorIndex[iActuator] = iActuator;
      }
      nActuators = ffGroup->actuators;
    } else {
      /*removed duplicate actuators */
      for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
        index = match_string(ffGroup->actuator[iActuator], actuatorData->controlName, nActuators, EXACT_MATCH);
        if (index == -1) {
          SDDS_CopyString(&(actuatorData->controlName[nActuators]), ffGroup->actuator[iActuator]);
          ffGroup->actuatorIndex[iActuator] = nActuators;
          nActuators++;
        } else {
          ffGroup->actuatorIndex[iActuator] = index;
        }
      }
    }
  }
  readback1Data->controlNames = feedforwardData->groups;
  readback1Data->computeValue = NULL;
  readback2Data->controlNames = feedforwardData->groups;
  readback2Data->computeValue = NULL;
  actuatorData->controlNames = nActuators;
  if (!(readback1Data->controlName = SDDS_Malloc(sizeof(*(readback1Data->controlName)) * feedforwardData->groups)) ||
      !(readback1Data->value = SDDS_Malloc(sizeof(*(readback1Data->value)) * feedforwardData->groups)) ||
      !(readback1Data->lastValue = SDDS_Malloc(sizeof(*(readback1Data->lastValue)) * feedforwardData->groups)) ||
      !(readback1Data->initialValue = SDDS_Malloc(sizeof(*(readback1Data->initialValue)) * feedforwardData->groups)) ||
      !(readback1Data->referenceValue = SDDS_Malloc(sizeof(*(readback1Data->referenceValue)) * feedforwardData->groups)) ||
      !(readback1Data->flag = SDDS_Malloc(sizeof(*(readback1Data->flag)) * feedforwardData->groups)) ||
      !(readback1Data->cid = SDDS_Calloc(1, sizeof(*(readback1Data->cid)) * feedforwardData->groups)) ||
      !(readback2Data->controlName = SDDS_Malloc(sizeof(*(readback2Data->controlName)) * feedforwardData->groups)) ||
      !(readback2Data->value = SDDS_Malloc(sizeof(*(readback2Data->value)) * feedforwardData->groups)) ||
      !(readback2Data->lastValue = SDDS_Malloc(sizeof(*(readback2Data->lastValue)) * feedforwardData->groups)) ||
      !(readback2Data->initialValue = SDDS_Malloc(sizeof(*(readback2Data->initialValue)) * feedforwardData->groups)) ||
      !(readback2Data->referenceValue = SDDS_Malloc(sizeof(*(readback2Data->referenceValue)) * feedforwardData->groups)) ||
      !(readback2Data->flag = SDDS_Malloc(sizeof(*(readback2Data->flag)) * feedforwardData->groups)) ||
      !(readback2Data->cid = SDDS_Calloc(1, sizeof(*(readback2Data->cid)) * feedforwardData->groups)) ||
      !(actuatorData->value = SDDS_Malloc(sizeof(*(actuatorData->value)) * nActuators)) ||
      !(actuatorData->lastValue = SDDS_Malloc(sizeof(*(actuatorData->lastValue)) * nActuators)) ||
      !(actuatorData->initialValue = SDDS_Malloc(sizeof(*(actuatorData->initialValue)) * nActuators)) ||
      !(actuatorData->referenceValue = SDDS_Malloc(sizeof(*(actuatorData->referenceValue)) * nActuators)) ||
      !(actuatorData->computeValue = SDDS_Malloc(sizeof(*(actuatorData->computeValue)) * nActuators)) ||
      !(actuatorData->updated = SDDS_Malloc(sizeof(*(actuatorData->updated)) * nActuators)) ||
      !(actuatorData->flag = SDDS_Malloc(sizeof(*(actuatorData->flag)) * nActuators)) ||
      !(actuatorData->cid = SDDS_Calloc(1, sizeof(*(actuatorData->cid)) * nActuators)))
    SDDS_Bomb("Memory allocation failure9");

  for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++) {
    ffGroup = feedforwardData->group + iGroup;
    if (!SDDS_CopyString(&(readback1Data->controlName[iGroup]), ffGroup->readback1))
      SDDS_Bomb("Memory allocation failure10");
    if (!SDDS_CopyString(&(readback2Data->controlName[iGroup]), ffGroup->readback2))
      SDDS_Bomb("Memory allocation failure10");
    if (!iGroup ||
        (match_string(readback1Data->controlName[iGroup], readback1Data->controlName, iGroup, EXACT_MATCH) == -1 &&
         match_string(readback2Data->controlName[iGroup], readback2Data->controlName, iGroup, EXACT_MATCH) == -1))
      readback1Data->flag[iGroup] = readback2Data->flag[iGroup] = 1;
    else
      readback1Data->flag[iGroup] = readback2Data->flag[iGroup] = 0;
  }
}

/* Returns the number readback values have changed by more than their 
 * threshold amount. 
 */
long checkActionThreshold(CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, FEEDFORWARD_DATA *feedforwardData, LOOP_PARAM *loopParam) {
  long iGroup, thresholdsExceeded = 0;
  FEEDFORWARD_GROUP *ffGroup;

  /* If no thresholds are given, it is as if the thresholds are all zero. */
  if (!(feedforwardData->flags & READBACK_CHANGE_THRESHOLD)) {
    for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++)
      feedforwardData->group[iGroup].thresholdExceeded = 1;
    return feedforwardData->groups;
  }

  for (iGroup = 0; iGroup < feedforwardData->groups; iGroup++) {
    ffGroup = feedforwardData->group + iGroup;
    ffGroup->thresholdExceeded = 0;
    if (ffGroup->readbackChangeThreshold <= 0 ||
        fabs(readback1Data->value[ffGroup->readbackIndex] - readback1Data->referenceValue[ffGroup->readbackIndex]) > ffGroup->readbackChangeThreshold ||
        fabs(readback2Data->value[ffGroup->readbackIndex] - readback2Data->referenceValue[ffGroup->readbackIndex]) > ffGroup->readbackChangeThreshold)
      {
      thresholdsExceeded++;
      ffGroup->thresholdExceeded = 1;
      readback1Data->referenceValue[ffGroup->readbackIndex] = readback1Data->value[ffGroup->readbackIndex];
      readback2Data->referenceValue[ffGroup->readbackIndex] = readback2Data->value[ffGroup->readbackIndex];
    }
  }

  return thresholdsExceeded;
}

/* Returns the number of test parameters are outside the allowed windows. */
long sdds2dfeedforward_checkOutOfRange(TESTS *test, BACKOFF *backoffParam, LOOP_PARAM *loopParam, double timeOfDay, double pendIOtime) {
  long i, nOutOfRange, exit_prog = 0;

  if (!test->file)
    return 0;
  if (loopParam->verbose) {
    fprintf(FPINFO, "Reading test devices at time %f\n", timeOfDay);
    fflush(FPINFO);
  }
  if (ReadScalarValues(test->controlName, NULL, NULL, test->value, test->n, test->cid, pendIOtime)) {
    fprintf(FPINFO, "Error reading test values---assuming test has failed.\n");
    fflush(FPINFO);
    return test->n;
  }
  for (i = nOutOfRange = 0; i < test->n; i++) {
    if ((test->outOfRange[i] =
           test->value[i] < test->min[i] || test->value[i] > test->max[i]) != 0) {
      if (test->exitOnFailure[i])
        exit_prog = 1;
      nOutOfRange++;
    }
  }
  if (nOutOfRange && loopParam->verbose) {
    fprintf(FPINFO, "%ld tests out of range:\n", nOutOfRange);
    fflush(FPINFO);
    for (i = 0; i < test->n; i++)
      if (test->outOfRange[i]) {
        fprintf(FPINFO, "\t%s\t%f\n", test->controlName[i], test->value[i]);
        fflush(FPINFO);
      }
  }
  if (exit_prog)
    SDDS_Bomb("Exit on tests failure.");
  return nOutOfRange;
}

long ComputeNewActuatorValues(FEEDFORWARD_DATA *feedforwardData, CONTROL_DATA *readback1Data, CONTROL_DATA *readback2Data, ENABLE_DATA *enablePVData,
                              CONTROL_DATA *actuatorData, LOOP_PARAM *loopParam,
                              long offsetMode, /* 0=no offset, -1=return values for use in setting offsets, 1=apply offsets */
                              double *offsetValue, long *settingOutOfRange) {
  long iReadback, iActuator, actuatorIndex = 0, code, codeAccum, enablePVIndex;
  FEEDFORWARD_GROUP *ffGroup;
  double readback1Value, readback2Value;
  /* double *actuatorValue; */
  double maxFracChange, fracChange;
  long changed = 0;

  *settingOutOfRange = 0;
  /* actuatorValue = actuatorData->value; */
  for (iActuator = 0; iActuator < actuatorData->controlNames; iActuator++) {
    actuatorData->computeValue[iActuator] = 0;
    actuatorData->updated[iActuator] = 0;
    /*reset the flag here; so that this flag will not be overwritten by other readback groups; one actuator is allowed to have multiple tables */
    actuatorData->flag[iActuator] = 0;
    actuatorData->lastValue[iActuator] = actuatorData->value[iActuator];
  }
  for (iReadback = 0; iReadback < feedforwardData->groups; iReadback++) {
    ffGroup = feedforwardData->group + iReadback;
    readback1Value = readback1Data->value[ffGroup->readbackIndex];
    readback2Value = readback2Data->value[ffGroup->readbackIndex];
    if (loopParam->advance > 0) {
      readback1Value += (readback1Data->value[ffGroup->readbackIndex] -
                        readback1Data->lastValue[ffGroup->readbackIndex]) /
                       loopParam->interval * loopParam->advance;
      readback2Value += (readback2Data->value[ffGroup->readbackIndex] -
                        readback2Data->lastValue[ffGroup->readbackIndex]) /
                       loopParam->interval * loopParam->advance;
      if (loopParam->verbose) {
        fprintf(FPINFO, "Readback value (%s, %s) is advanced from (%f, %f) to (%f, %f).\n",
                ffGroup->readback1, ffGroup->readback2,
                readback1Data->value[ffGroup->readbackIndex], readback2Data->value[ffGroup->readbackIndex],
                readback1Value, readback2Value);
        fflush(FPINFO);
      }
    }
    for (iActuator = codeAccum = 0; iActuator < ffGroup->actuators; iActuator++) {
      actuatorIndex = ffGroup->actuatorIndex[iActuator];
      /* actuatorData->flag[actuatorIndex] = 0; */
      enablePVIndex = ffGroup->enablePVIndex[iActuator];
      if (ffGroup->enablePVIndex[iActuator] != -1 &&
          ((enablePVData->outOfTest && enablePVData->outOfTest[enablePVIndex]) ||
           enablePVData->outOfLimit[enablePVIndex] ||
           (enablePVData->outOfLimit2 && enablePVData->outOfLimit2[enablePVIndex]))) {
        if (enablePVData->outOfTest && enablePVData->outOfTest[enablePVIndex]) {
          /*ignore due to test failure, do nothing */
          continue;
        } else if (enablePVData->outOfLimit[enablePVIndex] ||
                   enablePVData->outOfLimit2[enablePVIndex]) {
          /* actuatorData->lastValue[actuatorIndex] = actuatorData->value[actuatorIndex]; */
          actuatorData->value[actuatorIndex] =
            enablePVData->disabledOutputValue[enablePVIndex];
          actuatorData->flag[actuatorIndex] = ffGroup->thresholdExceeded;
          if (enablePVData->outOfLimit[ffGroup->enablePVIndex[iActuator]] || enablePVData->outOfLimit2[ffGroup->enablePVIndex[iActuator]])
            ffGroup->ffValue[iActuator] = actuatorData->value[iActuator];
          if (loopParam->verbose) {
            if (enablePVData->outOfLimit[ffGroup->enablePVIndex[iActuator]]) {
              fprintf(FPINFO, "Actuator %s set to %e due to state of enable PV %s (limits [%e, %e])\n",
                      ffGroup->actuator[iActuator], actuatorData->value[iActuator],
                      enablePVData->controlName[enablePVIndex],
                      enablePVData->lowerLimit[enablePVIndex],
                      enablePVData->upperLimit[enablePVIndex]);
              fflush(FPINFO);
            }
            if (enablePVData->outOfLimit2[ffGroup->enablePVIndex[iActuator]]) {
              fprintf(FPINFO, "Actuator %s set to %e due to state of enable2 PV %s (limits [%e, %e])\n",
                      ffGroup->actuator[iActuator], actuatorData->value[actuatorIndex],
                      enablePVData->controlName2[enablePVIndex],
                      enablePVData->lowerLimit2[enablePVIndex],
                      enablePVData->upperLimit2[enablePVIndex]);
              fflush(FPINFO);
            }
          }
        }
      } else {
        /* Set flags for actuators according to whether the readback change
               * threshold (if given) was exceeded. */
        actuatorData->flag[actuatorIndex] = ffGroup->thresholdExceeded;
        ffGroup->ffValue[iActuator] = ffInterpolate(ffGroup->actuatorData[iActuator],
                                                    ffGroup->readback1Data[iActuator], ffGroup->n1Data[iActuator],
                                                    ffGroup->readback2Data[iActuator], ffGroup->n2Data[iActuator],
                                                    readback1Value, readback2Value, 1, &code);
        actuatorData->computeValue[actuatorIndex] += ffGroup->ffValue[iActuator];
        actuatorData->updated[actuatorIndex] = 1;
        codeAccum += code * actuatorData->flag[actuatorIndex];
      }
    }
    if (codeAccum && loopParam->verbose) {
      fprintf(FPINFO,
              "Warning: readback (%s, %s) is outside the range of the feedforward data for one or more actuators\n",
              ffGroup->readback1, ffGroup->readback2);
      fflush(FPINFO);
    }
  }
  /*update acutator value */
  for (iActuator = 0; iActuator < actuatorData->controlNames; iActuator++) {
    if (actuatorData->updated[iActuator]) {
      actuatorData->value[iActuator] = actuatorData->computeValue[iActuator];
    }
    if (offsetMode == 1 && offsetValue)
      actuatorData->value[iActuator] -= offsetValue[iActuator];
  }
  for (iReadback = 0; iReadback < feedforwardData->groups; iReadback++) {
    ffGroup = feedforwardData->group + iReadback;
    if (feedforwardData->flags & ACTUATOR_CHANGE_LIMIT) {
      maxFracChange = 0;
      for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
        if (ffGroup->enablePVIndex[iActuator] != -1 &&
            (enablePVData->outOfLimit[ffGroup->enablePVIndex[iActuator]] ||
             (enablePVData->outOfTest && enablePVData->outOfTest[ffGroup->enablePVIndex[iActuator]])))
          continue;
        if (ffGroup->actuatorDeltaLimit[iActuator] > 0 &&
            (fracChange = fabs(actuatorData->value[actuatorIndex] -
                               actuatorData->lastValue[actuatorIndex]) /
                          ffGroup->actuatorDeltaLimit[iActuator]) > maxFracChange) {
          maxFracChange = fracChange;
        }
      }
      if (loopParam->verbose) {
        fprintf(FPINFO, "Maximum frac change for actuators linked to (%s, %s) is %e\n",
                ffGroup->readback1, ffGroup->readback2, maxFracChange);
        fflush(FPINFO);
      }
      if (maxFracChange > 1) {
        for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
          if (ffGroup->enablePVIndex[iActuator] != -1 &&
              (enablePVData->outOfLimit[ffGroup->enablePVIndex[iActuator]] ||
               (enablePVData->outOfTest && enablePVData->outOfTest[ffGroup->enablePVIndex[iActuator]])))
            continue;
          actuatorIndex = ffGroup->actuatorIndex[iActuator];
          actuatorData->value[actuatorIndex] =
            actuatorData->lastValue[actuatorIndex] +
            (actuatorData->value[actuatorIndex] - actuatorData->lastValue[actuatorIndex]) / maxFracChange;
        }
      }
    }
    if (loopParam->verbose && ffGroup->thresholdExceeded) {
      fprintf(FPINFO, "New computed values for actuators driven by (%s, %s)\n",
              ffGroup->readback1, ffGroup->readback2);
      fflush(FPINFO);
    }
    for (iActuator = 0; iActuator < ffGroup->actuators; iActuator++) {
      enablePVIndex = ffGroup->enablePVIndex[iActuator];
      actuatorIndex = ffGroup->actuatorIndex[iActuator];
      if (enablePVIndex != -1 && enablePVData->outOfTest && enablePVData->outOfTest[enablePVIndex]) {
        if (loopParam->verbose) {
          fprintf(FPINFO, "%s is skipped because of test pv %s - %f out of range (%f - %f)\n",
                  ffGroup->actuator[iActuator],
                  enablePVData->test_pvName[enablePVIndex],
                  enablePVData->test_value[enablePVIndex],
                  enablePVData->test_lowerLimit[enablePVIndex],
                  enablePVData->test_upperLimit[enablePVIndex]);
          fflush(FPINFO);
        }
        continue;
      }
      if ((feedforwardData->flags & ACTUATOR_LOWER_LIMIT && actuatorData->value[actuatorIndex] < ffGroup->actuatorLowerLimit[iActuator]) ||
          (feedforwardData->flags & ACTUATOR_UPPER_LIMIT && actuatorData->value[actuatorIndex] > ffGroup->actuatorUpperLimit[iActuator])) {
        *settingOutOfRange = 1;
        fprintf(FPINFO, "Actuator %s setting value %f is out of range. [",
                ffGroup->actuator[iActuator], actuatorData->value[actuatorIndex]);
        if (feedforwardData->flags & ACTUATOR_LOWER_LIMIT)
          fprintf(FPINFO, "Low: %f, ", ffGroup->actuatorLowerLimit[iActuator]);
        if (feedforwardData->flags & ACTUATOR_UPPER_LIMIT)
          fprintf(FPINFO, "High: %f", ffGroup->actuatorUpperLimit[iActuator]);
        fprintf(FPINFO, "]\n");
        fflush(FPINFO);
      }
      if (actuatorData->value[actuatorIndex] != actuatorData->lastValue[actuatorIndex])
        changed++;
      if (loopParam->verbose && ffGroup->thresholdExceeded) {
        fprintf(FPINFO, "%s %e (was %e)\n",
                ffGroup->actuator[iActuator],
                ffGroup->ffValue[iActuator],
                actuatorData->lastValue[actuatorIndex]);
        fflush(FPINFO);
      }
    }
  }
  return changed;
}

double ffInterpolate(double *f, double *x1, long n1, double *x2, long n2, double x1o, double x2o, 
                     long order, long *returnCode) {
  long hi, lo, mid, offset, code, i2, i1, i;
  double result0;
  double *indep = NULL, *result = NULL;
  long nResult = 0;

  /*
  fprintf(stderr, "Interpolating %ld x %ld grid of data at %le, %le\n", n1, n2, x1o, x2o);
  fprintf(stderr, "endpoints of grid: (%le, %le)  (%le, %le)\n", x1[0], x2[0], x1[n1*n2-1], x2[n1*n2-1]);
  */
  
  nResult = order+1;
  result = malloc(sizeof(*result)*nResult);
  indep = malloc(sizeof(*indep)*nResult);

  *returnCode = 0;

  /* find the index in the second dimension (varies fastest) */
  lo = 0;
  hi = n2 - 1;
  if (x2o > x2[hi]) {
    *returnCode = 1;
    return f[n2 - 1];
  }
  if (x2o < x2[lo]) {
    *returnCode = 1;
    return f[0];
  }
  if (lo == hi) {
    *returnCode = 1;
    return f[0];
  }
  /* do binary search for closest point */
  while ((hi - lo) > 1) {
    mid = (lo + hi) / 2;
    if (x2o < x2[mid])
      hi = mid;
    else
      lo = mid;
  }
  /* center the argument in the set of points for higher-order interpolation */
  if (order > n2 - 1)
    order = n2 - 1;
  offset = lo - (order - 1) / 2; 
  offset = MAX(offset, 0);
  offset = MIN(offset, n2 - order - 1);
  i2 = offset;
  /*
  fprintf(stderr, "i2 = %ld, bracketed by %le, %le\n", i2, x2[i2], x2[i2+1]);
  */
  
  /* Find index in first dimension */
  lo = 0;
  hi = n1 - 1;
  if (lo == hi) {
    *returnCode = 1;
    return f[0];
  }
  if (x1o > x1[hi*n2]) {
    *returnCode = 1;
    return f[hi*n2 - 1];
  }
  if (x1o < x1[lo*n2]) {
    *returnCode = 1;
    return f[0];
  }
  /* do binary search for closest point */
  while ((hi - lo) > 1) {
    mid = (lo + hi) / 2;
    if (x1o < x1[mid*n2])
      hi = mid;
    else
      lo = mid;
  }
  /* center the argument in the set of points for higher-order interpolation */
  if (order > n1 - 1)
    order = n1 - 1;
  offset = lo - (order - 1) / 2; 
  offset = MAX(offset, 0);
  offset = MIN(offset, n1 - order - 1);
  i1 = offset;
  /*
  fprintf(stderr, "i1 = %ld, bracketed by %le, %le\n", i1, x1[i1*n2], x1[(i1+1)*n2]);
  */
  
  /* interpolate vs second coordinate for each value of first coordinate */
  for (i=i1; i<(i1+order+1); i++) {
    result[i-i1] = LagrangeInterp(x2 + i*n2 + i2, f + i*n2 + i2, order + 1, x2o, &code);
    indep[i-i1] = x1[i*n2];
    if (!code)
      SDDS_Bomb("zero denominator in LagrangeInterp (1)");
    /*
    fprintf(stderr, "Interpolated value of %le for x1 = %le\n", result[i-i1], indep[i-i1]);
    */
  }
  result0 = LagrangeInterp(indep, result, order + 1, x1o, &code);
  if (!code)
    SDDS_Bomb("zero denominator in LagrangeInterp (2)");
  
  free(result);
  free(indep);

  return result0;
}

#if defined(vxWorks)
void sdds2dfeedforwardDeleteHook(WIND_TCB *pTcb)
#else
void sdds2dfeedforwardDeleteHook()
#endif
{
  SDDS2DFEEDFORWARD_GLOBAL *sg;
  int i, j, k;
  /* int offset; */
  int ca = 1;

#if defined(vxWorks)
  int tid;
  if (pTcb->entry == (FUNCPTR)sddsIocFeedforward) {
    tid = taskNameToId(pTcb->name);
    sg = (SDDS2DFEEDFORWARD_GLOBAL *)taskVarGet(tid, (int *)&sdds2dfeedforwardGlobal);
    taskDeleteHookDelete((FUNCPTR)sdds2dfeedforwardDeleteHook);
    if ((sg != NULL) && ((int)sg != ERROR)) {
#else
  sg = sdds2dfeedforwardGlobal;
#endif

#ifndef EPICS313
      if (ca_current_context() == NULL)
        ca = 0;
      if (ca)
        ca_attach_context(ca_current_context());
#endif

      if (ca) {
#ifdef USE_RUNCONTROL
        if ((sg->rcParam.PV) && (sg->rcParam.status != RUNCONTROL_DENIED)) {
#  ifdef USE_LOGDAEMON
          if (sg->useLogDaemon && sg->rcParam.PV) {
            logClose(sg->logHandle);
          }
#  endif
          strcpy(sg->rcParam.message, "Application exited.");
          runControlLogMessage(sg->rcParam.handle, sg->rcParam.message, MAJOR_ALARM, &(sg->rcParam.rcInfo));
          switch (runControlExit(sg->rcParam.handle, &(sg->rcParam.rcInfo))) {
          case RUNCONTROL_OK:
            break;
          case RUNCONTROL_ERROR:
            fprintf(FPINFO, "Error exiting run control.\n");
            fflush(FPINFO);
          default:
            fprintf(FPINFO, "Unrecognized error return code from runControlExit.\n");
            fflush(FPINFO);
          }
        }
#endif
        ca_task_exit();
      }

      if (sg->actuatorValueOffset) {
        free(sg->actuatorValueOffset);
        sg->actuatorValueOffset = NULL;
      }
      if (sg->feedforwardData.file) {
        free(sg->feedforwardData.file);
        sg->feedforwardData.file = NULL;
      }
      if (sg->feedforwardData.group) {
        /* offset = 0; */
        for (i = 0; i < sg->feedforwardData.groups; i++) {
          if (sg->feedforwardData.group[i].enablePVIndex) {
            free(sg->feedforwardData.group[i].enablePVIndex);
            sg->feedforwardData.group[i].enablePVIndex = NULL;
          }
          if (sg->feedforwardData.group[i].readback1) {
            free(sg->feedforwardData.group[i].readback1);
            sg->feedforwardData.group[i].readback1 = NULL;
          }
          if (sg->feedforwardData.group[i].readback2) {
            free(sg->feedforwardData.group[i].readback2);
            sg->feedforwardData.group[i].readback2 = NULL;
          }
          if (sg->feedforwardData.group[i].actuatorLowerLimit) {
            free(sg->feedforwardData.group[i].actuatorLowerLimit);
            sg->feedforwardData.group[i].actuatorLowerLimit = NULL;
          }
          if (sg->feedforwardData.group[i].actuatorUpperLimit) {
            free(sg->feedforwardData.group[i].actuatorUpperLimit);
            sg->feedforwardData.group[i].actuatorUpperLimit = NULL;
          }
          if (sg->readback1Data.controlName) {
            if (sg->readback1Data.controlName[i]) {
              free(sg->readback1Data.controlName[i]);
              sg->readback1Data.controlName[i] = NULL;
            }
          }
          if (sg->readback2Data.controlName) {
            if (sg->readback2Data.controlName[i]) {
              free(sg->readback2Data.controlName[i]);
              sg->readback2Data.controlName[i] = NULL;
            }
          }
          for (j = 0; j < sg->feedforwardData.group[i].actuators; j++) {
            if (sg->feedforwardData.group[i].actuator) {
              if (sg->feedforwardData.group[i].actuator[j]) {
                free(sg->feedforwardData.group[i].actuator[j]);
                sg->feedforwardData.group[i].actuator[j] = NULL;
              }
            }
            if (sg->feedforwardData.group[i].actuatorData) {
              if (sg->feedforwardData.group[i].actuatorData[j]) {
                free(sg->feedforwardData.group[i].actuatorData[j]);
                sg->feedforwardData.group[i].actuatorData[j] = NULL;
              }
            }
            if (sg->feedforwardData.group[i].readback1Data) {
              if (sg->feedforwardData.group[i].readback1Data[j]) {
                free(sg->feedforwardData.group[i].readback1Data[j]);
                sg->feedforwardData.group[i].readback1Data[j] = NULL;
              }
            }
            if (sg->feedforwardData.group[i].readback2Data) {
              if (sg->feedforwardData.group[i].readback2Data[j]) {
                free(sg->feedforwardData.group[i].readback2Data[j]);
                sg->feedforwardData.group[i].readback2Data[j] = NULL;
              }
            }
          }
          /* if (sg->feedforwardData.group[i].actuatorIndex) */
          /* offset = sg->feedforwardData.group[i].actuatorIndex[j-1]+1; */
          if (sg->feedforwardData.group[i].actuator) {
            free(sg->feedforwardData.group[i].actuator);
            sg->feedforwardData.group[i].actuator = NULL;
          }
          if (sg->feedforwardData.group[i].ffValue) {
            free(sg->feedforwardData.group[i].ffValue);
            sg->feedforwardData.group[i].ffValue = NULL;
          }
          if (sg->feedforwardData.group[i].actuatorData) {
            free(sg->feedforwardData.group[i].actuatorData);
            sg->feedforwardData.group[i].actuatorData = NULL;
          }
          if (sg->feedforwardData.group[i].readback1Data) {
            free(sg->feedforwardData.group[i].readback1Data);
            sg->feedforwardData.group[i].readback1Data = NULL;
          }
          if (sg->feedforwardData.group[i].readback2Data) {
            free(sg->feedforwardData.group[i].readback2Data);
            sg->feedforwardData.group[i].readback2Data = NULL;
          }
          if (sg->feedforwardData.group[i].actuatorDeltaLimit) {
            free(sg->feedforwardData.group[i].actuatorDeltaLimit);
            sg->feedforwardData.group[i].actuatorDeltaLimit = NULL;
          }
          if (sg->feedforwardData.group[i].n1Data) {
            free(sg->feedforwardData.group[i].n1Data);
            sg->feedforwardData.group[i].n1Data = NULL;
          }
          if (sg->feedforwardData.group[i].n2Data) {
            free(sg->feedforwardData.group[i].n2Data);
            sg->feedforwardData.group[i].n2Data = NULL;
          }
        }
        for (i = 0; i < sg->feedforwardData.groups; i++) {
          if (sg->feedforwardData.group[i].actuatorIndex) {
            free(sg->feedforwardData.group[i].actuatorIndex);
            sg->feedforwardData.group[i].actuatorIndex = NULL;
          }
        }
        free(sg->feedforwardData.group);
        sg->feedforwardData.group = NULL;
      }

      if (sg->enablePVData.controlNames) {
        for (k = 0; k < sg->enablePVData.controlNames; k++) {
          if (sg->enablePVData.controlName[k]) {
            free(sg->enablePVData.controlName[k]);
            sg->enablePVData.controlName[k] = NULL;
          }
        }
        free(sg->enablePVData.controlName);
        free(sg->enablePVData.value);
        free(sg->enablePVData.cid);
        free(sg->enablePVData.upperLimit);
        free(sg->enablePVData.lowerLimit);
        free(sg->enablePVData.disabledOutputValue);
        free(sg->enablePVData.outOfLimit);
        free(sg->enablePVData.flag);
        free(sg->enablePVData.outOfLimit2);
        if (sg->enablePVData.controlName2) {
          for (k = 0; k < sg->enablePVData.controlNames; k++) {
            if (sg->enablePVData.controlName2[k]) {
              free(sg->enablePVData.controlName2[k]);
              sg->enablePVData.controlName2[k] = NULL;
            }
          }
          free(sg->enablePVData.controlName2);
          free(sg->enablePVData.cid2);
          free(sg->enablePVData.value2);
          free(sg->enablePVData.upperLimit2);
          free(sg->enablePVData.lowerLimit2);
          free(sg->enablePVData.flag2);
          sg->enablePVData.flag2 = NULL;
          sg->enablePVData.controlName2 = NULL;
          sg->enablePVData.value2 = NULL;
          sg->enablePVData.cid2 = NULL;
          sg->enablePVData.upperLimit2 = sg->enablePVData.lowerLimit2 = NULL;
        }
        if (sg->enablePVData.test_pvName) {
          for (k = 0; k < sg->enablePVData.controlNames; k++) {
            if (sg->enablePVData.test_pvName[k]) {
              free(sg->enablePVData.test_pvName[k]);
              sg->enablePVData.test_pvName[k] = NULL;
            }
          }
          free(sg->enablePVData.test_pvName);
          free(sg->enablePVData.test_id);
          free(sg->enablePVData.test_value);
          free(sg->enablePVData.test_upperLimit);
          free(sg->enablePVData.test_lowerLimit);
          free(sg->enablePVData.test_flag);
          free(sg->enablePVData.outOfTest);
          sg->enablePVData.test_flag = NULL;
          sg->enablePVData.test_pvName = NULL;
          sg->enablePVData.test_value = NULL;
          sg->enablePVData.test_id = NULL;
          sg->enablePVData.outOfTest = NULL;
          sg->enablePVData.test_upperLimit = sg->enablePVData.test_lowerLimit = NULL;
        }
        sg->enablePVData.outOfLimit2 = NULL;
        sg->enablePVData.flag = NULL;
        sg->enablePVData.controlName = NULL;
        sg->enablePVData.value = NULL;
        sg->enablePVData.cid = NULL;
        sg->enablePVData.upperLimit = sg->enablePVData.lowerLimit =
          sg->enablePVData.disabledOutputValue = NULL;
        sg->enablePVData.outOfLimit = NULL;
      }
      if (sg->readback1Data.controlName) {
        free(sg->readback1Data.controlName);
        sg->readback1Data.controlName = NULL;
      }
      if (sg->readback2Data.controlName) {
        free(sg->readback2Data.controlName);
        sg->readback2Data.controlName = NULL;
      }
      if (sg->readback1Data.value) {
        free(sg->readback1Data.value);
        sg->readback1Data.value = NULL;
      }
      if (sg->readback2Data.value) {
        free(sg->readback2Data.value);
        sg->readback2Data.value = NULL;
      }
      if (sg->readback1Data.lastValue) {
        free(sg->readback1Data.lastValue);
        sg->readback1Data.lastValue = NULL;
      }
      if (sg->readback2Data.lastValue) {
        free(sg->readback2Data.lastValue);
        sg->readback2Data.lastValue = NULL;
      }
      if (sg->readback1Data.initialValue) {
        free(sg->readback1Data.initialValue);
        sg->readback1Data.initialValue = NULL;
      }
      if (sg->readback2Data.initialValue) {
        free(sg->readback2Data.initialValue);
        sg->readback2Data.initialValue = NULL;
      }
      if (sg->readback1Data.referenceValue) {
        free(sg->readback1Data.referenceValue);
        sg->readback1Data.referenceValue = NULL;
      }
      if (sg->readback2Data.referenceValue) {
        free(sg->readback2Data.referenceValue);
        sg->readback2Data.referenceValue = NULL;
      }
      if (sg->readback1Data.flag) {
        free(sg->readback1Data.flag);
        sg->readback1Data.flag = NULL;
      }
      if (sg->readback2Data.flag) {
        free(sg->readback2Data.flag);
        sg->readback2Data.flag = NULL;
      }
      if (sg->readback1Data.cid) {
        free(sg->readback1Data.cid);
        sg->readback1Data.cid = NULL;
      }
      if (sg->readback2Data.cid) {
        free(sg->readback2Data.cid);
        sg->readback2Data.cid = NULL;
      }
      if (sg->actuatorData.controlName) {
        for (k = 0; k < sg->actuatorData.controlNames; k++) {
          if (sg->actuatorData.controlName[k]) {
            free(sg->actuatorData.controlName[k]);
            sg->actuatorData.controlName[k] = NULL;
          }
        }
      }
      if (sg->actuatorData.controlName) {
        free(sg->actuatorData.controlName);
        sg->actuatorData.controlName = NULL;
      }
      if (sg->actuatorData.updated) {
        free(sg->actuatorData.updated);
        sg->actuatorData.updated = NULL;
      }
      if (sg->actuatorData.computeValue) {
        free(sg->actuatorData.computeValue);
        sg->actuatorData.computeValue = NULL;
      }
      if (sg->actuatorData.value) {
        free(sg->actuatorData.value);
        sg->actuatorData.value = NULL;
      }
      if (sg->actuatorData.lastValue) {
        free(sg->actuatorData.lastValue);
        sg->actuatorData.lastValue = NULL;
      }
      if (sg->actuatorData.initialValue) {
        free(sg->actuatorData.initialValue);
        sg->actuatorData.initialValue = NULL;
      }
      if (sg->actuatorData.referenceValue) {
        free(sg->actuatorData.referenceValue);
        sg->actuatorData.referenceValue = NULL;
      }
      if (sg->actuatorData.computeValue) {
        free(sg->actuatorData.computeValue);
        sg->actuatorData.computeValue = NULL;
      }
      if (sg->actuatorData.flag) {
        free(sg->actuatorData.flag);
        sg->actuatorData.flag = NULL;
      }
      if (sg->actuatorData.cid) {
        free(sg->actuatorData.cid);
        sg->actuatorData.cid = NULL;
      }
      if (sg->testConditions.file) {
        if (sg->testConditions.max) {
          free(sg->testConditions.max);
          sg->testConditions.max = NULL;
        }
        if (sg->testConditions.min) {
          free(sg->testConditions.min);
          sg->testConditions.min = NULL;
        }
        if (sg->testConditions.sleep) {
          free(sg->testConditions.sleep);
          sg->testConditions.sleep = NULL;
        }
        if (sg->testConditions.reset) {
          free(sg->testConditions.reset);
          sg->testConditions.reset = NULL;
        }
        if (sg->testConditions.controlName) {
          for (i = 0; i < sg->testConditions.n; i++) {
            if (sg->testConditions.controlName[i]) {
              free(sg->testConditions.controlName[i]);
              sg->testConditions.controlName[i] = NULL;
            }
          }
          free(sg->testConditions.controlName);
          sg->testConditions.controlName = NULL;
        }
        if (sg->testConditions.cid) {
          free(sg->testConditions.cid);
          sg->testConditions.cid = NULL;
        }
        if (sg->testConditions.value) {
          free(sg->testConditions.value);
          sg->testConditions.value = NULL;
        }
        if (sg->testConditions.outOfRange) {
          free(sg->testConditions.outOfRange);
          sg->testConditions.outOfRange = NULL;
        }
        free(sg->testConditions.file);
        sg->testConditions.file = NULL;
      }

#if defined(vxWorks)

      if (sg->argv) {
        for (i = 0; i < sg->argc; i++) {
          if ((sg->argv)[i]) {
            free((sg->argv)[i]);
          }
        }
        free(sg->argv);
      }
#endif

      free(sg);
      sg = NULL;

#if defined(vxWorks)
      taskVarDelete(tid, (int *)&sdds2dfeedforwardGlobal);
    }
  }
#endif
}

/* this function is for suspending runcontrol and log data when tests is out of range or
   actuator setting is out of range. */
/* message can be "Out-of-range test variables(s)" or "actuator setting out of range.*/
void operate_runcontrol(long prevValue, long currentValue, char *message, long caWriteError,
                        LOOP_PARAM *loopParam, BACKOFF *backoffParam) {
#ifdef USE_LOGDAEMON
  long prevOutOfRange=0;
#endif

  if (currentValue) {
#ifdef USE_RUNCONTROL
    if (sdds2dfeedforwardGlobal->rcParam.PV && !loopParam->dryRun) {
      /* This messages must be re-written at every iteration
             because the runcontrolMessage can be written by another process
             such as the tcl interface that runs sddscontrollaw */
      strcpy(sdds2dfeedforwardGlobal->rcParam.message, message);
      sdds2dfeedforwardGlobal->rcParam.status = runControlLogMessage(sdds2dfeedforwardGlobal->rcParam.handle,
                                                                   sdds2dfeedforwardGlobal->rcParam.message,
                                                                   MAJOR_ALARM,
                                                                   &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
      if (sdds2dfeedforwardGlobal->rcParam.status != RUNCONTROL_OK) {
        fprintf(FPINFO, "Unable to write status message and alarm severity\n");
        fflush(FPINFO);
        exit(1);
      }
#  ifdef USE_LOGDAEMON
      if (!prevOutOfRange && sdds2dfeedforwardGlobal->useLogDaemon && sdds2dfeedforwardGlobal->rcParam.PV) {
        switch (logArguments(sdds2dfeedforwardGlobal->logHandle, sdds2dfeedforwardGlobal->rcParam.PV, message, NULL)) {
        case LOG_OK:
          break;
        case LOG_ERROR:
        case LOG_TOOBIG:
          fprintf(FPINFO, "warning: something wrong in calling logArguments.\n");
          fflush(FPINFO);
          break;
        }
      }
#  endif /* USE_LOGDAEMON */
    }
#endif /* USE_RUNCONTROL */
  }    /* end of branch for test values out of range */
  else {
    if (prevValue && loopParam->verbose) {
      fprintf(FPINFO, "Iterations restarted.\n");
      fflush(FPINFO);
      /* reset backoff counter */
      backoffParam->counter = 0;
      backoffParam->level = 1;
    }
#ifdef USE_RUNCONTROL
    if (!loopParam->dryRun && sdds2dfeedforwardGlobal->rcParam.PV && !caWriteError) {
      /* These messages must be re-written at every iteration
           * because the runcontrolMessage can be written by another process
           * such as the tcl interface that runs sddscontrollaw */
      if (loopParam->step == 0)
        strcpy(sdds2dfeedforwardGlobal->rcParam.message, "Iterations started");
      else
        strcpy(sdds2dfeedforwardGlobal->rcParam.message, "Iterations re-started");
      sdds2dfeedforwardGlobal->rcParam.status = runControlLogMessage(sdds2dfeedforwardGlobal->rcParam.handle,
                                                                   sdds2dfeedforwardGlobal->rcParam.message,
                                                                   NO_ALARM,
                                                                   &(sdds2dfeedforwardGlobal->rcParam.rcInfo));
      if (sdds2dfeedforwardGlobal->rcParam.status != RUNCONTROL_OK) {
        fprintf(FPINFO, "Unable to write status message and alarm severity\n");
        fflush(FPINFO);
        exit(1);
      }
    }
#  ifdef USE_LOGDAEMON
    if (prevOutOfRange && sdds2dfeedforwardGlobal->useLogDaemon && sdds2dfeedforwardGlobal->rcParam.PV) {
      switch (logArguments(sdds2dfeedforwardGlobal->logHandle, sdds2dfeedforwardGlobal->rcParam.PV, "In-range", NULL)) {
      case LOG_OK:
        break;
      case LOG_ERROR:
      case LOG_TOOBIG:
        fprintf(FPINFO, "warning: something wrong in calling logArguments.\n");
        fflush(FPINFO);
        break;
      }
    }
#  endif /* USE_LOGDAEMON */
#endif   /* USE_RUNCONTROL */
  }      /* end of branch for no test values out of range */
} /* test evaluation */
