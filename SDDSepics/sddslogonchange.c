/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.17  2009/12/04 16:49:57  soliday
 * Improved the last changes.
 *
 * Revision 1.16  2009/12/03 22:13:59  soliday
 * Updated by moving the needed definitions from alarm.h and alarmString.h
 * into this file directly. This was needed because of a problem with
 * EPICS Base 3.14.11. Andrew Johnson said that the values for these
 * definitions will not change in the future.
 *
 * Revision 1.15  2007/12/12 17:17:26  soliday
 * Added code to hopefully stop an occasional problem that occurs at Midnight.
 *
 * Revision 1.14  2007/07/27 16:31:18  soliday
 * Modified to fix a problem seen when many PVs change at once.
 *
 * Revision 1.13  2006/12/14 22:25:49  soliday
 * Updated a bunch of programs because SDDS_SaveLayout is now called by
 * SDDS_WriteLayout and it is no longer required to be called directly.
 * Also the AutoCheckMode is turned off by default now so I removed calls to
 * SDDS_SetAutoCheckMode that would attempt to turn it off. It is now up to
 * the programmer to turn it on in new programs until debugging is completed
 * and then remove the call to SDDS_SetAutoCheckMode.
 *
 * Revision 1.12  2006/10/20 15:21:08  soliday
 * Fixed problems seen by linux-x86_64.
 *
 * Revision 1.11  2006/07/06 21:46:52  soliday
 * Updated to fix a problem when using runcontrol with a lot of PVs. This
 * program unlike other sdds logger programs connect to the PVs in the background.
 * This means that when we go to ping the runcontrol PV the pend_event may
 * return an error. This is only because it is still trying to connect to
 * the other PVs in the background.
 *
 * Revision 1.10  2006/07/06 20:53:45  soliday
 * Updated to reduce the load on the runcontrol IOC.
 *
 * Revision 1.9  2005/11/08 22:34:39  soliday
 * Updated to remove compiler warnings on Linux.
 *
 * Revision 1.8  2005/11/08 22:05:03  soliday
 * Added support for 64 bit compilers.
 *
 * Revision 1.7  2005/04/21 18:45:52  soliday
 * Fixed issue when killing the code with a signal.
 *
 * Revision 1.6  2005/04/21 15:26:21  soliday
 * Added runcontrol support.
 *
 * Revision 1.5  2004/11/04 16:34:54  shang
 * the watchInput feature now watches both the name of the final link-to file and the state of the input file.
 *
 * Revision 1.4  2004/09/27 16:19:00  soliday
 * Added missing ca_task_exit call.
 *
 * Revision 1.3  2004/07/19 17:39:37  soliday
 * Updated the usage message to include the epics version string.
 *
 * Revision 1.2  2004/06/10 16:41:51  soliday
 * With Base 3.14.x it is no longer possible to setup the event handler structure
 * to return a double for a string PV. This was done instead of trying to strip
 * the input file of all the string PVs. It now just ignores the string PVs in
 * the event handler function.
 *
 * Revision 1.1  2003/08/27 19:51:16  soliday
 * Moved into subdirectory
 *
 * Revision 1.16  2003/02/20 21:43:02  soliday
 * Fixed a problem with reading files over the network that were getting only
 * partly updated by setting the RowCountMode to fixed.
 *
 * Revision 1.15  2002/09/24 20:05:26  soliday
 * Added epicsSecondsWestOfUTC
 *
 * Revision 1.14  2002/09/24 19:10:27  soliday
 * Added support for OS X
 *
 * Revision 1.13  2002/09/13 21:51:57  soliday
 * Last changed only worked for Linux.
 *
 * Revision 1.12  2002/09/13 21:45:16  soliday
 * Changed the timezone variable to work with the new build environment
 *
 * Revision 1.11  2002/08/14 20:00:35  soliday
 * Added Open License
 *
 * Revision 1.10  2002/04/25 16:15:53  soliday
 * Added support for WIN32
 *
 * Revision 1.9  2002/04/05 21:28:27  soliday
 * I was using abs where I should have been using fabs
 *
 * Revision 1.8  2002/04/05 20:55:31  soliday
 * Fixed bug with tolerance when loginitial is not used
 *
 * Revision 1.7  2002/04/04 21:30:13  soliday
 * Negative tolerances are converted to 1e-99
 *
 * Revision 1.6  2002/04/04 15:37:32  soliday
 * Fixed a bug when not using daily files.
 *
 * Revision 1.5  2002/03/28 22:01:37  soliday
 * Added the waitTime option for the inhibitPV option
 *
 * Revision 1.4  2002/03/28 21:11:55  soliday
 * Added the dailyFiles option.
 *
 * Revision 1.3  2002/03/28 16:04:03  soliday
 * Added the ability to log the initial values.
 *
 * Revision 1.2  2002/03/27 16:36:54  soliday
 * Added the ability to work with tolerances and made the readbackname
 * column optional.
 *
 * Revision 1.1  2002/01/16 20:07:43  soliday
 * First version.
 *
 *
 *
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <cadef.h>
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

#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#if defined(__BORLANDC__)
#  define tzset _tzset
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#define CLO_VERBOSE 0
#define CLO_TIMEDURATION 1
#define CLO_PENDEVENTTIME 2
#define CLO_EXPLICIT 3
#define CLO_DURATIONS 4
#define CLO_TIMEOUT 5
#define CLO_ERASE 6
#define CLO_APPEND 7
#define CLO_GENERATIONS 8
#define CLO_COMMENT 9
#define CLO_REQUIRECHANGE 10
#define CLO_OFFSETTIMEOFDAY 11
#define CLO_INHIBIT 12
#define CLO_WATCHINPUT 13
#define CLO_LOGINITIALVALUES 14
#define CLO_LOGALARMS 15
#define CLO_INCLUDETIMEOFDAY 16
#define CLO_DAILY_FILES 17
#define CLO_RUNCONTROLPV 18
#define CLO_RUNCONTROLDESC 19
#define CLO_SDDSLOGGER 20
#define CLO_NOTOLERANCEMINLIMIT 21
#define COMMANDLINE_OPTIONS 22

static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "verbose",
  "timeduration",
  "pendeventtime",
  "explicit",
  "durations",
  "connecttimeout",
  "erasefile",
  "append",
  "generations",
  "comment",
  "requirechange",
  "offsettimeofday",
  "inhibitpv",
  "watchInput",
  "loginitialvalues",
  "logalarms",
  "includetimeofday",
  "dailyfiles",
  "runControlPV",
  "runControlDescription",
  "sddslogger",
  "noToleranceMinLimit",
};

static char *USAGE1 = "sddslogonchange <input> <output> \n\
[-timeDuration=<realValue>[,<time-units>]]\n\
[-append[=recover]] [-eraseFile]\n\
[-generations[=digits=<integer>][,delimiter=<string>]]\n\
[-dailyFiles]\n\
[-pendEventTime=<seconds>] [-durations] [-connectTimeout=<seconds>]\n\
[-explicit[=only]] [-verbose] [-comment=<parameterName>,<text>]\n\
[-requireChange[=severity][,status][,both]]\n\
[-inhibitPV=name=<name>[,pendIOTime=<seconds>][,waitTime=<seconds>]]\n\
[-includeTimeOfDay] [-offsetTimeOfDay]\n\
[-logInitialValues]\n\
[-logAlarms] [-watchInput]\n\
[-runControlPV=string=<string>,pingTimeout=<value>\n\
[-runControlDescription=string=<string>]\n\
[-sddslogger]\n\
[-noToleranceMinLimit]\n\n\
Logs data for the process variables named in the ControlName\n\
column of <input> to an SDDS file <output>.\n";
static char *USAGE2 = "-timeDuration   Specifies time duration for logging.  The default time units\n\
                are seconds; you may also specify days, hours, or minutes.\n\
-offsetTimeOfDay Adjusts the starting TimeOfDay value so that it corresponds\n\
                to the day for which the bulk of the data is taken.  Hence, a\n\
                26 hour job started at 11pm would have initial time of day of\n\
                -1 hour and final time of day of 25 hours.\n\
-append         Specifies appending to the file <output> if it exists already.\n\
                If the recover qualifier is given, recovery of a corrupted file\n\
                is attempted using sddsconvert, at the risk of loss of some of the\n\
                data already in the file.\n\
-eraseFile      Specifies erasing the file <output> if it exists already.\n\
-generations    Specifies use of file generations.  The output is sent to the file \n\
                <output>-<N>, where <N> is the smallest positive integer such that \n\
                the file does not already exist.   By default, four digits are used \n\
                for formatting <N>, so that the first generation number is 0001.\n\
dailyFiles      The output is sent to the file <output>-YYYY-JJJ-MMDD.<N>,\n\
                where YYYY=year, JJJ=Julian day, and MMDD=month+day.  A new file is\n\
                started after midnight.\n\
-pendEventTime  Specifies the CA pend event time, in seconds.  The default is 10 .\n\
-durations      Specifies including state duration and previous row reference data.\n\
                in output.\n";
static char *USAGE3 = "-connectTimeout Specifies maximum time in seconds to wait for a connection before\n\
                issuing an error message. 60 is the default.\n\
-explicit       Specifies that explicit columns with control name, alarm status,\n\
                and alarm severity strings be output in addition to the integer\n\
                codes.  If the \"only\" qualifier is given, the integer codes are\n\
                omitted from the output.\n\
-verbose        Specifies printing of possibly useful data to the standard output.\n\n\
-comment        Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                along with the text to be placed in the file.  \n\
-requireChange  Specifies that either severity, status, or both must change before an\n\
                event is logged.  The default behavior is to log an event whenever a\n\
                callback occurs, which means either severity or status has changed.\n\
-inhibitPV      Checks this PV periodically.  If nonzero, then data collection is aborted.\n\
-sddslogger     write the output file in sddslogger output format, i.e, the values of PVs\n\
                are stored in columns named by PV names.\n\
-noToleranceMinLimit\n\
                By default the minimum value change that will get logged is 1e-99. This\n\
                overrides this in order to log PVs that update with the exact same value.\n";
static char *USAGE4 = "-runControlPV   specifies a runControl PV name.\n\
-runControlDescription\n\
                specifies a runControl PV description record.\n\n\
Program by R. Soliday, ANL/APS.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifndef USE_RUNCONTROL
static char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#else
static char *USAGE_WARNING = "";
#endif

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

#define NTimeUnitNames 4
static char *TimeUnitName[NTimeUnitNames] = {
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

#define DEFAULT_PEND_EVENT 10.0
static double pendEventTime = DEFAULT_PEND_EVENT;

typedef struct
{
  short logPending;
  short isString;
  short isArray;
  char *controlName, *description;
  char *relatedControlName;
  chid channelID, relatedChannelID;
  long lastSeverity, lastRow, lastStatus;
  double lastChangeTime;
  double lastChangeTimeWS;
  /* values to be passed to LogEventToFile from RelatedValueEventHandler */
  long index;
  double theTime, theTimeWS, Hour, HourIOC, duration;
  short statusIndex, severityIndex;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  double value;
  double tolerance;
  char stringValue[MAX_STRING_SIZE];
  long elementCount;
  double *waveformValues;
} CHANNEL_INFO;

long GetControlNames(char *file, char ***name0, char ***readbackName0, char ***readbackUnits0, char ***relatedName0, char ***description0, double **tolerance0, short **expectNumeric0, char ***expectFieldType0, int32_t **expectElements0);
long SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **controlName, char **readbackName, char **readbackUnits, char **description, char **relatedName,
                  long controlNames, unsigned long mode,
                  char **commentParameter, char **commentText, long comments);
