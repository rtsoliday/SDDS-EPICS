/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddssynchlog
 * author: M. Borland, 1999
 * purpose: channel access with synchronization.
 *
 */

#include <time.h>

#ifdef _WIN32
#  include <winsock.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#include <cadef.h>

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#define CLO_VERBOSE 0
#define CLO_SAMPLES 1
#define CLO_PENDEVENTTIME 2
#define CLO_TIMEOUT 3
#define CLO_ERASE 4
#define CLO_COMMENT 5
#define CLO_SYNCHINTERVAL 6
#define CLO_TIMELIMIT 7
#define CLO_PRECISION 8
#define CLO_SLOWDATA 9
#define CLO_WAVEFORMDATA 10
#define CLO_PENDIOTIME 11
#define CLO_TIMESTAMPS 12
#define CLO_STEPS 13
#define CLO_INTERVAL 14
#define COMMANDLINE_OPTIONS 15

static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "verbose",
  "samples",
  "pendeventtime",
  "connecttimeout",
  "erasefile",
  "comment",
  "synchinterval",
  "timelimit",
  "precision",
  "slowdata",
  "waveformdata",
  "pendiotime",
  "savetimestamps",
  "steps",
  "interval",
};

static char *USAGE1 = "sddssynchlog <input> <output> [-slowData=<input>]\n\
[-waveformData=<filename>] -samples=<number> [-timeLimit=<value>[,<units>]]\n\
-eraseFile [-pendIOTime=<seconds>] [-connectTimeout=<seconds>]\n\
[-verbose] [-comment=<parameterName>,<text>]\n\
[-synchInterval=<seconds>] [-precision={single|double}]\n\
[-saveTimeStamps=<filename>] [-steps=<int>] [-interval=<value>[,<units>]] \n\n\
Logs numerical data for the process variables named in the ControlName\n\
column of <input> to an SDDS file <output>.  Synchronism is imposed by\n\
requiring that all PVs generated callbacks within the number of seconds\n\
given by the -synchInterval argument.\n\n\
-samples        Specifies the number of samples to attempt.  If synchronism problems\n\
                are encountered, fewer samples will appear in the output.\n\
-timelimit      Specifies maximum time to wait for data collection.\n\
-eraseFile      Specifies erasing the file <output> if it exists already.\n\
-pendIOTime     Specifies the CA pend event time, in seconds.  The default is 10 .\n\
-connectTimeout Specifies maximum time in seconds to wait for a connection before\n\
                issuing an error message. 60 is the default.\n";
static char *USAGE2 =
  "-verbose        Specifies printing of possibly useful data to the standard output.\n\
                 Giving several times increases the level of output\n\n\
-comment        Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                along with the text to be placed in the file.  \n\
-synchInterval  Specifies the time spread allowed for callbacks from the PVs being logged.\n\
                If any PV fails to callback within the specified interval, the data for\n\
                that sample is discarded.\n\
-slowData       Specifies an sddsmonitor-type input file giving the names of PVs to be\n\
                acquired at a \"slow\" rate, i.e., without synchronization.  Data for these PVs\n\
                are interpolated linearly to get values at each time sample of the synchronous\n\
                data. \n\
-waveformData   Specifies an sddswmonitor-type input file giving the names of waveform PVs to\n\
                be synchronously logged.  If this option is given, the output file is changed\n\
                so that waveform data is stored in columns while scalar data is stored in\n\
                parameters.\n\
-saveTimeStamps Specifies the name of a file to which to write raw time-stamp data for the\n\
                synchronously-acquired channels.  This can be useful in diagnosing the source\n\
                of poor sample alignment.\n\
-steps          specifies how many synchlogs will be taken, default is 1.\n\
-interval       specifies the waiting time between two synchlog steps.\n\
Program by M. Borland, ANL/APS.\n\
Link date: "__DATE__
  " "__TIME__
  ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

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

#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2
static char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double"};

typedef struct
{
  char *controlName;
  chid channelID;
  long sampleIndex, waveformLength, samplesAllocated, firstValueSeen;
  double *timeValue, *dataValue, lastTimeValue;
  double **waveformData;
  /* used with channel access routines to give index via callback: */
  long usrValue;
} CHANNEL_INFO;

void SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **controlName, char **readbackName, char **readbackUnits,
                  long controlNames,
                  char **controlName1, char **readbackName1, char **readbackUnist1,
                  long controlNames1,
                  char **waveformControlName, char **waveformReadbackName,
                  char **waveformUnits, int32_t *waveformDataType, long waveformNames,
                  char **commentParameter, long comments,
                  long precision);
long getScalarMonitorDataModifiedWithOffset(char ***DeviceName, char ***ReadMessage,
                                            char ***ReadbackName, char ***ReadbackUnits,
                                            double **scaleFactor, double **DeadBands,
                                            double **timeOffset,
                                            long *variables, char *inputFile, unsigned long mode,
                                            chid **DeviceCHID, double pendIOtime);
long InitiateConnections(char **cName, long cNames,
                         char **slowCName, long slowCNames,
                         char **wfCName, long wfCNames, long wfLength,
                         CHANNEL_INFO **chInfo0,
                         long samplesWanted);
void EventHandler(struct event_handler_args event);
void LogUnconnectedEvents(void);
long CheckTimeStampedData(CHANNEL_INFO *chInfo, long channels, double deltaLimit);
void WriteTimeStampsToFile(char *filename, CHANNEL_INFO *chInfo, long channels, long step, long steps);
void ResetSampleCounters();

static long controlNames, commonSampleIndex, samplesWanted, *channelsSampled, commonSamplesAllocated, callbacks,
  maxSampleIndex, readyToCollect = 0, slowControlNames, waveformControlNames, waveformLength = 0;
static CHANNEL_INFO *chInfo;
static double synchLimit = 0;
static double StartTime, StartDay, StartHour, StartJulianDay, StartMonth, StartYear;
static char *StartTimeStamp;

