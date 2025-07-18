/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  $Log: not supported by cvs2svn $
 *  Revision 1.24  2009/06/26 20:47:17  shang
 *  it now prints the normal text messages to the standard output instead of standard error
 *
 *  Revision 1.23  2009/06/18 22:16:06  shang
 *  fixed a bug in parsing the output of measurement script that it bumped if the output was an integer, which is wrong.
 *
 *  Revision 1.22  2008/09/19 21:28:52  shang
 *  now it waits for PauseAfterChange seconds after the variable script is executed.
 *
 *  Revision 1.21  2008/06/30 21:22:32  shang
 *  removed the definition of INFOBUF since it is now included in SDDSepics.h
 *
 *  Revision 1.20  2007/03/29 19:24:40  borland
 *  Improved some messages.
 *
 *  Revision 1.19  2006/12/14 22:25:49  soliday
 *  Updated a bunch of programs because SDDS_SaveLayout is now called by
 *  SDDS_WriteLayout and it is no longer required to be called directly.
 *  Also the AutoCheckMode is turned off by default now so I removed calls to
 *  SDDS_SetAutoCheckMode that would attempt to turn it off. It is now up to
 *  the programmer to turn it on in new programs until debugging is completed
 *  and then remove the call to SDDS_SetAutoCheckMode.
 *
 *  Revision 1.18  2005/11/08 22:34:39  soliday
 *  Updated to remove compiler warnings on Linux.
 *
 *  Revision 1.17  2005/11/08 22:05:04  soliday
 *  Added support for 64 bit compilers.
 *
 *  Revision 1.16  2005/10/27 17:05:46  shang
 *  it nows does not log data if logFile is not provided.
 *
 *  Revision 1.15  2005/08/22 22:11:19  shang
 *  added Time column to log file
 *
 *  Revision 1.14  2005/02/13 05:58:22  borland
 *  Simplex restart feature now works.
 *  Can lengthen the log-file table new required.
 *
 *  Revision 1.13  2005/02/12 20:13:24  borland
 *  When preparing value list for varScript, now uses %21.15e format instead of %f format, to improve precision.
 *
 *  Revision 1.12  2005/02/12 19:46:09  borland
 *  Added another missing pclose().
 *
 *  Revision 1.11  2005/02/12 19:21:49  borland
 *  Added pclose() so that optimization doesn't terminate due to running out of file descriptors.
 *
 *  Revision 1.10  2005/02/01 14:56:53  shang
 *  corrected the error message for -varScript
 *
 *  Revision 1.9  2004/11/08 22:36:03  shang
 *  modified such that the InitialValue column is required to be included in the input file for
 *  running with varScript.
 *
 *  Revision 1.8  2004/07/29 18:23:16  shang
 *  fixed the bug that restartFile options did not when -measScript and -varScript
 *  options are provided.
 *
 *  Revision 1.7  2004/07/22 22:05:18  soliday
 *  Improved signal support when using Epics/Base 3.14.6
 *
 *  Revision 1.6  2004/07/19 17:39:38  soliday
 *  Updated the usage message to include the epics version string.
 *
 *  Revision 1.5  2004/07/16 21:24:40  soliday
 *  Replaced sleep commands with ca_pend_event commands because Epics/Base
 *  3.14.x has an inactivity timer that was causing disconnects from PVs
 *  when the log interval time was too large.
 *
 *  Revision 1.4  2004/03/26 22:36:38  shang
 *  prints out the out-of-range test values
 *
 *  Revision 1.3  2004/01/24 18:16:20  borland
 *  No longer announces that too many evaluations have been done for non-simplex
 *  optimizer, since the simplexMinAbort() function doesn't work for non-simplex
 *  optimizers.
 *
 *  Revision 1.2  2003/11/16 03:56:48  borland
 *  Fixed bugs in reading of parameters from variable and measurement
 *  files.  The setting of flags to indicate that a parameter was found
 *  was not done correctly.
 *
 *  Revision 1.1  2003/08/27 19:51:18  soliday
 *  Moved into subdirectory
 *
 *  Revision 1.35  2003/06/17 20:11:01  soliday
 *  Added support for AIX.
 *
 *  Revision 1.34  2003/03/26 22:56:31  shang
 *  modified such that it also accepts the old parameter names
 *
 *  Revision 1.33  2003/03/26 15:27:41  shang
 *  changed the first letter of parameters of SDDS file to upper case
 *
 *  Revision 1.32  2003/03/18 23:03:06  shang
 *  fixed some bugs for running in windows
 *
 *  Revision 1.31  2003/03/18 22:25:45  shang
 *  made big changes and fixed knob pv problems
 *
 *  Revision 1.30  2003/02/25 22:20:21  shang
 *  removed extra ca_pend_io() statement
 *
 *  Revision 1.29  2003/02/25 16:45:35  shang
 *  made the varscript and measurement script work in windows system and
 *  added ca_pend_io(pendIOTime) after ca_search statement
 *
 *  Revision 1.28  2003/02/13 20:08:06  soliday
 *  Changed the simplexMin arguments because an additional argument has been added.
 *
 *  Revision 1.27  2003/01/16 16:34:38  borland
 *  Fixed bug introduced when EZCA was eliminated: the script tried to
 *  make CA connections even if a variable script was used.
 *
 *  Revision 1.26  2003/01/15 23:14:36  borland
 *  Added new argument for simplexMin().
 *
 *  Revision 1.25  2002/11/27 16:22:06  soliday
 *  Fixed issues on Windows when built without runcontrol
 *
 *  Revision 1.24  2002/11/26 22:07:08  soliday
 *  ezca has been 100% removed from all SDDS Epics programs.
 *
 *  Revision 1.23  2002/11/19 22:33:04  soliday
 *  Altered to work with the newer version of runcontrol.
 *
 *  Revision 1.22  2002/11/12 20:35:07  shang
*  added the disable ability to selectively exclude some of the control parameters from variation while still setting their values.
 *
 *  Revision 1.21  2002/10/08 16:16:14  soliday
 *  Committed Shang's version
 *
 *  Revision 1.20  2002/08/14 20:00:35  soliday
 *  Added Open License
 *
 *  Revision 1.19  2002/08/12 22:29:18  borland
 *  Fixed problem with processing of return from measurement script.
 *  Also added some "verbose" outputs and did some indenting.
 *
 *  Revision 1.18  2002/07/13 21:07:35  borland
 *  No longer exits with an error when number of evaluations limit is reached.
 *  Added sign randomization feature (for initial steps) to simplex mode.
 *
 *  Revision 1.17  2002/07/10 21:11:47  borland
 *  Restored code that sends the best values back out before terminating.
 *
 *  Revision 1.16  2002/05/08 18:28:18  shang
 *  removed the printout message of varScript
 *
 *  Revision 1.15  2002/05/08 18:25:59  shang
 *  added -varScript feature
 *
 *  Revision 1.14  2002/01/03 22:11:42  soliday
 *  Fixed problems on WIN32.
 *
 *  Revision 1.13  2001/09/12 21:52:13  soliday
 *  Set the default ping timeout to 2 and the ping interval to 1.
 *
 *  Revision 1.12  2001/09/10 19:18:28  soliday
 *  Fixed Linux compiler warnings.
 *
 *  Revision 1.11  2001/08/31 19:55:25  shang
 *  replaced 'sleeptime=MAX(test->longestSleep,PauseAfterChange);'
 *  by 'sleeptime=MAX(test->longestSleep,1);'
 *
 *  Revision 1.10  2001/08/22 00:38:45  shang
 *
 *  remove compile warnings
 *
 *  Revision 1.9  2001/08/17 19:45:51  shang
 *  add RunTest(TESTS *test) function and check the test values in the beginning, each
 *  time it takes measurement (if using a measurement file), or before and after running
 *  the script (if doing a measurement script).
 *
 *  Revision 1.8  2001/08/14 14:31:31  shang
 *  replace the total number of testing (test->count) by the failed testing number, if
 *  it is over the limited times, exit the application.
 *
 *  Revision 1.7  2001/08/13 20:16:06  shang
 *  delete unnecessary parts in func()
 *
 *  Revision 1.6  2001/08/13 19:40:54  shang
 *  fix test.count problem
 *
 *  Revision 1.5  2001/08/10 22:42:11  shang
 *  add -testValues and -runControlPV -runControlDescription options
 *
 *  Revision 1.4  2001/08/06 19:38:32  shang
 *  replaced tmalloc() by calloc()
 *
 *  Revision 1.3  2001/08/06 19:26:06  shang
 *  add checking whether the given initial values are out of range feature
 *
 *  Revision 1.2  2001/07/31 20:54:50  shang
 *  replace SDDS_WritePage() by SDDS_UpdatePage() so that the data is written into log
 *  file during processing.
 *
 *  Revision 1.1  2001/07/20 16:42:29  borland
 *  First version, by Hairong Shang.
 *
 */

#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "match_string.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <cadef.h>
#include <alarm.h>
#include <signal.h>

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

void interrupt_handler(int sig);
void rc_interrupt_handler();
void abortSimplex(int sig);

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define SET_INPUTMEAS 0
#define SET_SCRIPT 1
#define SET_INPUTVAR 2
#define SET_KNOBFILE 3
#define SET_SIMPLEX 4
#define SET_LOG 5
#define SET_VERBOSE 6
#define SET_TOLERANCE 7
#define SET_MAXIMIZE 8
#define SET_1DSCAN 9
#define SET_TARGET 10
#define SET_TEST 11
#define SET_RUNCONTROLPV 12
#define SET_RUNCONTROLDESC 13
#define SET_VARSCRIPT 14
#define SET_RESTARTFILE 15
#define SET_EZCATIMING 16
#define SET_PENDIOTIME 17
#define SET_RCDS 18
#define SET_DRYRUN 19
#define SET_EXTRALOGFILE 20
#define SET_CONDITIONING 21
#define N_OPTIONS 22
char *option[N_OPTIONS] = {
  "measFile", "measScript", "varfile", "knobFiles", "simplex", "logfile", "verbose", "tolerance",
  "maximize", "1dscan", "target", "testValues", "runControlPV", "runControlDescription", "varScript",
  "restartFile", "ezcatiming", "pendiotime", "rcds", "dryRun", "extraLogFile", "conditioning"};

static char *USAGE1 = "sddsoptimize [-measFile=<filename>|-measScript=<script>] \n\
       [-varScript=<scriptname>] [-restartFile=<filename>] [-pendIOtime=<seconds>] [-conditioning=<script>] \n\
       -varFile=<filename> -knobFiles=<filename1> , <filename2>,... \n\
       [-simplex=[restarts=<nRestarts>][,cycles=<nCycles>,] \n\
       [evaluations=<nEvals>,][no1dscans][,divisions=<int>][,randomSigns]] \n\
       [-logFile=<filename>] [-verbose] [-tolerance=<value>] [-maximize] \n\
       [-1dscan=[divisions=<value>,][cycles=<number>][,evaluations=<value>][,refresh]]\n\
       [-target=<value>] [-testValues=file=<filename>[,limit=<count>]]\n\
       [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>,\n\
         pingInterval=<value> [-dryRun] [-extraLogFile=<filename>] \n\
       [-rcds=[evaluations=<number>][,cycles=<number>][,noise=<value>][,step=<value>][,dmatFile=<filename>][,useMinForBracket][,verbose]] \n";
