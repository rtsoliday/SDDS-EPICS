/**
 * @file sddswmonitor.cc
 * @brief Capture waveforms and scalars from EPICS PVs into an SDDS file.
 *
 * sddswmonitor reads one or more waveform PVs along with optional scalar
 * PVs and writes the values to a binary SDDS file. Numerous options
 * control timing, file handling, accumulation, and data selection.
 *
 * @section Usage
 * ```
 * sddswmonitor [<inputfile> | -PVnames=<name>[,<name>]] <outputfile>
 *               {-erase | -generations[=digits=<integer>][,delimiter=<string>]}
 *               [-append[=recover]] [-pendIOtime=<seconds>]
 *               [-steps=<integer> | -time=<value>[,<units>]]
 *               [-interval=<value>[,<units>]] [-enforceTimeLimit]
 *               [-offsetTimeOfDay] [-verbose]
 *               [-singleShot[=noprompt|stdout]]
 *               [-precision={single|double}]
 *               [-dataType={short|long|float|double|character|string}]
 *               [-onCAerror={useZero|skipPage|exit}]
 *               [-scalars=<filename>]
 *               [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]
 *               [-accumulate={average|sum},number=<number>[,noquery]]
 *               [-logOnChange[=waveformOnly][,scalarsOnly][,anyChange]]
 *               [-comment=<parameter>,<text>] [-nowarnings] [-nounits]
 *               [-xParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>]]
 *               [-yParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>]]
 *               [-versus=name=<columnName>[,unitsValue=<string>|unitsPV=<string>][,deltaValue=<number>|deltaPV=<string>][,offsetValue=<integer>|offsetPV=<string>]]
 *               [-rms] [-bandwidthLimitedRMS=<minHz>,<maxHz>[,scaleByOneOverOmegaSquared][,noPV]]
 * ```
 *
 * @section Options
 * | Option                | Description |
 * |-----------------------|-------------|
 * | `-PVnames`            | Comma-separated list of PV names to monitor. |
 * | `-erase`              | Delete output file before writing. |
 * | `-generations`        | Create numbered output files. |
 * | `-append`             | Append data to existing file. |
 * | `-pendIOtime`         | Maximum time to wait for Channel Access I/O. |
 * | `-steps`              | Number of acquisition cycles. |
 * | `-time`               | Total run time. |
 * | `-interval`           | Time between readings. |
 * | `-enforceTimeLimit`   | Honor the time limit even if samples are missing. |
 * | `-offsetTimeOfDay`    | Adjust starting time of day. |
 * | `-verbose`            | Print messages during acquisition. |
 * | `-singleShot`         | Wait for user input before each sample. |
 * | `-precision`          | Choose single or double precision. |
 * | `-dataType`           | Override PV data type. |
 * | `-onCAerror`          | Action when Channel Access fails. |
 * | `-scalars`            | File listing scalar PVs to read. |
 * | `-conditions`         | File specifying PV limits that must be satisfied. |
 * | `-accumulate`         | Average or sum repeated samples. |
 * | `-logOnChange`        | Log data only when PV values change. |
 * | `-comment`            | Parameter and text to store in output file. |
 * | `-nowarnings`         | Suppress connection warnings. |
 * | `-nounits`            | Do not read PV units from database. |
 * | `-versus`             | Define physical units for waveform index. |
 * | `-xParameter`         | Define x-axis parameter information. |
 * | `-yParameter`         | Define y-axis parameter information. |
 * | `-rms`                | Create RMS parameters for each waveform. |
 * | `-bandwidthLimitedRMS`| RMS filtered by a bandwidth. |
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * M. Borland, L. Emery, H. Shang, R. Soliday
 */

#include <complex>
#include <iostream>
#include <ctime>

#if defined(_WIN32)
#  include <winsock.h>
#else
#  include <unistd.h>
#endif

#include <cadef.h>
#include <epicsVersion.h>
#if (EPICS_VERSION > 3)
#  include "pvaSDDS.h"
#  include "pv/thread.h"
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

#include "fftpackC.h"
#include "SDDSepics.h"

#ifdef _WIN32
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#endif

#define CLO_INTERVAL 0
#define CLO_STEPS 1
#define CLO_VERBOSE 2
#define CLO_ERASE 3
#define CLO_SINGLE_SHOT 4
#define CLO_TIME 5
#define CLO_APPEND 6
#define CLO_PRECISION 7
#define CLO_PROMPT 8
#define CLO_ONCAERROR 9
#define CLO_EZCATIMING 10
#define CLO_SCALARS 11
#define CLO_CONDITIONS 12
#define CLO_PVNAMES 13
#define CLO_GENERATIONS 14
#define CLO_COMMENT 15
#define CLO_ENFORCETIMELIMIT 16
#define CLO_OFFSETTIMEOFDAY 17
#define CLO_DATATYPE 18
#define CLO_ACCUMULATE 19
#define CLO_LOGONCHANGE 20
#define CLO_NOWARNINGS 21
#define CLO_NOUNITS 22
#define CLO_PENDIOTIME 23
#define CLO_VERSUS 24
#define CLO_XPARAMETER 25
#define CLO_YPARAMETER 26
#define CLO_RMS 27
#define CLO_BANDWIDTHLIMITEDRMS 28
#define COMMANDLINE_OPTIONS 29
char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"interval", (char *)"steps", (char *)"verbose", (char *)"erase", (char *)"singleshot", (char *)"time",
  (char *)"append", (char *)"precision", (char *)"prompt", (char *)"oncaerror",
  (char *)"ezcatiming", (char *)"scalars", (char *)"conditions", (char *)"pvnames", (char *)"generations", (char *)"comment",
  (char *)"enforcetimelimit", (char *)"offsettimeofday", (char *)"datatype", (char *)"accumulate",
  (char *)"logOnChange", (char *)"nowarnings", (char *)"nounits", (char *)"pendiotime", (char *)"versus", (char *)"xparameter",
  (char *)"yparameter", (char *)"rms", (char *)"bandwidthLimitedRMS"};

#define ONCAERROR_USEZERO 0
#define ONCAERROR_SKIPPAGE 1
#define ONCAERROR_EXIT 2
#define ONCAERROR_OPTIONS 3
char *oncaerror_options[ONCAERROR_OPTIONS] = {
  (char *)"usezero",
  (char *)"skippage",
  (char *)"exit",
};

#define NAuxiliaryParameterNames 4
char *AuxiliaryParameterNames[NAuxiliaryParameterNames] = {
  (char *)"Step",
  (char *)"Time",
  (char *)"TimeOfDay",
  (char *)"DayOfMonth",
};

#define NTimeUnitNames 4
char *TimeUnitNames[NTimeUnitNames] = {
  (char *)"seconds",
  (char *)"minutes",
  (char *)"hours",
  (char *)"days",
};
double TimeUnitFactor[NTimeUnitNames] = {
  1,
  60,
  3600,
  86400,
};

