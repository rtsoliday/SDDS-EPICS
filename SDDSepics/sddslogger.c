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
 * Revision 1.35  2010/12/16 15:50:51  soliday
 * Added a gap time that is to be printed if the file writting takes too long.
 *
 * Revision 1.34  2009/12/04 16:49:57  soliday
 * Improved the last changes.
 *
 * Revision 1.33  2009/12/03 22:13:59  soliday
 * Updated by moving the needed definitions from alarm.h and alarmString.h
 * into this file directly. This was needed because of a problem with
 * EPICS Base 3.14.11. Andrew Johnson said that the values for these
 * definitions will not change in the future.
 *
 * Revision 1.32  2009/09/08 19:37:52  soliday
 * Fixed a typo.
 *
 * Revision 1.31  2009/09/08 18:02:50  soliday
 * Added an addtitional diagnostics message when run in verbose mode.
 *
 * Revision 1.30  2008/06/03 16:27:58  soliday
 * Fixed WIN32 problem.
 *
 * Revision 1.29  2008/04/02 16:41:42  soliday
 * When using the data strobe it will skip a strobe if it is still writing
 * to the output files. Previously it would allocate another row for each
 * output file. This previous method could end up allocating a lot of memory
 * if there was a problem with the file server.
 *
 * Revision 1.28  2008/03/24 15:01:40  soliday
 * Made is so that you can append to dailyFiles and monthlyFiles.
 *
 * Revision 1.27  2008/03/13 15:04:48  soliday
 * Improved code when running in one file per pv mode.
 * Also added the -monthlyFiles option.
 *
 * Revision 1.26  2008/02/15 22:29:55  soliday
 * Modified code so it exits if it fails to properly terminate the files at
 * midnight.
 *
 * Revision 1.25  2006/10/20 15:21:08  soliday
 * Fixed problems seen by linux-x86_64.
 *
 * Revision 1.24  2006/06/30 21:05:13  soliday
 * Updated so that the runcontrol is not pinged at the sample interval if
 * it is less than 2 seconds. Some data loggers have a sample interval of
 * .25 seconds.
 *
 * Revision 1.23  2006/06/07 18:06:40  soliday
 * Commented out step to reset the run control message every step. The problem
 * with the run control system that required this was fixed.
 *
 * Revision 1.22  2005/11/09 19:42:30  soliday
 * Updated to remove compiler warnings on Linux
 *
 * Revision 1.21  2005/11/08 22:05:03  soliday
 * Added support for 64 bit compilers.
 *
 * Revision 1.20  2005/07/21 19:19:29  soliday
 * Made multiple changes for the onePVPerFile option. It is faster and more
 * reliable. Also changed the error message when some PVs do not connect. The
 * PV names are printed out to stderr.
 *
 * Revision 1.19  2005/06/29 19:14:25  soliday
 * Fixed problem with last change. Also added change for one PV per file mode
 * so that directory slashes in PV names are replaced with another symbol.
 *
 * Revision 1.18  2005/06/28 16:16:06  shang
 * added ControlName as column description
 *
 * Revision 1.17  2005/04/26 21:43:33  soliday
 * Fixed -time option when using -dataStrobePV option. There was a bug when
 * the conditions failed. It would increment the step number without pausing.
 * This would result in premature exiting of the program.
 *
 * Revision 1.16  2005/04/21 15:15:34  soliday
 * Added checking to ensure runControlPingWhileSleep commands returned without
 * an error.
 *
 * Revision 1.15  2005/03/16 22:15:07  soliday
 * Reindented code.
 *
 * Revision 1.14  2005/03/16 22:11:38  soliday
 * Updated to work with runcontrol. Also changed some stderr print statements
 * to stdout print statements.
 *
 * Revision 1.13  2005/03/03 17:22:42  soliday
 * Updated to work on WIN32
 *
 * Revision 1.12  2005/02/25 21:34:57  borland
 * Removed debugging.
 *
 * Revision 1.11  2005/02/12 19:46:09  borland
 * Added another missing pclose().
 *
 * Revision 1.10  2004/11/04 16:34:53  shang
 * the watchInput feature now watches both the name of the final link-to file and the state of the input file.
 *
 * Revision 1.9  2004/09/27 16:19:00  soliday
 * Added missing ca_task_exit call.
 *
 * Revision 1.8  2004/07/19 17:39:37  soliday
 * Updated the usage message to include the epics version string.
 *
 * Revision 1.7  2004/07/15 19:38:34  soliday
 * Replace sleep commands with ca_pend_event commands because Epics/Base 3.14.x
 * has an inactivity timer that was causing disconnects from PVs when the
 * log interval time was too large.
 *
 * Revision 1.6  2004/06/02 16:20:18  soliday
 * Replaced all ca_pend_io and ca_pend_event commands that had short polling times
 * with ca_poll.
 *
 * Revision 1.5  2004/02/05 20:32:52  soliday
 * Updated usage message so that it would compile on Windows.
 *
 * Revision 1.4  2004/01/19 18:27:00  borland
 * Added data strobe feature (mostly by H. Shang).
 *
 * Revision 1.3  2003/12/18 20:25:42  soliday
 * Fixed an issue where the logger would try to catch up after CA problems
 * stopped.
 *
 * Revision 1.2  2003/10/27 14:57:21  borland
 * Added the -flushInterval option to allow deferred writing of data to disk.
 *
 * Revision 1.1  2003/08/27 19:51:15  soliday
 * Moved into subdirectory
 *
 * Revision 1.33  2003/05/05 15:08:40  soliday
 * Fixed a problem when most of the PVs are inaccessable.
 *
 * Revision 1.32  2003/04/21 18:18:15  soliday
 * Improved the logging interval accuracy.
 *
 * Revision 1.31  2003/03/04 19:13:10  soliday
 * Changed the default pend time to 10 seconds.
 *
 * Revision 1.30  2002/10/15 19:13:41  soliday
 * Modified so that sddslogger, sddsglitchlogger, and sddsstatmon no longer
 * use ezca.
 *
 * Revision 1.29  2002/08/14 20:00:34  soliday
 * Added Open License
 *
 * Revision 1.28  2002/07/01 21:12:11  soliday
 * Fsync is no longer the default so it is now set after the file is opened.
 *
 * Revision 1.27  2002/05/08 18:29:39  shang
 * logged the string values of numeric PV and stored them in the array named as
 * <ReadbackName>String
 *
 * Revision 1.26  2002/04/08 20:23:59  soliday
 * Changed usage message per Shang's request.
 *
 * Revision 1.25  2002/04/08 19:25:30  soliday
 * Changed to fixed row count mode.
 *
 * Revision 1.24  2002/04/01 22:39:51  shang
 * added verbose feature back to -dailyFiles option
 *
 * Revision 1.23  2002/03/29 19:54:34  shang
 * added timetag feature to -dailyFiles option to generate file names with exention .HH:MM:SS
 *
 * Revision 1.22  2002/03/25 20:39:11  shang
 * added rowlimit and timelimit to -dailyFiles option
 *
 * Revision 1.21  2002/01/03 22:11:41  soliday
 * Fixed problems on WIN32.
 *
 * Revision 1.20  2001/11/15 15:30:50  borland
 * Added verbose qualifier to -dailyFiles option, which causes program to
 * announce when it is able to start a new file upon startup.
 *
 * Revision 1.19  2001/10/16 21:33:40  shang
 * added -append option and modified -watchinput to make it work for linking files too through replacing
 * st_mtime by st_ctime
 *
 * Revision 1.18  2001/07/10 16:21:26  soliday
 * Added the -watchInput option so that the program with exit if the inputfile
 * is changed.
 *
 * Revision 1.17  2001/05/03 19:53:45  soliday
 * Standardized the Usage message.
 *
 * Revision 1.16  2000/04/20 15:58:49  soliday
 * Fixed WIN32 definition of usleep.
 *
 * Revision 1.15  2000/04/19 15:51:38  soliday
 * Removed some solaris compiler warnings.
 *
 * Revision 1.14  2000/03/08 17:14:25  soliday
 * Removed compiler warnings under Linux.
 *
 * Revision 1.13  2000/01/04 22:36:56  borland
 * Now exits if the inhibit PV does not permit data collection when the job
 * starts.
 *
 * Revision 1.12  1999/12/14 14:53:58  borland
 * Now checks for file locks and inhibit PV value prior to any other CA.
 * This prevents a fair amount of pointless CA broadcasting.
 *
 * Revision 1.11  1999/09/17 22:12:11  soliday
 * This version now works with WIN32
 *
 * Revision 1.10  1999/08/25 17:56:31  borland
 * Rewrote code for data logging inhibiting.  Collected code in SDDSepics.c
 *
 * Revision 1.9  1999/08/23 14:43:45  borland
 * Add/modified -inhibitPV option:
 * For sddsstatmon, added this feature.
 * For others, modified the usage message and verbose message.
 *
 * Revision 1.8  1999/08/17 18:57:28  soliday
 * Added touchfile procedure when the inhibit pv is non-zero
 *
 * Revision 1.7  1999/08/17 18:29:22  soliday
 * Added the -inhibitPV option
 *
 * Revision 1.6  1999/08/06 18:31:40  emery
 * Tabified some blocks of code.
 *
 * Revision 1.5  1999/07/27 20:50:04  soliday
 * Fixed bug with the row counter when there are multiple files
 *
 * Revision 1.4  1999/07/26 19:41:39  soliday
 * Fixed bug with ONERROR_SKIPROW
 *
 */

#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

#include <cadef.h>

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#ifdef _WIN32
#  include <winsock.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#define CLO_SAMPLEINTERVAL 0
#define CLO_LOGINTERVAL 1
#define CLO_STEPS 2
#define CLO_VERBOSE 3
#define CLO_SINGLE_SHOT 4
#define CLO_TIME 5
#define CLO_PRECISION 6
#define CLO_HIGHTIMEPRECISION 7
#define CLO_ONERROR 8
#define CLO_PENDIOTIME 9
#define CLO_CONDITIONS 10
#define CLO_GENERATIONS 11
#define CLO_ENFORCETIMELIMIT 12
#define CLO_OFFSETTIMEOFDAY 13
#define CLO_DAILY_FILES 14
#define CLO_MONTHLY_FILES 15
#define CLO_INHIBIT 16
#define CLO_WATCHINPUT 17
#define CLO_APPEND 18
#define CLO_ERASE 19
#define CLO_FLUSHINTERVAL 20
#define CLO_DATASTROBEPV 21
#define CLO_ONE_PV_PER_FILE 22
#define CLO_RUNCONTROLPV 23
#define CLO_RUNCONTROLDESC 24
#define COMMANDLINE_OPTIONS 25
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "sampleInterval", "logInterval", "steps", "verbose",
  "singleshot", "time", "precision", "hightimeprecision", "onerror",
  "pendiotime", "conditions", "generations",
  "enforcetimelimit", "offsettimeofday", "dailyfiles", "monthlyfiles", "inhibitpv",
  "watchinput", "append", "erase", "flushinterval", "datastrobepv",
  "onepvperfile", "runControlPV", "runControlDescription"};

