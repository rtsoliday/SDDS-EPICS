/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddsvmonitor.c
 * purpose: reads process variable values as specified in two lists.  The first
 * list gives PV "rootnames".  The second list gives suffixes to apply to each of 
 * the rootnames.
 * output: each page of the output file is a separate snapshot.  
 *   columns: The file contains three or more columns.  "Index" and "Rootname"
 *       are always present.  The remainder are named after the suffixes, as given 
 *       in the second list.
 *   parameters: Step, Time (elapsed seconds), TimeOfDay (hours since midnight),
 *       DayOfMonth, TimeStamp (string), StartTime (sec since long ago),
 *       YearStartTime
 *
 * based on sddsmonitor2.c
 * L. Emery, M. Borland, 1994-1995.
 $Log: not supported by cvs2svn $
 Revision 1.6  2006/04/04 21:44:18  soliday
 Xuesong Jiao modified the error messages if one or more PVs don't connect.
 It will now print out all the non-connecting PV names.

 Revision 1.5  2005/11/09 19:42:30  soliday
 Updated to remove compiler warnings on Linux

 Revision 1.4  2005/11/08 22:05:04  soliday
 Added support for 64 bit compilers.

 Revision 1.3  2004/07/19 17:39:38  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/15 21:22:46  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base 3.14.x
 has an inactivity timer that was causing disconnects from PVs when the
 log interval time was too large.

 Revision 1.1  2003/08/27 19:51:25  soliday
 Moved into subdirectory

 Revision 1.30  2002/10/31 15:47:26  soliday
 It now converts the old ezca option into a pendtimeout value.

 Revision 1.29  2002/10/17 18:32:58  soliday
 BPS16monitor.c and sddsvmonitor.c no longer use ezca.

 Revision 1.28  2002/08/14 20:00:37  soliday
 Added Open License

 Revision 1.27  2002/07/01 21:12:12  soliday
 Fsync is no longer the default so it is now set after the file is opened.

 Revision 1.26  2001/10/10 15:29:08  shang
 fixed the problem that enforceTimeLimit did not work properly for -append mode by
 adding RunStartTime and RunTime to record the program starting time and actual
 running time.

 Revision 1.25  2001/05/03 19:53:48  soliday
 Standardized the Usage message.

 Revision 1.24  2000/04/20 15:59:42  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.23  2000/04/19 15:52:06  soliday
 Removed some solaris compiler warnings.

 Revision 1.22  2000/03/08 17:14:05  soliday
 Removed compiler warnings under Linux.

 Revision 1.21  1999/09/17 22:13:34  soliday
 This version now works with WIN32

 Revision 1.20  1998/11/18 14:33:59  borland
 Brought up to date with latest version of MakeGenerationFilename().

 Revision 1.19  1998/07/30 16:23:45  borland
 Added extra option to getScalarMonitorData as required by addition of
 dead-band feature for sddsmonitor.

 Revision 1.18  1997/07/31 18:42:15  borland
 Added conditions holdoff feature.

 Revision 1.17  1997/05/30 14:53:42  borland
 Added -offsetTimeOfDay option.  When given, causes TimeOfDay data to be
 offset by 24 hours if more than half of the time of the run will fall
 in the following day.  This permits starting a 24+ hour job before midnight
 with the TimeOfDay values starting at negative numbers, then running 0
 to 24 in the following day.  An identical job started just after midnight
 will have the same TimeOfDay values as the first.

 Revision 1.16  1997/02/04 22:10:47  borland
 Added initialization of useEzca variable.

 Revision 1.15  1996/12/13 20:25:23  borland
 Added -enforceTimeLimit option.

 Revision 1.14  1996/08/21 17:16:50  borland
 Check prompt/noprompt mode before emitting "Done." message at end.

 Revision 1.13  1996/06/13 19:46:35  borland
 Improved function of -singleShot option by providing control of whether
 prompts go to stderr or stdout.  Also added "Done." message before program
 exits.

 * Revision 1.12  1996/02/28  21:14:36  borland
 * Added mode argument to getScalarMonitorData() for control of units control.
 * sddsmonitor and sddsstatmon now have -getUnits option. sddswmonitor and
 * sddsvmonitor now get units from EPICS for any scalar that has a blank
 * units string.
 *
 * Revision 1.11  1996/02/16  17:20:13  borland
 * Added recover qualifier to -append option.
 *
 * Revision 1.10  1996/02/15  03:41:44  borland
 * Made some changes to the usage message for -conditions option to make it
 * consistent with actual function of programs.
 *
 * Revision 1.9  1996/02/14  05:08:01  borland
 * Switched from obsolete scan_item_list() routine to scanItemList(), which
 * offers better response to invalid/ambiguous qualifiers.
 *
 * Revision 1.8  1996/02/10  06:30:39  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.7  1996/02/07  18:31:57  borland
 * Brought up-to-date with new time routines; now uses common setup routines
 * for time data in SDDS output.
 *
 * Revision 1.6  1996/01/02  18:38:07  borland
 * Added -comment and -generations options to monitor and logging programs;
 * added support routines in SDDSepics.c .
 *
 * Revision 1.5  1995/11/13  16:23:59  borland
 * Cleaned up some parsing statements to check for presence of data before
 * scanning.  Fixed harmless uninitialized memory read problem with scalar
 * data lists.
 *
 * Revision 1.4  1995/11/09  03:22:46  borland
 * Added copyright message to source files.
 *
 * Revision 1.3  1995/11/04  04:45:40  borland
 * Added new program sddsalarmlog to Makefile.  Changed routine
 * getTimeBreakdown in SDDSepics, and modified programs to use the new
 * version; it computes the Julian day and the year now.
 *
 * Revision 1.2  1995/10/15  21:18:00  borland
 * Fixed bug in getScalarMonitorData (wasn't initializing *scaleFactor).
 * Fixed bug in sdds[vw]monitor argument parsing for -onCAerror option (had
 * exit outside of if branch).
 *
 * Revision 1.1  1995/09/25  20:15:54  saunders
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
#define CLO_APPEND 6
#define CLO_PRECISION 7
#define CLO_PROMPT 8
#define CLO_ONCAERROR 9
#define CLO_PENDIOTIME 10
#define CLO_EZCATIMING 11
#define CLO_LOGDUPLICATES 12
#define CLO_NOEZCA 13
#define CLO_PVLIST 14
#define CLO_ROOTNAME 15
#define CLO_SUFFIX 16
#define CLO_SCALARS 17
#define CLO_CONDITIONS 18
#define CLO_GENERATIONS 19
#define CLO_COMMENT 20
#define CLO_ENFORCETIMELIMIT 21
#define CLO_OFFSETTIMEOFDAY 22
#define COMMANDLINE_OPTIONS 23
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval",
  "steps",
  "verbose",
  "erase",
  "singleshot",
  "time",
  "append",
  "precision",
  "prompt",
  "oncaerror",
  "pendiotime",
  "ezcatiming",
  "logduplicates",
  "noezca",
  "pvlist",
  "rootname",
  "suffix",
  "scalars",
  "conditions",
  "generations",
  "comment",
  "enforcetimelimit",
  "offsettimeofday",
};

