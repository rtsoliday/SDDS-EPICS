/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <complex>
#include <iostream>
#include <ctime>
#include <alarm.h>

#include <cadef.h>
#include <epicsVersion.h>

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#if (EPICS_VERSION > 3)
#  include <pv/pvaClientMultiChannel.h>
#  include "pva/client.h"
#  include "pv/pvData.h"
#  include "pvaSDDS.h"
#endif

/* If you get an error compiling this next few lines it
   means that the number or elements in alarmSeverityString
   and/or alarmStatusString have to change to match those
   in EPICS/Base */
//#define COMPILE_TIME_ASSERT(expr) char constraint[expr]
//COMPILE_TIME_ASSERT(ALARM_NSEV == 4);
//COMPILE_TIME_ASSERT(ALARM_NSTATUS == 22);
const char *alarmSeverityString[] = {
  "NO_ALARM",
  "MINOR",
  "MAJOR",
  "INVALID"};
const char *alarmStatusString[] = {
  "NO_ALARM",
  "READ",
  "WRITE",
  "HIHI",
  "HIGH",
  "LOLO",
  "LOW",
  "STATE",
  "COS",
  "COMM",
  "TIMEOUT",
  "HWLIMIT",
  "CALC",
  "SCAN",
  "LINK",
  "SOFT",
  "BAD_SUB",
  "UDF",
  "DISABLE",
  "SIMM",
  "READ_ACCESS",
  "WRITE_ACCESS"};

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <libruncontrol.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2

#define DEFAULT_STEPS 10
#define DEFAULT_UPDATEINTERVAL 1
#define DEFAULT_TIME_INTERVAL 1.0

typedef struct
{
  unsigned long flags;
  char *controlName, *provider;
  char *expectFieldType;
  bool expectNumeric;
  int32_t expectElements;
  /* user's comma-separated list of severities/status for alarm conditions: */
  char severityNameList[200], *notSeverityNameList;
  char *statusNameList, *notStatusNameList;
  /* array of indices corresponding to severityNameList and notSeverityNameList */
  long *severityIndex, severityIndices;
  /* array of indices corresponding to statusNameList and notStatusNameList */
  long *statusIndex, statusIndices;
  double holdoff;
  short MajorAlarm, MinorAlarm, NoAlarm;
  short triggered, severity, status;        /* set by callback routine */
  long alarmParamIndex, severityParamIndex; /* parameter indices for SDDS output */
  chid channelID;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  long dataIndex, trigStep;
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that responds to this trigger*/
  /* trigStep: the Step where trigger occurs */
  /* parameterIndex: the parameterIndex in corresponding dataset */
  char *glitchScript;
} ALARMTRIG_REQUEST;

ALARMTRIG_REQUEST *alarmTrigRequest;
static long alarmTrigChannels;

#define ALARMTRIG_HOLDOFF 0x0001UL
#define ALARMTRIG_AUTOHOLD 0x0002UL
#define ALARMTRIG_CONTROLNAME 0x0004UL
#define ALARMTRIG_STATUSLIST 0x0008UL
#define ALARMTRIG_NOTSTATUSLIST 0x0010UL
#define ALARMTRIG_SEVERITYLIST 0x0020UL
#define ALARMTRIG_NOTSEVERITYLIST 0x0040UL

#define GLITCH_HOLDOFF 0x0001UL
#define GLITCH_AUTOHOLD 0x0002UL
#define GLITCH_BEFORE 0x0004UL
#define GLITCH_AFTER 0x0008UL
#define GLITCH_FRACTION 0x00010000UL
#define GLITCH_DELTA 0x00020000UL
#define GLITCH_BASELINE 0x00040000UL
#define GLITCH_MESSAGE 0x00080000UL
#define GLITCH_FILTERFRAC 0x00100000UL
#define GLITCH_NORESET 0x00200000UL

#define GLITCH_BEFORE_DEFAULT 100
#define GLITCH_AFTER_DEFAULT 1
#define GLITCH_BASELINE_DEFAULT 1

#define TRIGGER_HOLDOFF GLITCH_HOLDOFF
#define TRIGGER_AUTOHOLD GLITCH_AUTOHOLD
#define TRIGGER_BEFORE GLITCH_BEFORE
#define TRIGGER_AFTER GLITCH_AFTER
#define TRIGGER_LEVEL 0x00010000UL
#define TRIGGER_SLOPE 0x00020000UL
#define TRIGGER_MESSAGE 0x00040000UL
#define TRIGGER_AUTOARM 0x00080000UL
typedef struct
{
  unsigned long flags;
  char *controlName, *provider, *message;
  char *expectFieldType;
  bool expectNumeric;
  int32_t expectElements;
  double baselineValue, holdoff;
  double delta;
  short baselineAutoReset, glitchArmed;
  long parameterIndex, baselineSamples;
  long dataIndex, trigStep;
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that responds to this trigger*/
  /* trigStep: the Step where trigger occurs */
  /* parameterIndex: the parameterIndex in corresponding dataset */
  char *glitchScript;
} GLITCH_REQUEST;

typedef struct
{
  short triggerArmed;
  unsigned long flags;
  char *controlName, *provider, *message;
  char *expectFieldType;
  bool expectNumeric;
  int32_t expectElements;
  double value;
  short direction;
  /*direction=1, transition from below threshold to above threshold results in buffer dump */
  /*direction=-1, transition from above threshold to below threshold results in buffer dump */
  /*direction=0, ignore transition-based trigger for this PV */
  double holdoff, triggerLevel;
  long dataIndex, trigStep, parameterIndex;
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that responds to this trigger*/
  /* trigStep: the Step where trigger occurs */
  /* parameterIndex: the parameterIndex in corresponding dataset */
  char *glitchScript;
} TRIGGER_REQUEST;

/* GLTICH_REQUEST *glitchRequest;
   static long glitchChannels; */

#define TRIGGER_BEFORE_DEFAULT 100
#define TRIGGER_AFTER_DEFAULT 1
#define ALARMTRIGGER 0x0001UL
#define TRIGGER 0x0002UL
#define GLITCHTRIGGER 0x0004UL

typedef struct datanode {
  unsigned short hasdata;
  long step; /* fixed columns*/
  double ElapsedTime, EpochTime, TimeOfDay, DayOfMonth;
  double **values;
  long *elements;
  struct datanode *next;
} DATANODE, *PDATANODE;

DATANODE **glitchNode;

typedef struct
{
  SDDS_DATASET *dataset;
  char *outputFile;
  char *OutputRootname;
  char **ControlName, **ReadbackName, **ReadbackUnits;
  char **Provider, **ExpectFieldType;
  char *ExpectNumeric;
  int32_t *ExpectElements;
  char *TriggerControlName;
  short triggerType, triggerMode; /*ALARMTRIGGER, TRIGGER, or GLITCHTRIGGER */
  long variables;
  double **values;
  long *elements;
  double TriggerTime;
  long *ReadbackIndex, alarmIndex, glitchIndex, trigIndex;
  DATANODE *firstNode, *Node;
  PVA_OVERALL *pva;
} DATASETS;

/* DATASETS *datasets; */

#define NTimeUnitNames 4
static char *TimeUnitNames[NTimeUnitNames] = {
  (char *)"seconds",
  (char *)"minutes",
  (char *)"hours",
  (char *)"days",
};
static double TimeUnitFactor[NTimeUnitNames] = {
  1,
  60,
  3600,
  86400};

#define CLO_SAMPLEINTERVAL 0
#define CLO_CIRCULARBUFFER 1
#define CLO_HOLDOFFTIME 2
#define CLO_AUTOHOLDOFF 3
#define CLO_INHIBITPV 4
#define CLO_CONDITIONS 5
#define CLO_VERBOSE 6
#define CLO_TIME 7
#define CLO_TRIGGERFILE 8
#define CLO_LOCKFILE 9
#define CLO_WATCHINPUT 10
#define CLO_PENDIOTIME 11
#define CLO_RUNCONTROLPV 12
#define CLO_RUNCONTROLDESC 13
#define CLO_DELAY 14
#define COMMANDLINE_OPTIONS 15
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"sampleInterval",
  (char *)"circularBuffer",
  (char *)"holdoffTime",
  (char *)"autoHoldoff",
  (char *)"inhibitPV",
  (char *)"conditions",
  (char *)"verbose",
  (char *)"time",
  (char *)"triggerFile",
  (char *)"lockFile",
  (char *)"watchInput",
  (char *)"pendiotime",
  (char *)"runControlPV",
  (char *)"runControlDescription",
  (char *)"delay"};

static char *USAGE1 = (char *)"sddsglitchlogger <input> <outputDirectory>|<outputRootname>\n\
 [-triggerFile=<filename>] [-lockFile=<filename>[,verbose]]\n\
 [-sampleInterval=<secs>] [-time=<real-value>[,<time-units>]\n\
 [-circularBuffer=[before=<number>,][after=<number>]]\n\
 [-delay=<steps>] \
 [-holdoffTime=<seconds>] [-autoHoldoff] [-pendIOtime=<value>]\n\
 [-inhibitPV=name=<name>[,pendIOTime=<seconds>][,waitTime=<seconds>]]\n\
 [-conditions=<filename>,{allMustPass | oneMustPass}]\n\
 [-verbose] [-watchInput]\n\
 [-runControlPV=string=<string>,pingTimeout=<value>\n\
 [-runControlDescription=string=<string>]\n\n";
static char *USAGE2 = (char *)"<input>            the name of the input file\n\
                   the input file contains ControlName, ReadbackName, ReadbackUnits \n\
                   Provider, ExpectNumeric, ExpectFieldType and ExpectElements \n\
                   columns and it has following parameters: \n\
                   1. OutputRootname(string) -- the rootname for output file that log PVs \n\
                      defined in the input file. Different OutputRootmames go to different \n\
                      output files. If different pages have the same OutputRootname,\n\
                      the columns defined in new pages are ignored. Only the trigger \n\
                      defined in newer page is counted. \n\
                   2. GlitchScript(string) -- optional, provides the command to execute \n\
                      whenever a glitch occurs.\n\
                   a. TriggerControlName (string) -- PV to look at to determine\n\
                      if a glitch/trigger has occurred. \n\
                   b. MajorAlarm (short) -- if nonzero, then severity of MAJOR on \n\
                                            TriggerControlName, results in a buffer dump \n\
                   c. MinorAlarm (short) -- if nonzero, then severity of MINOR results in \n\
                                            a buffer dump. \n\
                   d. NoAlarm (short)    -- if nonzero, then severity of NO_ALARM results \n\
                                            in a buffer dump. \n\
                      if all of the above are zero, ignore alarm-based trigger. \n";
static char *USAGE3 = (char *)"                   e. TransitionDirection (short) --- \n\
                       -1  transition of TriggerControlName from above threshold to below \n\
                           threshold results in buffer dump \n\
                       0   ignore transition-based triggers for this PV \n\
                       1   transition from below threshold to above threshold results \n\
                           in buffer dump \n\
                   f. TransitionThreshold (double) --- See TransitionDirection. \n\
                   g. GlitchThreshold (double) --- \n\
                      0       ignore glitch-based triggers for this PV \n\
                      >0      absolute glitch level \n\
                     <0      -1*(fractional glitch level) \n\
                   h. GlitchBaselineSamples (short) --- number of samples to average to \n\
                      get the baseline value for glitch determination. \n\
                      A glitch occurs when newReading is different from the baseline \n\
                        by more than GlitchThreshold or (if GlitchThreshold<0) by  \n\
                        |GlitchThreshold|*baseline. \n\
                   i. GlitchBaselineAutoReset (short) --- \n\
                      Normally (if there is no glitch) the baseline is updated at each \n\
                      step using baseline -> (baseline*samples+newReading)/(samples+1). \n\
                      After a glitch, one  may want to do something different. \n\
                      1  After a glitch, the baseline is reassigned to its current value. \n\
                      0  The pre-glitch baseline is retained.  The sample that gave the  \n\
                         glitch is not averaged into the baseline. \n";
static char *USAGE4 = (char *)"<outputDirectory>  \n\
|<outputRootname>  the directory or rootname of the output file. When triggerFile option \n\
                   is given, <outputDirectory> is replaced by <outputRootname>, and there \n\
                   is only one output. \n\
triggerFile        when this option is given, the input file has no parameters, since the \n\
                   parameters are specified by the columns in triggerFile,i.e, triggerFile \n\
                   contains: TriggerControlName(string),MajorAlarm(short),MinorAlarm(short),\n\
                   NoAlarm(short), GlitchThreshold(double),GlitchBaselineSample(long),\n\
                   GlitchBaselineAutoReset(short),TransitionThreshold(double), \n\
                   TransitionDirection(short) etc. columns. \n\
                   It can have an optional column - GlitchScript, which will be execute when\n\
                   the corresponding glitch/trigger/alarm occurs.\n\
                   Additional columns are: Provider, ExpectNumeric, ExpectFieldType and ExpectElements.\n\
lockFile           when this option is given, sddsglitchlogger uses the named file to prevent\n\
                   running multiple versions of the program.  If the named file exists and is\n\
                   locked, the program exits.  If it does not exist or is not locked, it is\n\
                   created and locked.\n\
sampleInterval     desired interval in seconds for each reading step;\n\
time               Total time for mornitoring process variable or device.\n\
                   Valid time units are seconds,minutes,hours, or days. \n";
static char *USAGE5 = (char *)"verbose            prints out a message when data is taken.\n\
circularBuffer     Set how many samples to keep before and after the triggering event.\n\
holdoffTime        Set the number of seconds to wait after a trigger or alarm before accepting\n\
                   new triggers or alarms. \n\
autoHoldoff        Sets holdoff time so that the after-trigger samples are guaranteed to be \n\
                   collected before a new trigger or alarm is accept.\n\
pendIOtime         Sets the maximum time to wait for return of each value.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
inhibitPV          Checks this PV prior to each sample.  If the value is nonzero, then data\n\
                   collection is inhibited.  None of the conditions-related or other PVs are\n\
                   polled.\n\
delay              Used to delay reading PVs after the glitch or trigger occurs. This might be\n\
                   useful if the PVs are waveform PVs that acting as a circular buffer in the\n\
                   IOC.\n\
watchInput         If it is given, then the programs checks the input file to see if it is \n\
                   modified. If the inputfile is modified,then read the input files again and \n\
                   start the logging. \n\
runControlPV       specifies a runControl PV name.\n\
runControlDescription\n\
                   specifies a runControl PV description record.\n\n\
Program by M. Borland, H. Shang, R. Soliday ANL/APS\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;
RUNCONTROL_PARAM rcParam;

typedef struct
{
  DATASETS *datasets;
  GLITCH_REQUEST *glitchRequest;
  TRIGGER_REQUEST *triggerRequest;
  long triggerChannels, glitchChannels;
  char *triggerFile, *lockFile;
  short trigFileSet, lockFileVerbose;
  FILE *fpLock;
  int readCAvalues;
  long totalTimeSet;
  double TotalTime, quitingTime, PingTime;
  double TimeInterval;
  short *disconnected, *servicingTrigger, *writingPage, *severity, *tableStarted;
  long row, *pointsLeft;
  long numDatasets;
  char *inputfile, *outputDir;
  double HourNow, LastHour;
  char temp[1024];
  double StartTime, StartHour, StartJulianDay, StartDay, StartYear, StartMonth;
  double EpochTime, TimeOfDay, DayOfMonth;
  char *TimeStamp;
  long updateInterval;
  long lastConditionsFailed;
  long Step;
  double ElapsedTime;
  long CAerrors, verbose;
  long baselineStarted;
  char **triggerControlName, **triggerControlMessage;
  char **glitchControlName, **glitchControlMessage;
  double *glitchValue, *triggerValue;
  double **delayedGlitchValue, **delayedTriggerValue;
  double tmpValue;
  int delayedGlitchValuesInitialized, delayedTriggerValuesInitialized;
  short *glitched;  /* array of trigger states for glitch channels */
  short *triggered; /* array of trigger states for trigger channels */
  short *alarmed;   /* array of alarm states (0 or 1) for alarm trigger channels */
  int glitchDelay;
  int32_t glitchBefore, glitchAfter;
  long *trigBeforeCount;
  long bufferlength;
  double defaultHoldoffTime;
  long autoHoldoff;
  double pendIOtime;
  double InhibitPendIOTime;
  long inhibit;
  int32_t InhibitWaitTime;
  PV_VALUE InhibitPV;
  char *inhibitProvider;
  unsigned long CondMode;
  char **CondDeviceName, **CondProvider, **CondReadMessage, *CondFile;
  double *CondScaleFactor, *CondLowerLimit, *CondUpperLimit, *CondHoldoff;
  long CondChannels;
  double *CondDataBuffer;
  char **CondExpectFieldType;
  char *CondExpectNumeric;
  int32_t *CondExpectElements;
  char *trig_linkname;
  char *input_linkname;
  struct stat input_filestat;
  struct stat trig_filestat;
  int watchInput;
  long useArrays;
} LOGGER_DATA;

long IsGlitch(double value, double baseline, double delta); /*determine if PV is a glitch */
long IsTrigger(double previousValue, double currentValue, double level, short direction);
/*determine if PV is a trigger */
long DefineGlitchParameters(LOGGER_DATA *logger, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests, long dataIndex);

/*define the trigger parameter and trigger parameter index for dataset with index of dataIndex */

long *CSStringListsToIndexArray(long *arraySize, char *includeList,
                                char *excludeList, char **validItem, long validItems);
long SetupAlarmPVs(PVA_OVERALL *pvaAlarm, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigChannels);
/* the above three functions are copied from sddsmonitor.c */
long getGlitchLoggerData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels);

