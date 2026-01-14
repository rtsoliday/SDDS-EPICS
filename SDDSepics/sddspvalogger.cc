
#include <complex>
#include <iostream>
#include <vector>
#include <string>
#include <cerrno>
#include <csignal>
#include <cctype>
#include <cstdlib>

#include "pvaSDDS.h"
#include "pv/thread.h"

#include "mdb.h"
#include "SDDS.h"
#include "scan.h"
#include "SDDSepics.h"
#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif
#ifdef _WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#endif

static int runCommandNoShell(const char *command) {
  if (!command) {
    return 1;
  }

#ifdef _WIN32
  int rc = system(command);
  if (rc != 0) {
    fprintf(stderr, "Warning (sddspvalogger): command failed (%d): %s\n", rc, command);
  }
  return rc;
#else
  std::vector<std::string> args;
  const char *p = command;

  while (*p) {
    while (*p && isspace((unsigned char)*p)) {
      p++;
    }
    if (!*p) {
      break;
    }

    std::string token;
    while (*p && !isspace((unsigned char)*p)) {
      if (*p == '\'' || *p == '"') {
        char quote = *p++;
        while (*p && *p != quote) {
          if (quote == '"' && *p == '\\' && p[1]) {
            p++;
          }
          token.push_back(*p++);
        }
        if (*p != quote) {
          fprintf(stderr, "Warning (sddspvalogger): unmatched quote in command: %s\n", command);
          return 1;
        }
        p++;
      } else if (*p == '\\' && p[1]) {
        p++;
        token.push_back(*p++);
      } else {
        token.push_back(*p++);
      }
    }

    if (!token.empty()) {
      args.push_back(token);
    }
  }

  if (args.empty()) {
    return 0;
  }

  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (size_t i = 0; i < args.size(); i++) {
    char *s = strdup(args[i].c_str());
    if (!s) {
      fprintf(stderr, "Warning (sddspvalogger): strdup failed running command: %s\n", command);
      for (size_t k = 0; k < argv.size(); k++) {
        free(argv[k]);
      }
      return 1;
    }
    argv.push_back(s);
  }
  argv.push_back(NULL);

  pid_t pid = fork();
  if (pid == 0) {
    execvp(argv[0], argv.data());
    _exit(127);
  }
  if (pid < 0) {
    fprintf(stderr, "Warning (sddspvalogger): fork failed running command: %s\n", command);
    for (size_t k = 0; k < argv.size(); k++) {
      free(argv[k]);
    }
    return 1;
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
    /* retry */
  }

  for (size_t k = 0; k < argv.size(); k++) {
    free(argv[k]);
  }

  if (WIFEXITED(status)) {
    int rc = WEXITSTATUS(status);
    if (rc != 0) {
      fprintf(stderr, "Warning (sddspvalogger): command failed (%d): %s\n", rc, command);
    }
    return rc;
  }
  if (WIFSIGNALED(status)) {
    fprintf(stderr, "Warning (sddspvalogger): command terminated by signal %d: %s\n", WTERMSIG(status), command);
    return 128 + WTERMSIG(status);
  }
  fprintf(stderr, "Warning (sddspvalogger): command ended unexpectedly: %s\n", command);
  return 1;
#endif
}

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

#define ONERROR_USEZERO 0
#define ONERROR_SKIP 1
#define ONERROR_EXIT 2
#define ONERROR_OPTIONS 3
static char *onerror_options[ONERROR_OPTIONS] = {
  (char *)"usezero",
  (char *)"skip",
  (char *)"exit"};

#define STROBE_NOTTIMEVALUE 0x0001U

#define DAILYFILES_VERBOSE 0x0001U
#define MONTHLYFILES_VERBOSE 0x0001U

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
  char *inputfile, *inputfileLastlink;
  struct stat filestat;
  char *outputfile, *outputfileOrig;
  int32_t *elementIndex;
  bool *verifiedType;
  bool scalarArraysAsColumns;
  bool scalarsAsColumns;
  double *emptyColumn;
  char **emptyStringColumn;
  int32_t n_rows;
  int32_t caErrorsIndex, timeIndex, caErrorsIndexP, timeIndexP, stepIndex, timeofdayIndex, dayofmonthIndex;
  int32_t triggerTimeIndex;
  long double currentTime;
  bool *expectNumeric;
  short *storageType;
  bool *expectScalar;
  bool *expectScalarArray;
  bool *treatScalarArrayAsScalar;
  bool strictPVverification;
  bool truncateWaveforms;
  int32_t *expectElements;
  bool watchInput;
  bool monitor;
  bool monitorRandomTimedTrigger;
  bool append;
  bool overwrite;
  int32_t pvCount;
  char **controlName, **readbackName, **provider, **units;
  double *scaleFactor;
  int32_t *scalarArrayStartIndex;
  int32_t *scalarArrayEndIndex;
  char *average;
  int64_t *outputRow, *outputPage;
  double sampleInterval;
  long logInterval;
  long flushInterval;
  char *datastrobe_name;
  char *datastrobe_provider;
  bool datastrobe_is_time;
  double datastrobe_holdoff;
  char *CondFile;
  unsigned long CondMode;
  bool conditions_ScaleFactorDefined;
  bool conditions_HoldoffTimeDefined;
  int32_t conditions_pvCount;
  char **conditions_controlName;
  char **conditions_provider;
  double *conditions_lowerLimit;
  double *conditions_upperLimit;
  double *conditions_scaleFactor;
  double *conditions_holdoffTime;
  long onerrorindex;
  char *onerror;
  char *inhibit_name;
  char *inhibit_provider;
  double inhibit_waittime;
  double *averagedValues;
  int32_t step;
  double StartTime, StartDay, StartHour, StartJulianDay, StartMonth, StartYear;
  char *TimeStamp;
  bool generations, dailyFiles, monthlyFiles;
  int32_t generationsDigits, generationsRowLimit;
  char *generationsDelimiter;
  double generationsTimeLimit, generationsTimeLimitOrig;
  bool timetag, dailyFilesVerbose, monthlyFilesVerbose;
  bool verbose;
  char *onePv_OutputDirectory;
  char **onePv_outputfile, **onePv_outputfileOrig;
  bool mallocNeeded;
  bool doDisconnect;
  long Nsteps;
  bool steps_set;
  bool totalTimeSet;
  double TotalTime;
  double pendIOtime;
  double filesPerStep;
  long NstepsAdjusted;
  char *triggerFile, *triggerFileLastlink;
  bool trigFileSet;
  struct stat triggerfilestat;
  long circularbuffer_before;
  long circularbuffer_after;
  double ***circularbufferDouble;
  long double *circularbufferLongDouble;
  char ****circularbufferString;
  long circularbuffer_length;
  long circularbuffer_index;
  int delay;
  double defaultHoldoffTime;
  bool autoHoldoff;
  bool glitch_MajorAlarmDefined;
  bool glitch_MinorAlarmDefined;
  bool glitch_NoAlarmDefined;
  bool glitch_GlitchThresholdDefined;
  bool glitch_GlitchBaselineSamplesDefined;
  bool glitch_GlitchBaselineAutoResetDefined;
  bool glitch_GlitchScriptDefined;
  bool glitch_TransitionThresholdDefined;
  bool glitch_TransitionDirectionDefined;
  int32_t glitch_pvCount;
  char **glitch_controlName;
  char **glitch_provider;
  short *glitch_majorAlarm;
  short *glitch_minorAlarm;
  short *glitch_noAlarm;
  short *glitch_alarmLastValue;
  double *glitch_glitchThreshold;
  int32_t *glitch_glitchBaselineSamples;
  short *glitch_glitchBaselineAutoReset;
  long *glitch_glitchBaselineSamplesRead;
  double **glitch_glitchBaselineSampleValues;
  char **glitch_glitchScript;
  double *glitch_transitionThreshold;
  short *glitch_transitionDirection;
  double *glitch_transitionLastValue;
  int32_t glitch_step;
  long double glitch_time;
  int32_t *glitch_triggerIndex;
  int32_t *glitch_alarmIndex;
  int32_t *glitch_alarmSeverityIndex;
  int32_t *glitch_glitchIndex;
  short *glitch_triggered, *glitch_triggeredTemp;
  short *glitch_alarmed, *glitch_alarmedTemp;
  short *glitch_alarmSeverity, *glitch_alarmSeverityTemp;
  short *glitch_glitched, *glitch_glitchedTemp;
  double DayNow, LastDay, HourNow, LastHour;
} LOGGER_DATA;

