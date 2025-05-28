/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddsalarmlog
 * author: M. Borland, 1995
 * purpose: channel access alarm logging.
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
#define CLO_DAILYFILES 14
#define CLO_RUNCONTROLPV 15
#define CLO_RUNCONTROLDESC 16
#define COMMANDLINE_OPTIONS 17

static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "verbose", "timeduration", "pendeventtime", "explicit",
  "durations", "connecttimeout", "erasefile", "append",
  "generations", "comment", "requirechange", "offsettimeofday",
  "inhibitpv", "watchInput", "dailyfiles", "runControlPV", "runControlDescription"};

static char *USAGE1 = "sddsalarmlog <input> <output> \n\
-timeDuration=<realValue>[,<time-units>] [-offsetTimeOfDay]\n\
[-append[=recover] | -eraseFile | -generations[=digits=<integer>][,delimiter=<string>] |\n\
-dailyFiles[=verbose]]\n\
[-pendEventTime=<seconds>] [-durations] [-connectTimeout=<seconds>]\n\
[-explicit[=only]] [-verbose] [-comment=<parameterName>,<text>]\n\
[-requireChange[=severity][,status][,both]]\n\
[-inhibitPV=name=<name>,pendIOTime=<seconds>]\n\
[-watchInput]\n\
[-runControlPV=string=<string>,pingTimeout=<value>\n\
[-runControlDescription=string=<string>]\n\n\
Logs alarm data for the process variables named in the ControlName\n\
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
                for formating <N>, so that the first generation number is 0001.\n\
-dailyFiles     The output is sent to the file <SDDSoutputfile>-YYYY-JJJ-MMDD.<N>,\n\
                where YYYY=year, JJJ=Julian day, and MMDD=month+day. A new file is\n\
                started after the midnight. \n\
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
-runControlPV   specifies a runControl PV name.\n\
-runControlDescription\n\
                specifies a runControl PV description record.\n\n\
Program by M. Borland, ANL/APS.\n\
Link date: "__DATE__" "__TIME__", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

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
  int32_t bitLength;
  char **bitDescriptions;
} BIT_DECODER_ARRAY;

BIT_DECODER_ARRAY globalBDA[100];

typedef struct
{
  short logPending;
  char *controlName, *description;
  char *relatedControlName;
  chid channelID, relatedChannelID;
  long lastSeverity, lastRow, lastStatus, relatedIsSame;
  double lastChangeTime;
  /* values to be passed to LogEventToFile from RelatedValueEventHandler */
  long index;
  double theTime, Hour, HourIOC, duration;
  short statusIndex, severityIndex;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  /* bit decoder array index */
  int32_t bdaIndex;
} CHANNEL_INFO;

long GetControlNames(char *file, char ***name0, char ***relatedName0, char ***description0, int32_t **bitDecoderIndex0);
long SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **controlName, char **description, char **relatedName,
                  long controlNames, unsigned long mode,
                  char **commentParameter, char **commentText, long comments);
long InitiateConnections(char **cName, char **relatedName, char **description, int32_t *bitDecoderIndex0,
                         long cNames, CHANNEL_INFO **chInfo);
void EventHandler(struct event_handler_args event);
void RelatedValueEventHandler(struct event_handler_args event);
void LogUnconnectedEvents(void);
void LogEventToFile(long index, double theTime, double Hour, double HourIOC, double duration,
                    short statusIndex, short severityIndex, char *relatedValueString);
long CheckForRequiredElements(char *filename, unsigned long mode);
long CheckAlarmLogFile(char *filename, double *StartTime, double *StartDay, double *StartHour,
                       double *StartJulianDay, double *StartMonth,
                       double *StartYear, char **TimeStamp);
long GetLogFileIndices(SDDS_DATASET *outSet, unsigned long mode);

static long controlNames;
static CHANNEL_INFO *chInfo;
static SDDS_DATASET logDataset;
static long iControlNameIndex = -1, iAlarmStatusIndex = -1, iAlarmSeverityIndex = -1,
  iHour = -1, iControlName = -1, iAlarmStatus = -1, iAlarmSeverity = -1, iDuration = -1, iPreviousRow = -1,
  iDescription = -1, iRelatedName = -1, iRelatedValueString = -1,
  iHourIOC = -1;