/*read the input file, assign values to datasets, glitchRequest,triggerRequest and alarmTrigRequest.
  for each trigger request, there is one corresponding dataset indexed by its dataIndex member.
  each page may have one alarm trigger, and/or one glitchTrigger, and/or regular trigger (the same
  PV may act as alarm, trigger or glitch).
  different pages in the input with same OutputRootname parameter go to one output file, that is,
  one outputfile (dataset) may respond to multiple triggers. 
  when triggerFile is given, the input only contains logger PV's information, the trigger's
  information is obtained through getTriggerData*/
long getTriggerData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels);

/*it is called only when triggerFile is given and set the values to triggers */

void startGlitchLogFiles(LOGGER_DATA *logger, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests);

void CheckTriggerAndFlushData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long alarmTrigChannels);
long ReadCAValues(LOGGER_DATA *logger, long index);

#define DEBUG 0

void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();
long pvaThreadSleep(long double seconds);
long PingRunControl();
long InitiateRunControl();
int ReadInputFiles(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels);
long PassesConditionsTest(LOGGER_DATA *logger, PVA_OVERALL *pvaCond);
long getConditionsDataInfo(LOGGER_DATA *logger);
bool PassesInhibitPVA(LOGGER_DATA *logger, PVA_OVERALL *pvaInhibit);
long SetupPVs(LOGGER_DATA *logger, PVA_OVERALL *pvaTrigger, PVA_OVERALL *pvaGlitch, PVA_OVERALL *pvaCond);
long SetupAlarmPVs(PVA_OVERALL *pvaAlarm, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigChannels);
long SetupDatasetPVs(LOGGER_DATA *logger);
long SetupInhibitPV(LOGGER_DATA *logger, PVA_OVERALL *pvaInhibit);
long ReadCommandLineArgs(LOGGER_DATA *logger, int argc, SCANNED_ARG *s_arg);
long CheckCommandLineArgsValidity(LOGGER_DATA *logger);
void InitializeVariables(LOGGER_DATA *logger);

volatile int sigint = 0;