long WriteHeaders(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long WriteAccessoryData(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long WriteData(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long pvaThreadSleep(long double seconds);
long pvaThreadSleepWithPolling(PVA_OVERALL **pva, long count, long double targetTime);
long pvaThreadSleepWithPollingAndDataStrobe(PVA_OVERALL **pva, long count, PVA_OVERALL *pvaTrig, bool randomTime, double hold_off);
long VerifyPVTypes(PVA_OVERALL *pva, LOGGER_DATA *logger);
long ReadInputFiles(LOGGER_DATA *logger);
long ReadConditionsFile(LOGGER_DATA *logger);
long ReadGlitchTriggerFile(LOGGER_DATA *logger);
long VerifyFileIsAppendable(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long VerifyNumericScalarPVTypes(PVA_OVERALL *pva);
bool CheckGlitchPVA(PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger);
bool PassesConditionsPVA(PVA_OVERALL *pvaConditions, LOGGER_DATA *logger);
bool PassesInhibitPVA(PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger);
void AverageData(PVA_OVERALL *pva, LOGGER_DATA *logger);
long StartPages(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long UpdateAndWritePages(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long CloseFiles(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long InitiateRunControl();
long PingRunControl();
long PingRunControl(PVA_OVERALL *pva, long mode, long step);
void InitializeVariables(LOGGER_DATA *logger);
long ReadCommandLineArgs(LOGGER_DATA *logger, int argc, SCANNED_ARG *s_arg);
long CheckCommandLineArgsValidity(LOGGER_DATA *logger);
long CheckInputFileValidity(LOGGER_DATA *logger);
long SetupInhibitPV(PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger);
long SetupPVs(PVA_OVERALL *pva, PVA_OVERALL *pvaConditions, PVA_OVERALL *pvaTrigger, PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger);
long GenerationsCheck(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long CheckInhibitPV(SDDS_TABLE *SDDS_table, PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger);
void freeLogger(LOGGER_DATA *logger);
long WatchInput(LOGGER_DATA *logger);
void AllocateCircularBuffers(PVA_OVERALL *pva, LOGGER_DATA *logger);
void FreeCircularBuffers(LOGGER_DATA *logger);
void StoreDataIntoCircularBuffers(PVA_OVERALL *pva, LOGGER_DATA *logger);
long WriteGlitchPage(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger);
long GlitchLogicRoutines(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger);

void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

static volatile sig_atomic_t sigint = 0;
static volatile sig_atomic_t sigintSignal = 0;

#define CLO_SAMPLEINTERVAL 0
#define CLO_LOGINTERVAL 1
#define CLO_FLUSHINTERVAL 2
#define CLO_STEPS 3
#define CLO_TIME 4
#define CLO_PENDIOTIME 5
#define CLO_VERBOSE 6
#define CLO_WATCHINPUT 7
#define CLO_MONITOR 8
#define CLO_APPEND 9
#define CLO_OVERWRITE 10
#define CLO_DATASTROBEPV 11
#define CLO_CONDITIONS 12
#define CLO_ONERROR 13
#define CLO_INHIBIT 14
#define CLO_GENERATIONS 15
#define CLO_DAILY_FILES 16
#define CLO_MONTHLY_FILES 17
#define CLO_ONE_PV_PER_FILE 18
#define CLO_RUNCONTROLPV 19
#define CLO_RUNCONTROLDESC 20
#define CLO_TRIGGERFILE 21
#define CLO_CIRCULARBUFFER 22
#define CLO_DELAY 23
#define CLO_HOLDOFFTIME 24
#define CLO_AUTOHOLDOFF 25
#define CLO_STRICTPVVERIFICATION 26
#define CLO_TRUNCATEWAVEFORMS 27
#define COMMANDLINE_OPTIONS 28
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"sampleInterval",
  (char *)"logInterval",
  (char *)"flushInterval",
  (char *)"steps",
  (char *)"time",
  (char *)"pendiotime",
  (char *)"verbose",
  (char *)"watchinput",
  (char *)"monitorMode",
  (char *)"append",
  (char *)"overwrite",
  (char *)"datastrobepv",
  (char *)"conditions",
  (char *)"onerror",
  (char *)"inhibitpv",
  (char *)"generations",
  (char *)"dailyfiles",
  (char *)"monthlyfiles",
  (char *)"onepvperfile",
  (char *)"runControlPV",
  (char *)"runControlDescription",
  (char *)"triggerFile",
  (char *)"circularBuffer",
  (char *)"delay",
  (char *)"holdoffTime",
  (char *)"autoHoldoff",
  (char *)"strictPVverification",
  (char *)"truncateWaveforms",
};

static char *USAGE = (char *)"sddspvalogger <inputfile> <outputfile> \n\
Global Options:\n\
  [-pendIOtime=<value>]\n\
  [-watchInput]\n\
  [-monitorMode=[randomTimedTrigger]]\n\
  [-append | -overwrite]\n\
  [-sampleInterval=<real-value>[,<time-units>]]\n\
  [-dataStrobePV=<PVname>,<provider>[,notTimeValue][,holdoff=<seconds>]]\n\
  [-steps=<integer-value>]\n\
  [-time=<real-value>[,<time-units>]]\n\
  [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
  [-onerror={usezero|skip|exit}]\n\
  [-inhibitPV=name=<name>,provider=<provider>[,waitTime=<seconds>]]\n\
  [-generations[=digits=<integer>][,delimiter=<string>][,rowlimit=<number>][,timelimit=<secs>]]\n\
  [-dailyFiles[=rowlimit=<number>][,timelimit=<secs>][,timetag][,verbose]]\n\
  [-monthlyFiles[=rowlimit=<number>][,timelimit=<secs>][,timetag][,verbose]]\n\
  [-strictPVverification]\n\
  [-truncateWaveforms]\n\
  [-runControlPV=string=<string>,pingTimeout=<value>]\n\
  [-runControlDescription=string=<string>]\n\
  [-verbose]\n\
Logger Options:\n\
  [-logInterval=<integer-value>]\n\
  [-flushInterval=<integer-value>]\n\
  [-onePvPerFile=<dirName>]\n\
Glitch Logger Options:\n\
  [-triggerFile=<filename>]\n\
  [-circularBuffer=[before=<number>,][after=<number>]]\n\
  [-delay=<steps>] [NOT YET IMPLEMENTED]\n\
  [-holdoffTime=<seconds>]\n\
  [-autoHoldoff]\n\
\n\
It connects to PVs over the EPICS7 PVA and CA protocols and stores the data in an sdds file.\n\n\
inputfile        The file must contain the columns:\n\
                   ControlName (string type with PV names)\n\
                   Provider (string type with \"pva\" or \"ca\" values)\n\
                   ExpectNumeric (character type with 'y' or 'n' values)\n\
                   StorageType (optional, string type with 'double', 'long', 'short', 'string' and other valid data types)\n\
                   ExpectFieldType (string type with 'scalar' or 'scalarArray' values),\n\
                   ExpectElements (long type with scalar array length)\n\
                   ReadbackName (optional, string type)\n\
                   ScaleFactor (optional, numeric type)\n\
                   ScalarArrayStartIndex (optional, integer type, default=1)\n\
                   ScalarArrayEndIndex (optional, integer type)\n\
                   Average (optional, character type with 'y' or 'n' values, used with the -logInterval option)\n\
                   Units (optional, string type)\n\
conditions file  The file must contain the columns:\n\
                   ControlName (string type with PV names)\n\
                   Provider (string type with \"pva\" or \"ca\" values)\n\
                   LowerLimit (numeric type)\n\
                   UpperLimit (numeric type)\n\
                   ScaleFactor (optional, numeric type)\n\
                   HoldoffTime (optional, numeric type)\n\
trigger file     The file must contain the columns:\n\
                   TriggerControlName (string type with PV names)\n\
                   Provider (string type with \"pva\" or \"ca\" values)\n\
                   MajorAlarm (optional, integer type with '1' or '0' values)\n\
                   MinorAlarm (optional, integer type with '1' or '0' values)\n\
                   NoAlarm (optional, integer type with '1' or '0' values)\n\
                   GlitchThreshold (optional, numeric type)\n\
                     0=ignore, >0=absolute, <0=-1*(fractional glitch level)\n\
                   GlitchBaselineSamples (optional, integer type)\n\
                   GlitchBaselineAutoReset (optional, integer type with '1' or '0' values)\n\
                   GlitchScript (optional, string type)\n\
                   TransitionThreshold (optional, numeric type)\n\
                   TransitionDirection (optional, integer type)\n\
                     -1=downward, 0=ignore, 1=upward\n";

typedef struct
{
  char *PV, *Desc;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;
RUNCONTROL_PARAM rcParam;

int main(int argc, char *argv[]) {
  SCANNED_ARG *s_arg;
  long j;
  long double targetTime = 0, timeToWait = 0;
  SDDS_TABLE *SDDS_table;
  PVA_OVERALL pva, pva2, pva3, pva4, pva5;
  PVA_OVERALL *pvaConditions = NULL, *pvaInhibit = NULL, *pvaStrobe = NULL, *pvaGlitch = NULL;
  PVA_OVERALL **pvaArray;
  LOGGER_DATA logger;

  //Initialize variable values
  InitializeVariables(&logger);

  //Read command line arguments
  argc = scanargs(&s_arg, argc, argv);
  if (ReadCommandLineArgs(&logger, argc, s_arg) != 0) {
    return (1);
  }

  //Setup various PVA group pointers
  if (logger.CondFile) {
    pvaConditions = &pva2;
  }
  if (logger.inhibit_name) {
    pvaInhibit = &pva3;
  }
  if (logger.datastrobe_name != NULL) {
    pvaStrobe = &pva4;
  }
  if (logger.trigFileSet) {
    pvaGlitch = &pva5;
  }

  //Initiate run control
  if (InitiateRunControl() != 0) {
    return (1);
  }

  //Setup connection to inhibit PV
  if (SetupInhibitPV(pvaInhibit, &logger) == 1) {
    return (1);
  }

  //Read input files
  if (ReadInputFiles(&logger) == 1) {
    return (1);
  }

  //Setup connections to all other PVs (also pings run control)
  if (SetupPVs(&pva, pvaConditions, pvaStrobe, pvaGlitch, &logger) == 1) {
    return (1);
  }

  //Enable extra data needed for glitch logic
  if (pvaGlitch != NULL) {
    pvaGlitch->includeAlarmSeverity = true;
  }

  //Get Initial Time and Date
  getTimeBreakdown(&(logger.StartTime), &(logger.StartDay), &(logger.StartHour), &(logger.StartJulianDay),
                   &(logger.StartMonth), &(logger.StartYear), &(logger.TimeStamp));

  //Get PV values
  if (logger.verbose) {
    fprintf(stdout, "Getting PV values\n");
  }
  pvaArray = (PVA_OVERALL **)malloc(sizeof(PVA_OVERALL *) * 4);
  pvaArray[0] = &pva;
  pvaArray[1] = pvaStrobe;
  pvaArray[2] = pvaConditions;
  pvaArray[3] = pvaGlitch;
  if (GetPVAValues(pvaArray, 4) == 1) {
    return (1);
  }

  //Get PV units
  if (ExtractPVAUnits(&pva) == 1) {
    return (1);
  }

  //Ping run control if in use
  if (PingRunControl() != 0) {
    return (1);
  }

  //Verify PVs are what we expected them to be
  logger.verifiedType = (bool *)malloc(sizeof(bool) * pva.numPVs);
  for (j = 0; j < pva.numPVs; j++) {
    logger.verifiedType[j] = false;
  }
  if ((VerifyPVTypes(&pva, &logger) == 1) ||
      (VerifyNumericScalarPVTypes(pvaConditions) == 1) ||
      (VerifyNumericScalarPVTypes(pvaStrobe) == 1) ||
      (VerifyNumericScalarPVTypes(pvaGlitch) == 1)) {
    return (1);
  }

  //Setup circular buffer (glitch capture) only after PV verification has finalized expectElements.
  if (pvaGlitch != NULL) {
    AllocateCircularBuffers(&pva, &logger);
  }

  //Allocate SDDS_table pointers
  if (logger.onePv_OutputDirectory != NULL) {
    SDDS_table = (SDDS_TABLE *)malloc(sizeof(SDDS_TABLE) * pva.numPVs);
    if (pva.numPVs > 40) {
      logger.doDisconnect = true;
    }
    logger.filesPerStep = 1.0 * pva.numPVs / logger.flushInterval;
    if (logger.verbose) {
      fprintf(stdout, "Will write %ld to %ld files each sample interval\n", (long)(logger.filesPerStep), (long)(logger.filesPerStep) + 1);
    }
  } else {
    SDDS_table = (SDDS_TABLE *)malloc(sizeof(SDDS_TABLE));
  }

  //Define Columns and Parameters (also pings run control)
  if ((logger.verbose) && (logger.onePv_OutputDirectory != NULL)) {
    fprintf(stdout, "Opening output files\n");
  }
  if (WriteHeaders(SDDS_table, &pva, &logger)) {
    return (1);
  }

  if ((logger.verbose) && (logger.onePv_OutputDirectory != NULL)) {
    fprintf(stdout, "Starting logging\n");
  }

  //Setup empty column data in case scalarArrays are disconnected
  logger.emptyColumn = (double *)malloc(sizeof(double) * logger.n_rows);
  logger.emptyStringColumn = (char **)malloc(sizeof(char *) * logger.n_rows);
  for (j = 0; j < logger.n_rows; j++) {
    logger.emptyStringColumn[j] = (char *)malloc(sizeof(char));
    logger.emptyStringColumn[j][0] = 0;
    logger.emptyColumn[j] = 0;
  }

  //Setup monitoring
  if (logger.monitor) {
    if ((MonitorPVAValues(&pva) == 1) ||
        (MonitorPVAValues(pvaConditions) == 1) ||
        (MonitorPVAValues(pvaGlitch) == 1)) {
      return (1);
    }
    if (pvaThreadSleep(.1) == 1) {
      return (1);
    }
    pvaArray[0] = &pva;
    pvaArray[1] = pvaConditions;
    pvaArray[2] = pvaGlitch;
    pvaArray[3] = NULL;
    if (PollMonitoredPVA(pvaArray, 3) == -1) {
      return (1);
    }
  }

  //Setup monitor for strobe trigger PV
  if (pvaStrobe != NULL) {
    if (MonitorPVAValues(pvaStrobe) == 1) {
      return (1);
    }
    if (pvaThreadSleep(.1) == 1) {
      return (1);
    }
    if (PollMonitoredPVA(pvaStrobe) == -1) {
      return (1);
    }
  } else {
    //Set first step to occur in 1 second
    logger.currentTime = getLongDoubleTimeInSecs();
    targetTime = logger.currentTime + logger.sampleInterval;
  }

  pvaArray[0] = &pva;
  pvaArray[1] = pvaConditions;
  pvaArray[2] = pvaGlitch;
  pvaArray[3] = NULL;

  logger.NstepsAdjusted = logger.Nsteps;

  //Main loop
  for (logger.step = 0; logger.step < logger.Nsteps && logger.step != logger.NstepsAdjusted; logger.step++) {
    if (sigint) {
      break;
    }
    //Ping run control if in use
    if (PingRunControl() != 0) {
      break;
    }

    //Check to see if we should close the generations, daily or monthly file and start a new one
    if (GenerationsCheck(SDDS_table, &pva, &logger) == 1) {
      return (1);
    }

    //Check inhibit PV
    j = CheckInhibitPV(SDDS_table, pvaInhibit, &logger);
    if (j == -1) {
      CloseFiles(SDDS_table, &pva, &logger);
      return (1);
    } else if (j == 1) {
      continue;
    }

    //Get the PV values
    if (logger.monitor) {
      if (logger.verbose) {
        fprintf(stdout, "Monitoring PV values for step %ld\n", (long)(logger.step + 1));
      }
      if (pvaStrobe != NULL) {
        if (pvaThreadSleepWithPollingAndDataStrobe(pvaArray, 3, pvaStrobe, logger.monitorRandomTimedTrigger, logger.datastrobe_holdoff) != 0) //Pings run control every 5 seconds
        {
          if (sigint) {
            break;
          }
          CloseFiles(SDDS_table, &pva, &logger);
          return (1);
        }
      } else {
        timeToWait = targetTime - getLongDoubleTimeInSecs();
        if (pvaThreadSleepWithPolling(pvaArray, 3, targetTime) != 0) //Pings run control every 5 seconds
        {
          if (sigint) {
            break;
          }
          CloseFiles(SDDS_table, &pva, &logger);
          return (1);
        }
      }
    } else {
      freePVAGetReadings(&pva);
      freePVAGetReadings(pvaConditions);
      freePVAGetReadings(pvaGlitch);
      if (logger.verbose) {
        fprintf(stdout, "Getting PV values for step %ld\n", (long)(logger.step + 1));
      }
      if (pvaStrobe != NULL) {
        if (WaitEventMonitoredPVA(pvaStrobe, 0, INT_MAX) == 1) //FIX this it should be pinging the run control
        {
          CloseFiles(SDDS_table, &pva, &logger);
          return (1);
        }
        if (logger.datastrobe_holdoff > 1e-7) {
          if (pvaThreadSleep(logger.datastrobe_holdoff) == 1) {
            if (sigint) {
              break;
            }
            CloseFiles(SDDS_table, &pva, &logger);
            return (1);
          }
        }
      } else {
        timeToWait = targetTime - getLongDoubleTimeInSecs();
        if (pvaThreadSleep(timeToWait) == 1) {
          if (sigint) {
            break;
          }
          CloseFiles(SDDS_table, &pva, &logger);
          return (1);
        }
      }

      if (GetPVAValues(pvaArray, 3) == 1) {
        CloseFiles(SDDS_table, &pva, &logger);
        return (1);
      }
    }

    //Time column represents time after get command completed, not before. Unless we are
    //using the data strobe feature.
    logger.currentTime = getLongDoubleTimeInSecs();
    if (pvaStrobe != NULL) {
      if (logger.datastrobe_is_time)
        logger.currentTime = pvaStrobe->pvaData[0].monitorData[0].values[0];
    } else {
      //Calculate when next step should be
      if (timeToWait > 0) {
        targetTime = targetTime + logger.sampleInterval;
      } else {
        //There must have been a problem collecting data on time. Resetting interval to
        //the current time.
        targetTime = logger.currentTime + logger.sampleInterval;
      }
    }

    //Verify PV types that may have just connected for the first time
    if (VerifyPVTypes(&pva, &logger) == 1) {
      return (1);
    }

    //Check to see if we have run out of time
    if (logger.totalTimeSet) {
      if ((logger.currentTime - logger.StartTime) > logger.TotalTime) {
        break;
      }
    }

    //Check input file for changes
    if (WatchInput(&logger) == 1) {
      break;
    }

    //Check to see if we should exit or skip due to connection errors
    if ((logger.onerrorindex == ONERROR_EXIT) || (logger.onerrorindex == ONERROR_SKIP)) {
      if ((pva.numNotConnected > 0) ||
          ((pvaConditions != NULL) && (pvaConditions->numNotConnected)) ||
          ((pvaGlitch != NULL) && (pvaGlitch->numNotConnected)) ||
          ((pvaStrobe != NULL) && (pvaStrobe->numNotConnected))) {
        if (logger.onerrorindex == ONERROR_EXIT) {
          fprintf(stdout, "Exiting due to PV connection error.\n");
          return (1);
        } else {
          if (logger.verbose) {
            fprintf(stdout, "Skipping due to connection error.\n");
          }
          continue;
        }
      }
    }

    //Check to see if we pass the conditions test
    if (PassesConditionsPVA(pvaConditions, &logger) == false) {
      continue;
    }

    //Check for glitches and write glitch files if needed
    j = GlitchLogicRoutines(SDDS_table, &pva, pvaGlitch, &logger);
    if (j == 1) {
      return (1);
    } else if (j == 2) {
      continue;
    }

    //Start pages if needed
    if (StartPages(SDDS_table, &pva, &logger) == 1) {
      return (1);
    }

    //Average the data if needed
    if ((logger.logInterval > 1) && (logger.average != NULL) && (logger.onePv_OutputDirectory == NULL)) {
      AverageData(&pva, &logger); //FIX THIS for enum values
    }

    if ((logger.step + 1) % logger.logInterval == 0) //FIX THIS check that the loginterval option really works
    {
      //Write column and parameter data
      if (WriteData(SDDS_table, &pva, &logger)) {
        return (1);
      }
    }

    //Update and write pages if needed
    if (UpdateAndWritePages(SDDS_table, &pva, &logger) == 1) {
      return (1);
    }
  } //End Main Loop

  //Close SDDS file
  if ((logger.verbose) && (logger.onePv_OutputDirectory == NULL)) {
    fprintf(stdout, "Data written to %s\n", logger.outputfile);
  }
  if (CloseFiles(SDDS_table, &pva, &logger) == 1) {
    return (1);
  }

  //Free memory
  free(SDDS_table);
  free(pvaArray);
  if (pvaGlitch != NULL) {
    FreeCircularBuffers(&logger);
  }
  freePVA(&pva);
  freePVA(pvaConditions);
  freePVA(pvaStrobe);
  freePVA(pvaInhibit);
  freePVA(pvaGlitch);
  freeLogger(&logger);
  free_scanargs(&s_arg, argc);
  return (0);
}

long WriteHeaders(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j, len;
  int sddstype;
  char *units = NULL, buffer[1024], descrip[1024];
  SDDS_TABLE *sdds;

  if (logger->onePv_OutputDirectory != NULL) {
#if !defined(_WIN32)
    if (logger->mallocNeeded) {
      logger->onePv_outputfileOrig = (char **)malloc(sizeof(char *) * pva->numPVs);
      logger->onePv_outputfile = (char **)malloc(sizeof(char *) * pva->numPVs);
      logger->elementIndex = (int32_t *)malloc(sizeof(int32_t) * pva->numPVs);
      logger->outputRow = (int64_t *)malloc(sizeof(int64_t) * pva->numPVs);
      logger->outputPage = (int64_t *)malloc(sizeof(int64_t) * pva->numPVs);
    }
    logger->n_rows = 1; //The row count is needed for the empty data in the event a PV is disconnected.
    for (j = 0; j < pva->numPVs; j++) {
      if (j % 100 == 0) {
        //Ping run control if in use
        if (PingRunControl(pva, 1, j) != 0) {
          return (1);
        }
      }

      sdds = &(SDDS_table[j]);
      logger->outputRow[j] = 0;
      logger->outputPage[j] = -1;
      if (logger->mallocNeeded) {
        replace_string(buffer, logger->readbackName[j], (char *)"/", (char *)"+");
        len = strlen(logger->onePv_OutputDirectory) + strlen(buffer) + 6;
        logger->onePv_outputfileOrig[j] = (char *)malloc(sizeof(char) * len);
        snprintf(logger->onePv_outputfileOrig[j], len, "%s/%s", logger->onePv_OutputDirectory, buffer);
        if (access(logger->onePv_outputfileOrig[j], F_OK) != 0) {
          mode_t mode;
          mode = umask(0);
          umask(mode);
          mode = (0777 & ~mode) | S_IRUSR | S_IWUSR | S_IXUSR;
          if (mkdir(logger->onePv_outputfileOrig[j], mode) != 0) {
            fprintf(stderr, "Unable to create directory %s\n", logger->onePv_outputfileOrig[j]);
            return (1);
          }
        }
        snprintf(logger->onePv_outputfileOrig[j], len, "%s/%s/log", logger->onePv_OutputDirectory, buffer);
      }
      if (logger->generations) {
        if (logger->mallocNeeded == false) {
          free(logger->onePv_outputfile[j]);
        }
        logger->onePv_outputfile[j] = MakeGenerationFilename(logger->onePv_outputfileOrig[j], logger->generationsDigits, logger->generationsDelimiter, NULL);
      } else if (logger->dailyFiles) {
        if (logger->mallocNeeded == false) {
          free(logger->onePv_outputfile[j]);
        }
        logger->onePv_outputfile[j] = MakeDailyGenerationFilename(logger->onePv_outputfileOrig[j], logger->generationsDigits, logger->generationsDelimiter, logger->timetag);
      } else if (logger->monthlyFiles) {
        if (logger->mallocNeeded == false) {
          free(logger->onePv_outputfile[j]);
        }
        logger->onePv_outputfile[j] = MakeMonthlyGenerationFilename(logger->onePv_outputfileOrig[j], logger->generationsDigits, logger->generationsDelimiter, logger->timetag);
      } else {
        SDDS_CopyString(&(logger->onePv_outputfile[j]), logger->onePv_outputfileOrig[j]);
      }
      makeTimeBreakdown(getTimeInSecs(), NULL, &(logger->DayNow), &(logger->HourNow), NULL, NULL, NULL, NULL);

      if (logger->expectScalarArray[j] && !logger->treatScalarArrayAsScalar[j]) {
        logger->scalarsAsColumns = false;
        logger->scalarArraysAsColumns = true;
        if (logger->expectElements[j] > logger->n_rows) {
          if ((logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
            logger->n_rows = logger->scalarArrayEndIndex[j] - logger->scalarArrayStartIndex[j] + 1;
          } else {
            logger->n_rows = logger->expectElements[j];
          }
        }
      } else {
        logger->scalarsAsColumns = true;
        logger->scalarArraysAsColumns = false;
        if (logger->flushInterval > logger->n_rows) {
          logger->n_rows = logger->flushInterval;
        }
      }

      if (logger->overwrite == false) {
        /*Check if output file already exists */
        if (fexists(logger->onePv_outputfile[j])) {
          fprintf(stderr, "error: File %s already exists. Make sure you use -generations, -dailyFiles or -monthlyFiles when using the -onePvPerFile option.\n", logger->onePv_outputfile[j]);
          return (1);
        }
      }

      //Open SDDS file
      if (!SDDS_InitializeOutput(sdds, SDDS_BINARY, 1, NULL, NULL, logger->onePv_outputfile[j])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      SDDS_EnableFSync(sdds);

      if (logger->scalarsAsColumns) {
        if (!SDDS_SetRowCountMode(sdds, SDDS_FIXEDROWCOUNT)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
      if (logger->scalarsAsColumns) {
        logger->caErrorsIndex = SDDS_DefineColumn(sdds, "CAerrors", NULL, NULL, "Channel access errors for this row", NULL, SDDS_LONG, 0);
        logger->timeIndex = SDDS_DefineColumn(sdds, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0);
        if ((logger->caErrorsIndex < 0) || (logger->timeIndex < 0)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      } else if (logger->scalarArraysAsColumns) {
        logger->caErrorsIndexP = SDDS_DefineParameter(sdds, "CAerrors", NULL, NULL, "Channel access errors for this row", NULL, SDDS_LONG, NULL);
        logger->timeIndexP = SDDS_DefineParameter(sdds, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL);
        if ((logger->caErrorsIndexP < 0) || (logger->timeIndexP < 0)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }

      if (logger->expectNumeric[j]) {
        sddstype = SDDS_DOUBLE;
      } else {
        sddstype = SDDS_STRING;
      }
      if (logger->storageType) {
        if (logger->storageType[j] != 0) {
          sddstype = logger->storageType[j];
        }
      }
      if (logger->units) {
        units = logger->units[j];
        if (strlen(units) == 0) {
          units = pva->pvaData[j].units;
        }
      } else {
        units = pva->pvaData[j].units;
      }
      if (logger->scalarsAsColumns) {
        logger->elementIndex[j] = SDDS_DefineColumn(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, 0);
      } else if (logger->scalarArraysAsColumns) {
        logger->elementIndex[j] = SDDS_DefineColumn(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, 0);
      }
      if (logger->elementIndex[j] < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }

      if (logger->scalarArraysAsColumns) {
        //sdds->layout.data_mode.column_major = 1;
      }

      //Save header
      if (!SDDS_SaveLayout(sdds)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      if (!SDDS_WriteLayout(sdds)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      if (logger->scalarsAsColumns) {
        if (!SDDS_StartPage(sdds, logger->flushInterval)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        logger->outputRow[j] = 0;
        logger->outputPage[j]++;
      }
      if (logger->doDisconnect && !SDDS_DisconnectFile(sdds)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    }
    logger->mallocNeeded = false;
    return (0);
#endif
  }

  sdds = &(SDDS_table[0]);
  //Checking to see if we should use SDDS_COLUMNS or SDDS_ARRAYS
  for (j = 0; j < pva->numPVs; j++) {
    // Skip scalarArrays that are being treated as scalars
    if (logger->expectScalarArray[j] && !logger->treatScalarArrayAsScalar[j]) {
      if ((logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
        if (logger->n_rows == -1) {
          logger->n_rows = logger->scalarArrayEndIndex[j] - logger->scalarArrayStartIndex[j] + 1;
        } else if (logger->n_rows != (logger->scalarArrayEndIndex[j] - logger->scalarArrayStartIndex[j] + 1)) {
          logger->scalarArraysAsColumns = false;
        }
      } else {
        if (logger->n_rows == -1) {
          logger->n_rows = logger->expectElements[j];
        } else if (logger->n_rows != logger->expectElements[j]) {
          logger->scalarArraysAsColumns = false;
        }
      }
    }
  }
  if (logger->mallocNeeded) {
    logger->elementIndex = (int32_t *)malloc(sizeof(int32_t) * pva->numPVs);
    logger->outputRow = (int64_t *)malloc(sizeof(int64_t));
    logger->outputPage = (int64_t *)malloc(sizeof(int64_t));
  }
  logger->outputRow[0] = 0;
  logger->outputPage[0] = -1;

  if (logger->n_rows == -1) {
    logger->scalarsAsColumns = true;
    logger->scalarArraysAsColumns = false;
    logger->n_rows = logger->flushInterval; //The flush interval option is only used by scalarsAsColumns mode
  } else if (logger->scalarArraysAsColumns == false) {
    logger->n_rows = 0;
    if (logger->flushInterval > 1) {
      logger->n_rows = logger->flushInterval;
    }
  }
  if (logger->generations) {
    if (logger->mallocNeeded == false) {
      free(logger->outputfile);
    }
    logger->outputfile = MakeGenerationFilename(logger->outputfileOrig, logger->generationsDigits, logger->generationsDelimiter, NULL);
    if (logger->verbose) {
      fprintf(stdout, "New generation file started: %s\n", logger->outputfile);
    }
  } else if (logger->dailyFiles) {
    if (logger->mallocNeeded == false) {
      free(logger->outputfile);
    }
    logger->outputfile = MakeDailyGenerationFilename(logger->outputfileOrig, logger->generationsDigits, logger->generationsDelimiter, logger->timetag);
    if (logger->dailyFilesVerbose) {
      fprintf(stdout, "New generation file started: %s\n", logger->outputfile);
    }
  } else if (logger->monthlyFiles) {
    if (logger->mallocNeeded == false) {
      free(logger->outputfile);
    }
    logger->outputfile = MakeMonthlyGenerationFilename(logger->outputfileOrig, logger->generationsDigits, logger->generationsDelimiter, logger->timetag);
    if (logger->monthlyFilesVerbose) {
      fprintf(stdout, "New generation file started: %s\n", logger->outputfile);
    }
  } else {
    SDDS_CopyString(&(logger->outputfile), logger->outputfileOrig);
  }
  makeTimeBreakdown(getTimeInSecs(), NULL, &(logger->DayNow), &(logger->HourNow), NULL, NULL, NULL, NULL);
  logger->mallocNeeded = false;

  /*Check if output file already exists */
  if (fexists(logger->outputfile)) {
    if (!logger->append && !logger->overwrite) {
      fprintf(stderr, "error: File %s already exists.\n", logger->outputfile);
      return (1);
    }
  } else {
    logger->append = false;
  }

  if (logger->append) {
    if (!SDDS_CheckFile(logger->outputfile)) {
      if (!SDDS_RecoverFile(logger->outputfile, RECOVERFILE_VERBOSE)) {
        fprintf(stderr, "error: Unable to append to corrupted file\n");
        return (1);
      }
    }
    if (VerifyFileIsAppendable(sdds, pva, logger) == 1) {
      return (1);
    }
    if (logger->scalarsAsColumns) {
      if (!SDDS_InitializeAppendToPage(sdds, logger->outputfile, logger->n_rows, &(logger->outputRow[0]))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    } else {
      if (!SDDS_InitializeAppend(sdds, logger->outputfile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    }
    SDDS_EnableFSync(sdds);
    return (0);
  }

  //Open SDDS file
  if (!SDDS_InitializeOutput(sdds, SDDS_BINARY, 1, NULL, NULL, logger->outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  SDDS_EnableFSync(sdds);

  if (logger->scalarsAsColumns) {
    if (!SDDS_SetRowCountMode(sdds, SDDS_FIXEDROWCOUNT)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  }

  //Define parameters, columns and/or arrays
  if (logger->scalarsAsColumns) {
    logger->caErrorsIndex = SDDS_DefineColumn(sdds, "CAerrors", NULL, NULL, "Channel access errors for this row", NULL, SDDS_LONG, 0);
    logger->timeIndex = SDDS_DefineColumn(sdds, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0);
    logger->stepIndex = SDDS_DefineColumn(sdds, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, 0);
    logger->timeofdayIndex = SDDS_DefineColumn(sdds, "TimeOfDay", NULL, "h", "Hour of the day", NULL, SDDS_FLOAT, 0);
    logger->dayofmonthIndex = SDDS_DefineColumn(sdds, "DayOfMonth", NULL, "days", "Day of the month", NULL, SDDS_FLOAT, 0);
  } else {
    logger->caErrorsIndex = SDDS_DefineParameter(sdds, "CAerrors", NULL, NULL, "Channel access errors for this row", NULL, SDDS_LONG, NULL);
    logger->timeIndex = SDDS_DefineParameter(sdds, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL);
    logger->stepIndex = SDDS_DefineParameter(sdds, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, NULL);
    logger->timeofdayIndex = SDDS_DefineParameter(sdds, "TimeOfDay", NULL, "h", "Hour of the day", NULL, SDDS_FLOAT, NULL);
    logger->dayofmonthIndex = SDDS_DefineParameter(sdds, "DayOfMonth", NULL, "days", "Day of the month", NULL, SDDS_FLOAT, NULL);
  }
  if ((logger->caErrorsIndex < 0) ||
      (logger->timeIndex < 0) ||
      (logger->stepIndex < 0) ||
      (logger->timeofdayIndex < 0) ||
      (logger->dayofmonthIndex < 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (logger->glitch_pvCount != 0) {
    long numTrig = 0, numAlarm = 0, numGlitch = 0;
    logger->triggerTimeIndex = SDDS_DefineParameter(sdds, "TriggerTime", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL);
    if (logger->triggerTimeIndex < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    for (j = 0; j < logger->glitch_pvCount; j++) {
      if (logger->glitch_TransitionThresholdDefined && logger->glitch_TransitionDirectionDefined && (logger->glitch_transitionDirection[j] != 0)) {
        snprintf(buffer, sizeof(buffer), "%sTriggered%ld", logger->glitch_controlName[j], numTrig);
        snprintf(descrip, sizeof(descrip), "trigger status of %s%ld", logger->glitch_controlName[j], numTrig);
        logger->glitch_triggerIndex[j] = SDDS_DefineParameter(sdds, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL);
        if (logger->glitch_triggerIndex[j] < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        numTrig++;
      }
    }
    for (j = 0; j < logger->glitch_pvCount; j++) {
      if (((logger->glitch_NoAlarmDefined) && (logger->glitch_noAlarm[j] != 0)) ||
          ((logger->glitch_MinorAlarmDefined) && (logger->glitch_minorAlarm[j] != 0)) ||
          ((logger->glitch_MajorAlarmDefined) && (logger->glitch_majorAlarm[j] != 0))) {
        snprintf(buffer, sizeof(buffer), "%sAlarmTrigger%ld", logger->glitch_controlName[j], numAlarm);
        snprintf(descrip, sizeof(descrip), "alarm-trigger status of %s%ld", logger->glitch_controlName[j], numAlarm);
        logger->glitch_alarmIndex[j] = SDDS_DefineParameter(sdds, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL);
        if (logger->glitch_alarmIndex[j] < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        snprintf(buffer, sizeof(buffer), "%sAlarmSeverity%ld", logger->glitch_controlName[j], numAlarm);
        snprintf(descrip, sizeof(descrip), "EPICS alarm severity of %s", logger->glitch_controlName[j]);
        logger->glitch_alarmSeverityIndex[j] = SDDS_DefineParameter(sdds, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL);
        if (logger->glitch_alarmSeverityIndex[j] < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        numAlarm++;
      }
    }
    for (j = 0; j < logger->glitch_pvCount; j++) {
      if (logger->glitch_GlitchThresholdDefined && (logger->glitch_glitchThreshold[j] != 0) &&
          logger->glitch_GlitchBaselineSamplesDefined && (logger->glitch_glitchBaselineSamples[j] > 0)) {
        snprintf(buffer, sizeof(buffer), "%sGlitched%ld", logger->glitch_controlName[j], numGlitch);
        snprintf(descrip, sizeof(descrip), "glitch status of %s", logger->glitch_controlName[j]);
        logger->glitch_glitchIndex[j] = SDDS_DefineParameter(sdds, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL);
        if (logger->glitch_glitchIndex[j] < 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        numGlitch++;
      }
    }
  }

  for (j = 0; j < pva->numPVs; j++) {
    if (logger->expectNumeric[j]) {
      sddstype = SDDS_DOUBLE;
    } else {
      sddstype = SDDS_STRING;
    }
    if (logger->storageType) {
      if (logger->storageType[j] != 0) {
        sddstype = logger->storageType[j];
      }
    }
    if (logger->units) {
      units = logger->units[j];
      if (strlen(units) == 0) {
        units = pva->pvaData[j].units;
      }
    } else {
      units = pva->pvaData[j].units;
    }
    if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
      if (logger->scalarsAsColumns) { //No scalar arrays, so use SDDS_COLUMN for the scalar values
        logger->elementIndex[j] = SDDS_DefineColumn(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, 0);
      } else { //Use SDDS_PARAMETER for the scalar values
        logger->elementIndex[j] = SDDS_DefineParameter(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, NULL);
      }
    } else if (logger->expectScalarArray[j]) {
      if (logger->scalarArraysAsColumns) { //All scalar arrays are the same length
        logger->elementIndex[j] = SDDS_DefineColumn(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, 0);
      } else { //The scalar arrays are of different lengths
        logger->elementIndex[j] = SDDS_DefineArray(sdds, logger->readbackName[j], NULL, units, NULL, NULL, sddstype, 0, 1, NULL);
      }
    }
    if (logger->elementIndex[j] < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  }
  if (logger->scalarArraysAsColumns) {
    //SDDS_table[0]->layout.data_mode.column_major = 1;
  }

  //Save header
  if (!SDDS_SaveLayout(sdds)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (!SDDS_WriteLayout(sdds)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  //Ping run control if in use
  if (PingRunControl() != 0) {
    return (1);
  }
  return (0);
}

void AllocateCircularBuffers(PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long i, j, k;
  logger->circularbuffer_length = logger->circularbuffer_before + logger->circularbuffer_after + 1;
  logger->circularbuffer_index = 0;
  logger->circularbufferDouble = (double ***)malloc(sizeof(double **) * (logger->pvCount + 3));
  logger->circularbufferString = (char ****)malloc(sizeof(char ***) * logger->pvCount);
  for (i = 0; i < logger->pvCount; i++) {
    logger->circularbufferDouble[i] = (double **)malloc(sizeof(double *) * logger->circularbuffer_length);
    logger->circularbufferString[i] = (char ***)malloc(sizeof(char **) * logger->circularbuffer_length);
    for (j = 0; j < logger->circularbuffer_length; j++) {
      logger->circularbufferDouble[i][j] = (double *)malloc(sizeof(double) * logger->expectElements[i]);
      logger->circularbufferString[i][j] = (char **)malloc(sizeof(char *) * logger->expectElements[i]);
      for (k = 0; k < logger->expectElements[i]; k++) {
        logger->circularbufferString[i][j][k] = NULL;
      }
    }
  }
  //This is for the accessory data
  for (i = logger->pvCount; i < logger->pvCount + 3; i++) {
    logger->circularbufferDouble[i] = (double **)malloc(sizeof(double *) * logger->circularbuffer_length);
    for (j = 0; j < logger->circularbuffer_length; j++) {
      logger->circularbufferDouble[i][j] = (double *)malloc(sizeof(double));
    }
  }
  //This is the time data
  logger->circularbufferLongDouble = (long double *)malloc(sizeof(long double) * logger->circularbuffer_length);
}

void FreeCircularBuffers(LOGGER_DATA *logger) {
  long i, j, k;

  for (i = 0; i < logger->pvCount; i++) {
    for (j = 0; j < logger->circularbuffer_length; j++) {
      for (k = 0; k < logger->expectElements[i]; k++) {
        free(logger->circularbufferString[i][j][k]);
      }
      free(logger->circularbufferString[i][j]);
    }
    free(logger->circularbufferString[i]);
  }
  for (i = 0; i < logger->pvCount + 3; i++) {
    for (j = 0; j < logger->circularbuffer_length; j++) {
      free(logger->circularbufferDouble[i][j]);
    }
    free(logger->circularbufferDouble[i]);
  }
  free(logger->circularbufferDouble);
  free(logger->circularbufferString);
  free(logger->circularbufferLongDouble);
}

void StoreDataIntoCircularBuffers(PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long i, j, k;
  double value;
  j = logger->circularbuffer_index;
  logger->circularbufferDouble[pva->numPVs][j][0] = pva->numNotConnected;
  logger->circularbufferLongDouble[j] = logger->currentTime;
  logger->circularbufferDouble[pva->numPVs + 1][j][0] = logger->StartHour + ((double)logger->currentTime - logger->StartTime) / 3600.0;
  logger->circularbufferDouble[pva->numPVs + 2][j][0] = logger->StartDay + ((double)logger->currentTime - logger->StartTime) / 86400.0;

  for (i = 0; i < pva->numPVs; i++) {
    if ((pva->isConnected[i]) && (logger->verifiedType[i]) && ((logger->monitor == false) || (pva->pvaData[i].numMonitorReadings > 0))) {
      if (logger->expectScalar[i] || logger->treatScalarArrayAsScalar[i]) {
        // For treatScalarArrayAsScalar, determine the element index to use
        int32_t elemIdx = 0;
        if (logger->treatScalarArrayAsScalar[i] && (logger->scalarArrayStartIndex != NULL)) {
          elemIdx = logger->scalarArrayStartIndex[i] - 1;
        }
        if (logger->expectNumeric[i]) {
          value = logger->monitor ? pva->pvaData[i].monitorData[0].values[elemIdx] : pva->pvaData[i].getData[0].values[elemIdx];
          if (logger->scaleFactor) {
            value *= logger->scaleFactor[i];
          }
          logger->circularbufferDouble[i][j][0] = value;
        } else {
          if (logger->circularbufferString[i][j][0]) {
            free(logger->circularbufferString[i][j][0]);
            logger->circularbufferString[i][j][0] = NULL;
          }
          cp_str(&(logger->circularbufferString[i][j][0]),
                 logger->monitor ? pva->pvaData[i].monitorData[0].stringValues[elemIdx] : pva->pvaData[i].getData[0].stringValues[elemIdx]);
        }
      } else if (logger->expectScalarArray[i]) {
        if (logger->expectNumeric[i]) {
          if (logger->monitor) {
            for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
              logger->circularbufferDouble[i][j][k] = pva->pvaData[i].monitorData[0].values[k];
            }
          } else {
            for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
              logger->circularbufferDouble[i][j][k] = pva->pvaData[i].getData[0].values[k];
            }
          }
        } else {
          if (logger->monitor) {
            for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
              if (logger->circularbufferString[i][j][k]) {
                free(logger->circularbufferString[i][j][k]);
                logger->circularbufferString[i][j][k] = NULL;
              }
              cp_str(&(logger->circularbufferString[i][j][k]), pva->pvaData[i].monitorData[0].stringValues[k]);
            }
          } else {
            for (k = 0; k < pva->pvaData[i].numGetElements; k++) {
              if (logger->circularbufferString[i][j][k]) {
                free(logger->circularbufferString[i][j][k]);
                logger->circularbufferString[i][j][k] = NULL;
              }
              cp_str(&(logger->circularbufferString[i][j][k]), pva->pvaData[i].getData[0].stringValues[k]);
            }
          }
        }
      }
    } else {
      if (logger->expectScalar[i] || logger->treatScalarArrayAsScalar[i]) {
        if (logger->expectNumeric[i]) {
          logger->circularbufferDouble[i][j][0] = 0;
        } else {
          if (logger->circularbufferString[i][j][0]) {
            free(logger->circularbufferString[i][j][0]);
            logger->circularbufferString[i][j][0] = NULL;
          }
          cp_str(&(logger->circularbufferString[i][j][0]), (char *)"");
        }
      } else if (logger->expectScalarArray[i]) {
        if (logger->expectNumeric[i]) {
          for (k = 0; k < logger->expectElements[i]; k++) {
            logger->circularbufferDouble[i][j][k] = logger->emptyColumn[k];
          }
        } else {
          for (k = 0; k < logger->expectElements[i]; k++) {
            if (logger->circularbufferString[i][j][k]) {
              free(logger->circularbufferString[i][j][k]);
              logger->circularbufferString[i][j][k] = NULL;
            }
            cp_str(&(logger->circularbufferString[i][j][k]), (char *)"");
          }
        }
      }
    }
  }
  logger->circularbuffer_index++;
  if (logger->circularbuffer_index == logger->circularbuffer_length) {
    logger->circularbuffer_index = 0;
  }
}

long WriteAccessoryData(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  int32_t result;
  long n;
  long filecount;
  SDDS_TABLE *sdds;

  if (logger->onePv_OutputDirectory != NULL) {
    filecount = pva->numPVs;
  } else {
    filecount = 1;
  }
  for (n = 0; n < filecount; n++) {
    sdds = &(SDDS_table[n]);
    if (logger->onePv_OutputDirectory != NULL) {
      if (logger->expectScalar[n] || logger->treatScalarArrayAsScalar[n]) {
        logger->scalarsAsColumns = true;
        logger->scalarArraysAsColumns = false;
      } else if (logger->expectScalarArray[n]) {
        logger->scalarsAsColumns = false;
        logger->scalarArraysAsColumns = true;
      }
    }
    if (logger->scalarsAsColumns) {
      if (logger->onePv_OutputDirectory == NULL) {
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->caErrorsIndex, pva->numNotConnected, -1);
      } else if (pva->isConnected[n]) {
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->caErrorsIndex, 0, -1);
      } else {
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->caErrorsIndex, 1, -1);
      }
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }

      result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                 logger->timeIndex, (double)(logger->currentTime), -1);
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }

      if (logger->onePv_OutputDirectory == NULL) {
        //Don't use logger->step because we might be appending to an existing file
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->stepIndex, logger->outputRow[n], -1);
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        /*
              result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                         logger->timeofdayIndex, logger->StartHour + ((double)logger->currentTime - logger->StartTime) / 3600.0, -1);
*/
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->timeofdayIndex, logger->HourNow, -1);

        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }

        /*
              result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                         logger->dayofmonthIndex, logger->StartDay + ((double)logger->currentTime - logger->StartTime) / 86400.0, -1);
*/
        result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, logger->outputRow[n],
                                   logger->dayofmonthIndex, logger->DayNow, -1);
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
    } else {
      int32_t cIndex, tIndex;
      if (logger->onePv_OutputDirectory != NULL) {
        cIndex = logger->caErrorsIndexP;
        tIndex = logger->timeIndexP;
      } else {
        cIndex = logger->caErrorsIndex;
        tIndex = logger->timeIndex;
      }
      result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                 cIndex, pva->numNotConnected);
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                 tIndex, (double)(logger->currentTime));
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      if (logger->onePv_OutputDirectory == NULL) {
        result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   logger->stepIndex, logger->outputPage[n]);
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }

        /*
              result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                         logger->timeofdayIndex, logger->StartHour + ((double)logger->currentTime - logger->StartTime) / 3600.0);
*/
        result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   logger->timeofdayIndex, logger->HourNow);
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }

        /*
              result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                         logger->dayofmonthIndex, logger->StartDay + ((double)logger->currentTime - logger->StartTime) / 86400.0);
*/
        result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                   logger->dayofmonthIndex, logger->DayNow);
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
    }
  }
  return (0);
}

int32_t SetNumericRowValue(SDDS_TABLE *sdds, long outputRow, long elementIndex, int storageType, double value) {
    switch (storageType) {
        case SDDS_LONGDOUBLE:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (long double)value, -1);
        case SDDS_ULONG64:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (uint64_t)value, -1);
        case SDDS_LONG64:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (int64_t)value, -1);
        case SDDS_ULONG:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (uint32_t)value, -1);
        case SDDS_LONG:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (int32_t)value, -1);
        case SDDS_USHORT:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (unsigned short)value, -1);
        case SDDS_SHORT:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, (short)value, -1);
        default:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, value, -1);
    }
}

int32_t SetStringRowValue(SDDS_TABLE *sdds, long outputRow, long elementIndex, int storageType, char *value) {
    switch (storageType) {
        case SDDS_CHARACTER:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, value[0], -1);
        default:
            return SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, outputRow, elementIndex, value, -1);
    }
}

int32_t SetNumericParameterValue(SDDS_TABLE *sdds, long elementIndex, int storageType, double value) {
    switch (storageType) {
        case SDDS_LONGDOUBLE:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (long double)value);
        case SDDS_ULONG64:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (uint64_t)value);
        case SDDS_LONG64:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (int64_t)value);
        case SDDS_ULONG:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (uint32_t)value);
        case SDDS_LONG:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (int32_t)value);
        case SDDS_USHORT:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (unsigned short)value);
        case SDDS_SHORT:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, (short)value);
        default:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, value);
    }
}