/* offset in seconds between EPICS start-of-EPOCH and UNIX start-of-EPOCH */
#define EPOCH_OFFSET 631173600
/* total offset to local time */
double timeOffset;
double *channelTimeOffset = NULL;
SDDS_DATASET SDDS_timestamp;
short verbose = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET logDataset;
  char *inputFile, *outputFile, **controlName = NULL, **readbackName = NULL, **readbackUnits = NULL, *timeStampsFile;
  char *slowDataFile, **slowControlName = NULL, **slowReadbackName = NULL, **slowReadbackUnits = NULL;
  char *waveformDataFile, **waveformControlName, **waveformReadbackName, **waveformReadbackUnits = NULL;
  int32_t *waveformDataType = NULL;
  char **commentParameter, **commentText;
  long comments, validSamples, channel, precision, steps, step;
  int32_t *sampleNumberData = NULL;
  double zeroTime, theTime, connectTimeout, nonConnectsHandled, *scaleFactor, timeLimit, endTime, interval;
  double timeLeft, *slowScaleFactor;
  short eraseFile;
  long i_arg, timeUnits, i = 0;
  /*extern time_t timezone;*/
  /*long slowSampleIndex; */
  double **filledSlowData = NULL;
  chid *slowControlCHID = NULL, *controlCHID = NULL;

  SDDS_RegisterProgramName(argv[0]);

  timeOffset = EPOCH_OFFSET - epicsSecondsWestOfUTC();

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s", USAGE1, USAGE2);
    exit(1);
    /*  bomb(NULL, USAGE);*/
  }
  steps = 1;
  interval = 1;
  slowDataFile = NULL;
  waveformDataFile = NULL;
  pendEventTime = DEFAULT_PEND_EVENT;
  verbose = 0;
  samplesWanted = 0;
  inputFile = outputFile = NULL;
  connectTimeout = 60;
  eraseFile = 0;
  commentParameter = commentText = NULL;
  comments = 0;
  timeLimit = 0;
  precision = PRECISION_SINGLE;
  synchLimit = 0.001;
  /*slowSampleIndex = 0; */
  timeStampsFile = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_SAMPLES:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%ld", &samplesWanted) != 1 ||
            samplesWanted <= 0)
          SDDS_Bomb("invalid -samples syntax/values");
        break;
      case CLO_VERBOSE:
        verbose += 1;
        break;
      case CLO_PENDEVENTTIME:
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &pendEventTime) != 1 ||
            pendEventTime <= 0)
          SDDS_Bomb("invalid -pendIOTime syntax/values");
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
      case CLO_COMMENT:
        ProcessCommentOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1,
                             &commentParameter, &commentText, &comments);
        break;
      case CLO_TIMELIMIT:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &timeLimit) != 1 ||
            timeLimit <= 0)
          SDDS_Bomb("invalid -timeLimit syntax/values");
        if (s_arg[i_arg].n_items == 3) {
          if ((timeUnits = match_string(s_arg[i_arg].list[2], TimeUnitName, NTimeUnitNames, 0)) >= 0)
            timeLimit *= TimeUnitFactor[timeUnits];
          else
            SDDS_Bomb("invalid -timeLimit syntax/values");
        }
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items != 2 || !s_arg[i_arg].list[1])
          bomb("no value given for option -precision", NULL);
        switch (precision = match_string(s_arg[i_arg].list[1], precision_option, PRECISION_OPTIONS, 0)) {
        case PRECISION_SINGLE:
        case PRECISION_DOUBLE:
          break;
        default:
          printf("unrecognized precision value given for option %s\n", s_arg[i_arg].list[0]);
          exit(1);
        }
        break;
      case CLO_SYNCHINTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &synchLimit) != 1 ||
            synchLimit <= 0)
          SDDS_Bomb("invalid -synchInterval syntax/values");
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            !get_long(&steps, s_arg[i_arg].list[1]) ||
            steps <= 0)
          SDDS_Bomb("invalid -steps syntax/values");
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items < 2 || s_arg[i_arg].n_items > 3 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &interval) != 1 ||
            interval <= 0)
          SDDS_Bomb("invalid -interval syntax/values");
        if (s_arg[i_arg].n_items == 3) {
          if ((timeUnits = match_string(s_arg[i_arg].list[2], TimeUnitName, NTimeUnitNames, 0)) >= 0)
            interval *= TimeUnitFactor[timeUnits];
          else
            SDDS_Bomb("invalid -interval syntax/values");
        }
        break;
      case CLO_SLOWDATA:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -slowData syntax/values");
        if (!fexists(slowDataFile = s_arg[i_arg].list[1]))
          SDDS_Bomb("slowData file not found");
        break;
      case CLO_WAVEFORMDATA:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -waveformData syntax/values");
        if (!fexists(waveformDataFile = s_arg[i_arg].list[1]))
          SDDS_Bomb("waveformData file not found");
        break;
      case CLO_TIMESTAMPS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -timeStamps syntax/values");
        timeStampsFile = s_arg[i_arg].list[1];
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputFile)
        SDDS_CopyString(&inputFile, s_arg[i_arg].list[0]);
      else if (!outputFile)
        SDDS_CopyString(&outputFile, s_arg[i_arg].list[0]);
      else
        SDDS_Bomb("too many filenames given");
    }
  }
#if DEBUG
  fprintf(stderr, "Argument parsing done.\n");
#endif

  if (!inputFile)
    SDDS_Bomb("input filename not given");
  if (!outputFile)
    SDDS_Bomb("output filename not given");
  if (fexists(outputFile) && !eraseFile)
    SDDS_Bomb("output file already exists");
  if (samplesWanted <= 0)
    SDDS_Bomb("number of samples not provided");
  if (timeLimit <= 0)
    SDDS_Bomb("you must give a positive timeLimit or risk death by infinite loop.");

  if (comments)
    fprintf(stderr, "warning: comments ignored for -append mode\n");
#if DEBUG
  fprintf(stderr, "Argument checking done; output file will be %s.\n", outputFile);
#endif

  if (!getScalarMonitorDataModifiedWithOffset(&controlName, NULL, &readbackName,
                                              &readbackUnits, &scaleFactor, NULL, &channelTimeOffset,
                                              &controlNames, inputFile, 0, &controlCHID, pendEventTime))
    SDDS_Bomb("problem reading data from input file");

  slowControlName = slowReadbackName = NULL;
  slowControlNames = 0;
  if (slowDataFile &&
      !getScalarMonitorData(&slowControlName, NULL, &slowReadbackName,
                            &slowReadbackUnits, &slowScaleFactor, NULL,
                            &slowControlNames, slowDataFile, 0, &slowControlCHID, pendEventTime))
    SDDS_Bomb("problem reading data from slowData input file");

  waveformControlName = waveformReadbackName = NULL;
  waveformControlNames = 0;
  if (waveformDataFile) {
    if (!getWaveformMonitorData(&waveformControlName, &waveformReadbackName,
                                &waveformReadbackUnits, NULL,
                                NULL, NULL, &waveformDataType, &waveformControlNames,
                                &waveformLength, waveformDataFile, SDDS_FLOAT)) {
      SDDS_Bomb("problem reading data from waveform input file");
    }
    if (waveformLength <= 0)
      SDDS_Bomb("problem with WaveformLength in waveform input file: long parameter must exist and value must be positive");
  }