#if defined(_WIN32)
#  include <thread>
#  include <chrono>
int nanosleep(const struct timespec *req, struct timespec *rem) {
  long double ldSeconds;
  long mSeconds;
  ldSeconds = req->tv_sec + req->tv_nsec;
  mSeconds = ldSeconds / 1000000;
  std::this_thread::sleep_for(std::chrono::microseconds(mSeconds));
  return (0);
}
#endif

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  long i, j, k;
  long iGlitch, iAlarm, iTrigger;
  alarmTrigRequest = NULL;

  PVA_OVERALL pva1, pva2, pva3, pva4, pva5;
  PVA_OVERALL *pvaTrigger=NULL, *pvaGlitch=NULL, *pvaInhibit, *pvaCond=NULL, *pvaAlarm;
  LOGGER_DATA logger;

  //Initialize variable values
  InitializeVariables(&logger);

  //Read command line arguments
  argc = scanargs(&s_arg, argc, argv);
  if (ReadCommandLineArgs(&logger, argc, s_arg) != 0) {
    return (1);
  }

  //Read Inhibit PV
  if (logger.inhibit) {
    pvaInhibit = &pva3;
    if (SetupInhibitPV(&logger, pvaInhibit) == 1) {
      return (1);
    }
    if (PassesInhibitPVA(&logger, pvaInhibit) == false) {
      if (logger.verbose) {
        printf("Inhibit PV %s is nonzero. Data collection inhibited.\n", logger.InhibitPV.name);
        fflush(stdout);
      }
      return (1);
    }
    freePVAGetReadings(pvaInhibit);
  }

  //Initiate run control
  if (InitiateRunControl() != 0) {
    return (1);
  }

  //Setup lock file
  if (logger.lockFile) {
    if (SDDS_FileIsLocked(logger.lockFile)) {
#if !defined(_WIN32)
      static char s[100];
      long pid;
      if (!(logger.fpLock = fopen(logger.lockFile, "r")))
        SDDS_Bomb((char *)"unable to open lock file for reading");
      if (!fgets(s, 100, logger.fpLock))
        SDDS_Bomb((char *)"unable to read pid number from lock file");
      if (strlen(s) > 0)
        s[strlen(s) - 1] = 0;
      pid = atol(s);
      if (!fgets(s, 100, logger.fpLock))
        SDDS_Bomb((char *)"unable to read owner in lock file");
      if (strlen(s) > 0)
        s[strlen(s) - 1] = 0;
      fclose(logger.fpLock);
      if (pid == 0)
        SDDS_Bomb((char *)"invalid pid number in lock file");
      /* check if this is a valid pid number 
         using kill with a signal value of 0 does this */
      if (kill(pid, 0) != 0) {
        if (remove(logger.lockFile) != 0)
          SDDS_Bomb((char *)"unable to delete orphaned lock file");
      } else {
        char buf[100] = {0};
        char command[100];
        FILE *p;
        snprintf(command, sizeof(command), "ps -o fname -p %ld | tail -1", pid);
        ;
        p = popen(command, "r");
        if (NULL == p) {
          perror("sddsglitchlogger");
          SDDS_Bomb((char *)"sddsglitchlogger: unexpected error");
        } else {
          if (NULL == fgets(buf, 100, p)) {
            if (0 != errno) {
              perror("sddsglitchlogger");
              SDDS_Bomb((char *)"sddsglitchlogger: unexpected error");
            }
          }
          pclose(p);
        }
        if (strlen(buf) > 0)
          buf[strlen(buf) - 1] = 0;
        if (strncmp(s, buf, 8) != 0) {
          if (remove(logger.lockFile) != 0)
            SDDS_Bomb((char *)"unable to delete orphaned lock file");
        } else {
#endif
          fprintf(stderr, "File %s is locked---previous process still running--aborting.\n", logger.lockFile);
          return (1);
#if !defined(_WIN32)
        }
      }
#endif
    }
    if (!(logger.fpLock = fopen(logger.lockFile, "w")))
      SDDS_Bomb((char *)"unable to open lock file");
    if (!SDDS_LockFile(logger.fpLock, logger.lockFile, "sddsglitchlogger"))
      SDDS_Bomb((char *)"unable to establish lock on lock file");
#if !defined(_WIN32)
    fprintf(logger.fpLock, "%ld\nsddsglitchlogger\n", (long)(getpid()));
    fflush(logger.fpLock);
#endif
    if (logger.lockFileVerbose)
      fprintf(stdout, "Lock established on %s\n", logger.lockFile);
  }

  //Setup output directory
  i = strlen(logger.outputDir);
  if (logger.trigFileSet) {
    if (logger.outputDir[i - 1] == '/') {
      fprintf(stderr, "the outputRootname should not end with '/'\n");
      return (1);
    }
  } else {
    if (logger.outputDir[i - 1] != '/') {
      logger.temp[0] = 0;
      strcat(logger.temp, logger.outputDir);
      strcat(logger.temp, "/");
      logger.outputDir = (char *)malloc(sizeof(char) * (strlen(logger.temp) + 1));
      logger.outputDir[0] = 0;
      strcat(logger.outputDir, logger.temp);
    }
    logger.temp[0] = 0;
  }

  //Get mtime for input files
  if (logger.watchInput) {
    logger.input_linkname = read_last_link_to_file(logger.inputfile);
    if (get_file_stat(logger.inputfile, logger.input_linkname, &(logger.input_filestat)) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", logger.inputfile);
      return (1);
    }
    if (logger.trigFileSet) {
      logger.trig_linkname = read_last_link_to_file(logger.triggerFile);
      if (get_file_stat(logger.triggerFile, logger.trig_linkname, &(logger.trig_filestat)) != 0) {
        fprintf(stderr, "Problem getting modification time for %s\n", logger.triggerFile);
        return (1);
      }
    }
  }

  //Read Input Files
  if (ReadInputFiles(&logger, &alarmTrigRequest, &alarmTrigChannels) != 0) {
    return (1);
  }

  //Setup connection to logged PVs
  if (SetupDatasetPVs(&logger) == 1) {
    return (1);
  }
  for (i = 0; i < logger.numDatasets; i++) {
    for (j = 0; j < logger.datasets[i].variables; j++) {
      if (logger.datasets[i].pva->isConnected[j]) {
        logger.datasets[i].elements[j] = logger.datasets[i].pva->pvaData[j].numGetElements;
      } else {
        logger.datasets[i].elements[j] = logger.datasets[i].ExpectElements[j];
      }
      if (!(logger.datasets[i].values[j] = (double *)malloc(sizeof(*(logger.datasets[i].values[j])) * logger.datasets[i].elements[j])))
        SDDS_Bomb((char *)"Memory allocation failure!");
    }
    freePVAGetReadings(logger.datasets[i].pva);
  }

  //Ping run control if in use
  if (PingRunControl() != 0) {
    return (1);
  }
  if (sigint)
    return (1);

  //Start output files
  startGlitchLogFiles(&logger, alarmTrigRequest, alarmTrigChannels);
  for (i = 0; i < logger.numDatasets; i++) {
    /*set disconnected to 1, since the datasets are disconnceted in startGlitchLogFiles procedure*/
    logger.disconnected[i] = 1;
  }
  glitchNode = (DATANODE **)malloc(sizeof(*glitchNode) * logger.numDatasets);

  if (!logger.glitchChannels && !alarmTrigChannels && !logger.triggerChannels) {
    for (i = 0; i < logger.numDatasets; i++) {
      if (logger.disconnected[i])
        if (!SDDS_ReconnectFile(logger.datasets[i].dataset))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (!SDDS_Terminate(logger.datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  } else {
    /* glitch/trigger and/or alarm trigger */
    /* make buffers*/
    logger.bufferlength = logger.glitchBefore + 1 + logger.glitchAfter;
    for (i = 0; i < logger.numDatasets; i++) {
      logger.datasets[i].Node = (DATANODE *)malloc(sizeof(DATANODE));
      logger.datasets[i].Node->values = (double **)malloc(sizeof(double *) * logger.datasets[i].variables);
      logger.datasets[i].Node->elements = (long *)malloc(sizeof(long) * logger.datasets[i].variables);
      for (k = 0; k < logger.datasets[i].variables; k++) {
        logger.datasets[i].Node->values[k] = (double *)malloc(sizeof(double) * logger.datasets[i].elements[k]);
        logger.datasets[i].Node->elements[k] = logger.datasets[i].elements[k];
      }
      logger.datasets[i].Node->hasdata = 0;
      logger.datasets[i].firstNode = logger.datasets[i].Node;
      for (j = 1; j < logger.bufferlength; j++) {
        logger.datasets[i].Node->next = (DATANODE *)malloc(sizeof(DATANODE));
        logger.datasets[i].Node = logger.datasets[i].Node->next;
        logger.datasets[i].Node->values = (double **)malloc(sizeof(double *) * logger.datasets[i].variables);
        logger.datasets[i].Node->elements = (long *)malloc(sizeof(long) * logger.datasets[i].variables);
        for (k = 0; k < logger.datasets[i].variables; k++) {
          logger.datasets[i].Node->values[k] = (double *)malloc(sizeof(double) * logger.datasets[i].elements[k]);
          logger.datasets[i].Node->elements[k] = logger.datasets[i].elements[k];
        }
        logger.datasets[i].Node->hasdata = 0;
      }
      logger.datasets[i].Node->next = logger.datasets[i].firstNode;
      logger.datasets[i].Node = logger.datasets[i].firstNode;
    }

    for (iTrigger = 0; iTrigger < logger.triggerChannels; iTrigger++)
      logger.triggerRequest[iTrigger].triggerArmed = 1;
    for (iGlitch = 0; iGlitch < logger.glitchChannels; iGlitch++)
      logger.glitchRequest[iGlitch].glitchArmed = 1;

    /* make copies of control-names and messages for convenient use with CA routine */
    if (!(logger.glitchControlName = (char **)malloc(sizeof(char *) * logger.glitchChannels)) ||
        !(logger.glitchControlMessage = (char **)malloc(sizeof(char *) * logger.glitchChannels)) ||
        !(logger.glitchValue = (double *)malloc(sizeof(double) * logger.glitchChannels)) ||
        !(logger.glitched = (short *)malloc(sizeof(short) * logger.glitchChannels)) ||
        !(logger.triggerControlName = (char **)malloc(sizeof(char *) * logger.triggerChannels)) ||
        !(logger.triggerControlMessage = (char **)malloc(sizeof(char *) * logger.triggerChannels)) ||
        !(logger.triggerValue = (double *)malloc(sizeof(double) * logger.triggerChannels)) ||
        !(logger.triggered = (short *)malloc(sizeof(short) * logger.triggerChannels)) ||
        !(logger.alarmed = (short *)malloc(sizeof(short) * alarmTrigChannels)) ||
        !(logger.severity = (short *)malloc(sizeof(short) * alarmTrigChannels)))
      SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
    if (!(logger.delayedGlitchValue = (double **)malloc(sizeof(*(logger.delayedGlitchValue)) * (logger.glitchDelay + 1))) ||
        !(logger.delayedTriggerValue = (double **)malloc(sizeof(*(logger.delayedTriggerValue)) * (logger.glitchDelay + 1))))
      SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
    for (i = 0; i <= logger.glitchDelay; i++) {
      if (!(logger.delayedGlitchValue[i] = (double *)malloc(sizeof(*(logger.delayedGlitchValue[i])) * logger.glitchChannels)))
        SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
    }
    for (i = 0; i <= logger.glitchDelay; i++) {
      if (!(logger.delayedTriggerValue[i] = (double *)malloc(sizeof(*(logger.delayedTriggerValue[i])) * logger.triggerChannels)))
        SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
    }
    for (iGlitch = 0; iGlitch < logger.glitchChannels; iGlitch++) {
      if (!SDDS_CopyString(logger.glitchControlName + iGlitch, logger.glitchRequest[iGlitch].controlName))
        SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
      if (logger.glitchRequest[iGlitch].message != NULL) {
        if (!SDDS_CopyString(logger.glitchControlMessage + iGlitch, logger.glitchRequest[iGlitch].message))
          SDDS_Bomb((char *)"memory allocation failure (glitch data copies)");
        else
          logger.glitchControlMessage[iGlitch][0] = 0;
      }
      logger.glitched[iGlitch] = 0;
    }
    for (iTrigger = 0; iTrigger < logger.triggerChannels; iTrigger++) {
      if (!SDDS_CopyString(logger.triggerControlName + iTrigger, logger.triggerRequest[iTrigger].controlName))
        SDDS_Bomb((char *)"memory allocation failure (trigger data copies)");
      if (logger.triggerRequest[iTrigger].message != NULL) {
        if (!SDDS_CopyString(logger.triggerControlMessage + iTrigger, logger.triggerRequest[iTrigger].message))
          SDDS_Bomb((char *)"memory allocation failure (trigger data copies)");
        else
          logger.triggerControlMessage[iTrigger][0] = 0;
      }
      logger.triggered[iTrigger] = 0;
    }
    for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
      logger.alarmed[iAlarm] = 0;
      logger.severity[iAlarm] = 0;
    }

    logger.lastConditionsFailed = 0;

    /*PVA CONNECIONTS HERE*/
    if (alarmTrigChannels) {
      pvaAlarm = &pva5;
      if (SetupAlarmPVs(pvaAlarm, alarmTrigRequest, alarmTrigChannels) == 1) {
        return (1);
      }
    }
    if (logger.triggerChannels > 0) {
      pvaTrigger = &pva1;
    }
    if (logger.glitchChannels > 0) {
      pvaGlitch = &pva2;
    }
    if (logger.CondChannels > 0) {
      pvaCond = &pva4;
    }
    if (SetupPVs(&logger, pvaTrigger, pvaGlitch, pvaCond) == 1) {
      return (1);
    }

    logger.quitingTime = getTimeInSecs() + logger.TotalTime; /*TotalTime is the input time duration */

    /************
     steps loop 
    ************/
    logger.Step = 0;
    /* old_datasets=numDatasets; */
    /* old_alarms=alarmTrigChannels; */
    /* old_glitchs=glitchChannels; */
    /* old_triggers=triggerChannels; */
    logger.PingTime = getTimeInSecs() - 2;
    logger.HourNow = getHourOfDay();
    for (i = 0; i < logger.numDatasets; i++)
      logger.datasets[i].TriggerTime = getTimeInSecs();

    //Main loop
    while (getTimeInSecs() <= logger.quitingTime) {
      logger.ElapsedTime = getTimeInSecs() - logger.StartTime;
      //Ping run control if in use
      if (PingRunControl() != 0) {
        return (1);
      }
      if (logger.Step) {
        if (pvaThreadSleep(logger.TimeInterval) == 1) {
          return (1);
        }
        if (sigint)
          return (1);
      }

      //Check for updated input file
      if (logger.watchInput) {
        if (file_is_modified(logger.inputfile, &(logger.input_linkname), &(logger.input_filestat))) {
          if (logger.verbose)
            fprintf(stdout, "The inputfile has been modified, exit!\n");
          break;
        }
        if (logger.trigFileSet) {
          if (file_is_modified(logger.triggerFile, &(logger.trig_linkname), &(logger.trig_filestat))) {
            if (logger.verbose)
              fprintf(stdout, "the trigger file has been modified,exit!\n");
            break;
          }
        }
      }
      logger.LastHour = logger.HourNow;
      logger.HourNow = getHourOfDay();
#if DEBUG
      if (fabs(logger.HourNow - logger.LastHour) > 0.167)
        fprintf(stderr, "%le seconds left\n", logger.quitingTime - getTimeInSecs());
#endif
      /* Subtract time interval from last hour to reduce problems with apparent clock
       * nonmonotonicity on Solaris.  I think this results from once-daily correction
       * of the clock.
       */
      //Handle new files if we start a new day
      if (logger.HourNow < (logger.LastHour - logger.TimeInterval / 3600.0)) {
#if DEBUG
        fprintf(stderr, "Starting new file: HourNow=%le, LastHour=%le\n",
                logger.HourNow, logger.LastHour);
#endif
        /* after midnight or input file is modified, must start new file */
        /* first,terminate the old files*/
        for (i = 0; i < logger.numDatasets; i++) {
          if (logger.disconnected[i]) {
            if (!SDDS_ReconnectFile(logger.datasets[i].dataset))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (!logger.tableStarted[i]) { /*actually, write empty page */
            if (!SDDS_StartPage(logger.datasets[i].dataset, logger.updateInterval))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            logger.useArrays = 0;
            for (j = 0; j < logger.datasets[i].variables; j++) {
              if ((logger.datasets[i].elements)[j] != 1) {
                logger.useArrays = 1;
                if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (logger.datasets[i].ReadbackName)[j], SDDS_CONTIGUOUS_DATA, NULL, 0, 0)) {
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              }
            }
            if (logger.useArrays) {
              if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"Step", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"CAerrors", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"Time", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"TimeOfDay", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"DayOfMonth", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if (!SDDS_WritePage(logger.datasets[i].dataset))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (!SDDS_Terminate(logger.datasets[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        /* then, start new files */
        /*1. generate file name */
        for (i = 0; i < logger.numDatasets; i++) {
          logger.disconnected[i] = 0;
          logger.servicingTrigger[i] = 0;
          logger.writingPage[i] = 0;
          logger.pointsLeft[i] = logger.glitchAfter;
          /*  trigBeforeCount[i]=0; */
          logger.tableStarted[i] = 0;
          logger.datasets[i].triggerMode = 0;
          snprintf(logger.temp, sizeof(logger.temp), "%s%s", logger.outputDir, logger.datasets[i].OutputRootname);
          snprintf(logger.temp, sizeof(logger.temp), "%s", MakeDailyGenerationFilename(logger.temp, 4, (char *)".", 0));
          logger.datasets[i].outputFile = (char *)tmalloc(sizeof(char) * (strlen(logger.temp) + 1));
          strcpy(logger.datasets[i].outputFile, logger.temp);
#if DEBUG
          fprintf(stderr, "New output file is %s\n", logger.datasets[i].outputFile);
#endif
          if (logger.verbose)
            fprintf(stdout, "output file is %s\n", logger.datasets[i].outputFile);
        }

        /*2. starts log file */
        startGlitchLogFiles(&logger, alarmTrigRequest, alarmTrigChannels);

        for (i = 0; i < logger.numDatasets; i++) {
          /*set disconnected to 1, since the datasets are disconnceted 
            in startGlitchLogFiles procedure*/
          logger.disconnected[i] = 1;
        }
      }

      //Read Inhibit PV
      if (logger.inhibit) {
        if (GetPVAValues(pvaInhibit) == 1) {
          return (1);
        }
        if (PassesInhibitPVA(&logger, pvaInhibit) == false) {
          if (logger.CondMode & TOUCH_OUTPUT)
            for (i = 0; i < logger.numDatasets; i++)
              TouchFile(logger.datasets[i].outputFile);
          if (logger.verbose) {
            printf("Inhibit PV %s is nonzero. Data collection inhibited.\n", logger.InhibitPV.name);
            fflush(stdout);
          }
          if (logger.Step > 0 && !logger.lastConditionsFailed) {
            if (logger.verbose) {
              fprintf(stdout, "Step %ld---values failed condition test,flush data \n", logger.Step);
              fflush(stdout);
            }
            for (i = 0; i < logger.numDatasets; i++) {
              /*the data of this node should not be written to file, 
                because its data has not been taken yet*/
              logger.datasets[i].Node->hasdata = 0;
            }
            logger.lastConditionsFailed = 1;
            CheckTriggerAndFlushData(&logger, &alarmTrigRequest, alarmTrigChannels);
          }
          if (logger.InhibitWaitTime > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
            if (pvaThreadSleep(logger.InhibitWaitTime) == 1) {
              return (1);
            }
            if (sigint)
              return (1);
          }
          freePVAGetReadings(pvaInhibit);
          continue;
        } else {
          freePVAGetReadings(pvaInhibit);
        }
      }
      //Read Condition PVs
      if (logger.CondChannels) {
        if (GetPVAValues(pvaCond) == 1) {
          return (1);
        }
        for (j = 0; j < pvaCond->numPVs; j++) {
          if (pvaCond->isConnected[j] == true) {
            logger.CondDataBuffer[j] = pvaCond->pvaData[j].getData[0].values[0];
          } else {
            logger.CondDataBuffer[j] = 0;
          }
        }
        if (!PassesConditionsTest(&logger, pvaCond)) {
          freePVAGetReadings(pvaCond);
          if (logger.CondMode & TOUCH_OUTPUT) {
            for (i = 0; i < logger.numDatasets; i++)
              TouchFile(logger.datasets[i].outputFile);
          }
          if (logger.Step > 0 && !logger.lastConditionsFailed) {
            if (logger.verbose) {
              fprintf(stdout, "Step %ld---values failed condition test,flush data \n", logger.Step);
              fflush(stdout);
            }
            for (i = 0; i < logger.numDatasets; i++) {
              /*the data of this node should not be written to file, because it is the data after testing*/
              logger.datasets[i].Node->hasdata = 0;
            }
            logger.lastConditionsFailed = 1;
            CheckTriggerAndFlushData(&logger, &alarmTrigRequest, alarmTrigChannels);
          }
          if (logger.verbose) {
            printf("Step %ld---values failed condition test--continuing\n", logger.Step);
            fflush(stdout);
          }
          if (!logger.Step) {
            if (pvaThreadSleep(logger.TimeInterval) == 1) {
              return (1);
            }
            if (sigint)
              return (1);
          }
          continue;
        } else {
          freePVAGetReadings(pvaCond);
        }
      }

      logger.lastConditionsFailed = 0;
      logger.CAerrors = 0;
      logger.ElapsedTime = (logger.EpochTime = getTimeInSecs()) - logger.StartTime;
      if (logger.verbose) {
        printf("Step %ld. Reading devices at %f seconds.\n", logger.Step, logger.ElapsedTime);
        fflush(stdout);
      }

      //Read Glitch PVs
      if (logger.glitchChannels) { /*read glitch trigger values */
        if (GetPVAValues(pvaGlitch) == 1) {
          return (1);
        }
        for (j = 0; j < pvaGlitch->numPVs; j++) {
          if (pvaGlitch->isConnected[j] == true) {
            logger.glitchValue[j] = pvaGlitch->pvaData[j].getData[0].values[0];
          } else {
            logger.glitchValue[j] = 0;
          }
        }
        logger.CAerrors += pvaGlitch->numNotConnected;
        freePVAGetReadings(pvaGlitch);
      }
      //Read Trigger PVs
      if (logger.triggerChannels) { /*read trigger values */
        if (GetPVAValues(pvaTrigger) == 1) {
          return (1);
        }
        for (j = 0; j < pvaTrigger->numPVs; j++) {
          if (pvaTrigger->isConnected[j] == true) {
            logger.triggerValue[j] = pvaTrigger->pvaData[j].getData[0].values[0];
          } else {
            logger.triggerValue[j] = 0;
          }
        }
        logger.CAerrors += pvaTrigger->numNotConnected;
        freePVAGetReadings(pvaTrigger);
      }
      //Read Alarm PVs
      if (alarmTrigChannels) { /*read alarm states */
        if (GetPVAValues(pvaAlarm) == 1) {
          return (1);
        }
        for (i = 0; i < pvaAlarm->numPVs; i++) {
          if (!pvaAlarm->isConnected[i]) {
            fprintf(stderr, "DEBUG Alarm severity = %d\n", pvaAlarm->pvaData[i].alarmSeverity);
            if (alarmTrigRequest[i].severity != pvaAlarm->pvaData[i].alarmSeverity) {
              alarmTrigRequest[i].severity = pvaAlarm->pvaData[i].alarmSeverity;
              alarmTrigRequest[i].triggered = 1;
            }
          }
        }
        logger.CAerrors += pvaAlarm->numNotConnected;
        freePVAGetReadings(pvaAlarm);
      }
      //If CA errors then restart loop
      if (logger.CAerrors) {
        logger.Step++;
        for (i = 0; i < logger.numDatasets; i++)
          logger.datasets[i].Node = logger.datasets[i].Node->next;
      }

      //Setup Glitch delay system
      if (logger.glitchChannels) {
        if (logger.delayedGlitchValuesInitialized) {
          for (j = 0; j < logger.glitchChannels; j++) {
            logger.tmpValue = logger.glitchValue[j];
            logger.glitchValue[j] = logger.delayedGlitchValue[0][j];
            for (i = 0; i < logger.glitchDelay; i++) {
              logger.delayedGlitchValue[i][j] = logger.delayedGlitchValue[i + 1][j];
            }
            logger.delayedGlitchValue[logger.glitchDelay][j] = logger.tmpValue;
          }
        } else {
          for (i = 0; i <= logger.glitchDelay; i++) {
            for (j = 0; j < logger.glitchChannels; j++) {
              logger.delayedGlitchValue[i][j] = logger.glitchValue[j];
            }
          }
          logger.delayedGlitchValuesInitialized = 1;
        }
      }
      //Setup Trigger delay system
      if (logger.triggerChannels) {
        if (logger.delayedTriggerValuesInitialized) {
          for (j = 0; j < logger.triggerChannels; j++) {
            logger.tmpValue = logger.triggerValue[j];
            logger.triggerValue[j] = logger.delayedTriggerValue[0][j];
            for (i = 0; i < logger.glitchDelay; i++) {
              logger.delayedTriggerValue[i][j] = logger.delayedTriggerValue[i + 1][j];
            }
            logger.delayedTriggerValue[logger.glitchDelay][j] = logger.tmpValue;
          }
        } else {
          for (i = 0; i <= logger.glitchDelay; i++) {
            for (j = 0; j < logger.triggerChannels; j++) {
              logger.delayedTriggerValue[i][j] = logger.triggerValue[j];
            }
          }
          logger.delayedTriggerValuesInitialized = 1;
        }
      }

      //Setup glitch baseline and trigger request values
      if (!logger.baselineStarted) {
        for (iGlitch = 0; iGlitch < logger.glitchChannels; iGlitch++)
          logger.glitchRequest[iGlitch].baselineValue = logger.glitchValue[iGlitch];
        for (iTrigger = 0; iTrigger < logger.triggerChannels; iTrigger++)
          logger.triggerRequest[iTrigger].value = logger.triggerValue[iTrigger];
        logger.baselineStarted = 1;
      }

      //Check Glitch values
      if (logger.glitchChannels) {
        for (iGlitch = 0; iGlitch < logger.glitchChannels; iGlitch++) {
          if (logger.verbose)
            printf("glitch quantity %s: baseline=%.2f delta=%.2f current=%.2f.\n",
                   logger.glitchRequest[iGlitch].controlName,
                   logger.glitchRequest[iGlitch].baselineValue,
                   logger.glitchRequest[iGlitch].delta,
                   logger.glitchValue[iGlitch]);

          i = logger.glitchRequest[iGlitch].dataIndex;
          if (logger.glitchRequest[iGlitch].glitchArmed) {
            if (IsGlitch(logger.glitchValue[iGlitch], logger.glitchRequest[iGlitch].baselineValue,
                         logger.glitchRequest[iGlitch].delta)) {
              /* logger.glitched[iGlitch] = 1; */
              /*ignore the other triggers while writing data*/
              if (!logger.writingPage[i] && logger.pointsLeft[i] == logger.glitchAfter) {
                logger.servicingTrigger[i] = 1;
                logger.glitchRequest[iGlitch].trigStep = logger.Step;
                logger.datasets[i].TriggerTime = logger.EpochTime;
                logger.datasets[i].triggerMode |= GLITCHTRIGGER;
                logger.datasets[i].glitchIndex = iGlitch;
                logger.glitched[iGlitch] = 1;
              }
              logger.glitchRequest[iGlitch].glitchArmed = 0;
              if (logger.verbose) {
                printf("Glitched on %s\n", logger.glitchRequest[iGlitch].controlName);
                fflush(stdout);
              }
              if (logger.glitchRequest[iGlitch].baselineAutoReset)
                logger.glitchRequest[iGlitch].baselineValue = logger.glitchValue[iGlitch];
            }
          } else
            logger.glitchRequest[iGlitch].baselineValue =
              (logger.glitchValue[iGlitch] + logger.glitchRequest[iGlitch].baselineValue *
               logger.glitchRequest[iGlitch].baselineSamples) /
              (logger.glitchRequest[iGlitch].baselineSamples + 1);
        }
        fflush(stdout);
      }
      //Check Trigger values
      if (logger.triggerChannels) {
        for (iTrigger = 0; iTrigger < logger.triggerChannels; iTrigger++) {
          if (logger.verbose)
            printf("Trigger %s: level=%.2f direction=%c previous=%.2f current=%.2f\n",
                   logger.triggerRequest[iTrigger].controlName,
                   logger.triggerRequest[iTrigger].triggerLevel,
                   (logger.triggerRequest[iTrigger].direction > 0) ? '+' : '-',
                   logger.triggerRequest[iTrigger].value, logger.triggerValue[iTrigger]);

          i = logger.triggerRequest[iTrigger].dataIndex;
          if (logger.triggerRequest[iTrigger].triggerArmed) {
            if (IsTrigger(logger.triggerRequest[iTrigger].value, logger.triggerValue[iTrigger],
                          logger.triggerRequest[iTrigger].triggerLevel,
                          logger.triggerRequest[iTrigger].direction)) {
              if (logger.verbose) {
                printf("Triggered on %s \n", logger.triggerRequest[iTrigger].controlName);
                fflush(stdout);
              }
              /* triggered[iTrigger]=1; */
              /*this trigger is active only when the data writing is done */
              if (!logger.writingPage[i] && logger.pointsLeft[i] == logger.glitchAfter) {
                logger.servicingTrigger[i] = 1;
                logger.triggerRequest[iTrigger].trigStep = logger.Step;
                logger.datasets[i].triggerMode |= TRIGGER;
                logger.datasets[i].TriggerTime = logger.EpochTime;
                logger.datasets[i].trigIndex = iTrigger;
                logger.triggered[iTrigger] = 1;
              }
              logger.triggerRequest[iTrigger].triggerArmed = 0;
            } else {
              /* check for trigger re-arm */
              if ((logger.triggerRequest[iTrigger].flags & TRIGGER_AUTOARM) ||
                  ((logger.triggerRequest[iTrigger].direction > 0 &&
                    logger.triggerValue[iTrigger] < logger.triggerRequest[iTrigger].triggerLevel) ||
                   (logger.triggerRequest[iTrigger].direction < 0 &&
                    logger.triggerValue[iTrigger] > logger.triggerRequest[iTrigger].triggerLevel))) {
                logger.triggerRequest[iTrigger].triggerArmed = 1;
                logger.triggered[iTrigger] = 0;
                if (logger.verbose) {
                  printf("Trigger armed on channel %s.\n", logger.triggerRequest[iTrigger].controlName);
                  fflush(stdout);
                }
              }
            }
          }
          logger.triggerRequest[iTrigger].value = logger.triggerValue[iTrigger];
        }
      }
      //Check Alarm statuses
      for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
        if (logger.verbose)
          printf("alarm quantity %s:  triggered=%d  severity=%d\n",
                 alarmTrigRequest[iAlarm].controlName,
                 alarmTrigRequest[iAlarm].triggered,
                 alarmTrigRequest[iAlarm].severity);
        i = alarmTrigRequest[iAlarm].dataIndex;
        if (alarmTrigRequest[iAlarm].triggered) {
          /*this trigger is active only when the data writing is done */
          if (!logger.writingPage[i] && logger.pointsLeft[i] == logger.glitchAfter) {
            logger.servicingTrigger[i] = 1;
            alarmTrigRequest[iAlarm].trigStep = logger.Step;
            logger.datasets[i].TriggerTime = logger.EpochTime;
            logger.datasets[i].triggerMode |= ALARMTRIGGER;
            logger.alarmed[iAlarm] = 1;
            logger.severity[iAlarm] = alarmTrigRequest[iAlarm].severity;
            logger.datasets[i].alarmIndex = iAlarm;
          }
          alarmTrigRequest[iAlarm].triggered = 0;
        }
      }

      //Read the values of PVs to be recorded
      logger.readCAvalues = 0;
      if (logger.glitchBefore > 0) {
        logger.readCAvalues = 1;
      } else {
        for (i = 0; i < logger.numDatasets; i++) {
          if (logger.servicingTrigger[i]) {
            logger.readCAvalues = 1;
            break;
          }
        }
      }
      if (logger.readCAvalues) {
        logger.TimeOfDay = logger.StartHour + logger.ElapsedTime / 3600.0;
        logger.DayOfMonth = logger.StartDay + logger.ElapsedTime / 3600.0 / 24.0;
        for (i = 0; i < logger.numDatasets; i++) {
          logger.CAerrors += ReadCAValues(&logger, i);
        }
        /* write values to node,i.e, store data in buffer */
        for (i = 0; i < logger.numDatasets; i++) {
          logger.datasets[i].Node->step = logger.Step;
          logger.datasets[i].Node->ElapsedTime = logger.ElapsedTime;
          logger.datasets[i].Node->EpochTime = logger.EpochTime;
          logger.datasets[i].Node->TimeOfDay = logger.TimeOfDay;
          logger.datasets[i].Node->DayOfMonth = logger.DayOfMonth;
          for (j = 0; j < logger.datasets[i].variables; j++) {
            for (k = 0; k < logger.datasets[i].elements[j]; k++) {
              logger.datasets[i].Node->values[j][k] = logger.datasets[i].values[j][k];
            }
          }
          logger.datasets[i].Node->hasdata = 1;
        }
      }

      CheckTriggerAndFlushData(&logger, &alarmTrigRequest, alarmTrigChannels);

      /* old_datasets=numDatasets; */
      /* old_alarms=alarmTrigChannels; */
      /* old_glitchs=glitchChannels; */
      /* old_triggers=triggerChannels; */
      logger.Step++;
      for (i = 0; i < logger.numDatasets; i++)
        logger.datasets[i].Node = logger.datasets[i].Node->next;
    } /* while (<=quitingTime)  */
  }   /* end of if (!glitChannels .. */

  for (i = 0; i < logger.numDatasets; i++) {
    if (logger.disconnected[i]) {
      if (!SDDS_ReconnectFile(logger.datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!logger.tableStarted[i]) {
      if (!SDDS_StartPage(logger.datasets[i].dataset, logger.updateInterval))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      logger.useArrays = 0;
      for (j = 0; j < logger.datasets[i].variables; j++) {
        if ((logger.datasets[i].elements)[j] != 1) {
          logger.useArrays = 1;
          if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (logger.datasets[i].ReadbackName)[j], SDDS_POINTER_ARRAY, NULL, 0, 0)) {
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
      }
      if (logger.useArrays) {
        if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"Step", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"CAerrors", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"Time", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"TimeOfDay", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(logger.datasets[i].dataset, (char *)"DayOfMonth", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      }

      if (!SDDS_WritePage(logger.datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }

    if (!SDDS_Terminate(logger.datasets[i].dataset))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    free(logger.datasets[i].dataset);
    logger.datasets[i].Node = logger.datasets[i].firstNode;
    for (j = 0; j < logger.bufferlength; j++) {
      for (k = 0; k < logger.datasets[i].variables; k++)
        free(logger.datasets[i].Node->values[k]);
      free(logger.datasets[i].Node->values);
      free(logger.datasets[i].Node->elements);
      logger.datasets[i].Node = logger.datasets[i].Node->next;
    }
    for (k = logger.bufferlength - 2; k >= 0; k--) {
      logger.datasets[i].Node = logger.datasets[i].firstNode;
      for (j = 0; j < logger.bufferlength; j++) {
        if (j == k) {
          if (logger.datasets[i].Node->next)
            free(logger.datasets[i].Node->next);
          break;
        }
        logger.datasets[i].Node = logger.datasets[i].Node->next;
      }
    }
    free(logger.datasets[i].firstNode);
    /* free(logger.datasets[i].Node);*/
    for (j = 0; j < logger.datasets[i].variables; j++) {
      free(logger.datasets[i].values[j]);
      free(logger.datasets[i].ControlName[j]);
      free(logger.datasets[i].ReadbackName[j]);
      free(logger.datasets[i].ReadbackUnits[j]);
      //free(logger.datasets[i].Provider[j]);
      free(logger.datasets[i].ExpectFieldType[j]);
    }
    free(logger.datasets[i].values);
    free(logger.datasets[i].elements);
    free(logger.datasets[i].ControlName);
    free(logger.datasets[i].ReadbackName);
    free(logger.datasets[i].ReadbackIndex);
    free(logger.datasets[i].ReadbackUnits);
    free(logger.datasets[i].Provider);
    free(logger.datasets[i].ExpectFieldType);
    free(logger.datasets[i].ExpectNumeric);
    free(logger.datasets[i].ExpectElements);
    free(logger.datasets[i].outputFile);
    freePVA(logger.datasets[i].pva);
    delete (logger.datasets[i].pva);
  }
  free(logger.datasets);
  if (logger.CondFile) {
    if (logger.CondDeviceName) {
      for (i = 0; i < logger.CondChannels; i++) {
        free(logger.CondDeviceName[i]);
      }
      free(logger.CondDeviceName);
    }
    if (logger.CondProvider) {
      for (i = 0; i < logger.CondChannels; i++) {
        free(logger.CondProvider[i]);
      }
      free(logger.CondProvider);
    }
    if (logger.CondExpectFieldType) {
      for (i = 0; i < logger.CondChannels; i++) {
        free(logger.CondExpectFieldType[i]);
      }
      free(logger.CondExpectFieldType);
    }
    if (logger.CondReadMessage) {
      for (i = 0; i < logger.CondChannels; i++) {
        free(logger.CondReadMessage[i]);
      }
      free(logger.CondReadMessage);
    }
    if (logger.CondScaleFactor)
      free(logger.CondScaleFactor);
    if (logger.CondUpperLimit)
      free(logger.CondUpperLimit);
    if (logger.CondLowerLimit)
      free(logger.CondLowerLimit);
    if (logger.CondHoldoff)
      free(logger.CondHoldoff);
    if (logger.CondDataBuffer)
      free(logger.CondDataBuffer);
    if (logger.CondExpectNumeric)
      free(logger.CondExpectNumeric);
    if (logger.CondExpectElements)
      free(logger.CondExpectElements);
  }
  if (logger.inputfile)
    free(logger.inputfile);
  if (logger.outputDir)
    free(logger.outputDir);
  if (pvaTrigger != NULL)
    freePVA(pvaTrigger);
  if (pvaGlitch != NULL)
    freePVA(pvaGlitch);
  free_scanargs(&s_arg, argc);

  return 0;
}

void InitializeVariables(LOGGER_DATA *logger) {
  rcParam.PV = NULL;
  rcParam.Desc = NULL;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
  rcParam.pingInterval = 2;

  logger->triggerChannels = 0;
  logger->glitchChannels = 0;
  logger->triggerFile = NULL;
  logger->lockFile = NULL;
  logger->trigFileSet = 0;
  logger->lockFileVerbose = 0;
  logger->fpLock = NULL;

  logger->readCAvalues = 1;
  logger->totalTimeSet = 0;
  logger->TimeInterval = DEFAULT_TIME_INTERVAL;

  logger->severity = NULL;
  logger->row = 0;

  logger->numDatasets = 0;
  logger->inputfile = NULL;
  logger->outputDir = NULL;

  logger->updateInterval = DEFAULT_UPDATEINTERVAL;
  logger->lastConditionsFailed = 0;
  logger->CAerrors = 0;
  logger->verbose = 0;

  logger->baselineStarted = 0;
  logger->triggerControlName = NULL;
  logger->triggerControlMessage = NULL;
  logger->glitchControlName = NULL;
  logger->glitchControlMessage = NULL;
  logger->glitchValue = NULL;
  logger->triggerValue = NULL;
  logger->delayedGlitchValue = NULL;
  logger->delayedTriggerValue = NULL;
  logger->delayedGlitchValuesInitialized = 0;
  logger->delayedTriggerValuesInitialized = 0;
  logger->glitched = NULL;  /* array of trigger states for glitch channels */
  logger->triggered = NULL; /* array of trigger states for trigger channels */
  logger->alarmed = NULL;   /* array of alarm states (0 or 1) for alarm trigger channels */

  logger->glitchDelay = 0;
  logger->bufferlength = 0;
  logger->defaultHoldoffTime = 0;
  logger->autoHoldoff = 0;
  logger->pendIOtime = 10.0;
  logger->InhibitPendIOTime = 10;
  logger->inhibit = 0;
  logger->InhibitWaitTime = 5;
  logger->inhibitProvider = NULL;
  logger->CondChannels = 0;
  logger->CondMode = 0;
  logger->CondDeviceName = NULL;
  logger->CondProvider = NULL;
  logger->CondReadMessage = NULL;
  logger->CondFile = NULL;
  logger->CondScaleFactor = NULL;
  logger->CondLowerLimit = NULL;
  logger->CondUpperLimit = NULL;
  logger->CondHoldoff = NULL;
  logger->CondDataBuffer = NULL;
  logger->CondExpectFieldType = NULL;
  logger->CondExpectNumeric = NULL;
  logger->CondExpectElements = NULL;

  logger->trig_linkname = NULL;
  logger->input_linkname = NULL;
  logger->watchInput = 0;

  logger->glitchBefore = GLITCH_BEFORE_DEFAULT;
  logger->glitchAfter = GLITCH_AFTER_DEFAULT;
}

long CheckCommandLineArgsValidity(LOGGER_DATA *logger) {
  if (!logger->inputfile) {
    fprintf(stderr, "input filename not given\n");
    return (1);
  }
  if (!logger->outputDir) {
    fprintf(stderr, "output directory not given\n");
    return (1);
  }
  if (!logger->totalTimeSet) {
    fprintf(stderr, "the time is not given\n");
    return (1);
  }

  return (0);
}

long ReadCommandLineArgs(LOGGER_DATA *logger, int argc, SCANNED_ARG *s_arg) {
  long i_arg, optionCode;
  long TimeUnits;
  unsigned long dummyFlags;

  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5);
    return (1);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (optionCode = match_string(s_arg[i_arg].list[0],
                                        commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &(logger->pendIOtime)) != 1 ||
            logger->pendIOtime <= 0) {
          fprintf(stderr, "invalid -pendIOtime syntax\n");
          return (1);
        }
        break;
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&(logger->TimeInterval), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -sampleInterval\n");
          return (1);
        }
        break;
      case CLO_CIRCULARBUFFER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "before", SDDS_LONG, &(logger->glitchBefore), 1, 0,
                          "after", SDDS_LONG, &(logger->glitchAfter), 1, 0,
                          NULL) ||
            logger->glitchBefore < 0 || logger->glitchAfter < 0) {
          fprintf(stderr, "invalid -circularBuffer syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DELAY:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%d", &(logger->glitchDelay)) != 1 ||
            logger->glitchDelay < 0) {
          fprintf(stderr, "invalid -glitchDelay syntax/value\n");
          return (1);
        }
        break;
      case CLO_HOLDOFFTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &(logger->defaultHoldoffTime)) != 1 ||
            logger->defaultHoldoffTime < 0) {
          fprintf(stderr, "invalid -holdoffTime syntax/value\n");
          return (1);
        }
        break;
      case CLO_AUTOHOLDOFF:
        logger->autoHoldoff = 1;
        break;
      case CLO_INHIBITPV:
        logger->InhibitPendIOTime = 10;
        logger->InhibitWaitTime = 5;
        logger->inhibitProvider = (char *)malloc(sizeof(char) * 5);
        logger->inhibitProvider = (char *)"ca";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &(logger->InhibitPV.name), 1, 0,
                          "pendiotime", SDDS_DOUBLE, &(logger->InhibitPendIOTime), 1, 0,
                          "waittime", SDDS_LONG, &(logger->InhibitWaitTime), 1, 0,
                          NULL) ||
            !logger->InhibitPV.name || !strlen(logger->InhibitPV.name) ||
            logger->InhibitPendIOTime <= 0 || logger->InhibitWaitTime <= 0) {
          fprintf(stderr, "invalid -inhibitPV syntax/values\n");
          return (1);
        }
        logger->inhibit = 1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "invalid -conditions syntax/values\n");
          return (1);
        }
        SDDS_CopyString(&(logger->CondFile), s_arg[i_arg].list[1]);
        if (SDDS_StringIsBlank(logger->CondFile) ||
            !(logger->CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2))) {
          fprintf(stderr, "invalid -conditions syntax/values\n");
          return (1);
        }
        break;
      case CLO_VERBOSE:
        logger->verbose = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&(logger->TotalTime), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -time\n");
          return (1);
        }
        logger->totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            logger->TotalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_TRIGGERFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "invalid syntax for -triggerFile\n");
          return (1);
        }
        SDDS_CopyString(&(logger->triggerFile), s_arg[i_arg].list[1]);
        logger->trigFileSet = 1;
        break;
      case CLO_LOCKFILE:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3) {
          fprintf(stderr, "invalid syntax for -lockFile\n");
          return (1);
        }
        SDDS_CopyString(&(logger->lockFile), s_arg[i_arg].list[1]);
        if (s_arg[i_arg].n_items == 3) {
          str_tolower(s_arg[i_arg].list[2]);
          if (strncmp(s_arg[i_arg].list[2], "verbose",
                      strlen(s_arg[i_arg].list[2])) != 0) {
            fprintf(stderr, "invalid syntax for -lockFile\n");
            return (1);
          }
          logger->lockFileVerbose = 1;
        }
        break;
      case CLO_WATCHINPUT:
        logger->watchInput = 1;
        break;
      case CLO_RUNCONTROLPV:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PV)) {
          fprintf(stderr, "bad -runControlPV syntax\n");
          return (1);
        }
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_RUNCONTROLDESC:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc)) {
          fprintf(stderr, "bad -runControlDesc syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (!(logger->inputfile))
        SDDS_CopyString(&(logger->inputfile), s_arg[i_arg].list[0]);
      else if (!logger->outputDir)
        SDDS_CopyString(&(logger->outputDir), s_arg[i_arg].list[0]);
      else {
        fprintf(stderr, "too many filenames given\n");
        return (1);
      }
    }
  }

  //Check if command line options given make sense
  if (CheckCommandLineArgsValidity(logger) != 0) {
    return (1);
  }

  return (0);
}

