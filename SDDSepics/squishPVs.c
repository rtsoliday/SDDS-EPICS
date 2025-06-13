/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: squishPVs.c
 * purpose: do first turn correction and similar jobs
 *
 * M. Borland, 1995
 * Name by S. Milton.
 $Log: not supported by cvs2svn $
 Revision 1.10  2009/12/03 22:13:59  soliday
 Updated by moving the needed definitions from alarm.h and alarmString.h
 into this file directly. This was needed because of a problem with
 EPICS Base 3.14.11. Andrew Johnson said that the values for these
 definitions will not change in the future.

 Revision 1.9  2006/10/20 15:21:08  soliday
 Fixed problems seen by linux-x86_64.

 Revision 1.8  2006/04/04 21:44:18  soliday
 Xuesong Jiao modified the error messages if one or more PVs don't connect.
 It will now print out all the non-connecting PV names.

 Revision 1.7  2005/11/09 19:42:30  soliday
 Updated to remove compiler warnings on Linux

 Revision 1.6  2005/11/08 22:05:05  soliday
 Added support for 64 bit compilers.

 Revision 1.5  2004/09/10 14:37:57  soliday
 Changed the flag for oag_ca_pend_event to a volatile variable

 Revision 1.4  2004/07/22 22:05:19  soliday
 Improved signal support when using Epics/Base 3.14.6

 Revision 1.3  2004/07/19 17:39:39  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/16 21:24:40  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base
 3.14.x has an inactivity timer that was causing disconnects from PVs
 when the log interval time was too large.

 Revision 1.1  2003/08/27 19:51:29  soliday
 Moved into subdirectory

 Revision 1.24  2002/11/27 16:22:06  soliday
 Fixed issues on Windows when built without runcontrol

 Revision 1.23  2002/11/26 22:07:09  soliday
 ezca has been 100% removed from all SDDS Epics programs.

 Revision 1.22  2002/11/19 22:33:05  soliday
 Altered to work with the newer version of runcontrol.

 Revision 1.21  2002/08/14 20:00:38  soliday
 Added Open License

 Revision 1.20  2002/06/02 05:22:44  borland
 Fixed problem with maximize mode and threshold.
 Added minimum, maximum, and average criteria.

 Revision 1.19  2002/04/25 16:15:53  soliday
 Added support for WIN32

 Revision 1.18  2002/03/22 23:02:29  soliday
 Changed argument for free_scanargs

 Revision 1.17  2002/02/04 21:11:11  shang
 added -testValues option

 Revision 1.16  2001/05/03 19:53:49  soliday
 Standardized the Usage message.

 Revision 1.15  2000/10/16 21:48:05  soliday
 Removed tsDefs.h include statement.

 Revision 1.14  2000/04/20 16:00:03  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.13  2000/04/19 15:51:02  soliday
 Removed some solaris compiler warnings.

 Revision 1.12  2000/03/08 17:15:27  soliday
 Removed compiler warnings under Linux.

 Revision 1.11  1999/10/08 13:54:37  borland
 Fixed typo in variable name for run_control mode.

 Revision 1.10  1999/09/17 22:13:53  soliday
 This version now works with WIN32

 Revision 1.9  1998/12/16 21:53:40  borland
 Initializing starting step size to zero.

 Revision 1.8  1998/11/24 17:18:47  soliday
 Added RunControl and LogDaemon capabilities.

 Revision 1.7  1997/08/27 19:24:44  borland
 Added -maximize mode.  -average now accepts time-between-samples value.
 Input file may now have OffsetValue column to give fixed offsets for
 optimization.

 Revision 1.6  1997/04/24 16:34:33  borland
 Put remaining informational messages inside tests against `verbose` variable.

 Revision 1.5  1997/04/24 16:07:28  borland
 Fixed bug that popped up in printout when no offset PVs were given.

 * Revision 1.4  1996/02/14  05:08:04  borland
 * Switched from obsolete scan_item_list() routine to scanItemList(), which
 * offers better response to invalid/ambiguous qualifiers.
 *
 * Revision 1.3  1996/01/02  18:37:25  borland
 * Added support for readback offsets from secondary PVs.  Added features for
 * repeated optimization, action threshold for reoptimization, and stepsize
 * increases to speed optimization.
 *
 * Revision 1.2  1995/11/09  03:22:49  borland
 * Added copyright message to source files.
 *
 * Revision 1.1  1995/09/25  20:16:00  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <time.h>
#include <signal.h>
#include <sys/timeb.h>
/*#include <tsDefs.h>*/
#include <cadef.h>
#include <epicsVersion.h>
#include <alarm.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

#define CLO_COUNT 0
#define CLO_AVERAGES 1
#define CLO_EZCATIMING 2
#define CLO_STEPSIZE 3
#define CLO_VERBOSE 4
#define CLO_SETTLINGTIME 5
#define CLO_THRESHOLD 6
#define CLO_CRITERION 7
#define CLO_SUBDIVISIONS 8
#define CLO_REPEAT 9
#define CLO_ACTIONLEVEL 10
#define CLO_UPSTEP 11
#define CLO_MAXIMIZE 12
#define CLO_RUNCONTROLPV 13
#define CLO_RUNCONTROLDESC 14
#define CLO_TESTS 15
#define CLO_PENDIOTIME 16
#define COMMANDLINE_OPTIONS 17
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "count",
  "averages",
  "ezcatiming",
  "stepsize",
  "verbose",
  "settlingtime",
  "threshold",
  "criterion",
  "subdivisions",
  "repeat",
  "actionlevel",
  "upstepcount",
  "maximize",
  "runControlPV",
  "runControlDescription",
  "testValues",
  "pendiotime"};