#if DEBUG
  fprintf(stderr, "Setting up log file for %ld control names,\n", controlNames);
  fprintf(stderr, "%ld slow control names, and %ld waveforms\n", slowControlNames, waveformControlNames);
#endif

  getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                   &StartYear, &StartTimeStamp);

  SetupLogFile(&logDataset, outputFile, inputFile,
               controlName, readbackName, readbackUnits, controlNames,
               slowControlName, slowReadbackName, slowReadbackUnits, slowControlNames,
               waveformControlName, waveformReadbackName, waveformReadbackUnits,
               waveformDataType, waveformControlNames,
               commentParameter, comments,
               precision);

  /* zeroTime is when the clock for this run starts, as opposed to startTime, which is when
   * this run or the run for which this is a continuation opened the output file 
   */
  endTime = (zeroTime = getTimeInSecs()) + timeLimit;
  nonConnectsHandled = 0;
  maxSampleIndex = commonSampleIndex = callbacks = 0;

  readyToCollect = 0;

  if (!InitiateConnections(controlName, controlNames,
                           slowControlName, slowControlNames,
                           waveformControlName, waveformControlNames, waveformLength,
                           &chInfo, samplesWanted))
    SDDS_Bomb("unable to establish callbacks for PVs");
  readyToCollect = 1; /* event handler will now process events */
  timeLeft = endTime - zeroTime;
  for (step = 0; timeLeft > 0 && step < steps; step++) {
    commonSampleIndex = maxSampleIndex = 0;
    if (steps > 1 && verbose)
      fprintf(stderr, "step %ld\n", step);
    ResetSampleCounters();
    while (commonSampleIndex < (samplesWanted - 1) &&
           (timeLeft = endTime - (theTime = getTimeInSecs())) > 0) {
      if (!nonConnectsHandled && (long)(theTime - zeroTime) > connectTimeout) {
        LogUnconnectedEvents();
        nonConnectsHandled = 1;
      }
      ca_pend_event(pendEventTime);
      if (verbose > 1) {
        fprintf(stderr,
                "%ld callbacks and %ld samples (%ld max) taken with %.2f seconds left to run\n",
                callbacks, commonSampleIndex + 1, maxSampleIndex + 1, timeLeft);
        for (i = 0; i < controlNames; i++) {
          if ((maxSampleIndex - chInfo[i].sampleIndex) > 1)
            fprintf(stderr, "%s has only %ld samples\n",
                    chInfo[i].controlName, chInfo[i].sampleIndex);
        }
        for (i = 0; i < waveformControlNames; i++) {
          if ((maxSampleIndex - chInfo[i + controlNames].sampleIndex) > 1)
            fprintf(stderr, "%s has only %ld samples\n",
                    chInfo[i + controlNames].controlName, chInfo[i + controlNames].sampleIndex);
        }
      }
    }
    if (verbose)
      fprintf(stderr, "%ld samples accumulated\n", commonSampleIndex + 1);
    if (verbose > 2)
      fprintf(stderr, "timeStampsFile = %s\n", timeStampsFile);
    if (timeStampsFile)
      WriteTimeStampsToFile(timeStampsFile, chInfo, controlNames + waveformControlNames,
                            step, steps);
    if ((validSamples = CheckTimeStampedData(chInfo, controlNames + waveformControlNames, synchLimit)) < 0)
      SDDS_Bomb("Problem aligning time stamped data");

    if (verbose)
      fprintf(stderr, "%ld aligned samples found\n", validSamples);
    if (!(sampleNumberData = SDDS_Malloc(sizeof(*sampleNumberData) * validSamples)))
      SDDS_Bomb("Memory allocation failure\n");
    for (i = 0; i < validSamples; i++)
      sampleNumberData[i] = i;
    if (slowControlNames) {
      long code;
      /* apply scale factors and adjust time values for slow samples */
      if (!(filledSlowData = SDDS_Malloc(sizeof(*filledSlowData) * slowControlNames)))
        SDDS_Bomb("memory allocation failure");
      for (channel = 0; channel < slowControlNames; channel++) {
        if (!(filledSlowData[channel] = SDDS_Malloc(sizeof(**filledSlowData) * validSamples)))
          SDDS_Bomb("memory allocation failure");
        if (slowScaleFactor && (slowScaleFactor[channel] != 0))
          for (i = 0; i < chInfo[channel + controlNames + waveformControlNames].sampleIndex; i++)
            chInfo[channel + controlNames].dataValue[i] *= slowScaleFactor[channel];
        for (i = 0; i < chInfo[channel + controlNames + waveformControlNames].sampleIndex; i++) {
          if (i == 0)
            /* this is necessary to avoid bizarre interpolation, as the initial sample
                     * time for the "slow" channel may be a very long time ago (last time the
                     * channel updated)
                     */
            chInfo[channel + controlNames + waveformControlNames].timeValue[i] = zeroTime;
          else
            chInfo[channel + controlNames + waveformControlNames].timeValue[i] += timeOffset;
        }
        /* interpolate the slow data values at the timestamp values of the synchronous data */
        for (i = 0; i < validSamples; i++) {
          filledSlowData[channel][i] = interp(chInfo[channel + controlNames + waveformControlNames].dataValue,
                                              chInfo[channel + controlNames + waveformControlNames].timeValue,
                                              chInfo[channel + controlNames + waveformControlNames].sampleIndex,
                                              chInfo[0].timeValue[i],
                                              0, 1, &code);
        }
      }
    }
    if (scaleFactor)
      /* apply scale factors to synchronized scalar data */
      for (channel = 0; channel < controlNames; channel++)
        if (scaleFactor[channel] != 0)
          for (i = 0; i < validSamples; i++)
            chInfo[channel].dataValue[i] *= scaleFactor[channel];
    if (!waveformControlNames) {
      /* in this case, the scalar data goes into the columns of the file */
      if (!SDDS_StartPage(&logDataset, samplesWanted) ||
          !SDDS_SetParameters(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "TimeStamp", StartTimeStamp, "PageTimeStamp", StartTimeStamp,
                              "StartTime", StartTime, "StartYear", (short)StartYear,
                              "YearStartTime", computeYearStartTime(StartTime),
                              "StartJulianDay", (short)StartJulianDay, "StartMonth", (short)StartMonth,
                              "StartDayOfMonth", (short)StartDay, "StartHour", (float)StartHour,
                              NULL) ||
          !SetCommentParameters(&logDataset, commentParameter, commentText, comments) ||
          !SDDS_SetColumnFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                     chInfo[0].timeValue, validSamples, "Time") ||
          !SDDS_SetColumnFromLongs(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                   sampleNumberData, validSamples, "Sample"))
        SDDS_Bomb("Problem setting column data.");
      for (channel = 0; channel < controlNames; channel++)
        if (!SDDS_SetColumnFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                       chInfo[channel].dataValue, validSamples, readbackName[channel]))
          SDDS_Bomb("Problem setting column data.");
      for (channel = 0; channel < slowControlNames; channel++)
        if (!SDDS_SetColumnFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                       filledSlowData[channel],
                                       validSamples, slowReadbackName[channel]))
          SDDS_Bomb("Problem setting column data.");
      if (!SDDS_WritePage(&logDataset))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    } else {
      /* in this case, the waveform data goes into the columns, whereas the scalar data goes
           * into parameters 
           */
      for (i = 0; i < validSamples; i++) {
        if (!SDDS_StartPage(&logDataset, waveformLength))
          SDDS_Bomb("Problem starting page");
        if (!SDDS_SetParametersFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                           "Time", chInfo[0].timeValue[i], NULL))
          SDDS_Bomb("Problem setting parameter data (1a).");
        if (!SDDS_SetParameters(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "Sample", sampleNumberData[i], NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          SDDS_Bomb("Problem setting parameter data (1b).");
        }
        for (channel = 0; channel < controlNames; channel++)
          if (!SDDS_SetParametersFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                             readbackName[channel], chInfo[channel].dataValue[i],
                                             NULL))
            SDDS_Bomb("Problem setting parameter data (2).");
        for (channel = 0; channel < slowControlNames; channel++)
          if (!SDDS_SetParametersFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                             slowReadbackName[channel], filledSlowData[channel][i],
                                             NULL))
            SDDS_Bomb("Problem setting parameter data (3).");
        for (channel = 0; channel < waveformControlNames; channel++) {
          if (!SDDS_SetColumnFromDoubles(&logDataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                         chInfo[channel + controlNames].waveformData[i],
                                         waveformLength, waveformReadbackName[channel]))
            SDDS_Bomb("Problem setting waveform column data (4).");
        }
        if (!SDDS_WritePage(&logDataset))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    if (validSamples) {
      free(sampleNumberData);
      sampleNumberData = NULL;
      if (filledSlowData) {
        for (channel = 0; channel < slowControlNames; channel++) {
          free(filledSlowData[channel]);
          filledSlowData[channel] = NULL;
        }
        free(filledSlowData);
        filledSlowData = NULL;
      }
    }

    if (steps > 1 && step != steps - 1) {
      if (verbose)
        fprintf(stderr, "waiting for %f seconds...\n", interval);
      ca_pend_event(interval);
    }
  }

  if (!SDDS_Terminate(&logDataset))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  ca_task_exit();

  if (inputFile)
    free(inputFile);
  if (outputFile)
    free(outputFile);
  if (controlName) {
    for (i = 0; i < controlNames; i++)
      free(controlName[i]);
    free(controlName);
  }
  if (readbackName) {
    for (i = 0; i < controlNames; i++)
      free(readbackName[i]);
    free(readbackName);
  }
  if (readbackUnits) {
    for (i = 0; i < controlNames; i++)
      free(readbackUnits[i]);
    free(readbackUnits);
  }
  if (controlCHID)
    free(controlCHID);
  if (slowControlName) {
    for (i = 0; i < slowControlNames; i++)
      free(slowControlName[i]);
    free(slowControlName);
  }
  if (slowReadbackName) {
    for (i = 0; i < slowControlNames; i++)
      free(slowReadbackName[i]);
    free(slowReadbackName);
  }
  if (slowReadbackUnits) {
    for (i = 0; i < slowControlNames; i++)
      free(slowReadbackUnits[i]);
    free(slowReadbackUnits);
  }
  if (slowControlCHID)
    free(slowControlCHID);
  if (commentParameter)
    free(commentParameter);
  if (commentText)
    free(commentText);
  for (channel = 0; channel < waveformControlNames; channel++) {
    free(chInfo[channel + controlNames].waveformData[i]);
    free(waveformControlName[channel]);
    if (waveformReadbackName && waveformReadbackName[channel])
      free(waveformReadbackName[channel]);
    if (waveformReadbackUnits && waveformReadbackUnits[channel])
      free(waveformReadbackUnits[channel]);
  }
  if (waveformControlName)
    free(waveformControlName);
  if (waveformReadbackName)
    free(waveformReadbackName);
  if (waveformReadbackUnits)
    free(waveformReadbackUnits);

  free_scanargs(&s_arg, argc);
  return 0;
}