long SetupSDDSLoggerFile(SDDS_DATASET *logSet, char *output, char *input,
                         char **controlName, char **readbackName, char **readbackUnits, char **description, char **relatedName,
                         long controlNames, unsigned long mode,
                         char **commentParameter, char **commentText, long comments);
long InitiateConnections(char **cName, char **relatedName, char **description,
                         long cNames, double *tolerances, CHANNEL_INFO **chInfo, double toleranceMinLimit,
                         double connectTimeout, short *expectNumeric, char **expectFieldType, int32_t *expectElements);
void EventHandler(struct event_handler_args event);
void StringEventHandler(struct event_handler_args event);
void WaveformEventHandler(struct event_handler_args event);
void RelatedValueEventHandler(struct event_handler_args event);
long SetupEventCallbacks(void);
void LogUnconnectedEvents(void);
void LogEventToFile(long index, double theTime, double theTimeWS, double Hour, double HourIOC, double duration,
                    short statusIndex, short severityIndex, char *relatedValueString);
void LogWaveformEventToFile(long index, long actualCount, double theTime, double theTimeWS, double Hour, double HourIOC, double duration,
                            short statusIndex, short severityIndex);
long CheckForRequiredElements(char *filename, unsigned long mode);
long CheckLogOnChangeLogFile(char *filename, double *StartTime, double *StartDay, double *StartHour,
                             double *StartJulianDay, double *StartMonth,
                             double *StartYear, char **TimeStamp);
long GetLogFileIndices(SDDS_DATASET *outSet, unsigned long mode);

static long controlNames;
static CHANNEL_INFO *chInfo;
static SDDS_DATASET logDataset;
static long iControlNameIndex = -1, iAlarmStatusIndex = -1, iAlarmSeverityIndex = -1,
            iHour = -1, iControlName = -1, iAlarmStatus = -1, iAlarmSeverity = -1, iDuration = -1, iPreviousRow = -1,
            iDescription = -1, iRelatedName = -1, iRelatedValueString = -1,
            iHourIOC = -1, iValue = -1, iTime = -1, iTimeWS = -1, iStringValue = -1,
            iWaveformIndex = -1, iWaveformLength = -1;
static long logfileRow = 0;
static double startTime, startHour, hourOffset = 0, timeZoneOffset = 0;
static short eventOccurred = 0;
static double HourAdjust = 0;
static short disableLog = 0;
static short sddslogger = 0;
double lastLogTime = 0;
double lastLogTimeWS = 0;
/* offset in seconds between EPICS start-of-EPOCH and UNIX start-of-EPOCH */
#define EPOCH_OFFSET 631173600

unsigned long outputMode = 0;
#define EXPLICIT 0x0001U
#define NOINDICES 0x0002U
#define DURATIONS 0x0004U
#define APPEND 0x0008U
#define RECOVER 0x0010U
#define DESCRIPTIONS 0x0020U
#define REQSEVERITYCHANGE 0x0040U
#define REQSTATUSCHANGE 0x0080U
#define REQBOTHCHANGE 0x0100U
#define RELATEDVALUES 0x0200U
#define LOGALARMS 0x0400U
#define TIMEOFDAY 0x0800U
#define READBACKUNITS 0x1000U
#define READBACKNAME 0x2000U
#define TOLERANCES 0x4000U
#define LOGINITIAL 0x8000U

/*
  #define DEBUG 1
  #define DEBUGSDDS 1
*/
int runControlPingWhileSleep(double sleepTime);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

volatile int sigint = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  char *inputFile, *outputFile, *outputFileOrig, **controlName, **readbackName, **readbackUnits, **description, **relatedName, *linkname;
  double *tolerances;
  short *expectNumeric;
  char **expectFieldType;
  int32_t *expectElements;
  char **commentParameter, **commentText, *generationsDelimiter;
  long comments, inhibit = 0;
  double timeDuration, endTime, timeLeft, theTimeWS, connectTimeout, nonConnectsHandled, zeroTime;
  short verbose, eraseFile;
  long i_arg, timeUnits;
  long generations;
  int32_t generationsDigits;
  unsigned long dummyFlags;
  long offsetTimeOfDay;
  /*extern time_t timezone;*/
  chid inhibitID;
  double InhibitPendIOTime = 10;
  int32_t InhibitWaitTime = 5;
  PV_VALUE InhibitPV;
  long dailyFiles = 0, enforceTimeLimit = 0;
  double HourNow = 0, LastHour, Hour;
  struct stat input_filestat;
  int watchInput = 0, i, rcResult;
  double PingTime;
  double toleranceMinLimit = 1e-99;

  SDDS_RegisterProgramName(argv[0]);

  timeZoneOffset = epicsSecondsWestOfUTC();
  //timeZoneOffsetInHours = epicsSecondsWestOfUTC() / 3600.0;
  pendEventTime = DEFAULT_PEND_EVENT;
  verbose = 0;
  timeDuration = 0;
  inputFile = outputFile = outputFileOrig = linkname = NULL;
  connectTimeout = 60;
  eraseFile = 0;
  generations = 0;
  commentParameter = commentText = NULL;
  comments = 0;
  offsetTimeOfDay = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4);
    fprintf(stderr, "%s", USAGE_WARNING);
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

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_TIMEDURATION:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &timeDuration) != 1 ||
            timeDuration <= 0)
          SDDS_Bomb("invalid -timeDuration syntax/values");
        if (s_arg[i_arg].n_items == 3) {
          if ((timeUnits = match_string(s_arg[i_arg].list[2], TimeUnitName, NTimeUnitNames, 0)) >= 0)
            timeDuration *= TimeUnitFactor[timeUnits];
          else
            SDDS_Bomb("invalid -timeDuration syntax/values");
        }
        enforceTimeLimit = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_SDDSLOGGER:
        sddslogger = 1;
        break;
      case CLO_NOTOLERANCEMINLIMIT:
        toleranceMinLimit = 0;
        break;
      case CLO_PENDEVENTTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &pendEventTime) != 1 ||
            pendEventTime <= 0)
          SDDS_Bomb("invalid -pendEventTime syntax/values");
        break;
      case CLO_EXPLICIT:
        outputMode |= EXPLICIT;
        if (s_arg[i_arg].n_items >= 2) {
          outputMode |= NOINDICES;
          if (s_arg[i_arg].n_items > 2 ||
              strncmp("only", s_arg[i_arg].list[1],
                      strlen(s_arg[i_arg].list[1])) != 0)
            SDDS_Bomb("invalid -explicit qualifier(s)");
        }
        break;
      case CLO_DURATIONS:
        outputMode |= DURATIONS;
        break;
      case CLO_TIMEOUT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &connectTimeout) != 1 ||
            connectTimeout <= 0)
          SDDS_Bomb("invalid -connectTimeout syntax/values");
        break;
      case CLO_ERASE:
        eraseFile = 1;
        break;
      case CLO_APPEND:
        if (s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -append syntax/value");
        outputMode |= APPEND;
        if (s_arg[i_arg].n_items == 2) {
          if (strncmp(s_arg[i_arg].list[1], "recover", strlen(s_arg[i_arg].list[1])) == 0)
            outputMode |= RECOVER;
          else
            SDDS_Bomb("invalid -append syntax/value");
        }
        break;
      case CLO_GENERATIONS:
        generationsDigits = DEFAULT_GENERATIONS_DIGITS;
        generations = 1;
        generationsDelimiter = "-";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &generationsDigits, 1, 0,
                          "delimiter", SDDS_STRING, &generationsDelimiter, 1, 0,
                          NULL) ||
            generationsDigits < 1)
          SDDS_Bomb("invalid -generations syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DAILY_FILES:
        generationsDigits = 4;
        generationsDelimiter = ".";
        dailyFiles = 1;
        break;
      case CLO_COMMENT:
        ProcessCommentOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1,
                             &commentParameter, &commentText, &comments);
        break;
      case CLO_REQUIRECHANGE:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "severity", -1, NULL, 0, REQSEVERITYCHANGE,
                          "status", -1, NULL, 0, REQSTATUSCHANGE,
                          "both", -1, NULL, 0, REQBOTHCHANGE,
                          NULL) ||
            (dummyFlags & REQBOTHCHANGE && dummyFlags & (REQSEVERITYCHANGE + REQSTATUSCHANGE)))
          SDDS_Bomb("invalid -requireChange syntax");
        outputMode |= dummyFlags;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_INHIBIT:
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
          SDDS_Bomb("invalid -InhibitPV syntax/values");
        inhibit = 1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_WATCHINPUT:
        watchInput = 1;
        break;
      case CLO_LOGINITIALVALUES:
        outputMode |= LOGINITIAL;
        break;
      case CLO_LOGALARMS:
        outputMode |= LOGALARMS;
        break;
      case CLO_INCLUDETIMEOFDAY:
        outputMode |= TIMEOFDAY;
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
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else if (!outputFileOrig)
        outputFileOrig = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }
#if DEBUG
  fprintf(stderr, "Argument parsing done.\n");
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
      return (1);
    }
  }
#endif
  atexit(rc_interrupt_handler);

  if (inhibit) {
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 1)) {
      fprintf(stderr, "error: problem doing search for Inhibit PV %s\n", InhibitPV.name);
      return (1);
    }
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Inhibit PV %s is non zero. Exiting\n", InhibitPV.name);
      return (0);
    }
  }

  if (!inputFile)
    SDDS_Bomb("input filename not given");
  if (!outputFileOrig)
    SDDS_Bomb("output filename not given");

  if (watchInput) {
    linkname = read_last_link_to_file(inputFile);
    if (get_file_stat(inputFile, linkname, &input_filestat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputFile);
      return (1);
    }
  }

  if (eraseFile && (outputMode & APPEND))
    SDDS_Bomb("incompatible options -eraseFile and -append given");
  if ((generations || dailyFiles) && (outputMode & APPEND))
    SDDS_Bomb("incompatible options -generations/dailyFiles and -append given");
  if (eraseFile && (generations || dailyFiles))
    SDDS_Bomb("-eraseFile and -generations/dailyFiles are incompatible options");
  if (generations && dailyFiles)
    SDDS_Bomb("-generations and -dailyFiles are incompatible");
  if (generations)
    outputFile = MakeGenerationFilename(outputFileOrig, generationsDigits, generationsDelimiter, NULL);
  if (dailyFiles) {
    outputFile = MakeDailyGenerationFilename(outputFileOrig, generationsDigits, generationsDelimiter, 0);
    if (verbose)
      fprintf(stdout, "New daily file started: %s\n", outputFile);
  }
  if (!(generations) && !(dailyFiles))
    outputFile = outputFileOrig;
  HourNow = getHourOfDay();
  if (fexists(outputFile) && !(eraseFile || outputMode & APPEND))
    SDDS_Bomb("output file already exists");
  /*
    if (timeDuration<=0)
    SDDS_Bomb("time duration not specified");
  */
  if (comments && (outputMode & APPEND))
    fprintf(stderr, "warning: comments ignored for -append mode\n");
