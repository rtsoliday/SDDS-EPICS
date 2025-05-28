/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: BPSmonitor.c
 * purpose: monitor booster power supplies
 *   Produces an SDDS output file containing data for
 *   the following for each power supply: AFG, Vo, Io
 *
 * M. Borland, 1995
 $Log: not supported by cvs2svn $
 Revision 1.4  2005/11/08 22:05:02  soliday
 Added support for 64 bit compilers.

 Revision 1.3  2004/07/19 17:39:35  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/15 21:22:45  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base 3.14.x
 has an inactivity timer that was causing disconnects from PVs when the
 log interval time was too large.

 Revision 1.1  2003/08/27 19:52:27  soliday
 Moved to subdirectory.

 Revision 1.15  2002/10/31 15:47:23  soliday
 It now converts the old ezca option into a pendtimeout value.

 Revision 1.14  2002/10/17 18:32:56  soliday
 BPS16monitor.c and sddsvmonitor.c no longer use ezca.

 Revision 1.13  2002/08/14 20:00:30  soliday
 Added Open License

 Revision 1.12  2001/05/03 19:53:42  soliday
 Standardized the Usage message.

 Revision 1.11  2000/04/20 15:57:38  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.10  2000/04/19 15:49:11  soliday
 Removed some solaris compiler warnings.

 Revision 1.9  2000/03/08 17:12:36  soliday
 Removed compiler warnings under Linux.

 Revision 1.8  1999/09/17 22:11:07  soliday
 This version now works with WIN32

 Revision 1.7  1999/03/12 18:30:42  borland
 No changes.  Spurious need to commit due to CVS mixing up the commenting
 of update notes.

 * Revision 1.6  1996/02/10  06:30:23  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.5  1996/02/07  18:49:49  borland
 * Brought up-to-date with new time routines and common SDDS time output format.
 *
 * Revision 1.4  1995/11/14  23:57:21  borland
 * Fixed non-ANSI use of char * return from sprintf.
 *
 * Revision 1.3  1995/11/09  03:22:24  borland
 * Added copyright message to source files.
 *
 * Revision 1.2  1995/11/04  04:45:30  borland
 * Added new program sddsalarmlog to Makefile.  Changed routine
 * getTimeBreakdown in SDDSepics, and modified programs to use the new
 * version; it computes the Julian day and the year now.
 *
 * Revision 1.1  1995/09/25  20:15:17  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#define ONCAERROR_SKIPPAGE 0
#define ONCAERROR_EXIT 1
#define ONCAERROR_RETRY 2
#define ONCAERROR_OPTIONS 3
char *oncaerror_options[ONCAERROR_OPTIONS] = {
  "skippage",
  "exit",
  "retry",
};

#define NAuxiliaryColumnNames 4
char *AuxiliaryColumnNames[NAuxiliaryColumnNames] = {
  "Step",
  "Time",
  "TimeOfDay",
  "DayOfMonth"};

#define NTimeUnitNames 4
char *TimeUnitNames[NTimeUnitNames] = {
  "seconds",
  "minutes",
  "hours",
  "days"};
double TimeUnitFactor[NTimeUnitNames] = {
  1,
  60,
  3600,
  86400};

#define MAX_WAVEFORM_LENGTH 5274
/* Time between samples in ms */
#define SAMPLE_INTERVAL 0.09765625
static double sampleInterval;

#define DEFAULT_TIME_INTERVAL 10
#define DEFAULT_STEPS 100

#define STAT_BASELINE 0
#define STAT_SLOPE 1
#define STAT_INTERCEPT 2
#define STAT_LINEAR_CHI 3
#define STAT_ZERO_CROSSING 4
#define STAT_MEANEXPRATE 5
#define STAT_MAXEXPRATE 6
#define STAT_REFFIT_CHI 7
#define STAT_AMPLITUDE 8
#define STAT_MEAN 9
#define STAT_T25RISEREF 10
#define STAT_T50RISEREF 11
#define STAT_T75RISEREF 12
#define STAT_MAXIMUM 13
#define STAT_MINIMUM 14
#define STAT_FALL25 15
#define STAT_FALL50 16
#define STAT_FALL75 17
#define STAT_RISE25 18
#define STAT_RISE50 19
#define STAT_RISE75 20
#define STAT_PERIOD 21
#define STAT_SLOPE1 22
#define STAT_SLOPE2 23
#define STAT_SLOPE3 24
#define STAT_SLOPE4 25
#define STAT_MEAN1 26
#define STAT_MEAN2 27
#define STAT_MEAN3 28
#define STAT_MEAN4 29
#define STATISTICS 30

#define STAT_UNITS_Y 1
#define STAT_UNITS_X 2
#define STAT_UNITS_YoX 3