typedef struct
{
  double timeStamp, deltaLimit;
  long members, *channel, *sample;
} TIME_STAMP_INDEX;

int timeStampIndexCmp(void const *a1, void const *a2) {
  TIME_STAMP_INDEX *tsi1, *tsi2;
  double diff;
  tsi1 = (TIME_STAMP_INDEX *)a1;
  tsi2 = (TIME_STAMP_INDEX *)a2;
  diff = tsi1->timeStamp - tsi2->timeStamp;
  if (diff > tsi1->deltaLimit / 2)
    return 1;
  if (diff < -tsi1->deltaLimit / 2)
    return -1;
  return 0;
}

void WriteTimeStampsToFile(char *filename, CHANNEL_INFO *chInfo, long channels, long step, long steps) {
  long channel, sample;
  long maxSamples = 0;

  if (verbose > 2)
    fprintf(stderr, "Writing time stamps output file %s (step=%ld)\n", filename, step);
  if (!step) {
    if (verbose > 3)
      fprintf(stderr, "Setting up time stamps output file %s\n", filename);
    if (!SDDS_InitializeOutput(&SDDS_timestamp, SDDS_BINARY, 1, NULL, NULL, filename))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_EnableFSync(&SDDS_timestamp);
    for (channel = 0; channel < channels; channel++) {
      if (!SDDS_DefineSimpleColumn(&SDDS_timestamp, chInfo[channel].controlName,
                                   "s", SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_WriteLayout(&SDDS_timestamp))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  for (channel = 0; channel < channels; channel++)
    if (chInfo[channel].sampleIndex > maxSamples)
      maxSamples = chInfo[channel].sampleIndex;
  maxSamples += 1;
  if (verbose > 2)
    fprintf(stderr, "Maximum number of samples is %ld\n", maxSamples);

  if (!SDDS_StartPage(&SDDS_timestamp, maxSamples + 1))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (channel = 0; channel < channels; channel++) {
    for (sample = 0; sample < chInfo[channel].sampleIndex; sample++) {
      if (!SDDS_SetRowValues(&SDDS_timestamp, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, sample,
                             chInfo[channel].controlName,
                             chInfo[channel].timeValue[sample] + timeOffset, NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }
  if (!SDDS_WritePage(&SDDS_timestamp))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (step == steps - 1 && !SDDS_Terminate(&SDDS_timestamp))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (verbose > 2)
    fprintf(stderr, "Done writing time stamp values\n");
}

long CheckTimeStampedData(CHANNEL_INFO *chInfo, long channels, double deltaLimit) {
  long validSamples = 0;
  long channel, sample, member;
  TIME_STAMP_INDEX **timeStamp, *newTimeStamp = NULL;
  int32_t duplicate;
  long timeStamps, maxTimeStamps, index;
  long maxSamples = 0;

  for (channel = 0; channel < channels; channel++)
    if (chInfo[channel].sampleIndex > maxSamples)
      maxSamples = chInfo[channel].sampleIndex;
  maxSamples += 1;

  if (!(timeStamp = SDDS_Malloc(sizeof(*timeStamp) * (maxTimeStamps = maxSamples * 2))))
    SDDS_Bomb("Memory allocation failure");
  /* Make list of time stamps and collect channel/sample pair data for
   * each time stamp 
   */
  for (channel = timeStamps = 0; channel < channels; channel++) {
    for (sample = 0; sample < chInfo[channel].sampleIndex; sample++) {
      /* Convert time value to EPICS time-since-epoch, for output to SDDS file.
           */
      chInfo[channel].timeValue[sample] += timeOffset;

      if (timeStamps == maxTimeStamps &&
          !(timeStamp = SDDS_Realloc(timeStamp, sizeof(*timeStamp) * (maxTimeStamps += maxSamples))))
        SDDS_Bomb("Memory allocation failure");
      if (!newTimeStamp &&
          !(newTimeStamp = SDDS_Calloc(1, sizeof(*newTimeStamp))))
        SDDS_Bomb("Memory allocation failure");
      newTimeStamp->timeStamp = chInfo[channel].timeValue[sample];
      newTimeStamp->deltaLimit = deltaLimit;
      newTimeStamp->members = 0;
      index = binaryInsert((void **)timeStamp, timeStamps, (void *)newTimeStamp,
                           timeStampIndexCmp, &duplicate);
      timeStamps += duplicate ? 0 : 1;

      if (duplicate) {
        /* Add channel/sample data to list for existing time stamp */
        if (!(timeStamp[index]->channel = SDDS_Realloc(timeStamp[index]->channel, sizeof(*timeStamp[index]->channel) * (maxSamples + timeStamp[index]->members))) ||
            !(timeStamp[index]->sample = SDDS_Realloc(timeStamp[index]->sample, sizeof(*timeStamp[index]->sample) * (maxSamples + timeStamp[index]->members)))) {
          SDDS_Bomb("Memory allocation failure");
        }
        timeStamp[index]->channel[timeStamp[index]->members] = channel;
        timeStamp[index]->sample[timeStamp[index]->members] = sample;
        if ((timeStamp[index]->members += 1) > channels)
          SDDS_Bomb("Time stamp problem: too many samples have the same time stamp\nConsider reducing -synchLimit.");
      } else {
        /* Start channel/sample list for new time stamp */
        timeStamp[index]->members = 1;
        newTimeStamp = NULL;
        if (!(timeStamp[index]->channel = SDDS_Malloc(sizeof(*timeStamp[index]->channel) *
                                                      maxSamples)) ||
            !(timeStamp[index]->sample = SDDS_Malloc(sizeof(*timeStamp[index]->sample) *
                                                     maxSamples))) {
          SDDS_Bomb("Memory allocation failure");
        }
        timeStamp[index]->channel[0] = channel;
        timeStamp[index]->sample[0] = sample;
      }
    }
  }

#ifdef DEBUG
  fprintf(stderr, "%ld unique time stamps found\n", timeStamps);
  for (index = 0; index < timeStamps; index++) {
    fprintf(stderr, "   %18.6f  (%ld)\t", timeStamp[index]->timeStamp,
            timeStamp[index]->members);
    for (sample = 0; sample < timeStamp[index]->members; sample++) {
      fprintf(stderr, "%ld/%ld  ",
              timeStamp[index]->sample[sample], timeStamp[index]->channel[sample]);
    }
    fputs("\n", stderr);
  }
#endif

  for (index = 0; index < timeStamps; index++) {
    if (timeStamp[index]->members != channels)
      /* Too few channels recorded for this time stamp.  Discard data */
      continue;
    for (member = 0; member < timeStamp[index]->members; member++) {
      sample = timeStamp[index]->sample[member];
      channel = timeStamp[index]->channel[member];
      if (sample != validSamples) {
        /* copy the time stamp and data value to an earlier slot in the array */
        /* this may copy over invalid data that didn't align */
        if (sample < validSamples)
          SDDS_Bomb("index alignment problem.  Seek expert help.");
        chInfo[channel].timeValue[validSamples] =
          chInfo[channel].timeValue[sample];
        if (!chInfo[channel].waveformLength)
          chInfo[channel].dataValue[validSamples] =
            chInfo[channel].dataValue[sample];
        else
          SWAP_PTR(chInfo[channel].waveformData[validSamples],
                   chInfo[channel].waveformData[sample]);
      }
    }
    validSamples++;
    free(timeStamp[index]->sample);
    free(timeStamp[index]->channel);
    free(timeStamp[index]);
  }
  free(timeStamp);

  return validSamples;
}

void LogUnconnectedEvents(void) {
  long i;
  fprintf(stderr, "Checking for unconnected channels\n");

  for (i = 0; i < controlNames; i++) {
    if (ca_state(chInfo[i].channelID) == cs_conn)
      continue;
    fprintf(stderr, "Fatal error: the following channel did not connect: %s\n",
            chInfo[i].controlName);
    exit(1);
  }
  for (i = 0; i < slowControlNames; i++) {
    if (ca_state(chInfo[i + controlNames].channelID) == cs_conn)
      continue;
    fprintf(stderr, "Fatal error: the following channel did not connect: %s\n",
            chInfo[i + controlNames].controlName);
    exit(1);
  }
}

void EventHandler(struct event_handler_args event) {
  struct dbr_time_double *dbrValue;
  long index;
  double timeValue;
  TS_STAMP *tsStamp;

  callbacks++;

  if (commonSampleIndex >= samplesWanted)
    return;

  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_double *)event.dbr;
  tsStamp = &(dbrValue->stamp);

  if (!chInfo[index].firstValueSeen) {
    chInfo[index].firstValueSeen = 1;
    return;
  }

  if (!readyToCollect)
    return;

  if (verbose > 3)
    fprintf(stderr, "EventHandler: %32s, time=%d.%d, value=%le\n",
            chInfo[index].controlName, tsStamp->secPastEpoch, tsStamp->nsec, dbrValue->value);

  if (chInfo[index].sampleIndex >= chInfo[index].samplesAllocated) {
#ifdef DEBUG
    printf("Reallocating memory for %s (sampleIndex=%ld, samplesAllocated=%ld)\n", chInfo[index].controlName,
           chInfo[index].sampleIndex, chInfo[index].samplesAllocated);
    printf("dataValue = %x, timeValue = %x\n", chInfo[index].dataValue, chInfo[index].timeValue);
    fflush(stdout);
#endif
    if (!(chInfo[index].dataValue =
            SDDS_Realloc(chInfo[index].dataValue,
                         2 * sizeof(chInfo[index].dataValue[0]) * chInfo[index].samplesAllocated)) ||
        !(chInfo[index].timeValue =
            SDDS_Realloc(chInfo[index].timeValue,
                         2 * sizeof(chInfo[index].timeValue[0]) * chInfo[index].samplesAllocated)))
      SDDS_Bomb("memory allocation failure (reallocating slow data array)");
    chInfo[index].samplesAllocated *= 2;
#ifdef DEBUG
    printf("Done reallocating memory for %s\n", chInfo[index].controlName);
    printf("dataValue = %x, timeValue = %x\n", chInfo[index].dataValue, chInfo[index].timeValue);
    fflush(stdout);
#endif
  }

  timeValue = chInfo[index].timeValue[chInfo[index].sampleIndex] = tsStamp->secPastEpoch + 1e-9 * tsStamp->nsec;
  if (index >= controlNames + waveformControlNames) {
    /* slow channel */
    chInfo[index].dataValue[chInfo[index].sampleIndex] = dbrValue->value;
    chInfo[index].lastTimeValue = timeValue;
    chInfo[index].sampleIndex++;
  } else {
    if (index < controlNames && channelTimeOffset)
      chInfo[index].timeValue[chInfo[index].sampleIndex] =
        (timeValue += channelTimeOffset[index]);
    if (fabs(timeValue - chInfo[index].lastTimeValue) > synchLimit / 2) {
      /* don't accept samples too close to the last sample
         */
      if (!chInfo[index].waveformLength)
        chInfo[index].dataValue[chInfo[index].sampleIndex] = dbrValue->value;
      else
        memcpy(chInfo[index].waveformData[chInfo[index].sampleIndex],
               &dbrValue->value,
               sizeof(chInfo[index].waveformData[chInfo[index].sampleIndex][0]) * chInfo[index].waveformLength);
      chInfo[index].lastTimeValue = timeValue;
      if (chInfo[index].sampleIndex >= commonSamplesAllocated) {
        channelsSampled = SDDS_Realloc(channelsSampled, 2 * sizeof(*channelsSampled) * commonSamplesAllocated);
        memset(channelsSampled + commonSamplesAllocated, 0, sizeof(*channelsSampled) * commonSamplesAllocated);
        commonSamplesAllocated *= 2;
      }
      if ((channelsSampled[chInfo[index].sampleIndex] += 1) == controlNames)
        commonSampleIndex = chInfo[index].sampleIndex;
      if (chInfo[index].sampleIndex > maxSampleIndex)
        maxSampleIndex = chInfo[index].sampleIndex;
      chInfo[index].sampleIndex++;
    }
  }
}

long InitiateConnections(char **cName, long cNames,
                         char **slowCName, long slowCNames,
                         char **wfCName, long wfCNames, long wfLength,
                         CHANNEL_INFO **chInfo0,
                         long samplesWanted) {
  long i, j;
  CHANNEL_INFO *chInfo;

  ca_task_initialize();
#ifdef DEBUG
  fprintf(stderr, "initiating connections for %ld control names\n", cNames);
#endif
  if (!(chInfo = (CHANNEL_INFO *)SDDS_Malloc(sizeof(*chInfo) * (cNames + slowCNames + wfCNames))))
    return 0;
  if (!(channelsSampled = SDDS_Calloc(samplesWanted, sizeof(*channelsSampled))))
    return 0;
  commonSamplesAllocated = samplesWanted;
  *chInfo0 = chInfo;
  /* record info about synch channels and set up search */
  for (i = 0; i < cNames; i++) {
    chInfo[i].controlName = cName[i];
    chInfo[i].waveformLength = 0;
    chInfo[i].usrValue = i;
    chInfo[i].lastTimeValue = -1;
    chInfo[i].sampleIndex = chInfo[i].firstValueSeen = 0;
    if (!(chInfo[i].timeValue = SDDS_Malloc(sizeof(*(chInfo[i].timeValue)) * samplesWanted)) ||
        !(chInfo[i].dataValue = SDDS_Malloc(sizeof(*(chInfo[i].dataValue)) * samplesWanted))) {
      SDDS_Bomb("memory allocation failure");
    }
#ifdef DEBUG
    printf("Allocated memory for %ld samples for %s: %x, %x\n",
           samplesWanted, chInfo[i].controlName, chInfo[i].dataValue, chInfo[i].timeValue);
    fflush(stdout);
#endif
    chInfo[i].samplesAllocated = samplesWanted;
    if (ca_search(cName[i], &chInfo[i].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", cName[i]);
      return 0;
    }
  }

#ifdef DEBUG
  fprintf(stderr, "Initiating connections for %ld waveforms of length %ld\n", wfCNames, wfLength);
#endif
  /* record info about waveform channels a setu up search */
  for (i = 0; i < wfCNames; i++) {
    chInfo[i + cNames].controlName = wfCName[i];
    chInfo[i + cNames].waveformLength = wfLength;
    chInfo[i + cNames].usrValue = i + cNames;
    chInfo[i + cNames].lastTimeValue = -1;
    chInfo[i + cNames].sampleIndex = 0;
    if (!(chInfo[i + cNames].timeValue = SDDS_Malloc(sizeof(*(chInfo[i + cNames].timeValue)) * samplesWanted)) ||
        !(chInfo[i + cNames].waveformData = SDDS_Malloc(sizeof(*(chInfo[i + cNames].waveformData)) * samplesWanted))) {
      SDDS_Bomb("memory allocation failure");
    }
    for (j = 0; j < samplesWanted; j++)
      if (!(chInfo[i + cNames].waveformData[j] =
              SDDS_Malloc(sizeof(chInfo[i + cNames].waveformData[j][0]) * wfLength))) {
        SDDS_Bomb("memory allocation failure");
      }
    chInfo[i + cNames].samplesAllocated = samplesWanted;
    if (ca_search(wfCName[i], &chInfo[i + cNames].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", wfCName[i]);
      return 0;
    }
  }
#ifdef DEBUG
  fprintf(stderr, "Initiating connections for %ld slow channels\n", slowCNames);
#endif
  /* record info about slow channels and set up search */
  for (i = 0; i < slowCNames; i++) {
    chInfo[i + cNames + wfCNames].controlName = slowCName[i];
    chInfo[i + cNames + wfCNames].waveformLength = 0;
    chInfo[i + cNames + wfCNames].usrValue = i + cNames + wfCNames;
    chInfo[i + cNames + wfCNames].lastTimeValue = -1;
    chInfo[i + cNames + wfCNames].sampleIndex = 0;
    /* allow twice as many samples as indicated since the rate is different on these */
    /* the are nonsynchronous, not necessarily slow */
    if (!(chInfo[i + cNames + wfCNames].timeValue =
            SDDS_Malloc(2 * sizeof(*(chInfo[i + cNames + wfCNames].timeValue)) * samplesWanted)) ||
        !(chInfo[i + cNames + wfCNames].dataValue =
            SDDS_Malloc(2 * sizeof(*(chInfo[i + cNames + wfCNames].dataValue)) * samplesWanted))) {
      SDDS_Bomb("memory allocation failure");
    }
    chInfo[i + cNames + wfCNames].samplesAllocated = 2 * samplesWanted;
    if (ca_search(slowCName[i], &chInfo[i + cNames + wfCNames].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", slowCName[i]);
      return 0;
    }
  }
  /* do search */
  ca_pend_event(0.0001);

  /* set up callbacks for the channels */
  for (i = 0; i < cNames + wfCNames + slowCNames; i++) {
    if (!chInfo[i].waveformLength) {
      if (ca_add_event(DBR_TIME_DOUBLE, chInfo[i].channelID, EventHandler,
                       (void *)&chInfo[i].usrValue, NULL) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to setup callback for control name %s\n", chInfo[i].controlName);
        exit(1);
      }
    } else {
      if (ca_add_array_event(DBR_TIME_DOUBLE, chInfo[i].waveformLength,
                             chInfo[i].channelID, EventHandler,
                             (void *)&chInfo[i].usrValue,
                             (double)0.0, (double)0.0, (double)0.0,
                             NULL) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to setup callback for control name %s\n", chInfo[i].controlName);
        exit(1);
      }
    }
  }
  ca_pend_event(0.0001);
  return 1;
}

void SetupLogFile(SDDS_DATASET *logSet, char *output, char *input,
                  char **controlName, char **readbackName, char **readbackUnits,
                  long controlNames,
                  char **controlName1, char **readbackName1, char **readbackUnits1,
                  long controlNames1,
                  char **waveformControlName, char **waveformReadbackName,
                  char **waveformUnits, int32_t *waveformDataType, long waveformNames,
                  char **commentParameter, long comments,
                  long precision) {
  long dataType;

  if (!SDDS_InitializeOutput(logSet, SDDS_BINARY, 1, NULL, NULL, output) ||
      !DefineLoggingTimeParameters(logSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_EnableFSync(logSet);
  dataType = SDDS_DOUBLE;
  if (precision == PRECISION_SINGLE)
    dataType = SDDS_FLOAT;

  if (!waveformNames) {
    if (0 > SDDS_DefineColumn(logSet, "Sample", NULL, NULL, "Sample number", NULL, SDDS_LONG, 0) ||
        0 > SDDS_DefineColumn(logSet, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleColumns(logSet, controlNames, readbackName, readbackUnits, dataType) ||
        (controlNames1 &&
         !SDDS_DefineSimpleColumns(logSet, controlNames1, readbackName1, readbackUnits1, dataType)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleParameters(logSet, comments, commentParameter, NULL, SDDS_STRING) ||
        !SDDS_WriteLayout(logSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    long i;
    for (i = 0; i < waveformNames; i++)
      if (!SDDS_DefineSimpleColumn(logSet, waveformReadbackName[i],
                                   waveformUnits ? waveformUnits[i] : NULL,
                                   waveformDataType[i]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (0 > SDDS_DefineParameter(logSet, "Sample", NULL, NULL, "Sample number", NULL, SDDS_LONG, 0) ||
        0 > SDDS_DefineParameter(logSet, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleParameters(logSet, controlNames, readbackName, readbackUnits, dataType) ||
        (controlNames1 &&
         !SDDS_DefineSimpleParameters(logSet, controlNames1, readbackName1, readbackUnits1, dataType)))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleParameters(logSet, comments, commentParameter, NULL, SDDS_STRING) ||
        !SDDS_WriteLayout(logSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
}

long getScalarMonitorDataModifiedWithOffset(char ***DeviceName, char ***ReadMessage,
                                            char ***ReadbackName, char ***ReadbackUnits,
                                            double **scaleFactor, double **DeadBands,
                                            double **timeOffset,
                                            long *variables, char *inputFile, unsigned long mode,
                                            chid **DeviceCHID, double pendIOtime) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName, *MessageColumnName = NULL;
  short UnitsDefined, MessageDefined, ScaleFactorDefined, DeadBandDefined, TimeOffsetDefined;
  long i, noReadbackName, type;
  INFOBUF *info;
  *DeviceName = NULL;
  if (ReadMessage)
    *ReadMessage = NULL;
  if (ReadbackName)
    *ReadbackName = NULL;
  if (ReadbackUnits)
    *ReadbackUnits = NULL;
  if (scaleFactor)
    *scaleFactor = NULL;
  if (DeadBands)
    *DeadBands = NULL;
  if (DeviceCHID)
    *DeviceCHID = NULL;
  if (timeOffset)
    *timeOffset = NULL;
  *variables = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  noReadbackName = 1;
  if (ReadbackName) {
    noReadbackName = 0;
    if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)))
      noReadbackName = 1;
  }

  MessageDefined = 0;
  if (ReadMessage) {
    MessageDefined = 1;
    if (!(MessageColumnName =
            SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                            "ReadMessage", "Message", "ReadMsg", NULL))) {
      MessageDefined = 0;
    }
    if (MessageDefined)
      SDDS_Bomb("Messages (devSend) no longer supported.");
  }

  UnitsDefined = 0;
  if (ReadbackUnits) {
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
  }

  ScaleFactorDefined = 0;
  if (scaleFactor) {
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
  }

  DeadBandDefined = 0;
  if (DeadBands) {
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

  TimeOffsetDefined = 0;
  if (timeOffset) {
    switch (SDDS_CheckColumn(&inSet, "TimeOffset", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      TimeOffsetDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"TimeOffset\".\n");
      exit(1);
      break;
    }
  }

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;

  if (!(*variables = SDDS_CountRowsOfInterest(&inSet)))
    bomb("Zero rows defined in input file.\n", NULL);

  *DeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (noReadbackName)
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  else
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, "ReadbackName");
  if (ControlNameColumnName)
    free(ControlNameColumnName);

  if (MessageDefined)
    *ReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
  else if (ReadMessage) {
    *ReadMessage = (char **)malloc(sizeof(char *) * (*variables));
    for (i = 0; i < *variables; i++) {
      (*ReadMessage)[i] = (char *)malloc(sizeof(char));
      (*ReadMessage)[i][0] = 0;
    }
  }

  *DeviceCHID = (chid *)malloc(sizeof(chid) * (*variables));
  for (i = 0; i < *variables; i++)
    (*DeviceCHID)[i] = NULL;

  if (ReadbackUnits) {
    if (!(mode & GET_UNITS_FORCED) && UnitsDefined)
      *ReadbackUnits = (char **)SDDS_GetColumn(&inSet, "ReadbackUnits");
    else {
      *ReadbackUnits = (char **)malloc(sizeof(char *) * (*variables));
      for (i = 0; i < *variables; i++) {
        (*ReadbackUnits)[i] = (char *)malloc(sizeof(char) * (8 + 1));
        (*ReadbackUnits)[i][0] = 0;
      }
    }
    if (mode & GET_UNITS_FORCED || (mode & GET_UNITS_IF_NONE && !UnitsDefined) || mode & GET_UNITS_IF_BLANK) {
      info = (INFOBUF *)malloc(sizeof(INFOBUF) * (*variables));
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_search((*DeviceName)[i], &((*DeviceCHID)[i])) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for %s\n", (*DeviceName)[i]);
            exit(1);
          }
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for some channels\n");
        for (i = 0; i < *variables; i++) {
          if (ca_state((*DeviceCHID)[i]) != cs_conn)
            fprintf(stderr, "%s not connected\n", (*DeviceName)[i]);
        }
        /*  exit(1);*/
      }
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_state((*DeviceCHID)[i]) == cs_conn) {
            type = ca_field_type((*DeviceCHID)[i]);
            if ((type == DBF_STRING) || (type == DBF_ENUM))
              continue;
            if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                             (*DeviceCHID)[i], &info[i]) != ECA_NORMAL) {
              fprintf(stderr, "error: unable to read units.\n");
              exit(1);
            }
          }
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading units for some channels\n");
        /* exit(1);*/
      }
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_state((*DeviceCHID)[i]) == cs_conn) {
            type = ca_field_type((*DeviceCHID)[i]);
            switch (type) {
            case DBF_CHAR:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].c.units);
              break;
            case DBF_SHORT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].i.units);
              break;
            case DBF_LONG:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].l.units);
              break;
            case DBF_FLOAT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].f.units);
              break;
            case DBF_DOUBLE:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].d.units);
              break;
            default:
              break;
            }
          }
        }
      }
      free(info);
    }
  }

  if (scaleFactor && ScaleFactorDefined) {
    if (!(*scaleFactor = SDDS_GetColumnInDoubles(&inSet, "ScaleFactor")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < *variables; i++) {
      if ((*scaleFactor)[i] == 0) {
        fprintf(stderr, "Warning: scale factor for %s is 0.  Set to 1.\n",
                (*DeviceName)[i]);
        (*scaleFactor)[i] = 1;
      }
    }
  }

  if (DeadBands && DeadBandDefined &&
      !(*DeadBands = SDDS_GetColumnInDoubles(&inSet, "DeadBand")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (timeOffset && TimeOffsetDefined &&
      !(*timeOffset = SDDS_GetColumnInDoubles(&inSet, "TimeOffset")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_ReadPage(&inSet) > 0)
    fprintf(stderr, "Warning: extra pages in input file %s---they are ignored\n", inputFile);

  SDDS_Terminate(&inSet);
  return 1;
}

void ResetSampleCounters() {
  long i;
  for (i = 0; i < commonSamplesAllocated; i++)
    channelsSampled[i] = 0;
  for (i = 0; i < controlNames; i++)
    chInfo[i].sampleIndex = 0;
  for (i = 0; i < waveformControlNames; i++)
    chInfo[i + controlNames].sampleIndex = 0;
}