#if DEBUG
  fprintf(stderr, "Argument checking done; output file will be %s.\n", outputFile);
#endif

  description = NULL;
  relatedName = NULL;
  readbackUnits = NULL;
  readbackName = NULL;
  tolerances = NULL;
  expectNumeric = NULL;
  expectFieldType = NULL;
  expectElements = NULL;
  if ((controlNames = GetControlNames(inputFile, &controlName,
                                      &readbackName, &readbackUnits,
                                      &relatedName, &description,
                                      &tolerances, &expectNumeric,
                                      &expectFieldType, &expectElements)) <= 0)
    SDDS_Bomb("unable to get ControlName data from input file");
  if (readbackName)
    outputMode |= READBACKNAME;
  if (readbackUnits)
    outputMode |= READBACKUNITS;
  if (description)
    outputMode |= DESCRIPTIONS;
  if (relatedName)
    outputMode |= RELATEDVALUES;
  if (tolerances)
    outputMode |= TOLERANCES;

  if (!InitiateConnections(controlName, relatedName, description, controlNames, tolerances, &chInfo, toleranceMinLimit, connectTimeout, expectNumeric, expectFieldType, expectElements))
    SDDS_Bomb("unable to connect to PVs");

  /* Check that waveform PVs are not used with sddslogger mode */
  if (sddslogger) {
    for (i = 0; i < controlNames; i++) {
      if (chInfo[i].isArray) {
        fprintf(stderr, "error: waveform PV %s is not supported in sddslogger mode\n", controlName[i]);
        return (1);
      }
    }
  }

#if DEBUG
  fprintf(stderr, "Setting up log file\n");
#endif
  if (!sddslogger) {
    if (!SetupLogFile(&logDataset, outputFile, inputFile, controlName, readbackName, readbackUnits, description,
                      relatedName, controlNames, outputMode, commentParameter, commentText, comments))
      SDDS_Bomb("unable to set up log output file");
  } else {
    if (!SetupSDDSLoggerFile(&logDataset, outputFile, inputFile, controlName, readbackName, readbackUnits, description,
                             relatedName, controlNames, outputMode, commentParameter, commentText, comments))
      SDDS_Bomb("unable to set up sdds logger output file");
  }
  if (offsetTimeOfDay && enforceTimeLimit && (startHour * 3600.0 + timeDuration - 24.0 * 3600.0) > 0.5 * timeDuration)
    hourOffset = 24;

#ifdef DEBUGSDDS
  fprintf(stderr,
          "rows = %ld, page = %ld, written=%ld, last_written=%ld, first=%ld \n",
          logDataset.n_rows, logDataset.page_number, logDataset.n_rows_written,
          logDataset.last_row_written, logDataset.first_row_in_mem);
#endif

  eventOccurred = 0;
  HourAdjust = startHour - startTime / 3600.0 - hourOffset;
  //HourIOCAdjust = HourAdjust + EPOCH_OFFSET / 3600.0 - timeZoneOffsetInHours;

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    if (runControlPingWhileSleep(0)) {
      fprintf(stderr, "Problem pinging the run control record.\n");
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
  if (!SetupEventCallbacks())
    SDDS_Bomb("unable to establish callbacks for PVs");

#ifdef USE_RUNCONTROL
  if ((rcParam.PV) && (pendEventTime > 2)) {
    rcResult = runControlPingWhileSleep(pendEventTime);
    if ((rcResult) && (rcResult != RUNCONTROL_ERROR)) {
      fprintf(stderr, "Problem pinging the run control record.\n");
      return (1);
    }
  } else {
    oag_ca_pend_event(pendEventTime, &sigint);
  }
#else
  oag_ca_pend_event(pendEventTime, &sigint);
#endif
  if (sigint)
    return (1);

  /* zeroTime is when the clock for this run starts, as opposed to startTime, which is when
   * this run or the run for which this is a continuation opened the output file 
   */
  PingTime = getTimeInSecs() - 2;
  endTime = (zeroTime = getTimeInSecs()) + timeDuration;
  nonConnectsHandled = 0;
  while (((timeLeft = endTime - (theTimeWS = getTimeInSecs())) > 0) ||
         (!enforceTimeLimit)) {
    LastHour = HourNow;
    HourNow = getHourOfDay();
    if (dailyFiles && (HourNow < LastHour)) {
      /* must start a new file */
      disableLog = 1;
      if (!SDDS_Terminate(&logDataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      logfileRow = 0;
      outputFile = MakeDailyGenerationFilename(outputFileOrig, generationsDigits, generationsDelimiter, 0);
      if (verbose)
        fprintf(stdout, "New daily file started: %s\n", outputFile);
      if (!sddslogger) {
        if (!SetupLogFile(&logDataset, outputFile, inputFile, controlName, readbackName, readbackUnits, description,
                          relatedName, controlNames, outputMode, commentParameter, commentText, comments))
          SDDS_Bomb("unable to set up log output file");
      } else {
        if (!SetupSDDSLoggerFile(&logDataset, outputFile, inputFile, controlName, readbackName, readbackUnits, description,
                                 relatedName, controlNames, outputMode, commentParameter, commentText, comments))
          SDDS_Bomb("unable to set up log output file");
      }
      theTimeWS = getTimeInSecs();
      Hour = (theTimeWS) / 3600.0 + HourAdjust;
      for (i = 0; i < controlNames; i++) {
        chInfo[i].lastRow = -2;
        LogEventToFile(i, theTimeWS, theTimeWS, Hour, Hour, theTimeWS - chInfo[i].lastChangeTimeWS,
                       chInfo[i].lastStatus, chInfo[i].lastSeverity, NULL);
      }
      disableLog = 0;
    }
    if (inhibit && QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Inhibit PV %s is non zero. Exiting\n", InhibitPV.name);
      return (0);
    }
    if ((verbose) && (enforceTimeLimit))
      fprintf(stderr, "%ld total events with %.2f seconds left to run\n", logfileRow, timeLeft);
    if (!nonConnectsHandled && (long)(theTimeWS - zeroTime) > connectTimeout) {
      LogUnconnectedEvents();
      nonConnectsHandled = 1;
    }
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (PingTime + 2 < getTimeInSecs()) {
        PingTime = getTimeInSecs();
        if (runControlPingWhileSleep(0)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          return (1);
        }
      }
    }
#endif
#ifdef USE_RUNCONTROL
    if ((rcParam.PV) && (pendEventTime > 2)) {
      if (runControlPingWhileSleep(pendEventTime)) {
        fprintf(stderr, "Problem pinging the run control record.\n");
        return (1);
      }
    } else {
      oag_ca_pend_event(pendEventTime, &sigint);
    }
#else
    oag_ca_pend_event(pendEventTime, &sigint);
#endif
    if (sigint)
      return (1);

    if (eventOccurred && !SDDS_UpdatePage(&logDataset, FLUSH_TABLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    eventOccurred = 0;
    if (watchInput) {
      if (file_is_modified(inputFile, &linkname, &input_filestat))
        break;
    }
#ifdef DEBUGSDDS
    fprintf(stderr, "main: logfileRow = %ld\n", logfileRow);
    fprintf(stderr,
            "main: rows = %ld, page = %ld, written=%ld, last_written=%ld, first=%ld \n",
            logDataset.n_rows, logDataset.page_number, logDataset.n_rows_written,
            logDataset.last_row_written, logDataset.first_row_in_mem);
#endif
  }
  if (verbose)
    fprintf(stderr, "%ld total events\n", logfileRow);

  if (!SDDS_Terminate(&logDataset)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  }
  return (0);
}

void LogUnconnectedEvents(void) {
  long i;
  double theTimeWS, Hour;
  Hour = startHour + ((theTimeWS = getTimeInSecs()) - startTime) / 3600.0 - hourOffset;
  for (i = 0; i < controlNames; i++) {
    if (ca_state(chInfo[i].channelID) == cs_conn)
      continue;
    fprintf(stderr, "Warning: the following channel did not connect: %s\n",
            chInfo[i].controlName);
    chInfo[i].lastRow = -1;      /* indicates nothing logged prior to this */
    chInfo[i].lastSeverity = -1; /* will result in logging of a NO_ALARM event if one occurs next due to 
                                    * connection of the channel.  See EventHandler().
                                    */
    chInfo[i].lastChangeTime = theTimeWS;
    chInfo[i].lastChangeTimeWS = theTimeWS;
    if (chInfo[i].isArray) {
      memset(chInfo[i].waveformValues, 0, chInfo[i].elementCount * sizeof(double));
      LogWaveformEventToFile(i, chInfo[i].elementCount, theTimeWS, theTimeWS, Hour, Hour, theTimeWS - startTime,
                             TIMEOUT_ALARM, INVALID_ALARM);
    } else {
      if (chInfo[i].isString)
        chInfo[i].stringValue[0] = '\0';
      /* log this "event" as a TIMEOUT with INVALID status */
      LogEventToFile(i, theTimeWS, theTimeWS, Hour, Hour, theTimeWS - startTime,
                     TIMEOUT_ALARM, INVALID_ALARM, "");
    }
  }
  ca_pend_event(0.0001);
}

void EventHandler(struct event_handler_args event) {
  struct dbr_time_double *dbrValue;
  double Hour, theTime, theTimeWS, HourIOC;
  long index;
  short statusIndex, severityIndex, logEvent;
  TS_STAMP *tsStamp;

  if (disableLog) {
    return;
  }
  /* Hour = startHour + ((theTime=getTimeInSecs())-startTime)/3600.0 - hourOffset; */
  Hour = (theTimeWS = getTimeInSecs()) / 3600.0 + HourAdjust;

  index = *((long *)event.usr);
  if (ca_field_type(chInfo[index].channelID) == DBR_STRING) {
    return;
  }
  dbrValue = (struct dbr_time_double *)event.dbr;
  statusIndex = (short)dbrValue->status;
  severityIndex = (short)dbrValue->severity;
  tsStamp = &(dbrValue->stamp);
  theTime = tsStamp->secPastEpoch + 1e-9 * tsStamp->nsec + EPOCH_OFFSET - timeZoneOffset;
  //HourIOC = theTime / 3600.0 + HourIOCAdjust;
  HourIOC = theTime / 3600.0 + HourAdjust;

#ifdef DEBUG
  fprintf(stderr, "EventHandler : index = %ld, status = %ld, severity = %ld\n",
          index, statusIndex, severityIndex);
  fprintf(stderr, "chInfo pointer: %x\n", (unsigned long)chInfo);
  fprintf(stderr, "pv name pointer = %x\n", chInfo[index].controlName);
  fprintf(stderr, "event: pv=%s  status=%s  severity=%s\n",
          chInfo[index].controlName, alarmStatusString[statusIndex],
          alarmSeverityString[severityIndex]);
#endif
  if (!(outputMode & LOGINITIAL)) {
    if (chInfo[index].lastRow == -2 && (severityIndex == INVALID_ALARM || severityIndex == NO_ALARM)) {
      /* Channel valid but with no previous callbacks received--most valid channels come through here. */
      /* Don't log initial invalid or no-alarm state.  */
      chInfo[index].lastRow = -1; /* indicates initial callback received but nothing logged */
      chInfo[index].lastSeverity = severityIndex;
      chInfo[index].lastStatus = statusIndex;
#ifdef DEBUG
      fprintf(stderr, "event ignored: condition 1\n");
#endif
      return;
    }
  }
  if (chInfo[index].lastRow == -2) {
    //Use the workstation time when logging the initial value. The IOC timestamp is the last time the PV changed.
    theTime = theTimeWS;
  }
  if (chInfo[index].lastRow == -1 && severityIndex == NO_ALARM && chInfo[index].lastSeverity == INVALID_ALARM) {
    /* Don't log initial NO_ALARM "alarm" following INVALID_ALARM.  However, record the NO_ALARM
       * severity for future use.
       */
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
#ifdef DEBUG
    fprintf(stderr, "event ignored: condition 2\n");
#endif
    return;
  }
  if (chInfo[index].logPending && chInfo[index].severityIndex > severityIndex) {
    /* a write is pending for a more severe alarm, so ignore this one. */
#ifdef DEBUG
    fprintf(stderr, "write pending: event is not being logged.\n");
#endif
    return;
  }
  if ((chInfo[index].lastRow >= 0) &&
      (fabs(chInfo[index].value - (double)dbrValue->value) < chInfo[index].tolerance)) {
#ifdef DEBUG
    fprintf(stderr, "event ignored: change is too small\n");
#endif
    return;
  }

  logEvent = 1;
  if (outputMode & REQBOTHCHANGE) {
    logEvent = chInfo[index].lastSeverity != severityIndex && chInfo[index].lastStatus != statusIndex;
  } else if (outputMode & REQSTATUSCHANGE) {
    logEvent = chInfo[index].lastStatus != statusIndex;
  } else if (outputMode & REQSEVERITYCHANGE) {
    logEvent = chInfo[index].lastSeverity != severityIndex;
  }
  if (logEvent) {
#ifdef DEBUG
    fprintf(stderr, "event is being logged.\n");
#endif
    chInfo[index].value = (double)dbrValue->value;
    if (chInfo[index].relatedControlName) {
      /* save values pertinent to this alarm.
           */
      chInfo[index].logPending = 1;
      chInfo[index].index = index;
      chInfo[index].theTime = theTime;
      chInfo[index].theTimeWS = theTimeWS;
      chInfo[index].Hour = Hour;
      chInfo[index].HourIOC = HourIOC;
      chInfo[index].duration = theTime - chInfo[index].lastChangeTime;
      chInfo[index].statusIndex = statusIndex;
      chInfo[index].severityIndex = severityIndex;
      /* set up a callback to get the related value and write it out */
      if (ca_get_callback(DBR_STRING, chInfo[index].relatedChannelID,
                          RelatedValueEventHandler, (void *)&chInfo[index].usrValue) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to establish callback for control name %s (related value for %s)\n",
                chInfo[index].relatedControlName, chInfo[index].controlName);
        exit(1);
      }
#ifdef DEBUG
      fprintf(stderr, "Callback set up for related value\n");
#endif
    } else {
      LogEventToFile(index, theTime, theTimeWS, Hour, HourIOC,
                     theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                     NULL);
    }
  } else {
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
  }
}

void StringEventHandler(struct event_handler_args event) {
  struct dbr_time_string *dbrValue;
  double Hour, theTime, theTimeWS, HourIOC;
  long index;
  short statusIndex, severityIndex, logEvent;
  TS_STAMP *tsStamp;

  if (disableLog) {
    return;
  }
  Hour = (theTimeWS = getTimeInSecs()) / 3600.0 + HourAdjust;

  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_string *)event.dbr;
  statusIndex = (short)dbrValue->status;
  severityIndex = (short)dbrValue->severity;
  tsStamp = &(dbrValue->stamp);
  theTime = tsStamp->secPastEpoch + 1e-9 * tsStamp->nsec + EPOCH_OFFSET - timeZoneOffset;
  HourIOC = theTime / 3600.0 + HourAdjust;

#ifdef DEBUG
  fprintf(stderr, "StringEventHandler : index = %ld, status = %ld, severity = %ld\n",
          index, statusIndex, severityIndex);
  fprintf(stderr, "event: pv=%s  status=%s  severity=%s  value=%s\n",
          chInfo[index].controlName, alarmStatusString[statusIndex],
          alarmSeverityString[severityIndex], dbrValue->value);
#endif
  if (!(outputMode & LOGINITIAL)) {
    if (chInfo[index].lastRow == -2 && (severityIndex == INVALID_ALARM || severityIndex == NO_ALARM)) {
      chInfo[index].lastRow = -1;
      chInfo[index].lastSeverity = severityIndex;
      chInfo[index].lastStatus = statusIndex;
      strncpy(chInfo[index].stringValue, dbrValue->value, MAX_STRING_SIZE - 1);
      chInfo[index].stringValue[MAX_STRING_SIZE - 1] = '\0';
#ifdef DEBUG
      fprintf(stderr, "string event ignored: condition 1\n");
#endif
      return;
    }
  }
  if (chInfo[index].lastRow == -2) {
    theTime = theTimeWS;
  }
  if (chInfo[index].lastRow == -1 && severityIndex == NO_ALARM && chInfo[index].lastSeverity == INVALID_ALARM) {
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
#ifdef DEBUG
    fprintf(stderr, "string event ignored: condition 2\n");
#endif
    return;
  }
  if (chInfo[index].logPending && chInfo[index].severityIndex > severityIndex) {
#ifdef DEBUG
    fprintf(stderr, "string write pending: event is not being logged.\n");
#endif
    return;
  }
  /* For string PVs, compare string values instead of numeric tolerance */
  if ((chInfo[index].lastRow >= 0) &&
      strcmp(chInfo[index].stringValue, dbrValue->value) == 0) {
#ifdef DEBUG
    fprintf(stderr, "string event ignored: no string change\n");
#endif
    return;
  }

  logEvent = 1;
  if (outputMode & REQBOTHCHANGE) {
    logEvent = chInfo[index].lastSeverity != severityIndex && chInfo[index].lastStatus != statusIndex;
  } else if (outputMode & REQSTATUSCHANGE) {
    logEvent = chInfo[index].lastStatus != statusIndex;
  } else if (outputMode & REQSEVERITYCHANGE) {
    logEvent = chInfo[index].lastSeverity != severityIndex;
  }
  if (logEvent) {
#ifdef DEBUG
    fprintf(stderr, "string event is being logged.\n");
#endif
    strncpy(chInfo[index].stringValue, dbrValue->value, MAX_STRING_SIZE - 1);
    chInfo[index].stringValue[MAX_STRING_SIZE - 1] = '\0';
    if (chInfo[index].relatedControlName) {
      chInfo[index].logPending = 1;
      chInfo[index].index = index;
      chInfo[index].theTime = theTime;
      chInfo[index].theTimeWS = theTimeWS;
      chInfo[index].Hour = Hour;
      chInfo[index].HourIOC = HourIOC;
      chInfo[index].duration = theTime - chInfo[index].lastChangeTime;
      chInfo[index].statusIndex = statusIndex;
      chInfo[index].severityIndex = severityIndex;
      if (ca_get_callback(DBR_STRING, chInfo[index].relatedChannelID,
                          RelatedValueEventHandler, (void *)&chInfo[index].usrValue) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to establish callback for control name %s (related value for %s)\n",
                chInfo[index].relatedControlName, chInfo[index].controlName);
        exit(1);
      }
    } else {
      LogEventToFile(index, theTime, theTimeWS, Hour, HourIOC,
                     theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                     NULL);
    }
  } else {
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
  }
}