typedef struct
{
  char *name, *matchName, *descriptionTemplate;
  long unitsCode;
} STAT_CATEGORY;
static STAT_CATEGORY statistic[STATISTICS] = {
  {"Baseline", NULL, "Baseline of %s", STAT_UNITS_Y},
  {"Slope", NULL, "Slope of linear fit for %s", STAT_UNITS_YoX},
  {"Intercept", NULL, "Intercept of linear fit for %s", STAT_UNITS_Y},
  {"LinFitQuality", NULL, "Reduced chi-squared of linear fit for %s", 0},
  {"ZeroCrossing", NULL, "Zero-crossing of %s from linear fit", STAT_UNITS_X},
  {"MeanRate", NULL, "Mean exponential growth rate of linear fit residual for %s", STAT_UNITS_X},
  {"MaxAbsRate", NULL, "Maximum absolute value of exponential rate of linear fit residual for %s", STAT_UNITS_X},
  {"RefFitQuality", NULL, "Reduced chi-squared from reference waveform for %s", 0},
  {"Amplitude", NULL, "Amplitude above baseline for %s", STAT_UNITS_Y},
  {"Mean", NULL, "Mean value of %s", STAT_UNITS_Y},
  /* don't change the order of the next three items */
  {"RiseRef25", NULL, "Time at which %s rises past 25%% of maximum reference value", STAT_UNITS_X},
  {"RiseRef50", NULL, "Time at which %s rises past 50%% of maximum reference value", STAT_UNITS_X},
  {"RiseRef75", NULL, "Time at which %s rises past 75%% of maximum reference value", STAT_UNITS_X},
  {"Maximum", NULL, "Maximum value of %s", STAT_UNITS_Y},
  {"Minimum", NULL, "Minimum value of %s", STAT_UNITS_Y},
  /* don't change the order of the next three items */
  {"Fall25", NULL, "Time at which %s falls to 25%% of peak value", STAT_UNITS_X},
  {"Fall50", NULL, "Time at which %s falls to 50%% of peak value", STAT_UNITS_X},
  {"Fall75", NULL, "Time at which %s falls to 75%% of peak value", STAT_UNITS_X},
  /* don't change the order of the next three items */
  {"Rise25", NULL, "Time at which %s rises to 25%% of peak value", STAT_UNITS_X},
  {"Rise50", NULL, "Time at which %s rises to 50%% of peak value", STAT_UNITS_X},
  {"Rise75", NULL, "Time at which %s rises to 75%% of peak value", STAT_UNITS_X},
  {"Period", NULL, "Period of %s", STAT_UNITS_X},
  /* don't change the order of the next four items */
  {"Slope1", NULL, "Slope of the first quarter for %s", STAT_UNITS_YoX},
  {"Slope2", NULL, "Slope of the second quarter for %s", STAT_UNITS_YoX},
  {"Slope3", NULL, "Slope of the third quarter for %s", STAT_UNITS_YoX},
  {"Slope4", NULL, "Slope of the fourth quarter for %s", STAT_UNITS_YoX},
  /* don't change the order of the next four items */
  {"Mean1", NULL, "Mean of the first region for %s", STAT_UNITS_Y},
  {"Mean2", NULL, "Mean of the second region for %s", STAT_UNITS_Y},
  {"Mean3", NULL, "Mean of the third region for %s", STAT_UNITS_Y},
  {"Mean4", NULL, "Mean of the fourth region for %s", STAT_UNITS_Y},
};
#define MEAN_REGIONS 4

#define RESULT_TYPE SDDS_FLOAT
typedef double RESULT;
typedef struct
{
  long assignment, statistic;
  char *parameter, *units, *description;
  short desired;
  RESULT result;
} STAT_OUTPUT;

typedef struct
{
  double startFraction, lengthFraction;
} MEAN_REGION;

STAT_OUTPUT *organizeStatOutput(CHANNEL_ASSIGNMENT *assignment, long assignments,
                                STAT_CATEGORY *statistic, long statistics, long *statOutputs);
long analyseWaveform(STAT_OUTPUT *statOutput, long stats, float *waveform, long waveformLength,
                     double baselineFraction, double headFraction, double tailFraction,
                     double expIntervalFraction, double slopesStartFraction, MEAN_REGION *meanRegion,
                     float *referenceWaveform);
void initializeStatStructures(STAT_CATEGORY *statistic, long statistics);
double findRisingLevel(float *waveform, long waveformLength, long istart, double level);
double findFallingLevel(float *waveform, long length, long imax, double level);
long computePeriod(double *result, float *waveform, long waveformLength, double min, double max);
void listAnalyses(FILE *fp);
long quickLinearFit(double *slope, double *intercept, double *reducedChiSquared,
                    double *residual, float *waveform, double baseline, double sampleInterval,
                    long startIndex, long endIndex);

#define CLO_INTERVAL 0
#define CLO_STEPS 1
#define CLO_VERBOSE 2
#define CLO_SINGLE_SHOT 3
#define CLO_TIME 4
#define CLO_PROMPT 5
#define CLO_ONCAERROR 6
#define CLO_SPARSE 7
#define CLO_SCALARS 8
#define CLO_ASSIGNMENTS 9
#define CLO_BASELINEFRACTION 10
#define CLO_HEADFRACTION 11
#define CLO_TAILFRACTION 12
#define CLO_EXPINTERVALFRACTION 13
#define CLO_LOGWAVEFORMS 14
#define CLO_EZCATIMING 15
#define CLO_LISTANALYSES 16
#define CLO_SLOPESSTARTFRACTION 17
#define CLO_MEANREGION 18
#define CLO_WAVEFORMLENGTH 19
#define CLO_SAMPLEINTERVAL 20
#define CLO_PENDIOTIME 21
#define COMMANDLINE_OPTIONS 22
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval", "steps", "verbose", "singleshot", "time", "prompt", "oncaerror",
  "sparse", "scalars", "assignments", "baselinefraction", "headfraction", "tailfraction",
  "expintervalfraction", "logwaveforms", "ezcatiming", "listanalyses",
  "slopesstartfraction", "meanregion", "waveformlength", "sampleinterval",
  "pendiotime"};

