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
#include <cerrno>
#include <csignal>

#if defined(_WIN32)
#  include <winsock.h>
#  include <process.h>
#  include <io.h>
#  include <sys/locking.h>
#else
#  include <unistd.h>
#  include <pwd.h>
#endif

#include "pvaSDDS.h"
#include "pv/thread.h"

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "matlib.h"
#include <cadef.h>

#include <libruncontrol.h>
#ifdef USE_LOGDAEMON
#include <logDaemonLib.h>
#endif

void interrupt_handler(int sig);
void interrupt_handler2(int sig);
void sigint_interrupt_handler(int sig);

#define CLO_GAIN 0
#define CLO_TIME_INTERVAL 1
#define CLO_STEPS 2
#define CLO_UPDATE_INTERVAL 3
#define CLO_INTEGRAL 4
#define CLO_PROPORTIONAL 5
#define CLO_VERBOSE 6
#define CLO_CONTROL_QUANTITY_DEFINITION 7
#define CLO_HOLD_PRESENT_VALUES 8
#define CLO_DRY_RUN 9
#define CLO_TEST_VALUES 10
#define CLO_WARNING 11
#define CLO_EZCATIMING 12
#define CLO_UNGROUPEZCACALLS 13
#define CLO_RUNCONTROLPV 14
#define CLO_RUNCONTROLDESC 15
#define CLO_ACTUATORCOL 16
#define CLO_DELTALIMIT 17
#define CLO_ACTIONLIMIT 18
#define CLO_AVERAGE 19
#define CLO_STATISTICS 20
#define CLO_DESPIKE 21
#define CLO_SERVERMODE 22
#define CLO_CONTROLLOG 23
#define CLO_OFFSETS 24
#define CLO_TESTCASECURITY 25
#define CLO_GLITCHLOG 26
#define CLO_GENERATIONS 27
#define CLO_DAILY_FILES 28
#define CLO_READBACK_LIMIT 29
#define CLO_OVERLAP_COMPENSATION 30
#define CLO_PVOFFSETS 31
#define CLO_WAVEFORMS 32
#define CLO_INFINITELOOP 33
#define CLO_PENDIOTIME 34
#define CLO_LAUNCHERPV 35
#define CLO_SEARCHPATH 36
#define CLO_THRESHOLD_RAMP 37
#define CLO_POST_CHANGE_EXECUTION 38
#define CLO_FILTERFILE 39
#define COMMANDLINE_OPTIONS 40

#define CLO_READBACKWAVEFORM 0
#define CLO_OFFSETWAVEFORM 1
#define CLO_ACTUATORWAVEFORM 2
#define CLO_FFSETPOINTWAVEFORM 3
#define CLO_TESTWAVEFORM 4
#define WAVEFORMOPTIONS 5

#define DEFAULT_AVEINTERVAL 1 /* one second default time interval */

#define LIMIT_VALUE 0x0001U
#define LIMIT_MINVALUE 0x0002U
#define LIMIT_MAXVALUE 0x0004U
#define LIMIT_MINMAXVALUE 0x0008U
#define LIMIT_FILE 0x0010U

#define DEFAULT_GAIN 0.1
#define DEFAULT_COMPENSATION_GAIN 1.0
#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 100
#define DEFAULT_UPDATE_INTERVAL 1
#define DEFAULT_TIMEOUT_ERRORS 5

#define DESPIKE_AVERAGEOF 0x0001U
#define DESPIKE_STARTTHRESHOLD 0x0002U
#define DESPIKE_ENDTHRESHOLD 0x0004U
#define DESPIKE_STEPSTHRESHOLD 0x0008U
#define DESPIKE_THRESHOLD_RAMP_PV 0x0010U

typedef struct
{
  chid channelID;
  int32_t waveformLength;
  double value, *waveformData;
  /* used with channel access routines to give index via callback: */
  long usrValue, flag, *count;
} CHANNEL_INFO;

typedef struct
{
  char *file;       /* not necessarily used in all instances */
  char *searchPath; /*point to the searchPath of loopParam */
  int64_t n;
  int32_t *despike, *waveformIndex, valueIndexes;
  short *waveformMatchFound, *setFlag;
  char **controlName, **symbolicName;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL pva;
  double **value, *setpoint, *error, *initial, *old, **delta;
  MATRIX *history; /* array of past values arranged in a matrix form */
  MATRIX *historyFiltered;
  long integral; /*the default is integral */
  /*integral (integral=1): control->value[0] +=control->delta[0],
    proportional(integral=0): control->value[0]=control->delta[0] */
} CONTROL_NAME;

typedef struct
{
  unsigned long flag;
  char *file, **controlName;
  char *searchPath; /*point to the searchPath of loopParam */
  long n, *index;
  double *value, *minValue, *maxValue;
  double singleValue, singleMinValue, singleMaxValue;
} LIMITS;

typedef struct
{
  char *PVparam, *DescParam;
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status, init;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;

typedef struct
{
  long *step, steps, integral, holdPresentValues, dryRun, compensationIntegral;
  long updateInterval, n; /*n is the number of readbacks */
  long briefStatistics;
  double interval, gain, compensationGain, *elapsedTime, *epochTime;
  char *searchPath;
  char *offsetFile;
  char *offsetPVFile;
  char **offsetPV, *gainPV, *intervalPV, *averagePV, *launcherPV[5];
  char *postChangeExec;
  CHANNEL_INFO *channelInfo, gainPVInfo, intervalPVInfo, averagePVInfo, launcherPVInfo[5];
  PVA_OVERALL pva, pvaGain, pvaInterval, pvaAverage, pvaLauncher[5];
 /*chid *channelID, gainPVID; */
  double *offsetPVvalue;
} LOOP_PARAM;

typedef struct
{
  char **waveformFile;
  char *searchPath; /*point to the searchPath of loopParam */
  long waveforms, waveformFiles;
  char **waveformPV, **writeWaveformPV;
  CHANNEL_INFO *channelInfo, *writeChannelInfo;
  PVA_OVERALL pvaChannel, pvaWriteChannel;
  int32_t **readbackIndex, *waveformLength;
  double **waveformData;
  double *readbackValue; /*the index is consistent with that of readbacks */
} WAVE_FORMS;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  char *coefFile;
  char *definitionFile; /*definition file for controls and readbacks */
  /* READBACK_NAME *readback; */
  CONTROL_NAME *control, *readback;
  char *actuator;
  MATRIX *K;
  MATRIX *aCoef;
  MATRIX *bCoef;
} CORRECTION;

typedef struct
{
  unsigned long flag;
  int32_t neighbors, passes, averageOf, countLimit, stepsThreshold;
  long reramp;
  double threshold, startThreshold, endThreshold, deltaThreshold;
  /* file which give despiking flag for readback PVs in matrix */
  char *file, **controlName, **symbolicName, *thresholdPV, *rampThresholdPV;
  char *searchPath; /*point to the searchPath of loopParam */
  int32_t n, *despike;
  CHANNEL_INFO thresholdPVInfo, rampThresholdPVInfo;
  PVA_OVERALL pvaThreshold, pvaRampThreshold;
} DESPIKE_PARAM;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  long n, *outOfRange;
  /* despikeValues: number of PVs to despike */
  int32_t despikeValues, *despike, *despikeIndex, *writeToFile, writeToGlitchLogger;
  char **controlName;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL pva;
  double *value, *min, *max, *sleep, *holdOff, *reset, longestSleep, longestHoldOff, longestReset;
  char *glitchLog;
} TESTS;

/*defined for multiple waveform tests*/
typedef struct
{
  long waveformTests, testFiles;
  char **testFile;  /*file names of waveform tests */
  char *searchPath; /*point to the searchPath of loopParam */
  char **waveformPV, ***DeviceName;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL pvaChannel;
  int32_t *waveformLength, **outOfRange, *testFailed;
  int32_t **index;
  double **waveformData, **min, **max, **sleep, *waveformSleep, **holdOff, *waveformHoldOff;
  /*despikeValues: number of PVs to despike */
  int32_t *despikeValues, **despike, **despikeIndex, **writeToFile;
  long longestSleep, longestHoldOff;
  short holdOffPresent, **ignore;
} WAVEFORM_TESTS;

typedef struct
{
  long level, counter;
} BACKOFF;

typedef struct
{
  double RMS, ave, MAD, largest;
  char *largestName, msg[256];
} STATS;

typedef struct
{
  long n;
  double n2;
  double interval;
} AVERAGE_PARAM;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  double readbackRMSthreshold, controlRMSthreshold;
  int32_t rows;
  long avail_rows, glitched, row_pointer;
} GLITCH_PARAM;

typedef struct
{
  long doGenerations, daily;
  int32_t digits, rowLimit;
  double timeStop, timeLimit;
  char *delimiter;
} GENERATIONS;

void SetupRawPVAConnection(char **PVname, CHANNEL_INFO *channelInfo, long n, double pendIOTime, PVA_OVERALL *pva);
void initializeWaveforms(WAVE_FORMS *wave_forms);
void initializeTestWaveforms(WAVEFORM_TESTS *waveform_test);
long isSorted(int32_t *index, long n);
void SortBPMsByIndex(char ***PV, int32_t *index, long n);
void setupWaveforms(WAVE_FORMS *wave_forms, CONTROL_NAME *readback, long verbose, double pendIOTime);
long ReadWaveformData(WAVE_FORMS *wave_forms, double pendIOTime);
void setupWaveformTests(WAVEFORM_TESTS *waveform_tests, CONTROL_NAME *readback, CONTROL_NAME *control, double interval, long verbose, double pendIOTime);
long CheckWaveformTest(WAVEFORM_TESTS *waveform_tests, BACKOFF *backoff, LOOP_PARAM *loopParam, DESPIKE_PARAM *despikeParam, long verbose, long warning, double pendIOTime);
void cleanupWaveforms(WAVE_FORMS *wave_forms);
void cleanupTestWaveforms(WAVEFORM_TESTS *waveform_tests);
long WriteWaveformData(WAVE_FORMS *controlWaveforms, CONTROL_NAME *control, double pendIOTime);
void CleanUpCorrections(CORRECTION *correction);
void CleanUpOthers(DESPIKE_PARAM *despike, TESTS *tests, LOOP_PARAM *loopParam);
void CleanUpLimits(LIMITS *limits);

void setupDeltaLimitFile(LIMITS *delta);
void setupReadbackLimitFile(LIMITS *readbackLimits);
void setupActionLimitFile(LIMITS *action);
void setupInputFile(CORRECTION *correction);
void assignControlName(CORRECTION *correction, char *defitionFile);
void matchUpControlNames(LIMITS *limits, char **controlName, long n);
void setupTestsFile(TESTS *tests, double interval, long verbose, double pendIOTime); /*interval is the correction interval */
void setupDespikeFile(DESPIKE_PARAM *despikeParam, CONTROL_NAME *readback, long verbose);
void setupOutputFile(SDDS_TABLE *outputPage, char *file, CORRECTION *correction, TESTS *test, LOOP_PARAM *loopParam, char *timeStamp, WAVEFORM_TESTS *waveform_tests, long verbose);
void setupStatsFile(SDDS_TABLE *statsPage, char *statsFile, LOOP_PARAM *loopParam, char *timeStamp, long verbose);
void setupGlitchFile(SDDS_TABLE *glitchPage, GLITCH_PARAM *glitchParam, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, char *timeStamp, long verbose);
void getInitialValues(CORRECTION *correction, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime);
long initializeHistories(CORRECTION *compensation);
long getReadbackValues(CONTROL_NAME *readback, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *offsetWaveforms, long verbose, double pendIOTime);
long checkActionLimit(LIMITS *action, CONTROL_NAME *readback);
void adjustReadbacks(CONTROL_NAME *readback, LIMITS *readbackLimits, DESPIKE_PARAM *despikeParam, STATS *readbackAdjustedStats, long verbose);
long getControlDevices(CONTROL_NAME *control, STATS *controlStats, LOOP_PARAM *loopParam, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime);
void checkRCParam(SDDS_TABLE *inputPage, char *file);
void despikeTestValues(TESTS *test, DESPIKE_PARAM *despikeParam, long verbose);
long checkOutOfRange(TESTS *test, BACKOFF *backoff, STATS *readbackStats, STATS *readbackAdjustedStats, STATS *controlStats, LOOP_PARAM *loopParam, double timeOfDay, long verbose, long warning);
void apply_filter(CORRECTION *correction, long verbose);
void controlLaw(long skipIteration, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, long verbose, double pendIOTime);
/* return factor which was applied to force the control
   under the limit */
double applyDeltaLimit(LIMITS *delta, LOOP_PARAM *loopParam, CONTROL_NAME *control, long verbose, long warning);
void calcControlDeltaStats(CONTROL_NAME *control, STATS *controlStats, STATS *controlDeltaStats);
void writeToOutputFile(char *outputFile, SDDS_TABLE *outputPage, long *outputRow, LOOP_PARAM *loopParam, CORRECTION *correction, TESTS *test, WAVEFORM_TESTS *waveform_tests);
void readOffsetValues(double *offset, long n, char **controlName, char *offsetFile, char *searchPath);
void readOffsetPV(char **offsetPVs, long n, char **controlName, char *PVOffsetFile, char *searchPath);
void writeToStatsFile(char *statsFile, SDDS_TABLE *statsPage, long *statsRow, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, STATS *controlStats, STATS *controlDeltaStats);
void writeToGlitchFile(GLITCH_PARAM *glitchParam, SDDS_TABLE *glitchPage, long *glitchRow, LOOP_PARAM *loopParam, STATS *readbackAdjustedStats, STATS *controlDeltaStats, CORRECTION *correction, CORRECTION *overlapCompensation, TESTS *test);

void FreeEverything(void);

void commandServer(int sig);
void serverExit(int sig);
void exitIfServerRunning(void);
void setupServer(void);

long parseArguments(char ***argv,
                    int *argc,
                    CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *param,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    char **statsFile,
                    long *serverMode,
                    char **commandFile,
                    char **outputFile,
                    char **controlLogFile,
                    long *testCASecurity,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    char **compensationOutputFile,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests,
                    long firstTime,
                    long *verbose,
                    long *warning,
                    double *pendIOTime,
                    char *commandline_option[COMMANDLINE_OPTIONS],
                    char *waveformOption[WAVEFORMOPTIONS]);

void initializeStatsData(STATS *stats);
void initializeData(CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests);

void setupData(CORRECTION *correction,
               LIMITS *delta,
               LIMITS *readbackLimits,
               LIMITS *action,
               LOOP_PARAM *loopParam,
               DESPIKE_PARAM *despikeParam,
               TESTS *test,
               CORRECTION *overlapCompensation,
               WAVE_FORMS *readbackWaveforms,
               WAVE_FORMS *controlWaveforms,
               WAVE_FORMS *offsetWaveforms,
               WAVE_FORMS *ffSetpointWaveforms,
               WAVEFORM_TESTS *waveform_test,
               GLITCH_PARAM *glitchParam,
               long verbose,
               double pendIOTime);

void logActuator(char *controlLogFile, CONTROL_NAME *control, char *timeStamp);

/* There is an added argument of file pointer to M. Borland function 
   found in sddsplot.c */
long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc, FILE *fp);
int countArguments(char *s);
long readPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, PVA_OVERALL *pva, long n, AVERAGE_PARAM *aveParam, double pendIOTime);

long getAveragePVs(char **PVs, double *value, long number, long averages, double timeInterval);
long setPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, PVA_OVERALL *pva, long n, double pendIOTime);
long runPostChangeExec(char *scriptName, CONTROL_NAME *control, long verbose);
long CheckPVAWritePermissionMod(char **PVs, PVA_OVERALL *pva, long n);
int runControlPingWhileSleep(double sleepTime);
long setStringPV(char *PV, char *value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, long exitOnError);
long setEnumPV(char *PV, long value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, long exitOnError);
long readEnumPV(char *PV, long *value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, double pendIoTime, long exitOnError);

long pvaThreadSleep(long double seconds);
long pvaEscapableThreadSleep(long double seconds, volatile int *flag);

/* need to be global because some quantities are used
   in signal and interrupt handlers */
typedef struct
{
  CORRECTION correction, overlapCompensation;
  DESPIKE_PARAM despikeParam;
  TESTS test;
  LIMITS delta, action, readbackLimits;
  WAVE_FORMS readbackWaveforms, controlWaveforms;
  WAVE_FORMS offsetWaveforms, ffSetpointWaveforms;
  WAVEFORM_TESTS waveform_tests;
  char *outputFile, *statsFile, *controlLogFile;
  SDDS_TABLE outputPage, statsPage, glitchPage;
  RUNCONTROL_PARAM rcParam;
  LOOP_PARAM loopParam;
  GLITCH_PARAM glitchParam;
#ifdef USE_LOGDAEMON
  LOGHANDLE logHandle;
#endif
  long useLogDaemon;
  char *pidFile;
  long reparseFromFile;
  int32_t *sortIndex;
  int argc;
  char ***argv;
  volatile int sigint;
} SDDSCONTROLLAW_GLOBAL;
SDDSCONTROLLAW_GLOBAL *sddscontrollawGlobal;

