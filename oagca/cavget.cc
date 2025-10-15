/**
 * @file cavget.cc
 * @brief Retrieve values from EPICS process variables using Channel Access.
 *
 * cavget performs vector gets on one or more process variables specified on the
 * command line.  Various formatting and statistical options are provided for
 * scripting and data collection tasks.
 *
 * @section Usage
 * ```
 * cavget [-list=<string>[=<value>][,<string>[=<value>]...]]
 *        [-delist=<pattern>[,<pattern]...]
 *        [-range=begin=<integer>,end=<integer>[,format=<string>][,interval=<integer>]]
 *        [-floatformat=<printfString>] 
 *        [-charArray] 
 *        [-delimiter=<string>] 
 *        [-labeled] 
 *        [-noQuotes] 
 *        [-embrace=start=<string>,end=<string>] 
 *        [-cavputForm]
 *        [-statistics=number=<value>,pause=<value>[,format=[tagvalue][pretty][SDDS,file=<filename]]] 
 *        [-pendIoTime=<seconds>] [-dryRun]
 *        [-repeat=number=<integer>,pause=<seconds>[,average[,sigma]]]
 *        [-numerical] 
 *        [-errorValue=<string>] 
 *        [-excludeErrors]
 *        [-despike[[neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>]]]
 *        [-provider={ca|pva}]
 *        [-info]
 *        [-printErrors]
 * ```
 *
 * @section Options
 * | Option           | Description |
 * |------------------|-------------|
 * | `-list`          | Specify PV name components or explicit names. |
 * | `-delist`        | Give patterns that will be removed from lists.  |
 * | `-range`         | Provide a range of integer suffixes. |
 * | `-floatformat`   | Specify printf style format for floating point values. |
 * | `-charArray`     | Print character arrays as strings. |
 * | `-delimiter`     | Set string to separate values. |
 * | `-labeled`       | Prepend labels to values. |
 * | `-noQuotes`      | Do not enclose string values in quotes. |
 * | `-embrace`       | Surround output with specified strings. |
 * | `-cavputForm`    | Format output suitable for input to cavput. |
 * | `-statistics`    | Compute statistics over multiple readings. |
 * | `-pendIoTime`    | Maximum time to wait for connections. |
 * | `-dryRun`        | List PV names without fetching values. |
 * | `-repeat`        | Perform repeated readings. |
 * | `-numerical`     | Use numerical PV names instead of labels. |
 * | `-errorValue`    | Value to use when an error occurs. |
 * | `-excludeErrors` | Omit values when an error occurs. |
 * | `-despike`       | Remove spikes using optional parameters. |
 * | `-provider`      | Use Channel Access or PVAccess. |
 * | `-info`          | Display connection and PV information. |
 * | `-printErrors`   | Print error messages without halting execution. |
 *
 * @subsection Incompatibilities
 * - `-cavputForm` cannot be used with `-floatformat`, `-delimiter`, `-labeled`, `-noQuotes`, `-embrace`, or `-charArray`.
 * - `-excludeErrors` cannot be used with `-errorValue`.
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
 * M. Borland, R. Soliday
 */

#include <complex>
#include <iostream>
#include <cstdlib>
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
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"

#include "pvMultList.h"

#define CLO_LIST 0
#define CLO_RANGE 1
#define CLO_PENDIOTIME 2
#define CLO_DRYRUN 3
#define CLO_LABELED 4
#define CLO_FLOATFORMAT 5
#define CLO_DELIMITER 6
#define CLO_NOQUOTES 7
#define CLO_EMBRACE 8
#define CLO_REPEAT 9
#define CLO_ERRORVALUE 10
#define CLO_NUMERICAL 11
#define CLO_CAVPUTFORM 12
#define CLO_EXCLUDEERRORS 13
#define CLO_STATISTICS 14
#define CLO_DESPIKE 15
#define CLO_PRINTERRORS 16
#define CLO_PROVIDER 17
#define CLO_INFO 18
#define CLO_CHARARRAY 19
#define CLO_DELIST 20
#define COMMANDLINE_OPTIONS 21
char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"list", (char *)"range", (char *)"pendiotime", (char *)"dryrun", (char *)"labeled",
  (char *)"floatformat", (char *)"delimiter", (char *)"noquotes", (char *)"embrace", (char *)"repeat",
  (char *)"errorvalue", (char *)"numerical", (char *)"cavputform", (char *)"excludeerrors",
  (char *)"statistics", (char *)"despike", (char *)"printErrors", (char *)"provider", (char *)"info",
  (char *)"chararray", (char *)"delist"};

#define CLO_STATS_TAGVALUE 0
#define CLO_STATS_PRETTY 1
#define CLO_STATS_SDDS 2
#define STATISTICS_FORMATS 3
char *statisticsFormat[STATISTICS_FORMATS] = {
  (char *)"tagvalue",
  (char *)"pretty",
  (char *)"SDDS",
};

#define PROVIDER_CA 0
#define PROVIDER_PVA 1
#define PROVIDER_COUNT 2
char *providerOption[PROVIDER_COUNT] = {
  (char *)"ca",
  (char *)"pva",
};

static char *USAGE1 = (char *)"cavget [-list=<string>[=<value>][,<string>[=<value>]...]]\n\
[-delist=<pattern>[,<pattern>...]]\n\
[-range=begin=<integer>,end=<integer>[,format=<string>][,interval=<integer>]]\n\
[-floatformat=<printfString>] \n\
[-charArray] \n\
{ [-delimiter=<string>] [-labeled] [-noQuotes] [-embrace=start=<string>,end=<string>] \n\
    | [-cavputForm]  }\n\
[-statistics=number=<value>,pause=<value>[,format=[tagvalue][pretty][SDDS,file=<filename]]] \n\
[-pendIoTime=<seconds>] [-dryRun] [-repeat=number=<integer>,pause=<seconds>[,average[,sigma]]]\n\
[-numerical] [-errorValue=<string>] [-excludeErrors]\n\
[-despike[[neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>]] \n\
[-provider={ca|pva}]\n\
[-info]\n\
[-printErrors] \n\n\
-list        specifies PV name string components\n\
-delist      Exclude PVs that match any of the comma-separated glob patterns (supports * and ?).\n\
-range       specifies range of integers and format string\n\
-floatFormat specifies printf-style format string for printing\n\
             single- and double-precision values.  %g is the default.\n\
-charArray   Prints character array values as strings.\n\
-delimiter   specifies string to print between values.  The \n\
             default is a newline.\n\
-labeled     specifies that the PV name be echoed with the value.\n\
             A space separates the name and value.\n\
-noQuotes    Don't put quotes around string values that contain whitespace\n\
             or are blank.\n\
-embrace     Specifies putting braces around tag-value pairs.\n";
static char *USAGE2 = (char *)"             Typically used only with -label.\n\
-cavputForm  Specifies printout output in a format that can be given directly to\n\
             cavput for restoration of readback values.\n\
-pendIoTime  specifies maximum time to wait for connections and\n\
               return values.  Default is 1.0s \n\
-dryRun      lists PV names without fetching values.\n\
-errorValue  specifies a string to emit when there is a timeout or other\n\
             error for a PV.  The default is ?.\n\
-excludeErrors\n\
             specifies that when there is a timeout or other error for a PV,\n\
             there should be no output for that PV.\n\
-numerical   return a numerical value rather than a string for enumerated\n\
             types.\n\
-repeat      specifies repeated readings of the PVs at a specified time\n\
             interval.  If the average qualifier is given, then numerical\n\
             readbacks are averaged; for nonnumerical outputs, the last value\n\
             is given.\n\
-statistics  specifies statistical calculations of the PVs with reading times of \n\
             giving number at a specified time interval. \n\
             if format=tagvalue is given, the output format would be \n\
             \"mean <value> median <value> sigma <value> ....\" \n\
             if format=pretty, the statistical values are printed in columns format.\n\
             if format=SDDS,file=<filename> is given, the results will be written \n\
             to file. \n";
