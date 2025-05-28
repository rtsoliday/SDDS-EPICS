/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
  sddsglitchlogger.c (modifying based on sddsmonitor)
  programmed by Hairong Shang, June, 2001
  * $Log: not supported by cvs2svn $
  * Revision 1.19  2010/08/30 14:53:35  soliday
  * Updated to continue running if the trigger PV can't be connected. Otherwise
  * it was just restarting every 10 minutes and creating a bunch of empty files.
  *
  * Revision 1.18  2010/04/16 18:32:52  soliday
  * Updated so that verbose output goes to stdout instead of stderr.
  *
  * Revision 1.17  2009/12/04 16:49:57  soliday
  * Improved the last changes.
  *
  * Revision 1.16  2009/12/03 22:13:59  soliday
  * Updated by moving the needed definitions from alarm.h and alarmString.h
  * into this file directly. This was needed because of a problem with
  * EPICS Base 3.14.11. Andrew Johnson said that the values for these
  * definitions will not change in the future.
  *
  * Revision 1.15  2006/08/17 21:30:04  shang
  * added GlitchScript as an attribute of the glitch triggers. If the corresponding glitch triggered,
  * the GlitchScript will be executed after date is written.
  *
  * Revision 1.14  2006/07/06 20:00:37  soliday
  * Modified to reduce the load on the runcontrol IOC.
  *
  * Revision 1.13  2006/04/04 21:44:18  soliday
  * Xuesong Jiao modified the error messages if one or more PVs don't connect.
  * It will now print out all the non-connecting PV names.
  *
  * Revision 1.12  2005/12/16 16:24:56  soliday
  * First, I have added the -delay=<steps> option. Which creates a circular
  * buffer of the glitch and trigger values. When the trigger PV is read it
  * get sent to the back of the buffer and the value at the front of the
  * buffer gets used instead of the current value. So if it had a -delay=30
  * it would not read the PVs until 30 steps after it tripped. Second, there
  * was no way to turn the -circularBuffer off before. I have changed it so
  * that -circular=before=0,after=0 is a valid option. I also had to change
  * the order of reading PVs. It used to read the non-trigger PVs for the
  * circular buffer before reading the trigger PVs. This happened for every
  * step. I switched this around and I have it set so that the non-trigger
  * PVs will not be read if -circular=before=0 is set and no
  * trigger/glitch/alarm has occured. This will reduce the CA activity.
  *
  * Revision 1.11  2005/12/12 16:21:15  soliday
  * Fixed two usage message typos.
  *
  * Revision 1.10  2005/11/09 19:42:30  soliday
  * Updated to remove compiler warnings on Linux
  *
  * Revision 1.9  2005/11/08 22:05:03  soliday
  * Added support for 64 bit compilers.
  *
  * Revision 1.8  2005/08/25 16:07:32  soliday
  * Added runcontrol support.
  *
  * Revision 1.7  2004/11/04 16:34:53  shang
  * the watchInput feature now watches both the name of the final link-to file and the state of the input file.
  *
  * Revision 1.6  2004/09/27 16:19:01  soliday
  * Added missing ca_task_exit call.
  *
  * Revision 1.5  2004/07/19 17:39:37  soliday
  * Updated the usage message to include the epics version string.
  *
  * Revision 1.4  2004/07/15 19:38:34  soliday
  * Replace sleep commands with ca_pend_event commands because Epics/Base 3.14.x
  * has an inactivity timer that was causing disconnects from PVs when the
  * log interval time was too large.
  *
  * Revision 1.3  2004/05/13 18:39:05  soliday
  * Added the ability to log waveforms with sddsglitchlogger.
  *
  * Revision 1.2  2004/05/06 19:15:01  soliday
  * Indented code.
  *
  * Revision 1.1  2003/08/27 19:51:14  soliday
  * Moved into subdirectory
  *
  * Revision 1.17  2002/10/15 19:13:41  soliday
  * Modified so that sddslogger, sddsglitchlogger, and sddsstatmon no longer
  * use ezca.
  *
  * Revision 1.16  2002/08/14 20:00:34  soliday
  * Added Open License
  *
  * Revision 1.15  2002/07/02 21:50:28  soliday
  * Improved the last change.
  *
  * Revision 1.14  2002/07/02 20:24:44  soliday
  * Added the ability to detect orphaned lock files.
  *
  * Revision 1.13  2002/07/01 21:12:10  soliday
  * Fsync is no longer the default so it is now set after the file is opened.
  *
  * Revision 1.12  2002/04/08 17:08:33  borland
  * Added required extra argument to MakeDailyGenerationFilename() calls.
  *
  * Revision 1.11  2002/01/03 22:11:41  soliday
  * Fixed problems on WIN32.
  *
  * Revision 1.10  2001/11/15 15:31:18  borland
  * Added verbose qualifier to -lockFile option, which causes the program to
  * announce when it is successful in obtaining a file lock.
  *
  * Revision 1.9  2001/10/16 20:57:38  shang
  * replaced st_mtime by st_ctime to make it work for linking files, and removed -enforceTimeLimit option since
  * the program always enforces the time limit.
  *
  * Revision 1.8  2001/09/10 20:58:06  soliday
  * Fixed Linux compiler warnings.
  *
  * Revision 1.7  2001/08/08 17:02:36  shang
  * add watchInput feature
  *
  * Revision 1.6  2001/07/31 13:53:39  borland
  * REvised day boundary check again to avoid possible problems with fast-sampling
  * loggers.
  *
  * Revision 1.5  2001/07/30 22:32:56  borland
  * Revised the code that checks for passing of the midnight boundary to make
  * it more robust.
  *
  * Revision 1.4  2001/07/23 19:21:23  borland
  * Pre-loop code for inhibit-PV checking is now like the others.  It will
  * exit if no permission to run
  *
  * Revision 1.3  2001/07/20 16:43:01  borland
  * Added -lockFile option to allow preventing multiply instances using a
  * locked file.
  *
  * Revision 1.2  2001/07/16 20:21:30  borland
  * Added CVS header.
  *
  *
  */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <alarm.h>
/* If you get an error compiling this next few lines it
   means that the number or elements in alarmSeverityString
   and/or alarmStatusString have to change to match those
   in EPICS/Base */
#define COMPILE_TIME_ASSERT(expr) char constraint[expr]
COMPILE_TIME_ASSERT(ALARM_NSEV == 4);
COMPILE_TIME_ASSERT(ALARM_NSTATUS == 22);
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

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

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
  char *controlName;
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
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that reponds to this trigger*/
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
  char *controlName, *message;
  double baselineValue, holdoff;
  double delta;
  short baselineAutoReset, glitchArmed;
  long parameterIndex, baselineSamples;
  long dataIndex, trigStep;
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that reponds to this trigger*/
  /* trigStep: the Step where trigger occurs */
  /* parameterIndex: the parameterIndex in corresponding dataset */
  char *glitchScript;
} GLITCH_REQUEST;

typedef struct
{
  short triggerArmed;
  unsigned long flags;
  char *controlName, *message;
  double value;
  short direction;
  /*direction=1, transition from below threshold to above threshold results in buffer dump */
  /*direction=-1, transition from above threshold to below threshold results in buffer dump */
  /*direction=0, ignore transition-based trigger for this PV */
  double holdoff, triggerLevel;
  long dataIndex, trigStep, parameterIndex;
  /* dataIndex: the dataset index of the trigger,i.e., the index of dataset that reponds to this trigger*/
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
  char *TriggerControlName;
  short triggerType, triggerMode; /*ALARMTRIGGER, TRIGGER, or GLITCHTRIGGER */
  long variables;
  double **values;
  long *elements;
  double TriggerTime;
  long *ReadbackIndex, alarmIndex, glitchIndex, trigIndex;
  DATANODE *firstNode, *Node;
  chid *chid;
} DATASETS;

/* DATASETS *datasets; */

#define NTimeUnitNames 4
static char *TimeUnitNames[NTimeUnitNames] = {
  "seconds",
  "minutes",
  "hours",
  "days",
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
  "sampleInterval", "circularBuffer", "holdoffTime",
  "autoHoldoff", "inhibitPV", "conditions", "verbose", "time", "triggerFile",
  "lockFile", "watchInput", "pendiotime", "runControlPV", "runControlDescription",
  "delay"};

static char *USAGE1 = "sddsglitchlogger <input> <outputDirectory>|<outputRootname>\n\
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
static char *USAGE2 = "<input>            the name of the input file\n\
                   the input file contains ControlName, ReabbackName, and ReadbackUnits \n\
                   colummns and it has following parameters: \n\
                   1. OutputRootname(string) -- the rootname for output file that log PVs \n\
                      defined in the input file. Different OutputRootmames go to different \n\
                      output files. If different pages have the same OutputRootname,\n\
                      the columns defined in new pages are ignored. Only the trigger \n\
                      defined in newer page is counted. \n\
                   2. GlitchScript(string) -- optional, provides the command to execute \n\
                      whenever a glitch occurs.\n\
                   a. TriggerControlName (string) -- PV to look at to determine\n\
                      if a glitch/trigger has occured. \n\
                   b. MajorAlarm (short) -- if nonzero, then severity of MAJOR on \n\
                                            TriggerControlName, results in a buffer dump \n\
                   c. MinorAlarm (short) -- if nonzero, then severity of MINOR results in \n\
                                            a buffer dump. \n\
                   d. NoAlarm (short)    -- if nonzero, then severity of NO_ALARM results \n\
                                            in a buffer dump. \n\
                      if all of the above are zero, ignore alarm-based trigger. \n";
static char *USAGE3 = "                   e. TransitionDirection (short) --- \n\
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
static char *USAGE4 = "<outputDirectory>  \n\
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
                   the correspoding glitch/trigger/alarm occurs.\n\
lockFile           when this option is given, sddsglitchlogger uses the named file to prevent\n\
                   running multiple versions of the program.  If the named file exists and is\n\
                   locked, the program exits.  If it does not exist or is not locked, it is\n\
                   created and locked.\n\
sampleInterval     desired interval in seconds for each reading step;\n\
time               Total time for mornitoring process variable or device.\n\
                   Valid time units are seconds,minutes,hours, or days. \n";
static char *USAGE5 = "verbose            prints out a message when data is taken.\n\
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
                   modifed. If the inputfile is modified,then read the input files again and \n\
                   start the logging. \n\
runControlPV       specifies a runControl PV name.\n\
runControlDescription\n\
                   specifies a runControl PV description record.\n\n\
Program by M. Borland, H. Shang, R. Soliday ANL/APS\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV=string=<string>\n";
static char *runControlDescUsage = "-runControlDescription=string=<string>\n";
#endif

#ifdef USE_RUNCONTROL
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
#endif

long IsGlitch(double value, double baseline, double delta); /*determine if PV is a glitch */
long IsTrigger(double previousValue, double currentValue, double level, short direction);
/*determine if PV is a trigger */
long DefineGlitchParameters(SDDS_DATASET *SDDSout, GLITCH_REQUEST *glitchRequest,
                            long glitchRequests, TRIGGER_REQUEST *triggerRequest, long triggerRequests,
                            ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests, long dataIndex);
/*define the trigger parameter and trigger parameter index for dataset with index of dataIndex */

long *CSStringListsToIndexArray(long *arraySize, char *includeList,
                                char *excludeList, char **validItem, long validItems);
void alarmTriggerEventHandler(struct event_handler_args event);
long setUpAlarmTriggerCallbacks(ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigChannels);
/* the above three functions are copied from sddsmonitor.c */

long getGlitchLoggerData(DATASETS **datasets, long *numDatasets, GLITCH_REQUEST **glitchRequest,
                         long *glitchChannels, ALARMTRIG_REQUEST **alarmTrigRequest,
                         long *alarmTrigChannels, TRIGGER_REQUEST **triggerRequest,
                         long *triggerChannels,
                         char *inputFile, short trigFileSet);

/*read the input file, assign values to datasets, glitchRequest,triggerRequest and alarmTrigRequest.
  for each trigger request, there is one corresponding dataset indexed by its dataIndex member.
  each page may have one alarm trigger, and/or one glitchTrigger, and/or regular trigger (the same
  PV may act as alarm, trigger or glitch).
  different pages in the input with same OutputRootname parameter go to one output file, that is,
  one outputfile (dataset) may respond to multiple triggers. 
  when triggerFile is given, the input only contains logger PV's information, the trigger's
  information is obtained through getTriggerData*/

long getTriggerData(GLITCH_REQUEST **glitchRequest, long *glitchChannels,
                    ALARMTRIG_REQUEST **alarmTrigRequest,
                    long *alarmTrigChannels,
                    TRIGGER_REQUEST **triggerRequest, long *triggerChannels,
                    char *triggerFile);
/*it is called only when triggerFile is given and set the values to triggers */

void startGlitchLogFiles(DATASETS **datasets, long numDatasets,
                         ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests,
                         GLITCH_REQUEST *glitchRequest, long glitchRequests,
                         TRIGGER_REQUEST *triggerRequest, long triggerRequests,
                         double *StartTime, double *StartDay, double *StartHour,
                         double *StartJulianDay, double *StartMonth,
                         double *StartYear, char **TimeStamp, long glitchBefore, long glitchAfter);