int32_t SetStringParameterValue(SDDS_TABLE *sdds, long elementIndex, int storageType, char *value) {
    switch (storageType) {
        case SDDS_CHARACTER:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, value[0]);
        default:
            return SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, elementIndex, value);
    }
}

int32_t SetNumericArrayValues(SDDS_TABLE *sdds, char *name, int storageType, double *values, int32_t length) {
  int32_t result;
  switch (storageType) {
  case SDDS_LONGDOUBLE:
    {
      long double *v;
      v = (long double*)malloc(sizeof(long double) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (long double)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_ULONG64:
    {
      uint64_t *v;
      v = (uint64_t*)malloc(sizeof(uint64_t) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (uint64_t)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_LONG64:
    {
      int64_t *v;
      v = (int64_t*)malloc(sizeof(int64_t) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (int64_t)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_ULONG:
    {
      uint32_t *v;
      v = (uint32_t*)malloc(sizeof(uint32_t) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (uint64_t)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_LONG:
    {
      int32_t *v;
      v = (int32_t*)malloc(sizeof(int32_t) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (int32_t)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_USHORT:
    {
      unsigned short *v;
      v = (unsigned short*)malloc(sizeof(unsigned short) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (unsigned short)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  case SDDS_SHORT:
    {
      short *v;
      v = (short*)malloc(sizeof(short) * length);
      for (int32_t i=0; i < length; i++)
        v[i] = (short)(values[i]);
      result = SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, v, length);
      free(v);
    }
    break;
  default:
    return SDDS_SetArrayVararg(sdds, name, SDDS_CONTIGUOUS_DATA, values, length);
  }
  return result;
}

long WriteData(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  int32_t result;
  long j, n;
  double value;
  long filecount, pvStart, pvEnd;
  SDDS_TABLE *sdds;

  if (WriteAccessoryData(SDDS_table, pva, logger)) {
    return (1);
  }
  if (logger->onePv_OutputDirectory != NULL) {
    filecount = pva->numPVs;
  } else {
    filecount = 1;
  }
  for (n = 0; n < filecount; n++) {
    if (logger->onePv_OutputDirectory != NULL) {
      pvStart = n;
      pvEnd = n + 1;
    } else {
      pvStart = 0;
      pvEnd = pva->numPVs;
    }

    sdds = &(SDDS_table[n]);
    for (j = pvStart; j < pvEnd; j++) {
      if (logger->onePv_OutputDirectory != NULL) {
        if (logger->expectScalar[n] || logger->treatScalarArrayAsScalar[n]) {
          logger->scalarsAsColumns = true;
          logger->scalarArraysAsColumns = false;
        } else if (logger->expectScalarArray[n]) {
          logger->scalarsAsColumns = false;
          logger->scalarArraysAsColumns = true;
        }
      }

      if ((pva->isConnected[j]) && (logger->verifiedType[j]) && ((logger->monitor == false) || (pva->pvaData[j].numMonitorReadings > 0))) {
        if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
          // For treatScalarArrayAsScalar, determine the element index to use
          int32_t elemIdx = 0;
          if (logger->treatScalarArrayAsScalar[j] && (logger->scalarArrayStartIndex != NULL)) {
            elemIdx = logger->scalarArrayStartIndex[j] - 1;
          }
          if (logger->scalarsAsColumns) {
            if (logger->expectNumeric[j]) {
              if ((logger->logInterval > 1) && (logger->average) && ((logger->average[j] == 'y') || (logger->average[j] == 'Y'))) {
                value = logger->averagedValues[j];
              } else {
                value = logger->monitor ? pva->pvaData[j].monitorData[0].values[elemIdx] : pva->pvaData[j].getData[0].values[elemIdx];
                if (logger->scaleFactor) {
                  value *= logger->scaleFactor[j];
                }
              }
              result = SetNumericRowValue(sdds, logger->outputRow[n], logger->elementIndex[j], logger->storageType[j], value);
            } else {
              result = SetStringRowValue(sdds, logger->outputRow[n], logger->elementIndex[j], logger->storageType[j], 
                                           logger->monitor ? pva->pvaData[j].monitorData[0].stringValues[elemIdx] : pva->pvaData[j].getData[0].stringValues[elemIdx]);
            }
          } else {
            if (logger->expectNumeric[j]) {
              value = logger->monitor ? pva->pvaData[j].monitorData[0].values[elemIdx] : pva->pvaData[j].getData[0].values[elemIdx];
              if (logger->scaleFactor) {
                value *= logger->scaleFactor[j];
              }
              result = SetNumericParameterValue(sdds, logger->elementIndex[j], logger->storageType[j], value);
            } else {
              result = SetStringParameterValue(sdds, logger->elementIndex[j], logger->storageType[j], 
                                                logger->monitor ? pva->pvaData[j].monitorData[0].stringValues[elemIdx] : pva->pvaData[j].getData[0].stringValues[elemIdx]);
            }
          }
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        } else if (logger->expectScalarArray[j]) {
          if (logger->scalarArraysAsColumns) {
            if ((logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
              int32_t ee;
              ee = logger->scalarArrayEndIndex[j] - logger->scalarArrayStartIndex[j] + 1;
              if (logger->expectNumeric[j]) {
                double *p;
                p = logger->monitor ? pva->pvaData[j].monitorData[0].values + (logger->scalarArrayStartIndex[j] - 1) : pva->pvaData[j].getData[0].values + (logger->scalarArrayStartIndex[j] - 1);
                result = SDDS_SetColumnFromDoubles(sdds, SDDS_SET_BY_INDEX, p,
                                                   ee, logger->elementIndex[j]);
              } else {
                char **p;
                p = logger->monitor ? pva->pvaData[j].monitorData[0].stringValues + (logger->scalarArrayStartIndex[j] - 1) : pva->pvaData[j].getData[0].stringValues + (logger->scalarArrayStartIndex[j] - 1);
                result = SDDS_SetColumn(sdds, SDDS_SET_BY_INDEX, p,
                                        ee, logger->elementIndex[j]);
              }
            } else {
              if (logger->expectNumeric[j]) {
                result = SDDS_SetColumnFromDoubles(sdds, SDDS_SET_BY_INDEX, logger->monitor ? pva->pvaData[j].monitorData[0].values : pva->pvaData[j].getData[0].values,
                                        logger->expectElements[j], logger->elementIndex[j]);
              } else {
                result = SDDS_SetColumn(sdds, SDDS_SET_BY_INDEX, logger->monitor ? pva->pvaData[j].monitorData[0].stringValues : pva->pvaData[j].getData[0].stringValues,
                                        logger->expectElements[j], logger->elementIndex[j]);
              }
            }
          } else {
            if ((logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
              int32_t ee;
              ee = logger->scalarArrayEndIndex[j] - logger->scalarArrayStartIndex[j] + 1;
              if (logger->expectNumeric[j]) {
                double *p;
                p = logger->monitor ? pva->pvaData[j].monitorData[0].values + (logger->scalarArrayStartIndex[j] - 1) : pva->pvaData[j].getData[0].values + (logger->scalarArrayStartIndex[j] - 1);
                result = SetNumericArrayValues(sdds, (char *)logger->readbackName[j], logger->storageType[j], p, ee);
              } else {
                char **p;
                p = logger->monitor ? pva->pvaData[j].monitorData[0].stringValues + (logger->scalarArrayStartIndex[j] - 1) : pva->pvaData[j].getData[0].stringValues + (logger->scalarArrayStartIndex[j] - 1);
                result = SDDS_SetArrayVararg(sdds, (char *)logger->readbackName[j], SDDS_CONTIGUOUS_DATA, p, ee);
              }
            } else {
              if (logger->expectNumeric[j]) {
                result = SetNumericArrayValues(sdds, (char *)logger->readbackName[j], logger->storageType[j],
                                               logger->monitor ? pva->pvaData[j].monitorData[0].values : pva->pvaData[j].getData[0].values, (int32_t)(pva->pvaData[j].numGetElements));
              } else {
                result = SDDS_SetArrayVararg(sdds, (char *)logger->readbackName[j], SDDS_CONTIGUOUS_DATA,
                                             logger->monitor ? pva->pvaData[j].monitorData[0].stringValues : pva->pvaData[j].getData[0].stringValues, (int32_t)(pva->pvaData[j].numGetElements));
              }
            }
          }
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
      } else {
        if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
          if (logger->scalarsAsColumns) {
            if (logger->expectNumeric[j]) {
              result = SetNumericRowValue(sdds, logger->outputRow[n], logger->elementIndex[j], logger->storageType[j], (double)0);
            } else {
              result = SetStringRowValue(sdds, logger->outputRow[n], logger->elementIndex[j], logger->storageType[j], (char *)"");
            }
          } else {
            if (logger->expectNumeric[j]) {
              result = SetNumericParameterValue(sdds, logger->elementIndex[j], logger->storageType[j], (double)0);
            } else {
              result = SetStringParameterValue(sdds, logger->elementIndex[j], logger->storageType[j], (char *)"");
            }
          }
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        } else if (logger->expectScalarArray[j]) {
          if (logger->scalarArraysAsColumns) {
            if (logger->expectNumeric[j]) {
              result = SDDS_SetColumnFromDoubles(sdds, SDDS_SET_BY_INDEX, logger->emptyColumn,
                                      logger->expectElements[j], logger->elementIndex[j]);
            } else {
              result = SDDS_SetColumn(sdds, SDDS_SET_BY_INDEX, logger->emptyStringColumn,
                                      logger->expectElements[j], logger->elementIndex[j]);
            }
          } else {
            if (logger->expectNumeric[j]) {
              result = SetNumericArrayValues(sdds, (char *)logger->readbackName[j], logger->storageType[j], NULL, (int32_t)0);
            } else {
              result = SDDS_SetArrayVararg(sdds, (char *)logger->readbackName[j], SDDS_CONTIGUOUS_DATA,
                                           NULL, (int32_t)0);
            }
          }
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
      }
    }
    if (logger->scalarsAsColumns) {
      logger->outputRow[n]++;
    }
  }
  if ((logger->logInterval > 1) && (logger->average != NULL)) {
    for (j = 0; j < pva->numPVs; j++) {
      logger->averagedValues[j] = 0;
    }
  }
  return (0);
}

void AverageData(PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j;
  double value;

  for (j = 0; j < pva->numPVs; j++) {
    if ((pva->isConnected[j]) && ((logger->monitor == false) || (pva->pvaData[j].numMonitorReadings > 0))) {
      switch (pva->pvaData[j].fieldType) {
      case epics::pvData::scalar: {
        if (logger->scalarsAsColumns) {
          if (logger->expectNumeric[j]) {
            if ((logger->average[j] == 'y') || (logger->average[j] == 'Y')) {
              value = logger->monitor ? pva->pvaData[j].monitorData[0].values[0] : pva->pvaData[j].getData[0].values[0];
              value = value / logger->logInterval;
              if (logger->scaleFactor) {
                value *= logger->scaleFactor[j];
              }
              logger->averagedValues[j] += value;
            }
          }
        }
        break;
      }
      default: {
      }
      }
    }
  }
  return;
}

#if defined(_WIN32)
#  include <thread>
#  include <chrono>
int nanosleep(const struct timespec *req, struct timespec *rem) {
  (void)rem;
  if (!req) {
    return (0);
  }
  /*
   * Convert (tv_sec, tv_nsec) to microseconds.
   * Note: tv_nsec is nanoseconds, not fractional seconds.
   */
  long long usec = (long long)req->tv_sec * 1000000LL + (long long)(req->tv_nsec / 1000L);
  if (usec > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(usec));
  }
  return (0);
}
#endif

long pvaThreadSleep(long double seconds) {
  struct timespec delayTime;
  struct timespec remainingTime;
  long double targetTime;

  if (sigint) {
    return (1);
  }

  if (seconds > 0) {
    targetTime = getLongDoubleTimeInSecs() + seconds;
    while (seconds > 5) {
      if (sigint) {
        return (1);
      }
      delayTime.tv_sec = 5;
      delayTime.tv_nsec = 0;
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        delayTime = remainingTime;
        if (sigint) {
          return (1);
        }
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
      {
        delayTime = remainingTime;
        if (sigint) {
          return (1);
        }
      }
  }
  return (0);
}

long pvaThreadSleepWithPolling(PVA_OVERALL **pva, long count, long double targetTime) {
  struct timespec delayTime;
  struct timespec remainingTime;
  long double seconds, interval = .001L, pauseTargetTime;
  long rcInterval, j = 0;

  rcInterval = 5 / interval;   //Calculate how many intervals will equal 5 seconds
  delayTime.tv_sec = interval; //Converted into whole seconds
  delayTime.tv_nsec = (interval - delayTime.tv_sec) * 1e9L;

  pauseTargetTime = targetTime - .6 - (.00025 * pva[0]->numPVs);
  seconds = pauseTargetTime - getLongDoubleTimeInSecs();
  if (seconds > 0) {
    PausePVAMonitoring(pva, count); //Pause monitoring because background events cause the CPU usage to be high if there are thousands of PVs.
    while (seconds > 0) {
      if (sigint) {
        return (1);
      }
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        delayTime = remainingTime;
        if (sigint) {
          return (1);
        }
      }
      if (j % rcInterval == 0) {
        if (PingRunControl() != 0) //Ping the run control once every 5 seconds
        {
          return (1);
        }
      }
      j++;
      seconds = pauseTargetTime - getLongDoubleTimeInSecs();
    }
    ResumePVAMonitoring(pva, count);
  }

  seconds = targetTime - getLongDoubleTimeInSecs();
  if (seconds > 0) {
    while (seconds > 0) {
      if (sigint) {
        return (1);
      }
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        delayTime = remainingTime;
        if (sigint) {
          return (1);
        }
      }
      if (PollMonitoredPVA(pva, count) == -1) //Poll monitored PVs and extract values for those that changed
      {
        return (1);
      }
      seconds = targetTime - getLongDoubleTimeInSecs();
    }
  } else {
    if (PollMonitoredPVA(pva, count) == -1) //Poll monitored PVs and extract values for those that changed
    {
      return (1);
    }
    fprintf(stderr, "Logger cannot keep up. Too many PVs and too short an interval. Consider removing the -monitor option.\n");
  }

  return (0);
}

long pvaThreadSleepWithPollingAndDataStrobe(PVA_OVERALL **pva, long count, PVA_OVERALL *pvaST, bool randomTime, double hold_off) {
  struct timespec delayTime;
  struct timespec remainingTime;
  struct timespec holdoff;
  long j = 1, numUpdated, rcInterval;
  double initStrobeValue = 0;
  long double interval;
  static long double lastTriggerTime = 0, thisTriggerTime = 0, triggerInterval = -1;
  static int step = 0;
  long double targetTime, seconds;

  interval = .001L;
  rcInterval = 5 / interval;
  delayTime.tv_sec = interval; //Converted into whole seconds
  delayTime.tv_nsec = (interval - delayTime.tv_sec) * 1e9L;

  holdoff.tv_sec = hold_off; //Converted into whole seconds
  holdoff.tv_nsec = (hold_off - holdoff.tv_sec) * 1e9L;

  if (pvaST->isConnected[0] && (pvaST->pvaData[0].numMonitorReadings > 0)) {
    initStrobeValue = pvaST->pvaData[0].monitorData[0].values[0];
  }

  //Never goes in this loop because triggerInterval is -1. Looks like a bug
  //Sit in a waiting loop till .5 seconds before we expect the trigger
  if ((randomTime == false) && (triggerInterval > 0)) {
    PausePVAMonitoring(pva, count);                                                  //Pause monitoring because background events cause the CPU usage to be high if there are thousands of PVs.
    targetTime = lastTriggerTime + triggerInterval - .6 - (.00025 * pva[0]->numPVs); //the .00025sec per PV extra time is due to the extra time needed to resume PVA monitoring
    seconds = targetTime - getLongDoubleTimeInSecs();
    if (seconds <= 0) {
      fprintf(stderr, "Logger cannot keep up. Too many PVs and too short an interval. Consider removing the -monitor option.\n");
    }
    while (seconds > 0) {
      if (sigint) {
        return (1);
      }
      while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
        delayTime = remainingTime;
        if (sigint) {
          return (1);
        }
      }
      if (j % rcInterval == 0) {
        if (PingRunControl() != 0) //Ping the run control once every 5 seconds
        {
          return (1);
        }
      }
      j++;
      seconds = targetTime - getLongDoubleTimeInSecs();
    }
    ResumePVAMonitoring(pva, count);
    if (pvaThreadSleep(.1) == 1) {
      return (1);
    }
  }

  //Check to see if we missed the trigger. Perhaps our waiting loop was too long. This could be because something got hung up on the previous loop
  numUpdated = PollMonitoredPVA(pvaST);
  if (numUpdated == -1) {
    return (1);
  }
  if ((numUpdated == 1) && (pvaST->isConnected[0])) {
    if (initStrobeValue != pvaST->pvaData[0].monitorData[0].values[0]) {
      lastTriggerTime = 0;
      thisTriggerTime = 0;
      triggerInterval = -1;
      step = 0;
      numUpdated = PollMonitoredPVA(pva, count);
      fprintf(stderr, "Logger missed the strobe PV updating. It may be overloaded or an issue with the file system\n");
      if (numUpdated == -1) //Poll monitored PVs and extract values for those that changed
      {
        return (1);
      }
      return (0);
    }
  }

  while (1) {
    if (sigint) {
      return (1);
    }
    while ((nanosleep(&delayTime, &remainingTime) == -1) && (errno == EINTR)) {
      delayTime = remainingTime;
      if (sigint) {
        return (1);
      }
    }

    if (triggerInterval > 0) {
      numUpdated = PollMonitoredPVA(pva, count);
      if (numUpdated == -1) //Poll monitored PVs and extract values for those that changed
      {
        return (1);
      }
    }
    numUpdated = PollMonitoredPVA(pvaST); //Poll monitored strobe PVs and extract value if changed
    if (numUpdated == -1) {
      return (1);
    }
    if ((numUpdated == 1) && (pvaST->isConnected[0])) {
      if (initStrobeValue != pvaST->pvaData[0].monitorData[0].values[0]) {
        if (hold_off > 1e-7) 
          nanosleep(&holdoff, NULL);

        if (randomTime == true) {
          numUpdated = PollMonitoredPVA(pva, count);
          if (numUpdated == -1) //Poll monitored PVs and extract values for those that changed
          {
            return (1);
          }
        } else {
          thisTriggerTime = getLongDoubleTimeInSecs();
          if (triggerInterval <= 0) {
            numUpdated = PollMonitoredPVA(pva, count);
            if (numUpdated == -1) //Poll monitored PVs and extract values for those that changed
              {
                return (1);
              }
          }
          step++;
          if (step > 1) {
            triggerInterval = thisTriggerTime - lastTriggerTime;
          }
          lastTriggerTime = thisTriggerTime;
        }
        return (0);
      }
    }

    if (j % rcInterval == 0) {
      if (PingRunControl() != 0) //Ping the run control once every 5 seconds
      {
        return (1);
      }
    }
    j++;
  }
  //It will never get here
  return (1);
}

long VerifyPVTypes(PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j;
  static long minElements = LONG_MAX;
  long preMinElements = minElements;

  for (j = 0; j < pva->numPVs; j++) {
    if ((logger->verifiedType[j] == false) && (pva->isConnected[j] == true)) {
      if (((pva->pvaData[j].numeric == true) && (logger->expectNumeric[j] == true)) ||
          ((pva->pvaData[j].nonnumeric == true) && (logger->expectNumeric[j] == false))) {
      } else {
        fprintf(stderr, "Error (sddspvalogger): ExpectedNumeric does not match actual PV for %s\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
      if ((pva->pvaData[j].fieldType == epics::pvData::scalar) || (pva->pvaData[j].pvEnumeratedStructure)) {
        if (logger->expectScalar[j] == false) {
          fprintf(stderr, "Error (sddspvalogger): ExpectedFieldType does not match actual PV for %s\n", pva->pvaChannelNames[j].c_str());
          return (1);
        }
      } else if (pva->pvaData[j].fieldType == epics::pvData::scalarArray) {
        if (logger->expectScalarArray[j] == false) {
          fprintf(stderr, "Error (sddspvalogger): ExpectedFieldType does not match actual PV for %s\n", pva->pvaChannelNames[j].c_str());
          return (1);
        }
        // Only apply scalarArray restrictions if not being treated as a scalar
        if (!logger->treatScalarArrayAsScalar[j]) {
          if ((logger->scaleFactor) && ((logger->scaleFactor[j] < .999) || (logger->scaleFactor[j] > 1.001))) {
            fprintf(stderr, "Error (sddspvalogger): ScaleFactor limited to 1.0 for scalarArray type PVs\n");
            return (1);
          }
          if ((logger->average) && ((logger->average[j] == 'y') || (logger->average[j] == 'Y'))) {
            fprintf(stderr, "Error (sddspvalogger): Averaging not supported for scalarArray type PVs\n");
            return (1);
          }
        }
        if (logger->monitor) {
          if ((pva->pvaData[j].numMonitorElements > 0) && (minElements > pva->pvaData[j].numMonitorElements)) {
            minElements = pva->pvaData[j].numMonitorElements;
          } else if ((pva->pvaData[j].numGetElements > 0) && (minElements > pva->pvaData[j].numGetElements)) {
            minElements = pva->pvaData[j].numGetElements;
          }
        } else {
          if ((pva->pvaData[j].numGetElements > 0) && (minElements > pva->pvaData[j].numGetElements)) {
            minElements = pva->pvaData[j].numGetElements;
          }
        }
      } else {
        fprintf(stderr, "Error (sddspvalogger): Invalid expected field type for PV (%s)\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
      if (logger->expectElements[j] != pva->pvaData[j].numGetElements) {
        if (pva->pvaData[j].numGetElements == 0) {
          if (logger->monitor) {
            if (logger->expectElements[j] != pva->pvaData[j].numMonitorElements) {
              if (pva->pvaData[j].numMonitorElements == 0) {
                //PV is back up but hasn't been initialized yet
                continue;
              } else {
                if (logger->strictPVverification) {
                  fprintf(stderr, "Error (sddspvalogger): ExpectedElements (%d) does not match actual PV for %s (%ld)\n", logger->expectElements[j], pva->pvaChannelNames[j].c_str(), pva->pvaData[j].numMonitorElements);
                  return (1);
                } else {
                  fprintf(stderr, "Warning (sddspvalogger): ExpectedElements (%d) does not match actual PV for %s (%ld)\n", logger->expectElements[j], pva->pvaChannelNames[j].c_str(), pva->pvaData[j].numMonitorElements);
                  logger->expectElements[j] = pva->pvaData[j].numMonitorElements;
                }
              }
            }
          } else {
            //PV is back up but hasn't been initialized yet
            continue;
          }
        } else {
          if (logger->strictPVverification) {
            fprintf(stderr, "Error (sddspvalogger): ExpectedElements (%d) does not match actual PV for %s (%ld)\n", logger->expectElements[j], pva->pvaChannelNames[j].c_str(), pva->pvaData[j].numGetElements);
            return (1);
          } else {
            fprintf(stderr, "Warning (sddspvalogger): ExpectedElements (%d) does not match actual PV for %s (%ld)\n", logger->expectElements[j], pva->pvaChannelNames[j].c_str(), pva->pvaData[j].numGetElements);
            logger->expectElements[j] = pva->pvaData[j].numGetElements;
          }
        }
      }

      /*
       * Re-validate scalarArray slice indices after ExpectElements may have been adjusted.
       * Prevents out-of-bounds reads when applying ScalarArrayStartIndex/EndIndex.
       */
      if (logger->expectScalarArray[j] && (logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
        if (logger->scalarArrayStartIndex[j] < 1 ||
            logger->scalarArrayEndIndex[j] < logger->scalarArrayStartIndex[j] ||
            logger->scalarArrayEndIndex[j] > logger->expectElements[j]) {
          fprintf(stderr, "Error (sddspvalogger): Invalid ScalarArrayStartIndex/EndIndex for %s (start=%d, end=%d, ExpectElements=%d)\n",
                  pva->pvaChannelNames[j].c_str(),
                  (int)logger->scalarArrayStartIndex[j],
                  (int)logger->scalarArrayEndIndex[j],
                  (int)logger->expectElements[j]);
          return (1);
        }
      }
      if (logger->treatScalarArrayAsScalar[j] && (logger->scalarArrayStartIndex != NULL)) {
        if (logger->scalarArrayStartIndex[j] < 1 || logger->scalarArrayStartIndex[j] > logger->expectElements[j]) {
          fprintf(stderr, "Error (sddspvalogger): Invalid ScalarArrayStartIndex for scalar extraction for %s (start=%d, ExpectElements=%d)\n",
                  pva->pvaChannelNames[j].c_str(),
                  (int)logger->scalarArrayStartIndex[j],
                  (int)logger->expectElements[j]);
          return (1);
        }
      }
      logger->verifiedType[j] = true;
    }
  }
  if (logger->truncateWaveforms) {
    if (preMinElements != minElements) {
      for (j=0; j<pva->numPVs; j++) {
        if (logger->expectElements[j] > 1) {
          logger->expectElements[j] = minElements;
        }
      }
    }
  }
  return (0);
}

long VerifyNumericScalarPVTypes(PVA_OVERALL *pva) {
  long j;

  if (pva == NULL) {
    return (0);
  }
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->isConnected[j] == true) {
      if (pva->pvaData[j].numeric == false) {
        fprintf(stderr, "Error (sddspvalogger): %s is not numeric\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
      if ((pva->pvaData[j].fieldType != epics::pvData::scalar) && (pva->pvaData[j].pvEnumeratedStructure == false)) {
        fprintf(stderr, "Error (sddspvalogger): %s is not scalar\n", pva->pvaChannelNames[j].c_str());
        return (1);
      }
    } else {
      /*
          fprintf(stderr, "Error (sddspvalogger): %s is not connected\n", pva->pvaChannelNames[j].c_str());
          return (1);
*/
    }
  }
  return (0);
}

long VerifyFileIsAppendable(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  int sddstype;
  long j;
  int32_t numParameters = 0, numColumns = 0, numArrays = 0;
  if (!SDDS_InitializeInput(SDDS_table, logger->outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  if (logger->scalarsAsColumns) {
    if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, (char *)"CAerrors", NULL, SDDS_LONG, stderr)) ||
        (SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, (char *)"Time", NULL, SDDS_DOUBLE, stderr))) {
      return (1);
    }
    logger->caErrorsIndex = SDDS_GetColumnIndex(SDDS_table, (char *)"CAerrors");
    logger->timeIndex = SDDS_GetColumnIndex(SDDS_table, (char *)"Time");
    numColumns += 2;

    if (logger->onePv_OutputDirectory == NULL) {
      if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, (char *)"Step", NULL, SDDS_LONG, stderr)) ||
          (SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, (char *)"TimeOfDay", NULL, SDDS_FLOAT, stderr)) ||
          (SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, (char *)"DayOfMonth", NULL, SDDS_FLOAT, stderr))) {
        return (1);
      }
      logger->stepIndex = SDDS_GetColumnIndex(SDDS_table, (char *)"Step");
      logger->timeofdayIndex = SDDS_GetColumnIndex(SDDS_table, (char *)"TimeOfDay");
      logger->dayofmonthIndex = SDDS_GetColumnIndex(SDDS_table, (char *)"DayOfMonth");
      numColumns += 3;
    }
  } else {
    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, (char *)"CAerrors", NULL, SDDS_LONG, stderr)) ||
        (SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, (char *)"Time", NULL, SDDS_DOUBLE, stderr))) {
      return (1);
    }
    logger->caErrorsIndex = SDDS_GetParameterIndex(SDDS_table, (char *)"CAerrors");
    logger->timeIndex = SDDS_GetParameterIndex(SDDS_table, (char *)"Time");
    numParameters += 2;

    if (logger->onePv_OutputDirectory == NULL) {
      if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, (char *)"Step", NULL, SDDS_LONG, stderr)) ||
          (SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, (char *)"TimeOfDay", NULL, SDDS_FLOAT, stderr)) ||
          (SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, (char *)"DayOfMonth", NULL, SDDS_FLOAT, stderr))) {
        return (1);
      }
      logger->stepIndex = SDDS_GetParameterIndex(SDDS_table, (char *)"Step");
      logger->timeofdayIndex = SDDS_GetParameterIndex(SDDS_table, (char *)"TimeOfDay");
      logger->dayofmonthIndex = SDDS_GetParameterIndex(SDDS_table, (char *)"DayOfMonth");
      numParameters += 3;
    }
  }
  for (j = 0; j < pva->numPVs; j++) {
    if (logger->expectNumeric[j]) {
      sddstype = SDDS_DOUBLE;
    } else {
      sddstype = SDDS_STRING;
    }
    if (logger->storageType) {
      if (logger->storageType[j] != 0) {
        sddstype = logger->storageType[j];
      }
    }
    if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
      if (logger->scalarsAsColumns) {
        if (SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, logger->readbackName[j], NULL, sddstype, stderr)) {
          return (1);
        }
        logger->elementIndex[j] = SDDS_GetColumnIndex(SDDS_table, logger->readbackName[j]);
        numColumns++;
      } else {
        if (SDDS_CHECK_OKAY != SDDS_CheckParameter(SDDS_table, logger->readbackName[j], NULL, sddstype, stderr)) {
          return (1);
        }
        logger->elementIndex[j] = SDDS_GetParameterIndex(SDDS_table, logger->readbackName[j]);
        numParameters++;
      }
    } else if (logger->expectScalarArray[j]) {
      if (logger->scalarArraysAsColumns) {
        if (SDDS_CHECK_OKAY != SDDS_CheckColumn(SDDS_table, logger->readbackName[j], NULL, sddstype, stderr)) {
          return (1);
        }
        logger->elementIndex[j] = SDDS_GetColumnIndex(SDDS_table, logger->readbackName[j]);
        numColumns++;
      } else {
        if (SDDS_CHECK_OKAY != SDDS_CheckArray(SDDS_table, logger->readbackName[j], NULL, sddstype, stderr)) {
          return (1);
        }
        logger->elementIndex[j] = SDDS_GetArrayIndex(SDDS_table, logger->readbackName[j]);
        numArrays++;
      }
    }
  }
  if (SDDS_ParameterCount(SDDS_table) != numParameters) {
    fprintf(stderr, "Error: Unable to append to output file. Unexpected parameters exist\n");
  }
  if (SDDS_ColumnCount(SDDS_table) != numColumns) {
    fprintf(stderr, "Error: Unable to append to output file. Unexpected columns exist\n");
  }
  if (SDDS_ArrayCount(SDDS_table) != numArrays) {
    fprintf(stderr, "Error: Unable to append to output file. Unexpected arrays exist\n");
  }

  if (!SDDS_Terminate(SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  return (0);
}

long ReadInputFiles(LOGGER_DATA *logger) {
  SDDS_TABLE SDDS_table;
  int64_t n;
  char *UnitsColumnName = NULL;
  char *charColumn;
  char **stringColumn;
  bool ExpectNumericExists = false;
  bool StorageTypeExists = false;
  bool ScalarArrayStartIndexExists = false;
  bool ScalarArrayEndIndexExists = false;

  if (!SDDS_InitializeInput(&SDDS_table, logger->inputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"ControlName", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing ControlName in input file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"Provider", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing Provider in input file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL)) {
    ExpectNumericExists = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"StorageType", NULL, SDDS_STRING, NULL)) {
    StorageTypeExists = true;
  }
  if ((ExpectNumericExists == false) && (StorageTypeExists == false)) {
    fprintf(stderr, "Missing ExpectNumeric and StorageType in input file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"ExpectFieldType", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing ExpectFieldType in input file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"ExpectElements", NULL, SDDS_LONG, stderr)) {
    fprintf(stderr, "Missing ExpectElements in input file\n");
    return (1);
  }
  if (0 >= SDDS_ReadPage(&SDDS_table)) {
    fprintf(stderr, "No data in file\n");
    return (1);
  }
  logger->pvCount = SDDS_CountRowsOfInterest(&SDDS_table);
  if (logger->pvCount == 0) {
    fprintf(stderr, "Zero rows defined in input file.\n");
    return (1);
  }
  logger->controlName = (char **)SDDS_GetColumn(&SDDS_table, (char *)"ControlName");
  logger->provider = (char **)SDDS_GetColumn(&SDDS_table, (char *)"Provider");
  logger->expectElements = (int32_t *)SDDS_GetColumn(&SDDS_table, (char *)"ExpectElements");

  logger->expectNumeric = (bool *)malloc(sizeof(bool) * logger->pvCount);
  if (ExpectNumericExists) {
    charColumn = (char *)SDDS_GetColumn(&SDDS_table, (char *)"ExpectNumeric");
    for (n = 0; n < logger->pvCount; n++) {
      if ((charColumn[n] == 'y') || (charColumn[n] == 'Y')) {
        logger->expectNumeric[n] = true;
      } else {
        logger->expectNumeric[n] = false;
      }
    }
    free(charColumn);
  } else {
    for (n = 0; n < logger->pvCount; n++) {
      logger->expectNumeric[n] = true;
    }
  }
  
  stringColumn = (char **)SDDS_GetColumn(&SDDS_table, (char *)"ExpectFieldType");
  logger->expectScalar = (bool *)malloc(sizeof(bool) * logger->pvCount);
  logger->expectScalarArray = (bool *)malloc(sizeof(bool) * logger->pvCount);
  logger->treatScalarArrayAsScalar = (bool *)malloc(sizeof(bool) * logger->pvCount);
  for (n = 0; n < logger->pvCount; n++) {
    logger->treatScalarArrayAsScalar[n] = false;
    if (strcmp(stringColumn[n], (char *)"scalarArray") == 0) {
      logger->expectScalar[n] = false;
      logger->expectScalarArray[n] = true;
    } else if (strcmp(stringColumn[n], (char *)"scalar") == 0) {
      logger->expectScalar[n] = true;
      logger->expectScalarArray[n] = false;
    } else {
      fprintf(stderr, "ExpectFieldType column only accepts scalar or scalarArray\n");
      return (1);
    }
    free(stringColumn[n]);
  }
  free(stringColumn);

  logger->storageType = (short *)malloc(sizeof(short) * logger->pvCount);
  if (StorageTypeExists) {
    stringColumn = (char **)SDDS_GetColumn(&SDDS_table, (char *)"StorageType");
    for (n = 0; n < logger->pvCount; n++) {
      if (strcmp(stringColumn[n], (char *)"longdouble") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_LONGDOUBLE;
      } else if (strcmp(stringColumn[n], (char *)"double") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_DOUBLE;
      } else if (strcmp(stringColumn[n], (char *)"float") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_FLOAT;
      } else if (strcmp(stringColumn[n], (char *)"long64") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_LONG64;
      } else if (strcmp(stringColumn[n], (char *)"ulong64") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_ULONG64;
      } else if ((strcmp(stringColumn[n], (char *)"long") == 0) || (strcmp(stringColumn[n], (char *)"int") == 0)) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_LONG;
      } else if ((strcmp(stringColumn[n], (char *)"ulong") == 0) || (strcmp(stringColumn[n], (char *)"uint") == 0)) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_ULONG;
      } else if ((strcmp(stringColumn[n], (char *)"short") == 0) || (strcmp(stringColumn[n], (char *)"byte") == 0)) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_SHORT;
      } else if (strcmp(stringColumn[n], (char *)"ushort") == 0) {
        logger->expectNumeric[n] = true;
        logger->storageType[n] = SDDS_USHORT;
      } else if (strcmp(stringColumn[n], (char *)"string") == 0) {
        logger->expectNumeric[n] = false;
        logger->storageType[n] = SDDS_STRING;
      } else if (strcmp(stringColumn[n], (char *)"character") == 0) {
        if (logger->expectScalarArray[n]) {
          fprintf(stderr, "StorageType character is not supported for character arrays yet. Use string for now.\n");
          return (1);
        }
        logger->expectNumeric[n] = false;
        logger->storageType[n] = SDDS_CHARACTER;
      } else if (strcmp(stringColumn[n], (char *)"") == 0) {
        logger->storageType[n] = 0;
      } else {
        fprintf(stderr, "StorageType has an invalid type (%s)\n", stringColumn[n]);
        return (1);
      }
      free(stringColumn[n]);
    }
    free(stringColumn);
  } else {
    for (n = 0; n < logger->pvCount; n++) {
      logger->storageType[n] = 0;
    }
  }

  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"ReadbackName", NULL, SDDS_STRING, NULL)) {
    logger->readbackName = logger->controlName;
  } else {
    logger->readbackName = (char **)SDDS_GetColumn(&SDDS_table, (char *)"ReadbackName");
  }

  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    logger->scaleFactor = SDDS_GetColumnInDoubles(&SDDS_table, (char *)"ScaleFactor");
  }

  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"ScalarArrayStartIndex", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    ScalarArrayStartIndexExists = true;
    logger->scalarArrayStartIndex = SDDS_GetColumnInLong(&SDDS_table, (char *)"ScalarArrayStartIndex");
  }

  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"ScalarArrayEndIndex", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    ScalarArrayEndIndexExists = true;
    logger->scalarArrayEndIndex = SDDS_GetColumnInLong(&SDDS_table, (char *)"ScalarArrayEndIndex");
  }

  /* If user specifies slicing, require both columns so bounds are well-defined. */
  if (ScalarArrayStartIndexExists != ScalarArrayEndIndexExists) {
    fprintf(stderr, "Error (sddspvalogger): ScalarArrayStartIndex and ScalarArrayEndIndex must be provided together\n");
    return (1);
  }

  /* Validate slice bounds against ExpectElements from the input file. */
  if (ScalarArrayStartIndexExists && ScalarArrayEndIndexExists) {
    for (n = 0; n < logger->pvCount; n++) {
      if (logger->expectScalarArray[n]) {
        if (logger->scalarArrayStartIndex[n] < 1 ||
            logger->scalarArrayEndIndex[n] < logger->scalarArrayStartIndex[n] ||
            logger->scalarArrayEndIndex[n] > logger->expectElements[n]) {
          fprintf(stderr, "Error (sddspvalogger): Invalid ScalarArrayStartIndex/EndIndex for PV %s (start=%d, end=%d, ExpectElements=%d)\n",
                  logger->controlName[n],
                  (int)logger->scalarArrayStartIndex[n],
                  (int)logger->scalarArrayEndIndex[n],
                  (int)logger->expectElements[n]);
          return (1);
        }
      }
    }
  }

  // Mark single-element scalarArrays to be treated as scalars
  for (n = 0; n < logger->pvCount; n++) {
    if (logger->expectScalarArray[n]) {
      int32_t effectiveElements = logger->expectElements[n];
      if ((logger->scalarArrayStartIndex != NULL) && (logger->scalarArrayEndIndex != NULL)) {
        effectiveElements = logger->scalarArrayEndIndex[n] - logger->scalarArrayStartIndex[n] + 1;
      }
      if (effectiveElements == 1) {
        logger->treatScalarArrayAsScalar[n] = true;
      }
    }
  }

  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"Average", NULL, SDDS_CHARACTER, NULL)) {
    logger->average = (char *)SDDS_GetColumn(&SDDS_table, (char *)"Average");
  }

  UnitsColumnName = SDDS_FindColumn(&SDDS_table, FIND_SPECIFIED_TYPE, SDDS_STRING,
                                    "ReadbackUnits", "ReadbackUnit", "Units", NULL);
  if (UnitsColumnName) {
    logger->units = (char **)SDDS_GetColumn(&SDDS_table, UnitsColumnName);
    free(UnitsColumnName);
  }

  //Not sure if I need to setup DeadBand column code. I may add this in the future.

  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  //Read conditions file
  if (logger->CondFile) {
    if (ReadConditionsFile(logger) == 1) {
      return (1);
    }
  }

  //Read trigger file
  if (logger->trigFileSet) {
    if (ReadGlitchTriggerFile(logger) == 1) {
      return (1);
    }
  }

  //Check if input file contents make sense
  if (CheckInputFileValidity(logger) != 0) {
    return (1);
  }

  return (0);
}