static char *USAGE3 = (char *)"-despike     despike must come along with repeat or statistics option which supplies\n\
             the number of readings. Despike specifies despiking of the number of \n\
             reading values given by repeat or statistics under the assumption\n\
             that consecutive readings are nearest neighbors. While, \n\
             <neighbors> is the number of neighbors for checking \n\
             despiking; <averageOf> is the number of consecutive readings around \n\
             the despiking for averaging and the averaged value (excluding the despike)\n\
             is going to replace this despiking data; <passes> is the number of times for\n\
             processing despiking; <threshold> is the despiking threshold, if the threshold\n\
             is zero or if the sum of absolute delta from the left most neighbor to \n\
             current reading is greater than <threshold>*<neighbors>, this reading value \n\
             is  going to be replace by the averaged value of <averageOf> consecutive\n\
             readings around this reading and excluding this reading, i.e., if threshold\n\
             is zero, despiking works similar to averaging. The value of <neighbors> and \n\
             <averageOf> must be smaller than the number of readings. \n\
-provider    defaults to CA (channel access) calls.\n\
-info        print meta data for PVs.\n\
-printErrors  print error message to standard error if channel not found.\n\n\
Program by Michael Borland and Robert Soliday, ANL/APS (" EPICS_VERSION_STRING ", " __DATE__ ")\n";

void sleep_us(unsigned int nusecs) {
  struct timeval tval;

  tval.tv_sec = nusecs / 1000000;
  tval.tv_usec = nusecs % 1000000;
  select(0, NULL, NULL, NULL, &tval);
}

#define REPEAT_AVERAGE 0x0001U
#define REPEAT_SIGMA 0x0002U

long printResult(FILE *fp, char *startBrace, long labeled, long connectedInTime,
                 long channelType, PV_VALUE PVvalue, char *endBrace,
                 char *floatFormat, short charArray, char *errorValue, long quotes,
                 long cavputForm, long excludeErrors,
                 long delimNeeded, char *delimiter, long secondValue, long doRepeats,
                 long doStats, long repeats, double *value, long average, short printErrors);
void printResultPretty(FILE *fp, long *channelType, PV_VALUE *PVvalue, long PVs, long quotes,
                       short *connectedInTime, char *errorValue, short printErrors);
long writeStatisticsToSDDSFile(char *outputFile, long *channelType, PV_VALUE *PVvalue, long PVs,
                               short *connectedInTime, char *errorValue, short printErrors);
void oag_ca_exception_handler(struct exception_handler_args args);

#if (EPICS_VERSION > 3)

void printResultPVA(char *startBrace, long labeled, PVA_OVERALL *pva, PV_VALUE *PVvalue, char *endBrace,
                    char *floatFormat, short charArray, char *errorValue, long quotes, long cavputForm,
                    long excludeErrors, char *delimiter, long secondValue,
                    long doRepeat, long doStats, long repeats, long average, short printErrors);
void printResultPrettyPVA(PVA_OVERALL *pva, long quotes, char *errorValue, short printErrors);
void writeStatisticsToSDDSFilePVA(char *outputFile, PVA_OVERALL *pva, long quotes, char *errorValue, short printErrors);
void GetInfoData(PVA_OVERALL *pva, PV_VALUE *PVvalue);
#endif