#define DAILYFILES_VERBOSE 0x0001U
#define MONTHLYFILES_VERBOSE 0x0001U
#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2
static char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double"};

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

#define ONERROR_USEZERO 0
#define ONERROR_SKIPROW 1
#define ONERROR_EXIT 2
#define ONERROR_OPTIONS 3
static char *onerror_options[ONERROR_OPTIONS] = {
  "usezero",
  "skiprow",
  "exit"};
typedef struct
{
  long no_str;
  char **strs, *PVname;
  short flag; /*the flag for ca reading done */
} ENUM_PV;

typedef struct
{
  char *filename, **DeviceNames, **ReadMessages, **ReadbackNames, **ReadbackUnits;
  long n_variables, CAerrors;
  int32_t *Average, *DoublePrecision;
  double *ScaleFactors, *DeadBands, *values, *baselineValues, *averageValues;
  chid *channelID;
  short *connectedInTime;
  long enumPVs;
  ENUM_PV *enumPV;
  char *lastlink; /* name of last link of filename, eg. lastlink is filename2 for filename-->filename1-->filenam2-->filename3 */
  struct stat filestat;
} INPUTFILENAME;

typedef struct
{
  char *filename, *origOutputFile;
  long *ReadbackIndex;
} OUTPUTFILENAME;

typedef struct
{
  char *PV;
  chid channelID;
  double currentValue;
  short triggered, datalogged, initialized;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  long trigStep; /* trigStep: the Step where trigger occurs */
  long datastrobeMissed;
} DATASTROBE_TRIGGER;

static char *USAGE1 = "sddslogger <SDDSinputfile1> <SDDSoutputfile1> <SDDSinputfile2> <SDDSoutputfile2>...\n\
 [-generations[=digits=<integer>][,delimiter=<string>][,rowlimit=<number>][,timelimit=<secs>] | \n\
 [-dailyFiles[=rowlimit=<number>][,timelimit=<secs>,timetag][,verbose]]\n\
 [-monthlyFiles[=rowlimit=<number>][,timelimit=<secs>,timetag][,verbose]]\n\
 [-onePvPerFile=<dirName>]\n\
 [-erase | -append[=recover][,toPage]] | \n\
 [-sampleInterval=<real-value>[,<time-units>] | -dataStrobePV=<PVname>] \n\
 [-logInterval=<integer-value>] [-flushInterval=<integer-value>] [-watchInput] \n\
 [-steps=<integer-value> | -time=<real-value>[,<time-units>]] \n\
 [-enforceTimeLimit] [-offsetTimeOfDay]\n\
 [-verbose] [-singleshot{=noprompt|stdout}] \n\
 [-precision={single|double}]\n\
 [-highTimePrecision]\n\
 [-onerror={usezero|skiprow|exit}] [-pendIOtime=<value>] \n\
 [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
 [-inhibitPV=name=<name>[,pendIOTime=<seconds>][,waitTime=<seconds>]]\n\
 [-runControlPV=string=<string>,pingTimeout=<value>\n\
 [-runControlDescription=string=<string>]\n\n";
static char *USAGE2 = "Writes values of process variables or devices to a binary SDDS file.\n\
<SDDSinputfile>    defines the process variables or device names to be read;\n\
                   One column must be \"Device\" or \"ControlName\" (they have the same effect),\n\
                   \"ReadbackUnits\" is required, columns \"ReadbackName\", \"Message\",\n\
                   \"ScaleFactor\", \"Average\", \"DoublePrecision\", and \"DeadBand\" are optional.\n\
<SDDSoutputfile>   Contains the data read from the process variables.\n\
generations        The output is sent to the file <SDDSoutputfile>-<N>, where <N> is\n\
                   the smallest positive integer such that the file does not already \n\
                   exist.   By default, four digits are used for formating <N>, so that\n\
                   the first generation number is 0001.  If a row limit or time limit\n\
                   is given, a new file is started when the given limit is reached.\n";
static char *USAGE3 = "dailyFiles         The output is sent to the file <SDDSoutputfile>-YYYY-JJJ-MMDD.<N>,\n\
                   where YYYY=year, JJJ=Julian day, and MMDD=month+day.  A new file is\n\
                   started after midnight. If a row limit or time limit\n\
                   is given, a new file is started when the given limit is reached.\n\
                   If timetag is given, the file name would look like \n\
                   rootname-YYYY-JJJ-MMDD.HH:MM:SS, and it overrides <N>.\n\
monthlyFiles       The output is sent to the file <SDDSoutputfile>-YYYY-MM.<N>,\n\
                   where YYYY=year and MM=month.  A new file is started after\n\
                   midnight on the 1st of the month. If a row limit or time limit\n\
                   is given, a new file is started when the given limit is reached.\n\
                   If timetag is given, the file name would look like \n\
                   rootname-YYYY-MM.HH:MM:SS, and it overrides <N>.\n\
onePvPerFile       Data for each PV is written to a different file.  Files are in subdirectories\n\
                   <dirName>/<pvName>.  In this case, only one input file is allowed and no \n\
                   output filenames are expected.\n";
static char *USAGE4 = "erase              The output file is erased before execution.\n\
append             Appends new values in a new SDDS page in the output file.\n\
                   Optionally recovers garbled SDDS data.  The 'toPage' qualifier\n\
                   results in appending to the last data page, rather than creation\n\
                   of a new page.\n\
sampleInterval     Desired interval in seconds for each reading step,\n\
                   valid time units are seconds, minutes, hours, or days.\n\
dataStrobePV       Give the name of a data strobe process variable.  This PV\n\
                   is expected to change value whenever data is to be logged.\n\
                   The expected value is the UNIX time-since-epoch.\n";
static char *USAGE5 = "logInterval        desired interval in sampling steps.\n\
                   If any PVs are averaged, then this is the number of samples\n\
                   that are averaged.\n\
flushInterval      desired interval in logging steps for flushing the files.\n\
steps              Number of reads for each process variable or device.\n\
time               Total time for monitoring process variables.\n\
                   Valid time units are seconds, minutes, hours, or days.\n\
enforceTimeLimit   Enforces the time limit given with the -time option, even if\n\
                   the expected number of samples has not been taken.\n\
offsetTimeOfDay    Adjusts the starting TimeOfDay value so that it corresponds\n\
                   to the day for which the bulk of the data is taken.  Hence, a\n\
                   26 hour job started at 11pm would have initial time of day of\n\
                   -1 hour and final time of day of 25 hours.\n";
static char *USAGE6 = "verbose            Prints out a message when data is taken.\n\
singleshot         Single shot read initiated by a <cr> key press; time_interval is disabled.\n\
                   By default, a prompt is issued to stderr.  The qualifiers may be used to\n\
                   remove the prompt or have it go to stdout.\n\
precision          Specify single (default) or double precision for PV data.\n\
onerror            Action taken when a channel access error occurs. Default is\n\
                   to use zeroes for values.\n\
pendIOtime         Sets the maximum time to wait for connection to each PV.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
inhibitPV          Checks this PV prior to each sample.  If the value is nonzero, then data\n\
                   collection is inhibited.  None of the conditions-related or other PVs are\n\
                   polled.\n";
static char *USAGE7 = "runControlPV       specifies a runControl PV name.\n\
runControlDescription\n\
                   specifies a runControl PV description record.\n\n\
Program by Robert Soliday, H. Shang\n\
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

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 100
#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4

#define NAuxiliaryColumnNames 5
static char *AuxiliaryColumnNames[NAuxiliaryColumnNames] = {
  "Step",
  "Time",
  "TimeOfDay",
  "DayOfMonth",
  "CAerrors",
};

long ReadScalarValuesModified(INPUTFILENAME *input, long numFiles, double factor, double pendIOTime);

long getScalarMonitorDataLocal(char ***DeviceName, char ***ReadMessage,
                               char ***ReadbackName, char ***ReadbackUnits,
                               double **scaleFactor, double **DeadBands,
                               long *variables, char *inputFile,
                               int32_t **Average, int32_t **DoublePrecision);
void getStringValuesOfEnumPV(struct event_handler_args event);
long CheckEnumCACallbackStatus(ENUM_PV *enumPV, double pendIOTime);

void startMonitorFile(SDDS_DATASET *output_page, char *outputfile, long Precision,
                      long n_variables, char **ReadbackNames, char **DeviceNames, char **ReadbackUnits,
                      double *StartTime, double *StartDay, double *StartHour, double *StartJulianDay,
                      double *StartMonth, double *StartYear, char **TimeStamp, int32_t *DoublePrecision,
                      ENUM_PV *enumPV, long enumPVs, long flushInterval, long datastrobeUsed,
                      long disconnect, long minimal, short highTimePrecision);
void FreeOutputFiles(OUTPUTFILENAME *output, long numFiles);
void FreeInputFiles(INPUTFILENAME *input, long numFiles);

long setUpOnePvPerFileOutput(INPUTFILENAME **input, OUTPUTFILENAME **output, long numFiles,
                             char *onePvOutputDirectory);

/*for datastrobe trigger */
void datastrobeTriggerEventHandler(struct event_handler_args event);
long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger);
void valueRelatedEventHandler(struct event_handler_args event);

DATASTROBE_TRIGGER datastrobeTrigger;

int runControlPingWhileSleep(double sleepTime);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