static char *USAGE1 = "squishPVs <inputfile> \n\
    [-count=<pvName>,<lower>,<upper>,<number>] \n\
    [-averages=<number>,<seconds-between-readings>]\n\
    [-stepSize=<value>] \n\
    [-subdivisions=<number>,<factor>]\n\
    [-upstep=count=<number>,factor=<value>]\n\
    [-repeat={number=<integer>|forever}[,pause=<seconds>]]\n\
    [-pendIOtime=<seconds>]\n\
    [-settlingTime=<seconds>] [-verbose]\n\
    [-criterion={mav|rms|min|max|sum}]\n\
    [-threshold=<value>]\n\
    [-actionLevel=<value>]\n\
    [-maximize] [-testValues=<SDDSfile>[,limit=<number>]]\n\
    [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>\n\
    [-runControlDescription={string=<string>|parameter=<string>}]\n\n";
static char *USAGE2 = "-count               specify PV to read, validity limits, and number of\n\
                     valid readings required between reading PVs.\n\
-averages            number of PV readings to average and the interval between readings.\n\
-stepsize            current step to attempt.\n\
-subdivisions        specify number and size of interval\n\
                     subdivisions.\n\
-upstep              specifies the number of steps to take in one direction\n\
                     before increasing stepsize by the factor given.\n\
-repeat              Specifies repeat squishing (i.e., to run\n\
                     through the whole set of correctors many times).\n\
-settlingTime        settling time after corrector changes.\n\
-verbose             requests output during run.\n\
-threshold           how much the value has to improve to consider it real.\n\
-actionLevel         how large the value has to be before action is taken.\n\
-testValues=<file>,[limit=<number>] \n\
                     file is sdds format file containing minimum and maximum values\n\
                     of PV's specifying a range outside of which the feedback\n\
                     is temporarily suspended. Column names are ControlName,\n\
                     MinimumValue, MaximumValue. Optional column names are \n\
                     SleepTime, ResetTime. \n\
                     limits is the maixum number of failure times. The program will be \n\
                     terminated when the continuous failure times reaches the limit.\n\
                     The default failure times limit is 5.\n\
-criterion           specify mean-absolute-value or RMS reduction.\n\
-maximize            specifies maximizing the criterion, rather than default\n\
                     minimization.\n";
static char *USAGE3 = "-runControlPV        specifies a string parameter in <inputfile> whose value\n\
                     is a runControl PV name.\n\
-runControlDescription\n\
                     specifies a string parameter in <inputfile> whose value\n\
                     is a runControl PV description record.\n\n\
Program by M. Borland.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifndef USE_RUNCONTROL
static char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#else
static char *USAGE_WARNING = "";
#endif

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV={string=<string>|parameter=<string>}\n";
static char *runControlDescUsage = "-runControlDescription={string=<string>|parameter=<string>}\n";
#endif

#ifdef USE_LOGDAEMON
LOGHANDLE logHandle;
long useLogDaemon;
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

typedef struct
{
  char *corrector, **BPM, **offsetPV;
  long BPMs;
  double gain, *weight, lowerLimit, upperLimit, *offsetValue;
  chid PVchid, *bpmPVchid, *offsetPVchid;
} CORRECTION_GROUP;

typedef struct
{
  char *PVname;
  chid PVchid;
  double lower, upper;
  long number;
} COUNT_SPEC;

typedef struct
{
  char *file;
  long n, *outOfRange, count;
  int32_t limit;
  /*limits is the limited times of tests failure*/
  char **controlName;
  chid *controlPVchid;
  double *value, *min, *max, *sleep, *reset, longestSleep, longestReset;
} TESTS;

void waitForCount(chid PVchid, double lower, double upper, long count, long verbose, double pendIOtime);
long getBPMdata(double *rms, chid *bpmPVchid, chid *offsetPVchid, double *offsetValue,
                double *weight, long bpms, long criterion, long averages,
                double avePause, COUNT_SPEC *count, long verbose, TESTS *tests,
                double pendIOtime);
CORRECTION_GROUP *readGroups(long *groups, char *inputFile, long verbose);
void freeStringArray(char **ptr, long n);
int runControlPingWhileSleep(double sleepTime);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();
void setupTestsFile(TESTS *tests, long verbose);
long checkOutOfRange(TESTS *test, long verbose);
void RunTest(TESTS *test, long verbose, double pendIOtime);

#define CRITERION_MAV 0
#define CRITERION_RMS 1
#define CRITERION_MIN 2
#define CRITERION_MAX 3
#define CRITERION_AVE 4
#define CRITERIONOPTIONS 5
static char *criterionOption[CRITERIONOPTIONS] = {
  "mav",
  "rms",
  "minimum",
  "maximum",
  "average",
};

#define REP_NUMBER_GIVEN 0x001U
#define REP_FOREVER 0x002U
#define REP_PAUSE_GIVEN 0x004U

volatile int sigint = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  char *inputFile;

  long i_arg, verbose, improved, i;
  long retries;
  double timeout, pendIOtime = 10.0;
  double settlingTime, threshold;
  long iSubdivision;
  long averages, groups, iGroup, criterion, subdivisions;
  CORRECTION_GROUP *group;
  double startCurrent, lastStartCurrent, startingStepSize, stepSize, stepSize0, val0, val1, subdivisionFactor;
  long maximize;
  int32_t repeats, upstepCount;
  double repeatPause, avePause;
  unsigned long repeatFlags, dummyFlags;
  COUNT_SPEC countSpec;
  double actionLevel, upstepFactor;
  TESTS tests;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    fprintf(stderr, "%s", USAGE_WARNING);
    exit(1);
  }

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
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

  tests.file = NULL;
  tests.controlName = NULL;
  tests.outOfRange = NULL;
  tests.count = 0;
  tests.limit = INT32_MAX;
  tests.value = tests.min = tests.max = tests.sleep = tests.reset = NULL;
  startingStepSize = stepSize = stepSize0 = 0;
  inputFile = NULL;
  averages = 1;
  settlingTime = 0;
  verbose = 0;
  threshold = 0.1;
  criterion = CRITERION_MAV;
  subdivisions = 0;
  subdivisionFactor = 2;
  repeats = 1;
  repeatPause = 0.0;
  avePause = 0.5;
  countSpec.PVname = NULL;
  actionLevel = 0;
  upstepCount = 0;
  upstepFactor = 1.0;
  maximize = 0;