int main(int argc, char **argv) {
  int32_t beginRange, endRange, rangeInterval;
  long dryRun;
  long PVs, j, i_arg, labeled, quotes, numerical;
  PV_VALUE *PVvalue;
  TERM_LIST *List;
  unsigned long flags;
  SCANNED_ARG *s_arg;
  char *rangeFormat, *floatFormat, *delimiter;
  short charArray=0;
  char **excludePatterns = NULL;
  long excludeCount = 0;
  double pendIOTime, repeatPause, despikeThreshold;
  chid *channelID;
  short *connectedInTime = NULL;
  char *startBrace, *endBrace;
  long average, sigma, repeats0, cavputForm, excludeErrors, doStats, doRepeat, doDespike;
  int32_t repeats, despikeNeighbors, despikePasses, despikeAverageOf;
  char *errorValue = (char *)"?";
  char *statsFormat, *statsOutput;
  double **value = NULL;
  long *channelType = NULL;
  short printErrors = 0;
  long providerMode = 0;
  long infoMode = 0;
#if (EPICS_VERSION > 3)
  PVA_OVERALL pva;
#else
  long delimNeeded;
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    exit(EXIT_FAILURE);
  }
  statsFormat = statsOutput = NULL;
  PVvalue = NULL;
  List = NULL;
  PVs = dryRun = labeled = numerical = 0;
  floatFormat = (char *)"%g";
  delimiter = (char *)"\n";
  quotes = 1;
  pendIOTime = 1.0;
  startBrace = endBrace = NULL;
  repeats = average = sigma = 0;
  doStats = doRepeat = doDespike = 0;
  repeatPause = 1;
  cavputForm = excludeErrors = 0;
  channelID = NULL;
  despikeNeighbors = 10;
  despikePasses = 2;
  despikeAverageOf = 4;
  despikeThreshold = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_LIST:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -list syntax (cavget)");
        List = (TERM_LIST *)trealloc(List, sizeof(*List) * (s_arg[i_arg].n_items - 1));
        for (j = 1; j < s_arg[i_arg].n_items; j++) {
          List[j - 1].flags = 0;
          List[j - 1].string = s_arg[i_arg].list[j];
          List[j - 1].value = NULL;
        }
        multiplyWithList(&PVvalue, &PVs, List, s_arg[i_arg].n_items - 1);
        break;
      case CLO_RANGE:
        s_arg[i_arg].n_items--;
        rangeFormat = NULL;
        rangeInterval = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "begin", SDDS_LONG, &beginRange, 1, BEGIN_GIVEN,
                          "end", SDDS_LONG, &endRange, 1, END_GIVEN,
                          "interval", SDDS_LONG, &rangeInterval, 1, INTERVAL_GIVEN,
                          "format", SDDS_STRING, &rangeFormat, 1, FORMAT_GIVEN,
                          NULL) ||
            !(flags & BEGIN_GIVEN) || !(flags & END_GIVEN) || beginRange > endRange ||
            (flags & FORMAT_GIVEN && SDDS_StringIsBlank(rangeFormat)))
          SDDS_Bomb((char *)"invalid -range syntax/values");
        if (!rangeFormat)
          rangeFormat = (char *)"%ld";
        multiplyWithRange(&PVvalue, &PVs, beginRange, endRange, rangeInterval, rangeFormat);
        s_arg[i_arg].n_items++;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"wrong number of items for -pendIoTime");
        if (sscanf(s_arg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          SDDS_Bomb((char *)"invalid -pendIoTime value (cavget)");
        break;
      case CLO_DRYRUN:
        dryRun = 1;
        break;
      case CLO_LABELED:
        labeled = 1;
        break;
      case CLO_PRINTERRORS:
        printErrors = 1;
        break;
      case CLO_INFO:
        infoMode = 1;
        break;
      case CLO_CHARARRAY:
        charArray = 1;
        break;
      case CLO_DELIST: {
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -delist syntax (cavget)");
        long add = s_arg[i_arg].n_items - 1;
        excludePatterns = (char **)trealloc(excludePatterns, sizeof(*excludePatterns) * (excludeCount + add));
        for (j = 1; j < s_arg[i_arg].n_items; j++) {
          excludePatterns[excludeCount++] = s_arg[i_arg].list[j];
        }
        break;
      }
      case CLO_FLOATFORMAT:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"wrong number of items for -floatFormat");
        floatFormat = s_arg[i_arg].list[1];
        break;
      case CLO_DELIMITER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"wrong number of items for -delimiter");
        delimiter = s_arg[i_arg].list[1];
        break;
      case CLO_NOQUOTES:
        quotes = 0;
        break;
      case CLO_EMBRACE:
        s_arg[i_arg].n_items--;
        startBrace = (char *)"{";
        endBrace = (char *)"}";
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "start", SDDS_STRING, &startBrace, 1, 0,
                          "end", SDDS_STRING, &endBrace, 1, 0,
                          NULL))
          SDDS_Bomb((char *)"invalid -embrace syntax");
        s_arg[i_arg].n_items++;
        break;
      case CLO_DESPIKE:
        s_arg[i_arg].n_items--;
        doDespike = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "neighbors", SDDS_LONG, &despikeNeighbors, 1, 0,
                          "passes", SDDS_LONG, &despikePasses, 1, 0,
                          "averageOf", SDDS_LONG, &despikeAverageOf, 1, 0,
                          "threshold", SDDS_DOUBLE, &despikeThreshold, 1, 0,
                          NULL) ||
            despikeNeighbors < 0 ||
            despikePasses < 0 || despikeAverageOf < 0 || despikeThreshold < 0)
          SDDS_Bomb((char *)"invalid -despike syntax");
        s_arg[i_arg].n_items++;
        break;
      case CLO_REPEAT:
        s_arg[i_arg].n_items--;
        doRepeat = 1;
        repeats = average = 0;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "number", SDDS_LONG, &repeats, 1, 0,
                          "pause", SDDS_DOUBLE, &repeatPause, 1, 0,
                          "average", -1, NULL, 0, REPEAT_AVERAGE,
                          "sigma", -1, NULL, 0, REPEAT_SIGMA,
                          NULL) ||
            repeats < 1)
          SDDS_Bomb((char *)"invalid -repeat syntax");
        if (flags & REPEAT_AVERAGE)
          average = 1;
        if (flags & REPEAT_SIGMA)
          sigma = 1;
        s_arg[i_arg].n_items++;
        break;
      case CLO_STATISTICS:
        s_arg[i_arg].n_items--;
        doStats = 1;
        repeats = 0;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "number", SDDS_LONG, &repeats, 1, 0,
                          "pause", SDDS_DOUBLE, &repeatPause, 1, 0,
                          "format", SDDS_STRING, &statsFormat, 1, 0,
                          "file", SDDS_STRING, &statsOutput, 1, 0,
                          NULL) ||
            repeats < 1)
          SDDS_Bomb((char *)"invalid -statistics syntax");
        if (!statsFormat)
          SDDS_Bomb((char *)"invalid -statistics syntax, the format is not given.");
        switch (match_string(statsFormat, statisticsFormat, STATISTICS_FORMATS, 0)) {
        case CLO_STATS_TAGVALUE:
        case CLO_STATS_PRETTY:
          if (statsOutput)
            SDDS_Bomb((char *)"invalid -statics format syntax");
          break;
        case CLO_STATS_SDDS:
          if (!statsOutput)
            SDDS_Bomb((char *)"invalid -statics format=SDDS syntax, output filename is not given");
          break;
        default:
          SDDS_Bomb((char *)"unknown format given to -statistics option");
          break;
        }
        s_arg[i_arg].n_items++;
        break;
      case CLO_ERRORVALUE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"invalid -errorValue syntax");
        errorValue = s_arg[i_arg].list[1];
        break;
      case CLO_NUMERICAL:
        numerical = 1;
        break;
      case CLO_CAVPUTFORM:
        cavputForm = 1;
        break;
      case CLO_EXCLUDEERRORS:
        excludeErrors = 1;
        break;
      case CLO_PROVIDER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"no value given for option -provider");
        if ((providerMode = match_string(s_arg[i_arg].list[1], providerOption,
                                         PROVIDER_COUNT, 0)) < 0)
          SDDS_Bomb((char *)"invalid -provider");
        break;
      default:
        SDDS_Bomb((char *)"unknown option (cavget)");
        break;
      }
    } else
      SDDS_Bomb((char *)"unknown option");
  }

  /* Apply exclusion filters before any data retrieval/printing */
  if (excludeCount > 0 && PVs > 0) {
    long kept = 0;
    PVvalue = excludePVsFromLists(PVvalue, PVs, &kept, excludePatterns, excludeCount);
    PVs = kept;
  }

  if (doRepeat && doStats)
    SDDS_Bomb((char *)"-repeat and -statistics are incompatible.");

  /*if (cavputForm && doStats)
    SDDS_Bomb((char*)"-cavputForm and -statistics are incompatible."); */
  if (doDespike) {
    if (!doRepeat && !doStats)
      SDDS_Bomb((char *)"neither repeat or statistics is given, can not do despiking!");
    if (doRepeat) {
      sigma = 0;
      average = 1;
    }
    if (despikeNeighbors > repeats)
      despikeNeighbors = repeats;
    if (despikeAverageOf > repeats)
      despikeAverageOf = repeats;
  }
  if (cavputForm)
    delimiter = (char *)",";
  if (dryRun) {
    for (j = 0; j < PVs; j++) {
      if (strncmp(PVvalue[j].name, "pva://", 6) == 0) {
        PVvalue[j].name += 6;
        printf("%32s\n", PVvalue[j].name);
        PVvalue[j].name -= 6;
      } else if (strncmp(PVvalue[j].name, "ca://", 5) == 0) {
        PVvalue[j].name += 5;
        printf("%32s\n", PVvalue[j].name);
        PVvalue[j].name -= 5;
      } else {
        printf("%32s\n", PVvalue[j].name);
      }
    }
  } else {
#if (EPICS_VERSION > 3)
    //Allocate memory for pva structure
    allocPVA(&pva, PVs, repeats);
    //List PV names
    epics::pvData::shared_vector<std::string> names(pva.numPVs);
    epics::pvData::shared_vector<std::string> provider(pva.numPVs);
    for (j = 0; j < pva.numPVs; j++) {
      if (strncmp(PVvalue[j].name, "pva://", 6) == 0) {
        PVvalue[j].name += 6;
        names[j] = PVvalue[j].name;
        PVvalue[j].name -= 6;
        provider[j] = "pva";
      } else if (strncmp(PVvalue[j].name, "ca://", 5) == 0) {
        PVvalue[j].name += 5;
        names[j] = PVvalue[j].name;
        PVvalue[j].name -= 5;
        provider[j] = "ca";
      } else {
        names[j] = PVvalue[j].name;
        if (providerMode == PROVIDER_PVA)
          provider[j] = "pva";
        else
          provider[j] = "ca";
      }
    }
    pva.pvaChannelNames = freeze(names);
    pva.pvaProvider = freeze(provider);
    //Connect to PVs
    ConnectPVA(&pva, pendIOTime);



    repeats0 = 0;

    do {
      //Get data from PVs and return a PVStructure
      if (infoMode) {
        pva.includeAlarmSeverity = true;
      }
      if (GetPVAValues(&pva) == 1) {
        return (EXIT_FAILURE);
      }
      if (infoMode) {
        GetInfoData(&pva, PVvalue);
        exit(EXIT_SUCCESS);
      }
      //Change enumerated PV values to int from string if the numerical option is given
      if ((repeats0 == 0) && (numerical)) {
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j] && pva.pvaData[j].pvEnumeratedStructure) {
            pva.pvaData[j].scalarType = epics::pvData::pvInt;
          }
        }
      }

      if (!repeats) {
        for (j = 0; j < PVs; j++) {
          if ((pva.pvaData[j].numGetElements == 1) && (pva.pvaData[j].numeric)) {
            pva.pvaData[j].mean = pva.pvaData[j].getData[0].values[0];
          }
        }
      }
      repeats0++;
      if ((repeats0 < repeats) && (repeatPause > 0)) {
        usleepSystemIndependent(repeatPause * 1000000);
      }
    } while (repeats0 < repeats);

    if (repeats) {
      for (j = 0; j < PVs; j++) {
        if ((pva.pvaData[j].numGetElements == 1) && (pva.pvaData[j].numeric)) {
          double *v;
          long i;
          v = (double *)malloc(sizeof(double) * repeats);
          for (i = 0; i < repeats; i++) {
            v[i] = pva.pvaData[j].getData[i].values[0];
          }
          if (doDespike) {
            despikeData(v, repeats, despikeNeighbors, despikePasses, despikeAverageOf, despikeThreshold, 0);
          }
          find_min_max(&pva.pvaData[j].min, &pva.pvaData[j].max, v, repeats);
          computeMoments(&pva.pvaData[j].mean, &pva.pvaData[j].rms, &pva.pvaData[j].stDev, &pva.pvaData[j].MAD, v, repeats);
          pva.pvaData[j].spread = pva.pvaData[j].max - pva.pvaData[j].min;
          pva.pvaData[j].sigma = pva.pvaData[j].stDev / sqrt(repeats);
          find_median(&pva.pvaData[j].median, v, repeats);
          free(v);
        }
      }
    }

    if (!statsFormat ||
        match_string(statsFormat, statisticsFormat, STATISTICS_FORMATS, 0) == CLO_STATS_TAGVALUE) {
      printResultPVA(startBrace, labeled, &pva, PVvalue, endBrace,
                     floatFormat, charArray, errorValue, quotes, cavputForm,
                     excludeErrors, delimiter, sigma,
                     doRepeat, doStats, repeats, average, printErrors);
    }
    if (statsFormat) {
      switch (match_string(statsFormat, statisticsFormat, STATISTICS_FORMATS, 0)) {
      case CLO_STATS_PRETTY:
        printResultPrettyPVA(&pva, quotes, errorValue, printErrors);
        break;
      case CLO_STATS_SDDS:
        writeStatisticsToSDDSFilePVA(statsOutput, &pva, quotes, errorValue, printErrors);
        break;
      default:
        break;
      }
    }
    if (PVvalue) {
      for (j = 0; j < PVs; j++) {
        if (PVvalue[j].name)
          free(PVvalue[j].name);
      }
      free(PVvalue);
    }
    freePVA(&pva);
    if (statsFormat)
      free(statsFormat);
    if (statsOutput)
      free(statsOutput);
    if (List)
      free(List);
    free_scanargs(&s_arg, argc);
    return (EXIT_SUCCESS);
