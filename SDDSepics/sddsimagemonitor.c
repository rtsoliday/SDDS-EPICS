/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddsimagemonitor.c
 * purpose: reads multiple dimension image data and writes to output file. 
 * output: if the image data is two dimensional, there will only be one page of data if steps=1, the number
 *         of pages will be equal to the number of steps.
 *         if the third dimension number is greater than 1, then only one step is allowed. The number of pages will 
 *         the third dimension size.
 * 
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "fftpackC.h"

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
#define CLO_ERASE 3
#define CLO_SINGLE_SHOT 4
#define CLO_TIME 5
#define CLO_PRECISION 6
#define CLO_PROMPT 7
#define CLO_ONCAERROR 8
#define CLO_EZCATIMING 9
#define CLO_SCALARS 10
#define CLO_CONDITIONS 11
#define CLO_PVNAMES 12
#define CLO_GENERATIONS 13
#define CLO_COMMENT 14
#define CLO_ENFORCETIMELIMIT 15
#define CLO_OFFSETTIMEOFDAY 16
#define CLO_DATATYPE 17
#define CLO_NOWARNINGS 18
#define CLO_PENDIOTIME 19
#define CLO_XPARAMETER 20
#define CLO_YPARAMETER 21
#define CLO_SINGLECOLUMN 22
#define CLO_LOGONCHANGE 23
#define CLO_DEVICE 24
#define CLO_RETAKESTEP 25
#define CLO_FLUSHSTEPS 26
#define COMMANDLINE_OPTIONS 27
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval", "steps", "verbose", "erase", "singleshot", "time",
  "precision", "prompt", "oncaerror",
  "ezcatiming", "scalars", "conditions", "pvnames", "generations", "comment",
  "enforcetimelimit", "offsettimeofday", "datatype", "nowarnings", "pendiotime", "xparameter",
  "yparameter", "singleColumn", "logOnChange", "device", "retakestep", "flushSteps"};

#define ONCAERROR_USEZERO 0
#define ONCAERROR_SKIPPAGE 1
#define ONCAERROR_EXIT 2
#define ONCAERROR_OPTIONS 3
char *oncaerror_options[ONCAERROR_OPTIONS] = {
  "usezero",
  "skippage",
  "exit",
};

#define NAuxiliaryParameterNames 4
char *AuxiliaryParameterNames[NAuxiliaryParameterNames] = {
  "Step",
  "Time",
  "TimeOfDay",
  "DayOfMonth",
};

#define NTimeUnitNames 4
char *TimeUnitNames[NTimeUnitNames] = {
  "seconds",
  "minutes",
  "hours",
  "days",
};
double TimeUnitFactor[NTimeUnitNames] = {
  1,
  60,
  3600,
  86400,
};