static char *USAGE2 = "       [-runControlDescription={string=<string>|parameter=<string>}]\n\
-varFile    contains ControlName, LowerLimit, UpperLimits, InitialChange (and InitialValue) \n\
            columns and pauseBetweenReadings parameter, which sets the waiting time \n\
            between two settings (seconds). Column ControlName, LowerLimit, UpperLimits, and InitialChange \n\
            are required, InitialChange column provides the initial step size of the variable PVs. \n\
            Column InitialValue, ReadbackName and Tolerance are optional, if InitialValue column is \n\
            not provided, the initial values will be read from PVs. If Readback column provided, \n\
            the difference between the setpoint and readback will be checked, \n\
            it waits the difference between the readback and setpoint to be within the given tolerance \n\
            the tolerance can be provided by column Tolerance, or parameter ControlTolerance, or in command line with varFile option. \n\
            if RelativeLimit column is provided, the LowerLimit and UpperLimit will be changed to InitialValue-RelativeLimit and \n\
            InitialValue+RelativeLimit, but must be within the LowerLimit and UpperLimit provided by the input file.\n\
-measFile   give the PVs to optimize, it contains ControlName, ReadbackName, \n\
            (and ReadbackUnits, Weight) columns. It could contain one PV or \n\
            many PVs. The RMS of measurement PV (or PVs) is optimized by sddsoptimize. \n\
            It also contains tolerance (double), numberToAverage(long) and \n\
            pauseBetweenReadings (double) parameters, which sets the converging limit, \n\
            number of averages and interval between two readings respectively. \n\
-measScript user given script for measuring PVs. Either -measScript or -measFile is given\n\
            for the measurement. \n\
-restartFile user given filename for writing the optimized values into this file, which \n\
            could be used for the input of next optimization. \n\
-varScript  user given script for setting setpoint PVs. Must take arguments -tagList and\n\
            -valueList. \n\
-simplex    Give parameters of the simplex optimization. Each start or restart allows \n\
            <nCycles> cycles with up to <nEvals> evaluations of the function.  Defaults \n\
            are 1 restarts, 1 cycles, and 100 evaluations (iterations). \n\
            If no1dscan is given, then turn off the 1D scan function in simplex.\n\
            Divisions gives the parameter of maximum divisions used in simplex.\n\
            if refresh is given, then read the previous better point again to keep track \n\
            with the noise. If there is not much noise, it should be turned off to have \n\
            a better performance.\n\
-rcds       Give parameters for RCDS (robust conjugate direction method) optimization. \n\
            evaluations gives the maximum number of function evaluations, and cycles gives the maximum \n\
            iterations of bracket minimization search, noise gives the experimental noise (default 0.001)\n\
            step is the normalized step size when the values of the variables are normalized to between 0 and 1,\n\
            default step is 0.01, dmatFile provides the filename for initial Dmatrix which is used in computing \n\
            the initial conjugate direction.\n";
static char *USAGE3 = "-1dscan     Give parameters of one dimensional scan optimization. Cycles gives the max. \n\
            number of cycles (i.e. loops) of 1dscan and with an maximum number of \n\
            evaluations. Divisions is the max. number of division applied in 1dscan. \n\
            Either 1dscan or simplex method is used for optimize. \n\
            repeat: read the measurement again if the variable PVs are set back \n\
            to their previous values to keep track with noise.\n\
-verbose    Specifies printing of possibly useful data to the standard output.\n\
-target     the target value for RMS of measurement PV (or PVs). \n\
-tolerance  tolerance should be given if measScript is used, otherwise, a default value \n\
            of 0.001 is set to tolerance.\n\
-testValues sdds format file containing minimum and maximum values \n\
            of PV's specifying a range outside of which the feedback \n\
            is temporarily suspended. Column names are ControlName,\n\
            MinimumValue, MaximumValue. limit specifies the maximum times of testing.\n\
-extraLogFile provide PVs for extra logging, must have ControlName column.\n\
-maximize   If maximize option is given, sddsoptimize maximize measurement by varying control\n\
            PVs and/or knobs. Otherwise, it minimize the measurement.\n\
runControlPV  specifies the runControl PV record.\n\
runControlDescription\n\
            specifies a string parameter whose value is a runControl PV description \n\
            record.\n\n\
Program by H. Shang, ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

/* following is defined for reading knobs */
typedef struct
{
  char *knobName;
  long pvIndex; /*the index in the variables */
  double gain;
  double value;
  double stepSize; /*step limit*/
  double rampPause; /*wait time between ramp steps */
  long variables; /*the number of PVs the knob contains */
  char **ControlPV;
  chid *ControlPVchid;
  double *Weight, *values;
  int32_t *Sector;
} KNOB;

/*struct for control name */
typedef struct
{
  long Variables, controlTolProvided;
  char **ControlName, **ReadbackName;
  chid *ChannelID, *ReadbackID;
  short *KnobFlag;
  double *Value, *tol, *Readback, controlTol;
  KNOB *Knob;
  long Knobs;
  double *LowerLimit;
  double *UpperLimit;
  double *InitialChange;
  double *InitialValue;
  double *RelativeLimit;
  short *Disable;
  double *HoldOffTime;
  double PauseAfterChange;
  char *varScriptExtraOption;
  char *measScriptExtraOption;
} CONTROL_NAME;

/*struct for readback name */
typedef struct
{
  long Variables;
  char **ControlName, **ReadbackName, **ReadbackUnits;
  chid *ChannelID;
  double *Value;
  double *DesiredValue;
  double *Weight;
  double Interval, Tolerance;
  int32_t Averages;
} READBACK_NAME;

typedef struct
{
  char *file;
  long n, *outOfRange;
  char **controlName;
  chid *controlPVchid;
  double *value, *min, *max, *sleep, *reset, longestSleep, longestReset;
  int count;
} TESTS;

typedef struct
{
  long verbose, count, dryRun;
  char *logFile, *extraLogFile;
  SDDS_DATASET *logData;
  char **extra_pvname;
  chid *extra_pvID;
  double pendIOTime, controlTol, *extra_pvvalue;
  short maximize;
  int32_t nEvalMax, extra_pvs;
  char *varScript, *measScript;
  /*test parameters*/
  int32_t limit;
  TESTS *test;
  char *conditioningCommand;
} COMMON_PARAM;

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

int runControlPingWhileSleep(double sleepTime);
RUNCONTROL_PARAM rcParam; /* need to be global because some quantities are used
                             in signal and interrupt handlers */
#endif
#ifndef USE_RUNCONTROL
static char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#else
static char *USAGE_WARNING = "";
#endif

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV={string=<string>|parameter=<string>}\n";
static char *runControlDescUsage = "-runControlDescription={string=<string>|parameter=<string>}\n";
#endif

void commandServer(int sig);
void serverExit(int sig);
void exitIfServerRunning(void);

void InitializeReadbackName(READBACK_NAME *readback);
void InitializeControlName(CONTROL_NAME *control);
void InitializeKnob(KNOB *knob);
void InitializeParam(COMMON_PARAM *param);
void InitializeTest(TESTS *test);
void FreeEverything(CONTROL_NAME *control, READBACK_NAME *readback, COMMON_PARAM *param);

long getVariablesData(CONTROL_NAME *control, char *inputFile,
                      char *restartFile, SDDS_TABLE *restartTable,
                      long knobFiles, char **knobFile);
long getMeasurementData(READBACK_NAME *readback, char *inputFile, unsigned long mode,
                        short getreadbackUnits, double pendIOTime);

short simplex = 0, rcds = 0;
double localfunc(double *pValue, long *invalid);
void report(double y, double *x, long pass, long nEval, long n_dimen);
void setupOutputFile(SDDS_DATASET *outTable, COMMON_PARAM *param, char *outputFile, char *extraLogFile);

void setupTestsFile(TESTS *tests);
long checkOutOfRange(TESTS *test);
void RunTest(TESTS *test);
long SetKnobArray(char **knobFile, long konbFiles, CONTROL_NAME *control);
double ReadKnobValue(KNOB knob);
long SetKnobValue(KNOB *knob, double pValue);

long ReadPVValues(CONTROL_NAME *control);
long WritePVValues(CONTROL_NAME *control, double *pValue);

/*global variables */
CONTROL_NAME *control;
READBACK_NAME *readback;
COMMON_PARAM *param;

static long rowLimit = 0;