#else
    if (!(value = (double **)malloc(sizeof(double *) * PVs)))
      SDDS_Bomb((char *)"memory allocation failure (cavget)");
    for (j = 0; j < PVs; j++) {
      if (repeats) {
        if (!(value[j] = (double *)calloc((repeats), sizeof(double))))
          SDDS_Bomb((char *)"memory allocation failure (cavget)");
      } else {
        if (!(value[j] = (double *)calloc(1, sizeof(double))))
          SDDS_Bomb((char *)"memory allocation failure (cavget)");
      }
    }
    repeats0 = 0;

    ca_task_initialize();
    ca_add_exception_event(oag_ca_exception_handler, NULL);
    if (!(channelID = (chid *)malloc(sizeof(*channelID) * PVs)) ||
        !(channelType = (long *)malloc(sizeof(*channelType) * PVs)) ||
        !(connectedInTime = (short *)malloc(sizeof(*connectedInTime) * PVs)))
      SDDS_Bomb((char *)"memory allocation failure (cavget)");
    for (j = 0; j < PVs; j++)
      if (ca_search(PVvalue[j].name, channelID + j) != ECA_NORMAL) {
        fprintf(stderr, "error (cavget): problem doing search for %s\n",
                PVvalue[j].name);
        ca_task_exit();
        exit(EXIT_FAILURE);
      }
    ca_pend_io(pendIOTime);
    if (numerical)
      for (j = 0; j < PVs; j++) {
        channelType[j] = ca_field_type(channelID[j]);
        if ((channelType[j] = ca_field_type(channelID[j])) == DBF_ENUM)
          channelType[j] = DBF_DOUBLE;
      }
    else
      for (j = 0; j < PVs; j++)
        channelType[j] = ca_field_type(channelID[j]);
    do {
      for (j = 0; j < PVs; j++) {
        if (!(connectedInTime[j] = ca_state(channelID[j]) == cs_conn))
          continue;
        switch (channelType[j]) {
        case DBF_STRING:
        case DBF_ENUM:
          PVvalue[j].value = (char *)malloc(sizeof(char) * 256);
          if (ca_get(DBR_STRING, channelID[j], PVvalue[j].value) != ECA_NORMAL) {
            fprintf(stderr, "problem doing get for %s\n",
                    PVvalue[j].name);
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
          break;
        case DBF_NO_ACCESS:
          fprintf(stderr, "Error: No access to PV %s\n", PVvalue[j].name);
          ca_task_exit();
          exit(EXIT_FAILURE);
          break;
        default:
          if (ca_get(DBR_DOUBLE, channelID[j],
                     &value[j][repeats0]) != ECA_NORMAL) {
            fprintf(stderr, "problem doing get for %s\n",
                    PVvalue[j].name);
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
          break;
        }
      }
      ca_pend_io(pendIOTime);
      repeats0++;
      if (!repeats) {
        for (j = 0; j < PVs; j++)
          PVvalue[j].mean = value[j][0];
      }
      if ((repeats0 < repeats) && (repeatPause > 0)) {
        ca_pend_event(repeatPause);
      }
    } while (repeats0 < repeats);

    if (repeats) {
      for (j = 0; j < PVs; j++) {
        if (doDespike) {
          despikeData(value[j], repeats, despikeNeighbors, despikePasses, despikeAverageOf,
                      despikeThreshold, 0);
        }
        find_min_max(&PVvalue[j].min, &PVvalue[j].max, value[j], repeats);
        computeMoments(&PVvalue[j].mean, &PVvalue[j].rms, &PVvalue[j].stDev, &PVvalue[j].MAD,
                       value[j], repeats);
        PVvalue[j].spread = PVvalue[j].max - PVvalue[j].min;
        PVvalue[j].sigma = PVvalue[j].stDev / sqrt(repeats);
        find_median(&PVvalue[j].median, value[j], repeats);
      }
    }

    delimNeeded = 0;
    if (!statsFormat ||
        match_string(statsFormat, statisticsFormat, STATISTICS_FORMATS, 0) == CLO_STATS_TAGVALUE) {
      for (j = 0; j < PVs; j++) {
        if (printResult(stdout, startBrace, labeled, connectedInTime[j],
                        channelType[j], PVvalue[j], endBrace,
                        floatFormat, charArray, errorValue, quotes, cavputForm,
                        excludeErrors, delimNeeded, delimiter, sigma, doRepeat, doStats, repeats, value[j], average,
                        printErrors))
          delimNeeded = 1;
      }
      if (delimNeeded)
        fputc('\n', stdout);
    }
    if (statsFormat) {
      switch (match_string(statsFormat, statisticsFormat, STATISTICS_FORMATS, 0)) {
      case CLO_STATS_PRETTY:
        printResultPretty(stdout, channelType, PVvalue, PVs, quotes, connectedInTime, errorValue, printErrors);
        break;
      case CLO_STATS_SDDS:
        writeStatisticsToSDDSFile(statsOutput, channelType, PVvalue, PVs, connectedInTime, errorValue, printErrors);
        break;
      default:
        break;
      }
    }
    ca_task_exit();
#endif
  }
  for (j = 0; j < PVs; j++) {
    if (PVvalue[j].name)
      free(PVvalue[j].name);
    if (PVvalue[j].value)
      free(PVvalue[j].value);
    PVvalue[j].name = PVvalue[j].value = NULL;
  }
  if (value) {
    for (j = 0; j < PVs; j++)
      free(value[j]);
    free(value);
  }
  if (PVvalue)
    free(PVvalue);
  if (channelID)
    free(channelID);
  if (channelType)
    free(channelType);
  if (connectedInTime)
    free(connectedInTime);
  free_scanargs(&s_arg, argc);
  if (List)
    free(List);

  return EXIT_SUCCESS;
}