static char *USAGE1 = "BPS16monitor <outputfile> -assignments=<filename>\n\
    [-scalars=<filename>] [-interval=<real-value>[,<time-units>]]\n\
    [-steps=<integer-value> | -time=<real-value>[,<time-units>]]\n\
    [-singleShot=[noprompt]] [-onCAerror={skippage|exit}]\n\
    [-sparse=<interval>] [-baselineFraction=<value>] \n\
    [-headFraction=<value>] [-tailFraction=<value>] \n\
    [-slopesStartFraction=<value>] [-expIntervalFraction=<value>]\n\
    [-meanRegion=<regionNumber>,<startFraction>,<lengthFraction>]\n\
    [-pendIOtime=<seconds>] [-logWaveforms] [-listAnalyses]\n\
    [-waveformLength=<integer>] [-sampleInterval=<milli-sec>]\n\n\
Saves booster power supply waveforms to a SDDS file.\n\
<outputfile>       contains one column for AFG, voltage, and current\n\
                   for BM, QF, and QD.\n\
assignments        contains columns PVname and SignalName to indicate which\n\
                   waveform PVs to read and the physical signals that are\n\
                   connected.\n\
interval           desired interval in seconds for each reading step;\n\
                   valid time units are seconds, minutes, hours, or days.\n\
steps              number of reads for each process variable or device.\n\
time               total time (in seconds) for monitoring;\n\
                   valid time units are seconds, minutes, hours, or days.\n\
singleshot         single shot read initiated by a <cr> key press; interval is disabled.\n\
scalars            specify a file from which to take process variable names to\n\
                   be read along with the waveforms.  If no column name is given,\n\
                   the program looks for ControlName and then DeviceName.\n\
sparse             uses and logs only every ith point from waveforms.\n\
XFraction          specifies fraction of the waveform to use for analysis X.\n\
logWaveforms       specifies that waveforms should be stored in the output file.\n";
static char *USAGE2 = "onCAerror          action taken when a channel access error occurs. Default is\n\
                   to skip the page.\n\
meanRegion         specifies position and extent of mean regions 1-4.\n\
                   Defaults are 15%, 40%, 65% and 90% for position, and\n\
                   5% for length.\n\
listAnalyses       Lists available waveform analyses.\n\
waveformLength     Length of waveform from digitizer.  Default is 5274.\n\
sampleInterval     Sample interval in ms between points.  Default is 0.09765625.\n\n\
Program by M. Borland.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define ANSWER_LENGTH 256

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE outTable;
  char *outputFile, *scalarFile, **scalarParameter, **scalarPV, *assignmentFile;
  chid *scalarCHID = NULL;
  unsigned short **waveformData;
  float **waveformOutput, **waveformReference, *time;
  CHANNEL_ASSIGNMENT *assignment;
  STAT_OUTPUT *statOutput;

  long i, j, i_arg, scalarParameters, assignments, statOutputs, region;
  long waveformLength;
  char s[1024], s1[1024];
  char *TimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartDay, StartHour, TimeOfDay;
  double DayOfMonth, timeToWait, timeToRead, timeNow;
  double TotalTime, StartTime, YearStartTime, *scalarValue;
  double baselineFraction, headFraction, tailFraction, expIntervalFraction, slopesStartFraction;
  long Step, NStep, verbose, singleShot;
  long stepsSet, totalTimeSet;
  char answer[ANSWER_LENGTH];
  long TimeUnits;
  long onCAerror, logWaveforms, problemCount;
  int32_t sparse;
  short gapInData, referencePresent;
  long maxWaveformLength;
  double pendIOtime = 10.0, timeout;
  long retries;

  MEAN_REGION meanRegion[MEAN_REGIONS] = {{0.15, 0.05}, {0.40, 0.05}, {0.65, 0.05}, {0.90, 0.05}};

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s\n", USAGE1, USAGE2);
    exit(1);
  }

  outputFile = assignmentFile = scalarFile = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = verbose = singleShot = stepsSet = assignments = logWaveforms = 0;
  onCAerror = ONCAERROR_SKIPPAGE;
  sparse = 1;
  assignment = NULL;
  baselineFraction = 0.05;
  headFraction = 0.15;
  tailFraction = 0.85;
  expIntervalFraction = 0.01;
  slopesStartFraction = 0.10;
  maxWaveformLength = MAX_WAVEFORM_LENGTH;
  sampleInterval = SAMPLE_INTERVAL;
  scalarValue = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items < 2 || !(get_double(&TimeInterval, s_arg[i_arg].list[1])) ||
            TimeInterval <= 0)
          bomb("no value or invalid value given for option -interval", NULL);
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            TimeInterval *= TimeUnitFactor[TimeUnits];
          else
            bomb("unknown/ambiguous time units given for -interval", NULL);
        }
        break;
      case CLO_STEPS:
        if (!(get_long(&NStep, s_arg[i_arg].list[1])) || NStep <= 0)
          bomb("no value or invalid value given for option -steps", NULL);
        stepsSet = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_SINGLE_SHOT:
      case CLO_PROMPT:
        singleShot = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TotalTime, s_arg[i_arg].list[1])) || TotalTime <= 0)
          bomb("no value or invalid value given for option -time", NULL);
        totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            TotalTime *= TimeUnitFactor[TimeUnits];
          else
            bomb("invalid units for -time option", NULL);
        }
        break;
      case CLO_ONCAERROR:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -onCAerror syntax", NULL);
        if ((onCAerror = match_string(s_arg[i_arg].list[1], oncaerror_options, ONCAERROR_OPTIONS, 0)) < 0)
          fprintf(stderr, "error: unrecognized oncaerror option given: %s\n", s_arg[i_arg].list[1]);
        break;
      case CLO_SPARSE:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%" SCNd32, &sparse) != 1 ||
            sparse <= 0)
          bomb("no value or invalid value given for option -sparse", NULL);
        break;
      case CLO_SCALARS:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -scalars syntax", NULL);
        scalarFile = s_arg[i_arg].list[1];
        break;
      case CLO_ASSIGNMENTS:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -assignments syntax", NULL);
        assignmentFile = s_arg[i_arg].list[1];
        break;
      case CLO_BASELINEFRACTION:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &baselineFraction) != 1 ||
            baselineFraction <= 0 || baselineFraction >= 1)
          bomb("no value or invalid value given for option -baselineFraction", NULL);
        break;
      case CLO_HEADFRACTION:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &headFraction) != 1 ||
            headFraction <= 0 || headFraction >= 1)
          bomb("no value or invalid value given for option -headFraction", NULL);
        break;
      case CLO_TAILFRACTION:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &tailFraction) != 1 ||
            tailFraction <= 0 || tailFraction >= 1)
          bomb("no value or invalid value given for option -tailFraction", NULL);
        break;
      case CLO_EXPINTERVALFRACTION:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &expIntervalFraction) != 1 ||
            expIntervalFraction <= 0 || expIntervalFraction >= 1)
          bomb("no value or invalid value given for option -expIntervalFraction", NULL);
        break;
      case CLO_LOGWAVEFORMS:
        logWaveforms = 1;
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
      case CLO_LISTANALYSES:
        listAnalyses(stdout);
        exit(1);
        break;
      case CLO_SLOPESSTARTFRACTION:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &slopesStartFraction) != 1 ||
            slopesStartFraction <= 0 || slopesStartFraction >= 1)
          bomb("no value or invalid value given for option -slopesStartFraction", NULL);
        break;
      case CLO_MEANREGION:
        if (s_arg[i_arg].n_items != 4 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &region) != 1 ||
            region > MEAN_REGIONS || region < 1 ||
            sscanf(s_arg[i_arg].list[2], "%lf", &meanRegion[region - 1].startFraction) != 1 ||
            sscanf(s_arg[i_arg].list[3], "%lf", &meanRegion[region - 1].lengthFraction) != 1 ||
            meanRegion[region - 1].startFraction < 0 || meanRegion[region - 1].lengthFraction <= 0 ||
            (meanRegion[region - 1].startFraction + meanRegion[region - 1].lengthFraction) > 1)
          bomb("invalid -meanRegion syntax/values", NULL);
        break;
      case CLO_WAVEFORMLENGTH:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &maxWaveformLength) != 1 ||
            maxWaveformLength < 2)
          bomb("invalid -waveformLength syntax/values", NULL);
        break;
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &sampleInterval) != 1 ||
            sampleInterval <= 0)
          bomb("invalid -sampleInterval syntax/values", NULL);
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!outputFile)
        outputFile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }
  setVerbosity(verbose);

  if (!outputFile)
    bomb("output filename not given", NULL);
  if (baselineFraction > headFraction)
    bomb("baselineFraction > headFraction", NULL);
  if ((headFraction + 1 - tailFraction) >= 1)
    bomb("headFraction+1-tailFraction>=1", NULL);

  ca_task_initialize();
  if (totalTimeSet) {
    if (stepsSet)
      fprintf(stderr, "warning: option -time supersedes option -steps\n");
    NStep = TotalTime / TimeInterval + 1;
  }

  if (fexists(outputFile)) {
    printf("Error. File %s already exists.\n", outputFile);
    exit(1);
  }

  getTimeBreakdown(&StartTime, &StartDay, &StartHour, NULL, NULL, NULL, &TimeStamp);
  YearStartTime = computeYearStartTime(StartTime);
  if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 0, "Booster ramp signals",
                             "ramp signals", outputFile) ||
      0 > SDDS_DefineParameter(&outTable, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "Time", NULL, NULL, "Time since start of epoch", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "TimeOfDay", NULL,
                               "h", "Time of day", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "DayOfMonth", NULL,
                               "days", "Day of the month", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "TimeStamp", NULL, NULL, "Time stamp for page", NULL,
                               SDDS_STRING, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "StartTime", NULL, "s", "Start time", NULL,
                               SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "YearStartTime", NULL, "s",
                               "Start time of present year",
                               NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter1(&outTable, "Sparsing", NULL, NULL, "Sparsing factor applied to waveforms",
                                NULL, SDDS_LONG, &sparse) ||
      0 > SDDS_DefineParameter(&outTable, "GapInData", NULL, NULL, NULL,
                               NULL, SDDS_SHORT, NULL) ||
      0 > SDDS_DefineParameter(&outTable, "ProblemCount", NULL, NULL, NULL,
                               NULL, SDDS_LONG, NULL) ||
      (logWaveforms && 0 > SDDS_DefineColumn(&outTable, "t", NULL, "ms", NULL, NULL, SDDS_FLOAT, 0)))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (i = 0; i < MEAN_REGIONS; i++) {
    sprintf(s, "MeanRegion%ldStart", i + 1);
    sprintf(s1, "%f", meanRegion[i].startFraction);
    if (0 > SDDS_DefineParameter(&outTable, s, NULL, NULL, NULL, NULL, SDDS_FLOAT, s1))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    sprintf(s, "MeanRegion%ldLength", i + 1);
    sprintf(s1, "%f", meanRegion[i].lengthFraction);
    if (0 > SDDS_DefineParameter(&outTable, s, NULL, NULL, NULL, NULL, SDDS_FLOAT, s1))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (scalarFile) {
    addScalarParameters(&outTable, &scalarPV, &scalarParameter, &scalarParameters,
                        scalarFile, RESULT_TYPE, &scalarCHID, pendIOtime);
    if (scalarParameters)
      scalarValue = tmalloc(sizeof(*scalarValue) * scalarParameters);
  }

  assignment = makeAssignments(assignmentFile, &assignments);
  statOutput = organizeStatOutput(assignment, assignments, statistic, STATISTICS, &statOutputs);
  for (i = 0; i < assignments; i++) {
    for (j = 0; j < STATISTICS; j++) {
      if (statOutput[i * STATISTICS + j].desired &&
          SDDS_DefineParameter(&outTable, statOutput[i * STATISTICS + j].parameter,
                               NULL, statOutput[i * STATISTICS + j].units,
                               statOutput[i * STATISTICS + j].description, NULL,
                               RESULT_TYPE, NULL) < 0)
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (logWaveforms &&
        SDDS_DefineColumn(&outTable, assignment[i].signalName, NULL, assignment[i].convertedUnits,
                          NULL, NULL, SDDS_FLOAT, 0) < 0)
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  if (!SDDS_WriteLayout(&outTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  waveformLength = maxWaveformLength / sparse;
  time = tmalloc(sizeof(*time) * waveformLength);
  for (i = 0; i < waveformLength; i++)
    time[i] = i * sparse * sampleInterval;
  waveformData = tmalloc(sizeof(*waveformData) * assignments);
  waveformOutput = tmalloc(sizeof(*waveformOutput) * assignments);
  waveformReference = tmalloc(sizeof(*waveformReference) * assignments);
  for (i = 0; i < assignments; i++) {
    waveformData[i] = tmalloc(sizeof(**waveformData) * maxWaveformLength);
    waveformOutput[i] = tmalloc(sizeof(**waveformOutput) * waveformLength);
    waveformReference[i] = tmalloc(sizeof(**waveformReference) * waveformLength);
  }

  gapInData = 0;
  referencePresent = 0;
  timeToRead = 0;
  for (Step = 0; Step < NStep; Step++) {
    if (Step && !singleShot) {
      if ((timeToWait = TimeInterval - timeToRead) < 0)
        timeToWait = 0;
      if (verbose)
        fputs("Sleeping...", stderr);
      if (timeToWait > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(timeToWait);
      } else {
        ca_poll();
      }
      if (verbose)
        fputs("done.\n", stderr);
    }
    if (singleShot) {
      if (singleShot == SS_PROMPT) {
        fputs("Type <cr> to read, q to quit:", stderr);
        fflush(stderr);
      }
      fgets(answer, ANSWER_LENGTH, stdin);
      if (answer[0] == 'q' || answer[0] == 'Q') {
        SDDS_Terminate(&outTable);
        exit(0);
      }
    }
    timeToRead = getTimeInSecs();
    if (!Read16BitWaveforms(waveformData, maxWaveformLength, assignment, assignments, pendIOtime) ||
        0 != ReadScalarValues(scalarPV, NULL, NULL, scalarValue, scalarParameters, scalarCHID, pendIOtime)) {
      switch (onCAerror) {
      case ONCAERROR_SKIPPAGE:
        if (verbose)
          printf("CA errors--skipping page\n");
        gapInData = 1;
        continue;
      case ONCAERROR_RETRY:
        if (verbose)
          printf("CA errors--will try again\n");
        usleep(1000000);
        continue;
      case ONCAERROR_EXIT:
        fprintf(stderr, "CA errors--exiting\n");
        exit(1);
        break;
      }
    }
    timeToRead = getTimeInSecs() - timeToRead;
    for (i = 0; i < assignments; i++) {
      for (j = 0; j < waveformLength; j++)
        waveformOutput[i][j] = assignment[i].factor * ((float)waveformData[i][j * sparse]) +
                               assignment[i].offset;
    }
    if (!referencePresent) {
      for (i = 0; i < assignments; i++)
        memcpy((char *)waveformReference[i], (char *)waveformOutput[i],
               sizeof(*waveformOutput[i]) * waveformLength);
      referencePresent = 1;
    }
    for (i = problemCount = 0; i < assignments; i++) {
      problemCount +=
        analyseWaveform(statOutput + i * STATISTICS, STATISTICS, waveformOutput[i], waveformLength,
                        baselineFraction, headFraction, tailFraction, expIntervalFraction,
                        slopesStartFraction, meanRegion, waveformReference[i]);
    }
    ElapsedTime = (timeNow = getTimeInSecs()) - StartTime;
    EpochTime = timeNow;
    TimeOfDay = StartHour + ElapsedTime / 3600.0;
    DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
    TimeStamp = makeTimeStamp(timeNow);
    if (verbose)
      printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
    if (!SDDS_StartTable(&outTable, logWaveforms ? waveformLength : 0) ||
        !SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                            "Step", Step, "Time", EpochTime, "TimeOfDay", TimeOfDay,
                            "DayOfMonth", DayOfMonth, "TimeStamp", TimeStamp,
                            "StartTime", StartTime, "YearStartTime", YearStartTime,
                            "GapInData", gapInData, "ProblemCount", problemCount,
                            NULL))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    for (i = 0; i < scalarParameters; i++)
      if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                              scalarParameter[i], (RESULT)scalarValue[i], NULL)) {
        fprintf(stderr, "error: unable to set value for parameter %s\n", scalarParameter[i]);
        SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    for (i = 0; i < assignments; i++) {
      for (j = 0; j < STATISTICS; j++) {
        if (statOutput[i * STATISTICS + j].desired &&
            !SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                statOutput[i * STATISTICS + j].parameter,
                                (RESULT)statOutput[i * STATISTICS + j].result, NULL)) {
          fprintf(stderr, "error: unable to set value for parameter %s\n",
                  statOutput[i * STATISTICS + j].parameter);
        }
      }
    }
    if (logWaveforms) {
      if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, time, waveformLength, "t"))
        SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      for (i = 0; i < assignments; i++)
        if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, waveformOutput[i], waveformLength,
                            assignment[i].signalName))
          SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!SDDS_WriteTable(&outTable))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    gapInData = 0;
  }
  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  ca_task_exit();
  return 0;
}