long ReadConditionsFile(LOGGER_DATA *logger) {
  SDDS_TABLE SDDS_table;
  if (!SDDS_InitializeInput(&SDDS_table, logger->CondFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"ControlName", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing ControlName in conditions file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"Provider", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing Provider in conditions file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"LowerLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
    fprintf(stderr, "Missing LowerLimit in conditions file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"UpperLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
    fprintf(stderr, "Missing UpperLimit in conditions file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    logger->conditions_ScaleFactorDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"HoldoffTime", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    logger->conditions_HoldoffTimeDefined = true;
  }
  if (0 >= SDDS_ReadPage(&SDDS_table)) {
    fprintf(stderr, "No data in conditions file\n");
    return (1);
  }
  logger->conditions_pvCount = SDDS_CountRowsOfInterest(&SDDS_table);
  if (logger->conditions_pvCount == 0) {
    fprintf(stderr, "Zero rows defined in conditions file.\n");
    return (1);
  }
  logger->conditions_controlName = (char **)SDDS_GetColumn(&SDDS_table, (char *)"ControlName");
  if (logger->conditions_controlName == NULL) {
    fprintf(stderr, "Unable to find ControlName column\n");
    return (1);
  }
  logger->conditions_provider = (char **)SDDS_GetColumn(&SDDS_table, (char *)"Provider");
  if (logger->conditions_provider == NULL) {
    fprintf(stderr, "Unable to find Provider column\n");
    return (1);
  }
  logger->conditions_lowerLimit = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"LowerLimit");
  if (logger->conditions_lowerLimit == NULL) {
    fprintf(stderr, "Unable to find LowerLimit column\n");
    return (1);
  }
  logger->conditions_upperLimit = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"UpperLimit");
  if (logger->conditions_upperLimit == NULL) {
    fprintf(stderr, "Unable to find UpperLimit column\n");
    return (1);
  }
  if (logger->conditions_ScaleFactorDefined) {
    logger->conditions_scaleFactor = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"ScaleFactor");
    if (logger->conditions_scaleFactor == NULL) {
      fprintf(stderr, "Unable to find ScaleFactor column\n");
      return (1);
    }
  }
  if (logger->conditions_HoldoffTimeDefined) {
    logger->conditions_holdoffTime = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"HoldoffTime");
    if (logger->conditions_holdoffTime == NULL) {
      fprintf(stderr, "Unable to find HoldoffTime column\n");
      return (1);
    }
  }

  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  return (0);
}