volatile int sigint = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  INPUTFILENAME *input = NULL;
  OUTPUTFILENAME *output = NULL;
  SDDS_DATASET *input_page, *output_page, original_page;
  long i_arg, optionCode, TimeUnits, NStep, steps_set, verbose, singleShot, totalTimeSet,
    Precision, onerrorindex, generations,
    dailyFiles, enforceTimeLimit, offsetTimeOfDay, numFiles, fileSwitch, i, j,
    conditions, newFileCountdown, Step, *outputRow, outRow, CAerrors,
    LogInterval, LogStep, errorFound, dailyFilesVerbose,
    monthlyFiles, monthlyFilesVerbose,
    inhibit, append, appendToPage, recover, erase, flushStep, flushInterval;
  int32_t generationsDigits, generationsRowLimit, NColumnNames;
  int64_t *initialOutputRow;
  double SampleTimeInterval, TotalTime, pendIOtime,
    generationsTimeLimit, *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL,
                          *CondHoldoff = NULL, *CondDataBuffer = NULL, HourNow, DayNow, StartTime, RunStartTime, StartJulianDay, StartDay,
                          StartYear, StartMonth, StartHour, generationsTimeStop, ElapsedTime, LastHour, LastDay,
                          EpochTime, TimeOfDay, DayOfMonth, SampleTimeToExecute, SampleTimeToWait, factor, RunTime, PingTime;
  long double LEpochTime;
  unsigned long CondMode, dummyFlags, appendFlags, dailyFilesFlags, monthlyFilesFlags;
  char *precision_string, *onerror, *CondFile, *generationsDelimiter, **CondDeviceName = NULL,
                                                                      **CondReadMessage = NULL, *TimeStamp, answer[256], **ColumnNames, *ptr;
  chid *CondCHID = NULL;
  char *onePvOutputDirectory = NULL;
  long minimal = 0;
  int32_t InhibitWaitTime = 5;
  long timetag = 0, length;
  double InhibitPendIOTime = 10;
  chid inhibitID;
  PV_VALUE InhibitPV;
  int filestat_failed;
  int watchInput = 0, ping;
  long datastrobeTriggerPresent = 0, doDisconnect = 0;
  long currentFile = 0;
  double t1, t2;
  short highTimePrecision = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5, USAGE6, USAGE7);
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

  datastrobeTrigger.PV = NULL;
  append = appendToPage = recover = erase = outRow = 0;
  initialOutputRow = NULL;
  outputRow = NULL;
  input = tmalloc(sizeof(*input) * 40);
  for (i = 0; i < 40; i++)
    input[i].filename = input[i].lastlink = NULL;
  output = tmalloc(sizeof(*output) * 40);
  input_page = tmalloc(sizeof(*input_page) * 40);
  output_page = tmalloc(sizeof(*output_page) * 40);
  SampleTimeInterval = DEFAULT_TIME_INTERVAL;
  LogInterval = 1;
  flushInterval = 1;
  NStep = DEFAULT_STEPS;
  dailyFilesVerbose = monthlyFilesVerbose = 0;
  dailyFiles = offsetTimeOfDay = enforceTimeLimit = generationsRowLimit = 0;
  monthlyFiles = 0;
  generations = totalTimeSet = singleShot = verbose = steps_set = 0;
  fileSwitch = numFiles = 0;
  Precision = PRECISION_SINGLE;
  onerrorindex = ONERROR_USEZERO;
  pendIOtime = 10.0;
  CondFile = NULL;
  generationsTimeLimit = 0.0;
  errorFound = inhibit = 0;
  CondDataBuffer = NULL;
  HourNow = DayNow = 0;
  CondMode = 0;
  precision_string = onerror = NULL;

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
      switch (optionCode = match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&SampleTimeInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -sampleInterval\n", NULL);
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            SampleTimeInterval *= TimeUnitFactor[TimeUnits];
          else
            bomb("unknown/ambiguous time units given for -sampleInterval\n", NULL);
        }
        break;
      case CLO_LOGINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_long(&LogInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -logInterval\n", NULL);
        if ((LogInterval < 1) || (LogInterval > 20))
          bomb("value for -logInterval must be between 1 and 20\n", NULL);
        break;
      case CLO_FLUSHINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_long(&flushInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -flushInterval\n", NULL);
        if ((flushInterval < 1))
          bomb("value for -flushInterval must be positive", NULL);
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&NStep, s_arg[i_arg].list[1])))
          bomb("no value given for option -steps\n", NULL);
        steps_set = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_SINGLE_SHOT:
        singleShot = SS_PROMPT;
        if (s_arg[i_arg].n_items != 1) {
          if (strncmp(s_arg[i_arg].list[1], "noprompt", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_NOPROMPT;
          else if (strncmp(s_arg[i_arg].list[1], "stdout", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_STDOUTPROMPT;
          else
            SDDS_Bomb("invalid -singleShot qualifier");
        }
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
      case CLO_APPEND:
        append = 1;
        recover = 0;
        appendToPage = 0;
        s_arg[i_arg].n_items--;
        if (!scanItemList(&appendFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "recover", -1, NULL, 0, 1,
                          "topage", -1, NULL, 0, 2,
                          NULL))
          SDDS_Bomb("invalid -append syntax/value");
        recover = appendFlags & 1;
        appendToPage = appendFlags & 2;
        s_arg[i_arg].n_items++;
        break;
      case CLO_ERASE:
        erase = 1;
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items != 2 ||
            !(precision_string = s_arg[i_arg].list[1]))
          bomb("no value given for option -precision\n", NULL);
        switch (Precision = match_string(precision_string, precision_option,
                                         PRECISION_OPTIONS, 0)) {
        case PRECISION_SINGLE:
        case PRECISION_DOUBLE:
          break;
        default:
          fprintf(stderr, "unrecognized precision value given for option %s\n", s_arg[i_arg].list[0]);
          return (1);
        }
        break;
      case CLO_HIGHTIMEPRECISION:
        highTimePrecision = 1;
        break;
      case CLO_ONERROR:
        if (s_arg[i_arg].n_items != 2 ||
            !(onerror = s_arg[i_arg].list[1])) {
          bomb("No value given for option -onerror.\n", NULL);
        }
        switch (onerrorindex = match_string(onerror, onerror_options, ONERROR_OPTIONS, 0)) {
        case ONERROR_USEZERO:
        case ONERROR_SKIPROW:
        case ONERROR_EXIT:
          break;
        default:
          fprintf(stderr, "unrecognized onerror option given: %s\n", onerror);
          return (1);
        }
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items < 3 ||
            SDDS_StringIsBlank(CondFile = s_arg[i_arg].list[1]) ||
            !(CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2)))
          SDDS_Bomb("invalid -conditions syntax/values");
        break;
      case CLO_GENERATIONS:
        if (dailyFiles) {
          SDDS_Bomb("-dailyFiles and -generations are incompatible options");
        }
        if (monthlyFiles) {
          SDDS_Bomb("-monthlyFiles and -generations are incompatible options");
        }
        generationsDigits = DEFAULT_GENERATIONS_DIGITS;
        generations = 1;
        generationsDelimiter = "-";
        generationsRowLimit = 0;
        generationsTimeLimit = 0;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &generationsDigits, 1, 0,
                          "delimiter", SDDS_STRING, &generationsDelimiter, 1, 0,
                          "rowlimit", SDDS_LONG, &generationsRowLimit, 1, 0,
                          "timelimit", SDDS_DOUBLE, &generationsTimeLimit, 1, 0,
                          NULL) ||
            generationsDigits < 1)
          SDDS_Bomb("invalid -generations syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_ENFORCETIMELIMIT:
        enforceTimeLimit = 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_DAILY_FILES:
        if (monthlyFiles) {
          SDDS_Bomb("-dailyFiles and -monthlyFiles are incompatible options");
        }
        if (generations) {
          SDDS_Bomb("-dailyFiles and -generations are incompatible options");
        }
        generationsDigits = 4;
        generationsDelimiter = ".";
        dailyFiles = 1;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dailyFilesFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "rowlimit", SDDS_LONG, &generationsRowLimit, 1, 0,
                          "timelimit", SDDS_DOUBLE, &generationsTimeLimit, 1, 0,
                          "timetage", -1, NULL, 0, USE_TIMETAG,
                          "verbose", -1, NULL, 0, DAILYFILES_VERBOSE,
                          NULL))
          SDDS_Bomb("invalid -dailyFiles syntax/values");
        if (dailyFilesFlags & USE_TIMETAG)
          timetag = 1;
        if (dailyFilesFlags & DAILYFILES_VERBOSE)
          dailyFilesVerbose = 1;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_MONTHLY_FILES:
        if (dailyFiles) {
          SDDS_Bomb("-dailyFiles and -monthlyFiles are incompatible options");
        }
        if (generations) {
          SDDS_Bomb("-monthlyFiles and -generations are incompatible options");
        }
        generationsDigits = 4;
        generationsDelimiter = ".";
        monthlyFiles = 1;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&monthlyFilesFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "rowlimit", SDDS_LONG, &generationsRowLimit, 1, 0,
                          "timelimit", SDDS_DOUBLE, &generationsTimeLimit, 1, 0,
                          "timetage", -1, NULL, 0, USE_TIMETAG,
                          "verbose", -1, NULL, 0, MONTHLYFILES_VERBOSE,
                          NULL))
          SDDS_Bomb("invalid -monthlyFiles syntax/values");
        if (monthlyFilesFlags & USE_TIMETAG)
          timetag = 1;
        if (monthlyFilesFlags & MONTHLYFILES_VERBOSE)
          monthlyFilesVerbose = 1;
        s_arg[i_arg].n_items += 1;
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
      case CLO_DATASTROBEPV:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -dataStrobePV syntax!");
        datastrobeTrigger.PV = s_arg[i_arg].list[1];
        datastrobeTriggerPresent = 1;
        break;
      case CLO_ONE_PV_PER_FILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -onePvPerFile syntax!");
#if defined(_WIN32)
        SDDS_Bomb("-onePvPerFile option is not currently available on Windows");
#endif
        onePvOutputDirectory = s_arg[i_arg].list[1];
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
        fprintf(stderr, "unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (fileSwitch == 0) {
        input[numFiles].filename = s_arg[i_arg].list[0];
        fileSwitch++;
      } else {
        output[numFiles].origOutputFile = s_arg[i_arg].list[0];
        numFiles++;
        fileSwitch--;
      }
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

#if !defined(_WIN32)
  if (onePvOutputDirectory) {
    if (numFiles != 0 && fileSwitch != 1)
      SDDS_Bomb("Give one and only one input filename for -onePvPerFile mode");
    if (!(numFiles = setUpOnePvPerFileOutput(&input, &output, numFiles, onePvOutputDirectory)))
      SDDS_Bomb("Problem setting up for one-pv-per-file output");
    if (!(output_page = malloc(sizeof(*output_page) * numFiles)))
      SDDS_Bomb("Memory allocation failure");
    SDDS_SetDefaultIOBufferSize(1000);
    minimal = 1;
    if (watchInput) {
      input[0].lastlink = read_last_link_to_file(input[0].filename);
      if (input[0].lastlink)
        fprintf(stdout, "link-to file is %s\n", input[0].lastlink);
      if (get_file_stat(input[0].filename, input[0].lastlink, &input[0].filestat) != 0) {
        fprintf(stderr, "Problem getting modification time for %s\n", input[0].filename);
        return (1);
      }
    }
  } else {
    if (fileSwitch != 0)
      bomb("There must be one output file for each input file\n", NULL);
    if (numFiles == 0)
      bomb("input file not given\n", NULL);
    if (watchInput) {
      for (i = 0; i < numFiles; i++) {
        input[i].lastlink = read_last_link_to_file(input[i].filename);
        if (input[i].lastlink)
          fprintf(stdout, "link-to file is %s\n", input[i].lastlink);
        if (get_file_stat(input[i].filename, input[i].lastlink, &input[i].filestat) != 0) {
          fprintf(stderr, "Problem getting modification time for %s\n", input[i].filename);
          return (1);
        }
      }
    }
  }