static char *USAGE1 = "sddsimagemonitor [<inputfile> | -PVnames=<name>[,<name>]]\n\
    <outputfile> [{-erase | -generations[=digits=<integer>][,delimiter=<string>}]\n\
    [-pendIOtime=<seconds>]\n\
    [-steps=<integer> | -time=<value>[,<units>]] [-interval=<value>[,<units>]]\n\
    [-enforceTimeLimit] [-offsetTimeOfDay]\n\
    [-verbose] [-singleShot{=noprompt|stdout}] [-precision={single|double}]\n\
    [-dataType={short|long|float|double|character|string}]\n\
    [-onCAerror={useZero|skipPage|exit}] \n\
    [-scalars=<filename>] [-flushSteps=<interger>]\n\
    [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
    [-comment=<parameterName>,<text>] [-nowarnings] \n\
    [-xParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>] \n\
    [-yParameter=dimension=<value>[,name=<name>][,minimum=<value>][,maximum=<value>][,interval=<value>] \n\
     \n\n";
static char *USAGE2 = "Writes values of process variables to a binary SDDS file.\n\
<inputfile>        SDDS input file containing the column \"WaveformPV\" for providing the image pv name \n\
                   for example AR:SLM1:image1.\n\
                   the default dataType for image data is unsigned short. For now, we only consider two dimensional image data.\n\
PVnames            specifies a list of image PV names to read for example AR:SLM1:image1, the image data is stored \n\
                   by pv <imagePV>:ArrayData, the column size is provided by <imagePV>:ArraySize0_RBV, the row size is provived \n\
                   by <imagePV>:ArraySize1_RBV, for now, we only consider two dimensional image data.\n\
<outputfile>       SDDS output file, each page contains the data for one image pv, the number of columns is provided by  \n\
                   <imagePV>:ArraySize0_RBV, and the number of rows is provided  <imagePV>:ArraySize1_RBV, the data is provided by \n\
                   <imagePV>:ArrayData. \n\
generations        The output is sent to the file <outputfile>-<N>, where <N> is\n\
                   the smallest positive integer such that the file does not already \n\
                   exist.   By default, four digits are used for formating <N>, so that\n\
                   the first generation number is 0001.\n\
erase              outputfile is erased before execution.\n\
steps              number of reads for each process variable.\n\
time               total time (in seconds) for monitoring;\n\
                   valid time units are seconds, minutes, hours, or days.\n";
static char *USAGE3 = "enforceTimeLimit   Enforces the time limit given with the -time option, even if \n\
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
dataType           Optionally specifies the data type for the output file (default datatype for image data is unsigned char).  Overrides\n\
                   the precision option.\n\
singleColumn       if provided, the image data will be written into one column, and the row and column numner will be saved in the parameter\n\
                   the output file can be directly plotted by sddscontour with \n\
                   'sddscontour -quantity=Waveform -swapxy <filename> -shade'\n\
x|yParameter       provide the required information for the parameters that are needed by sddscontour quantity plot\n";
static char *USAGE4 = "onCAerror          action taken when a channel access error occurs. Default is\n\
                   to use zeroes for values.\n\
scalars            specifies sddsmonitor input file to get names of scalar PVs\n\
                   from.  These will be logged in the output file as parameters.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
logOnChange        if given, the data is written only if one or more of the scalar pvs changes.\n\
device             device option can be areadector, FLID, ... etc. \n\
                   default is areadector, the first 4 data will saved in the array of output file.\n\
retakeStep         if provided, the step will be retaken if the timestamp or unique id or scalar pv values not changed.\n\
flushSteps         if provided, the data will stored in the memory until the step reaches flushSteps, default is 1, the data will \n\
                   be written to output file each step.\n\
Program by M. Borland, L. Emery, H. Shang, R. Soliday ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 1

#define AA_AVERAGE 0x001
#define AA_SUM 0x002
#define AA_NOQUERY 0x004

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

void FreeReadMemory(long variables, char **pvs, char **names, char **units, char **typeStrings,
                    double *data1, double *data2, double *data3, double *data4, int32_t *dataType);

double rmsValueFloat(float *y, long n);
double rmsValueLong(long *y, long n);
double rmsValueShort(short *y, long n);
double bandwidthLimitedRMS(double *fftIndepVariable, double *fftDepenQuantity, long rows, double minFreq, double maxFreq, short scaleByOneOverOmegaSquared);
int scalarPVchanged(long scalars, double *scalarData, double *previousScalarData, long *scalarDataType, char **strScalarData, char **prevStrScalarData);

#define devices 2
char *deviceList[devices] = {"areadector", "FLID"};

typedef struct {
  unsigned short **meta_data;
  double **image_data;
  long image_pvs, scalars;
  double *imageTime;
  uint32_t *imageID, **index; /*for each image pv, if there no singleColumn required, each image will have an Index column */
  double *scalarData;
  char **strScalarData;
} PAGE_DATA; /*data for each page */

int setupOutputFile(SDDS_DATASET *outTable, char *outputFile, short singleColumn, int32_t outputColumns, long DataType,
                    long scalars, char **scalarPV, long *scalarDatatype, char **scalarUnits, char **scalarSymbol,
                    char *xParName, char *yParName, long device, long comments, char **commentParameter);

int allocate_page_memory(PAGE_DATA *page_data, short singleColumn, long image_pvs, int32_t *dim0, int32_t *dim1, long device, long scalars, long *scalarDataType);
void free_page_memory(PAGE_DATA *page_data, long image_pvs, long scalars);
int writeOutputFile(SDDS_DATASET *outTable, short singleColumn, PAGE_DATA *page_data, long npages, long device,
                    int32_t *dim0, int32_t *dim1, long image_pvs, char **image_pv, long scalars, long *scalarDataType,
                    int32_t xParGiven, int32_t yParGiven, char *xParName, double xParInterval, double xParMin, double xParMax, int32_t xParDim,
                    char *yParName, double yParInterval, double yParMin, double yParMax, int32_t yParDim, long comments, char **commentText);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET outTable, inSet;
  char *inputfile, *outputfile, answer[ANSWER_LENGTH];
  long CAerrors, waveformLength, optPVnames;
  long enforceTimeLimit, noWarnings = 0;
  char **optPVname;
  char **readbackPV;
  chid *readbackCHID = NULL, *scalarCHID = NULL, *readbackDim0CHID = NULL, *readbackDim1CHID = NULL;
  chid *readbackTimeCHID = NULL, *readbackIDCHID = NULL;

  unsigned short *metaData = NULL;
  double *waveformData=NULL;
  double *readbackTime = NULL, *prevReadbackTime = NULL;
  uint32_t *readbackID = NULL, *prevReadbackID = NULL;
  char **scalarPV, **scalarName, **scalarUnits, **readMessage, **scalarSymbol = NULL;
  double *scalarData, *scalarFactor, *previousScalarData = NULL;
  char **strScalarData = NULL, **previousStrScalarData = NULL;
  long i, i_arg, outputColumns, readback, readbacks = 0, scalars = 0, *scalarDataType = NULL, strScalarExist = 0;
  char *TimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartDay, StartHour;
  double TotalTime, StartTime, StartJulianDay, StartYear, StartMonth, RunStartTime, RunTime;
  long Step, NStep, flushSteps = 1, verbose, singleShot;
  long stepsSet, totalTimeSet;
  long outputPreviouslyExists, erase;
  char *precision_string, *datatype_string, *scalarFile;
  long Precision, DataType, TimeUnits;
  long onCAerror;
  double timeToWait, expectedTime;
  unsigned long dummyFlags;
  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  chid *CondCHID = NULL;
  long conditions = 0;
  unsigned long CondMode = 0;
  double *CondDataBuffer = NULL;
  char *xParName = NULL, *yParName = NULL;
  int32_t xParDim = 0, yParDim = 0, xParGiven = 0, yParGiven = 0;
  double xParMin = 0, xParInterval = 1.0, xParMax = 100, yParMin = 0, yParInterval = 1.0, yParMax = 100.0;

  char **commentParameter, **commentText, *generationsDelimiter;
  long comments, generations;
  int32_t generationsDigits;
  long offsetTimeOfDay, TimeOfDayOffset;
  int32_t *dim0 = NULL, *dim1 = NULL;
  double pendIOtime = 10.0, timeout;
  long retries, singleColumn = 0, writePage, logOnChange = 0, retakeStep = 0, device = 0;

  PAGE_DATA *page_data = NULL;
  long npages, page;
  char image_pv[256];

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4);
    exit(1);
  }
 
  conditions = 0;
  CondDeviceName = CondReadMessage = NULL;
  CondScaleFactor = CondLowerLimit = CondUpperLimit = CondHoldoff = NULL;

  offsetTimeOfDay = TimeOfDayOffset = 0;
  inputfile = outputfile = scalarFile = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = verbose = erase = singleShot = stepsSet = generations = 0;
  onCAerror = ONCAERROR_USEZERO;
  Precision = PRECISION_SINGLE;
  DataType = SDDS_USHORT;
  CondFile = NULL;
  optPVname = NULL;
  optPVnames = 0;
  commentParameter = commentText = NULL;
  comments = 0;
  enforceTimeLimit = 0;
  CondDataBuffer = scalarData = NULL;
  precision_string = datatype_string = NULL;
  /*dataTypeColumnGiven = 0; */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax", NULL);
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
      case CLO_NOWARNINGS:
        noWarnings = 1;
        break;
      case CLO_STEPS:
        if (!(get_long(&NStep, s_arg[i_arg].list[1])) || NStep <= 0)
          bomb("no value or invalid value given for option -steps", NULL);
        stepsSet = 1;
        break;
      case CLO_FLUSHSTEPS:
        if (!(get_long(&flushSteps, s_arg[i_arg].list[1])) || flushSteps <= 0)
          bomb("no value or invalid value given for option -flushSteps", NULL);
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
            SDDS_Bomb("invalid -singleShot qualifier");
        }
        break;
      case CLO_ERASE:
        erase = 1;
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items < 2 ||
            !(precision_string = s_arg[i_arg].list[1]))
          bomb("no value given for option -precision", NULL);
        if ((Precision = identifyPrecision(precision_string)) < 0)
          bomb("invalid -precision value", NULL);
        break;
      case CLO_DATATYPE:
        if (s_arg[i_arg].n_items < 2 ||
            !(datatype_string = s_arg[i_arg].list[1]))
          bomb("no value given for option -dataType", NULL);
        if ((DataType = SDDS_IdentifyType(datatype_string)) < 0)
          bomb("invalid -dataType value", NULL);
	if (DataType==SDDS_CHARACTER || DataType==SDDS_STRING)
	  bomb("invalid -dataType: non-numeric type", NULL);
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
        if ((onCAerror = match_string(s_arg[i_arg].list[1], oncaerror_options, ONCAERROR_OPTIONS, 0)) < 0) {
          fprintf(stderr, "error: unrecognized oncaerror option given: %s\n", s_arg[i_arg].list[1]);
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
      case CLO_SCALARS:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of keywords for -scalars", NULL);
        scalarFile = s_arg[i_arg].list[1];
        break;
      case CLO_CONDITIONS:
        if (s_arg[i_arg].n_items < 3 ||
            SDDS_StringIsBlank(CondFile = s_arg[i_arg].list[1]) ||
            !(CondMode = IdentifyConditionMode(s_arg[i_arg].list + 2, s_arg[i_arg].n_items - 2)))
          SDDS_Bomb("invalid -conditions syntax/values");
        break;
      case CLO_PVNAMES:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -PVnames syntax");
        optPVnames = s_arg[i_arg].n_items - 1;
        optPVname = s_arg[i_arg].list + 1;
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
      case CLO_ENFORCETIMELIMIT:
        enforceTimeLimit = 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_XPARAMETER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "di ension", SDDS_LONG, &xParDim, 1, 0,
                          "name", SDDS_STRING, &xParName, 1, 0,
                          "interval", SDDS_DOUBLE, &xParInterval, 1, 0,
                          "minimum", SDDS_DOUBLE, &xParMin, 1, 0,
                          "maximum", SDDS_DOUBLE, &yParMax, 1, 0,
                          NULL) ||
            xParDim < 1)
          SDDS_Bomb("invalid -xParameter syntax/values (the dimension size should be greater than 1)");
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
          SDDS_Bomb("invalid -yParameter syntax/values (the dimension size should be greater than 1)");
        s_arg[i_arg].n_items += 1;
        if (!yParName)
          SDDS_CopyString(&yParName, "y");
        yParGiven = 1;
        break;
      case CLO_SINGLECOLUMN:
        singleColumn = 1;
        break;
      case CLO_LOGONCHANGE:
        logOnChange = 1;
        break;
      case CLO_DEVICE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -device argument provided!");
        device = match_string(s_arg[i_arg].list[1], deviceList, devices, 0);
        break;
      case CLO_RETAKESTEP:
        retakeStep = 1;
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
  if (xParName && yParName && strcmp(xParName, yParName) == 0)
    SDDS_Bomb("xparameter name and y parameter name can not be the same!");

  ca_task_initialize();

  if (!inputfile && optPVnames == 0)
    SDDS_Bomb("neither input filename nor -PVnames was given");
  if (optPVnames && outputfile)
    SDDS_Bomb("too many filenames given");
  if (optPVnames) {
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (!outputfile)
    bomb("output filename not given", NULL);
  if (erase && generations)
    SDDS_Bomb("-erase and -generations are incompatible options");
  if (generations)
    outputfile = MakeGenerationFilename(outputfile, generationsDigits, generationsDelimiter, NULL);

  outputPreviouslyExists = fexists(outputfile);
  if (outputPreviouslyExists && !erase) {
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

  readbackPV = NULL;

  if (inputfile) {
    if (!SDDS_InitializeInput(&inSet, inputfile))
      SDDS_Bomb("Error reading input monitor file!");

    if (SDDS_CheckColumn(&inSet, "WaveformPV", NULL, SDDS_STRING, stderr) != SDDS_CHECK_OKAY)
      SDDS_Bomb("Missing WaveformPV column in input file!");
    if (!(readbacks = SDDS_CountRowsOfInterest(&inSet)))
      SDDS_Bomb("Zero rows in input file");
    if (!(readbackPV = (char **)SDDS_GetColumn(&inSet, "WaveformPV")))
      SDDS_Bomb("Unable read Waveform PV in input file");
  } else {
    readbacks = optPVnames;
    readbackPV = optPVname;
  }
  waveformLength = 0;
  if (!(readbackCHID = malloc(sizeof(chid) * readbacks)) ||
      !(readbackDim0CHID = malloc(sizeof(chid) * readbacks)) ||
      !(readbackDim1CHID = malloc(sizeof(chid) * readbacks)) ||
      !(dim0 = malloc(sizeof(*dim1) * readbacks)) ||
      !(dim1 = malloc(sizeof(*dim1) * readbacks)) ||
      !(readbackTimeCHID = malloc(sizeof(*readbackTimeCHID) * readbacks)) ||
      !(readbackIDCHID = malloc(sizeof(*readbackIDCHID) * readbacks)) ||
      !(readbackTime = malloc(sizeof(*readbackTime) * readbacks)) ||
      !(readbackID = malloc(sizeof(*readbackID) * readbacks)) ||
      !(prevReadbackTime = malloc(sizeof(*prevReadbackTime) * readbacks)) ||
      !(prevReadbackID = malloc(sizeof(*prevReadbackID) * readbacks)))
    SDDS_Bomb("memory allocation failure");
  for (i = 0; i < readbacks; i++) {
    readbackCHID[i] = readbackDim0CHID[i] = readbackDim1CHID[i] = readbackTimeCHID[i] = readbackIDCHID[i] = NULL;
  }
  for (i = 0; i < readbacks; i++) {
    sprintf(image_pv, "%s:ArrayData", readbackPV[i]);

    if (ca_search(image_pv, &(readbackCHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", image_pv);
      exit(1);
    }
    sprintf(image_pv, "%s:ArraySize0_RBV", readbackPV[i]);
    if (ca_search(image_pv, &(readbackDim0CHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", image_pv);
      exit(1);
    }
    sprintf(image_pv, "%s:ArraySize1_RBV", readbackPV[i]);
    if (ca_search(image_pv, &(readbackDim1CHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", image_pv);
      exit(1);
    }
    sprintf(image_pv, "%s:UniqueId_RBV", readbackPV[i]);
    if (ca_search(image_pv, &(readbackIDCHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", image_pv);
      exit(1);
    }
    sprintf(image_pv, "%s:TimeStamp_RBV", readbackPV[i]);
    if (ca_search(image_pv, &(readbackTimeCHID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", image_pv);
      exit(1);
    }
  }

  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
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
      SDDS_Bomb("allocation faliure");
    if (!(CondCHID = (chid *)malloc(sizeof(*CondCHID) * conditions)))
      SDDS_Bomb("allocation faliure");
    for (i = 0; i < conditions; i++)
      CondCHID[i] = NULL;
  }

  /*check how many output columns */
  for (i = 0; i < readbacks; i++) {
    /*dimenion 0 is the number of rows of image data, and dimension 1 is the number of columns of image data */
    if (ca_get(DBR_LONG, readbackDim0CHID[i], &dim0[i]) != ECA_NORMAL)
      SDDS_Bomb("problem reading dimension0 size of image pv");
    if (ca_get(DBR_LONG, readbackDim1CHID[i], &dim1[i]) != ECA_NORMAL)
      SDDS_Bomb("problem reading dimension1 size of image pv");
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
  }

  outputColumns = 0;
  if (!singleColumn) {
    for (i = 0; i < readbacks; i++) {
      if (outputColumns < dim1[i])
        outputColumns = dim1[i];
    }
  }

  if (scalars) {
    scalarData = calloc(scalars, sizeof(*scalarData));

    if (!scalarUnits)
      scalarUnits = tmalloc(sizeof(*scalarUnits) * scalars);
    strScalarData = (char **)malloc(sizeof(*strScalarData) * scalars);
    if (logOnChange) {
      previousScalarData = calloc(scalars, sizeof(*previousScalarData));
      previousStrScalarData = (char **)malloc(sizeof(*previousStrScalarData) * scalars);
    }
    for (i = 0; i < scalars; i++) {
      strScalarData[i] = NULL;
      if (logOnChange)
        previousStrScalarData[i] = NULL;
      if (scalarDataType[i] == SDDS_STRING) {
        strScalarExist = 1;
        strScalarData[i] = (char *)malloc(sizeof(char) * 40);
        if (logOnChange) {
          previousStrScalarData[i] = (char *)malloc(sizeof(char) * 40);
        }
      }
    }
  }

  if ((scalars) && (!scalarUnits)) {
    getUnits(scalarPV, scalarUnits, scalars, scalarCHID, pendIOtime);
  }
  if (singleColumn) {
    if (!xParGiven) {
      SDDS_CopyString(&xParName, "HLine");
    }
    if (!yParGiven) {
      SDDS_CopyString(&yParName, "Index");
    }
  }
  /* start with a new file */
  getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth, &StartYear, &TimeStamp);
  if (!setupOutputFile(&outTable, outputfile, singleColumn, outputColumns, DataType, scalars, scalarPV, scalarDataType, scalarUnits, scalarSymbol,
                       xParName, yParName, device, comments, commentParameter)) {
    fprintf(stderr, "Error setting up output file.\n");
    exit(1);
  }

  RunStartTime = StartTime;

  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }
  /*found out how many data can be stored in the memory */
  if (flushSteps > NStep)
    flushSteps = NStep;
  if (!(page_data = malloc(sizeof(*page_data) * flushSteps)))
    SDDS_Bomb("Error allocate memory for page_data");
  npages = 0;

  for (i = 0; i < flushSteps; i++) {
    if (!allocate_page_memory(&(page_data[i]), singleColumn, readbacks, dim0, dim1, device, scalars, scalarDataType)) {
      break;
    }
    npages++;
  }
  if (npages == 0) {
    fprintf(stderr, "Error: no memory allocated for page_data\n");
    exit(1);
  }
  expectedTime = getTimeInSecs();
  page = 0;
  
 
  for (Step = 0; Step < NStep; Step++) {
    if (singleShot) {
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
      if (CondMode & (RETAKE_STEP || retakeStep))
        Step--;
      continue;
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
      if (logOnChange && Step == 0) {
        for (i = 0; i < scalars; i++)
          previousScalarData[i] = scalarData[i];
        if (strScalarExist) {
          for (i = 0; i < scalars; i++) {
            if (scalarDataType[i] == SDDS_STRING) {
              strcpy(previousStrScalarData[i], strScalarData[i]);
            }
          }
        }
      }
      if (logOnChange && Step) {
        /*check if scalar pvs value changed */
        if (!scalarPVchanged(scalars, scalarData, previousScalarData, scalarDataType, strScalarData, previousStrScalarData)) {
          if (retakeStep)
            Step--;
          continue;
        }
      }
    }

    ElapsedTime = EpochTime = getTimeInSecs();
    ElapsedTime -= StartTime;
    RunTime = getTimeInSecs() - RunStartTime;
    if (enforceTimeLimit && RunTime > TotalTime) {
      NStep = Step;
    }
    if (verbose) {
      printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
      fflush(stdout);
    }
    /*dim0 is the number of rows, dim1 is the number of columns */
   
    
    for (readback = 0; readback < readbacks; readback++) {
      if (ca_get(DBR_DOUBLE, readbackTimeCHID[readback], &readbackTime[readback]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading time stamp of %s\n", readbackPV[readback]);
        exit(1);
      }
      if (ca_get(DBR_LONG, readbackIDCHID[readback], &readbackID[readback]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading time stamp of %s\n", readbackPV[readback]);
        exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading timestamp/uniqueID\n");
      exit(1);
    }

    if (Step) {
      writePage = 0;
      for (readback = 0; readback < readbacks; readback++) {
        if (readbackTime[readback] != prevReadbackTime[readback] && readbackID[readback] != prevReadbackID[readback]) {
          writePage = 1;
          break;
        }
      }
      if (writePage) {
        for (readback = 0; readback < readbacks; readback++) {
          prevReadbackTime[readback] = readbackTime[readback];
          prevReadbackID[readback] = readbackID[readback];
        }
      }
    } else {
      writePage = 1;
      for (readback = 0; readback < readbacks; readback++) {
        prevReadbackTime[readback] = readbackTime[readback];
        prevReadbackID[readback] = readbackID[readback];
      }
    }

    if (!writePage) {
      if (retakeStep)
        Step--;
      continue;
    }
    
    for (readback = 0; readback < readbacks; readback++) {
      waveformLength = dim0[readback] * dim1[readback];
      waveformData = page_data[page].image_data[readback];
      ElapsedTime = EpochTime = getTimeInSecs();
      ElapsedTime -= StartTime;
      if (verbose) {
        printf("Step %ld. reading image waveform at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      if (ca_array_get(DBR_DOUBLE, waveformLength, readbackCHID[readback], waveformData) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", readbackPV[readback]);
        exit(1);
      }

      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for some channels\n");
        exit(1);
      }
      ElapsedTime = EpochTime = getTimeInSecs();
      ElapsedTime -= StartTime;
      if (verbose) {
        printf("Step %ld. reading image waveform done at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      page_data[page].imageTime[readback] = readbackTime[readback];
      page_data[page].imageID[readback] = readbackID[readback];
      if (device == 0) {
        for (i = 0; i < 4; i++) {
          page_data[page].meta_data[readback][i] = (unsigned short)waveformData[i];
          waveformData[i] = 64;
        }
      }
      
    } /*end of for readbacks */
    if (scalars) {
      memcpy(page_data[page].scalarData, scalarData, sizeof(double) * scalars);
      for (i = 0; i < scalars; i++) {
        if (scalarDataType[i] == SDDS_STRING) {
          strcpy(page_data[page].strScalarData[i], strScalarData[i]);
        }
      }
    }
    
    page++;
    if (page == npages) {
      ElapsedTime = EpochTime = getTimeInSecs();
      ElapsedTime -= StartTime;
      if (verbose) {
        printf("Step %ld. writing output file at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      /*write data to output file */
      writeOutputFile(&outTable, singleColumn, page_data, npages, device, 
                      dim0, dim1, readbacks, readbackPV, scalars, scalarDataType,
                      xParGiven, yParGiven, xParName, xParInterval, xParMin, xParMax, xParDim,
                      yParName, yParInterval, yParMin, yParMax, yParDim, comments, commentText);
      page = 0;
      ElapsedTime = EpochTime = getTimeInSecs();
      ElapsedTime -= StartTime;
      if (verbose) {
        printf("Step %ld. writing output file done at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
    }
    /*go to next step */
  }
  if (page > 0 && page < npages) {
    ElapsedTime = EpochTime = getTimeInSecs();
    ElapsedTime -= StartTime;
    if (verbose) {
      printf("Step %ld. writing output file at %f seconds.\n", Step, ElapsedTime);
      fflush(stdout);
    }
    /* need write the rest data into file */
    writeOutputFile(&outTable, singleColumn, page_data, page, device, 
                    dim0, dim1, readbacks, readbackPV, scalars, scalarDataType,
                    xParGiven, yParGiven, xParName, xParInterval, xParMin, xParMax, xParDim,
                    yParName, yParInterval, yParMin, yParMax, yParDim, comments, commentText);
  }
  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  /*free page_data memory, if page==0, the last page is being freed my SDDS_Terminate,
    if page>0, the page-1 is being freed by SDDS_Terminate */
  for (i = 0; i < npages; i++) {
    free_page_memory(&(page_data[i]), readbacks, scalars);
  }
  free(page_data);
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }
  FreeReadMemory(scalars, scalarPV, scalarName, scalarUnits, readMessage, scalarData,
                 scalarFactor, NULL, NULL, NULL);

  free(dim0);
  free(dim1);
  if (inputfile) {
    for (i = 0; i < readbacks; i++)
      free(readbackPV[i]);
    free(readbackPV);
  }
  FreeReadMemory(conditions, CondDeviceName, CondReadMessage, NULL, NULL, CondScaleFactor,
                 CondLowerLimit, CondUpperLimit, CondHoldoff, NULL);

  if (readbackCHID)
    free(readbackCHID);
  if (readbackDim0CHID)
    free(readbackDim0CHID);
  if (readbackDim1CHID)
    free(readbackDim1CHID);
  if (scalarCHID)
    free(scalarCHID);
  free(readbackTimeCHID);
  free(readbackIDCHID);
  free(readbackTime);
  free(readbackID);
  free(prevReadbackTime);
  free(prevReadbackID);
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
  if (metaData)
    free(metaData);
  if (CondCHID)
    free(CondCHID);
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return 0;
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

  real_imag = tmalloc(sizeof(double) * (2 * rows + 2));
  for (i = 0; i < rows; i++) {
    real_imag[2 * i] = data[i];
    real_imag[2 * i + 1] = 0;
  }
  complexFFT(real_imag, rows, 0);
  n_freq = rows / 2 + 1;

  df = 1.0 / length;

  fdata = tmalloc(sizeof(double) * n_freq);
  psd = tmalloc(sizeof(*psd) * n_freq);
  psdInteg = tmalloc(sizeof(*psdInteg) * n_freq);
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

int scalarPVchanged(long scalars, double *scalarData, double *previousScalarData, long *scalarDataType, char **strScalarData, char **previousStrScalarData) {
  long changed = 0;
  long i;

  for (i = 0; i < scalars; i++) {
    if (scalarDataType[i] == SDDS_STRING) {
      if (strcmp(strScalarData[i], previousStrScalarData[i]) != 0) {
        changed = 1;
        break;
      }
    } else {
      if (scalarData[i] != previousScalarData[i]) {
        changed = 1;
        break;
      }
    }
  }
  if (changed) {
    for (i = 0; i < scalars; i++) {
      if (scalarDataType[i] == SDDS_STRING) {
        strcpy(previousStrScalarData[i], strScalarData[i]);
      } else {
        previousScalarData[i] = scalarData[i];
      }
    }
  }
  return changed;
}

int setupOutputFile(SDDS_DATASET *outTable, char *outputFile, short singleColumn, int32_t outputColumns, long DataType,
                    long scalars, char **scalarPV, long *scalarDataType, char **scalarUnits, char **scalarSymbol,
                    char *xParName, char *yParName, long device, long comments, char **commentParameter) {
  long i;
  int32_t digits, pIndex = 0;
  char buffer[256], col_name[256];

  if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 1, "Area Dector", "Area Dector Devices", outputFile) ||
      !SDDS_DefineSimpleParameter(outTable, "Dimension0", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(outTable, "Dimension1", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(outTable, "ImagePV", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(outTable, "ImageTime", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(outTable, "ImageTimeStamp", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(outTable, "ImageUniqueID", NULL, SDDS_ULONG))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  outTable->layout.data_mode.column_major = 1;
  SDDS_EnableFSync(outTable);
  if (scalars) {
    for (i = 0; i < scalars; i++) {
      pIndex++;
      if (scalarSymbol) {
        if (!SDDS_DefineParameter(outTable, scalarPV[i], scalarSymbol[i], scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_DefineParameter(outTable, scalarPV[i], NULL, scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  if (DataType<0)
    DataType = SDDS_USHORT; /*default data type*/
  if (!singleColumn) {
    if (!!SDDS_DefineSimpleColumn(outTable, "Index", NULL, SDDS_LONG))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    digits = log10(outputColumns) + 1;
    sprintf(buffer, "HLine%%0%dd", digits);
    for (i = 0; i < outputColumns; i++) {
      sprintf(col_name, buffer, i);
      if (!SDDS_DefineSimpleColumn(outTable, col_name, NULL, DataType))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  } else {
    if (!SDDS_DefineSimpleColumn(outTable, "Waveform", NULL, DataType))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    /*note that the x and y are swapped for sddscontour */
  }
  if (xParName) {
    char buff[4][256];
    sprintf(buff[0], "%sDimension", xParName);
    sprintf(buff[1], "%sInterval", xParName);
    sprintf(buff[2], "%sMinimum", xParName);
    sprintf(buff[3], "%sMaximum", xParName);
    if (!SDDS_DefineSimpleParameter(outTable, "Variable1Name", NULL, SDDS_STRING) ||
        !SDDS_DefineSimpleParameter(outTable, buff[0], NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleParameter(outTable, buff[1], NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(outTable, buff[2], NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(outTable, buff[3], NULL, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (yParName) {
    char buff[4][256];
    sprintf(buff[0], "%sDimension", yParName);
    sprintf(buff[1], "%sInterval", yParName);
    sprintf(buff[2], "%sMinimum", yParName);
    sprintf(buff[3], "%sMaximum", yParName);
    if (!SDDS_DefineSimpleParameter(outTable, "Variable2Name", NULL, SDDS_STRING) ||
        !SDDS_DefineSimpleParameter(outTable, buff[0], NULL, SDDS_LONG) ||
        !SDDS_DefineSimpleParameter(outTable, buff[1], NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(outTable, buff[2], NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleParameter(outTable, buff[3], NULL, SDDS_DOUBLE))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (device == 0) {
    if (SDDS_DefineArray(outTable, "MetaData", NULL, NULL, NULL, NULL, SDDS_USHORT, 0, 1, "metaData") < 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_DefineSimpleParameters(outTable, comments, commentParameter, NULL, SDDS_STRING) ||
      !SDDS_WriteLayout(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return 1;
}

int allocate_page_memory(PAGE_DATA *page_data, short singleColumn, long image_pvs, int32_t *dim0, int32_t *dim1, long device, long scalars, long *scalarDataType) {
  long i, j;
  int32_t waveformLength;

  page_data->image_data=NULL;
  page_data->meta_data = NULL;
  page_data->imageTime = NULL;
  page_data->imageID = NULL;
  page_data->scalarData = NULL;
  page_data->strScalarData = NULL;
  page_data->index = NULL;

  if (!(page_data->image_data = malloc(sizeof(*page_data->image_data) * image_pvs))) {
    fprintf(stdout, "Error allocating memory for image data\n");
    return 0;
  }
  if (!(page_data->imageID = malloc(sizeof(*page_data->imageID) * image_pvs))) {
    fprintf(stdout, "Error allocating memory for imageID\n");
    free(page_data->image_data);
    return 0;
  }
  if (!(page_data->imageTime = malloc(sizeof(*page_data->imageTime) * image_pvs))) {
    fprintf(stdout, "Error allocating memory for imageTime\n");
    free(page_data->image_data);
    free(page_data->imageID);
    return 0;
  }
  for (i = 0; i < image_pvs; i++) {
    waveformLength = dim0[i] * dim1[i];
    page_data->image_data[i] = NULL;
    if (!(page_data->image_data[i] = malloc(sizeof(double) * waveformLength))) {
      fprintf(stdout, "Error allocating memory for image data2\n");
      for (j = 0; j < i; j++) {
        free(page_data->image_data[j]);
      }
      free(page_data->image_data);
      free(page_data->imageID);
      free(page_data->imageTime);
      return 0;
    }
  }
  if (device == 0) {
    if (!(page_data->meta_data = malloc(sizeof(*page_data->meta_data) * image_pvs))) {
      fprintf(stdout, "Error allocating memory for meta data1\n");
      free(page_data->imageID);
      free(page_data->imageTime);
      for (i = 0; i < image_pvs; i++)
        free(page_data->image_data[i]);
      free(page_data->image_data);
      return 0;
    }
    for (i = 0; i < image_pvs; i++) {
      page_data->meta_data[i] = NULL;
      /*first 4 of the image data are meta data */
      if (!(page_data->meta_data[i] = malloc(sizeof(unsigned short) * 4))) {
        fprintf(stdout, "Error allocating memory for meta data2\n");
        for (j = 0; j < i; j++)
          free(page_data->meta_data[j]);
        free(page_data->meta_data);
        free(page_data->imageID);
        free(page_data->imageTime);
        for (j = 0; j < image_pvs; j++)
          free(page_data->image_data[j]);
        free(page_data->image_data);
        return 0;
      }
    }
  }
  if (!singleColumn) {
    /*there is Index column for each image pv (one page data)*/
    if (!(page_data->index = malloc(sizeof(*page_data->index) * image_pvs))) {
      fprintf(stdout, "Error allocating memory for index1\n");
      for (i = 0; i < image_pvs; i++) {
        free(page_data->image_data[i]);
        if (device == 0)
          free(page_data->meta_data[i]);
      }
      free(page_data->imageID);
      free(page_data->imageTime);
      free(page_data->image_data);
      if (device == 0)
        free(page_data->meta_data);
      return 0;
    }
    for (i = 0; i < image_pvs; i++) {
      page_data->index[i] = NULL;
      if (!(page_data->index[i] = malloc(sizeof(int32_t) * dim0[i]))) {
        fprintf(stdout, "Error allocating memory for index1\n");
        for (j = 0; j < i; j++)
          free(page_data->index[j]);
        free(page_data->index);
        for (j = 0; j < image_pvs; j++) {
          free(page_data->image_data[j]);
          if (device == 0)
            free(page_data->meta_data[j]);
        }
        free(page_data->image_data);
        free(page_data->imageID);
        free(page_data->imageTime);
        if (device == 0)
          free(page_data->meta_data);
        return 0;
      } else {
        for (j = 0; j < dim0[i]; j++)
          page_data->index[i][j] = j;
      }
    }
  }

  if (scalars) {
    if (!(page_data->scalarData = malloc(sizeof(*page_data->scalarData) * scalars))) {
      fprintf(stdout, "Error allocating memory for scalar data\n");
      free(page_data->imageID);
      free(page_data->imageTime);
      for (i = 0; i < image_pvs; i++)
        free(page_data->image_data[i]);
      free(page_data->image_data);
      if (device == 0) {
        for (i = 0; i < image_pvs; i++)
          free(page_data->meta_data[i]);
        free(page_data->meta_data);
      }
      if (!singleColumn) {
        for (i = 0; i < image_pvs; i++)
          free(page_data->index[i]);
        free(page_data->index);
      }
      return 0;
    }
    if (!(page_data->strScalarData = malloc(sizeof(*page_data->strScalarData) * scalars))) {
      fprintf(stdout, "Error allocating memory for string scalar data1\n");
      free(page_data->scalarData);
      free(page_data->imageID);
      free(page_data->imageTime);
      for (i = 0; i < image_pvs; i++)
        free(page_data->image_data[i]);
      free(page_data->image_data);
      if (device == 0) {
        for (i = 0; i < image_pvs; i++)
          free(page_data->meta_data[i]);
        free(page_data->meta_data);
      }
      if (!singleColumn) {
        for (i = 0; i < image_pvs; i++)
          free(page_data->index[i]);
        free(page_data->index);
      }
      return 0;
    }
    for (i = 0; i < scalars; i++) {
      page_data->strScalarData[i] = NULL;
      if (scalarDataType[i] == SDDS_STRING && !(page_data->strScalarData[i] = malloc(sizeof(char) * 40))) {
        fprintf(stdout, "Error allocating memory for string scalar data2!\n");
        for (j = 0; j < i; j++)
          free(page_data->strScalarData[j]);
        free(page_data->strScalarData);
        free(page_data->scalarData);

        free(page_data->imageID);
        free(page_data->imageTime);
        for (i = 0; i < image_pvs; i++)
          free(page_data->image_data[i]);
        free(page_data->image_data);
        if (device == 0) {
          for (i = 0; i < image_pvs; i++)
            free(page_data->meta_data[i]);
          free(page_data->meta_data);
        }
        if (!singleColumn) {
          for (i = 0; i < image_pvs; i++)
            free(page_data->index[i]);
          free(page_data->index);
        }
        return 0;
      }
    }
  }
  return 1;
}

void free_page_memory(PAGE_DATA *page_data, long image_pvs, long scalars) {
  long i;
  if (image_pvs) {
    for (i = 0; i < image_pvs; i++) {
      if (page_data->image_data && page_data->image_data[i])
        free(page_data->image_data[i]);
      if (page_data->meta_data && page_data->meta_data[i])
        free(page_data->meta_data[i]);
      if (page_data->index && page_data->index[i])
        free(page_data->index[i]);
    }
    if (page_data->image_data)
      free(page_data->image_data);
    if (page_data->meta_data)
      free(page_data->meta_data);
    if (page_data->imageTime)
      free(page_data->imageTime);
    if (page_data->imageID)
      free(page_data->imageID);
    if (page_data->index)
      free(page_data->index);
    page_data->image_data = NULL;
    page_data->meta_data = NULL;
    page_data->imageTime = NULL;
    page_data->imageID = NULL;
    page_data->index = NULL;
  }
  if (scalars) {
    for (i = 0; i < scalars; i++) {
      if (page_data->strScalarData && page_data->strScalarData[i])
        free(page_data->strScalarData[i]);
    }
    if (page_data->strScalarData)
      free(page_data->strScalarData);
    if (page_data->scalarData)
      free(page_data->scalarData);
    page_data->scalarData = NULL;
    page_data->strScalarData = NULL;
  }
}

int writeOutputFile(SDDS_DATASET *outTable, short singleColumn, PAGE_DATA *page_data, long npages, long device, 
                    int32_t *dim0, int32_t *dim1, long image_pvs, char **image_pv, long scalars, long *scalarDataType,
                    int32_t xParGiven, int32_t yParGiven, char *xParName, double xParInterval, double xParMin, double xParMax, int32_t xParDim,
                    char *yParName, double yParInterval, double yParMin, double yParMax, int32_t yParDim, long comments, char **commentText) {
  long i, j, page;
  char *timeStamp = NULL;
  int32_t rows, par_index;

  for (page = 0; page < npages; page++) {
    /*write page*/
    /*each image pv is written into a page*/
    for (i = 0; i < image_pvs; i++) {
      if (singleColumn) {
        rows = dim0[i] * dim1[i];
      } else {
        rows = dim0[i];
      }
      if (!SDDS_StartPage(outTable, rows))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      /*first 6 parameters are defined and set */
      timeStamp = makeTimeStamp(page_data[page].imageTime[i]);
      if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              0, dim0[i],
                              1, dim1[i],
                              2, image_pv[i],
                              3, page_data[page].imageTime[i],
                              4, timeStamp,
                              5, page_data[page].imageID[i], -1)) {
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      par_index = 6;

      /*set the parameter in order */
      if (scalars) {
        for (j = 0; j < scalars; j++) {
          if (scalarDataType[j] == SDDS_STRING) {
            if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    par_index, page_data[page].strScalarData[j], -1))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          } else {
            if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    par_index, page_data[page].scalarData[j], -1))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          par_index++;
        }
      }
      if (singleColumn) {
        xParDim = dim1[i];
        yParDim = dim0[i];
        if (!xParGiven) {
          xParMin = 0;
          xParMax = dim1[i] - 1;
          xParInterval = 1;
        }
        if (!yParGiven) {
          yParMin = 0;
          yParMax = dim0[i - 1];
          yParInterval = 1;
        }
      }
      if (xParName) {
        if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                par_index, xParName,
                                par_index + 1, xParDim,
                                par_index + 2, xParInterval,
                                par_index + 3, xParMin,
                                par_index + 4, xParMax, -1))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        par_index = par_index + 5;
      }
      if (yParName) {
        if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                par_index, yParName,
                                par_index + 1, yParDim,
                                par_index + 2, yParInterval,
                                par_index + 3, yParMin,
                                par_index + 4, yParMax, -1))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        par_index = par_index + 5;
      }
      for (j = 0; j < comments; j++) {
        if (!SDDS_SetParameters(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                par_index, commentText[j], -1))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        par_index++;
      }
      if (device == 0) {
        /*set array data (only one meta_data array */
        if (!SDDS_SetArrayVararg(outTable, "MetaData", SDDS_POINTER_ARRAY, page_data[page].meta_data[i], 4))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      /*set colulmn*/
      if (singleColumn) {
        if (!SDDS_SetColumnFromDoubles(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, page_data->image_data[i], rows, 0))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      } else {
        if (!SDDS_SetColumn(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, page_data->index[i], rows, 0))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        for (j = 0; j < dim1[i]; j++) {
          if (!SDDS_SetColumnFromDoubles(outTable, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, &(page_data->image_data[i][j * rows]), rows, j + 1))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      }
      /*write page for image */

      if (!SDDS_WritePage(outTable)) {
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        return 0;
      }
    } /*end of image_pvs loop*/
  }   /*end of page loop */
  return 1;
}