#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = rcParam.PVparam = rcParam.DescParam = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_COUNT:
        if (s_arg[i_arg].n_items != 5 ||
            sscanf(s_arg[i_arg].list[2], "%le", &countSpec.lower) != 1 ||
            sscanf(s_arg[i_arg].list[3], "%le", &countSpec.upper) != 1 ||
            countSpec.lower >= countSpec.upper ||
            sscanf(s_arg[i_arg].list[4], "%ld", &countSpec.number) != 1 ||
            countSpec.number <= 0)
          bomb("invalid -count syntax", NULL);
        countSpec.PVname = s_arg[i_arg].list[1];
        break;
      case CLO_AVERAGES:
        if (s_arg[i_arg].n_items != 3 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &averages) != 1 || averages < 1 ||
            sscanf(s_arg[i_arg].list[2], "%lf", &avePause) != 1 || avePause <= 0)
          bomb("no values or invalid values given for option -averages", NULL);
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("wrong number of items for -ezcaTiming");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || retries < 0)
          bomb("invalid -ezca values", NULL);
        pendIOtime = timeout * (retries + 1);
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_STEPSIZE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &startingStepSize) != 1 ||
            startingStepSize <= 0)
          bomb("invalid -stepsize syntax", NULL);
        break;
      case CLO_SETTLINGTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%le", &settlingTime) != 1 ||
            settlingTime < 0)
          bomb("no value or invalid value given for option -settlingTime", NULL);
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_THRESHOLD:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%le", &threshold) != 1 ||
            threshold < 0)
          bomb("no value or invalid value given for option -threshold", NULL);
        break;
      case CLO_CRITERION:
        if (s_arg[i_arg].n_items != 2 ||
            (criterion = match_string(s_arg[i_arg].list[1],
                                      criterionOption, CRITERIONOPTIONS, 0)) < 0)
          bomb("invalid -criterion syntax", NULL);
        break;
      case CLO_SUBDIVISIONS:
        if (s_arg[i_arg].n_items != 3 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &subdivisions) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%le", &subdivisionFactor) != 1 ||
            subdivisions <= 0 || subdivisionFactor <= 1)
          bomb("invalid -subdivision syntax/values", NULL);
        break;
      case CLO_REPEAT:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&repeatFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "number", SDDS_LONG, &repeats, 1, REP_NUMBER_GIVEN,
                          "forever", -1, NULL, 0, REP_FOREVER,
                          "pause", SDDS_DOUBLE, &repeatPause, 1, REP_PAUSE_GIVEN,
                          NULL) ||
            repeats <= 0 || repeatPause < 0)
          bomb("invalid -repeat syntax/values", NULL);
        if (!(repeatFlags & REP_NUMBER_GIVEN) && !(repeatFlags & REP_FOREVER))
          bomb("given either number of repetitions or 'forever' with -repeat", NULL);
        if (repeatFlags & REP_FOREVER)
          repeats = -1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_ACTIONLEVEL:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &actionLevel) != 1 ||
            actionLevel < 0)
          bomb("invalid -actionLevel syntax/values", NULL);
        break;
      case CLO_UPSTEP:
        upstepFactor = 2.0;
        upstepCount = 0;
        if (((s_arg[i_arg].n_items--) < 1) ||
            (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "count", SDDS_LONG, &upstepCount, 1, 0,
                           "factor", SDDS_DOUBLE, &upstepFactor, 1, 0,
                           NULL)) ||
            (upstepCount < 1) ||
            (upstepFactor <= 1.0))
          bomb("invalid -upstep syntax/values", NULL);
        s_arg[i_arg].n_items++;
        break;
      case CLO_MAXIMIZE:
        maximize = 1;
        break;
      case CLO_TESTS:
        SDDS_CopyString(&tests.file, s_arg[i_arg].list[1]);
        if (!strlen(tests.file))
          SDDS_Bomb("bad -testValues syntax, tests filename is not given.");
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "limit", SDDS_LONG, &tests.limit, 1, 0,
                          NULL)) {
          SDDS_Bomb("bad -testValues syntax.");
        }
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PVparam && !rcParam.PV) ||
            (rcParam.PVparam && rcParam.PV))
          bomb("bad -runControlPV syntax", runControlPVUsage);
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          exit(1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &rcParam.DescParam, 1, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.DescParam && !rcParam.Desc) ||
            (rcParam.DescParam && rcParam.Desc))
          bomb("bad -runControlDesc syntax", runControlDescUsage);
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }

  if (!inputFile)
    bomb("input filename not given", NULL);

  if (!(group = readGroups(&groups, inputFile, verbose)))
    bomb("unable to read correction groups from input file", NULL);
  /*get tests data*/
  setupTestsFile(&tests, verbose);

#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  useLogDaemon = 0;
  if (rcParam.PV) {
    switch (logOpen(&logHandle, NULL, "squishPVsAudit", "Instance Action")) {
    case LOG_OK:
      useLogDaemon = 1;
      break;
    case LOG_INVALID:
      fprintf(stderr, "warning: logDaemon rejected given sourceId and tagList. logDaemon messages not logged.\n");
      break;
    case LOG_ERROR:
      fprintf(stderr, "warning: unable to open connection to logDaemon. logDaemon messages not logged.\n");
      break;
    case LOG_TOOBIG:
      fprintf(stderr, "warning: one of the string supplied is too long. logDaemon messages not logged.\n");
      break;
    default:
      fprintf(stderr, "warning: unrecognized return code from logOpen.\n");
      break;
    }
  }