STAT_OUTPUT *organizeStatOutput(CHANNEL_ASSIGNMENT *assignment, long assignments,
                                STAT_CATEGORY *statistic, long statistics, long *statOutputs) {
  STAT_OUTPUT *statOutput = NULL;
  long iAssign, iStat, iOutput;
  static char *defaultYunits = "1", *defaultXunits = "ms";
  char *yUnits, *xUnits, *ptr;

  xUnits = defaultXunits;
  statOutput = tmalloc(sizeof(*statOutput) * assignments * statistics);
  initializeStatStructures(statistic, statistics);
  for (iAssign = iOutput = 0; iAssign < assignments; iAssign++) {
    for (iStat = 0; iStat < statistics; iStat++, iOutput++) {
      statOutput[iOutput].assignment = iAssign;
      if (assignment[iAssign].analyses) {
        if (strstr(assignment[iAssign].analyses, "<all>"))
          statOutput[iOutput].desired = 1;
        else if (strstr(assignment[iAssign].analyses, "<none>"))
          statOutput[iOutput].desired = 0;
        if ((ptr = strstr(assignment[iAssign].analyses, statistic[iStat].matchName))) {
          if (ptr == assignment[iAssign].analyses || *(ptr - 1) != '-')
            statOutput[iOutput].desired = 1;
          else
            statOutput[iOutput].desired = 0;
        }
      } else
        statOutput[iOutput].desired = 1;

      statOutput[iOutput].statistic = iStat;
      statOutput[iOutput].parameter = tmalloc(sizeof(*statOutput[iOutput].parameter) *
                                              (strlen(statistic[iStat].name) +
                                               strlen(assignment[iAssign].signalName) + 1));
      sprintf(statOutput[iOutput].parameter, "%s%s",
              statistic[iStat].name, assignment[iAssign].signalName);
      statOutput[iOutput].description = tmalloc(sizeof(*statOutput[iOutput].description) *
                                                (strlen(statistic[iStat].descriptionTemplate) +
                                                 strlen(assignment[iAssign].signalName) + 1));
      sprintf(statOutput[iOutput].description, statistic[iStat].descriptionTemplate, assignment[iAssign].signalName);
      statOutput[iOutput].units = NULL;
      if (!(yUnits = assignment[iAssign].convertedUnits))
        yUnits = defaultYunits;
      switch (statistic[iStat].unitsCode) {
      case STAT_UNITS_Y:
        if (yUnits == defaultYunits)
          statOutput[iOutput].units = NULL;
        else {
          statOutput[iOutput].units = tmalloc(sizeof(*statOutput[iOutput].units) * (strlen(yUnits) + 1));
          strcpy(statOutput[iOutput].units, yUnits);
        }
        break;
      case STAT_UNITS_X:
        statOutput[iOutput].units = tmalloc(sizeof(*statOutput[iOutput].units) * (strlen(xUnits) + 1));
        strcpy(statOutput[iOutput].units, xUnits);
        break;
      case STAT_UNITS_YoX:
        statOutput[iOutput].units = tmalloc(sizeof(*statOutput[iOutput].units) * (strlen(yUnits) + strlen(xUnits) + 2));
        sprintf(statOutput[iOutput].units, "%s/%s", yUnits, xUnits);
        break;
      default:
        statOutput[iOutput].units = NULL;
        break;
      }
    }
  }
  return statOutput;
}