long SetupInhibitPV(LOGGER_DATA *logger, PVA_OVERALL *pvaInhibit) {
  if (logger->verbose) {
    fprintf(stdout, "Connecting to Inhibit PV\n");
  }
  if (pvaInhibit != NULL) {
    allocPVA(pvaInhibit, 1);
    epics::pvData::shared_vector<std::string> inhibitNames(1);
    epics::pvData::shared_vector<std::string> inhibitProviders(1);
    inhibitNames[0] = logger->InhibitPV.name;
    inhibitProviders[0] = logger->inhibitProvider;
    pvaInhibit->pvaChannelNames = freeze(inhibitNames);
    pvaInhibit->pvaProvider = freeze(inhibitProviders);
    ConnectPVA(pvaInhibit, logger->InhibitPendIOTime);
    pvaInhibit->limitGetReadings = true;
    if (!pvaInhibit->isConnected[0]) {
      fprintf(stderr, "Error1: Problem doing search for %s\n", logger->InhibitPV.name);
      return (1);
    }
    //Get initial values
    if (GetPVAValues(pvaInhibit) == 1) {
      return (1);
    }
  }
  return (0);
}

long SetupDatasetPVs(LOGGER_DATA *logger) {
  long i, j;
  if (logger->verbose) {
    fprintf(stdout, "Connecting to logged PVs\n");
  }
  for (i = 0; i < logger->numDatasets; i++) {
    allocPVA(logger->datasets[i].pva, logger->datasets[i].variables);
    epics::pvData::shared_vector<std::string> datasetNames(logger->datasets[i].variables);
    epics::pvData::shared_vector<std::string> datasetProviders(logger->datasets[i].variables);
    for (j = 0; j < logger->datasets[i].variables; j++) {
      datasetNames[j] = logger->datasets[i].ControlName[j];
      datasetProviders[j] = logger->datasets[i].Provider[j];
    }
    logger->datasets[i].pva->pvaChannelNames = freeze(datasetNames);
    logger->datasets[i].pva->pvaProvider = freeze(datasetProviders);
    ConnectPVA(logger->datasets[i].pva, logger->pendIOtime);
    logger->datasets[i].pva->limitGetReadings = true;
    for (j = 0; j < logger->datasets[i].pva->numPVs; j++) {
      if (!logger->datasets[i].pva->isConnected[j]) {
        fprintf(stderr, "Error2: Problem doing search for %s\n", logger->datasets[i].ControlName[j]);
        //return (1);
      }
    }
    //Get initial values
    if (GetPVAValues(logger->datasets[i].pva) == 1) {
      return (1);
    }
  }
  return (0);
}

long SetupAlarmPVs(PVA_OVERALL *pvaAlarm, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigChannels) {
  long i;
  for (i = 0; i < alarmTrigChannels; i++) {
    alarmTrigRequest[i].severity = -1;
    alarmTrigRequest[i].triggered = 0;
  }
  allocPVA(pvaAlarm, alarmTrigChannels);
  epics::pvData::shared_vector<std::string> alarmNames(alarmTrigChannels);
  epics::pvData::shared_vector<std::string> alarmProviders(alarmTrigChannels);
  for (i = 0; i < alarmTrigChannels; i++) {
    alarmNames[i] = alarmTrigRequest[i].controlName;
    alarmProviders[i] = alarmTrigRequest[i].provider;
  }
  pvaAlarm->pvaChannelNames = freeze(alarmNames);
  pvaAlarm->pvaProvider = freeze(alarmProviders);
  ConnectPVA(pvaAlarm, 10.0);
  pvaAlarm->limitGetReadings = true;
  pvaAlarm->includeAlarmSeverity = true;
  for (i = 0; i < pvaAlarm->numPVs; i++) {
    if (!pvaAlarm->isConnected[i]) {
      fprintf(stderr, "Error3: Problem doing search for %s\n", alarmTrigRequest[i].controlName);
      return (1);
    }
  }
  //Get initial values
  if (GetPVAValues(pvaAlarm) == 1) {
    return (1);
  }
  for (i = 0; i < pvaAlarm->numPVs; i++) {
    if (!pvaAlarm->isConnected[i]) {
      alarmTrigRequest[i].severity = pvaAlarm->pvaData[i].alarmSeverity;
    }
  }
  freePVAGetReadings(pvaAlarm);
  return (0);
  return 1;
}