#  endif
#endif

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
  }
#endif
  atexit(rc_interrupt_handler);

  if (startingStepSize <= 0)
    bomb("starting step size not given", NULL);

  for (iGroup = 0; iGroup < groups; iGroup++) {
    if (ca_search(group[iGroup].corrector, &group[iGroup].PVchid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", group[iGroup].corrector);
      exit(1);
    }
    group[iGroup].bpmPVchid = (chid *)malloc(sizeof(chid) * group[iGroup].BPMs);
    if (group[iGroup].offsetPV)
      group[iGroup].offsetPVchid = (chid *)malloc(sizeof(chid) * group[iGroup].BPMs);
    else
      group[iGroup].offsetPVchid = NULL;
    for (i = 0; i < group[iGroup].BPMs; i++) {
      if (ca_search(group[iGroup].BPM[i], &group[iGroup].bpmPVchid[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", group[iGroup].BPM[i]);
        exit(1);
      }
      if (group[iGroup].offsetPV) {
        if (ca_search(group[iGroup].offsetPV[i], &group[iGroup].offsetPVchid[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", group[iGroup].offsetPV[i]);
          exit(1);
        }
      }
    }
  }
  if (countSpec.PVname) {
    if (ca_search(countSpec.PVname, &countSpec.PVchid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", countSpec.PVname);
      exit(1);
    }
  }
  if (tests.file) {
    tests.controlPVchid = (chid *)malloc(sizeof(chid) * tests.n);
    for (i = 0; i < tests.n; i++) {
      if (ca_search(tests.controlName[i], &tests.controlPVchid[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", tests.controlName[i]);
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < tests.n; i++) {
      if (ca_state(tests.controlPVchid[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", tests.controlName[i]);
    }
    exit(1);
  }
  while (repeats--) {
    for (iGroup = 0; iGroup < groups; iGroup++) {

      if (sigint) {
        return (1);
      }

#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        /* ping once at the beginning */
        if (runControlPingWhileSleep(0.1)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          exit(1);
        }
      }
#endif
      RunTest(&tests, verbose, pendIOtime);
      if (ca_get(DBR_DOUBLE, group[iGroup].PVchid, &startCurrent) != ECA_NORMAL) {
        fprintf(stderr, "CA error--unable to get corrector value\n");
        exit(1);
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "CA error--unable to get corrector value\n");
        exit(1);
      }
      if (verbose)
        fprintf(stderr, "** corrector %s at %f\n", group[iGroup].corrector, startCurrent);
      if (!getBPMdata(&val0, group[iGroup].bpmPVchid, group[iGroup].offsetPVchid,
                      group[iGroup].offsetValue, group[iGroup].weight,
                      group[iGroup].BPMs, criterion, averages, avePause,
                      &countSpec, verbose, &tests, pendIOtime))
        bomb("CA error--unable to get BPM values for computation", NULL);
      if (fabs(val0) < actionLevel) {
        if (verbose)
          fprintf(stderr, "Value %e below action level (%e)\n",
                  val0, actionLevel);
        continue;
      }

      stepSize = startingStepSize;
      for (iSubdivision = 0; iSubdivision <= subdivisions; iSubdivision++, stepSize /= subdivisionFactor) {
        improved = 0;
#ifdef USE_RUNCONTROL
        /* These messages must be re-written at every iteration
                 because the runcontrolMessage can be written by another process
                 such as the tcl interface that runs sddscontrollaw */
        if (rcParam.PV) {
          strcpy(rcParam.message, "Working");
          rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
          if (rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            exit(1);
          }
        }
#endif
        if (verbose)
          fprintf(stderr, "  Working on stepsize %e\n", stepSize);
        stepSize0 = stepSize;
        lastStartCurrent = startCurrent;
        do {
          RunTest(&tests, verbose, pendIOtime);
          startCurrent -= stepSize * group[iGroup].gain;
          if (group[iGroup].lowerLimit < group[iGroup].upperLimit) {
            if (startCurrent > group[iGroup].upperLimit) {
              fprintf(stderr, "  %s at upper limit\n", group[iGroup].corrector);
              startCurrent = group[iGroup].upperLimit;
            }
            if (startCurrent < group[iGroup].lowerLimit) {
              fprintf(stderr, "  %s at lower limit\n", group[iGroup].corrector);
              startCurrent = group[iGroup].lowerLimit;
            }
          }
          if (ca_put(DBR_DOUBLE, group[iGroup].PVchid, &startCurrent) != ECA_NORMAL) {
            fprintf(stderr, "CA error--unable to put corrector value\n");
            exit(1);
          }
          if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
            fprintf(stderr, "CA error--unable to get corrector value\n");
            exit(1);
          }
          if (verbose)
            fprintf(stderr, "  %s set to %f\n", group[iGroup].corrector, startCurrent);
          if (settlingTime) {
#ifdef USE_RUNCONTROL
            if (rcParam.PV) {
              runControlPingWhileSleep(settlingTime);
            } else {
              oag_ca_pend_event(settlingTime, &sigint);
            }
#else
            oag_ca_pend_event(settlingTime, &sigint);
#endif
            if (sigint)
              exit(1);
          }
          if (!getBPMdata(&val1, group[iGroup].bpmPVchid, group[iGroup].offsetPVchid,
                          group[iGroup].offsetValue, group[iGroup].weight,
                          group[iGroup].BPMs, criterion, averages, avePause,
                          &countSpec, verbose, &tests, pendIOtime))
            bomb("CA error--unable to get BPM values for computation", NULL);
          if ((!maximize && (val0 - val1) > threshold) ||
              (maximize && (val1 - val0) > threshold)) {
            if (verbose)
              fprintf(stderr, "  %s improved from %f to %f\n",
                      criterionOption[criterion], val0, val1);
            val0 = val1;
            improved++;
            if (improved > upstepCount && upstepFactor > 1) {
              stepSize *= upstepFactor;
              if (verbose)
                fprintf(stderr, " Stepsize increased to %e\n", stepSize);
              improved = 1;
            }
            lastStartCurrent = startCurrent;
          } else {
            if (verbose)
              fprintf(stderr, "  %s not improved (%f went to %f)\n",
                      criterionOption[criterion], val0, val1);
            if (lastStartCurrent != startCurrent) {
              startCurrent += stepSize * group[iGroup].gain;
              if (ca_put(DBR_DOUBLE, group[iGroup].PVchid, &startCurrent) != ECA_NORMAL) {
                fprintf(stderr, "CA error--unable to put corrector value\n");
                exit(1);
              }
              if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
                fprintf(stderr, "CA error--unable to get corrector value\n");
                exit(1);
              }
              if (settlingTime) {
#ifdef USE_RUNCONTROL
                if (rcParam.PV) {
                  runControlPingWhileSleep(settlingTime);
                } else {
                  oag_ca_pend_event(settlingTime, &sigint);
                }
#else
                oag_ca_pend_event(settlingTime, &sigint);
#endif
                if (sigint)
                  exit(1);
              }
            }
            improved = 0;
            break;
          }
        } while (1);

        if (improved)
          continue;

        if (stepSize != stepSize0) {
          /* put the step size back when changing directions */
          if (verbose)
            fprintf(stderr, "  Stepsize changed back to %e (upstepping removed)\n",
                    stepSize0);
          stepSize = stepSize0;
        }

        improved = 0;
        lastStartCurrent = startCurrent;
        do {
          RunTest(&tests, verbose, pendIOtime);
          startCurrent += stepSize * group[iGroup].gain;
          if (group[iGroup].lowerLimit < group[iGroup].upperLimit) {
            if (startCurrent > group[iGroup].upperLimit) {
              fprintf(stderr, "  %s at upper limit\n", group[iGroup].corrector);
              startCurrent = group[iGroup].upperLimit;
            }
            if (startCurrent < group[iGroup].lowerLimit) {
              fprintf(stderr, "  %s at lower limit\n", group[iGroup].corrector);
              startCurrent = group[iGroup].lowerLimit;
            }
          }
          if (ca_put(DBR_DOUBLE, group[iGroup].PVchid, &startCurrent) != ECA_NORMAL) {
            fprintf(stderr, "CA error--unable to put corrector value\n");
            exit(1);
          }
          if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
            fprintf(stderr, "CA error--unable to get corrector value\n");
            exit(1);
          }
          if (verbose)
            fprintf(stderr, "  %s set to %f\n", group[iGroup].corrector, startCurrent);
          if (settlingTime) {
#ifdef USE_RUNCONTROL
            if (rcParam.PV) {
              runControlPingWhileSleep(settlingTime);
            } else {
              oag_ca_pend_event(settlingTime, &sigint);
            }
#else
            oag_ca_pend_event(settlingTime, &sigint);
#endif
            if (sigint)
              exit(1);
          }
          if (!getBPMdata(&val1, group[iGroup].bpmPVchid, group[iGroup].offsetPVchid,
                          group[iGroup].offsetValue, group[iGroup].weight,
                          group[iGroup].BPMs, criterion, averages, avePause,
                          &countSpec, verbose, &tests, pendIOtime))
            bomb("CA error--unable to get BPM values for computation", NULL);
          if ((!maximize && (val0 - val1) > threshold) ||
              (maximize && (val1 - val0) > threshold)) {
            if (verbose)
              fprintf(stderr, "  %s improved from %f to %f\n",
                      criterionOption[criterion], val0, val1);
            val0 = val1;
            improved++;
            if (improved > upstepCount && upstepFactor > 1) {
              stepSize *= upstepFactor;
              if (verbose)
                fprintf(stderr, " Stepsize increased to %e\n", stepSize);
              improved = 1;
            }
            lastStartCurrent = startCurrent;
          } else {
            if (verbose)
              fprintf(stderr, "  %s not improved (%f went to %f)\n",
                      criterionOption[criterion], val0, val1);
            if (lastStartCurrent != startCurrent) {
              startCurrent -= stepSize * group[iGroup].gain;
              if (ca_put(DBR_DOUBLE, group[iGroup].PVchid, &startCurrent) != ECA_NORMAL) {
                fprintf(stderr, "CA error--unable to put corrector value\n");
                exit(1);
              }
              if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
                fprintf(stderr, "CA error--unable to get corrector value\n");
                exit(1);
              }
              if (settlingTime) {
#ifdef USE_RUNCONTROL
                if (rcParam.PV) {
                  runControlPingWhileSleep(settlingTime);
                } else {
                  oag_ca_pend_event(settlingTime, &sigint);
                }
#else
                oag_ca_pend_event(settlingTime, &sigint);
#endif
                if (sigint)
                  exit(1);
              }
            }
            improved = 0;
            break;
          }
        } while (1);

        if (improved)
          continue;

        if (stepSize != stepSize0) {
          /* put the step size back when changing directions */
          if (verbose)
            fprintf(stderr, "  Stepsize changed back to %e (upstepping removed)\n",
                    stepSize0);
          stepSize = stepSize0;
        }
      }
    }
    if (repeats) {
      if (repeatPause) {
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          runControlPingWhileSleep(repeatPause);
        } else {
          oag_ca_pend_event(repeatPause, &sigint);
        }
#else
        oag_ca_pend_event(repeatPause, &sigint);
#endif
        if (sigint)
          exit(1);
      }
      if (verbose) {
        if (repeats > 0)
          fprintf(stderr, "%" PRId32 " repetitions left\n", repeats);
        else
          fprintf(stderr, "starting new repetition\n");
      }
    }
  }
#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
#  ifdef USE_LOGDAEMON
    if (useLogDaemon) {
      logClose(logHandle);
    }
#  endif
    strcpy(rcParam.message, "Application completed normally.");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error exiting run control.\n");
      exit(1);
    default:
      fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
      exit(1);
    }
  }
#endif
  if (tests.file) {
    if (tests.outOfRange)
      free(tests.outOfRange);
    if (tests.controlName) {
      for (i_arg = 0; i_arg < tests.n; i_arg++)
        free(tests.controlName[i_arg]);
      free(tests.controlName);
    }
    if (tests.value)
      free(tests.value);
    if (tests.min)
      free(tests.min);
    if (tests.max)
      free(tests.max);
    if (tests.sleep)
      free(tests.sleep);
    if (tests.reset)
      free(tests.reset);
    free(tests.file);
  }
  free_scanargs(&s_arg, argc);
  return 0;
}

void freeStringArray(char **ptr, long n) {
  long i;
  if (!ptr)
    return;
  for (i = 0; i < n; i++)
    if (ptr[i])
      free(ptr[i]);
  free(ptr);
}

CORRECTION_GROUP *readGroups(long *groups, char *inputFile, long verbose) {
  SDDS_TABLE inTable;
  CORRECTION_GROUP *group;
  long i, offsetPVPresent, weightPresent, limitsPresent, offsetPresent;

  if (!SDDS_InitializeInput(&inTable, inputFile))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  *groups = 0;
  group = NULL;
#ifdef USE_RUNCONTROL
  if (rcParam.PVparam) {
    if (!SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING,
                            rcParam.PVparam, NULL))
      bomb("run control parameter not in input file", NULL);
  }
  if (rcParam.DescParam) {
    if (!SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING,
                            rcParam.DescParam, NULL))
      bomb("run control description parameter not in input file", NULL);
  }
#endif
  if (!SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_DOUBLE,
                          "Gain", NULL))
    bomb("double parameter Gain not in input file", NULL);
  if (!SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "CorrectorPV", NULL))
    bomb("string parameter CorrectorPV not in input file", NULL);
  if (!SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING,
                       "BpmPV", NULL))
    bomb("string column BpmPV not in input file", NULL);
  limitsPresent = 1;
  if (!SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_DOUBLE,
                          "UpperLimit", NULL) ||
      !SDDS_FindParameter(&inTable, FIND_SPECIFIED_TYPE, SDDS_DOUBLE,
                          "LowerLimit", NULL))
    limitsPresent = 0;

  offsetPVPresent = 1;
  if (!SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING,
                       "OffsetPV", NULL)) {
    offsetPVPresent = 0;
  }

  offsetPresent = 1;
  if (!SDDS_FindColumn(&inTable, FIND_NUMERIC_TYPE, "OffsetValue", NULL)) {
    offsetPresent = 0;
  }

  weightPresent = 1;
  if (!SDDS_FindColumn(&inTable, FIND_NUMERIC_TYPE, "Weight", NULL)) {
    weightPresent = 0;
  }

  while (SDDS_ReadTable(&inTable) >= 1) {
    group = SDDS_Realloc(group, sizeof(*group) * (*groups += 1));
    if ((group[*groups - 1].BPMs = SDDS_CountRowsOfInterest(&inTable)) <= 0)
      continue;
#ifdef USE_RUNCONTROL
    if (rcParam.PVparam) {
      if (!(rcParam.PV = *(char **)SDDS_GetParameter(&inTable, rcParam.PVparam, NULL))) {
        bomb("unable to get runcontrol pv name", NULL);
      }
    }
    if (rcParam.DescParam) {
      if (!(rcParam.Desc = *(char **)SDDS_GetParameter(&inTable, rcParam.DescParam, NULL))) {
        bomb("unable to get runcontrol description", NULL);
      }
    }
#endif
    if (!(group[*groups - 1].corrector = *(char **)SDDS_GetParameter(&inTable, "CorrectorPV", NULL)))
      bomb("unable to get value of corrector name from input file", NULL);
    if (!SDDS_GetParameter(&inTable, "Gain", &group[*groups - 1].gain))
      bomb("unable to get value of corrector gain from input file", NULL);
    group[*groups - 1].lowerLimit = group[*groups - 1].upperLimit = 0;
    if (limitsPresent &&
        (!SDDS_GetParameter(&inTable, "LowerLimit", &group[*groups - 1].lowerLimit) ||
         !SDDS_GetParameter(&inTable, "UpperLimit", &group[*groups - 1].upperLimit)))
      SDDS_Bomb("Unable to get lower/upper limits from file");
    if (!(group[*groups - 1].BPM = SDDS_GetColumn(&inTable, "BpmPV")))
      bomb("unable to get column of BPM names", NULL);
    if (offsetPVPresent) {
      if (!(group[*groups - 1].offsetPV = SDDS_GetColumn(&inTable, "OffsetPV")))
        bomb("unable to get column of offset PV names", NULL);
    } else
      group[*groups - 1].offsetPV = NULL;
    if (offsetPresent) {
      if (!(group[*groups - 1].offsetValue = SDDS_GetColumnInDoubles(&inTable, "OffsetValue")))
        bomb("unable to get column of offset values", NULL);
    } else
      group[*groups - 1].offsetValue = NULL;
    if (weightPresent) {
      if (!(group[*groups - 1].weight = SDDS_GetColumnInDoubles(&inTable, "Weight")))
        bomb("unable to get column of weight values", NULL);
    } else {
      if (!(group[*groups - 1].weight = (double *)malloc(sizeof(*group[*groups - 1].weight) * group[*groups - 1].BPMs)))
        bomb("memory allocation failure", NULL);
      for (i = 0; i < group[*groups - 1].BPMs; i++)
        group[*groups - 1].weight[i] = 1;
    }

    if (verbose) {
      fprintf(stderr, "Will use corrector %s for the following BPMs:\n",
              group[*groups - 1].corrector);
      for (i = 0; i < group[*groups - 1].BPMs; i++)
        fprintf(stderr, "(%s-%s)%c", group[*groups - 1].BPM[i],
                group[*groups - 1].offsetPV ? group[*groups - 1].offsetPV[i] : "0",
                (i + 1) % 10 == 0 ? '\n' : ' ');
      fputc('\n', stderr);
    }
  }
  /*checkRCParam( &inTable, inputFile);*/
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return group;
}