#endif

  factor = 1.0 / LogInterval;
  if (totalTimeSet) {
    if (steps_set) {
      printf("Option time supersedes option steps\n");
    }
    NStep = TotalTime / SampleTimeInterval + 1;
    enforceTimeLimit = 1;
  } else {
    enforceTimeLimit = 0;
  }
  if (append && erase)
    SDDS_Bomb("-append and -erase are incompatible options");
  if (append && generations)
    SDDS_Bomb("-append and -generations are incompatible options");
  if (erase && generations)
    SDDS_Bomb("-erase and -generations are incompatible options");

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
  initialOutputRow = malloc(sizeof(*initialOutputRow) * numFiles);
  outputRow = malloc(sizeof(*outputRow) * numFiles);
  for (i = 0; i < numFiles; i++)
    initialOutputRow[i] = 0;

  if (append || erase) {
    generationsDigits = 0;
    generationsDelimiter = NULL;
  }
  j = 0;
  for (i = 0; i < numFiles; i++) {
    if (!onePvOutputDirectory) {
      if (!getScalarMonitorDataLocal(&input[i].DeviceNames, &input[i].ReadMessages,
                                     &input[i].ReadbackNames, &input[i].ReadbackUnits,
                                     &input[i].ScaleFactors, &input[i].DeadBands,
                                     &input[i].n_variables, input[i].filename,
                                     &input[i].Average, &input[i].DoublePrecision))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!input[i].n_variables) {
        fprintf(stderr, "error: no data in input file %s \n", input[i].filename);
        return (1);
      }
    }
    /*
        fprintf(stderr, "%ld values for file %s\n", input[i].n_variables,
        output[i].origOutputFile);
      */

    if (!(input[i].values = (double *)malloc(sizeof(double) * input[i].n_variables)) ||
        !(input[i].averageValues = (double *)malloc(sizeof(double) * input[i].n_variables)) ||
        !(input[i].baselineValues = (double *)malloc(sizeof(double) * input[i].n_variables)) ||
        !(input[i].channelID = (chid *)malloc(sizeof(*input[i].channelID) * input[i].n_variables)) ||
        !(input[i].connectedInTime = (short *)malloc(sizeof(*input[i].connectedInTime) * input[i].n_variables)))
      SDDS_Bomb("memory allocation failure");

    if (generations) {
      output[i].filename =
        MakeGenerationFilename(output[i].origOutputFile, generationsDigits,
                               generationsDelimiter, NULL);
      if (verbose)
        fprintf(stdout, "New generation file started: %s\n",
                output[i].filename);
    } else if (dailyFiles) {
      output[i].filename =
        MakeDailyGenerationFilename(output[i].origOutputFile,
                                    generationsDigits, generationsDelimiter, timetag);
      if (dailyFilesVerbose)
        fprintf(stdout, "New generation file started: %s\n",
                output[i].filename);
    } else if (monthlyFiles) {
      output[i].filename =
        MakeMonthlyGenerationFilename(output[i].origOutputFile,
                                      generationsDigits, generationsDelimiter, timetag);
      if (monthlyFilesVerbose)
        fprintf(stdout, "New generation file started: %s\n",
                output[i].filename);
    } else
      SDDS_CopyString(&output[i].filename, output[i].origOutputFile);

    makeTimeBreakdown(getTimeInSecs(), NULL, &DayNow, &HourNow, NULL, NULL, NULL, NULL);

    if (verbose) {
      fprintf(stdout, "Output filename: %s\n", output[i].filename);
      fflush(stdout);
    }

#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (j == 100) {
        if (runControlPingWhileSleep(0)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          return (1);
        }
        sprintf(rcParam.message, "Initializing %d percent", (int)(100 * i / numFiles));
        /*strcpy( rcParam.message, "Initializing");*/
        rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
        if (rcParam.status != RUNCONTROL_OK) {
          fprintf(stderr, "Unable to write status message and alarm severity\n");
          return (1);
        }
        j = 0;
      } else {
        j++;
      }
    }