#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2
char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double",
};

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

static char *USAGE1 = "sddsvmonitor [<inputfile> | -rootnames=<file> -suffixes=<file>]\n\
    <outputfile> [{-erase | -append[=recover] | -generations[=digits=<integer>][,delimiter=<string>}]\n\
    [-steps=<integer> | -time=<value>[,<units>]] [-interval=<value>[,<units>]]\n\
    [-enforceTimeLimit] [-offsetTimeOfDay]\n\
    [-verbose] [-singleShot{=noprompt|stdout}] [-precision={single|double}]\n\
    [-onCAerror={useZero|skipPage|exit}] [-PVlist=<filename>]\n\
    [-scalars=<filename>]\n\
    [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
    [-logDuplicates[=countThreshold=<number>]]\n\
    [-comment=<parameterName>,<text>]\n\n\
Writes values of process variables to a binary SDDS file.\n\
<inputfile>        SDDS input file containing the parameter \"ListType\" and the column\n\
                   \"ListData\". ListType has values \"rootnames\" or \"suffixes\".\n\
rootnames          specifies SDDS input file from which rootnames will be extracted.\n\
                   File must contain the column \"Rootname\" (like sddsvexperiment rootname file).\n\
suffixes           specifies SDDS input file from which suffixes will be extracted.\n\
                   File must contain the column \"Suffix\" (like sddsvexperiment suffix file).\n";