int main(int argc, char **argv)
{
  SCANNED_ARG *s_arg = NULL;
  SDDS_TABLE compensationOutputPage;
  CONTROL_NAME *control = NULL, *controlComp = NULL, *readback = NULL, *readbackComp = NULL;
  char *outputRoot = NULL, *compensationOutputFile = NULL;
  long i, outputRow, compensationOutputRow, statsRow, glitchRow, firstTime, icontrol;
  long skipIteration = 0, outOfRange, prevOutOfRange = 0, waveformOutOfRange, testOutOfRange;
  char *timeStamp = NULL;
  double startTime, startHour, timeOfDay, targetTime, timeLeft, sleepTime = 0;
  BACKOFF backoff;
  AVERAGE_PARAM aveParam;
  STATS readbackStats, readbackDeltaStats, readbackAdjustedStats;
  STATS controlStats, controlDeltaStats;
  STATS readbackCompStats, readbackCompDeltaStats, readbackCompAdjustedStats;
  STATS controlCompStats, controlCompDeltaStats;
  long serverMode;
  long testCASecurity, caWriteError;
  double HourNow = 0, LastHour;
  long newFileCountdown = 0;
  GENERATIONS generations;
  double factor;
  double startReadTime = 0, endReadTime = 0, averageTime = 0, previousAverage = 0;
  long pre_n = 1;

  char *commandline_option[COMMANDLINE_OPTIONS] = {
    (char*)"gain",
    (char*)"interval",
    (char*)"steps",
    (char*)"updateInterval",
    (char*)"integral",
    (char*)"proportional",
    (char*)"verbose",
    (char*)"controlQuantityDefinition",
    (char*)"holdPresentValues",
    (char*)"dryRun",
    (char*)"testValues",
    (char*)"warning",
    (char*)"ezca",
    (char*)"ungroupEzca",
    (char*)"runControlPV",
    (char*)"runControlDescription",
    (char*)"actuatorColumn",
    (char*)"deltaLimit",
    (char*)"actionLimit",
    (char*)"average",
    (char*)"statistics",
    (char*)"despike",
    (char*)"servermode",
    (char*)"controlLogFile",
    (char*)"offsets",
    (char*)"CASecurityTest",
    (char*)"glitchlogfile",
    (char*)"generations",
    (char*)"dailyFiles",
    (char*)"readbackLimit",
    (char*)"auxiliaryOutput",
    (char*)"PVOffsets",
    (char*)"waveforms",
    (char*)"infiniteLoop",
    (char*)"pendIOTime",
    (char*)"launcherPV",
    (char*)"searchPath",
    (char*)"thresholdRamp",
    (char*)"postChangeExecution",
    (char*)"filterFile",
  };
  char *waveformOption[WAVEFORMOPTIONS] = {
    (char*)"readback", (char*)"offset", (char*)"actuator", (char*)"ffSetpoint", (char*)"test"};
  char *USAGE1 = (char*)"sddspvacontrollaw <inputfile> [-searchPath=<dir-path>] [-actuatorColumn=<string>]\n\
       [<outputfile>] [-infiniteLoop] [-pendIOTime] \n\
       [-filterFile=<file>]\n\
       [-generations[=digits=<integer>][,delimiter=<string>][,rowlimit=<number>][,timelimit=<secs>] | \n\
       -dailyFiles] [-controlQuantityDefinition=<file>]\n\
       [-gain={<real-value>|PVname=<name>}]\n\
       [-interval={<real-value>|PVname=<name>}] [-steps=<integer=value>]\n\
       [-updateInterval=<integer=value>]\n\
       [{-integration | -proportional}]\n\
       [-holdPresentValues] [-offsets=<offsetFile>]\n\
       [-PVOffsets=<filename>] \n\
       [-average={<number>|PVname=<name>}[,interval=<seconds>]] \n\
       [-despike[=neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>][,pvthreshold=<pvname][,file=<filename>][,countLimit=<integer>][,startThreshold=<value>,endThreshold=<value>,stepsThreshold=<integer>][,rampThresholdPV=<string>]]\n\
       [-deltaLimit={value=<value>|file=<filename>}]\n\
       [-readbackLimit={value=<value>|minValue=<value>,maxValue=<value>|file=<filename>}]\n\
       [-actionLimit={value=<value>|file=<filename>}]\n\
       [-testValues=<SDDSfile>] [-statistics=<filename>[,mode=<full|brief>]] \n\
       [-auxiliaryOutput=matrixFile=<file>,controlQuantityDefinition=<file>,\n\
          filterFile=<file>],controlLogFile=<file>[,mode=<integral|proportional>] \n\
       [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>\n\
       [-runControlDescription={string=<string>|parameter=<string>}]\n\
       [-launcherPV=<pvname>]\n\
       [-verbose] [-dryRun] [-warning]\n\
       [-servermode=pid=<file>,command=<file>]\n\
       [-controlLogFile=<file>] \n\
       [-glitchLogFile=file=<string>,[readbackRmsThreshold=<value>][,controlRmsThreshold=<value>][,rows=<integer]]\n\
       [-CASecurityTest] [-waveforms=<filename>,<type>] [-postChangeExecution=<string>] \n\n";
  char *USAGE2 = (char*)"Perform simple feedback on APS control system process variables using ca calls.\n\
<inputfile>    gain matrix in sdds format\n\
<searchPath>   the directory path for the input files.\n\
actuatorColumn String column in the input file to be used for\n\
               actuator names. The default is the first string column\n\
               of the input file.\n\
<outputfile>   optional file in which the readback and\n\
               control variables are printed at every step.\n\
<filterFile>   optional file which provides the filtering information to be applied to accuators.\n\
               the file must contain DeviceName, a0, a1,..., b0, b1,... columns";
  char *USAGE3 = (char*)"generations    The output is sent to the file <SDDSoutputfile>-<N>, where <N> is\n\
               the smallest positive integer such that the file does not already \n\
               exist. By default, four digits are used for formatting <N>, so that\n\
               the first generation number is 0001.  If a row limit or time limit\n\
               is given, a new file is started when the given limit is reached.\n\
dailyFiles     The output is sent to the file <SDDSoutputfile>-YYYY-JJJ-MMDD.<N>,\n\
               where YYYY=year, JJJ=Julian day, and MMDD=month+day.  A new file is\n\
               started after midnight.\n\
controlQuantityDefinition\n\
               a cross-reference file which matches the simplified\n\
               names used in the inputfile to PV names.\n\
               Column names are SymbolicName and controlName.\n\
gain           quantity multiplying the inputfile matrix.\n\
               If the gain matrix is the inverse response matrix\n\
               then this should be less than one.\n\
interval       time interval between each correction.\n\
steps          total number of corrections.\n\
postChangeExecution run given execution after changing the setpoints.\n";
  char *USAGE4 = (char*)"updateInterval number of steps between each outputfile updates.\n\
integral | proportional\n\
               switch the control law to either integral or proportional\n\
               control;  use integral control for orbit correction; \n\
               the default control law is integral control.\n\
holdPresentValues\n\
               causes regulation of the readback variables to the\n\
               values at the start of the running.\n\
offsets        Gives a file of offset values to be subtracted from the\n\
               readback values to obtain the values to be held to zero.\n\
               Expected to contain at least two columns: ControlName and Offset. \n\
PVOffsets      Gives a file of offset PVs to be read and their values are \n\
               subtracted from the readback values of the primary readbacks.\n\
               Expected to contain at least two columns: ControlName (the names \n\
               of the primary readbacks), OffsetControlName (the name of \n\
               the offset PVs). \n\
auxiliaryOutput reads in an additional matrix to calculate values for PVs  \n\
               (not necessarily corrector PVs) based \n\
               on the correction that is being done. The formula \n\
               y = (gain) M (delta_C) is used for integral mode and \n\
               y = (gain) M (C) for proportional mode. Time filtering \n\
               is available through a filter file with \n\
               a0, b0, a1, etc, coefficients. \n\
               A control quantity definition file for the matrix \n\
               is available as option. The default mode is integral, if \n\
               mode=proportional is given, proportional control will be applied. \n\
verbose        prints extra information.\n\
dryRun         readback variables are read, but the control variables\n\
               aren't changed.\n";
  char *USAGE5 = (char*)"average        number of readbacks to average before making a correction.\n\
               Default interval is one second. Units of the specified \n\
               interval is seconds. The total time for averaging will\n\
               not add to the time between corrections.\n\
testValues     sdds format file containing minimum and maximum values\n\
               of PV's specifying a range outside of which the feedback\n\
               is temporarily suspended. Column names are ControlName,\n\
               MinimumValue, MaximumValue. Optional column names are \n\
               SleepTime, ResetTime, Despike, and GlitchLog.\n\
deltaLimit     Specifies maximum change made in one step for any actuator,\n\
               either as a single value or as a column of values (column name\n\
               DeltaName) from a file. The calculated change vector is scaled\n\
               to enforce these limits, if necessary.\n\
readbackLimit  Specifies the maximum negative and positive error to\n\
               recognize for each PV. The values can be specified\n\
               as a single value, as two values, or as columns of\n\
               values in a file (clumns should be minValue and maxValue).\n\
actionLimit    Specifies minimum values in readback before any action is\n\
               taken, either as a single value or as a column of values\n\
               (column name ActionLimit) from a file.\n\
warning        prints warning messages.\n\
pendIoTime     specifies maximum time to wait for connections and\n\
               return values.  Default is 30.0s \n\
runControlPV   specifies a string parameter in <inputfile> whose value\n\
               is a runControl PV name.\n\
runControlDescription\n\
               specifies a string parameter in <inputfile> whose value\n\
               is a runControl PV description record.\n";
  char *USAGE6 = (char*)"despike        specifies despiking of readback variables,\n\
               under the assumption\n\
               that consecutive readbacks are nearest neighbors. If a file is\n\
               specified, then the PVs with column Despike value of 0 will not\n\
               get despiked. If the testsValues file has a Despike column defined,\n\
               the test PVs with a nonzero value for Despike will get despiked\n\
               with the same option parameters. If countLimit is greater than 0\n\
               and there are more than N readings outside the despiking threshold,\n\
               then no despiking is done.\n\
               If startThreshold, endThreshold and stepsThreshold \n\
               are provided, then ramp the despiking threshold from startThreshold to \n\
               endThreshold over stepsThreshold correction steps. It then remains at \n\
               endThreshold, unless the threshold pv is given with -despike.  \n\
               In that case, the threshold can be changed via the PV.\n\
               If rampThresholdPV is provided, the threshold will be reramped whenever \n\
               the value of rampThresholdPV is no-zero and it is reset to 0 after reramping.\n";
  char *USAGE7 = (char*)"servermode     allows one to change the commandline options while the program is\n\
               running. Program reads the command file for new options whenever\n\
               SIGUSR1 is received, and exits when SIGUSR2 is received.\n\
               The process id is stored in file specified by pid.\n\
controlLogFile At each change of actuators, the old and new values of the actuators\n\
               are written to this file. The previous instance of the file \n\
               is over-written at the same time.\n\
glitchLogFile  Readback and control data values\n\
               are written at each glitch event defined by the\n\
               by the RMS thresholds specified as sub-options.\n\
CASecurityTest checks the channel access write permission of the control PVs.\n\
               If at least one PV has a write restriction then the program suspends\n\
               the runcontrol record.\n";
  char *USAGE8 = (char*)"waveforms=<filename>,<type> the waveform file name, and the type of\n\
               waveforms. <type>=readback, offset, actuator or ffSetpoint or test.\n\
               The waveform file contains \"WaveformPV\" parameter, \n\
               \"DeviceName\" and \"Index\" columns, which is the index of DeviceName in \n\
               the vector. Note that for actuators, if the reading and writing name are\n\
               different, two parameters \"ReadWaveformPV\" and \"WriteWaveformPV\" \n\
               should be given. For testing waveforms, there are two additional required columns: \n\
               MaximumValue and MinimumValue, and one optional short column - Ignore: \n\
               which set the flags of whether ignore the pvs in the waveform. If Ignore column \n\
               does not exist, then the readbacks and controls will consider to be testing pvs \n\
               in the waveforms. \n\n\
Program by Louis Emery, ANL\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";
  char *USAGE_WARNING = (char*)"";
  double lastRCPingTime = 0.0;

  int pingSkipIteration = 0;
  double pendIOTime;
  long verbose, warning, rampDone;
  char *commandFile;
#ifdef DEBUGTIMES
  int debugTimes = 0;
  double debugTime[100];
  for (i = 0; i < 99; i++)
    debugTime[i] = 0;
#endif
  outputRoot = NULL;
  caWriteError = 0;
  rampDone = 0;

  SDDS_RegisterProgramName("sddspvacontrollaw");

  SDDS_CheckDatasetStructureSize(sizeof(SDDS_DATASET));
  if ((sddscontrollawGlobal = (SDDSCONTROLLAW_GLOBAL *)calloc(1, sizeof(SDDSCONTROLLAW_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddscontrollawGlobal\n");
    return (1);
  }

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

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5, USAGE6, USAGE7, USAGE8);
    fprintf(stderr, "%s", USAGE_WARNING);
    free_scanargs(&s_arg, argc);
    FreeEverything();
    return (1);
  }
  free_scanargs(&s_arg, argc);

  firstTime = 1;
  initializeStatsData(&readbackStats);
  initializeStatsData(&readbackDeltaStats);
  initializeStatsData(&readbackAdjustedStats);
  initializeStatsData(&controlDeltaStats);

  initializeStatsData(&readbackCompStats);
  initializeStatsData(&readbackCompDeltaStats);
  initializeStatsData(&readbackCompAdjustedStats);
  initializeStatsData(&controlCompStats);

  sddscontrollawGlobal->waveform_tests.longestHoldOff = 1;

  controlStats.msg[0] = 0;
  if (parseArguments(&argv,
                     &argc,
                     &sddscontrollawGlobal->correction,
                     &sddscontrollawGlobal->loopParam,
                     &aveParam,
                     &sddscontrollawGlobal->delta,
                     &sddscontrollawGlobal->readbackLimits,
                     &sddscontrollawGlobal->action,
                     &sddscontrollawGlobal->despikeParam,
                     &sddscontrollawGlobal->glitchParam,
                     &sddscontrollawGlobal->test,
                     &sddscontrollawGlobal->statsFile,
                     &serverMode,
                     &commandFile,
                     &sddscontrollawGlobal->outputFile,
                     &sddscontrollawGlobal->controlLogFile,
                     &testCASecurity,
                     &generations,
                     &sddscontrollawGlobal->overlapCompensation,
                     &compensationOutputFile,
                     &sddscontrollawGlobal->readbackWaveforms,
                     &sddscontrollawGlobal->controlWaveforms,
                     &sddscontrollawGlobal->offsetWaveforms,
                     &sddscontrollawGlobal->ffSetpointWaveforms,
                     &sddscontrollawGlobal->waveform_tests,
                     firstTime,
                     &verbose,
                     &warning,
                     &pendIOTime,
                     commandline_option,
                     waveformOption)) {
    fprintf(stderr, "Problem with parsing arguments.\n");
    FreeEverything();
    return (1);
  }
  /*set search path */
  if (sddscontrollawGlobal->loopParam.searchPath)
    setSearchPath(sddscontrollawGlobal->loopParam.searchPath);
  setupData(&sddscontrollawGlobal->correction,
            &sddscontrollawGlobal->delta,
            &sddscontrollawGlobal->readbackLimits,
            &sddscontrollawGlobal->action,
            &sddscontrollawGlobal->loopParam,
            &sddscontrollawGlobal->despikeParam,
            &sddscontrollawGlobal->test,
            &sddscontrollawGlobal->overlapCompensation,
            &sddscontrollawGlobal->readbackWaveforms,
            &sddscontrollawGlobal->controlWaveforms,
            &sddscontrollawGlobal->offsetWaveforms,
            &sddscontrollawGlobal->ffSetpointWaveforms,
            &sddscontrollawGlobal->waveform_tests,
            &sddscontrollawGlobal->glitchParam,
            verbose,
            pendIOTime);
  readback = sddscontrollawGlobal->correction.readback;
  control = sddscontrollawGlobal->correction.control;
  if (sddscontrollawGlobal->overlapCompensation.file) {
    readbackComp = sddscontrollawGlobal->overlapCompensation.readback;
    controlComp = sddscontrollawGlobal->overlapCompensation.control;
    if (sddscontrollawGlobal->overlapCompensation.file && (readbackComp->n != control->n)) {
      FreeEverything();
      SDDS_Bomb((char*)"Size of auxiliary output file is not compatible with correction file.");
    }
  }

  if (!sddscontrollawGlobal->correction.file) {
    FreeEverything();
    SDDS_Bomb((char*)"input filename not given");
  }
  if (sddscontrollawGlobal->loopParam.updateInterval > sddscontrollawGlobal->loopParam.steps)
    sddscontrollawGlobal->loopParam.updateInterval = sddscontrollawGlobal->loopParam.steps;
  if (aveParam.n > 1) {
    if (sddscontrollawGlobal->loopParam.interval < (aveParam.n - 1) * aveParam.interval) {
      fprintf(stderr, "Time interval between corrections (%e) is too short for the averaging requested (%ldx%e).\n", sddscontrollawGlobal->loopParam.interval, (aveParam.n - 1), aveParam.interval);
      FreeEverything();
      return (1);
    }
  }

  if (serverMode && (sddscontrollawGlobal->outputFile || sddscontrollawGlobal->loopParam.holdPresentValues)) {
    FreeEverything();
    SDDS_Bomb((char*)"Server mode is incompatible with output file logging and holdPresentValues.");
  }

  firstTime = 0;

  if (serverMode) {
    /* if a server is already running in this directory
         then exit since we don't want two sddscontrollaw running */
    exitIfServerRunning();
    /* if a server is not already running then start one */
    setupServer();
  }

  sddscontrollawGlobal->useLogDaemon = 0;
  #ifdef USE_LOGDAEMON
  if (sddscontrollawGlobal->rcParam.PV) {
    char tagList[] = "Instance Action";
    switch (logOpen(&sddscontrollawGlobal->logHandle, NULL, (char*)"controllawAudit", tagList)) {
    case LOG_OK:
      sddscontrollawGlobal->useLogDaemon = 1;
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
  #endif

  if (sddscontrollawGlobal->rcParam.PV) {
    sddscontrollawGlobal->rcParam.handle[0] = (char)0;
    sddscontrollawGlobal->rcParam.Desc = (char *)SDDS_Realloc(sddscontrollawGlobal->rcParam.Desc, 41 * sizeof(char));
    sddscontrollawGlobal->rcParam.PV = (char *)SDDS_Realloc(sddscontrollawGlobal->rcParam.PV, 41 * sizeof(char));
    sddscontrollawGlobal->rcParam.status = runControlInit(sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc, sddscontrollawGlobal->rcParam.pingTimeout, sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo), pendIOTime);
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (sddscontrollawGlobal->rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      FreeEverything();
      return (1);
    }
    sddscontrollawGlobal->rcParam.init = 1;
    lastRCPingTime = getTimeInSecs();

    strcpy(sddscontrollawGlobal->rcParam.message, "Running");
    sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      FreeEverything();
      return (1);
    }
  }

  /*******************************\
   * define table for output files *
   \*******************************/
  getTimeBreakdown(&startTime, NULL, &startHour, NULL, NULL, NULL, &timeStamp);

  if (!serverMode) {
    if (generations.doGenerations) {
      if (!generations.daily) {
        sddscontrollawGlobal->outputFile = MakeGenerationFilename(outputRoot = sddscontrollawGlobal->outputFile, generations.digits, generations.delimiter, NULL);
      } else {
        /* when daily files are requested then the variable outputRoot
	         contains the root part of the output file names.
	       */
        sddscontrollawGlobal->outputFile = MakeDailyGenerationFilename(outputRoot = sddscontrollawGlobal->outputFile, generations.digits, generations.delimiter, 0);
      }
      HourNow = getHourOfDay();
    }
    setupOutputFile(&sddscontrollawGlobal->outputPage, sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
    if (sddscontrollawGlobal->overlapCompensation.file && compensationOutputFile) {
      fprintf(stderr, "Setting up file %s.\n", compensationOutputFile);
      setupOutputFile(&compensationOutputPage, compensationOutputFile, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
    }
  }

  setupStatsFile(&sddscontrollawGlobal->statsPage, sddscontrollawGlobal->statsFile, &sddscontrollawGlobal->loopParam, timeStamp, verbose);

  setupGlitchFile(&sddscontrollawGlobal->glitchPage, &sddscontrollawGlobal->glitchParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, timeStamp, verbose);
  /*********************\
   * Initial values    *
   \*********************/
  getInitialValues(&sddscontrollawGlobal->correction, &aveParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->readbackWaveforms, &sddscontrollawGlobal->controlWaveforms, verbose, pendIOTime);
  if (sddscontrollawGlobal->correction.coefFile)
    initializeHistories(&sddscontrollawGlobal->correction);

  if (sddscontrollawGlobal->overlapCompensation.file) {
    /*here, controlWaveforms is corresponding to readbackComp, and
         ffSetpointWaveforms is corresponding to controlComp */
    getInitialValues(&sddscontrollawGlobal->overlapCompensation, &aveParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->controlWaveforms, &sddscontrollawGlobal->ffSetpointWaveforms, verbose, pendIOTime);
    initializeHistories(&sddscontrollawGlobal->overlapCompensation);
  }

  getTimeBreakdown(&startTime, NULL, NULL, NULL, NULL, NULL, NULL);
  targetTime = startTime;
  /*********************\
   * Iteration loop    *
   \*********************/
  if (warning || verbose) {
    backoff.level = 1;
    backoff.counter = 0;
  }

  if (verbose)
    fprintf(stderr, "Starting loop of %ld steps.\n", sddscontrollawGlobal->loopParam.steps);
  outputRow = 0;             /* row is incremented only when writing to the 
				   output file (i.e. successful iteration) */
  compensationOutputRow = 0; /* row is incremented only when writing to the 
				   compensation output file (i.e. successful iteration) */
  statsRow = 0;
  glitchRow = 0;
  testOutOfRange = 0;
  outOfRange = 0;
  waveformOutOfRange = 0;
  if (generations.rowLimit)
    newFileCountdown = generations.rowLimit;
  /* row numbering in SDDS pages starts at 0 */

  if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
    setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], (char*)"Iterations started", sddscontrollawGlobal->loopParam.launcherPVInfo[0], &(sddscontrollawGlobal->loopParam.pvaLauncher[0]), 1);
    setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[1], &(sddscontrollawGlobal->loopParam.pvaLauncher[1]), 1);
/*
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "pendIOerror: unable to put PV values\n");
      FreeEverything();
      exit(1);
    }
*/
  }

  for (sddscontrollawGlobal->loopParam.step[0] = 0; sddscontrollawGlobal->loopParam.step[0] < sddscontrollawGlobal->loopParam.steps; sddscontrollawGlobal->loopParam.step[0]++) {
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[0] = getTimeInSecs();
#endif

    if (sddscontrollawGlobal->sigint) {
      FreeEverything();
      return (1);
    }

    if (sddscontrollawGlobal->loopParam.averagePV) {
      pre_n = aveParam.n;
      readPVs(&sddscontrollawGlobal->loopParam.averagePV, &aveParam.n2, &sddscontrollawGlobal->loopParam.averagePVInfo, &sddscontrollawGlobal->loopParam.pvaAverage, 1, NULL, pendIOTime);
      aveParam.n = round(aveParam.n2);
    }
    if (sddscontrollawGlobal->loopParam.intervalPV) {
      readPVs(&sddscontrollawGlobal->loopParam.intervalPV, &sddscontrollawGlobal->loopParam.interval, &sddscontrollawGlobal->loopParam.intervalPVInfo, &sddscontrollawGlobal->loopParam.pvaInterval, 1, NULL, pendIOTime);
    }
    if (sddscontrollawGlobal->despikeParam.rampThresholdPV && sddscontrollawGlobal->despikeParam.startThreshold != sddscontrollawGlobal->despikeParam.endThreshold) {
      readEnumPV(sddscontrollawGlobal->despikeParam.rampThresholdPV, &sddscontrollawGlobal->despikeParam.reramp, sddscontrollawGlobal->despikeParam.rampThresholdPVInfo, &(sddscontrollawGlobal->despikeParam.pvaRampThreshold), pendIOTime, 0);
      if (sddscontrollawGlobal->despikeParam.reramp) {
        if (verbose)
          fprintf(stderr, "Re-ramp despike threshold.\n");
        sddscontrollawGlobal->despikeParam.reramp = 0;
        sddscontrollawGlobal->despikeParam.threshold = sddscontrollawGlobal->despikeParam.startThreshold - sddscontrollawGlobal->despikeParam.deltaThreshold;
        setEnumPV(sddscontrollawGlobal->despikeParam.rampThresholdPV, sddscontrollawGlobal->despikeParam.reramp, sddscontrollawGlobal->despikeParam.rampThresholdPVInfo, &(sddscontrollawGlobal->despikeParam.pvaRampThreshold), 0);
        rampDone = 0;
      }
    }
    if (fabs(sddscontrollawGlobal->despikeParam.threshold - sddscontrollawGlobal->despikeParam.endThreshold) <= 1e-6)
      rampDone = 1;
    /*the endThreshold is smaller than the start threshold and deltaThreshold is negative */
    if (sddscontrollawGlobal->despikeParam.flag & DESPIKE_STARTTHRESHOLD && !outOfRange && !rampDone) {
      sddscontrollawGlobal->despikeParam.threshold += sddscontrollawGlobal->despikeParam.deltaThreshold;
      if (sddscontrollawGlobal->despikeParam.threshold < sddscontrollawGlobal->despikeParam.endThreshold)
        sddscontrollawGlobal->despikeParam.threshold = sddscontrollawGlobal->despikeParam.endThreshold;
      if (sddscontrollawGlobal->despikeParam.thresholdPV) {
        /* send the ramped value to the despiking PV */
        setPVs(&sddscontrollawGlobal->despikeParam.thresholdPV, &sddscontrollawGlobal->despikeParam.threshold, &sddscontrollawGlobal->despikeParam.thresholdPVInfo, &sddscontrollawGlobal->despikeParam.pvaThreshold, 1, pendIOTime);
      }
    }

    if (verbose)
      fprintf(stderr, "Despike threshold %f\n", sddscontrollawGlobal->despikeParam.threshold);
    if (sddscontrollawGlobal->despikeParam.thresholdPV) {
      readPVs(&sddscontrollawGlobal->despikeParam.thresholdPV, &sddscontrollawGlobal->despikeParam.threshold, &sddscontrollawGlobal->despikeParam.thresholdPVInfo, &sddscontrollawGlobal->despikeParam.pvaThreshold, 1, NULL, pendIOTime);
    }
    if (sddscontrollawGlobal->despikeParam.threshold < 0)
      sddscontrollawGlobal->despikeParam.threshold = 0;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[1] = getTimeInSecs();
#endif
    if ((sddscontrollawGlobal->loopParam.intervalPV) || (sddscontrollawGlobal->loopParam.averagePV)) {
      if ((pre_n > 1) && (!sddscontrollawGlobal->loopParam.intervalPV))
        sddscontrollawGlobal->loopParam.interval += (pre_n - 1) * aveParam.interval;
      if (aveParam.n > 1) {
        if (sddscontrollawGlobal->loopParam.interval < (aveParam.n - 1) * aveParam.interval) {
          fprintf(stderr, "Time interval between corrections (%e) is too short for the averaging requested (%ldx%e).\n", sddscontrollawGlobal->loopParam.interval, (aveParam.n - 1), aveParam.interval);
          FreeEverything();
          return (1);
        }
      }
    }
    targetTime += sddscontrollawGlobal->loopParam.interval; /* time at which the loop should
								   return to the beginning */
    /* pingSkipIteration is used to detect if the program was suspended. If it was we do not want to use old data collected
         before it was suspended so we will skip this iteration */
    pingSkipIteration = 0;
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        /* ping once at the beginning */
        if (runControlPingWhileSleep(0.0)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          FreeEverything();
          return (1);
        }
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    if (!(sddscontrollawGlobal->loopParam.step[0] % 2))
      startReadTime = sddscontrollawGlobal->loopParam.elapsedTime[0];
    else
      endReadTime = sddscontrollawGlobal->loopParam.elapsedTime[0];
    previousAverage = averageTime;
    if (sddscontrollawGlobal->loopParam.step[0]) {
      if (sddscontrollawGlobal->loopParam.step[0] == 1)
        averageTime = fabs(endReadTime - startReadTime);
      else
        averageTime = 0.1 * (fabs(endReadTime - startReadTime)) + 0.9 * previousAverage;
      if (verbose)
        fprintf(stderr, "average interval time: %f\n\n", averageTime);
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[2] = getTimeInSecs();
#endif
    if (getReadbackValues(readback, &aveParam, &sddscontrollawGlobal->loopParam, &readbackStats, &readbackDeltaStats, &sddscontrollawGlobal->readbackWaveforms, &sddscontrollawGlobal->offsetWaveforms, verbose, pendIOTime)) {
      FreeEverything();
      SDDS_Bomb((char*)"Error code return from getReadbackValues.");
    }
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[3] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->overlapCompensation.file) {
      if (getReadbackValues(readbackComp, &aveParam, &sddscontrollawGlobal->loopParam, &readbackCompStats, &readbackCompDeltaStats, &sddscontrollawGlobal->controlWaveforms, NULL, verbose, pendIOTime)) {
        FreeEverything();
        SDDS_Bomb((char*)"Error code return from getReadbackValues.");
      }
    }

    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    timeOfDay = startHour + sddscontrollawGlobal->loopParam.elapsedTime[0] / 3600.0;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[4] = getTimeInSecs();
#endif

    adjustReadbacks(readback, &sddscontrollawGlobal->readbackLimits, &sddscontrollawGlobal->despikeParam, &readbackAdjustedStats, verbose);
    for (i = 0; i < readback->n; i++)
      if (isnan(readback->value[0][i]))
        fprintf(stderr, "Error the value of PV %s (index=%ld) is not a number (nan)\n", readback->controlName[i], i);
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[5] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        runControlPingWhileSleep(0.0);
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }

    if ((skipIteration = checkActionLimit(&sddscontrollawGlobal->action, readback)) != 0) {
      if (warning || verbose)
        fprintf(stderr, "Readback values are less than the action limit. Skipping correction.\n");
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[6] = getTimeInSecs();
#endif

    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    if (getControlDevices(control, &controlStats, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->controlWaveforms, verbose, pendIOTime)) {
      FreeEverything();
      SDDS_Bomb((char*)"Error code return from getControlDevices");
    }
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[7] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->overlapCompensation.file) {
      if (getControlDevices(controlComp, &controlCompStats, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->ffSetpointWaveforms, verbose, pendIOTime)) {
        FreeEverything();
        SDDS_Bomb((char*)"Error code return from getControlDevices");
      }
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[8] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        runControlPingWhileSleep(0.0);
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }

    /********************\*
       * test values        *
     \*********************/

    if (sddscontrollawGlobal->test.file || sddscontrollawGlobal->waveform_tests.waveformTests) {
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      if (sddscontrollawGlobal->test.file) {
        if (verbose)
          fprintf(stderr, "Reading plain test devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
        readPVs(sddscontrollawGlobal->test.controlName, sddscontrollawGlobal->test.value, sddscontrollawGlobal->test.channelInfo, &sddscontrollawGlobal->test.pva, sddscontrollawGlobal->test.n, NULL, pendIOTime);
      }
      if (sddscontrollawGlobal->rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0);
          if ((getTimeInSecs() - lastRCPingTime) > 5.0)
            pingSkipIteration = 1;
        }
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[9] = getTimeInSecs();
#endif
      despikeTestValues(&sddscontrollawGlobal->test, &sddscontrollawGlobal->despikeParam, verbose);
      prevOutOfRange = outOfRange;

#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[10] = getTimeInSecs();
#endif
      if (sddscontrollawGlobal->test.file)
        testOutOfRange = checkOutOfRange(&sddscontrollawGlobal->test, &backoff, &readbackStats, &readbackAdjustedStats, &controlStats, &sddscontrollawGlobal->loopParam, timeOfDay, verbose, warning);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[11] = getTimeInSecs();
#endif
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      waveformOutOfRange = CheckWaveformTest(&sddscontrollawGlobal->waveform_tests, &backoff, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->despikeParam, verbose, warning, pendIOTime);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[12] = getTimeInSecs();
#endif
      if (waveformOutOfRange || testOutOfRange) {
        /*if out of range, set the control delta to 0 */
        outOfRange = 1;
        for (icontrol = 0; icontrol < control->n; icontrol++) {
          control->delta[0][icontrol] = 0;
          control->old[icontrol] = control->value[0][icontrol];
        }
        calcControlDeltaStats(control, &controlStats, &controlDeltaStats);
      } else
        outOfRange = 0;
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[13] = getTimeInSecs();
#endif
      if (outOfRange) {
        if (prevOutOfRange == 1) {
          if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], (char*)"Out-of-range test variable(s)", sddscontrollawGlobal->loopParam.launcherPVInfo[0], &(sddscontrollawGlobal->loopParam.pvaLauncher[0]), 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 1, sddscontrollawGlobal->loopParam.launcherPVInfo[1], &(sddscontrollawGlobal->loopParam.pvaLauncher[1]), 1);
          }
        }
        if (sddscontrollawGlobal->rcParam.PV) {
          /* This messages must be re-written at every iteration
		     because the runcontrolMessage can be written by another process
		     such as the tcl interface that runs sddscontrollaw */
          strcpy(sddscontrollawGlobal->rcParam.message, "Out-of-range test variable(s)");
          sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
          if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            FreeEverything();
            return (1);
          }
#ifdef USE_LOGDAEMON
          if (!prevOutOfRange && sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
            switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "Out-of-range", NULL)) {
            case LOG_OK:
              break;
            case LOG_ERROR:
            case LOG_TOOBIG:
              fprintf(stderr, "warning: something wrong in calling logArguments.\n");
              break;
            default:
              fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
              break;
            }
          }
#endif
        }
      } else {
        if (prevOutOfRange && (warning || verbose)) {
          fprintf(stderr, "Iterations restarted.\n");
          /* reset backoff counter */
          backoff.counter = 0;
          backoff.level = 1;
        }
        if (sddscontrollawGlobal->loopParam.launcherPV[0] && !caWriteError) {
          if (prevOutOfRange == 0) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], (char*)"Iterations re-started", sddscontrollawGlobal->loopParam.launcherPVInfo[0], &(sddscontrollawGlobal->loopParam.pvaLauncher[0]), 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[1], &(sddscontrollawGlobal->loopParam.pvaLauncher[1]), 1);
          }
        }
        /* These messages must be re-written at every iteration
	         because the runcontrolMessage can be written by another process
	         such as the tcl interface that runs sddscontrollaw */
        if (sddscontrollawGlobal->rcParam.PV && !caWriteError) {
          if (sddscontrollawGlobal->loopParam.step[0] == 0)
            strcpy(sddscontrollawGlobal->rcParam.message, "Iterations started");
          else
            strcpy(sddscontrollawGlobal->rcParam.message, "Iterations re-started");
          sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
          if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            FreeEverything();
            return (1);
          }
        }
        /* The "#if defined()" preprocessor command doesn't work in some compilers. */
#ifdef USE_LOGDAEMON
        if (prevOutOfRange && sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
          switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "In-range", NULL)) {
          case LOG_OK:
            break;
          case LOG_ERROR:
          case LOG_TOOBIG:
            fprintf(stderr, "warning: something wrong in calling logArguments.\n");
            break;
          default:
            fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
            break;
          }
        }
#endif
      }
    }

    /*********************\
       * control law       *
     \*********************/
    /* this skips an iteration if the previous step
         was out of range and averaging is requested. */
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[14] = getTimeInSecs();
#endif
    if ((!(outOfRange || ((aveParam.n > 1) && prevOutOfRange))) && !(pingSkipIteration)) {
      if (prevOutOfRange && (sddscontrollawGlobal->test.holdOff || sddscontrollawGlobal->waveform_tests.holdOffPresent)) {
        sleepTime = MAX(sddscontrollawGlobal->test.longestHoldOff, sddscontrollawGlobal->waveform_tests.longestHoldOff);
        if (verbose)
          fprintf(stderr, "Hold for %f secondes after tests passed.\n", sleepTime);
        /* targetTime += sleepTime; */
        /*recompute the target time with hold off */
        targetTime = getTimeInSecs() + sleepTime + sddscontrollawGlobal->loopParam.interval;
        if (pvaEscapableThreadSleep(sleepTime, &(sddscontrollawGlobal->sigint)) == 1) {
          FreeEverything();
          exit(1);
        }
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[15] = getTimeInSecs();
#endif
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      if (verbose)
        fprintf(stderr, "Calling controlLaw function at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
      controlLaw(skipIteration, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, verbose, pendIOTime);

#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[16] = getTimeInSecs();
#endif
      /* for now compensation doesn't work with applyDeltaLimit */
      factor = applyDeltaLimit(&sddscontrollawGlobal->delta, &sddscontrollawGlobal->loopParam, control, verbose, warning);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[17] = getTimeInSecs();
#endif

      calcControlDeltaStats(control, &controlStats, &controlDeltaStats);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[18] = getTimeInSecs();
#endif
      if (sddscontrollawGlobal->overlapCompensation.file) {
        if (factor < 1) {
          for (i = 0; i < controlComp->n; i++) {
            controlComp->delta[0][i] *= factor;
            controlComp->value[0][i] = controlComp->old[i] + controlComp->delta[0][i];
            /*for proportional mode, controlComp->old[i] is set to 0 */
          }
        }
        calcControlDeltaStats(controlComp, &controlCompStats, &controlCompDeltaStats);
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[19] = getTimeInSecs();
#endif

      if (!sddscontrollawGlobal->loopParam.dryRun) {
        sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;

        if (verbose) {
          fprintf(stderr, "Testing access of control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
        }
        caWriteError = 0;

#ifdef DEBUGTIMES
        if (debugTimes)
          debugTime[20] = getTimeInSecs();
#endif
        if (testCASecurity && (caWriteError = CheckPVAWritePermissionMod(control->controlName, &control->pva, control->n))) {
          fprintf(stderr, "Write access denied to at least one PV.\n");

          if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], (char*)"CA Security denies access.", sddscontrollawGlobal->loopParam.launcherPVInfo[0], &(sddscontrollawGlobal->loopParam.pvaLauncher[0]), 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 1, sddscontrollawGlobal->loopParam.launcherPVInfo[1], &(sddscontrollawGlobal->loopParam.pvaLauncher[1]), 1);
          }
          if (sddscontrollawGlobal->rcParam.PV) {
            /* This messages must be re-written at every iteration
		         because the runcontrolMessage can be written by another process
		         such as the tcl interface that runs sddscontrollaw */
            strcpy(sddscontrollawGlobal->rcParam.message, "CA Security denies access.");
            sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
            if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
              fprintf(stderr, "Unable to write status message and alarm severity\n");
              FreeEverything();
              return (1);
            }
#ifdef USE_LOGDAEMON
            if (sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
              switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "CA denied", NULL)) {
              case LOG_OK:
                break;
              case LOG_ERROR:
              case LOG_TOOBIG:
                fprintf(stderr, "warning: something wrong in calling logArguments.\n");
                break;
              default:
                fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
                break;
              }
            }
#endif
          }
        }
#ifdef DEBUGTIMES
        if (debugTimes)
          debugTime[21] = getTimeInSecs();