long analyseWaveform(STAT_OUTPUT *statOutput, long stats, float *waveform, long waveformLength,
                     double baselineFraction, double headFraction, double tailFraction,
                     double expIntervalFraction, double slopesStartFraction,
                     MEAN_REGION *meanRegion, float *referenceWaveform) {
  long i, startLinearFit, endLinearFit, points, j, problem, imax = 0;
  double baseline = DBL_MAX;
  double slope, intercept, linChiSquared, refChiSquared;
  long baselinePoints, expInterval;
  double *residual, rate, averageRate, maxRate, delta, rawSum;
  double level, fractionalLevel[3] = {0.25, 0.50, 0.75};
  double min, max;
  double fraction1, fraction2, deltaFraction;

  problem = 0;

  baselinePoints = baselineFraction * waveformLength;
  if (baselinePoints < 1)
    baselinePoints = 1;

  /* compute baseline */
  if (statOutput[STAT_BASELINE].desired) {
    for (i = baseline = 0; i < baselinePoints; i++)
      baseline += waveform[i];
    statOutput[STAT_BASELINE].result = (baseline /= baselinePoints);
  } else
    baseline = 0;

  min = -(max = -DBL_MAX);
  for (i = 0; i < waveformLength; i++) {
    if (min > waveform[i])
      min = waveform[i];
    if (max < waveform[i]) {
      max = waveform[i];
      imax = i;
    }
  }

  if (statOutput[STAT_MAXIMUM].desired ||
      statOutput[STAT_MINIMUM].desired) {
    if (statOutput[STAT_MAXIMUM].desired)
      statOutput[STAT_MAXIMUM].result = max;
    if (statOutput[STAT_MINIMUM].desired)
      statOutput[STAT_MINIMUM].result = min;
  }

  if (statOutput[STAT_AMPLITUDE].desired) {
    double amplitude;
    amplitude = -DBL_MAX;
    for (i = 0; i < waveformLength; i++) {
      delta = waveform[i] - baseline;
      if (amplitude < delta)
        amplitude = delta;
    }
    if (amplitude == -DBL_MAX)
      problem++;
    statOutput[STAT_AMPLITUDE].result = amplitude;
  }

  if (statOutput[STAT_MEAN].desired) {
    for (i = rawSum = 0; i < waveformLength; i++)
      rawSum += waveform[i];
    statOutput[STAT_MEAN].result = rawSum / waveformLength;
  } else
    statOutput[STAT_MEAN].result = 0;

  residual = tmalloc(sizeof(*residual) * waveformLength);
  startLinearFit = waveformLength * headFraction + 0.5;
  endLinearFit = waveformLength * tailFraction + 0.5;
  if ((points = endLinearFit - startLinearFit) > 2) {
    if (quickLinearFit(&slope, &intercept, &linChiSquared, residual,
                       waveform, baseline, sampleInterval,
                       startLinearFit, endLinearFit)) {
      statOutput[STAT_SLOPE].result = slope;
      statOutput[STAT_INTERCEPT].result = intercept;
      if (slope)
        statOutput[STAT_ZERO_CROSSING].result = -intercept / slope;
      else {
        if (statOutput[STAT_MEAN].desired)
          problem++;
        statOutput[STAT_ZERO_CROSSING].result = DBL_MAX;
      }
      statOutput[STAT_LINEAR_CHI].result = linChiSquared;
      if (statOutput[STAT_MEANEXPRATE].desired) {
        if ((expInterval = expIntervalFraction * points + 0.5) < 1)
          expInterval = 1;
        averageRate = maxRate = points = 0;
        for (i = startLinearFit; i < endLinearFit - expInterval; i++, points++) {
          rate = (residual[i] * expInterval) / (residual[i + expInterval] - residual[i]);
          averageRate += rate;
          rate = fabs(rate);
          if (rate > maxRate)
            maxRate = rate;
        }
        statOutput[STAT_MEANEXPRATE].result = averageRate / points * sampleInterval;
        statOutput[STAT_MAXEXPRATE].result = maxRate * sampleInterval;
      }
    } else {
      problem++;
      statOutput[STAT_SLOPE].result = statOutput[STAT_INTERCEPT].result = statOutput[STAT_LINEAR_CHI].result = statOutput[STAT_MEANEXPRATE].result = statOutput[STAT_MAXEXPRATE].result = 0;
    }
  } else {
    if (statOutput[STAT_SLOPE].desired || statOutput[STAT_INTERCEPT].desired || statOutput[STAT_LINEAR_CHI].desired || statOutput[STAT_MEANEXPRATE].desired || statOutput[STAT_MAXEXPRATE].desired)
      problem++;
    statOutput[STAT_SLOPE].result = statOutput[STAT_INTERCEPT].result = statOutput[STAT_LINEAR_CHI].result = statOutput[STAT_MEANEXPRATE].result = statOutput[STAT_MAXEXPRATE].result = 0;
  }

  refChiSquared = 0;
  for (i = 0; i < waveformLength; i++) {
    delta = waveform[i] - referenceWaveform[i];
    refChiSquared += sqr(delta);
  }
  if (waveformLength)
    refChiSquared /= waveformLength;
  else {
    if (statOutput[STAT_REFFIT_CHI].desired)
      problem++;
  }
  statOutput[STAT_REFFIT_CHI].result = refChiSquared;

  for (j = 0; j < 3; j++) {

    /* rise times to fractional levels of reference waveform */
    level = fractionalLevel[j] * referenceWaveform[waveformLength - 1];
    if (statOutput[STAT_T25RISEREF + j].desired) {
      if ((statOutput[STAT_T25RISEREF + j].result =
             sampleInterval * findRisingLevel(waveform, waveformLength, 0, level)) < 0)
        problem++;
    }

    /* fall times from maximum value of present waveform */
    if (statOutput[STAT_FALL25 + j].desired) {
      level = fractionalLevel[j] * max;
      if ((statOutput[STAT_FALL25 + j].result =
             sampleInterval * findFallingLevel(waveform, waveformLength, imax, level)) < 0)
        problem++;
    }

    /* rise times to maximum value of present waveform */
    if (statOutput[STAT_RISE25 + j].desired) {
      level = fractionalLevel[j] * max;
      if ((statOutput[STAT_RISE25 + j].result =
             sampleInterval * findRisingLevel(waveform, waveformLength, 0, level)) < 0)
        problem++;
    }
  }

  if (statOutput[STAT_PERIOD].desired) {
    if (!computePeriod(&statOutput[STAT_PERIOD].result, waveform, waveformLength, min, max))
      problem++;
  }

  fraction1 = 0;
  fraction2 = deltaFraction = (1 - slopesStartFraction) / 4;
  for (i = 0; i < 4; i++) {
    fraction1 = fraction2;
    fraction2 += deltaFraction;
    if (fraction2 > 1)
      fraction2 = 1;
    if (fraction1 > 1)
      bomb("fraction1 > 1 in analyseWaveform", NULL);
    startLinearFit = waveformLength * fraction1;
    endLinearFit = waveformLength * fraction2;
    if (statOutput[STAT_SLOPE1 + i].desired) {
      if (quickLinearFit(&slope, &intercept, &linChiSquared, NULL,
                         waveform, baseline, sampleInterval,
                         startLinearFit, endLinearFit)) {
        statOutput[STAT_SLOPE1 + i].result = slope;
      } else {
        statOutput[STAT_SLOPE1 + i].result = 0;
        problem++;
      }
    }
  }

  for (i = 0; i < MEAN_REGIONS; i++) {
    double mean;
    long start, end;
    if (statOutput[STAT_MEAN1 + i].desired) {
      start = meanRegion[i].startFraction * waveformLength;
      end = (meanRegion[i].startFraction + meanRegion[i].lengthFraction) * waveformLength;
      if (end > waveformLength - 1)
        end = waveformLength - 1;
      mean = 0;
      for (j = start; j <= end; j++)
        mean += waveform[j];
      statOutput[STAT_MEAN1 + i].result = mean / (end - start + 1);
    }
  }

  free(residual);
  return problem;
}