long printResult(FILE *fp, char *startBrace, long labeled, long connectedInTime,
                 long channelType, PV_VALUE PVvalue, char *endBrace,
                 char *floatFormat, short charArray, char *errorValue, long quotes,
                 long cavputForm, long excludeErrors,
                 long delimNeeded, char *delimiter, long secondValue,
                 long doRepeat, long doStats, long repeats, double *value, long average, short printErrors) {
  long i;

  if (excludeErrors && !connectedInTime)
    return 0;
  if (delimNeeded && delimiter)
    fputs(delimiter, fp);
  if (cavputForm) {
    fprintf(fp, "%s=", PVvalue.name);
    quotes = 0;
  } else {
    if (startBrace)
      fputs(startBrace, fp);
    if (labeled)
      fprintf(fp, "%s ", PVvalue.name);
  }
  if (!connectedInTime) {
    if (printErrors)
      fprintf(stderr, "%s is not connected or invalid PV name.\n", PVvalue.name);
    else
      fputs(errorValue, fp);
  } else {
    switch (channelType) {
    case DBF_SHORT:
    case DBF_LONG:
    case DBF_CHAR:
      if (cavputForm || (!doRepeat && !doStats)) {
        fprintf(fp, "%.0f", PVvalue.mean);
      } else {
        if (doRepeat) {
          if (!average) {
            for (i = 0; i < repeats; i++) {
              fprintf(fp, "%.0f ", value[i]);
            }
          } else {
            fprintf(fp, "%.0f", PVvalue.mean);
            if (secondValue) {
              fprintf(fp, " ");
              fprintf(fp, "%.0f", PVvalue.sigma);
            }
          }
        }
        if (doStats) {
          fprintf(fp, "mean ");
          fprintf(fp, "%.0f", PVvalue.mean);
          fprintf(fp, " median ");
          fprintf(fp, "%.0f", PVvalue.median);
          fprintf(fp, " sigma ");
          fprintf(fp, "%.0f", PVvalue.sigma);
          fprintf(fp, " stDev ");
          fprintf(fp, "%.0f", PVvalue.stDev);
          fprintf(fp, " MAD ");
          fprintf(fp, "%.0f", PVvalue.MAD);
          fprintf(fp, " min ");
          fprintf(fp, "%.0f", PVvalue.min);
          fprintf(fp, " max ");
          fprintf(fp, "%.0f", PVvalue.max);
          fprintf(fp, " spread ");
          fprintf(fp, "%.0f", PVvalue.spread);
        }
      }
      break;
    case DBF_STRING:
    case DBF_ENUM:
      if (!quotes)
        fputs(PVvalue.value, fp);
      else {
        if (SDDS_StringIsBlank(PVvalue.value) ||
            SDDS_HasWhitespace(PVvalue.value))
          fprintf(fp, "\"%s\"", PVvalue.value);
        else
          fputs(PVvalue.value, fp);
      }
      break;
    case DBF_DOUBLE:
    case DBF_FLOAT:
    default:
      if (cavputForm || (!doRepeat && !doStats)) {
        fprintf(fp, floatFormat, PVvalue.mean);
      } else {
        if (doRepeat) {
          if (!average) {
            for (i = 0; i < repeats; i++) {
              fprintf(fp, floatFormat, value[i]);
              fprintf(fp, " ");
            }
          } else {
            fprintf(fp, floatFormat, PVvalue.mean);
            if (secondValue) {
              fprintf(fp, " ");
              fprintf(fp, floatFormat, PVvalue.sigma);
            }
          }
        }
        if (doStats) {
          fprintf(fp, " mean ");
          fprintf(fp, floatFormat, PVvalue.mean);
          fprintf(fp, " median ");
          fprintf(fp, floatFormat, PVvalue.median);
          fprintf(fp, " sigma ");
          fprintf(fp, floatFormat, PVvalue.sigma);
          fprintf(fp, " stDev ");
          fprintf(fp, floatFormat, PVvalue.stDev);
          fprintf(fp, " MAD ");
          fprintf(fp, floatFormat, PVvalue.MAD);
          fprintf(fp, " min ");
          fprintf(fp, floatFormat, PVvalue.min);
          fprintf(fp, " max ");
          fprintf(fp, floatFormat, PVvalue.max);
          fprintf(fp, " spread ");
          fprintf(fp, floatFormat, PVvalue.spread);
        }
      }
      break;
    }
  }
  if (endBrace && !cavputForm)
    fputs(endBrace, fp);
  return 1;
}