long SetupPVs(LOGGER_DATA *logger, PVA_OVERALL *pvaTrigger, PVA_OVERALL *pvaGlitch, PVA_OVERALL *pvaCond) {
  long j;
  if (logger->verbose) {
    fprintf(stdout, "Connecting to PVs\n");
  }
  if (pvaTrigger != NULL) {
    allocPVA(pvaTrigger, logger->triggerChannels);
    epics::pvData::shared_vector<std::string> triggerNames(logger->triggerChannels);
    epics::pvData::shared_vector<std::string> triggerProviders(logger->triggerChannels);
    for (j = 0; j < logger->triggerChannels; j++) {
      triggerNames[j] = logger->triggerControlName[j];
      triggerProviders[j] = logger->triggerRequest[j].provider;
    }
    pvaTrigger->pvaChannelNames = freeze(triggerNames);
    pvaTrigger->pvaProvider = freeze(triggerProviders);
    ConnectPVA(pvaTrigger, 2.0);
    pvaTrigger->limitGetReadings = true;
    for (j = 0; j < pvaTrigger->numPVs; j++) {
      if (!pvaTrigger->isConnected[j]) {
        fprintf(stderr, "Error4: Problem doing search for %s\n", logger->triggerControlName[j]);
        return (1);
      }
    }
    //Get initial values
    if (GetPVAValues(pvaTrigger) == 1) {
      return (1);
    }
    freePVAGetReadings(pvaTrigger);
  }
  if (pvaGlitch != NULL) {
    allocPVA(pvaGlitch, logger->glitchChannels);
    epics::pvData::shared_vector<std::string> glitchNames(logger->glitchChannels);
    epics::pvData::shared_vector<std::string> glitchProviders(logger->glitchChannels);
    for (j = 0; j < logger->glitchChannels; j++) {
      glitchNames[j] = logger->glitchControlName[j];
      glitchProviders[j] = logger->glitchRequest[j].provider;
    }
    pvaGlitch->pvaChannelNames = freeze(glitchNames);
    pvaGlitch->pvaProvider = freeze(glitchProviders);
    ConnectPVA(pvaGlitch, 2.0);
    pvaGlitch->limitGetReadings = true;
    for (j = 0; j < pvaGlitch->numPVs; j++) {
      if (!pvaGlitch->isConnected[j]) {
        fprintf(stderr, "Error5: Problem doing search for %s\n", logger->glitchControlName[j]);
        //return (1);
      }
    }
    //Get initial values
    if (GetPVAValues(pvaGlitch) == 1) {
      return (1);
    }
    freePVAGetReadings(pvaGlitch);
  }
  if (pvaCond != NULL) {
    allocPVA(pvaCond, logger->CondChannels);
    epics::pvData::shared_vector<std::string> condNames(logger->CondChannels);
    epics::pvData::shared_vector<std::string> condProviders(logger->CondChannels);
    for (j = 0; j < logger->CondChannels; j++) {
      condNames[j] = logger->CondDeviceName[j];
      condProviders[j] = logger->CondProvider[j];
    }
    pvaCond->pvaChannelNames = freeze(condNames);
    pvaCond->pvaProvider = freeze(condProviders);
    ConnectPVA(pvaCond, logger->pendIOtime);
    pvaCond->limitGetReadings = true;
    for (j = 0; j < pvaCond->numPVs; j++) {
      if (!pvaCond->isConnected[j]) {
        fprintf(stderr, "Error6: Problem doing search for %s\n", logger->CondDeviceName[j]);
        return (1);
      }
    }
    //Get initial values
    if (GetPVAValues(pvaCond) == 1) {
      return (1);
    }
    freePVAGetReadings(pvaCond);
  }

  return (0);
}

bool PassesInhibitPVA(LOGGER_DATA *logger, PVA_OVERALL *pvaInhibit) {
  double value;

  if (pvaInhibit->isConnected[0] == true) {
    value = pvaInhibit->pvaData[0].getData[0].values[0];
    if (value > .1) {
      return false;
    }
  }
  return true;
}