long ReadGlitchTriggerFile(LOGGER_DATA *logger) {
  SDDS_TABLE SDDS_table;
  long i;
  if (!SDDS_InitializeInput(&SDDS_table, logger->triggerFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"TriggerControlName", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing TriggerControlName in trigger file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY != SDDS_CheckColumn(&SDDS_table, (char *)"Provider", NULL, SDDS_STRING, stderr)) {
    fprintf(stderr, "Missing Providere in trigger file\n");
    return (1);
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"MajorAlarm", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_MajorAlarmDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"MinorAlarm", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_MinorAlarmDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"NoAlarm", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_NoAlarmDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"GlitchThreshold", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    logger->glitch_GlitchThresholdDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"GlitchBaselineSamples", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_GlitchBaselineSamplesDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"GlitchBaselineAutoReset", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_GlitchBaselineAutoResetDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"GlitchScript", NULL, SDDS_STRING, NULL)) {
    logger->glitch_GlitchScriptDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"TransitionThreshold", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    logger->glitch_TransitionThresholdDefined = true;
  }
  if (SDDS_CHECK_OKAY == SDDS_CheckColumn(&SDDS_table, (char *)"TransitionDirection", NULL, SDDS_ANY_INTEGER_TYPE, NULL)) {
    logger->glitch_TransitionDirectionDefined = true;
  }
  if (0 >= SDDS_ReadPage(&SDDS_table)) {
    fprintf(stderr, "No data in trigger file\n");
    return (1);
  }
  logger->glitch_pvCount = SDDS_CountRowsOfInterest(&SDDS_table);
  if (logger->glitch_pvCount == 0) {
    fprintf(stderr, "Zero rows defined in trigger file.\n");
    return (1);
  }
  logger->glitch_controlName = (char **)SDDS_GetColumn(&SDDS_table, (char *)"TriggerControlName");
  if (logger->glitch_controlName == NULL) {
    fprintf(stderr, "Unable to find TriggerControlName column\n");
    return (1);
  }
  logger->glitch_provider = (char **)SDDS_GetColumn(&SDDS_table, (char *)"Provider");
  if (logger->glitch_provider == NULL) {
    fprintf(stderr, "Unable to find Provider column\n");
    return (1);
  }
  if (logger->glitch_MajorAlarmDefined) {
    logger->glitch_majorAlarm = (short *)SDDS_GetColumnInShort(&SDDS_table, (char *)"MajorAlarm");
    if (logger->glitch_majorAlarm == NULL) {
      fprintf(stderr, "Unable to find MajorAlarm column\n");
      return (1);
    }
  }
  if (logger->glitch_MinorAlarmDefined) {
    logger->glitch_minorAlarm = (short *)SDDS_GetColumnInShort(&SDDS_table, (char *)"MinorAlarm");
    if (logger->glitch_minorAlarm == NULL) {
      fprintf(stderr, "Unable to find MinorAlarm column\n");
      return (1);
    }
  }
  if (logger->glitch_NoAlarmDefined) {
    logger->glitch_noAlarm = (short *)SDDS_GetColumnInShort(&SDDS_table, (char *)"NoAlarm");
    if (logger->glitch_noAlarm == NULL) {
      fprintf(stderr, "Unable to find NoAlarm column\n");
      return (1);
    }
  }
  if (logger->glitch_GlitchThresholdDefined) {
    logger->glitch_glitchThreshold = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"GlitchThreshold");
    if (logger->glitch_glitchThreshold == NULL) {
      fprintf(stderr, "Unable to find GlitchThreshold column\n");
      return (1);
    }
  }
  if (logger->glitch_GlitchBaselineSamplesDefined) {
    logger->glitch_glitchBaselineSamples = (int32_t *)SDDS_GetColumnInLong(&SDDS_table, (char *)"GlitchBaselineSamples");
    if (logger->glitch_glitchBaselineSamples == NULL) {
      fprintf(stderr, "Unable to find GlitchBaselineSamples column\n");
      return (1);
    }
  }
  if (logger->glitch_GlitchBaselineAutoResetDefined) {
    logger->glitch_glitchBaselineAutoReset = (short *)SDDS_GetColumnInShort(&SDDS_table, (char *)"GlitchBaselineAutoReset");
    if (logger->glitch_glitchBaselineAutoReset == NULL) {
      fprintf(stderr, "Unable to find GlitchBaselineAutoReset column\n");
      return (1);
    }
  }
  if (logger->glitch_GlitchScriptDefined) {
    logger->glitch_glitchScript = (char **)SDDS_GetColumn(&SDDS_table, (char *)"GlitchScript");
    if (logger->glitch_glitchScript == NULL) {
      fprintf(stderr, "Unable to find GlitchScript column\n");
      return (1);
    }
  }
  if (logger->glitch_TransitionThresholdDefined) {
    logger->glitch_transitionThreshold = (double *)SDDS_GetColumnInDoubles(&SDDS_table, (char *)"TransitionThreshold");
    if (logger->glitch_transitionThreshold == NULL) {
      fprintf(stderr, "Unable to find TransitionThreshold column\n");
      return (1);
    }
  }
  if (logger->glitch_TransitionDirectionDefined) {
    logger->glitch_transitionDirection = (short *)SDDS_GetColumnInShort(&SDDS_table, (char *)"TransitionDirection");
    if (logger->glitch_transitionDirection == NULL) {
      fprintf(stderr, "Unable to find TransitionDirection column\n");
      return (1);
    }
  }

  if (!SDDS_Terminate(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return (1);
  }

  if (logger->glitch_TransitionThresholdDefined && logger->glitch_TransitionDirectionDefined) {
    logger->glitch_triggerIndex = (int32_t *)malloc(sizeof(int32_t) * logger->glitch_pvCount);
    logger->glitch_triggered = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_triggeredTemp = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_transitionLastValue = (double *)malloc(sizeof(double) * logger->glitch_pvCount);
    for (i = 0; i < logger->glitch_pvCount; i++) {
      logger->glitch_transitionLastValue[i] = DBL_MAX;
      logger->glitch_triggeredTemp[i] = 0;
    }
  }
  if (logger->glitch_GlitchThresholdDefined && logger->glitch_GlitchBaselineSamplesDefined) {
    logger->glitch_glitchIndex = (int32_t *)malloc(sizeof(int32_t) * logger->glitch_pvCount);
    logger->glitch_glitched = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_glitchedTemp = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_glitchBaselineSamplesRead = (long *)malloc(sizeof(long) * logger->glitch_pvCount);
    logger->glitch_glitchBaselineSampleValues = (double **)malloc(sizeof(double *) * logger->glitch_pvCount);
    for (i = 0; i < logger->glitch_pvCount; i++) {
      logger->glitch_glitchBaselineSamplesRead[i] = 0;
      logger->glitch_glitchBaselineSampleValues[i] = (double *)malloc(sizeof(double) * logger->glitch_glitchBaselineSamples[i]);
      logger->glitch_glitchedTemp[i] = 0;
    }
  }
  if (logger->glitch_MajorAlarmDefined || logger->glitch_MinorAlarmDefined || logger->glitch_NoAlarmDefined) {
    logger->glitch_alarmIndex = (int32_t *)malloc(sizeof(int32_t) * logger->glitch_pvCount);
    logger->glitch_alarmSeverityIndex = (int32_t *)malloc(sizeof(int32_t) * logger->glitch_pvCount);
    logger->glitch_alarmed = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_alarmedTemp = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_alarmSeverity = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_alarmSeverityTemp = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    logger->glitch_alarmLastValue = (short *)malloc(sizeof(short) * logger->glitch_pvCount);
    for (i = 0; i < logger->glitch_pvCount; i++) {
      logger->glitch_alarmLastValue[i] = -1;
      logger->glitch_alarmedTemp[i] = 0;
      logger->glitch_alarmSeverityTemp[i] = 0;
    }
  }

  return (0);
}