#endif

    /* check if output file already exists */
    if (fexists(output[i].filename)) {
      if (append) {
        continue;
      } else if (!erase) {
        fprintf(stderr, "error: File %s already exists.\n", output[i].filename);
        return (1);
      }
    } else {
      append = 0;
    }
  }

  if (inhibit) {
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 1)) {
      fprintf(stderr, "possible error: possible problem doing search for Inhibit PV %s\n", InhibitPV.name);
      return (1);
    }
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Data collection inhibited---not starting\n");
      return (0);
    }
  }
  for (i = 0; i < numFiles; i++) {
    if (verbose)
      fprintf(stdout, "Searching for PVs for file %ld (%s)\n", i, output[i].filename);
    input[i].enumPVs = 0;
    input[i].enumPV = NULL;
    for (j = 0; j < input[i].n_variables; j++) {
      input[i].averageValues[j] = 0;
      if (ca_search(input[i].DeviceNames[j], input[i].channelID + j) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", input[i].DeviceNames[j]);
        return (1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for the following channels:\n");
    for (i = 0; i < numFiles; i++) {
      for (j = 0; j < input[i].n_variables; j++) {
        if (ca_state(input[i].channelID[j]) != cs_conn) {
          fprintf(stderr, " %s\n", input[i].DeviceNames[j]);
        }
      }
    }
  }
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

  for (i = 0; i < numFiles; i++) {
    for (j = 0; j < input[i].n_variables; j++) {
      /*get the string values of enumerated pv*/
      if (ca_field_type(input[i].channelID[j]) == DBR_ENUM) {
        input[i].enumPV = SDDS_Realloc(input[i].enumPV, sizeof(*(input[i].enumPV)) * (input[i].enumPVs + 1));
        input[i].enumPV[input[i].enumPVs].flag = 0;
        length = strlen(input[i].ReadbackNames[j]);
        input[i].enumPV[input[i].enumPVs].PVname = malloc(sizeof(char) * (length + 20));
        strcpy(input[i].enumPV[input[i].enumPVs].PVname, input[i].ReadbackNames[j]);
        strcat(input[i].enumPV[input[i].enumPVs].PVname, "String");
        /*
                SDDS_CopyString(&input[i].enumPV[input[i].enumPVs].PVname,input[i].ReadbackNames[j]); */

        if (ca_array_get_callback(DBR_CTRL_ENUM, ca_element_count(input[i].channelID[j]),
                                  input[i].channelID[j], getStringValuesOfEnumPV,
                                  (void *)&input[i].enumPV[input[i].enumPVs]) != ECA_NORMAL) {
          fprintf(stderr, "error: unable to establish callback.\n");
          return (1);
        }
        ca_poll();
        if (!input[i].enumPV[input[i].enumPVs].flag &&
            CheckEnumCACallbackStatus(&input[i].enumPV[input[i].enumPVs], pendIOtime))
          return (1);
        input[i].enumPVs++;
      }
    }
  }
  if (verbose)
    fprintf(stdout, "Done searching for PVs\n");

  if (!onePvOutputDirectory) {
    if (numFiles > 40)
      doDisconnect = 1; /* disconnect files after writing */
  }
  if (verbose)
    fprintf(stdout, "Note: will %s disconnecting files\n",
            doDisconnect ? "be" : "not be");
  for (i = 0; i < numFiles; i++) {
    if (append) {
      if (!SDDS_CheckFile(output[i].filename)) {
        if (recover) {
          char commandBuffer[1024];
          fprintf(stderr, "warning: file %s is corrupted--reconstructing before appending--some data may be lost.\n", output[i].filename);
          sprintf(commandBuffer, "sddsconvert -recover -nowarnings %s", output[i].filename);
          system(commandBuffer);
        } else {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          SDDS_Bomb("unable to get data from existing file---try -append=recover (may truncate file)");
        }
      }
      /* compare columns of output file to ReadbackNames column in input file */
      if (!SDDS_InitializeInput(&original_page, output[i].filename)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        return (1);
      }
      ColumnNames = (char **)SDDS_GetColumnNames(&original_page, &NColumnNames);
      for (j = 0; j < input[i].n_variables; j++) {
        if (-1 == match_string(input[i].ReadbackNames[j], ColumnNames, NColumnNames, EXACT_MATCH)) {
          printf("ReadbackName %s doesn't match any columns in output file.\n", input[i].ReadbackNames[j]);
          return (1);
        }
      }
      for (j = 0; j < NColumnNames; j++) {
        if (-1 == match_string(ColumnNames[j], input[i].ReadbackNames, input[i].n_variables, EXACT_MATCH)) {
          if (-1 == match_string(ColumnNames[j], AuxiliaryColumnNames, NAuxiliaryColumnNames, EXACT_MATCH)) {
            printf("Column %s in output doesn't match any ReadbackName value "
                   "in input file.\n",
                   ColumnNames[j]);
            return (1);
          }
        }
      }
      SDDS_FreeStringArray(ColumnNames, NColumnNames);
      if ((SDDS_ReadPage(&original_page)) != 1) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to get data from existing file---try -append=recover");
      }
      if (!SDDS_GetParameter(&original_page, "StartHour", &StartHour) ||
          !SDDS_GetParameterAsDouble(&original_page, "StartDayOfMonth", &StartDay) ||
          !SDDS_GetParameter(&original_page, "StartTime", &StartTime))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      StartDay += StartHour / 24.0;
      if (appendToPage) {
        if (!SDDS_Terminate(&original_page)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          return (1);
        }
        if (!SDDS_InitializeAppendToPage(&output_page[i], output[i].filename, 1,
                                         &initialOutputRow[i])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          return (1);
        }
      } else {
        if (!SDDS_InitializeAppend(&output_page[i], output[i].filename) ||
            !SDDS_StartTable(&output_page[i], flushInterval) ||
            !SDDS_CopyParameters(&output_page[i], &original_page) ||
            !SDDS_Terminate(&original_page)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
          return (1);
        }
      }
      getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, &TimeStamp);
      if (SDDS_CHECK_OKAY == SDDS_CheckParameter(&output_page[i], "PageTimeStamp", NULL, SDDS_STRING, NULL)) {
        if (!SDDS_SetParameters(&output_page[i], SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "PageTimeStamp", TimeStamp, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    } else {
      /*start new file */
      if (verbose)
        fprintf(stdout, "Starting new file (output %ld) %s\n", i, output[i].filename);
      startMonitorFile(&output_page[i], output[i].filename, Precision, input[i].n_variables,
                       input[i].ReadbackNames, input[i].DeviceNames, input[i].ReadbackUnits, &StartTime,
                       &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                       &StartYear, &TimeStamp, input[i].DoublePrecision,
                       input[i].enumPV, input[i].enumPVs, flushInterval,
                       datastrobeTriggerPresent,
                       doDisconnect, minimal, highTimePrecision);
      RunStartTime = StartTime;
    }
  }
  if (verbose)
    fprintf(stdout, "Done starting files\n");

  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }

  for (i = 0; i < numFiles; i++) {
    if (!(output[i].ReadbackIndex = malloc(sizeof(*output[i].ReadbackIndex) * input[i].n_variables)))
      bomb("Memory allocation failure\n", NULL);
    for (j = 0; j < input[i].n_variables; j++) {
      if ((output[i].ReadbackIndex[j] = SDDS_GetColumnIndex(&output_page[i], input[i].ReadbackNames[j])) < 0) {
        fprintf(stderr, "Unable to retrieve index for column %s\n", input[i].ReadbackNames[j]);
        return (1);
      }
    }
  }

  if (generationsRowLimit)
    newFileCountdown = generationsRowLimit;
  else
    newFileCountdown = -1;

  LogStep = flushStep = 0;
  SampleTimeToExecute = getTimeInSecs() - SampleTimeInterval;
  generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
  outRow = 0;
  for (i = 0; i < numFiles; i++)
    outputRow[i] = initialOutputRow[i];

  if (datastrobeTriggerPresent) {
    fprintf(stdout, "setup trigger...\n");
    fflush(stdout);
    setupDatastrobeTriggerCallbacks(&datastrobeTrigger);
  }

  PingTime = getTimeInSecs() - 2;
  for (Step = 0; Step < NStep; Step++) {
    if (sigint) {
      return (1);
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
      /*
            strcpy( rcParam.message, "Running");
            rcParam.status = runControlLogMessage( rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
            if ( rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr,"Unable to write status message and alarm severity\n");
            return(1);
            }
          */
    }
#endif

    if (datastrobeTriggerPresent) {
      datastrobeTrigger.datastrobeMissed = 0;
      datastrobeTrigger.trigStep = Step;
    }
    ElapsedTime = getTimeInSecs() - StartTime;
    RunTime = getTimeInSecs() - RunStartTime;
    if (enforceTimeLimit && RunTime > TotalTime) {
      /* time's up */
      break;
    }

    if (singleShot) {
      /* optionally prompt, then wait for user instruction */
      if (singleShot != SS_NOPROMPT) {
        fputs("Type <cr> to read, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
      }
      fgets(answer, 256, stdin);
      if (answer[0] == 'q' || answer[0] == 'Q') {
        for (i = 0; i < numFiles; i++) {
          if (doDisconnect && !SDDS_ReconnectFile(&output_page[i]))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          if (!SDDS_UpdatePage(&output_page[i], FLUSH_TABLE) ||
              !SDDS_Terminate(&output_page[i]))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        return (0);
      }
    } else if (Step && !datastrobeTriggerPresent) {
      if ((SampleTimeToWait = SampleTimeInterval - (getTimeInSecs() - SampleTimeToExecute)) < 0) {
        if (SampleTimeToWait < (SampleTimeInterval * -2))
          SampleTimeToExecute = getTimeInSecs() - SampleTimeInterval;
        SampleTimeToWait = 0;
      }
      /* execute wait to give the desired time interval */
      if (SampleTimeToWait > 0) {
#ifdef USE_RUNCONTROL
        if ((rcParam.PV) && (SampleTimeToWait > 2)) {
          if (runControlPingWhileSleep(SampleTimeToWait)) {
            fprintf(stderr, "Problem pinging the run control record.\n");
            return (1);
          }
        } else {
          oag_ca_pend_event(SampleTimeToWait, &sigint);
        }
#else
        oag_ca_pend_event(SampleTimeToWait, &sigint);
#endif
      } else {
        ca_poll();
      }
      if (sigint)
        return (1);
    }
    if (datastrobeTriggerPresent) {
      ping = 0;
      while (!datastrobeTrigger.triggered) {
        if (ping == 20) {
          ping = 0;
#ifdef USE_RUNCONTROL
          if (rcParam.PV) {
            if (runControlPingWhileSleep(.1)) {
              fprintf(stderr, "Problem pinging the run control record.\n");
              return (1);
            }
          } else {
            oag_ca_pend_event(.1, &sigint);
          }
#else
          oag_ca_pend_event(.1, &sigint);
#endif
        } else {
          ping++;
          oag_ca_pend_event(.1, &sigint);
        }
        if (sigint)
          return (1);
      }
    }

    SampleTimeToExecute += SampleTimeInterval;
    LastHour = HourNow;
    LastDay = DayNow;
    makeTimeBreakdown(getTimeInSecs(), NULL, &DayNow, &HourNow, NULL, NULL, NULL, NULL);

    if ((dailyFiles && HourNow < LastHour) ||
        ((generationsTimeLimit > 0 || generationsRowLimit) && !newFileCountdown) ||
        (monthlyFiles && DayNow < LastDay)) {
      /* must start a new file */
      for (i = 0; i < numFiles; i++) {
#ifdef DEBUG
        fprintf(stdout, "Closing file %s\n", output[i].filename);
#endif
        if ((doDisconnect && !SDDS_ReconnectFile(&output_page[i])))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#ifdef DEBUG
        fprintf(stdout, "File reconnected\n");
#endif
        if (!SDDS_UpdatePage(&output_page[i], FLUSH_TABLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#ifdef DEBUG
        fprintf(stdout, "File updated\n");
#endif
        if (!SDDS_Terminate(&output_page[i]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
#ifdef DEBUG
        fprintf(stdout, "File terminated\n");
#endif
      }
      if (generations) {
        /* user-specified generation files */
        char *ptr1;
#ifdef DEBUG
        fprintf(stdout, "Making new generation filenames\n");
#endif
        if (generationsRowLimit)
          newFileCountdown = generationsRowLimit;
        else
          newFileCountdown = -1;
        generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
        for (i = 0; i < numFiles; i++) {
          ptr1 = output[i].filename;
          output[i].filename =
            MakeGenerationFilename(output[i].origOutputFile, generationsDigits,
                                   generationsDelimiter, ptr = output[i].filename);
          if (ptr1)
            free(ptr1);
        }
        if (verbose) {
          for (i = 0; i < numFiles; i++) {
            fprintf(stdout, "New file %s started\n", output[i].filename);
          }
        }
      } else if (dailyFiles) {
#ifdef DEBUG
        fprintf(stdout, "Making new daily filenames\n");
#endif
        /* "daily" log files with the OAG log name format */
        if (generationsRowLimit)
          newFileCountdown = generationsRowLimit;
        else
          newFileCountdown = -1;
        generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
        for (i = 0; i < numFiles; i++) {
          if (output[i].filename)
            free(output[i].filename);
          output[i].filename =
            MakeDailyGenerationFilename(output[i].origOutputFile,
                                        generationsDigits, generationsDelimiter, timetag);
        }
        if (dailyFilesVerbose)
          for (i = 0; i < numFiles; i++)
            fprintf(stdout, "New file %s started\n", output[i].filename);
      } else if (monthlyFiles) {
#ifdef DEBUG
        fprintf(stdout, "Making new monthly filenames\n");
#endif
        /* "monthly" log files with the OAG log name format */
        if (generationsRowLimit)
          newFileCountdown = generationsRowLimit;
        else
          newFileCountdown = -1;
        generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
        for (i = 0; i < numFiles; i++) {
          if (output[i].filename)
            free(output[i].filename);
          output[i].filename =
            MakeMonthlyGenerationFilename(output[i].origOutputFile,
                                          generationsDigits, generationsDelimiter, timetag);
        }
        if (monthlyFilesVerbose)
          for (i = 0; i < numFiles; i++)
            fprintf(stdout, "New file %s started\n", output[i].filename);
      }
      flushStep = 0;
      currentFile = 0;
      for (i = 0; i < numFiles; i++) {
        outputRow[i] = 0;
#ifdef DEBUG
        fprintf(stdout, "Starting file %s\n", output[i].filename);
#endif
        startMonitorFile(&output_page[i], output[i].filename, Precision, input[i].n_variables,
                         input[i].ReadbackNames, input[i].DeviceNames, input[i].ReadbackUnits, &StartTime,
                         &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                         &StartYear, &TimeStamp, input[i].DoublePrecision,
                         input[i].enumPV, input[i].enumPVs, flushInterval,
                         datastrobeTriggerPresent, doDisconnect, minimal, highTimePrecision);
      }
    }
    if (inhibit &&
        QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (CondMode & TOUCH_OUTPUT) {
        for (i = 0; i < numFiles; i++) {
          TouchFile(output[i].filename);
        }
      }
      if (verbose) {
        printf("Inhibit PV %s is nonzero. Data collection inhibited.\n", InhibitPV.name);
        fflush(stdout);
      }
      if (CondMode & RETAKE_STEP)
        Step--;
      if (datastrobeTriggerPresent) {
        datastrobeTrigger.datalogged = 1;
        datastrobeTrigger.triggered = 0;
        datastrobeTrigger.datastrobeMissed = 0;
      }
      if (InhibitWaitTime > 0) {
#ifdef USE_RUNCONTROL
        if ((rcParam.PV) && (InhibitWaitTime > 2)) {
          if (runControlPingWhileSleep(InhibitWaitTime)) {
            fprintf(stderr, "Problem pinging the run control record.\n");
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
    if (CondFile &&
        !PassesConditions(CondDeviceName, CondReadMessage, CondScaleFactor,
                          CondDataBuffer, CondLowerLimit, CondUpperLimit,
                          CondHoldoff, conditions, CondMode, CondCHID, pendIOtime)) {
      if (CondMode & TOUCH_OUTPUT) {
        for (i = 0; i < numFiles; i++) {
          TouchFile(output[i].filename);
        }
      }

      if (verbose) {
        printf("Step %ld---values failed condition test--continuing\n", Step);
        fflush(stdout);
      }
      if (!Step && !datastrobeTriggerPresent) {
        /* execute wait to give the desired time interval */
        if (SampleTimeInterval > 0) {
#ifdef USE_RUNCONTROL
          if (rcParam.PV) {
            if (runControlPingWhileSleep(SampleTimeInterval)) {
              fprintf(stderr, "Problem pinging the run control record.\n");
              return (1);
            }
          } else {
            oag_ca_pend_event(SampleTimeInterval, &sigint);
          }
#else
          oag_ca_pend_event(SampleTimeInterval, &sigint);
#endif
        } else {
          ca_poll();
        }
        if (sigint)
          return (1);
      }
      if (datastrobeTriggerPresent) {
        ping = 0;
        while (!datastrobeTrigger.triggered) {
          if (ping == 20) {
            ping = 0;
#ifdef USE_RUNCONTROL
            if (rcParam.PV) {
              if (runControlPingWhileSleep(.1)) {
                fprintf(stderr, "Problem pinging the run control record.\n");
                return (1);
              }
            } else {
              oag_ca_pend_event(.1, &sigint);
            }
#else
            oag_ca_pend_event(.1, &sigint);
#endif
          } else {
            ping++;
            oag_ca_pend_event(.1, &sigint);
          }
          if (sigint)
            return (1);
        }
      }

      if (CondMode & RETAKE_STEP)
        Step--;
      if (datastrobeTriggerPresent) {
        datastrobeTrigger.datalogged = 1;
        datastrobeTrigger.triggered = 0;
        datastrobeTrigger.datastrobeMissed = 0;
      }
      continue;
    }

    if ((CAerrors = ReadScalarValuesModified(input, numFiles, factor, pendIOtime)) != 0) {
      if (onerrorindex == ONERROR_USEZERO) {
      } else if (onerrorindex == ONERROR_SKIPROW) {
        for (i = 0; i < numFiles; i++) {
          for (j = 0; j < input[i].n_variables; j++) {
            input[i].averageValues[j] = 0;
          }
        }
        LogStep = 0;
        if (datastrobeTriggerPresent) {
          datastrobeTrigger.datalogged = 1;
          datastrobeTrigger.triggered = 0;
          datastrobeTrigger.datastrobeMissed = 0;
        }
        continue;
      } else if (onerrorindex == ONERROR_EXIT) {
        return (1);
      }
    }

    for (i = 0; i < numFiles; i++) {
      if (outRow && input[i].DeadBands &&
          !OutsideDeadBands(input[i].values, input[i].baselineValues, input[i].DeadBands, input[i].n_variables)) {
        if (verbose) {
          printf("Step %ld---changes not outside deadbands---continuing\n", Step);
          fflush(stdout);
          break;
        }
      }
    }
    if (i != numFiles) {
      for (i = 0; i < numFiles; i++) {
        for (j = 0; j < input[i].n_variables; j++) {
          input[i].averageValues[j] = 0;
        }
      }
      LogStep = 0;
      if (datastrobeTriggerPresent) {
        datastrobeTrigger.datalogged = 1;
        datastrobeTrigger.triggered = 0;
        datastrobeTrigger.datastrobeMissed = 0;
      }
      continue;
    }

    /*Don't know why this would ever be true but it seems to happen*/
    /*
        j = outputRow[0] - (&output_page[0])->first_row_in_mem - (&output_page[0])->n_rows_allocated + 1;
        if (j >= 1) {
        for (i = 0; i < numFiles; i++) {
        if (!SDDS_LengthenTable(&output_page[i], j)) {
        SDDS_PrintErrors(stderr,SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
        }
        }
        allocatedRows += j;
        fprintf(stderr, "%lf - lengthening allocated SDDS tables to %ld rows (shouldn't ever get here)\n", getTimeInSecs(), allocatedRows);
        }
      */

    if (highTimePrecision) {
      if (datastrobeTriggerPresent)
        LEpochTime = datastrobeTrigger.currentValue; /*This is a double value because EPICS does not support long doubles */
      else
        LEpochTime = getLongDoubleTimeInSecs();
      ElapsedTime = LEpochTime - StartTime;
      RunTime = LEpochTime - RunStartTime;
    } else {
      if (datastrobeTriggerPresent)
        EpochTime = datastrobeTrigger.currentValue;
      else
        EpochTime = getTimeInSecs();
      ElapsedTime = EpochTime - StartTime;
      RunTime = EpochTime - RunStartTime;
    }
    if (enforceTimeLimit && RunTime > TotalTime) {
      /* terminate after this pass through loop */
      NStep = Step;
    }
    TimeOfDay = StartHour + ElapsedTime / 3600.0;
    DayOfMonth = StartDay + ElapsedTime / 86400.0;
    if (verbose) {
      printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
      fflush(stdout);
    }
    LogStep++;
    if (LogStep != LogInterval) {
      continue;
    }
    if (datastrobeTriggerPresent) {
      if (datastrobeTrigger.datastrobeMissed) {
        fprintf(stderr, "%lf - data strobe missed = %ld\n", getTimeInSecs(), datastrobeTrigger.datastrobeMissed);
      }
    }

    for (i = 0; i < numFiles; i++) {

      if (highTimePrecision) {
        if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                               "Time", LEpochTime, NULL)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          errorFound = 1;
        }
      } else {
        if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                               "Time", EpochTime, NULL)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          errorFound = 1;
        }
      }

      if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                             "CAerrors", input[i].CAerrors, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        errorFound = 1;
      }
      if (!minimal) {
        if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                               "Step", Step,
                               "TimeOfDay", (float)TimeOfDay,
                               "DayOfMonth", (float)DayOfMonth, NULL)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          errorFound = 1;
        }
      }
      if (datastrobeTriggerPresent && (!minimal)) {
        if (highTimePrecision) {
          if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                                 "TimeWS", getLongDoubleTimeInSecs(),
                                 NULL)) {
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            errorFound = 1;
          }
        } else {
          if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                                 "TimeWS", getTimeInSecs(),
                                 NULL)) {
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            errorFound = 1;
          }
        }
        if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow[i],
                               "MissedStrobes", datastrobeTrigger.datastrobeMissed,
                               NULL)) {
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          errorFound = 1;
        }
      }

      if (!input[i].DoublePrecision) {
        switch (Precision) {
        case PRECISION_SINGLE:
          for (j = 0; j < input[i].n_variables; j++) {
            if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow[i],
                                   output[i].ReadbackIndex[j], (float)input[i].averageValues[j], -1)) {
              fprintf(stderr, "problem setting column %ld, row %ld\n", output[i].ReadbackIndex[j], outputRow[i]);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              errorFound = 1;
            }
          }
          break;
        case PRECISION_DOUBLE:
          for (j = 0; j < input[i].n_variables; j++) {
            if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow[i],
                                   output[i].ReadbackIndex[j], input[i].averageValues[j], -1)) {
              fprintf(stderr, "problem setting column %ld, row %ld\n", output[i].ReadbackIndex[j], outputRow[i]);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              errorFound = 1;
            }
          }
          break;
        default:
          bomb("Unknown precision encountered.\n", NULL);
        }
      } else {
        for (j = 0; j < input[i].n_variables; j++) {
          if (input[i].DoublePrecision[j] == 0) {
            if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow[i],
                                   output[i].ReadbackIndex[j], (float)input[i].averageValues[j], -1)) {
              fprintf(stderr, "problem setting column %ld, row %ld\n", output[i].ReadbackIndex[j], outputRow[i]);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              errorFound = 1;
            }
          } else {
            if (!SDDS_SetRowValues(&output_page[i], SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow[i],
                                   output[i].ReadbackIndex[j], input[i].averageValues[j], -1)) {
              fprintf(stderr, "problem setting column %ld, row %ld\n", output[i].ReadbackIndex[j], outputRow[i]);
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              errorFound = 1;
            }
          }
        }
      }
      if (errorFound) {
        SDDS_Bomb("Aborting (Unable to set column)");
      }
      if (input[i].DeadBands)
        /* use the last values as the new set of baseline values */
        SWAP_PTR(input[i].averageValues, input[i].baselineValues);
    }
    outRow++;
    for (i = 0; i < numFiles; i++)
      outputRow[i]++;
    --newFileCountdown;
    if (generationsTimeLimit > 0 && getTimeInSecs() > generationsTimeStop)
      newFileCountdown = 0;
    flushStep++;
    if (datastrobeTriggerPresent) {
      datastrobeTrigger.datalogged = 1;
      datastrobeTrigger.triggered = 0;
      datastrobeTrigger.datastrobeMissed = 0;
    }
    if (flushInterval == 1 || flushStep >= flushInterval) {
      t1 = getTimeInSecs();
      if (verbose) {
        printf("Flushing data at %lf\n", t1);
        fflush(stdout);
      }
      for (i = currentFile; i < numFiles; i++) {
        if (doDisconnect && !SDDS_ReconnectFile(&output_page[i]))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (!SDDS_UpdatePage(&output_page[i], FLUSH_TABLE))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (doDisconnect && !SDDS_DisconnectFile(&output_page[i]))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

        /* Initially I thought it would be good to check if the data strobe tiggered
                 while we were writting to the file system. The idea would be to then stop
                 writting the data and lengthen the SDDS tables. Then next time through the
                 loop we would then finish writting the SDDS files. One problem with this
                 method is that we would just continue to allocate memory if there was a problem
                 with the file system. So instead I have opted to ignore the data strobe
                 until all the files have been written. This will mean we don't have to
                 allocate any more memory. The downside is that it will log the data with an
                 an offset to the data strobe and it may miss a strobe entirely.
                 if (datastrobeTrigger.triggered) {
                 fprintf(stderr, "%lf - data strobe detected while writting files\n", getTimeInSecs());
                 break;
                 }
              */
      }
      if (datastrobeTrigger.triggered) {
        t2 = getTimeInSecs();
        fprintf(stderr, "Data strobe detected while writting files (%lf). Gap time = %lf\n", t2, t2 - t1);
      }
      /*
            if (i+1 >= numFiles) {
          */
      flushStep = 0;
      currentFile = 0;
      /*
            } else {
            currentFile = i+1;
            if (flushStep == allocatedRows) {
            for (i = 0; i < numFiles; i++) {
            if (!SDDS_LengthenTable(&output_page[i], 1)) {
            SDDS_PrintErrors(stderr,SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
            }
            }
            allocatedRows++;
            fprintf(stderr, "%lf - lengthening allocated SDDS tables to %ld rows\n", getTimeInSecs(), allocatedRows);
            }
            }
          */
    }
    for (i = 0; i < numFiles; i++) {
      for (j = 0; j < input[i].n_variables; j++) {
        input[i].averageValues[j] = 0;
      }
    }
    LogStep = 0;

    if (watchInput) {
      filestat_failed = 0;
      if (onePvOutputDirectory) {
        if (file_is_modified(input[0].filename, &input[0].lastlink, &input[0].filestat)) {
          if (verbose)
            fprintf(stderr, "The input file has been modified, exit!\n");
          filestat_failed = 1;
          break;
        }
      } else {
        for (i = 0; i < numFiles; i++) {
          if (file_is_modified(input[i].filename, &input[i].lastlink, &input[i].filestat)) {
            if (verbose)
              fprintf(stderr, "The input file has been modified, exit!\n");
            filestat_failed = 1;
            break;
          }
        }
      }
      if (filestat_failed)
        break;
    }
  }

  for (i = 0; i < numFiles; i++) {
    if ((doDisconnect && !SDDS_ReconnectFile(&output_page[i])) ||
        !SDDS_UpdatePage(&output_page[i], FLUSH_TABLE) ||
        !SDDS_Terminate(&output_page[i]))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    strcpy(rcParam.message, "Application completed normally.");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error exiting run control.\n");
      return (1);
    default:
      fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
      return (1);
    }
  }