static long logfileRow = 0, logfileRows = 0, logfileRowsWritten = 0;
static double startTime, startHour, hourOffset = 0, timeZoneOffsetInHours = 0;
static short alarmsOccurred = 0;
static double HourAdjust = 0, HourIOCAdjust = 0;

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
  char *inputFile, *outputFile, **controlName, **description, **relatedName, *rootName;
  int32_t *bitDecoderIndex;
  char **commentParameter, **commentText, *generationsDelimiter;
  long comments, inhibit = 0;
  double timeDuration, endTime, timeLeft, theTime, connectTimeout, nonConnectsHandled, zeroTime;
  short verbose, eraseFile;
  long i_arg, timeUnits;
  long generations, dailyFiles, dailyFilesVerbose, updatePages = 0;
  int32_t generationsDigits;
  unsigned long dummyFlags;
  long offsetTimeOfDay;
  /*extern time_t timezone;*/
  chid inhibitID;
  double InhibitPendIOTime = 10;
  PV_VALUE InhibitPV;
  double LastHour, HourNow, Hour;
  long i;

  struct stat filestat;
  struct stat input_filestat;
  int watchInput = 0;

  SDDS_RegisterProgramName(argv[0]);

  timeZoneOffsetInHours = epicsSecondsWestOfUTC() / 3600.0;

  pendEventTime = DEFAULT_PEND_EVENT;
  verbose = 0;
  timeDuration = 0;
  inputFile = outputFile = rootName = NULL;
  connectTimeout = 60;
  eraseFile = 0;
  generations = dailyFiles = dailyFilesVerbose = 0;
  commentParameter = commentText = NULL;
  comments = 0;
  offsetTimeOfDay = 0;
  HourNow = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
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
        break;
      case CLO_VERBOSE:
        verbose = 1;
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
        break;
      case CLO_DAILYFILES:
        generationsDigits = 4;
        generationsDelimiter = ".";
        dailyFiles = 1;
        dailyFilesVerbose = 0;
        if (s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -dailyFiles syntax");
        if (s_arg[i_arg].n_items == 2) {
          str_tolower(s_arg[i_arg].list[1]);
          if (strncmp(s_arg[i_arg].list[1], "verbose", strlen(s_arg[i_arg].list[1])) != 0)
            SDDS_Bomb("invalid -dailyFiles syntax");
          dailyFilesVerbose = 1;
        }
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
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_INHIBIT:
        InhibitPendIOTime = 10;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &InhibitPV.name, 1, 0,
                          "pendiotime", SDDS_DOUBLE, &InhibitPendIOTime, 1, 0,
                          NULL) ||
            !InhibitPV.name || !strlen(InhibitPV.name) ||
            InhibitPendIOTime <= 0)
          SDDS_Bomb("invalid -InhibitPV syntax/values");
        inhibit = 1;
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
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else if (!outputFile) {
        outputFile = s_arg[i_arg].list[0];
        rootName = s_arg[i_arg].list[0];
      } else
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
  if (!outputFile)
    SDDS_Bomb("output filename not given");

  if (watchInput) {
    if (stat(inputFile, &input_filestat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputFile);
      return (1);
    }
  }

  if (eraseFile && (outputMode & APPEND))
    SDDS_Bomb("incompatible options -eraseFile and -append given");
  if (generations && dailyFiles)
    SDDS_Bomb("-generations and -dailyFiles are incompatible");
  if ((generations || dailyFiles) && (outputMode & APPEND))
    SDDS_Bomb("incompatible options -generations/dailyFiles and -append given");
  if (eraseFile && (generations || dailyFiles))
    SDDS_Bomb("-eraseFile and -generations/dailyFiles are incompatible options");
  if (generations)
    outputFile = MakeGenerationFilename(rootName, generationsDigits, generationsDelimiter, NULL);
  if (dailyFiles) {
    outputFile = MakeDailyGenerationFilename(rootName, generationsDigits, generationsDelimiter, 0);
    if (dailyFilesVerbose)
      fprintf(stdout, "New generation file started: %s\n",
              outputFile);
  }

  HourNow = getHourOfDay();
  if (fexists(outputFile) && !(eraseFile || outputMode & APPEND))
    SDDS_Bomb("output file already exists");
  if (timeDuration <= 0)
    SDDS_Bomb("time duration not specified");
  if (comments && (outputMode & APPEND))
    fprintf(stderr, "warning: comments ignored for -append mode\n");
#if DEBUG
  fprintf(stderr, "Argument checking done; output file will be %s.\n", outputFile);
#endif

  description = NULL;
  relatedName = NULL;
  bitDecoderIndex = NULL;
  if ((controlNames = GetControlNames(inputFile, &controlName, &relatedName, &description, &bitDecoderIndex)) <= 0)
    SDDS_Bomb("unable to get ControlName data from input file");
  if (description)
    outputMode |= DESCRIPTIONS;
  if (relatedName)
    outputMode |= RELATEDVALUES;

#if DEBUG
  fprintf(stderr, "Setting up log file\n");
#endif
  if (!SetupLogFile(&logDataset, outputFile, inputFile, controlName, description,
                    relatedName, controlNames, outputMode, commentParameter, commentText,
                    comments))
    SDDS_Bomb("unable to set up log output file");

  if (offsetTimeOfDay && (startHour * 3600.0 + timeDuration - 24.0 * 3600.0) > 0.5 * timeDuration)
    hourOffset = 24;

#ifdef DEBUGSDDS
  fprintf(stderr,
          "rows = %ld, page = %ld, written=%ld, last_written=%ld, first=%ld \n",
          logDataset.n_rows, logDataset.page_number, logDataset.n_rows_written,
          logDataset.last_row_written, logDataset.first_row_in_mem);