void initializeStatStructures(STAT_CATEGORY *statistic, long statistics) {
  long i;
  for (i = 0; i < statistics; i++) {
    if (statistic[i].matchName)
      continue;
    statistic[i].matchName = tmalloc(sizeof(*statistic[i].matchName) *
                                     (strlen(statistic[i].name) + 2));
    strcpy(statistic[i].matchName, statistic[i].name);
    strcat(statistic[i].matchName, " ");
  }
}

double findFallingLevel(float *waveform, long length, long imax, double level) {
  long i;
  if (imax == 0)
    imax = 1;
  for (i = imax; i < length; i++) {
    if (waveform[i - 1] > waveform[i] && waveform[i - 1] >= level && waveform[i] <= level)
      return (i - 1) + (level - waveform[i - 1]) / (waveform[i] - waveform[i - 1]);
  }
  return -1;
}

double findRisingLevel(float *waveform, long waveformLength, long istart, double level) {
  long i;
  if (istart < 0)
    istart = 0;
  for (i = istart + 1; i < waveformLength; i++) {
    if (waveform[i - 1] < waveform[i] && level >= waveform[i - 1] && level <= waveform[i])
      return i - 1 + (level - waveform[i - 1]) / (waveform[i] - waveform[i - 1]);
  }
  return -1;
}

