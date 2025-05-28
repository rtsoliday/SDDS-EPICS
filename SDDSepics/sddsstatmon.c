/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
#endif

#define CLO_INTERVAL 0
#define CLO_STEPS 1
#define CLO_VERBOSE 2
#define CLO_SINGLE_SHOT 3
#define CLO_TIME 4
#define CLO_PRECISION 5
#define CLO_PROMPT 6
#define CLO_ONCAERROR 7
#define CLO_EZCATIMING 8
#define CLO_UPDATE_INTERVAL 9
#define CLO_SAMPLESPERSTAT 10
#define CLO_ERASE 11
#define CLO_GENERATIONS 12
#define CLO_COMMENT 13
#define CLO_GETUNITS 14
#define CLO_INCLUDESTATISTICS 15
#define CLO_ENFORCETIMELIMIT 16
#define CLO_OFFSETTIMEOFDAY 17
#define CLO_CONDITIONS 18
#define CLO_INHIBIT 19
#define CLO_WATCHINPUT 20
#define CLO_APPEND 21
#define CLO_PENDIOTIME 22
#define CLO_DATASTROBESAMPLEPV 23
#define CLO_DATASTROBEOUTPUTPV 24
#define COMMANDLINE_OPTIONS 25
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval",
  "steps",
  "verbose",
  "singleshot",
  "time",
  "precision",
  "prompt",
  "oncaerror",
  "ezcatiming",
  "updateinterval",
  "samplesperstatistic",
  "erase",
  "generations",
  "comment",
  "getunits",
  "includestatistics",
  "enforcetimelimit",
  "offsettimeofday",
  "conditions",
  "inhibitpv",
  "watchInput",
  "append",
  "pendiotime",
  "datastrobesamplepv",
  "datastrobeoutputpv",
};

#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2
static char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double"};

#define ONCAERROR_REPEAT 0
#define ONCAERROR_SKIP 1
#define ONCAERROR_EXIT 2
#define ONCAERROR_OPTIONS 3
static char *oncaerror_options[ONCAERROR_OPTIONS] = {
  "repeat",
  "skip",
  "exit"};

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

#define STAT_MEAN 0
#define STAT_STDEV 1
#define STAT_MIN 2
#define STAT_MAX 3
#define STAT_SIGMA 4
#define STAT_EXTREME 5
#define STAT_SAMPLE 6
#define STAT_SUM 7
#define STAT_LAST 8
#define STAT_FIRST 9
#define NSTATS 10
static char *statSuffix[NSTATS] = {
  "Mean", "StDev", "Min", "Max", "Sigma", "Extreme", "", "Sum", "Last", "First"};
#define DO_MEAN 0x0001U
#define DO_STDEV 0x0002U
#define DO_MIN 0x0004U
#define DO_MAX 0x0008U
#define DO_SIGMA 0x0010U
#define DO_EXTREME 0x0020U
#define DO_SAMPLE 0x0040U
#define DO_SUM 0x0080U
#define DO_LAST 0x0100U
#define DO_FIRST 0x0200U
static unsigned long statFlag[NSTATS] = {
  DO_MEAN, DO_STDEV, DO_MIN, DO_MAX, DO_SIGMA, DO_EXTREME,
  DO_SAMPLE, DO_SUM, DO_LAST, DO_FIRST};
#define DO_DEFAULT (DO_MEAN | DO_STDEV | DO_MIN | DO_MAX | DO_SIGMA | DO_EXTREME)

typedef struct
{
  double value[NSTATS];
  double sum, sumSqr, lastSample, firstSample;
  long values;
} STAT_DATA;

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

void datastrobeTriggerEventHandler1(struct event_handler_args event);
void datastrobeTriggerEventHandler2(struct event_handler_args event);
long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger, int index);
DATASTROBE_TRIGGER datastrobeSample;
DATASTROBE_TRIGGER datastrobeOutput;

void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
volatile int sigint = 0;