int main(int argc, char **argv) {

#ifdef USE_RUNCONTROL
  unsigned long dummyFlags;
#endif
  long iRestart;
  int32_t nRestartMax = 1, nPassMax = 1, nDivisionMax = 12, nRepeatMax = 1;
  unsigned long simplexFlags = 0, scanflags = 0, testFlags = 0, rcdsFlags = 0;
  long i, i_arg, invalid = 0, nEval = 0;
  SCANNED_ARG *s_arg;
  double *paramValue, *paramDelta = NULL;
  double result, *bestParamValue = NULL, bestResult = 0.0, lastResult = 0.0, target;
  char *inputMeasureFile, *inputVarFile;
  short scan = 0, targetFlag = 0;
  long knobFiles = 0;
  int32_t dmat_cols = 0, dmat_rows = 0;
  char **KnobFile = NULL;
  char *restartFile, **dcolName = NULL;
  SDDS_TABLE restartTable, dmatTable;
  double rcdsStep = 0.01, rcdsNoise = 0.0;
  double **dmat0 = NULL; /*rcds Dmatrix */
  char *dmatFile = NULL; /*rcds Dmatrix input file */

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif

  restartFile = NULL;

  target = 1e-14;
  inputMeasureFile = inputVarFile = NULL;

  /*initialize control and readback */
  control = malloc(sizeof(*control));
  readback = malloc(sizeof(*readback));
  param = malloc(sizeof(*param));

  InitializeControlName(control);
  InitializeReadbackName(readback);
  InitializeParam(param);

  /*initialize rcParam */
#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = rcParam.PVparam = rcParam.DescParam = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  signal(SIGINT, abortSimplex);
  signal(SIGTERM, abortSimplex);
  signal(SIGILL, interrupt_handler);
  signal(SIGABRT, interrupt_handler);
  signal(SIGFPE, interrupt_handler);
  signal(SIGSEGV, interrupt_handler);
#ifndef _WIN32
  signal(SIGHUP, interrupt_handler);
  signal(SIGQUIT, abortSimplex);
  signal(SIGTRAP, interrupt_handler);
  signal(SIGBUS, interrupt_handler);
#endif

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 3) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    fprintf(stderr, "%s", USAGE_WARNING);
    exit(1);
  }

  /* for structure checks the syntax */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_CONDITIONING:
	if (s_arg[i_arg].n_items != 2)
          bomb("invalid -conditioning syntax", NULL);
        SDDS_CopyString(&param->conditioningCommand, s_arg[i_arg].list[1]);
        break;
      case SET_INPUTMEAS:
        if (s_arg[i_arg].n_items > 2)
          bomb("invalid -measFile syntax", NULL);
        if (s_arg[i_arg].n_items == 2)
          SDDS_CopyString(&inputMeasureFile, s_arg[i_arg].list[1]);
        break;
      case SET_SCRIPT:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -measScript syntax", NULL);
        SDDS_CopyString(&param->measScript, s_arg[i_arg].list[1]);
        break;
      case SET_VARSCRIPT:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -varScript syntax", NULL);
        SDDS_CopyString(&param->varScript, s_arg[i_arg].list[1]);
        break;
      case SET_INPUTVAR:
        if (s_arg[i_arg].n_items == 2)
          SDDS_CopyString(&inputVarFile, s_arg[i_arg].list[1]);
        else if (s_arg[i_arg].n_items == 3) {
          SDDS_CopyString(&inputVarFile, s_arg[i_arg].list[1]);
          s_arg[i_arg].list += 2;
          s_arg[i_arg].n_items -= 2;
          if (!!scanItemList(&dummyFlags, s_arg[i_arg].list, &s_arg[i_arg].n_items, 0,
                             "tolerance", SDDS_DOUBLE, &control->controlTol, 1, 0, NULL))
            SDDS_Bomb("invalid -varFile syntax");
        } else
          bomb("invalid -varFile syntax", NULL);
        break;
      case SET_RESTARTFILE:
        if (s_arg[i_arg].n_items > 2)
          bomb("invalid -restartFile syntax", NULL);
        if (s_arg[i_arg].n_items == 2)
          SDDS_CopyString(&restartFile, s_arg[i_arg].list[1]);
        break;
      case SET_KNOBFILE:
        knobFiles = s_arg[i_arg].n_items - 1;
        if (knobFiles < 1)
          SDDS_Bomb("Invalid -knobFiles syntax");
        KnobFile = (char **)malloc(sizeof(char *) * knobFiles);
        for (i = 1; i <= knobFiles; i++) {
          SDDS_CopyString(&KnobFile[i - 1], s_arg[i_arg].list[i]);
        }
        break;
      case SET_SIMPLEX:
        s_arg[i_arg].n_items -= 1;
        /* simplexFlags = 0;*/
        simplex = 1;
        if (scan)
          SDDS_Bomb("1DScan has been chosen as optimization method!");
        if (rcds)
          SDDS_Bomb("rcds has been chosen as optimization method!");
        if (!scanItemList(&simplexFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "restarts", SDDS_LONG, &nRestartMax, 1, 0,
                          "cycles", SDDS_LONG, &nPassMax, 1, 0,
                          "evaluations", SDDS_LONG, &param->nEvalMax, 1, 0,
                          "divisions", SDDS_LONG, &nDivisionMax, 1, 0,
                          "no1dscans", -1, NULL, 0, SIMPLEX_NO_1D_SCANS,
                          "randomsigns", -1, NULL, 0, SIMPLEX_RANDOM_SIGNS,
                          NULL) ||
            nRestartMax < 0 || nPassMax <= 0 || param->nEvalMax <= 0 || nDivisionMax <= 0)
          SDDS_Bomb("invalid -simplex syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case SET_RCDS:
        rcds = 1;
        if (scan)
          SDDS_Bomb("1DScan has been chosen as optimization method!");
        if (simplex)
          SDDS_Bomb("simplex has been chosen as optimization method!");
        s_arg[i_arg].n_items -= 1;
        rcdsFlags = 0;
        if (!scanItemList(&rcdsFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "cycles", SDDS_LONG, &nPassMax, 1, 0,
                          "evaluations", SDDS_LONG, &param->nEvalMax, 1, 0,
                          "step", SDDS_DOUBLE, &rcdsStep, 1, 0,
                          "noise", SDDS_DOUBLE, &rcdsNoise, 1, 0,
                          "dmatFile", SDDS_STRING, &dmatFile, 1, 0,
                          "verbose", -1, NULL, 0, SIMPLEX_VERBOSE_LEVEL1,
                          "useMinForBracket", -1, NULL, 0, RCDS_USE_MIN_FOR_BRACKET,
                          NULL) ||
            nPassMax <= 0 || param->nEvalMax <= 0 || nDivisionMax <= 0)
          SDDS_Bomb("invalid -rcds syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case SET_LOG:
        if (s_arg[i_arg].n_items > 2)
          bomb("invalid -logFile syntax", NULL);
        if (s_arg[i_arg].n_items == 2)
          SDDS_CopyString(&param->logFile, s_arg[i_arg].list[1]);
        break;
      case SET_EXTRALOGFILE:
	if (s_arg[i_arg].n_items > 2)
          bomb("invalid -extraLogFile syntax", NULL);
        if (s_arg[i_arg].n_items == 2)
          SDDS_CopyString(&param->extraLogFile, s_arg[i_arg].list[1]);
        break;
      case SET_VERBOSE:
        param->verbose = 1;
        break;
      case SET_DRYRUN:
        param->dryRun = 1;
        break;
      case SET_TOLERANCE:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&(readback->Tolerance), s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -tolerance");
        break;
      case SET_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &param->pendIOTime) != 1 ||
            param->pendIOTime <= 0)
          bomb("invalid -pendIOTime syntax\n", NULL);
        break;
      case SET_TARGET:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&target, s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -target");
        targetFlag = 1;
        break;
      case SET_MAXIMIZE:
        param->maximize = 1;
        break;
      case SET_1DSCAN:
        scan = 1;
        if (simplex)
          SDDS_Bomb("Simplex has been chosen as optimization method!");
        if (rcds)
          SDDS_Bomb("rcds has been chosen as optimization method!");
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&scanflags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "divisions", SDDS_LONG, &nDivisionMax, 1, 0,
                          "cycles", SDDS_LONG, &nRepeatMax, 1, 0,
                          "dmatFile", SDDS_LONG, &dmatFile, 1, 0,
                          "evaluations", SDDS_LONG, &param->nEvalMax, 1, 0,
                          "refresh", -1, NULL, 0, ONEDSCANOPTIMIZE_REFRESH,
                          NULL) ||
            nDivisionMax < 0 || nRepeatMax <= 0)
          SDDS_Bomb("invalid -1dscan syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case SET_TEST:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&testFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "file", SDDS_STRING, &param->test->file, 1, 0,
                          "limit", SDDS_LONG, &param->limit, 1, 0,
                          NULL) ||
            !(param->test->file)) {
          SDDS_Bomb("bad -testValues syntax.");
        }
        s_arg[i_arg].n_items += 1;
        break;
      case SET_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          "pingInterval", SDDS_DOUBLE, &rcParam.pingInterval, 1, 0,
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
        fprintf(stdout, "runControl is not available. Option -runControlPV ignored.\n");
        fflush(stdout);
#endif
        break;
      case SET_RUNCONTROLDESC:
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
        fprintf(stdout, "runControl is not available. Option -runControlDescription ignored.\n");
        fflush(stdout);
#endif
        break;
      default:
        fprintf(stderr, "unrecognized option: %s (sddsoptimize)\n",
                s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      bomb("Invalid syntax (sddsoptimize)", NULL);
    }
  }

  if (param->measScript && inputMeasureFile)
    SDDS_Bomb("Too many measurements are given!");
#ifdef USE_RUNCONTROL
  if (rcParam.PV && !rcParam.Desc) {
    fprintf(stderr, "the runControlDescription should not be null!");
    exit(1);
  }
#endif
  if (!simplex && !scan && !rcds)
    SDDS_Bomb("No method is given for optimization!");
  if (simplex && scan)
    SDDS_Bomb("only one method (simplex or 1dscan) should be given.");
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

  /*read the measurement info */
  if (inputMeasureFile) {
    if (param->verbose) {
      fprintf(stdout, "\nGet the measurement information ...");
      fflush(stdout);
    }
    if (!getMeasurementData(readback, inputMeasureFile, GET_UNITS_IF_BLANK, 1, param->pendIOTime))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (param->verbose) {
      fprintf(stdout, "\npauseBetweenReadings= %f ", readback->Interval);
      fprintf(stdout, "\nnumber of averages= %d ", readback->Averages);
      fprintf(stdout, "\ntolerance= %e ", readback->Tolerance);
      fflush(stdout);
    }
  }

  if (param->verbose && param->measScript) {
    fprintf(stdout, "The script command is %s\n", param->measScript);
    fflush(stdout);
  }

  if (param->maximize) {
    if (!targetFlag)
      target = -DBL_MAX;
    else
      target = -1.0 * target;
  }

  /*read the testfile */
  if (param->test)
    setupTestsFile(param->test);
  if (inputVarFile) {
    if (param->verbose) {
      fprintf(stdout, "\nGet the variable information ...");
      fflush(stdout);
    }
    if (!getVariablesData(control, inputVarFile, restartFile, &restartTable, knobFiles, KnobFile))
      SDDS_Bomb("Error in reading variable file!");
  }

  if (param->varScript) {
    long total = 0;
    param->varScript = SDDS_Realloc(param->varScript, sizeof(*param->varScript) * (strlen(param->varScript) + 25));
    strcat(param->varScript, " -tagList \"");
    for (i = 0; i < control->Variables; i++) {
      total = strlen(param->varScript) + strlen(control->ControlName[i]) + 2;
      param->varScript = SDDS_Realloc(param->varScript, sizeof(*param->varScript) * total);
      strcat(param->varScript, control->ControlName[i]);
      if (i < control->Variables - 1)
        strcat(param->varScript, " ");
    }

    if (control->varScriptExtraOption) {
      param->varScript = SDDS_Realloc(param->varScript, sizeof(*param->varScript) * (total + 25));
      strcat(param->varScript, "\" -extra \"");
      total = strlen(param->varScript) + strlen(control->varScriptExtraOption) + 2;
      param->varScript = SDDS_Realloc(param->varScript, sizeof(*param->varScript) * total);
      strcat(param->varScript, control->varScriptExtraOption);
    }

    param->varScript = SDDS_Realloc(param->varScript, sizeof(*param->varScript) * (total + 25));
    strcat(param->varScript, "\" -valueList \"");
    if (param->verbose) {
      fprintf(stdout, "\nvarScript: %s\n", param->varScript);
      fflush(stdout);
    }
  }
  if (param->measScript) {
    long total = 0;
    if (control->measScriptExtraOption) {
      param->measScript = SDDS_Realloc(param->measScript, sizeof(*param->measScript) * (strlen(param->measScript) + 25));
      strcat(param->measScript, " -extra \"");
      total = strlen(param->measScript) + strlen(control->measScriptExtraOption) + 4;
      param->measScript = SDDS_Realloc(param->measScript, sizeof(*param->measScript) * total);
      strcat(param->measScript, control->measScriptExtraOption);
      strcat(param->measScript, "\"");
    }
    if (param->verbose) {
      fprintf(stdout, "\nmeasScript: %s\n", param->measScript);
      fflush(stdout);
    }
  }

  /*if the InitialValue is not given in var file, read the present
    value as Initial Value */
  if (!control->InitialValue) {
    control->InitialValue = calloc(control->Variables, sizeof(*(control->InitialValue)));
    if (!param->varScript) {
      if (param->verbose) {
        fprintf(stdout, "Reading the present values:\n");
        fflush(stdout);
      }
      if (ReadPVValues(control))
        SDDS_Bomb("Error occurred in reading PV values");
    } else {
      fprintf(stderr, "The initial values are not provided (required by running varScript).\n");
      exit(1);
    }
  }
  /*update the limit if RelativeLimit column exist */
  if (control->RelativeLimit) {
    double lower, upper;
    for (i=0; i<control->Variables; i++) {
      lower = control->InitialValue[i] - control->RelativeLimit[i];
      upper = control->InitialValue[i] + control->RelativeLimit[i];
      if (lower>control->LowerLimit[i] && lower<control->UpperLimit[i])
	control->LowerLimit[i]=lower;
      if (upper<control->UpperLimit[i] && upper>control->LowerLimit[i])
	control->UpperLimit[i]=upper;
      
    }
  }
  for (i = 0; i < control->Variables; i++) {
    if (!control->KnobFlag[i]) {
      if (control->InitialValue[i] > control->UpperLimit[i] ||
          control->InitialValue[i] < control->LowerLimit[i]) {
        fprintf(stderr, "The initial value of %s is out of range (%e, %e), %e\n",
                control->ControlName[i], control->InitialValue[i], control->LowerLimit[i], control->UpperLimit[i]);
        exit(1);
      }
    }
  }
  if (param->verbose) {
    fprintf(stdout, "\nPauseAfterChange= %f ", control->PauseAfterChange);
    fflush(stdout);
    for (i = 0; i < control->Variables; i++) {
      fprintf(stdout, "\ncontrol PV=%s  InitialValue=%.2e   LowerLimit=%.2e   upperLimit=%.2e",
              control->ControlName[i], control->InitialValue[i], control->LowerLimit[i], control->UpperLimit[i]);
      fflush(stdout);
    }
  }

  /* allocate memory */
  if (!(paramValue = tmalloc(sizeof(*paramValue) * (control->Variables))) ||
      !(paramDelta = tmalloc(sizeof(*paramDelta) * (control->Variables))) ||
      !(bestParamValue = tmalloc(sizeof(*bestParamValue) * (control->Variables))))
    SDDS_Bomb("memory allocation failure");
  if (param->logFile) {
    if (!(param->logData = tmalloc(sizeof(*param->logData))))
      SDDS_Bomb("memory allocation failure");
  }
  
  /*set the parameter value and parameter changes for simplex */
  for (i = 0; i < control->Variables; i++) {
    paramValue[i] = control->InitialValue[i];
    paramDelta[i] = control->InitialChange[i];
  }
  /* show starting values */
  if (param->verbose) {
    fprintf(stdout, "\n\nStarting values and step sizes:\n");
    fflush(stdout);
    for (i = 0; i < control->Variables; i++)
      if (param->verbose) {
        fprintf(stdout, "%s = %f    %f\n", control->ControlName[i], paramValue[i], paramDelta[i]);
        fflush(stdout);
      }
  }

  if (param->logFile) {
    setupOutputFile(param->logData, param, param->logFile, param->extraLogFile);
    rowLimit = param->nEvalMax + 10;
    if (!SDDS_StartPage(param->logData, param->nEvalMax + 10)) {
      SDDS_PrintErrors(stderr, 1);
      exit(1);
    }
    if (param->verbose) {
      fprintf(stdout, "\nThe log file is %s\n", param->logFile);
      fflush(stdout);
    }
    /*write to log file */
    if (!SDDS_SetParameters(param->logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "measurementFile", inputMeasureFile, "variableFile",
                            inputVarFile, "tolerance",
                            readback->Tolerance, NULL)) {
      SDDS_PrintErrors(stderr, 1);
      exit(1);
    }
  }
  if (param->verbose) {
    fprintf(stdout, "\nStarting optimization...\n");
    fflush(stdout);
  }