long computePeriod(double *result, float *waveform, long waveformLength, double min, double max) {
  long offset, crossings;
  double crossing1, crossing2, crossing, level1, level2;

  level1 = (max + min) / 2;
  level2 = min + (max - min) * 0.9;
  offset = 0;
  crossing1 = -1;
  crossings = crossing2 = 0;
  *result = DBL_MAX / 2;

  while (offset < waveformLength &&
         (crossing = findRisingLevel(waveform, waveformLength, offset, level1)) != -1) {
#if defined(DEBUG)
    printf("new crossing = %e  after offset %ld\n", crossing, offset);
#endif
    if (crossing1 == -1)
      crossing1 = crossing;
    else
      crossing2 = crossing;
    crossings++;
    offset = crossing;
    while (offset < waveformLength && waveform[offset] < level2)
      offset++;
#if defined(DEBUG)
    printf("  [%ld] = %e   [%ld] = %e\n",
           offset, waveform[offset], offset + 1, waveform[offset + 1]);
#endif
  }
#if defined(DEBUG)
  printf("%ld crossings  crossing1/2 = %e/%e\n",
         crossings, crossing1, crossing2);
#endif
  if (crossings < 2)
    return 0;
  *result = sampleInterval * (crossing2 - crossing1) / (crossings - 1);
  return 1;
}