static char *USAGE2 = "<outputfile>       SDDS output file, each page of which contains a snapshot of each attribute\n\
                   for each object, one object per row.\n\
erase              outputfile is erased before execution.\n\
append             appends new data to the output file.\n\
generations        The output is sent to the file <outputfile>-<N>, where <N> is\n\
                   the smallest positive integer such that the file does not already \n\
                   exist.   By default, four digits are used for formating <N>, so that\n\
                   the first generation number is 0001.\n\
scalars            specifies sddsmonitor input file to get names of scalar PVs\n\
                   from.  These will be logged in the output file as parameters.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
steps              number of reads for each process variable.\n\
time               total time (in seconds) for monitoring;\n\
                   valid time units are seconds, minutes, hours, or days.\n\
enforceTimeLimit   Enforces the time limit given with the -time option, even if\n\
                   the expected number of samples has not been taken.\n";
static char *USAGE3 = "offsetTimeOfDay    Adjusts the starting TimeOfDay value so that it corresponds\n\
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
onCAerror          action taken when a channel access error occurs. Default is\n\
                   to use zeroes for values.\n\
logDuplicates      specifies that data should be logged even if it is exactly the\n\
                   same as the last data.\n\
comment            Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                   along with the text to be placed in the file.  \n\
Program by M. Borland, L. Emery, ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 1

#define ROOTNAME_LIST 0
#define SUFFIX_LIST 1
#define LIST_TYPES 2
static char *listTypeOption[LIST_TYPES] = {
  "rootnames",
  "suffixes",
};

#define EXTRA_COLUMNS 2