long getConditionsDataInfo(LOGGER_DATA *logger) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName = NULL, *MessageColumnName = NULL;
  short MessageDefined, ScaleFactorDefined, HoldoffDefined, ProviderDefined;
  long i;

  logger->CondDeviceName = logger->CondProvider = logger->CondReadMessage = NULL;
  logger->CondExpectFieldType = NULL;
  logger->CondExpectNumeric = NULL;
  logger->CondExpectElements = NULL;
  logger->CondChannels = 0;
  logger->CondLowerLimit = logger->CondUpperLimit = NULL;
  logger->CondHoldoff = NULL;

  if (!SDDS_InitializeInput(&inSet, logger->CondFile))
    return 0;

  if (!(ControlNameColumnName =
        SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                        "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  MessageDefined = 1;
  if (!(MessageColumnName =
        SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                        "ReadMessage", "Message", "ReadMsg", NULL))) {
    MessageDefined = 0;
  }

  ProviderDefined = 0;
  switch (SDDS_CheckColumn(&inSet, (char *)"Provider", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    ProviderDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"Provider\".\n");
    exit(1);
    break;
  }
  if (ProviderDefined) {
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectFieldType", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectFieldType\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectFieldType\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectNumeric\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectNumeric\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectElements", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectNumeric\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectNumeric\".\n");
      exit(1);
      break;
    }
  }

  ScaleFactorDefined = 0;
  switch (SDDS_CheckColumn(&inSet, (char *)"ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    ScaleFactorDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"ScaleFactor\".\n");
    exit(1);
    break;
  }

  if (0 >= SDDS_ReadPage(&inSet)) {
    if (ControlNameColumnName)
      free(ControlNameColumnName);
    if (MessageColumnName)
      free(MessageColumnName);
    return 0;
  }

  if (!(logger->CondChannels = SDDS_CountRowsOfInterest(&inSet)))
    SDDS_Bomb((char *)"Zero rows defined in input file.\n");

  logger->CondDeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);

  logger->CondReadMessage = NULL;
  if (MessageDefined)
    logger->CondReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
  else {
    logger->CondReadMessage = (char **)malloc(sizeof(char *) * (logger->CondChannels));
    for (i = 0; i < logger->CondChannels; i++) {
      (logger->CondReadMessage)[i] = (char *)malloc(sizeof(char));
      (logger->CondReadMessage)[i][0] = 0;
    }
  }

  if (ProviderDefined) {
    logger->CondProvider = (char **)SDDS_GetColumn(&inSet, (char *)"Provider");
    logger->CondExpectFieldType = (char **)SDDS_GetColumn(&inSet, (char *)"ExpectFieldType");
    logger->CondExpectNumeric = (char *)SDDS_GetColumn(&inSet, (char *)"ExpectNumeric");
    logger->CondExpectElements = (int32_t *)SDDS_GetColumn(&inSet, (char *)"ExpectElements");
  } else {
    logger->CondProvider = (char **)malloc(sizeof(char *) * (logger->CondChannels));
    logger->CondExpectFieldType = (char **)malloc(sizeof(char *) * (logger->CondChannels));
    logger->CondExpectNumeric = (char *)malloc(sizeof(char) * (logger->CondChannels));
    logger->CondExpectElements = (int32_t *)malloc(sizeof(int32_t) * (logger->CondChannels));
    for (i = 0; i < logger->CondChannels; i++) {
      (logger->CondProvider)[i] = (char *)malloc(sizeof(char) * 5);
      (logger->CondExpectFieldType)[i] = (char *)malloc(sizeof(char) * (8 + 1));
      (logger->CondProvider)[i] = (char *)"ca";
      (logger->CondExpectFieldType)[i][0] = 0;
    }
  }

  logger->CondScaleFactor = NULL;
  if (ScaleFactorDefined &&
      !(logger->CondScaleFactor = SDDS_GetColumnInDoubles(&inSet, (char *)"ScaleFactor")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  switch (SDDS_CheckColumn(&inSet, (char *)"LowerLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
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
  switch (SDDS_CheckColumn(&inSet, (char *)"UpperLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
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

  if (!(logger->CondLowerLimit = SDDS_GetColumnInDoubles(&inSet, (char *)"LowerLimit")) ||
      !(logger->CondUpperLimit = SDDS_GetColumnInDoubles(&inSet, (char *)"UpperLimit")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  HoldoffDefined = 0;
  switch (SDDS_CheckColumn(&inSet, (char *)"HoldoffTime", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    HoldoffDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"HoldoffTime\".\n");
    exit(1);
    break;
  }
  if (HoldoffDefined &&
      !(logger->CondHoldoff = SDDS_GetColumnInDoubles(&inSet, (char *)"HoldoffTime")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  SDDS_Terminate(&inSet);
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  if (MessageColumnName)
    free(MessageColumnName);
  return 1;
}
long PassesConditionsTest(LOGGER_DATA *logger, PVA_OVERALL *pvaCond) {
  long passed, CAerrors, i, tested;
  double maxHoldoff;

  CAerrors = pvaCond->numNotConnected;
  if (CAerrors &&
      (logger->CondMode & MUST_PASS_ALL || CAerrors == logger->CondChannels))
    return 0;

  passed = tested = 0;
  maxHoldoff = 0;
  for (i = 0; i < logger->CondChannels; i++) {
    if (logger->CondLowerLimit[i] < logger->CondUpperLimit[i]) {
      if (logger->CondScaleFactor)
        logger->CondDataBuffer[i] *= logger->CondScaleFactor[i];
      tested++;
      if ((logger->CondDataBuffer[i] < logger->CondLowerLimit[i]) ||
          (logger->CondDataBuffer[i] > logger->CondUpperLimit[i])) {
        if (logger->CondHoldoff && (logger->CondHoldoff[i] > maxHoldoff))
          maxHoldoff = logger->CondHoldoff[i];
      } else
        passed++;
    }
  }
  if ((logger->CondMode & MUST_PASS_ALL && tested != passed) ||
      (logger->CondMode & MUST_PASS_ONE && !passed)) {
    if (maxHoldoff > 0) {
      /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
      if (pvaThreadSleep(maxHoldoff) == 1) {
        return (1);
      }
      //ca_pend_event(maxHoldoff);
    } else {
      if (pvaThreadSleep(.1) == 1) {
        return (1);
      }
      //ca_poll();
    }
    return 0;
  }
  return passed;
}

int ReadInputFiles(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels) {
  long i, iGlitch, iAlarm, iTrigger;

  //Read Conditions file data
  if (logger->CondFile) {
    if (!getConditionsDataInfo(logger))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!(logger->CondDataBuffer = (double *)malloc(sizeof(*(logger->CondDataBuffer)) * logger->CondChannels)))
      SDDS_Bomb((char *)"allocation failure");
  }

  //Read Input File
  if (!getGlitchLoggerData(logger, alarmTrigRequest, alarmTrigChannels))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  //Read Trigger File
  if (logger->trigFileSet) {
    if (logger->numDatasets > 1)
      SDDS_Bomb((char *)"Both the input file and trigger file should be single-paged");
    if (!getTriggerData(logger, alarmTrigRequest, alarmTrigChannels))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /*only one dataset, i.e, one output, numDatasets=1*/
    if (*alarmTrigChannels)
      logger->datasets[0].triggerType |= ALARMTRIGGER;
    if (logger->triggerChannels)
      logger->datasets[0].triggerType |= TRIGGER;
    if (logger->glitchChannels)
      logger->datasets[0].triggerType |= GLITCHTRIGGER;
    /*the OutputRootname of datasets[0] is empty, and outputDir is the OutputRootname */
    logger->datasets[0].OutputRootname = (char *)malloc(sizeof(char) * 2);
    logger->datasets[0].OutputRootname[0] = 0;
  }

  //Set defaults for autoholdoff and holdoff time
  if (logger->glitchChannels) {
    for (iGlitch = 0; iGlitch < logger->glitchChannels; iGlitch++) {
      if (logger->autoHoldoff)
        logger->glitchRequest[iGlitch].flags |= TRIGGER_AUTOHOLD;
      if (logger->defaultHoldoffTime > 0 && !(logger->glitchRequest[iGlitch].flags & TRIGGER_HOLDOFF)) {
        logger->glitchRequest[iGlitch].holdoff = logger->defaultHoldoffTime;
        logger->glitchRequest[iGlitch].flags |= TRIGGER_HOLDOFF;
      }
    }
  }
  if (logger->triggerChannels) {
    for (iTrigger = 0; iTrigger < logger->triggerChannels; iTrigger++) {
      if (logger->autoHoldoff)
        logger->triggerRequest[iTrigger].flags |= TRIGGER_AUTOHOLD;
      if (logger->defaultHoldoffTime > 0 && !(logger->triggerRequest[iTrigger].flags & TRIGGER_HOLDOFF)) {
        logger->triggerRequest[iTrigger].holdoff = logger->defaultHoldoffTime;
        logger->triggerRequest[iTrigger].flags |= TRIGGER_HOLDOFF;
      }
    }
  }
  if (*alarmTrigChannels) {
    for (iAlarm = 0; iAlarm < *alarmTrigChannels; iAlarm++) {
      if (logger->autoHoldoff)
        (*alarmTrigRequest)[iAlarm].flags |= ALARMTRIG_AUTOHOLD;
      if (logger->defaultHoldoffTime > 0 && !((*alarmTrigRequest)[iAlarm].flags & ALARMTRIG_HOLDOFF)) {
        (*alarmTrigRequest)[iAlarm].holdoff = logger->defaultHoldoffTime;
        (*alarmTrigRequest)[iAlarm].flags |= ALARMTRIG_HOLDOFF;
      }
    }
  }

  //Allocate and initialize variables
  logger->disconnected = (short *)malloc(sizeof(*(logger->disconnected)) * logger->numDatasets);
  logger->servicingTrigger = (short *)malloc(sizeof(*(logger->servicingTrigger)) * logger->numDatasets);
  logger->writingPage = (short *)malloc(sizeof(*(logger->writingPage)) * logger->numDatasets);
  logger->pointsLeft = (long *)malloc(sizeof(*(logger->pointsLeft)) * logger->numDatasets);
  logger->trigBeforeCount = (long *)malloc(sizeof(*(logger->trigBeforeCount)) * logger->numDatasets);
  logger->tableStarted = (short *)malloc(sizeof(*(logger->tableStarted)) * logger->numDatasets);
  for (i = 0; i < logger->numDatasets; i++) {
    if (!(logger->datasets[i].dataset = (SDDS_DATASET *)malloc(sizeof(*(logger->datasets[i].dataset)))) ||
        !(logger->datasets[i].values = (double **)malloc(sizeof(*(logger->datasets[i].values)) * (logger->datasets[i].variables))) ||
        !(logger->datasets[i].elements = (long *)malloc(sizeof(*(logger->datasets[i].elements)) * (logger->datasets[i].variables))))
      SDDS_Bomb((char *)"Memory allocation failure!");
    logger->disconnected[i] = 0;
    logger->servicingTrigger[i] = 0;
    logger->writingPage[i] = 0;
    logger->pointsLeft[i] = logger->glitchAfter;
    logger->trigBeforeCount[i] = 0;
    logger->tableStarted[i] = 0;
    logger->datasets[i].triggerMode = 0;
    logger->datasets[i].pva = new PVA_OVERALL;

    snprintf(logger->temp, sizeof(logger->temp), "%s%s", logger->outputDir, logger->datasets[i].OutputRootname);
    snprintf(logger->temp, sizeof(logger->temp), "%s", MakeDailyGenerationFilename(logger->temp, 4, (char *)".", 0));
    logger->datasets[i].outputFile = (char *)tmalloc(sizeof(char) * (strlen(logger->temp) + 1));
    strcpy(logger->datasets[i].outputFile, logger->temp);
    if (logger->verbose)
      fprintf(stdout, "output file is %s\n", logger->datasets[i].outputFile);
#if DEBUG
    fprintf(stdout, "Initial output file is %s\n", logger->datasets[i].outputFile);
#endif
  }

  return (0);
}

long getGlitchLoggerData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest,
                         long *alarmTrigChannels) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName, *OutputRootname;
  long page, rows = 0, i, j, found = 0, dIndex = 0;
  short noReadbackName = 0, UnitsDefined = 0, ProviderDefined = 0;
  short MajorAlarmDefined = 0, MinorAlarmDefined = 0, NoAlarmDefined = 0;
  short TransitionThresholdDefined = 0, GlitchThresholdDefined = 0, TransitionDirectionDefined = 0;
  short GlitchSamplesDefined = 0, GlitchAutoResetDefined = 0, glitchScriptExist = 0;
  short major, minor, noalarm, direction;
  double glitchDelta = 0;
  char *glitchScript;

  major = minor = noalarm = direction = 0;
  ControlNameColumnName = OutputRootname = NULL;

  logger->datasets = NULL;
  if (logger->glitchRequest)
    logger->glitchRequest = NULL;
  if (logger->glitchChannels) {
    logger->glitchChannels = 0;
  }
  if (logger->triggerRequest)
    logger->triggerRequest = NULL;
  if (logger->triggerChannels) {
    logger->triggerChannels = 0;
  }
  if (alarmTrigRequest)
    *alarmTrigRequest = NULL;
  if (alarmTrigChannels) {
    *alarmTrigChannels = 0;
  }

  logger->numDatasets = 0;

  if (!SDDS_InitializeInput(&inSet, logger->inputfile))
    return 0;

  if (!(ControlNameColumnName =
        SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                        "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, (char *)"ReadbackName", NULL, SDDS_STRING, NULL)))
    noReadbackName = 1;

  if (SDDS_CheckParameter(&inSet, (char *)"GlitchScript", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY)
    glitchScriptExist = 1;

  switch (SDDS_CheckColumn(&inSet, (char *)"ReadbackUnits", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    UnitsDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"ReadbackUnits\".\n");
    exit(1);
    break;
  }
  if (!UnitsDefined) {
    switch (SDDS_CheckColumn(&inSet, (char *)"ReadbackUnit", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      UnitsDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ReadbackUnit\".\n");
      exit(1);
      break;
    }
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"Provider", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    ProviderDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"Provider\".\n");
    exit(1);
    break;
  }
  if (ProviderDefined) {
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectFieldType", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectFieldType\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectFieldType\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectNumeric\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectNumeric\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectElements", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Column \"ExpectNumeric\".not found\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectNumeric\".\n");
      exit(1);
      break;
    }
  }

  if (!logger->trigFileSet) {
    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&inSet, (char *)"TriggerControlName", NULL,
                                                SDDS_STRING, NULL))) {
      fprintf(stderr, "Error: TriggerControlName parameter is missing or not string type\n");
      exit(1);
    }

    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&inSet, (char *)"OutputRootname", NULL,
                                                SDDS_STRING, NULL))) {
      fprintf(stderr, "Error: OutputRootname parameter is missing or not string type\n");
      exit(1);
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"MajorAlarm", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      MajorAlarmDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"MajorAlarm\".\n");
      exit(1);
      break;
    }

    switch (SDDS_CheckParameter(&inSet, (char *)"MinorAlarm", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      MinorAlarmDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"MinorAlarm\".\n");
      exit(1);
      break;
    }

    switch (SDDS_CheckParameter(&inSet, (char *)"NoAlarm", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      NoAlarmDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"NoAlarm\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"TransitionThreshold", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      TransitionThresholdDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"TransitionThreshold\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"TransitionDirection", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      TransitionDirectionDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"TransitionDirection\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"GlitchThreshold", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      GlitchThresholdDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"GlitchThreshold\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"GlitchBaselineSamples", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      GlitchSamplesDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"GlitchBaselineSamples\".\n");
      exit(1);
      break;
    }
    switch (SDDS_CheckParameter(&inSet, (char *)"GlitchBaselineAutoReset", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      GlitchAutoResetDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"GlitchBaselineAutoReset\".\n");
      exit(1);
      break;
    }
  }
  while ((page = SDDS_ReadPage(&inSet)) > 0) {
    if (!logger->trigFileSet) {
      if (SDDS_GetParameter(&inSet, (char *)"OutputRootname", &OutputRootname) == NULL)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      found = 0;
      for (j = 0; j < (logger->numDatasets); j++) {
        if (!strcmp(OutputRootname, (logger->datasets + j)->OutputRootname)) {
          found = 1;
          dIndex = j;
          break;
        }
      }
    } else {
      if (page > 1) {
        fprintf(stderr, "file %s has multi-pages, only the first page is used.\n", logger->inputfile);
        return 1;
      }
    }
    /*if triggerFile is given, found=0, only read the columns in the first page of inputFile */
    if (!found) {
      (logger->numDatasets)++;
      if (!(rows = SDDS_RowCount(&inSet)))
        bomb((char *)"Zero rows defined in input file.\n", NULL);
      if (!(logger->datasets = (DATASETS *)realloc(logger->datasets, sizeof(DATASETS) * (logger->numDatasets))))
        SDDS_Bomb((char *)"Memory allocation failure for datasets");
      (logger->datasets + (logger->numDatasets) - 1)->variables = rows;
      (logger->datasets + (logger->numDatasets) - 1)->ControlName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
      if (noReadbackName)
        (logger->datasets + (logger->numDatasets) - 1)->ReadbackName = (char **)SDDS_GetColumn(&inSet,
                                                                                               ControlNameColumnName);
      else
        (logger->datasets + (logger->numDatasets) - 1)->ReadbackName = (char **)SDDS_GetColumn(&inSet, (char *)"ReadbackName");

      if (UnitsDefined)
        (logger->datasets + (logger->numDatasets) - 1)->ReadbackUnits = (char **)SDDS_GetColumn(&inSet, (char *)"ReadbackUnits");
      else {
        (logger->datasets + (logger->numDatasets) - 1)->ReadbackUnits = (char **)malloc(sizeof(char *) * rows);
        for (i = 0; i < rows; i++) {
          (logger->datasets + (logger->numDatasets) - 1)->ReadbackUnits[i] =
            (char *)malloc(sizeof(char) * (8 + 1));
          (logger->datasets + (logger->numDatasets) - 1)->ReadbackUnits[i][0] = 0;
        }
      }
      if (ProviderDefined) {
        (logger->datasets + (logger->numDatasets) - 1)->Provider = (char **)SDDS_GetColumn(&inSet, (char *)"Provider");
        (logger->datasets + (logger->numDatasets) - 1)->ExpectFieldType = (char **)SDDS_GetColumn(&inSet, (char *)"ExpectFieldType");
        (logger->datasets + (logger->numDatasets) - 1)->ExpectNumeric = (char *)SDDS_GetColumn(&inSet, (char *)"ExpectNumeric");
        (logger->datasets + (logger->numDatasets) - 1)->ExpectElements = (int32_t *)SDDS_GetColumnInLong(&inSet, (char *)"ExpectElements");
      } else {
        (logger->datasets + (logger->numDatasets) - 1)->Provider = (char **)malloc(sizeof(char *) * rows);
        (logger->datasets + (logger->numDatasets) - 1)->ExpectFieldType = (char **)malloc(sizeof(char *) * rows);
        (logger->datasets + (logger->numDatasets) - 1)->ExpectNumeric = (char *)malloc(sizeof(char) * rows);
        (logger->datasets + (logger->numDatasets) - 1)->ExpectElements = (int32_t *)malloc(sizeof(int32_t) * rows);
        for (i = 0; i < rows; i++) {
          (logger->datasets + (logger->numDatasets) - 1)->Provider[i] = (char *)malloc(sizeof(char) * 5);
          (logger->datasets + (logger->numDatasets) - 1)->ExpectFieldType[i] = (char *)malloc(sizeof(char) * (8 + 1));
          (logger->datasets + (logger->numDatasets) - 1)->Provider[i] = (char *)"ca";
          (logger->datasets + (logger->numDatasets) - 1)->ExpectFieldType[i][0] = 0;
        }
      }

      if (!logger->trigFileSet) { /*set the OutputRootname and TriggerControlName of datasets */
        if (!((logger->datasets + (logger->numDatasets) - 1)->OutputRootname =
              (char *)malloc(sizeof(char) * (strlen(OutputRootname) + 1))))
          SDDS_Bomb((char *)"failure allocation memory1");
        strcpy((logger->datasets + (logger->numDatasets) - 1)->OutputRootname, OutputRootname);
        if (SDDS_GetParameter(&inSet, (char *)"TriggerControlName",
                              &(logger->datasets + (logger->numDatasets) - 1)->TriggerControlName) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"TriggerControlName\".\n");
          exit(1);
        }
        (logger->datasets + (logger->numDatasets) - 1)->triggerType = 0;
      }
    }                           /*end of if (!found) */
    if (!logger->trigFileSet) { /*get the trigger information for inputFile if triggerFile is not given */
      /*get the information of alarm triggers if any */
      if (glitchScriptExist && !SDDS_GetParameter(&inSet, (char *)"GlitchScript", &glitchScript))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (MajorAlarmDefined || MinorAlarmDefined || NoAlarmDefined) {
        if (MajorAlarmDefined) {
          if (SDDS_GetParameter(&inSet, (char *)"MajorAlarm",
                                &major) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"MajorAlarm\".\n");
            exit(1);
          }
        }
        if (MinorAlarmDefined) {
          if (SDDS_GetParameter(&inSet, (char *)"MinorAlarm",
                                &minor) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"MinorAlarm\".\n");
            exit(1);
          }
        }
        if (NoAlarmDefined) {
          if (SDDS_GetParameter(&inSet, (char *)"NoAlarm",
                                &noalarm) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"NoAlarm\".\n");
            exit(1);
          }
        }
        if (major || minor || noalarm) {
          (*alarmTrigChannels)++;
          *alarmTrigRequest = (ALARMTRIG_REQUEST *)realloc(*alarmTrigRequest, sizeof(**alarmTrigRequest) * (*alarmTrigChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript), glitchScript);
          else
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = NULL;

          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList =
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider =
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList = NULL;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList[0] = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList = NULL;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->flags = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->holdoff = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->trigStep = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->triggered = 0;

          if (SDDS_GetParameter(&inSet, (char *)"TriggerControlName",
                                &(*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"TriggerControlName\".\n");
            exit(1);
          }
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider = (char *)"ca";

          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MajorAlarm = major;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MinorAlarm = minor;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->NoAlarm = noalarm;
          if (major) {
            if (minor) {
              if (noalarm)
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MAJOR,MINOR,NO_ALARM");
              else
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MAJOR,MINOR");
            } else {
              if (noalarm)
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MAJOR,NO_ALARM");
              else
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MAJOR");
            }
          } else {
            if (minor) {
              if (noalarm)
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MINOR,NO_ALARM");
              else
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "MINOR");
            } else {
              if (noalarm)
                strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                       "NO_ALARM");
            }
          }
          if (!((*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusIndex =
                CSStringListsToIndexArray(&(*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusIndices,
                                          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList,
                                          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList,
                                          (char **)alarmStatusString,
                                          ALARM_NSTATUS)))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          if (!((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityIndex =
                CSStringListsToIndexArray(&(*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityIndices,
                                          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                                          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList,
                                          (char **)alarmSeverityString,
                                          ALARM_NSEV)))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          if (found) {
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->dataIndex = dIndex;
            if (!((logger->datasets + dIndex)->triggerType & ALARMTRIGGER))
              (logger->datasets + dIndex)->triggerType |= ALARMTRIGGER;
          } else {
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->dataIndex = (logger->numDatasets) - 1;
            (logger->datasets + (logger->numDatasets) - 1)->triggerType |= ALARMTRIGGER;
          }
        }
      }

      /*get the information of triggers if any */
      if (TransitionThresholdDefined && TransitionDirectionDefined) {
        if (SDDS_GetParameter(&inSet, (char *)"TransitionDirection",
                              &direction) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"TransitionDiection\".\n");
          exit(1);
        }
        if (direction) {
          (logger->triggerChannels)++;
          logger->triggerRequest = (TRIGGER_REQUEST *)realloc(logger->triggerRequest, sizeof(TRIGGER_REQUEST) * (logger->triggerChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((logger->triggerRequest + (logger->triggerChannels) - 1)->glitchScript), glitchScript);
          else
            (logger->triggerRequest + (logger->triggerChannels) - 1)->glitchScript = NULL;

          (logger->triggerRequest + (logger->triggerChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (logger->triggerRequest + (logger->triggerChannels) - 1)->provider = (char *)"ca";
          (logger->triggerRequest + (logger->triggerChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
          (logger->triggerRequest + (logger->triggerChannels) - 1)->message[0] = 0;
          (logger->triggerRequest + (logger->triggerChannels) - 1)->holdoff = 0;
          (logger->triggerRequest + (logger->triggerChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
          (logger->triggerRequest + (logger->triggerChannels) - 1)->trigStep = 0;

          if (SDDS_GetParameter(&inSet, (char *)"TriggerControlName",
                                &(logger->triggerRequest + (logger->triggerChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"TriggerControlName\".\n");
            exit(1);
          }

          if (TransitionThresholdDefined) {
            if (SDDS_GetParameter(&inSet, (char *)"TransitionThreshold",
                                  &(logger->triggerRequest + (logger->triggerChannels) - 1)->triggerLevel) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"TransitionThreshold\".\n");
              exit(1);
            }
          } else {
            (logger->triggerRequest + (logger->triggerChannels) - 1)->triggerLevel = 0;
          }

          (logger->triggerRequest + (logger->triggerChannels) - 1)->direction = direction;
          if (found) {
            (logger->triggerRequest + (logger->triggerChannels) - 1)->dataIndex = dIndex;
            if (!((logger->datasets + dIndex)->triggerType & TRIGGER))
              (logger->datasets + dIndex)->triggerType |= TRIGGER;
          } else {
            (logger->triggerRequest + (logger->triggerChannels) - 1)->dataIndex = (logger->numDatasets) - 1;
            (logger->datasets + (logger->numDatasets) - 1)->triggerType |= TRIGGER;
          }
        }
      }

      /*get information of glitch if any */
      if (GlitchThresholdDefined) {
        if (SDDS_GetParameter(&inSet, (char *)"GlitchThreshold",
                              &glitchDelta) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"GlitchThreshold\".\n");
          exit(1);
        }
        if (glitchDelta) {
          (logger->glitchChannels)++;
          logger->glitchRequest = (GLITCH_REQUEST *)realloc(logger->glitchRequest, sizeof(GLITCH_REQUEST) * (logger->glitchChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((logger->glitchRequest + (logger->glitchChannels) - 1)->glitchScript), glitchScript);
          else
            (logger->glitchRequest + (logger->glitchChannels) - 1)->glitchScript = NULL;

          (logger->glitchRequest + (logger->glitchChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (logger->glitchRequest + (logger->glitchChannels) - 1)->provider = (char *)"ca";
          (logger->glitchRequest + (logger->glitchChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
          (logger->glitchRequest + (logger->glitchChannels) - 1)->message[0] = 0;
          (logger->glitchRequest + (logger->glitchChannels) - 1)->delta =
            (logger->glitchRequest + (logger->glitchChannels) - 1)->holdoff = 0;
          (logger->glitchRequest + (logger->glitchChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
          (logger->glitchRequest + (logger->glitchChannels) - 1)->trigStep = 0;

          if (SDDS_GetParameter(&inSet, (char *)"TriggerControlName",
                                &(logger->glitchRequest + (logger->glitchChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"GlitchControlName\".\n");
            exit(1);
          }

          (logger->glitchRequest + (logger->glitchChannels) - 1)->delta = glitchDelta;
          if (GlitchSamplesDefined) {
            if (SDDS_GetParameter(&inSet, (char *)"GlitchBaselineSamples",
                                  &(logger->glitchRequest + (logger->glitchChannels) - 1)->baselineSamples) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"GlitchBaselineSamples\".\n");
              exit(1);
            }
          } else
            (logger->glitchRequest + (logger->glitchChannels) - 1)->baselineSamples = 0;
          if (GlitchAutoResetDefined) {
            if (SDDS_GetParameter(&inSet, (char *)"GlitchBaselineAutoReset",
                                  &(logger->glitchRequest + (logger->glitchChannels) - 1)->baselineAutoReset) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"GlitchBaselineAutoReset\".\n");
              exit(1);
            }
          } else
            (logger->glitchRequest + (logger->glitchChannels) - 1)->baselineAutoReset = 0;
          if (found) {
            (logger->glitchRequest + (logger->glitchChannels) - 1)->dataIndex = dIndex;
            if (!((logger->datasets + dIndex)->triggerType & GLITCHTRIGGER))
              (logger->datasets + dIndex)->triggerType |= GLITCHTRIGGER;
          } else {
            (logger->glitchRequest + (logger->glitchChannels) - 1)->dataIndex = (logger->numDatasets) - 1;
            (logger->datasets + (logger->numDatasets) - 1)->triggerType |= GLITCHTRIGGER;
          }
        }
      } /*end of get information of glitch trigger */
      if (glitchScriptExist) {
        free(glitchScript);
        glitchScript = NULL;
      }
    } /*end of if (!logger->trigFileSet) */
  }   /*end of while(page=SDDS_ReadPage  */
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  if (OutputRootname)
    free(OutputRootname);

  SDDS_Terminate(&inSet);
  return 1;
}

void startGlitchLogFiles(LOGGER_DATA *logger, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests) {
  long i, j, useArrays = 0;
  getTimeBreakdown(&(logger->StartTime), &(logger->StartDay), &(logger->StartHour), &(logger->StartJulianDay), &(logger->StartMonth),
                   &(logger->StartYear), &(logger->TimeStamp));

  for (i = 0; i < logger->numDatasets; i++) {
    useArrays = 0;
    for (j = 0; j < (logger->datasets + i)->variables; j++) {
      if (((logger->datasets + i)->elements)[j] != 1) {
        useArrays = 1;
      }
    }
    if (!SDDS_InitializeOutput((logger->datasets + i)->dataset, SDDS_BINARY, 1, NULL, NULL,
                               (logger->datasets + i)->outputFile) ||
        !DefineLoggingTimeParameters((logger->datasets + i)->dataset) ||
        !DefineLoggingTimeDetail((logger->datasets + i)->dataset, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (useArrays) {
      if (!DefineLoggingTimeDetail((logger->datasets + i)->dataset, TIMEDETAIL_ARRAYS | TIMEDETAIL_EXTRAS))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_EnableFSync((logger->datasets + i)->dataset);
    if (!DefineGlitchParameters(logger, alarmTrigRequest, alarmTrigRequests, i))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (SDDS_DefineParameter((logger->datasets + i)->dataset, "TriggerTime", NULL, "seconds", NULL, NULL,
                             SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (j = 0; j < (logger->datasets + i)->variables; j++) {
      if (((logger->datasets + i)->elements)[j] == 1) {
        if (!SDDS_DefineSimpleColumn((logger->datasets + i)->dataset,
                                     ((logger->datasets + i)->ReadbackName)[j],
                                     ((logger->datasets + i)->ReadbackUnits)[j],
                                     SDDS_DOUBLE)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      } else {
        if (!SDDS_DefineArray((logger->datasets + i)->dataset,
                              ((logger->datasets + i)->ReadbackName)[j],
                              NULL,
                              ((logger->datasets + i)->ReadbackUnits)[j],
                              NULL,
                              NULL,
                              SDDS_DOUBLE,
                              0,
                              2,
                              NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      }
    }
    if (SDDS_DefineColumn((logger->datasets + i)->dataset, "PostTrigger", NULL, NULL,
                          NULL, NULL, SDDS_SHORT, 0) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    (logger->datasets + i)->ReadbackIndex = (long *)malloc(sizeof(long) * ((logger->datasets + i)->variables));
    for (j = 0; j < (logger->datasets)[i].variables; j++) {
      if (((logger->datasets + i)->elements)[j] == 1) {
        if (((logger->datasets + i)->ReadbackIndex[j] = SDDS_GetColumnIndex((logger->datasets + i)->dataset,
                                                                            (logger->datasets + i)->ReadbackName[j])) < 0) {
          fprintf(stderr, "Unable to retrieve index for column %s\n", (logger->datasets + i)->ReadbackName[j]);
          exit(1);
        }
      } else {
        if (((logger->datasets + i)->ReadbackIndex[j] = SDDS_GetArrayIndex((logger->datasets + i)->dataset,
                                                                           (logger->datasets + i)->ReadbackName[j])) < 0) {
          fprintf(stderr, "Unable to retrieve index for array %s\n", (logger->datasets + i)->ReadbackName[j]);
          exit(1);
        }
      }
    }
    if (!SDDS_WriteLayout((logger->datasets + i)->dataset))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_DisconnectFile((logger->datasets + i)->dataset))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

long DefineGlitchParameters(LOGGER_DATA *logger, ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests, long dataIndex) {
  long i, j;
  char buffer[1024], descrip[1024];
  for (i = 0; i < logger->glitchChannels; i++) {
    if (logger->glitchRequest[i].dataIndex == dataIndex) {
      snprintf(buffer, sizeof(buffer), "%sGlitched%ld", logger->glitchRequest[i].controlName, i);
      snprintf(descrip, sizeof(descrip), "sddsmonitor glitch status of %s", logger->glitchRequest[i].controlName);
      if ((logger->glitchRequest[i].parameterIndex =
           SDDS_DefineParameter((logger->datasets + dataIndex)->dataset, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
    }
  }
  for (i = 0; i < logger->triggerChannels; i++) {
    if (logger->triggerRequest[i].dataIndex == dataIndex) {
      snprintf(buffer, sizeof(buffer), "%sTriggered%ld", logger->triggerRequest[i].controlName, i);
      snprintf(descrip, sizeof(descrip), "sddsmonitor trigger status of %s%ld", logger->triggerRequest[i].controlName, i);
      if ((logger->triggerRequest[i].parameterIndex =
           SDDS_DefineParameter((logger->datasets + dataIndex)->dataset, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
    }
  }

  for (i = 0; i < alarmTrigRequests; i++) {
    if (alarmTrigRequest[i].dataIndex == dataIndex) {
      snprintf(descrip, sizeof(descrip), "sddsmonitor alarm-trigger status of %s",
              alarmTrigRequest[i].controlName);
      j = 0;
      while (1) {
        snprintf(buffer, sizeof(buffer), "%sAlarmTrigger%ld", alarmTrigRequest[i].controlName, j);
        if (SDDS_GetParameterIndex((logger->datasets + dataIndex)->dataset, buffer) >= 0)
          j++;
        else
          break;
      }
      if ((alarmTrigRequest[i].alarmParamIndex =
           SDDS_DefineParameter((logger->datasets + dataIndex)->dataset, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
      snprintf(buffer, sizeof(buffer), "%sAlarmSeverity%ld", alarmTrigRequest[i].controlName, j);
      snprintf(descrip, sizeof(descrip), "EPICS alarm severity of %s",
              alarmTrigRequest[i].controlName);
      if ((alarmTrigRequest[i].severityParamIndex =
           SDDS_DefineParameter((logger->datasets + dataIndex)->dataset, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
    }
  }
  return 1;
}

long *CSStringListsToIndexArray(
                                long *arraySize,
                                char *includeList,
                                char *excludeList,
                                char **validItem,
                                long validItems) {
  long *returnValue = NULL;
  char *item;
  long returnValues = 0, matchIndex, i;
  short *valuePicked = NULL;

  if (!(valuePicked = (short *)calloc(validItems, sizeof(*valuePicked))) ||
      !(returnValue = (long *)calloc(validItems, sizeof(*returnValue)))) {
    SDDS_Bomb((char *)"Memory allocation failure (CSStringListsToArray)");
  }
  if (includeList && excludeList)
    return NULL;
  if (includeList) {
    while ((item = get_token(includeList))) {
      if ((matchIndex = match_string(item, validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_SetError((char *)"Invalid list entry (CSStringListsToArray)");
        return NULL;
      }
      valuePicked[matchIndex] = 1;
    }
  } else {
    for (i = 0; i < validItems; i++)
      valuePicked[i] = 1;
    if (excludeList) {
      while ((item = get_token(excludeList))) {
        if ((matchIndex = match_string(item, validItem, validItems, EXACT_MATCH)) < 0) {
          SDDS_SetError((char *)"Invalid list entry (CSStringListsToArray)");
          return NULL;
        }
        valuePicked[matchIndex] = 0;
      }
    }
  }
  for (i = returnValues = 0; i < validItems; i++) {
    if (valuePicked[i]) {
      if ((returnValue[returnValues++] = match_string(validItem[i], validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_Bomb((char *)"item not found in second search---this shouldn't happen.");
      }
    }
  }
  if (!returnValues)
    return NULL;
  *arraySize = returnValues;
  return returnValue;
}

long IsGlitch(double value, double baseline, double delta) {
  double diff;
  diff = fabs(value - baseline);
  if (delta > 0) {
    if (diff > delta)
      return 1;
    else
      return 0;
  } else if (delta < 0) {
    if (diff > fabs(delta * baseline))
      return 1;
    else
      return 0;
  } else /* delta =0, ignore this glitch */
    return 0;
}

long IsTrigger(double previousValue, double currentValue, double level, short direction) {
  if (direction > 0) {
    if (previousValue < level && level < currentValue)
      return 1;
    else
      return 0;
  } else if (direction == 0)
    return 0;
  else {
    if (currentValue < level && level < previousValue)
      return 1;
    else
      return 0;
  }
}

long getTriggerData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels) {
  SDDS_DATASET inSet;
  short majorDefined, minorDefined, noalarmDefined, glitchThresholdDefined, glitchBaselineSamplesDefined;
  short glitchAutoResetDefined, transitionThresholdDefined, transitionDirectionDefined;
  long rows = 0;
  char **TriggerControlName;
  short *MajorAlarm, *MinorAlarm, *NoAlarm, *TransitionDirection, *GlitchBaselineAutoReset;
  long *GlitchBaselineSamples, i;
  double *TransitionThreshold, *GlitchThreshold;
  char **glitchScript = NULL;
  bool providerDefined = false;
  char **provider = NULL, **expectFieldType = NULL;
  char *expectNumeric = NULL;
  int32_t *expectElements = NULL;

  majorDefined = minorDefined = noalarmDefined = glitchThresholdDefined = glitchBaselineSamplesDefined = glitchAutoResetDefined = transitionThresholdDefined = transitionDirectionDefined = 0;
  TriggerControlName = NULL;
  MajorAlarm = MinorAlarm = NoAlarm = TransitionDirection = GlitchBaselineAutoReset = NULL;
  GlitchBaselineSamples = NULL;
  TransitionThreshold = GlitchThreshold = NULL;

  logger->glitchRequest = NULL;
  logger->triggerRequest = NULL;
  *alarmTrigRequest = NULL;
  logger->glitchChannels = *alarmTrigChannels = logger->triggerChannels = 0;

  if (!SDDS_InitializeInput(&inSet, logger->triggerFile))
    return 0;
  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, (char *)"TriggerControlName", NULL, SDDS_STRING, NULL))) {
    fprintf(stderr, "TriggerControlName column does not exist in %s", logger->triggerFile);
    exit(1);
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"MajorAlarm", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    majorDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"MajorAlarm\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"MinorAlarm", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    minorDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"minorAlarm\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"NoAlarm", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    noalarmDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"NoAlarm\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"GlitchThreshold", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    glitchThresholdDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"GlitchThreshold\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"GlitchBaselineSamples", NULL, SDDS_LONG, NULL)) {
  case SDDS_CHECK_OKAY:
    glitchBaselineSamplesDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"GlitchBaselineSamples\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"GlitchBaselineAutoReset", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    glitchAutoResetDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"GlitchBaselineAutoReset\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"TransitionThreshold", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    transitionThresholdDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"TransitionThreshold\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, (char *)"TransitionDirection", NULL, SDDS_SHORT, NULL)) {
  case SDDS_CHECK_OKAY:
    transitionDirectionDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"TransitionDirection\".\n");
    exit(1);
    break;
  }

  switch (SDDS_CheckColumn(&inSet, (char *)"Provider", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    providerDefined = true;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"Provider\".\n");
    exit(1);
    break;
  }

  if (providerDefined) {
    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectFieldType", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Missing column \"ExpectFieldType\".\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectFieldType\".\n");
      exit(1);
      break;
    }

    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Missing column \"ExpectNumeric\".\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectNumeric\".\n");
      exit(1);
      break;
    }

    switch (SDDS_CheckColumn(&inSet, (char *)"ExpectElements", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    case SDDS_CHECK_NONEXISTENT:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Missing column \"ExpectElements\".\n");
      exit(1);
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ExpectElements\".\n");
      exit(1);
      break;
    }
  }

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;
  if (!(rows = SDDS_CountRowsOfInterest(&inSet)))
    SDDS_Bomb((char *)"Zero rows defined in trigger file.\n");
  /*read the trigger file */
  TriggerControlName = (char **)SDDS_GetColumn(&inSet, (char *)"TriggerControlName");
  if (majorDefined) {
    if (!(MajorAlarm = (short *)SDDS_GetColumn(&inSet, (char *)"MajorAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(MajorAlarm = (short *)calloc(rows, sizeof(*MajorAlarm))))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    /*allocated memory for MajorAlarm and assign 0 to its elements */
  }

  if (minorDefined) {
    if (!(MinorAlarm = (short *)SDDS_GetColumn(&inSet, (char *)"MinorAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(MinorAlarm = (short *)calloc(rows, sizeof(*MinorAlarm))))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    /*allocated memory for MinorrAlarm and assign 0 to its elements */
  }

  if (noalarmDefined) {
    if (!(NoAlarm = (short *)SDDS_GetColumn(&inSet, (char *)"NoAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(NoAlarm = (short *)calloc(rows, sizeof(*NoAlarm))))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    /*allocated memory for NoAlarm and assign 0 to its elements */
  }

  if (transitionThresholdDefined)
    if (!(TransitionThreshold = (double *)SDDS_GetColumn(&inSet, (char *)"TransitionThreshold")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (transitionDirectionDefined)
    if (!(TransitionDirection = (short *)SDDS_GetColumn(&inSet, (char *)"TransitionDirection")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (glitchThresholdDefined)
    if (!(GlitchThreshold = (double *)SDDS_GetColumn(&inSet, (char *)"GlitchThreshold")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (glitchBaselineSamplesDefined) {
    if (!(GlitchBaselineSamples = (long *)SDDS_GetColumn(&inSet, (char *)"GlitchBaselineSamples")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(GlitchBaselineSamples = (long *)calloc(rows, sizeof(*GlitchBaselineSamples))))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    /*allocated memory for GlitchBaselineSamples and assign 0 to its elements */
  }

  if (glitchAutoResetDefined) {
    if (!(GlitchBaselineAutoReset = (short *)SDDS_GetColumn(&inSet, (char *)"GlitchBaselineAutoReset")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(GlitchBaselineAutoReset = (short *)calloc(rows, sizeof(*GlitchBaselineAutoReset))))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    /*allocated memory for GlitchBaselineAutoReset and assign 0 to its elements */
  }
  if (SDDS_CheckColumn(&inSet, (char *)"GlitchScript", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY &&
      !(glitchScript = (char **)SDDS_GetColumn(&inSet, (char *)"GlitchScript")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (providerDefined) {
    if (!(provider = (char **)SDDS_GetColumn(&inSet, (char *)"Provider")))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    if (!(expectFieldType = (char **)SDDS_GetColumn(&inSet, (char *)"ExpectFieldType")))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    if (!(expectElements = (int32_t *)SDDS_GetColumn(&inSet, (char *)"ExpectElements")))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
    if (!(expectNumeric = (char *)SDDS_GetColumn(&inSet, (char *)"ExpectNumeric")))
      SDDS_Bomb((char *)"Memory allocation failure (getTriggerData)");
  }

  /*assign values to trigger requests */
  for (i = 0; i < rows; i++) {
    /*check for alarm triggers, major, minor and noalarm do not have to be defined at the same time */
    /*if none of them is defined, there is no alarm triggers */
    /*if all of them are 0, ignore alarm-based trigger of this PV */
    if (majorDefined || minorDefined || noalarmDefined) {
      if (MajorAlarm[i] || MinorAlarm[i] || NoAlarm[i]) {
        (*alarmTrigChannels)++;
        *alarmTrigRequest = (ALARMTRIG_REQUEST *)realloc(*alarmTrigRequest, sizeof(**alarmTrigRequest) * (*alarmTrigChannels));
        if (glitchScript)
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = glitchScript[i];
        else
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = NULL;

        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectFieldType =
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList =
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList = NULL;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList[0] = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList = NULL;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->flags = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->holdoff = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->trigStep = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->triggered = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->dataIndex = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectNumeric = true;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectElements = 1;
        /*only one dataset, and its index it 0 */
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MajorAlarm = MajorAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MinorAlarm = MinorAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->NoAlarm = NoAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
          (char *)malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName, TriggerControlName[i]);
        if (providerDefined) {
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider = (char *)malloc(sizeof(char) * (strlen(provider[i]) + 1));
          strcpy((*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider, provider[i]);
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectFieldType = (char *)malloc(sizeof(char) * (strlen(expectFieldType[i]) + 1));
          strcpy((*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectFieldType, expectFieldType[i]);
          if ((expectNumeric[i] == 'n') || (expectNumeric[i] == 'N'))
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectNumeric = false;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->expectElements = expectElements[i];
        } else {
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->provider = (char *)"ca";
        }
        if (MajorAlarm[i] && MinorAlarm[i] && NoAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MAJOR,MINOR,NO_ALARM");
        else if (MajorAlarm[i] && MinorAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MAJOR,MINOR");
        else if (MajorAlarm[i] && NoAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MAJOR,NO_ALARM");
        else if (MinorAlarm[i] && NoAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MINOR,NO_ALARM");
        else if (MajorAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MAJOR");
        else if (MinorAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "MINOR");
        else if (NoAlarm[i])
          strcat((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList, "NO_ALARM");
        if (!((*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusIndex =
              CSStringListsToIndexArray(&(*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusIndices,
                                        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList,
                                        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList,
                                        (char **)alarmStatusString,
                                        ALARM_NSTATUS)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        if (!((*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityIndex =
              CSStringListsToIndexArray(&(*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityIndices,
                                        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList,
                                        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList,
                                        (char **)alarmSeverityString,
                                        ALARM_NSEV)))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      }
    } /*end of if (majorDefined || minorDefined || noalarmDefind) */

    /*check for transition triggers, its threshold and direction have to be defined at the same time */
    /*if TransitionDirection is 0, ignore transitio-based trigger of this PV */
    if (transitionThresholdDefined && transitionDirectionDefined) {
      if (TransitionDirection[i]) {
        (logger->triggerChannels)++;
        logger->triggerRequest = (TRIGGER_REQUEST *)realloc(logger->triggerRequest, sizeof(TRIGGER_REQUEST) * (logger->triggerChannels));
        if (glitchScript)
          (logger->triggerRequest + (logger->triggerChannels) - 1)->glitchScript = glitchScript[i];
        else
          (logger->triggerRequest + (logger->triggerChannels) - 1)->glitchScript = NULL;

        if (providerDefined) {
          (logger->triggerRequest + (logger->triggerChannels) - 1)->provider = (char *)malloc(sizeof(char) * (strlen(provider[i]) + 1));
          strcpy((logger->triggerRequest + (logger->triggerChannels) - 1)->provider, provider[i]);
          (logger->triggerRequest + (logger->triggerChannels) - 1)->expectFieldType = (char *)malloc(sizeof(char) * (strlen(expectFieldType[i]) + 1));
          strcpy((logger->triggerRequest + (logger->triggerChannels) - 1)->expectFieldType, expectFieldType[i]);
          if ((expectNumeric[i] == 'n') || (expectNumeric[i] == 'N'))
            (logger->triggerRequest + (logger->triggerChannels) - 1)->expectNumeric = false;
          else
            (logger->triggerRequest + (logger->triggerChannels) - 1)->expectNumeric = true;
          (logger->triggerRequest + (logger->triggerChannels) - 1)->expectElements = expectElements[i];
        } else {
          (logger->triggerRequest + (logger->triggerChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (logger->triggerRequest + (logger->triggerChannels) - 1)->provider = (char *)"ca";
        }
        (logger->triggerRequest + (logger->triggerChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
        (logger->triggerRequest + (logger->triggerChannels) - 1)->message[0] = 0;
        (logger->triggerRequest + (logger->triggerChannels) - 1)->holdoff = 0;
        (logger->triggerRequest + (logger->triggerChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
        (logger->triggerRequest + (logger->triggerChannels) - 1)->trigStep = 0;
        (logger->triggerRequest + (logger->triggerChannels) - 1)->controlName =
          (char *)malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((logger->triggerRequest + (logger->triggerChannels) - 1)->controlName, TriggerControlName[i]);
        (logger->triggerRequest + (logger->triggerChannels) - 1)->triggerLevel = TransitionThreshold[i];
        (logger->triggerRequest + (logger->triggerChannels) - 1)->direction = TransitionDirection[i];
        (logger->triggerRequest + (logger->triggerChannels) - 1)->dataIndex = 0;
      }
    }

    /*check for glitch triggers, glitchThreshold has to be defined */
    /*if glitchThreshold is 0, ignore glitch-base trigger of this PV */
    if (glitchThresholdDefined) {
      if (GlitchThreshold[i]) {
        (logger->glitchChannels)++;
        logger->glitchRequest = (GLITCH_REQUEST *)realloc(logger->glitchRequest, sizeof(GLITCH_REQUEST) * (logger->glitchChannels));
        if (glitchScript)
          (logger->glitchRequest + (logger->glitchChannels) - 1)->glitchScript = glitchScript[i];
        else
          (logger->glitchRequest + (logger->glitchChannels) - 1)->glitchScript = NULL;

        if (providerDefined) {
          (logger->glitchRequest + (logger->glitchChannels) - 1)->provider = (char *)malloc(sizeof(char) * (strlen(provider[i]) + 1));
          strcpy((logger->glitchRequest + (logger->glitchChannels) - 1)->provider, provider[i]);
          (logger->glitchRequest + (logger->glitchChannels) - 1)->expectFieldType = (char *)malloc(sizeof(char) * (strlen(expectFieldType[i]) + 1));
          strcpy((logger->glitchRequest + (logger->glitchChannels) - 1)->expectFieldType, expectFieldType[i]);
          if ((expectNumeric[i] == 'n') || (expectNumeric[i] == 'N'))
            (logger->glitchRequest + (logger->glitchChannels) - 1)->expectNumeric = false;
          else
            (logger->glitchRequest + (logger->glitchChannels) - 1)->expectNumeric = true;
          (logger->glitchRequest + (logger->glitchChannels) - 1)->expectElements = expectElements[i];
        } else {
          (logger->glitchRequest + (logger->glitchChannels) - 1)->provider = (char *)malloc(sizeof(char) * 5);
          (logger->glitchRequest + (logger->glitchChannels) - 1)->provider = (char *)"ca";
        }
        (logger->glitchRequest + (logger->glitchChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
        (logger->glitchRequest + (logger->glitchChannels) - 1)->message[0] = 0;
        (logger->glitchRequest + (logger->glitchChannels) - 1)->delta =
          (logger->glitchRequest + (logger->glitchChannels) - 1)->holdoff = 0;
        (logger->glitchRequest + (logger->glitchChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
        (logger->glitchRequest + (logger->glitchChannels) - 1)->trigStep = 0;
        (logger->glitchRequest + (logger->glitchChannels) - 1)->controlName =
          (char *)malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((logger->glitchRequest + (logger->glitchChannels) - 1)->controlName, TriggerControlName[i]);
        (logger->glitchRequest + (logger->glitchChannels) - 1)->delta = GlitchThreshold[i];
        (logger->glitchRequest + (logger->glitchChannels) - 1)->baselineSamples = GlitchBaselineSamples[i];
        (logger->glitchRequest + (logger->glitchChannels) - 1)->baselineAutoReset = GlitchBaselineAutoReset[i];
        (logger->glitchRequest + (logger->glitchChannels) - 1)->dataIndex = 0;
      }
    }
  } /* end of for (i=0;i<rows;i++) */
  if (providerDefined) {
    for (i = 0; i < rows; i++) {
      free(provider[i]);
      if (expectFieldType[i] != NULL)
        free(expectFieldType[i]);
    }
    free(provider);
    free(expectFieldType);
    free(expectElements);
    free(expectNumeric);
  }
  free(MajorAlarm);
  free(MinorAlarm);
  free(NoAlarm);
  if (TransitionThreshold)
    free(TransitionThreshold);
  if (TransitionDirection)
    free(TransitionDirection);
  if (GlitchThreshold)
    free(GlitchThreshold);
  free(GlitchBaselineSamples);
  free(GlitchBaselineAutoReset);
  free(TriggerControlName);
  return 1;
}

void CheckTriggerAndFlushData(LOGGER_DATA *logger, ALARMTRIG_REQUEST **alarmTrigRequest, long alarmTrigChannels) {
  int i, a = 0, t = 0, g = 0, j, useArrays, index;
  char *PageTimeStamp;
  long caErrors = 0, executes = 0;
  char **executeScript = NULL;

  for (i = 0; i < logger->numDatasets; i++) {
    if (logger->verbose)
      fprintf(stdout, "glitchbefore counter[%d] %ld, pointsleft[%d] %ld\n", i,
              (logger->trigBeforeCount)[i], i, (logger->pointsLeft)[i]);
    if (!(logger->servicingTrigger)[i]) {
      if (!(logger->writingPage)[i]) {
        (logger->pointsLeft)[i] = logger->glitchAfter;
        if ((logger->trigBeforeCount)[i] < logger->glitchBefore)
          (logger->trigBeforeCount)[i]++;
      }
    } else {
      (logger->pointsLeft)[i]--;
      /*writing the data to file when buffer is full */
      if (((logger->pointsLeft)[i] < 0) || getTimeInSecs() > logger->quitingTime || logger->lastConditionsFailed) {
        (logger->writingPage)[i] = 1;
        logger->row = 0;
        if (logger->verbose)
          fprintf(stdout, "Writing Data ...\n");
        glitchNode[i] = (logger->datasets)[i].Node;
        /* search for first valid node of the circular buffer after the glitch node*/
        (logger->datasets)[i].Node = (logger->datasets)[i].Node->next;
        while (!(logger->datasets)[i].Node->hasdata) {
          (logger->datasets)[i].Node = (logger->datasets)[i].Node->next;
        }

        if ((logger->disconnected)[i]) {
          if (!SDDS_ReconnectFile((logger->datasets)[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          (logger->disconnected)[i] = 0;
        }

        if (!SDDS_StartPage((logger->datasets)[i].dataset, ((logger->trigBeforeCount)[i] + logger->glitchAfter + 1)))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        (logger->tableStarted)[i] = 1;

        if (!SDDS_CopyString(&PageTimeStamp, makeTimeStamp(getTimeInSecs())))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (!SDDS_SetParameters((logger->datasets)[i].dataset,
                                SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "TimeStamp", logger->TimeStamp, "PageTimeStamp", PageTimeStamp,
                                "StartTime", logger->StartTime, "StartYear", (short)(logger->StartYear),
                                "YearStartTime", computeYearStartTime(logger->StartTime),
                                "StartJulianDay", (short)(logger->StartJulianDay),
                                "StartMonth", (short)(logger->StartMonth),
                                "StartDayOfMonth", (short)(logger->StartDay), "StartHour",
                                (float)(logger->StartHour),
                                "TriggerTime", (logger->datasets)[i].TriggerTime, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if ((logger->datasets)[i].triggerType & GLITCHTRIGGER) {
          for (g = 0; g < logger->glitchChannels; g++) {
            if ((logger->glitched)[g]) {
              if ((logger->glitchRequest)[g].glitchScript && strlen((logger->glitchRequest)[g].glitchScript)) {
                if (!executes) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (logger->glitchRequest)[g].glitchScript;
                  executes++;
                } else if ((index = match_string((logger->glitchRequest)[g].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (logger->glitchRequest)[g].glitchScript;
                  executes++;
                }
              }
            }
            if ((logger->glitchRequest)[g].dataIndex == i) {
              if (!SDDS_SetParameters((logger->datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      (logger->glitchRequest)[g].parameterIndex,
                                      (logger->glitched)[g], -1))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              (logger->glitched)[g] = 0;
            }
          }
        }
        if ((logger->datasets)[i].triggerType & TRIGGER) {
          for (t = 0; t < logger->triggerChannels; t++) {
            if ((logger->triggered)[t]) {
              if ((logger->triggerRequest)[t].glitchScript && strlen((logger->triggerRequest)[t].glitchScript)) {
                if (!executes) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (logger->triggerRequest)[t].glitchScript;
                  executes++;
                } else if ((index = match_string((logger->triggerRequest)[t].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (logger->triggerRequest)[t].glitchScript;
                  executes++;
                }
              }
            }

            if ((logger->triggerRequest)[t].dataIndex == i) {
              if (!SDDS_SetParameters((logger->datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      (logger->triggerRequest)[t].parameterIndex,
                                      (logger->triggered)[t], -1))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              (logger->triggered)[t] = 0;
            }
          }
        }

        if ((logger->datasets)[i].triggerType & ALARMTRIGGER) {
          for (a = 0; a < alarmTrigChannels; a++) {
            if ((logger->alarmed)[a] || (logger->severity)[a]) {
              if ((*alarmTrigRequest)[a].glitchScript && strlen((*alarmTrigRequest)[a].glitchScript)) {
                if (!executes) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*alarmTrigRequest)[a].glitchScript;
                  executes++;
                } else if ((index = match_string((*alarmTrigRequest)[a].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = (char **)SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*alarmTrigRequest)[a].glitchScript;
                  executes++;
                }
              }
            }

            if ((*alarmTrigRequest)[a].dataIndex == i) {
              if ((logger->datasets)[i].triggerMode & ALARMTRIGGER) {
                if (!SDDS_SetParameters((logger->datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                        (*alarmTrigRequest)[a].severityParamIndex,
                                        (logger->severity)[a],
                                        (*alarmTrigRequest)[a].alarmParamIndex,
                                        (logger->alarmed)[a], -1))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                (logger->severity)[a] = 0;
              } else {
                if (!SDDS_SetParameters((logger->datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                        (*alarmTrigRequest)[a].severityParamIndex,
                                        (*alarmTrigRequest)[a].severity,
                                        (*alarmTrigRequest)[a].alarmParamIndex,
                                        (logger->alarmed)[a], -1))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              (logger->alarmed)[a] = 0;
            }
          }
        }
        do {
          if ((logger->datasets)[i].Node->hasdata) {
            if (logger->verbose) {
              printf("Writing values at step: %ld at time: %f  row :%ld for data[%d]\n",
                     (logger->datasets)[i].Node->step,
                     (logger->datasets)[i].Node->ElapsedTime, logger->row, i);
              fflush(stdout);
            }
            if ((logger->datasets)[i].triggerMode & TRIGGER) {
              t = (logger->datasets)[i].trigIndex;
              if ((logger->datasets)[i].Node->step <= (logger->triggerRequest)[t].trigStep) {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if ((logger->datasets)[i].triggerMode & GLITCHTRIGGER) {
              g = (logger->datasets)[i].glitchIndex;
              if ((logger->datasets)[i].Node->step <= (logger->glitchRequest)[g].trigStep) {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if ((logger->datasets)[i].triggerMode & ALARMTRIGGER) {
              a = (logger->datasets)[i].alarmIndex;
              if ((logger->datasets)[i].Node->step <= (*alarmTrigRequest)[a].trigStep) {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       logger->row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, logger->row,
                                   "Step", (logger->datasets)[i].Node->step,
                                   "Time", (logger->datasets)[i].Node->EpochTime,
                                   "TimeOfDay", (logger->datasets)[i].Node->TimeOfDay,
                                   "DayOfMonth", (logger->datasets)[i].Node->DayOfMonth, NULL))
              /* terminate with NULL if SDDS_BY_NAME or with -1 if SDDS_BY_INDEX */
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            useArrays = 0;
            for (j = 0; j < (logger->datasets)[i].variables; j++) {
              if (((logger->datasets + i)->elements)[j] == 1) {
                if (!SDDS_SetRowValues((logger->datasets)[i].dataset, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, logger->row,
                                       ((logger->datasets)[i].ReadbackIndex)[j], ((logger->datasets)[i].Node->values)[j][0], -1)) {
                  fprintf(stderr, "problem setting column %ld, row %ld\n", ((logger->datasets)[i].ReadbackIndex)[j], logger->row);
                  exit(1);
                }
              } else {
                useArrays = 1;
                if (!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, ((logger->datasets)[i].ReadbackName)[j], SDDS_CONTIGUOUS_DATA, ((logger->datasets)[i].Node->values)[j], ((logger->datasets + i)->elements)[j], logger->row + 1, ((logger->datasets + i)->elements)[j])) {
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              }
            }
            if (useArrays) {
              if ((!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, (char *)"Step", SDDS_CONTIGUOUS_DATA, &((logger->datasets)[i].Node->step), 1, logger->row + 1)) ||
                  (!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, (char *)"Time", SDDS_CONTIGUOUS_DATA, &((logger->datasets)[i].Node->EpochTime), 1, logger->row + 1)) ||
                  (!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, (char *)"TimeOfDay", SDDS_CONTIGUOUS_DATA, &((logger->datasets)[i].Node->TimeOfDay), 1, logger->row + 1)) ||
                  (!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, (char *)"DayOfMonth", SDDS_CONTIGUOUS_DATA, &((logger->datasets)[i].Node->DayOfMonth), 1, logger->row + 1)) ||
                  (!SDDS_AppendToArrayVararg((logger->datasets)[i].dataset, (char *)"CAerrors", SDDS_CONTIGUOUS_DATA, &(caErrors), 1, logger->row + 1))) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
          }
          (logger->datasets)[i].Node->hasdata = 0;
          (logger->datasets)[i].Node = (logger->datasets)[i].Node->next;
          logger->row++;
        } while ((logger->datasets)[i].Node != glitchNode[i]->next);
        if (!(logger->disconnected)[i]) {
          if (!SDDS_DisconnectFile((logger->datasets)[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          (logger->disconnected)[i] = 1;
        }
        (logger->servicingTrigger)[i] = 0;
        (logger->writingPage)[i] = 0;
        (logger->trigBeforeCount)[i] = 0;
        (logger->pointsLeft)[i] = logger->glitchAfter;
        (logger->datasets)[i].Node = glitchNode[i]->next;
        if ((logger->datasets)[i].triggerMode & TRIGGER) {
          (logger->triggerRequest)[t].triggerArmed = 1;
          (logger->triggerRequest)[t].trigStep = 0;
        }
        if ((logger->datasets)[i].triggerMode & ALARMTRIGGER) {
          (*alarmTrigRequest)[a].trigStep = 0;
        }
        if ((logger->datasets)[i].triggerMode & GLITCHTRIGGER) {
          (logger->glitchRequest)[g].trigStep = 0;
          (logger->glitchRequest)[g].glitchArmed = 1;
        }
        (logger->datasets)[i].triggerMode = 0;
      }
    } /* end of if (!servingTrigger )*/
  }   /* end of for (i<logger->numDatasets) */
  if (executes) {
    for (i = 0; i < executes; i++) {
      if (logger->verbose)
        fprintf(stdout, "Executing %s\n", executeScript[i]);
      system(executeScript[i]);
    }
    free(executeScript);
  }
}

//ca_search ca_array_get logger->datasets[index].ControlName[i]
long ReadCAValues(LOGGER_DATA *logger, long index) {
  long i, j, caErrors;

  caErrors = 0;
  if (!logger->datasets[index].variables)
    return 0;
  if (!logger->datasets[index].ControlName || !logger->datasets[index].values) {
    fprintf(stderr, "Error: null pointer passed to ReadCAValues()\n");
    return (-1);
  }
  if (GetPVAValues(logger->datasets[index].pva) == 1) {
    return (-1);
  }
  for (i = 0; i < logger->datasets[index].pva->numPVs; i++) {
    if (logger->datasets[index].pva->isConnected[i] == true) {
      for (j = 0; j < logger->datasets[index].pva->pvaData[i].numGetElements; j++)
        logger->datasets[index].values[i][j] = logger->datasets[index].pva->pvaData[i].getData[0].values[j];
    } else {
      for (j = 0; j < logger->datasets[index].ExpectElements[i]; j++)
        logger->datasets[index].values[i][j] = 0;
    }
  }
  caErrors = logger->datasets[index].pva->numNotConnected;
  freePVAGetReadings(logger->datasets[index].pva);
  return caErrors;
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
  if (rcParam.PV) {
    rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
  }
}

long InitiateRunControl() {
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
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n");
        fprintf(stderr, "\tor has the same description string,\n");
        fprintf(stderr, "\tor the runcontrol record has not been cleared from previous use.\n");
      }
      return (1);
    }
    strcpy(rcParam.message, "Initializing");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      return (1);
    }
    atexit(rc_interrupt_handler);

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
  }
  return (0);
}

long PingRunControl() {
  if (rcParam.PV) {
    rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
    switch (rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
      return (1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      strcpy(rcParam.message, "Application timed-out");
      runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
      return (1);
      break;
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Communications error with runcontrol record.\n");
      return (1);
    default:
      fprintf(stderr, "Unknown run control error code.\n");
      return (1);
      break;
    }
    strcpy(rcParam.message, "Running");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      return (1);
    }
  }
  return (0);
}

long pvaThreadSleep(long double seconds) {
  struct timespec delayTime;
  struct timespec remainingTime;
  long double targetTime;

  if (seconds > 0) {
    targetTime = getLongDoubleTimeInSecs() + seconds;
    while (seconds > 5) {
      delayTime.tv_sec = 5;
      delayTime.tv_nsec = 0;
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        delayTime = remainingTime;
      }
      if (PingRunControl() != 0) //Ping the run control once every 5 seconds
        {
          return (1);
        }
      seconds = targetTime - getLongDoubleTimeInSecs();
    }

    delayTime.tv_sec = seconds;
    delayTime.tv_nsec = (seconds - delayTime.tv_sec) * 1e9;
    while (nanosleep(&delayTime, &remainingTime) == -1 &&
           errno == EINTR)
      delayTime = remainingTime;
  }
  return (0);
}