void printResultPretty(FILE *fp, long *channelType, PV_VALUE *PVvalue, long PVs, long quotes,
                       short *connectedInTime, char *errorValue, short printErrors) {
  long i;

  fprintf(fp, "%20s", "ControlName");
  fprintf(fp, "%10s", "mean");
  fprintf(fp, "%10s", "median");
  fprintf(fp, "%10s", "sigma");
  fprintf(fp, "%10s", "stDev");
  fprintf(fp, "%10s", "MAD");
  fprintf(fp, "%10s", "min");
  fprintf(fp, "%10s", "max");
  fprintf(fp, "%10s", "spread");
  fputc('\n', fp);
  for (i = 0; i < 100; i++)
    fputc('-', fp);
  fputc('\n', fp);

  for (i = 0; i < PVs; i++) {
    fprintf(fp, "%20s", PVvalue[i].name);
    if (!connectedInTime[i]) {
      if (printErrors)
        fprintf(stderr, "%s is not connected or invalid PV name.\n", PVvalue[i].name);
      else {
        fprintf(fp, "      ");
        fputs(errorValue, fp);
      }
    } else {
      switch (channelType[i]) {
      case DBF_STRING:
      case DBF_ENUM:
        if (!quotes)
          fputs(PVvalue[i].value, fp);
        else {
          if (SDDS_StringIsBlank(PVvalue[i].value) ||
              SDDS_HasWhitespace(PVvalue[i].value))
            fprintf(fp, "\"%s\"", PVvalue[i].value);
          else
            fputs(PVvalue[i].value, fp);
        }
        break;
      default:
        fprintf(fp, "%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f", PVvalue[i].mean,
                PVvalue[i].median, PVvalue[i].sigma, PVvalue[i].stDev, PVvalue[i].MAD,
                PVvalue[i].min, PVvalue[i].max, PVvalue[i].spread);
        break;
      }
    }
    fputc('\n', fp);
  }
}