#endif
        if (!caWriteError) {
          sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
          if (verbose) {
            fprintf(stderr, "Setting control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
          }
          if (sddscontrollawGlobal->controlWaveforms.waveforms) {
            if (WriteWaveformData(&sddscontrollawGlobal->controlWaveforms, control, pendIOTime)) {
              FreeEverything();
              return (1);
            }
          } else {
            if (setPVs(control->controlName, control->value[0], control->channelInfo, &control->pva, control->n, pendIOTime)) {
              fprintf(stderr, "Error in setting PVs of the control variables.\n");
            }
          }
          if (sddscontrollawGlobal->loopParam.postChangeExec) {
            /*run post change execution */
            if (runPostChangeExec(sddscontrollawGlobal->loopParam.postChangeExec, control, verbose)) {
              fprintf(stderr, "Error in running post change execution %s\n", sddscontrollawGlobal->loopParam.postChangeExec);
              FreeEverything();
              return (1);
            }
          }
#ifdef DEBUGTIMES
          if (debugTimes)
            debugTime[22] = getTimeInSecs();
#endif
          if (sddscontrollawGlobal->overlapCompensation.file) {
            sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
            if (verbose) {
              fprintf(stderr, "Setting compensating control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
            }
            if (sddscontrollawGlobal->ffSetpointWaveforms.waveforms) {
              if (WriteWaveformData(&sddscontrollawGlobal->ffSetpointWaveforms, controlComp, pendIOTime)) {
                FreeEverything();
                return (1);
              }
            } else {
              if (setPVs(controlComp->controlName, controlComp->value[0], controlComp->channelInfo, &controlComp->pva, controlComp->n, pendIOTime)) {
                fprintf(stderr, "Error in setting PVs of the compensation control variables.\n");
              }
            }
          }
#ifdef DEBUGTIMES
          if (debugTimes)
            debugTime[23] = getTimeInSecs();
#endif
        }
      }
      if (sddscontrollawGlobal->controlLogFile) {
        getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &timeStamp);
        logActuator(sddscontrollawGlobal->controlLogFile, control, timeStamp);
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[24] = getTimeInSecs();
#endif
      if (verbose) {
        fprintf(stderr, "Stats                     Average       rms        "
                        "mad     Largest\n"
                        "%s\n%s%s\n%s\n%s\n",
                readbackStats.msg, readbackAdjustedStats.msg, readbackDeltaStats.msg, controlStats.msg, controlDeltaStats.msg);
        if (sddscontrollawGlobal->overlapCompensation.file) {
          fprintf(stderr, "For compensation...\nStats                     "
                          "Average       rms        mad     Largest\n"
                          "%s\n%s\n%s\n%s\n",
                  readbackCompStats.msg, readbackCompDeltaStats.msg, controlCompStats.msg, controlCompDeltaStats.msg);
        }
      }

      if (sddscontrollawGlobal->rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0);
        }
      }
    }

    if (!serverMode) {
      LastHour = HourNow;
      HourNow = getHourOfDay();
      if (generations.doGenerations) {
        if ((generations.daily && HourNow < LastHour) || ((generations.timeLimit > 0 || generations.rowLimit) && !newFileCountdown)) {
          /* must start a new file */
          if (!SDDS_Terminate(&sddscontrollawGlobal->outputPage))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          outputRow = 0;             /* row is incremented only when writing to the 
					   output file (i.e. successful iteration) */
          compensationOutputRow = 0; /* row is incremented only when writing to the 
						   compensation output file (i.e. successful iteration) */
          if (!generations.daily) {
            /* user-specified generation files */
            char *ptr = NULL;
            if (generations.rowLimit)
              newFileCountdown = generations.rowLimit;
            else
              newFileCountdown = -1;
            generations.timeStop = getTimeInSecs() + generations.timeLimit;
            sddscontrollawGlobal->outputFile = MakeGenerationFilename(outputRoot, generations.digits, generations.delimiter, ptr = sddscontrollawGlobal->outputFile);
            if (ptr)
              free(ptr);
          } else {
            /* "daily" log files with the OAG log name format */
            sddscontrollawGlobal->outputFile = MakeDailyGenerationFilename(outputRoot, generations.digits, generations.delimiter, 0);
          }
          fprintf(stderr, "Setting up file %s \n", sddscontrollawGlobal->outputFile);
          setupOutputFile(&sddscontrollawGlobal->outputPage, sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
          if (verbose) {
            fprintf(stderr, "New file %s started\n", sddscontrollawGlobal->outputFile);
          }
        }
      }

      writeToOutputFile(sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->outputPage, &outputRow, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->waveform_tests);
      if (sddscontrollawGlobal->overlapCompensation.file && compensationOutputFile) {
        writeToOutputFile(compensationOutputFile, &compensationOutputPage, &compensationOutputRow, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test, &sddscontrollawGlobal->waveform_tests);
      }
    }
    writeToStatsFile(sddscontrollawGlobal->statsFile, &sddscontrollawGlobal->statsPage, &statsRow, &sddscontrollawGlobal->loopParam, &readbackStats, &readbackDeltaStats, &controlStats, &controlDeltaStats);
    writeToGlitchFile(&sddscontrollawGlobal->glitchParam, &sddscontrollawGlobal->glitchPage, &glitchRow, &sddscontrollawGlobal->loopParam, &readbackAdjustedStats, &controlDeltaStats, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test);

    /*******************************\
       * pause for iteration interval *
     \******************************/
    /* calculate time that is left to the iteration */
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[25] = getTimeInSecs();
#endif
    timeLeft = targetTime - getTimeInSecs();
    if (timeLeft < 0) {
      /* if the runcontrol PV had been paused for a long time, say, the target
	     time is no longer valid, since it is substantially in the past.
	     The target time should be reset to now.
	   */

      targetTime = getTimeInSecs();
      timeLeft = 0;
    }
    if (testOutOfRange)
      sleepTime = MAX(sddscontrollawGlobal->test.longestSleep, sddscontrollawGlobal->loopParam.interval);
    if (waveformOutOfRange)
      sleepTime = MAX(sddscontrollawGlobal->waveform_tests.longestSleep, sddscontrollawGlobal->loopParam.interval);
    sleepTime = MAX(sleepTime, sddscontrollawGlobal->loopParam.interval);
    /*if (outOfRange)
         fprintf( stderr, "Waiting for %f seconds.\n", sleepTime); */
    if (sddscontrollawGlobal->rcParam.PV) {
      lastRCPingTime = getTimeInSecs();
      runControlPingWhileSleep(outOfRange ? MAX(sleepTime, sddscontrollawGlobal->loopParam.interval) : timeLeft);
    } else {
      if (pvaEscapableThreadSleep((outOfRange ? sleepTime : timeLeft), &(sddscontrollawGlobal->sigint)) == 1) {
        FreeEverything();
        exit(1);
      }
      
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[26] = getTimeInSecs();
#endif
    if (verbose)
      fprintf(stderr, "\n");
    if (sddscontrollawGlobal->reparseFromFile) {
      sddscontrollawGlobal->reparseFromFile = 0;
      if (parseArguments(&argv,
                         &argc,
                         &sddscontrollawGlobal->correction,
                         &sddscontrollawGlobal->loopParam,
                         &aveParam,
                         &sddscontrollawGlobal->delta,
                         &sddscontrollawGlobal->readbackLimits,
                         &sddscontrollawGlobal->action,
                         &sddscontrollawGlobal->despikeParam,
                         &sddscontrollawGlobal->glitchParam,
                         &sddscontrollawGlobal->test,
                         &sddscontrollawGlobal->statsFile,
                         &serverMode,
                         &commandFile,
                         &sddscontrollawGlobal->outputFile,
                         &sddscontrollawGlobal->controlLogFile,
                         &testCASecurity,
                         &generations,
                         &sddscontrollawGlobal->overlapCompensation,
                         &compensationOutputFile,
                         &sddscontrollawGlobal->readbackWaveforms,
                         &sddscontrollawGlobal->controlWaveforms,
                         &sddscontrollawGlobal->offsetWaveforms,
                         &sddscontrollawGlobal->ffSetpointWaveforms,
                         &sddscontrollawGlobal->waveform_tests,
                         firstTime,
                         &verbose,
                         &warning,
                         &pendIOTime,
                         commandline_option,
                         waveformOption)) {
        fprintf(stderr, "Problem parsing arguments. Forging ahead anyways.\n");
      }
      setupData(&sddscontrollawGlobal->correction,
                &sddscontrollawGlobal->delta,
                &sddscontrollawGlobal->readbackLimits,
                &sddscontrollawGlobal->action,
                &sddscontrollawGlobal->loopParam,
                &sddscontrollawGlobal->despikeParam,
                &sddscontrollawGlobal->test,
                &sddscontrollawGlobal->overlapCompensation,
                &sddscontrollawGlobal->readbackWaveforms,
                &sddscontrollawGlobal->controlWaveforms,
                &sddscontrollawGlobal->offsetWaveforms,
                &sddscontrollawGlobal->ffSetpointWaveforms,
                &sddscontrollawGlobal->waveform_tests,
                &sddscontrollawGlobal->glitchParam,
                verbose,
                pendIOTime);
      /* exit runcontrol and re-init. */
      /* This is necessary in order to change the description field even
	     though the PV name hasn't changed. */
      if (sddscontrollawGlobal->rcParam.PV) {
        switch (runControlExit(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo))) {
        case RUNCONTROL_OK:
          sddscontrollawGlobal->rcParam.init = 0;
          break;
        case RUNCONTROL_ERROR:
          fprintf(stderr, "Error exiting run control.\n");
          sddscontrollawGlobal->rcParam.init = 0;
          FreeEverything();
          return (1);
        default:
          fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
          sddscontrollawGlobal->rcParam.init = 0;
          FreeEverything();
          return (1);
        }
        fprintf(stderr, "PV %s\nDesc %s\n", sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc);
        sddscontrollawGlobal->rcParam.status = runControlInit(sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc, sddscontrollawGlobal->rcParam.pingTimeout, sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo), pendIOTime);
        if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
          fprintf(stderr, "Error initializing run control.\n");
          if (sddscontrollawGlobal->rcParam.status == RUNCONTROL_DENIED) {
            fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
          }
          FreeEverything();
          return (1);
        }
        sddscontrollawGlobal->rcParam.init = 1;
      }
    }
    if (generations.doGenerations) {
      --newFileCountdown;
      if (generations.timeLimit > 0 && getTimeInSecs() > generations.timeStop)
        newFileCountdown = 0;
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[27] = getTimeInSecs();
    if (sddscontrollawGlobal->loopParam.step[0] == 19)
      debugTimes = 1;
    if (sddscontrollawGlobal->loopParam.step[0] == 20) {
      debugTimes = 0;
      for (i = 0; i < 28; i++) {
        if (debugTime[i] != 0)
          fprintf(stderr, "debug time #%ld = %f\n", i, debugTime[i] - debugTime[0]);
      }
    }
#endif
  }
  FreeEverything();
  return (0);
}

long readPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, PVA_OVERALL *pva, long n, AVERAGE_PARAM *aveParam, double pendIOTime) {
  int i, j;
  long average;
  double interval;

  if (!n)
    return 0;
  if (!aveParam) {
    average = 1;
    interval = 1;
  } else {
    average = aveParam->n;
    interval = aveParam->interval;
  }
  for (i = 0; i < n; i++) {
    channelInfo[i].waveformLength = 0;
    value[i] = 0.0;
  }
  while (average > 0) {
    *(channelInfo[0].count) = n;
    for (i = 0; i < n; i++) {
      if (pva->isConnected[i] == false) {
        fprintf(stderr, "Error, no connection for %s\n", PVs[i]);
        FreeEverything();
        exit(1);
      }
      channelInfo[i].flag = 0;
    }
    freePVAGetReadings(pva);
    if (GetPVAValues(pva) == 1) {
      fprintf(stderr, "Error reading PVs\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < n; i++) {
      if (pva->pvaData[i].numeric == false) {
        fprintf(stderr, "Error, string PV %s is not allowed\n", PVs[i]);
        FreeEverything();
        exit(1);
      }
      channelInfo[i].value = pva->pvaData[i].getData[0].values[0];
    }

    *(channelInfo[0].count) = 0;
    average--;
    for (j = 0; j < n; j++)
      value[j] = channelInfo[j].value + value[j];
    if (average) {
      if (pvaEscapableThreadSleep(interval, &(sddscontrollawGlobal->sigint)) == 1) {
        FreeEverything();
        exit(1);
      }
    }
  }
  if (aveParam && aveParam->n > 1) {
    for (i = 0; i < n; i++)
      value[i] = value[i] / aveParam->n;
  }
  return 0;
}

long CheckPVAWritePermissionMod(char **PVs, PVA_OVERALL *pva, long n) {
  long caDenied = 0;
  long i;
  for (i = 0; i < n; i++) {
    if (HaveWriteAccess(pva, i) == false) {
      fprintf(stderr, "No write access to %s\n", PVs[i]);
      caDenied++;
    }
  }
  return caDenied;
}

long setPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, PVA_OVERALL *pva, long n, double pendIOTime) {
#if defined(DRYRUN) || defined(NOCAPUT)
  return 0;
#else
  long j;
  *(channelInfo[0].count) = n;

  for (j = 0; j < n; j++) {
    channelInfo[j].flag = 0;
    if (pva->isConnected[j] == false) {
      pva->pvaData[j].skip = true;
      continue;
    }
    pva->pvaData[j].skip = false;
    switch (pva->pvaData[j].fieldType) {
    case epics::pvData::scalar: {
      pva->pvaData[j].numPutElements = 1;
      if (pva->pvaData[j].numeric) {
        if (pva->pvaData[j].putData[0].values == NULL) {
          pva->pvaData[j].putData[0].values = (double*)malloc(sizeof(double));
        }
        pva->pvaData[j].putData[0].values[0] = value[j];
      } else {
        fprintf(stderr, "error: %s is not a numeric PV\n", PVs[j]);
        FreeEverything();
        exit(1);
      }
      break;
    }
    default: {
      fprintf(stderr, "error: unexpected field type for %s\n", PVs[j]);
      FreeEverything();
      exit(1);
    }
    }

  }
  *(channelInfo[0].count) = 0;
  if (PutPVAValues(pva) == 1) {
    fprintf(stderr, "Error setting PVs\n");
    FreeEverything();
    exit(1);
  }
  return 0;
#endif
}

long runPostChangeExec(char *script, CONTROL_NAME *control, long verbose) {
  FILE *handle;
  char message[1024];
  char *setCommd = NULL;
#ifdef _WIN32
  FILE *f_tmp, *f_tmp1;
  char *tmpfile, *tmpComm;
  tmpfile = tmpComm = NULL;
#endif

  handle = NULL;

  setCommd = (char*)SDDS_Realloc(setCommd, sizeof(char) * (strlen(script) + 25));
  strcpy(setCommd, script);
  if (verbose)
    fprintf(stderr, "running post change command %s\n", setCommd);
#ifdef _WIN32
  cp_str(&tmpfile, tmpname(NULL));
  strcat(setCommd, " 2> ");
  strcat(setCommd, tmpfile);
  system(setCommd);
  if (!(f_tmp1 = fopen(tmpfile, "r"))) {
    fclose(f_tmp1);
    fprintf(stderr, "Error, can not open tmpfile");
    return 1;
  }
  if (fgets(message, sizeof(message), f_tmp1)) {
    if (strlen(message) && wild_match(message, (char*)"*error*")) {
      fprintf(stderr, "error message: %s\n", message);
      return 1;
    }
    fclose(f_tmp1);
    tmpComm = (char*)malloc(sizeof(char) * (strlen(tmpfile) + 20));
    cp_str(&tmpComm, "rm -f ");
    strcat(tmpComm, tmpfile);
    system(tmpComm);
    free(tmpComm);
    free(tmpfile);
    tmpComm = tmpfile = NULL;
  }
#else
  strcat(setCommd, " 2> /dev/stdout");
  handle = popen(setCommd, "r");
  free(setCommd);
  if (handle == NULL) {
    fprintf(stderr, "Error: popen call failed");
    return 1;
  }
  if (!feof(handle)) {
    if (fgets(message, sizeof(message), handle)) {
      if (strlen(message) && wild_match(message, (char*)"*error*")) {
        fprintf(stderr, "error message from varScript: %s\n", message);
        return 1;
      }
    }
  }
  pclose(handle);
#endif
  return 0;
}