#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    /* ping once at the beginning */
    if (runControlPingWhileSleep(0.0)) {
      fprintf(stderr, "Problem pinging the run control record.\n");
      exit(1);
    }
    strcpy(rcParam.message, "sddsoptimize started");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable tp write status message and alarm severity\n");
      exit(1);
    }
  }
#endif
  /*run tests in the beginning of optimization */
  if (param->verbose)
    fprintf(stdout, "Run test in the beginning...\n");
  RunTest(param->test);
  if (rcds && dmatFile) {
    dmat0 = malloc(sizeof(*dmat0) * control->Variables);
    if (!SDDS_InitializeInput(&dmatTable, dmatFile)) {
      fprintf(stderr, "Error reading dmatFile\n");
      exit(1);
    }
    if (!(dcolName = (char **)SDDS_GetColumnNames(&dmatTable, &dmat_cols)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (dmat_cols != control->Variables) {
      fprintf(stderr, "Error: dmatrix columns should be the same as control variables!\n");
      exit(1);
    }
    if (SDDS_ReadPage(&dmatTable) <= 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    dmat_rows = SDDS_CountRowsOfInterest(&dmatTable);
    if (dmat_rows != control->Variables) {
      fprintf(stderr, "Error: dmatrix rows should be the same as control variables!\n");
      exit(1);
    }
    for (i = 0; i < control->Variables; i++) {
      dmat0[i] = SDDS_GetColumnInDoubles(&dmatTable, dcolName[i]);
    }
    SDDS_FreeStringArray(dcolName, control->Variables);
    if (!SDDS_Terminate(&dmatTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (param->verbose)
    fprintf(stdout, "start optimize iterations...\n");
  fflush(stdout);
  for (iRestart = nEval = 0; iRestart < nRestartMax; iRestart++) {
    for (i = 0; i < control->Variables; i++) {
      if (paramValue[i] < control->LowerLimit[i])
        paramValue[i] = control->LowerLimit[i] + paramDelta[i];
      if (paramValue[i] > control->UpperLimit[i])
        paramValue[i] = control->UpperLimit[i] - paramDelta[i];
    }

    if (simplex)
      nEval += simplexMin(&result, paramValue, paramDelta, control->LowerLimit, control->UpperLimit,
                          control->Disable, control->Variables, target, readback->Tolerance, localfunc,
                          report, param->nEvalMax, nPassMax, nDivisionMax, 3.0, 1.0, simplexFlags);
    if (scan)
      nEval += OneDScanOptimize(&result, paramValue, paramDelta, control->LowerLimit, control->UpperLimit,
                                control->Disable, control->Variables, target, readback->Tolerance, localfunc,
                                report, param->nEvalMax, nDivisionMax, nRepeatMax, simplexFlags);
    if (rcds)
      nEval += rcdsMin(&result, paramValue, paramValue, paramDelta, control->LowerLimit, control->UpperLimit, dmat0, control->Variables,
                       target, readback->Tolerance, localfunc,
                       report, param->nEvalMax, nPassMax, rcdsNoise, rcdsStep, rcdsFlags);
    if (nEval < 0) {
      fprintf(stderr, "Error occurred during simplex or not converged\n");
      exit(1);
    }
    if (iRestart != 0 && result > bestResult) {
      result = bestResult;
      for (i = 0; i < control->Variables; i++)
        paramValue[i] = bestParamValue[i];
    } else {
      bestResult = result;
      for (i = 0; i < control->Variables; i++)
        bestParamValue[i] = paramValue[i];
    }
    if (nEval && param->verbose) {
      fprintf(stdout, "Result of optimization: %le\n", bestResult);
      fflush(stdout);
    }
    if (result <= target) {
      if (param->verbose) {
        fprintf(stdout, "result=%f, Target (%f) achieved, exiting optimization loop\n", result, target);
        fflush(stdout);
      }
      break;
    }
    if (iRestart != 0 && fabs(lastResult - result) <= readback->Tolerance) {
      if (param->verbose) {
        fprintf(stdout, "No change to within tolerance (%e), exiting optimization loop\n",
                readback->Tolerance);
        fflush(stdout);
      }
      break;
    }
    lastResult = result;
    if (simplexMinAbort(0) || rcdsMinAbort(0)) {
      if (param->verbose) {
        fprintf(stdout, "Simplex|rcds aborted, exiting optimization loop.\n");
        fflush(stdout);
      }
      break;
    }
    if (nRestartMax > 1 && param->verbose) {
      fprintf(stdout, "Performing restart %ld\n", iRestart + 1);
      fflush(stdout);
    }
  }
  if (param->verbose) {
    fprintf(stdout, "target=%e\n", target);
    fprintf(stdout, "The best parameters after %ld function evaluations obtained is:\n", nEval);
    fflush(stdout);
    for (i = 0; i < control->Variables; i++) {
      fprintf(stdout, "ControlName=%s   bestValue=%le \n", control->ControlName[i], paramValue[i]);
    }
    fflush(stdout);
  }
  if ((simplex && !simplexMinAbort(0)) || (rcds && !rcdsMinAbort(0))) {
    /*set the PVs to the best values */
    /*call localfunc() for the last time with the last result obtained */
    if (param->verbose) {
      fprintf(stdout, "Setting to best values.\n");
      fflush(stdout);
    }
    result = localfunc(paramValue, &invalid);
    if (param->verbose) {
      fprintf(stdout, "The final result is %f\n", result);
      fflush(stdout);
    }
  } else {
    if (!param->dryRun) {
      if (!param->varScript) {
        if (param->verbose) {
          fprintf(stdout, "Setting to best values.\n");
          fflush(stdout);
        }
        if (WritePVValues(control, paramValue) > 0)
          SDDS_Bomb("unable to set the PV values.");
      }
    } else {
      if (param->verbose) {
        fprintf(stdout, "pretending setting pvs...\n");
        fflush(stdout);
      }
    }
  }
  if (param->logFile) {
    if (!SDDS_UpdatePage(param->logData, 0) || !SDDS_Terminate(param->logData))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (restartFile && inputVarFile) {
    if (SDDS_SetColumnFromDoubles(&restartTable, SDDS_SET_BY_NAME, bestParamValue, control->Variables, "InitialValue") != 1)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_UpdatePage(&restartTable, 0) || !SDDS_Terminate(&restartTable))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  free(paramValue);
  free(paramDelta);
  free(bestParamValue);
  if (knobFiles) {
    for (i = 0; i < knobFiles; i++)
      free(KnobFile[i]);
    free(KnobFile);
  }
  free_scanargs(&s_arg, argc);

  if (inputMeasureFile)
    free(inputMeasureFile);
  if (inputVarFile)
    free(inputVarFile);
  if (restartFile)
    free(restartFile);
  FreeEverything(control, readback, param);
  free(control);
  free(readback);

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    strcpy(rcParam.message, "sddsoptimize completed normally.");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
    case RUNCONTROL_OK:
      rcParam.PV = NULL;
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
  if (param->verbose) {
    fprintf(stdout, "done.\n");
    fflush(stdout);
  }
  free(param);
  return 0;
}

long getVariablesData(CONTROL_NAME *control, char *inputFile, char *restartFile,
                      SDDS_TABLE *restartTable, long knobFiles, char **knobFile) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName;
  long i, pauseDefined = 0, code, varScriptExtraOptionDefined = 0, measScriptExtraOptionDefined = 0;
  short Initial = 0, disableExist = 0, HoldoffDefined, readbackColExist = 0, controlTolColExist = 0, controlTolParExist = 0, relativeLimitExist=0;

  ControlNameColumnName = NULL;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "controlPV", "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  switch (SDDS_CheckColumn(&inSet, "Disable", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    disableExist = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"Disable\".\n");
    exit(1);
    break;
  }

  
  pauseDefined = 0;
  switch (code = SDDS_CheckParameter(&inSet, "PauseAfterChange", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    pauseDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"PauseAfterChange\".\n");
    exit(1);
    break;
  }
  switch (code = SDDS_CheckParameter(&inSet, "pauseAfterChange", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    pauseDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"PauseAfterChange\".\n");
    exit(1);
    break;
  }

  if (SDDS_CheckColumn(&inSet, "RelativeLimit", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)==SDDS_CHECK_OK)
    relativeLimitExist=1;
  
  switch (SDDS_CheckColumn(&inSet, "LowerLimit", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"LowerLimit\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, "UpperLimit", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"UpperLimit\".\n");
    exit(1);
    break;
  }

  switch (SDDS_CheckColumn(&inSet, "InitialValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    Initial = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"InitialValue\".\n");
    exit(1);
    break;
  }

  switch (SDDS_CheckColumn(&inSet, "InitialChange", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"InitialChange\".\n");
    exit(1);
    break;
  }

  HoldoffDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "HoldoffTime", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    HoldoffDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"HoldoffTime\".\n");
    exit(1);
    break;
  }

  switch (SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    readbackColExist = 1;
    break;
  default:
    break;
  }
  if (readbackColExist && !control->controlTolProvided) {
    /*if ReadbackName column exist, and control tolerance not provided from commandline -varFile option */
    /*check if control tolerance par exist */
    switch (SDDS_CheckParameter(&inSet, "ControlTolerance", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      controlTolParExist = 1;
      break;
    }
    if (!controlTolParExist) {
      /*check if Tolerance column exist */
      switch (SDDS_CheckColumn(&inSet, "Tolerance", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
      case SDDS_CHECK_OKAY:
        controlTolColExist = 1;
        break;
      }
    }
    /*if control tolerance not provide by any, the default 0.05 will be used */
  }

  switch (code = SDDS_CheckParameter(&inSet, "varScriptExtraOption", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    varScriptExtraOptionDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"varScriptExtraOption\".\n");
    exit(1);
    break;
  }

  switch (code = SDDS_CheckParameter(&inSet, "measScriptExtraOption", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    measScriptExtraOptionDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
  default:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"measScriptExtraOption\".\n");
    exit(1);
    break;
  }

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;
  if (restartFile) {
    if (!SDDS_InitializeCopy(restartTable, &inSet, restartFile, "w"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!Initial) {
      if (SDDS_DefineColumn(restartTable, "InitialValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) == -1)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WriteLayout(restartTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_CopyPage(restartTable, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (pauseDefined) {
    if (!SDDS_GetParameter(&inSet, "PauseAfterChange", &(control->PauseAfterChange)) &&
        !SDDS_GetParameter(&inSet, "pauseAfterChange", &(control->PauseAfterChange))) {
      SDDS_SetError("PauseAfterChange parameter is missing");
      return 0;
    }
  }
  if (varScriptExtraOptionDefined) {
    if (!SDDS_GetParameterAsString(&inSet, "varScriptExtraOption", &(control->varScriptExtraOption))) {
      SDDS_SetError("varScriptExtraOption parameter is missing");
      return 0;
    }
  }
  if (measScriptExtraOptionDefined) {
    if (!SDDS_GetParameterAsString(&inSet, "measScriptExtraOption", &(control->measScriptExtraOption))) {
      SDDS_SetError("measScriptExtraOption parameter is missing");
      return 0;
    }
  }

  if (!(control->Variables = SDDS_CountRowsOfInterest(&inSet)))
    SDDS_Bomb("Zero rows defined in input file.\n");

  if (readbackColExist) {
    control->ReadbackName = (char **)SDDS_GetColumn(&inSet, "ReadbackName");
    if (controlTolParExist) {
      if (!SDDS_GetParameter(&inSet, "ControlTolerance", &(control->controlTol)))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (controlTolColExist) {
      if (!(control->tol = SDDS_GetColumnInDoubles(&inSet, "Tolerance")))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      control->tol = (double *)malloc(sizeof(*control->tol) * control->Variables);
      for (i = 0; i < control->Variables; i++)
        control->tol[i] = control->controlTol;
    }
    control->Readback = (double *)malloc(sizeof(*control->Readback) * control->Variables);
    for (i = 0; i < control->Variables; i++)
      control->Readback[i] = 0;
  }

  control->ControlName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (disableExist) {
    if (!(control->Disable = (short *)SDDS_GetColumn(&inSet, "Disable")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!(control->LowerLimit = SDDS_GetColumnInDoubles(&inSet, "LowerLimit")) ||
      !(control->UpperLimit = SDDS_GetColumnInDoubles(&inSet, "UpperLimit")) ||
      !(control->InitialChange = SDDS_GetColumnInDoubles(&inSet, "InitialChange")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (Initial) {
    if (!(control->InitialValue = SDDS_GetColumnInDoubles(&inSet, "InitialValue")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (HoldoffDefined &&
      !(control->HoldOffTime = SDDS_GetColumnInDoubles(&inSet, "HoldoffTime")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  control->RelativeLimit=NULL;
  if (relativeLimitExist)
    control->RelativeLimit= SDDS_GetColumnInDoubles(&inSet, "RelativeLimit");
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  SDDS_Terminate(&inSet);
  control->KnobFlag = (short *)calloc(control->Variables, sizeof(*(control->KnobFlag)));
  if (knobFiles)
    SetKnobArray(knobFile, knobFiles, control);
  control->ChannelID = malloc(sizeof(*(control->ChannelID)) * (control->Variables));
  if (control->ReadbackName)
    control->ReadbackID = malloc(sizeof(*(control->ChannelID)) * (control->Variables));
  if (!param->varScript) {
    for (i = 0; i < control->Variables; i++) {
      if (!control->KnobFlag[i]) {
        if (ca_search(control->ControlName[i], &(control->ChannelID[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", control->ControlName[i]);
          exit(1);
        }
        if (control->ReadbackName) {
          if (ca_search(control->ReadbackName[i], &(control->ReadbackID[i])) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for %s\n", control->ControlName[i]);
            exit(1);
          }
        }
      }
    }
    if (ca_pend_io(param->pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for variable channels\n");
      exit(1);
    }
  }
  return 1;
}

long getMeasurementData(READBACK_NAME *readback, char *inputFile, unsigned long mode,
                        short getreadbackUnits, double pendIOTime) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName;
  long i, pauseDefined = 0, toleranceDefined = 0, averageDefined = 0, weightDefined = 0;
  long desiredDefined = 0, type, readbackUnitExist = 0, readbackNameExist = 0;
  INFOBUF *info;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  switch (SDDS_CheckColumn(&inSet, "Weight", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    weightDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    weightDefined = 0;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"Weight\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    readbackNameExist = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    readbackNameExist = 0;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"ReadbackName\".\n");
    exit(1);
    break;
  }

  switch (SDDS_CheckColumn(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    readbackUnitExist = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    readbackUnitExist = 0;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"ReadbackUnits\".\n");
    exit(1);
    break;
  }
  pauseDefined = 0;
  switch (SDDS_CheckParameter(&inSet, "pauseBetweenReadings", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    pauseDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"pauseBetweenReadings\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&inSet, "PauseBetweenReadings", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    pauseDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    pauseDefined = 0;
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"pauseBetweenReadings\".\n");
    exit(1);
    break;
  }
  averageDefined = 0;
  switch (SDDS_CheckParameter(&inSet, "numberToAverage", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    averageDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"numberToAverage\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&inSet, "NumberToAverage", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    averageDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"numberToAverage\".\n");
    exit(1);
    break;
  }
  toleranceDefined = 0;
  switch (SDDS_CheckParameter(&inSet, "tolerance", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    toleranceDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"tolerance\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckParameter(&inSet, "Tolerance", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    toleranceDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with parameter \"tolerance\".\n");
    exit(1);
    break;
  }
  desiredDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "DesiredValue", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    desiredDefined += 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"DesiredValue\".\n");
    exit(1);
    break;
  }

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;
  if (pauseDefined) {
    if (!SDDS_GetParameter(&inSet, "PauseBetweenReadings", &(readback->Interval)) &&
        !SDDS_GetParameter(&inSet, "pauseBetweenReadings", &(readback->Interval))) {
      SDDS_SetError("error in getting pauseBetweenReading parameter");
      return 0;
    }
  }
  if (toleranceDefined) {
    if (!SDDS_GetParameter(&inSet, "Tolerance", &(readback->Tolerance)) &&
        !SDDS_GetParameter(&inSet, "tolerance", &(readback->Tolerance))) {
      SDDS_SetError("error in getting tolerance parameter");
      return 0;
    }
  }
  if (averageDefined) {
    if (!SDDS_GetParameterAsLong(&inSet, "NumberToAverage", &(readback->Averages)) &&
        !SDDS_GetParameterAsLong(&inSet, "numberToAverage", &(readback->Averages))) {
      SDDS_SetError("error in getting numberToAverage parameter");
      return 0;
    }
  } else {
    readback->Averages = 1;
  }
  if (!(readback->Variables = SDDS_CountRowsOfInterest(&inSet)))
    bomb("Zero rows defined in input file.\n", NULL);
  readback->ControlName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (readbackNameExist)
    readback->ReadbackName = (char **)SDDS_GetColumn(&inSet, "Readback");
  else
    readback->ReadbackName = NULL;

  if (desiredDefined) {
    if (!(readback->DesiredValue = SDDS_GetColumnInDoubles(&inSet, "DesiredValue"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
  } else
    readback->DesiredValue = calloc(readback->Variables, sizeof(*(readback->DesiredValue)));

  if (weightDefined) {
    if (!(readback->Weight = SDDS_GetColumnInDoubles(&inSet, "Weight"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
  } else {
    readback->Weight = (double *)malloc(sizeof(*(readback->Weight)) * readback->Variables);
    for (i = 0; i < readback->Variables; i++)
      readback->Weight[i] = 1.0;
  }
  readback->Value = (double *)calloc(readback->Variables, sizeof(*(readback->Value)));

  if (ControlNameColumnName)
    free(ControlNameColumnName);
  readback->ChannelID = (chid *)malloc(sizeof(chid) * (readback->Variables));
  for (i = 0; i < readback->Variables; i++) {
    /*readback->ChannelID[i] = NULL; */
    if (ca_search(readback->ControlName[i], &(readback->ChannelID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", readback->ControlName[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for measurement channels\n");
    exit(1);
  }
  if (getreadbackUnits) {
    if (!(mode & GET_UNITS_FORCED) && readbackUnitExist)
      readback->ReadbackUnits = (char **)SDDS_GetColumn(&inSet, "ReadbackUnits");
    else {
      readback->ReadbackUnits = (char **)malloc(sizeof(char *) * (readback->Variables));
      for (i = 0; i < readback->Variables; i++) {
        readback->ReadbackUnits[i] = (char *)malloc(sizeof(char) * (9));
        readback->ReadbackUnits[i][0] = 0;
      }
    }
    if (mode & GET_UNITS_FORCED || (mode & GET_UNITS_IF_NONE) || mode & GET_UNITS_IF_BLANK) {
      info = (INFOBUF *)malloc(sizeof(INFOBUF) * (readback->Variables));
      for (i = 0; i < readback->Variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank(readback->ReadbackUnits[i])) {
          if (ca_state(readback->ChannelID[i]) == cs_conn) {
            type = ca_field_type(readback->ChannelID[i]);
            if ((type == DBF_STRING) || (type == DBF_ENUM))
              continue;
            if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                             readback->ChannelID[i], &info[i]) != ECA_NORMAL) {
              fprintf(stderr, "error: unable to read units.\n");
              exit(1);
            }
          }
        }
      }
      if (ca_pend_io(pendIOTime) != ECA_NORMAL)
        fprintf(stderr, "error: problem reading units for measurement pvs.\n");
      for (i = 0; i < readback->Variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank(readback->ReadbackUnits[i])) {
          type = ca_field_type(readback->ChannelID[i]);
          if ((type == DBF_STRING) || (type == DBF_ENUM))
            continue;
          if (ca_state(readback->ChannelID[i]) == cs_conn) {
            free(readback->ReadbackUnits[i]);
            SDDS_CopyString(&(readback->ReadbackUnits[i]), info[i].c.units);
          }
        }
      }
      free(info);
    }
  }
  SDDS_Terminate(&inSet);
  return 1;
}

double localfunc(double *pValue, long *invalid) {
  int i, nondigit = 0, sciFlag = 0, plusFlag = 0, minusFlag = 0, length = 0, totalLength = 0, periodFlag = 0, caErrors = 0;
#ifdef _WIN32
  FILE *f_tmp, *f_tmp1;
  char *tmpComm;
  char *tmpfile;
#endif
  double sum = 0.0, ave = 0.0;
  FILE *handle;
  char message[1024];
  char *message1, *lmessage1, *comd, *setCommd;
  /*for (i=0;i<variables;i++)
    fprintf(stderr,"%s=%f,",PVName[i],pValue[i]);
    fprintf(stderr,"\n\n"); */
  message1 = lmessage1 = comd = NULL;
  handle = NULL;
  *invalid = 0;
#ifdef _WIN32
  tmpComm = NULL;
  tmpfile = NULL;
#endif

  if (!control->Variables || !control->ControlName) {
    fprintf(stderr, "no PV variables for setting or reading");
    *invalid = 1;
    exit(1);
  }
  if (param->varScript && !param->dryRun) {
    char valueStr[1024];
    if (param->verbose)
      fprintf(stdout, "running varScript...\n");
    setCommd = malloc(sizeof(char) * (strlen(param->varScript) + 1));
    strcpy(setCommd, param->varScript);
    for (i = 0; i < control->Variables; i++) {
      sprintf(valueStr, "%21.15e", pValue[i]);
      totalLength = strlen(valueStr) + strlen(setCommd) + 2;
      setCommd = SDDS_Realloc(setCommd, sizeof(char) * totalLength);
      strcat(setCommd, valueStr);
      if (i < control->Variables - 1)
        strcat(setCommd, " ");
      else
        strcat(setCommd, "\"");
    }
    setCommd = SDDS_Realloc(setCommd, sizeof(char) * (totalLength + 25));
#ifdef _WIN32
    cp_str(&tmpfile, tmpname(NULL));
    strcat(setCommd, " 2> ");
    strcat(setCommd, tmpfile);
    if (param->verbose) {
      fprintf(stdout, "The setting command is \"%s\"\n", setCommd);
      fflush(stdout);
    }
    system(setCommd);
    if (!(f_tmp1 = fopen(tmpfile, "r"))) {
      fclose(f_tmp1);
      SDDS_Bomb("Error, can not open tmpfile");
    }
    if (fgets(message, sizeof(message), f_tmp1)) {
      if (strlen(message) && wild_match(message, "*error*")) {
        *invalid = 1;
        fprintf(stderr, "error message: %s\n", message);
        SDDS_Bomb("Error occurred in varScript call");
      }
    }
    fclose(f_tmp1);
    tmpComm = malloc(sizeof(char) * (strlen(tmpfile) + 20));
    cp_str(&tmpComm, "rm -f ");
    strcat(tmpComm, tmpfile);
    system(tmpComm);
    free(tmpComm);
    free(tmpfile);
    tmpComm = tmpfile = NULL;
#else
    strcat(setCommd, " 2> /dev/stdout");
    if (param->verbose) {
      fprintf(stdout, "\nvarScript: %s\n", setCommd);
      fflush(stdout);
    }

    /*Ensure child script ignores signals intended for sddsoptimize */
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
#  ifndef _WIN32
    signal(SIGQUIT, SIG_IGN);
#  endif
    handle = popen(setCommd, "r");
    signal(SIGINT, abortSimplex);
    signal(SIGTERM, abortSimplex);
#  ifndef _WIN32
    signal(SIGQUIT, abortSimplex);
#  endif

    free(setCommd);
    if (handle == NULL) {
      *invalid = 1;
      SDDS_Bomb("Error: popen call failed");
    }
    if (!feof(handle)) {
      if (fgets(message, sizeof(message), handle)) {
        if (strlen(message) && wild_match(message, "*error*")) {
          *invalid = 1;
          fprintf(stderr, "error message from varScript: %s\n", message);
          SDDS_Bomb("Error occurred in varScript call");
        }
      }
    }
    pclose(handle);
#endif
#ifdef USE_RUNCONTROL
    if (control->PauseAfterChange > 0) {
      if (rcParam.PV) {
        runControlPingWhileSleep(control->PauseAfterChange);
      } else {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(control->PauseAfterChange);
      }
    }
#else
    if (control->PauseAfterChange > 0)
      ca_pend_event(control->PauseAfterChange);
#endif
  } else if (!param->dryRun) {
    if (WritePVValues(control, pValue) > 0) {
      *invalid = 1;
      exit(1);
    }

    /*check if readback reaches the setpoint*/
    if (control->ReadbackName) {
      short done, first = 1;
      while (1) {
        done = 1;
        caErrors = 0;
        for (i = 0; i < control->Variables; i++) {
	  if (control->KnobFlag[i])
	    continue;
          if (ca_get(DBR_DOUBLE, control->ReadbackID[i], &(control->Readback[i])) != ECA_NORMAL) {
            control->Readback[i] = pValue[i] + 2 * control->tol[i];
            caErrors++;
          }
        }
        if (caErrors) {
          fprintf(stderr, "Error reading pvs\n");
          exit(1);
        }
        if (ca_pend_io(param->pendIOTime) != ECA_NORMAL) {
          return control->Variables;
        }
        for (i = 0; i < control->Variables; i++) {
	  if (control->KnobFlag[i])
	    continue;
          if (fabs(control->Readback[i] - pValue[i]) > control->tol[i]) {
            if (param->verbose && first) {
              fprintf(stderr, "%s did not reach the setpoint, wait...\n", control->ReadbackName[i]);
              first = 0;
            }
            done = 0;
          }
        }
        if (done)
          break;
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          runControlPingWhileSleep(0.5);
        } else {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(0.5);
        }
#else
        ca_pend_event(0.5);
#endif
      }
    }
  } else {
    if (param->verbose) {
      fprintf(stdout, "Dry run mode, pretending changing variables...\n");
    }
  }
  /*conditioning after setting pvs*/
  if (!param->dryRun && param->conditioningCommand) {
    if (param->verbose) {
      fprintf(stdout, "start conditioning...)");
      fflush(stdout);
    }
    system(param->conditioningCommand);
  }
  RunTest(param->test);
  if (param->extra_pvs) {
    caErrors = 0;
    for (i = 0; i < param->extra_pvs; i++) {
      if (ca_get(DBR_DOUBLE, param->extra_pvID[i], &(param->extra_pvvalue[i])) != ECA_NORMAL) 
            caErrors++;
    }
    if (caErrors || ca_pend_io(param->pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "Error reading extra log pvs\n");
      exit(1);
    }
  }
  if (param->measScript) {
    comd = (char *)malloc(sizeof(*comd) * (strlen(param->measScript) + 25));
    strcpy(comd, param->measScript);
#ifdef _WIN32
    cp_str(&tmpfile, tmpname(NULL));
    strcat(comd, " 2> ");
    strcat(comd, tmpfile);
    if (param->verbose) {
      fprintf(stdout, "Calling measure script: %s\n", comd);
      fflush(stdout);
    }
    system(comd);
    if (!(f_tmp = fopen(tmpfile, "r"))) {
      fprintf(stderr, "Error in opening file %s.\n", tmpfile);
      exit(1);
    }
    if (!fgets(message, sizeof(message), f_tmp)) {
      *invalid = 1;
      fclose(f_tmp);
      SDDS_Bomb("Error: no data get from measure script file");
    }
    *invalid = 0;
    fclose(f_tmp);
    tmpComm = malloc(sizeof(char) * (strlen(tmpfile) + 20));
    cp_str(&tmpComm, "rm -f ");
    strcat(tmpComm, tmpfile);
    system(tmpComm);
    free(tmpComm);
    free(tmpfile);
    tmpComm = tmpfile = NULL;
#else
    strcat(comd, " 2> /dev/stdout");

    /* Ensure child script ignores signals intended for sddsoptimize */
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
#  ifndef _WIN32
    signal(SIGQUIT, SIG_IGN);
#  endif

    handle = popen(comd, "r");

    signal(SIGINT, abortSimplex);
    signal(SIGTERM, abortSimplex);
#  ifndef _WIN32
    signal(SIGQUIT, abortSimplex);
#  endif

    free(comd);
    if (handle == NULL) {
      *invalid = 1;
      SDDS_Bomb("Error: popen call failed");
    }
    if (feof(handle)) {
      *invalid = 1;
      SDDS_Bomb("Error: no data returned from popen call");
    }
    if (!fgets(message, sizeof(message), handle)) {
      pclose(handle);
      SDDS_Bomb("can not read data from popen call!\n");
    }
    pclose(handle);
#endif
    if (!(message1 = strtok(message, "\n")))
      SDDS_Bomb("empty output of the script!");
    length = strlen(message1);
    /*different script may output different combinations of uppercase and lowercase of
      "error", to catch all kinds of string "error": transfer the message1 string to 
      lowercase and then compare it with "error" */
    lmessage1 = malloc(sizeof(*lmessage1) * (length + 1));
    for (i = 0; i < length; i++)
      lmessage1[i] = tolower(message1[i]);
    lmessage1[length] = '\0';
    if (!strstr(lmessage1, "error")) {
      for (i = 0; i < strlen(message1); i++) {
        if (lmessage1[i] == 'e') {
          sciFlag++;
          continue;
        }
        if (lmessage1[i] == '-') { /*scientific notation might be -2.00e-3 */
          minusFlag++;
          continue;
        }
        if (lmessage1[i] == '+') { /* scientific notation might be 2.00e+3 */
          plusFlag++;
          continue;
        }
        if (lmessage1[i] == '.') { /* float numbers can contain periods like 2.00*/
          periodFlag++;
          continue;
        }
        if (!isdigit(lmessage1[i]))
          nondigit = 1; /*at least one character is not a digit or -, +, e, .*/
      }
      if (nondigit || (sciFlag > 1) || (minusFlag > 2) || (plusFlag > 1) || (periodFlag > 1)) {
        fprintf(stderr, "measScript output: %s\n", message);
        SDDS_Bomb("Unrecognized output from script");
        *invalid = 1;
        exit(1);
      }
      *invalid = 0;
      ave = strtod(message1, NULL);
      if (param->maximize)
        ave = -ave;
      free(lmessage1);
      param->count++;
      /*
        if (param->count>param->nEvalMax && simplex) {
        fprintf(stderr, "warning: maximum number of iterations exceeded---terminating.\n");
        simplexMinAbort(1);
        }
      */
    } else {
      fprintf(stderr, "script \"%s\" returns error message:\n", param->measScript);
      while (message1 != NULL) {
        fprintf(stderr, "%s\n", message1);
        message1 = strtok(NULL, "\n");
      }
      while (fgets(message, sizeof(message), handle) != NULL) {
        message1 = strtok(message, "\n");
        while (message1 != NULL) {
          fprintf(stderr, "%s\n", message1);
          message1 = strtok(NULL, "\n");
        }
      }
      *invalid = 1;
      pclose(handle);
      free(message1);
      exit(1);
    }
    
    /*run tests again after the script running*/
    if (param->verbose) {
      fprintf(stdout, "the script value is %le now\n", ave);
      fflush(stdout);
    }
    RunTest(param->test);
  }

  if (!param->measScript) {
    if (ReadAverageScalarValues(readback->ControlName, NULL, NULL, readback->Value, readback->Variables,
                                readback->Averages, readback->Interval,
                                readback->ChannelID, param->pendIOTime)) {
      *invalid = 1;
      exit(1);
    }
    for (i = 0; i < readback->Variables; i++)
      sum = sum + readback->Weight[i] * (readback->Value[i] - readback->DesiredValue[i]) * (readback->Value[i] - readback->DesiredValue[i]);
    ave = sqrt(sum / readback->Variables);
    *invalid = 0;
    if (param->maximize)
      ave = -ave;
    param->count++;
    if (param->verbose) {
      fprintf(stdout, "the average value is now %le\n", ave);
      fflush(stdout);
    }
    if (param->count > param->nEvalMax && simplex) {
      fprintf(stderr, "warning: maximum number of iterations exceeded---terminating.\n");
      simplexMinAbort(1);
      rcdsMinAbort(1);
    }
  }
  if (param->logFile) {
    if (param->count >= rowLimit && !SDDS_LengthenTable(param->logData, rowLimit += 1000))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_SetRowValues(param->logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, param->count - 1,
                           "currentValue", ave, "EvalIndex", param->count - 1,
                           "Time", getTimeInSecs(),
                           NULL))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < control->Variables; i++)
      if (!SDDS_SetRowValues(param->logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, param->count - 1,
                             control->ControlName[i], pValue[i], NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (param->test) {
      for (i = 0; i < param->test->n; i++) {
        if (!SDDS_SetRowValues(param->logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, param->count - 1,
                               param->test->controlName[i], param->test->value[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (param->extra_pvs) {
      for (i=0; i<param->extra_pvs;i++) {
	if (!SDDS_SetRowValues(param->logData, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, param->count - 1,
                               param->extra_pvname[i], param->extra_pvvalue[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (!SDDS_UpdatePage(param->logData, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  return ave;
}

void report(double y, double *x, long pass, long nEval, long n_dimen) {
  long i;
  fprintf(stdout, "pass %ld, after %ld evaluations: result = %.16e\na = ", pass, nEval, y);
  fflush(stdout);
  for (i = 0; i < n_dimen; i++)
    fprintf(stdout, "%.8e ", x[i]);
  fputc('\n', stdout);
  fflush(stdout);
}

void setupOutputFile(SDDS_DATASET *outTable, COMMON_PARAM *param, char *outputFile, char *extraLogFile) {
  SDDS_DATASET extraLog;
  int32_t i, names=0, cols=0;
  char **name, **column;
  if (!SDDS_InitializeOutput(outTable, SDDS_ASCII, 1, NULL, NULL, outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_SetNoRowCounts(outTable, 1);
  if (SDDS_DefineParameter(outTable, "variableFile", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineParameter(outTable, "measurementFile", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineParameter(outTable, "tolerance", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(outTable, "Time", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(outTable, "currentValue", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(outTable, "EvalIndex", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_DefineSimpleColumns(outTable, control->Variables, control->ControlName, NULL, SDDS_DOUBLE))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (param->test) {
    if (!SDDS_DefineSimpleColumns(outTable, param->test->n, param->test->controlName, NULL, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  
  if (extraLogFile) {
    if (!SDDS_InitializeInput(&extraLog, extraLogFile)) {
      fprintf(stderr, "Error reading extraLogFile\n");
      exit(1);
    }
    if (SDDS_ReadPage(&extraLog) <= 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    names=SDDS_CountRowsOfInterest(&extraLog);
    if (names> 0) {
      name = (char**)SDDS_GetColumn(&extraLog, "ControlName");
      column = (char**)SDDS_GetColumnNames(outTable, &cols);
      if (!SDDS_Terminate(&extraLog))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      param->extra_pvs=0;
      param->extra_pvname=NULL;
      param->extra_pvID=NULL;
      for (i=0; i<names;i++) {
	if (match_string(name[i], column, cols, EXACT_MATCH)<0) {
	  param->extra_pvname = SDDS_Realloc(param->extra_pvname, sizeof(*param->extra_pvname)*(param->extra_pvs+1));
	  param->extra_pvID = SDDS_Realloc(param->extra_pvID, sizeof(*param->extra_pvID)*(param->extra_pvs+1));
	  param->extra_pvvalue = SDDS_Realloc(param->extra_pvvalue, sizeof(*param->extra_pvvalue)*(param->extra_pvs+1));
	  SDDS_CopyString(&param->extra_pvname[param->extra_pvs], name[i]);
	  if (ca_search(param->extra_pvname[param->extra_pvs], &(param->extra_pvID[param->extra_pvs])) != ECA_NORMAL) {
	    fprintf(stderr, "error: problem doing search for %s\n", name[i]);
	    exit(1);
	  }
	  param->extra_pvs++;
	}
      }
      if (ca_pend_io(param->pendIOTime) != ECA_NORMAL)
        fprintf(stderr, "error: problem doing search for extra log pvs\n");
      SDDS_FreeStringArray(name, names);
      if (!SDDS_DefineSimpleColumns(outTable, param->extra_pvs, param->extra_pvname, NULL, SDDS_DOUBLE))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_WriteLayout(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

long SetKnobValue(KNOB *knob, double pValue) {
  double gain, value;
  long i, n, steps=0, stepSize=0, j;

  gain = knob->gain;
  n = knob->variables;
  if (param->verbose) {
    fprintf(stdout, "changing knob %s value\n", knob->knobName);
    fflush(stdout);
  }
  if (ReadScalarValues(knob->ControlPV, NULL, NULL, knob->values, n, knob->ControlPVchid, param->pendIOTime) > 0)
    SDDS_Bomb("problem reading present values of knob PV variables");
  if (!knob->stepSize || fabs(pValue - knob->value)<=knob->stepSize) {
    for (i = 0; i < n; i++) {
      knob->values[i] += gain * (pValue - knob->value) * (knob->Weight[i]);
      if (param->verbose) {
	fprintf(stdout, "%s     %lf\n", knob->ControlPV[i], knob->values[i]);
	fflush(stdout);
      }
    }
    if (WriteScalarValues(knob->ControlPV, knob->values, NULL, n, knob->ControlPVchid, param->pendIOTime)) {
      fprintf(stderr, "Problem in writing knob %s value.\n", knob->knobName);
      return 1;
    }
  } else {
    /*ramp the knobs with stepSize limit */
    steps = fabs(pValue - knob->value)/knob->stepSize+0.5;
    stepSize = (pValue - knob->value)/steps;
    for (j=0; j<steps; j++) {
      value = knob->value + (j+1)*stepSize;
      for (i=0; i<n; i++) {
	knob->values[i] += gain * value * (knob->Weight[i]);
	if (param->verbose) {
	  fprintf(stdout, "step %ld, %s     %lf\n", j+1, knob->ControlPV[i], knob->values[i]);
	}
      }
      if (WriteScalarValues(knob->ControlPV, knob->values, NULL, n, knob->ControlPVchid, param->pendIOTime)) {
	fprintf(stderr, "Problem in writing knob %s value.\n", knob->knobName);
	return 1;
      }
      /*wait pause time */
      ca_pend_event(knob->rampPause);
    }
  }
  
  knob->value = pValue;
  return 0;
}

long ReadPVValues(CONTROL_NAME *control) {
  long i, caErrors;

  caErrors = 0;
  if (!control->Variables)
    return 0;
  for (i = 0; i < control->Variables; i++) {
    if (!control->KnobFlag[i]) {
      if (ca_get(DBR_DOUBLE, control->ChannelID[i], control->InitialValue + i) != ECA_NORMAL) {
        control->InitialValue[i] = 0;
        caErrors++;
      }
    } else
      control->InitialValue[i] = 0;
  }
  if (ca_pend_io(param->pendIOTime) != ECA_NORMAL) {
    return control->Variables;
  }
  return caErrors;
}

long WritePVValues(CONTROL_NAME *control, double *pValue) {
  long caErrors = 0;
  long i, k;

  if (!control->Variables)
    return 0;

  for (i = 0; i < control->Knobs; i++) {
    k = control->Knob[i].pvIndex;
    if (SetKnobValue(&(control->Knob[i]), pValue[k]))
      SDDS_Bomb("Error occurs in setting knob values!");
  }
  for (i = 0; i < control->Variables; i++) {
    if (!control->KnobFlag[i]) {
      if (ca_put(DBR_DOUBLE, control->ChannelID[i], pValue + i) != ECA_NORMAL) {
        caErrors++;
      }
    }
  }
  if (ca_pend_io(param->pendIOTime) != ECA_NORMAL) {
    return 1;
  }
  /*wait for pauseAfterChange time */
  if (control->PauseAfterChange > 0) {
    /* Wait inside ca_end_event so the CA library does not disconnect us if we wait too long */
    ca_pend_event(control->PauseAfterChange);
  } else {
    ca_poll();
  }
  return caErrors;
}

long SetKnobArray(char **knobFile, long knobFiles, CONTROL_NAME *control) {
  SDDS_DATASET inSet;
  long i, matchIndex, j, variables, i_knob, k;
  char *knobName;
  double gain, stepSize=0, rampPause=2.0;
  short Weight = 0, Sector = 0;

  knobName = NULL;
  variables = 0;

  if (knobFiles <= 0)
    return 0;
  for (i = 0; i < knobFiles; i++) {
    if (!SDDS_InitializeInput(&inSet, knobFile[i])) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    while (SDDS_ReadPage(&inSet) > 0) {
      if (!(knobName = *(char **)SDDS_GetParameter(&inSet, "ControlName", NULL))) {
        if (param->verbose)
          fprintf(stderr, "Parameter ControlName does not exist in the knob file %s\n", knobFile[i]);
        break;
      }
      if ((matchIndex = match_string(knobName, control->ControlName, control->Variables, EXACT_MATCH)) < 0) {
        free(knobName);
        knobName = NULL;
        continue;
      }
      j = control->Knobs;
      control->KnobFlag[matchIndex] = 1;
      control->Knob = SDDS_Realloc(control->Knob, sizeof(*(control->Knob)) * (control->Knobs + 1));
      InitializeKnob(&control->Knob[j]);
      control->Knob[j].pvIndex = matchIndex;
      SDDS_CopyString(&(control->Knob[j].knobName), knobName);
      free(knobName);
      knobName = NULL;
      control->Knobs++;
      if (!SDDS_GetParameter(&inSet, "Gain", &gain)) {
        SDDS_SetError("the Gain parameter is missing, set it to 1");
        gain = 1;
      }
      control->Knob[j].gain = gain;
      if (SDDS_CheckParameter(&inSet, "StepSize", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)==SDDS_CHECK_OKAY) {
	if (!SDDS_GetParameter(&inSet, "StepSize", &stepSize)) {
	  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
	}
      }
      control->Knob[j].stepSize = stepSize;
      if (SDDS_CheckParameter(&inSet, "WaitTime", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)==SDDS_CHECK_OKAY) {
	if (!SDDS_GetParameter(&inSet, "WaitTime", &rampPause)) {
	  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
	}
      }
      control->Knob[j].rampPause = rampPause;
      switch (SDDS_CheckColumn(&inSet, "Weight", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
      case SDDS_CHECK_OKAY:
        Weight = 1;
        break;
      case SDDS_CHECK_NONEXISTENT:
        break;
      case SDDS_CHECK_WRONGTYPE:
      case SDDS_CHECK_WRONGUNITS:
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        fprintf(stderr, "Something wrong with column \"Weight\".\n");
        exit(1);
        break;
      }
      switch (SDDS_CheckColumn(&inSet, "Sector", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
      case SDDS_CHECK_OKAY:
        Sector = 1;
        break;
      case SDDS_CHECK_NONEXISTENT:
        break;
      case SDDS_CHECK_WRONGTYPE:
      case SDDS_CHECK_WRONGUNITS:
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        fprintf(stderr, "Something wrong with column \"Sector\".\n");
        exit(1);
        break;
      }
      if (!(variables = SDDS_CountRowsOfInterest(&inSet)))
        SDDS_Bomb("Zero rows defined in input file.\n");
      control->Knob[j].variables = variables;
      control->Knob[j].values = calloc(variables, sizeof(*(control->Knob[j].values)));
      control->Knob[j].ControlPV = (char **)SDDS_GetColumn(&inSet, "ControlName");
      if (!Weight) {
        if (!(control->Knob[j].Weight = malloc(sizeof(double) * variables)))
          SDDS_Bomb("memory allocation failure");
        for (k = 0; k < variables; k++)
          control->Knob[j].Weight[k] = 1.0;
      } else {
        if (!(control->Knob[j].Weight = SDDS_GetColumnInDoubles(&inSet, "Weight"))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(1);
        }
      }
      control->Knob[j].ControlPVchid = malloc(sizeof(*(control->Knob[j].ControlPVchid)) * variables);
      for (i_knob = 0; i_knob < variables; i_knob++) {
        if (ca_search(control->Knob[j].ControlPV[i_knob], &(control->Knob[j].ControlPVchid[i_knob])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", control->Knob[j].ControlPV[i_knob]);
          exit(1);
        }
      }
      if (ca_pend_io(param->pendIOTime) != ECA_NORMAL)
        fprintf(stderr, "error: problem doing search for knob pvs\n");
      if (Sector) {
        if (!(control->Knob[j].Sector = SDDS_GetColumnInLong(&inSet, "Sector"))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          exit(1);
        }
      }
    }
    SDDS_Terminate(&inSet);
  }
  return 0;
}

double ReadKnobValue(KNOB knob) {
  double *readValue, value = 0, gain;
  long i, n;
  n = knob.variables;
  gain = knob.gain;

  if (!(readValue = tmalloc(sizeof(*readValue) * n)))
    SDDS_Bomb("Memory allocation for reading knob values failed");
  if (ReadScalarValues(knob.ControlPV, NULL, NULL, readValue, n, knob.ControlPVchid, param->pendIOTime) > 0)
    SDDS_Bomb("problem reading present values of knob PV variables");
  for (i = 0; i < n; i++) {
    value += readValue[i] / gain / knob.Weight[i];
  }
  value = value / n;
  free(readValue);
  return value;
}

void setupTestsFile(TESTS *test) {
  SDDS_TABLE testValuesPage;
  long sleepTimeColumnPresent, resetTimeColumnPresent, i;

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
  if (param->verbose)
    fprintf(stdout, "number of test values = %ld\n", test->n);
  test->controlName = (char **)SDDS_GetColumn(&testValuesPage, "ControlName");
  test->min = SDDS_GetColumnInDoubles(&testValuesPage, "MinimumValue");
  test->max = SDDS_GetColumnInDoubles(&testValuesPage, "MaximumValue");
  if (sleepTimeColumnPresent)
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, "SleepTime");
  if (resetTimeColumnPresent)
    test->reset = SDDS_GetColumnInDoubles(&testValuesPage, "ResetTime");

  test->controlPVchid = (chid *)malloc(sizeof(chid) * test->n);
  for (i = 0; i < test->n; i++) {
    if (ca_search(test->controlName[i], &(test->controlPVchid[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", test->controlName[i]);
      exit(1);
    }
  }
  if (ca_pend_io(param->pendIOTime) != ECA_NORMAL)
    fprintf(stderr, "error: problem doing search for test channels\n");
  if (!SDDS_Terminate(&testValuesPage))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return;
}

void RunTest(TESTS *test) {
  double sleeptime;
  long outOfRange;
  if (!test->file)
    return;
  if (test->file) {
    test->count = 0;
    while (test->count < param->limit) {
      if (ReadScalarValues(test->controlName, NULL, NULL, test->value,
                           test->n, test->controlPVchid, param->pendIOTime) > 0)
        SDDS_Bomb("problem reading values of test variables");
      outOfRange = checkOutOfRange(test);
      sleeptime = MAX(test->longestSleep, 1);
      if (!outOfRange) { /*passes test*/
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          strcpy(rcParam.message, "Test passes!");
          if (param->verbose) {
            fprintf(stdout, "Test passes!\n");
            fflush(stdout);
          }
          rcParam.status = runControlLogMessage(rcParam.handle,
                                                rcParam.message,
                                                NO_ALARM,
                                                &(rcParam.rcInfo));
          if (rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            exit(1);
          }
        }
#endif
#ifdef USE_RUNCONTROL
        if (rcParam.PV) {
          runControlPingWhileSleep(rcParam.pingInterval);
        } else {
          if (sleeptime > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
            ca_pend_event(sleeptime);
          } else {
            ca_poll();
          }
        }
#else
        if (sleeptime > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(sleeptime);
        } else {
          ca_poll();
        }
#endif
        break;
      } else {         /*test fails*/
        test->count++; /*count fail times only */
        if (param->verbose) {
          fprintf(stdout, "test failed, sleep for %f seconds.\n", sleeptime);
          fflush(stdout);
        }
        /*if test variable is out of range, sleep  */
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
          runControlPingWhileSleep(sleeptime);
        } else {
          if (sleeptime > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
            ca_pend_event(sleeptime);
          } else {
            ca_poll();
          }
        }
#else
        if (sleeptime > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(sleeptime);
        } else {
          ca_poll();
        }
#endif
      }
    } /*end of while loop */
    if (test->count >= param->limit) {
      fprintf(stderr, "Tests failed for %" PRId32 " limited times, exit sddsoptimize!\n", param->limit);
      exit(1);
    }
  } else { /*no tests */
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      runControlPingWhileSleep(rcParam.pingInterval);
    } else {
      if (control->PauseAfterChange > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        fprintf(stderr, "Waiting %lf seconds\n", control->PauseAfterChange);
        ca_pend_event(control->PauseAfterChange);
      } else {
        ca_poll();
      }
    }
#else
    if (control->PauseAfterChange > 0) {
      /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
      ca_pend_event(control->PauseAfterChange);
    } else {
      ca_poll();
    }
#endif
  }
}

long checkOutOfRange(TESTS *test) {

  long i, outOfRange;

  outOfRange = 0; /* flag to indicate an out-of-range PV */
  test->longestSleep = 0;
  test->longestReset = 0;
  for (i = 0; i < test->n; i++) {
    if (test->value[i] < test->min[i] ||
        test->value[i] > test->max[i]) {
      test->outOfRange[i] = 1;
      outOfRange = 1;
      fprintf(stdout, "The test value of %s is out of range: %e\n", test->controlName[i], test->value[i]);
      fflush(stdout);
      /* determine longest time of those tests that failed */
      if (test->sleep)
        test->longestSleep = MAX(test->longestSleep, test->sleep[i]);
    } else {
      test->outOfRange[i] = 0;
    }
  }
  return outOfRange;
}

#ifdef USE_RUNCONTROL
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
    switch (rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control sddsoptimize aborted1.\n");
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control sddsoptimize timed out.\n");
      strcpy(rcParam.message, "Sddsoptimize timed-out");
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
    if ((MIN(timeLeft, rcParam.pingInterval)) > 0) {
      /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
      ca_pend_event(MIN(timeLeft, rcParam.pingInterval));
    } else {
      ca_poll();
    }
    timeLeft -= rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void interrupt_handler(int sig) {
  exit(1);
}

void abortSimplex(int sig) {
  if (!rcds)
    simplexMinAbort(1);
  else
    rcdsMinAbort(1);
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

void InitializeControlName(CONTROL_NAME *control) {
  control->Variables = 0;
  control->controlTolProvided = 0;
  control->controlTol = 0.05; /*default tolerance*/
  control->ControlName = control->ReadbackName = NULL;
  control->KnobFlag = NULL;
  control->ChannelID = control->ReadbackID = NULL;
  control->Value = control->tol = control->Readback = NULL;
  control->Knob = NULL;
  control->Knobs = 0;
  control->LowerLimit = NULL;
  control->UpperLimit = NULL;
  control->InitialChange = NULL;
  control->InitialValue = NULL;
  control->Disable = NULL;
  control->PauseAfterChange = 0.1;
  control->HoldOffTime = NULL;
  control->varScriptExtraOption = NULL;
  control->measScriptExtraOption = NULL;
  return;
}
void InitializeReadbackName(READBACK_NAME *readback) {
  readback->Variables = 0;
  readback->ControlName = NULL;
  readback->ChannelID = NULL;
  readback->Value = NULL;
  readback->DesiredValue = NULL;
  readback->Weight = NULL;
  readback->Interval = 1.0;
  readback->Averages = 1;
  readback->Tolerance = 0.01;
  return;
}

void InitializeKnob(KNOB *knob) {
  knob->knobName = NULL;
  knob->gain = 1.0;
  knob->variables = 0;
  knob->ControlPV = NULL;
  knob->ControlPVchid = NULL;
  knob->Weight = NULL;
  knob->Sector = NULL;
  knob->values = NULL;
  knob->value = 0;
  knob->stepSize = 0;
  knob->rampPause = 2.0;
}
void InitializeParam(COMMON_PARAM *param) {
  param->verbose = param->dryRun = 0;
  param->count = 0;
  param->maximize = 0;
  param->pendIOTime = 10.0;
  param->logFile = NULL;
  param->logData = NULL;
  param->nEvalMax = 100;
  param->varScript = param->measScript = NULL;
  param->conditioningCommand = NULL;
  param->extraLogFile = NULL;
  param->limit = 2;
  param->test = malloc(sizeof(*(param->test)));
  param->extra_pvs = 0;
  param->extra_pvname = NULL;
  param->extra_pvID = NULL;
  param->extra_pvvalue = NULL;
  InitializeTest(param->test);
}
 
void InitializeTest(TESTS *test) {
  test->file = NULL;
  test->n = 0;
  test->count = 0;
  test->outOfRange = NULL;
  test->controlName = NULL;
  test->controlPVchid = NULL;
  test->value = test->min = test->max = test->sleep = test->reset = NULL;
  test->longestSleep = 1.0;
  test->longestReset = 1.0;
}

void FreeEverything(CONTROL_NAME *control, READBACK_NAME *readback, COMMON_PARAM *param) {
  /*free control */
  long i, j;
  if (control->Variables) {
    for (i = 0; i < control->Variables; i++) {
      free(control->ControlName[i]);
      if (control->ReadbackName && control->ReadbackName[i])
        free(control->ReadbackName[i]);
    }
    if (control->Knobs) {
      for (i = 0; i < control->Knobs; i++) {
        if (control->Knob[i].knobName)
          free(control->Knob[i].knobName);
        for (j = 0; j < control->Knob[i].variables; j++)
          free(control->Knob[i].ControlPV[j]);
        if (control->Knob[i].ControlPV)
          free(control->Knob[i].ControlPV);
        if (control->Knob[i].ControlPVchid)
          free(control->Knob[i].ControlPVchid);
        if (control->Knob[i].Sector)
          free(control->Knob[i].Sector);
        if (control->Knob[i].values)
          free(control->Knob[i].values);
        if (control->Knob[i].Weight)
          free(control->Knob[i].Weight);
      }
      free(control->Knob);
    }
    if (control->ReadbackName)
      free(control->ReadbackName);
    if (control->ReadbackID)
      free(control->ReadbackID);
    if (control->Readback)
      free(control->Readback);
    if (control->tol)
      free(control->tol);

    if (control->KnobFlag)
      free(control->KnobFlag);
    if (control->ChannelID)
      free(control->ChannelID);
    if (control->ControlName)
      free(control->ControlName);
    if (control->Value)
      free(control->Value);
    if (control->LowerLimit)
      free(control->LowerLimit);
    if (control->UpperLimit)
      free(control->UpperLimit);
    if (control->InitialValue)
      free(control->InitialValue);
    if (control->InitialChange)
      free(control->InitialChange);
    if (control->Disable)
      free(control->Disable);
    if (control->HoldOffTime)
      free(control->HoldOffTime);
    if (control->varScriptExtraOption)
      free(control->varScriptExtraOption);
    if (control->measScriptExtraOption)
      free(control->measScriptExtraOption);
    if (control->RelativeLimit)
      free(control->RelativeLimit);
  }

  /*free readback */
  if (readback->Variables) {
    for (i = 0; i < readback->Variables; i++) {
      if (readback->ReadbackName && readback->ReadbackName[i])
        free(readback->ReadbackName[i]);
      if (readback->ReadbackUnits && readback->ReadbackUnits[i])
        free(readback->ReadbackUnits[i]);
      free(readback->ControlName[i]);
    }
    if (readback->ReadbackName)
      free(readback->ReadbackName);
    if (readback->ReadbackUnits)
      free(readback->ReadbackUnits);
    free(readback->ControlName);
    free(readback->ChannelID);
    free(readback->Value);
    free(readback->DesiredValue);
    if (readback->Weight)
      free(readback->Weight);
  }
  
  /*free param */
  if (param->varScript)
    free(param->varScript);
  if (param->measScript)
    free(param->measScript);
  if (param->conditioningCommand)
    free(param->conditioningCommand);
  if (param->logData)
    free(param->logData);
  if (param->logFile)
    free(param->logFile);
  if (param->test->file) {
    for (i = 0; i < param->test->n; i++)
      free(param->test->controlName[i]);
    free(param->test->controlName);
    free(param->test->controlPVchid);
    if (param->test->value)
      free(param->test->value);
    if (param->test->min)
      free(param->test->min);
    if (param->test->max)
      free(param->test->max);
    if (param->test->sleep)
      free(param->test->sleep);
    if (param->test->reset)
      free(param->test->reset);
    if (param->test->outOfRange)
      free(param->test->outOfRange);
    free(param->test->file);
  }
  if (param->extraLogFile) {
    if (param->extra_pvs) {
      free(param->extra_pvID);
      free(param->extra_pvvalue);
      SDDS_FreeStringArray(param->extra_pvname, param->extra_pvs);
    }
    free(param->extraLogFile);
  }
  free(param->test);
}