void CheckTriggerAndFlushData(DATASETS **datasets, long numDatasets, long **trigBeforeCount,
                              long **pointsLeft, short **servicingTrigger, short **writingPage,
                              short **disconnected, short **tableStarted,
                              GLITCH_REQUEST **glitchRequest, long glitchChannels,
                              ALARMTRIG_REQUEST **alarmTrigRequest, long alarmTrigChannels,
                              TRIGGER_REQUEST **triggerRequest, long triggerChannels,
                              short **triggered, short **severity, short **alarmed, short **glitched,
                              double quitingTime, long glitchAfter, long glitchBefore,
                              double StartTime, double StartHour, double StartJulianDay,
                              double StartDay, double StartYear, double StartMonth,
                              char *TimeStamp, long verbose, long row, long lastConditionsFailed);

long ReadCAValues(char **deviceName, char **readMessage,
                  double *scaleFactor, double **value, long *elements, long n,
                  chid *cid, double pendIOTime);

#define DEBUG 1

int runControlPingWhileSleep(double sleepTime);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

volatile int sigint = 0;

int main(int argc, char **argv) {
  DATASETS *datasets;
  SCANNED_ARG *s_arg;
  GLITCH_REQUEST *glitchRequest;
  TRIGGER_REQUEST *triggerRequest;
  long triggerChannels = 0, glitchChannels = 0;
  char *triggerFile = NULL, *lockFile = NULL;
  short trigFileSet = 0, lockFileVerbose = 0;
  FILE *fpLock = NULL;

  int readCAvalues = 1;
  /*monitoring time parameters */
  long totalTimeSet = 0;
  double TotalTime, quitingTime, PingTime;
  /* -sampleInterval */
  double TimeInterval = DEFAULT_TIME_INTERVAL;

  short *disconnected, *servicingTrigger, *writingPage, *severity = NULL, *tableStarted;
  long row = 0, *pointsLeft, TimeUnits;

  long i, j, k, i_arg, optionCode, numDatasets = 0;
  unsigned long dummyFlags;
  long iGlitch, iAlarm, iTrigger;

  char *inputfile = NULL, *outputDir = NULL;

  double HourNow, LastHour;
  char temp[1024];
  double StartTime, StartHour, StartJulianDay, StartDay, StartYear, StartMonth;
  double EpochTime, TimeOfDay, DayOfMonth;
  char *TimeStamp;
  long updateInterval = DEFAULT_UPDATEINTERVAL;
  /* long initialOutputRow=0; */
  /* 
     double ezcaTimeout=0.010;
     long ezcaRetries=1000;
  */
  long lastConditionsFailed = 0;
  long Step;
  double ElapsedTime;
  long CAerrors = 0, verbose = 0;

  long baselineStarted = 0;
  char **triggerControlName = NULL, **triggerControlMessage = NULL;
  char **glitchControlName = NULL, **glitchControlMessage = NULL;
  double *glitchValue = NULL, *triggerValue = NULL;
  double **delayedGlitchValue = NULL, **delayedTriggerValue = NULL;
  double tmpValue;
  int delayedGlitchValuesInitialized = 0, delayedTriggerValuesInitialized = 0;
  chid *triggerCHID = NULL, *glitchCHID = NULL;
  short *glitched = NULL;  /* array of trigger states for glitch channels */
  short *triggered = NULL; /* array of trigger states for trigger channels */
  short *alarmed = NULL;   /* array of alarm states (0 or 1) for alarm trigger channels */

  int glitchDelay = 0;
  /* -circularBuffer */
  int32_t glitchBefore, glitchAfter;
  long *trigBeforeCount;
  long bufferlength = 0;
  /* -holdoffTime    */
  double defaultHoldoffTime = 0;
  /* -autoHoldoff    */
  long autoHoldoff = 0;
  double pendIOtime = 10.0;
  /* -inhibitPV      */
  double InhibitPendIOTime = 10;
  long inhibit = 0;
  int32_t InhibitWaitTime = 5;
  chid inhibitID;
  PV_VALUE InhibitPV;
  /* -conditions     */
  unsigned long CondMode = 0;
  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile = NULL;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  long conditions;
  double *CondDataBuffer = NULL;
  chid *CondCHID = NULL;

  char *trig_linkname = NULL;
  char *input_linkname = NULL;
  struct stat input_filestat;
  struct stat trig_filestat;
  int watchInput = 0;
  /* long old_datasets,old_alarms,old_triggers,old_glitchs; */
  long useArrays;

  alarmTrigRequest = NULL;
  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5);
    return (1);
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

#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  glitchBefore = GLITCH_BEFORE_DEFAULT;
  glitchAfter = GLITCH_AFTER_DEFAULT;
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (optionCode = match_string(s_arg[i_arg].list[0],
                                        commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TimeInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -sampleInterval", NULL);
        break;
      case CLO_CIRCULARBUFFER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "before", SDDS_LONG, &glitchBefore, 1, 0,
                          "after", SDDS_LONG, &glitchAfter, 1, 0,
                          NULL) ||
            glitchBefore < 0 || glitchAfter < 0)
          SDDS_Bomb("invalid -circularBuffer syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DELAY:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%d", &glitchDelay) != 1 ||
            glitchDelay < 0)
          SDDS_Bomb("invalid -glitchDelay syntax/value");
        break;
      case CLO_HOLDOFFTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &defaultHoldoffTime) != 1 ||
            defaultHoldoffTime < 0)
          SDDS_Bomb("invalid -holdoffTime syntax/value");
        break;
      case CLO_AUTOHOLDOFF:
        autoHoldoff = 1;
        break;
      case CLO_INHIBITPV:
        InhibitPendIOTime = 10;
        InhibitWaitTime = 5;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &InhibitPV.name, 1, 0,
                          "pendiotime", SDDS_DOUBLE, &InhibitPendIOTime, 1, 0,
                          "waittime", SDDS_LONG, &InhibitWaitTime, 1, 0,
                          NULL) ||
            !InhibitPV.name || !strlen(InhibitPV.name) ||
            InhibitPendIOTime <= 0 || InhibitWaitTime <= 0)
          SDDS_Bomb("invalid -inhibitPV syntax/values");
        inhibit = 1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb("invalid -conditions syntax/values");
        SDDS_CopyString(&CondFile, s_arg[i_arg].list[1]);
        if (SDDS_StringIsBlank(CondFile) ||
            !(CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2)))
          SDDS_Bomb("invalid -conditions syntax/values");
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TotalTime, s_arg[i_arg].list[1])))
          bomb("no value given for option -time\n", NULL);
        totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            TotalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_TRIGGERFILE:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid syntax for -triggerFile\n", NULL);
        SDDS_CopyString(&triggerFile, s_arg[i_arg].list[1]);
        trigFileSet = 1;
        break;
      case CLO_LOCKFILE:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3)
          bomb("invalid syntax for -lockFile\n", NULL);
        SDDS_CopyString(&lockFile, s_arg[i_arg].list[1]);
        if (s_arg[i_arg].n_items == 3) {
          str_tolower(s_arg[i_arg].list[2]);
          if (strncmp(s_arg[i_arg].list[2], "verbose",
                      strlen(s_arg[i_arg].list[2])) != 0)
            bomb("invalid syntax for -lockFile\n", NULL);
          lockFileVerbose = 1;
        }
        break;
      case CLO_WATCHINPUT:
        watchInput = 1;
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PV))
          bomb("bad -runControlPV syntax", runControlPVUsage);
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          return (1);
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
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc))
          bomb("bad -runControlDesc syntax", runControlDescUsage);
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (!inputfile)
        SDDS_CopyString(&inputfile, s_arg[i_arg].list[0]);
      else if (!outputDir)
        SDDS_CopyString(&outputDir, s_arg[i_arg].list[0]);
      else
        SDDS_Bomb("too many filenames given");
    }
  }

  if (inhibit) {
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 1)) {
      fprintf(stderr, "error: problem doing search for Inhibit PV %s\n", InhibitPV.name);
      return (1);
    }
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stdout, "Data collection inhibited---not starting\n");
      return (0);
    }
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
      return (1);
    }
  }