static char *USAGE1 = (char *)"sddswmonitor [<inputfile> | -PVnames=<name>[,<name>]]\n\
    <outputfile> [{-erase | -generations[=digits=<integer>][,delimiter=<string>}]\n\
    [-append[=reover]] [-pendIOtime=<seconds>]\n\
    [-steps=<integer> | -time=<value>[,<units>]] [-interval=<value>[,<units>]]\n\
    [-enforceTimeLimit] [-offsetTimeOfDay]\n\
    [-verbose] [-singleShot{=noprompt|stdout}] [-precision={single|double}]\n\
    [-dataType={short|long|float|double|character|string}]\n\
    [-onCAerror={useZero|skipPage|exit}] \n\
    [-scalars=<filename>]\n\
    [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
    [-accumulate={average|sum},number=<number>[,noquery]\n\
    [-logOnChange[=waveformOnly][,scalarsOnly][,anyChange]]\n\
    [-comment=<parameterName>,<text>] [-nowarnings] [-nounits]\n\
    [-xParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>] \n\
    [-yParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>] \n\
    [-versus=name=<columnName>[,unitsValue=<string>|unitsPV=<string>][,deltaValue=<number>|deltaPV=<string>][,offsetValue=<integer>|offsetPV=<string>]] \n\
    [-rms] [-bandwidthLimitedRMS=<MinHZ>,<MaxHZ>[,scaleByOneOverOmegaSquared][,noPV]]\n\n";
static char *USAGE2 = (char *)"Writes values of process variables to a binary SDDS file.\n\
<inputfile>        SDDS input file containing the columns \"WaveformPV\" and\n\
                   \"WaveformName\", plus a parameter \"WaveformLength\".  A \"DataType\"\n\
                   column is optional to specify the data type (short, long, float, double,\n\
                   character, or string) for each pv, but this will be overridden if the\n\
                   -dataType option is specified on the command line. Optionally, a column\n\
                   \"WaveformOffset\" may be supplied; if present, the waveform is expected to\n\
                   be longer than \"WaveformLength\" and the data written starts at index\n\
                   \"WaveformOffset\".\n\
PVnames            specifies a list of PV names to read.  If the waveforms are\n\
                   of different lengths, the short ones are padded with zeros.\n\
<outputfile>       SDDS output file, each page of which one instance of each waveform.\n\
generations        The output is sent to the file <outputfile>-<N>, where <N> is\n\
                   the smallest positive integer such that the file does not already \n\
                   exist.   By default, four digits are used for formatting <N>, so that\n\
                   the first generation number is 0001.\n\
erase              outputfile is erased before execution.\n\
append             Appends new values in a new SDDS page in the output file.\n\
                   Optionally recovers garbled SDDS data. \n\
steps              number of reads for each process variable.\n\
time               total time (in seconds) for monitoring;\n\
                   valid time units are seconds, minutes, hours, or days.\n";
static char *USAGE3 = (char *)"enforceTimeLimit   Enforces the time limit given with the -time option, even if \n\
                   the expected number of samples has not been taken.\n\
offsetTimeOfDay    Adjusts the starting TimeOfDay value so that it corresponds\n\
                   to the day for which the bulk of the data is taken.  Hence, a\n\
                   26 hour job started at 11pm would have initial time of day of\n\
                   -1 hour and final time of day of 25 hours.\n\
interval           desired time interval between reading PVs.\n\
                   Valid time units are seconds, minutes, hours, or days.\n\
verbose            prints out a message when data is taken.\n\
singleShot         single shot read initiated by a <cr> key press; time_interval is disabled.\n\
                   By default, a prompt is issued to stderr.  The qualifiers may be used to\n\
                   remove the prompt or have it go to stdout.\n\
precision          specify single (default) or double precision for PV data.\n\
dataType           Optionally specifies the data type for the output file.  Overrides\n\
                   the precision option.\n\
x|yParameter       provide the required information for the parameters that are needed by sddscontour quantity plot\n";
static char *USAGE4 = (char *)"onCAerror          action taken when a channel access error occurs. Default is\n\
                   to use zeroes for values.\n\
scalars            specifies sddsmonitor input file to get names of scalar PVs\n\
                   from.  These will be logged in the output file as parameters.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
accumulate         Directs accumulation or averaging of several readings of each waveform.\n\
                   If noquery is given, then in single-shot mode there is only one query\n\
                   for each set of averaged readings. \n\
versus             for user to specify the physical quantity of the waveform corresponding to \n\
                   the Index. The name will be the column name in the output, units, delta and offset \n\
                   can be specified by value or PV. If none of units and units pv are provided,\n\
                   the units will be obtained from the units of deltaPV if provided.\n";
static char *USAGE5 = (char *)"logOnChange        If given, the data is written only if one or more elements of one or more\n\
                   waveforms changes.\n\
nounits            do not get units for the readback PVs if they are not provided. \n\
comment            Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                   along with the text to be placed in the file.  \n\
rms                Creates parameters with the RMS values of each waveform.\n\
Program by M. Borland, L. Emery, H. Shang, R. Soliday ANL\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 1

#define AA_AVERAGE 0x001
#define AA_SUM 0x002
#define AA_NOQUERY 0x004

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

#define LOGONCHANGE_WAVEFORMS 0x0001U
#define LOGONCHANGE_SCALARS 0x0002U
#define LOGONCHANGE_ANYCHANGE (LOGONCHANGE_WAVEFORMS | LOGONCHANGE_SCALARS)

void ScaleWaveformData(void *data, long dataType, long length, double offset, double scale);
void AccumulateWaveformData(void *data, void *accData, long dataType, long length, long accumulate, long accumulateClear);
void AveAccumulate(void *accData, long dataType, long length, long number);
void CopyWaveformData(void *data, void *previousData, long dataType, long length);
long CheckIfWaveformChanged(void *data, void *previousData, long dataType, long length);

void AllocateWaveformMemory(void **waveformData, int32_t *readbackDataType,
                            long readbacks, long waveformLength, int32_t *waveformOffset);
void FreeWaveformMemory(void **waveformData, int32_t *readbackDataType,
                        long readbacks, long waveformLength, int32_t *waveformOffset);
void FreeReadMemory(long variables, char **pvs, char **names, char **units, char **typeStrings,
                    double *data1, double *data2, double *data3, double *data4, int32_t *dataType);

double rmsValueFloat(float *y, long n);
double rmsValueLong(long *y, long n);
double rmsValueShort(short *y, long n);
double bandwidthLimitedRMS(double *fftIndepVariable, double *fftDepenQuantity, long rows, double minFreq, double maxFreq, short scaleByOneOverOmegaSquared);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE outTable, originalfile_page;
  char *inputfile, *outputfile, answer[ANSWER_LENGTH];
  long CAerrors, waveformLength, optPVnames, unitsFromFile, rmsPVPrefixFromFile;
  int32_t *waveformOffset = NULL;
  long enforceTimeLimit, noWarnings = 0, noUnits = 0;
  char **optPVname;
  char **readbackPV, **readbackName, **readbackUnits, **readbackDataTypeStrings, **rmsPVPrefix;
  chid *readbackCHID = NULL, *scalarCHID = NULL;

  void **outputColumn, **waveformData, **accWaveformData, **previousWaveformData;
  double *readbackOffset, *readbackScale;
  char **scalarPV, **scalarName, **scalarUnits, **readMessage, **scalarSymbol = NULL;
  double *scalarData, *scalarFactor, *scalarAccumData, *previousScalarData;
  char **strScalarData = NULL;
  long i, j, n, pvIndex, i_arg, outputColumns, readbacks = 0, scalars = 0, *scalarDataType = NULL, strScalarExist = 0;
  char *TimeStamp, *PageTimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartDay, StartHour, TimeOfDay, DayOfMonth;
  double TotalTime, StartTime, StartJulianDay, StartYear, StartMonth, RunStartTime, RunTime;
  long Step, NStep, verbose, singleShot, firstPage;
  long accumulateAction, accumulate, accumulateClear, accumulateNoQuery;
  int32_t accumulateNumber;
  long stepsSet, totalTimeSet;
  long outputPreviouslyExists, append, erase, recover;
  char *precision_string, *datatype_string, *scalarFile;
  long Precision, DataType, TimeUnits;
  int32_t *index;
  long onCAerror;
  double timeToWait, expectedTime;
  unsigned long dummyFlags;
  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  chid *CondCHID = NULL;
  long conditions = 0;
  unsigned long CondMode = 0;
  double *CondDataBuffer = NULL;
  char **ColumnNames, **ParameterNames, **temp, *xParName = NULL, *yParName = NULL;
  int32_t NColumnNames = 0, NParameters = 0, xParDim = 0, yParDim = 0, xParGiven = 0, yParGiven = 0;
  double xParMin = 0, xParInterval = 1.0, xParMax = 100, yParMin = 0, yParInterval = 1.0, yParMax = 100.0;

  char **commentParameter, **commentText, *generationsDelimiter;
  long comments, generations;
  int32_t generationsDigits;
  long offsetTimeOfDay, TimeOfDayOffset;
  int32_t *readbackDataType;
  long dataTypeCLOGiven;
  /*long dataTypeColumnGiven; */
  unsigned long logOnChange;
  double pendIOtime = 10.0, timeout;
  long retries;

  char *indeptName, *indeptUnits, *indeptUnitPV, *indeptDeltaPV, *indeptOffsetPV = NULL;
  double indeptDelta = 0, *indeptValue = NULL;
  chid indeptUnitChid, indeptDeltaChid, indeptOffsetChid;
  long indeptOffset = 0;

  short rms = 0;
  char buffer[50];
  double *fftIndepVariable = NULL, *fftDepenQuantity = NULL;

  short bandwidthLimitedRMSCount = 0;
  double bandwidthLimitedRMS_MIN[10], bandwidthLimitedRMS_MAX[10], bandwidthLimitedRMSResult;
  short bandwidthLimitedRMS_Omega[10];
  short bandwidthLimitedRMS_noPV[10];
  chid *bandwidthLimitedRMS_CHID = NULL;
  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5);
    exit(1);
  }
  conditions = 0;
  CondDeviceName = CondReadMessage = NULL;
  CondScaleFactor = CondLowerLimit = CondUpperLimit = CondHoldoff = NULL;

  offsetTimeOfDay = TimeOfDayOffset = 0;
  inputfile = outputfile = scalarFile = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = verbose = append = erase = singleShot = stepsSet = generations = recover = 0;
  onCAerror = ONCAERROR_USEZERO;
  Precision = PRECISION_SINGLE;
  DataType = -1;
  CondFile = NULL;
  optPVname = NULL;
  optPVnames = 0;
  commentParameter = commentText = NULL;
  comments = 0;
  enforceTimeLimit = 0;
  accumulateAction = accumulateNumber = accumulateNoQuery = 0;
  accWaveformData = previousWaveformData = NULL;
  CondDataBuffer = scalarData = NULL;
  precision_string = datatype_string = NULL;

  dataTypeCLOGiven = 0;
  /*dataTypeColumnGiven = 0; */
  logOnChange = 0;
  indeptName = indeptUnitPV = indeptUnits = indeptDeltaPV = NULL;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb((char *)"invalid -pendIOtime syntax", NULL);
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items < 2 || !(get_double(&TimeInterval, s_arg[i_arg].list[1])) ||
            TimeInterval <= 0)
          bomb((char *)"no value or invalid value given for option -interval", NULL);
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            TimeInterval *= TimeUnitFactor[TimeUnits];
          else
            bomb((char *)"unknown/ambiguous time units given for -interval", NULL);
        }
        break;
      case CLO_NOWARNINGS:
        noWarnings = 1;
        break;
      case CLO_NOUNITS:
        noUnits = 1;
        break;
      case CLO_STEPS:
        if (s_arg[i_arg].n_items != 2)
          bomb((char *)"no value or invalid value given for option -steps", NULL);
        if (!(get_long(&NStep, s_arg[i_arg].list[1])) || NStep <= 0)
          bomb((char *)"no value or invalid value given for option -steps", NULL);
        stepsSet = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_PROMPT:
        singleShot = SS_PROMPT;
      case CLO_SINGLE_SHOT:
        singleShot = SS_PROMPT;
        if (s_arg[i_arg].n_items != 1) {
          if (strncmp(s_arg[i_arg].list[1], "noprompt", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_NOPROMPT;
          else if (strncmp(s_arg[i_arg].list[1], "stdout", strlen(s_arg[i_arg].list[1])) == 0)
            singleShot = SS_STDOUTPROMPT;
          else
            SDDS_Bomb((char *)"invalid -singleShot qualifier");
        }
        break;
      case CLO_APPEND:
        if (s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -append syntax/value");
        append = 1;
        recover = 0;
        if (s_arg[i_arg].n_items == 2) {
          if (strncmp(s_arg[i_arg].list[1], "recover", strlen(s_arg[i_arg].list[1])) == 0)
            recover = 1;
          else
            SDDS_Bomb((char *)"invalid -append syntax/value");
        }
        break;
      case CLO_ERASE:
        erase = 1;
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items < 2 ||
            !(precision_string = s_arg[i_arg].list[1]))
          bomb((char *)"no value given for option -precision", NULL);
        if ((Precision = identifyPrecision(precision_string)) < 0)
          bomb((char *)"invalid -precision value", NULL);
        break;
      case CLO_DATATYPE:
        if (s_arg[i_arg].n_items < 2 ||
            !(datatype_string = s_arg[i_arg].list[1]))
          bomb((char *)"no value given for option -dataType", NULL);
        if ((DataType = SDDS_IdentifyType(datatype_string)) < 0)
          bomb((char *)"invalid -dataType value", NULL);
        dataTypeCLOGiven = 1;
        break;
      case CLO_TIME:
        if (s_arg[i_arg].n_items < 2 ||
            !(get_double(&TotalTime, s_arg[i_arg].list[1])) || TotalTime <= 0)
          bomb((char *)"no value or invalid value given for option -time", NULL);
        totalTimeSet = 1;
        if (s_arg[i_arg].n_items == 3) {
          if ((TimeUnits = match_string(s_arg[i_arg].list[2], TimeUnitNames, NTimeUnitNames, 0)) >= 0)
            TotalTime *= TimeUnitFactor[TimeUnits];
          else
            bomb((char *)"invalid units for -time option", NULL);
        }
        break;
      case CLO_ONCAERROR:
        if (s_arg[i_arg].n_items != 2)
          bomb((char *)"invalid -onCAerror syntax", NULL);
        if ((onCAerror = match_string(s_arg[i_arg].list[1], oncaerror_options, ONCAERROR_OPTIONS, 0)) < 0) {
          fprintf(stderr, "error: unrecognized oncaerror option given: %s\n", s_arg[i_arg].list[1]);
          exit(1);
        }
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3)
          SDDS_Bomb((char *)"wrong number of items for -ezcaTiming");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || retries < 0)
          bomb((char *)"invalid -ezca values", NULL);
        pendIOtime = timeout * (retries + 1);
        break;
      case CLO_SCALARS:
        if (s_arg[i_arg].n_items != 2)
          bomb((char *)"wrong number of keywords for -scalars", NULL);
        scalarFile = s_arg[i_arg].list[1];
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items < 3 ||
            SDDS_StringIsBlank(CondFile = s_arg[i_arg].list[1]) ||
            !(CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2)))
          SDDS_Bomb((char *)"invalid -conditions syntax/values");
        break;
      case CLO_PVNAMES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -PVnames syntax");
        optPVnames = s_arg[i_arg].n_items - 1;
        optPVname = s_arg[i_arg].list + 1;
        break;
      case CLO_GENERATIONS:
        generationsDigits = DEFAULT_GENERATIONS_DIGITS;
        generations = 1;
        generationsDelimiter = (char *)"-";
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &generationsDigits, 1, 0,
                          "delimiter", SDDS_STRING, &generationsDelimiter, 1, 0,
                          NULL) ||
            generationsDigits < 1)
          SDDS_Bomb((char *)"invalid -generations syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_VERSUS:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "name", SDDS_STRING, &indeptName, 1, 0,
                          "unitsValue", SDDS_STRING, &indeptUnits, 1, 0,
                          "unitsPV", SDDS_STRING, &indeptUnitPV, 1, 0,
                          "deltaValue", SDDS_DOUBLE, &indeptDelta, 1, 0,
                          "deltaPV", SDDS_STRING, &indeptDeltaPV, 1, 0,
                          "offsetValue", SDDS_LONG, &indeptOffset, 1, 0,
                          "offsetPV", SDDS_STRING, &indeptOffsetPV, 1, 0,
                          NULL))
          SDDS_Bomb((char *)"invalid -versus syntax/values");
        s_arg[i_arg].n_items += 1;
        if (!indeptName) {
          fprintf(stderr, "The independent column name is not provided!\n");
          exit(1);
        }
        if (!indeptDeltaPV && !indeptDelta) {
          fprintf(stderr, "Either the delta or detla pv for independent column name has to be provided!\n");
          exit(1);
        }
        break;
      case CLO_COMMENT:
        ProcessCommentOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1,
                             &commentParameter, &commentText, &comments);
        break;
      case CLO_ENFORCETIMELIMIT:
        enforceTimeLimit = 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_ACCUMULATE:
        if (s_arg[i_arg].n_items < 3)
          bomb((char *)"wrong number of keywords for -accumulate", NULL);
        s_arg[i_arg].n_items -= 1;
        accumulateNumber = 0;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "average", -1, NULL, 0, AA_AVERAGE,
                          "sum", -1, NULL, 0, AA_SUM,
                          "noquery", -1, NULL, 0, AA_NOQUERY,
                          "number", SDDS_LONG, &accumulateNumber, 1, 0,
                          NULL) ||
            accumulateNumber <= 0)
          bomb((char *)"invalid -accumulation syntax/values", NULL);
        if ((!(dummyFlags & AA_AVERAGE) && !(dummyFlags & AA_SUM)) ||
            (dummyFlags & AA_AVERAGE && dummyFlags & AA_SUM))
          bomb((char *)"give one and only one of average or sum qualifiers for -accumulate", NULL);
        accumulateAction = dummyFlags & (AA_AVERAGE + AA_SUM);
        accumulateNoQuery = dummyFlags & AA_NOQUERY;
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_LOGONCHANGE:
        if ((s_arg[i_arg].n_items -= 1) == 0)
          logOnChange = LOGONCHANGE_ANYCHANGE;
        else if (!scanItemList(&logOnChange, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                               "waveform", -1, NULL, 0, LOGONCHANGE_WAVEFORMS,
                               "scalars", -1, NULL, 0, LOGONCHANGE_SCALARS,
                               "anychange", -1, NULL, 0, LOGONCHANGE_ANYCHANGE,
                               NULL))
          bomb((char *)"invalid -logOnChange syntax/values", NULL);
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_XPARAMETER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "dimension", SDDS_LONG, &xParDim, 1, 0,
                          "name", SDDS_STRING, &xParName, 1, 0,
                          "interval", SDDS_DOUBLE, &xParInterval, 1, 0,
                          "minimum", SDDS_DOUBLE, &xParMin, 1, 0,
                          "maximum", SDDS_DOUBLE, &yParMax, 1, 0,
                          NULL) ||
            xParDim < 1)
          SDDS_Bomb((char *)"invalid -xParameter syntax/values (the dimension size should be greater than 1)");
        s_arg[i_arg].n_items += 1;
        if (!xParName)
          SDDS_CopyString(&xParName, "x");
        xParGiven = 1;
        break;
      case CLO_YPARAMETER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "dimension", SDDS_LONG, &yParDim, 1, 0,
                          "name", SDDS_STRING, &yParName, 1, 0,
                          "interval", SDDS_DOUBLE, &yParInterval, 1, 0,
                          "minimum", SDDS_DOUBLE, &yParMin, 1, 0,
                          "maximum", SDDS_DOUBLE, &yParMax, 1, 0,
                          NULL) ||
            yParDim < 1)
          SDDS_Bomb((char *)"invalid -yParameter syntax/values (the dimension size should be greater than 1)");
        s_arg[i_arg].n_items += 1;
        if (!yParName)
          SDDS_CopyString(&yParName, "y");
        yParGiven = 1;
        break;
      case CLO_RMS:
        rms = 1;
        break;
      case CLO_BANDWIDTHLIMITEDRMS:
        if (bandwidthLimitedRMSCount == 10) {
          bomb((char *)"Limited to 10 -bandwidthLimitedRMS options", NULL);
        }
        if ((s_arg[i_arg].n_items != 3) && (s_arg[i_arg].n_items != 4) && (s_arg[i_arg].n_items != 5))
          bomb((char *)"no value or invalid value given for option -bandwidthLimitedRMS", NULL);
        if (!(get_double(&(bandwidthLimitedRMS_MIN[bandwidthLimitedRMSCount]), s_arg[i_arg].list[1])))
          bomb((char *)"no value or invalid value given for option -bandwidthLimitedRMS", NULL);
        if (!(get_double(&(bandwidthLimitedRMS_MAX[bandwidthLimitedRMSCount]), s_arg[i_arg].list[2])))
          bomb((char *)"no value or invalid value given for option -bandwidthLimitedRMS", NULL);
        if (bandwidthLimitedRMS_MIN[bandwidthLimitedRMSCount] >= bandwidthLimitedRMS_MAX[bandwidthLimitedRMSCount])
          bomb((char *)"no value or invalid value given for option -bandwidthLimitedRMS", NULL);
        bandwidthLimitedRMS_Omega[bandwidthLimitedRMSCount] = 0;
        bandwidthLimitedRMS_noPV[bandwidthLimitedRMSCount] = 0;

        if (s_arg[i_arg].n_items >= 4) {
          if (strncasecmp(s_arg[i_arg].list[3], "scaleByOneOverOmegaSquared", strlen(s_arg[i_arg].list[3])) == 0) {
            bandwidthLimitedRMS_Omega[bandwidthLimitedRMSCount] = 1;
          } else if (strncasecmp(s_arg[i_arg].list[3], "noPV", strlen(s_arg[i_arg].list[3])) == 0) {
            bandwidthLimitedRMS_noPV[bandwidthLimitedRMSCount] = 1;
          }
        }
        if (s_arg[i_arg].n_items == 5) {
          if (strncasecmp(s_arg[i_arg].list[4], "scaleByOneOverOmegaSquared", strlen(s_arg[i_arg].list[4])) == 0) {
            bandwidthLimitedRMS_Omega[bandwidthLimitedRMSCount] = 1;
          } else if (strncasecmp(s_arg[i_arg].list[4], "noPV", strlen(s_arg[i_arg].list[4])) == 0) {
            bandwidthLimitedRMS_noPV[bandwidthLimitedRMSCount] = 1;
          }
        }
        bandwidthLimitedRMSCount++;
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
        bomb((char *)"too many filenames given", NULL);
    }
  }
  if (xParName && yParName && strcmp(xParName, yParName) == 0)
    SDDS_Bomb((char *)"xparameter name and y parameter name can not be the same!");

  ca_task_initialize();

  if (!dataTypeCLOGiven) {
    if (Precision == PRECISION_SINGLE) {
      DataType = SDDS_FLOAT;
    } else {
      DataType = SDDS_DOUBLE;
    }
  }
  if (!inputfile && optPVnames == 0)
    SDDS_Bomb((char *)"neither input filename nor -PVnames was given");
  if (optPVnames && outputfile)
    SDDS_Bomb((char *)"too many filenames given");
  if (optPVnames) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (!outputfile)
    bomb((char *)"output filename not given", NULL);
  if (append && erase)
    SDDS_Bomb((char *)"-append and -erase are incompatible options");
  if (append && generations)
    SDDS_Bomb((char *)"-append and -generations are incompatible options");
  if (erase && generations)
    SDDS_Bomb((char *)"-erase and -generations are incompatible options");
  if (generations)
    outputfile = MakeGenerationFilename(outputfile, generationsDigits, generationsDelimiter, NULL);

  outputPreviouslyExists = fexists(outputfile);
  if (outputPreviouslyExists && !erase && !append) {
    fprintf(stderr, "Error. File %s already exists.\n", outputfile);
    exit(1);
  }

  if (totalTimeSet) {
    if (stepsSet && !noWarnings)
      fprintf(stderr, "warning: option -time supersedes option -steps\n");
    NStep = TotalTime / TimeInterval + 1;
  } else {
    enforceTimeLimit = 0;
  }

  unitsFromFile = rmsPVPrefixFromFile = 0;
  readbackDataType = NULL;
  readbackPV = readbackName = readbackUnits = rmsPVPrefix = NULL;
  readbackOffset = readbackScale = NULL;
  readbackDataTypeStrings = NULL;

  if (inputfile) {
    long i;
    if (!getWaveformMonitorData(&readbackPV, &readbackName, &readbackUnits,
                                &rmsPVPrefix,
                                &readbackOffset, &readbackScale, &readbackDataType,
                                &readbacks, &waveformLength, &waveformOffset, inputfile,
                                DataType))
      SDDS_Bomb((char *)"problem getting waveform monitor input data");
    if (readbackUnits)
      unitsFromFile = 1;
    if (rmsPVPrefix)
      rmsPVPrefixFromFile = 1;
    if (!(readbackCHID = (chid *)malloc(sizeof(chid) * readbacks)))
      SDDS_Bomb((char *)"memory allocation failure");
    for (i = 0; i < readbacks; i++)
      readbackCHID[i] = NULL;
  } else {
    long i;
    readbacks = optPVnames;
    readbackPV = optPVname;
    waveformLength = 0;
    if (!(readbackName = (char **)malloc(sizeof(*readbackName) * readbacks)))
      SDDS_Bomb((char *)"memory allocation failure");
    if (!(readbackCHID = (chid *)malloc(sizeof(chid) * readbacks)))
      SDDS_Bomb((char *)"memory allocation failure");
    waveformLength = 0;
    if (!(readbackDataType = (int32_t *)malloc(sizeof(*readbackDataType) * readbacks)))
      SDDS_Bomb((char *)"memory allocation failure");
    for (i = 0; i < readbacks; i++) {
      readbackName[i] = readbackPV[i];
      readbackDataType[i] = DataType;
      readbackCHID[i] = NULL;
    }
  }
  for (i = 0; i < readbacks; i++) {
    if (ca_search(readbackPV[i], &(readbackCHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", readbackName[i]);
      exit(1);
    }
  }
  if (indeptUnitPV) {
    if (ca_search(indeptUnitPV, &indeptUnitChid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", indeptUnitPV);
      exit(1);
    }
  }
  if (indeptDeltaPV) {
    if (ca_search(indeptDeltaPV, &indeptDeltaChid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", indeptDeltaPV);
      exit(1);
    }
  }
  if (indeptOffsetPV) {
    if (ca_search(indeptOffsetPV, &indeptOffsetChid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", indeptOffsetPV);
      exit(1);
    }
  }

  if (rmsPVPrefixFromFile) {
    n = 0;
    for (i = 0; i < readbacks; i++) {
      for (j = 0; j < bandwidthLimitedRMSCount; j++) {
        if (strlen(rmsPVPrefix[i]) > 0) {
          n++;
        }
      }
    }
    if (!(bandwidthLimitedRMS_CHID = (chid *)malloc(sizeof(chid) * n)))
      SDDS_Bomb((char *)"memory allocation failure");
    n = 0;
    for (i = 0; i < readbacks; i++) {
      for (j = 0; j < bandwidthLimitedRMSCount; j++) {
        if (strlen(rmsPVPrefix[i]) > 0) {
          snprintf(buffer, sizeof(buffer), "%s:%ldHzBW", rmsPVPrefix[i], (long)(bandwidthLimitedRMS_MAX[j]));
          if (bandwidthLimitedRMS_noPV[j] == 0) {
            if (ca_search(buffer, &(bandwidthLimitedRMS_CHID[n])) != ECA_NORMAL) {
              fprintf(stderr, "error: problem doing search for %s\n", buffer);
              exit(1);
            }
            n++;
          }
        }
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
  }

  if (!inputfile && !dataTypeCLOGiven) {
    /* get the datatype from pv record */
    for (i = 0; i < readbacks; i++)
      readbackDataType[i] = CaTypeToDataType(ca_field_type(readbackCHID[i]));
  }
  if (waveformLength <= 0) {
    for (i = 0; i < readbacks; i++) {
      int wfLength;
      wfLength = ca_element_count(readbackCHID[i]);
      if (wfLength > waveformLength)
        waveformLength = wfLength;
    }
    if (waveformLength == 0)
      SDDS_Bomb((char *)"waveforms have zero length!");
  }
  if (indeptUnitPV) {
    indeptUnits = (char *)malloc(sizeof(char) * 40);
    if (ca_get(DBR_STRING, indeptUnitChid, indeptUnits) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading values for %s\n", indeptUnitPV);
      exit(1);
    }
  }
  if (indeptDeltaPV) {
    if (ca_get(DBR_DOUBLE, indeptDeltaChid, &indeptDelta) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading values for %s\n", indeptDeltaPV);
      exit(1);
    }
    if (!indeptUnitPV && !indeptUnits) {
      getUnits(&indeptDeltaPV, &indeptUnits, 1, &indeptDeltaChid, pendIOtime);
    }
  }
  if (indeptOffsetPV) {
    if (ca_get(DBR_LONG, indeptOffsetChid, &indeptOffset) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading values for %s\n", indeptOffsetPV);
      exit(1);
    }
  }
  scalars = 0;
  scalarPV = scalarName = scalarUnits = NULL;
  scalarFactor = scalarData = NULL;
  readMessage = NULL;

  if (scalarFile) {
    if (!getScalarMonitorDataModified(&scalarPV, &readMessage, &scalarName,
                                      &scalarUnits, &scalarSymbol, &scalarFactor, NULL, &scalarDataType,
                                      &scalars, scalarFile, GET_UNITS_IF_BLANK, &scalarCHID, pendIOtime))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  CondDeviceName = CondReadMessage = NULL;
  CondScaleFactor = NULL;
  CondLowerLimit = CondUpperLimit = NULL;
  CondHoldoff = NULL;

  if (CondFile) {
    if (!getConditionsData(&CondDeviceName, &CondReadMessage, &CondScaleFactor,
                           &CondLowerLimit, &CondUpperLimit, &CondHoldoff, &conditions,
                           CondFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!(CondDataBuffer = (double *)malloc(sizeof(*CondDataBuffer) * conditions)))
      SDDS_Bomb((char *)"allocation failure");
    if (!(CondCHID = (chid *)malloc(sizeof(*CondCHID) * conditions)))
      SDDS_Bomb((char *)"allocation failure");
    for (i = 0; i < conditions; i++)
      CondCHID[i] = NULL;
  }

  outputColumn = (void **)tmalloc(sizeof(void *) * (outputColumns = readbacks + 1));
  SDDS_CopyString((char **)&outputColumn[0], "Index");
  if (!unitsFromFile)
    readbackUnits = (char **)tmalloc(sizeof(*readbackUnits) * readbacks);
  waveformData = (void **)tmalloc(sizeof(void *) * readbacks);
  accWaveformData = (void **)tmalloc(sizeof(void *) * readbacks);
  if (logOnChange & LOGONCHANGE_WAVEFORMS)
    previousWaveformData = (void **)tmalloc(sizeof(void *) * readbacks);
  for (i = 0; i < readbacks; i++) {
    outputColumn[i + 1] = readbackName[i];
    if (!unitsFromFile)
      readbackUnits[i] = NULL;
  }
  if ((!unitsFromFile) && (!noUnits)) {
    getUnits(readbackPV, readbackUnits, readbacks, readbackCHID, pendIOtime);
  }
  scalarAccumData = previousScalarData = NULL;
  if (scalars) {
    scalarData = (double *)calloc(scalars, sizeof(*scalarData));
    if (accumulateAction)
      scalarAccumData = (double *)calloc(scalars, sizeof(*scalarAccumData));
    if (logOnChange & LOGONCHANGE_SCALARS)
      previousScalarData = (double *)calloc(scalars, sizeof(*previousScalarData));
    if (!scalarUnits) {
      scalarUnits = (char **)tmalloc(sizeof(*scalarUnits) * scalars);
    }
    strScalarData = (char **)malloc(sizeof(*strScalarData) * scalars);
    for (i = 0; i < scalars; i++) {
      strScalarData[i] = NULL;
      if (scalarDataType[i] == SDDS_STRING) {
        strScalarExist = 1;
        strScalarData[i] = (char *)malloc(sizeof(char) * 40);
      }
    }
  }

  if ((scalars) && (!scalarUnits)) {
    getUnits(scalarPV, scalarUnits, scalars, scalarCHID, pendIOtime);
  }

  index = (int32_t *)tmalloc(sizeof(*index) * waveformLength);
  if (indeptName)
    indeptValue = (double *)tmalloc(sizeof(*indeptValue) * waveformLength);
  for (i = 0; i < waveformLength; i++)
    index[i] = i;
  if (indeptName)
    for (i = 0; i < waveformLength; i++)
      indeptValue[i] = (i - indeptOffset) * indeptDelta;

  /* if append is true and the specified output file already exists, then move
     the file to a temporary file, then copy all tables. */

  if (append && outputPreviouslyExists) {
    if (!SDDS_CheckFile(outputfile)) {
      if (recover) {
        char commandBuffer[1024];
        fprintf(stderr, "warning: file %s is corrupted--reconstructing before appending--some data may be lost.\n", outputfile);
        snprintf(commandBuffer, sizeof(commandBuffer), "sddsconvert -recover -nowarnings %s", outputfile);
        system(commandBuffer);
      } else {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        SDDS_Bomb((char *)"unable to get data from existing file---try -append=recover (may truncate file)");
      }
    }

    if (!SDDS_InitializeInput(&originalfile_page, outputfile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    /* compare columns of output file to ReadbackNames column in input file */
    ColumnNames = (char **)SDDS_GetColumnNames(&originalfile_page, &NColumnNames);
    if (-1 == match_string((char *)"Index", ColumnNames, NColumnNames, EXACT_MATCH)) {
      printf("\"Index\" column doesn't exist in output file.\n");
      exit(1);
    }
    for (i = 0; i < readbacks; i++) {
      if (-1 == match_string(readbackName[i], ColumnNames, NColumnNames, EXACT_MATCH)) {
        printf("ReadbackName %s doesn't match any columns in output file.\n", readbackName[i]);
        exit(1);
      }
    }
    if (indeptName) {
      if (-1 == match_string(indeptName, ColumnNames, NColumnNames, EXACT_MATCH)) {
        printf("Independent column %s doesn't exist in output file.\n", indeptName);
        exit(1);
      }
    }
    temp = (char **)malloc(sizeof(char *));
    SDDS_CopyString(&temp[0], "Index");
    for (i = 0; i < NColumnNames; i++) {
      if (strcmp(ColumnNames[i], "Index") == 0)
        continue;
      if (indeptName && strcmp(ColumnNames[i], indeptName) == 0)
        continue;
      if (-1 == match_string(ColumnNames[i], readbackName, readbacks, EXACT_MATCH)) {
        printf("Column %s in output file doesn't match any readbackName.\n", ColumnNames[i]);
        exit(1);
      }
    }
    SDDS_FreeStringArray(temp, 1);
    SDDS_FreeStringArray(ColumnNames, NColumnNames);
    free(ColumnNames);

    /* compare parameters of output file to scalarName in input file */
    ParameterNames = (char **)SDDS_GetParameterNames(&originalfile_page, &NParameters);
    for (i = 0; i < scalars; i++) {
      if (-1 == match_string(scalarName[i], ParameterNames, NParameters, EXACT_MATCH)) {
        printf("ScalarName %s doesn't match any parameters in output file.\n", scalarName[i]);
        exit(1);
      }
    }
    for (j = 0; j < bandwidthLimitedRMSCount; j++) {
      for (i = 0; i < readbacks; i++) {
        snprintf(buffer, sizeof(buffer), "%s:RMS:%ldHz:%ldHz:BW", readbackName[i], (long)(bandwidthLimitedRMS_MIN[j]), (long)(bandwidthLimitedRMS_MAX[j]));
        if (-1 == match_string(buffer, ParameterNames, NParameters, EXACT_MATCH)) {
          printf("%s doesn't match any columns in output file.\n", buffer);
          exit(1);
        }
      }
    }
    if (rms) {
      for (i = 0; i < readbacks; i++) {
        snprintf(buffer, sizeof(buffer), "%s:RMS", readbackName[i]);
        if (-1 == match_string(buffer, ParameterNames, NParameters, EXACT_MATCH)) {
          printf("%s doesn't match any columns in output file.\n", buffer);
          exit(1);
        }
      }
    }

    SDDS_FreeStringArray(ParameterNames, NParameters);
    free(ParameterNames);

    /*check if output has Step, CAerros,Time,TimeOfDay, DayOfMonth auxiliary parameters*/
    if (SDDS_ReadPage(&originalfile_page) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Bomb((char *)"unable to get data from existing file---try -append=recover");
    }
    if (SDDS_CheckParameter(&originalfile_page, (char *)"Step", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
        SDDS_CheckParameter(&originalfile_page, (char *)"Time", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
        SDDS_CheckParameter(&originalfile_page, (char *)"TimeOfDay", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
        SDDS_CheckParameter(&originalfile_page, (char *)"DayOfMonth", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
        SDDS_CheckParameter(&originalfile_page, (char *)"CAerrors", NULL, SDDS_ANY_NUMERIC_TYPE, stderr) != SDDS_CHECK_OKAY ||
        SDDS_CheckParameter(&originalfile_page, (char *)"PageTimeStamp", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      SDDS_Bomb((char *)"unable to get auxiliary parameters from existing file.");
    }

    if (!SDDS_GetParameter(&originalfile_page, (char *)"StartHour", &StartHour) ||
        !SDDS_GetParameterAsDouble(&originalfile_page, (char *)"StartDayOfMonth", &StartDay) ||
        !SDDS_GetParameter(&originalfile_page, (char *)"StartTime", &StartTime)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }

    StartDay += StartHour / 24.0;
    if (!SDDS_InitializeAppend(&outTable, outputfile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, NULL);
    firstPage = 0;
  } else {
    /* start with a new file */
    getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth, &StartYear, &TimeStamp);
    RunStartTime = StartTime;
    if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 1, "Monitored devices", "Monitored devices", outputfile) ||
        !DefineLoggingTimeParameters(&outTable) ||
        !DefineLoggingTimeDetail(&outTable, TIMEDETAIL_EXTRAS) ||
        0 > SDDS_DefineColumn(&outTable, "Index", NULL, NULL, NULL, NULL, SDDS_LONG, 0))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (indeptName) {
      if (SDDS_DefineColumn(&outTable, indeptName, NULL, indeptUnits, NULL, NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    SDDS_EnableFSync(&outTable);
    if (scalars) {
      for (i = 0; i < scalars; i++)
        if (scalarSymbol) {
          if (!SDDS_DefineParameter(&outTable, scalarName[i], scalarSymbol[i], scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        } else {
          if (!SDDS_DefineParameter(&outTable, scalarName[i], NULL, scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        }
    }
    for (j = 0; j < bandwidthLimitedRMSCount; j++) {
      for (i = 0; i < readbacks; i++) {
        snprintf(buffer, sizeof(buffer), "%s:RMS:%ldHz:%ldHz:BW", readbackName[i], (long)(bandwidthLimitedRMS_MIN[j]), (long)(bandwidthLimitedRMS_MAX[j]));
        if (!SDDS_DefineSimpleParameter(&outTable, buffer, NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    if (rms) {
      for (i = 0; i < readbacks; i++) {
        snprintf(buffer, sizeof(buffer), "%s:RMS", readbackName[i]);
        if (!SDDS_DefineSimpleParameter(&outTable, buffer, NULL, SDDS_DOUBLE))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }

    for (pvIndex = 0; pvIndex < readbacks; pvIndex++) {
      if (!SDDS_DefineSimpleColumn(&outTable, readbackName[pvIndex], readbackUnits[pvIndex],
                                   readbackDataType[pvIndex]))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (xParGiven) {
      char buff[4][256];
      snprintf(buff[0], sizeof(buff[0]), "%sDimension", xParName);
      snprintf(buff[1], sizeof(buff[1]), "%sInterval", xParName);
      snprintf(buff[2], sizeof(buff[2]), "%sMinimum", xParName);
      snprintf(buff[3], sizeof(buff[3]), "%sMaximum", xParName);
      if (!SDDS_DefineSimpleParameter(&outTable, "Variable1Name", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[0], NULL, SDDS_LONG) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[1], NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[2], NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[3], NULL, SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (yParGiven) {
      char buff[4][256];
      snprintf(buff[0], sizeof(buff[0]), "%sDimension", yParName);
      snprintf(buff[1], sizeof(buff[1]), "%sInterval", yParName);
      snprintf(buff[2], sizeof(buff[2]), "%sMinimum", yParName);
      snprintf(buff[3], sizeof(buff[3]), "%sMaximum", yParName);
      if (!SDDS_DefineSimpleParameter(&outTable, "Variable2Name", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[0], NULL, SDDS_LONG) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[1], NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[2], NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleParameter(&outTable, buff[3], NULL, SDDS_DOUBLE))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (!SDDS_DefineSimpleParameters(&outTable, comments, commentParameter, NULL, SDDS_STRING) ||
        !SDDS_WriteLayout(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }

  for (i = 0; i < readbacks; i++) {
    accWaveformData[i] = tmalloc(SizeOfDataType(readbackDataType[i]) * waveformLength);
  }
  if (logOnChange & LOGONCHANGE_WAVEFORMS) {
    for (i = 0; i < readbacks; i++) {
      previousWaveformData[i] = tmalloc(SizeOfDataType(readbackDataType[i]) * waveformLength);
    }
  }

  if (bandwidthLimitedRMSCount > 0) {
    if (!(fftIndepVariable = (double *)malloc(sizeof(double) * waveformLength)))
      SDDS_Bomb((char *)"memory allocation failure");
    if (!(fftDepenQuantity = (double *)malloc(sizeof(double) * waveformLength)))
      SDDS_Bomb((char *)"memory allocation failure");
    for (j = 0; j < waveformLength; j++) {
      fftIndepVariable[j] = TimeInterval * j / waveformLength;
    }
  }

  accumulate = 1;      /* number of point we are about to accumulate */
  accumulateClear = 1; /* indicates need to clear accumulated data for waveforms */
  expectedTime = getTimeInSecs();
  firstPage = 1;
  if (!totalTimeSet && accumulateNumber != 0)
    NStep = NStep * accumulateNumber;
  /*allocate memory for waveformData*/
  AllocateWaveformMemory(waveformData, readbackDataType, readbacks, waveformLength, waveformOffset);

  for (Step = 0; Step < NStep; Step++) {
    if (singleShot && (!accumulateNoQuery || accumulateClear)) {
      if (singleShot != SS_NOPROMPT) {
        fputs("Type <cr> to read, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
      }
      fgets(answer, ANSWER_LENGTH, stdin);
      if (answer[0] == 'q' || answer[0] == 'Q') {
        SDDS_Terminate(&outTable);
        exit(0);
      }
    } else if (Step) {
      if ((timeToWait = expectedTime - getTimeInSecs()) < 0)
        timeToWait = 0;
      if (timeToWait > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(timeToWait);
      } else {
        ca_poll();
      }
      expectedTime += TimeInterval;
    } else if (Step == 0) {
      expectedTime += TimeInterval;
    }
    if (CondFile &&
        !PassesConditions(CondDeviceName, CondReadMessage, CondScaleFactor,
                          CondDataBuffer, CondLowerLimit, CondUpperLimit,
                          CondHoldoff, conditions, CondMode, CondCHID, pendIOtime)) {
      if (CondMode & TOUCH_OUTPUT)
        TouchFile(outputfile);
      if (verbose) {
        printf("Step %ld---values failed condition test--continuing\n", Step);
        fflush(stdout);
      }
      if (CondMode & RETAKE_STEP)
        Step--;
      continue;
    }
    if ((CAerrors = ReadWaveforms(readbackPV, waveformData, waveformLength, waveformOffset, readbacks, readbackDataType, readbackCHID, pendIOtime)) != 0)
      switch (onCAerror) {
      case ONCAERROR_USEZERO:
        break;
      case ONCAERROR_SKIPPAGE:
        continue;
      case ONCAERROR_EXIT:
        exit(1);
        break;
      }
    if (scalarPV) {
      if (!strScalarExist)
        CAerrors = ReadScalarValues(scalarPV, readMessage, scalarFactor,
                                    scalarData, scalars, scalarCHID, pendIOtime);
      else
        CAerrors = ReadMixScalarValues(scalarPV, readMessage, scalarFactor, scalarData, scalarDataType,
                                       strScalarData, scalars, scalarCHID, pendIOtime);
      if (CAerrors != 0) {
        switch (onCAerror) {
        case ONCAERROR_USEZERO:
          break;
        case ONCAERROR_SKIPPAGE:
          continue;
        case ONCAERROR_EXIT:
          exit(1);
          break;
        }
      }
      if (scalarAccumData) {
        for (i = 0; i < scalars; i++)
          scalarAccumData[i] += scalarData[i];
      }
    }
    ElapsedTime = EpochTime = getTimeInSecs();

    for (i = 0; i < readbacks; i++) {
      if (readbackOffset && readbackScale)
        ScaleWaveformData((void *)waveformData[i], readbackDataType[i], waveformLength, readbackOffset[i], readbackScale[i]);
    }
    if (accumulateNumber != 0 && accumulateNumber == accumulate) {
      for (i = 0; i < readbacks; i++) {
        AccumulateWaveformData((void *)waveformData[i], (void *)accWaveformData[i], readbackDataType[i], waveformLength, accumulate, accumulateClear);
        if (accumulateAction == AA_AVERAGE)
          AveAccumulate((void *)accWaveformData[i], readbackDataType[i], waveformLength, accumulateNumber);
      }
    }

    if ((logOnChange) && (!firstPage)) {
      if (accumulateNumber == 0 || accumulateNumber == accumulate) {
        j = 0;
        if (logOnChange & LOGONCHANGE_SCALARS) {
          if (scalarPV) {
            if (!scalarAccumData) {
              for (i = 0; i < scalars; i++) {
                if (scalarData[i] != previousScalarData[i]) {
                  j = 1;
                  break;
                }
              }
            } else {
              for (i = 0; i < scalars; i++) {
                if (scalarAccumData[i] != previousScalarData[i]) {
                  j = 1;
                  break;
                }
              }
            }
          }
        }

        if (logOnChange & LOGONCHANGE_WAVEFORMS)
          for (i = 0; i < readbacks; i++) {
            if (accumulateNumber == 0) {
              if (!j) {
                if (CheckIfWaveformChanged((void *)waveformData[i], (void *)previousWaveformData[i], readbackDataType[i], waveformLength))
                  j = 1;
              }
            } else {
              if (!j) {
                if (CheckIfWaveformChanged((void *)accWaveformData[i], (void *)previousWaveformData[i], readbackDataType[i], waveformLength))
                  j = 1;
              }
            }
          }
        if (!j) {
          if (verbose) {
            printf("Step %ld---Not logging because nothing changed--continuing\n", Step);
            fflush(stdout);
          }
          accumulateClear = 1;
          accumulate = 1;
          continue;
        }
      }
    }

    if (firstPage)
      SDDS_CopyString(&TimeStamp, makeTimeStamp(EpochTime));
    PageTimeStamp = makeTimeStamp(EpochTime);

    ElapsedTime -= StartTime;
    RunTime = getTimeInSecs() - RunStartTime;
    if (enforceTimeLimit && RunTime > TotalTime) {
      NStep = Step;
    }
    TimeOfDay = StartHour + ElapsedTime / 3600.0;
    DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
    if (verbose) {
      printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
      fflush(stdout);
    }
    if (accumulateNumber == 0 || accumulateNumber == accumulate) {
      /* must write out data as there is no accumulation or accumulators are full */
      if (firstPage) {
        if (append && outputPreviouslyExists) {
          if (!SDDS_StartTable(&outTable, waveformLength) ||
              !SDDS_CopyParameters(&outTable, &originalfile_page) ||
              !SDDS_Terminate(&originalfile_page))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        } else {
          if (!SDDS_StartTable(&outTable, waveformLength) ||
              (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                   "TimeStamp", TimeStamp,
                                   "StartTime", StartTime, "StartJulianDay", (short)StartJulianDay,
                                   "YearStartTime", computeYearStartTime(StartTime),
                                   "StartDayOfMonth", (short)StartDay, "StartHour", (float)StartHour,
                                   "StartYear", (short)StartYear, "StartMonth",
                                   (short)StartMonth, NULL)) ||
              !SetCommentParameters(&outTable, commentParameter, commentText, comments))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          if (xParGiven) {
            char buff[4][256];
            snprintf(buff[0], sizeof(buff[0]), "%sDimension", xParName);
            snprintf(buff[1], sizeof(buff[1]), "%sInterval", xParName);
            snprintf(buff[2], sizeof(buff[2]), "%sMinimum", xParName);
            snprintf(buff[3], sizeof(buff[3]), "%sMaximum", xParName);
            if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                    "Variable1Name", xParName,
                                    buff[0], xParDim, buff[1], xParInterval, buff[2], xParMin,
                                    buff[3], xParMax, NULL))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (yParGiven) {
            char buff[4][256];
            snprintf(buff[0], sizeof(buff[0]), "%sDimension", yParName);
            snprintf(buff[1], sizeof(buff[1]), "%sInterval", yParName);
            snprintf(buff[2], sizeof(buff[2]), "%sMinimum", yParName);
            snprintf(buff[3], sizeof(buff[3]), "%sMaximum", yParName);
            if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                    "Variable2Name", yParName,
                                    buff[0], yParDim, buff[1], yParInterval, buff[2], yParMin,
                                    buff[3], yParMax, NULL))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
      }
      firstPage = 0;
      if (!SDDS_StartTable(&outTable, waveformLength) ||
          !SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                              "Step", Step, "Time", EpochTime, "TimeOfDay", (float)TimeOfDay,
                              "DayOfMonth", (float)DayOfMonth, "PageTimeStamp", PageTimeStamp,
                              "CAerrors", CAerrors, NULL))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (!scalarAccumData) {
        /* no accumulation for scalars */
        for (i = 0; i < scalars; i++) {
          if (scalarDataType[i] == SDDS_STRING) {
            if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                    scalarName[i], strScalarData[i], NULL))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          } else {
            if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                    scalarName[i], scalarData[i], NULL))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
        if (logOnChange & LOGONCHANGE_SCALARS) {
          for (i = 0; i < scalars; i++)
            previousScalarData[i] = scalarData[i];
        }
      } else {
        /* accumulation (averaging) for scalars */
        for (i = 0; i < scalars; i++) {
          if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                  scalarName[i], scalarAccumData[i] / accumulateNumber, NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          if (logOnChange & LOGONCHANGE_SCALARS) {
            for (i = 0; i < scalars; i++)
              previousScalarData[i] = scalarAccumData[i];
          }
          scalarAccumData[i] = 0;
        }
      }
      if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, index, waveformLength, "Index"))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (indeptName && !SDDS_SetColumn(&outTable, SDDS_BY_NAME, indeptValue, waveformLength, indeptName))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      n = 0;
      for (i = 0; i < readbacks; i++) {
        if (accumulateNumber == 0) {
          /* no accumulation is being done so just set the column */
          if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, waveformData[i],
                              waveformLength, readbackName[i])) {
            fprintf(stderr, "error: unable to set values for column %s\nknown columns are:",
                    readbackName[i]);
            for (j = 0; j < outTable.layout.n_columns; j++)
              fprintf(stderr, "  %s\n", outTable.layout.column_definition[j].name);
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (bandwidthLimitedRMSCount > 0) {
            if (readbackDataType[i] == SDDS_DOUBLE) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((double *)waveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_FLOAT) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((float *)waveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_LONG) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((long *)waveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_SHORT) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((short *)waveformData[i])[j];
              }
            } else {
              fprintf(stderr, "Error: sddswmonitor: the -rms option requires that waveforms be of type double, float, long or short\n");
              return (1);
            }
            for (j = 0; j < bandwidthLimitedRMSCount; j++) {
              snprintf(buffer, sizeof(buffer), "%s:RMS:%ldHz:%ldHz:BW", readbackName[i], (long)(bandwidthLimitedRMS_MIN[j]), (long)(bandwidthLimitedRMS_MAX[j]));
              bandwidthLimitedRMSResult = bandwidthLimitedRMS(fftIndepVariable, fftDepenQuantity, waveformLength, bandwidthLimitedRMS_MIN[j], bandwidthLimitedRMS_MAX[j], bandwidthLimitedRMS_Omega[j]);
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      bandwidthLimitedRMSResult, NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

              if (rmsPVPrefixFromFile) {
                if (strlen(rmsPVPrefix[i]) > 0) {
                  if (bandwidthLimitedRMS_noPV[j] == 0) {
                    if (ca_put(DBR_DOUBLE, bandwidthLimitedRMS_CHID[n], &bandwidthLimitedRMSResult) != ECA_NORMAL) {
                      fprintf(stderr, "error: setting one or more bandwidth limited RMS PVs\n");
                      exit(1);
                    }
                    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
                      fprintf(stderr, "error: setting one or more bandwidth limited RMS PVs\n");
                      exit(1);
                    }
                    n++;
                  }
                }
              }
            }
          }
          if (rms) {
            snprintf(buffer, sizeof(buffer), "%s:RMS", readbackName[i]);
            if (readbackDataType[i] == SDDS_DOUBLE) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValue((double *)waveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_FLOAT) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueFloat((float *)waveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_LONG) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueLong((long *)waveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_SHORT) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueShort((short *)waveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else {
              fprintf(stderr, "Error: sddswmonitor: the -rms option requires that waveforms be of type double, float, long or short\n");
              return (1);
            }
          }
          if (logOnChange & LOGONCHANGE_WAVEFORMS)
            CopyWaveformData((void *)waveformData[i], (void *)previousWaveformData[i], readbackDataType[i], waveformLength);
        } else {
          /* accumulation is being done, so finish it up and set the column */
          if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, accWaveformData[i],
                              waveformLength, readbackName[i])) {
            fprintf(stderr, "error: unable to set values for column %s\nknown columns are:",
                    readbackName[i]);
            for (j = 0; j < outTable.layout.n_columns; j++)
              fprintf(stderr, "  %s\n", outTable.layout.column_definition[j].name);
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (bandwidthLimitedRMSCount > 0) {
            if (readbackDataType[i] == SDDS_DOUBLE) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((double *)accWaveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_FLOAT) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((float *)accWaveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_LONG) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((long *)accWaveformData[i])[j];
              }
            } else if (readbackDataType[i] == SDDS_SHORT) {
              for (j = 0; j < waveformLength; j++) {
                fftDepenQuantity[j] = ((short *)accWaveformData[i])[j];
              }
            } else {
              fprintf(stderr, "Error: sddswmonitor: the -rms option requires that waveforms be of type double, float, long or short\n");
              return (1);
            }
            for (j = 0; j < bandwidthLimitedRMSCount; j++) {
              snprintf(buffer, sizeof(buffer), "%s:RMS:%ldHz:%ldHz:BW", readbackName[i], (long)(bandwidthLimitedRMS_MIN[j]), (long)(bandwidthLimitedRMS_MAX[j]));
              bandwidthLimitedRMSResult = bandwidthLimitedRMS(fftIndepVariable, fftDepenQuantity, waveformLength, bandwidthLimitedRMS_MIN[j], bandwidthLimitedRMS_MAX[j], bandwidthLimitedRMS_Omega[j]);
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      bandwidthLimitedRMSResult, NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              if (rmsPVPrefixFromFile) {
                if (strlen(rmsPVPrefix[i]) > 0) {
                  if (bandwidthLimitedRMS_noPV[j] == 0) {
                    if (ca_put(DBR_DOUBLE, bandwidthLimitedRMS_CHID[n], &bandwidthLimitedRMSResult) != ECA_NORMAL) {
                      fprintf(stderr, "error: setting one or more bandwidth limited RMS PVs\n");
                      exit(1);
                    }
                    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
                      fprintf(stderr, "error: setting one or more bandwidth limited RMS PVs\n");
                      exit(1);
                    }
                    n++;
                  }
                }
              }
            }
          }
          if (rms) {
            snprintf(buffer, sizeof(buffer), "%s:RMS", readbackName[i]);
            if (readbackDataType[i] == SDDS_DOUBLE) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValue((double *)accWaveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_FLOAT) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueFloat((float *)accWaveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_LONG) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueLong((long *)accWaveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else if (readbackDataType[i] == SDDS_SHORT) {
              if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, buffer,
                                      rmsValueShort((short *)accWaveformData[i], waveformLength), NULL))
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            } else {
              fprintf(stderr, "Error: sddswmonitor: the -rms option requires that waveforms be of type double, float, long or short\n");
              return (1);
            }
          }
          if (logOnChange & LOGONCHANGE_WAVEFORMS)
            CopyWaveformData((void *)accWaveformData[i], (void *)previousWaveformData[i], readbackDataType[i], waveformLength);
        }
      }
      /* record fact that we need to clear the waveform accumulators on the next step */
      accumulateClear = 1;
      if (!SDDS_WriteTable(&outTable))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      accumulate = 1;
    } else {
      for (i = 0; i < readbacks; i++) {
        AccumulateWaveformData((void *)waveformData[i], (void *)accWaveformData[i], readbackDataType[i], waveformLength, accumulate, accumulateClear);
      }
      accumulateClear = 0;
      accumulate++;
    }
  }

  if (bandwidthLimitedRMSCount > 0) {
    free(fftIndepVariable);
    free(fftDepenQuantity);
  }

  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }
  /*clean up memory */
  if (accWaveformData) {
    for (i = 0; i < readbacks; i++)
      free(accWaveformData[i]);
    free(accWaveformData);
  }
  if (logOnChange & LOGONCHANGE_WAVEFORMS) {
    for (i = 0; i < readbacks; i++)
      free(previousWaveformData[i]);
    free(previousWaveformData);
  }
  if (TimeStamp)
    free(TimeStamp);
  if (index)
    free(index);
  FreeWaveformMemory(waveformData, readbackDataType, readbacks, waveformLength, waveformOffset);

  FreeReadMemory(scalars, scalarPV, scalarName, scalarUnits, readMessage, scalarData,
                 scalarFactor, scalarAccumData, previousScalarData, NULL);
  if (inputfile)
    FreeReadMemory(readbacks, readbackPV, readbackName, readbackUnits, readbackDataTypeStrings,
                   readbackOffset, readbackScale, NULL, NULL, readbackDataType);
  else
    FreeReadMemory(readbacks, NULL, NULL, readbackUnits, readbackDataTypeStrings,
                   readbackOffset, readbackScale, NULL, NULL, readbackDataType);
  FreeReadMemory(conditions, CondDeviceName, CondReadMessage, NULL, NULL, CondScaleFactor,
                 CondLowerLimit, CondUpperLimit, CondHoldoff, NULL);

  if (outputColumn) {
    free((char *)outputColumn[0]); /*index column name*/
    free((char **)outputColumn);
    outputColumn = NULL;
  }
  if (indeptName)
    free(indeptName);
  if (indeptUnits)
    free(indeptUnits);
  if (indeptUnitPV)
    free(indeptUnitPV);
  if (indeptDeltaPV)
    free(indeptDeltaPV);
  if (indeptOffsetPV)
    free(indeptOffsetPV);
  if (indeptValue)
    free(indeptValue);
  if (readbackCHID)
    free(readbackCHID);
  if (scalarCHID)
    free(scalarCHID);
  if (strScalarData) {
    for (i = 0; i < scalars; i++)
      if (strScalarData[i])
        free(strScalarData[i]);
    free(strScalarData);
  }
  if (scalarDataType)
    free(scalarDataType);
  if (scalarSymbol)
    free(scalarSymbol);

  if (CondCHID)
    free(CondCHID);
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return 0;
}

/*allocate memory for waveformdata */
void AllocateWaveformMemory(void **waveformData, int32_t *readbackDataType,
                            long readbacks, long waveformLength, int32_t *waveformOffset) {
  long i, j;

  for (i = 0; i < readbacks; i++) {
    if (readbackDataType[i] == SDDS_STRING) {
      ((char ***)waveformData)[i] = (char **)tmalloc(sizeof(char *) * readbacks);
      for (j = 0; j < waveformLength; j++)
        ((char **)waveformData[i])[j] = (char *)tmalloc(sizeof(char) * 40);
    } else {
      waveformData[i] = tmalloc(SizeOfDataType(readbackDataType[i]) * (waveformLength+(waveformOffset?waveformOffset[i]:0)));
    }
  }
}

void FreeWaveformMemory(void **waveformData, int32_t *readbackDataType,
                        long readbacks, long waveformLength, int32_t *waveformOffset) {
  long i, j;

  if (waveformData) {
    for (i = 0; i < readbacks; i++) {
      if (readbackDataType[i] == SDDS_STRING) {
        for (j = 0; j < (waveformLength+(waveformOffset?waveformOffset[i]:0)); j++)
          free(((char **)waveformData[i])[j]);
        free((char **)waveformData[i]);
      } else
        free(waveformData[i]);
    }
    free(waveformData);
  }
}
void FreeReadMemory(long variables, char **pvs, char **names, char **units, char **typeStrings,
                    double *data1, double *data2, double *data3, double *data4, int32_t *dataType) {
  long i;
  if (!variables)
    return;
  if (data1)
    free(data1);
  if (data2)
    free(data2);
  if (data3)
    free(data3);
  if (data4)
    free(data4);
  if (dataType)
    free(dataType);
  if (!pvs && !names && !units && !typeStrings)
    return;
  for (i = 0; i < variables; i++) {
    if (names && names[i]) {
      free(names[i]);
      names[i] = NULL;
    }
    if (units && units[i])
      free(units[i]);
    if (typeStrings && typeStrings[i])
      free(typeStrings[i]);
  }
  if (names)
    free(names);
  if (units)
    free(units);
  if (typeStrings)
    free(typeStrings);
  names = units = typeStrings = NULL;
  if (pvs) {
    for (i = 0; i < variables; i++) {
      if (pvs[i]) {
        free(pvs[i]);
        pvs[i] = NULL;
      }
    }
    free(pvs);
    pvs = NULL;
  }
}

void ScaleWaveformData(void *data, long dataType, long length, double offset, double scale) {
  switch (dataType) {
  case SDDS_SHORT:
    while (length--)
      ((short *)data)[length] = (((short *)data)[length] - offset) * scale;
    break;
  case SDDS_LONG:
    while (length--)
      ((int32_t *)data)[length] = (((int32_t *)data)[length] - offset) * scale;
    break;
  case SDDS_FLOAT:
    while (length--)
      ((float *)data)[length] = (((float *)data)[length] - offset) * scale;
    break;
  case SDDS_DOUBLE:
    while (length--)
      ((double *)data)[length] = (((double *)data)[length] - offset) * scale;
    break;
  case SDDS_CHARACTER:
    if (!(offset == 0 && scale == 1))
      bomb((char *)"Scaling waveform data not supported for characters.", NULL);
    break;
  case SDDS_STRING:
    if (!(offset == 0 && scale == 1))
      bomb((char *)"Scaling waveform data not supported for strings.", NULL);
    break;
  default:
    bomb((char *)"Unsupported type passed to ScaleWaveformData.", NULL);
    break;
  }
}

void CopyWaveformData(void *data, void *previousData, long dataType, long length) {
  switch (dataType) {
  case SDDS_SHORT:
    while (length--)
      ((short *)previousData)[length] = ((short *)data)[length];
    break;
  case SDDS_LONG:
    while (length--)
      ((int32_t *)previousData)[length] = ((int32_t *)data)[length];
    break;
  case SDDS_FLOAT:
    while (length--)
      ((float *)previousData)[length] = ((float *)data)[length];
    break;
  case SDDS_DOUBLE:
    while (length--)
      ((double *)previousData)[length] = ((double *)data)[length];
    break;
  default:
    bomb((char *)"Unsupported type passed to CopyWaveformData.", NULL);
    break;
  }
}

long CheckIfWaveformChanged(void *data, void *previousData, long dataType, long length) {
  switch (dataType) {
  case SDDS_SHORT:
    while (length--) {
      if (((short *)previousData)[length] != ((short *)data)[length])
        return 1;
    }
    break;
  case SDDS_LONG:
    while (length--) {
      if (((int32_t *)previousData)[length] != ((int32_t *)data)[length])
        return 1;
    }
    break;
  case SDDS_FLOAT:
    while (length--) {
      if (((float *)previousData)[length] != ((float *)data)[length])
        return 1;
    }
    break;
  case SDDS_DOUBLE:
    while (length--) {
      if (((double *)previousData)[length] != ((double *)data)[length])
        return 1;
    }
    break;
  default:
    bomb((char *)"Unsupported type passed to CheckIfWaveformChanged.", NULL);
    break;
  }
  return 0;
}

void AccumulateWaveformData(void *data, void *accData, long dataType, long length, long accumulate, long accumulateClear) {
  long length2;
  length2 = length;
  if (accumulateClear == 1) {
    switch (dataType) {
    case SDDS_SHORT:
      while (length2--)
        ((short *)accData)[length2] = 0;
      break;
    case SDDS_LONG:
      while (length2--)
        ((int32_t *)accData)[length2] = 0;
      break;
    case SDDS_FLOAT:
      while (length2--)
        ((float *)accData)[length2] = 0.0;
      break;
    case SDDS_DOUBLE:
      while (length2--)
        ((double *)accData)[length2] = 0.0;
      break;
    default:
      break;
    }
  }
  switch (dataType) {
  case SDDS_SHORT:
    while (length--)
      ((short *)accData)[length] = ((short *)accData)[length] + ((short *)data)[length];
    break;
  case SDDS_LONG:
    while (length--)
      ((int32_t *)accData)[length] = ((int32_t *)accData)[length] + ((int32_t *)data)[length];
    break;
  case SDDS_FLOAT:
    while (length--)
      ((float *)accData)[length] = ((float *)accData)[length] + ((float *)data)[length];
    break;
  case SDDS_DOUBLE:
    while (length--)
      ((double *)accData)[length] = ((double *)accData)[length] + ((double *)data)[length];
    break;
  default:
    bomb((char *)"Unsupported type passed to AccumulateWaveformData.", NULL);
    break;
  }
}

void AveAccumulate(void *accData, long dataType, long length, long number) {
  switch (dataType) {
  case SDDS_SHORT:
    while (length--)
      ((short *)accData)[length] = (((short *)accData)[length]) / number;
    break;
  case SDDS_LONG:
    while (length--)
      ((int32_t *)accData)[length] = (((int32_t *)accData)[length]) / number;
    break;
  case SDDS_FLOAT:
    while (length--)
      ((float *)accData)[length] = (((float *)accData)[length]) / number;
    break;
  case SDDS_DOUBLE:
    while (length--)
      ((double *)accData)[length] = (((double *)accData)[length]) / number;
    break;
  default:
    bomb((char *)"Unsupported type passed to AveAccumulate.", NULL);
    break;
  }
}

double rmsValueFloat(float *y, long n) {
  long i;
  double sum;

  if (!n)
    return (0.0);
  for (i = sum = 0; i < n; i++)
    sum += y[i] * y[i];
  return (sqrt(sum / n));
}

double rmsValueLong(long *y, long n) {
  long i;
  double sum;

  if (!n)
    return (0.0);
  for (i = sum = 0; i < n; i++)
    sum += y[i] * y[i];
  return (sqrt(sum / n));
}

double rmsValueShort(short *y, long n) {
  long i;
  double sum;

  if (!n)
    return (0.0);
  for (i = sum = 0; i < n; i++)
    sum += y[i] * y[i];
  return (sqrt(sum / n));
}

double bandwidthLimitedRMS(double *tdata, double *data, long rows, double minFreq, double maxFreq, short scaleByOneOverOmegaSquared) {
  long n_freq, i, max;
  double length, *real_imag, df, *fdata, *psd, *psdInteg, result;

  length = ((double)rows) * (tdata[rows - 1] - tdata[0]) / ((double)rows - 1.0);

  real_imag = (double *)tmalloc(sizeof(double) * (2 * rows + 2));
  for (i = 0; i < rows; i++) {
    real_imag[2 * i] = data[i];
    real_imag[2 * i + 1] = 0;
  }
  complexFFT(real_imag, rows, 0);
  n_freq = rows / 2 + 1;

  df = 1.0 / length;

  fdata = (double *)tmalloc(sizeof(double) * n_freq);
  psd = (double *)tmalloc(sizeof(*psd) * n_freq);
  psdInteg = (double *)tmalloc(sizeof(*psdInteg) * n_freq);
  psdInteg[0] = 0;
  max = 0;

  for (i = 0; i < n_freq; i++) {
    fdata[i] = i * df;
    psd[i] = (sqr(real_imag[2 * i]) + sqr(real_imag[2 * i + 1])) / df;
    if (i != 0 && !(i == (n_freq - 1) && rows % 2 == 0)) {
      real_imag[2 * i] *= 2;
      real_imag[2 * i + 1] *= 2;
      psd[i] *= 2;
    }
    if ((fdata[i] >= minFreq) && (fdata[i] <= maxFreq)) {
      if (scaleByOneOverOmegaSquared) {
        psd[i] = psd[i] / (pow((fdata[i] * PI * 2), 4));
      }
      if (i > 0) {
        psdInteg[i] = psdInteg[i - 1] + (psd[i - 1] + psd[i]) * df / 2.;
        max = i;
      }
    } else {
      psd[i] = 0;
      psdInteg[i] = 0;
    }
  }

  /*RMS mean taking the square root of cumulative power*/
  result = sqrt(psdInteg[max]);

  free(psdInteg);
  free(fdata);
  free(psd);
  free(real_imag);
  return (result);
}