long writeStatisticsToSDDSFile(char *outputFile, long *channelType, PV_VALUE *PVvalue, long PVs,
                               short *connectedInTime, char *errorValue, short printErrors) {
  SDDS_DATASET SDDS_out;
  long i, row;

  for (i = 0; i < PVs; i++) {
    switch (channelType[i]) {
    case DBF_STRING:
    case DBF_ENUM:
    case DBF_CHAR:
      fprintf(stderr, "Error, doing statistical analysis for PVs of non-numeric type is meaningless.\n");
      return 0;
    default:
      break;
    }
  }

  if (!SDDS_InitializeOutput(&SDDS_out, SDDS_BINARY, 0, NULL, "cavget output", outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_DefineColumn(&SDDS_out, "ControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Mean", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Median", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Sigma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "StDev", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "MAD", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Minimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Maximum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Spread", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_StartTable(&SDDS_out, PVs))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = row = 0; i < PVs; i++, row++) {
    if (printErrors && !connectedInTime[i])
      fprintf(stderr, "%s is not connected or invalid PV name.\n", PVvalue[i].name);
    if (!connectedInTime[i] ||
        channelType[i] == DBF_STRING || channelType[i] == DBF_ENUM) {
      row--;
      continue;
    } else {
      if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                             "ControlName", PVvalue[i].name,
                             "Mean", PVvalue[i].mean, "Median", PVvalue[i].median,
                             "Sigma", PVvalue[i].sigma, "StDev", PVvalue[i].stDev,
                             "MAD", PVvalue[i].MAD, "Minimum", PVvalue[i].min,
                             "Maximum", PVvalue[i].max, "Spread", PVvalue[i].spread, NULL)) {
        SDDS_SetError((char *)"Unable to set statistical values in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        return 0;
      }
    }
  }
  if (!SDDS_WritePage(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!SDDS_Terminate(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return 1;
}

void oag_ca_exception_handler(struct exception_handler_args args) {
  char *pName;
  int severityInt;
  static const char *severity[] =
    {
      "Warning",
      "Success",
      "Error",
      "Info",
      "Fatal",
      "Fatal",
      "Fatal",
      "Fatal"};

  severityInt = CA_EXTRACT_SEVERITY(args.stat);

  fprintf(stderr, "CA.Client.Exception................\n");
  fprintf(stderr, "    %s: \"%s\"\n",
          severity[severityInt],
          ca_message(args.stat));

  if (args.ctx) {
    fprintf(stderr, "    Context: \"%s\"\n", args.ctx);
  }
  if (args.chid) {
    pName = (char *)ca_name(args.chid);
    fprintf(stderr, "    Channel: \"%s\"\n", pName);
    fprintf(stderr, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
  }
  fprintf(stderr, "This sometimes indicates an IOC that is hung up.\n");
  _exit(EXIT_FAILURE);
}

#if (EPICS_VERSION > 3)

void printResultPVA(char *startBrace, long labeled, PVA_OVERALL *pva, PV_VALUE *PVvalue, char *endBrace,
                    char *floatFormat, short charArray, char *errorValue, long quotes, long cavputForm,
                    long excludeErrors, char *delimiter, long secondValue,
                    long doRepeat, long doStats, long repeats, long average, short printErrors) {
  long i, j, n, delimNeeded = 0;
  char *s;

  if (doStats) {
    for (i = 0; i < pva->numPVs; i++) {
      if (pva->isConnected[i]) {
        switch (pva->pvaData[i].scalarType) {
        case epics::pvData::pvString:
        case epics::pvData::pvBoolean:
          fprintf(stderr, "Error, doing statistical analysis for PVs of non-numeric type is meaningless.\n");
          exit(EXIT_FAILURE);
        default:
          break;
        }
        if (pva->pvaData[i].numGetElements != 1) {
          fprintf(stderr, "Error, doing statistical analysis for PVs of array types is not available.\n");
          exit(EXIT_FAILURE);
        }
      }
    }
  }

  for (j = 0; j < pva->numPVs; j++) {
    if (excludeErrors && !pva->isConnected[j])
      continue;
    if (delimNeeded && delimiter)
      fputs(delimiter, stdout);
    if (cavputForm) {
      if (strncmp(PVvalue[j].name, "pva://", 6) == 0) {
        PVvalue[j].name += 6;
        fprintf(stdout, "pva://%s=", PVvalue[j].name);
        PVvalue[j].name -= 6;
      } else if (strncmp(PVvalue[j].name, "ca://", 5) == 0) {
        PVvalue[j].name += 5;
        fprintf(stdout, "ca://%s=", PVvalue[j].name);
        PVvalue[j].name -= 5;
      } else {
        fprintf(stdout, "%s=", PVvalue[j].name);
      }
      quotes = 0;
    } else {
      if (startBrace)
        fputs(startBrace, stdout);
      if (labeled)
        fprintf(stdout, "%s ", pva->pvaChannelNames[j].c_str());
    }
    if (!pva->isConnected[j]) {
      if (printErrors)
        fprintf(stderr, "%s is not connected or invalid PV name.\n", pva->pvaChannelNames[j].c_str());
      else
        fputs(errorValue, stdout);
    } else if ((pva->pvaData[j].numGetElements == 1) && (pva->pvaData[j].fieldType != epics::pvData::scalarArray)) {
      switch (pva->pvaData[j].scalarType) {
      case epics::pvData::pvByte:
      case epics::pvData::pvUByte:
      case epics::pvData::pvShort:
      case epics::pvData::pvUShort:
      case epics::pvData::pvInt:
      case epics::pvData::pvUInt:
      case epics::pvData::pvLong:
      case epics::pvData::pvULong:
        if (cavputForm || (!doRepeat && !doStats)) {
          fprintf(stdout, "%.0f", pva->pvaData[j].mean);
        } else {
          if (doRepeat) {
            if (!average) {
              for (i = 0; i < repeats; i++) {
                fprintf(stdout, "%.0f ", pva->pvaData[j].getData[i].values[0]);
              }
            } else {
              fprintf(stdout, "%.0f", pva->pvaData[j].mean);
              if (secondValue) {
                fprintf(stdout, " ");
                fprintf(stdout, "%.0f", pva->pvaData[j].sigma);
              }
            }
          }
          if (doStats) {
            fprintf(stdout, "mean ");
            fprintf(stdout, "%.0f", pva->pvaData[j].mean);
            fprintf(stdout, " median ");
            fprintf(stdout, "%.0f", pva->pvaData[j].median);
            fprintf(stdout, " sigma ");
            fprintf(stdout, "%.0f", pva->pvaData[j].sigma);
            fprintf(stdout, " stDev ");
            fprintf(stdout, "%.0f", pva->pvaData[j].stDev);
            fprintf(stdout, " MAD ");
            fprintf(stdout, "%.0f", pva->pvaData[j].MAD);
            fprintf(stdout, " min ");
            fprintf(stdout, "%.0f", pva->pvaData[j].min);
            fprintf(stdout, " max ");
            fprintf(stdout, "%.0f", pva->pvaData[j].max);
            fprintf(stdout, " spread ");
            fprintf(stdout, "%.0f", pva->pvaData[j].spread);
          }
        }
        break;
      case epics::pvData::pvString:
      case epics::pvData::pvBoolean:
        n = 0;
        if ((doRepeat) && (average)) {
          n = pva->pvaData[j].numGetReadings - 1;
        }
        while (n < pva->pvaData[j].numGetReadings) {
          s = pva->pvaData[j].getData[n].stringValues[0];
          if (quotes && (SDDS_StringIsBlank((char *)s) || SDDS_HasWhitespace((char *)s))) {
            fprintf(stdout, "\"%s\"", s);
          } else {
            fputs(s, stdout);
          }
          if ((doRepeat) && (secondValue)) {
            fprintf(stdout, " 0");
          }
          if (n != pva->pvaData[j].numGetReadings - 1) {
            fprintf(stdout, " ");
          }
          n++;
        }
        break;
      case epics::pvData::pvDouble:
      case epics::pvData::pvFloat:
      default:
        if (cavputForm || (!doRepeat && !doStats)) {
          fprintf(stdout, floatFormat, pva->pvaData[j].mean);
        } else {
          if (doRepeat) {
            if (!average) {
              for (i = 0; i < repeats; i++) {
                fprintf(stdout, floatFormat, pva->pvaData[j].getData[i].values[0]);
                fprintf(stdout, " ");
              }
            } else {
              fprintf(stdout, floatFormat, pva->pvaData[j].mean);
              if (secondValue) {
                fprintf(stdout, " ");
                fprintf(stdout, floatFormat, pva->pvaData[j].sigma);
              }
            }
          }
          if (doStats) {
            fprintf(stdout, " mean ");
            fprintf(stdout, floatFormat, pva->pvaData[j].mean);
            fprintf(stdout, " median ");
            fprintf(stdout, floatFormat, pva->pvaData[j].median);
            fprintf(stdout, " sigma ");
            fprintf(stdout, floatFormat, pva->pvaData[j].sigma);
            fprintf(stdout, " stDev ");
            fprintf(stdout, floatFormat, pva->pvaData[j].stDev);
            fprintf(stdout, " MAD ");
            fprintf(stdout, floatFormat, pva->pvaData[j].MAD);
            fprintf(stdout, " min ");
            fprintf(stdout, floatFormat, pva->pvaData[j].min);
            fprintf(stdout, " max ");
            fprintf(stdout, floatFormat, pva->pvaData[j].max);
            fprintf(stdout, " spread ");
            fprintf(stdout, floatFormat, pva->pvaData[j].spread);
          }
        }
        break;
      }
    } else {
      n = 0;
      if ((doRepeat) && (average)) {
        n = pva->pvaData[j].numGetReadings - 1;
      }
      switch (pva->pvaData[j].scalarType) {
      case epics::pvData::pvString:
      case epics::pvData::pvBoolean: {
        while (n < pva->pvaData[j].numGetReadings) {
          fprintf(stdout, "%ld", pva->pvaData[j].numGetElements);
          for (i = 0; i < pva->pvaData[j].numGetElements; i++) {
            if (quotes && (SDDS_StringIsBlank((char *)pva->pvaData[j].getData[n].stringValues[i]) || SDDS_HasWhitespace((char *)pva->pvaData[j].getData[n].stringValues[i]))) {
              fprintf(stdout, ",\"%s\"", pva->pvaData[j].getData[n].stringValues[i]);
            } else {
              fprintf(stdout, ",%s", pva->pvaData[j].getData[n].stringValues[i]);
            }
          }
          if ((doRepeat) && (secondValue)) {
            fprintf(stdout, " 0");
          }
          if (n != pva->pvaData[j].numGetReadings - 1) {
            fprintf(stdout, " ");
          }
          n++;
        }
        break;
      }
      case epics::pvData::pvByte:
      case epics::pvData::pvUByte: {
        while (n < pva->pvaData[j].numGetReadings) {
          if (charArray) {
            if (quotes) {
              fprintf(stdout, "\"");
            }
            for (i = 0; i < pva->pvaData[j].numGetElements; i++) {
              fprintf(stdout, "%c", (char)pva->pvaData[j].getData[n].values[i]);
            }
            if (quotes) {
              fprintf(stdout, "\"");
            }
          } else {
            fprintf(stdout, "%ld", pva->pvaData[j].numGetElements);
            for (i = 0; i < pva->pvaData[j].numGetElements; i++) {
              fprintf(stdout, ",%.0f", pva->pvaData[j].getData[n].values[i]);
            }
            if ((doRepeat) && (secondValue)) {
              fprintf(stdout, " 0");
            }
          }
          if (n != pva->pvaData[j].numGetReadings - 1) {
            fprintf(stdout, " ");
          }
          n++;
        }
        break;
      }
      case epics::pvData::pvShort:
      case epics::pvData::pvUShort:
      case epics::pvData::pvInt:
      case epics::pvData::pvUInt:
      case epics::pvData::pvLong:
      case epics::pvData::pvULong: {
        while (n < pva->pvaData[j].numGetReadings) {
          fprintf(stdout, "%ld", pva->pvaData[j].numGetElements);
          for (i = 0; i < pva->pvaData[j].numGetElements; i++) {
            fprintf(stdout, ",%.0f", pva->pvaData[j].getData[n].values[i]);
          }
          if ((doRepeat) && (secondValue)) {
            fprintf(stdout, " 0");
          }
          if (n != pva->pvaData[j].numGetReadings - 1) {
            fprintf(stdout, " ");
          }
          n++;
        }
        break;
      }
      case epics::pvData::pvDouble:
      case epics::pvData::pvFloat:
      default: {
        while (n < pva->pvaData[j].numGetReadings) {
          fprintf(stdout, "%ld", pva->pvaData[j].numGetElements);
          for (i = 0; i < pva->pvaData[j].numGetElements; i++) {
            fprintf(stdout, ",");
            fprintf(stdout, floatFormat, pva->pvaData[j].getData[n].values[i]);
          }
          if ((doRepeat) && (secondValue)) {
            fprintf(stdout, " 0");
          }
          if (n != pva->pvaData[j].numGetReadings - 1) {
            fprintf(stdout, " ");
          }
          n++;
        }
        break;
      }
      }
    }
    if (endBrace && !cavputForm)
      fputs(endBrace, stdout);

    delimNeeded = 1;
  }
  if (delimNeeded)
    fputc('\n', stdout);
  return;
}

void printResultPrettyPVA(PVA_OVERALL *pva, long quotes, char *errorValue, short printErrors) {
  long i;

  fprintf(stdout, "%20s", "ControlName");
  fprintf(stdout, "%10s", "mean");
  fprintf(stdout, "%10s", "median");
  fprintf(stdout, "%10s", "sigma");
  fprintf(stdout, "%10s", "stDev");
  fprintf(stdout, "%10s", "MAD");
  fprintf(stdout, "%10s", "min");
  fprintf(stdout, "%10s", "max");
  fprintf(stdout, "%10s", "spread");
  fputc('\n', stdout);
  for (i = 0; i < 100; i++)
    fputc('-', stdout);
  fputc('\n', stdout);

  for (i = 0; i < pva->numPVs; i++) {
    if (!pva->isConnected[i]) {
      if (printErrors)
        fprintf(stderr, "%s is not connected or invalid PV name.\n", pva->pvaChannelNames[i].c_str());
      else {
        fprintf(stdout, "%20s", pva->pvaChannelNames[i].c_str());
        fprintf(stdout, "      ");
        fputs(errorValue, stdout);
      }
    } else {
      fprintf(stdout, "%20s", pva->pvaChannelNames[i].c_str());
      if (pva->pvaData[i].numGetElements == 1) {
        switch (pva->pvaData[i].scalarType) {
        case epics::pvData::pvString:
        case epics::pvData::pvBoolean:
          break;
        default:
          fprintf(stdout, "%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f%10.4f", pva->pvaData[i].mean,
                  pva->pvaData[i].median, pva->pvaData[i].sigma, pva->pvaData[i].stDev, pva->pvaData[i].MAD,
                  pva->pvaData[i].min, pva->pvaData[i].max, pva->pvaData[i].spread);
          break;
        }
      } else {
      }
    }
    fputc('\n', stdout);
  }
}

void writeStatisticsToSDDSFilePVA(char *outputFile, PVA_OVERALL *pva, long quotes, char *errorValue, short printErrors) {
  SDDS_DATASET SDDS_out;
  long i, row;

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->isConnected[i]) {
      switch (pva->pvaData[i].scalarType) {
      case epics::pvData::pvString:
      case epics::pvData::pvBoolean:
        fprintf(stderr, "Error, doing statistical analysis for PVs of non-numeric type is meaningless.\n");
        exit(EXIT_FAILURE);
      default:
        break;
      }
      if (pva->pvaData[i].numGetElements != 1) {
        fprintf(stderr, "Error, doing statistical analysis for PVs of array types is not available.\n");
        exit(EXIT_FAILURE);
      }
    }
  }

  if (!SDDS_InitializeOutput(&SDDS_out, SDDS_BINARY, 0, NULL, "cavget output", outputFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_DefineColumn(&SDDS_out, "ControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Mean", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Median", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Sigma", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "StDev", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "MAD", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Minimum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Maximum", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_out, "Spread", NULL, NULL, NULL, NULL, SDDS_DOUBLE, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_StartTable(&SDDS_out, pva->numPVs))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = row = 0; i < pva->numPVs; i++, row++) {
    if (printErrors && !pva->isConnected[i])
      fprintf(stderr, "%s is not connected or invalid PV name.\n", pva->pvaChannelNames[i].c_str());
    if (!pva->isConnected[i] ||
        pva->pvaData[i].scalarType == epics::pvData::pvString ||
        pva->pvaData[i].scalarType == epics::pvData::pvBoolean) {
      row--;
      continue;
    } else {
      if (!SDDS_SetRowValues(&SDDS_out, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, row,
                             "ControlName", pva->pvaChannelNames[i].c_str(),
                             "Mean", pva->pvaData[i].mean, "Median", pva->pvaData[i].median,
                             "Sigma", pva->pvaData[i].sigma, "StDev", pva->pvaData[i].stDev,
                             "MAD", pva->pvaData[i].MAD, "Minimum", pva->pvaData[i].min,
                             "Maximum", pva->pvaData[i].max, "Spread", pva->pvaData[i].spread, NULL)) {
        SDDS_SetError((char *)"Unable to set statistical values in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
  }
  if (!SDDS_WritePage(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!SDDS_Terminate(&SDDS_out))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return;
}

void GetInfoData(PVA_OVERALL *pva, PV_VALUE *PVvalue) {
  long n;
  uint32_t EnumNumOfChoices;
  char **EnumChoices=NULL;
  for (n=0; n<pva->numPVs; n++) {
    if (n>0)
      fprintf(stdout, "\n");
    fprintf(stdout, "%s\n", PVvalue[n].name);
    if (pva->isConnected[n])
      fprintf(stdout, "            Active:   y\n");
    else
      fprintf(stdout, "            Active:   n\n");
    fprintf(stdout, "          Provider:   %s\n", GetProviderName(pva, n).c_str());
    fprintf(stdout, "              Host:   %s\n", GetRemoteAddress(pva,n).c_str());
    if (HaveReadAccess(pva, n))
      fprintf(stdout, "       Read Access:   y\n");
    else
      fprintf(stdout, "       Read Access:   n\n");
    if (HaveWriteAccess(pva, n))
      fprintf(stdout, "      Write Access:   y\n");
    else
      fprintf(stdout, "      Write Access:   n\n");
    fprintf(stdout, "    Alarm Severity:   %s\n", GetAlarmSeverity(pva, n).c_str());
    fprintf(stdout, "      Structure ID:   %s\n", GetStructureID(pva, n).c_str());
    fprintf(stdout, "        Field Type:   %s\n", GetFieldType(pva,n).c_str());
    fprintf(stdout, "     Element Count:   %" PRIu32 "\n", GetElementCount(pva,n));
    fprintf(stdout, "  Native Data Type:   %s\n", GetNativeDataType(pva,n).c_str());
    if (IsEnumFieldType(pva,n)) {
      EnumNumOfChoices = GetEnumChoices(pva, n, &EnumChoices);
      fprintf(stdout, " Number of Choices:   %" PRIu32 "\n", EnumNumOfChoices);
      for (uint32_t m = 0; m < EnumNumOfChoices; m++) {
        fprintf(stdout, "           Index %" PRIu32 ":   %s\n", m, EnumChoices[m]);
        free(EnumChoices[m]);
        EnumChoices[m]=NULL;
      }
      if (EnumChoices) {
        free(EnumChoices);
        EnumChoices=NULL;
      }
    }
  }

}

#endif