bool PassesConditionsPVA(PVA_OVERALL *pvaConditions, LOGGER_DATA *logger) {
  long passed = 0, j;
  double value, maxHoldoff = 0;
  bool result = true;

  if (pvaConditions != NULL) {
    for (j = 0; j < pvaConditions->numPVs; j++) {
      if (pvaConditions->isConnected[j] == true) {
        if (logger->monitor) {
          value = pvaConditions->pvaData[j].monitorData[0].values[0];
        } else {
          value = pvaConditions->pvaData[j].getData[0].values[0];
        }
        if (logger->conditions_ScaleFactorDefined) {
          value *= logger->conditions_scaleFactor[j];
        }
        if ((logger->conditions_lowerLimit[j] <= value) &&
            (logger->conditions_upperLimit[j] >= value)) {
          passed++;
        } else {
          if (logger->conditions_HoldoffTimeDefined) {
            if (logger->conditions_holdoffTime[j] > maxHoldoff) {
              maxHoldoff = logger->conditions_holdoffTime[j];
            }
          }
        }
      }
    }
    if (((logger->CondMode & MUST_PASS_ALL) && (pvaConditions->numPVs != passed)) ||
        ((logger->CondMode & MUST_PASS_ONE) && (passed == 0))) {
     if (maxHoldoff > 0) {
        if (pvaThreadSleep(maxHoldoff) == 1) {
          //The run control ping failed in pvaThreadSleep. Returning false here will cause another ping to occur and exit the program properly.
        }
      }
      result = false;
      if (logger->verbose) {
        if (logger->CondMode & RETAKE_STEP) {
          fprintf(stdout, "Failed conditions test. PV logging paused.\n");
        } else {
          fprintf(stdout, "Failed conditions test. PV logging skipped.\n");
        }
      }
      if (logger->CondMode & TOUCH_OUTPUT) {
        TouchFile(logger->outputfile);
      }
      if (logger->CondMode & RETAKE_STEP) {
        logger->step--;
      }
    }
  }
  return (result);
}

bool CheckGlitchPVA(PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger) {
  long i, j;
  double value, baseline;
  bool result = false, pass;
  if (pvaGlitch != NULL) {
    for (i = 0; i < pvaGlitch->numPVs; i++) {
      logger->glitch_triggeredTemp[i] = 0;
      logger->glitch_glitchedTemp[i] = 0;
      logger->glitch_alarmedTemp[i] = 0;
      logger->glitch_alarmSeverityTemp[i] = 0;
      if (pvaGlitch->isConnected[i] == true) {
        if ((logger->monitor) && (pvaGlitch->pvaData[i].numMonitorReadings > 0)) {
          value = pvaGlitch->pvaData[i].monitorData[0].values[0];
        } else {
          value = pvaGlitch->pvaData[i].getData[0].values[0];
        }
        if (logger->glitch_TransitionThresholdDefined && logger->glitch_TransitionDirectionDefined && (logger->glitch_transitionDirection[i] != 0)) {
          if (logger->glitch_transitionLastValue[i] == DBL_MAX) {
            //logger->glitch_transitionLastValue[i] = value;
          } else if (((logger->glitch_transitionDirection[i] > 0) &&
                      (logger->glitch_transitionLastValue[i] <= logger->glitch_transitionThreshold[i]) &&
                      (value > logger->glitch_transitionThreshold[i])) ||
                     ((logger->glitch_transitionDirection[i] < 0) &&
                      (logger->glitch_transitionLastValue[i] >= logger->glitch_transitionThreshold[i]) &&
                      (value < logger->glitch_transitionThreshold[i]))) {
            result = true;
            logger->glitch_triggeredTemp[i] = 1;
          }
          logger->glitch_transitionLastValue[i] = value;
        }
        if (logger->glitch_MajorAlarmDefined || logger->glitch_MinorAlarmDefined || logger->glitch_NoAlarmDefined) {
          if (logger->glitch_alarmLastValue[i] == -1) {
            //logger->glitch_alarmLastValue[i] = pvaGlitch->pvaData[i].alarmSeverity;
          } else if (logger->glitch_alarmLastValue[i] != pvaGlitch->pvaData[i].alarmSeverity) {
            if (((pvaGlitch->pvaData[i].alarmSeverity == 0) && (logger->glitch_NoAlarmDefined) && (logger->glitch_noAlarm[i] != 0)) ||
                ((pvaGlitch->pvaData[i].alarmSeverity == 1) && (logger->glitch_MinorAlarmDefined) && (logger->glitch_minorAlarm[i] != 0)) ||
                ((pvaGlitch->pvaData[i].alarmSeverity == 2) && (logger->glitch_MajorAlarmDefined) && (logger->glitch_majorAlarm[i] != 0))) {
              result = true;
              logger->glitch_alarmedTemp[i] = 1;
              logger->glitch_alarmSeverityTemp[i] = pvaGlitch->pvaData[i].alarmSeverity;
            }
          }
          logger->glitch_alarmLastValue[i] = pvaGlitch->pvaData[i].alarmSeverity;
        }
        if (logger->glitch_GlitchThresholdDefined && logger->glitch_GlitchBaselineSamplesDefined && (logger->glitch_glitchThreshold[i] != 0) && (logger->glitch_glitchBaselineSamples[i] > 0)) {
          if (logger->glitch_glitchBaselineSamplesRead[i] < logger->glitch_glitchBaselineSamples[i]) {
            logger->glitch_glitchBaselineSampleValues[i][logger->glitch_glitchBaselineSamplesRead[i]] = value / logger->glitch_glitchBaselineSamples[i];
            logger->glitch_glitchBaselineSamplesRead[i]++;
          } else {
            baseline = logger->glitch_glitchBaselineSampleValues[i][0];
            for (j = 1; j < logger->glitch_glitchBaselineSamples[i]; j++) {
              baseline += logger->glitch_glitchBaselineSampleValues[i][j];
            }
            pass = true;
            if (logger->glitch_glitchThreshold[i] > 0) {
              if (fabs(value - baseline) > logger->glitch_glitchThreshold[i]) {
                pass = false;
              }
            } else {
              if (fabs(value - baseline) > logger->glitch_glitchThreshold[i] * -1 * baseline) {
                pass = false;
              }
            }
            if (pass == false) {
              result = true;
              logger->glitch_glitchedTemp[i] = 1;
              if (logger->glitch_GlitchBaselineAutoResetDefined && (logger->glitch_glitchBaselineAutoReset[i] != 0)) {
                logger->glitch_glitchBaselineSamplesRead[i] = 0;
              }
            } else {
              for (j = 0; j < logger->glitch_glitchBaselineSamples[i] - 1; j++) {
                logger->glitch_glitchBaselineSampleValues[i][j] = logger->glitch_glitchBaselineSampleValues[i][j + 1];
              }
              logger->glitch_glitchBaselineSampleValues[i][logger->glitch_glitchBaselineSamples[i] - 1] = value / logger->glitch_glitchBaselineSamples[i];
            }
          }
        }
      }
    }
  }
  if ((result) && (logger->verbose)) {
    fprintf(stdout, "Glitch event seen\n");
  }
  return (result);
}

bool PassesInhibitPVA(PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger) {
  double value;

  if (pvaInhibit->isConnected[0] == true) {
    value = pvaInhibit->pvaData[0].monitorData[0].values[0];
    if (value > .1) {
      return false;
    }
  }
  return true;
}