void WaveformEventHandler(struct event_handler_args event) {
  struct dbr_time_double *dbrValue;
  double Hour, theTime, theTimeWS, HourIOC;
  long index, j, actualCount;
  short statusIndex, severityIndex, logEvent;
  TS_STAMP *tsStamp;
  double *newData;

  if (disableLog)
    return;
  Hour = (theTimeWS = getTimeInSecs()) / 3600.0 + HourAdjust;

  index = *((long *)event.usr);
  actualCount = event.count;
  dbrValue = (struct dbr_time_double *)event.dbr;
  statusIndex = (short)dbrValue->status;
  severityIndex = (short)dbrValue->severity;
  tsStamp = &(dbrValue->stamp);
  theTime = tsStamp->secPastEpoch + 1e-9 * tsStamp->nsec + EPOCH_OFFSET - timeZoneOffset;
  HourIOC = theTime / 3600.0 + HourAdjust;
  newData = &dbrValue->value;

#ifdef DEBUG
  fprintf(stderr, "WaveformEventHandler : index = %ld, actualCount = %ld (max %ld), status = %hd, severity = %hd\n",
          index, actualCount, chInfo[index].elementCount, statusIndex, severityIndex);
#endif

  if (!(outputMode & LOGINITIAL)) {
    if (chInfo[index].lastRow == -2 && (severityIndex == INVALID_ALARM || severityIndex == NO_ALARM)) {
      chInfo[index].lastRow = -1;
      chInfo[index].lastSeverity = severityIndex;
      chInfo[index].lastStatus = statusIndex;
      memcpy(chInfo[index].waveformValues, newData, actualCount * sizeof(double));
      return;
    }
  }
  if (chInfo[index].lastRow == -2) {
    theTime = theTimeWS;
  }
  if (chInfo[index].lastRow == -1 && severityIndex == NO_ALARM && chInfo[index].lastSeverity == INVALID_ALARM) {
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
    return;
  }
  if (chInfo[index].logPending && chInfo[index].severityIndex > severityIndex) {
    return;
  }

  /* No tolerance for waveform PVs -- check for any element change */
  if (chInfo[index].lastRow >= 0) {
    short changed = 0;
    for (j = 0; j < actualCount; j++) {
      if (chInfo[index].waveformValues[j] != newData[j]) {
        changed = 1;
        break;
      }
    }
    if (!changed)
      return;
  }

  logEvent = 1;
  if (outputMode & REQBOTHCHANGE)
    logEvent = chInfo[index].lastSeverity != severityIndex && chInfo[index].lastStatus != statusIndex;
  else if (outputMode & REQSTATUSCHANGE)
    logEvent = chInfo[index].lastStatus != statusIndex;
  else if (outputMode & REQSEVERITYCHANGE)
    logEvent = chInfo[index].lastSeverity != severityIndex;

  if (logEvent) {
    memcpy(chInfo[index].waveformValues, newData, actualCount * sizeof(double));
    LogWaveformEventToFile(index, actualCount, theTime, theTimeWS, Hour, HourIOC,
                           theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex);
  } else {
    chInfo[index].lastSeverity = severityIndex;
    chInfo[index].lastStatus = statusIndex;
  }
}

void RelatedValueEventHandler(struct event_handler_args event) {
  long index;
  index = *((long *)event.usr);
#ifdef DEBUG
  fprintf(stderr, "RelatedValueEventHandler called with index %ld\n", index);
#endif
  LogEventToFile(index, chInfo[index].theTime, chInfo[index].theTimeWS, chInfo[index].Hour, chInfo[index].HourIOC,
                 chInfo[index].duration,
                 chInfo[index].statusIndex, chInfo[index].severityIndex,
                 (char *)(event.dbr));
  return;
}