#endif

  alarmsOccurred = 0;
  HourAdjust = startHour - startTime / 3600.0 - hourOffset;
  HourIOCAdjust = HourAdjust + EPOCH_OFFSET / 3600.0 - timeZoneOffsetInHours;

  if (!InitiateConnections(controlName, relatedName, description, bitDecoderIndex, controlNames, &chInfo))
    SDDS_Bomb("unable to establish callbacks for PVs");

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
  if (sigint)
    return (1);

  /* zeroTime is when the clock for this run starts, as opposed to startTime, which is when
   * this run or the run for which this is a continuation opened the output file 
   */
  endTime = (zeroTime = getTimeInSecs()) + timeDuration;
  nonConnectsHandled = 0;
  while ((timeLeft = endTime - (theTime = getTimeInSecs())) > 0) {
    LastHour = HourNow;
    HourNow = getHourOfDay();
    if ((dailyFiles && HourNow < LastHour)) {
      /* must start a new file */
      if (!SDDS_Terminate(&logDataset)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      outputFile = MakeDailyGenerationFilename(rootName, generationsDigits, generationsDelimiter, 0);
      if (dailyFilesVerbose)
        fprintf(stdout, "New generation file started: %s\n",
                outputFile);
      logfileRow = logfileRows = logfileRowsWritten = 0;
      if (!SetupLogFile(&logDataset, outputFile, inputFile, controlName, description,
                        relatedName, controlNames, outputMode, commentParameter, commentText,
                        comments))
        SDDS_Bomb("unable to set up log output file");

      HourAdjust = startHour - startTime / 3600.0;
      HourIOCAdjust = HourAdjust + EPOCH_OFFSET / 3600.0 - timeZoneOffsetInHours;

      theTime = getTimeInSecs();
      Hour = (theTime) / 3600.0 + HourAdjust;
      for (i = 0; i < controlNames; i++) {
        chInfo[i].lastRow = -2;
        LogEventToFile(i, theTime, Hour, Hour, theTime - chInfo[i].lastChangeTime,
                       chInfo[i].lastStatus, chInfo[i].lastSeverity, NULL);
      }
    }

    if (inhibit && QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Inhibit PV %s is non zero. Exiting\n", InhibitPV.name);
      return (0);
    }
    if (verbose)
      fprintf(stderr, "%ld total alarms with %.2f seconds left to run\n", logfileRow, timeLeft);
    if (!nonConnectsHandled && (long)(theTime - zeroTime) > connectTimeout) {
      LogUnconnectedEvents();
      nonConnectsHandled = 1;
    }
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
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

    if (alarmsOccurred)
      updatePages++;
    if (alarmsOccurred && !SDDS_UpdatePage(&logDataset, FLUSH_TABLE)) {
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    alarmsOccurred = 0;
    logfileRowsWritten = logfileRow;
    if (watchInput) {
      if (stat(inputFile, &filestat) != 0)
        break;
      if (input_filestat.st_mtime != filestat.st_mtime)
        break;
    }
#ifdef DEBUGSDDS
    fprintf(stderr, "main: logfileRow = %ld, logfileRows = %ld, logfileRowsWritten = %ld\n", logfileRow,
            logfileRows, logfileRowsWritten);
    fprintf(stderr,
            "main: rows = %ld, page = %ld, written=%ld, last_written=%ld, first=%ld \n",
            logDataset.n_rows, logDataset.page_number, logDataset.n_rows_written,
            logDataset.last_row_written, logDataset.first_row_in_mem);
#endif
  }
  if (verbose)
    fprintf(stderr, "%ld total alarms\n", logfileRow);

  if (!updatePages && !SDDS_UpdatePage(&logDataset, FLUSH_TABLE)) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (!SDDS_Terminate(&logDataset))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return 0;
}

void LogUnconnectedEvents(void) {
  long i;
  double theTime, Hour;
  Hour = startHour + ((theTime = getTimeInSecs()) - startTime) / 3600.0 - hourOffset;
  for (i = 0; i < controlNames; i++) {
    if (ca_state(chInfo[i].channelID) != cs_conn) {
      fprintf(stderr, "Warning: the following channel did not connect: %s\n",
              chInfo[i].controlName);
      chInfo[i].lastRow = -1;      /* indicates nothing logged prior to this */
      chInfo[i].lastSeverity = -1; /* will result in logging of a NO_ALARM event if one occurs next due to 
                                    * connection of the channel.  See EventHandler().
                                    */
      chInfo[i].lastChangeTime = theTime;
      /* log this "event" as a TIMEOUT with INVALID status */
      LogEventToFile(i, chInfo[i].lastChangeTime, Hour, Hour, theTime - startTime,
                     TIMEOUT_ALARM, INVALID_ALARM, "");
    }
    if (chInfo[i].relatedControlName &&
        !chInfo[i].relatedIsSame && ca_state(chInfo[i].relatedChannelID) != cs_conn) {
      fprintf(stderr, "Warning: the following channel did not connect: %s\n",
              chInfo[i].controlName);
      LogEventToFile(i, chInfo[i].lastChangeTime, Hour, Hour, theTime - startTime,
                     TIMEOUT_ALARM, INVALID_ALARM, chInfo[i].relatedControlName);
    }
  }
  ca_pend_event(0.0001);
}

int decimalToBinary(const char* decimal, int binary[], int arraySize) {
    // First, convert the decimal string to an integer value
    long num = strtol(decimal, NULL, 10);

    // Initialize the binary array to zeroes
    for (int i = 0; i < arraySize; i++) {
        binary[i] = 0;
    }

    // Validate conversion
    if (num < 0) {
        printf("Conversion error or negative number detected.\n");
        return 1;
    }

    // Convert to binary and store in the array
    int index = arraySize - 1; // Start filling the array from the end
    while (num > 0 && index >= 0) {
        binary[index--] = num % 2;
        num /= 2;
    }

    // Handle potential error if the number is too large for the array
    if (num > 0) {
      printf("The number is too large for the provided array. Increase the array size.\n");
      return 1;
    }
    return 0;
}

void EventHandler(struct event_handler_args event) {
  struct dbr_time_string *dbrValue;
  double Hour, theTime, HourIOC;
  long index;
  short statusIndex, severityIndex, logEvent;
  TS_STAMP *tsStamp;
  int binary[32], logged;

  /* Hour = startHour + ((theTime=getTimeInSecs())-startTime)/3600.0 - hourOffset; */
  Hour = (theTime = getTimeInSecs()) / 3600.0 + HourAdjust;

  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_string *)event.dbr;
  statusIndex = (short)dbrValue->status;
  severityIndex = (short)dbrValue->severity;
  tsStamp = &(dbrValue->stamp);
  HourIOC = (tsStamp->secPastEpoch + 1e-9 * tsStamp->nsec) / 3600.0 + HourIOCAdjust;

#ifdef DEBUG
  fprintf(stderr, "EventHandler : index = %ld, status = %ld, severity = %ld\n",
          index, statusIndex, severityIndex);
  fprintf(stderr, "chInfo pointer: %x\n", (unsigned long)chInfo);
  fprintf(stderr, "pv name pointer = %x\n", chInfo[index].controlName);
  fprintf(stderr, "event: pv=%s  status=%s  severity=%s\n",
          chInfo[index].controlName, alarmStatusString[statusIndex],
          alarmSeverityString[severityIndex]);
#endif
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
    if (chInfo[index].relatedControlName && !chInfo[index].relatedIsSame) {
      /* save values pertinent to this alarm.
       */
      long status;
      chInfo[index].logPending = 1;
      chInfo[index].index = index;
      chInfo[index].theTime = theTime;
      chInfo[index].Hour = Hour;
      chInfo[index].HourIOC = HourIOC;
      chInfo[index].duration = theTime - chInfo[index].lastChangeTime;
      chInfo[index].statusIndex = statusIndex;
      chInfo[index].severityIndex = severityIndex;
      /* set up a callback to get the related value and write it out */
      if ((status = ca_get_callback(DBR_STRING, chInfo[index].relatedChannelID,
                                    RelatedValueEventHandler, (void *)&chInfo[index].usrValue)) != ECA_NORMAL) {
        fprintf(stderr, "warning: unable to establish callback for control name %s (related value for %s): %s\n",
                chInfo[index].relatedControlName, chInfo[index].controlName,
                ca_message(status));
        LogEventToFile(index, theTime, Hour, HourIOC,
                       theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                       (char *)ca_message(status));
      }
#ifdef DEBUG
      fprintf(stderr, "Callback set up for related value\n");
#endif
    } else {
      /* come here if there is no related control name or if the related control name
       * is the same as the alarm control name (in which case we already have the
       * related value)
       */
      logged = 0;
      if (chInfo[index].bdaIndex >= 0) {
        if (decimalToBinary((char *)(dbrValue->value), binary, 32) == 1) {
          LogEventToFile(index, theTime, Hour, HourIOC,
                         theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                         chInfo[index].relatedControlName ? (char *)(dbrValue->value) : NULL);
          fprintf(stderr, "BidDecoderArray is invalid for %s\n", chInfo[index].controlName);
        } else {
          for (int i = 31, j = 0; i >= 0; i--, j++) {
            if (binary[i]) {
              LogEventToFile(index, theTime, Hour, HourIOC,
                             theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                             globalBDA[chInfo[index].bdaIndex].bitDescriptions[j]);
              logged = 1;
            }
          }
        }
      }
      if (!logged) {
        LogEventToFile(index, theTime, Hour, HourIOC,
                       theTime - chInfo[index].lastChangeTime, statusIndex, severityIndex,
                       chInfo[index].relatedControlName ? (char *)(dbrValue->value) : NULL);
      }
    }
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
  LogEventToFile(index, chInfo[index].theTime, chInfo[index].Hour, chInfo[index].HourIOC,
                 chInfo[index].duration,
                 chInfo[index].statusIndex, chInfo[index].severityIndex,
                 (char *)(event.dbr));
  return;
}