static char *USAGE1 = "sddsstatmon <input> <output>\n\
[-erase | -generations[=digits=<integer>][,delimiter=<string>]]\n\
[-steps=<integer-value> | -time=<real-value>[,<time-units>]] \n\
[-dataStrobeSamplePV=<PVname>] [-dataStrobeOutputPV=<PVname>] \n\
[-interval=<real-value>[,<time-units>] | [-singleShot{=noprompt|stdout}]]\n\
[-enforceTimeLimit] [-offsetTimeOfDay]\n\
[-includeStatistics=[mean,][standardDeviation,][minimum,][maximum,][sigma][,first][,last][,sample][,sum][,all]]\n\
[-samplesPerStatistic=<integer>] \n\
[-verbose] [-precision={single|double}]\n\
[-updateInterval=<integer>] [-watchInput]\n\
[-oncaerror={skip|exit|repeat}\n\
[-comment=<parameterName>,<text>]\n\
[-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
[-getUnits={force|ifBlank|ifNoneGiven}]\n\
[-append[=recover][,toPage]]] \n\
[-pendIOtime=<seconds>\n\
[-inhibitPV=name=<name>[,pendIOTime=<seconds>][,waitTime=<seconds>]]\n\n";
static char *USAGE2 = "Writes values of process variables or devices to a binary SDDS file.\n\
<inputfile>          defines the process variables or device names to be read;\n\
                     Two columns are required: One column must be \"Device\" or\n\
                     \"ControlName\" (they have the same effect),\n\
                     and one other column must be \"ReadbackName\".  An optional\n\
                     column \"ReadbackUnits\" may be supplied.\n\
erase                if <output> exists, overwrite it.\n\
append               Appends new values in a new SDDS page in the output file.\n\
                     Optionally recovers garbled SDDS data.  The 'toPage' qualifier\n\
                     results in appending to the last data page, rather than creation\n\
                     of a new page.\n\
generations          The output is sent to the file <output>-<N>, where <N> is\n\
                     the smallest positive integer such that the file does not already \n\
                     exist.   By default, four digits are used for formating <N>, so that\n\
                     the first generation number is 0001.\n\
steps                number of sets of statistics for each process variable.\n\
time                 total time for monitoring, in seconds by default;\n\
                     valid time units are seconds, minutes, hours, or days.\n\
enforceTimeLimit     Enforces the time limit given with the -time option, even if\n\
                     the expected number of samples has not been taken.\n";
static char *USAGE3 = "offsetTimeOfDay      Adjusts the starting TimeOfDay value so that it corresponds\n\
                     to the day for which the bulk of the data is taken.  Hence, a\n\
                     26 hour job started at 11pm would have initial time of day of\n\
                     -1 hour and final time of day of 25 hours.\n\
samplesPerStatistic  the number of samples to use for computing each statistic.\n\
                     The default is 25.\n\
includeStatistics    only the statistics given are placed in the output file, rather\n\
                     than the default set.  The default set includes mean, standard-\n\
                     deviation, minimum, maximum, and sigma.  For all statistics, give\n\
                     -includeStatistics=all.  The 'sample' statistic is the last sample\n\
                     but with no suffix attached to the name.\n";
static char *USAGE4 = "interval             desired interval between samples, in seconds by default;\n\
                     valid time units are seconds, minutes, hours, or days.\n\
singleShot           single shot set of samples initiated by a <cr> key press; time_interval is\n\
                     time between samples.  By default, a prompt is issued to stderr.  The\n\
                     qualifiers may be used to remove the prompt or have it go to stdout.\n\
updateInterval       number of sample sets between each output file update.\n\
                     The default is 1.\n\
verbose              prints out a message when data is taken.\n\
precision            specify single (default) or double precision for PV data.\n\
oncaerror            action taken when a channel access error occurs. Default is\n\
                     to repeat.\n\
comment              Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                     along with the text to be placed in the file.  \n\
conditions           Names an SDDS file containing PVs to read and limits on each PV that must\n\
                     be satisfied for data to be taken and logged.  The file is like the main\n\
                     input file, but has numerical columns LowerLimit and UpperLimit.\n";
static char *USAGE5 = "getUnits             Gets the units of quantities from EPICS.  'force' means ignore the ReadbackUnits\n\
                     data in the input, if any.  'ifBlank' means attempt to get units for any quantity\n\
                     that has a blank string given for units.  'ifNoneGiven' (default) means get units\n\
                     for all quantities, but only if no ReadbackUnits column is given in the file.\n\
dataStrobeSamplePV   PV trigger to sample PV data.\n\
dataStrobeOutputPV   PV trigger to calculate statistics and write out a new SDDS row.\n\
inhibitPV          Checks this PV prior to each sample.  If the value is nonzero, then data\n\
                   collection is inhibited.  None of the conditions-related or other PVs are\n\
                   polled.\n\n\
Program by M. Borland, L. Emery ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 100
#define DEFAULT_UPDATE_INTERVAL 1

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

/*
  #define NAuxiliaryColumnNames 5
  static char *AuxiliaryColumnNames[NAuxiliaryColumnNames] = {
  "Step",
  "Time",
  "TimeOfDay",
  "DayOfMonth",
  "CAerrors",
  };
*/

long SetStatValues(SDDS_DATASET *page, long row, long statIndex, STAT_DATA *statistic,
                   long n_variables, long Precision, unsigned long includeStatistics, long append, long **ReadbackIndex);
long SDDS_DefineSimpleSuffixColumns(SDDS_DATASET *outSet, long variables, char **name, char **units,
                                    long type, char *suffix);
long reduceStatistics(STAT_DATA *statistic, long variables);
void accumulateStatistics(STAT_DATA *statistic, long sample, double *variable, long variables);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET output_page, originalfile_page;
  long SDDSFlushNeeded = 0;

  char *inputfile, *outputfile, answer[ANSWER_LENGTH], *inputlink;
  long n_variables;
  int32_t samplesPerStat;
  char **DeviceNames = NULL, **ReadMessages = NULL, **ReadbackNames = NULL, **ReadbackUnits = NULL, **ColumnNames;
  chid *DeviceCHID = NULL;
  char **commentParameter = NULL, **commentText = NULL;
  long comments, enforceTimeLimit;
  double *value, *ScaleFactors;
  long i_arg;
  int32_t NColumnNames = 0;
  char *TimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartDay, StartMonth, StartHour, TimeOfDay, DayOfMonth;
  double TimeToWait;
  long Step, NStep, updateInterval, outputRow, updateCountdown;
  long verbose, singleShot, erase, generations;
  int32_t generationsDigits;
  unsigned long dummyFlags, getUnits, appendFlags;
  char *generationsDelimiter;
  long steps_set, totalTimeSet, append = 0, appendToPage = 0, recover = 0;
  double TotalTime;
  double StartTime, StartJulianDay, StartYear, RunTime, RunStartTime;
  long Precision;
  char *precision_string, *oncaerror;
  long TimeUnits, oncaerrorindex, CAerrors = 0;
  long statIndex=5, istat, sample, index;
  long **ReadbackIndex;
  STAT_DATA *statistic = NULL;
  unsigned long includeStatistics;
  long offsetTimeOfDay;
  int64_t initialOutputRow = 0;
  long i;
  double pendIOtime = 10.0;

  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  chid *CondCHID = NULL;
  long conditions;
  unsigned long CondMode = 0;
  double *CondDataBuffer;

  long inhibit = 0, doInhibit;
  int32_t InhibitWaitTime = 5;
  double InhibitPendIOTime = 10;
  chid inhibitID;
  PV_VALUE InhibitPV;

  struct stat input_filestat;
  int watchInput = 0;
  double timeout;
  long retries;

  long datastrobeSamplePresent = 0;
  long datastrobeOutputPresent = 0;
  short samplesNeeded = 0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5);
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

  offsetTimeOfDay = 0;
  verbose = 0;
  ReadbackIndex = NULL;
  inputfile = outputfile = inputlink = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = steps_set = generations = 0;
  erase = 0;
  singleShot = 0;
  oncaerrorindex = ONCAERROR_REPEAT;
  Precision = PRECISION_SINGLE;
  updateInterval = DEFAULT_UPDATE_INTERVAL;
  samplesPerStat = 25;
  commentParameter = commentText = NULL;
  comments = 0;
  getUnits = 0;
  includeStatistics = DO_DEFAULT;
  enforceTimeLimit = 0;
  CondFile = NULL;
  inhibit = 0;
  CondDataBuffer = NULL;
  precision_string = NULL;
  ColumnNames = NULL;
  datastrobeSample.PV = NULL;
  datastrobeOutput.PV = NULL;

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
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TimeInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -interval", NULL);
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            TimeInterval *= TimeUnitFactor[TimeUnits];
          else
            bomb("unknown/ambiguous time units given for -interval", NULL);
        }
        break;
      case CLO_UPDATE_INTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&updateInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -updateInterval", NULL);
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&NStep, s_arg[i_arg].list[1])))
          bomb("no value given for option -steps", NULL);
        steps_set = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PROMPT:
        singleShot = SS_PROMPT;
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
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items != 2 ||
            !(precision_string = s_arg[i_arg].list[1]))
          bomb("no value given for option -precision", NULL);
        switch (Precision = match_string(precision_string, precision_option,
                                         PRECISION_OPTIONS, 0)) {
        case PRECISION_SINGLE:
        case PRECISION_DOUBLE:
          break;
        default:
          printf("unrecognized precision value given for option %s\n", s_arg[i_arg].list[0]);
          exit(1);
        }
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TotalTime, s_arg[i_arg].list[1])))
          bomb("no value given for option -time", NULL);
        totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) != 0) {
            TotalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_ONCAERROR:
        if (s_arg[i_arg].n_items != 2 ||
            !(oncaerror = s_arg[i_arg].list[1])) {
          printf("No value given for option -oncaerror.\n");
          exit(1);
        }
        switch (oncaerrorindex = match_string(oncaerror, oncaerror_options, ONCAERROR_OPTIONS, 0)) {
        case ONCAERROR_REPEAT:
        case ONCAERROR_SKIP:
        case ONCAERROR_EXIT:
          break;
        default:
          printf("unrecognized oncaerror option given: %s\n", oncaerror);
          exit(1);
        }
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
      case CLO_SAMPLESPERSTAT:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%" SCNd32, &samplesPerStat) != 1 ||
            samplesPerStat <= 1)
          bomb("invalid -samplesPerStatistic syntax/value", NULL);
        break;
      case CLO_ERASE:
        erase = 1;
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
      case CLO_COMMENT:
        ProcessCommentOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1,
                             &commentParameter, &commentText, &comments);
        break;
      case CLO_GETUNITS:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&getUnits, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "force", -1, NULL, 0, GET_UNITS_FORCED,
                          "ifblank", -1, NULL, 0, GET_UNITS_IF_BLANK,
                          "ifnonegiven", -1, NULL, 0, GET_UNITS_IF_NONE,
                          NULL))
          SDDS_Bomb("invalid -getUnits syntax/values");
        if (!getUnits)
          getUnits = GET_UNITS_IF_NONE;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_INCLUDESTATISTICS:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&includeStatistics, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "mean", -1, NULL, 0, DO_MEAN,
                          "standarddeviation", -1, NULL, 0, DO_STDEV,
                          "minimum", -1, NULL, 0, DO_MIN,
                          "maximum", -1, NULL, 0, DO_MAX,
                          "sigma", -1, NULL, 0, DO_SIGMA,
                          "extreme", -1, NULL, 0, DO_EXTREME,
                          "sample", -1, NULL, 0, DO_SAMPLE,
                          "sum", -1, NULL, 0, DO_SUM,
                          "first", -1, NULL, 0, DO_FIRST,
                          "last", -1, NULL, 0, DO_LAST,
                          "all", -1, NULL, 0, 0xffff,
                          NULL) ||
            !includeStatistics)
          SDDS_Bomb("invalid -includeStatistics syntax");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_ENFORCETIMELIMIT:
        enforceTimeLimit = 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items < 3 ||
            SDDS_StringIsBlank(CondFile = s_arg[i_arg].list[1]) ||
            !(CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2)))
          SDDS_Bomb("invalid -conditions syntax/values");
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
      case CLO_DATASTROBESAMPLEPV:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -dataStrobeSamplePV syntax!");
        datastrobeSample.PV = s_arg[i_arg].list[1];
        datastrobeSamplePresent = 1;
        break;
      case CLO_DATASTROBEOUTPUTPV:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -dataStrobeOutputPV syntax!");
        datastrobeOutput.PV = s_arg[i_arg].list[1];
        datastrobeOutputPresent = 1;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }

  if (inhibit) {
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 1)) {
      fprintf(stderr, "error: problem doing search for Inhibit PV %s\n", InhibitPV.name);
      exit(1);
    }
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Data collection inhibited---not starting\n");
      exit(0);
    }
  }

  if (!inputfile)
    bomb("input filename not given", NULL);
  if (!outputfile)
    bomb("output filename not given", NULL);

  if (watchInput) {
    inputlink = read_last_link_to_file(inputfile);
    if (get_file_stat(inputfile, inputlink, &input_filestat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputfile);
      exit(1);
    }
  }

  if (erase && generations)
    SDDS_Bomb("-erase and -generations are incompatible options");
  if (append && erase)
    SDDS_Bomb("-append and -erase are incompatible options");
  if (append && generations)
    SDDS_Bomb("-append and -generations are incompatible options");
  if (generations)
    outputfile = MakeGenerationFilename(outputfile, generationsDigits, generationsDelimiter, NULL);

  if (fexists(outputfile)) {
    if (append) {
      if (comments)
        fprintf(stderr, "warning: comments ignored for file append (sddsmonitor)\n");
    } else if (!erase) {
      fprintf(stderr, "Error. File %s already exists.\n", outputfile);
      exit(1);
    }
  } else
    /* isn't an append (even if requested) since file doesn't exist */
    append = 0;

  if (datastrobeSamplePresent && !datastrobeOutputPresent) {
    SDDS_Bomb("-dataStrobeOutputPV must be set if also using -dataStrobeSamplePV");
  }
  if (!datastrobeSamplePresent && datastrobeOutputPresent) {
    SDDS_Bomb("-dataStrobeSamplePV must be set if also using -dataStrobeOutputPV");
  }

  if (totalTimeSet) {
    if (steps_set)
      fprintf(stderr, "Warning: option total_time supersedes option steps\n");
    if (!datastrobeSamplePresent && !datastrobeOutputPresent) {
      NStep = TotalTime / (TimeInterval * samplesPerStat) + 1;
    }
  } else {
    enforceTimeLimit = 0;
  }

  if (!getScalarMonitorData(&DeviceNames, &ReadMessages, &ReadbackNames,
                            &ReadbackUnits, &ScaleFactors, NULL,
                            &n_variables, inputfile, getUnits, &DeviceCHID, pendIOtime))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!n_variables) {
    fprintf(stderr, "error: no data in input file");
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

  /* if append is true and the specified output file already exists, then move
     the file to a temporary file, then copy all tables. */

  if (append) {
    if (!SDDS_CheckFile(outputfile)) {
      if (recover) {
        char commandBuffer[1024];
        fprintf(stderr, "warning: file %s is corrupted--reconstructing before appending--some data may be lost.\n", outputfile);
        sprintf(commandBuffer, "sddsconvert -recover -nowarnings %s", outputfile);
        system(commandBuffer);
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb("unable to get data from existing file---try -append=recover (may truncate file)");
      }
    }

    /* compare columns of output file to ReadbackNames column in input file */
    if (!SDDS_InitializeInput(&originalfile_page, outputfile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    ColumnNames = (char **)SDDS_GetColumnNames(&originalfile_page, &NColumnNames);
    ReadbackIndex = (long **)malloc(sizeof(*ReadbackIndex) * NSTATS);
    for (istat = 0; istat < NSTATS; istat++) {
      ReadbackIndex[istat] = (long *)malloc(sizeof(**ReadbackIndex) * n_variables);
      if (includeStatistics & statFlag[istat]) {
        char **s;
        s = malloc(sizeof(char *) * (n_variables));
        for (i = 0; i < n_variables; i++) {
          s[i] = malloc(sizeof(char) * (strlen(ReadbackNames[i]) + 20));
          sprintf(s[i], "%s%s", ReadbackNames[i], statSuffix[istat]);
          index = match_string(s[i], ColumnNames, NColumnNames, EXACT_MATCH);
          if (index == -1) {
            printf("ReadbackName %s doesn't match any columns in output file.\n", ReadbackNames[i]);
            exit(1);
          }
          ReadbackIndex[istat][i] = index;
        }
        for (i = 0; i < n_variables; i++)
          free(s[i]);
        free(s);
      }
    }
    /*
        for(i=0;i<NColumnNames;i++) {
        if (-1==match_string(ColumnNames[i],ReadbackNames,n_variables,DCL_STYLE_MATCH)) {
        if (-1==match_string(ColumnNames[i],AuxiliaryColumnNames,NAuxiliaryColumnNames,EXACT_MATCH)) {
        printf("Column %s in output doesn't match any ReadbackName value "
        "in input file.\n",ColumnNames[i]);
        exit(1);
        }
        }
        }
    
        SDDS_FreeStringArray(ColumnNames,NColumnNames); */
    for (i = 0; i < NColumnNames; i++)
      free(ColumnNames[i]);
    free(ColumnNames);

    if (SDDS_ReadPage(&originalfile_page) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Bomb("unable to get data from existing file---try -append=recover");
    }
    if (!SDDS_GetParameter(&originalfile_page, "StartHour", &StartHour) ||
        !SDDS_GetParameterAsDouble(&originalfile_page, "StartDayOfMonth", &StartDay) ||
        !SDDS_GetParameter(&originalfile_page, "StartTime", &StartTime)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    StartDay += StartHour / 24.0;
    if (appendToPage) {
      if (!SDDS_InitializeAppendToPage(&output_page, outputfile, updateInterval,
                                       &initialOutputRow) ||
          !SDDS_Terminate(&originalfile_page)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(1);
      }
    } else {
      if (!SDDS_InitializeAppend(&output_page, outputfile) ||
          !SDDS_StartTable(&output_page, updateInterval) ||
          !SDDS_CopyParameters(&output_page, &originalfile_page) ||
          !SDDS_Terminate(&originalfile_page)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        exit(1);
      }
    }
  } else {
    /*start with a new file */
    if (datastrobeSamplePresent) {
      samplesNeeded = 1;
      if (!SDDS_InitializeOutput(&output_page, SDDS_BINARY, 1, "sddsstatmon output",
                                 "sddsstatmon output", outputfile) ||
          0 > SDDS_DefineParameter1(&output_page, "Samples", NULL, NULL,
                                    "Number of samples used for statistics computation", NULL, SDDS_LONG,
                                    NULL) ||
          !DefineLoggingTimeParameters(&output_page) ||
          !DefineLoggingTimeDetail(&output_page, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else {
      if (!SDDS_InitializeOutput(&output_page, SDDS_BINARY, 1, "sddsstatmon output",
                                 "sddsstatmon output", outputfile) ||
          0 > SDDS_DefineParameter1(&output_page, "Samples", NULL, NULL,
                                    "Number of samples used for statistics computation", NULL, SDDS_LONG,
                                    &samplesPerStat) ||
          !DefineLoggingTimeParameters(&output_page) ||
          !DefineLoggingTimeDetail(&output_page, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_EnableFSync(&output_page);
    statIndex = SDDS_ColumnCount(&output_page);
    switch (Precision) {
    case PRECISION_SINGLE:
      for (istat = 0; istat < NSTATS; istat++) {
        if (includeStatistics & statFlag[istat] &&
            !SDDS_DefineSimpleSuffixColumns(&output_page, n_variables, ReadbackNames, ReadbackUnits, SDDS_FLOAT,
                                            statSuffix[istat]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      break;
    case PRECISION_DOUBLE:
      for (istat = 0; istat < NSTATS; istat++) {
        if (includeStatistics & statFlag[istat] &&
            !SDDS_DefineSimpleSuffixColumns(&output_page, n_variables, ReadbackNames, ReadbackUnits, SDDS_DOUBLE,
                                            statSuffix[istat]))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      break;
    default:
      bomb("Unknown precision encountered.\n", NULL);
    }
    if (!SDDS_DefineSimpleParameters(&output_page, comments, commentParameter, NULL, SDDS_STRING) ||
        !SDDS_WriteLayout(&output_page) ||
        !SDDS_StartPage(&output_page, updateInterval + 1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  value = (double *)malloc(sizeof(*value) * n_variables);
  statistic = (STAT_DATA *)malloc(sizeof(*statistic) * n_variables);

  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }

  if (datastrobeSamplePresent) {
    if (setupDatastrobeTriggerCallbacks(&datastrobeSample, 1) == 0) {
      return(1);
    }
    if (setupDatastrobeTriggerCallbacks(&datastrobeOutput, 2) == 0) {
      return(1);
    }
    NStep = INT32_MAX;
    samplesPerStat = INT32_MAX;
    while (!datastrobeOutput.triggered) {
      oag_ca_pend_event(.01, &sigint);
      if (sigint)
        return (1);
    }
    datastrobeOutput.triggered=0;
    datastrobeSample.triggered=0;
  }
  if (append) {
    getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, &TimeStamp);
    if (SDDS_CHECK_OKAY == SDDS_CheckParameter(&output_page, "PageTimeStamp", NULL, SDDS_STRING, NULL)) {
      /*SDDS_CopyString(&TimeStamp, makeTimeStamp(getTimeInSecs())); */
      SDDS_SetParameters(&output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "PageTimeStamp", TimeStamp, NULL);
    }
    statIndex = 5; /*there is no meaning for append option, just for allocating it memory*/
  } else {
    getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay,
                     &StartMonth, &StartYear, &TimeStamp);
    if (datastrobeOutputPresent) {
      StartTime = datastrobeOutput.currentValue;
    }
    RunStartTime = StartTime;
    if (!SDDS_SetParameters(&output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "TimeStamp", TimeStamp, "PageTimeStamp", TimeStamp,
                            "StartTime", StartTime, "StartYear", (short)StartYear,
                            "YearStartTime", computeYearStartTime(StartTime),
                            "StartJulianDay", (short)StartJulianDay, "StartMonth", (short)StartMonth,
                            "StartDayOfMonth", (short)StartDay, "StartHour", (float)StartHour,
                            NULL) ||
        !SetCommentParameters(&output_page, commentParameter, commentText, comments))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  updateCountdown = updateInterval;

  for (Step = 0, outputRow = initialOutputRow; Step < NStep; Step++, outputRow++) {
    if (singleShot) {
      if (singleShot != SS_NOPROMPT) {
        fputs("Type <cr> to read, q to quit:\n",
              singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
      }
      fgets(answer, ANSWER_LENGTH, stdin);
      if (answer[0] == 'q' || answer[0] == 'Q') {
        if (SDDSFlushNeeded)
          SDDS_UpdatePage(&output_page, FLUSH_TABLE);
        SDDS_Terminate(&output_page);
        exit(0);
      }
    }
    for (sample = 0; sample < samplesPerStat; sample++) {
      if (enforceTimeLimit) {
        if (datastrobeOutputPresent) {
          EpochTime = datastrobeOutput.currentValue;
        } else {
          EpochTime = getTimeInSecs();
        }
        RunTime = EpochTime - RunStartTime;
        if (RunTime > TotalTime) {
          if (verbose) {
            printf("Exiting because the time limit was reached.\n");
            fflush(stdout);
          }
          if (!SDDS_Terminate(&output_page))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          exit(0);
        }
      }

      doInhibit = 0;
      if (inhibit) {
        if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
          if (verbose) {
            printf("Inhibit PV %s is nonzero. Data collection inhibited.\n", InhibitPV.name);
            fflush(stdout);
          }
          doInhibit = 1;
          if (InhibitWaitTime > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
            ca_pend_event(InhibitWaitTime);
          }
        }
      }
      if (doInhibit ||
          (CondFile &&
           !PassesConditions(CondDeviceName, CondReadMessage, CondScaleFactor,
                             CondDataBuffer, CondLowerLimit, CondUpperLimit,
                             CondHoldoff, conditions, CondMode, CondCHID, pendIOtime))) {
        if (CondMode & TOUCH_OUTPUT)
          TouchFile(outputfile);
        if (!doInhibit && verbose) {
          printf("Step %ld---values failed condition test\n", Step);
          fflush(stdout);
        }
        if (SDDSFlushNeeded) {
          if (verbose)
            printf("tests failed, flushing data.\n");
          if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          SDDSFlushNeeded = 0;
        }

        if (datastrobeOutputPresent) {
          while (!datastrobeOutput.triggered) {
            oag_ca_pend_event(.01, &sigint);
            if (sigint)
              return (1);
          }
          datastrobeOutput.triggered=0;
        } else {
          TimeToWait = (samplesPerStat - sample) * TimeInterval;
          if (TimeToWait > 0) {
            /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
            ca_pend_event(TimeToWait);
          } else {
            ca_poll();
          }
        }
        if (CondMode & RETAKE_STEP)
          Step--;
        outputRow--;
        sample = 0; /* used to indicate that no samples are available */
        break;
      }
      if ((CAerrors = ReadScalarValues(DeviceNames, ReadMessages, ScaleFactors,
                                       value, n_variables, DeviceCHID, pendIOtime)) != 0) {
        switch (oncaerrorindex) {
        case ONCAERROR_REPEAT:
          sample--;
          /* case fall-through is intended */
        case ONCAERROR_SKIP:
          if (datastrobeOutputPresent) {
            while (!datastrobeOutput.triggered) {
              oag_ca_pend_event(.01, &sigint);
              if (sigint)
                return (1);
            }
            datastrobeOutput.triggered=0;
          } else {
            if (TimeInterval > 0) {
              /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
              ca_pend_event(TimeInterval);
            } else {
              ca_poll();
            }
          }
          continue;
        case ONCAERROR_EXIT:
          exit(1);
          break;
        }
      }
      accumulateStatistics(statistic, sample, value, n_variables);
      if (datastrobeSamplePresent) {
        while (!datastrobeSample.triggered) {
          oag_ca_pend_event(.01, &sigint);
          if (sigint)
            return (1);
        }
        datastrobeSample.triggered = 0;
        if (datastrobeOutput.triggered) {
          break;
        }
      } else {
        if (TimeInterval > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(TimeInterval);
        } else {
          ca_poll();
        }
      }
    }
    if (sample) {
      if (!reduceStatistics(statistic, n_variables))
        bomb("unable to reduce statistics", NULL);

      if (datastrobeOutputPresent) {
        EpochTime = datastrobeOutput.currentValue;
        if (samplesNeeded) {
          samplesNeeded = 0;
          if (!SDDS_SetParameters(&output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  "Samples", sample + 1, NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      } else {
        EpochTime = getTimeInSecs();
      }
      ElapsedTime = EpochTime - StartTime;
      RunTime = EpochTime - RunStartTime;
      if (enforceTimeLimit && RunTime > TotalTime) {
        NStep = Step;
      }
      TimeOfDay = StartHour + ElapsedTime / 3600.0;
      DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
      if (verbose) {
        printf("Step %ld. Done analysing samples at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      if (!SDDS_SetRowValues(&output_page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow, "Step", Step,
                             "Time", EpochTime, "TimeOfDay", (float)TimeOfDay,
                             "DayOfMonth", (float)DayOfMonth, "CAerrors", CAerrors, NULL))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

      if (SetStatValues(&output_page, outputRow, statIndex, statistic, n_variables,
                        Precision, includeStatistics, append, ReadbackIndex))
        bomb("unable to set statistics values", NULL);

      SDDSFlushNeeded = 1;
      if (!--updateCountdown) {
        updateCountdown = updateInterval;
        if (verbose) {
          printf("Flushing data\n");
          fflush(stdout);
        }
        SDDSFlushNeeded = 0;
        if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      } else if (Step >= NStep - 1) {
        if (SDDSFlushNeeded)
          if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        break;
      }
    }
    if (watchInput) {
      if (file_is_modified(inputfile, &inputlink, &input_filestat))
        break;
    }
    if (datastrobeOutputPresent) {
      datastrobeSample.triggered = 0;
      datastrobeOutput.triggered = 0;
    }
  }
  if (!SDDS_Terminate(&output_page))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }
  if (ReadbackIndex) {
    for (i = 0; i < NSTATS; i++)
      free(ReadbackIndex[i]);
    free(ReadbackIndex);
  }
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
  if (statistic)
    free(statistic);
  if (DeviceNames) {
    for (i = 0; i < n_variables; i++)
      free(DeviceNames[i]);
    free(DeviceNames);
  }
  if (ReadbackNames) {
    for (i = 0; i < n_variables; i++)
      free(ReadbackNames[i]);
    free(ReadbackNames);
  }
  if (ReadbackUnits) {
    for (i = 0; i < n_variables; i++)
      free(ReadbackUnits[i]);
    free(ReadbackUnits);
  }
  if (ReadMessages) {
    for (i = 0; i < n_variables; i++)
      free(ReadMessages[i]);
    free(ReadMessages);
  }
  if (DeviceCHID)
    free(DeviceCHID);
  if (commentParameter)
    free(commentParameter);
  if (commentText)
    free(commentText);
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return 0;
}

long SetStatValues(SDDS_DATASET *page, long row, long statIndex, STAT_DATA *statistic,
                   long n_variables, long Precision, unsigned long includeStatistics,
                   long append, long **ReadbackIndex) {
  long stat, offset, iPV;

  switch (Precision) {
  case PRECISION_SINGLE:
    for (stat = offset = 0; stat < NSTATS; stat++) {
      if (!(includeStatistics & statFlag[stat]))
        continue;
      for (iPV = 0; iPV < n_variables; iPV++, offset++) {
        if (!append) {
          if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                                 statIndex + offset, (float)statistic[iPV].value[stat],
                                 -1)) {
            fprintf(stderr, "problem setting column %ld for row %ld\n", statIndex + offset, row);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return 1;
          }
        } else {
          if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                                 ReadbackIndex[stat][iPV], (float)statistic[iPV].value[stat],
                                 -1)) {
            fprintf(stderr, "problem setting column %ld for row %ld\n", ReadbackIndex[stat][iPV], row);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return 1;
          }
        }
      }
    }
    break;
  case PRECISION_DOUBLE:
    for (stat = offset = 0; stat < NSTATS; stat++) {
      if (!(includeStatistics & statFlag[stat]))
        continue;
      for (iPV = 0; iPV < n_variables; iPV++, offset++) {
        if (!append) {
          if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                                 statIndex + offset, statistic[iPV].value[stat],
                                 -1)) {
            fprintf(stderr, "problem setting column %ld for row %ld\n", statIndex + offset, row);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return 1;
          }
        } else {
          if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                                 ReadbackIndex[stat][iPV], statistic[iPV].value[stat],
                                 -1)) {
            fprintf(stderr, "problem setting column %ld for row %ld\n", ReadbackIndex[stat][iPV], row);
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return 1;
          }
        }
      }
    }
    break;
  default:
    bomb("Unknown precision encountered.\n", NULL);
  }
  return (0);
}

long SDDS_DefineSimpleSuffixColumns(SDDS_DATASET *outSet, long variables, char **name, char **units,
                                    long type, char *suffix) {
  char nameBuffer[256];
  long ivar;

  for (ivar = 0; ivar < variables; ivar++) {
    sprintf(nameBuffer, "%s%s", name[ivar], suffix);
    if (!SDDS_DefineSimpleColumn(outSet, nameBuffer, units ? units[ivar] : NULL, SDDS_DOUBLE))
      return 0;
  }
  return 1;
}

void accumulateStatistics(STAT_DATA *statistic, long sample, double *variable, long variables) {
  long ivariable, istat;
  if (sample == 0) {
    /* zero out sums */
    for (ivariable = 0; ivariable < variables; ivariable++) {
      statistic[ivariable].values = statistic[ivariable].sum = statistic[ivariable].sumSqr = 0;
      for (istat = 0; istat < NSTATS; istat++)
        statistic[ivariable].value[istat] = 0;
      statistic[ivariable].value[STAT_MIN] = statistic[ivariable].value[STAT_MAX] = variable[ivariable];
    }
  }
  for (ivariable = 0; ivariable < variables; ivariable++) {
    if (sample == 0)
      statistic[ivariable].firstSample = variable[ivariable];
    if (variable[ivariable] > statistic[ivariable].value[STAT_MAX])
      statistic[ivariable].value[STAT_MAX] = variable[ivariable];
    if (variable[ivariable] < statistic[ivariable].value[STAT_MIN])
      statistic[ivariable].value[STAT_MIN] = variable[ivariable];
    statistic[ivariable].sum += variable[ivariable];
    statistic[ivariable].sumSqr += variable[ivariable] * variable[ivariable];
    statistic[ivariable].values += 1;
    statistic[ivariable].lastSample = variable[ivariable];
  }
}

long reduceStatistics(STAT_DATA *statistic, long variables) {
  long ivariable, n;
  double sum1, sum2;
  for (ivariable = 0; ivariable < variables; ivariable++) {
    if ((n = statistic[ivariable].values) < 2)
      return 0;
    sum1 = statistic[ivariable].sum;
    sum2 = statistic[ivariable].sumSqr;
    statistic[ivariable].value[STAT_MEAN] = sum1 / n;
    statistic[ivariable].value[STAT_SUM] = sum1;
    statistic[ivariable].value[STAT_SAMPLE] = statistic[ivariable].lastSample;
    statistic[ivariable].value[STAT_LAST] = statistic[ivariable].lastSample;
    statistic[ivariable].value[STAT_FIRST] = statistic[ivariable].firstSample;
    if (fabs(statistic[ivariable].value[STAT_MEAN] - statistic[ivariable].value[STAT_MAX]) >
        fabs(statistic[ivariable].value[STAT_MEAN] - statistic[ivariable].value[STAT_MIN]))
      statistic[ivariable].value[STAT_EXTREME] = statistic[ivariable].value[STAT_MAX];
    else
      statistic[ivariable].value[STAT_EXTREME] = statistic[ivariable].value[STAT_MIN];
    if (0 < (statistic[ivariable].value[STAT_STDEV] = (sum2 - sum1 * sum1 / n) / (n - 1))) {
      statistic[ivariable].value[STAT_STDEV] = sqrt(statistic[ivariable].value[STAT_STDEV]);
      statistic[ivariable].value[STAT_SIGMA] = statistic[ivariable].value[STAT_STDEV] / sqrt(n);
    } else
      statistic[ivariable].value[STAT_STDEV] = statistic[ivariable].value[STAT_SIGMA] = 0;
  }
  return 1;
}

void datastrobeTriggerEventHandler1(struct event_handler_args event) {
  if (event.status != ECA_NORMAL) {
    fprintf(stderr, "Error received on data strobe PV\n");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Received callback on data strobe PV\n");
#endif
  if (datastrobeSample.initialized == 0) {
    /* the first callback is just the connection event */
    datastrobeSample.initialized = 1;
    datastrobeSample.datalogged = 1;
    return;
  }
  datastrobeSample.currentValue = *((double *)event.dbr);
#ifdef DEBUG
  fprintf(stderr, "chid=%d, current=%f\n", (int)datastrobeSample.channelID, datastrobeSample.currentValue);
#endif
  datastrobeSample.triggered = 1;
  if (!datastrobeSample.datalogged)
    datastrobeSample.datastrobeMissed++;
  datastrobeSample.datalogged = 0;
  return;
}

void datastrobeTriggerEventHandler2(struct event_handler_args event) {
  if (event.status != ECA_NORMAL) {
    fprintf(stderr, "Error received on data strobe PV\n");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Received callback on data strobe PV\n");
#endif
  if (datastrobeOutput.initialized == 0) {
    /* the first callback is just the connection event */
    datastrobeOutput.initialized = 1;
    datastrobeOutput.datalogged = 1;
    return;
  }
  datastrobeOutput.currentValue = *((double *)event.dbr);
#ifdef DEBUG
  fprintf(stderr, "chid=%d, current=%f\n", (int)datastrobeOutput.channelID, datastrobeOutput.currentValue);
#endif
  datastrobeOutput.triggered = 1;
  if (!datastrobeOutput.datalogged)
    datastrobeOutput.datastrobeMissed++;
  datastrobeOutput.datalogged = 0;
  return;
}

long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger, int index) {
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
  if (ca_pend_io(5) != ECA_NORMAL) {
    fprintf(stderr, "error: search failed for control name %s\n", datastrobeTrigger->PV);
    return 0;
  }
  if (index == 1) {
    if (ca_add_masked_array_event(DBR_DOUBLE, 1, datastrobeTrigger->channelID,
                                  datastrobeTriggerEventHandler1,
                                  (void *)&datastrobeTrigger, (ca_real)0, (ca_real)0,
                                  (ca_real)0, NULL, DBE_VALUE) != ECA_NORMAL) {
      fprintf(stderr, "error: unable to setup datastrobe callback for control name %s\n",
              datastrobeTrigger->PV);
      exit(1);
    }
  } else if (index == 2) {
    if (ca_add_masked_array_event(DBR_DOUBLE, 1, datastrobeTrigger->channelID,
                                  datastrobeTriggerEventHandler2,
                                  (void *)&datastrobeTrigger, (ca_real)0, (ca_real)0,
                                  (ca_real)0, NULL, DBE_VALUE) != ECA_NORMAL) {
      fprintf(stderr, "error: unable to setup datastrobe callback for control name %s\n",
              datastrobeTrigger->PV);
      exit(1);
    }
  }
#ifdef DEBUG
  fprintf(stderr, "Set up callback on data strobe PV\n");
#endif
  ca_poll();
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