void listAnalyses(FILE *fp) {
  long i;
  char buffer[256];

  for (i = 0; i < STATISTICS; i++) {
    sprintf(buffer, statistic[i].descriptionTemplate, "y");
    fprintf(fp, "%20s %s\n",
            statistic[i].name, buffer);
  }
  fprintf(fp, "You may use \"<all>\" to obtain all analyses, \"<none>\" to\n");
  fprintf(fp, "obtain none, and [+-]<analysisName> to add/delete\n");
  fprintf(fp, "specific analyses.\n");
}

long quickLinearFit(double *slope, double *intercept, double *reducedChiSquared,
                    double *residual, float *waveform, double baseline, double sampleInterval,
                    long startIndex, long endIndex) {
  long i, points;
  double y1, sum_x2, sum_x, sum_xy, sum_y, D, residual1;

  /* compute linear fit and associated parameters */
  /* linear fit to y = a + bx:
     a = (S x^2 Sy - S x S xy)/D
     b = (N S xy  - Sx Sy)/D
     D = N S x^2 - (S x)^2
  */
  sum_x = sum_x2 = sum_y = sum_xy = 0;
  points = endIndex - startIndex;
  for (i = startIndex; i < endIndex; i++) {
    sum_x += i;
    sum_x2 += i * ((double)i);
    y1 = waveform[i] - baseline;
    sum_y += y1;
    sum_xy += i * y1;
  }
  if ((D = points * sum_x2 - sum_x * sum_x) != 0) {
    *slope = (points * sum_xy - sum_x * sum_y) / D / sampleInterval;
    *intercept = (sum_x2 * sum_y - sum_x * sum_xy) / D;
    /*
        printf("s_x, s_y, s_x2, s_xy, slope, intercept, D, points = \n%e, %e, %e, %e, %e, %e, %e, %ld\n",
        sum_x, sum_y, sum_x2, sum_xy, *slope, *intercept, D, points);
      */
    *reducedChiSquared = 0;
    for (i = startIndex; i < endIndex; i++) {
      residual1 = (waveform[i] - baseline) - (i * sampleInterval * (*slope) + (*intercept));
      *reducedChiSquared += residual1 * residual1;
      if (residual)
        residual[i] = residual1;
    }
    if (points > 2)
      *reducedChiSquared /= (points - 2);
    else
      *reducedChiSquared = DBL_MAX;
    return 1;
  } else
    return 0;
}