long StartPages(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j;
  int64_t rows;
  static bool pageStarted = false;
  if (logger->step == 0) {
    pageStarted = false;
  }
  if (logger->onePv_OutputDirectory != NULL) {
    for (j = 0; j < pva->numPVs; j++) {
      if (logger->expectScalarArray[j] && !logger->treatScalarArrayAsScalar[j]) {
        rows = logger->expectElements[j];
        if (logger->doDisconnect && !SDDS_ReconnectFile(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (!SDDS_StartPage(&(SDDS_table[j]), rows)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (logger->doDisconnect && !SDDS_DisconnectFile(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        logger->outputRow[j] = 0;
        logger->outputPage[j]++;
      }
    }
  } else if ((logger->scalarsAsColumns == false) || ((pageStarted == false) && (logger->append == false))) {
    if (!SDDS_StartPage(&(SDDS_table[0]), logger->n_rows)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    logger->outputRow[0] = 0;
    logger->outputPage[0]++;
    pageStarted = true;
  }
  return (0);
}

long UpdateAndWritePages(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j, n;
  double nStart, nEnd;

  if (logger->onePv_OutputDirectory != NULL) {
    n = logger->step % logger->flushInterval;
    nStart = n * logger->filesPerStep;
    nEnd = nStart + logger->filesPerStep;
    if (nEnd > pva->numPVs) {
      nEnd = pva->numPVs;
    }
    for (j = 0; j < pva->numPVs; j++) {
      if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
        if ((j >= nStart) && (j < nEnd)) {
          if (logger->doDisconnect && !SDDS_ReconnectFile(&(SDDS_table[j]))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (!SDDS_UpdatePage(&(SDDS_table[j]), FLUSH_TABLE)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          if (logger->doDisconnect && !SDDS_DisconnectFile(&(SDDS_table[j]))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
      } else if (logger->expectScalarArray[j]) {
        if (logger->doDisconnect && !SDDS_ReconnectFile(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (!SDDS_WriteTable(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        logger->outputPage[j]++;
        if (logger->doDisconnect && !SDDS_DisconnectFile(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
    }
  } else if ((logger->scalarsAsColumns) && (logger->outputRow[0] % logger->flushInterval == 0)) {
    if (!SDDS_UpdatePage(&(SDDS_table[0]), FLUSH_TABLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  } else if (logger->scalarsAsColumns == false) {
    if (!SDDS_WriteTable(&(SDDS_table[0]))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
    logger->outputPage[0]++;
  }
  return (0);
}

long CloseFiles(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  long j, result = 0;
  if (logger->onePv_OutputDirectory != NULL) {
    for (j = 0; j < pva->numPVs; j++) {
      if (logger->expectScalar[j] || logger->treatScalarArrayAsScalar[j]) {
        if (logger->doDisconnect && !SDDS_ReconnectFile(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          result = 1;
          continue;
        }
        if (!SDDS_UpdatePage(&(SDDS_table[j]), FLUSH_TABLE)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (!SDDS_WriteTable(&(SDDS_table[j]))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          result = 1;
          continue;
        }
      }
      if (!SDDS_Terminate(&(SDDS_table[j]))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        result = 1;
      }
    }
  } else {
    if (!SDDS_Terminate(&(SDDS_table[0]))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      result = 1;
    }
  }
  return (result);
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
  return (PingRunControl(NULL, 0, 0));
}

long PingRunControl(PVA_OVERALL *pva, long mode, long step) {
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
    if (mode == 1) {
      snprintf(rcParam.message, sizeof(rcParam.message), "Initializing %d percent", (int)(100 * step / pva->numPVs));
    } else {
      strcpy(rcParam.message, "Running");
    }
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      return (1);
    }
  }
  return (0);
}

void InitializeVariables(LOGGER_DATA *logger) {
  rcParam.PV = NULL;
  rcParam.Desc = NULL;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;

  logger->inputfile = NULL;
  logger->inputfileLastlink = NULL;
  logger->outputfile = NULL;
  logger->outputfileOrig = NULL;
  logger->scalarArraysAsColumns = true;
  logger->scalarsAsColumns = false;
  logger->emptyColumn = NULL;
  logger->emptyStringColumn = NULL;
  logger->n_rows = -1;
  logger->watchInput = false;
  logger->monitor = false;
  logger->monitorRandomTimedTrigger = false;
  logger->append = false;
  logger->overwrite = false;
  logger->outputRow = NULL;
  logger->outputPage = NULL;
  logger->sampleInterval = -1;
  logger->logInterval = 1;
  logger->flushInterval = 1;
  logger->datastrobe_name = NULL;
  logger->datastrobe_provider = NULL;
  logger->datastrobe_is_time = true;
  logger->datastrobe_holdoff = 0.0;
  logger->CondFile = NULL;
  logger->CondMode = 0;
  logger->conditions_ScaleFactorDefined = false;
  logger->conditions_HoldoffTimeDefined = false;
  logger->conditions_pvCount = 0;
  logger->conditions_controlName = NULL;
  logger->conditions_provider = NULL;
  logger->conditions_lowerLimit = NULL;
  logger->conditions_upperLimit = NULL;
  logger->conditions_scaleFactor = NULL;
  logger->conditions_holdoffTime = NULL;
  logger->onerrorindex = ONERROR_USEZERO;
  logger->onerror = NULL;
  logger->inhibit_name = NULL;
  logger->inhibit_provider = NULL;
  logger->inhibit_waittime = 5.0;
  logger->scaleFactor = NULL;
  logger->scalarArrayStartIndex = NULL;
  logger->scalarArrayEndIndex = NULL;
  logger->treatScalarArrayAsScalar = NULL;
  logger->units = NULL;
  logger->average = NULL;
  logger->averagedValues = NULL;
  logger->generations = false;
  logger->dailyFiles = false;
  logger->monthlyFiles = false;
  logger->generationsDigits = 0;
  logger->generationsRowLimit = -1;
  logger->generationsDelimiter = NULL;
  logger->generationsTimeLimitOrig = -1;
  logger->timetag = false;
  logger->dailyFilesVerbose = false;
  logger->monthlyFilesVerbose = false;
  logger->verbose = false;
  logger->onePv_OutputDirectory = NULL;
  logger->onePv_outputfile = NULL;
  logger->onePv_outputfileOrig = NULL;
  logger->mallocNeeded = true;
  logger->doDisconnect = false;
  logger->Nsteps = 0;
  logger->step = 0;
  logger->steps_set = false;
  logger->totalTimeSet = false;
  logger->TotalTime = 0;
  logger->pendIOtime = 10.0;
  logger->filesPerStep = 1;
  logger->NstepsAdjusted = 0;
  logger->triggerFile = NULL;
  logger->trigFileSet = false;
  logger->triggerFileLastlink = NULL;
  logger->circularbuffer_before = GLITCH_BEFORE_DEFAULT;
  logger->circularbuffer_after = GLITCH_AFTER_DEFAULT;
  logger->delay = 0;
  logger->defaultHoldoffTime = -1;
  logger->autoHoldoff = false;
  logger->strictPVverification = false;
  logger->truncateWaveforms = false;
  logger->storageType = NULL;
  logger->glitch_MajorAlarmDefined = false;
  logger->glitch_MinorAlarmDefined = false;
  logger->glitch_NoAlarmDefined = false;
  logger->glitch_GlitchThresholdDefined = false;
  logger->glitch_GlitchBaselineSamplesDefined = false;
  logger->glitch_GlitchBaselineAutoResetDefined = false;
  logger->glitch_GlitchScriptDefined = false;
  logger->glitch_TransitionThresholdDefined = false;
  logger->glitch_TransitionDirectionDefined = false;
  logger->glitch_pvCount = 0;
  logger->glitch_controlName = NULL;
  logger->glitch_provider = NULL;
  logger->glitch_majorAlarm = NULL;
  logger->glitch_minorAlarm = NULL;
  logger->glitch_noAlarm = NULL;
  logger->glitch_glitchThreshold = NULL;
  logger->glitch_glitchBaselineSamples = NULL;
  logger->glitch_glitchBaselineAutoReset = NULL;
  logger->glitch_glitchScript = NULL;
  logger->glitch_transitionThreshold = NULL;
  logger->glitch_transitionDirection = NULL;
  logger->glitch_step = -1;
  logger->circularbufferDouble = NULL;
  logger->circularbufferLongDouble = NULL;
  logger->circularbufferString = NULL;
  logger->glitch_alarmLastValue = NULL;
  logger->glitch_transitionLastValue = NULL;
  logger->glitch_glitchBaselineSamplesRead = NULL;
  logger->glitch_glitchBaselineSampleValues = NULL;
  logger->glitch_time = 0;
  logger->glitch_triggerIndex = NULL;
  logger->glitch_alarmIndex = NULL;
  logger->glitch_alarmSeverityIndex = NULL;
  logger->glitch_glitchIndex = NULL;
  logger->glitch_triggered = NULL;
  logger->glitch_alarmed = NULL;
  logger->glitch_alarmSeverity = NULL;
  logger->glitch_glitched = NULL;
  logger->glitch_triggeredTemp = NULL;
  logger->glitch_alarmedTemp = NULL;
  logger->glitch_alarmSeverityTemp = NULL;
  logger->glitch_glitchedTemp = NULL;
}

long ReadCommandLineArgs(LOGGER_DATA *logger, int argc, SCANNED_ARG *s_arg) {
  long i_arg, optionCode;
  long TimeUnits;
  unsigned long dummyFlags, dailyFilesFlags, monthlyFilesFlags, strobeFlags;

  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE);
    return (1);
  }

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (optionCode = match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_SAMPLEINTERVAL:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&(logger->sampleInterval), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -sampleInterval\n");
          return (1);
        }
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            logger->sampleInterval *= TimeUnitFactor[TimeUnits];
          else {
            fprintf(stderr, "unknown/ambiguous time units given for -sampleInterval\n");
            return (1);
          }
        }
        break;
      case CLO_LOGINTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&(logger->logInterval), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -logInterval\n");
          return (1);
        }
        if ((logger->logInterval < 1) || (logger->logInterval > 20)) {
          fprintf(stderr, "value for -logInterval must be between 1 and 20\n");
          return (1);
        }
        break;
      case CLO_FLUSHINTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&(logger->flushInterval), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -flushInterval\n");
          return (1);
        }
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&(logger->Nsteps), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -steps\n");
          return (1);
        }
        logger->steps_set = true;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&(logger->TotalTime), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -time\n");
          return (1);
        }
        logger->totalTimeSet = true;
        if (s_arg[i_arg].n_items == 3) {
          TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0);
          if (TimeUnits < 0) {
            fprintf(stderr, "unknown/ambiguous time units given for -time\n");
            return (1);
          }
          if (TimeUnits != 0) {
            logger->TotalTime *= TimeUnitFactor[TimeUnits];
          }
        }
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &(logger->pendIOtime)) != 1 ||
            logger->pendIOtime <= 0) {
          fprintf(stderr, "invalid -pendIOtime syntax\n");
          return (1);
        }
        break;
      case CLO_WATCHINPUT:
        logger->watchInput = true;
        break;
      case CLO_MONITOR:
        logger->monitor = true;
        if (s_arg[i_arg].n_items == 2) {
          if (strncmp((char*)"randomTimedTrigger", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            logger->monitorRandomTimedTrigger = true;
          } else {
            fprintf(stderr, "Invalid -monitorMode syntax!\n");
            return (1);
          }
        }
        break;
      case CLO_TRUNCATEWAVEFORMS:
        logger->truncateWaveforms = true;
        break;
      case CLO_STRICTPVVERIFICATION:
        logger->strictPVverification = true;
        break;
      case CLO_APPEND:
        logger->append = true;
        break;
      case CLO_OVERWRITE:
        logger->overwrite = true;
        break;
      case CLO_VERBOSE:
        logger->verbose = true;
        break;
      case CLO_DATASTROBEPV:
        if (s_arg[i_arg].n_items < 3) {
          fprintf(stderr, "Invalid -dataStrobePV syntax!\n");
          return (1);
        }
        logger->datastrobe_name = s_arg[i_arg].list[1];
        logger->datastrobe_provider = s_arg[i_arg].list[2];
        s_arg[i_arg].n_items -= 3;
        if (!scanItemList(&strobeFlags, s_arg[i_arg].list + 3, &s_arg[i_arg].n_items, 0,
                          "holdoff", SDDS_DOUBLE, &(logger->datastrobe_holdoff), 1, 0, 
                          "notTimeValue", -1, NULL, 0, STROBE_NOTTIMEVALUE,  
                          NULL)) {
          fprintf(stderr, "invalid -dataStrobePV syntax/values\n");
          return (1);
       }
        s_arg[i_arg].n_items += 3;
        if (strobeFlags & STROBE_NOTTIMEVALUE)
          logger->datastrobe_is_time = false;
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items < 3 ||
            SDDS_StringIsBlank(logger->CondFile = s_arg[i_arg].list[1]) ||
            !(logger->CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2))) {
          fprintf(stderr, "invalid -conditions syntax/values\n");
          return (1);
        }
        break;
      case CLO_ONERROR:
        if (s_arg[i_arg].n_items != 2 ||
            !(logger->onerror = s_arg[i_arg].list[1])) {
          fprintf(stderr, "No value given for option -onerror.\n");
          return (1);
        }
        switch (logger->onerrorindex = match_string(logger->onerror, onerror_options, ONERROR_OPTIONS, 0)) {
        case ONERROR_USEZERO:
        case ONERROR_SKIP:
        case ONERROR_EXIT:
          break;
        default:
          fprintf(stderr, "unrecognized onerror option given: %s\n", logger->onerror);
          return (1);
        }
        break;
      case CLO_INHIBIT:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &(logger->inhibit_name), 1, 0,
                          "provider", SDDS_STRING, &(logger->inhibit_provider), 1, 0,
                          "waittime", SDDS_DOUBLE, &(logger->inhibit_waittime), 1, 0,
                          NULL) ||
            (logger->inhibit_name == NULL) ||
            (logger->inhibit_provider == NULL) ||
            (strlen(logger->inhibit_name) == 0) ||
            (logger->inhibit_waittime <= 0)) {
          fprintf(stderr, "invalid -InhibitPV syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_GENERATIONS:
        if (logger->dailyFiles) {
          fprintf(stderr, "-dailyFiles and -generations are incompatible options\n");
          return (1);
        }
        if (logger->monthlyFiles) {
          fprintf(stderr, "-monthlyFiles and -generations are incompatible options\n");
          return (1);
        }
        logger->generations = true;
        logger->generationsDigits = DEFAULT_GENERATIONS_DIGITS;
        logger->generationsDelimiter = (char *)"-";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &(logger->generationsDigits), 1, 0,
                          "delimiter", SDDS_STRING, &(logger->generationsDelimiter), 1, 0,
                          "rowlimit", SDDS_LONG, &(logger->generationsRowLimit), 1, 0,
                          "timelimit", SDDS_DOUBLE, &(logger->generationsTimeLimitOrig), 1, 0,
                          NULL) ||
            logger->generationsDigits < 1) {
          fprintf(stderr, "invalid -generations syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DAILY_FILES:
        if (logger->monthlyFiles) {
          fprintf(stderr, "-dailyFiles and -monthlyFiles are incompatible options\n");
          return (1);
        }
        if (logger->generations) {
          fprintf(stderr, "-dailyFiles and -generations are incompatible options\n");
          return (1);
        }
        logger->dailyFiles = true;
        logger->generationsDigits = 4;
        logger->generationsDelimiter = (char *)".";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dailyFilesFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "rowlimit", SDDS_LONG, &(logger->generationsRowLimit), 1, 0,
                          "timelimit", SDDS_DOUBLE, &(logger->generationsTimeLimitOrig), 1, 0,
                          "timetag", -1, NULL, 0, USE_TIMETAG,
                          "verbose", -1, NULL, 0, DAILYFILES_VERBOSE,
                          NULL)) {
          fprintf(stderr, "invalid -dailyFiles syntax/values\n");
          return (1);
        }
        if (dailyFilesFlags & USE_TIMETAG)
          logger->timetag = true;
        if (dailyFilesFlags & DAILYFILES_VERBOSE)
          logger->dailyFilesVerbose = true;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_MONTHLY_FILES:
        if (logger->dailyFiles) {
          fprintf(stderr, "-dailyFiles and -monthlyFiles are incompatible options\n");
          return (1);
        }
        if (logger->generations) {
          fprintf(stderr, "-monthlyFiles and -generations are incompatible options\n");
          return (1);
        }
        logger->monthlyFiles = true;
        logger->generationsDigits = 4;
        logger->generationsDelimiter = (char *)".";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&monthlyFilesFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "rowlimit", SDDS_LONG, &(logger->generationsRowLimit), 1, 0,
                          "timelimit", SDDS_DOUBLE, &(logger->generationsTimeLimitOrig), 1, 0,
                          "timetag", -1, NULL, 0, USE_TIMETAG,
                          "verbose", -1, NULL, 0, MONTHLYFILES_VERBOSE,
                          NULL)) {
          fprintf(stderr, "invalid -monthlyFiles syntax/values\n");
          return (1);
        }
        if (monthlyFilesFlags & USE_TIMETAG)
          logger->timetag = true;
        if (monthlyFilesFlags & MONTHLYFILES_VERBOSE)
          logger->monthlyFilesVerbose = true;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_ONE_PV_PER_FILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Invalid -onePvPerFile syntax!\n");
          return (1);
        }
#if defined(_WIN32)
        fprintf(stderr, "-onePvPerFile option is not currently available on Windows\n");
        return (1);
#endif
        logger->onePv_OutputDirectory = s_arg[i_arg].list[1];
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
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
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
      case CLO_TRIGGERFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "invalid syntax for -triggerFile\n");
          return (1);
        }
        logger->triggerFile = s_arg[i_arg].list[1];
        logger->trigFileSet = true;
        logger->triggerFileLastlink = read_last_link_to_file(logger->triggerFile);
        if (get_file_stat(logger->triggerFile, logger->triggerFileLastlink, &(logger->triggerfilestat)) != 0) {
          fprintf(stderr, "Problem getting modification time for %s\n", logger->triggerFile);
          return (1);
        }
        break;
      case CLO_CIRCULARBUFFER:
        if (((s_arg[i_arg].n_items -= 1) < 0) ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "before", SDDS_LONG, &(logger->circularbuffer_before), 1, 0,
                          "after", SDDS_LONG, &(logger->circularbuffer_after), 1, 0,
                          NULL) ||
            (logger->circularbuffer_before < 0) ||
            (logger->circularbuffer_after < 0)) {
          fprintf(stderr, "invalid -circularBuffer syntax/values\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DELAY:
        if ((s_arg[i_arg].n_items != 2) ||
            (sscanf(s_arg[i_arg].list[1], "%d", &(logger->delay)) != 1) ||
            (logger->delay < 0)) {
          fprintf(stderr, "invalid -glitchDelay syntax/value\n");
          return (1);
        }
        break;
      case CLO_HOLDOFFTIME:
        if ((s_arg[i_arg].n_items != 2) ||
            (sscanf(s_arg[i_arg].list[1], "%lf", &(logger->defaultHoldoffTime)) != 1) ||
            (logger->defaultHoldoffTime < 0)) {
          fprintf(stderr, "invalid -holdoffTime syntax/value\n");
          return (1);
        }
        break;
      case CLO_AUTOHOLDOFF:
        logger->autoHoldoff = true;
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (logger->inputfile) {
        if (logger->outputfileOrig) {
          fprintf(stderr, "too many files given\n");
          return (1);
        }
        logger->outputfileOrig = s_arg[i_arg].list[0];
      } else {
        logger->inputfile = s_arg[i_arg].list[0];
        logger->inputfileLastlink = read_last_link_to_file(logger->inputfile);
        if (get_file_stat(logger->inputfile, logger->inputfileLastlink, &(logger->filestat)) != 0) {
          fprintf(stderr, "Problem getting modification time for %s\n", logger->inputfile);
          return (1);
        }
      }
    }
  }

  //Check if command line options given make sense
  if (CheckCommandLineArgsValidity(logger) != 0) {
    return (1);
  }

  return (0);
}

long CheckCommandLineArgsValidity(LOGGER_DATA *logger) {
  if (!logger->inputfile) {
    fprintf(stderr, "no input file given\n");
    return (1);
  }

  if (logger->onePv_OutputDirectory != NULL) {
    if (logger->outputfileOrig != NULL) {
      fprintf(stderr, "Do not include an output file when using the -onePvPerFile mode\n");
      return (1);
    }
  } else {
    if (!logger->outputfileOrig) {
      fprintf(stderr, "no output file given\n");
      return (1);
    }
  }
  if (logger->append && (logger->onePv_OutputDirectory != NULL)) {
    fprintf(stderr, "-append and -onePvPerFile are incompatible options\n");
    return (1);
  }
  if (logger->append && logger->overwrite) {
    fprintf(stderr, "-append and -overwrite are incompatible options\n");
    return (1);
  }
  if (logger->append && logger->generations) {
    fprintf(stderr, "-append and -generations are incompatible options\n");
    return (1);
  }
  if (logger->overwrite && logger->generations) {
    fprintf(stderr, "-overwrite and -generations are incompatible options\n");
    return (1);
  }
  if (logger->trigFileSet && (logger->onePv_OutputDirectory != NULL)) {
    fprintf(stderr, "-triggerFile and -onePvPerFile are incompatible options\n");
    return (1);
  }

  if ((logger->append || logger->generations) && (logger->dailyFiles || logger->monthlyFiles)) {
    logger->generationsDigits = 0;
    logger->generationsDelimiter = NULL;
    logger->generationsRowLimit = -1;
    logger->generationsTimeLimitOrig = -1;
  }
  logger->generationsTimeLimit = logger->generationsTimeLimitOrig;

  if (!logger->steps_set && !logger->totalTimeSet) {
    logger->Nsteps = INT_MAX;
    logger->steps_set = true;
  }
  if ((logger->sampleInterval < 0) && (logger->datastrobe_name == NULL)) {
    fprintf(stderr, "missing both -sampleInterval and -dataStrobePV options\n");
    return (1);
  }
  if (logger->totalTimeSet) {
    if (logger->steps_set) {
      printf("Option time supersedes option steps\n");
    }
    if (logger->datastrobe_name != NULL) {
      logger->Nsteps = INT_MAX;
    } else {
      logger->Nsteps = logger->TotalTime / logger->sampleInterval + 1;
    }
  }
  return (0);
}

long CheckInputFileValidity(LOGGER_DATA *logger) {
  long j;
  logger->scalarsAsColumns = true;
  for (j = 0; j < logger->pvCount; j++) {
    // Only set scalarsAsColumns to false for scalarArrays that are NOT being treated as scalars
    if (logger->expectScalarArray[j] && !logger->treatScalarArrayAsScalar[j]) {
      logger->scalarsAsColumns = false;
    }
  }
  if ((logger->logInterval > 1) && (logger->scalarsAsColumns == false) && (logger->average != NULL)) {
    for (j = 0; j < logger->pvCount; j++) {
      if ((logger->average[j] == 'y') || (logger->average[j] == 'Y')) {
        fprintf(stderr, "Averaging with scalarArray data isn't supported\n");
        return (1);
      }
    }
  }
  if ((logger->logInterval > 1) && (logger->average != NULL) && (logger->onePv_OutputDirectory != NULL)) {
    fprintf(stderr, "Averaging with one pv per file output isn't supported\n");
    return (1);
  }
  if ((logger->logInterval > 1) && (logger->average != NULL)) {
    logger->averagedValues = (double *)malloc(sizeof(double) * logger->pvCount);
    for (j = 0; j < logger->pvCount; j++) {
      logger->averagedValues[j] = 0;
    }
  }

  return (0);
}

void interrupt_handler(int sig) {
  /*
   * Async-signal-safe hard stop. We intentionally avoid exit(), stdio,
   * malloc/free, and any other non-async-signal-safe work here.
   */
  _Exit(128 + sig);
}

void sigint_interrupt_handler(int sig) {
  /*
   * First signal requests graceful shutdown (checked in main loop).
   * Subsequent signal forces an immediate hard stop.
   */
  if (sigint) {
    _Exit(128 + sig);
  }
  sigint = 1;
  sigintSignal = sig;
}

void rc_interrupt_handler() {
  if (rcParam.PV) {
    rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
  }
}

long SetupInhibitPV(PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger) {
  if (pvaInhibit != NULL) {
    //Allocate inhibit pva structure
    allocPVA(pvaInhibit, 1);
    epics::pvData::shared_vector<std::string> inhibitName(1);
    epics::pvData::shared_vector<std::string> inhibitProvider(1);
    inhibitName[0] = logger->inhibit_name;
    inhibitProvider[0] = logger->inhibit_provider;
    pvaInhibit->pvaChannelNames = freeze(inhibitName);
    pvaInhibit->pvaProvider = freeze(inhibitProvider);

    //Connect to inhibit PV
    ConnectPVA(pvaInhibit, 2.0);

    if (pvaInhibit->numNotConnected > 0) {
      fprintf(stdout, "Exiting due to connection error with inhibit PV.\n");
      return (1);
    }

    //Read inhibit PV
    if (GetPVAValues(pvaInhibit) == 1) {
      return (1);
    }

    //Make sure it is a numeric scalar value
    if (VerifyNumericScalarPVTypes(pvaInhibit) == 1) {
      return (1);
    }

    //Exit if inhibit PV isn't set to 0
    if (pvaInhibit->pvaData[0].getData[0].values[0] > .1) {
      if (logger->verbose) {
        fprintf(stdout, "Data collection inhibited--not starting\n");
      }
      exit(0);
    }

    //Setup monitoring for later
    if (MonitorPVAValues(pvaInhibit) == 1) {
      return (1);
    }
    if (pvaThreadSleep(.1) == 1) {
      return (1);
    }
    if (PollMonitoredPVA(pvaInhibit) == -1) {
      return (1);
    }
  }
  return (0);
}

/* Results: -1 error
   0 no inhibit seen
   1 inhibit seen
*/
long CheckInhibitPV(SDDS_TABLE *SDDS_table, PVA_OVERALL *pvaInhibit, LOGGER_DATA *logger) {
  if (pvaInhibit != NULL) {
    if (PollMonitoredPVA(pvaInhibit) == -1) {
      return (-1);
    }
    if (PassesInhibitPVA(pvaInhibit, logger) == false) {
      if (logger->CondMode & TOUCH_OUTPUT) {
        TouchFile(logger->outputfile);
      }
      if (logger->verbose) {
        printf("Inhibit PV %s is nonzero. Data collection inhibited.\n", logger->inhibit_name);
        fflush(stdout);
      }
      if (logger->CondMode & RETAKE_STEP) {
        logger->step--;
      }
      if (logger->inhibit_waittime > 0) {
        double waittime, timewaited = 0;
        if (rcParam.PV) {
          waittime = rcParam.pingTimeout / 1000.0 / 2.0;
          /* Guard against zero/negative wait intervals (would cause infinite loop). */
          if (waittime <= 0) {
            if (pvaThreadSleep(logger->inhibit_waittime) == 1) {
              return (-1);
            }
          } else {
            while (timewaited < logger->inhibit_waittime) {
              double remaining = logger->inhibit_waittime - timewaited;
              double slice = remaining > waittime ? waittime : remaining;
              if (pvaThreadSleep(slice) == 1) {
                return (-1);
              }
              timewaited += slice;
              /* Ping run control periodically while waiting. */
              if (timewaited + 1e-12 < logger->inhibit_waittime) {
                if (PingRunControl() != 0) {
                  return (-1);
                }
              }
            }
          }
        }
      }
      return (1);
    }
  }
  return (0);
}