long getBPMdata(double *value, chid *bpmPVchid, chid *offsetPVchid, double *offsetValue,
                double *weight,
                long bpms, long criterion, long averages, double avePause,
                COUNT_SPEC *count, long verbose, TESTS *tests, double pendIOtime) {
  long ibpm, i;
  static double *bpmBuffer = NULL, *offsetBuffer = NULL;
  static double *aveValue = NULL;
  static long bufferSize = 0;

  if (!bpms)
    return 0;

  if (bufferSize < bpms) {
    bufferSize = 2 * bpms;
    bpmBuffer = SDDS_Realloc(bpmBuffer, sizeof(*bpmBuffer) * bufferSize);
    offsetBuffer = SDDS_Realloc(offsetBuffer, sizeof(*offsetBuffer) * bufferSize);
    aveValue = SDDS_Realloc(aveValue, sizeof(*aveValue) * bufferSize);
  }

  for (ibpm = 0; ibpm < bpms; ibpm++)
    aveValue[ibpm] = 0;
  for (i = 0; i < averages; i++) {
    RunTest(tests, verbose, pendIOtime);
    if (count && count->PVname)
      waitForCount(count->PVchid, count->lower, count->upper, count->number, verbose, pendIOtime);
    for (ibpm = 0; ibpm < bpms; ibpm++) {
      if (ca_get(DBR_DOUBLE, bpmPVchid[ibpm], bpmBuffer + ibpm) != ECA_NORMAL) {
        fprintf(stderr, "CA error--unable to get BPM value for %s\n", ca_name(bpmPVchid[ibpm]));
        exit(1);
      }
    }
    if (offsetPVchid) {
      for (ibpm = 0; ibpm < bpms; ibpm++) {
        if (ca_get(DBR_DOUBLE, offsetPVchid[ibpm], offsetBuffer + ibpm) != ECA_NORMAL) {
          fprintf(stderr, "CA error--unable to get offset value for %s\n", ca_name(offsetPVchid[ibpm]));
          exit(1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "CA error--unable to get BPM and or offset values\n");
      exit(1);
    }
    if (offsetPVchid) {
      for (ibpm = 0; ibpm < bpms; ibpm++)
        bpmBuffer[ibpm] -= offsetBuffer[ibpm];
    }
    if (offsetValue) {
      for (ibpm = 0; ibpm < bpms; ibpm++) {
        fprintf(stderr, "Subtracting offset %e from %ld\n",
                offsetValue[ibpm], ibpm);
        bpmBuffer[ibpm] -= offsetValue[ibpm];
      }
    }
    for (ibpm = 0; ibpm < bpms; ibpm++)
      aveValue[ibpm] += bpmBuffer[ibpm];
    if (verbose)
      fprintf(stderr, "  Set %ld of readings done.\n", i);
    oag_ca_pend_event(avePause, &sigint);
    if (sigint)
      exit(1);
  }
  for (ibpm = 0; ibpm < bpms; ibpm++)
    aveValue[ibpm] *= weight[ibpm] / averages;

  switch (criterion) {
  case CRITERION_MAV:
    for (*value = ibpm = 0; ibpm < bpms; ibpm++)
      *value += fabs(aveValue[ibpm]);
    *value = *value / bpms;
    break;
  case CRITERION_RMS:
    for (*value = ibpm = 0; ibpm < bpms; ibpm++)
      *value += aveValue[ibpm] * aveValue[ibpm];
    *value = sqrt(*value / bpms);
    break;
  case CRITERION_MAX:
    *value = -DBL_MAX;
    for (ibpm = 0; ibpm < bpms; ibpm++)
      if (*value < aveValue[ibpm])
        *value = aveValue[ibpm];
    break;
  case CRITERION_MIN:
    *value = DBL_MAX;
    for (ibpm = 0; ibpm < bpms; ibpm++)
      if (*value > aveValue[ibpm])
        *value = aveValue[ibpm];
    break;
  case CRITERION_AVE:
    for (*value = ibpm = 0; ibpm < bpms; ibpm++)
      *value += aveValue[ibpm];
    *value /= bpms;
    break;
  default:
    bomb("invalid criterion code (getBPMdata)", NULL);
    break;
  }
  return 1;
}

void waitForCount(chid PVchid, double lower, double upper, long count, long verbose, double pendIOtime) {
  double value;

  if (verbose && count > 1)
    fprintf(stderr, "Counting %s values on [%f, %f]: ", ca_name(PVchid), lower, upper);
  else
    verbose = 0;
  while (count > 0) {
    if (ca_get(DBR_DOUBLE, PVchid, &value) != ECA_NORMAL) {
      fprintf(stderr, "problem getting PV value for wait count\n");
      exit(1);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "problem getting PV value for wait count\n");
      exit(1);
    }
    if (value >= lower && value <= upper) {
      if (verbose)
        fprintf(stderr, "%ld", count);
      oag_ca_pend_event(0.1, &sigint);
      count--;
    } else
      oag_ca_pend_event(0.1, &sigint);
    if (sigint)
      exit(1);
  }
  if (verbose)
    fputc('\n', stderr);
}

void interrupt_handler(int sig) {
  exit(1);
}

void sigint_interrupt_handler(int sig) {
  sigint = 1;
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
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
      rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Error during exiting run control.\n");
      }
    }