void copyDoubleArray(double *target, double *source, long n);
long doubleArrayDifferences(double *d1, double *d2, long n);
void writePVlist(char *file, char **PVname, char **PVunits, long PVnames, long divisor);
long readRootnameFile(char *rootnameFile, char ***rootname);
long readSuffixFile(char *suffixFile, char ***suffix);

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE inTable, outTable;
  char *inputfile, *outputfile, *PVlistFile, *rootnameFile, *suffixFile, answer[ANSWER_LENGTH];
  long rootnames, suffixes = 0, CAerrors;
  char **rootname = NULL, **suffix = NULL, **readbackName, **readbackUnits = NULL, **list = NULL, *ListType, **outputColumn = NULL;
  chid *readbackCHID = NULL;
  double *doubleValue = NULL, *lastDoubleValue = NULL;
  long i, j, k, i_arg, outputColumns, readbackNames, length, logDuplicates, differences;
  long enforceTimeLimit;
  char s[1024];
  char *TimeStamp = NULL, *PageTimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartDay, StartHour, TimeOfDay, DayOfMonth;
  double TotalTime, StartTime, StartJulianDay, StartYear, StartMonth, timeNow, RunStartTime, RunTime;
  long Step, NStep, verbose, singleShot, firstPage;
  long stepsSet, totalTimeSet;
  long outputPreviouslyExists = 0, append, erase, recover;
  char *precision_string;
  long Precision, TimeUnits, rows = 0;
  int32_t dupCountThreshold;
  int32_t *index = NULL;
  long onCAerror;
  double timeToRead, timeToWait;
  unsigned long dummyFlags;
  char *scalarFile, **scalarPV, **scalarName = NULL, **scalarUnits = NULL, **readMessage = NULL;
  chid *scalarCHID = NULL;
  double *scalarData, *scalarFactor;
  long scalars;

  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  long conditions;
  unsigned long CondMode = 0;
  double *CondDataBuffer;
  chid *CondCHID = NULL;

  char **commentParameter = NULL, **commentText = NULL, *generationsDelimiter;
  long comments, generations;
  int32_t generationsDigits;
  long offsetTimeOfDay;
  double pendIOtime = 10.0, timeout;
  long type, retries;
  INFOBUF *info = NULL;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    exit(1);
  }

  offsetTimeOfDay = 0;
  inputfile = outputfile = PVlistFile = rootnameFile = suffixFile = scalarFile = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = verbose = append = erase = singleShot = stepsSet = generations = recover = 0;
  onCAerror = ONCAERROR_USEZERO;
  Precision = PRECISION_SINGLE;
  logDuplicates = 0;
  CondFile = NULL;
  commentParameter = commentText = NULL;
  comments = 0;
  enforceTimeLimit = 0;
  CondDataBuffer = scalarData = NULL;
  precision_string = NULL;
  list = NULL;

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
      case CLO_STEPS:
        if (!(get_long(&NStep, s_arg[i_arg].list[1])) || NStep <= 0)
          bomb("no value or invalid value given for option -steps", NULL);
        stepsSet = 1;
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
      case CLO_APPEND:
        if (s_arg[i_arg].n_items != 1 && s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -append syntax/value");
        append = 1;
        recover = 0;
        if (s_arg[i_arg].n_items == 2) {
          if (strncmp(s_arg[i_arg].list[1], "recover", strlen(s_arg[i_arg].list[1])) == 0)
            recover = 1;
          else
            SDDS_Bomb("invalid -append syntax/value");
        }
        break;
      case CLO_ERASE:
        erase = 1;
        break;
      case CLO_PRECISION:
        if (s_arg[i_arg].n_items < 2 ||
            !(precision_string = s_arg[i_arg].list[1]))
          bomb("no value given for option -precision", NULL);
        if ((Precision = match_string(precision_string, precision_option, PRECISION_OPTIONS, 0)) < 0)
          bomb("invalid -precision value", NULL);
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
      case CLO_PVLIST:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of keywords for -PVlist", NULL);
        PVlistFile = s_arg[i_arg].list[1];
        break;
      case CLO_ROOTNAME:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of keywords for -rootname", NULL);
        rootnameFile = s_arg[i_arg].list[1];
        break;
      case CLO_SUFFIX:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of keywords for -suffix", NULL);
        suffixFile = s_arg[i_arg].list[1];
        break;
      case CLO_LOGDUPLICATES:
        s_arg[i_arg].n_items--;
        dupCountThreshold = 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "countthreshold", SDDS_LONG, &dupCountThreshold, 1, 0,
                          NULL) ||
            dupCountThreshold < 0)
          bomb("invalid -logDuplicates syntax", NULL);
        logDuplicates = 1;
        s_arg[i_arg].n_items++;
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

  if (!inputfile && !(rootnameFile && suffixFile))
    bomb("input filename not given, and -rootname/-suffix options not given", NULL);
  if (rootnameFile && suffixFile) {
    if (outputfile)
      bomb("too many filenames given", NULL);
    outputfile = inputfile;
    inputfile = NULL;
  }
  if (!outputfile)
    bomb("output filename not given", NULL);
  if (append && erase)
    SDDS_Bomb("-append and -erase are incompatible options");
  if (append && generations)
    SDDS_Bomb("-append and -generations are incompatible options");
  if (erase && generations)
    SDDS_Bomb("-erase and -generations are incompatible options");
  if (generations)
    outputfile = MakeGenerationFilename(outputfile, generationsDigits, generationsDelimiter, NULL);

  if (totalTimeSet) {
    if (stepsSet)
      fprintf(stderr, "warning: option -time supersedes option -steps\n");
    NStep = TotalTime / TimeInterval + 1;
  } else {
    enforceTimeLimit = 0;
  }

  if ((rootnameFile && !suffixFile) || (!rootnameFile && suffixFile))
    bomb("you must give both -rootname and -suffix, or neither", NULL);

  if (rootnameFile && suffixFile) {
    if ((rootnames = readRootnameFile(rootnameFile, &rootname)) < 0 ||
        (suffixes = readSuffixFile(suffixFile, &suffix)) < 0)
      bomb("unable to read rootnames and/or suffixes", NULL);
  } else {
    rootname = suffix = NULL;
    rootnames = suffixes = 0;
    SDDS_InitializeInput(&inTable, inputfile);
    if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&inTable, "ListType", NULL, SDDS_STRING, stdout)) ||
        (SDDS_CHECK_OKAY != SDDS_CheckColumn(&inTable, "ListData", NULL, SDDS_STRING, stdout))) {
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    while (SDDS_ReadTable(&inTable) >= 1) {
      if (!SDDS_GetParameter(&inTable, "ListType", &ListType) ||
          !(list = SDDS_GetColumn(&inTable, "ListData")))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!(rows = SDDS_CountRowsOfInterest(&inTable)))
        continue;
      switch (match_string(ListType, listTypeOption, LIST_TYPES, 0)) {
      case ROOTNAME_LIST:
        rootname = SDDS_Realloc(rootname, sizeof(*rootname) * (rows + rootnames));
        for (i = 0; i < rows; i++)
          rootname[i + rootnames] = list[i];
        rootnames += rows;
        break;
      case SUFFIX_LIST:
        suffix = SDDS_Realloc(suffix, sizeof(*suffix) * (rows + suffixes));
        for (i = 0; i < rows; i++)
          suffix[i + suffixes] = list[i];
        suffixes += rows;
        break;
      default:
        fprintf(stderr, "error: unknown/ambiguous ListType \"%s\" seen\n",
                ListType);
        exit(1);
        break;
      }
    }
    if (!SDDS_Terminate(&inTable))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  scalars = 0;
  scalarPV = scalarName = NULL;

  if (scalarFile) {
    if (!getScalarMonitorData(&scalarPV, &readMessage, &scalarName,
                              &scalarUnits, &scalarFactor, NULL,
                              &scalars, scalarFile, GET_UNITS_IF_BLANK,
                              &scalarCHID, pendIOtime))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
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

  readbackNames = rootnames * suffixes;
  doubleValue = (double *)malloc(sizeof(double) * readbackNames);
  lastDoubleValue = (double *)malloc(sizeof(double) * readbackNames);
  outputColumns = suffixes + EXTRA_COLUMNS;
  outputColumn = tmalloc(sizeof(*outputColumn) * outputColumns);
  SDDS_CopyString(&outputColumn[0], "Index");
  SDDS_CopyString(&outputColumn[1], "Rootname");
  for (i = 0; i < suffixes; i++)
    outputColumn[i + EXTRA_COLUMNS] = suffix[i];
  readbackName = tmalloc(sizeof(*readbackName) * readbackNames);
  readbackUnits = tmalloc(sizeof(*readbackUnits) * suffixes);
  readbackCHID = tmalloc(sizeof(*readbackCHID) * readbackNames);
  info = tmalloc(sizeof(INFOBUF) * suffixes);
  for (i = 0; i < readbackNames; i++)
    readbackCHID[i] = NULL;
  ca_task_initialize();
  for (j = k = 0; j < suffixes; j++) {
    for (i = 0; i < rootnames; i++, k++) {
      sprintf(s, "%s%s", rootname[i], suffix[j]);
      SDDS_CopyString(readbackName + k, s);
    }
    if ((length = strlen(s)) > 4 && strncmp(s + (length - 4), ".VAL", 4) == 0)
      s[length - 4] = 0;
    readbackUnits[j] = NULL;
    if (!strchr(s, '.')) {
      if (ca_search(readbackName[((j + 1) * rootnames) - 1], &readbackCHID[((j + 1) * rootnames) - 1]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", readbackName[((j + 1) * rootnames) - 1]);
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < readbackNames; i++) {
      if (ca_state(readbackCHID[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", readbackName[i]);
    }
    exit(1);
  }
  for (j = k = 0; j < suffixes; j++) {
    for (i = 0; i < rootnames; i++, k++) {
      sprintf(s, "%s%s", rootname[i], suffix[j]);
    }
    if ((length = strlen(s)) > 4 && strncmp(s + (length - 4), ".VAL", 4) == 0)
      s[length - 4] = 0;
    if (!strchr(s, '.')) {
      type = ca_field_type(readbackCHID[((j + 1) * rootnames) - 1]);
      if ((type == DBF_STRING) || (type == DBF_ENUM)) {
        readbackUnits[j] = malloc(sizeof(char) * 9);
        readbackUnits[j][0] = 0;
        continue;
      }
      if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                       readbackCHID[((j + 1) * rootnames) - 1], &info[j]) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to read units.\n");
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    exit(1);
  }
  for (j = k = 0; j < suffixes; j++) {
    for (i = 0; i < rootnames; i++, k++) {
      sprintf(s, "%s%s", rootname[i], suffix[j]);
    }
    if ((length = strlen(s)) > 4 && strncmp(s + (length - 4), ".VAL", 4) == 0)
      s[length - 4] = 0;
    if (!strchr(s, '.')) {
      type = ca_field_type(readbackCHID[((j + 1) * rootnames) - 1]);
      switch (type) {
      case DBF_CHAR:
        SDDS_CopyString(readbackUnits + j, info[j].c.units);
        break;
      case DBF_SHORT:
        SDDS_CopyString(readbackUnits + j, info[j].i.units);
        break;
      case DBF_LONG:
        SDDS_CopyString(readbackUnits + j, info[j].l.units);
        break;
      case DBF_FLOAT:
        SDDS_CopyString(readbackUnits + j, info[j].f.units);
        break;
      case DBF_DOUBLE:
        SDDS_CopyString(readbackUnits + j, info[j].d.units);
        break;
      default:
        break;
      }
    }
  }
  if (info)
    free(info);
  if (PVlistFile)
    writePVlist(PVlistFile, readbackName, readbackUnits, readbackNames, rootnames);

  index = tmalloc(sizeof(*index) * rootnames);
  for (i = 0; i < rootnames; i++)
    index[i] = i;

  if (scalars) {
    scalarData = tmalloc(sizeof(*scalarData) * scalars);
  }

  if (append)
    outputPreviouslyExists = fexists(outputfile);
  else if (fexists(outputfile) && !erase) {
    printf("Error. File %s already exists.\n", outputfile);
    exit(1);
  }

  if (append && outputPreviouslyExists) {
    SDDS_TABLE originalTable;
    char **columnName;
    int32_t columnNames;

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
    if (!SDDS_InitializeInput(&originalTable, outputfile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    columnName = (char **)SDDS_GetColumnNames(&originalTable, &columnNames);
    for (i = 0; i < outputColumns; i++) {
      if (-1 == match_string(outputColumn[i], columnName, columnNames, EXACT_MATCH)) {
        fprintf(stderr,
                "error: suffix %s doesn't match any columns in original output file.\n",
                suffix[i]);
        exit(1);
      }
    }
    if (match_string("Index", columnName, columnNames, EXACT_MATCH) < 0)
      bomb("column Index not found in original file.\n", NULL);
    if (match_string("Rootname", columnName, columnNames, EXACT_MATCH) < 0)
      bomb("column Rootname not found in original file.\n", NULL);

    for (i = 0; i < columnNames; i++) {
      if (-1 == match_string(columnName[i], outputColumn, outputColumns, EXACT_MATCH)) {
        fprintf(stderr,
                "Column %s in original output doesn't match any readback name value "
                "in input file.\n",
                columnName[i]);
        exit(1);
      }
    }
    if (SDDS_ReadTable(&originalTable) != 1) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      bomb("unable to get data from existing file", NULL);
    }
    if (!SDDS_GetParameter(&originalTable, "StartHour", &StartHour) ||
        !SDDS_GetParameterAsDouble(&originalTable, "StartDayOfMonth", &StartDay) ||
        !SDDS_GetParameter(&originalTable, "StartTime", &StartTime)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      exit(1);
    }
    StartDay += StartHour / 24.0;

    if (!SDDS_InitializeAppend(&outTable, outputfile) ||
        !SDDS_StartTable(&outTable, NStep) ||
        !SDDS_CopyParameters(&outTable, &originalTable) ||
        !SDDS_Terminate(&originalTable))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    firstPage = 0;
    getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, &PageTimeStamp);
  } else {
    /* start with a new file */
    getTimeBreakdown(&StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                     &StartYear, &PageTimeStamp);
    RunStartTime = StartTime;
    SDDS_CopyString(&TimeStamp, PageTimeStamp);
    if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 1, "Monitored devices", "Monitored devices", outputfile) ||
        !DefineLoggingTimeParameters(&outTable) ||
        !DefineLoggingTimeDetail(&outTable, TIMEDETAIL_EXTRAS) ||
        0 > SDDS_DefineColumn(&outTable, "Index", NULL, NULL, NULL, NULL, SDDS_LONG, 0) ||
        0 > SDDS_DefineColumn(&outTable, "Rootname", NULL, NULL, NULL, NULL, SDDS_STRING, 0))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    SDDS_EnableFSync(&outTable);
    if ((scalars && !SDDS_DefineSimpleParameters(&outTable, scalars, scalarName, scalarUnits,
                                                 (Precision == PRECISION_SINGLE ? SDDS_FLOAT : SDDS_DOUBLE))) ||
        !SDDS_DefineSimpleColumns(&outTable, suffixes, suffix, readbackUnits,
                                  (Precision == PRECISION_SINGLE ? SDDS_FLOAT : SDDS_DOUBLE)))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_DefineSimpleParameters(&outTable, comments, commentParameter, NULL, SDDS_STRING) ||
        !SDDS_WriteLayout(&outTable))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    firstPage = 1;
  }
  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }

  timeToRead = 0;
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
      if ((timeToWait = TimeInterval - timeToRead) < 0)
        timeToWait = 0;
      if (timeToWait > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(timeToWait);
      } else {
        ca_poll();
      }
    }
    timeToRead = getTimeInSecs();
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
      if (!Step) {
        if ((timeToWait = TimeInterval - timeToRead) < 0)
          timeToWait = 0;
        if (timeToWait > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(timeToWait);
        } else {
          ca_poll();
        }
      }

      if (CondMode & RETAKE_STEP)
        Step--;
      timeToRead = getTimeInSecs() - timeToRead;
      continue;
    }
    if ((CAerrors = ReadScalarValues(readbackName, NULL, NULL, doubleValue, readbackNames, readbackCHID, pendIOtime)) != 0) {
      switch (onCAerror) {
      case ONCAERROR_USEZERO:
        break;
      case ONCAERROR_SKIPPAGE:
        continue;
      case ONCAERROR_EXIT:
        fprintf(stderr, "%ld errors reading vector of values--aborting\n", CAerrors);
        exit(1);
        break;
      }
    }
    if (scalarPV &&
        (CAerrors = ReadScalarValues(scalarPV, readMessage, scalarFactor,
                                     scalarData, scalars, scalarCHID, pendIOtime)))
      switch (onCAerror) {
      case ONCAERROR_USEZERO:
        break;
      case ONCAERROR_SKIPPAGE:
        continue;
      case ONCAERROR_EXIT:
        fprintf(stderr, "%ld errors reading scalar values--aborting\n", CAerrors);
        exit(1);
        break;
      }
    if (Step) {
      differences = doubleArrayDifferences(doubleValue, lastDoubleValue, readbackNames);
      if ((!logDuplicates && !differences) ||
          (logDuplicates && differences < dupCountThreshold)) {
        if (verbose) {
          printf("  data not logged--hasn't changed sufficiently since last acquisition\n");
          fflush(stdout);
        }
        timeToRead = getTimeInSecs() - timeToRead;
        continue;
      }
    }
    copyDoubleArray(lastDoubleValue, doubleValue, readbackNames);
    ElapsedTime = (timeNow = getTimeInSecs()) - StartTime;
    RunTime = getTimeInSecs() - RunStartTime;
    if (enforceTimeLimit && RunTime > TotalTime) {
      /* terminate after this pass through loop */
      NStep = Step;
    }
    EpochTime = timeNow;
    TimeOfDay = StartHour + ElapsedTime / 3600.0;
    DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
    PageTimeStamp = makeTimeStamp(timeNow);
    if (verbose) {
      printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
      fflush(stdout);
    }
    if (!SDDS_StartPage(&outTable, rootnames) ||
        !SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                            "Step", Step, "Time", EpochTime, "TimeOfDay", (float)TimeOfDay,
                            "DayOfMonth", (float)DayOfMonth, "PageTimeStamp", PageTimeStamp,
                            "CAerrors", CAerrors, NULL) ||
        (firstPage &&
         !SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                             "StartTime", StartTime, "StartJulianDay", (short)StartJulianDay,
                             "TimeStamp", TimeStamp, "YearStartTime", computeYearStartTime(StartTime),
                             "StartDayOfMonth", (short)StartDay, "StartHour", (float)StartHour,
                             "StartYear", (short)StartYear, "StartMonth", (short)StartMonth, NULL)) ||
        !SetCommentParameters(&outTable, commentParameter, commentText, comments))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    for (i = 0; i < scalars; i++) {
      if (!SDDS_SetParametersFromDoubles(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                         scalarName[i], scalarData[i], NULL))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, index, rootnames, "Index") ||
        !SDDS_SetColumn(&outTable, SDDS_BY_NAME, rootname, rootnames, "Rootname"))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    for (i = 0; i < suffixes; i++)
      if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_BY_NAME,
                                     doubleValue + i * rootnames, rootnames, suffix[i])) {
        fprintf(stderr, "error: unable to set values for column %s\nknown columns are:", suffix[i]);
        for (j = 0; j < outTable.layout.n_columns; j++)
          fprintf(stderr, "  %s\n", outTable.layout.column_definition[j].name);
        SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    if (!SDDS_WriteTable(&outTable))
      SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    timeToRead = getTimeInSecs() - timeToRead;
  }
  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stdout, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }
  if (readbackName) {
    for (i = 0; i < readbackNames; i++)
      if (readbackName[i])
        free(readbackName[i]);
    free(readbackName);
  }
  if (scalarName) {
    for (i = 0; i < scalars; i++)
      if (scalarName[i])
        free(scalarName[i]);
    free(scalarName);
  }
  if (readMessage) {
    for (i = 0; i < scalars; i++)
      if (readMessage[i])
        free(readMessage[i]);
    free(readMessage);
  }
  if (list) {
    for (i = 0; i < rows; i++)
      free(list[i]);
    free(list);
  }
  if (TimeStamp)
    free(TimeStamp);
  if (lastDoubleValue)
    free(lastDoubleValue);
  if (doubleValue)
    free(doubleValue);
  if (outputColumn) {
    if (outputColumn[0])
      free(outputColumn[0]);
    if (outputColumn[1])
      free(outputColumn[1]);
    free(outputColumn);
  }
  if (index)
    free(index);
  if (rootname)
    free(rootname);
  if (readbackCHID)
    free(readbackCHID);
  if (scalarCHID)
    free(scalarCHID);
  if (readbackUnits)
    free(readbackUnits);
  if (scalarUnits)
    free(scalarUnits);
  if (commentParameter) {
    free(commentParameter);
  }
  if (commentText) {
    free(commentText);
  }
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return 0;
}