long setStringPV(char *PV, char *value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, long exitOnError) {
#if defined(DRYRUN)
  return 0;
#else
  if (pva->isConnected[0] == false)
    return 1;
  pva->pvaData[0].skip = false;
  switch (pva->pvaData[0].fieldType) {
  case epics::pvData::scalar: {
    pva->pvaData[0].numPutElements = 1;
    if (pva->pvaData[0].numeric) {
      fprintf(stderr, "error: %s is not a string PV\n", PV);
      FreeEverything();
      exit(1);
    } else {
      if (pva->pvaData[0].putData[0].stringValues == NULL) {
        pva->pvaData[0].putData[0].stringValues = (char**)malloc(sizeof(char*));
        pva->pvaData[0].putData[0].stringValues[0] = (char*)malloc(sizeof(char) * 1024);
      }
      strcpy(pva->pvaData[0].putData[0].stringValues[0], trim_spaces(value));
    }
    break;
  }
  default: {
    fprintf(stderr, "error: unexpected field type for %s\n", PV);
    FreeEverything();
    exit(1);
  }
  }
  
  if (PutPVAValues(pva) == 1) {
    if (exitOnError) {
      fprintf(stderr, "Error setting PVs\n");
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
  return 0;
#endif
}

long setEnumPV(char *PV, long value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, long exitOnError) {
#if defined(DRYRUN)
  return 0;
#else
  if (pva->isConnected[0] == false)
    return 1;
  pva->pvaData[0].skip = false;
  switch (pva->pvaData[0].fieldType) {
  case epics::pvData::structure: {
    if (pva->pvaData[0].pvEnumeratedStructure) {
      pva->pvaData[0].numPutElements = 1;
      
      if (pva->pvaData[0].putData[0].values == NULL) {
        pva->pvaData[0].putData[0].values = (double*)malloc(sizeof(double));
      }
      pva->pvaData[0].putData[0].values[0] = value;
    } else {
      fprintf(stderr, "error: unexpected structure type for %s\n", PV);
      return (1);
    }
    break;
  }
  default: {
    fprintf(stderr, "error: unexpected field type for %s\n", PV);
    FreeEverything();
    exit(1);
  }
  }

  if (PutPVAValues(pva) == 1) {
    if (exitOnError) {
      fprintf(stderr, "Error setting PVs\n");
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
  return 0;
#endif
}

void interrupt_handler2(int sig) {
  exit(1);
}

void sigint_interrupt_handler(int sig) {
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
  sddscontrollawGlobal->sigint = 1;
  return;
}

long readEnumPV(char *PV, long *value, CHANNEL_INFO channelInfo, PVA_OVERALL *pva, double pendIOTime, long exitOnError) {
#if defined(DRYRUN)
  return 0;
#else
  if (pva->isConnected[0] == false)
    return 1;
  freePVAGetReadings(pva);
  if (GetPVAValues(pva) == 1) {
    if (exitOnError) {
      fprintf(stderr, "error: unable to get value for %s\n", PV);
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
  if (IsEnumFieldType(pva, 0) == false) {
    fprintf(stderr, "error: expected enum PV for %s\n", PV);
    FreeEverything();
    exit(1);
  }
  *value = (long)pva->pvaData[0].getData[0].values[0];

  return 0;
#endif
}

#ifdef SUNOS4
void interrupt_handler()
#else
void interrupt_handler(int sig)
#endif
{
  FreeEverything();
  exit(1);
}

/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;
  timeLeft = sleepTime;
  do {
    sddscontrollawGlobal->rcParam.status = runControlPing(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo));
    switch (sddscontrollawGlobal->rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
      FreeEverything();
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      strcpy(sddscontrollawGlobal->rcParam.message, "Application timed-out");
      runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
      FreeEverything();
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
    if (pvaEscapableThreadSleep((MIN(timeLeft, sddscontrollawGlobal->rcParam.pingInterval)), &(sddscontrollawGlobal->sigint)) == 1) {
      FreeEverything();
      exit(1);
    }
    
    timeLeft -= sddscontrollawGlobal->rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}

void setupReadbackLimitFile(LIMITS *readbackLimits) {
  SDDS_TABLE readbackLimitPage;

  if (readbackLimits->file) {
    if (readbackLimits->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&readbackLimitPage, readbackLimits->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&readbackLimitPage, readbackLimits->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&readbackLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, (char*)"minValue", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "minValue");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, (char*)"maxValue", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "maxValue");
      FreeEverything();
      exit(1);
    }
    readbackLimits->n = SDDS_CountRowsOfInterest(&readbackLimitPage);
    readbackLimits->controlName = (char **)SDDS_GetColumn(&readbackLimitPage, (char*)"ControlName");
    readbackLimits->minValue = (double *)SDDS_GetColumn(&readbackLimitPage, (char*)"minValue");
    readbackLimits->maxValue = (double *)SDDS_GetColumn(&readbackLimitPage, (char*)"maxValue");

    if (!SDDS_Terminate(&readbackLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    readbackLimits->n = 0;
    readbackLimits->controlName = NULL;
    readbackLimits->minValue = NULL;
    readbackLimits->maxValue = NULL;
  }
  return;
}

void setupDeltaLimitFile(LIMITS *delta) {
  SDDS_TABLE deltaLimitPage;

  if (delta->file) {
    if (delta->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&deltaLimitPage, delta->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&deltaLimitPage, delta->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&deltaLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&deltaLimitPage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&deltaLimitPage, (char*)"DeltaLimit", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "DeltaLimit");
      FreeEverything();
      exit(1);
    }
    delta->n = SDDS_CountRowsOfInterest(&deltaLimitPage);
    delta->controlName = (char **)SDDS_GetColumn(&deltaLimitPage, (char*)"ControlName");
    delta->value = (double *)SDDS_GetColumn(&deltaLimitPage, (char*)"DeltaLimit");
    if (!SDDS_Terminate(&deltaLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    delta->n = 0;
    delta->controlName = NULL;
    delta->value = NULL;
  }
  return;
}

void setupActionLimitFile(LIMITS *action) {
  SDDS_TABLE actionLimitPage;

  if (action->file) {
    if (action->searchPath) {
      if (SDDS_InitializeInputFromSearchPath(&actionLimitPage, action->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&actionLimitPage, action->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&actionLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&actionLimitPage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&actionLimitPage, (char*)"ActionLimit", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ActionLimit");
      FreeEverything();
      exit(1);
    }
    action->n = SDDS_CountRowsOfInterest(&actionLimitPage);
    action->controlName = (char **)SDDS_GetColumn(&actionLimitPage, (char*)"ControlName");
    action->value = (double *)SDDS_GetColumn(&actionLimitPage, (char*)"ActionLimit");
    if (!SDDS_Terminate(&actionLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    action->n = 0;
    action->controlName = NULL;
    action->value = NULL;
  }
  return;
}

void setupInputFile(CORRECTION *correction) {
  long i, aorder, border, subscript, j, k;
  int32_t columns, coefRows;
  long columnType;
  long *doubleColumnIndex;
  SDDS_TABLE inputPage, coefPage;
  char **name, **coefName = NULL, **coefDeviceNames = NULL;
  CONTROL_NAME *control, *readback;
  MATRIX *K, *A, *B;
  double **acoef, **bcoef;
  char *coefNameString = NULL;

  readback = correction->readback;
  control = correction->control;
  K = correction->K;
  control->file = correction->file;
  if (correction->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&inputPage, correction->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&inputPage, correction->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  name = (char **)SDDS_GetColumnNames(&inputPage, &columns);
  if ((readback->symbolicName = (char **)SDDS_Realloc(readback->symbolicName, (columns - 1) * sizeof(char *))) == NULL) {
    fprintf(stderr, "can't realloc readback->symbolicName\n");
    FreeEverything();
    exit(1);
  }
  if (readback->waveformMatchFound) {
    free(readback->waveformMatchFound);
  }
  if ((readback->waveformMatchFound = (short*)calloc(columns, sizeof(short))) == NULL) {
    fprintf(stderr, "can't calloc readback->waveformMatchFound\n");
    FreeEverything();
    exit(1);
  }

  if (0 > SDDS_ReadTable(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  control->n = SDDS_CountRowsOfInterest(&inputPage);
  if (control->waveformMatchFound) {
    free(control->waveformMatchFound);
  }
  if ((control->waveformMatchFound = (short*)calloc(control->n, sizeof(short))) == NULL) {
    fprintf(stderr, "can't calloc control->waveformMatchFound\n");
    FreeEverything();
    exit(1);
  }

  /* check existence of actuator column */
  if (correction->actuator) {
    switch (SDDS_CheckColumn(&inputPage, correction->actuator, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, correction->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", correction->actuator, correction->file);
      FreeEverything();
      exit(1);
    }
  }
  /* find numeric columns */
  if (!SDDS_SetColumnFlags(&inputPage, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!(doubleColumnIndex = (long *)calloc(1, (columns) * sizeof(long)))) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->n = 0;
  for (i = 0; i < columns; i++) {
    long index;
    if ((index = SDDS_GetColumnIndex(&inputPage, name[i])) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, index))) {
      doubleColumnIndex[readback->n] = i;
      SDDS_CopyString(&readback->symbolicName[readback->n], name[i]);
      if (!SDDS_AssertColumnFlags(&inputPage, SDDS_INDEX_LIMITS, index, index, 1L)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      readback->n++;
    } else if (columnType == SDDS_STRING && !(correction->actuator)) {
      SDDS_CopyString(&correction->actuator, name[i]);
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, correction->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
  }

  for (i = 0; i < columns; i++)
    free(name[i]);
  free(name);
  free(doubleColumnIndex);

  control->controlName = control->symbolicName;
  readback->controlName = readback->symbolicName;
  /*assign controlName to readback and controls */
  if (correction->definitionFile) {
    assignControlName(correction, correction->definitionFile);
  } else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  /*********************\
             * read matrix       *
   \********************/
  /*  m_alloc(&K, control->n, readback->n); */
  K->m = control->n;
  K->n = readback->n;
  K->a = (double **)SDDS_GetMatrixOfRows(&inputPage, &(control->n));
  if (!SDDS_Terminate(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  /************************ \
             * read coefficients file*
   \************************/
  if (correction->coefFile) {
    if ((coefNameString = (char *)malloc(sizeof(char) * 4)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if (correction->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&coefPage, correction->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&coefPage, correction->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    coefName = (char **)SDDS_GetColumnNames(&coefPage, &columns);
    if (0 > SDDS_ReadTable(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    coefRows = SDDS_CountRowsOfInterest(&coefPage);
    if (!(coefDeviceNames = (char **)SDDS_GetColumn(&coefPage, (char*)"DeviceName"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    /*********************                      \
                 * read coefficients *
     \********************/
    /* find columns that start with letter "a" and determine the order of the filter */
    aorder = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "a%ld", &subscript)) {
          if (aorder < subscript)
            aorder = subscript;
        }
      }
    }
    border = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "b%ld", &subscript)) {
          if (aorder < subscript)
            border = subscript;
        }
      }
    }
    for (i = 0; i < columns; i++)
      free(coefName[i]);
    free(coefName);
    m_alloc(&A, (aorder + 1), control->n);
    correction->aCoef = A;
    m_zero(A);
    m_alloc(&B, (border + 1), control->n);
    correction->bCoef = B;
    m_zero(B);
    /* allocate arrays for history */
    m_alloc(&(control->history), (border + 1), control->n);
    m_alloc(&(control->historyFiltered), (aorder + 1), control->n);

    acoef = (double**)malloc(sizeof(*acoef) * (aorder + 1));
    bcoef = (double**)malloc(sizeof(*bcoef) * (border + 1));

    for (subscript = 0; subscript <= aorder; subscript++) {
      acoef[subscript] = NULL;
      snprintf(coefNameString, 4, "a%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
        /*set it to be zero if not provided */
        acoef[subscript] = (double *)calloc(coefRows, sizeof(**acoef));
      } else {
        acoef[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }
    for (subscript = 0; subscript <= border; subscript++) {
      bcoef[subscript] = NULL;
      snprintf(coefNameString, 4, "b%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
        /*set it to be zero if not provided */
        bcoef[subscript] = (double*)calloc(coefRows, sizeof(**bcoef));
      } else {
        bcoef[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }

    for (i = 0; i < control->n; i++) {
      j = match_string(control->symbolicName[i], coefDeviceNames, coefRows, EXACT_MATCH);
      if (j < 0) {
        A->a[0][i] = 1.0;
        B->a[0][i] = 1.0;
      } else {
        for (k = 0; k <= aorder; k++)
          A->a[k][i] = acoef[k][j];
        for (k = 0; k <= border; k++)
          B->a[k][i] = bcoef[k][j];
      }
    }
    for (k = 0; k <= aorder; k++)
      free(acoef[k]);
    free(acoef);
    acoef = NULL;
    for (k = 0; k <= border; k++)
      free(bcoef[k]);
    free(bcoef);
    bcoef = NULL;

    if (!SDDS_Terminate(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < coefRows; i++)
      free(coefDeviceNames[i]);
    free(coefDeviceNames);
    free(coefNameString);
  }
  checkRCParam(&inputPage, correction->file);
}

void setupCompensationFiles(CORRECTION *compensation) {
  long i, rows;
  int32_t columns;
  long columnType;
  long *doubleColumnIndex;
  SDDS_TABLE inputPage, coefPage;
  char **name, **coefName, *coefNameString;
  CONTROL_NAME *control, *readback;
  MATRIX *K, *A, *B;
  long aorder, border, subscript;

  if (!compensation->file)
    return;

  readback = compensation->readback;
  control = compensation->control;

  K = compensation->K;
  A = compensation->aCoef;
  B = compensation->bCoef;
  control->file = compensation->file;
  if (compensation->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&inputPage, compensation->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&inputPage, compensation->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  name = (char **)SDDS_GetColumnNames(&inputPage, &columns);
  if ((readback->symbolicName = (char **)SDDS_Realloc(readback->symbolicName, columns * sizeof(char *))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (readback->waveformMatchFound) {
    free(readback->waveformMatchFound);
  }
  if ((readback->waveformMatchFound = (short*)calloc(columns, sizeof(short))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  if (0 > SDDS_ReadTable(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  control->n = SDDS_CountRowsOfInterest(&inputPage);
  if (control->waveformMatchFound) {
    free(control->waveformMatchFound);
  }
  if ((control->waveformMatchFound = (short*)calloc(control->n, sizeof(short))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  /* check existence of actuator column */
  if (compensation->actuator) {
    switch (SDDS_CheckColumn(&inputPage, compensation->actuator, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, compensation->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", compensation->actuator, compensation->file);
      FreeEverything();
      exit(1);
    }
  }
  /* find numeric columns */
  if (!SDDS_SetColumnFlags(&inputPage, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if ((doubleColumnIndex = (long *)calloc(1, columns * sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->n = 0;
  for (i = 0; i < columns; i++) {
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, i))) {
      doubleColumnIndex[readback->n] = i;
      SDDS_CopyString(&readback->symbolicName[readback->n], name[i]);
      SDDS_SetColumnsOfInterest(&inputPage, SDDS_MATCH_STRING, name[i], SDDS_OR);
      readback->n++;
    } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
      SDDS_CopyString(&compensation->actuator, name[i]);
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, compensation->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
  }

  /*assign control names */
  if (compensation->definitionFile)
    assignControlName(compensation, compensation->definitionFile);
  else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  /*********************\
             * read matrix       *
   \********************/
  /*  m_alloc(&K, control->n, readback->n); */
  K->m = control->n;
  K->n = readback->n;
  K->a = (double **)SDDS_GetMatrixOfRows(&inputPage, &(control->n));
  if (!SDDS_Terminate(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  /*************************\
             * read coefficients file*
   \************************/
  if (compensation->coefFile) {
    if (compensation->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&inputPage, compensation->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&coefPage, compensation->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    coefName = (char **)SDDS_GetColumnNames(&coefPage, &columns);
    if (0 > SDDS_ReadTable(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    rows = SDDS_CountRowsOfInterest(&coefPage);
    if (rows != control->n) {
      FreeEverything();
      SDDS_Bomb((char*)"Number of rows in filter and compensation matrix files do not match.");
    }
    /*********************\
                 * read coefficients *
     \********************/
    /* find columns that start with letter "a" and determine the order of the filter */
    aorder = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "a%ld", &subscript)) {
          if (aorder < subscript)
            aorder = subscript;
        }
      } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
        compensation->actuator = coefName[i];
        if (!(control->symbolicName = (char **)SDDS_GetColumn(&coefPage, compensation->actuator))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
    }
    m_alloc(&A, (aorder + 1), control->n);
    compensation->aCoef = A;
    m_zero(A);
    if ((coefNameString = (char *)malloc(sizeof(char) * 4)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (subscript = 0; subscript <= aorder; subscript++) {
      snprintf(coefNameString, 4, "a%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
      } else {
        free(A->a[subscript]);
        A->a[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }
    /* find columns that start with letter "b" and determine the order of the filter */
    border = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "b%ld", &subscript)) {
          if (border < subscript)
            border = subscript;
        }
      } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
        compensation->actuator = coefName[i];
        if (!(control->symbolicName = (char **)SDDS_GetColumn(&coefPage, compensation->actuator))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
    }
    m_alloc(&B, (border + 1), control->n);
    compensation->bCoef = B;
    m_zero(B);
    for (subscript = 0; subscript <= border; subscript++) {
      snprintf(coefNameString, 4, "b%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
      } else {
        free(B->a[subscript]);
        B->a[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }

    /* allocate arrays for history */
    m_alloc(&control->history, (border + 1), control->n);
    m_alloc(&control->historyFiltered, (aorder + 1), control->n);
    if (!SDDS_Terminate(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    /* no filter file given */
  }
  for (i = 0; i < columns; i++)
    free(name[i]);
  free(name);
  free(doubleColumnIndex);
  return;
}

/*assign controlName for controls or readbacks */
void assignControlName(CORRECTION *correction, char *definitionFile) {
  long i, j, row_match, rows = 0;
  SDDS_DATASET definitionPage;
  char **def_symbolicName, **def_controlName;
  CONTROL_NAME *control, *readback;

  def_symbolicName = def_controlName = NULL;
  control = correction->control;
  readback = correction->readback;
  control->controlName = NULL;
  readback->controlName = NULL;

  if (!control->n && !readback->n)
    return;
  if (definitionFile) {
    /*get information from defitionFile */
    if (correction->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&definitionPage, definitionFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&definitionPage, definitionFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadPage(&definitionPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&definitionPage, (char*)"SymbolicName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "SymbolicName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&definitionPage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }

    rows = SDDS_CountRowsOfInterest(&definitionPage);
    def_symbolicName = (char **)SDDS_GetColumn(&definitionPage, (char*)"SymbolicName");
    def_controlName = (char **)SDDS_GetColumn(&definitionPage, (char*)"ControlName");
    if (!SDDS_Terminate(&definitionPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    /*assign controlName */
    if (control->n) {
      if ((control->controlName = (char **)malloc(sizeof(char *) * control->n)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if (0 > (row_match = match_string(control->symbolicName[i], def_symbolicName, rows, EXACT_MATCH))) {
        fprintf(stderr, "Name %s doesn't exist in column %s of file %s.\n", control->symbolicName[i], "SymbolicName", correction->definitionFile);
        for (j = 0; j < i; j++) {
          free(control->controlName[j]);
        }
        free(control->controlName);
        control->controlName = NULL;
        FreeEverything();
        exit(1);
      }
      SDDS_CopyString(&control->controlName[i], def_controlName[row_match]);
    }
    if (readback->n) {
      if ((readback->controlName = (char **)malloc(sizeof(char *) * readback->n)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < readback->n; i++) {
      if (0 > (row_match = match_string(readback->symbolicName[i], def_symbolicName, rows, EXACT_MATCH))) {
        fprintf(stderr, "Name %s doesn't exist in column %s of file %s.\n", readback->symbolicName[i], "SymbolicName", correction->definitionFile);
        for (j = 0; j < i; j++) {
          free(readback->controlName[j]);
        }
        free(readback->controlName);
        readback->controlName = NULL;
        FreeEverything();
        exit(1);
      }
      SDDS_CopyString(&readback->controlName[i], def_controlName[row_match]);
    }
    /*free memory */
    for (i = 0; i < rows; i++) {
      free(def_symbolicName[i]);
      free(def_controlName[i]);
    }
    free(def_symbolicName);
    free(def_controlName);
  } else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  return;
}

void matchUpControlNames(LIMITS *limits, char **controlName, long n) {
  long i, row_match;

  if (limits->n == 0)
    return;
  if ((limits->index = (long *)malloc(sizeof(long) * (limits->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (limits->flag & LIMIT_FILE) {
    for (i = 0; i < limits->n; i++) {
      if (0 > (row_match = match_string(limits->controlName[i], controlName, n, EXACT_MATCH))) {
        fprintf(stderr, "Name %s in file %s doesn't match known control names.\n", limits->controlName[i], limits->file);
        FreeEverything();
        exit(1);
      }
      limits->index[i] = row_match;
    }
  }
  return;
}

void setupTestsFile(TESTS *test, double interval, long verbose, double pendIOTime) {
  long itest;
  SDDS_TABLE testValuesPage;
  long sleepTimeColumnPresent = 0, resetTimeColumnPresent = 0;
  long despikeColumnPresent = 0, holdOffTimeColumnPresent = 0;
  long holdOffIntervalColumnPresent = 0, sleepIntervalColumnPresent = 0, i;
  long glitchLogColumnPresent = 0;

  if (!test->file)
    return;
  if (test->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&testValuesPage, test->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&testValuesPage, test->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  switch (SDDS_CheckColumn(&testValuesPage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"MinimumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MinimumValue", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"MaximumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MaximumValue", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"SleepTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SleepTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"SleepIntervals", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepIntervalColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepIntervalColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SleepIntervals", test->file);
    FreeEverything();
    exit(1);
  }
  if (sleepTimeColumnPresent && sleepIntervalColumnPresent) {
    fprintf(stderr, "SleepTime Column and SleepIntervals column can not exist in the same time.\n");
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, (char*)"ResetTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    resetTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    resetTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"HoldOffTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    holdOffTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    holdOffTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, (char*)"HoldOffIntervals", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    holdOffIntervalColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    holdOffIntervalColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  if (holdOffTimeColumnPresent && holdOffIntervalColumnPresent) {
    fprintf(stderr, "HoldOffTime column and HoldOffIntervals column can not exist in the same time.\n");
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, (char*)"Despike", NULL, SDDS_LONG, NULL)) {
  case SDDS_CHECK_OKAY:
    despikeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    despikeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", test->file);
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, (char*)"GlitchLog", NULL, SDDS_CHARACTER, NULL)) {
  case SDDS_CHECK_OKAY:
    glitchLogColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    glitchLogColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "GlitchLog", test->file);
    FreeEverything();
    exit(1);
  }

  if (SDDS_ReadTable(&testValuesPage) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  test->n = SDDS_CountRowsOfInterest(&testValuesPage);
  if ((test->value = (double *)malloc(sizeof(double) * (test->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((test->outOfRange = (long *)malloc(sizeof(long) * (test->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose) {
    fprintf(stderr, "number of test values = %ld\n", test->n);
  }
  test->controlName = (char **)SDDS_GetColumn(&testValuesPage, (char*)"ControlName");
  test->min = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"MinimumValue");
  test->max = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"MaximumValue");
  if (sleepTimeColumnPresent)
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"SleepTime");
  if (sleepIntervalColumnPresent) {
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"SleepIntervals");
    for (i = 0; i < test->n; i++)
      test->sleep[i] = test->sleep[i] * interval;
  }
  if (holdOffTimeColumnPresent)
    test->holdOff = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"HoldOffTime");
  if (holdOffIntervalColumnPresent) {
    test->holdOff = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"HoldOffIntervals");
    for (i = 0; i < test->n; i++)
      test->holdOff[i] = test->holdOff[i] * interval;
  }
  if (resetTimeColumnPresent)
    test->reset = SDDS_GetColumnInDoubles(&testValuesPage, (char*)"ResetTime");
  if (despikeColumnPresent) {
    test->despike = (int32_t *)SDDS_GetColumn(&testValuesPage, (char*)"Despike");
    /* make new array for test values that are to be despiked */
    /* the following arrays only need to be as long as the number
         of test variables with despike flag!=0, but this number
         is unknown in advance. */
    if ((test->despikeIndex = (int32_t *)malloc(sizeof(long) * test->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    test->despikeValues = 0;
    for (itest = 0; itest < test->n; itest++) {
      if (test->despike[itest]) {
        test->despikeIndex[test->despikeValues] = itest;
        test->despikeValues++;
      }
    }
  }
  if (glitchLogColumnPresent) {
    test->glitchLog = (char *)SDDS_GetColumn(&testValuesPage, (char*)"GlitchLog");
  }
  /*setup raw connection */
  if ((test->channelInfo = (CHANNEL_INFO*)calloc(1, sizeof(CHANNEL_INFO) * test->n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for test PVs.\n");
  if ((test->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  SetupRawPVAConnection(test->controlName, test->channelInfo, test->n, pendIOTime, &(test->pva));
  if (verbose) {
    for (itest = 0; itest < test->n; itest++) {
      fprintf(stderr, "%3ld%30s\t%11.3f\t%11.3f", itest, test->controlName[itest], test->min[itest], test->max[itest]);
      if (sleepTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->sleep[itest]);
      if (resetTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->reset[itest]);
      fprintf(stderr, "\n");
    }
  }
  if (!SDDS_Terminate(&testValuesPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  return;
}

void setupDespikeFile(DESPIKE_PARAM *despikeParam, CONTROL_NAME *readback, long verbose) {

  SDDS_TABLE despikePage;
  long i, row_match, symbolicExist = 0;

  if (!despikeParam->file) {
    return;
  }
  if (despikeParam->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&despikePage, despikeParam->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&despikePage, despikeParam->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  switch (SDDS_CheckColumn(&despikePage, (char*)"ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&despikePage, (char*)"Despike", NULL, SDDS_LONG, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&despikePage, (char*)"SymbolicName", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    symbolicExist = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    symbolicExist = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SymolicName", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  if (SDDS_ReadTable(&despikePage) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  despikeParam->n = SDDS_CountRowsOfInterest(&despikePage);
  if (verbose)
    fprintf(stderr, "Number of rows in file %s = %d\n", despikeParam->file, despikeParam->n);
  despikeParam->controlName = (char **)SDDS_GetColumn(&despikePage, (char*)"ControlName");
  despikeParam->despike = (int32_t *)SDDS_GetColumn(&despikePage, (char*)"Despike");
  if (symbolicExist)
    despikeParam->symbolicName = (char **)SDDS_GetColumn(&despikePage, (char*)"SymbolicName");
  else
    despikeParam->symbolicName = despikeParam->controlName;
  /* match up the control names from this file
     with the control names from the CONTROL_NAME structure.
     and transfer the value of the despike column */
  for (i = 0; i < readback->n; i++) /* initial value. Assume all readbacks to be despiked */
    readback->despike[i] = 1;
  for (i = 0; i < despikeParam->n; i++) {
    if (0 > (row_match = match_string(despikeParam->controlName[i], readback->controlName, readback->n, EXACT_MATCH))) {
      if (verbose) {
        fprintf(stderr, "Warning: Name %s in file %s doesn't match with any name.\n", despikeParam->controlName[i], despikeParam->file);
      }
    } else
      readback->despike[row_match] = despikeParam->despike[i];
  }

  if (!SDDS_Terminate(&despikePage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  return;
}

void setupOutputFile(SDDS_TABLE *outputPage, char *file, CORRECTION *correction, TESTS *test, LOOP_PARAM *loopParam, char *timeStamp, WAVEFORM_TESTS *waveform_tests, long verbose) {
  long i, j;
  CONTROL_NAME *control, *readback;

  readback = correction->readback;
  control = correction->control;
  if (file) {
    if (!SDDS_InitializeOutput(outputPage, SDDS_BINARY, 1,
                               "Output and control variables",
                               "Readback and control variables", file) ||
        (0 > SDDS_DefineParameter(outputPage, "Gain", "Gain", NULL,
                                  "Gain", NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(outputPage, "OverlapCompensationGain",
                                  "OverlapCompensationGain", NULL,
                                  "OverlapCompensationGain", NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(outputPage, "TimeStamp",
                                  "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(outputPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(outputPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(outputPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(outputPage);
    for (i = 0; i < readback->n; i++) {
      if (0 > SDDS_DefineColumn(outputPage, readback->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if (0 > SDDS_DefineColumn(outputPage, control->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    /* some tests PVs may be the same as those
         in the readback set or the control set. If there is 
         a case of identical column names, then the program will return an error.
         So far, we haven't encountered that situation.
       */
    if (test && test->n) {
      if ((test->writeToFile = (int32_t *)malloc(test->n * sizeof(int32_t))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (i = 0; i < test->n; i++) {
        if (match_string(test->controlName[i], readback->controlName, readback->n, EXACT_MATCH) >= 0 ||
            match_string(test->controlName[i], control->controlName, control->n, EXACT_MATCH) >= 0) {
          test->writeToFile[i] = 0;
        } else {
          test->writeToFile[i] = 1;
          if (0 > SDDS_DefineColumn(outputPage, test->controlName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            FreeEverything();
            exit(1);
          }
        }
      }
    }
    if (waveform_tests->waveformTests) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
          if (!waveform_tests->ignore[i][j]) {
            if (match_string(waveform_tests->DeviceName[i][j], readback->symbolicName, readback->n, EXACT_MATCH) >= 0 ||
                match_string(waveform_tests->DeviceName[i][j], control->symbolicName, control->n, EXACT_MATCH) >= 0) {
              waveform_tests->writeToFile[i][j] = 0;
            } else {
              waveform_tests->writeToFile[i][j] = 1;
              if (0 > SDDS_DefineColumn(outputPage, waveform_tests->DeviceName[i][j], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                FreeEverything();
                exit(1);
              }
            }
          } else
            waveform_tests->writeToFile[i][j] = 0;
        }
      }
    }

    if (!SDDS_WriteLayout(outputPage) || !SDDS_StartTable(outputPage, loopParam->updateInterval)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "TimeStamp of output file %s: %s\n", file, timeStamp);
    if (!SDDS_SetParameters(outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, NULL) ||
        !SDDS_SetParameters(outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Gain", loopParam->gain, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void setupGlitchFile(SDDS_TABLE *glitchPage, GLITCH_PARAM *glitchParam, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, char *timeStamp, long verbose) {
  long i;
  CONTROL_NAME *control, *readback;
  CONTROL_NAME *controlComp, *readbackComp;
  char *buffer = NULL;

  readback = correction->readback;
  control = correction->control;

  if (glitchParam->file) {
    if (!SDDS_InitializeOutput(glitchPage, SDDS_BINARY, 1,
                               "Readback and control variables at glitch",
                               "Readback and control variables at glitch", glitchParam->file) ||
        (0 > SDDS_DefineParameter(glitchPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "ReadbackRmsThreshold", "ReadbackRmsThreshold", NULL, "ReadbackRmsThreshold",
                                  NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "ControlRmsThreshold", "ControlRmsThreshold", NULL, "ControlRmsThreshold",
                                  NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "GlitchRows", "GlitchRows", NULL, "GlitchRows",
                                  NULL, SDDS_LONG, NULL)) ||
        (0 > SDDS_DefineColumn(glitchPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(glitchPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(glitchPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(glitchPage);
    for (i = 0; i < readback->n; i++)
      if (0 > SDDS_DefineColumn(glitchPage, readback->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    for (i = 0; i < control->n; i++) {
      if (0 > SDDS_DefineColumn(glitchPage, control->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      if ((buffer = (char*)malloc(sizeof(char) * (strlen(control->symbolicName[i]) + 6))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      snprintf(buffer, strlen(control->symbolicName[i]) + 6, "%sDelta", control->symbolicName[i]);
      if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        free(buffer);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      free(buffer);
    }
    if (overlapCompensation->file) {
      readbackComp = overlapCompensation->readback;
      controlComp = overlapCompensation->control;
      for (i = 0; i < readbackComp->n; i++) {
        if ((buffer = (char*)malloc(sizeof(char) * (strlen(readbackComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer, strlen(readbackComp->symbolicName[i]) + 6, "%sAuxil", readbackComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
      }
      for (i = 0; i < controlComp->n; i++) {
        if ((buffer = (char*)malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer, strlen(controlComp->symbolicName[i]) + 6, "%sAuxil", controlComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
        if ((buffer = (char*)malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 11))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer, strlen(controlComp->symbolicName[i]) + 11, "%sAuxilDelta", controlComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
      }
    }
    if (!SDDS_WriteLayout(glitchPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void setupStatsFile(SDDS_TABLE *statsPage, char *statsFile, LOOP_PARAM *loopParam, char *timeStamp, long verbose) {

  if (statsFile) {
    if (!SDDS_InitializeOutput(statsPage, SDDS_BINARY, 1,
                               "sddscontrollaw statistics of readback and control variables",
                               NULL, statsFile) ||
        (0 > SDDS_DefineParameter(statsPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(statsPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(statsPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(statsPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(statsPage);
    if (!loopParam->briefStatistics) {
      if (0 > SDDS_DefineColumn(statsPage, "readbackRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaRMS", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaAve", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaMAD", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (0 > SDDS_DefineColumn(statsPage, "readbackLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackDeltaLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackDeltaLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlDeltaLargest", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlDeltaLargestName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_WriteLayout(statsPage) || !SDDS_StartTable(statsPage, loopParam->updateInterval)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "TimeStamp of stats file: %s\n", timeStamp);
    if (!SDDS_SetParameters(statsPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void getInitialValues(CORRECTION *correction, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime) {

  CONTROL_NAME *control, *readback;
  long i;

  if (!correction->file)
    return;

  readback = correction->readback;
  control = correction->control;

  if (controlWaveforms && controlWaveforms->waveforms) {
    if (control->integral) {
      if (ReadWaveformData(controlWaveforms, pendIOTime)) {
        FreeEverything();
        exit(1);
      }
      for (i = 0; i < control->n; i++)
        control->value[0][i] = controlWaveforms->readbackValue[i];
    } else {
      for (i = 0; i < control->n; i++)
        control->value[0][i] = 0;
    }
  } else {
    /*setup connection */
    if ((control->channelInfo = (CHANNEL_INFO*)calloc(1, sizeof(CHANNEL_INFO) * control->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->setFlag = (short*)malloc(sizeof(short) * control->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawPVAConnection(control->controlName, control->channelInfo, control->n, pendIOTime, &(control->pva));
    if (control->integral) {
      if (verbose)
        fprintf(stderr, "Reading initial values for %" PRId64 " control devices.\n", control->n);
      readPVs(control->controlName, control->value[0], control->channelInfo, &control->pva, control->n, NULL, pendIOTime);
    } else {
      if (verbose)
        fprintf(stderr, "Reading initial values for %" PRId64 " control devices.\n", control->n);
      readPVs(control->controlName, control->value[0], control->channelInfo, &control->pva, control->n, NULL, pendIOTime);
      for (i = 0; i < control->n; i++)
        control->value[0][i] = 0;
    }
  }

  if (verbose)
    fprintf(stderr, "Reading initial values for %" PRId64 " readback PVs.\n", readback->n);
  if (readbackWaveforms && readbackWaveforms->waveforms) {
    if (ReadWaveformData(readbackWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] = readbackWaveforms->readbackValue[i];
  } else {
    if ((readback->channelInfo = (CHANNEL_INFO*)calloc(1, sizeof(CHANNEL_INFO) * readback->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawPVAConnection(readback->controlName, readback->channelInfo, readback->n, pendIOTime, &(readback->pva));
    readPVs(readback->controlName, readback->value[0], readback->channelInfo, &readback->pva, readback->n, aveParam, pendIOTime);
  }
  for (i = 0; i < readback->n; i++)
    if (isnan(readback->value[0][i]))
      fprintf(stderr, "i=%ld, name=%s, value=%f\n", i, readback->controlName[i], readback->value[0][i]);
  for (i = 0; i < control->n; i++) {
    control->initial[i] = control->value[0][i];
    control->old[i] = control->value[0][i];
  }
  for (i = 0; i < readback->n; i++) {
    if (!loopParam->offsetFile)
      readback->initial[i] = readback->value[0][i];
    readback->old[i] = readback->value[0][i];
  }
  if (loopParam->holdPresentValues) {
    if (!loopParam->offsetFile)
      for (i = 0; i < readback->n; i++)
        readback->setpoint[i] = readback->value[0][i];
    else
      for (i = 0; i < readback->n; i++)
        readback->setpoint[i] = readback->initial[i];
  }

  return;
}

long initializeHistories(CORRECTION *compensation) {
  long i, j;
  CONTROL_NAME *control;
  long aorder, border;

  if (!compensation->coefFile)
    return 0;
  control = compensation->control;
  aorder = control->historyFiltered->n - 1;
  border = control->history->n - 1;

  for (j = 0; j < control->n; j++) {
    for (i = 0; i <= border; i++) {
      control->history->a[i][j] = 0.0;
    }
    for (i = 0; i <= aorder; i++)
      /* the filtered data is supposed to be 0 A at DC. */
      control->historyFiltered->a[i][j] = 0.0;
  }
  return 0;
}

long getReadbackValues(CONTROL_NAME *readback, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *offsetWaveforms, long verbose, double pendIOTime) {
  int64_t i, largestIndex, maxIndex, minIndex;

  if (verbose) {
    fprintf(stderr, "Reading readback devices at %f seconds.\n", loopParam->elapsedTime[0]);
  }
  for (i = 0; i < readback->n; i++)
    readback->old[i] = readback->value[0][i];
  if (readbackWaveforms && readbackWaveforms->waveforms) {
    if (ReadWaveformData(readbackWaveforms, pendIOTime))
      return 1;
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] = readbackWaveforms->readbackValue[i];
  } else {
    readPVs(readback->controlName, readback->value[0], readback->channelInfo, &readback->pva, readback->n, aveParam, pendIOTime);
  }
  if (offsetWaveforms && offsetWaveforms->waveforms) {
    if (ReadWaveformData(offsetWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] -= offsetWaveforms->readbackValue[i];
  } else {
    if (loopParam->offsetPVFile) {
      readPVs(loopParam->offsetPV, loopParam->offsetPVvalue, loopParam->channelInfo, &loopParam->pva, readback->n, aveParam, pendIOTime);
      for (i = 0; i < readback->n; i++)
        readback->value[0][i] -= loopParam->offsetPVvalue[i];
    }
  }

  if (readbackStats) {
    readbackStats->RMS = standardDeviation(readback->value[0], readback->n);
    readbackStats->ave = arithmeticAverage(readback->value[0], readback->n);
    readbackStats->MAD = meanAbsoluteDeviation(readback->value[0], readback->n);
    index_min_max(&minIndex, &maxIndex, readback->value[0], readback->n);
    if ((readback->value[0])[maxIndex] > -(readback->value[0])[minIndex]) {
      largestIndex = maxIndex;
    } else {
      largestIndex = minIndex;
    }
    readbackStats->largest = readback->value[0][largestIndex];
    readbackStats->largestName = readback->controlName[largestIndex];
    snprintf(readbackStats->msg, 256, "Readback devices        %10.3g %10.3g"
                                " %10.3g %10.3g (%s)",
            readbackStats->ave, readbackStats->RMS, readbackStats->MAD, readbackStats->largest, readbackStats->largestName);
  }
  if (readbackDeltaStats) {
    for (i = 0; i < readback->n; i++) {
      readback->delta[0][i] = readback->value[0][i] - readback->old[i];
    }
    readbackDeltaStats->RMS = standardDeviation(readback->delta[0], readback->n);
    readbackDeltaStats->ave = arithmeticAverage(readback->delta[0], readback->n);
    readbackDeltaStats->MAD = meanAbsoluteDeviation(readback->delta[0], readback->n);
    index_min_max(&minIndex, &maxIndex, readback->delta[0], readback->n);
    if (readback->delta[0][maxIndex] > -readback->delta[0][minIndex]) {
      largestIndex = maxIndex;
    } else {
      largestIndex = minIndex;
    }
    readbackDeltaStats->largest = readback->delta[0][largestIndex];
    readbackDeltaStats->largestName = readback->controlName[largestIndex];
    snprintf(readbackDeltaStats->msg, 256, "Readback device deltas %10.3g %10.3g"
                                     " %10.3g %10.3g (%s)",
            readbackDeltaStats->ave, readbackDeltaStats->RMS, readbackDeltaStats->MAD, readbackDeltaStats->largest, readbackDeltaStats->largestName);
  }

  return (0);
}

long checkActionLimit(LIMITS *action, CONTROL_NAME *readback) {

  long skipIteration;
  long i, actionLimitRow;

  skipIteration = 0;
  if (action->flag & LIMIT_VALUE) {
    skipIteration = 1;
    for (i = 0; i < readback->n; i++) {
      if (fabs(readback->value[0][i]) > action->singleValue) {
        skipIteration = 0;
        break;
      }
    }
  } else if (action->flag & LIMIT_FILE) {
    skipIteration = 1;
    for (actionLimitRow = 0; actionLimitRow < action->n; actionLimitRow++) {
      if (fabs(readback->value[0][action->index[actionLimitRow]]) > action->value[actionLimitRow]) {
        skipIteration = 0;
        break;
      }
    }
  }
  return (skipIteration);
}

void adjustReadbacks(CONTROL_NAME *readback, LIMITS *limits, DESPIKE_PARAM *despikeParam, STATS *readbackAdjustedStats, long verbose) {
  long i, j, pos, restored, spikesDone;
  int64_t min_index, max_index, largest_index;
  double *tempArray;

  if (limits->flag & LIMIT_VALUE) {
    for (i = 0; i < readback->n; i++) {
      if (fabs(readback->value[0][i]) > limits->singleValue) {
        readback->value[0][i] = SIGN(readback->value[0][i]) * limits->singleValue;
      }
    }
  } else if (limits->flag & LIMIT_MINMAXVALUE) {
    for (i = 0; i < readback->n; i++) {
      if (readback->value[0][i] > limits->singleMaxValue) {
        readback->value[0][i] = limits->singleMaxValue;
      } else if (readback->value[0][i] < limits->singleMinValue) {
        readback->value[0][i] = limits->singleMinValue;
      }
    }
  } else if (limits->flag & LIMIT_FILE) {
    for (i = 0; i < limits->n; i++) {
      pos = limits->index[i];
      if (readback->value[0][pos] > limits->maxValue[i]) {
        readback->value[0][pos] = limits->maxValue[i];
      } else if (readback->value[0][pos] < limits->minValue[i]) {
        readback->value[0][pos] = limits->minValue[i];
      }
    }
  }

  if (readback->despike && despikeParam->threshold > 0) {
    if (despikeParam->file) {
      if ((tempArray = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (i = 0, j = 0; i < readback->n; i++) {
        if (readback->despike[i]) {
          tempArray[j] = readback->value[0][i];
          j++;
        }
      }
      if ((spikesDone = despikeData(tempArray, j, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
        if (verbose) {
          fprintf(stderr, "%ld spikes removed.\n", spikesDone);
        }
      }
      /* if some PV are not to be despiked, then restore their values
	     after despiking */
      restored = 0;
      for (i = 0, j = 0; i < readback->n; i++) {
        if (readback->despike[i]) {
          readback->value[0][i] = tempArray[j];
          j++;
        } else {
          /* leave readback->value[0][i] unchanged */
          restored++;
        }
      }
      if ((restored) && (verbose))
        fprintf(stderr, "%ld despiked values restored.\n", restored);
      free(tempArray);
    } else if ((spikesDone = despikeData(readback->value[0], readback->n, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
      if (verbose) {
        fprintf(stderr, "%ld spikes removed.\n", spikesDone);
      }
    }
  }

  if (readback->despike && (despikeParam->threshold > 0 || limits->flag)) {
    index_min_max(&min_index, &max_index, readback->value[0], readback->n);
    if (readback->value[0][max_index] > -readback->value[0][min_index]) {
      largest_index = max_index;
    } else {
      largest_index = min_index;
    }
    readbackAdjustedStats->ave = arithmeticAverage(readback->value[0], readback->n);
    readbackAdjustedStats->RMS = standardDeviation(readback->value[0], readback->n);
    readbackAdjustedStats->MAD = meanAbsoluteDeviation(readback->value[0], readback->n);
    readbackAdjustedStats->largest = readback->value[0][largest_index];
    readbackAdjustedStats->largestName = readback->controlName[largest_index];
    snprintf(readbackAdjustedStats->msg, 256, "Adjusted readback devs  %10.3g %10.3g"
                                        " %10.3g %10.3g (%s)\n",
            readbackAdjustedStats->ave, readbackAdjustedStats->RMS, readbackAdjustedStats->MAD, readbackAdjustedStats->largest, readbackAdjustedStats->largestName);
  } else {
    readbackAdjustedStats->msg[0] = 0;
  }
  return;
}

long getControlDevices(CONTROL_NAME *control, STATS *controlStats, LOOP_PARAM *loopParam, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime) {
  /*long maxIndex, minIndex, largestIndex; */
  long i;
  if (!control->file || !control->integral)
    return 0;
  if (verbose)
    fprintf(stderr, "Reading control devices at %f seconds.\n", loopParam->elapsedTime[0]);
  if (controlWaveforms->waveforms) {
    if (ReadWaveformData(controlWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < control->n; i++) {
      control->value[0][i] = controlWaveforms->readbackValue[i];
    }
  } else {
    readPVs(control->controlName, control->value[0], control->channelInfo, &control->pva, control->n, NULL, pendIOTime);
  }
  return (0);
}

void checkRCParam(SDDS_TABLE *inputPage, char *file) {

  if (sddscontrollawGlobal->rcParam.PVparam) {
    switch (SDDS_CheckParameter(inputPage, sddscontrollawGlobal->rcParam.PVparam, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!SDDS_GetParameter(inputPage, sddscontrollawGlobal->rcParam.PVparam, sddscontrollawGlobal->rcParam.PV)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with parameter %s in file %s.\n", sddscontrollawGlobal->rcParam.PVparam, file);
      FreeEverything();
      exit(1);
    }
  }
  if (sddscontrollawGlobal->rcParam.DescParam) {
    switch (SDDS_CheckParameter(inputPage, sddscontrollawGlobal->rcParam.DescParam, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!SDDS_GetParameter(inputPage, sddscontrollawGlobal->rcParam.DescParam, sddscontrollawGlobal->rcParam.Desc)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with parameter %s in file %s.\n", sddscontrollawGlobal->rcParam.DescParam, file);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void despikeTestValues(TESTS *test, DESPIKE_PARAM *despikeParam, long verbose) {

  long i, spikesDone;
  double *despikeTestData;

  if ((despikeParam->threshold > 0) && test->despikeValues) {
    if ((despikeTestData = (double *)malloc(sizeof(double) * (test->despikeValues))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < test->despikeValues; i++) {
      despikeTestData[i] = test->value[test->despikeIndex[i]];
    }
    /* uses the same despike parameters as the readback values */
    if ((spikesDone = despikeData(despikeTestData, test->despikeValues, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
      if (verbose) {
        fprintf(stderr, "%ld spikes from test variables removed.\n", spikesDone);
      }
    }
    for (i = 0; i < test->despikeValues; i++) {
      test->value[test->despikeIndex[i]] = despikeTestData[i];
    }
    free(despikeTestData);
  }
  return;
}

long checkOutOfRange(TESTS *test, BACKOFF *backoff, STATS *readbackStats, STATS *readbackAdjustedStats, STATS *controlStats, LOOP_PARAM *loopParam, double timeOfDay, long verbose, long warning) {

  long i, outOfRange;

  outOfRange = 0; /* flag to indicate an out-of-range PV */
  test->longestSleep = 0;
  test->longestReset = 0;
  test->writeToGlitchLogger = 0;
  for (i = 0; i < test->n; i++) {
    if (test->value[i] < test->min[i] || test->value[i] > test->max[i]) {
      test->outOfRange[i] = 1;
      outOfRange = 1;
      /* determine longest time of those tests that failed */
      if (test->sleep)
        test->longestSleep = MAX(test->longestSleep, test->sleep[i]);
      if (test->reset)
        test->longestReset = MAX(test->longestReset, test->reset[i]);
      if (test->holdOff)
        test->longestHoldOff = MAX(test->longestHoldOff, test->holdOff[i]);
      if (test->glitchLog && ((test->glitchLog[i] == 'y') || (test->glitchLog[i] == 'Y')))
        test->writeToGlitchLogger = 1;
    } else {
      test->outOfRange[i] = 0;
    }
  }
  if (outOfRange) {
    if (verbose || warning) {
      backoff->counter++;
      if (backoff->counter >= backoff->level) {
        backoff->counter = 0;
        if (backoff->level < 8)
          backoff->level *= 2;
        if (verbose) {
          fprintf(stderr, "Test variables out of range at %f seconds.\n", loopParam->elapsedTime[0]);
          fprintf(stderr, "Stats                     Average       rms        mad     Largest\n"
                          "%s\n%s%s\n\n",
                  readbackStats->msg, readbackAdjustedStats->msg, controlStats->msg);
        }
        fprintf(stderr, "Test variables out of range:\n");
        for (i = 0; i < test->n; i++) {
          if (test->outOfRange[i])
            fprintf(stderr, "\t%s\t%f\n", test->controlName[i], test->value[i]);
        }
        fprintf(stderr, "Waiting for %f seconds.\n", MAX(test->longestSleep, loopParam->interval));
        fflush(stderr);
      }
    }
  }
  return (outOfRange);
}

void apply_filter(CORRECTION *correction, long verbose) {
  long i, k;
  CONTROL_NAME *control;
  MATRIX *A, *B;
  long aorder, border, sign;
  double *ptr;

  control = correction->control;
  /* filtering follows the equation 
     a0 y[k] + a1 y[k-1] + a2 y[k-2] = b0 x[k] + b1 x[k-1] + b2 x[k-2]
     where x is the raw data, i.e. controlComp->error[i]
     and y is the data to be output, i.e. controlComp->delta[0][i]
   */
  /* 0th column of history is x(n)        
     1th column of history is x(n-1), etc */
  /* Rotate history pointers. Throw out last but
     use the memory allocated for this one for the new readings */
  border = control->history->n - 1;
  ptr = control->history->a[border];
  for (k = border; k > 0; k--)
    control->history->a[k] = control->history->a[k - 1];
  control->history->a[0] = ptr;
  /* filter the control->error[i] quantity rather than 
     control->value[0][i] */
  for (i = 0; i < control->n; i++) {
    control->history->a[0][i] = control->delta[0][i];
  }

  /* 0th column of history is y(n-1)        */
  /* 1th column of history is y(n-2), etc   */
  aorder = control->historyFiltered->n - 1;
  ptr = control->historyFiltered->a[aorder];
  for (k = aorder; k > 0; k--)
    control->historyFiltered->a[k] = control->historyFiltered->a[k - 1];
  control->historyFiltered->a[0] = ptr;
  /* elements controlComp->history->a[0][k] are to be determined next. */

  A = correction->aCoef;
  B = correction->bCoef;
  aorder = A->n - 1;
  border = B->n - 1;
  /* loop over correctors, and do a direct product of matrices */

  for (i = 0; i < control->n; i++) {
    control->historyFiltered->a[0][i] = 0;
    for (k = 1; k <= aorder; k++) {
#ifdef FLOAT_MATH
      control->historyFiltered->a[0][i] -= ((float)A->a[k][i]) * ((float)control->historyFiltered->a[k][i]);
#else
      control->historyFiltered->a[0][i] -= A->a[k][i] * control->historyFiltered->a[k][i];
#endif
    }
    for (k = 0; k <= border; k++) {
#ifdef FLOAT_MATH
      control->historyFiltered->a[0][i] += ((float)B->a[k][i]) * ((float)control->history->a[k][i]);
#else
      control->historyFiltered->a[0][i] += B->a[k][i] * control->history->a[k][i];
#endif
    }
    control->historyFiltered->a[0][i] /= A->a[0][i];
    /*  if( verbose ) {
         fprintf(stderr,"after filtering %s   ", control->controlName[i]);
         for (k=0; k<aorder+1; k++) 
         fprintf(stderr,"a=%d filtered=%11.6f ", k,  control->historyFiltered->a[k][i]);
         fprintf(stderr, "\n");
         for (k=0; k<border+1; k++)
         fprintf(stderr, "b=%d history=%11.6f ", k,  control->history->a[k][i]);
         fprintf(stderr, "\n");
         } */

    sign = 1; /* added sign variable for debugging */
    /* reevaluated  controlComp->delta[0] after filtering */
    control->delta[0][i] = sign * control->historyFiltered->a[0][i];
    control->value[0][i] = control->delta[0][i] + control->old[i];
  }

  /*  fprintf(stderr,"%s initial[0]: %8.3f; old[0]: %8.3f; new[0]: %8.3f\n",
     control->controlName[0],
     control->initial[0], control->old[0], control->value[0][0]); */
}

void controlLaw(long skipIteration, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *compensation, long verbose, double pendIOTime) {
  long i, j, k;
  CONTROL_NAME *control, *controlComp, *readback, *readbackComp;
  MATRIX *K, *A, *B;
  long aorder, border, sign;
  double *ptr;
#ifdef FLOAT_MATH
  float accumulator;
#else
  double accumulator;
#endif

  readback = correction->readback;
  control = correction->control;
  readbackComp = compensation->readback;
  controlComp = compensation->control;

  K = correction->K;
  if (skipIteration) {
    /* make no change in correction */
    for (i = 0; i < control->n; i++) {
      control->old[i] = control->value[0][i];
      control->delta[0][i] = 0;
    }
  } else {
    for (i = 0; i < control->n; i++) {
      control->old[i] = control->value[0][i];
      if (!control->integral)
        /* for proportional mode the correction control values
	       is made proportional to the error readback. The values
	       in control->value[0][i] are set to zero so that the 
	       increment expression (-=) used later will not actually
	       make a relative change to the actuator values.
	     */
        control->old[i] = control->value[0][i] = 0;
      if (loopParam->gainPV) {
        readPVs(&loopParam->gainPV, &loopParam->gain, &loopParam->gainPVInfo, &loopParam->pvaGain, 1, NULL, pendIOTime);
      }

      if (loopParam->holdPresentValues) {
        for (j = accumulator = 0; j < readback->n; j++) {
          /* for hold present values mode, correct relative to 
		     initial values stored in readback->initial[j]
		   */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * (((float)readback->value[0][j]) - ((float)readback->initial[j]));
#else
          accumulator += K->a[i][j] * (readback->value[0][j] - readback->initial[j]);
#endif
        }
      } else {
        for (j = accumulator = 0; j < readback->n; j++) {
          /* standard feedback using readback as error signal
		     to apply a change to control values
		   */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * ((float)readback->value[0][j]);
#else
          accumulator += K->a[i][j] * readback->value[0][j];
#endif
        }
      }
      control->value[0][i] -= accumulator * loopParam->gain;
      control->delta[0][i] = control->value[0][i] - control->old[i];
    }
    if (correction->coefFile)
      apply_filter(correction, verbose);
    if (verbose)
      fprintf(stderr, "%s initial[0]: %8.3f; old[0]: %8.3f; new[0]: %8.3f\n", control->controlName[0], control->initial[0], control->old[0], control->value[0][0]);
  }

  /* determine correction with other matrix */
  /* for now the sets of actuator control quantities for the two matrices should not and
     cannot intersect. Otherwise one of the values will be overwritten. */
  if (compensation->file) {
    K = compensation->K;
    if (skipIteration) {
      /* make no change in correction */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->old[i] = controlComp->value[0][i];
        controlComp->delta[0][i] = 0;
      }
    } else {
      /* the signal for compensation (controlComp->error) 
	     is the same as the output
	     from the previous correction (control->delta[0]) */
      for (j = 0; j < readbackComp->n; j++) {
        readbackComp->error[j] = control->delta[0][j];
      }

      for (i = 0; i < controlComp->n; i++) {
        controlComp->old[i] = controlComp->value[0][i];
        controlComp->delta[0][i] = 0.0;
        controlComp->error[i] = 0.0;
        if (!controlComp->integral)
          /* for proportional mode the correction control values
		   is made proportional to the error readback. The values
		   in control->value[0][i] are set to zero so that the 
		   increment expression (-=) used later will not actually
		   make a relative change to the actuator values.
		 */
          controlComp->value[0][i] = controlComp->old[i] = 0;
        for (j = accumulator = 0; j < readbackComp->n; j++) {
          /* cascade calculation using correction effort above
		     to apply a change to control values.
		     Also the flag holdPresentValues has no meaning here.
		   */
          /* + sign is the convention for feedforward compensation */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * ((float)readbackComp->error[j]);
#else
          accumulator += K->a[i][j] * readbackComp->error[j];
#endif
          /* controlComp->delta[0][i] may be modified by filtering below */
        }
        controlComp->delta[0][i] += accumulator * loopParam->compensationGain;
      }
    }
    if (compensation->coefFile) {
      /* filtering follows the equation 
	     a0 y[k] + a1 y[k-1] + a2 y[k-2] = b0 x[k] + b1 x[k-1] + b2 x[k-2]
	     where x is the raw data, i.e. controlComp->error[i]
	     and y is the data to be output, i.e. controlComp->delta[0][i]
	   */
      /* 0th column of history is x(n)        
	     1th column of history is x(n-1), etc */
      /* Rotate history pointers. Throw out last but
	     use the memory allocated for this one for the new readings */
      border = controlComp->history->n - 1;
      ptr = controlComp->history->a[border];
      for (k = border; k > 0; k--)
        controlComp->history->a[k] = controlComp->history->a[k - 1];
      controlComp->history->a[0] = ptr;
      /* filter the controlComp->error[i] quantity rather than 
	     controlComp->value[0][i] */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->history->a[0][i] = controlComp->delta[0][i];
      }

      /* 0th column of history is y(n-1)        */
      /* 1th column of history is y(n-2), etc   */
      aorder = controlComp->historyFiltered->n - 1;
      ptr = controlComp->historyFiltered->a[aorder];
      for (k = aorder; k > 0; k--)
        controlComp->historyFiltered->a[k] = controlComp->historyFiltered->a[k - 1];
      controlComp->historyFiltered->a[0] = ptr;
      /* elements controlComp->history->a[0][k] are to be determined next. */

      A = compensation->aCoef;
      B = compensation->bCoef;
      aorder = A->n - 1;
      border = B->n - 1;
      /* loop over correctors, and do a direct product of matrices */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->historyFiltered->a[0][i] = 0;
        for (k = 1; k <= aorder; k++) {
#ifdef FLOAT_MATH
          controlComp->historyFiltered->a[0][i] -= ((float)A->a[k][i]) * ((float)controlComp->historyFiltered->a[k][i]);
#else
          controlComp->historyFiltered->a[0][i] -= A->a[k][i] * controlComp->historyFiltered->a[k][i];
#endif
        }
        for (k = 0; k <= border; k++) {
#ifdef FLOAT_MATH
          controlComp->historyFiltered->a[0][i] += ((float)B->a[k][i]) * ((float)controlComp->history->a[k][i]);
#else
          controlComp->historyFiltered->a[0][i] += B->a[k][i] * controlComp->history->a[k][i];
#endif
        }
        controlComp->historyFiltered->a[0][i] /= A->a[0][i];
        if (verbose)
          fprintf(stderr, "%3ld   %11.6f %11.6f %11.6f    %11.6f %11.6f %11.6f\n", i,
                  controlComp->historyFiltered->a[0][i], controlComp->historyFiltered->a[1][i], controlComp->historyFiltered->a[2][i], controlComp->history->a[0][i], controlComp->history->a[1][i], controlComp->history->a[2][i]);
        sign = 1; /* added sign variable for debugging */
        /* reevaluated  controlComp->delta[0] after filtering */
        controlComp->delta[0][i] = sign * controlComp->historyFiltered->a[0][i];
        /*here should use old instead of initial; H. Shang 5-9-2017 */
        controlComp->value[0][i] = controlComp->delta[0][i] + controlComp->initial[i];
      }
      fprintf(stderr, "%s initial[0]: %8.3f old[0]: %8.3f new[0]: %8.3f\n", controlComp->controlName[0], controlComp->initial[0], controlComp->old[0], controlComp->value[0][0]);
    } else {
      /* no filter files given */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->value[0][i] += controlComp->delta[0][i];
      }
    }
  }
  return;
}

double applyDeltaLimit(LIMITS *delta, LOOP_PARAM *loopParam, CONTROL_NAME *control, long verbose, long warning) {

  long i;
  double factor, trialFactor, change;

  factor = 1;
  if (delta->flag & LIMIT_VALUE) {
    if (control->integral) {
      for (i = 0; i < control->n; i++) {
        if ((change = fabs(control->value[0][i] - control->old[i])) > delta->singleValue) {
          trialFactor = delta->singleValue / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    } else {
      for (i = 0; i < control->n; i++) {
        if ((change = fabs(control->value[0][i])) > delta->singleValue) {
          trialFactor = delta->singleValue / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    }
  } else if (delta->flag & LIMIT_FILE) {
    if (control->integral) {
      for (i = 0; i < delta->n; i++) {
        if ((change = fabs(control->value[0][delta->index[i]] - control->old[delta->index[i]])) > delta->value[i]) {
          trialFactor = delta->value[i] / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    } else {
      for (i = 0; i < delta->n; i++) {
        if ((change = fabs(control->value[0][delta->index[i]])) > delta->value[i]) {
          trialFactor = delta->value[i] / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    }
  }

  if (factor < 1) {
    if (warning || verbose)
      fprintf(stderr, "Limiting changes by factor %.2g\n", factor);
    if (control->integral) {
      for (i = 0; i < control->n; i++) {
        control->delta[0][i] = (control->value[0][i] - control->old[i]) * factor;
        control->value[0][i] = control->old[i] + control->delta[0][i];
      }
    } else {
      for (i = 0; i < control->n; i++) {
        control->value[0][i] *= factor;
        control->delta[0][i] *= factor;
      }
    }
  }
  return factor;
}

void calcControlDeltaStats(CONTROL_NAME *control, STATS *controlStats, STATS *controlDeltaStats) {
  int64_t minIndex, maxIndex, largestIndex;

  /* calculate control stats */
  controlStats->RMS = standardDeviation(control->value[0], control->n);
  controlStats->ave = arithmeticAverage(control->value[0], control->n);
  controlStats->MAD = meanAbsoluteDeviation(control->value[0], control->n);
  index_min_max(&minIndex, &maxIndex, control->value[0], control->n);
  if (control->value[0][maxIndex] > -control->value[0][minIndex]) {
    largestIndex = maxIndex;
  } else {
    largestIndex = minIndex;
  }
  controlStats->largest = control->value[0][largestIndex];
  controlStats->largestName = control->controlName[largestIndex];
  snprintf(controlStats->msg, sizeof(controlStats->msg), "Control devices         %10.3g %10.3g"
                             " %10.3g %10.3g (%s)",
          controlStats->ave, controlStats->RMS, controlStats->MAD, controlStats->largest, controlStats->largestName);

  /*calculate control delta stats */
  controlDeltaStats->RMS = standardDeviation(control->delta[0], control->n);
  controlDeltaStats->ave = arithmeticAverage(control->delta[0], control->n);
  controlDeltaStats->MAD = meanAbsoluteDeviation(control->delta[0], control->n);
  index_min_max(&minIndex, &maxIndex, control->delta[0], control->n);
  if (control->delta[0][maxIndex] > -control->delta[0][minIndex]) {
    largestIndex = maxIndex;
  } else {
    largestIndex = minIndex;
  }
  controlDeltaStats->largest = control->delta[0][largestIndex];
  controlDeltaStats->largestName = control->controlName[largestIndex];
  snprintf(controlDeltaStats->msg, sizeof(controlDeltaStats->msg), "Control device deltas   %10.3g %10.3g"
                                  " %10.3g %10.3g (%s)",
          controlDeltaStats->ave, controlDeltaStats->RMS, controlDeltaStats->MAD, controlDeltaStats->largest, controlDeltaStats->largestName);
  return;
}

void writeToOutputFile(char *outputFile, SDDS_TABLE *outputPage, long *outputRow, LOOP_PARAM *loopParam, CORRECTION *correction, TESTS *test, WAVEFORM_TESTS *waveform_tests) {
  float readbackValuesSPrec, controlValuesSPrec, testValueSPrec;
  long i, j;
  CONTROL_NAME *control, *readback;

  readback = correction->readback;
  control = correction->control;
  if (outputFile) {
    /* first three columns */
    if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, "Step", loopParam->step[0], "Time", loopParam->epochTime[0], "ElapsedTime", loopParam->elapsedTime[0], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++) {
      readbackValuesSPrec = (float)readback->value[0][i];
      if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, readback->symbolicName[i], readbackValuesSPrec, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      controlValuesSPrec = (float)control->value[0][i];
      if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, control->symbolicName[i], controlValuesSPrec, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (test && test->file) {
      for (i = 0; i < test->n; i++) {
        if (test->writeToFile[i]) {
          testValueSPrec = (float)test->value[i];
          if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, test->controlName[i], testValueSPrec, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            FreeEverything();
            exit(1);
          }
        }
      }
    }
    if (waveform_tests->waveformTests) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
          if (waveform_tests->writeToFile[i][j]) {
            if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, waveform_tests->DeviceName[i][j], waveform_tests->waveformData[i][j], NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              FreeEverything();
              exit(1);
            }
          }
        }
      }
    }

    if (!(((loopParam->step[0]) + 1) % loopParam->updateInterval)) {
      if (!SDDS_UpdatePage(outputPage, FLUSH_TABLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else if (loopParam->step[0] == (loopParam->steps) - 1) {
      if (!SDDS_UpdatePage(outputPage, FLUSH_TABLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    (*outputRow)++;
  }
  return;
}

void writeToStatsFile(char *statsFile, SDDS_TABLE *statsPage, long *statsRow, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, STATS *controlStats, STATS *controlDeltaStats) {

  if (statsFile) {
    if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow, "Step", loopParam->step[0], "Time", loopParam->epochTime[0], "ElapsedTime", loopParam->elapsedTime[0], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!loopParam->briefStatistics) {
      if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow,
                             "readbackRMS", readbackStats->RMS,
                             "readbackAve", readbackStats->ave,
                             "readbackMAD", readbackStats->MAD,
                             "readbackDeltaRMS", readbackDeltaStats->RMS,
                             "readbackDeltaAve", readbackDeltaStats->ave,
                             "readbackDeltaMAD", readbackDeltaStats->MAD,
                             "controlRMS", controlStats->RMS,
                             "controlAve", controlStats->ave,
                             "controlMAD", controlStats->MAD,
                             "controlDeltaRMS", controlDeltaStats->RMS,
                             "controlDeltaAve", controlDeltaStats->ave,
                             "controlDeltaMAD", controlDeltaStats->MAD, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow,
                           "readbackLargest", readbackStats->largest,
                           "readbackLargestName", readbackStats->largestName,
                           "readbackDeltaLargest", readbackDeltaStats->largest,
                           "readbackDeltaLargestName", readbackDeltaStats->largestName,
                           "controlLargest", controlStats->largest,
                           "controlLargestName", controlStats->largestName,
                           "controlDeltaLargest", controlDeltaStats->largest,
                           "controlDeltaLargestName", controlDeltaStats->largestName, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!((loopParam->step[0] + 1) % loopParam->updateInterval)) {
      if (!SDDS_UpdatePage(statsPage, FLUSH_TABLE) || !SDDS_FreeStringData(statsPage)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else if (loopParam->step[0] == loopParam->steps - 1) {
      if (!SDDS_UpdatePage(statsPage, FLUSH_TABLE) || !SDDS_FreeStringData(statsPage)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    (*statsRow)++;
  }
  return;
}

#include <time.h>

void writeToGlitchFile(GLITCH_PARAM *glitchParam, SDDS_TABLE *glitchPage, long *glitchRow, LOOP_PARAM *loopParam, STATS *readbackAdjustedStats, STATS *controlDeltaStats, CORRECTION *correction, CORRECTION *overlapCompensation, TESTS *test) {
  long i, n, index;
  CONTROL_NAME *control, *readback;
  CONTROL_NAME *controlComp = NULL, *readbackComp = NULL;
  char *buffer = NULL, *buffer2 = NULL;
  char *timeStamp = NULL;

  readback = correction->readback;
  control = correction->control;
  if (overlapCompensation->file) {
    readbackComp = overlapCompensation->readback;
    controlComp = overlapCompensation->control;
  }
  if (!glitchParam->file)
    return;
  if (glitchParam->avail_rows < glitchParam->rows)
    glitchParam->avail_rows++;
  glitchParam->glitched = 1;
  if ((controlDeltaStats->RMS <= glitchParam->controlRMSthreshold) && (readbackAdjustedStats->RMS <= glitchParam->readbackRMSthreshold) && (!test->writeToGlitchLogger)) {
    glitchParam->glitched = 0;
  }
  /* determine if glitch data should be written to a file */
  if (glitchParam->glitched == 0) {
    if (glitchParam->rows > 1) {
      glitchParam->row_pointer++;
      if (glitchParam->row_pointer == glitchParam->rows)
        glitchParam->row_pointer = 1;
      loopParam->step[glitchParam->row_pointer] = loopParam->step[0];
      loopParam->epochTime[glitchParam->row_pointer] = loopParam->epochTime[0];
      loopParam->elapsedTime[glitchParam->row_pointer] = loopParam->elapsedTime[0];
      for (n = 0; n < readback->n; n++)
        readback->value[glitchParam->row_pointer][n] = readback->value[0][n];
      for (n = 0; n < control->n; n++) {
        control->value[glitchParam->row_pointer][n] = control->value[0][n];
        control->delta[glitchParam->row_pointer][n] = control->delta[0][n];
      }
      if (overlapCompensation->file) {
        for (n = 0; n < readbackComp->n; n++)
          readbackComp->value[glitchParam->row_pointer][n] = readbackComp->value[0][n];
        for (n = 0; n < controlComp->n; n++) {
          controlComp->value[glitchParam->row_pointer][n] = controlComp->value[0][n];
          controlComp->delta[glitchParam->row_pointer][n] = controlComp->delta[0][n];
        }
      }
    }
    /*
         for (i=glitchParam->rows-1 ; i > 0 ; i--) {
         loopParam->step[i] = loopParam->step[i-1];
         loopParam->epochTime[i] = loopParam->epochTime[i-1];
         loopParam->elapsedTime[i] = loopParam->elapsedTime[i-1];
         for (n = 0; n < readback->n; n++)
         readback->value[i][n] = readback->value[i-1][n];
         for (n = 0; n < control->n; n++) {
         control->value[i][n] = control->value[i-1][n];
         control->delta[i][n] = control->delta[i-1][n];
         }
         if (overlapCompensation->file) {
         for (n = 0; n < readbackComp->n; n++)
         readbackComp->value[i][n] = readbackComp->value[i-1][n];
         for (n = 0; n < controlComp->n; n++) {
         controlComp->value[i][n] = controlComp->value[i-1][n];
         controlComp->delta[i][n] = controlComp->delta[i-1][n];
         }
         }
         }
       */
    return;
  }

  if (!SDDS_StartTable(glitchPage, loopParam->updateInterval * glitchParam->rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &timeStamp);
  if (!SDDS_SetParameters(glitchPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, "ReadbackRmsThreshold", glitchParam->readbackRMSthreshold, "ControlRmsThreshold", glitchParam->controlRMSthreshold, "GlitchRows", glitchParam->rows, NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  /* first three columns, as in output file  */
  for (n = glitchParam->avail_rows - 1; n >= 0; n--) {
    if ((glitchParam->rows < 2) || (n == 0))
      index = 0;
    else if (glitchParam->row_pointer < n)
      index = glitchParam->avail_rows - (n - glitchParam->row_pointer);
    else
      index = glitchParam->row_pointer - n + 1;
    if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, "Step", loopParam->step[index], "Time", loopParam->epochTime[index], "ElapsedTime", loopParam->elapsedTime[index], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++) {
      if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, readback->symbolicName[i], (float)readback->value[index][i], NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if ((buffer = (char*)malloc(sizeof(char) * (strlen(control->symbolicName[i]) + 6))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      snprintf(buffer, strlen(control->symbolicName[i]) + 6, "%sDelta", control->symbolicName[i]);
      if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, control->symbolicName[i], (float)control->value[index][i], buffer, (float)control->delta[index][i], NULL)) {
        free(buffer);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      free(buffer);
    }
    if (overlapCompensation->file) {
      for (i = 0; i < readbackComp->n; i++) {
        if ((buffer = (char*)malloc(sizeof(char) * (strlen(readbackComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer, strlen(readbackComp->symbolicName[i]) + 6, "%sAuxil", readbackComp->symbolicName[i]);
        if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, buffer, (float)readbackComp->value[index][i], NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
      for (i = 0; i < controlComp->n; i++) {
        if ((buffer = (char*)malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer, strlen(controlComp->symbolicName[i]) + 6, "%sAuxil", controlComp->symbolicName[i]);
        if ((buffer2 = (char*)malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 11))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        snprintf(buffer2, strlen(controlComp->symbolicName[i]) + 11, "%sAuxilDelta", controlComp->symbolicName[i]);
        if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, buffer, (float)controlComp->value[index][i], buffer2, (float)controlComp->delta[index][i], NULL)) {
          free(buffer);
          free(buffer2);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
        free(buffer2);
      }
    }
    (*glitchRow)++;
  }
  if (!SDDS_WritePage(glitchPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  glitchParam->row_pointer = 0;
  glitchParam->avail_rows = 0;
  *glitchRow = 0;
  return;
}

void serverExit(int sig) {
  char s[1024];
  snprintf(s, sizeof(s), "rm %s", sddscontrollawGlobal->pidFile);
  system(s);
  fprintf(stderr, "Program terminated by signal.\n");
  if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
    setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], (char*)"Terminated by signal", sddscontrollawGlobal->loopParam.launcherPVInfo[0], &(sddscontrollawGlobal->loopParam.pvaLauncher[0]), 1);
    setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 2, sddscontrollawGlobal->loopParam.launcherPVInfo[1], &(sddscontrollawGlobal->loopParam.pvaLauncher[1]), 1);
  }
  if (sddscontrollawGlobal->rcParam.PV) {
    strcpy(sddscontrollawGlobal->rcParam.message, "Terminated by signal");
    sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      FreeEverything();
      exit(1);
    }
  }
  FreeEverything();
  exit(0);
}

void commandServer(int sig) {
  sddscontrollawGlobal->reparseFromFile = 1;
#  ifndef _WIN32
  signal(SIGUSR1, commandServer);
#  endif
  return;
}

void exitIfServerRunning() {
  FILE *fp;
  int pid;
  char s[1024];

  if (!(fp = fopen(sddscontrollawGlobal->pidFile, "r"))) {
    fprintf(stderr, "No server file found--starting new server\n");
    return;
  }
  pid = -1;
  if (fscanf(fp, "%d", &pid) != 1 ||
#  if defined(_WIN32)
      0
#  else
      getpgid(pid) == -1
#  endif
  ) {
#  ifdef DEBUG
    fprintf(stderr, "Old server pid was %ld\n", pid);
    fprintf(stderr, "Removing old server PID file\n");
#  endif
    snprintf(s, sizeof(s), "rm %s", sddscontrollawGlobal->pidFile);
    if (system(s)) {
      fprintf(stderr, "Problem with %s.\n", s);
    }
#  ifdef DEBUG
    fprintf(stderr, "Starting new server\n");
#  endif
    return;
  }
#  ifdef DEBUG
  fprintf(stderr, "Server %s already running\n", sddscontrollawGlobal->pidFile);
#  endif
  FreeEverything();
  exit(0);
}

void setupServer() {
  FILE *fp;
  long pid;
  char s[1024];

  pid = getpid();
  if (!sddscontrollawGlobal->pidFile) {
    FreeEverything();
    SDDS_Bomb((char*)"Pointer to pidFile is NULL detected in setupServer.");
  }
  fprintf(stderr, "pid file is %s.\n", sddscontrollawGlobal->pidFile);
  if (!(fp = fopen(sddscontrollawGlobal->pidFile, "w"))) {
    FreeEverything();
    SDDS_Bomb((char*)"Unable to open PID file");
  }
  fprintf(fp, "%ld\n", pid);
  fclose(fp);
  snprintf(s, sizeof(s), "chmod g+w %s", sddscontrollawGlobal->pidFile);
  system(s);

#    ifndef _WIN32
  signal(SIGUSR1, commandServer);
  signal(SIGUSR2, serverExit);
#    endif
  return;
}

long parseArguments(char ***argv,
                    int *argc,
                    CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    char **statsFile,
                    long *serverMode,
                    char **commandFile,
                    char **outputFile,
                    char **controlLogFile,
                    long *testCASecurity,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    char **compensationOutputFile,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests,
                    long firstTime,
                    long *verbose,
                    long *warning,
                    double *pendIOTime,
                    char *commandline_option[COMMANDLINE_OPTIONS],
                    char *waveformOption[WAVEFORMOPTIONS]) {
  FILE *fp = NULL;
  SCANNED_ARG *s_arg = NULL;
  long i, i_arg, infinite;
  long unsigned dummyFlags;
  double timeout;
  long retries;
  char *mode = NULL;
  char *mode_option[2] = {
    (char*)"integral",
    (char*)"proportional",
  };
  char *statistics_mode_option[2] = {
    (char*)"full",
    (char*)"brief",
  };

  infinite = 0;

  if (!firstTime) {
    if (!(fp = fopen(*commandFile, "r"))) {
      fprintf(stderr, "Command file %s not found.\n", *commandFile);
      return (1);
    }
    ReadMoreArguments(argv, argc, NULL, 0, fp);
  }
  *argc = scanargs(&s_arg, *argc, *argv);

  if (firstTime) {
    *verbose = 0;
    *warning = 0;
    *pendIOTime = 30.0;
    *outputFile = NULL;
    *statsFile = NULL;
    sddscontrollawGlobal->pidFile = NULL;
    *commandFile = NULL;
    *serverMode = 0;
    *controlLogFile = NULL;
    *compensationOutputFile = NULL;
    *testCASecurity = 0;
    initializeData(correction, loopParam, aveParam, delta, readbackLimits, action, despikeParam, glitchParam, test, generations, overlapCompensation, readbackWaveforms, controlWaveforms, offsetWaveforms, ffSetpointWaveforms, waveform_tests);
  }
  for (i_arg = 1; i_arg < *argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char*)"_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_DAILY_FILES:
        /* daily files are a special case of generations option */
        if (generations->doGenerations && !generations->daily) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"Options -generation and -daily are incompatible.");
        }
        generations->doGenerations = 1;
        generations->daily = 1;
        generations->digits = 4;
        generations->delimiter = (char*)".";
        break;
      case CLO_GENERATIONS:
        if (generations->doGenerations && generations->daily) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"Options -generation and -daily are incompatible.");
        }
        generations->doGenerations = 1;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &(generations->digits), 1, 0,
                          "delimiter", SDDS_STRING, &(generations->delimiter), 1, 0,
                          "rowlimit", SDDS_LONG, &(generations->rowLimit), 1, 0,
                          "timelimit", SDDS_DOUBLE, &(generations->timeLimit), 1, 0, NULL) ||
            generations->digits < 1) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"invalid -generations syntax/values");
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_GAIN:
        if (s_arg[i_arg].n_items != 2) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"invalid -gain syntax (too many qualifiers)");
        }
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -gain syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -gain syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->gainPV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for gain PV.\n");
            if ((loopParam->gainPVInfo.count = (long*)malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawPVAConnection(&loopParam->gainPV, &loopParam->gainPVInfo, 1, *pendIOTime, &(loopParam->pvaGain));
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -gain syntax (unknown qualifier)");
          }
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->gain) != 1) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -gain syntax/value");
          }
        }
        break;
      case CLO_TIME_INTERVAL:
        if (s_arg[i_arg].n_items != 2) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"invalid -interval syntax (too many qualifiers)");
        }
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -interval syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -interval syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->intervalPV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for interval PV.\n");
            if ((loopParam->intervalPVInfo.count = (long*)malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawPVAConnection(&loopParam->intervalPV, &loopParam->intervalPVInfo, 1, *pendIOTime, &(loopParam->pvaInterval));
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -interval syntax (unknown qualifier)");
          }
          readPVs(&loopParam->intervalPV, &loopParam->interval, &loopParam->intervalPVInfo, &loopParam->pvaInterval, 1, NULL, *pendIOTime);
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->interval) != 1) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -interval syntax/value");
          }
        }
        break;
      case CLO_PENDIOTIME:
        if ((s_arg[i_arg].n_items < 2) || !(get_double(pendIOTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no number given for option -pendIOTime.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_STEPS:
        if ((s_arg[i_arg].n_items < 2) || !(get_long(&loopParam->steps, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no number given for option -steps.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_INFINITELOOP:
        infinite = 1;
        break;
      case CLO_UPDATE_INTERVAL:
        if ((s_arg[i_arg].n_items < 2) || !(get_long(&loopParam->updateInterval, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no integer given for option -updateInterval.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_INTEGRAL:
        loopParam->integral = 1;
        break;
      case CLO_PROPORTIONAL:
        loopParam->integral = 0;
        break;
      case CLO_UNGROUPEZCACALLS:
        /* Do nothing because this option is obsolete */
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3) {
          FreeEverything();
          SDDS_Bomb((char*)"wrong number of items for -ezcaTiming");
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || retries < 0) {
          FreeEverything();
          bomb((char*)"invalid -ezca values", NULL);
        }
        *pendIOTime = timeout * (retries + 1);
        break;
      case CLO_DRY_RUN:
        loopParam->dryRun = 1;
        break;
      case CLO_HOLD_PRESENT_VALUES:
        loopParam->holdPresentValues = 1;
        break;
      case CLO_OFFSETS:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -offsets syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->offsetFile, s_arg[i_arg].list[1]);
        break;
      case CLO_SEARCHPATH:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -searchPath syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->searchPath, s_arg[i_arg].list[1]);
        break;
      case CLO_PVOFFSETS:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -PVOffsets syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->offsetPVFile, s_arg[i_arg].list[1]);
        break;
      case CLO_WAVEFORMS:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "bad -waveforms syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (!strlen(s_arg[i_arg].list[1])) {
          fprintf(stderr, "no filename is given for -waveforms syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        switch (match_string(s_arg[i_arg].list[2], waveformOption, WAVEFORMOPTIONS, UNIQUE_MATCH)) {
        case CLO_READBACKWAVEFORM:
          if ((readbackWaveforms->waveformFile = (char**)SDDS_Realloc(readbackWaveforms->waveformFile, sizeof(char *) * (readbackWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&readbackWaveforms->waveformFile[readbackWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          readbackWaveforms->waveformFiles++;
          break;
        case CLO_OFFSETWAVEFORM:
          if ((offsetWaveforms->waveformFile = (char**)SDDS_Realloc(offsetWaveforms->waveformFile, sizeof(char *) * (offsetWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&offsetWaveforms->waveformFile[offsetWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          offsetWaveforms->waveformFiles++;
          break;
        case CLO_ACTUATORWAVEFORM:
          if ((controlWaveforms->waveformFile = (char**)SDDS_Realloc(controlWaveforms->waveformFile, sizeof(char *) * (controlWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&controlWaveforms->waveformFile[controlWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          controlWaveforms->waveformFiles++;
          break;
        case CLO_FFSETPOINTWAVEFORM:
          if ((ffSetpointWaveforms->waveformFile = (char**)SDDS_Realloc(ffSetpointWaveforms->waveformFile, sizeof(char *) * (ffSetpointWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&ffSetpointWaveforms->waveformFile[ffSetpointWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          ffSetpointWaveforms->waveformFiles++;
          break;
        case CLO_TESTWAVEFORM:
          if ((waveform_tests->testFile = (char**)SDDS_Realloc(waveform_tests->testFile, sizeof(char *) * (waveform_tests->testFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&waveform_tests->testFile[waveform_tests->testFiles], s_arg[i_arg].list[1]);
          waveform_tests->testFiles++;
          break;
        default:
          fprintf(stderr, "unknown \"%s\" syntax for the <type> of -waveforms.\n", s_arg[i_arg].list[2]);
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          exit(1);
          break;
        }
        break;
      case CLO_RUNCONTROLPV:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddscontrollawGlobal->rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &sddscontrollawGlobal->rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &sddscontrollawGlobal->rcParam.pingTimeout, 1, 0, NULL) ||
            (!sddscontrollawGlobal->rcParam.PVparam && !sddscontrollawGlobal->rcParam.PV) ||
            (sddscontrollawGlobal->rcParam.PVparam && sddscontrollawGlobal->rcParam.PV)) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb((char*)"bad -runControlPV syntax", (char*)"-runControlPV={string=<string>|parameter=<string>}\n");
        }
        s_arg[i_arg].n_items += 1;
        sddscontrollawGlobal->rcParam.pingTimeout *= 1000;
        if (sddscontrollawGlobal->rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          exit(1);
        }
        break;
      case CLO_RUNCONTROLDESC:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddscontrollawGlobal->rcParam.DescParam, 1, 0,
                          "string", SDDS_STRING, &sddscontrollawGlobal->rcParam.Desc, 1, 0, NULL) ||
            (!sddscontrollawGlobal->rcParam.DescParam && !sddscontrollawGlobal->rcParam.Desc) ||
            (sddscontrollawGlobal->rcParam.DescParam && sddscontrollawGlobal->rcParam.Desc)) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb((char*)"bad -runControlDesc syntax", (char*)"-runControlDescription={string=<string>|parameter=<string>}\n");
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_STATISTICS:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(statsFile, s_arg[i_arg].list[1])) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb((char*)"No file given in option -statistics");
        }
        s_arg[i_arg].n_items -= 2;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "mode", SDDS_STRING, &mode, 1, 0, NULL))) {
          fprintf(stderr, "invalid -statistics syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (mode) {
          switch (match_string(mode, statistics_mode_option, 2, 0)) {
          case 0:
            loopParam->briefStatistics = 0;
            free(mode);
            break;
          case 1:
            loopParam->briefStatistics = 1;
            free(mode);
            break;
          default:
            fprintf(stderr, "invalid mode given for -statistics syntax/values.\n");
            s_arg[i_arg].n_items += 2;
            free_scanargs(&s_arg, *argc);
            free(mode);
            return (1);
          }
        } else {
          loopParam->briefStatistics = 0;
        }
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_SERVERMODE:
        if (firstTime) {
          *serverMode = 1;
          if ((s_arg[i_arg].n_items -= 1) < 0 || !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "pid", SDDS_STRING, &sddscontrollawGlobal->pidFile, 1, 0, "command", SDDS_STRING, commandFile, 1, 0, NULL)) {
            s_arg[i_arg].n_items += 1;
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"bad -servermode syntax/value");
          }
          s_arg[i_arg].n_items += 1;
        } else {
          fprintf(stderr, "%s option ignored when a subsequent command is issued in server mode.\nThe option may have already been specified in the original command, and would stay in force.\n", s_arg[i_arg].list[0]);
        }
        break;
      case CLO_VERBOSE:
        *verbose = 1;
        fprintf(stderr, "Link date: %s %s\n", __DATE__, __TIME__);
        break;
      case CLO_WARNING:
        *warning = 1;
        break;
      case CLO_TESTCASECURITY:
        *testCASecurity = 1;
        break;
      case CLO_TEST_VALUES:
        if ((s_arg[i_arg].n_items < 2) || !strlen(s_arg[i_arg].list[1]) || !SDDS_CopyString(&test->file, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No file given in option -testValues.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_ACTUATORCOL:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(&correction->actuator, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No string given in option -actuatorColumn.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_CONTROLLOG:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(controlLogFile, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No string given in option -controlLogFile.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_CONTROL_QUANTITY_DEFINITION:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(&correction->definitionFile, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No file given in option -controlQuantityDefinition.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_AVERAGE:
        if ((s_arg[i_arg].n_items -= 2) < 0 || !scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "interval", SDDS_DOUBLE, &aveParam->interval, 1, 0, NULL) || aveParam->interval <= 0) {
          s_arg[i_arg].n_items += 2;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb((char*)"bad -average syntax", (char*)"-average={<number>|PVname=<name>}[,interval=<seconds>]\n");
        }
        s_arg[i_arg].n_items += 2;
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -average syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -average syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->averagePV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for average PV.\n");
            if ((loopParam->averagePVInfo.count = (long*)malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawPVAConnection(&loopParam->averagePV, &loopParam->averagePVInfo, 1, *pendIOTime, &(loopParam->pvaAverage));
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb((char*)"invalid -average syntax (unknown qualifier)");
          }
          readPVs(&loopParam->averagePV, &aveParam->n2, &loopParam->averagePVInfo, &loopParam->pvaAverage, 1, NULL, *pendIOTime);
          aveParam->n = round(aveParam->n2);
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &aveParam->n) != 1) {
            fprintf(stderr, "bad -average syntax.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        }
        if (aveParam->n <= 0) {
          FreeEverything();
          SDDS_Bomb((char*)"invalid -average value");
        }
        break;
      case CLO_DELTALIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&delta->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &delta->singleValue, 1, LIMIT_VALUE, "file", SDDS_STRING, &delta->file, 1, LIMIT_FILE, NULL) ||
            (delta->flag & LIMIT_VALUE && delta->flag & LIMIT_FILE) || (!(delta->flag & LIMIT_VALUE) && !(delta->flag & LIMIT_FILE))) {
          fprintf(stderr, "bad -deltaLimit syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_READBACK_LIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&readbackLimits->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &readbackLimits->singleValue, 1, LIMIT_VALUE,
                          "minValue", SDDS_DOUBLE, &readbackLimits->singleMinValue, 1, LIMIT_MINVALUE, "maxValue", SDDS_DOUBLE, &readbackLimits->singleMaxValue, 1, LIMIT_MAXVALUE, "file", SDDS_STRING, &readbackLimits->file, 1, LIMIT_FILE, NULL)) {
          fprintf(stderr, "bad -readbackLimits syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        if ((readbackLimits->flag & LIMIT_MINVALUE && !(readbackLimits->flag & LIMIT_MAXVALUE)) || (!(readbackLimits->flag & LIMIT_MINVALUE) && readbackLimits->flag & LIMIT_MAXVALUE)) {
          fprintf(stderr, "Bad -readbackLimits syntax/value. Need both minValue and maxValue specified.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((readbackLimits->flag & LIMIT_MINVALUE && readbackLimits->flag & LIMIT_MAXVALUE))
          readbackLimits->flag |= LIMIT_MINMAXVALUE;
        i = 0;
        if (readbackLimits->flag & LIMIT_VALUE)
          i++;
        if (readbackLimits->flag & LIMIT_FILE)
          i++;
        if (readbackLimits->flag & LIMIT_MINMAXVALUE)
          i++;
        if (i > 1) {
          fprintf(stderr, "bad -readbackLimits syntax/value.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_ACTIONLIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&action->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &action->singleValue, 1, LIMIT_VALUE, "file", SDDS_STRING, &action->file, 1, LIMIT_FILE, NULL) ||
          (action->flag & LIMIT_VALUE && action->flag & LIMIT_FILE) || (!(action->flag & LIMIT_VALUE) && !(action->flag & LIMIT_FILE))) {
          fprintf(stderr, "bad -actionLimit syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DESPIKE:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&despikeParam->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "neighbors", SDDS_LONG, &despikeParam->neighbors, 1, 0,
                           "passes", SDDS_LONG, &despikeParam->passes, 1, 0,
                           "averageof", SDDS_LONG, &despikeParam->averageOf, 1, DESPIKE_AVERAGEOF,
                           "threshold", SDDS_DOUBLE, &despikeParam->threshold, 1, 0,
                           "startThreshold", SDDS_DOUBLE, &despikeParam->startThreshold, 1, DESPIKE_STARTTHRESHOLD,
                           "endThreshold", SDDS_DOUBLE, &despikeParam->endThreshold, 1, DESPIKE_ENDTHRESHOLD,
                           "stepsThreshold", SDDS_LONG, &despikeParam->stepsThreshold, 1, DESPIKE_STEPSTHRESHOLD,
                           "pvthreshold", SDDS_STRING, &despikeParam->thresholdPV, 1, 0,
                           "rampThresholdPV", SDDS_STRING, &despikeParam->rampThresholdPV, 1, DESPIKE_THRESHOLD_RAMP_PV,
                           "file", SDDS_STRING, &despikeParam->file, 1, 0, "countLimit", SDDS_LONG, &despikeParam->countLimit, 1, 0, NULL) ||
             despikeParam->neighbors < 2 || despikeParam->passes < 1 || despikeParam->averageOf < 2)) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items++;
        if (despikeParam->threshold < 0) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (despikeParam->flag & DESPIKE_STARTTHRESHOLD || despikeParam->flag & DESPIKE_ENDTHRESHOLD || despikeParam->flag & DESPIKE_STEPSTHRESHOLD) {
          if (!(despikeParam->flag & DESPIKE_STARTTHRESHOLD) || !(despikeParam->flag & DESPIKE_ENDTHRESHOLD) || !(despikeParam->flag & DESPIKE_STEPSTHRESHOLD)) {
            fprintf(stderr, "invalid -despike threshold ramp syntax/values, startThreshold, endThreshold and stepsThreshold should all be provided.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
          if (despikeParam->startThreshold < 0 || despikeParam->endThreshold < 0 || despikeParam->endThreshold > despikeParam->startThreshold || despikeParam->stepsThreshold < 2) {
            fprintf(stderr, "invalid -despike threshold ramp syntax/values, the value of startThreshold or endThreshold or stepsThreshold is not valid, they all should be positive and the endThreshold should be smaller than the startThreshold.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
          despikeParam->deltaThreshold = (despikeParam->endThreshold - despikeParam->startThreshold) / (despikeParam->stepsThreshold - 1.0);
          despikeParam->threshold = despikeParam->startThreshold - despikeParam->deltaThreshold;
        }
        if (despikeParam->flag & DESPIKE_THRESHOLD_RAMP_PV && !(despikeParam->flag & DESPIKE_STEPSTHRESHOLD)) {
          if (*verbose)
            fprintf(stderr, "rampThreasholdPV is ignored since threshold ramping is not specified.\n");

          free(despikeParam->rampThresholdPV);
          despikeParam->rampThresholdPV = NULL;
        }

        if (!(despikeParam->flag & DESPIKE_AVERAGEOF))
          despikeParam->averageOf = despikeParam->neighbors;
        if (despikeParam->averageOf > despikeParam->neighbors) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (despikeParam->thresholdPV) {
          if (*verbose)
            fprintf(stderr, "Set up connections for despike threshold PV.\n");
          if ((despikeParam->thresholdPVInfo.count = (long*)malloc(sizeof(long))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SetupRawPVAConnection(&despikeParam->thresholdPV, &despikeParam->thresholdPVInfo, 1, *pendIOTime, &(despikeParam->pvaThreshold));
          readPVs(&despikeParam->thresholdPV, &despikeParam->threshold, &despikeParam->thresholdPVInfo, &despikeParam->pvaThreshold, 1, NULL, *pendIOTime);
        }
        if (despikeParam->rampThresholdPV) {
          if (*verbose)
            fprintf(stderr, "Set up connections for despike threshold Ramp PV %s.\n", despikeParam->rampThresholdPV);
          if ((despikeParam->rampThresholdPVInfo.count = (long*)malloc(sizeof(long))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SetupRawPVAConnection(&despikeParam->rampThresholdPV, &despikeParam->rampThresholdPVInfo, 1, *pendIOTime, &(despikeParam->pvaRampThreshold));
          setEnumPV(despikeParam->rampThresholdPV, despikeParam->reramp, despikeParam->rampThresholdPVInfo, &(despikeParam->pvaRampThreshold), 0);
        }
        break;
      case CLO_GLITCHLOG:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "file", SDDS_STRING, &glitchParam->file, 1, 0,
                           "readbackRmsThreshold", SDDS_DOUBLE, &glitchParam->readbackRMSthreshold, 1, 0,
                           "controlRmsThreshold", SDDS_DOUBLE, &glitchParam->controlRMSthreshold, 1, 0, "rows", SDDS_LONG, &glitchParam->rows, 1, 0, NULL) ||
             glitchParam->readbackRMSthreshold < 0 || glitchParam->controlRMSthreshold < 0 || glitchParam->rows < 1)) {
          fprintf(stderr, "invalid -glitchLogFile syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      case CLO_OVERLAP_COMPENSATION:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "matrixfile", SDDS_STRING, &overlapCompensation->file, 1, 0,
                           "controlquantitydefinition", SDDS_STRING, &overlapCompensation->definitionFile, 1, 0,
                           "coeffile", SDDS_STRING, &overlapCompensation->coefFile, 1, 0, "controllogfile", SDDS_STRING, compensationOutputFile, 1, 0, "gain", SDDS_DOUBLE, &loopParam->compensationGain, 1, 0, "mode", SDDS_STRING, &mode, 1, 0, NULL))) {
          fprintf(stderr, "invalid -auxiliaryOutput syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (mode) {
          switch (match_string(mode, mode_option, 2, 0)) {
          case 0:
            loopParam->compensationIntegral = 1;
            free(mode);
            break;
          case 1:
            loopParam->compensationIntegral = 0;
            free(mode);
            break;
          default:
            fprintf(stderr, "invalid mode given for -auxiliaryOutput syntax/values.\n");
            s_arg[i_arg].n_items++;
            free_scanargs(&s_arg, *argc);
            free(mode);
            return (1);
          }
        }
        s_arg[i_arg].n_items++;
        break;
      case CLO_LAUNCHERPV:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -messagePV syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->launcherPV[0] = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[0], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[0], ".MSG");

        if ((loopParam->launcherPV[1] = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[1], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[1], ".ALRM");

        if ((loopParam->launcherPV[2] = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[2], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[2], ".WAIT");

        if ((loopParam->launcherPV[3] = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[3], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[3], ".VAL");

        if ((loopParam->launcherPV[4] = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[4], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[4], ".CMND");

        if (*verbose)
          fprintf(stderr, "Set up connections for message PV.\n");
        if ((loopParam->launcherPVInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[1].count = (long*)malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[2].count = (long*)malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[3].count = (long*)malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[4].count = (long*)malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        SetupRawPVAConnection(&loopParam->launcherPV[0], &loopParam->launcherPVInfo[0], 1, *pendIOTime, &(loopParam->pvaLauncher[0]));
        SetupRawPVAConnection(&loopParam->launcherPV[1], &loopParam->launcherPVInfo[1], 1, *pendIOTime, &(loopParam->pvaLauncher[1]));
        SetupRawPVAConnection(&loopParam->launcherPV[2], &loopParam->launcherPVInfo[2], 1, *pendIOTime, &(loopParam->pvaLauncher[2]));
        SetupRawPVAConnection(&loopParam->launcherPV[3], &loopParam->launcherPVInfo[3], 1, *pendIOTime, &(loopParam->pvaLauncher[3]));
        SetupRawPVAConnection(&loopParam->launcherPV[4], &loopParam->launcherPVInfo[4], 1, *pendIOTime, &(loopParam->pvaLauncher[4]));
        break;
      case CLO_POST_CHANGE_EXECUTION:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -postChangeExecution syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->postChangeExec = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->postChangeExec, s_arg[i_arg].list[1]);
        break;
      case CLO_FILTERFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -filterFilter syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((correction->coefFile = (char*)SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure for filter file\n");
          FreeEverything();
          exit(1);
        }
        strcpy(correction->coefFile, s_arg[i_arg].list[1]);
        break;
      default:
        fprintf(stderr, "Unrecognized option %s given.\n", s_arg[i_arg].list[0]);
        free_scanargs(&s_arg, *argc);
        return (1);
      }
    } else {
      if (firstTime) {
        if (!correction->file) {
          if (!SDDS_CopyString(&correction->file, s_arg[i_arg].list[0])) {
            fprintf(stderr, "Problem reading input file.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        } else if (!*outputFile) {
          if (!SDDS_CopyString(outputFile, s_arg[i_arg].list[0])) {
            fprintf(stderr, "Problem reading output file.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        } else {
          fprintf(stderr, "Too many filenames given: %s.\n", s_arg[i_arg].list[0]);
          free_scanargs(&s_arg, *argc);
          return (1);
        }
      } else {
        if (!SDDS_CopyString(&correction->file, s_arg[i_arg].list[0])) {
          fprintf(stderr, "Problem reading input file.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
      }
    }
  }

  if (loopParam->holdPresentValues && loopParam->offsetFile) {
    fprintf(stderr, "Can't hold present values and use offset file.\n");
    free_scanargs(&s_arg, *argc);
    return (1);
  }
  if (infinite)
    loopParam->steps = INT_MAX;
  free_scanargs(&s_arg, *argc);
  return (0);
}

void initializeStatsData(STATS *stats) {
  stats->RMS = stats->ave = stats->MAD = stats->largest = 0;
  stats->largestName = NULL;
  return;
}

/* initializes contents of data structures */
void initializeData(CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests) {
  CONTROL_NAME *control, *controlComp, *readback, *readbackComp;

  if ((readback = (CONTROL_NAME*)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->symbolicName = readback->controlName = NULL;
  readback->value = NULL;
  readback->initial = NULL;
  readback->setpoint = readback->error = NULL;
  readback->old = NULL;
  readback->delta = NULL;
  readback->despike = NULL;
  readback->file = NULL;
  readback->channelInfo = NULL;
  readback->waveformMatchFound = NULL;
  readback->waveformIndex = NULL;
  readback->valueIndexes = 0;
  correction->readback = readback;

  if ((control = (CONTROL_NAME *)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  control->channelInfo = NULL;
  control->symbolicName = control->controlName = NULL;
  control->value = NULL;
  control->initial = NULL;
  control->setpoint = control->error = NULL;
  control->old = NULL;
  control->delta = NULL;
  control->setFlag = NULL;
  control->despike = NULL;
  control->file = NULL;
  control->waveformMatchFound = NULL;
  control->waveformIndex = NULL;
  control->historyFiltered = NULL;
  control->history = NULL;
  control->valueIndexes = 0;
  correction->aCoef = NULL;
  correction->bCoef = NULL;
  correction->K = NULL;
  correction->control = control;
  correction->coefFile = NULL;

  if ((readbackComp = (CONTROL_NAME*)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readbackComp->symbolicName = readbackComp->controlName = NULL;
  readbackComp->value = NULL;
  readbackComp->initial = NULL;
  readbackComp->setpoint = readbackComp->error = NULL;
  readbackComp->old = NULL;
  readbackComp->delta = NULL;
  readbackComp->despike = NULL;
  readbackComp->file = NULL;
  readbackComp->channelInfo = NULL;
  readbackComp->waveformMatchFound = NULL;
  readbackComp->waveformIndex = NULL;
  readbackComp->valueIndexes = 0;
  overlapCompensation->file = overlapCompensation->definitionFile = NULL;
  overlapCompensation->readback = readbackComp;
  overlapCompensation->actuator = NULL;

  if ((controlComp = (CONTROL_NAME *)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  controlComp->symbolicName = controlComp->controlName = NULL;
  controlComp->value = NULL;
  controlComp->initial = NULL;
  controlComp->setpoint = controlComp->error = NULL;
  controlComp->old = NULL;
  controlComp->delta = NULL;
  controlComp->despike = NULL;
  controlComp->file = NULL;
  controlComp->setFlag = NULL;
  controlComp->waveformMatchFound = NULL;
  controlComp->waveformIndex = NULL;
  controlComp->channelInfo = NULL;
  if ((controlComp->history = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((controlComp->historyFiltered = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  controlComp->valueIndexes = 0;
  overlapCompensation->control = controlComp;
  overlapCompensation->K = NULL;

  if ((correction->K = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  correction->K->m = 0;
  correction->K->a = NULL;
  /* if ((correction->aCoef = (MATRIX *) calloc(1, sizeof(MATRIX))) == NULL) {
     fprintf(stderr, "memory allocation failure\n");
     FreeEverything();
     exit(1);
     }
     if ((correction->bCoef = (MATRIX *) calloc(1, sizeof(MATRIX))) == NULL) {
     fprintf(stderr, "memory allocation failure\n");
     FreeEverything();
     exit(1);
     } */
  correction->file = NULL;
  correction->coefFile = correction->definitionFile = NULL;
  correction->actuator = NULL;
  if ((overlapCompensation->K = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  overlapCompensation->K->m = 0;
  overlapCompensation->K->a = NULL;
  if ((overlapCompensation->aCoef = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((overlapCompensation->bCoef = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  overlapCompensation->file = NULL;
  overlapCompensation->coefFile = NULL;
  overlapCompensation->actuator = NULL;
  loopParam->steps = DEFAULT_STEPS;
  loopParam->integral = 1;
  loopParam->compensationIntegral = 1;
  loopParam->holdPresentValues = 0;
  loopParam->offsetFile = NULL;
  loopParam->searchPath = NULL;
  loopParam->offsetPVFile = NULL;
  loopParam->offsetPV = NULL;
  loopParam->channelInfo = NULL;
  loopParam->offsetPVvalue = NULL;
  loopParam->n = 0;
  loopParam->dryRun = 0;
  loopParam->epochTime = NULL;
  loopParam->elapsedTime = NULL;
  loopParam->step = NULL;
  loopParam->postChangeExec = NULL;
  /* loopParam->dryRun =1; temporarily, for safe reason */

  loopParam->updateInterval = DEFAULT_UPDATE_INTERVAL;
  loopParam->gain = DEFAULT_GAIN;
  loopParam->compensationGain = DEFAULT_COMPENSATION_GAIN;
  loopParam->gainPV = NULL;
  loopParam->intervalPV = NULL;
  loopParam->averagePV = NULL;
  loopParam->launcherPV[0] = NULL;
  loopParam->launcherPV[1] = NULL;
  loopParam->launcherPV[2] = NULL;
  loopParam->launcherPV[3] = NULL;
  loopParam->launcherPV[4] = NULL;
  loopParam->interval = DEFAULT_TIME_INTERVAL;
  loopParam->briefStatistics = 0;
  aveParam->n = 1;
  aveParam->interval = DEFAULT_AVEINTERVAL;
  delta->flag = 0UL;
  delta->file = NULL;
  delta->singleValue = 0;
  delta->n = 0;
  delta->controlName = NULL;
  delta->value = NULL;
  readbackLimits->flag = 0UL;
  readbackLimits->file = NULL;
  readbackLimits->singleValue = 0;
  readbackLimits->singleMinValue = 0;
  readbackLimits->singleMaxValue = 0;
  readbackLimits->n = 0;
  readbackLimits->controlName = NULL;
  readbackLimits->minValue = NULL;
  readbackLimits->maxValue = NULL;
  action->flag = 0UL;
  action->file = NULL;
  action->singleValue = 0;
  action->n = 0;
  action->controlName = NULL;
  action->value = NULL;
  despikeParam->neighbors = 4;
  despikeParam->averageOf = 2;
  despikeParam->reramp = 0;
  despikeParam->passes = 1;
  despikeParam->threshold = 0;
  despikeParam->thresholdPV = NULL;
  despikeParam->rampThresholdPV = NULL;
  despikeParam->file = NULL;
  despikeParam->flag = 0;
  despikeParam->countLimit = 0;
  despikeParam->startThreshold = 0;
  despikeParam->endThreshold = 0;
  despikeParam->stepsThreshold = 0;
  despikeParam->deltaThreshold = 0.0;

  test->file = NULL;
  test->n = 0;
  test->value = test->min = test->sleep = test->reset = test->holdOff = NULL;
  test->despikeValues = 0;
  test->despike = test->despikeIndex = NULL;
  test->writeToFile = NULL;
  test->controlName = NULL;
  test->channelInfo = NULL;
  test->longestSleep = 1;
  test->longestReset = 0;
  test->longestHoldOff = 1;
  test->glitchLog = NULL;
  test->writeToGlitchLogger = 0;

  glitchParam->file = NULL;
  glitchParam->readbackRMSthreshold = 0;
  glitchParam->controlRMSthreshold = 0;
  glitchParam->rows = 1;
  glitchParam->avail_rows = 0;
  glitchParam->row_pointer = 0;
  glitchParam->glitched = 0;
  /* global variables are done here too. */
  sddscontrollawGlobal->rcParam.PV = sddscontrollawGlobal->rcParam.Desc = sddscontrollawGlobal->rcParam.PVparam = sddscontrollawGlobal->rcParam.DescParam = NULL;

  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  sddscontrollawGlobal->rcParam.pingInterval = 2;
  sddscontrollawGlobal->rcParam.pingTimeout = 10;
  sddscontrollawGlobal->rcParam.alarmSeverity = NO_ALARM;
  sddscontrollawGlobal->rcParam.init = 0;
  generations->doGenerations = 0;
  generations->daily = 0;
  generations->digits = DEFAULT_GENERATIONS_DIGITS;
  generations->rowLimit = 0;
  generations->delimiter = (char*)".";
  generations->timeLimit = 0;

  initializeWaveforms(readbackWaveforms);
  initializeWaveforms(controlWaveforms);
  initializeWaveforms(offsetWaveforms);
  initializeWaveforms(ffSetpointWaveforms);
  initializeTestWaveforms(waveform_tests);
  correction->searchPath = delta->searchPath = readbackLimits->searchPath = action->searchPath =
    despikeParam->searchPath = test->searchPath = overlapCompensation->searchPath = readbackWaveforms->searchPath = controlWaveforms->searchPath = offsetWaveforms->searchPath = ffSetpointWaveforms->searchPath = waveform_tests->searchPath = glitchParam->searchPath = loopParam->searchPath = NULL;
  return;
}

void initializeTestWaveforms(WAVEFORM_TESTS *waveform_tests) {
  waveform_tests->waveformTests = waveform_tests->testFiles = 0;
  waveform_tests->DeviceName = NULL;
  waveform_tests->waveformPV = NULL;
  waveform_tests->testFile = NULL;
  waveform_tests->longestSleep = 1;
  waveform_tests->longestHoldOff = 1;
  waveform_tests->holdOffPresent = 0;
  waveform_tests->channelInfo = NULL;
  waveform_tests->waveformLength = waveform_tests->testFailed = NULL;
  waveform_tests->outOfRange = NULL;
  waveform_tests->index = NULL;
  waveform_tests->despikeValues = NULL;
  waveform_tests->despike = waveform_tests->despikeIndex = waveform_tests->writeToFile = NULL;
  waveform_tests->waveformData = waveform_tests->min = waveform_tests->max = waveform_tests->sleep = waveform_tests->holdOff = NULL;
  waveform_tests->waveformSleep = waveform_tests->waveformHoldOff = NULL;
  waveform_tests->ignore = NULL;
}

void initializeWaveforms(WAVE_FORMS *wave_forms) {
  wave_forms->waveformFile = NULL;
  wave_forms->waveforms = wave_forms->waveformFiles = 0;
  wave_forms->waveformPV = wave_forms->writeWaveformPV = NULL;
  wave_forms->waveformData = NULL;
  wave_forms->readbackIndex = NULL;
  wave_forms->waveformLength = NULL;
  wave_forms->readbackValue = NULL;
  wave_forms->channelInfo = NULL;
  wave_forms->writeChannelInfo = NULL;
}

void setupData(CORRECTION *correction,
               LIMITS *delta,
               LIMITS *readbackLimits,
               LIMITS *action,
               LOOP_PARAM *loopParam,
               DESPIKE_PARAM *despikeParam,
               TESTS *test, CORRECTION *overlapCompensation,
               WAVE_FORMS *readbackWaveforms,
               WAVE_FORMS *controlWaveforms,
               WAVE_FORMS *offsetWaveforms,
               WAVE_FORMS *ffSetpointWaveforms,
               WAVEFORM_TESTS *waveform_tests,
               GLITCH_PARAM *glitchParam,
               long verbose,
               double pendIOTime) {

  CONTROL_NAME *control, *controlComp = NULL, *readback, *readbackComp = NULL;
  long i;

  correction->searchPath = delta->searchPath = readbackLimits->searchPath = action->searchPath =
    despikeParam->searchPath = test->searchPath = overlapCompensation->searchPath = readbackWaveforms->searchPath = controlWaveforms->searchPath = offsetWaveforms->searchPath = ffSetpointWaveforms->searchPath = waveform_tests->searchPath = glitchParam->searchPath = loopParam->searchPath;

  setupInputFile(correction);
  setupReadbackLimitFile(readbackLimits);
  setupDeltaLimitFile(delta);
  setupActionLimitFile(action);
  setupCompensationFiles(overlapCompensation);

  if (sddscontrollawGlobal->rcParam.pingTimeout == 0.0) {
    sddscontrollawGlobal->rcParam.pingTimeout = (float)(1000 * 2 * MAX(loopParam->interval, sddscontrollawGlobal->rcParam.pingInterval));
  }
  readback = correction->readback;
  control = correction->control;
  if (overlapCompensation->file) {
    readbackComp = overlapCompensation->readback;
    controlComp = overlapCompensation->control;
  }
  if (loopParam->step != NULL) {
    free(loopParam->step);
    free(loopParam->epochTime);
    free(loopParam->elapsedTime);
    for (i = 0; i < readback->valueIndexes; i++)
      if (readback->value[i] != NULL) {
        free(readback->value[i]);
        free(readback->delta[i]);
      }
    for (i = 0; i < control->valueIndexes; i++)
      if (control->value[i] != NULL) {
        free(control->value[i]);
        free(control->delta[i]);
      }
    for (i = 0; i < readbackComp->valueIndexes; i++)
      if (readbackComp->value[i] != NULL) {
        free(readbackComp->value[i]);
        free(readbackComp->delta[i]);
      }
    for (i = 0; i < controlComp->valueIndexes; i++)
      if (controlComp->value[i] != NULL) {
        free(controlComp->value[i]);
        free(controlComp->delta[i]);
      }
    free(readback->value);
    free(control->value);
    free(readbackComp->value);
    free(controlComp->value);
    free(readback->delta);
    free(control->delta);
    free(readbackComp->delta);
    free(controlComp->delta);
  }
  readback->valueIndexes = control->valueIndexes = glitchParam->rows;

  if ((loopParam->step = (long *)SDDS_Calloc(1, sizeof(long) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((loopParam->epochTime = (double *)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((loopParam->elapsedTime = (double *)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  if ((readback->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < glitchParam->rows; i++) {
    if ((readback->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if ((readback->initial = (double *)SDDS_Realloc(readback->initial, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->old = (double *)SDDS_Realloc(readback->old, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->despike = (int32_t *)SDDS_Realloc(readback->despike, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  control->integral = loopParam->integral;
  if ((control->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((control->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < glitchParam->rows; i++) {
    if ((control->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (control->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (control->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if ((control->initial = (double *)SDDS_Realloc(control->initial, sizeof(double) * (control->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((control->old = (double *)SDDS_Realloc(control->old, sizeof(double) * (control->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (waveform_tests->testFiles)
    setupWaveformTests(waveform_tests, readback, control, loopParam->interval, verbose, pendIOTime);
  if (readbackWaveforms->waveformFiles)
    setupWaveforms(readbackWaveforms, readback, verbose, pendIOTime);
  if (offsetWaveforms->waveformFiles)
    setupWaveforms(offsetWaveforms, readback, verbose, pendIOTime);
  if (controlWaveforms->waveformFiles)
    setupWaveforms(controlWaveforms, control, verbose, pendIOTime);

  /* overlap compensation data */
  if (overlapCompensation->file) {
    readbackComp->valueIndexes = controlComp->valueIndexes = glitchParam->rows;
    if ((readbackComp->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < glitchParam->rows; i++) {
      if ((readbackComp->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readbackComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((readbackComp->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readbackComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    if ((readbackComp->initial = (double *)SDDS_Realloc(readbackComp->initial, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->old = (double *)SDDS_Realloc(readbackComp->old, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->setpoint = (double *)SDDS_Realloc(readbackComp->setpoint, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->error = (double *)SDDS_Realloc(readbackComp->error, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    readbackComp->despike = NULL;

    controlComp->integral = loopParam->compensationIntegral;
    /*now the controlComp is array of bpm Setpoints, the readbackComp is the same as control */
    if (ffSetpointWaveforms->waveformFiles)
      setupWaveforms(ffSetpointWaveforms, controlComp, verbose, pendIOTime);
    if ((controlComp->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < glitchParam->rows; i++) {
      if ((controlComp->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (controlComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((controlComp->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (controlComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    if ((controlComp->initial = (double *)SDDS_Realloc(controlComp->initial, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->old = (double *)SDDS_Realloc(controlComp->old, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->setpoint = (double *)SDDS_Realloc(controlComp->setpoint, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->error = (double *)SDDS_Realloc(controlComp->error, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if (loopParam->holdPresentValues || loopParam->offsetFile || loopParam->offsetPVFile) {
    if ((readback->setpoint = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->error = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  matchUpControlNames(delta, control->controlName, control->n);
  matchUpControlNames(readbackLimits, readback->controlName, readback->n);
  matchUpControlNames(action, readback->controlName, readback->n);
  if (loopParam->offsetFile) {
    readOffsetValues(readback->initial, readback->n, readback->controlName, loopParam->offsetFile, loopParam->searchPath);
    /* Can't use hold present values and offset file command line options together
         but set holdPresentValues flag here because the function is equivalent
       */
    loopParam->holdPresentValues = 1;
  }
  if (loopParam->offsetPVFile) {
    /*get the offset PV name and rearrange it according to the order of readbacks */
    if ((loopParam->offsetPV = (char **)malloc(sizeof(char *) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((loopParam->offsetPVvalue = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    readOffsetPV(loopParam->offsetPV, readback->n, readback->controlName, loopParam->offsetPVFile, loopParam->searchPath);
    loopParam->n = readback->n;
    /*setup connection for offset PVs */
    if ((loopParam->channelInfo = (CHANNEL_INFO*)malloc(sizeof(*loopParam->channelInfo) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((loopParam->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawPVAConnection(loopParam->offsetPV, loopParam->channelInfo, readback->n, pendIOTime, &(loopParam->pva));
  }
  setupTestsFile(test, loopParam->interval, verbose, pendIOTime);
  setupDespikeFile(despikeParam, readback, verbose);

  return;
}

void logActuator(char *file, CONTROL_NAME *control, char *timeStamp) {
  SDDS_TABLE outputPage;

  if (file) {
    if (!SDDS_InitializeOutput(&outputPage, SDDS_BINARY, 1,
                               "Control variables",
                               "Control variables", file) ||
        (0 > SDDS_DefineParameter(&outputPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(&outputPage, "ControlName", NULL,
                               NULL, "Control name", NULL, SDDS_STRING, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Value", NULL,
                               NULL, "Actual setpoint sent", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Old", NULL, NULL, "Previous setpoint sent", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Delta", NULL, NULL, "Change in setpoint", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(&outputPage);
    if (!SDDS_WriteLayout(&outputPage) || !SDDS_StartTable(&outputPage, control->n)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "TimeStamp", timeStamp, NULL) ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->controlName,
                        control->n, "ControlName") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->value[0], control->n, "Value") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->old, control->n, "Old") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->delta[0], control->n, "Delta")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_UpdatePage(&outputPage, FLUSH_TABLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_Terminate(&outputPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

#define CL_BUFFER_SIZE 16384
long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc, FILE *fp) {
  long i, l;
  char buffer[CL_BUFFER_SIZE];

  if (!fgets(buffer, CL_BUFFER_SIZE, fp) || SDDS_StringIsBlank(buffer))
    return 0;
  buffer[strlen(buffer) - 1] = 0;

  *argc = countArguments(buffer) + commandlineArgc;
  *argv = (char**)tmalloc((*argc + 1) * sizeof(**argv));
  for (i = 0; i < commandlineArgc; i++)
    SDDS_CopyString((*argv) + i, commandlineArgv[i]);

  while (((*argv)[i] = get_token_t(buffer, (char*)" "))) {
    l = strlen((*argv)[i]);
    if ((*argv)[i][0] == '"' && (*argv)[i][l - 1] == '"') {
      strslide((*argv)[i], -1);
      (*argv)[i][l - 2] = 0;
    }
    i++;
  }

  if (i != *argc) {
    FreeEverything();
    SDDS_Bomb((char*)"argument count problem in ReadMoreArguments");
  }
  return 1;
}

int countArguments(char *s) {
  int count;
  char *ptr, *copy;

  count = 0;
  cp_str(&copy, s);

  while ((ptr = get_token_t(copy, (char*)" "))) {
    count++;
    free(ptr);
  }
  free(copy);
  return (count);
}

void readOffsetValues(double *offset, long n, char **controlName, char *offsetFile, char *searchPath) {
  SDDS_DATASET SDDSin;
  char **inputName;
  double *inputOffset = 0;
  long rows = 0, i, j, found;

  inputName = NULL;
  if (searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&SDDSin, offsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&SDDSin, offsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  if (SDDS_ReadPage(&SDDSin) < 0 || (rows = SDDS_CountRowsOfInterest(&SDDSin)) <= 0 ||
      !(inputName = (char**)SDDS_GetInternalColumn(&SDDSin, (char*)"ControlName")) ||
      !(inputOffset = (double*)SDDS_GetInternalColumn(&SDDSin, (char*)"Offset"))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!controlName) {
    FreeEverything();
    SDDS_Bomb((char*)"problem with control name data (readOffsetValues)");
  }
  for (i = 0; i < n; i++) {
    for (j = found = 0; j < rows; j++) {
      if (strcmp(inputName[j], controlName[i]) == 0) {
        offset[i] = inputOffset[j];
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Error: no offset found for readback %s in file %s\n", controlName[i], offsetFile);
      FreeEverything();
      exit(1);
    }
  }
  /*
     for (i=0;i<rows;i++) 
     free(inputName[i]);
     free(inputName);
     free(inputOffset);
   */
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
}

void readOffsetPV(char **offsetPVs, long n, char **controlName, char *PVOffsetFile, char *searchPath) {
  SDDS_DATASET SDDSin;
  char **inputName, **inputOffsetPV = NULL;

  long rows = 0, i, j, found;

  inputName = NULL;
  if (searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&SDDSin, PVOffsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&SDDSin, PVOffsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  if (SDDS_ReadPage(&SDDSin) < 0 || (rows = SDDS_CountRowsOfInterest(&SDDSin)) <= 0 ||
      !(inputOffsetPV = (char**)SDDS_GetInternalColumn(&SDDSin, (char*)"OffsetControlName")) ||
      !(inputName = (char**)SDDS_GetInternalColumn(&SDDSin, (char*)"ControlName"))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!controlName) {
    FreeEverything();
    SDDS_Bomb((char*)"problem with control name data (readPVOffsets)");
  }
  for (i = 0; i < n; i++) {
    for (j = found = 0; j < rows; j++) {
      if (strcmp(inputName[j], controlName[i]) == 0) {
        if ((offsetPVs[i] = (char*)malloc(sizeof(char) * (strlen(inputOffsetPV[j]) + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(offsetPVs[i], inputOffsetPV[j]);
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Error: no offset PV found for readback %s in file %s\n", controlName[i], PVOffsetFile);
      FreeEverything();
      exit(1);
    }
  }
  /*
     for (i=0;i<rows;i++) {
     free(inputName[i]);
     free(inputOffsetPV[i]);
     }
     free(inputName);
     free(inputOffsetPV);
   */
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
}

void setupWaveforms(WAVE_FORMS *wave_forms, CONTROL_NAME *CONTROL, long verbose, double pendIOTime) {
  long i, j, n, k, m, unmatched, samePV = 0;
  char **DeviceNames, **readback, **controlName;
  int32_t *index;
  SDDS_DATASET dataset;
  int32_t rows;

  rows = 0;
  DeviceNames = readback = NULL;
  index = NULL;
  if (!wave_forms->waveformFiles)
    return;
  readback = CONTROL->symbolicName;
  controlName = CONTROL->controlName;

  n = CONTROL->n;
  if ((wave_forms->readbackValue = (double*)calloc(n, sizeof(*wave_forms->readbackValue))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < wave_forms->waveformFiles; i++) {
    if (wave_forms->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&dataset, wave_forms->waveformFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&dataset, wave_forms->waveformFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    switch (SDDS_CheckColumn(&dataset, (char*)"DeviceName", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"DevicName\" column in file %s.\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"Index", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"Index\" column in file %s.\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }

    /*if only WaveformPV is given, then it is used for both reading and writing, otherwise
         two parameters "ReadbackWaveformPV" and "WriteWaveformPV" should be given. */
    switch (SDDS_CheckParameter(&dataset, (char*)"WaveformPV", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      samePV = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      if (SDDS_CheckParameter(&dataset, (char*)"WriteWaveformPV", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK ||
          SDDS_CheckParameter(&dataset, (char*)"ReadWaveformPV", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      samePV = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with parameter \"WaveformPV\" in file %s\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }

    while (SDDS_ReadPage(&dataset) > 0) {
      m = wave_forms->waveforms;
      if ((wave_forms->waveformPV = (char**)SDDS_Realloc(wave_forms->waveformPV, sizeof(*wave_forms->waveformPV) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->channelInfo = (CHANNEL_INFO*)SDDS_Realloc(wave_forms->channelInfo, sizeof(*wave_forms->channelInfo) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->readbackIndex = (int32_t**)SDDS_Realloc(wave_forms->readbackIndex, sizeof(*wave_forms->readbackIndex) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformData = (double**)SDDS_Realloc(wave_forms->waveformData, sizeof(*wave_forms->waveformData) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformLength = (int32_t*)SDDS_Realloc(wave_forms->waveformLength, sizeof(*wave_forms->waveformLength) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if (samePV) {
        if (!SDDS_GetParameter(&dataset, (char*)"WaveformPV", &wave_forms->waveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      } else {
        if ((wave_forms->writeWaveformPV = (char**)SDDS_Realloc(wave_forms->writeWaveformPV, sizeof(*wave_forms->writeWaveformPV) * (m + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((wave_forms->writeChannelInfo = (CHANNEL_INFO*)SDDS_Realloc(wave_forms->writeChannelInfo, sizeof(*wave_forms->writeChannelInfo) * (m + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if (!SDDS_GetParameter(&dataset, (char*)"WriteWaveformPV", &wave_forms->writeWaveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        if (!SDDS_GetParameter(&dataset, (char*)"ReadWaveformPV", &wave_forms->waveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
      if (!(rows = SDDS_CountRowsOfInterest(&dataset))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      wave_forms->waveformLength[m] = rows;
      if ((wave_forms->readbackIndex[m] = (int32_t*)malloc(sizeof(long) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformData[m] = (double*)malloc(sizeof(double) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      DeviceNames = (char **)SDDS_GetColumn(&dataset, (char*)"DeviceName");
      index = SDDS_GetColumnInLong(&dataset, (char*)"Index");
      wave_forms->channelInfo[m].waveformLength = rows;
      wave_forms->channelInfo[m].waveformData = wave_forms->waveformData[m];
      if (!samePV) {
        wave_forms->writeChannelInfo[m].waveformLength = rows;
        wave_forms->writeChannelInfo[m].waveformData = wave_forms->waveformData[m];
      }

      /* sort the bpms by the index in waveform */
      SortBPMsByIndex(&DeviceNames, index, rows);
      for (j = 0; j < rows; j++) {
        k = match_string(DeviceNames[j], readback, n, EXACT_MATCH);
        if (k < 0)
          k = match_string(DeviceNames[j], controlName, n, UNIQUE_MATCH);
        wave_forms->readbackIndex[m][j] = k;
        if (k >= 0)
          CONTROL->waveformMatchFound[k] = 1;
        free(DeviceNames[j]);
      }
      free(DeviceNames);
      free(index);
      wave_forms->waveforms++;
    }
    if (!SDDS_Terminate(&dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  if ((wave_forms->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for reading waveform.\n");
  SetupRawPVAConnection(wave_forms->waveformPV, wave_forms->channelInfo, wave_forms->waveforms, pendIOTime, &(wave_forms->pvaChannel));
  if (samePV) {
    wave_forms->writeWaveformPV = wave_forms->waveformPV;
    wave_forms->writeChannelInfo = wave_forms->channelInfo;
  } else {
    if ((wave_forms->writeChannelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "Set up connections for writinging waveform.\n");
    SetupRawPVAConnection(wave_forms->writeWaveformPV, wave_forms->writeChannelInfo, wave_forms->waveforms, pendIOTime, &(wave_forms->pvaWriteChannel));
  }
  for (k = unmatched = 0; k < n; k++) {
    if (!CONTROL->waveformMatchFound[k]) {
      unmatched++;
      fprintf(stderr, "Error: column %s in the matrix has no match in the waveform files\n", readback[k]);
    }
  }
  if (unmatched) {
    FreeEverything();
    exit(1);
  }
}

long isSorted(int32_t *index, long n) {
  int i;
  if (!n)
    return 1;
  for (i = 0; i < n - 1; i++) {
    if (index[i + 1] < index[i])
      return 0;
  }
  return 1;
}

int Compare2Longs(const void *vindex1, const void *vindex2)
{
  long i1, i2;

  i1 = *(long *)vindex1;
  i2 = *(long *)vindex2;
  if (sddscontrollawGlobal->sortIndex[i1] > sddscontrollawGlobal->sortIndex[i2]) {
    return 1;
  } else
    return 0;
}

/* sort the bpms by the index in waveform */
void SortBPMsByIndex(char ***PV, int32_t *index, long n) {
  long i, *sort;
  char **temp, **sortPV;
  sortPV = *PV;

  if (isSorted(index, n))
    return;
  sddscontrollawGlobal->sortIndex = index;
  if ((sort = (long*)malloc(sizeof(*sort) * n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < n; i++)
    sort[i] = i;
  if ((temp = (char**)malloc(sizeof(*temp) * n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  qsort((void *)sort, n, sizeof(*sort), Compare2Longs);
  for (i = 0; i < n; i++) {
    SDDS_CopyString(&temp[i], sortPV[i]);
  }
  for (i = 0; i < n; i++) {
    free(sortPV[i]);
    SDDS_CopyString(&sortPV[i], temp[sort[i]]);
  }
  for (i = 0; i < n; i++)
    free(temp[i]);
  free(temp);
  free(sort);
}

long ReadWaveformData(WAVE_FORMS *wave_forms, double pendIOTime) {
  int32_t i, length, j, k, n;
  char **PVs;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL *pva;

  length = 0;
  if (!wave_forms->waveforms)
    return 0;
  n = wave_forms->waveforms;
  PVs = wave_forms->waveformPV;
  channelInfo = wave_forms->channelInfo;
  pva = &wave_forms->pvaChannel;
  *(channelInfo[0].count) = n;


  for (i = 0; i < n; i++) {
    if (pva->isConnected[i] == false) {
      fprintf(stderr, "Error, no connection for %s\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    channelInfo[i].flag = 0;
  }
  freePVAGetReadings(pva);
  if (GetPVAValues(pva) == 1) {
    fprintf(stderr, "Error reading PVs\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < n; i++) {
    if (pva->pvaData[i].numGetElements != channelInfo[i].waveformLength) {
      fprintf(stderr, "Error: %s waveform length different from expected\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    for (j = 0; j < channelInfo[i].waveformLength; j++) {
      channelInfo[i].waveformData[j] = pva->pvaData[i].getData[0].values[j];
    }
  }
  *(channelInfo[0].count) = 0;
  for (i = 0; i < n; i++) {
    wave_forms->waveformData[i] = channelInfo[i].waveformData;
    length = wave_forms->waveformLength[i];
    for (j = 0; j < length; j++) {
      k = wave_forms->readbackIndex[i][j];
      if (k >= 0)
        wave_forms->readbackValue[k] = wave_forms->waveformData[i][j];
    }
  }
  return 0;
}

void setupWaveformTests(WAVEFORM_TESTS *waveform_tests, CONTROL_NAME *readback, CONTROL_NAME *control, double interval, long verbose, double pendIOTime) {
  long i, n, k = 0, rows = 0, ignoreExist = 0, m, despikeColumnPresent = 0;
  SDDS_DATASET dataset;
  int32_t *index;
  int32_t *despike;
  short *ignore;
  char **DeviceName;
  double *max, *min, *sleep, *holdOff;
  long sleepTimeColumnPresent = 0, sleepIntervalColumnPresent = 0, row;
  long holdOffTimeColumnPresent = 0, holdOffIntervalColumnPresent = 0;

  DeviceName = NULL;
  max = min = sleep = holdOff = NULL;
  index = NULL;
  despike = NULL;

  if (!waveform_tests->testFiles)
    return;

  n = waveform_tests->testFiles;
  for (i = 0; i < n; i++) {
    if (waveform_tests->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&dataset, waveform_tests->testFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&dataset, waveform_tests->testFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    /*check the existence of required columns */
    switch (SDDS_CheckColumn(&dataset, (char*)"DeviceName", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"DevicName\" column in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"Index", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"Index\" column in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"MinimumValue", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"MinimumValue\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"MaximumValue", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"MaximumValue\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"Ignore", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      ignoreExist = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"Ignore\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"SleepTime", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      sleepTimeColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      sleepTimeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"SleepTime\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"SleepIntervals", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      sleepIntervalColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      sleepIntervalColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"SleepIntervals\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    if (sleepTimeColumnPresent && sleepIntervalColumnPresent) {
      fprintf(stderr, "SleepTime column and SleepIntervals can not exist in the same time in test file %s\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"HoldOffTime", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      holdOffTimeColumnPresent = 1;
      waveform_tests->holdOffPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      holdOffTimeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"HoldOffTime\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"HoldOffIntervals", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      holdOffIntervalColumnPresent = 1;
      waveform_tests->holdOffPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      holdOffIntervalColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"HoldOffIntervals\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    if (holdOffTimeColumnPresent && holdOffIntervalColumnPresent) {
      fprintf(stderr, "HoldOffTime column and HoldOffIntervals can not exist in the same time in test file %s\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, (char*)"Despike", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      despikeColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      despikeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    while (SDDS_ReadPage(&dataset) > 0) {
      k = waveform_tests->waveformTests;
      if ((waveform_tests->DeviceName = (char***)SDDS_Realloc(waveform_tests->DeviceName, sizeof(*waveform_tests->DeviceName) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformPV = (char**)SDDS_Realloc(waveform_tests->waveformPV, sizeof(*waveform_tests->waveformPV) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->channelInfo = (CHANNEL_INFO*)SDDS_Realloc(waveform_tests->channelInfo, sizeof(*waveform_tests->channelInfo) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformLength = (int32_t*)SDDS_Realloc(waveform_tests->waveformLength, sizeof(*waveform_tests->waveformLength) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->outOfRange = (int32_t**)SDDS_Realloc(waveform_tests->outOfRange, sizeof(*waveform_tests->outOfRange) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->index = (int32_t**)SDDS_Realloc(waveform_tests->index, sizeof(*waveform_tests->index) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->testFailed = (int32_t*)SDDS_Realloc(waveform_tests->testFailed, sizeof(*waveform_tests->testFailed) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformData = (double**)SDDS_Realloc(waveform_tests->waveformData, sizeof(*waveform_tests->waveformData) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->min = (double**)SDDS_Realloc(waveform_tests->min, sizeof(*waveform_tests->min) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->max = (double**)SDDS_Realloc(waveform_tests->max, sizeof(*waveform_tests->max) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->writeToFile = (int32_t**)SDDS_Realloc(waveform_tests->writeToFile, sizeof(*waveform_tests->writeToFile) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->ignore = (short**)SDDS_Realloc(waveform_tests->ignore, sizeof(*waveform_tests->ignore) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->despikeValues = (int32_t*)SDDS_Realloc(waveform_tests->despikeValues, sizeof(*waveform_tests->despikeValues) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->despikeValues[k] = 0;
      if ((waveform_tests->waveformSleep = (double*)SDDS_Realloc(waveform_tests->waveformSleep, sizeof(*waveform_tests->waveformSleep) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformSleep[k] = 1;
      if ((waveform_tests->waveformHoldOff = (double*)SDDS_Realloc(waveform_tests->waveformHoldOff, sizeof(*waveform_tests->waveformHoldOff) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformHoldOff[k] = 1;
      if (!SDDS_GetParameter(&dataset, (char*)"WaveformPV", &waveform_tests->waveformPV[k])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      if (!(rows = SDDS_CountRowsOfInterest(&dataset))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformLength[k] = rows;
      waveform_tests->testFailed[k] = 0;
      if ((waveform_tests->writeToFile[k] = (int32_t*)malloc(sizeof(**waveform_tests->writeToFile) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if (despikeColumnPresent) {
        if ((waveform_tests->despikeIndex = (int32_t**)SDDS_Realloc(waveform_tests->despikeIndex, sizeof(*waveform_tests->despikeIndex) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((waveform_tests->despike = (int32_t**)SDDS_Realloc(waveform_tests->despike, sizeof(*waveform_tests->despike) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        waveform_tests->despikeIndex[k] = NULL;
      }
      if (sleepTimeColumnPresent || sleepIntervalColumnPresent) {
        if ((waveform_tests->sleep = (double**)SDDS_Realloc(waveform_tests->sleep, sizeof(*waveform_tests->sleep) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
      }
      if (holdOffTimeColumnPresent || holdOffIntervalColumnPresent) {
        if ((waveform_tests->holdOff = (double**)SDDS_Realloc(waveform_tests->holdOff, sizeof(*waveform_tests->holdOff) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
      }
      if ((waveform_tests->waveformData[k] = (double*)malloc(sizeof(**waveform_tests->waveformData) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->channelInfo[k].waveformLength = rows;
      waveform_tests->channelInfo[k].waveformData = waveform_tests->waveformData[k];

      DeviceName = (char **)SDDS_GetColumn(&dataset, (char*)"DeviceName");
      index = SDDS_GetColumnInLong(&dataset, (char*)"Index");
      if (ignoreExist)
        ignore = (short*)SDDS_GetColumn(&dataset, (char*)"Ignore");
      else {
        /*test value only contains readbacks and controls, ignore all others */
        if ((ignore = (short*)malloc(sizeof(*ignore) * rows)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        for (m = 0; m < rows; m++) {
          if (match_string(DeviceName[m], readback->symbolicName, readback->n, EXACT_MATCH) >= 0 ||
              match_string(DeviceName[m], control->symbolicName, control->n, EXACT_MATCH) >= 0)
            ignore[m] = 0;
          else
            ignore[m] = 1;
        }
      }
      min = SDDS_GetColumnInDoubles(&dataset, (char*)"MinimumValue");
      max = SDDS_GetColumnInDoubles(&dataset, (char*)"MaximumValue");
      if (sleepTimeColumnPresent)
        sleep = SDDS_GetColumnInDoubles(&dataset, (char*)"SleepTime");
      if (sleepIntervalColumnPresent) {
        sleep = SDDS_GetColumnInDoubles(&dataset, (char*)"SleepIntervals");
        for (row = 0; row < rows; row++)
          sleep[row] = sleep[row] * interval;
      }
      if (holdOffTimeColumnPresent)
        holdOff = SDDS_GetColumnInDoubles(&dataset, (char*)"HoldOffTime");
      if (holdOffIntervalColumnPresent) {
        holdOff = SDDS_GetColumnInDoubles(&dataset, (char*)"HoldOffIntervals");
        for (row = 0; row < rows; row++)
          holdOff[row] = holdOff[row] * interval;
      }
      if (despikeColumnPresent)
        despike = (int32_t *)SDDS_GetColumn(&dataset, (char*)"Despike");
      /*sort data by the index if it is not sorted */
      /*delete sorting the waveform index part -- we should make the waveform file is sorted by index */

      waveform_tests->index[k] = index;
      waveform_tests->DeviceName[k] = DeviceName;
      waveform_tests->max[k] = max;
      waveform_tests->min[k] = min;
      waveform_tests->ignore[k] = ignore;
      if (despikeColumnPresent)
        waveform_tests->despike[k] = despike;
      if (sleepTimeColumnPresent || sleepIntervalColumnPresent)
        waveform_tests->sleep[k] = sleep;
      if (holdOffTimeColumnPresent || holdOffIntervalColumnPresent)
        waveform_tests->holdOff[k] = holdOff;
    }
    if ((waveform_tests->outOfRange[k] = (int32_t*)calloc(rows, sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    waveform_tests->waveformTests++;

    if (despikeColumnPresent) {
      if ((waveform_tests->despikeIndex[k] = (int32_t *)malloc(sizeof(int32_t) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (m = 0; m < rows; m++) {
        if (waveform_tests->despike[k][m]) {
          waveform_tests->despikeIndex[k][waveform_tests->despikeValues[k]] = m;
          waveform_tests->despikeValues[k]++;
        }
      }
    }
    if (!SDDS_Terminate(&dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  if ((waveform_tests->channelInfo[0].count = (long*)malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for testing waveform.\n");
  SetupRawPVAConnection(waveform_tests->waveformPV, waveform_tests->channelInfo, waveform_tests->waveformTests, pendIOTime, &(waveform_tests->pvaChannel));
  if (verbose) {
    fprintf(stderr, "number of waveform tests =%ld\n", waveform_tests->waveformTests);
    for (i = 0; i < waveform_tests->waveformTests; i++)
      fprintf(stderr, "  %ld\t\t%s\n", i, waveform_tests->waveformPV[i]);
  }
}

long CheckWaveformTest(WAVEFORM_TESTS *waveform_tests, BACKOFF *backoff, LOOP_PARAM *loopParam, DESPIKE_PARAM *despikeParam, long verbose, long warning, double pendIOTime) {
  long i, j, outOfRange, tests, k, spikesDone;
  double *despikeTestData, **value, **max, **min;
  int32_t *despikeValues, **despikeIndex, *length, **outRange;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL *pva;

  outOfRange = 0;
  waveform_tests->longestSleep = 0;
  if (!waveform_tests->waveformTests)
    return 0;
  tests = waveform_tests->waveformTests;
  despikeValues = waveform_tests->despikeValues;
  value = waveform_tests->waveformData;
  max = waveform_tests->max;
  min = waveform_tests->min;
  despikeIndex = waveform_tests->despikeIndex;
  length = waveform_tests->waveformLength;
  outRange = waveform_tests->outOfRange;
  channelInfo = waveform_tests->channelInfo;
  pva = &waveform_tests->pvaChannel;

  if (verbose)
    fprintf(stderr, "Reading waveform test devices at %f seconds.\n", loopParam->elapsedTime[0]);
  /*read test waveform data */
  *(channelInfo[0].count) = tests;

  for (i = 0; i < tests; i++) {
    if (pva->isConnected[i] == false) {
      fprintf(stderr, "Error, no connection for %s\n", waveform_tests->waveformPV[i]);
      FreeEverything();
      exit(1);
    }
    channelInfo[i].flag = 0;
  }
  freePVAGetReadings(pva);
  if (GetPVAValues(pva) == 1) {
    fprintf(stderr, "Error reading PVs\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < tests; i++) {
    if (pva->pvaData[i].numGetElements != channelInfo[i].waveformLength) {
      fprintf(stderr, "Error: %s waveform length different from expected\n", waveform_tests->waveformPV[i]);
      FreeEverything();
      exit(1);
    }
    for (j = 0; j < channelInfo[i].waveformLength; j++) {
      channelInfo[i].waveformData[j] = pva->pvaData[i].getData[0].values[j];
    }
  }


  *(channelInfo[0].count) = 0;
  for (i = 0; i < tests; i++) {
    waveform_tests->waveformSleep[i] = 0;
    waveform_tests->testFailed[i] = 0;
    if ((despikeParam->threshold > 0) && despikeValues[i]) {
      if ((despikeTestData = (double *)malloc(sizeof(double) * (despikeValues[i]))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (k = 0; k < despikeValues[i]; k++)
        despikeTestData[k] = value[i][despikeIndex[i][k]];
      /* uses the same despike parameters as the readback values */
      if ((spikesDone = despikeData(despikeTestData, despikeValues[i], despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
        if (verbose) {
          fprintf(stderr, "%ld spikes from %s waveform test removed.\n", spikesDone, waveform_tests->waveformPV[i]);
        }
      }
      for (k = 0; k < despikeValues[i]; k++)
        value[i][despikeIndex[i][k]] = despikeTestData[k];
      free(despikeTestData);
    }
    for (j = 0; j < length[i]; j++) {
      if (waveform_tests->ignore[i][j]) {
        outRange[i][j] = 0;
        continue;
      }
      if (value[i][j] < min[i][j] || value[i][j] > max[i][j]) {
        outRange[i][j] = 1;
        waveform_tests->testFailed[i] = 1;
        outOfRange = 1;
        if (waveform_tests->sleep)
          waveform_tests->waveformSleep[i] = MAX(waveform_tests->waveformSleep[i], waveform_tests->sleep[i][j]);
        if (waveform_tests->holdOff)
          waveform_tests->waveformHoldOff[i] = MAX(waveform_tests->waveformHoldOff[i], waveform_tests->holdOff[i][j]);
      } else
        outRange[i][j] = 0;
    }
    waveform_tests->longestSleep = (long)(MAX(waveform_tests->longestSleep, waveform_tests->waveformSleep[i]));
    waveform_tests->longestHoldOff = (long)(MAX(waveform_tests->longestHoldOff, waveform_tests->waveformHoldOff[i]));
  }
  if (outOfRange) {
    if (verbose || warning) {
      backoff->counter++;
      if (backoff->counter >= backoff->level) {
        backoff->counter = 0;
        if (backoff->level < 8)
          backoff->level *= 2;
      }
      if (verbose) {
        fprintf(stderr, "Waveform tests out of range:\n");
        for (i = 0; i < tests; i++) {
          if (waveform_tests->testFailed[i])
            fprintf(stderr, "\tWaveform %s test failed.\n", waveform_tests->waveformPV[i]);
          for (j = 0; j < length[i]; j++) {
            if (outRange[i][j])
              fprintf(stderr, "\t\t%s\t\t%g\n", waveform_tests->DeviceName[i][j], value[i][j]);
          }
        }
        fprintf(stderr, "Waiting for %f seconds.\n", MAX(waveform_tests->longestSleep, loopParam->interval));
      }
    }
  }
  return outOfRange;
}

void cleanupWaveforms(WAVE_FORMS *wave_forms) {
  long i;
  if (wave_forms && wave_forms->waveformFiles) {
    if (wave_forms->waveformFile) {
      for (i = 0; i < wave_forms->waveformFiles; i++) {
        if (wave_forms->waveformFile[i])
          free(wave_forms->waveformFile[i]);
      }
      free(wave_forms->waveformFile);
      wave_forms->waveformFile = NULL;
    }
  }
  if (wave_forms && wave_forms->waveforms) {
    for (i = 0; i < wave_forms->waveforms; i++) {
      if (wave_forms->waveformPV) {
        if (wave_forms->waveformPV[i])
          free(wave_forms->waveformPV[i]);
        wave_forms->waveformPV[i] = NULL;
      }
      if (wave_forms->writeWaveformPV)
        if (wave_forms->writeWaveformPV[i])
          free(wave_forms->writeWaveformPV[i]);
      if (wave_forms->readbackIndex)
        if (wave_forms->readbackIndex[i])
          free(wave_forms->readbackIndex[i]);
      if (wave_forms->waveformData)
        if (wave_forms->waveformData[i])
          free(wave_forms->waveformData[i]);
    }
    wave_forms->waveforms = 0;
    if (wave_forms->waveformData) {
      free(wave_forms->waveformData);
      wave_forms->waveformData = NULL;
    }
    if (wave_forms->writeWaveformPV && (wave_forms->writeWaveformPV != wave_forms->waveformPV)) {
      free(wave_forms->writeWaveformPV);
      wave_forms->writeWaveformPV = NULL;
    }
    if (wave_forms->waveformPV) {
      free(wave_forms->waveformPV);
      wave_forms->waveformPV = NULL;
    }
    if (wave_forms->waveformLength) {
      free(wave_forms->waveformLength);
      wave_forms->waveformLength = NULL;
    }
    if (wave_forms->writeChannelInfo && (wave_forms->writeChannelInfo != wave_forms->channelInfo)) {
      if (wave_forms->writeChannelInfo[0].count)
        free(wave_forms->writeChannelInfo[0].count);
      free(wave_forms->writeChannelInfo);
      wave_forms->writeChannelInfo = NULL;
    }
    if (wave_forms->channelInfo) {
      if (wave_forms->channelInfo[0].count)
        free(wave_forms->channelInfo[0].count);
      free(wave_forms->channelInfo);
      wave_forms->channelInfo = NULL;
    }
    if (wave_forms->readbackIndex) {
      free(wave_forms->readbackIndex);
      wave_forms->readbackIndex = NULL;
    }
    if (wave_forms->readbackValue) {
      free(wave_forms->readbackValue);
      wave_forms->readbackValue = NULL;
    }
  }
}

void cleanupTestWaveforms(WAVEFORM_TESTS *waveform_tests) {
  long i, j;
  if (waveform_tests->testFile) {
    for (i = 0; i < waveform_tests->testFiles; i++) {
      if (waveform_tests->testFile[i])
        free(waveform_tests->testFile[i]);
      waveform_tests->testFile[i] = NULL;
    }
    free(waveform_tests->testFile);
    waveform_tests->testFile = NULL;
  }
  if (waveform_tests->waveformTests) {
    if (waveform_tests->DeviceName) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        if (waveform_tests->DeviceName[i]) {
          if (waveform_tests->waveformLength) {
            for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
              if (waveform_tests->DeviceName[i][j])
                free(waveform_tests->DeviceName[i][j]);
            }
          }
          free(waveform_tests->DeviceName[i]);
        }
      }
      free(waveform_tests->DeviceName);
      waveform_tests->DeviceName = NULL;
    }
    for (i = 0; i < waveform_tests->waveformTests; i++) {
      if (waveform_tests->waveformPV)
        if (waveform_tests->waveformPV[i])
          free(waveform_tests->waveformPV[i]);
      if (waveform_tests->index)
        if (waveform_tests->index[i])
          free(waveform_tests->index[i]);
      if (waveform_tests->outOfRange)
        if (waveform_tests->outOfRange[i])
          free(waveform_tests->outOfRange[i]);
      if (waveform_tests->writeToFile)
        if (waveform_tests->writeToFile[i])
          free(waveform_tests->writeToFile[i]);
      if (waveform_tests->waveformData)
        if (waveform_tests->waveformData[i])
          free(waveform_tests->waveformData[i]);
      if (waveform_tests->min)
        if (waveform_tests->min[i])
          free(waveform_tests->min[i]);
      if (waveform_tests->max)
        if (waveform_tests->max[i])
          free(waveform_tests->max[i]);
      if (waveform_tests->ignore)
        if (waveform_tests->ignore[i])
          free(waveform_tests->ignore[i]);
      if (waveform_tests->sleep)
        if (waveform_tests->sleep[i])
          free(waveform_tests->sleep[i]);
      if (waveform_tests->holdOff)
        if (waveform_tests->holdOff[i])
          free(waveform_tests->holdOff[i]);
      if (waveform_tests->despike)
        if (waveform_tests->despike[i])
          free(waveform_tests->despike[i]);
      if (waveform_tests->despikeIndex)
        if (waveform_tests->despikeIndex[i])
          free(waveform_tests->despikeIndex[i]);
      if (waveform_tests->sleep)
        if (waveform_tests->sleep[i])
          free(waveform_tests->sleep[i]);
    }
    if (waveform_tests->ignore) {
      free(waveform_tests->ignore);
      waveform_tests->ignore = NULL;
    }
    if (waveform_tests->waveformPV) {
      free(waveform_tests->waveformPV);
      waveform_tests->waveformPV = NULL;
    }
    if (waveform_tests->waveformData) {
      free(waveform_tests->waveformData);
      waveform_tests->waveformData = NULL;
    }
    if (waveform_tests->channelInfo) {
      if (waveform_tests->channelInfo[0].count)
        free(waveform_tests->channelInfo[0].count);
      free(waveform_tests->channelInfo);
      waveform_tests->channelInfo = NULL;
    }
    if (waveform_tests->max) {
      free(waveform_tests->max);
      waveform_tests->max = NULL;
    }
    if (waveform_tests->min) {
      free(waveform_tests->min);
      waveform_tests->min = NULL;
    }
    if (waveform_tests->outOfRange) {
      free(waveform_tests->outOfRange);
      waveform_tests->outOfRange = NULL;
    }
    if (waveform_tests->index) {
      free(waveform_tests->index);
      waveform_tests->index = NULL;
    }
    if (waveform_tests->testFailed) {
      free(waveform_tests->testFailed);
      waveform_tests->testFailed = NULL;
    }
    if (waveform_tests->waveformLength) {
      free(waveform_tests->waveformLength);
      waveform_tests->waveformLength = NULL;
    }
    if (waveform_tests->sleep) {
      free(waveform_tests->sleep);
      waveform_tests->sleep = NULL;
    }
    if (waveform_tests->holdOff) {
      free(waveform_tests->holdOff);
      waveform_tests->holdOff = NULL;
    }
    if (waveform_tests->despike) {
      free(waveform_tests->despike);
      waveform_tests->despike = NULL;
    }
    if (waveform_tests->despikeIndex) {
      free(waveform_tests->despikeIndex);
      waveform_tests->despikeIndex = NULL;
    }
    if (waveform_tests->writeToFile) {
      free(waveform_tests->writeToFile);
      waveform_tests->writeToFile = NULL;
    }
    if (waveform_tests->despikeValues) {
      free(waveform_tests->despikeValues);
      waveform_tests->despikeValues = NULL;
    }
    if (waveform_tests->waveformSleep) {
      free(waveform_tests->waveformSleep);
      waveform_tests->waveformSleep = NULL;
    }
    if (waveform_tests->waveformHoldOff) {
      free(waveform_tests->waveformHoldOff);
      waveform_tests->waveformHoldOff = NULL;
    }
  }
  waveform_tests->waveformTests = 0;
}

long WriteWaveformData(WAVE_FORMS *controlWaveforms, CONTROL_NAME *control, double pendIOTime) {
  long i, n, length, j, k;
  char **PVs;
  CHANNEL_INFO *channelInfo;
  PVA_OVERALL *pva;

  length = 0;
  if (!controlWaveforms->waveforms)
    return 0;
  n = controlWaveforms->waveforms;
  PVs = controlWaveforms->writeWaveformPV;
  channelInfo = controlWaveforms->writeChannelInfo;
  pva = &controlWaveforms->pvaWriteChannel;

  for (i = 0; i < n; i++) {
    length = controlWaveforms->waveformLength[i];
    for (j = 0; j < length; j++) {
      k = controlWaveforms->readbackIndex[i][j];
      controlWaveforms->waveformData[i][j] = 0;
      if (k >= 0) {
        if (isnan(control->value[0][k])) {
          fprintf(stderr, "error: NaN found after matrix-readback multiplication (if no NaN found in readbacks which should be printed before this error message, check the matrix). Execution aborted.\n");
          FreeEverything();
          exit(1);
        }
        controlWaveforms->waveformData[i][j] = control->value[0][k];
      }
    }
  }
#if defined(NOCAPUT)
  return (0);
#endif
  *(channelInfo[0].count) = n;

  for (i = 0; i < n; i++) {
    channelInfo[i].flag = 0;
    if (pva->isConnected[i] == false) {
      pva->pvaData[i].skip = true;
      continue;
    }
    pva->pvaData[i].skip = false;
    switch (pva->pvaData[j].fieldType) {
    case epics::pvData::scalarArray: {
      pva->pvaData[i].numPutElements = controlWaveforms->waveformLength[i];
      if (pva->pvaData[i].numeric) {
        if (pva->pvaData[i].putData[0].values == NULL) {
          pva->pvaData[i].putData[0].values = (double *)malloc(sizeof(double) * pva->pvaData[i].numPutElements);
        }
        for (j = 0; j < pva->pvaData[i].numPutElements; j++) {
          pva->pvaData[i].putData[0].values[j] = controlWaveforms->waveformData[i][j];
        }
      } else {
        fprintf(stderr, "error: %s is not a numeric waveform PV\n", PVs[i]);
        FreeEverything();
        exit(1);
      }
      break;
    }
    default: {
      fprintf(stderr, "error: unexpected field type for %s\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    }
  }
  *(channelInfo[0].count) = 0;
  if (PutPVAValues(pva) == 1) {
    fprintf(stderr, "Error setting PVs\n");
    FreeEverything();
    exit(1);
  }

  return 0;
}

void CleanUpCorrections(CORRECTION *correction) {
  int i;
  if (correction->file) {
    if (correction->K->a) {
      for (i = 0; i < correction->K->m; i++) {
        if (correction->K->a[i])
          free(correction->K->a[i]);
      }
      free(correction->K->a);
      correction->K->a = NULL;
    }
    if (correction->actuator) {
      free(correction->actuator);
      correction->actuator = NULL;
    }
    if (correction->control->symbolicName) {
      for (i = 0; i < correction->control->n; i++) {
        if (correction->control->symbolicName[i])
          free(correction->control->symbolicName[i]);
        correction->control->symbolicName[i] = NULL;
      }
      free(correction->control->symbolicName);
      correction->control->symbolicName = NULL;
    }
    if (correction->readback->symbolicName) {
      for (i = 0; i < correction->readback->n; i++) {
        if (correction->readback->symbolicName[i])
          free(correction->readback->symbolicName[i]);
        correction->readback->symbolicName[i] = NULL;
      }
      free(correction->readback->symbolicName);
      correction->readback->symbolicName = NULL;
    }
    if (correction->readback->channelInfo) {
      if (correction->readback->channelInfo[0].count)
        free(correction->readback->channelInfo[0].count);
      free(correction->readback->channelInfo);
      freePVAGetReadings(&(correction->readback->pva));
      freePVA(&(correction->readback->pva));
    }
    if (correction->control->channelInfo) {
      if (correction->control->channelInfo[0].count)
        free(correction->control->channelInfo[0].count);
      free(correction->control->channelInfo);
      freePVAGetReadings(&(correction->control->pva));
      freePVA(&(correction->control->pva));
    }
    if (correction->control->setFlag)
      free(correction->control->setFlag);
    if (correction->definitionFile) {
      if (correction->control->controlName) {
        for (i = 0; i < correction->control->n; i++) {
          if (correction->control->controlName[i])
            free(correction->control->controlName[i]);
        }
        free(correction->control->controlName);
        correction->control->controlName = NULL;
      }
      if (correction->readback->controlName) {
        for (i = 0; i < correction->readback->n; i++) {
          if (correction->readback->controlName[i])
            free(correction->readback->controlName[i]);
        }
        free(correction->readback->controlName);
        correction->readback->controlName = NULL;
      }
      free(correction->definitionFile);
      correction->definitionFile = NULL;
    }
    if (correction->control->despike)
      free(correction->control->despike);
    if (correction->control->waveformIndex)
      free(correction->control->waveformIndex);
    if (correction->control->waveformMatchFound)
      free(correction->control->waveformMatchFound);
    if (correction->control->value) {
      for (i = 0; i < correction->control->valueIndexes; i++) {
        if (correction->control->value[i])
          free(correction->control->value[i]);
      }
      free(correction->control->value);
    }
    if (correction->control->setpoint)
      free(correction->control->setpoint);
    if (correction->control->error)
      free(correction->control->error);
    if (correction->control->initial)
      free(correction->control->initial);
    if (correction->control->old)
      free(correction->control->old);
    if (correction->control->delta) {
      for (i = 0; i < correction->control->valueIndexes; i++) {
        if (correction->control->delta[i])
          free(correction->control->delta[i]);
      }
      free(correction->control->delta);
    }
    if (correction->readback->despike)
      free(correction->readback->despike);
    if (correction->readback->waveformIndex)
      free(correction->readback->waveformIndex);
    if (correction->readback->waveformMatchFound)
      free(correction->readback->waveformMatchFound);
    if (correction->readback->value) {
      for (i = 0; i < correction->readback->valueIndexes; i++) {
        if (correction->readback->value[i])
          free(correction->readback->value[i]);
      }
      free(correction->readback->value);
    }
    if (correction->readback->setpoint)
      free(correction->readback->setpoint);
    if (correction->readback->error)
      free(correction->readback->error);
    if (correction->readback->initial)
      free(correction->readback->initial);
    if (correction->readback->old)
      free(correction->readback->old);
    if (correction->readback->delta) {
      for (i = 0; i < correction->readback->valueIndexes; i++) {
        if (correction->readback->delta[i])
          free(correction->readback->delta[i]);
      }
      free(correction->readback->delta);
    }
    free(correction->file);
    correction->file = NULL;
  }
  if (correction->coefFile) {
    if (correction->aCoef->a) {
      for (i = 0; i < correction->aCoef->n; i++)
        if (correction->aCoef->a[i])
          free(correction->aCoef->a[i]);
      free(correction->aCoef->a);
      correction->aCoef->a = NULL;
    }

    if (correction->bCoef->a) {
      for (i = 0; i < correction->bCoef->n; i++)
        if (correction->bCoef->a[i])
          free(correction->bCoef->a[i]);
      free(correction->bCoef->a);
      correction->bCoef->a = NULL;
    }

    if (correction->control->history->a) {
      for (i = 0; i < correction->control->history->n; i++)
        if (correction->control->history->a[i])
          free(correction->control->history->a[i]);
      free(correction->control->history->a);
      correction->control->history->a = NULL;
    }
    if (correction->control->historyFiltered->a) {
      for (i = 0; i < correction->control->historyFiltered->n; i++)
        if (correction->control->historyFiltered->a[i])
          free(correction->control->historyFiltered->a[i]);
      free(correction->control->historyFiltered->a);
      correction->control->historyFiltered->a = NULL;
    }
    free(correction->coefFile);
    correction->coefFile = NULL;
  }
  if (correction->control) {
    if (correction->control->historyFiltered) {
      free(correction->control->historyFiltered);
      correction->control->historyFiltered = NULL;
    }
    if (correction->control->history) {
      free(correction->control->history);
      correction->control->history = NULL;
    }
  }
  if (correction->aCoef) {
    free(correction->aCoef);
    correction->aCoef = NULL;
  }
  if (correction->bCoef) {
    free(correction->bCoef);
    correction->bCoef = NULL;
  }
  if (correction->readback) {
    free(correction->readback);
    correction->readback = NULL;
  }
  if (correction->control) {
    free(correction->control);
    correction->control = NULL;
  }
  if (correction->K) {
    free(correction->K);
    correction->K = NULL;
  }
}

void CleanUpOthers(DESPIKE_PARAM *despike, TESTS *tests, LOOP_PARAM *loopParam) {
  long i;

  if (loopParam->offsetFile) {
    free(loopParam->offsetFile);
    loopParam->offsetFile = NULL;
  }
  if (loopParam->searchPath) {
    free(loopParam->searchPath);
    loopParam->searchPath = NULL;
  }
  if (loopParam->gainPV) {
    free(loopParam->gainPV);
    free(loopParam->gainPVInfo.count);
    loopParam->gainPV = NULL;
  }
  if (loopParam->intervalPV) {
    free(loopParam->intervalPV);
    free(loopParam->intervalPVInfo.count);
    loopParam->intervalPV = NULL;
  }
  if (loopParam->averagePV) {
    free(loopParam->averagePV);
    free(loopParam->averagePVInfo.count);
    loopParam->averagePV = NULL;
  }
  if (loopParam->launcherPV[0]) {
    free(loopParam->launcherPV[0]);
    free(loopParam->launcherPV[1]);
    free(loopParam->launcherPV[2]);
    free(loopParam->launcherPV[3]);
    free(loopParam->launcherPV[4]);
    free(loopParam->launcherPVInfo[0].count);
    free(loopParam->launcherPVInfo[1].count);
    free(loopParam->launcherPVInfo[2].count);
    free(loopParam->launcherPVInfo[3].count);
    free(loopParam->launcherPVInfo[4].count);
    loopParam->launcherPV[0] = NULL;
  }
  if (loopParam->step) {
    free(loopParam->step);
    loopParam->step = NULL;
  }
  if (loopParam->epochTime) {
    free(loopParam->epochTime);
    loopParam->epochTime = NULL;
  }
  if (loopParam->elapsedTime) {
    free(loopParam->elapsedTime);
    loopParam->elapsedTime = NULL;
  }
  if (loopParam->offsetPVFile) {
    if (loopParam->offsetPV) {
      for (i = 0; i < loopParam->n; i++) {
        if (loopParam->offsetPV[i])
          free(loopParam->offsetPV[i]);
      }
      free(loopParam->offsetPV);
      loopParam->offsetPV = NULL;
    }
    if (loopParam->channelInfo) {
      free(loopParam->channelInfo[0].count);
      free(loopParam->channelInfo);
      loopParam->channelInfo = NULL;
      freePVAGetReadings(&(loopParam->pva));
      freePVA(&(loopParam->pva));
    }
    if (loopParam->offsetPVvalue) {
      free(loopParam->offsetPVvalue);
      loopParam->offsetPVvalue = NULL;
    }
    free(loopParam->offsetPVFile);
    loopParam->offsetPVFile = NULL;
  }
  if (loopParam->postChangeExec)
    free(loopParam->postChangeExec);
  if (despike->file) {
    if (despike->symbolicName && despike->symbolicName != despike->controlName) {
      for (i = 0; i < despike->n; i++)
        if (despike->symbolicName[i])
          free(despike->symbolicName[i]);
      free(despike->symbolicName);
      despike->symbolicName = NULL;
    }
    if (despike->controlName) {
      for (i = 0; i < despike->n; i++)
        if (despike->controlName[i])
          free(despike->controlName[i]);
      free(despike->controlName);
      despike->controlName = NULL;
    }
    if (despike->despike)
      free(despike->despike);
    if (despike->thresholdPV) {
      free(despike->thresholdPV);
      free(despike->thresholdPVInfo.count);
      despike->thresholdPV = NULL;
    }
    free(despike->file);
    despike->file = NULL;
  }
  if (tests->file) {
    if (tests->controlName) {
      for (i = 0; i < tests->n; i++)
        free(tests->controlName[i]);
      free(tests->controlName);
      tests->controlName = NULL;
    }
    if (tests->outOfRange)
      free(tests->outOfRange);
    if (tests->value)
      free(tests->value);
    if (tests->min)
      free(tests->min);
    if (tests->max)
      free(tests->max);
    if (tests->sleep)
      free(tests->sleep);
    if (tests->reset)
      free(tests->reset);
    if (tests->writeToFile)
      free(tests->writeToFile);
    if (tests->channelInfo) {
      if (tests->channelInfo[0].count)
        free(tests->channelInfo[0].count);
      free(tests->channelInfo);
    }
    if (tests->holdOff)
      free(tests->holdOff);
    if (tests->despike)
      free(tests->despike);
    if (tests->despikeIndex)
      free(tests->despikeIndex);
    if (tests->glitchLog)
      free(tests->glitchLog);
    free(tests->file);
    tests->file = NULL;
  }
}

void CleanUpLimits(LIMITS *limits) {
  long i;
  if (limits->file) {
    if (limits->controlName) {
      for (i = 0; i < limits->n; i++)
        if (limits->controlName[i])
          free(limits->controlName[i]);
      limits->n = 0;
      free(limits->controlName);
      limits->controlName = NULL;
    }
    if (limits->value) {
      free(limits->value);
      limits->value = NULL;
    }
    if (limits->minValue) {
      free(limits->minValue);
      limits->minValue = NULL;
    }
    if (limits->maxValue) {
      free(limits->maxValue);
      limits->maxValue = NULL;
    }
    if (limits->index) {
      free(limits->index);
      limits->index = NULL;
    }
  }
}

void SetupRawPVAConnection(char **PVname, CHANNEL_INFO *channelInfo, long n, double pendIOTime, PVA_OVERALL *pva) {
  long j, i;

  if (!channelInfo) {
    FreeEverything();
    SDDS_Bomb((char*)"Memory has not been allocated for channelID.\n");
  }
  for (i = 1; i < n; i++)
    channelInfo[i].count = channelInfo[0].count;
  *(channelInfo[0].count) = n;

  allocPVA(pva, n);
  epics::pvData::shared_vector<std::string> names(n);
  epics::pvData::shared_vector<std::string> providerNames(n);
  for (j = 0; j < n; j++) {
    if (strncmp(PVname[j], "pva://", 6) == 0) {
      PVname[j] += 6;
      names[j] = PVname[j];
      PVname[j] -= 6;
      providerNames[j] = "pva";
    } else if (strncmp(PVname[j], "ca://", 5) == 0) {
      PVname[j] += 5;
      names[j] = PVname[j];
      PVname[j] -= 5;
      providerNames[j] = "ca";
    } else {
      names[j] = PVname[j];
      providerNames[j] = "ca";
    }
  }
  pva->pvaChannelNames = freeze(names);
  pva->pvaProvider = freeze(providerNames);
  ConnectPVA(pva, pendIOTime);
  pva->limitGetReadings = true;
  if (pva->numNotConnected > 0) {
    for (j = 0; j < n; j++) {
      if (pva->isConnected[j] == false) {
        fprintf(stderr, "error: problem doing search for %s\n", PVname[j]);
      }
    }
    FreeEverything();
    exit(1);
  }

}

void FreeEverything(void) {
  int ca = 1;
  signal(SIGINT, interrupt_handler2);
  signal(SIGTERM, interrupt_handler2);
  signal(SIGILL, interrupt_handler2);
  signal(SIGABRT, interrupt_handler2);
  signal(SIGFPE, interrupt_handler2);
  signal(SIGSEGV, interrupt_handler2);
#ifndef _WIN32
  signal(SIGHUP, interrupt_handler2);
  signal(SIGQUIT, interrupt_handler2);
  signal(SIGTRAP, interrupt_handler2);
  signal(SIGBUS, interrupt_handler2);
#endif

  if (ca) {
    if ((sddscontrollawGlobal->rcParam.init)) {
      if ((sddscontrollawGlobal->rcParam.PV) && (sddscontrollawGlobal->rcParam.status != RUNCONTROL_DENIED)) {
#ifdef USE_LOGDAEMON
        if (sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
          logClose(sddscontrollawGlobal->logHandle);
        }
#endif
        strcpy(sddscontrollawGlobal->rcParam.message, "Application exited.");
        runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
        switch (runControlExit(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo))) {
        case RUNCONTROL_OK:
          break;
        case RUNCONTROL_ERROR:
          fprintf(stderr, "Error exiting run control.\n");
          break;
        default:
          fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
        }
      }
    }
    
  }
  CleanUpCorrections(&(sddscontrollawGlobal->correction));
  CleanUpCorrections(&(sddscontrollawGlobal->overlapCompensation));
  CleanUpOthers(&(sddscontrollawGlobal->despikeParam), &(sddscontrollawGlobal->test), &(sddscontrollawGlobal->loopParam));
  CleanUpLimits(&(sddscontrollawGlobal->delta));
  CleanUpLimits(&(sddscontrollawGlobal->action));
  CleanUpLimits(&(sddscontrollawGlobal->readbackLimits));

  cleanupWaveforms(&(sddscontrollawGlobal->readbackWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->controlWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->offsetWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->ffSetpointWaveforms));
  cleanupTestWaveforms(&(sddscontrollawGlobal->waveform_tests));

  if (sddscontrollawGlobal->outputFile) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->outputPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->outputFile);
    sddscontrollawGlobal->outputFile = NULL;
  }
  if (sddscontrollawGlobal->statsFile) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->statsPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->statsFile);
    sddscontrollawGlobal->statsFile = NULL;
  }
  if (sddscontrollawGlobal->glitchParam.file) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->glitchPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->glitchParam.file);
    sddscontrollawGlobal->glitchParam.file = NULL;
  }

  if (sddscontrollawGlobal->controlLogFile) {
    free(sddscontrollawGlobal->controlLogFile);
    sddscontrollawGlobal->controlLogFile = NULL;
  }

  free(sddscontrollawGlobal);
  sddscontrollawGlobal = NULL;
}

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
      if (runControlPingWhileSleep(0.0)) { //Ping the run control once every 5 seconds
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

long pvaEscapableThreadSleep(long double seconds, volatile int *flag) {
  struct timespec delayTime;
  struct timespec remainingTime;
  long double targetTime;

  if (seconds > 0) {
    targetTime = getLongDoubleTimeInSecs() + seconds;
    while (seconds > 5) {
      delayTime.tv_sec = 5;
      delayTime.tv_nsec = 0;
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        if (*flag)
          return (1);
        delayTime = remainingTime;
      }
      if (runControlPingWhileSleep(0.0)) { //Ping the run control once every 5 seconds
        return (1);
      }
      seconds = targetTime - getLongDoubleTimeInSecs();
    }

    delayTime.tv_sec = seconds;
    delayTime.tv_nsec = (seconds - delayTime.tv_sec) * 1e9;
    while (nanosleep(&delayTime, &remainingTime) == -1 && errno == EINTR) {
      if (*flag)
        return (1);
      delayTime = remainingTime;
    }
  }
  return (0);
}