void LogEventToFile(long index, double theTime, double Hour, double HourIOC, double duration,
                    short statusIndex, short severityIndex, char *relatedValueString) {
  alarmsOccurred = 1;
  while (logfileRows - 2 < logfileRow - logfileRowsWritten) {
#ifdef DEBUGSDDS
    fprintf(stderr, "lengthening table from %ld to %ld rows   present row %ld, %ld rows written so far\n",
            logfileRows, logfileRows + 100, logfileRow, logfileRowsWritten);
#endif
    logfileRows += 100;
    if (!SDDS_LengthenTable(&logDataset, 100)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  if (!SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX,
                         logfileRow, iHour, (float)Hour, iHourIOC, (float)HourIOC,
                         iPreviousRow, chInfo[index].lastRow, -1) ||
      (outputMode & DURATIONS &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iDuration, (float)(theTime - chInfo[index].lastChangeTime), -1)) ||
      (outputMode & RELATEDVALUES &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iRelatedValueString, relatedValueString, -1)) ||
      (!(outputMode & NOINDICES) &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iControlNameIndex, index,
                          iAlarmStatusIndex, statusIndex,
                          iAlarmSeverityIndex, severityIndex,
                          -1)) ||
      (outputMode & EXPLICIT &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iControlName, chInfo[index].controlName,
                          iAlarmStatus, alarmStatusString[statusIndex],
                          iAlarmSeverity, alarmSeverityString[severityIndex],
                          -1)) ||
      (outputMode & EXPLICIT && outputMode & DESCRIPTIONS &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iDescription, chInfo[index].description, -1)) ||
      (outputMode & EXPLICIT && outputMode & RELATEDVALUES &&
       !SDDS_SetRowValues(&logDataset, SDDS_PASS_BY_VALUE | SDDS_SET_BY_INDEX, logfileRow,
                          iRelatedName, chInfo[index].relatedControlName, -1))) {
#ifdef DEBUGSDDS
    fprintf(stderr, "logfileRow = %ld, logfileRows = %ld, logfileRowsWritten = %ld\n", logfileRow,
            logfileRows, logfileRowsWritten);
    fprintf(stderr, "rows = %ld, page = %ld, written=%ld, last_written=%ld, first=%ld \n",
            logDataset.n_rows, logDataset.page_number, logDataset.n_rows_written,
            logDataset.last_row_written, logDataset.first_row_in_mem);
#endif
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  chInfo[index].lastChangeTime = theTime;
  chInfo[index].lastRow = logfileRow;
  chInfo[index].lastSeverity = severityIndex;
  chInfo[index].lastStatus = statusIndex;
  logfileRow++;
  chInfo[index].logPending = 0;
}