#endif
    ca_task_exit();
  }
}

#ifdef USE_RUNCONTROL
/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
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
    oag_ca_pend_event((MIN(timeLeft, rcParam.pingInterval)), &sigint);
    if (sigint)
      exit(1);
    timeLeft -= rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void setupTestsFile(TESTS *test, long verbose) {
  long itest;
  SDDS_TABLE testValuesPage;
  long sleepTimeColumnPresent, resetTimeColumnPresent;

  if (!test->file)
    return;
  if (!SDDS_InitializeInput(&testValuesPage, test->file))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  switch (SDDS_CheckColumn(&testValuesPage, "ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", test->file);
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MinimumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MinimumValue", test->file);
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MaximumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n",
            "MaximumValue", test->file);
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "SleepTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n",
            "SleepTime", test->file);
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "ResetTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    resetTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    resetTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n",
            "ResetTime", test->file);
    exit(1);
  }

  if (SDDS_ReadTable(&testValuesPage) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  test->n = SDDS_CountRowsOfInterest(&testValuesPage);
  test->value = (double *)malloc(sizeof(double) * (test->n));
  test->outOfRange = (long *)malloc(sizeof(long) * (test->n));
  if (verbose) {
    fprintf(stderr, "number of test values = %ld\n", test->n);
  }
  test->controlName = (char **)SDDS_GetColumn(&testValuesPage, "ControlName");
  test->min = SDDS_GetColumnInDoubles(&testValuesPage, "MinimumValue");
  test->max = SDDS_GetColumnInDoubles(&testValuesPage, "MaximumValue");
  if (sleepTimeColumnPresent)
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, "SleepTime");
  if (resetTimeColumnPresent)
    test->reset = SDDS_GetColumnInDoubles(&testValuesPage, "ResetTime");
  if (verbose) {
    for (itest = 0; itest < test->n; itest++) {
      fprintf(stderr, "%3ld%30s\t%11.3f\t%11.3f", itest, test->controlName[itest],
              test->min[itest], test->max[itest]);
      if (sleepTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->sleep[itest]);
      if (resetTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->reset[itest]);
      fprintf(stderr, "\n");
    }
  }
  if (!SDDS_Terminate(&testValuesPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return;
}

long checkOutOfRange(TESTS *test, long verbose) {
  long i, outOfRange;

  outOfRange = 0; /* flag to indicate an out-of-range PV */
  test->longestSleep = 1;
  test->longestReset = 0;
  for (i = 0; i < test->n; i++) {
    if (test->value[i] < test->min[i] ||
        test->value[i] > test->max[i]) {
      test->outOfRange[i] = 1;
      outOfRange = 1;
      /* determine longest time of those tests that failed */
      if (test->sleep)
        test->longestSleep = MAX(test->longestSleep, test->sleep[i]);
      if (test->reset)
        test->longestReset = MAX(test->longestReset, test->reset[i]);
    } else {
      test->outOfRange[i] = 0;
    }
  }
  if (outOfRange) {
    if (verbose) {
      fprintf(stderr, "Test variables out of range:\n");
      for (i = 0; i < test->n; i++) {
        if (test->outOfRange[i])
          fprintf(stderr, "\t%s\t\t%f\n", test->controlName[i], test->value[i]);
      }
      fprintf(stderr, "Waiting for %f seconds.\n", test->longestSleep);
      fflush(stderr);
    }
  }
  return (outOfRange);
}

void RunTest(TESTS *tests, long verbose, double pendIOtime) {
  long outofRange = 0, i_test;

  if (tests->file) {
    tests->count = 0;
    do {
      tests->count++;
      if (tests->count >= tests->limit) {
        if (verbose)
          fprintf(stderr, "Tests failed for %" PRId32 " limited times, exit squishPVs!\n", tests->limit);
        exit(1);
      }
      for (i_test = 0; i_test < tests->n; i_test++) {
        if (ca_get(DBR_DOUBLE, tests->controlPVchid[i_test], tests->value + i_test) != ECA_NORMAL) {
          fprintf(stderr, "CA error--unable to get test value\n");
          exit(1);
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "CA error--unable to get test value\n");
        exit(1);
      }
      outofRange = checkOutOfRange(tests, verbose);
      if (!outofRange) /*passes test*/
        return;
        /*test fails*/
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        strcpy(rcParam.message, "Out-of-range test variable(s)");
        rcParam.status = runControlLogMessage(rcParam.handle,
                                              rcParam.message,
                                              MAJOR_ALARM,
                                              &(rcParam.rcInfo));
        if (rcParam.status != RUNCONTROL_OK) {
          fprintf(stderr, "Unable to write status message and alarm severity\n");
          exit(1);
        }
      }
#endif
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        runControlPingWhileSleep(tests->longestSleep);
      } else {
        oag_ca_pend_event(tests->longestSleep, &sigint);
      }
#else
      oag_ca_pend_event(tests->longestSleep, &sigint);
#endif
      if (sigint)
        exit(1);
    } while (tests->count < tests->limit);
  }
}