void LogEventToFile(long index, double theTime, double theTimeWS, double Hour, double HourIOC, double duration,
                    short statusIndex, short severityIndex, char *relatedValueString) {
  eventOccurred = 1;
  while (logDataset.n_rows_allocated <= logDataset.n_rows) {
    if (!SDDS_LengthenTable(&logDataset, 30)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  if (!sddslogger) {
    if ((outputMode & TIMEOFDAY &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iHour, (float)Hour,
                            iHourIOC, (float)HourIOC, -1)) ||
        !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                           iPreviousRow, chInfo[index].lastRow,
                           iValue, chInfo[index].value,
                           iTime, theTime, iTimeWS, theTimeWS, -1) ||
        (outputMode & DURATIONS &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iDuration, (float)(theTime - chInfo[index].lastChangeTime), -1)) ||
        (outputMode & RELATEDVALUES &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iRelatedValueString, relatedValueString, -1)) ||
        (!(outputMode & NOINDICES) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iControlNameIndex, index,
                            -1)) ||
        (!(outputMode & NOINDICES) && (outputMode & LOGALARMS) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iAlarmStatusIndex, statusIndex,
                            iAlarmSeverityIndex, severityIndex,
                            -1)) ||
        (outputMode & EXPLICIT &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iControlName, chInfo[index].controlName,
                            -1)) ||
        (outputMode & EXPLICIT && (outputMode & LOGALARMS) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iAlarmStatus, alarmStatusString[statusIndex],
                            iAlarmSeverity, alarmSeverityString[severityIndex],
                            -1)) ||
        (outputMode & EXPLICIT && outputMode & DESCRIPTIONS &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iDescription, chInfo[index].description, -1)) ||
        (outputMode & EXPLICIT && outputMode & RELATEDVALUES &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iRelatedName, chInfo[index].relatedControlName, -1)) ||
        (iStringValue >= 0 &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iStringValue, chInfo[index].isString ? chInfo[index].stringValue : "", -1)) ||
        (iWaveformIndex >= 0 &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iWaveformIndex, (int32_t)-1,
                            iWaveformLength, (int32_t)0, -1))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  } else {
    long i;
    if ((theTime == lastLogTime) && (theTimeWS == lastLogTimeWS)) {
      logfileRow--;
      for (i = 0; i < controlNames; i++) {
        if (chInfo[i].isString) {
          if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, i + 1, chInfo[i].stringValue, -1)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        } else {
          if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, i + 1, chInfo[i].value, -1)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
      }
    } else {
      lastLogTime = theTime;
      lastLogTimeWS = theTimeWS;
      if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, 0, theTime, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, 1, theTimeWS, -1)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (i = 0; i < controlNames; i++) {
        if (chInfo[i].isString) {
          if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, i + 2, chInfo[i].stringValue, -1)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        } else {
          if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow, i + 2, chInfo[i].value, -1)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        }
      }
    }
  }
  chInfo[index].lastChangeTime = theTime;
  chInfo[index].lastChangeTimeWS = theTimeWS;
  chInfo[index].lastRow = logfileRow;
  chInfo[index].lastSeverity = severityIndex;
  chInfo[index].lastStatus = statusIndex;
  logfileRow++;
  chInfo[index].logPending = 0;
}

void LogWaveformEventToFile(long index, long actualCount, double theTime, double theTimeWS, double Hour, double HourIOC, double duration,
                            short statusIndex, short severityIndex) {
  long j;
  eventOccurred = 1;
  while (logDataset.n_rows_allocated <= logDataset.n_rows + actualCount) {
    if (!SDDS_LengthenTable(&logDataset, actualCount + 30)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  for (j = 0; j < actualCount; j++) {
    if ((outputMode & TIMEOFDAY &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iHour, (float)Hour,
                            iHourIOC, (float)HourIOC, -1)) ||
        !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                           iPreviousRow, chInfo[index].lastRow,
                           iValue, chInfo[index].waveformValues[j],
                           iTime, theTime, iTimeWS, theTimeWS, -1) ||
        !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                           iWaveformIndex, (int32_t)j,
                           iWaveformLength, (int32_t)actualCount, -1) ||
        (outputMode & DURATIONS &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iDuration, (float)duration, -1)) ||
        (outputMode & RELATEDVALUES &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iRelatedValueString, "", -1)) ||
        (!(outputMode & NOINDICES) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iControlNameIndex, index, -1)) ||
        (!(outputMode & NOINDICES) && (outputMode & LOGALARMS) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iAlarmStatusIndex, statusIndex,
                            iAlarmSeverityIndex, severityIndex, -1)) ||
        (outputMode & EXPLICIT &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iControlName, chInfo[index].controlName, -1)) ||
        (outputMode & EXPLICIT && (outputMode & LOGALARMS) &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iAlarmStatus, alarmStatusString[statusIndex],
                            iAlarmSeverity, alarmSeverityString[severityIndex], -1)) ||
        (outputMode & EXPLICIT && outputMode & DESCRIPTIONS &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iDescription, chInfo[index].description, -1)) ||
        (outputMode & EXPLICIT && outputMode & RELATEDVALUES &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iRelatedName, chInfo[index].relatedControlName, -1)) ||
        (iStringValue >= 0 &&
         !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                            iStringValue, "", -1))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    logfileRow++;
  }
  chInfo[index].lastChangeTime = theTime;
  chInfo[index].lastChangeTimeWS = theTimeWS;
  chInfo[index].lastRow = logfileRow - actualCount;
  chInfo[index].lastSeverity = severityIndex;
  chInfo[index].lastStatus = statusIndex;
  chInfo[index].logPending = 0;
}