#endif

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
  for (i = 0; i < numFiles; i++)
    if (input[i].lastlink != input[i].filename)
      free(input[i].lastlink);

  free(input_page);
  free(output_page);
  FreeInputFiles(input, numFiles);
  FreeOutputFiles(output, numFiles);
  free_scanargs(&s_arg, argc);
  if (outputRow)
    free(outputRow);
  outputRow = NULL;
  if (initialOutputRow)
    free(initialOutputRow);
  return 0;
}

void FreeOutputFiles(OUTPUTFILENAME *output, long numFiles) {
  long i;
  if (output) {
    for (i = 0; i < numFiles; i++) {
      free(output[i].filename);
      free(output[i].ReadbackIndex);
    }
    free(output);
  }
}

void FreeInputFiles(INPUTFILENAME *input, long numFiles) {
  long i, j, k;
  for (i = 0; i < numFiles; i++) {
    for (j = 0; j < input[i].n_variables; j++) {
      if (input[i].DeviceNames[j])
        free(input[i].DeviceNames[j]);
      input[i].DeviceNames[j] = NULL;
      if (input[i].ReadbackNames[j])
        free(input[i].ReadbackNames[j]);
      if (input[i].ReadMessages && input[i].ReadMessages[j])
        free(input[i].ReadMessages[j]);
      if (input[i].ReadbackUnits && input[i].ReadbackUnits[j])
        free(input[i].ReadbackUnits[j]);
    }
    if (input[i].DeviceNames)
      free(input[i].DeviceNames);
    input[i].DeviceNames = NULL;
    if (input[i].ReadbackNames)
      free(input[i].ReadbackNames);
    input[i].ReadbackNames = NULL;
    if (input[i].ReadbackUnits)
      free(input[i].ReadbackUnits);
    if (input[i].ReadMessages)
      free(input[i].ReadMessages);

    if (input[i].ScaleFactors)
      free(input[i].ScaleFactors);
    if (input[i].Average)
      free(input[i].Average);
    if (input[i].DoublePrecision)
      free(input[i].DoublePrecision);
    if (input[i].DeadBands)
      free(input[i].DeadBands);
    if (input[i].values)
      free(input[i].values);
    if (input[i].baselineValues)
      free(input[i].baselineValues);
    if (input[i].averageValues)
      free(input[i].averageValues);
    if (input[i].channelID)
      free(input[i].channelID);
    if (input[i].connectedInTime)
      free(input[i].connectedInTime);
    for (j = 0; j < input[i].enumPVs; j++) {
      for (k = 0; k < input[i].enumPV[j].no_str; k++)
        free(input[i].enumPV[j].strs[k]);
      if (input[i].enumPV[j].strs)
        free(input[i].enumPV[j].strs);
      if (input[i].enumPV[j].PVname)
        free(input[i].enumPV[j].PVname);
    }
    if (input[i].enumPV)
      free(input[i].enumPV);
  }
  free(input);
}