long InitiateConnections(char **cName, char **relatedName, char **description, int32_t *bitDecoderIndex, long cNames, CHANNEL_INFO **chInfo0) {
  long i;
  CHANNEL_INFO *chInfo;

#ifdef DEBUG
  fprintf(stderr, "initiating connections for %ld control names\n", cNames);
#endif
  if (!(chInfo = (CHANNEL_INFO *)malloc(sizeof(*chInfo) * cNames)))
    return 0;
  *chInfo0 = chInfo;
  for (i = 0; i < cNames; i++) {
    chInfo[i].lastRow = -2;      /* indicates no data logged yet for this channel */
    chInfo[i].lastSeverity = -1; /* invalid severity code--indicates no callbacks yet for this channel */
    chInfo[i].controlName = cName[i];
    if (description)
      chInfo[i].description = description[i];
    else
      chInfo[i].description = NULL;
    if (relatedName && relatedName[i] && strlen(relatedName[i]))
      chInfo[i].relatedControlName = relatedName[i];
    else
      chInfo[i].relatedControlName = NULL;
    chInfo[i].usrValue = i;
    chInfo[i].lastChangeTime = getTimeInSecs(); /* will be used if channel connection times out */
    chInfo[i].logPending = 0;
    if (bitDecoderIndex) {
      chInfo[i].bdaIndex = bitDecoderIndex[i];
    } else {
      chInfo[i].bdaIndex = -1;
    }
  }
  for (i = 0; i < cNames; i++) {
    if (ca_search(cName[i], &chInfo[i].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", cName[i]);
      return 0;
    }
    if (chInfo[i].relatedControlName) {
      if (strcmp(chInfo[i].relatedControlName, cName[i]) == 0)
        chInfo[i].relatedIsSame = 1;
      else {
        chInfo[i].relatedIsSame = 0;
        if (ca_search(relatedName[i], &chInfo[i].relatedChannelID) != ECA_NORMAL) {
          fprintf(stderr, "error: search failed for related control name %s\n", relatedName[i]);
          return 0;
        }
      }
    }
  }

  /* Use ca_pend_io here because we need to clear out any errors from the 
     previous ca_search command so that the runcontrol will not confuse
     these connections errors with connection errors to the runcontrol PVs.*/
  ca_pend_io(0.0001);

  for (i = 0; i < cNames; i++) {
    if (ca_add_masked_array_event(DBR_TIME_STRING, 1, chInfo[i].channelID, EventHandler,
                                  (void *)&chInfo[i].usrValue, (ca_real)0, (ca_real)0, (ca_real)0,
                                  NULL, DBE_ALARM) != ECA_NORMAL) {
      fprintf(stderr, "error: unable to setup alarm callback for control name %s\n", chInfo[i].controlName);
      exit(1);
    }
  }
  ca_pend_event(0.0001);
  return 1;
}

long SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **cName, char **description, char **relatedName,
                  long cNames, unsigned long mode,
                  char **commentParameter, char **commentText, long comments) {
  char *timeStamp, *pageTimeStamp;
  double startDay, startJulianDay, startYear, startMonth;

  if (!(mode & APPEND) || !fexists(output)) {
    getTimeBreakdown(&startTime, &startDay, &startHour, &startJulianDay,
                     &startMonth, &startYear, &timeStamp);
    SDDS_CopyString(&pageTimeStamp, timeStamp);
    if (!SDDS_InitializeOutput(logSet, SDDS_BINARY, 0, NULL, NULL, output) ||
        0 > SDDS_DefineParameter(logSet, "InputFile", NULL, NULL, "Alarm log input file",
                                 NULL, SDDS_STRING, input) ||
        !DefineLoggingTimeParameters(logSet) ||
        0 > (iPreviousRow =
             SDDS_DefineColumn(logSet, "PreviousRow", NULL, NULL, NULL, "%6ld", SDDS_LONG, 0)) ||
        0 > (iHour =
             SDDS_DefineColumn(logSet, "TimeOfDayWS", NULL, "h", "Time of day in hours per workstation",
                               "%8.5f", SDDS_FLOAT, 0)) ||
        0 > (iHourIOC =
             SDDS_DefineColumn(logSet, "TimeOfDay", NULL, "h", "Time of day in hours per IOC",
                               "%8.5f", SDDS_FLOAT, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    SDDS_EnableFSync(logSet);
    if (!(mode & NOINDICES) &&
        (0 > SDDS_DefineArray(logSet, "ControlNameString", NULL, NULL, NULL, "%32s", SDDS_STRING, 0, 1, NULL) ||
         0 > SDDS_DefineArray(logSet, "AlarmStatusString", NULL, NULL, NULL, "%12s", SDDS_STRING, 0, 1, NULL) ||
         0 > SDDS_DefineArray(logSet, "AlarmSeverityString", NULL, NULL, NULL, "%10s", SDDS_STRING, 0, 1, NULL) ||
         (mode & DESCRIPTIONS &&
          0 > SDDS_DefineArray(logSet, "DescriptionString", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL)) ||
         (mode & RELATEDVALUES &&
          0 > SDDS_DefineArray(logSet, "RelatedControlNameString", NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL)) ||
         0 > (iControlNameIndex =
              SDDS_DefineColumn(logSet, "ControlNameIndex", NULL, NULL, NULL, "%6ld", SDDS_LONG, 0)) ||
         0 > (iAlarmStatusIndex =
              SDDS_DefineColumn(logSet, "AlarmStatusIndex", NULL, NULL, NULL, "%6hd", SDDS_SHORT, 0)) ||
         0 > (iAlarmSeverityIndex =
              SDDS_DefineColumn(logSet, "AlarmSeverityIndex", NULL, NULL, NULL, "%6hd", SDDS_SHORT, 0)))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (mode & DURATIONS &&
        0 > (iDuration =
             SDDS_DefineColumn(logSet, "Duration", NULL, "s", NULL, "%8.4f", SDDS_FLOAT, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (mode & RELATEDVALUES &&
        0 > (iRelatedValueString =
             SDDS_DefineColumn(logSet, "RelatedValueString", NULL, NULL, NULL, NULL, SDDS_STRING, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (mode & EXPLICIT &&
        (0 > (iControlName =
              SDDS_DefineColumn(logSet, "ControlName", NULL, NULL, NULL, "%32s",
                                SDDS_STRING, 0)) ||
         0 > (iAlarmStatus =
              SDDS_DefineColumn(logSet, "AlarmStatus", NULL, NULL, NULL, "%12s",
                                SDDS_STRING, 0)) ||
         0 > (iAlarmSeverity =
              SDDS_DefineColumn(logSet, "AlarmSeverity", NULL, NULL, NULL, "%10s",
                                SDDS_STRING, 0)) ||
         0 > (iDescription =
              SDDS_DefineColumn(logSet, "Description", NULL, NULL, NULL, NULL,
                                SDDS_STRING, 0)) ||
         0 > (iRelatedName =
              SDDS_DefineColumn(logSet, "RelatedControlName", NULL, NULL, NULL, NULL,
                                SDDS_STRING, 0)))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (comments && !SDDS_DefineSimpleParameters(logSet, comments, commentParameter, NULL, SDDS_STRING)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (!SDDS_WriteLayout(logSet) || !SDDS_StartPage(logSet, 0) ||
        (comments && !SetCommentParameters(logSet, commentParameter, commentText, comments))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
  } else {
    if (!CheckForRequiredElements(output, mode)) {
      fprintf(stderr, "error (sddsalarmlog): file %s is inconsistent with commandline options\n",
              output);
      exit(1);
    }
    getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &pageTimeStamp);
    if (!CheckAlarmLogFile(output, &startTime, &startDay, &startHour,
                           &startJulianDay, &startMonth, &startYear,
                           &timeStamp)) {
      if (mode & RECOVER) {
        if (!SDDS_RecoverFile(output, RECOVERFILE_VERBOSE))
          SDDS_Bomb("unable to recover existing file");
        if (!CheckAlarmLogFile(output, &startTime, &startDay, &startHour,
                               &startJulianDay, &startMonth, &startYear, &timeStamp))
          SDDS_Bomb("recovery of corrupted input file failed");
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to get data from existing file---try -append=recover (may truncate file)");
      }
    }
    if (!SDDS_InitializeAppend(logSet, output) || !SDDS_StartPage(logSet, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return 0;
    }
    if (!GetLogFileIndices(logSet, mode)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      fprintf(stderr, "error: bad column indices in file append\n");
      return 0;
    }
  }

  if (!(mode & NOINDICES) &&
      (!SDDS_SetArrayVararg(logSet, "ControlNameString", SDDS_POINTER_ARRAY, cName, cNames) ||
       !SDDS_SetArrayVararg(logSet, "AlarmSeverityString", SDDS_POINTER_ARRAY, alarmSeverityString, ALARM_NSEV) ||
       !SDDS_SetArrayVararg(logSet, "AlarmStatusString", SDDS_POINTER_ARRAY, alarmStatusString, ALARM_NSTATUS))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!(mode & NOINDICES) && mode & DESCRIPTIONS &&
      !SDDS_SetArrayVararg(logSet, "DescriptionString", SDDS_POINTER_ARRAY, description, cNames)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (!(mode & NOINDICES) && mode & RELATEDVALUES &&
      !SDDS_SetArrayVararg(logSet, "RelatedControlNameString", SDDS_POINTER_ARRAY,
                           relatedName, cNames)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
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
    return 0;
  }
  return 1;
}

long GetControlNames(char *file, char ***name0, char ***relatedName0, char ***description0, int32_t **bitDecoderIndex0) {
  char **name, **newName, **newDescription, **description, **relatedName, **newRelatedName;
  int32_t *bitDecoderIndex;
  char **newBitDecoderArray=NULL;
  long newNames, i, j=0, code, names, doDescriptions, doRelatedNames, doBitDecoderArray;
  int32_t k;
  SDDS_DATASET inSet;

  newDescription = newRelatedName = NULL;

  if (!SDDS_InitializeInput(&inSet, file)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return -1;
  }
#if DEBUGSDDS
  fprintf(stderr, "Reading file %s\n", file);
#endif
  if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
    return -1;
#if DEBUGSDDS
  fprintf(stderr, "ControlName present\n");
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

  doBitDecoderArray = 0;
  if (SDDS_CheckColumn(&inSet, "BitDecoderArray", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    doBitDecoderArray = 1;

  names = 0;
  description = name = relatedName = NULL;
  bitDecoderIndex = NULL;
  while ((code = SDDS_ReadPage(&inSet)) > 0) {
#if DEBUGSDDS
    fprintf(stderr, "Page %ld read\n", code);
#endif
    if (code == 1) {
      j = SDDS_ArrayCount(&inSet);
      for (i = 0; i < j; i++) {
        globalBDA[i].bitDescriptions = SDDS_GetArrayInString(&inSet, (&inSet)->layout.array_definition[i].name, &(globalBDA[i].bitLength));
      }
    }
    if (!(newNames = SDDS_CountRowsOfInterest(&inSet)))
      continue;
    if ((newNames < 0 || !(newName = SDDS_GetColumn(&inSet, "ControlName"))) ||
        (doDescriptions && !(newDescription = SDDS_GetColumn(&inSet, "Description"))) ||
        (doRelatedNames && !(newRelatedName = SDDS_GetColumn(&inSet, "RelatedControlName"))) ||
        (doBitDecoderArray && !(newBitDecoderArray = SDDS_GetColumn(&inSet, "BitDecoderArray")))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return -1;
    }
    if (!(name = SDDS_Realloc(name, (names + newNames) * sizeof(*name)))) {
      fprintf(stderr, "Unable to get ControlName data--allocation failure\n");
      return -1;
    }
    if (doDescriptions && !(description = SDDS_Realloc(description, (names + newNames) * sizeof(*description)))) {
      fprintf(stderr, "Unable to get Description data--allocation failure\n");
      return -1;
    }
    if (doRelatedNames && !(relatedName = SDDS_Realloc(relatedName, (names + newNames) * sizeof(*relatedName)))) {
      fprintf(stderr, "Unable to get RelatedControlName data--allocation failure\n");
      return -1;
    }
    if (doBitDecoderArray && !(bitDecoderIndex = SDDS_Realloc(bitDecoderIndex, (names + newNames) * sizeof(*bitDecoderIndex)))) {
      fprintf(stderr, "Unable to get BitDecoderArray data--allocation failure\n");
      return -1;
    }

    for (i = 0; i < newNames; i++) {
      name[names + i] = newName[i];
      if (doDescriptions)
        description[names + i] = newDescription[i];
      if (doRelatedNames)
        relatedName[names + i] = newRelatedName[i];
      if (doBitDecoderArray) {
        bitDecoderIndex[names + i] = -1;
        for (k = 0; k < j; k++) {
          if (strcmp(newBitDecoderArray[i], (&inSet)->layout.array_definition[k].name) == 0) {
            bitDecoderIndex[names + i] = k;
          }
        }
      }
    }
    names += newNames;
    free(newName);
    if (doDescriptions)
      free(newDescription);
    if (doRelatedNames)
      free(newRelatedName);
    if (doBitDecoderArray)
      free(newBitDecoderArray);
#if DEBUGSDDS
    fprintf(stderr, "Page %ld processed\n", code);
#endif
  }
#if DEBUGSDDS
  fprintf(stderr, "File processed\n");
#endif
  if (code == 0 || !SDDS_Terminate(&inSet)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return -1;
  }
#if DEBUGSDDS
  fprintf(stderr, "File terminated\n");
#endif
  *name0 = name;
  if (description0 && doDescriptions)
    *description0 = description;
  if (relatedName0 && doRelatedNames)
    *relatedName0 = relatedName;
  if (bitDecoderIndex0 && doBitDecoderArray)
    *bitDecoderIndex0 = bitDecoderIndex;
  return names;
}

long CheckForRequiredElements(char *filename, unsigned long mode) {
  SDDS_DATASET inSet;
  int32_t dimensions;
  if (!SDDS_InitializeInput(&inSet, filename)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return 0;
  }
  if (mode & EXPLICIT) {
    if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverity", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatus", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
      return 0;
  } else {
    if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverity", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatus", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
      return 0;
  }
  if (!(mode & NOINDICES)) {
    if (SDDS_CheckColumn(&inSet, "ControlNameIndex", NULL, SDDS_LONG, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverityIndex", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatusIndex", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK)
      return 0;
    if (SDDS_CheckArray(&inSet, "ControlNameString", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmSeverityString", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmStatusString", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK)
      return 0;
    if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME,
                                 "ControlNameString") != SDDS_LONG ||
        dimensions != 1) {
      fprintf(stderr, "error (sddsalarmlog): dimensions wrong for ControlNameString in %s\n",
              filename);
      return 0;
    }
    if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME,
                                 "AlarmSeverityString") != SDDS_LONG ||
        dimensions != 1) {
      fprintf(stderr, "error (sddsalarmlog): dimensions wrong for AlarmSeverityString in %s\n",
              filename);
      return 0;
    }
    if (SDDS_GetArrayInformation(&inSet, "dimensions", &dimensions, SDDS_GET_BY_NAME,
                                 "AlarmStatusString") != SDDS_LONG ||
        dimensions != 1) {
      fprintf(stderr, "error (sddsalarmlog): dimensions wrong for AlarmStatusString in %s\n",
              filename);
      return 0;
    }
  } else {
    if (SDDS_CheckColumn(&inSet, "ControlNameIndex", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmSeverityIndex", NULL, SDDS_SHORT, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckColumn(&inSet, "AlarmStatusIndex", NULL, SDDS_SHORT, NULL) == SDDS_CHECK_OK)
      return 0;
    if (SDDS_CheckArray(&inSet, "ControlNameString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmSeverityString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK ||
        SDDS_CheckArray(&inSet, "AlarmStatusString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
      return 0;
  }
  if (mode & DURATIONS) {
    if (SDDS_CheckColumn(&inSet, "Duration", NULL, SDDS_FLOAT, stderr) != SDDS_CHECK_OK)
      return 0;
  } else {
    if (SDDS_CheckColumn(&inSet, "Duration", NULL, SDDS_FLOAT, NULL) == SDDS_CHECK_OK)
      return 0;
  }
  if (SDDS_CheckColumn(&inSet, "PreviousRow", NULL, SDDS_LONG, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckColumn(&inSet, "TimeOfDay", "h", SDDS_FLOAT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "InputFile", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "TimeStamp", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartTime", "s", SDDS_DOUBLE, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartYear", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartMonth", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartJulianDay", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK ||
      SDDS_CheckParameter(&inSet, "StartDayOfMonth", NULL, SDDS_SHORT, stderr) != SDDS_CHECK_OK)
    return 0;
  return 1;
}

long CheckAlarmLogFile(char *filename, double *StartTime, double *StartDay, double *StartHour,
                       double *StartJulianDay, double *StartMonth, double *StartYear,
                       char **TimeStamp) {
  SDDS_DATASET inSet;
  long code;
  if (!SDDS_InitializeInput(&inSet, filename))
    return 0;
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
      return 0;
    }
  }
  SDDS_Terminate(&inSet);
  if (code == -1)
    return 1;
  return 0;
}

long GetLogFileIndices(SDDS_DATASET *outSet, unsigned long mode) {
  if ((iPreviousRow = SDDS_GetColumnIndex(outSet, "PreviousRow")) < 0 ||
      (iHour = SDDS_GetColumnIndex(outSet, "TimeOfDayWS")) < 0 ||
      (iHourIOC = SDDS_GetColumnIndex(outSet, "TimeOfDay")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): PreviousRow or TimeOfDay index unobtainable.\n");
    return 0;
  }
  if (!(mode & NOINDICES) &&
      ((iControlNameIndex = SDDS_GetColumnIndex(outSet, "ControlNameIndex")) < 0 ||
       (iAlarmStatusIndex = SDDS_GetColumnIndex(outSet, "AlarmStatusIndex")) < 0 ||
       (iAlarmSeverityIndex = SDDS_GetColumnIndex(outSet, "AlarmSeverityIndex")) < 0)) {
    fprintf(stderr, "error (sddsalarmlog): ControlNameIndex, AlarmStatusIndex, or AlarmSeverityIndex index unobtainable.\n");
    return 0;
  }
  if (mode & DURATIONS &&
      (iDuration = SDDS_GetColumnIndex(outSet, "Duration")) < 0) {
    fprintf(stderr, "error (sddsalarmlog): Duration index unobtainable.\n");
    return 0;
  }
  if (mode & EXPLICIT &&
      ((iControlName = SDDS_GetColumnIndex(outSet, "ControlName")) < 0 ||
       (iAlarmStatus = SDDS_GetColumnIndex(outSet, "AlarmStatus")) < 0 ||
       (iAlarmSeverity = SDDS_GetColumnIndex(outSet, "AlarmSeverity")) < 0)) {
    fprintf(stderr, "error (sddsalarmlog): ControlName, AlarmStatus, or AlarmSeverity index unobtainable.\n");
    return 0;
  }
  return 1;
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