long doubleArraysIdentical(double *d1, double *d2, long n) {
  while (n--)
    if (d1[n] != d2[n])
      return 0;
  return 1;
}

long doubleArrayDifferences(double *d1, double *d2, long n) {
  long diffs = 0;
  while (n--)
    if (d1[n] != d2[n])
      diffs++;
  return diffs;
}

void copyDoubleArray(double *target, double *source, long n) {
  while (n--)
    target[n] = source[n];
}

void writePVlist(char *file, char **name, char **units, long count, long divisor) {
  long i, iControlType, iControlName, iControlUnits;
  SDDS_TABLE outTable;

  iControlType = iControlName = iControlUnits = 0;

  if (!SDDS_InitializeOutput(&outTable, SDDS_ASCII, 1, "PV list from sddsvmonitor", NULL, file) ||
      0 > (iControlName = SDDS_DefineColumn(&outTable, "ControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (iControlType = SDDS_DefineColumn(&outTable, "ControlType", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      0 > (iControlUnits = SDDS_DefineColumn(&outTable, "ControlUnits", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) ||
      !SDDS_WriteLayout(&outTable) || !SDDS_StartTable(&outTable, count))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_EnableFSync(&outTable);
  for (i = 0; i < count; i++)
    if (!SDDS_SetRowValues(&outTable, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE,
                           i,
                           iControlName, name[i],
                           iControlType, "pv",
                           iControlUnits, units[i / divisor] ? units[i / divisor] : "",
                           -1))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteTable(&outTable) || !SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

long readSuffixFile(char *suffixFile, char ***suffix) {
  SDDS_TABLE inTable;
  long suffixes, code;

  suffixes = code = 0;

  if (!SDDS_InitializeInput(&inTable, suffixFile) || (code = SDDS_ReadTable(&inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (code == 0 || (suffixes = SDDS_CountRowsOfInterest(&inTable)) < 1)
    bomb("no data in measurement suffix file", NULL);
  if (!((*suffix) = SDDS_GetColumn(&inTable, "Suffix"))) {
    fprintf(stderr, "error: required column \"Suffix\" not found in file %s\n", suffixFile);
    exit(1);
  }
  if (SDDS_ReadTable(&inTable) > 1)
    fprintf(stderr, "warning: file %s has multiple pages--only the first is used!\n",
            suffixFile);
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return suffixes;
}

long readRootnameFile(char *rootnameFile, char ***rootname) {
  SDDS_TABLE inTable;
  long rootnames, code;

  rootnames = code = 0;

  if (!SDDS_InitializeInput(&inTable, rootnameFile) || (code = SDDS_ReadTable(&inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (code == 0 || (rootnames = SDDS_CountRowsOfInterest(&inTable)) < 1)
    bomb("no data in measurement rootname file", NULL);
  if (!((*rootname) = SDDS_GetColumn(&inTable, "Rootname"))) {
    fprintf(stderr, "error: required column \"Rootname\" not found in file %s\n", rootnameFile);
    exit(1);
  }
  if (SDDS_ReadTable(&inTable) > 1)
    fprintf(stderr, "warning: file %s has multiple pages--only the first is used!\n",
            rootnameFile);
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return rootnames;
}