long SetupPVs(PVA_OVERALL *pva, PVA_OVERALL *pvaConditions, PVA_OVERALL *pvaStrobe, PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger) {
  long j;
  if (logger->verbose) {
    fprintf(stdout, "Connecting to PVs\n");
  }
  if (pva != NULL) {
    //Allocate memory for pva structures
    allocPVA(pva, logger->pvCount);
    //List PV names
    epics::pvData::shared_vector<std::string> names(pva->numPVs);
    epics::pvData::shared_vector<std::string> providerNames(pva->numPVs);
    for (j = 0; j < pva->numPVs; j++) {
      names[j] = logger->controlName[j];
      providerNames[j] = logger->provider[j];
    }
    pva->pvaChannelNames = freeze(names);
    pva->pvaProvider = freeze(providerNames);
    ConnectPVA(pva, logger->pendIOtime);
    if (logger->verbose) {
      if (pva->numNotConnected > 0) {
        fprintf(stdout, "%ld of %ld PVs did not connect\n", pva->numNotConnected, pva->numPVs);
      } else {
        fprintf(stdout, "Connected to all %ld PVs\n", pva->numPVs);
      }
    }
    pva->limitGetReadings = true;
  }
  if (pvaConditions != NULL) {
    allocPVA(pvaConditions, logger->conditions_pvCount);
    epics::pvData::shared_vector<std::string> conditionsNames(logger->conditions_pvCount);
    epics::pvData::shared_vector<std::string> conditionsProviders(logger->conditions_pvCount);
    for (j = 0; j < logger->conditions_pvCount; j++) {
      conditionsNames[j] = logger->conditions_controlName[j];
      conditionsProviders[j] = logger->conditions_provider[j];
    }
    pvaConditions->pvaChannelNames = freeze(conditionsNames);
    pvaConditions->pvaProvider = freeze(conditionsProviders);
    ConnectPVA(pvaConditions, 2.0);
    pvaConditions->limitGetReadings = true;
  }
  if (pvaStrobe != NULL) {
    allocPVA(pvaStrobe, 1);
    epics::pvData::shared_vector<std::string> triggerName(1);
    epics::pvData::shared_vector<std::string> triggerProvider(1);
    triggerName[0] = logger->datastrobe_name;
    triggerProvider[0] = logger->datastrobe_provider;
    pvaStrobe->pvaChannelNames = freeze(triggerName);
    pvaStrobe->pvaProvider = freeze(triggerProvider);
    ConnectPVA(pvaStrobe, 2.0);
  }
  if (pvaGlitch != NULL) {
    allocPVA(pvaGlitch, logger->glitch_pvCount);
    epics::pvData::shared_vector<std::string> glitchNames(logger->glitch_pvCount);
    epics::pvData::shared_vector<std::string> glitchProviders(logger->glitch_pvCount);
    for (j = 0; j < logger->glitch_pvCount; j++) {
      glitchNames[j] = logger->glitch_controlName[j];
      glitchProviders[j] = logger->glitch_provider[j];
    }
    pvaGlitch->pvaChannelNames = freeze(glitchNames);
    pvaGlitch->pvaProvider = freeze(glitchProviders);
    ConnectPVA(pvaGlitch, 2.0);
    pvaGlitch->limitGetReadings = true;
  }

  //Check to see if we should exit due to connection error
  if (logger->onerrorindex == ONERROR_EXIT) {
    if ((pva->numNotConnected > 0) ||
        ((pvaConditions != NULL) && (pvaConditions->numNotConnected)) ||
        ((pvaStrobe != NULL) && (pvaStrobe->numNotConnected))) {
      fprintf(stdout, "Exiting due to PV connection error.\n");
      return (1);
    }
  }

  //Ping run control if in use
  if (PingRunControl() != 0) {
    return (1);
  }

  return (0);
}

long GenerationsCheck(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  logger->LastHour = logger->HourNow;
  logger->LastDay = logger->DayNow;
  makeTimeBreakdown(getTimeInSecs(), NULL, &(logger->DayNow), &(logger->HourNow), NULL, NULL, NULL, NULL);
  if (((logger->generationsRowLimit > 0) && (logger->step == logger->generationsRowLimit)) ||
      ((logger->generationsTimeLimit > 0) && ((logger->currentTime - logger->StartTime) > logger->generationsTimeLimit)) ||
      ((logger->dailyFiles) && (logger->HourNow < logger->LastHour)) ||
      ((logger->monthlyFiles) && (logger->DayNow < logger->LastDay))) {
    //Close SDDS files
    if ((logger->verbose) && (logger->onePv_OutputDirectory == NULL)) {
      fprintf(stdout, "Data written to %s\n", logger->outputfile);
    }
    if (CloseFiles(SDDS_table, pva, logger) == 1) {
      return (1);
    }
    logger->NstepsAdjusted -= logger->step;
    logger->step = 0;
    logger->generationsTimeLimit += logger->generationsTimeLimitOrig;
    //Start new files (run control pings if in one-pv-per-file mode)
    if (WriteHeaders(SDDS_table, pva, logger)) {
      return (1);
    }
  }
  return (0);
}

void freeLogger(LOGGER_DATA *logger) {
  long j;
  if (logger->onePv_outputfile) {
    for (j = 0; j < logger->pvCount; j++)
      free(logger->onePv_outputfile[j]);
    free(logger->onePv_outputfile);
  }
  if (logger->onePv_outputfileOrig) {
    for (j = 0; j < logger->pvCount; j++)
      free(logger->onePv_outputfileOrig[j]);
    free(logger->onePv_outputfileOrig);
  }
  if (logger->outputfile)
    free(logger->outputfile);
  if (logger->outputRow)
    free(logger->outputRow);
  if (logger->outputPage)
    free(logger->outputPage);
  if (logger->averagedValues) {
    free(logger->averagedValues);
  }
  if (logger->controlName != logger->readbackName) {
    if (logger->readbackName) {
      for (j = 0; j < logger->pvCount; j++)
        free(logger->readbackName[j]);
      free(logger->readbackName);
      logger->readbackName = NULL;
    }
  }
  if (logger->controlName) {
    for (j = 0; j < logger->pvCount; j++)
      free(logger->controlName[j]);
    free(logger->controlName);
    logger->controlName = NULL;
  }
  if (logger->provider) {
    for (j = 0; j < logger->pvCount; j++)
      free(logger->provider[j]);
    free(logger->provider);
  }
  if (logger->expectScalar) {
    free(logger->expectScalar);
  }
  if (logger->expectScalarArray) {
    free(logger->expectScalarArray);
  }
  if (logger->expectElements) {
    free(logger->expectElements);
  }
  if (logger->expectNumeric) {
    free(logger->expectNumeric);
  }
  if (logger->storageType) {
    free(logger->storageType);
  }
  if (logger->units) {
    for (j = 0; j < logger->pvCount; j++)
      free(logger->units[j]);
    free(logger->units);
  }
  if (logger->scaleFactor) {
    free(logger->scaleFactor);
  }
  if (logger->average) {
    free(logger->average);
  }
  if (logger->conditions_controlName) {
    for (j = 0; j < logger->conditions_pvCount; j++)
      free(logger->conditions_controlName[j]);
    free(logger->conditions_controlName);
  }
  if (logger->conditions_provider) {
    for (j = 0; j < logger->conditions_pvCount; j++)
      free(logger->conditions_provider[j]);
    free(logger->conditions_provider);
  }
  if (logger->conditions_holdoffTime) {
    free(logger->conditions_holdoffTime);
  }
  if (logger->conditions_scaleFactor) {
    free(logger->conditions_scaleFactor);
  }
  if (logger->conditions_lowerLimit) {
    free(logger->conditions_lowerLimit);
  }
  if (logger->conditions_upperLimit) {
    free(logger->conditions_upperLimit);
  }
  if (logger->glitch_controlName) {
    for (j = 0; j < logger->glitch_pvCount; j++)
      free(logger->glitch_controlName[j]);
    free(logger->glitch_controlName);
  }
  if (logger->glitch_provider) {
    for (j = 0; j < logger->glitch_pvCount; j++)
      free(logger->glitch_provider[j]);
    free(logger->glitch_provider);
  }
  if (logger->glitch_majorAlarm) {
    free(logger->glitch_majorAlarm);
  }
  if (logger->glitch_minorAlarm) {
    free(logger->glitch_minorAlarm);
  }
  if (logger->glitch_noAlarm) {
    free(logger->glitch_noAlarm);
  }
  if (logger->glitch_glitchThreshold) {
    free(logger->glitch_glitchThreshold);
  }
  if (logger->glitch_glitchBaselineSamples) {
    free(logger->glitch_glitchBaselineSamples);
  }
  if (logger->glitch_glitchBaselineAutoReset) {
    free(logger->glitch_glitchBaselineAutoReset);
  }
  if (logger->glitch_glitchScript) {
    for (j = 0; j < logger->glitch_pvCount; j++)
      free(logger->glitch_glitchScript[j]);
    free(logger->glitch_glitchScript);
  }
  if (logger->glitch_transitionThreshold) {
    free(logger->glitch_transitionThreshold);
  }
  if (logger->glitch_transitionDirection) {
    free(logger->glitch_transitionDirection);
  }
  if (logger->inhibit_name) {
    free(logger->inhibit_name);
  }
  if (logger->inhibit_provider) {
    free(logger->inhibit_provider);
  }

  if (logger->emptyStringColumn) {
    for (j = 0; j < logger->n_rows; j++) {
      free(logger->emptyStringColumn[j]);
    }
    free(logger->emptyStringColumn);
  }
  if (logger->emptyColumn)
    free(logger->emptyColumn);
  if (logger->elementIndex)
    free(logger->elementIndex);
  if (logger->verifiedType)
    free(logger->verifiedType);
  if (logger->glitch_alarmLastValue)
    free(logger->glitch_alarmLastValue);
  if (logger->glitch_transitionLastValue)
    free(logger->glitch_transitionLastValue);
  if (logger->glitch_glitchBaselineSamplesRead)
    free(logger->glitch_glitchBaselineSamplesRead);
  if (logger->glitch_glitchBaselineSampleValues) {
    for (j = 0; j < logger->glitch_pvCount; j++) {
      free(logger->glitch_glitchBaselineSampleValues[j]);
    }
    free(logger->glitch_glitchBaselineSampleValues);
  }
  if (logger->glitch_triggerIndex)
    free(logger->glitch_triggerIndex);
  if (logger->glitch_triggered)
    free(logger->glitch_triggered);
  if (logger->glitch_glitchIndex)
    free(logger->glitch_glitchIndex);
  if (logger->glitch_glitched)
    free(logger->glitch_glitched);
  if (logger->glitch_alarmIndex)
    free(logger->glitch_alarmIndex);
  if (logger->glitch_alarmSeverityIndex)
    free(logger->glitch_alarmSeverityIndex);
  if (logger->glitch_alarmed)
    free(logger->glitch_alarmed);
  if (logger->glitch_alarmSeverity)
    free(logger->glitch_alarmSeverity);

  if (logger->glitch_triggeredTemp)
    free(logger->glitch_triggeredTemp);
  if (logger->glitch_glitchedTemp)
    free(logger->glitch_glitchedTemp);
  if (logger->glitch_alarmedTemp)
    free(logger->glitch_alarmedTemp);
  if (logger->glitch_alarmSeverityTemp)
    free(logger->glitch_alarmSeverityTemp);
}

long WatchInput(LOGGER_DATA *logger) {
  if (logger->watchInput) {
    if (file_is_modified(logger->inputfile, &(logger->inputfileLastlink), &(logger->filestat))) {
      if (logger->verbose) {
        fprintf(stdout, "The input file has been modified, exiting\n");
      }
      return (1);
    }
    if (logger->trigFileSet) {
      if (file_is_modified(logger->triggerFile, &(logger->triggerFileLastlink), &(logger->triggerfilestat))) {
        if (logger->verbose) {
          fprintf(stdout, "The input file has been modified, exiting\n");
        }
        return (1);
      }
    }
  }

  return (0);
}

long WriteGlitchPage(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, LOGGER_DATA *logger) {
  int64_t n, count, i, j, length, n_rows, stepOffset;
  int32_t result;
  SDDS_TABLE *sdds;
  sdds = &(SDDS_table[0]);

  if (logger->glitch_step > logger->circularbuffer_before) {
    length = logger->circularbuffer_before + 1 + logger->step - logger->glitch_step;
    n = logger->circularbuffer_index;
    for (i = 0; i < logger->circularbuffer_length - length; i++) {
      n++;
      if (n == logger->circularbuffer_length) {
        n = 0;
      }
    }
    stepOffset = logger->circularbuffer_before;
  } else {
    length = 1 + logger->step;
    n = 0;
    stepOffset = logger->glitch_step;
  }

  if (logger->scalarsAsColumns) {
    n_rows = length;
  } else {
    n_rows = logger->n_rows;
  }

  for (count = 0; count < length; count++) {
    if ((logger->scalarsAsColumns == false) || (count == 0)) {
      if (!SDDS_StartPage(sdds, n_rows)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }

      result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                 logger->triggerTimeIndex, (double)(logger->glitch_time));
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }

      for (j = 0; j < logger->glitch_pvCount; j++) {
        if (logger->glitch_TransitionThresholdDefined && logger->glitch_TransitionDirectionDefined && (logger->glitch_transitionDirection[j] != 0)) {
          result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     logger->glitch_triggerIndex[j], logger->glitch_triggered[j]);
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
        if (((logger->glitch_NoAlarmDefined) && (logger->glitch_noAlarm[j] != 0)) ||
            ((logger->glitch_MinorAlarmDefined) && (logger->glitch_minorAlarm[j] != 0)) ||
            ((logger->glitch_MajorAlarmDefined) && (logger->glitch_majorAlarm[j] != 0))) {
          result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     logger->glitch_alarmIndex[j], logger->glitch_alarmed[j]);
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
          result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     logger->glitch_alarmSeverityIndex[j], logger->glitch_alarmSeverity[j]);
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
        if (logger->glitch_GlitchThresholdDefined && (logger->glitch_glitchThreshold[j] != 0) &&
            logger->glitch_GlitchBaselineSamplesDefined && (logger->glitch_glitchBaselineSamples[j] > 0)) {
          result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                     logger->glitch_glitchIndex[j], logger->glitch_glitched[j]);
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        }
      }
    }

    if (logger->scalarsAsColumns) {
      result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, count,
                                 logger->caErrorsIndex, (int32_t)(logger->circularbufferDouble[pva->numPVs][n][0]),
                                 logger->timeIndex, (double)(logger->circularbufferLongDouble[n]),
                                 logger->stepIndex, (int32_t)(count - stepOffset),
                                 logger->timeofdayIndex, (float)(logger->circularbufferDouble[pva->numPVs + 1][n][0]),
                                 logger->dayofmonthIndex, (float)(logger->circularbufferDouble[pva->numPVs + 2][n][0]),
                                 -1);
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      for (i = 0; i < pva->numPVs; i++) {
        if (logger->expectNumeric[i]) {
          result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, count,
                                     logger->elementIndex[i], logger->circularbufferDouble[i][n][0], -1);
        } else {
          result = SDDS_SetRowValues(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, count,
                                     logger->elementIndex[i], logger->circularbufferString[i][n][0], -1);
        }
        if (result == 0) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
      }
    } else {
      result = SDDS_SetParameters(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                  logger->caErrorsIndex, (int32_t)(logger->circularbufferDouble[pva->numPVs][n][0]),
                                  logger->timeIndex, (double)(logger->circularbufferLongDouble[n]),
                                  logger->stepIndex, (int32_t)count,
                                  logger->timeofdayIndex, (float)(logger->circularbufferDouble[pva->numPVs + 1][n][0]),
                                  logger->dayofmonthIndex, (float)(logger->circularbufferDouble[pva->numPVs + 2][n][0]),
                                  -1);
      if (result == 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
      for (i = 0; i < pva->numPVs; i++) {
        if (logger->expectScalar[i] || logger->treatScalarArrayAsScalar[i]) {
          if (logger->expectNumeric[i]) {
            result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                       logger->elementIndex[i], logger->circularbufferDouble[i][n][0]);
          } else {
            result = SDDS_SetParameter(sdds, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                       logger->elementIndex[i], logger->circularbufferString[i][n][0]);
          }
          if (result == 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            return (1);
          }
        } else {
          if (logger->scalarArraysAsColumns) {
            if (logger->expectNumeric[i]) {
              result = SDDS_SetColumn(sdds, SDDS_SET_BY_INDEX,
                                      logger->circularbufferDouble[i][n], logger->expectElements[i], logger->elementIndex[i]);
            } else {
              result = SDDS_SetColumn(sdds, SDDS_SET_BY_INDEX,
                                      logger->circularbufferString[i][n], logger->expectElements[i], logger->elementIndex[i]);
            }
            if (result == 0) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (1);
            }
          } else {
            if (logger->expectNumeric[i]) {
              result = SDDS_SetArrayVararg(sdds, (char *)logger->readbackName[i], SDDS_CONTIGUOUS_DATA,
                                           logger->circularbufferDouble[i][n], logger->expectElements[i]);
            } else {
              result = SDDS_SetArrayVararg(sdds, (char *)logger->readbackName[i], SDDS_CONTIGUOUS_DATA,
                                           logger->circularbufferString[i][n], logger->expectElements[i]);
            }
            if (result == 0) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              return (1);
            }
          }
        }
      }
    }
    if (logger->scalarsAsColumns == false) {
      if (!SDDS_WriteTable(sdds)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return (1);
      }
    }

    n++;
    if (n == logger->circularbuffer_length) {
      n = 0;
    }
  }

  if (logger->scalarsAsColumns) {
    if (!SDDS_WriteTable(sdds)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return (1);
    }
  }

  return (0);
}

long GlitchLogicRoutines(SDDS_TABLE *SDDS_table, PVA_OVERALL *pva, PVA_OVERALL *pvaGlitch, LOGGER_DATA *logger) {
  long i, numScripts = 0;
  char **executeScript = NULL;
  if (pvaGlitch != NULL) {
    //Add the current data to the circular buffer;
    StoreDataIntoCircularBuffers(pva, logger);

    //Write out glitch data
    if ((logger->glitch_step > 0) && (logger->step - logger->glitch_step == logger->circularbuffer_after)) {
      //Write pages here
      if (WriteGlitchPage(SDDS_table, pva, logger) == 1) {
        return (1);
      }
    }
    //Reset for next glitch
    if ((logger->glitch_step > 0) && (logger->step - logger->glitch_step >= logger->circularbuffer_after)) {
      if (logger->currentTime - logger->glitch_time >= logger->defaultHoldoffTime) {
        logger->glitch_step = -1;
      }
    }

    if (((logger->glitch_step < 0) || ((logger->autoHoldoff == false) && (logger->currentTime - logger->glitch_time >= logger->defaultHoldoffTime))) && (CheckGlitchPVA(pvaGlitch, logger) == true)) {
      if (logger->glitch_step > 0) {
        //Write pages here. Only partial data was captured prior to a new glitch event
        if (WriteGlitchPage(SDDS_table, pva, logger) == 1) {
          return (1);
        }
      }
      logger->glitch_step = logger->step;
      logger->glitch_time = logger->currentTime;
      for (i = 0; i < pvaGlitch->numPVs; i++) {
        logger->glitch_triggered[i] = logger->glitch_triggeredTemp[i];
        logger->glitch_glitched[i] = logger->glitch_glitchedTemp[i];
        logger->glitch_alarmed[i] = logger->glitch_alarmedTemp[i];
        logger->glitch_alarmSeverity[i] = logger->glitch_alarmSeverityTemp[i];
        if (logger->glitch_triggered[i] || logger->glitch_glitched[i] || logger->glitch_alarmed[i]) {
          if (logger->glitch_GlitchScriptDefined && (strlen(logger->glitch_glitchScript[i]) > 0)) {
            if (numScripts == 0) {
              executeScript = (char **)SDDS_Realloc(executeScript, sizeof(char *) * (numScripts + 1));
              executeScript[numScripts] = logger->glitch_glitchScript[i];
              numScripts++;
            } else if (match_string(logger->glitch_glitchScript[i], executeScript, numScripts, EXACT_MATCH) < 0) {
              executeScript = (char **)SDDS_Realloc(executeScript, sizeof(char *) * (numScripts + 1));
              executeScript[numScripts] = logger->glitch_glitchScript[i];
              numScripts++;
            }
          }
        }
      }
      if (numScripts > 0) {
        for (i = 0; i < numScripts; i++) {
          if (logger->verbose) {
            fprintf(stdout, "Executing %s\n", executeScript[i]);
          }
          runCommandNoShell(executeScript[i]);
        }
        free(executeScript);
      }
      return (2);
    } else {
      return (2);
    }
  }
  return (0);
}