#endif
  atexit(rc_interrupt_handler);

  if (lockFile) {
    if (SDDS_FileIsLocked(lockFile)) {
#if !defined(_WIN32)
      static char s[100];
      long pid;
      if (!(fpLock = fopen(lockFile, "r")))
        SDDS_Bomb("unable to open lock file for reading");
      if (!fgets(s, 100, fpLock))
        SDDS_Bomb("unable to read pid number from lock file");
      if (strlen(s) > 0)
        s[strlen(s) - 1] = 0;
      pid = atol(s);
      if (!fgets(s, 100, fpLock))
        SDDS_Bomb("unable to read owner in lock file");
      if (strlen(s) > 0)
        s[strlen(s) - 1] = 0;
      fclose(fpLock);
      if (pid == 0)
        SDDS_Bomb("invalid pid number in lock file");
      /* check if this is a valid pid number 
             using kill with a signal value of 0 does this */
      if (kill(pid, 0) != 0) {
        if (remove(lockFile) != 0)
          SDDS_Bomb("unable to delete orphaned lock file");
      } else {
        char buf[100] = {0};
        char command[100];
        FILE *p;
        sprintf(command, "ps -o fname -p %ld | tail -1", pid);
        ;
        p = popen(command, "r");
        if (NULL == p) {
          perror("sddsglitchlogger");
          SDDS_Bomb("sddsglitchlogger: unexpected error");
        } else {
          if (NULL == fgets(buf, 100, p)) {
            if (0 != errno) {
              perror("sddsglitchlogger");
              SDDS_Bomb("sddsglitchlogger: unexpected error");
            }
          }
          pclose(p);
        }
        if (strlen(buf) > 0)
          buf[strlen(buf) - 1] = 0;
        if (strncmp(s, buf, 8) != 0) {
          if (remove(lockFile) != 0)
            SDDS_Bomb("unable to delete orphaned lock file");
        } else {
#endif
          fprintf(stderr, "File %s is locked---previous process still running--aborting.\n", lockFile);
          return (1);
#if !defined(_WIN32)
        }
      }
#endif
    }
    if (!(fpLock = fopen(lockFile, "w")))
      SDDS_Bomb("unable to open lock file");
    if (!SDDS_LockFile(fpLock, lockFile, "sddsglitchlogger"))
      SDDS_Bomb("unable to establish lock on lock file");
#if !defined(_WIN32)
    fprintf(fpLock, "%ld\nsddsglitchlogger\n", (long)(getpid()));
    fflush(fpLock);
#endif
    if (lockFileVerbose)
      fprintf(stdout, "Lock established on %s\n", lockFile);
  }

  if (!inputfile)
    bomb("input filename not given", NULL);
  if (!outputDir)
    bomb("output directory not given", NULL);
  if (!totalTimeSet)
    bomb("the time is not given", NULL);
  i = strlen(outputDir);
  if (trigFileSet) {
    if (outputDir[i - 1] == '/') {
      fprintf(stderr, "the outputRootname should not end with '/'\n");
      return (1);
    }
  } else {
    if (outputDir[i - 1] != '/') {
      temp[0] = 0;
      strcat(temp, outputDir);
      strcat(temp, "/");
      outputDir = (char *)malloc(sizeof(char) * (strlen(temp) + 1));
      outputDir[0] = 0;
      strcat(outputDir, temp);
    }
    temp[0] = 0;
  }

  if (watchInput) {
    input_linkname = read_last_link_to_file(inputfile);
    if (get_file_stat(inputfile, input_linkname, &input_filestat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputfile);
      return (1);
    }
    if (trigFileSet) {
      trig_linkname = read_last_link_to_file(triggerFile);
      if (get_file_stat(triggerFile, trig_linkname, &trig_filestat) != 0) {
        fprintf(stderr, "Problem getting modification time for %s\n", triggerFile);
        return (1);
      }
    }
  }

  if (CondFile) {
    if (!getConditionsData(&CondDeviceName, &CondReadMessage, &CondScaleFactor,
                           &CondLowerLimit, &CondUpperLimit, &CondHoldoff, &conditions,
                           CondFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!(CondDataBuffer = (double *)malloc(sizeof(*CondDataBuffer) * conditions)))
      SDDS_Bomb("allocation faliure");
    if (!(CondCHID = (chid *)malloc(sizeof(*CondCHID) * conditions)))
      SDDS_Bomb("allocation faliure");
    for (i = 0; i < conditions; i++)
      CondCHID[i] = NULL;
  }

  if (!getGlitchLoggerData(&datasets, &numDatasets, &glitchRequest,
                           &glitchChannels, &alarmTrigRequest,
                           &alarmTrigChannels, &triggerRequest, &triggerChannels,
                           inputfile, trigFileSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (trigFileSet) {
    if (numDatasets > 1)
      SDDS_Bomb("Both the input file and trigger file should be single-paged");
    if (!getTriggerData(&glitchRequest, &glitchChannels, &alarmTrigRequest,
                        &alarmTrigChannels, &triggerRequest, &triggerChannels,
                        triggerFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /*only one dataset, i.e, one output, numDatasets=1*/
    if (alarmTrigChannels)
      datasets[0].triggerType |= ALARMTRIGGER;
    if (triggerChannels)
      datasets[0].triggerType |= TRIGGER;
    if (glitchChannels)
      datasets[0].triggerType |= GLITCHTRIGGER;
    /*the OutputRootname of datasets[0] is empty, and outputDir is the OutputRootname */
    datasets[0].OutputRootname = (char *)malloc(sizeof(char) * 2);
    datasets[0].OutputRootname[0] = 0;
  }

  /* assert defaults for autoholdoff and holdoff time */
  if (glitchChannels) {
    for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
      if (autoHoldoff)
        glitchRequest[iGlitch].flags |= TRIGGER_AUTOHOLD;
      if (defaultHoldoffTime > 0 && !(glitchRequest[iGlitch].flags & TRIGGER_HOLDOFF)) {
        glitchRequest[iGlitch].holdoff = defaultHoldoffTime;
        glitchRequest[iGlitch].flags |= TRIGGER_HOLDOFF;
      }
    }
  }
  if (triggerChannels) {
    for (iTrigger = 0; iTrigger < triggerChannels; iTrigger++) {
      if (autoHoldoff)
        triggerRequest[iTrigger].flags |= TRIGGER_AUTOHOLD;
      if (defaultHoldoffTime > 0 && !(triggerRequest[iTrigger].flags & TRIGGER_HOLDOFF)) {
        triggerRequest[iTrigger].holdoff = defaultHoldoffTime;
        triggerRequest[iTrigger].flags |= TRIGGER_HOLDOFF;
      }
    }
  }
  if (alarmTrigChannels) {
    for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
      if (autoHoldoff)
        alarmTrigRequest[iAlarm].flags |= ALARMTRIG_AUTOHOLD;
      if (defaultHoldoffTime > 0 && !(alarmTrigRequest[iAlarm].flags & ALARMTRIG_HOLDOFF)) {
        alarmTrigRequest[iAlarm].holdoff = defaultHoldoffTime;
        alarmTrigRequest[iAlarm].flags |= ALARMTRIG_HOLDOFF;
      }
    }
  }

  disconnected = (short *)malloc(sizeof(*disconnected) * numDatasets);
  servicingTrigger = (short *)malloc(sizeof(*servicingTrigger) * numDatasets);
  writingPage = (short *)malloc(sizeof(*writingPage) * numDatasets);
  pointsLeft = (long *)malloc(sizeof(*pointsLeft) * numDatasets);
  trigBeforeCount = (long *)malloc(sizeof(*trigBeforeCount) * numDatasets);
  tableStarted = (short *)malloc(sizeof(*tableStarted) * numDatasets);
  /*generate outputFile name */
  for (i = 0; i < numDatasets; i++) {
    disconnected[i] = 0;
    servicingTrigger[i] = 0;
    writingPage[i] = 0;
    pointsLeft[i] = glitchAfter;
    trigBeforeCount[i] = 0;
    tableStarted[i] = 0;
    datasets[i].triggerMode = 0;

    sprintf(temp, "%s%s", outputDir, datasets[i].OutputRootname);
    sprintf(temp, "%s", MakeDailyGenerationFilename(temp, 4, ".", 0));
    datasets[i].outputFile = tmalloc(sizeof(char) * (strlen(temp) + 1));
    strcpy(datasets[i].outputFile, temp);
    if (verbose)
      fprintf(stdout, "output file is %s\n", datasets[i].outputFile);
#if DEBUG
    fprintf(stdout, "Initial output file is %s\n", datasets[i].outputFile);
#endif
  }

  for (i = 0; i < numDatasets; i++) {
    if (!(datasets[i].dataset = malloc(sizeof(*(datasets[i].dataset)))) ||
        !(datasets[i].values = malloc(sizeof(*(datasets[i].values)) * (datasets[i].variables))) ||
        !(datasets[i].elements = malloc(sizeof(*(datasets[i].elements)) * (datasets[i].variables))))
      SDDS_Bomb("Memory allocation failure!");
    if (!(datasets[i].chid = malloc(sizeof(*(datasets[i].chid)) * (datasets[i].variables))))
      SDDS_Bomb("Memory allocation failure!");
    for (j = 0; j < datasets[i].variables; j++)
      datasets[i].chid[j] = NULL;
  }

  for (i = 0; i < numDatasets; i++) {
    for (j = 0; j < datasets[i].variables; j++) {
      if (ca_search(datasets[i].ControlName[j], &(datasets[i].chid[j])) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", datasets[i].ControlName[j]);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < numDatasets; i++) {
      for (j = 0; j < datasets[i].variables; j++) {
        if (ca_state(datasets[i].chid[j]) != cs_conn)
          fprintf(stderr, "%s not connected\n", datasets[i].ControlName[j]);
      }
    }
  }
  for (i = 0; i < numDatasets; i++) {
    for (j = 0; j < datasets[i].variables; j++) {
      if (ca_state(datasets[i].chid[j]) == cs_conn) {
        datasets[i].elements[j] = ca_element_count(datasets[i].chid[j]);
      } else {
        datasets[i].elements[j] = 1;
      }
      if (!(datasets[i].values[j] = malloc(sizeof(*(datasets[i].values[j])) * datasets[i].elements[j])))
        SDDS_Bomb("Memory allocation failure!");
    }
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    if (runControlPingWhileSleep(0)) {
      fprintf(stderr, "Problem pinging the run control record1.\n");
      return (1);
    }
    strcpy(rcParam.message, "Running");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      return (1);
    }
  }
#endif
  if (sigint)
    return (1);

  startGlitchLogFiles(&datasets, numDatasets, alarmTrigRequest, alarmTrigChannels,
                      glitchRequest, glitchChannels,
                      triggerRequest, triggerChannels,
                      &StartTime, &StartDay, &StartHour,
                      &StartJulianDay, &StartMonth,
                      &StartYear, &TimeStamp, glitchBefore, glitchAfter);
  for (i = 0; i < numDatasets; i++) {
    /*set disconnected to 1, since the datasets are disconnceted in startGlitchLogFiles procedure*/
    disconnected[i] = 1;
  }
  glitchNode = malloc(sizeof(*glitchNode) * numDatasets);

  if (!glitchChannels && !alarmTrigChannels && !triggerChannels) {
    for (i = 0; i < numDatasets; i++) {
      if (disconnected[i])
        if (!SDDS_ReconnectFile(datasets[i].dataset))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (!SDDS_Terminate(datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  } else {
    /* glitch/trigger and/or alarm trigger */
    /* make buffers*/
    bufferlength = glitchBefore + 1 + glitchAfter;
    for (i = 0; i < numDatasets; i++) {
      datasets[i].Node = (DATANODE *)malloc(sizeof(DATANODE));
      datasets[i].Node->values = (double **)malloc(sizeof(double *) * datasets[i].variables);
      datasets[i].Node->elements = (long *)malloc(sizeof(long) * datasets[i].variables);
      for (k = 0; k < datasets[i].variables; k++) {
        datasets[i].Node->values[k] = (double *)malloc(sizeof(double) * datasets[i].elements[k]);
        datasets[i].Node->elements[k] = datasets[i].elements[k];
      }
      datasets[i].Node->hasdata = 0;
      datasets[i].firstNode = datasets[i].Node;
      for (j = 1; j < bufferlength; j++) {
        datasets[i].Node->next = (DATANODE *)malloc(sizeof(DATANODE));
        datasets[i].Node = datasets[i].Node->next;
        datasets[i].Node->values = (double **)malloc(sizeof(double *) * datasets[i].variables);
        datasets[i].Node->elements = (long *)malloc(sizeof(long) * datasets[i].variables);
        for (k = 0; k < datasets[i].variables; k++) {
          datasets[i].Node->values[k] = (double *)malloc(sizeof(double) * datasets[i].elements[k]);
          datasets[i].Node->elements[k] = datasets[i].elements[k];
        }
        datasets[i].Node->hasdata = 0;
      }
      datasets[i].Node->next = datasets[i].firstNode;
      datasets[i].Node = datasets[i].firstNode;
    }

    for (iTrigger = 0; iTrigger < triggerChannels; iTrigger++)
      triggerRequest[iTrigger].triggerArmed = 1;
    for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
      glitchRequest[iGlitch].glitchArmed = 1;

    /* make copies of control-names and messages for convenient use with CA routine */
    if (!(glitchControlName = malloc(sizeof(*glitchControlName) * glitchChannels)) ||
        !(glitchControlMessage = malloc(sizeof(*glitchControlMessage) * glitchChannels)) ||
        !(glitchValue = malloc(sizeof(*glitchValue) * glitchChannels)) ||
        !(glitchCHID = malloc(sizeof(*glitchCHID) * glitchChannels)) ||
        !(glitched = malloc(sizeof(*glitched) * glitchChannels)) ||
        !(triggerControlName = malloc(sizeof(*triggerControlName) * triggerChannels)) ||
        !(triggerControlMessage = malloc(sizeof(*triggerControlMessage) * triggerChannels)) ||
        !(triggerValue = malloc(sizeof(*triggerValue) * triggerChannels)) ||
        !(triggerCHID = malloc(sizeof(*triggerCHID) * triggerChannels)) ||
        !(triggered = malloc(sizeof(*triggered) * triggerChannels)) ||
        !(alarmed = malloc(sizeof(*alarmed) * alarmTrigChannels)) ||
        !(severity = malloc(sizeof(*severity) * alarmTrigChannels)))
      SDDS_Bomb("memory allocation failure (glitch data copies)");
    if (!(delayedGlitchValue = malloc(sizeof(*delayedGlitchValue) * (glitchDelay + 1))) ||
        !(delayedTriggerValue = malloc(sizeof(*delayedTriggerValue) * (glitchDelay + 1))))
      SDDS_Bomb("memory allocation failure (glitch data copies)");
    for (i = 0; i <= glitchDelay; i++) {
      if (!(delayedGlitchValue[i] = malloc(sizeof(*(delayedGlitchValue[i])) * glitchChannels)))
        SDDS_Bomb("memory allocation failure (glitch data copies)");
    }
    for (i = 0; i <= glitchDelay; i++) {
      if (!(delayedTriggerValue[i] = malloc(sizeof(*(delayedTriggerValue[i])) * triggerChannels)))
        SDDS_Bomb("memory allocation failure (glitch data copies)");
    }
    for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
      glitchCHID[iGlitch] = NULL;
      if (!SDDS_CopyString(glitchControlName + iGlitch, glitchRequest[iGlitch].controlName))
        SDDS_Bomb("memory allocation failure (glitch data copies)");
      if (glitchRequest[iGlitch].message != NULL) {
        if (!SDDS_CopyString(glitchControlMessage + iGlitch, glitchRequest[iGlitch].message))
          SDDS_Bomb("memory allocation failure (glitch data copies)");
        else
          glitchControlMessage[iGlitch][0] = 0;
      }
      glitched[iGlitch] = 0;
    }
    for (iTrigger = 0; iTrigger < triggerChannels; iTrigger++) {
      triggerCHID[iTrigger] = NULL;
      if (!SDDS_CopyString(triggerControlName + iTrigger, triggerRequest[iTrigger].controlName))
        SDDS_Bomb("memory allocation failure (trigger data copies)");
      if (triggerRequest[iTrigger].message != NULL) {
        if (!SDDS_CopyString(triggerControlMessage + iTrigger, triggerRequest[iTrigger].message))
          SDDS_Bomb("memory allocation failure (trigger data copies)");
        else
          triggerControlMessage[iTrigger][0] = 0;
      }
      triggered[iTrigger] = 0;
    }
    for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
      alarmed[iAlarm] = 0;
      severity[iAlarm] = 0;
    }

    lastConditionsFailed = 0;

    if (alarmTrigChannels) /*wait for alarm triggers */
      setUpAlarmTriggerCallbacks(alarmTrigRequest, alarmTrigChannels);

    quitingTime = getTimeInSecs() + TotalTime; /*TotalTime is the input time duration */

    /************
     steps loop 
      ************/
    Step = 0;
    /* old_datasets=numDatasets; */
    /* old_alarms=alarmTrigChannels; */
    /* old_glitchs=glitchChannels; */
    /* old_triggers=triggerChannels; */
    PingTime = getTimeInSecs() - 2;
    HourNow = getHourOfDay();
    for (i = 0; i < numDatasets; i++)
      datasets[i].TriggerTime = getTimeInSecs();
    while (getTimeInSecs() <= quitingTime) {
      ElapsedTime = getTimeInSecs() - StartTime;
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        if (PingTime + 2 < getTimeInSecs()) {
          PingTime = getTimeInSecs();
          if (runControlPingWhileSleep(0)) {
            fprintf(stderr, "Problem pinging the run control record2.\n");
            return (1);
          }
        }
      }
#endif
      if (Step) {
#ifdef USE_RUNCONTROL
        if ((rcParam.PV) && (TimeInterval > 2)) {
          if (runControlPingWhileSleep(TimeInterval)) {
            fprintf(stderr, "Problem pinging the run control record3.\n");
            return (1);
          }
        } else {
          oag_ca_pend_event(TimeInterval, &sigint);
        }
#else
        oag_ca_pend_event(TimeInterval, &sigint);
#endif
        if (sigint)
          return (1);
      }
      if (watchInput) {
        if (file_is_modified(inputfile, &input_linkname, &input_filestat)) {
          if (verbose)
            fprintf(stdout, "The inputfile has been modified, exit!\n");
          break;
        }
        if (trigFileSet) {
          if (file_is_modified(triggerFile, &trig_linkname, &trig_filestat)) {
            if (verbose)
              fprintf(stdout, "the trigger file has been modified,exit!\n");
            break;
          }
        }
      }
      LastHour = HourNow;
      HourNow = getHourOfDay();
#if DEBUG
      if (fabs(HourNow - LastHour) > 0.167)
        fprintf(stderr, "%le seconds left\n", quitingTime - getTimeInSecs());
#endif
      /* Subtract time interval from last hour to reduce problems with apparent clock
           * nonmonotonicity on Solaris.  I think this results from once-daily correction
           * of the clock.
           */
      if (HourNow < (LastHour - TimeInterval / 3600.0)) {
#if DEBUG
        fprintf(stderr, "Starting new file: HourNow=%le, LastHour=%le\n",
                HourNow, LastHour);
#endif
        /* after midnight or input file is modified, must start new file */
        /* first,terminate the old files*/
        for (i = 0; i < numDatasets; i++) {
          if (disconnected[i]) {
            if (!SDDS_ReconnectFile(datasets[i].dataset))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (!tableStarted[i]) { /*actually, write empty page */
            if (!SDDS_StartPage(datasets[i].dataset, updateInterval))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            useArrays = 0;
            for (j = 0; j < datasets[i].variables; j++) {
              if ((datasets[i].elements)[j] != 1) {
                useArrays = 1;
                if (!SDDS_SetArrayVararg(datasets[i].dataset, (datasets[i].ReadbackName)[j], SDDS_CONTIGUOUS_DATA, NULL, 0, 0)) {
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              }
            }
            if (useArrays) {
              if (!SDDS_SetArrayVararg(datasets[i].dataset, "Step", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(datasets[i].dataset, "CAerrors", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(datasets[i].dataset, "Time", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(datasets[i].dataset, "TimeOfDay", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              if (!SDDS_SetArrayVararg(datasets[i].dataset, "DayOfMonth", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if (!SDDS_WritePage(datasets[i].dataset))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (!SDDS_Terminate(datasets[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        /* then, start new files */
        /*1. generate file name */
        for (i = 0; i < numDatasets; i++) {
          disconnected[i] = 0;
          servicingTrigger[i] = 0;
          writingPage[i] = 0;
          pointsLeft[i] = glitchAfter;
          /*  trigBeforeCount[i]=0; */
          tableStarted[i] = 0;
          datasets[i].triggerMode = 0;
          sprintf(temp, "%s%s", outputDir, datasets[i].OutputRootname);
          sprintf(temp, "%s", MakeDailyGenerationFilename(temp, 4, ".", 0));
          datasets[i].outputFile = tmalloc(sizeof(char) * (strlen(temp) + 1));
          strcpy(datasets[i].outputFile, temp);
#if DEBUG
          fprintf(stderr, "New output file is %s\n", datasets[i].outputFile);
#endif
          if (verbose)
            fprintf(stdout, "output file is %s\n", datasets[i].outputFile);
        }

        /*2. starts log file */
        startGlitchLogFiles(&datasets, numDatasets, alarmTrigRequest, alarmTrigChannels,
                            glitchRequest, glitchChannels,
                            triggerRequest, triggerChannels,
                            &StartTime, &StartDay, &StartHour,
                            &StartJulianDay, &StartMonth,
                            &StartYear, &TimeStamp, glitchBefore, glitchAfter);
        for (i = 0; i < numDatasets; i++) {
          /*set disconnected to 1, since the datasets are disconnceted 
                    in startGlitchLogFiles procedure*/
          disconnected[i] = 1;
        }
      }
      if (inhibit) {
        if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
          if (CondMode & TOUCH_OUTPUT)
            for (i = 0; i < numDatasets; i++)
              TouchFile(datasets[i].outputFile);
          if (verbose) {
            printf("Inhibit PV %s is nonzero. Data collection inhibited\n", InhibitPV.name);
            fflush(stdout);
          }
          if (Step > 0 && !lastConditionsFailed) {
            if (verbose) {
              fprintf(stdout, "Step %ld---values failed condition test,flush data \n", Step);
              fflush(stdout);
            }
            for (i = 0; i < numDatasets; i++) {
              /*the data of this node should not be written to file, 
                            because its data has not been taken yet*/
              datasets[i].Node->hasdata = 0;
            }
            lastConditionsFailed = 1;
            CheckTriggerAndFlushData(&datasets, numDatasets, &trigBeforeCount,
                                     &pointsLeft, &servicingTrigger, &writingPage,
                                     &disconnected, &tableStarted,
                                     &glitchRequest, glitchChannels,
                                     &alarmTrigRequest, alarmTrigChannels,
                                     &triggerRequest, triggerChannels,
                                     &triggered, &severity, &alarmed, &glitched,
                                     quitingTime, glitchAfter, glitchBefore,
                                     StartTime, StartHour, StartJulianDay,
                                     StartDay, StartYear, StartMonth,
                                     TimeStamp, verbose, row, lastConditionsFailed);
          }
          /*
                    if (!lastConditionsFailed) {
                    for (i=0;i<numDatasets;i++) {
                    datasets[i].Node=datasets[i].firstNode;
                    trigBeforeCount[i]=0;
                    pointsLeft[i]=glitchAfter;
                    }
            
                    for (iTrigger=0; iTrigger<triggerChannels; iTrigger++)
                    triggerRequest[iTrigger].triggerArmed = 1;
                    for (iGlitch=0; iGlitch<glitchChannels; iGlitch++)
                    glitchRequest[iGlitch].glitchArmed = 1;
                    for (j=0;j<numDatasets;j++) {
                    for (i=0;i<bufferlength;i++){
                    datasets[j].Node->hasdata = 0;
                    }
                    }
                    }
                    lastConditionsFailed = 1; */
          if (InhibitWaitTime > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
#ifdef USE_RUNCONTROL
            if ((rcParam.PV) && (InhibitWaitTime > 2)) {
              if (runControlPingWhileSleep(InhibitWaitTime)) {
                fprintf(stderr, "Problem pinging the run control record4.\n");
                return (1);
              }
            } else {
              oag_ca_pend_event(InhibitWaitTime, &sigint);
            }
#else
            oag_ca_pend_event(InhibitWaitTime, &sigint);
#endif
            if (sigint)
              return (1);
          }
          continue;
        }
      }
      if (CondFile &&
          !PassesConditions(CondDeviceName, CondReadMessage, CondScaleFactor,
                            CondDataBuffer, CondLowerLimit, CondUpperLimit,
                            CondHoldoff, conditions, CondMode, CondCHID, pendIOtime)) {
        if (CondMode & TOUCH_OUTPUT) {
          for (i = 0; i < numDatasets; i++)
            TouchFile(datasets[i].outputFile);
        }

        if (Step > 0 && !lastConditionsFailed) {
          if (verbose) {
            fprintf(stdout, "Step %ld---values failed condition test,flush data \n", Step);
            fflush(stdout);
          }
          for (i = 0; i < numDatasets; i++) {
            /*the data of this node should not be written to file, because it is the data after testing*/
            datasets[i].Node->hasdata = 0;
          }
          lastConditionsFailed = 1;
          CheckTriggerAndFlushData(&datasets, numDatasets, &trigBeforeCount,
                                   &pointsLeft, &servicingTrigger, &writingPage,
                                   &disconnected, &tableStarted,
                                   &glitchRequest, glitchChannels,
                                   &alarmTrigRequest, alarmTrigChannels,
                                   &triggerRequest, triggerChannels,
                                   &triggered, &severity, &alarmed, &glitched,
                                   quitingTime, glitchAfter, glitchBefore,
                                   StartTime, StartHour, StartJulianDay,
                                   StartDay, StartYear, StartMonth,
                                   TimeStamp, verbose, row, lastConditionsFailed);
        }
        if (verbose) {
          printf("Step %ld---values failed condition test--continuing\n", Step);
          fflush(stdout);
        }
        if (!Step) {
#ifdef USE_RUNCONTROL
          if ((rcParam.PV) && (TimeInterval > 2)) {
            if (runControlPingWhileSleep(TimeInterval)) {
              fprintf(stderr, "Problem pinging the run control record5.\n");
              return (1);
            }
          } else {
            oag_ca_pend_event(TimeInterval, &sigint);
          }
#else
          oag_ca_pend_event(TimeInterval, &sigint);
#endif
          if (sigint)
            return (1);
        }
        /*
                if (!lastConditionsFailed) {
                for (i=0;i<numDatasets;i++) {
                trigBeforeCount[i]=0; 
                pointsLeft[i]=glitchAfter;
                datasets[i].Node=datasets[i].firstNode;
                datasets[i].Node->hasdata = 0;
                for (j=1;j<bufferlength;j++) {
                datasets[i].Node=datasets[i].Node->next;
                datasets[i].Node->hasdata=0;
                }
                datasets[i].Node->next=datasets[i].firstNode;
                datasets[i].Node=datasets[i].firstNode;
                }
          
                for (iTrigger=0; iTrigger<triggerChannels; iTrigger++) {
                triggerRequest[iTrigger].trigStep=0;
                triggerRequest[iTrigger].triggerArmed = 1;
                }
          
                for (iGlitch=0; iGlitch<glitchChannels; iGlitch++) {
                glitchRequest[iGlitch].trigStep=0;
                glitchRequest[iGlitch].glitchArmed = 1;
                }
                for (iAlarm=0; iAlarm<alarmTrigChannels; iAlarm++) {
                alarmTrigRequest[iAlarm].trigStep=0;
                } 
          
                }
                lastConditionsFailed = 1; */
        continue;
      }
      lastConditionsFailed = 0;
      CAerrors = 0;
      ElapsedTime = (EpochTime = getTimeInSecs()) - StartTime;
      TimeOfDay = StartHour + ElapsedTime / 3600.0;
      DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
      if (verbose) {
        printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }

      if (glitchChannels) { /*read glitch trigger values */
        CAerrors += ReadScalarValues(glitchControlName, glitchControlMessage, NULL,
                                     glitchValue, glitchChannels, glitchCHID, pendIOtime);
        /* Setup glitch delay system */
        if (delayedGlitchValuesInitialized) {
          for (j = 0; j < glitchChannels; j++) {
            tmpValue = glitchValue[j];
            glitchValue[j] = delayedGlitchValue[0][j];
            for (i = 0; i < glitchDelay; i++) {
              delayedGlitchValue[i][j] = delayedGlitchValue[i + 1][j];
            }
            delayedGlitchValue[glitchDelay][j] = tmpValue;
          }
        } else {
          for (i = 0; i <= glitchDelay; i++) {
            for (j = 0; j < glitchChannels; j++) {
              delayedGlitchValue[i][j] = glitchValue[j];
            }
          }
          delayedGlitchValuesInitialized = 1;
        }
      }
      if (triggerChannels) { /*read trigger values */
        CAerrors += ReadScalarValues(triggerControlName, triggerControlMessage, NULL,
                                     triggerValue, triggerChannels, triggerCHID, pendIOtime);
        /* Setup trigger delay system */

        if (delayedTriggerValuesInitialized) {
          for (j = 0; j < triggerChannels; j++) {
            tmpValue = triggerValue[j];
            triggerValue[j] = delayedTriggerValue[0][j];
            for (i = 0; i < glitchDelay; i++) {
              delayedTriggerValue[i][j] = delayedTriggerValue[i + 1][j];
            }
            delayedTriggerValue[glitchDelay][j] = tmpValue;
          }
        } else {
          for (i = 0; i <= glitchDelay; i++) {
            for (j = 0; j < triggerChannels; j++) {
              delayedTriggerValue[i][j] = triggerValue[j];
            }
          }
          delayedTriggerValuesInitialized = 1;
        }
      }
      if (!CAerrors) {
        if (!baselineStarted) {
          for (iTrigger = 0; iTrigger < triggerChannels; iTrigger++)
            /*set triggerRequest's initial value */
            triggerRequest[iTrigger].value = triggerValue[iTrigger];
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
            /*set the initial baseline to the initial value */
            glitchRequest[iGlitch].baselineValue = glitchValue[iGlitch];
          baselineStarted = 1;
        }

        if (glitchChannels) {
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
            if (verbose)
              printf("glitch quantity %s: baseline=%.2f delta=%.2f current=%.2f.\n",
                     glitchRequest[iGlitch].controlName,
                     glitchRequest[iGlitch].baselineValue,
                     glitchRequest[iGlitch].delta,
                     glitchValue[iGlitch]);

            i = glitchRequest[iGlitch].dataIndex;
            if (glitchRequest[iGlitch].glitchArmed) {
              if (IsGlitch(glitchValue[iGlitch], glitchRequest[iGlitch].baselineValue,
                           glitchRequest[iGlitch].delta)) {
                /* glitched[iGlitch] = 1; */
                /*ignore the other triggers while writing data*/
                if (!writingPage[i] && pointsLeft[i] == glitchAfter) {
                  servicingTrigger[i] = 1;
                  glitchRequest[iGlitch].trigStep = Step;
                  datasets[i].TriggerTime = EpochTime;
                  datasets[i].triggerMode |= GLITCHTRIGGER;
                  datasets[i].glitchIndex = iGlitch;
                  glitched[iGlitch] = 1;
                }
                glitchRequest[iGlitch].glitchArmed = 0;
                if (verbose) {
                  printf("Glitched on %s\n", glitchRequest[iGlitch].controlName);
                  fflush(stdout);
                }
                if (glitchRequest[iGlitch].baselineAutoReset)
                  glitchRequest[iGlitch].baselineValue = glitchValue[iGlitch];
              }
            } else
              glitchRequest[iGlitch].baselineValue =
                (glitchValue[iGlitch] + glitchRequest[iGlitch].baselineValue *
                                          glitchRequest[iGlitch].baselineSamples) /
                (glitchRequest[iGlitch].baselineSamples + 1);
          }
          fflush(stdout);
        }
        if (triggerChannels) {
          for (iTrigger = 0; iTrigger < triggerChannels; iTrigger++) {
            if (verbose)
              printf("Trigger %s: level=%.2f direction=%c previous=%.2f current=%.2f\n",
                     triggerRequest[iTrigger].controlName,
                     triggerRequest[iTrigger].triggerLevel,
                     (triggerRequest[iTrigger].direction > 0) ? '+' : '-',
                     triggerRequest[iTrigger].value, triggerValue[iTrigger]);

            i = triggerRequest[iTrigger].dataIndex;
            if (triggerRequest[iTrigger].triggerArmed) {
              if (IsTrigger(triggerRequest[iTrigger].value, triggerValue[iTrigger],
                            triggerRequest[iTrigger].triggerLevel,
                            triggerRequest[iTrigger].direction)) {
                if (verbose) {
                  printf("Triggered on %s \n", triggerRequest[iTrigger].controlName);
                  fflush(stdout);
                }
                /* triggered[iTrigger]=1; */
                /*this trigger is active only when the data writing is done */
                if (!writingPage[i] && pointsLeft[i] == glitchAfter) {
                  servicingTrigger[i] = 1;
                  triggerRequest[iTrigger].trigStep = Step;
                  datasets[i].triggerMode |= TRIGGER;
                  datasets[i].TriggerTime = EpochTime;
                  datasets[i].trigIndex = iTrigger;
                  triggered[iTrigger] = 1;
                }
                triggerRequest[iTrigger].triggerArmed = 0;
              } else {
                /* check for trigger re-arm */
                if ((triggerRequest[iTrigger].flags & TRIGGER_AUTOARM) ||
                    ((triggerRequest[iTrigger].direction > 0 &&
                      triggerValue[iTrigger] < triggerRequest[iTrigger].triggerLevel) ||
                     (triggerRequest[iTrigger].direction < 0 &&
                      triggerValue[iTrigger] > triggerRequest[iTrigger].triggerLevel))) {
                  triggerRequest[iTrigger].triggerArmed = 1;
                  triggered[iTrigger] = 0;
                  if (verbose) {
                    printf("Trigger armed on channel %s.\n", triggerRequest[iTrigger].controlName);
                    fflush(stdout);
                  }
                }
              }
            }
            triggerRequest[iTrigger].value = triggerValue[iTrigger];
          } /* end of for <triggerChannels */
        }   /*end of if (triggerChannels) */

        for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
          if (verbose)
            printf("alarm quantity %s:  triggered=%d  severity=%d\n",
                   alarmTrigRequest[iAlarm].controlName,
                   alarmTrigRequest[iAlarm].triggered,
                   alarmTrigRequest[iAlarm].severity);
          i = alarmTrigRequest[iAlarm].dataIndex;
          if (alarmTrigRequest[iAlarm].triggered) {
            /*this trigger is active only when the data writing is done */
            if (!writingPage[i] && pointsLeft[i] == glitchAfter) {
              servicingTrigger[i] = 1;
              alarmTrigRequest[iAlarm].trigStep = Step;
              datasets[i].TriggerTime = EpochTime;
              datasets[i].triggerMode |= ALARMTRIGGER;
              alarmed[iAlarm] = 1;
              severity[iAlarm] = alarmTrigRequest[iAlarm].severity;
              datasets[i].alarmIndex = iAlarm;
            }
            alarmTrigRequest[iAlarm].triggered = 0;
          }
        }

        /*read the values of PVs to be recorded */
        readCAvalues = 0;
        if (glitchBefore > 0) {
          readCAvalues = 1;
        } else {
          for (i = 0; i < numDatasets; i++) {
            if (servicingTrigger[i]) {
              readCAvalues = 1;
              break;
            }
          }
        }
        if (readCAvalues) {
          for (i = 0; i < numDatasets; i++) {
            CAerrors += ReadCAValues(datasets[i].ControlName, NULL, NULL, datasets[i].values, datasets[i].elements,
                                     datasets[i].variables, datasets[i].chid, pendIOtime);
          }
          /* write values to node,i.e, store data in buffer */
          for (i = 0; i < numDatasets; i++) {
            datasets[i].Node->step = Step;
            datasets[i].Node->ElapsedTime = ElapsedTime;
            datasets[i].Node->EpochTime = EpochTime;
            datasets[i].Node->TimeOfDay = TimeOfDay;
            datasets[i].Node->DayOfMonth = DayOfMonth;
            for (j = 0; j < datasets[i].variables; j++) {
              for (k = 0; k < datasets[i].elements[j]; k++) {
                datasets[i].Node->values[j][k] = datasets[i].values[j][k];
              }
            }
            datasets[i].Node->hasdata = 1;
          }
        }

        //if (!CAerrors)
        //{

        CheckTriggerAndFlushData(&datasets, numDatasets, &trigBeforeCount,
                                 &pointsLeft, &servicingTrigger, &writingPage,
                                 &disconnected, &tableStarted,
                                 &glitchRequest, glitchChannels,
                                 &alarmTrigRequest, alarmTrigChannels,
                                 &triggerRequest, triggerChannels,
                                 &triggered, &severity, &alarmed, &glitched,
                                 quitingTime, glitchAfter, glitchBefore,
                                 StartTime, StartHour, StartJulianDay,
                                 StartDay, StartYear, StartMonth,
                                 TimeStamp, verbose, row, lastConditionsFailed);
        //}
      }
      /* old_datasets=numDatasets; */
      /* old_alarms=alarmTrigChannels; */
      /* old_glitchs=glitchChannels; */
      /* old_triggers=triggerChannels; */
      Step++;
      for (i = 0; i < numDatasets; i++)
        datasets[i].Node = datasets[i].Node->next;
    } /* while (<=quitingTime)  */
  }   /* end of if (!glitChannels .. */

  for (i = 0; i < numDatasets; i++) {
    if (disconnected[i]) {
      if (!SDDS_ReconnectFile(datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!tableStarted[i]) {
      if (!SDDS_StartPage(datasets[i].dataset, updateInterval))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      useArrays = 0;
      for (j = 0; j < datasets[i].variables; j++) {
        if ((datasets[i].elements)[j] != 1) {
          useArrays = 1;
          if (!SDDS_SetArrayVararg(datasets[i].dataset, (datasets[i].ReadbackName)[j], SDDS_POINTER_ARRAY, NULL, 0, 0)) {
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
      }
      if (useArrays) {
        if (!SDDS_SetArrayVararg(datasets[i].dataset, "Step", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(datasets[i].dataset, "CAerrors", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(datasets[i].dataset, "Time", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(datasets[i].dataset, "TimeOfDay", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (!SDDS_SetArrayVararg(datasets[i].dataset, "DayOfMonth", SDDS_CONTIGUOUS_DATA, NULL, 0)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      }

      if (!SDDS_WritePage(datasets[i].dataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }

    if (!SDDS_Terminate(datasets[i].dataset))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    free(datasets[i].dataset);
    datasets[i].Node = datasets[i].firstNode;
    for (j = 0; j < bufferlength; j++) {
      for (k = 0; k < datasets[i].variables; k++)
        free(datasets[i].Node->values[k]);
      free(datasets[i].Node->values);
      free(datasets[i].Node->elements);
      datasets[i].Node = datasets[i].Node->next;
    }
    for (k = bufferlength - 2; k >= 0; k--) {
      datasets[i].Node = datasets[i].firstNode;
      for (j = 0; j < bufferlength; j++) {
        if (j == k) {
          if (datasets[i].Node->next)
            free(datasets[i].Node->next);
          break;
        }
        datasets[i].Node = datasets[i].Node->next;
      }
    }
    free(datasets[i].firstNode);
    /* free(datasets[i].Node);*/
    for (j = 0; j < datasets[i].variables; j++) {
      free(datasets[i].values[j]);
      free(datasets[i].ControlName[j]);
      free(datasets[i].ReadbackName[j]);
      free(datasets[i].ReadbackUnits[j]);
    }
    free(datasets[i].values);
    free(datasets[i].elements);
    free(datasets[i].ControlName);
    free(datasets[i].ReadbackName);
    free(datasets[i].ReadbackIndex);
    free(datasets[i].ReadbackUnits);
    free(datasets[i].outputFile);
    free(datasets[i].chid);
  }
  free(datasets);
  if (CondFile) {
    if (CondDeviceName) {
      for (i = 0; i < conditions; i++) {
        free(CondDeviceName[i]);
      }
      free(CondDeviceName);
    }
    if (CondReadMessage) {
      for (i = 0; i < conditions; i++) {
        free(CondReadMessage[i]);
      }
      free(CondReadMessage);
    }
    if (CondScaleFactor)
      free(CondScaleFactor);
    if (CondUpperLimit)
      free(CondUpperLimit);
    if (CondLowerLimit)
      free(CondLowerLimit);
    if (CondHoldoff)
      free(CondHoldoff);
    if (CondDataBuffer)
      free(CondDataBuffer);
    if (CondCHID)
      free(CondCHID);
  }
  if (inputfile)
    free(inputfile);
  if (outputDir)
    free(outputDir);
  free_scanargs(&s_arg, argc);

  return 0;
}

long getGlitchLoggerData(DATASETS **datasets, long *numDatasets, GLITCH_REQUEST **glitchRequest,
                         long *glitchChannels, ALARMTRIG_REQUEST **alarmTrigRequest,
                         long *alarmTrigChannels, TRIGGER_REQUEST **triggerRequest,
                         long *triggerChannels,
                         char *inputFile, short trigFileSet) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName, *OutputRootname;
  long page, rows = 0, i, j, found = 0, dIndex = 0;
  short noReadbackName = 0, UnitsDefined = 0;
  short MajorAlarmDefined = 0, MinorAlarmDefined = 0, NoAlarmDefined = 0;
  short TransitionThresholdDefined = 0, GlitchThresholdDefined = 0, TransitionDirectionDefined = 0;
  short GlitchSamplesDefined = 0, GlitchAutoResetDefined = 0, glitchScriptExist = 0;
  short major, minor, noalarm, direction;
  double glitchDelta = 0;
  char *glitchScript;

  major = minor = noalarm = direction = 0;
  ControlNameColumnName = OutputRootname = NULL;

  *datasets = NULL;
  if (glitchRequest)
    *glitchRequest = NULL;
  if (glitchChannels) {
    *glitchChannels = 0;
  }
  if (triggerRequest)
    *triggerRequest = NULL;
  if (triggerChannels) {
    *triggerChannels = 0;
  }
  if (alarmTrigRequest)
    *alarmTrigRequest = NULL;
  if (alarmTrigChannels) {
    *alarmTrigChannels = 0;
  }

  *numDatasets = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)))
    noReadbackName = 1;

  if (SDDS_CheckParameter(&inSet, "GlitchScript", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY)
    glitchScriptExist = 1;

  switch (SDDS_CheckColumn(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL)) {
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
    switch (SDDS_CheckColumn(&inSet, "ReadbackUnit", NULL, SDDS_STRING, NULL)) {
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
  if (!trigFileSet) {
    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&inSet, "TriggerControlName", NULL,
                                                SDDS_STRING, NULL))) {
      fprintf(stderr, "Error: TriggerControlName parameter is missing or not string type\n");
      exit(1);
    }

    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&inSet, "OutputRootname", NULL,
                                                SDDS_STRING, NULL))) {
      fprintf(stderr, "Error: OutputRootname parameter is missing or not string type\n");
      exit(1);
    }
    switch (SDDS_CheckParameter(&inSet, "MajorAlarm", NULL, SDDS_SHORT, NULL)) {
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

    switch (SDDS_CheckParameter(&inSet, "MinorAlarm", NULL, SDDS_SHORT, NULL)) {
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

    switch (SDDS_CheckParameter(&inSet, "NoAlarm", NULL, SDDS_SHORT, NULL)) {
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
    switch (SDDS_CheckParameter(&inSet, "TransitionThreshold", NULL, SDDS_DOUBLE, NULL)) {
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
    switch (SDDS_CheckParameter(&inSet, "TransitionDirection", NULL, SDDS_SHORT, NULL)) {
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
    switch (SDDS_CheckParameter(&inSet, "GlitchThreshold", NULL, SDDS_DOUBLE, NULL)) {
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
    switch (SDDS_CheckParameter(&inSet, "GlitchBaselineSamples", NULL, SDDS_LONG, NULL)) {
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
    switch (SDDS_CheckParameter(&inSet, "GlitchBaselineAutoReset", NULL, SDDS_SHORT, NULL)) {
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
    if (!trigFileSet) {
      if (SDDS_GetParameter(&inSet, "OutputRootname", &OutputRootname) == NULL)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      found = 0;
      for (j = 0; j < (*numDatasets); j++) {
        if (!strcmp(OutputRootname, (*datasets + j)->OutputRootname)) {
          found = 1;
          dIndex = j;
          break;
        }
      }
    } else {
      if (page > 1) {
        fprintf(stderr, "file %s has multi-pages, only the first page is used.\n", inputFile);
        return 1;
      }
    }
    /*if triggerFile is given, found=0, only read the columns in the first page of inputFile */
    if (!found) {
      (*numDatasets)++;
      if (!(rows = SDDS_RowCount(&inSet)))
        bomb("Zero rows defined in input file.\n", NULL);
      if (!(*datasets = realloc(*datasets, sizeof(**datasets) * (*numDatasets))))
        SDDS_Bomb("Memory allocation failure for datasets");
      (*datasets + (*numDatasets) - 1)->variables = rows;
      (*datasets + (*numDatasets) - 1)->ControlName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
      if (noReadbackName)
        (*datasets + (*numDatasets) - 1)->ReadbackName = (char **)SDDS_GetColumn(&inSet,
                                                                                 ControlNameColumnName);
      else
        (*datasets + (*numDatasets) - 1)->ReadbackName = (char **)SDDS_GetColumn(&inSet, "ReadbackName");

      if (UnitsDefined)
        (*datasets + (*numDatasets) - 1)->ReadbackUnits = (char **)SDDS_GetColumn(&inSet, "ReadbackUnits");
      else {
        (*datasets + (*numDatasets) - 1)->ReadbackUnits = (char **)malloc(sizeof(char *) * rows);
        for (i = 0; i < rows; i++) {
          (*datasets + (*numDatasets) - 1)->ReadbackUnits[i] =
            (char *)malloc(sizeof(char) * (8 + 1));
          (*datasets + (*numDatasets) - 1)->ReadbackUnits[i][0] = 0;
        }
      }

      if (!trigFileSet) { /*set the OutputRootname and TriggerControlName of datasets */
        if (!((*datasets + (*numDatasets) - 1)->OutputRootname =
                malloc(sizeof(char) * (strlen(OutputRootname) + 1))))
          SDDS_Bomb("failure allocation memory1");
        strcpy((*datasets + (*numDatasets) - 1)->OutputRootname, OutputRootname);
        if (SDDS_GetParameter(&inSet, "TriggerControlName",
                              &(*datasets + (*numDatasets) - 1)->TriggerControlName) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"TriggerControlName\".\n");
          exit(1);
        }
        (*datasets + (*numDatasets) - 1)->triggerType = 0;
      }
    }                   /*end of if (!found) */
    if (!trigFileSet) { /*get the trigger information for inputFile if triggerFile is not given */
      /*get the information of alarm triggers if any */
      if (glitchScriptExist && !SDDS_GetParameter(&inSet, "GlitchScript", &glitchScript))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (MajorAlarmDefined || MinorAlarmDefined || NoAlarmDefined) {
        if (MajorAlarmDefined) {
          if (SDDS_GetParameter(&inSet, "MajorAlarm",
                                &major) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"MajorAlarm\".\n");
            exit(1);
          }
        }
        if (MinorAlarmDefined) {
          if (SDDS_GetParameter(&inSet, "MinorAlarm",
                                &minor) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"MinorAlarm\".\n");
            exit(1);
          }
        }
        if (NoAlarmDefined) {
          if (SDDS_GetParameter(&inSet, "NoAlarm",
                                &noalarm) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"NoAlarm\".\n");
            exit(1);
          }
        }
        if (major || minor || noalarm) {
          (*alarmTrigChannels)++;
          *alarmTrigRequest = realloc(*alarmTrigRequest, sizeof(**alarmTrigRequest) * (*alarmTrigChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript), glitchScript);
          else
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = NULL;

          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList =
              (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList = NULL;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList[0] = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList = NULL;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->flags = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->holdoff = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->trigStep = 0;
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->triggered = 0;

          if (SDDS_GetParameter(&inSet, "TriggerControlName",
                                &(*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"TriggerControlName\".\n");
            exit(1);
          }

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
            if (!((*datasets + dIndex)->triggerType & ALARMTRIGGER))
              (*datasets + dIndex)->triggerType |= ALARMTRIGGER;
          } else {
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->dataIndex = (*numDatasets) - 1;
            (*datasets + (*numDatasets) - 1)->triggerType |= ALARMTRIGGER;
          }
        }
      }

      /*get the information of triggers if any */
      if (TransitionThresholdDefined && TransitionDirectionDefined) {
        if (SDDS_GetParameter(&inSet, "TransitionDirection",
                              &direction) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"TransitionDiection\".\n");
          exit(1);
        }
        if (direction) {
          (*triggerChannels)++;
          *triggerRequest = realloc(*triggerRequest, sizeof(**triggerRequest) * (*triggerChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((*triggerRequest + (*triggerChannels) - 1)->glitchScript), glitchScript);
          else
            (*triggerRequest + (*triggerChannels) - 1)->glitchScript = NULL;

          (*triggerRequest + (*triggerChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
          (*triggerRequest + (*triggerChannels) - 1)->message[0] = 0;
          (*triggerRequest + (*triggerChannels) - 1)->holdoff = 0;
          (*triggerRequest + (*triggerChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
          (*triggerRequest + (*triggerChannels) - 1)->trigStep = 0;

          if (SDDS_GetParameter(&inSet, "TriggerControlName",
                                &(*triggerRequest + (*triggerChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"TriggerControlName\".\n");
            exit(1);
          }

          if (TransitionThresholdDefined) {
            if (SDDS_GetParameter(&inSet, "TransitionThreshold",
                                  &(*triggerRequest + (*triggerChannels) - 1)->triggerLevel) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"TransitionThreshold\".\n");
              exit(1);
            }
          } else {
            (*triggerRequest + (*triggerChannels) - 1)->triggerLevel = 0;
          }

          (*triggerRequest + (*triggerChannels) - 1)->direction = direction;
          if (found) {
            (*triggerRequest + (*triggerChannels) - 1)->dataIndex = dIndex;
            if (!((*datasets + dIndex)->triggerType & TRIGGER))
              (*datasets + dIndex)->triggerType |= TRIGGER;
          } else {
            (*triggerRequest + (*triggerChannels) - 1)->dataIndex = (*numDatasets) - 1;
            (*datasets + (*numDatasets) - 1)->triggerType |= TRIGGER;
          }
        }
      }

      /*get information of glitch if any */
      if (GlitchThresholdDefined) {
        if (SDDS_GetParameter(&inSet, "GlitchThreshold",
                              &glitchDelta) == NULL) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          printf("Something wrong with parameter \"GlitchThreshold\".\n");
          exit(1);
        }
        if (glitchDelta) {
          (*glitchChannels)++;
          *glitchRequest = realloc(*glitchRequest, sizeof(**glitchRequest) * (*glitchChannels));
          if (glitchScriptExist)
            SDDS_CopyString(&((*glitchRequest + (*glitchChannels) - 1)->glitchScript), glitchScript);
          else
            (*glitchRequest + (*glitchChannels) - 1)->glitchScript = NULL;

          (*glitchRequest + (*glitchChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
          (*glitchRequest + (*glitchChannels) - 1)->message[0] = 0;
          (*glitchRequest + (*glitchChannels) - 1)->delta =
            (*glitchRequest + (*glitchChannels) - 1)->holdoff = 0;
          (*glitchRequest + (*glitchChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
          (*glitchRequest + (*glitchChannels) - 1)->trigStep = 0;

          if (SDDS_GetParameter(&inSet, "TriggerControlName",
                                &(*glitchRequest + (*glitchChannels) - 1)->controlName) == NULL) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
            printf("Something wrong with parameter \"GlitchControlName\".\n");
            exit(1);
          }

          (*glitchRequest + (*glitchChannels) - 1)->delta = glitchDelta;
          if (GlitchSamplesDefined) {
            if (SDDS_GetParameter(&inSet, "GlitchBaselineSamples",
                                  &(*glitchRequest + (*glitchChannels) - 1)->baselineSamples) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"GlitchBaselineSamples\".\n");
              exit(1);
            }
          } else
            (*glitchRequest + (*glitchChannels) - 1)->baselineSamples = 0;
          if (GlitchAutoResetDefined) {
            if (SDDS_GetParameter(&inSet, "GlitchBaselineAutoReset",
                                  &(*glitchRequest + (*glitchChannels) - 1)->baselineAutoReset) == NULL) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
              printf("Something wrong with parameter \"GlitchBaselineAutoReset\".\n");
              exit(1);
            }
          } else
            (*glitchRequest + (*glitchChannels) - 1)->baselineAutoReset = 0;
          if (found) {
            (*glitchRequest + (*glitchChannels) - 1)->dataIndex = dIndex;
            if (!((*datasets + dIndex)->triggerType & GLITCHTRIGGER))
              (*datasets + dIndex)->triggerType |= GLITCHTRIGGER;
          } else {
            (*glitchRequest + (*glitchChannels) - 1)->dataIndex = (*numDatasets) - 1;
            (*datasets + (*numDatasets) - 1)->triggerType |= GLITCHTRIGGER;
          }
        }
      } /*end of get information of glitch trigger */
      if (glitchScriptExist) {
        free(glitchScript);
        glitchScript = NULL;
      }
    } /*end of if (!trigFileSet) */
  }   /*end of while(page=SDDS_ReadPage  */
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  if (OutputRootname)
    free(OutputRootname);

  SDDS_Terminate(&inSet);
  return 1;
}

void startGlitchLogFiles(DATASETS **datasets, long numDatasets,
                         ALARMTRIG_REQUEST *alarmTrigRequest, long alarmTrigRequests,
                         GLITCH_REQUEST *glitchRequest, long glitchRequests,
                         TRIGGER_REQUEST *triggerRequest, long triggerRequests,
                         double *StartTime, double *StartDay, double *StartHour,
                         double *StartJulianDay, double *StartMonth,
                         double *StartYear, char **TimeStamp, long glitchBefore, long glitchAfter) {
  long i, j, useArrays = 0;
  getTimeBreakdown(StartTime, StartDay, StartHour, StartJulianDay, StartMonth,
                   StartYear, TimeStamp);

  for (i = 0; i < numDatasets; i++) {
    useArrays = 0;
    for (j = 0; j < (*datasets + i)->variables; j++) {
      if (((*datasets + i)->elements)[j] != 1) {
        useArrays = 1;
      }
    }
    if (!SDDS_InitializeOutput((*datasets + i)->dataset, SDDS_BINARY, 1, NULL, NULL,
                               (*datasets + i)->outputFile) ||
        !DefineLoggingTimeParameters((*datasets + i)->dataset) ||
        !DefineLoggingTimeDetail((*datasets + i)->dataset, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (useArrays) {
      if (!DefineLoggingTimeDetail((*datasets + i)->dataset, TIMEDETAIL_ARRAYS | TIMEDETAIL_EXTRAS))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_EnableFSync((*datasets + i)->dataset);
    if (!DefineGlitchParameters((*datasets + i)->dataset, glitchRequest, glitchRequests,
                                triggerRequest, triggerRequests, alarmTrigRequest, alarmTrigRequests, i))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (SDDS_DefineParameter((*datasets + i)->dataset, "TriggerTime", NULL, "seconds", NULL, NULL,
                             SDDS_DOUBLE, 0) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (j = 0; j < (*datasets + i)->variables; j++) {
      if (((*datasets + i)->elements)[j] == 1) {
        if (!SDDS_DefineSimpleColumn((*datasets + i)->dataset,
                                     ((*datasets + i)->ReadbackName)[j],
                                     ((*datasets + i)->ReadbackUnits)[j],
                                     SDDS_DOUBLE)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
      } else {
        if (!SDDS_DefineArray((*datasets + i)->dataset,
                              ((*datasets + i)->ReadbackName)[j],
                              NULL,
                              ((*datasets + i)->ReadbackUnits)[j],
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
    /*
        if (!SDDS_DefineSimpleColumns((*datasets+i)->dataset, (*datasets+i)->variables, 
        (*datasets+i)->ReadbackName,(*datasets+i)->ReadbackUnits,SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors|SDDS_EXIT_PrintErrors);
      */
    if (SDDS_DefineColumn((*datasets + i)->dataset, "PostTrigger", NULL, NULL,
                          NULL, NULL, SDDS_SHORT, 0) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    (*datasets + i)->ReadbackIndex =
      malloc(sizeof(*((*datasets + i)->ReadbackIndex)) * ((*datasets + i)->variables));
    for (j = 0; j < (*datasets)[i].variables; j++) {
      if (((*datasets + i)->elements)[j] == 1) {
        if (((*datasets + i)->ReadbackIndex[j] = SDDS_GetColumnIndex((*datasets + i)->dataset,
                                                                     (*datasets + i)->ReadbackName[j])) < 0) {
          fprintf(stderr, "Unable to retrieve index for column %s\n", (*datasets + i)->ReadbackName[j]);
          exit(1);
        }
      } else {
        if (((*datasets + i)->ReadbackIndex[j] = SDDS_GetArrayIndex((*datasets + i)->dataset,
                                                                    (*datasets + i)->ReadbackName[j])) < 0) {
          fprintf(stderr, "Unable to retrieve index for array %s\n", (*datasets + i)->ReadbackName[j]);
          exit(1);
        }
      }
    }
    if (!SDDS_WriteLayout((*datasets + i)->dataset))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!SDDS_DisconnectFile((*datasets + i)->dataset))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

long setUpAlarmTriggerCallbacks(
  ALARMTRIG_REQUEST *alarmTrigRequest,
  long alarmTrigChannels) {
  long i;

  if (!alarmTrigChannels)
    return 0;
  for (i = 0; i < alarmTrigChannels; i++) {
    alarmTrigRequest[i].severity = -1;
    alarmTrigRequest[i].triggered = 0;
    if (ca_search(alarmTrigRequest[i].controlName, &alarmTrigRequest[i].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", alarmTrigRequest[i].controlName);
      return 0;
    }
  }
  if (ca_pend_io(10) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some trigger channels\n");
    for (i = 0; i < alarmTrigChannels; i++) {
      if (ca_state(alarmTrigRequest[i].channelID) != cs_conn)
        fprintf(stderr, "%s not connected\n", alarmTrigRequest[i].controlName);
    }
  }

  for (i = 0; i < alarmTrigChannels; i++) {
    alarmTrigRequest[i].usrValue = i;
    if (ca_add_masked_array_event(DBR_TIME_STRING, 1, alarmTrigRequest[i].channelID,
                                  alarmTriggerEventHandler,
                                  (void *)&alarmTrigRequest[i].usrValue, (ca_real)0, (ca_real)0,
                                  (ca_real)0, NULL, DBE_ALARM) != ECA_NORMAL) {
      fprintf(stderr, "error: unable to setup alarm callback for control name %s\n",
              alarmTrigRequest[i].controlName);
      exit(1);
    }
  }
  ca_poll();
  return 1;
}

void alarmTriggerEventHandler(struct event_handler_args event) {
  long index, i;
  struct dbr_time_double *dbrValue;

  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_double *)event.dbr;
  alarmTrigRequest[index].status = (short)dbrValue->status;
  alarmTrigRequest[index].severity = (short)dbrValue->severity;
  for (i = 0; i < alarmTrigRequest[index].severityIndices; i++) {
    if (alarmTrigRequest[index].severity == alarmTrigRequest[index].severityIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].severityIndices)
    return;
  for (i = 0; i < alarmTrigRequest[index].severityIndices; i++) {
    if (alarmTrigRequest[index].severity == alarmTrigRequest[index].severityIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].severityIndices)
    return;
  for (i = 0; i < alarmTrigRequest[index].statusIndices; i++) {
    if (alarmTrigRequest[index].status == alarmTrigRequest[index].statusIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].statusIndices)
    return;
  alarmTrigRequest[index].triggered = 1;
}

long DefineGlitchParameters(
  SDDS_DATASET *SDDSout,
  GLITCH_REQUEST *glitchRequest,
  long glitchRequests,
  TRIGGER_REQUEST *triggerRequest,
  long triggerRequests,
  ALARMTRIG_REQUEST *alarmTrigRequest,
  long alarmTrigRequests, long dataIndex) {
  long i, j;
  char buffer[1024], descrip[1024];
  for (i = 0; i < glitchRequests; i++) {
    if (glitchRequest[i].dataIndex == dataIndex) {
      sprintf(buffer, "%sGlitched%ld", glitchRequest[i].controlName, i);
      sprintf(descrip, "sddsmonitor glitch status of %s", glitchRequest[i].controlName);
      if ((glitchRequest[i].parameterIndex =
             SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
    }
  }
  for (i = 0; i < triggerRequests; i++) {
    if (triggerRequest[i].dataIndex == dataIndex) {
      sprintf(buffer, "%sTriggered%ld", triggerRequest[i].controlName, i);
      sprintf(descrip, "sddsmonitor trigger status of %s%ld", triggerRequest[i].controlName, i);
      if ((triggerRequest[i].parameterIndex =
             SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
    }
  }

  for (i = 0; i < alarmTrigRequests; i++) {
    if (alarmTrigRequest[i].dataIndex == dataIndex) {
      sprintf(descrip, "sddsmonitor alarm-trigger status of %s",
              alarmTrigRequest[i].controlName);
      j = 0;
      while (1) {
        sprintf(buffer, "%sAlarmTrigger%ld", alarmTrigRequest[i].controlName, j);
        if (SDDS_GetParameterIndex(SDDSout, buffer) >= 0)
          j++;
        else
          break;
      }
      if ((alarmTrigRequest[i].alarmParamIndex =
             SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
        return 0;
      sprintf(buffer, "%sAlarmSeverity%ld", alarmTrigRequest[i].controlName, j);
      sprintf(descrip, "EPICS alarm severity of %s",
              alarmTrigRequest[i].controlName);
      if ((alarmTrigRequest[i].severityParamIndex =
             SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
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

  if (!(valuePicked = calloc(validItems, sizeof(*valuePicked))) ||
      !(returnValue = calloc(validItems, sizeof(*returnValue)))) {
    SDDS_Bomb("Memory allocation failure (CSStringListsToArray)");
  }
  if (includeList && excludeList)
    return NULL;
  if (includeList) {
    while ((item = get_token(includeList))) {
      if ((matchIndex = match_string(item, validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_SetError("Invalid list entry (CSStringListsToArray)");
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
          SDDS_SetError("Invalid list entry (CSStringListsToArray)");
          return NULL;
        }
        valuePicked[matchIndex] = 0;
      }
    }
  }
  for (i = returnValues = 0; i < validItems; i++) {
    if (valuePicked[i]) {
      if ((returnValue[returnValues++] = match_string(validItem[i], validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_Bomb("item not found in second search---this shouldn't happen.");
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

long getTriggerData(GLITCH_REQUEST **glitchRequest, long *glitchChannels,
                    ALARMTRIG_REQUEST **alarmTrigRequest, long *alarmTrigChannels,
                    TRIGGER_REQUEST **triggerRequest, long *triggerChannels,
                    char *triggerFile) {
  SDDS_DATASET inSet;
  short majorDefined, minorDefined, noalarmDefined, glitchThresholdDefined, glitchBaselineSamplesDefined;
  short glitchAutoResetDefined, transitionThresholdDefined, transitionDirectionDefined;
  long rows = 0;
  char **TriggerControlName;
  short *MajorAlarm, *MinorAlarm, *NoAlarm, *TransitionDirection, *GlitchBaselineAutoReset;
  long *GlitchBaselineSamples, i;
  double *TransitionThreshold, *GlitchThreshold;
  char **glitchScript = NULL;

  majorDefined = minorDefined = noalarmDefined = glitchThresholdDefined = glitchBaselineSamplesDefined = glitchAutoResetDefined = transitionThresholdDefined = transitionDirectionDefined = 0;
  TriggerControlName = NULL;
  MajorAlarm = MinorAlarm = NoAlarm = TransitionDirection = GlitchBaselineAutoReset = NULL;
  GlitchBaselineSamples = NULL;
  TransitionThreshold = GlitchThreshold = NULL;

  *glitchRequest = NULL;
  *triggerRequest = NULL;
  *alarmTrigRequest = NULL;
  *glitchChannels = *alarmTrigChannels = *triggerChannels = 0;

  if (!SDDS_InitializeInput(&inSet, triggerFile))
    return 0;
  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "TriggerControlName", NULL, SDDS_STRING, NULL))) {
    fprintf(stderr, "TriggerControlName column does not exist in %s", triggerFile);
    exit(1);
  }
  switch (SDDS_CheckColumn(&inSet, "MajorAlarm", NULL, SDDS_SHORT, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "MinorAlarm", NULL, SDDS_SHORT, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "NoAlarm", NULL, SDDS_SHORT, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "GlitchThreshold", NULL, SDDS_DOUBLE, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "GlitchBaselineSamples", NULL, SDDS_LONG, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "GlitchBaselineAutoReset", NULL, SDDS_SHORT, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "TransitionThreshold", NULL, SDDS_DOUBLE, NULL)) {
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
  switch (SDDS_CheckColumn(&inSet, "TransitionDirection", NULL, SDDS_SHORT, NULL)) {
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
  if (0 >= SDDS_ReadPage(&inSet))
    return 0;
  if (!(rows = SDDS_CountRowsOfInterest(&inSet)))
    SDDS_Bomb("Zero rows defined in trigger file.\n");
  /*read the trigger file */
  TriggerControlName = (char **)SDDS_GetColumn(&inSet, "TriggerControlName");
  if (majorDefined) {
    if (!(MajorAlarm = SDDS_GetColumn(&inSet, "MajorAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(MajorAlarm = calloc(rows, sizeof(*MajorAlarm))))
      SDDS_Bomb("Memory allocation failure (getTriggerData)");
    /*allocated memory for MajorAlarm and assign 0 to its elements */
  }

  if (minorDefined) {
    if (!(MinorAlarm = SDDS_GetColumn(&inSet, "MinorAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(MinorAlarm = calloc(rows, sizeof(*MinorAlarm))))
      SDDS_Bomb("Memory allocation failure (getTriggerData)");
    /*allocated memory for MinorrAlarm and assign 0 to its elements */
  }

  if (noalarmDefined) {
    if (!(NoAlarm = SDDS_GetColumn(&inSet, "NoAlarm")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(NoAlarm = calloc(rows, sizeof(*NoAlarm))))
      SDDS_Bomb("Memory allocation failure (getTriggerData)");
    /*allocated memory for NoAlarm and assign 0 to its elements */
  }

  if (transitionThresholdDefined)
    if (!(TransitionThreshold = SDDS_GetColumn(&inSet, "TransitionThreshold")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (transitionDirectionDefined)
    if (!(TransitionDirection = SDDS_GetColumn(&inSet, "TransitionDirection")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (glitchThresholdDefined)
    if (!(GlitchThreshold = SDDS_GetColumn(&inSet, "GlitchThreshold")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (glitchBaselineSamplesDefined) {
    if (!(GlitchBaselineSamples = SDDS_GetColumn(&inSet, "GlitchBaselineSamples")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(GlitchBaselineSamples = calloc(rows, sizeof(*GlitchBaselineSamples))))
      SDDS_Bomb("Memory allocation failure (getTriggerData)");
    /*allocated memory for GlitchBaselineSamples and assign 0 to its elements */
  }

  if (glitchAutoResetDefined) {
    if (!(GlitchBaselineAutoReset = SDDS_GetColumn(&inSet, "GlitchBaselineAutoReset")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!(GlitchBaselineAutoReset = calloc(rows, sizeof(*GlitchBaselineAutoReset))))
      SDDS_Bomb("Memory allocation failure (getTriggerData)");
    /*allocated memory for GlitchBaselineAutoReset and assign 0 to its elements */
  }
  if (SDDS_CheckColumn(&inSet, "GlitchScript", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY &&
      !(glitchScript = (char **)SDDS_GetColumn(&inSet, "GlitchScript")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  /*assign values to trigger requests */
  for (i = 0; i < rows; i++) {
    /*check for alarm triggers, major, minor and noalarm do not have to be defined at the same time */
    /*if none of them is defined, there is no alarm triggers */
    /*if all of them are 0, ignore alarm-based trigger of this PV */
    if (majorDefined || minorDefined || noalarmDefined) {
      if (MajorAlarm[i] || MinorAlarm[i] || NoAlarm[i]) {
        (*alarmTrigChannels)++;
        *alarmTrigRequest = realloc(*alarmTrigRequest, sizeof(**alarmTrigRequest) * (*alarmTrigChannels));
        if (glitchScript)
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = glitchScript[i];
        else
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->glitchScript = NULL;

        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
          (*alarmTrigRequest + (*alarmTrigChannels) - 1)->statusNameList =
            (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notStatusNameList = NULL;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->severityNameList[0] = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->notSeverityNameList = NULL;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->flags = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->holdoff = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->trigStep = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->triggered = 0;
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->dataIndex = 0;
        /*only one dataset, and its index it 0 */
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MajorAlarm = MajorAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->MinorAlarm = MinorAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->NoAlarm = NoAlarm[i];
        (*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName =
          malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((*alarmTrigRequest + (*alarmTrigChannels) - 1)->controlName, TriggerControlName[i]);
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
        (*triggerChannels)++;
        *triggerRequest = realloc(*triggerRequest, sizeof(**triggerRequest) * (*triggerChannels));
        if (glitchScript)
          (*triggerRequest + (*triggerChannels) - 1)->glitchScript = glitchScript[i];
        else
          (*triggerRequest + (*triggerChannels) - 1)->glitchScript = NULL;

        (*triggerRequest + (*triggerChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
        (*triggerRequest + (*triggerChannels) - 1)->message[0] = 0;
        (*triggerRequest + (*triggerChannels) - 1)->holdoff = 0;
        (*triggerRequest + (*triggerChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
        (*triggerRequest + (*triggerChannels) - 1)->trigStep = 0;
        (*triggerRequest + (*triggerChannels) - 1)->controlName =
          malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((*triggerRequest + (*triggerChannels) - 1)->controlName, TriggerControlName[i]);
        (*triggerRequest + (*triggerChannels) - 1)->triggerLevel = TransitionThreshold[i];
        (*triggerRequest + (*triggerChannels) - 1)->direction = TransitionDirection[i];
        (*triggerRequest + (*triggerChannels) - 1)->dataIndex = 0;
      }
    }

    /*check for glitch triggers, glitchThreshold has to be defined */
    /*if glitchThreshold is 0, ignore glitch-base trigger of this PV */
    if (glitchThresholdDefined) {
      if (GlitchThreshold[i]) {
        (*glitchChannels)++;
        *glitchRequest = realloc(*glitchRequest, sizeof(**glitchRequest) * (*glitchChannels));
        if (glitchScript)
          (*glitchRequest + (*glitchChannels) - 1)->glitchScript = glitchScript[i];
        else
          (*glitchRequest + (*glitchChannels) - 1)->glitchScript = NULL;

        (*glitchRequest + (*glitchChannels) - 1)->message = (char *)malloc(sizeof(char) * 64);
        (*glitchRequest + (*glitchChannels) - 1)->message[0] = 0;
        (*glitchRequest + (*glitchChannels) - 1)->delta =
          (*glitchRequest + (*glitchChannels) - 1)->holdoff = 0;
        (*glitchRequest + (*glitchChannels) - 1)->flags |= TRIGGER_AUTOHOLD;
        (*glitchRequest + (*glitchChannels) - 1)->trigStep = 0;
        (*glitchRequest + (*glitchChannels) - 1)->controlName =
          malloc(sizeof(char) * (strlen(TriggerControlName[i]) + 1));
        strcpy((*glitchRequest + (*glitchChannels) - 1)->controlName, TriggerControlName[i]);
        (*glitchRequest + (*glitchChannels) - 1)->delta = GlitchThreshold[i];
        (*glitchRequest + (*glitchChannels) - 1)->baselineSamples = GlitchBaselineSamples[i];
        (*glitchRequest + (*glitchChannels) - 1)->baselineAutoReset = GlitchBaselineAutoReset[i];
        (*glitchRequest + (*glitchChannels) - 1)->dataIndex = 0;
      }
    }
  } /* end of for (i=0;i<rows;i++) */
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

void CheckTriggerAndFlushData(DATASETS **datasets, long numDatasets, long **trigBeforeCount,
                              long **pointsLeft, short **servicingTrigger, short **writingPage,
                              short **disconnected, short **tableStarted,
                              GLITCH_REQUEST **glitchRequest, long glitchChannels,
                              ALARMTRIG_REQUEST **alarmTrigRequest, long alarmTrigChannels,
                              TRIGGER_REQUEST **triggerRequest, long triggerChannels,
                              short **triggered, short **severity, short **alarmed, short **glitched,
                              double quitingTime, long glitchAfter, long glitchBefore,
                              double StartTime, double StartHour, double StartJulianDay,
                              double StartDay, double StartYear, double StartMonth,
                              char *TimeStamp, long verbose, long row, long lastConditionsFailed) {
  int i, a = 0, t = 0, g = 0, j, useArrays, index;
  char *PageTimeStamp;
  long caErrors = 0, executes = 0;
  char **executeScript = NULL;

  for (i = 0; i < numDatasets; i++) {
    if (verbose)
      fprintf(stdout, "glitchbefore counter[%d] %ld, pointsleft[%d] %ld\n", i,
              (*trigBeforeCount)[i], i, (*pointsLeft)[i]);
    if (!(*servicingTrigger)[i]) {
      if (!(*writingPage)[i]) {
        (*pointsLeft)[i] = glitchAfter;
        if ((*trigBeforeCount)[i] < glitchBefore)
          (*trigBeforeCount)[i]++;
      }
    } else {
      (*pointsLeft)[i]--;
      /*writing the data to file when buffer is full */
      if (((*pointsLeft)[i] < 0) || getTimeInSecs() > quitingTime || lastConditionsFailed) {
        (*writingPage)[i] = 1;
        row = 0;
        if (verbose)
          fprintf(stdout, "Writing Data ...\n");
        glitchNode[i] = (*datasets)[i].Node;
        /* search for first valid node of the circular buffer after the glitch node*/
        (*datasets)[i].Node = (*datasets)[i].Node->next;
        while (!(*datasets)[i].Node->hasdata) {
          (*datasets)[i].Node = (*datasets)[i].Node->next;
        }

        if ((*disconnected)[i]) {
          if (!SDDS_ReconnectFile((*datasets)[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          (*disconnected)[i] = 0;
        }

        if (!SDDS_StartPage((*datasets)[i].dataset, ((*trigBeforeCount)[i] + glitchAfter + 1)))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        (*tableStarted)[i] = 1;

        if (!SDDS_CopyString(&PageTimeStamp, makeTimeStamp(getTimeInSecs())))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (!SDDS_SetParameters((*datasets)[i].dataset,
                                SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "TimeStamp", TimeStamp, "PageTimeStamp", PageTimeStamp,
                                "StartTime", StartTime, "StartYear", (short)StartYear,
                                "YearStartTime", computeYearStartTime(StartTime),
                                "StartJulianDay", (short)StartJulianDay,
                                "StartMonth", (short)StartMonth,
                                "StartDayOfMonth", (short)StartDay, "StartHour",
                                (float)StartHour,
                                "TriggerTime", (*datasets)[i].TriggerTime, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if ((*datasets)[i].triggerType & GLITCHTRIGGER) {
          for (g = 0; g < glitchChannels; g++) {
            if ((*glitched)[g]) {
              if ((*glitchRequest)[g].glitchScript && strlen((*glitchRequest)[g].glitchScript)) {
                if (!executes) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*glitchRequest)[g].glitchScript;
                  executes++;
                } else if ((index = match_string((*glitchRequest)[g].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*glitchRequest)[g].glitchScript;
                  executes++;
                }
              }
            }
            if ((*glitchRequest)[g].dataIndex == i) {
              if (!SDDS_SetParameters((*datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      (*glitchRequest)[g].parameterIndex,
                                      (*glitched)[g], -1))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              (*glitched)[g] = 0;
            }
          }
        }
        if ((*datasets)[i].triggerType & TRIGGER) {
          for (t = 0; t < triggerChannels; t++) {
            if ((*triggered)[t]) {
              if ((*triggerRequest)[t].glitchScript && strlen((*triggerRequest)[t].glitchScript)) {
                if (!executes) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*triggerRequest)[t].glitchScript;
                  executes++;
                } else if ((index = match_string((*triggerRequest)[t].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*triggerRequest)[t].glitchScript;
                  executes++;
                }
              }
            }

            if ((*triggerRequest)[t].dataIndex == i) {
              if (!SDDS_SetParameters((*datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                      (*triggerRequest)[t].parameterIndex,
                                      (*triggered)[t], -1))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              (*triggered)[t] = 0;
            }
          }
        }

        if ((*datasets)[i].triggerType & ALARMTRIGGER) {
          for (a = 0; a < alarmTrigChannels; a++) {
            if ((*alarmed)[a] || (*severity)[a]) {
              if ((*alarmTrigRequest)[a].glitchScript && strlen((*alarmTrigRequest)[a].glitchScript)) {
                if (!executes) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*alarmTrigRequest)[a].glitchScript;
                  executes++;
                } else if ((index = match_string((*alarmTrigRequest)[a].glitchScript, executeScript, executes, EXACT_MATCH)) < 0) {
                  executeScript = SDDS_Realloc(executeScript, sizeof(*executeScript) * (executes + 1));
                  executeScript[executes] = (*alarmTrigRequest)[a].glitchScript;
                  executes++;
                }
              }
            }

            if ((*alarmTrigRequest)[a].dataIndex == i) {
              if ((*datasets)[i].triggerMode & ALARMTRIGGER) {
                if (!SDDS_SetParameters((*datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                        (*alarmTrigRequest)[a].severityParamIndex,
                                        (*severity)[a],
                                        (*alarmTrigRequest)[a].alarmParamIndex,
                                        (*alarmed)[a], -1))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                (*severity)[a] = 0;
              } else {
                if (!SDDS_SetParameters((*datasets)[i].dataset, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                        (*alarmTrigRequest)[a].severityParamIndex,
                                        (*alarmTrigRequest)[a].severity,
                                        (*alarmTrigRequest)[a].alarmParamIndex,
                                        (*alarmed)[a], -1))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
              (*alarmed)[a] = 0;
            }
          }
        }
        do {
          if ((*datasets)[i].Node->hasdata) {
            if (verbose) {
              printf("Writing values at step: %ld at time: %f  row :%ld for data[%d]\n",
                     (*datasets)[i].Node->step,
                     (*datasets)[i].Node->ElapsedTime, row, i);
              fflush(stdout);
            }
            if ((*datasets)[i].triggerMode & TRIGGER) {
              t = (*datasets)[i].trigIndex;
              if ((*datasets)[i].Node->step <= (*triggerRequest)[t].trigStep) {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if ((*datasets)[i].triggerMode & GLITCHTRIGGER) {
              g = (*datasets)[i].glitchIndex;
              if ((*datasets)[i].Node->step <= (*glitchRequest)[g].trigStep) {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if ((*datasets)[i].triggerMode & ALARMTRIGGER) {
              a = (*datasets)[i].alarmIndex;
              if ((*datasets)[i].Node->step <= (*alarmTrigRequest)[a].trigStep) {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)0,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              } else {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                       row, "PostTrigger", (short)1,
                                       NULL))
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
            if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, row,
                                   "Step", (*datasets)[i].Node->step,
                                   "Time", (*datasets)[i].Node->EpochTime,
                                   "TimeOfDay", (*datasets)[i].Node->TimeOfDay,
                                   "DayOfMonth", (*datasets)[i].Node->DayOfMonth, NULL))
              /* terminate with NULL if SDDS_BY_NAME or with -1 if SDDS_BY_INDEX */
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            useArrays = 0;
            for (j = 0; j < (*datasets)[i].variables; j++) {
              if (((*datasets + i)->elements)[j] == 1) {
                if (!SDDS_SetRowValues((*datasets)[i].dataset, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                                       ((*datasets)[i].ReadbackIndex)[j], ((*datasets)[i].Node->values)[j][0], -1)) {
                  fprintf(stderr, "problem setting column %ld, row %ld\n", ((*datasets)[i].ReadbackIndex)[j], row);
                  exit(1);
                }
              } else {
                useArrays = 1;
                if (!SDDS_AppendToArrayVararg((*datasets)[i].dataset, ((*datasets)[i].ReadbackName)[j], SDDS_CONTIGUOUS_DATA, ((*datasets)[i].Node->values)[j], ((*datasets + i)->elements)[j], row + 1, ((*datasets + i)->elements)[j])) {
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              }
            }
            if (useArrays) {
              if ((!SDDS_AppendToArrayVararg((*datasets)[i].dataset, "Step", SDDS_CONTIGUOUS_DATA, &((*datasets)[i].Node->step), 1, row + 1)) ||
                  (!SDDS_AppendToArrayVararg((*datasets)[i].dataset, "Time", SDDS_CONTIGUOUS_DATA, &((*datasets)[i].Node->EpochTime), 1, row + 1)) ||
                  (!SDDS_AppendToArrayVararg((*datasets)[i].dataset, "TimeOfDay", SDDS_CONTIGUOUS_DATA, &((*datasets)[i].Node->TimeOfDay), 1, row + 1)) ||
                  (!SDDS_AppendToArrayVararg((*datasets)[i].dataset, "DayOfMonth", SDDS_CONTIGUOUS_DATA, &((*datasets)[i].Node->DayOfMonth), 1, row + 1)) ||
                  (!SDDS_AppendToArrayVararg((*datasets)[i].dataset, "CAerrors", SDDS_CONTIGUOUS_DATA, &(caErrors), 1, row + 1))) {
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              }
            }
          }
          (*datasets)[i].Node->hasdata = 0;
          (*datasets)[i].Node = (*datasets)[i].Node->next;
          row++;
        } while ((*datasets)[i].Node != glitchNode[i]->next);
        if (!(*disconnected)[i]) {
          if (!SDDS_DisconnectFile((*datasets)[i].dataset))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          (*disconnected)[i] = 1;
        }
        (*servicingTrigger)[i] = 0;
        (*writingPage)[i] = 0;
        (*trigBeforeCount)[i] = 0;
        (*pointsLeft)[i] = glitchAfter;
        (*datasets)[i].Node = glitchNode[i]->next;
        if ((*datasets)[i].triggerMode & TRIGGER) {
          (*triggerRequest)[t].triggerArmed = 1;
          (*triggerRequest)[t].trigStep = 0;
        }
        if ((*datasets)[i].triggerMode & ALARMTRIGGER) {
          (*alarmTrigRequest)[a].trigStep = 0;
        }
        if ((*datasets)[i].triggerMode & GLITCHTRIGGER) {
          (*glitchRequest)[g].trigStep = 0;
          (*glitchRequest)[g].glitchArmed = 1;
        }
        (*datasets)[i].triggerMode = 0;
      }
    } /* end of if (!servingTrigger )*/
  }   /* end of for (i<numDatasets) */
  if (executes) {
    for (i = 0; i < executes; i++) {
      if (verbose)
        fprintf(stdout, "Executing %s\n", executeScript[i]);
      system(executeScript[i]);
    }
    free(executeScript);
  }
}

long ReadCAValues(char **deviceName, char **readMessage,
                  double *scaleFactor, double **value, long *elements, long n,
                  chid *cid, double pendIOTime) {
  long i, j, caErrors, pend = 0;

  caErrors = 0;
  if (!n)
    return 0;
  if (!deviceName || !value || !cid) {
    fprintf(stderr, "Error: null pointer passed to ReadCAValues()\n");
    return (-1);
  }

  for (i = 0; i < n; i++) {
    if (readMessage && readMessage[i][0] != 0) {
      fprintf(stderr, "Devices (devSend) no longer supported");
      return (-1);
    } else {
      if (!cid[i]) {
        pend = 1;
        if (ca_search(deviceName[i], &(cid[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", deviceName[i]);
          return (-1);
        }
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
    }
  }
  for (i = 0; i < n; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (ca_array_get(DBR_DOUBLE, elements[i], cid[i], value[i]) != ECA_NORMAL) {
        for (j = 0; j < elements[i]; j++)
          value[i][j] = 0;
        caErrors++;
        fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
      }
    } else {
      /*fprintf(stderr, "PV %s is not connected\n", deviceName[i]);*/
      value[i][0] = 0;
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
  }

  if (scaleFactor)
    for (i = 0; i < n; i++)
      for (j = 0; j < elements[i]; j++)
        value[i][j] *= scaleFactor[i];
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