long InitiateConnections(char **cName, char **relatedName, char **description, long cNames, double *tolerances, CHANNEL_INFO **chInfo0, double toleranceMinLimit, double connectTimeout, short *expectNumeric, char **expectFieldType, int32_t *expectElements) {
  long i;
  CHANNEL_INFO *chInfo;

#ifdef DEBUG
  fprintf(stderr, "initiating connections for %ld control names\n", cNames);
#endif
  if (!(chInfo = (CHANNEL_INFO *)malloc(sizeof(*chInfo) * cNames)))
    return (0);
  *chInfo0 = chInfo;
  for (i = 0; i < cNames; i++) {
    chInfo[i].lastRow = -2;      /* indicates no data logged yet for this channel */
    chInfo[i].lastSeverity = -1; /* invalid severity code--indicates no callbacks yet for this channel */
    chInfo[i].value = 0.0;
    /* Use ExpectNumeric hint from input file if available */
    if (expectNumeric)
      chInfo[i].isString = (expectNumeric[i] == 'n' || expectNumeric[i] == 'N') ? 1 : 0;
    else
      chInfo[i].isString = 0;
    chInfo[i].stringValue[0] = '\0';
    /* Use ExpectFieldType hint for array detection */
    if (expectFieldType && expectFieldType[i] && strcmp(expectFieldType[i], "scalarArray") == 0) {
      chInfo[i].isArray = 1;
      chInfo[i].elementCount = (expectElements && expectElements[i] > 1) ? expectElements[i] : 1;
    } else {
      chInfo[i].isArray = 0;
      chInfo[i].elementCount = 1;
    }
    chInfo[i].waveformValues = NULL;
    chInfo[i].controlName = cName[i];
    if (description)
      chInfo[i].description = description[i];
    else
      chInfo[i].description = NULL;
    if (relatedName && relatedName[i] && strlen(relatedName[i]))
      chInfo[i].relatedControlName = relatedName[i];
    else
      chInfo[i].relatedControlName = NULL;
    if (tolerances) {
      if (tolerances[i] < 0) {
        chInfo[i].tolerance = toleranceMinLimit;
      } else {
        chInfo[i].tolerance = tolerances[i];
      }
    } else
      chInfo[i].tolerance = toleranceMinLimit;
    chInfo[i].usrValue = i;
    chInfo[i].lastChangeTime = getTimeInSecs(); /* will be used if channel connection times out */
    chInfo[i].logPending = 0;
  }
  for (i = 0; i < cNames; i++) {
    if (ca_search(cName[i], &chInfo[i].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", cName[i]);
      return (0);
    }
    if (chInfo[i].relatedControlName &&
        ca_search(relatedName[i], &chInfo[i].relatedChannelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for related control name %s\n", relatedName[i]);
      return (0);
    }
  }
  ca_pend_io(connectTimeout);
  /* Determine field types and element counts after connections are established.
   * If PV is connected, ca_field_type and ca_element_count override hints.
   * If PV is not connected, the hints (if provided) are kept.
   */
  for (i = 0; i < cNames; i++) {
    if (ca_state(chInfo[i].channelID) == cs_conn) {
      long nelm = ca_element_count(chInfo[i].channelID);
      if (nelm > 1) {
        /* PV is a waveform array */
        if (!expectFieldType || !expectFieldType[i] || strcmp(expectFieldType[i], "scalarArray") != 0) {
          fprintf(stderr, "error: PV %s is a waveform (NELM %ld) but ExpectFieldType is not set to \"scalarArray\"\n",
                  cName[i], nelm);
          return (0);
        }
        chInfo[i].isArray = 1;
        chInfo[i].isString = 0;
        /* Use ExpectElements for the subscription/logging count if provided;
         * ca_element_count() returns NELM (max capacity), not NORD (actual count).
         * ExpectElements should match NORD or the desired element count.
         * NELM is only used for buffer allocation to handle the maximum safely.
         */
        if (expectElements && expectElements[i] > 1)
          chInfo[i].elementCount = expectElements[i];
        else
          chInfo[i].elementCount = nelm;
      } else {
        /* Scalar PV */
        chInfo[i].isArray = 0;
        chInfo[i].elementCount = 1;
        if (ca_field_type(chInfo[i].channelID) == DBR_STRING)
          chInfo[i].isString = 1;
        else
          chInfo[i].isString = 0;
      }
#ifdef DEBUG
      fprintf(stderr, "PV %s: isArray=%d, isString=%d, elementCount=%ld (NELM=%ld) via CA\n",
              cName[i], chInfo[i].isArray, chInfo[i].isString, chInfo[i].elementCount, nelm);
#endif
      /* Allocate waveform buffer using NELM for safety */
      if (chInfo[i].isArray && nelm > 1) {
        if (!(chInfo[i].waveformValues = (double *)calloc(nelm, sizeof(double)))) {
          fprintf(stderr, "error: memory allocation failed for waveform buffer of PV %s (%ld elements)\n",
                  cName[i], nelm);
          return (0);
        }
      }
    } else {
#ifdef DEBUG
      fprintf(stderr, "PV %s not connected, using hints: isArray=%d, isString=%d, elementCount=%ld\n",
              cName[i], chInfo[i].isArray, chInfo[i].isString, chInfo[i].elementCount);
#endif
      /* Allocate waveform buffer for unconnected array PVs using ExpectElements */
      if (chInfo[i].isArray && chInfo[i].elementCount > 1) {
        if (!(chInfo[i].waveformValues = (double *)calloc(chInfo[i].elementCount, sizeof(double)))) {
          fprintf(stderr, "error: memory allocation failed for waveform buffer of PV %s (%ld elements)\n",
                  cName[i], chInfo[i].elementCount);
          return (0);
        }
      }
    }
  }
  return (1);
}

long SetupEventCallbacks(void) {
  long i;
  for (i = 0; i < controlNames; i++) {
    if (chInfo[i].isArray) {
      if (ca_add_masked_array_event(DBR_TIME_DOUBLE, chInfo[i].elementCount, chInfo[i].channelID, WaveformEventHandler,
                                    (void *)&chInfo[i].usrValue, (ca_real)0, (ca_real)0, (ca_real)0,
                                    NULL, DBE_VALUE | DBE_ALARM) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to setup waveform callback for control name %s\n", chInfo[i].controlName);
        return (0);
      }
    } else if (chInfo[i].isString) {
      if (ca_add_masked_array_event(DBR_TIME_STRING, 1, chInfo[i].channelID, StringEventHandler,
                                    (void *)&chInfo[i].usrValue, (ca_real)0, (ca_real)0, (ca_real)0,
                                    NULL, DBE_VALUE | DBE_ALARM) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to setup string callback for control name %s\n", chInfo[i].controlName);
        return (0);
      }
    } else {
      if (ca_add_masked_array_event(DBR_TIME_DOUBLE, 1, chInfo[i].channelID, EventHandler,
                                    (void *)&chInfo[i].usrValue, (ca_real)0, (ca_real)0, (ca_real)0,
                                    NULL, DBE_VALUE | DBE_ALARM) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to setup alarm callback for control name %s\n", chInfo[i].controlName);
        return (0);
      }
    }
  }
  ca_pend_io(1);
  return (1);
}

long SetupSDDSLoggerFile(SDDS_DATASET *logSet, char *output, char *input,
                         char **cName, char **rName, char **rUnits, char **description, char **relatedName,
                         long cNames, unsigned long mode,
                         char **commentParameter, char **commentText, long comments) {
  char *timeStamp, *pageTimeStamp;
  double startDay, startJulianDay, startYear, startMonth;
  long i;
  if (!(mode & APPEND) || !fexists(output)) {
    getTimeBreakdown(&startTime, &startDay, &startHour, &startJulianDay,
                     &startMonth, &startYear, &timeStamp);
    SDDS_CopyString(&pageTimeStamp, timeStamp);
    if (!SDDS_InitializeOutput(logSet, SDDS_BINARY, 0, NULL, NULL, output)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (0 > SDDS_DefineParameter(logSet, "InputFile", NULL, NULL, "log input file", NULL, SDDS_STRING, input)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (!DefineLoggingTimeParameters(logSet)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (0 > SDDS_DefineColumn(logSet, "Time", NULL, "s", "IOC time since start of epoch", NULL, SDDS_DOUBLE, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (0 > SDDS_DefineColumn(logSet, "TimeWS", NULL, "s", "Workstation time since start of epoch", NULL, SDDS_DOUBLE, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    for (i = 0; i < cNames; i++) {
      long colType = (chInfo && chInfo[i].isString) ? SDDS_STRING : SDDS_DOUBLE;
      if (SDDS_DefineColumn(logSet, rName ? rName[i] : cName[i], NULL, rUnits ? rUnits[i] : NULL, cName[i], NULL, colType, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_SetRowCountMode(logSet, SDDS_FIXEDROWCOUNT) ||
        !SDDS_WriteLayout(logSet) ||
        !SDDS_StartPage(logSet, 0) ||
        (comments && !SetCommentParameters(logSet, commentParameter, commentText, comments))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  } else {
    char **ColumnName;
    int32_t ColumnNames;

    ColumnName = (char **)SDDS_GetColumnNames(logSet, &ColumnNames);
    for (i = 0; i < cNames; i++) {
      if (rName) {
        if (-1 == match_string(rName[i], ColumnName, ColumnNames, EXACT_MATCH)) {
          printf("ReadbackName %s doesn't match any columns in output file.\n", rName[i]);
          return (0);
        }
      } else {
        if (-1 == match_string(cName[i], ColumnName, ColumnNames, EXACT_MATCH)) {
          printf("ControlName %s doesn't match any columns in output file.\n", rName[i]);
          return (0);
        }
      }
    }
    for (i = 0; i < ColumnNames; i++) {
      if (rName) {
        if (strcmp(ColumnName[i], "Time") != 0 &&
            strcmp(ColumnName[i], "TimeWS") != 0 &&
            -1 == match_string(ColumnName[i], rName, cNames, EXACT_MATCH)) {
          fprintf(stderr, "Column name %s in output file does not match any readback names.\n", ColumnName[i]);
          return (0);
        }
      } else {
        if (strcmp(ColumnName[i], "Time") != 0 &&
            strcmp(ColumnName[i], "TimeWS") != 0 &&
            -1 == match_string(ColumnName[i], cName, cNames, EXACT_MATCH)) {
          fprintf(stderr, "Column name %s in output file does not match any control names.\n", ColumnName[i]);
          return (0);
        }
      }
    }
    getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &pageTimeStamp);
    if (!CheckLogOnChangeLogFile(output, &startTime, &startDay, &startHour,
                                 &startJulianDay, &startMonth, &startYear,
                                 &timeStamp)) {
      if (mode & RECOVER) {
        if (!SDDS_RecoverFile(output, RECOVERFILE_VERBOSE))
          SDDS_Bomb("unable to recover existing file");
        if (!CheckLogOnChangeLogFile(output, &startTime, &startDay, &startHour,
                                     &startJulianDay, &startMonth, &startYear, &timeStamp))
          SDDS_Bomb("recovery of corrupted input file failed");
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to get data from existing file---try -append=recover (may truncate file)");
      }
      if (!SDDS_InitializeAppend(logSet, output) || !SDDS_StartPage(logSet, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (0);
      }
    }
  }
  if (!SDDS_SetParameters(logSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                          "TimeStamp", timeStamp, "PageTimeStamp", pageTimeStamp,
                          "StartTime", startTime, "YearStartTime", computeYearStartTime(startTime),
                          "StartYear", (short)startYear,
                          "StartJulianDay", (short)startJulianDay,
                          "StartDayOfMonth", (short)startDay,
                          "StartMonth", (short)startMonth,
                          "StartHour", (float)startHour, NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

long SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **cName, char **rName, char **rUnits, char **description, char **relatedName,
                  long cNames, unsigned long mode,
                  char **commentParameter, char **commentText, long comments) {
  char *timeStamp, *pageTimeStamp;
  double startDay, startJulianDay, startYear, startMonth;

  if (!(mode & APPEND) || !fexists(output)) {
    getTimeBreakdown(&startTime, &startDay, &startHour, &startJulianDay,
                     &startMonth, &startYear, &timeStamp);
    SDDS_CopyString(&pageTimeStamp, timeStamp);
    if (!SDDS_InitializeOutput(logSet, SDDS_BINARY, 0, NULL, NULL, output) ||
        0 > SDDS_DefineParameter(logSet, "InputFile", NULL, NULL, "log input file",
                                 NULL, SDDS_STRING, input) ||
        !DefineLoggingTimeParameters(logSet) ||
        0 > (iPreviousRow =
               SDDS_DefineColumn(logSet, "PreviousRow", NULL, NULL, NULL, "%6ld", SDDS_LONG, 0)) ||
        (mode & TIMEOFDAY &&
         0 > (iHour =
                SDDS_DefineColumn(logSet, "TimeOfDayWS", NULL, "h", "Time of day in hours per workstation",
                                  "%8.5f", SDDS_FLOAT, 0))) ||
        (mode & TIMEOFDAY &&
         0 > (iHourIOC =
                SDDS_DefineColumn(logSet, "TimeOfDay", NULL, "h", "Time of day in hours per IOC",
                                  "%8.5f", SDDS_FLOAT, 0))) ||
        0 > (iTime = SDDS_DefineColumn(logSet, "Time", NULL, "s", NULL, NULL, SDDS_DOUBLE, 0)) ||
        0 > (iTimeWS = SDDS_DefineColumn(logSet, "TimeWS", NULL, "s", NULL, NULL, SDDS_DOUBLE, 0)) ||
        0 > (iValue = SDDS_DefineColumn(logSet, "Value", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0)) ||
        0 > (iStringValue = SDDS_DefineColumn(logSet, "StringValue", NULL, NULL, "String value for string PVs", NULL, SDDS_STRING, 0)) ||
        0 > (iWaveformIndex = SDDS_DefineColumn(logSet, "WaveformIndex", NULL, NULL, "Waveform element index (-1 for scalar PVs)", "%6ld", SDDS_LONG, 0)) ||
        0 > (iWaveformLength = SDDS_DefineColumn(logSet, "WaveformLength", NULL, NULL, "Total waveform element count (0 for scalar PVs)", "%6ld", SDDS_LONG, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (!(mode & NOINDICES) &&
        (0 > SDDS_DefineArray(logSet, "ControlName", NULL, NULL, NULL, "%32s", SDDS_STRING, 0, 1, NULL) ||
         (mode & READBACKNAME &&
          0 > SDDS_DefineArray(logSet, "ReadbackName", NULL, NULL, NULL, "%32s", SDDS_STRING, 0, 1, NULL)) ||
         (mode & READBACKUNITS &&
          0 > SDDS_DefineArray(logSet, "ReadbackUnits", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL)) ||
         (mode & LOGALARMS &&
          0 > SDDS_DefineArray(logSet, "AlarmStatusString", NULL, NULL, NULL, "%12s", SDDS_STRING, 0, 1, NULL)) ||
         (mode & LOGALARMS &&
          0 > SDDS_DefineArray(logSet, "AlarmSeverityString", NULL, NULL, NULL, "%10s", SDDS_STRING, 0, 1, NULL)) ||
         (mode & DESCRIPTIONS &&
          0 > SDDS_DefineArray(logSet, "DescriptionString", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL)) ||
         (mode & RELATEDVALUES &&
          0 > SDDS_DefineArray(logSet, "RelatedControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL)) ||
         0 > (iControlNameIndex =
                SDDS_DefineColumn(logSet, "ControlNameIndex", NULL, NULL, NULL, "%6ld", SDDS_LONG, 0)) ||
         (mode & LOGALARMS &&
          0 > (iAlarmStatusIndex =
                 SDDS_DefineColumn(logSet, "AlarmStatusIndex", NULL, NULL, NULL, "%6hd", SDDS_SHORT, 0))) ||
         (mode & LOGALARMS &&
          0 > (iAlarmSeverityIndex =
                 SDDS_DefineColumn(logSet, "AlarmSeverityIndex", NULL, NULL, NULL, "%6hd", SDDS_SHORT, 0))))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (mode & DURATIONS &&
        0 > (iDuration =
               SDDS_DefineColumn(logSet, "Duration", NULL, "s", NULL, "%8.4f", SDDS_FLOAT, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (mode & RELATEDVALUES &&
        0 > (iRelatedValueString =
               SDDS_DefineColumn(logSet, "RelatedValueString", NULL, NULL, NULL, NULL, SDDS_STRING, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (mode & EXPLICIT &&
        (0 > (iControlName =
                SDDS_DefineColumn(logSet, "ControlName", NULL, NULL, NULL, "%32s",
                                  SDDS_STRING, 0)) ||
         (mode & LOGALARMS &&
          0 > (iAlarmStatus =
                 SDDS_DefineColumn(logSet, "AlarmStatus", NULL, NULL, NULL, "%12s",
                                   SDDS_STRING, 0))) ||
         (mode & LOGALARMS &&
          0 > (iAlarmSeverity =
                 SDDS_DefineColumn(logSet, "AlarmSeverity", NULL, NULL, NULL, "%10s",
                                   SDDS_STRING, 0))) ||
         0 > (iDescription =
                SDDS_DefineColumn(logSet, "Description", NULL, NULL, NULL, NULL,
                                  SDDS_STRING, 0)) ||
         0 > (iRelatedName =
                SDDS_DefineColumn(logSet, "RelatedControlName", NULL, NULL, NULL, NULL,
                                  SDDS_STRING, 0)))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (comments && !SDDS_DefineSimpleParameters(logSet, comments, commentParameter, NULL, SDDS_STRING)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    /* SDDS_EnableFSync(logSet); */
    if (!SDDS_SetRowCountMode(logSet, SDDS_FIXEDROWCOUNT) ||
        !SDDS_WriteLayout(logSet) ||
        !SDDS_StartPage(logSet, 0) ||
        (comments && !SetCommentParameters(logSet, commentParameter, commentText, comments))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
  } else {
    if (!CheckForRequiredElements(output, mode)) {
      fprintf(stderr, "error (sddsalarmlog): file %s is inconsistent with commandline options\n",
              output);
      exit(1);
    }
    getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &pageTimeStamp);
    if (!CheckLogOnChangeLogFile(output, &startTime, &startDay, &startHour,
                                 &startJulianDay, &startMonth, &startYear,
                                 &timeStamp)) {
      if (mode & RECOVER) {
        if (!SDDS_RecoverFile(output, RECOVERFILE_VERBOSE))
          SDDS_Bomb("unable to recover existing file");
        if (!CheckLogOnChangeLogFile(output, &startTime, &startDay, &startHour,
                                     &startJulianDay, &startMonth, &startYear, &timeStamp))
          SDDS_Bomb("recovery of corrupted input file failed");
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to get data from existing file---try -append=recover (may truncate file)");
      }
    }
    if (!SDDS_InitializeAppend(logSet, output) || !SDDS_StartPage(logSet, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (0);
    }
    if (!GetLogFileIndices(logSet, mode)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      fprintf(stderr, "error: bad column indices in file append\n");
      return (0);
    }
  }

  if (!(mode & NOINDICES) &&
      (!SDDS_SetArrayVararg(logSet, "ControlName", SDDS_POINTER_ARRAY, cName, cNames) ||
       (mode & READBACKNAME &&
        !SDDS_SetArrayVararg(logSet, "ReadbackName", SDDS_POINTER_ARRAY, rName, cNames)) ||
       (mode & READBACKUNITS &&
        !SDDS_SetArrayVararg(logSet, "ReadbackUnits", SDDS_POINTER_ARRAY, rUnits, cNames)) ||
       (mode & LOGALARMS &&
        !SDDS_SetArrayVararg(logSet, "AlarmSeverityString", SDDS_POINTER_ARRAY, alarmSeverityString, ALARM_NSEV)) ||
       (mode & LOGALARMS &&
        !SDDS_SetArrayVararg(logSet, "AlarmStatusString", SDDS_POINTER_ARRAY, alarmStatusString, ALARM_NSTATUS)))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!(mode & NOINDICES) && mode & DESCRIPTIONS &&
      !SDDS_SetArrayVararg(logSet, "DescriptionString", SDDS_POINTER_ARRAY, description, cNames)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (!(mode & NOINDICES) && mode & RELATEDVALUES &&
      !SDDS_SetArrayVararg(logSet, "RelatedControlName", SDDS_POINTER_ARRAY,
                           relatedName, cNames)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }

  if (!SDDS_SetParameters(logSet, SDDS_PASS_BY_VALUE | SDDS_SET_BY_NAME,
                          "TimeStamp", timeStamp, "PageTimeStamp", pageTimeStamp,
                          "StartTime", startTime, "YearStartTime", computeYearStartTime(startTime),
                          "StartYear", (short)startYear,
                          "StartJulianDay", (short)startJulianDay,
                          "StartDayOfMonth", (short)startDay,
                          "StartMonth", (short)startMonth,
                          "StartHour", (float)startHour, NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  return (1);
}

long GetControlNames(char *file, char ***name0,
                     char ***readbackName0, char ***readbackUnits0,
                     char ***relatedName0, char ***description0,
                     double **tolerances0, short **expectNumeric0,
                     char ***expectFieldType0, int32_t **expectElements0) {
  char **name, **newName, **readback, **newReadback = NULL, **readbackUnits = NULL, **newReadbackUnits = NULL, **newDescription, **description, **relatedName, **newRelatedName;
  double *tolerances, *newTolerances = NULL;
  short *expectNumeric = NULL, *newExpectNumeric = NULL;
  char **expectFieldType = NULL, **newExpectFieldType = NULL;
  int32_t *expectElements = NULL;
  double *newExpectElementsD = NULL;
  long newNames, i, code, names, doUnits, doDescriptions, doRelatedNames, doReadbackNames, doTolerances, doExpectNumeric, doExpectFieldType, doExpectElements;
  char *unitsColumnName = NULL;
  SDDS_DATASET inSet;

  newDescription = newRelatedName = NULL;

  if (!SDDS_InitializeInput(&inSet, file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (-1);
  }
#if DEBUGSDDS
  fprintf(stderr, "Reading file %s\n", file);
#endif
  if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
    return (-1);
#if DEBUGSDDS
  fprintf(stderr, "ControlName present\n");
#endif

  doReadbackNames = 0;
  if (SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    doReadbackNames = 1;
#if DEBUGSDDS
  fprintf(stderr, "ReadbackName %s\n", doReadbackNames ? "present" : "absent");
#endif

  doUnits = 0;
  if (SDDS_CheckColumn(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    doUnits = 1;
    unitsColumnName = "ReadbackUnits";
  } else if (SDDS_CheckColumn(&inSet, "Units", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    doUnits = 1;
    unitsColumnName = "Units";
  }
#if DEBUGSDDS
  fprintf(stderr, "Units %s\n", doUnits ? "present" : "absent");
#endif

  doDescriptions = 0;
  if (SDDS_CheckColumn(&inSet, "Description", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    doDescriptions = 1;
#if DEBUGSDDS
  fprintf(stderr, "Description %s\n", doDescriptions ? "present" : "absent");
#endif

  doRelatedNames = 0;
  if (SDDS_CheckColumn(&inSet, "RelatedControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    doRelatedNames = 1;

  doTolerances = 0;
  if (SDDS_CheckColumn(&inSet, "Tolerance", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK)
    doTolerances = 1;

  doExpectNumeric = 0;
  if (SDDS_CheckColumn(&inSet, "ExpectNumeric", NULL, SDDS_CHARACTER, NULL) == SDDS_CHECK_OK)
    doExpectNumeric = 1;

  doExpectFieldType = 0;
  if (SDDS_CheckColumn(&inSet, "ExpectFieldType", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    doExpectFieldType = 1;

  doExpectElements = 0;
  if (SDDS_CheckColumn(&inSet, "ExpectElements", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK)
    doExpectElements = 1;

  names = 0;
  description = name = relatedName = readback = readbackUnits = NULL;
  tolerances = NULL;
  expectNumeric = NULL;
  expectFieldType = NULL;
  expectElements = NULL;

  while ((code = SDDS_ReadPage(&inSet)) > 0) {
#if DEBUGSDDS
    fprintf(stderr, "Page %ld read\n", code);
#endif
    if (!(newNames = SDDS_CountRowsOfInterest(&inSet)))
      continue;
    if ((newNames < 0 ||
         !(newName = SDDS_GetColumn(&inSet, "ControlName"))) ||
        (doReadbackNames && !(newReadback = SDDS_GetColumn(&inSet, "ReadbackName"))) ||
        (doUnits && !(newReadbackUnits = SDDS_GetColumn(&inSet, unitsColumnName))) ||
        (doDescriptions && !(newDescription = SDDS_GetColumn(&inSet, "Description"))) ||
        (doRelatedNames && !(newRelatedName = SDDS_GetColumn(&inSet, "RelatedControlName"))) ||
        (doTolerances && !(newTolerances = SDDS_GetColumnInDoubles(&inSet, "Tolerance"))) ||
        (doExpectNumeric && !(newExpectNumeric = SDDS_GetColumn(&inSet, "ExpectNumeric"))) ||
        (doExpectFieldType && !(newExpectFieldType = SDDS_GetColumn(&inSet, "ExpectFieldType"))) ||
        (doExpectElements && !(newExpectElementsD = SDDS_GetColumnInDoubles(&inSet, "ExpectElements")))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (-1);
    }
    if (!(name = SDDS_Realloc(name, (names + newNames) * sizeof(*name)))) {
      fprintf(stderr, "Unable to get ControlName data--allocation failure\n");
      return (-1);
    }
    if (doReadbackNames && !(readback = SDDS_Realloc(readback, (names + newNames) * sizeof(*readback)))) {
      fprintf(stderr, "Unable to get ReadbackName data--allocation failure\n");
      return (-1);
    }
    if (doUnits && !(readbackUnits = SDDS_Realloc(readbackUnits, (names + newNames) * sizeof(*readbackUnits)))) {
      fprintf(stderr, "Unable to get Description data--allocation failure\n");
      return (-1);
    }
    if (doDescriptions && !(description = SDDS_Realloc(description, (names + newNames) * sizeof(*description)))) {
      fprintf(stderr, "Unable to get Description data--allocation failure\n");
      return (-1);
    }
    if (doRelatedNames && !(relatedName = SDDS_Realloc(relatedName, (names + newNames) * sizeof(*relatedName)))) {
      fprintf(stderr, "Unable to get RelatedControlName data--allocation failure\n");
      return (-1);
    }
    if (doTolerances && !(tolerances = SDDS_Realloc(tolerances, (names + newNames) * sizeof(*tolerances)))) {
      fprintf(stderr, "Unable to get Tolerance data--allocation failure\n");
      return (-1);
    }
    if (doExpectNumeric && !(expectNumeric = SDDS_Realloc(expectNumeric, (names + newNames) * sizeof(*expectNumeric)))) {
      fprintf(stderr, "Unable to get ExpectNumeric data--allocation failure\n");
      return (-1);
    }
    if (doExpectFieldType && !(expectFieldType = SDDS_Realloc(expectFieldType, (names + newNames) * sizeof(*expectFieldType)))) {
      fprintf(stderr, "Unable to get ExpectFieldType data--allocation failure\n");
      return (-1);
    }
    if (doExpectElements && !(expectElements = SDDS_Realloc(expectElements, (names + newNames) * sizeof(*expectElements)))) {
      fprintf(stderr, "Unable to get ExpectElements data--allocation failure\n");
      return (-1);
    }

    for (i = 0; i < newNames; i++) {
      name[names + i] = newName[i];
      if (doReadbackNames)
        readback[names + i] = newReadback[i];
      if (doUnits)
        readbackUnits[names + i] = newReadbackUnits[i];
      if (doDescriptions)
        description[names + i] = newDescription[i];
      if (doRelatedNames)
        relatedName[names + i] = newRelatedName[i];
      if (doTolerances)
        tolerances[names + i] = newTolerances[i];
      if (doExpectNumeric)
        expectNumeric[names + i] = newExpectNumeric[i];
      if (doExpectFieldType)
        expectFieldType[names + i] = newExpectFieldType[i];
      if (doExpectElements)
        expectElements[names + i] = (int32_t)newExpectElementsD[i];
    }
    names += newNames;
    free(newName);
    if (doReadbackNames)
      free(newReadback);
    if (doUnits)
      free(newReadbackUnits);
    if (doDescriptions)
      free(newDescription);
    if (doRelatedNames)
      free(newRelatedName);
    if (doTolerances)
      free(newTolerances);
    if (doExpectNumeric)
      free(newExpectNumeric);
    if (doExpectFieldType)
      free(newExpectFieldType);
    if (doExpectElements)
      free(newExpectElementsD);
#if DEBUGSDDS
    fprintf(stderr, "Page %ld processed\n", code);
#endif
  }
#if DEBUGSDDS
  fprintf(stderr, "File processed\n");
#endif
  if (code == 0 || !SDDS_Terminate(&inSet)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (-1);
  }
#if DEBUGSDDS
  fprintf(stderr, "File terminated\n");
#endif
  *name0 = name;
  if (readbackName0 && doReadbackNames)
    *readbackName0 = readback;
  if (readbackUnits0 && doUnits)
    *readbackUnits0 = readbackUnits;
  if (description0 && doDescriptions)
    *description0 = description;
  if (relatedName0 && doRelatedNames)
    *relatedName0 = relatedName;
  if (tolerances0 && doTolerances)
    *tolerances0 = tolerances;
  if (expectNumeric0 && doExpectNumeric)
    *expectNumeric0 = expectNumeric;
  if (expectFieldType0 && doExpectFieldType)
    *expectFieldType0 = expectFieldType;
  if (expectElements0 && doExpectElements)
    *expectElements0 = expectElements;
  return (names);
}

long CheckForRequiredElements(char *filename, unsigned long mode) {
  SDDS_DATASET inSet;
  int32_t dimensions;
  if (!SDDS_InitializeInput(&inSet, filename)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (0);
  }
  if (mode & EXPLICIT) {
    if (mode & LOGALARMS) {
      if (SDDS_CheckColumn(&inSet, "AlarmSeverity", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
          SDDS_CheckColumn(&inSet, "AlarmStatus", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        return (0);
    } else {
      if (SDDS_CheckColumn(&inSet, "AlarmSeverity", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OK ||
          SDDS_CheckColumn(&inSet, "AlarmStatus", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OK)
        return (0);
    }
    if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
      return (0);
  } else {
    if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverity", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatus", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
      return (0);
  }
  if (!(mode & NOINDICES)) {
    if (SDDS_CheckColumn(&inSet, "ControlNameIndex", NULL, SDDS_LONG, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "ControlName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
      return (0);
    if (mode & READBACKNAME) {
      if (SDDS_CheckArray(&inSet, "ReadbackName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        return (0);
    }
    if (mode & READBACKUNITS) {
      if (SDDS_CheckArray(&inSet, "ReadbackUnits", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        return (0);
      if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME, "ReadbackUnits") != SDDS_LONG ||
          dimensions != 1) {
        fprintf(stderr, "error (sddsalarmlog): dimensions wrong for ReadbackUnits in %s\n", filename);
        return (0);
      }
    } else {
      if (SDDS_CheckArray(&inSet, "ReadbackUnits", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OK)
        return (0);
    }
    if (mode & LOGALARMS) {
      if (SDDS_CheckColumn(&inSet, "AlarmSeverityIndex", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
          SDDS_CheckColumn(&inSet, "AlarmStatusIndex", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
          SDDS_CheckArray(&inSet, "AlarmSeverityString", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
          SDDS_CheckArray(&inSet, "AlarmStatusString", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
        return (0);
      if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME,
                                   "AlarmSeverityString") != SDDS_LONG ||
          dimensions != 1) {
        fprintf(stderr, "error (sddsalarmlog): dimensions wrong for AlarmSeverityString in %s\n",
                filename);
        return (0);
      }
      if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME,
                                   "AlarmStatusString") != SDDS_LONG ||
          dimensions != 1) {
        fprintf(stderr, "error (sddsalarmlog): dimensions wrong for AlarmStatusString in %s\n",
                filename);
        return (0);
      }
    } else {
      if (SDDS_CheckColumn(&inSet, "AlarmSeverityIndex", NULL, SDDS_SHORT, stderr) == SDDS_CHECK_OK ||
          SDDS_CheckColumn(&inSet, "AlarmStatusIndex", NULL, SDDS_SHORT, stderr) == SDDS_CHECK_OK ||
          SDDS_CheckArray(&inSet, "AlarmSeverityString", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OK ||
          SDDS_CheckArray(&inSet, "AlarmStatusString", NULL, SDDS_STRING, stderr) == SDDS_CHECK_OK)
        return (0);
    }
    if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME, "ControlName") != SDDS_LONG ||
        dimensions != 1) {
      fprintf(stderr, "error (sddsalarmlog): dimensions wrong for ControlName in %s\n", filename);
      return (0);
    }
    if (mode & READBACKNAME) {
      if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME, "ReadbackName") != SDDS_LONG ||
          dimensions != 1) {
        fprintf(stderr, "error (sddsalarmlog): dimensions wrong for ReadbackName in %s\n", filename);
        return (0);
      }
    }
  } else {
    if (SDDS_CheckColumn(&inSet, "ControlNameIndex", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverityIndex", NULL, SDDS_SHORT, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatusIndex", NULL, SDDS_SHORT, NULL) == SDDS_CHECK_OK)
      return (0);
    if (SDDS_CheckArray(&inSet, "ControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmSeverityString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmStatusString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
      return (0);
  }
  if (mode & DURATIONS) {
    if (SDDS_CheckColumn(&inSet, "Duration", NULL, SDDS_FLOAT, stderr) != SDDS_CHECK_OK)
      return (0);
  } else {
    if (SDDS_CheckColumn(&inSet, "Duration", NULL, SDDS_FLOAT, NULL) == SDDS_CHECK_OK)
      return (0);
  }
  if (mode & TIMEOFDAY) {
    if (SDDS_CheckColumn(&inSet, "TimeOfDay", "h", SDDS_FLOAT, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "TimeOfDayWS", "h", SDDS_FLOAT, stderr) != SDDS_CHECK_OK)
      return (0);
  }
  if (SDDS_CheckColumn(&inSet, "PreviousRow", NULL, SDDS_LONG, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckColumn(&inSet, "Time", "s", SDDS_DOUBLE, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckColumn(&inSet, "Value", NULL, SDDS_DOUBLE, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "InputFile", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "TimeStamp", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartTime", "s", SDDS_DOUBLE, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartYear", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartMonth", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartJulianDay", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartDayOfMonth", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK)
    return (0);
  return (1);
}

long CheckLogOnChangeLogFile(char *filename, double *StartTime, double *StartDay, double *StartHour,
                             double *StartJulianDay, double *StartMonth, double *StartYear,
                             char **TimeStamp) {
  SDDS_DATASET inSet;
  long code;
  if (!SDDS_InitializeInput(&inSet, filename))
    return (0);
  while ((code = SDDS_ReadPage(&inSet)) > 0) {
    if (code == 1 &&
        (!SDDS_GetParameterAsDouble(&inSet, "StartTime", StartTime) ||
         !SDDS_GetParameterAsDouble(&inSet, "StartYear", StartYear) ||
         !SDDS_GetParameterAsDouble(&inSet, "StartJulianDay", StartJulianDay) ||
         !SDDS_GetParameterAsDouble(&inSet, "StartDayOfMonth", StartDay) ||
         !SDDS_GetParameterAsDouble(&inSet, "StartHour", StartHour) ||
         !SDDS_GetParameterAsDouble(&inSet, "StartMonth", StartMonth) ||
         !SDDS_GetParameter(&inSet, "TimeStamp", TimeStamp))) {
      fprintf(stderr,
              "error (sddsalarmlog): unable to append---parameter data couldn't be obtained\n");
      return (0);
    }
  }
  SDDS_Terminate(&inSet);
  if (code == -1)
    return (1);
  return (0);
}

long GetLogFileIndices(SDDS_DATASET *outSet, unsigned long mode) {
  if ((iPreviousRow = SDDS_GetColumnIndex(outSet, "PreviousRow")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): PreviousRow index unobtainable.\n");
    return (0);
  }
  if ((outputMode & TIMEOFDAY &&
       (iHour = SDDS_GetColumnIndex(outSet, "TimeOfDay")) < 0)) {
    fprintf(stderr, "error (sddsalarmlog): TimeOfDay index unobtainable.\n");
    return (0);
  }
  if ((iValue = SDDS_GetColumnIndex(outSet, "Value")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): Value index unobtainable.\n");
    return (0);
  }
  /* StringValue is optional for backward compatibility with older files */
  iStringValue = SDDS_GetColumnIndex(outSet, "StringValue");
  /* WaveformIndex and WaveformLength are optional for backward compatibility */
  iWaveformIndex = SDDS_GetColumnIndex(outSet, "WaveformIndex");
  iWaveformLength = SDDS_GetColumnIndex(outSet, "WaveformLength");
  if ((iTime = SDDS_GetColumnIndex(outSet, "Time")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): Time index unobtainable.\n");
    return (0);
  }
  if ((iTimeWS = SDDS_GetColumnIndex(outSet, "TimeWS")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): TimeWS index unobtainable.\n");
    return (0);
  }
  if (!(mode & NOINDICES) &&
      ((iControlNameIndex = SDDS_GetColumnIndex(outSet, "ControlNameIndex")) < 0 ||
       (iAlarmStatusIndex = SDDS_GetColumnIndex(outSet, "AlarmStatusIndex")) < 0 ||
       (iAlarmSeverityIndex = SDDS_GetColumnIndex(outSet, "AlarmSeverityIndex")) < 0)) {
    fprintf(stderr, "error (sddsalarmlog): ControlNameIndex, AlarmStatusIndex, or AlarmSeverityIndex index unobtainable.\n");
    return (0);
  }
  if (mode & DURATIONS &&
      (iDuration = SDDS_GetColumnIndex(outSet, "Duration")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): Duration index unobtainable.\n");
    return (0);
  }
  if (mode & EXPLICIT &&
      ((iControlName = SDDS_GetColumnIndex(outSet, "ControlName")) < 0 ||
       (iAlarmStatus = SDDS_GetColumnIndex(outSet, "AlarmStatus")) < 0 ||
       (iAlarmSeverity = SDDS_GetColumnIndex(outSet, "AlarmSeverity")) < 0)) {
    fprintf(stderr, "error (sddsalarmlog): ControlName, AlarmStatus, or AlarmSeverity index unobtainable.\n");
    return (0);
  }
  return (1);
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
      /* fprintf( stderr, "Communications error with runcontrol record.\n");*/
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