long ReadScalarValuesModified(INPUTFILENAME *input, long numFiles, double factor, double pendIOtime) {
  long i, j, caErrors;

  caErrors = 0;
  for (i = 0; i < numFiles; i++) {
    if (!input[i].n_variables) {
      fprintf(stderr, "Error: no process variables in %s \n", input[i].filename);
      exit(1);
    }
    if (!input[i].values)
      bomb("Error: null pointer passed to ReadScalarValuesModified()\n", NULL);
    input[i].CAerrors = 0;
  }

  for (j = 0; j < numFiles; j++) {
    for (i = 0; i < input[j].n_variables; i++) {
      if (!(input[j].connectedInTime[i] = ca_state(input[j].channelID[i]) == cs_conn)) {
        input[j].values[i] = 0;
        input[j].CAerrors++;
        caErrors = 1;
        continue;
      }
      if (ca_get(DBR_DOUBLE, input[j].channelID[i], &input[j].values[i]) != ECA_NORMAL) {
        input[j].values[i] = 0;
        input[j].CAerrors++;
        caErrors = 1;
        continue;
      }
    }
  }
  ca_pend_io(pendIOtime);

  for (j = 0; j < numFiles; j++) {
    if (input[j].ScaleFactors) {
      for (i = 0; i < input[j].n_variables; i++) {
        input[j].values[i] *= input[j].ScaleFactors[i];
      }
    }
    if (!input[j].Average) {
      for (i = 0; i < input[j].n_variables; i++) {
        input[j].averageValues[i] += (input[j].values[i] * factor);
      }
    } else {
      for (i = 0; i < input[j].n_variables; i++) {
        if (input[j].Average[i] == 0) {
          input[j].averageValues[i] = input[j].values[i];
        } else {
          input[j].averageValues[i] += (input[j].values[i] * factor);
        }
      }
    }
  }
  return caErrors;
}

long getScalarMonitorDataLocal(char ***DeviceName, char ***ReadMessage,
                               char ***ReadbackName, char ***ReadbackUnits,
                               double **scaleFactor, double **DeadBands,
                               long *variables, char *inputFile,
                               int32_t **Average, int32_t **DoublePrecision) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName = NULL, *MessageColumnName = NULL, *ReadbackUnitsColumnName = NULL;
  short UnitsDefined, MessageDefined, ScaleFactorDefined, DeadBandDefined, AverageDefined, PrecisionDefined;
  long i, noReadbackName;

  *DeviceName = *ReadMessage = *ReadbackName = *ReadbackUnits = NULL;
  *Average = *DoublePrecision = NULL;
  *scaleFactor = NULL;
  if (DeadBands)
    *DeadBands = NULL;
  *variables = 0;
  DeadBandDefined = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }
  noReadbackName = 0;
  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)))
    noReadbackName = 1;

  MessageDefined = 1;
  if (!(MessageColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ReadMessage", "Message", "ReadMsg", NULL))) {
    MessageDefined = 0;
  }

  UnitsDefined = 0;
  if (!(ReadbackUnitsColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ReadbackUnits", "ReadbackUnit", "Units", NULL))) {
    fprintf(stderr, "Warning: ReadbackUnits and Units columns both missing or not string type\n");
  }
  switch (SDDS_CheckColumn(&inSet, ReadbackUnitsColumnName, NULL, SDDS_STRING, NULL)) {
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

  PrecisionDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "DoublePrecision", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    PrecisionDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"DoublePrecision\".\n");
    exit(1);
    break;
  }

  ScaleFactorDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
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
  AverageDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "Average", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    AverageDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"Average\".\n");
    exit(1);
    break;
  }

  if (DeadBands) {
    DeadBandDefined = 0;
    switch (SDDS_CheckColumn(&inSet, "DeadBand", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      DeadBandDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"DeadBand\".\n");
      exit(1);
      break;
    }
  }

  if (0 >= SDDS_ReadPage(&inSet)) {
    if (ControlNameColumnName)
      free(ControlNameColumnName);
    if (MessageColumnName)
      free(MessageColumnName);
    return 0;
  }

  if (!(*variables = SDDS_CountRowsOfInterest(&inSet)))
    bomb("Zero rows defined in input file.\n", NULL);

  *DeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (noReadbackName)
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  else
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, "ReadbackName");

  if (MessageDefined)
    *ReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
  else {
    *ReadMessage = (char **)malloc(sizeof(char *) * (*variables));
    for (i = 0; i < *variables; i++) {
      (*ReadMessage)[i] = (char *)malloc(sizeof(char));
      (*ReadMessage)[i][0] = 0;
    }
  }

  if (UnitsDefined)
    *ReadbackUnits = (char **)SDDS_GetColumn(&inSet, ReadbackUnitsColumnName);
  else {
    *ReadbackUnits = (char **)malloc(sizeof(char *) * (*variables));
    for (i = 0; i < *variables; i++) {
      (*ReadbackUnits)[i] = (char *)malloc(sizeof(char) * (9 + 1));
      (*ReadbackUnits)[i][0] = 0;
    }
  }

  if (ScaleFactorDefined &&
      !(*scaleFactor = SDDS_GetColumnInDoubles(&inSet, "ScaleFactor")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (AverageDefined &&
      !(*Average = SDDS_GetColumnInLong(&inSet, "Average")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (PrecisionDefined &&
      !(*DoublePrecision = SDDS_GetColumnInLong(&inSet, "DoublePrecision")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (DeadBands && DeadBandDefined &&
      !(*DeadBands = SDDS_GetColumnInDoubles(&inSet, "DeadBand")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_ReadPage(&inSet) > 0)
    fprintf(stderr, "Warning: extra pages in input file %s---they are ignored\n", inputFile);

  SDDS_Terminate(&inSet);
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  if (MessageColumnName)
    free(MessageColumnName);
  return 1;
}

void startMonitorFile(SDDS_DATASET *output_page, char *outputfile, long Precision,
                      long n_variables, char **ReadbackNames, char **DeviceNames, char **ReadbackUnits,
                      double *StartTime, double *StartDay, double *StartHour,
                      double *StartJulianDay, double *StartMonth, double *StartYear,
                      char **TimeStamp, int32_t *DoublePrecision, ENUM_PV *enumPV, long enumPVs,
                      long flushInterval, long datastrobeUsed, long disconnect, long minimal, short highTimePrecision) {
  long i;
  int size = SDDS_DOUBLE;
  if (highTimePrecision) {
    size = SDDS_LONGDOUBLE;
  }

  getTimeBreakdown(StartTime, StartDay, StartHour, StartJulianDay, StartMonth,
                   StartYear, TimeStamp);
  SDDS_ClearErrors();
  if (!SDDS_InitializeOutput(output_page, SDDS_BINARY, 1, NULL, NULL, outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    fprintf(stderr, "error %s", strerror(errno));
    exit(1);
  }

  if (!DefineLoggingTimeParameters(output_page))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (minimal) {
    if (0 > SDDS_DefineColumn(output_page, "CAerrors", NULL, NULL, "Channel access errors for this row", NULL, SDDS_LONG, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (0 > SDDS_DefineColumn(output_page, "Time", NULL, "s", "Time since start of epoch", NULL, size, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else if (highTimePrecision) {
    if (!DefineLoggingTimeDetail(output_page, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS | TIMEDETAIL_LONGDOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!DefineLoggingTimeDetail(output_page, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (datastrobeUsed && (!minimal) &&
      (0 > SDDS_DefineColumn(output_page, "TimeWS", NULL, "s", "Time since start of epoch according to the workstation", NULL, size, 0) ||
       0 > SDDS_DefineColumn(output_page, "MissedStrobes", NULL, NULL, "Number of missed strobes", NULL, SDDS_LONG, 0)))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_EnableFSync(output_page);
  if (!DoublePrecision) {
    switch (Precision) {
    case PRECISION_SINGLE:
      for (i = 0; i < n_variables; i++) {
        if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_FLOAT, 0) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      break;
    case PRECISION_DOUBLE:
      for (i = 0; i < n_variables; i++) {
        if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_DOUBLE, 0) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      break;
    default:
      bomb("Unknown precision encountered.\n", NULL);
    }
  } else {
    for (i = 0; i < n_variables; i++) {
      if (DoublePrecision[i] == 0) {
        if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_FLOAT, 0) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_DOUBLE, 0) < 0)
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  for (i = 0; i < enumPVs; i++) {
    if (SDDS_DefineArray(output_page, enumPV[i].PVname, NULL, NULL, NULL, NULL, SDDS_STRING, 0, 1, NULL) < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_DefineSimpleParameters(output_page, 0, NULL, NULL, SDDS_STRING) ||
      !SDDS_SetRowCountMode(output_page, SDDS_FIXEDROWCOUNT) ||
      !SDDS_WriteLayout(output_page) ||
      !SDDS_StartTable(output_page, flushInterval))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SetParameters(output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "TimeStamp", *TimeStamp, "PageTimeStamp", *TimeStamp,
                          "StartTime", *StartTime, "StartYear", (short)*StartYear,
                          "YearStartTime", computeYearStartTime(*StartTime),
                          "StartJulianDay", (short)*StartJulianDay, "StartMonth", (short)*StartMonth,
                          "StartDayOfMonth", (short)*StartDay, "StartHour", (float)*StartHour,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < enumPVs; i++) {
    if (!SDDS_SetArrayVararg(output_page, enumPV[i].PVname,
                             SDDS_POINTER_ARRAY, enumPV[i].strs, enumPV[i].no_str))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (disconnect && !SDDS_DisconnectFile(output_page))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void getStringValuesOfEnumPV(struct event_handler_args event) {
  int i;
  struct dbr_gr_enum *pvalue = (struct dbr_gr_enum *)(event.dbr);
  ENUM_PV *enumPV;
  enumPV = (ENUM_PV *)(event.usr);
  enumPV->no_str = pvalue->no_str;
  enumPV->strs = malloc(sizeof(char *) * (enumPV->no_str));
  for (i = 0; i < enumPV->no_str; i++)
    SDDS_CopyString(&enumPV->strs[i], pvalue->strs[i]);
  enumPV->flag = 1;
}

long CheckEnumCACallbackStatus(ENUM_PV *enumPV, double pendIOtime) {
  long ntries; /*try 10 times*/

  ca_poll();
  ntries = (long)(pendIOtime * 100);
  while (ntries) {
    if (!enumPV->flag) {
      ca_poll();
      usleepSystemIndependent(1000);
    } else {
      return 0;
    }
    ntries--;
  }
  if (enumPV->flag)
    return 0;
  fprintf(stderr, "callback failed for enumeric PV %s\n", enumPV->PVname);
  return 1;
}

void datastrobeTriggerEventHandler(struct event_handler_args event) {
  if (event.status != ECA_NORMAL) {
    fprintf(stderr, "Error received on data strobe PV\n");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Received callback on data strobe PV\n");
#endif
  if (datastrobeTrigger.initialized == 0) {
    /* the first callback is just the connection event */
    datastrobeTrigger.initialized = 1;
    datastrobeTrigger.datalogged = 1;
    return;
  }
  datastrobeTrigger.currentValue = *((double *)event.dbr);
#ifdef DEBUG
  fprintf(stderr, "chid=%d, current=%f\n", (int)datastrobeTrigger.channelID, datastrobeTrigger.currentValue);
#endif
  datastrobeTrigger.triggered = 1;
  if (!datastrobeTrigger.datalogged)
    datastrobeTrigger.datastrobeMissed++;
  datastrobeTrigger.datalogged = 0;
  return;
}

long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger) {
  datastrobeTrigger->trigStep = -1;
  datastrobeTrigger->currentValue = 0;
  datastrobeTrigger->triggered = 0;
  datastrobeTrigger->datastrobeMissed = 0;
  datastrobeTrigger->usrValue = 1;
  datastrobeTrigger->datalogged = 0;
  datastrobeTrigger->initialized = 0;
  if (ca_search(datastrobeTrigger->PV, &datastrobeTrigger->channelID) != ECA_NORMAL) {
    fprintf(stderr, "error: search failed for control name %s\n", datastrobeTrigger->PV);
    return 0;
  }
  ca_poll();
  if (ca_add_masked_array_event(DBR_DOUBLE, 1, datastrobeTrigger->channelID,
                                datastrobeTriggerEventHandler,
                                (void *)&datastrobeTrigger, (ca_real)0, (ca_real)0,
                                (ca_real)0, NULL, DBE_VALUE) != ECA_NORMAL) {
    fprintf(stderr, "error: unable to setup datastrobe callback for control name %s\n",
            datastrobeTrigger->PV);
    exit(1);
  }
#ifdef DEBUG
  fprintf(stderr, "Set up callback on data strobe PV\n");
#endif
  ca_poll();
  return 1;
}

#if !defined(_WIN32)
long setUpOnePvPerFileOutput(INPUTFILENAME **input, OUTPUTFILENAME **output, long numFiles,
                             char *onePvOutputDirectory) {
  INPUTFILENAME input1;
  long i;
  char buffer[1024];

  input1.filename = (*input)[0].filename;
  (*input)[0].filename = NULL;
  /* Read the file to get the PV names */
  if (!getScalarMonitorDataLocal(&input1.DeviceNames, &input1.ReadMessages,
                                 &input1.ReadbackNames, &input1.ReadbackUnits,
                                 &input1.ScaleFactors, &input1.DeadBands,
                                 &input1.n_variables, input1.filename,
                                 &input1.Average, &input1.DoublePrecision))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!input1.n_variables) {
    fprintf(stderr, "error: no data in input file %s \n", input1.filename);
    exit(1);
  }
  numFiles = input1.n_variables;

  input1.lastlink = read_last_link_to_file(input1.filename);
  if (input1.lastlink)
    fprintf(stderr, "link-to file is %s\n", input1.lastlink);
  if (get_file_stat(input1.filename, input1.lastlink, &input1.filestat) != 0) {
    fprintf(stderr, "Problem getting modification time for %s\n", input1.filename);
    exit(1);
  }

  if (!(*input = SDDS_Realloc((*input), sizeof(**input) * input1.n_variables)) ||
      !((*output) = SDDS_Realloc((*output), sizeof(*(*output)) * input1.n_variables)))
    SDDS_Bomb("memory allocation failure");
  for (i = 0; i < input1.n_variables; i++) {
    /* Create entries in input[i] and output[i] arrays.  All input[i] arrays are the same filename
       * while all output[i] arrays are the file rootnames (<dirname>/<pvName>/log)
       */
    (*input)[i].n_variables = 1;
    if (!((*input)[i].DeviceNames = malloc(sizeof(*((*input)[i].DeviceNames)))))
      SDDS_Bomb("memory allocation failure");
    (*input)[i].DeviceNames[0] = input1.DeviceNames[i];

    (*input)[i].ReadMessages = NULL;
    if (input1.ReadMessages) {
      if (!((*input)[i].ReadMessages = malloc(sizeof(*((*input)[i].ReadMessages)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].ReadMessages[0] = input1.ReadMessages[i];
    }

    (*input)[i].ReadbackNames = NULL;
    if (input1.ReadbackNames) {
      if (!((*input)[i].ReadbackNames = malloc(sizeof(*((*input)[i].ReadbackNames)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].ReadbackNames[0] = input1.ReadbackNames[i];
    }

    (*input)[i].ReadbackUnits = NULL;
    if (input1.ReadbackUnits) {
      if (!((*input)[i].ReadbackUnits = malloc(sizeof(*((*input)[i].ReadbackUnits)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].ReadbackUnits[0] = input1.ReadbackUnits[i];
    }

    (*input)[i].ScaleFactors = NULL;
    if (input1.ScaleFactors) {
      if (!((*input)[i].ScaleFactors = malloc(sizeof(*((*input)[i].ScaleFactors)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].ScaleFactors[0] = input1.ScaleFactors[i];
    }

    (*input)[i].DeadBands = NULL;
    if (input1.DeadBands) {
      if (!((*input)[i].DeadBands = malloc(sizeof(*((*input)[i].DeadBands)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].DeadBands[0] = input1.DeadBands[i];
    }

    (*input)[i].Average = NULL;
    if (input1.Average) {
      if (!((*input)[i].Average = malloc(sizeof(*((*input)[i].Average)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].Average[0] = input1.Average[i];
    }

    (*input)[i].DoublePrecision = NULL;
    if (input1.DoublePrecision) {
      if (!((*input)[i].DoublePrecision = malloc(sizeof(*((*input)[i].DoublePrecision)))))
        SDDS_Bomb("memory allocation failure");
      (*input)[i].DoublePrecision[0] = input1.DoublePrecision[i];
    }

    (*input)[i].lastlink = NULL;
    if (!SDDS_CopyString(&(*input)[i].filename, input1.filename) ||
        (input1.lastlink && !SDDS_CopyString(&((*input)[i].lastlink), input1.lastlink)))
      SDDS_Bomb("memory allocation failure");
    if (input1.ReadbackNames) {
      if (!((*output)[i].origOutputFile =
              malloc(sizeof(*((*output)[i].filename)) * (strlen(input1.ReadbackNames[i]) +
                                                         strlen(onePvOutputDirectory) + strlen("//log") + 1))))
        SDDS_Bomb("memory allocation failure");
    } else {
      if (!((*output)[i].origOutputFile =
              malloc(sizeof(*((*output)[i].filename)) * (strlen(input1.DeviceNames[i]) +
                                                         strlen(onePvOutputDirectory) + strlen("//log") + 1))))
        SDDS_Bomb("memory allocation failure");
    }
  }
  for (i = 0; i < input1.n_variables; i++) {
    if (input1.ReadbackNames) {
      replace_string(buffer, input1.ReadbackNames[i], "/", "+");
    } else {
      replace_string(buffer, input1.DeviceNames[i], "/", "+");
    }
    sprintf((*output)[i].origOutputFile, "%s/%s", onePvOutputDirectory, buffer);
    if (access((*output)[i].origOutputFile, F_OK) != 0) {
      mode_t mode;
      mode = umask(0);
      umask(mode);
      mode = (0777 & ~mode) | S_IRUSR | S_IWUSR | S_IXUSR;
      if (mkdir((*output)[i].origOutputFile, mode) != 0) {
        fprintf(stderr, "Unable to create directory %s\n", (*output)[i].origOutputFile);
        exit(1);
      }
    }
    sprintf((*output)[i].origOutputFile, "%s/%s/log", onePvOutputDirectory, buffer);
  }
  free(input1.DeviceNames);
  if (input1.ReadMessages)
    free(input1.ReadMessages);
  if (input1.ReadbackNames)
    free(input1.ReadbackNames);
  if (input1.ReadbackUnits)
    free(input1.ReadbackUnits);
  if (input1.ScaleFactors)
    free(input1.ScaleFactors);
  if (input1.DeadBands)
    free(input1.DeadBands);
  if (input1.Average)
    free(input1.Average);
  if (input1.DoublePrecision)
    free(input1.DoublePrecision);

  return numFiles;
}
#endif

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
