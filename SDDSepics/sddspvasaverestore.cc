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
#include <ctime>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif

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
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSepics.h"


/*#include <chandata.h>*/
#include <signal.h>
#ifdef _WIN32
#  include <windows.h>
#  include <process.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif
#include <alarm.h>
#if defined(vxWorks)
#  include <taskLib.h>
#  include <taskVarLib.h>
#  include <taskHookLib.h>
#  include <sysLib.h>
#endif

#if defined(DBAccess)
#  include "dbDefs.h"
#  include "dbAccess.h"
#  include "errlog.h"
#  include "link.h"
#else
#  include <cadef.h>
#endif

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

#define CLO_DAEMON 0
#define CLO_SEMAPHORE 1
#define CLO_DAILYFILES 2
#define CLO_VERBOSE 3
#define CLO_PVASAVE 4
#define CLO_PVARESTORE 5
#define CLO_PENDIOTIME 6
#define CLO_LOGFILE 7
#define CLO_RUNCONTROLPV 8
#define CLO_RUNCONTROLDESC 9
#define CLO_PIDFILE 10
#define CLO_TRIGGERPV 11
#define CLO_INTERVAL 12
#define CLO_UNIQUE 13
#define CLO_PIPE 14
#define CLO_DESCRIPTION 15
#define CLO_OUTPUTFILEPV 16
#define CLO_WAVEFORM 17
#define CLO_NUMERICAL 18
#define CLO_INPUTFILEPV 19
#define COMMANDLINE_OPTIONS 20

#define ONE_WAVEFORM_FILE 0x0001UL
#define SAVE_WAVEFORM_FILE 0x0002UL
#define FULL_WAVEFORM_FILENAME 0x0004UL

typedef struct
{
  short isEnumPV;
  long length; /*length=1. means scalar pv */
  char charValue;
  long dataType;
  char **stringValue;
  void *waveformData;
  /* used with channel access routines to give index via callback: */
  long usrValue, flag, *count;
} CHANNEL_INFO;

typedef struct
{
  char **ControlName;
  char **Provider, **ExpectFieldType;
  char *ExpectNumeric;
  int32_t *ExpectElements;
  long variables, validCount; /*variables is the total number of pv, validCount is the number of valid pvs */
  long *rowIndex;             /* rowIndex is the row position of PV in the input file */
  char **IndirectName;        /*same as ControlName for vectors, or "-" for scalar pvs */
  char **ValueString;         /*the value of pvs are saved as string, input/output column is ValueString */
  char *CAError;              /*y or n for CAerror, the corresponding column name is CAError */
  CHANNEL_INFO *channelInfo;
  short *newFlag; /*indicates if the PV is new or old */
  short *valid;   /*indicates if the PV is still valid, if not, do nothing about it */
  double pendIOTime;
  short verbose, pvasave, pvarestore, numerical, verify;
  FILE *logFp;                          /*file pointer for logfile to log the print out infomation*/
  char *logFile;                        /*logfile to log the print out infomation*/
  char *triggerPV, *triggerPV_provider; /*monitor pv to turn on/off sddspvasaverestore */
  double interval;
  short unique;
  char *inputFilePV, *inputFilePV_provider;   /*a char waveform pv which contains the input filename */
  char *descriptPV, *descriptPV_provider;     /*a char waveform pv which contains the description for SnapshotDescription parameter */
  char *description;                          /* write to output file SnapshotDescription parameter */
  char *outputFilePV, *outputFilePV_provider; /*store the output file name*/
  /*variables for login id, group id ,etc. */
  char userID[256];
  long effective_uid, group_id;
  char *waveformRootname, *waveformExtension; /*rootname for waveform pv file for restoring option, in case it is different from the input file name, default is the SnapshotFilename par in the input file*/
  char *waveformDir;                          /* directory of the waveform file for restoring option, default is [pwd] */
  double *doubleValue;                        /*for keep the value of enumerical pvs if -numerical option is provided. */
  short waveformRootGiven, oneWaveformFile, hasWaveform, saveWaveformFile, fullname;
  //long waveformDataType;
  double ca_connect_time; /*CA connection time */
} CONTROL_NAME;

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PVparam, *DescParam;
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
#  if !defined(vxWorks)
  RUNCONTROL_INFO rcInfo;
#  endif
} RUNCONTROL_PARAM;

int runControlPingWhileSleep(double sleepTime, CONTROL_NAME *control_name);
RUNCONTROL_PARAM rcParam; /* need to be global because some quantities are used
                             in signal and interrupt handlers */
#endif

static int SignalEventArrived, NewPVs;
volatile int sigint = 0;

void SignalEventHandler(int i);
void InitializeData(CONTROL_NAME *control_name);
long UpdateDataInfo(CONTROL_NAME *control_name, PVA_OVERALL *pva, long variables, char **ControlName,
                    char **Provider, char **ExpectFieldType, char *ExpectNumeric, int32_t *ExpectElements,
                    char **ValueString, char **IndirectName, char *inputFile);
long GetWaveformValueFromFile(char *directory, char *rootname, char *waveformPV, CHANNEL_INFO *channelInfo);
long GetWaveformValuesFromOneFile(CONTROL_NAME *control_name);
long ReadInputAndMakeCAConnections(CONTROL_NAME *control_name, PVA_OVERALL *pva, char *inputFile, char *outputFile, SDDS_DATASET *outTable, long firstTime);
long SetupPVAConnection(CONTROL_NAME *control_name, PVA_OVERALL *pva);
long SetupCAConnection(CONTROL_NAME *control_name);
void FreeDataMemory(CONTROL_NAME *control_name);
void sddspvasaverestore(CONTROL_NAME *control_name, PVA_OVERALL *pva, PVA_OVERALL *pvaDescriptPV, PVA_OVERALL *pvaInputFilePV, PVA_OVERALL *pvaOutputFilePV, PVA_OVERALL *pvaTriggerPV, char **inputFile, char *outputRoot,
                        long dailyFiles, char *semaphoreFile, long daemon);

long ReadDataAndSave(CONTROL_NAME *control_name, PVA_OVERALL *pva, PVA_OVERALL *pvaDescriptPV, SDDS_DATASET *outTable, char *inputFile, char *outputFile, long firstTime, long daemon);
long InputfileModified(char *inputFile, char *prevInput, struct stat *input_stat);
long WriteVectorFile(char *waveformPV, char *filename, void *waveformData, long waveformLength,
                     long waveformDataType, char *TimeStamp, double RunStartTime);
long WriteOneVectorFile(CONTROL_NAME *control_name, char *outputFile, char *TimeStamp, double RunStartTime);
long SetPVValue(CONTROL_NAME *control_name, PVA_OVERALL *pva);
long WriteOutputLayout(SDDS_DATASET *outTable, SDDS_TABLE *inTable);
void GetUserInformation(CONTROL_NAME *control_name);
long SDDS_UnsetDupicateRows(SDDS_TABLE *inSet);
void connection_callback(struct connection_handler_args event);
char *read_char_waveform(char **stringval, PVA_OVERALL *pva);
long setup_other_caconnections(CONTROL_NAME *control_name, PVA_OVERALL *pvaDescriptPV, PVA_OVERALL *pvaTriggerPV, PVA_OVERALL *pvaOutputFilePV, PVA_OVERALL *pvaInputFilePV);
char *getWaveformFileName(CONTROL_NAME *control_name, char *outputFile, char *waveformPV);

char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"daemon",
  (char *)"semaphore",
  (char *)"dailyFiles",
  (char *)"verbose",
  (char *)"save",
  (char *)"restore",
  (char *)"pendIOTime",
  (char *)"logFile",
  (char *)"runControlPV",
  (char *)"runControlDescription",
  (char *)"pidFile",
  (char *)"triggerPV",
  (char *)"interval",
  (char *)"unique",
  (char *)"pipe",
  (char *)"description",
  (char *)"outputFilePV",
  (char *)"waveform",
  (char *)"numerical",
  (char *)"inputFilePV",
};
char *USAGE1 = (char *)"sddspvasaverestore <inputfile> <outputRoot> [-pipe=[input|output]] \n\
  [-save] [-restore[=verify]] \n\
  [-daemon] \n\
    [-inputFilePV=<pvname>,<provider>] [-outputFilePV=<pvname>,<provider>] \n\
    [-triggerPV=<string>,<provider>] \n\
    [-description=<string>|pv=<pvname>,provider=<provider>] \n\
  [-interval=<seconds>] [-pendIOTime=<seconds>]\n\
  [-dailyFiles] [-semaphore=<filename>] [-logFile=<filename>] \n\
  [-unique] [-pidFile=<pidFile>] [-verbose] \n\
  [-waveform=[rootname=<string>][,directory=<string>][,extension=<string>][,pingTimeout=<seconds>][,onefile][,saveWaveformFile][,fullname]] \n\
  [-numerical] \n\
  [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>,pingInterval=<value>] \n\
  [-runControlDescription={string=<string>|parameter=<string>}] \n\
inputFile      inputFile name (SDDS file). \n\
outputRoot     output file or root if -dailyFiles option is specified. \n\
-verbose       print out messages. \n\
-dailyFiles    output file name is attached by current date. \n\
-semaphore     specify the flag file for indicating CA connection completence. \n\
               the current outputFile name is written to semaphore file. \n";
char *USAGE2 = (char *)"-daemon        run in daemon mode, that is, stay in background and start running whenever \n\
               changes in the input detected or signal event arrived until terminating \n\
               signal received. \n\
-save          read the values of process variables given in the input file and write \n\
               to the output file. \n\
-restore       set the values of process variables given in the input file. \n\
               -save and -restore can not be given in the same time. \n\
               if -restore option is provide, the outputRoot is not needed.\n\
               If verify is requested, then it will read the values from the PVs\n\
               after they are set to verify they values were changed.\n\
-logFile       file for logging the printout information. \n\
-pendIOTime    specifies maximum time to wait for connections and\n\
               return values.  Default is 30.0s \n\
-pidFile       provide the file to write the PID into.\n\
-interval      when in daemon mode it is the interval (in seconds) of checking monitor pv or \n\
               the sleeping time if no signal handling arrived. \n\
-triggerPV     a monitor pv to turn on/off sddspvasaverestore. When the pv changed from 0 to 1, \n\
               sddssave make a save and change the pv back to 0. \n\
-outputFilePV  a charachter-waveform pv to store the output file name with full path. \n\
-inputFilePV   a charachter-waveform pv which has the input file name with full path. \n\
               it overwrites the inputFile from command line, for example, if inputFilePV is \n\
               provided, the inputFile will be obtained from this PV. If inputFile is also \n\
               given from from command line, but outputRoot is not provided, then the value of \n\
               inputFile from the command line is given to outputRoot. \n\
-unique        remove all duplicates but the first occurrence of each PV from the input file \n\
               to prevent channel access errors when setting pv values. \n";
char *USAGE3 = (char *)"-pipe          input/output data to the pipe.\n\
               -dailyOption is ignored if -pipe=out or -pipe option is present.\n\
-description   specify the string value SnapshotDescription parameter of output file. \n\
               or specify the character waveform pv name, whose value is written to \n\
               the SnapshotDescription parameter of the output file.\n\
-runControlPV  specifies the runControl PV record.\n\
-runControlDescription\n\
               specifies a string parameter whose value is a runControl PV description \n\
               record.\n\
-numerical     return a numerical string value rather than a string for enumerated\n\
               types.\n\
-waveform      provide the waveform rootname and directory for waveform PV's restore and save. \n\
               By default, the directory is pwd, \n\
               the rootname is the SnapshotFilename parameter in the input file. \n\
               if there is no SnapshotFielname parameter in the input file, the rootname is \n\
               the input file name. If \"onefile\" is provided, then all waveforms are stored \n\
               in one file (<rootname>.waveform) with multiple pages, otherwise, each waveform \n\
               is stored in one file with name of <rootname>.<waveform pv name>. \n\
               If saveWaveformFile is provided, the ValueString for waveform pv will the file name of \n\
               the waveform file, instead of \"WaveformPV\". If saveWaveformFile and full name provided, \n\
               the full filename will be recorded, if full name not provided, only the \n\
               file root will be recored. \n\n\
program by Hairong Shang and Robert Soliday, ANL\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

/*****************************************************************************
 *
 * Main routine 
 *
 *	Process the command line args
 *	Read the PV-related information from the input files
 *	Connect to the named PVs
 *	Figure out the data types of all the PVs
 *	Read the value of all the PVs
 *	Write the data to the oputput file(s)
 *	Disconnect from the PVs
 *
 *****************************************************************************/

int main(int argc, char **argv) {
#ifdef USE_RUNCONTROL
  unsigned long dummyFlags;
#endif
  SCANNED_ARG *s_arg;
  long i_arg, pid, tmpfile_used = 0;
  CONTROL_NAME *control_name = NULL;
  long dailyFiles = 0;
  long daemon = 0;
  char *semaphoreFile, *inputFile, *outputRoot, *pidFile, *tmpstr;
  FILE *fp = NULL;
  unsigned long pipeFlags = 0;
  PVA_OVERALL pva, pvaDescriptPV, pvaTriggerPV, pvaOutputFilePV, pvaInputFilePV;

  pvaDescriptPV.numPVs = 0;
  pvaTriggerPV.numPVs = 0;
  pvaOutputFilePV.numPVs = 0;
  pvaInputFilePV.numPVs = 0;
  semaphoreFile = NULL;
  inputFile = outputRoot = pidFile = NULL;
  control_name = (CONTROL_NAME *)calloc(sizeof(*control_name), 1);
  /*initialize rcParam */
#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = rcParam.PVparam = rcParam.DescParam = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10.0;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  SDDS_RegisterProgramName("sddspvasaverestore");
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    return (1);
  }
  /*initialize data */
  InitializeData(control_name);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_NUMERICAL:
        control_name->numerical = 1;
        break;
      case CLO_UNIQUE:
        control_name->unique = 1;
        break;
      case CLO_DESCRIPTION:
        if ((s_arg[i_arg].n_items != 2) && (s_arg[i_arg].n_items != 3)) {
          fprintf(stderr, "invalid -description syntax.\n");
          return (1);
        }
        SDDS_CopyString(&tmpstr, s_arg[i_arg].list[1]);
        str_tolower(tmpstr);
        if ((strncmp("pv", tmpstr, 2) != 0))
          SDDS_CopyString(&control_name->description, s_arg[i_arg].list[1]);
        else {
          s_arg[i_arg].n_items -= 1;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "pv", SDDS_STRING, &control_name->descriptPV, 1, 0,
                            "provider", SDDS_STRING, &control_name->descriptPV_provider, 1, 0,
                            NULL) ||
              (!control_name->descriptPV) ||
              (!control_name->descriptPV_provider)) {
            fprintf(stderr, "invalid -description syntax.\n");
            return (1);
          }
          s_arg[i_arg].n_items += 1;
        }
        free(tmpstr);
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags)) {
          fprintf(stderr, "invalid -pipe syntax\n");
          return (1);
        }
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&(control_name->interval), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -interval\n");
          return (1);
        }
        break;
      case CLO_PIDFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "wrong number of items for -pidFile\n");
          return (1);
        }
        SDDS_CopyString(&pidFile, s_arg[i_arg].list[1]);
        break;
      case CLO_TRIGGERPV:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "wrong number of items for -triggerPV\n");
          return (1);
        }
        SDDS_CopyString(&control_name->triggerPV, s_arg[i_arg].list[1]);
        control_name->triggerPV_provider = s_arg[i_arg].list[2];
        break;
      case CLO_OUTPUTFILEPV:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "wrong number of items for -outputFilePV\n");
          return (1);
        }
        SDDS_CopyString(&control_name->outputFilePV, s_arg[i_arg].list[1]);
        control_name->outputFilePV_provider = s_arg[i_arg].list[2];
        break;
      case CLO_INPUTFILEPV:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "wrong number of items for -inputFilePV\n");
          return (1);
        }
        SDDS_CopyString(&control_name->inputFilePV, s_arg[i_arg].list[1]);
        control_name->inputFilePV_provider = s_arg[i_arg].list[2];
        break;
      case CLO_DAILYFILES:
        dailyFiles = 1;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&(control_name->pendIOTime), s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no value given for option -pendIOTime\n");
          return (1);
        }
        break;
      case CLO_VERBOSE:
        control_name->verbose = 1;
        break;
      case CLO_PVARESTORE:
        if (s_arg[i_arg].n_items == 1) {
          control_name->pvarestore = 1;
        } else if (s_arg[i_arg].n_items == 2) {
          char *verifyMode[1] = {(char *)"verify"};
          control_name->pvarestore = 1;
          if (match_string(s_arg[i_arg].list[1], verifyMode, 1, 0) == 0) {
            control_name->verify = 1;
          } else {
            fprintf(stderr, "unknown option for -restore\n");
            return (1);
          }
        } else {
          fprintf(stderr, "wrong number of items for -restore\n");
          return (1);
        }
        break;
      case CLO_PVASAVE:
        control_name->pvasave = 1;
        break;
      case CLO_DAEMON:
#if defined(_WIN32)
        fprintf(stderr, "-daemon is not available on Windows\n");
        return (1);
#endif
        daemon = 1;
        break;
      case CLO_SEMAPHORE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Invalid -semaphore syntax.\n");
          return (1);
        }
        semaphoreFile = s_arg[i_arg].list[1];
        break;
      case CLO_LOGFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "Invalid -semaphore syntax.\n");
          return (1);
        }
        control_name->logFile = s_arg[i_arg].list[1];
        if (!(control_name->logFp = fopen(s_arg[i_arg].list[1], "w"))) {
          fprintf(stderr, "Can not open log file %s for writing.\n", s_arg[i_arg].list[1]);
          return (1);
        }
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        s_arg[i_arg].n_items -= 1;
        if ((s_arg[i_arg].n_items < 0) ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          "pingInterval", SDDS_DOUBLE, &rcParam.pingInterval, 1, 0,
                          NULL) ||
            (!rcParam.PVparam && !rcParam.PV) ||
            (rcParam.PVparam && rcParam.PV)) {
          fprintf(stderr, "bad -runControlPV syntax\n");
          return (1);
        }
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        s_arg[i_arg].n_items -= 1;
        if ((s_arg[i_arg].n_items < 0) ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &rcParam.DescParam, 1, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.DescParam && !rcParam.Desc) ||
            (rcParam.DescParam && rcParam.Desc)) {
          fprintf(stderr, "bad -runControlDesc syntax\n");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      case CLO_WAVEFORM:
        s_arg[i_arg].n_items -= 1;
        dummyFlags = 0;
        if ((s_arg[i_arg].n_items < 0) ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "rootname", SDDS_STRING, &control_name->waveformRootname, 1, 0,
                          "extension", SDDS_STRING, &control_name->waveformExtension, 1, 0,
                          "directory", SDDS_STRING, &control_name->waveformDir, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          "onefile", -1, NULL, 0, ONE_WAVEFORM_FILE,
                          "savewaveformFile", -1, NULL, 0, SAVE_WAVEFORM_FILE,
                          "fullname", -1, NULL, 0, FULL_WAVEFORM_FILENAME,
                          NULL)) {
          fprintf(stderr, "Invalid -waveform option (sddspvasaverestore).\n");
          return (1);
        }
        if (dummyFlags & ONE_WAVEFORM_FILE)
          control_name->oneWaveformFile = 1;
        if (dummyFlags & SAVE_WAVEFORM_FILE)
          control_name->saveWaveformFile = 1;
        if (dummyFlags & FULL_WAVEFORM_FILENAME)
          control_name->fullname = 1;
        if (!control_name->waveformExtension)
          SDDS_CopyString(&(control_name->waveformExtension), ".waveform");
        if (control_name->waveformRootname)
          control_name->waveformRootGiven = 1;
        s_arg[i_arg].n_items += 1;
        break;
      default:
        fprintf(stderr, "unrecognized option: %s (sddspvasaverestore)\n",
                s_arg[i_arg].list[0]);
        return (1);
        break;
      }
    } else {
      if (!inputFile)
        SDDS_CopyString(&inputFile, s_arg[i_arg].list[0]);
      else if (!outputRoot)
        SDDS_CopyString(&outputRoot, s_arg[i_arg].list[0]);
      else {
        fprintf(stderr, "too many filenames given\n");
        return (1);
      }
    }
  }
  if (setup_other_caconnections(control_name, &pvaDescriptPV, &pvaTriggerPV, &pvaOutputFilePV, &pvaInputFilePV) != 0) {
    return (1);
  }
  if (control_name->inputFilePV) {
    if (inputFile && !outputRoot)
      SDDS_CopyString(&outputRoot, inputFile);
    read_char_waveform(&inputFile, &pvaInputFilePV);
  }
  if (control_name->pvasave) {
    processFilenames((char *)"sddspvasaverestore", &inputFile, &outputRoot, pipeFlags, 1, &tmpfile_used);
    if (!outputRoot && !(control_name->outputFilePV) && !(pipeFlags & USE_STDOUT)) {
      fprintf(stderr, "Neither output file nor output file pv or pipe=out are given for -save!\n");
      return (1);
    }
  }
  if (control_name->pvarestore) {
    if (inputFile && pipeFlags & USE_STDIN) {
      fprintf(stderr, "Too many file names!\n");
      return (1);
    }
    if (pipeFlags & USE_STDOUT || outputRoot) {
      fprintf(stderr, "Invalid output argmument for restore!\n");
      return (1);
    }
  }

  if (pipeFlags) {
    if (control_name->verbose && daemon)
      fprintf(stderr, "Warning, daemon mode is ignored with -pipe option!\n");
    daemon = 0;
  }
  if (pipeFlags & USE_STDOUT)
    dailyFiles = 0;
  if (control_name->pvasave && control_name->pvarestore) {
    fprintf(stderr, "-save and -restore can not exist in the same time (sddspvasaverestore).\n");
    return (1);
  }
  if (!control_name->pvasave && !control_name->pvarestore) {
    fprintf(stderr, "one of -save and -restore has to be given (sddspvasaverestore).\n");
    return (1);
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV && !rcParam.Desc) {
    fprintf(stderr, "the runControlDescription should not be null!");
    return (1);
  }
#endif
#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    if (control_name->pendIOTime * 1000 > rcParam.pingTimeout) {
      /* runcontrol will time out if CA connection
             takes longer than its timeout, so the runcontrol ping timeout should be longer
             than the pendIOTime*/
      rcParam.pingTimeout = (control_name->pendIOTime + 10) * 1000;
    }
    rcParam.handle[0] = (char)0;
    rcParam.Desc = (char *)realloc(rcParam.Desc, 41 * sizeof(char));
    rcParam.PV = (char *)realloc(rcParam.PV, 41 * sizeof(char));
    rcParam.status = runControlInit(rcParam.PV,
                                    rcParam.Desc,
                                    rcParam.pingTimeout,
                                    rcParam.handle,
                                    &(rcParam.rcInfo),
                                    30.0);
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n");
        fprintf(stderr, "\tor has the same description string,\n");
        fprintf(stderr, "\tor the runcontrol record has not been cleared from previous use.\n");
      }
      return (1);
    }
  }
#endif
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

  pid = getpid();
  if (pidFile) {
    if (!(fp = fopen(pidFile, "w")))
      bomb((char *)"unable to open PID file", NULL);
    fprintf(fp, "%ld\n", pid);
    fclose(fp);
  }
  GetUserInformation(control_name);
  /*read input and make CA/PVA connection */
  sddspvasaverestore(control_name, &pva, &pvaDescriptPV, &pvaInputFilePV, &pvaOutputFilePV, &pvaTriggerPV, &inputFile, outputRoot, dailyFiles, semaphoreFile, daemon);
#ifdef DEBUG
  fprintf(stderr, "Returned from sddspvasaverestore\n");
#endif
  if (pvaDescriptPV.numPVs)
    freePVA(&pvaDescriptPV);
  if (pvaInputFilePV.numPVs)
    freePVA(&pvaInputFilePV);
  if (pvaOutputFilePV.numPVs)
    freePVA(&pvaOutputFilePV);
  if (pvaTriggerPV.numPVs)
    freePVA(&pvaTriggerPV);
  freePVA(&pva);

  if (control_name->logFp)
    fclose(control_name->logFp);
#ifdef DEBUG
  fprintf(stderr, "Closed log file\n");
#endif
  FreeDataMemory(control_name);
#ifdef DEBUG
  fprintf(stderr, "FreeDataMemory returned\n");
#endif
  free(control_name);
#ifdef DEBUG
  fprintf(stderr, "Free'd control_name\n");
#endif
  free_scanargs(&s_arg, argc);
#ifdef DEBUG
  fprintf(stderr, "free_scanargs returned\n");
#endif
  if (tmpfile_used && !replaceFileAndBackUp(inputFile, outputRoot))
    exit(1);
  if (inputFile)
    free(inputFile);
  if (outputRoot)
    free(outputRoot);
#ifdef DEBUG
  fprintf(stderr, "Free'd filenames\n");
#endif

#ifdef DEBUG
  fprintf(stderr, "returning from main routine\n");
#endif
  return 0;
}

#if !defined(_WIN32)
void SignalEventHandler(int i) {
  signal(SIGUSR1, SignalEventHandler);
  SignalEventArrived = 1;
}
#endif

void InitializeData(CONTROL_NAME *control_name) {
  if (!control_name) {
    if (!(control_name = (CONTROL_NAME *)malloc(sizeof(*control_name))))
      SDDS_Bomb((char *)"Memory allocation error for control_name!");
  }
  control_name->doubleValue = NULL;
  control_name->numerical = 0;
  control_name->waveformRootname = NULL;
  control_name->waveformExtension = NULL;
  control_name->oneWaveformFile = 0;
  control_name->saveWaveformFile = 0;
  control_name->fullname = 0;
  control_name->hasWaveform = 0;
  control_name->waveformDir = NULL;
  control_name->outputFilePV = NULL;
  control_name->description = NULL;
  control_name->unique = 0;
  control_name->interval = 0.5;
  control_name->ControlName = NULL;
  control_name->ValueString = NULL;
  control_name->channelInfo = NULL;
  control_name->CAError = NULL;
  control_name->IndirectName = NULL;
  control_name->logFp = NULL;
  control_name->logFile = NULL;
  control_name->variables = 0;
  control_name->newFlag = control_name->valid = NULL;
  control_name->verbose = control_name->pvasave = control_name->pvarestore = control_name->verify = 0;
  control_name->pendIOTime = 100.0; /*this is to prevent ioc connecting error when the input files is big*/
  control_name->triggerPV = NULL;
  control_name->descriptPV = NULL;
  control_name->descriptPV_provider = NULL;
  control_name->inputFilePV = NULL;
  control_name->waveformRootGiven = 0;
  control_name->rowIndex = NULL;
  control_name->ca_connect_time = 0;
}

long UpdateDataInfo(CONTROL_NAME *control_name, PVA_OVERALL *pva, long variables, char **ControlName,
                    char **Provider, char **ExpectFieldType, char *ExpectNumeric, int32_t *ExpectElements,
                    char **ValueString, char **IndirectName, char *inputFile) {
  long i;
  KEYED_EQUIVALENT **keyGroup; /*using qsort and binary search to find the existence */
  long keyGroups = 0, index, count, newpvs = 0;
  short *existed; /*indicates if the pv from input file is already contained in the data */

  if (!variables) {
    return 1;
  }
  if (!ControlName) {
    fprintf(stderr, "No controlname and IndirectName provided!");
    return 0;
  }
  if (control_name->pvarestore && !ValueString) {
    fprintf(stderr, "The ValueString column is missing or has not value in the input file, unable to restore settings!\n");
    return 0;
  }
  if (!control_name->variables) {
    /*the first time of reading input file, where the data is empty */
    control_name->variables = control_name->validCount = variables;
    control_name->ControlName = (char **)malloc(sizeof(*(control_name->ControlName)) * variables);
    control_name->Provider = (char **)malloc(sizeof(*(control_name->Provider)) * variables);
    control_name->ExpectFieldType = (char **)malloc(sizeof(*(control_name->ExpectFieldType)) * variables);
    control_name->ExpectNumeric = (char *)malloc(sizeof(*(control_name->ExpectNumeric)) * variables);
    control_name->ExpectElements = (int32_t *)malloc(sizeof(*(control_name->ExpectElements)) * variables);
    control_name->IndirectName = (char **)malloc(sizeof(*(control_name->IndirectName)) * variables);
    control_name->ValueString = (char **)malloc(sizeof(*(control_name->ValueString)) * variables);
    control_name->CAError = (char *)malloc(sizeof(*(control_name->CAError)) * variables);
    control_name->valid = (short *)malloc(sizeof(*control_name->valid) * variables);
    control_name->newFlag = (short *)malloc(sizeof(*control_name->newFlag) * variables);
    control_name->channelInfo = (CHANNEL_INFO *)malloc(sizeof(*control_name->channelInfo) * variables);
    control_name->channelInfo[0].count = (long *)malloc(sizeof(long));
    control_name->doubleValue = (double *)malloc(sizeof(double) * variables);
    control_name->rowIndex = (long *)malloc(sizeof(long) * variables);
    for (i = 0; i < variables; i++) {
      control_name->rowIndex[i] = i;
      control_name->newFlag[i] = 1;
      control_name->valid[i] = 1;
      SDDS_CopyString(&control_name->ControlName[i], ControlName[i]);
      SDDS_CopyString(&control_name->Provider[i], Provider[i]);
      SDDS_CopyString(&control_name->ExpectFieldType[i], ExpectFieldType[i]);
      control_name->ExpectNumeric[i] = ExpectNumeric[i];
      control_name->ExpectElements[i] = ExpectElements[i];
      control_name->channelInfo[i].waveformData = NULL;
      control_name->IndirectName[i] = NULL;
      control_name->channelInfo[i].stringValue = NULL;
      control_name->ValueString[i] = NULL;
    }
  } else {
    /*control_name->variables is the number of previously connected PVs
        variables is the number of PVs in the current file*/
    existed = (short *)malloc(sizeof(short) * variables);
    for (i = 0; i < variables; i++) {
      existed[i] = 0;
    }
    for (i = 0; i < control_name->variables; i++) {
      control_name->valid[i] = 0;
      control_name->newFlag[i] = 0;
    }
    /*set the valid to 0 if the pv does not exist in the new input file */
    keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, control_name->ControlName, control_name->variables);
    for (i = 0; i < variables; i++) {
      if ((index = FindMatchingKeyGroup(keyGroup, keyGroups, SDDS_STRING, ControlName + i, 0)) >= 0) {
        control_name->valid[index] = 1;
        control_name->rowIndex[index] = i;
        existed[i] = 1;
      } else
        newpvs++;
    }
    for (i = 0; i < keyGroups; i++) {
      free(keyGroup[i]->equivalent);
      free(keyGroup[i]);
    }
    free(keyGroup);
    count = control_name->variables;
    if (newpvs) {
      control_name->variables += newpvs;
      control_name->rowIndex = (long *)SDDS_Realloc(control_name->rowIndex, sizeof(*control_name->rowIndex) * control_name->variables);
      control_name->ControlName = (char **)SDDS_Realloc(control_name->ControlName, sizeof(*control_name->ControlName) * control_name->variables);
      control_name->Provider = (char **)SDDS_Realloc(control_name->Provider, sizeof(*(control_name->Provider)) * control_name->variables);
      control_name->ExpectFieldType = (char **)SDDS_Realloc(control_name->ExpectFieldType, sizeof(*(control_name->ExpectFieldType)) * control_name->variables);
      control_name->ExpectNumeric = (char *)SDDS_Realloc(control_name->ExpectNumeric, sizeof(*(control_name->ExpectNumeric)) * control_name->variables);
      control_name->ExpectElements = (int32_t *)SDDS_Realloc(control_name->ExpectElements, sizeof(*(control_name->ExpectElements)) * control_name->variables);
      control_name->ValueString = (char **)SDDS_Realloc(control_name->ValueString, sizeof(*control_name->ValueString) * control_name->variables);
      control_name->IndirectName = (char **)SDDS_Realloc(control_name->IndirectName, sizeof(*control_name->IndirectName) * control_name->variables);
      control_name->channelInfo = (CHANNEL_INFO *)SDDS_Realloc(control_name->channelInfo, sizeof(*control_name->channelInfo) * control_name->variables);
      control_name->CAError = (char *)SDDS_Realloc(control_name->CAError, sizeof(*control_name->CAError) * control_name->variables);
      control_name->newFlag = (short *)SDDS_Realloc(control_name->newFlag, sizeof(*control_name->newFlag) * control_name->variables);
      control_name->valid = (short *)SDDS_Realloc(control_name->valid, sizeof(*control_name->valid) * control_name->variables);
      control_name->doubleValue = (double *)SDDS_Realloc(control_name->doubleValue, sizeof(*control_name->doubleValue) * control_name->variables);
      for (i = 0; i < variables; i++) {
        if (existed[i])
          continue;
        /*add new pvs to the data */
        SDDS_CopyString(&control_name->ControlName[count], ControlName[i]);
        SDDS_CopyString(&control_name->Provider[count], Provider[i]);
        SDDS_CopyString(&control_name->ExpectFieldType[count], ExpectFieldType[i]);
        control_name->ExpectNumeric[count] = ExpectNumeric[i];
        control_name->ExpectElements[count] = ExpectElements[i];
        control_name->rowIndex[count] = i;
        control_name->IndirectName[count] = NULL;
        control_name->ValueString[count] = NULL;
        control_name->channelInfo[count].waveformData = NULL;
        control_name->channelInfo[count].stringValue = NULL;
        control_name->channelInfo[count].length = 1;
        control_name->valid[count] = 1;
        control_name->newFlag[count] = 1;
        count++;
      }
    }
    free(existed);
  }
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i]) {
      if (control_name->pvarestore) {
        if (control_name->IndirectName[i])
          free(control_name->IndirectName[i]);
        if (IndirectName && IndirectName[control_name->rowIndex[i]])
          SDDS_CopyString(&control_name->IndirectName[i], IndirectName[control_name->rowIndex[i]]);
        else
          SDDS_CopyString(&control_name->IndirectName[i], "-");
        if (!ValueString || !ValueString[control_name->rowIndex[i]]) {
          fprintf(stderr, "The value is not provided for restoring pv values!\n");
          return 0;
        }
        if (control_name->ValueString[i]) {
          free(control_name->ValueString[i]);
          control_name->ValueString[i] = NULL;
        }
        SDDS_CopyString(&control_name->ValueString[i], ValueString[control_name->rowIndex[i]]);
        if (strlen(control_name->IndirectName[i]) && !is_blank(control_name->IndirectName[i]) && strcmp(control_name->IndirectName[i], "-") != 0) {
          control_name->hasWaveform = 1;
          if (!control_name->oneWaveformFile) {
            if (!GetWaveformValueFromFile(control_name->waveformDir, control_name->waveformRootname,
                                          control_name->ControlName[i],
                                          &control_name->channelInfo[i])) {
              fprintf(stderr, "Error in reading waveform values from file.\n");
              exit(1);
            }
          }
        }
      } else {
        if (control_name->ValueString[i]) {
          free(control_name->ValueString[i]);
        }
        control_name->ValueString[i] = (char *)malloc(sizeof(char) * 250);
      }
    }
  }
  if (control_name->pvarestore && control_name->hasWaveform && control_name->oneWaveformFile) {
    /* read waveform values from one file */
    GetWaveformValuesFromOneFile(control_name);
  }
  /*setup pva connections */
  if (control_name->logFp)
    fprintf(control_name->logFp, "%s, Connecting to CA...\n", getHourMinuteSecond());
  SetupPVAConnection(control_name, pva);
  if (control_name->logFp)
    fprintf(control_name->logFp, "%s, Connecting to CA done\n", getHourMinuteSecond());
  return 1;
}

char *getWaveformFileName(CONTROL_NAME *control_name, char *outputFile, char *waveformPV) {
  char vectorOut[1024], *filename = NULL, *f1 = NULL, *f2 = NULL;
  if (control_name->oneWaveformFile) {
    if (control_name->waveformRootname) {
      if (control_name->waveformDir)
        sprintf(vectorOut, "%s/%s.waveformDir", control_name->waveformDir, control_name->waveformRootname);
      else
        sprintf(vectorOut, "%s.waveform", control_name->waveformRootname);
    } else if (outputFile)
      sprintf(vectorOut, "%s.waveform", outputFile);
  } else {
    if (control_name->waveformRootname) {
      if (control_name->waveformDir)
        sprintf(vectorOut, "%s/%s.%s", control_name->waveformDir,
                control_name->waveformRootname, waveformPV);
      else
        sprintf(vectorOut, "%s.%s", control_name->waveformRootname, waveformPV);
    } else if (outputFile)
      sprintf(vectorOut, "%s.%s", outputFile, waveformPV);
  }
  SDDS_CopyString(&f1, vectorOut);
  f2 = f1;
  if (!control_name->fullname) {
    char *token = NULL;
    token = strtok(f2, "/");
    while (token != NULL) {
      f2 = token;
      token = strtok(NULL, "/");
    }
  }
  SDDS_CopyString(&filename, f2);
  free(f1);
  return filename;
}

long ReadInputAndMakeCAConnections(CONTROL_NAME *control_name, PVA_OVERALL *pva, char *inputFile, char *outputFile, SDDS_DATASET *outTable, long firstTime) {
  char **ControlName, **IndirectName;
  char **ValueString;
  char **Provider, **ExpectFieldType;
  char *ExpectNumeric;
  int32_t *ExpectElements;
  SDDS_DATASET inSet;
  short valuestringExist = 0, IndirectNameExist = 0;
  long i;
  long variables, caOnly = 0;
  
  ControlName = IndirectName = ValueString = NULL;
  Provider = ExpectFieldType = NULL;
  ExpectNumeric = NULL;
  ExpectElements = NULL;

  if (!control_name) {
    fprintf(stderr, "Memory was not allocated for control_name!\n");
    return 0;
  }
  if (control_name->logFp)
    fprintf(control_name->logFp, "%s, reading from input file...\n", getHourMinuteSecond());
  if (!SDDS_InitializeInput(&inSet, inputFile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    return 0;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"ControlName", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"ControlName\" does not exist or something wrong with it in input file %s\n", inputFile);
    return 0;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"Provider", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    caOnly = 1;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"ExpectNumeric", NULL, SDDS_CHARACTER, NULL) != SDDS_CHECK_OK) {
    caOnly = 1;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"ExpectFieldType", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    caOnly = 1;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"ExpectElements", NULL, SDDS_LONG, NULL) != SDDS_CHECK_OK) {
    caOnly = 1;
  }
  if (SDDS_CheckColumn(&inSet, (char *)"IndirectName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    IndirectNameExist = 1;
  if (SDDS_CheckColumn(&inSet, (char *)"ValueString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    valuestringExist = 1;

  if (0 >= SDDS_ReadPage(&inSet)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    return 0;
  }
  if (control_name->unique && !SDDS_UnsetDupicateRows(&inSet))
    SDDS_Bomb((char *)"Error in unsetting duplicate rows of the input file");
  if (!(variables = SDDS_CountRowsOfInterest(&inSet))) {
    return 1;
  }
  if (!(ControlName = (char **)SDDS_GetColumn(&inSet, (char *)"ControlName")))
    SDDS_Bomb((char *)"No data provided in the input file.");
  if (caOnly) {
    Provider = (char **)malloc(sizeof(char *) * variables);
    ExpectFieldType = (char **)malloc(sizeof(char *) * variables);
    ExpectNumeric = (char *)malloc(sizeof(char) * variables);
    ExpectElements = (int32_t *)malloc(sizeof(int32_t) * variables);
    for (i = 0; i < variables; i++) {
      Provider[i] = (char *)malloc(sizeof(char) * 3);
      sprintf(Provider[i], "ca");
      ExpectFieldType[i] = (char *)malloc(sizeof(char) * 7);
      sprintf(ExpectFieldType[i], "scalar");
      ExpectNumeric[i] = 'y';
      ExpectElements[i] = 1;
    }
  } else {
    if (!(Provider = (char **)SDDS_GetColumn(&inSet, (char *)"Provider")) ||
        !(ExpectFieldType = (char **)SDDS_GetColumn(&inSet, (char *)"ExpectFieldType")) ||
        !(ExpectNumeric = (char *)SDDS_GetColumn(&inSet, (char *)"ExpectNumeric")) ||
        !(ExpectElements = (int32_t *)SDDS_GetColumn(&inSet, (char *)"ExpectElements")))
      SDDS_Bomb((char *)"No data provided in the input file.");
  }
  if (IndirectNameExist)
    if (!(IndirectName = (char **)SDDS_GetColumn(&inSet, (char *)"IndirectName")))
      SDDS_Bomb((char *)"Unable to get the vaue of IndirectName column in the input file");
  if (valuestringExist) {
    if (!(ValueString = (char **)SDDS_GetColumn(&inSet, (char *)"ValueString")))
      SDDS_Bomb((char *)"Unable to get the vaue of ValueString column in the input file.");
    for (i = 0; i < variables; i++) {
      if (ValueString[i][0] == '\"' && ValueString[i][strlen(ValueString[i]) - 1] == '\"') {
        strslide(ValueString[i], -1);
        ValueString[i][strlen(ValueString[i]) - 1] = '\0';
      }
    }
  }
  if (control_name->pvarestore && !valuestringExist)
    SDDS_Bomb((char *)"The ValueString column is missing in the input file, unable to restore pv values!");
  if ((control_name->pvasave) && (firstTime)) {
    if (!SDDS_InitializeCopy(outTable, &inSet, outputFile, (char *)"w"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!WriteOutputLayout(outTable, &inSet))
      SDDS_Bomb((char *)"Error in writing output layout!");
    if (!SDDS_CopyPage(outTable, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!control_name->waveformRootGiven && control_name->pvarestore) {
    if (control_name->waveformRootname)
      free(control_name->waveformRootname);
    if (!SDDS_GetParameter(&inSet, (char *)"SnapshotFilename", &control_name->waveformRootname))
      SDDS_CopyString(&control_name->waveformRootname, inputFile);
  }
  if (!SDDS_Terminate(&inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  /*update the data info from the input */
  if (!UpdateDataInfo(control_name, pva, variables, ControlName, Provider, ExpectFieldType, ExpectNumeric, ExpectElements, ValueString, IndirectName, inputFile))
    return 0;
  for (i = 0; i < variables; i++) {
    free(ControlName[i]);
    free(Provider[i]);
    free(ExpectFieldType[i]);
    if (IndirectName && IndirectName[i])
      free(IndirectName[i]);
    if (ValueString && ValueString[i])
      free(ValueString[i]);
  }
  free(ControlName);
  free(Provider);
  free(ExpectFieldType);
  free(ExpectNumeric);
  free(ExpectElements);
  if (IndirectName)
    free(IndirectName);
  if (ValueString)
    free(ValueString);

  return 1;
}

long WriteOutputLayout(SDDS_DATASET *outTable, SDDS_DATASET *inTable) {
  short indirectExist = 0, valuestringExist = 0, CAErrorExist = 0, description = 0;
  short timestamp = 0, starttime = 0, loginid = 0, effectiveid = 0, groupid = 0, snaptype = 0, timeFlag = 0;
  short request = 0, snapshot = 0, countExist = 0, LineageExist = 0;
  short elapsedTimeCAExists = 0, elapsedTimeSaveExists = 0, pendiotimeExists = 0;

  if (SDDS_CheckColumn(inTable, (char *)"IndirectName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    indirectExist = 1;
  if (SDDS_CheckColumn(inTable, (char *)"ValueString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    valuestringExist = 1;
  if (SDDS_CheckColumn(inTable, (char *)"CAError", NULL, SDDS_CHARACTER, NULL) == SDDS_CHECK_OK)
    CAErrorExist = 1;
  if (SDDS_CheckParameter(inTable, (char *)"TimeStamp", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    timestamp = 1;
  if (SDDS_CheckParameter(inTable, (char *)"StartTime", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    starttime = 1;
  if (SDDS_CheckParameter(inTable, (char *)"Time", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK) {
    long index, type;
    char *units = NULL;
    index = SDDS_GetParameterIndex(inTable, (char *)"Time");
    type = SDDS_GetParameterType(inTable, index);
    if (type != SDDS_DOUBLE) {
      if (!SDDS_ChangeParameterInformation(outTable, (char *)"type", (char *)"double",
                                           SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                           (char *)"Time")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (!SDDS_GetParameterInformation(inTable, (char *)"units", &units, SDDS_GET_BY_NAME, (char *)"Time"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!units || strcmp(units, "s")) {
      if (!SDDS_ChangeParameterInformation(outTable, (char *)"units", (char *)"s", SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, (char *)"Time")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (units)
      free(units);
    timeFlag = 1;
  }
  if (SDDS_CheckParameter(inTable, (char *)"LoginID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    loginid = 1;
  if (SDDS_CheckParameter(inTable, (char *)"EffectiveUID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    effectiveid = 1;
  if (SDDS_CheckParameter(inTable, (char *)"GroupID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    groupid = 1;
  if (SDDS_CheckParameter(inTable, (char *)"SnapType", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    snaptype = 1;
  if (SDDS_CheckParameter(inTable, (char *)"SnapshotDescription", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    description = 1;
  if (SDDS_CheckParameter(inTable, (char *)"SnapshotFilename", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    snapshot = 1;
  if (SDDS_CheckParameter(inTable, (char *)"RequestFile", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    request = 1;
  if (SDDS_CheckColumn(inTable, (char *)"Count", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK)
    countExist = 1;
  if (SDDS_CheckColumn(inTable, (char *)"Lineage", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    LineageExist = 1;
  if (SDDS_CheckParameter(inTable, (char *)"ElapsedTimeToCAConnect", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    elapsedTimeCAExists = 1;
  if (SDDS_CheckParameter(inTable, (char *)"ElapsedTimeToSave", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    elapsedTimeSaveExists = 1;
  if (SDDS_CheckParameter(inTable, (char *)"PendIOTime", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    pendiotimeExists = 1;
  if (!countExist && SDDS_DefineColumn(outTable, (char *)"Count", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!LineageExist && SDDS_DefineColumn(outTable, (char *)"Lineage", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!indirectExist && SDDS_DefineColumn(outTable, (char *)"IndirectName", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!valuestringExist && SDDS_DefineColumn(outTable, (char *)"ValueString", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!CAErrorExist && SDDS_DefineColumn(outTable, (char *)"CAError", NULL, NULL, NULL, NULL, SDDS_CHARACTER, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!timestamp && SDDS_DefineParameter(outTable, (char *)"TimeStamp", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!starttime && SDDS_DefineParameter(outTable, (char *)"StartTime", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!timeFlag && SDDS_DefineParameter(outTable, (char *)"Time", NULL, "s", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!snaptype && SDDS_DefineParameter(outTable, (char *)"SnapType", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!description && SDDS_DefineParameter(outTable, (char *)"SnapshotDescription", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!request && SDDS_DefineParameter(outTable, (char *)"RequestFile", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!snapshot && SDDS_DefineParameter(outTable, (char *)"SnapshotFilename", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!loginid && SDDS_DefineParameter(outTable, (char *)"LoginID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!effectiveid && SDDS_DefineParameter(outTable, (char *)"EffectiveUID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!groupid && SDDS_DefineParameter(outTable, (char *)"GroupID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!elapsedTimeCAExists && SDDS_DefineParameter(outTable, (char *)"ElapsedTimeToCAConnect", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!elapsedTimeSaveExists && SDDS_DefineParameter(outTable, (char *)"ElapsedTimeToSave", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!pendiotimeExists && SDDS_DefineParameter(outTable, (char *)"PendIOTime", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long SetupPVAConnection(CONTROL_NAME *control_name, PVA_OVERALL *pva) {
  long j, variables, validCount, newCount = 0, i;
  char **ControlName, **Provider;
  CHANNEL_INFO *channelInfo;
  double starttime, endtime;
  static bool firsttime = true;

  ControlName = control_name->ControlName;
  Provider = control_name->Provider;
  channelInfo = control_name->channelInfo;

  variables = control_name->variables;
  validCount = 0;

  if (!control_name->channelInfo)
    SDDS_Bomb((char *)"Memory has not been allocated for channelID.\n");
  for (j = 0; j < variables; j++) {
    if (control_name->valid[j]) {
      validCount++;
      if (control_name->newFlag[j]) {
        channelInfo[j].flag = 0;
        newCount++;
      } else {
        //In daemon mode we previously connected to this PV. We can skip trying to connect a second time.
        pva->pvaData[j].skip = false;
      }
    } else {
      //In deamon mode we previously connected to this PV but don't need it at the moment
      pva->pvaData[j].skip = true;
    }
  }
  if (!newCount)
    return 0;

  getTimeBreakdown(&starttime, NULL, NULL, NULL, NULL, NULL, NULL);
  //Allocate memory for pva structures
  if (firsttime) {
    allocPVA(pva, variables); //skip fields set to false
    firsttime = false;
  } else {
    freePVAGetReadings(pva);    //skip fields honored
    reallocPVA(pva, variables); //new skip fields set to false
  }
  //List PV names
  epics::pvData::shared_vector<std::string> names(pva->numPVs);
  epics::pvData::shared_vector<std::string> providerNames(pva->numPVs);
  for (j = 0; j < pva->numPVs; j++) {
    names[j] = ControlName[j];
    providerNames[j] = Provider[j];
  }
  pva->pvaChannelNames = freeze(names);
  pva->pvaProvider = freeze(providerNames);
  ConnectPVA(pva, control_name->pendIOTime); //skip field not used. All new PVs are connected
  if (GetPVAValues(pva) == 1)                //skip fields honored
  {
    exit(1);
  }
  firsttime = false;

  getTimeBreakdown(&endtime, NULL, NULL, NULL, NULL, NULL, NULL);
  control_name->ca_connect_time = endtime - starttime;
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->isConnected[j] == false) {
      if (control_name->verbose)
        fprintf(stderr, "Warning: problem doing search for %s\n", control_name->ControlName[j]);
      else if (control_name->logFp)
        fprintf(control_name->logFp, "Warning: problem doing search for %s\n", control_name->ControlName[j]);
      control_name->CAError[j] = 'y';
    } else {
      control_name->CAError[j] = 'n';
    }
  }

  for (i = 0; i < pva->numPVs; i++) {
    if (pva->isConnected[i] == false) {
      //If initial connection fails, need good defaults.
      control_name->channelInfo[i].isEnumPV = 0;
      if ((control_name->ExpectNumeric[i] == 'Y') || (control_name->ExpectNumeric[i] == 'y'))
        control_name->channelInfo[i].dataType = SDDS_DOUBLE;
      else
        control_name->channelInfo[i].dataType = SDDS_STRING;
      if (control_name->IndirectName[i])
        free(control_name->IndirectName[i]);
      SDDS_CopyString(&control_name->IndirectName[i], "-");
      control_name->channelInfo[i].length = control_name->ExpectElements[i];
      continue;
    }
    control_name->channelInfo[i].length = pva->pvaData[i].numGetElements;
    if (control_name->channelInfo[i].length != control_name->ExpectElements[i]) {
      if ((strcmp(control_name->Provider[i], (char *)"ca") == 0) && (control_name->ExpectElements[i] == 1)) {
        control_name->ExpectElements[i] = control_name->channelInfo[i].length;
        free(control_name->ExpectFieldType[i]);
        control_name->ExpectFieldType[i] = (char *)malloc(sizeof(char) * 12);
        sprintf(control_name->ExpectFieldType[i], "scalarArray");
      } else {
        fprintf(stderr, (char *)"Error: %s has %ld elements. Expected %d\n", control_name->ControlName[i], control_name->channelInfo[i].length, control_name->ExpectElements[i]);
        exit(1);
      }
    }
    if (pva->pvaData[i].pvEnumeratedStructure) {
      control_name->channelInfo[i].isEnumPV = 1;
    } else {
      control_name->channelInfo[i].isEnumPV = 0;
    }
    if (control_name->newFlag[i]) {
      if ((control_name->channelInfo[i].length == 1) || control_name->pvasave) {
        switch (pva->pvaData[i].scalarType) {
        case epics::pvData::pvDouble: {
          control_name->channelInfo[i].dataType = SDDS_DOUBLE;
          break;
        }
        case epics::pvData::pvFloat: {
          control_name->channelInfo[i].dataType = SDDS_FLOAT;
          break;
        }
        case epics::pvData::pvLong: {
          control_name->channelInfo[i].dataType = SDDS_LONG64;
          break;
        }
        case epics::pvData::pvULong: {
          control_name->channelInfo[i].dataType = SDDS_ULONG64;
          break;
        }
        case epics::pvData::pvInt: {
          control_name->channelInfo[i].dataType = SDDS_LONG;
          break;
        }
        case epics::pvData::pvUInt: {
          control_name->channelInfo[i].dataType = SDDS_ULONG;
          break;
        }
        case epics::pvData::pvShort: {
          control_name->channelInfo[i].dataType = SDDS_SHORT;
          break;
        }
        case epics::pvData::pvUShort: {
          control_name->channelInfo[i].dataType = SDDS_USHORT;
          break;
        }
        case epics::pvData::pvByte: {
          control_name->channelInfo[i].dataType = SDDS_SHORT;
          break;
        }
        case epics::pvData::pvUByte: {
          control_name->channelInfo[i].dataType = SDDS_USHORT;
          break;
        }
        case epics::pvData::pvString:
        case epics::pvData::pvBoolean: {
          if ((control_name->channelInfo[i].isEnumPV == 1) && (control_name->numerical == 1)) {
            control_name->channelInfo[i].dataType = SDDS_SHORT;
          } else {
            control_name->channelInfo[i].dataType = SDDS_STRING;
          }
          break;
        }
        default: {
          std::cerr << "ERROR: Need code to handle scalar type " << pva->pvaData[i].scalarType << std::endl;
          exit(1);
        }
        }
        if (control_name->pvasave) {
          if (control_name->channelInfo[i].length > 1) {
            SDDS_CopyString(&control_name->IndirectName[i], control_name->ControlName[i]);
            control_name->hasWaveform = 1;
          } else {
            SDDS_CopyString(&control_name->IndirectName[i], "-");
          }
        }
      }
    }
  }

  return 0;
}

void FreeDataMemory(CONTROL_NAME *control_name) {
  long i, j;
  if (!control_name)
    return;
  if (control_name->rowIndex)
    free(control_name->rowIndex);
  for (i = 0; i < control_name->variables; i++) {
    free(control_name->ControlName[i]);
    if (control_name->Provider[i])
      free(control_name->Provider[i]);
    if (control_name->ExpectFieldType[i])
      free(control_name->ExpectFieldType[i]);
    if (control_name->IndirectName[i])
      free(control_name->IndirectName[i]);
    if (control_name->ValueString[i])
      free(control_name->ValueString[i]);
    if (control_name->channelInfo[i].length > 1) {
      if (control_name->channelInfo[i].stringValue) {
        for (j = 0; j < control_name->channelInfo[i].length; j++)
          free(control_name->channelInfo[i].stringValue[j]);
        free(control_name->channelInfo[i].stringValue);
      }
    }
  }
  if (control_name->doubleValue)
    free(control_name->doubleValue);
  if (control_name->description)
    free(control_name->description);
  if (control_name->outputFilePV)
    free(control_name->outputFilePV);
  if (control_name->waveformRootname)
    free(control_name->waveformRootname);
  if (control_name->waveformExtension)
    free(control_name->waveformExtension);
  if (control_name->waveformDir)
    free(control_name->waveformDir);
  free(control_name->channelInfo[0].count);
  free(control_name->CAError);
  free(control_name->ControlName);
  free(control_name->Provider);
  free(control_name->ExpectFieldType);
  free(control_name->ExpectNumeric);
  free(control_name->ExpectElements);
  free(control_name->IndirectName);
  free(control_name->ValueString);
  free(control_name->newFlag);
  free(control_name->valid);
  free(control_name->channelInfo);
  if (control_name->triggerPV)
    free(control_name->triggerPV);
  if (control_name->descriptPV)
    free(control_name->descriptPV);
  if (control_name->descriptPV_provider)
    free(control_name->descriptPV_provider);
  if (control_name->inputFilePV)
    free(control_name->inputFilePV);
}

long GetWaveformValuesFromOneFile(CONTROL_NAME *control_name) {
  char vectorFile[1024];
  SDDS_TABLE inSet;
  long index, i, j;
  long waveforms = 0, waveformDataType = 0;
  long *waveformLength = NULL;
  char ***waveformStringData = NULL;
  char **waveformPV = NULL;
  CHANNEL_INFO *channelInfo;
  /* note that this waveform file assume that all contained waveforms have the same type either numeric or string */
  if (control_name->waveformDir)
    sprintf(vectorFile, "%s/%s%s", control_name->waveformDir, control_name->waveformRootname, control_name->waveformExtension);
  else
    sprintf(vectorFile, "%s%s", control_name->waveformRootname, control_name->waveformExtension);

  if (!SDDS_InitializeInput(&inSet, vectorFile)) {
    fprintf(stderr, "The waveform file %s does not exist.\n", vectorFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    exit(1);
  }
  if ((index = SDDS_GetColumnIndex(&inSet, (char *)"Waveform")) < 0) {
    fprintf(stderr, "column Waveform does not exist in waveform file %s\n", vectorFile);
    exit(1);
  }
  waveformDataType = SDDS_GetColumnType(&inSet, index);
  while (SDDS_ReadPage(&inSet) > 0) {
    if (waveforms == 0) {
      if (!(waveformStringData = (char ***)SDDS_Malloc(sizeof(char **))))
        SDDS_Bomb((char *)"memory allocation failure");
    } else {
      if (!(waveformStringData = (char ***)SDDS_Realloc(waveformStringData, sizeof(char **) * (waveforms + 1))))
        SDDS_Bomb((char *)"memory allocation failure");
    }
    if (waveforms == 0) {
      if (!(waveformPV = (char **)SDDS_Malloc(sizeof(*waveformPV))) ||
          !(waveformLength = (long *)SDDS_Malloc(sizeof(*waveformLength))))
        SDDS_Bomb((char *)"memory allocation failure");
    } else {
      if (!(waveformPV = (char **)SDDS_Realloc(waveformPV, sizeof(*waveformPV) * (waveforms + 1))) ||
          !(waveformLength = (long *)SDDS_Realloc(waveformLength, sizeof(*waveformLength) * (waveforms + 1))))
        SDDS_Bomb((char *)"memory allocation failure");
    }
    if ((waveformLength[waveforms] = SDDS_RowCount(&inSet)) <= 0)
      continue;

    if (!SDDS_GetParameter(&inSet, (char *)"WaveformPV", waveformPV + waveforms))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    if (!(waveformStringData[waveforms] = (char **)SDDS_GetColumnInString(&inSet, (char *)"Waveform")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    waveforms++;
  }
  if (!SDDS_Terminate(&inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!waveforms) {
    fprintf(stderr, "No data found in waveform file %s\n", vectorFile);
    exit(1);
  }
  for (i = 0; i < control_name->variables; i++) {
    if (strlen(control_name->IndirectName[i]) && !is_blank(control_name->IndirectName[i]) && strcmp(control_name->IndirectName[i], "-") != 0) {
      if ((index = match_string(control_name->ControlName[i], waveformPV, waveforms, EXACT_MATCH)) < 0) {
        fprintf(stderr, "waveform pv %s not found in waveform file %s.\n", control_name->ControlName[i], vectorFile);
        exit(1);
      }
      channelInfo = &(control_name->channelInfo[i]);
      if (channelInfo->stringValue) {
        for (j = 0; j < channelInfo->length; j++) {
          if (channelInfo->stringValue[j]) {
            free(channelInfo->stringValue[j]);
          }
        }
        free(channelInfo->stringValue);
      }
      channelInfo->length = waveformLength[index];
      channelInfo->dataType = waveformDataType;
      channelInfo->stringValue = (char **)malloc(sizeof(*(channelInfo->stringValue)) * channelInfo->length);
      for (j = 0; j < channelInfo->length; j++) {
        SDDS_CopyString(&(channelInfo->stringValue[j]), waveformStringData[index][j]);
      }
    }
  }
  for (i = 0; i < waveforms; i++) {
    free(waveformPV[i]);
  }
  free(waveformPV);
  for (i = 0; i < waveforms; i++) {
    for (j = 0; j < waveformLength[i]; j++) {
      free(waveformStringData[i][j]);
    }
    free(waveformStringData[i]);
  }
  free(waveformStringData);
  free(waveformLength);
  return 1;
}

long GetWaveformValueFromFile(char *directory, char *rootname, char *waveformPV, CHANNEL_INFO *channelInfo) {
  char vectorFile[1024];
  SDDS_TABLE inSet;
  long index, exist = 0;
  char *parameterName;

  channelInfo->stringValue = NULL;
  channelInfo->length = 0;
  if (!rootname || !waveformPV) {
    fprintf(stderr, "No file name provided for getting the waveform setting values\n!");
    return 0;
  }
  if (directory)
    sprintf(vectorFile, "%s/%s.%s", directory, rootname, waveformPV);
  else
    sprintf(vectorFile, "%s.%s", rootname, waveformPV);
  if (!SDDS_InitializeInput(&inSet, vectorFile)) {
    fprintf(stderr, "The waveform file %s does not exist (the waveformPV name may be wrong!).\n", vectorFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    exit(1);
  }
  while (SDDS_ReadPage(&inSet) > 0) {
    if (!SDDS_GetParameter(&inSet, (char *)"WaveformPV", &parameterName))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (strcmp(parameterName, waveformPV) != 0) {
      if (parameterName)
        free(parameterName);
      continue;
    }
    if (parameterName)
      free(parameterName);
    channelInfo->length = SDDS_CountRowsOfInterest(&inSet);
    if ((index = SDDS_GetColumnIndex(&inSet, (char *)"Waveform")) < 0) {
      fprintf(stderr, "\"Waveform\" column does not exist.!");
      exit(1);
    }
    channelInfo->dataType = SDDS_GetColumnType(&inSet, index);

    if (!(channelInfo->stringValue = (char **)SDDS_GetColumnInString(&inSet, (char *)"Waveform")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

    exist = 1;
    break;
  }
  if (!SDDS_Terminate(&inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!exist) {
    fprintf(stderr, "waveformPV does not exist in %s.\n", vectorFile);
    exit(1);
  }
  return 1;
}

void sddspvasaverestore(CONTROL_NAME *control_name, PVA_OVERALL *pva, PVA_OVERALL *pvaDescriptPV, PVA_OVERALL *pvaInputFilePV, PVA_OVERALL *pvaOutputFilePV, PVA_OVERALL *pvaTriggerPV, char **inputFile, char *outputRoot,
                        long dailyFiles, char *semaphoreFile, long daemon) {
  char *outputFile = NULL;
  char *prevInput = NULL;
  char *linkname = NULL;
  long firstTime = 1;
  struct stat input_stat;
  SDDS_DATASET outTable;

  if (semaphoreFile && fexists(semaphoreFile))
    remove(semaphoreFile);
  if (*inputFile) {
    linkname = read_last_link_to_file(*inputFile);
    if (get_file_stat(*inputFile, linkname, &input_stat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", *inputFile);
      exit(1);
    }
  }
  if (control_name->pvasave) {
    if (dailyFiles)
      outputFile = MakeSCRDailyTimeGenerationFilename(outputRoot);
    else
      outputFile = outputRoot;
  }
  if (!ReadInputAndMakeCAConnections(control_name, pva, *inputFile, outputFile, &outTable, firstTime))
    SDDS_Bomb((char *)"Error in reading input file and make ca connections!");
  if (!control_name->variables) {
    fprintf(stdout, "Warning: no pvs provided, nothing is done!\n");
    exit(0);
  }
#ifdef USE_RUNCONTROL
  /*ping after CA connection, note that runcontrol still times out if ca connection
    takes longer than its timeout, so the runcontrol ping timeout should be longer
    than the pendIOTime*/
  if (rcParam.PV) {
    if (runControlPingWhileSleep(0.1, control_name)) {
      fprintf(stderr, "Problem pinging the run control record.\n");
      exit(1);
    }
    strcpy(rcParam.message, "sddspvasaverestore started");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable tp write status message and alarm severity\n");
      exit(1);
    }
  }
#endif
#if !defined(_WIN32)
  if (daemon && !control_name->triggerPV)
    signal(SIGUSR1, SignalEventHandler);
#endif
  do {
    if (daemon) {
      /* Wait for a kill signal to let us know to take a snapshot */
      SignalEventArrived = 0;
      if (!firstTime) {
        while (SignalEventArrived == 0) {
#ifdef USE_RUNCONTROL
          if (rcParam.PV) {
            if (runControlPingWhileSleep(0.01, control_name)) {
              fprintf(stderr, "Problem pinging the run control record.\n");
              exit(1);
            }
            strcpy(rcParam.message, "sddspvasaverestore sleeping");
            rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
            if (rcParam.status != RUNCONTROL_OK) {
              fprintf(stderr, "Unable tp write status message and alarm severity\n");
              exit(1);
            }
          }
#endif
          if (control_name->triggerPV) {
            freePVAGetReadings(pvaTriggerPV);
            if (GetPVAValues(pvaTriggerPV) == 1) {
              fprintf(stderr, "Error, unable to get value of %s\n", control_name->triggerPV);
              exit(1);
            }
            if (pvaTriggerPV->numNotConnected != 0) {
              fprintf(stderr, "Error, no connection for %s\n", control_name->triggerPV);
              exit(1);
            }
            SignalEventArrived = pvaTriggerPV->pvaData[0].getData[0].values[0];
          }
          if (!SignalEventArrived) {
            usleepSystemIndependent(control_name->interval * 1000000);
          }

          if (sigint) {
            exit(1);
          }
        }
        if (semaphoreFile && fexists(semaphoreFile))
          remove(semaphoreFile);

        if (control_name->pvasave) {
          if (dailyFiles)
            outputFile = MakeSCRDailyTimeGenerationFilename(outputRoot);
          else
            outputFile = outputRoot;
        }
        /*set outputfile pv to an empty string */
        if (outputFile && control_name->outputFilePV) {
          if (PrepPut(pvaOutputFilePV, 0, outputFile) == 1) {
            fprintf(stderr, "Error1, unable to set value of %s\n", control_name->outputFilePV);
            exit(1);
          }
          if (PutPVAValues(pvaOutputFilePV) == 1) {
            fprintf(stderr, "Error1, unable to set value of %s\n", control_name->outputFilePV);
            exit(1);
          }
        }
        if (*inputFile) {
          if (prevInput)
            free(prevInput);
          prevInput = NULL;
          SDDS_CopyString(&prevInput, *inputFile);
        }
        if (control_name->inputFilePV) {
          read_char_waveform(inputFile, pvaInputFilePV);
        }
        if (strcmp(*inputFile, prevInput) != 0) {
          if (linkname)
            free(linkname);
          linkname = read_last_link_to_file(*inputFile);
          get_file_stat(*inputFile, linkname, &input_stat);
        }

        if (strcmp(*inputFile, prevInput) != 0 ||
            file_is_modified(*inputFile, &linkname, &input_stat) || control_name->pvarestore) {
          if (!ReadInputAndMakeCAConnections(control_name, pva, *inputFile, outputFile, &outTable, firstTime))
            SDDS_Bomb((char *)"Error in reading input file");
        }
      }
    }
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (runControlPingWhileSleep(0.01, control_name)) {
        fprintf(stderr, "Problem pinging the run control record.\n");
        exit(1);
      }
      strcpy(rcParam.message, "sddspvasaverestore taking data");
      rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Unable tp write status message and alarm severity\n");
        exit(1);
      }
    }
#endif
    if (control_name->logFp)
      fprintf(control_name->logFp, "%s, signal handle recieved.\n", getHourMinuteSecond());

    /*take a snapshot and save it into output file */
    if (control_name->pvasave) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, reading data ...\n", getHourMinuteSecond());
      if (!ReadDataAndSave(control_name, pva, pvaDescriptPV, &outTable, *inputFile, outputFile, firstTime, daemon))
        SDDS_Bomb((char *)"Error in reading data and saving into output file");
    }
    if (control_name->pvarestore) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, setting values ...\n", getHourMinuteSecond());
      if (!SetPVValue(control_name, pva))
        SDDS_Bomb((char *)"Error in setting pv data");
    }
    if (control_name->pvasave) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, wrote to file %s\n", getHourMinuteSecond(), outputFile);
    }
    if (control_name->pvarestore) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, setting PV values done.\n", getHourMinuteSecond());
    }

    if (semaphoreFile) {
      FILE *fp;
      if ((fp = fopen(semaphoreFile, "w"))) {
        if (outputFile)
          fprintf(fp, "%s", outputFile);
        fclose(fp);
      } else {
        fprintf(stderr, "Unable to open %s file for writing\n", semaphoreFile);
        exit(1);
      }
    }
    if (outputFile && control_name->outputFilePV) {
      if (PrepPut(pvaOutputFilePV, 0, outputFile) == 1) {
        fprintf(stderr, "Error1, unable to set value of %s\n", control_name->outputFilePV);
        exit(1);
      }
      if (PutPVAValues(pvaOutputFilePV) == 1) {
        fprintf(stderr, "Error1, unable to set value of %s\n", control_name->outputFilePV);
        exit(1);
      }
    }
    if (outputFile && dailyFiles)
      free(outputFile);
    if (control_name->logFp)
      fflush(control_name->logFp);
    if (control_name->triggerPV) {
      if (PrepPut(pvaTriggerPV, 0, (double)0) == 1) {
        fprintf(stderr, "Error, unable to set value of %s\n", control_name->triggerPV);
        exit(1);
      }
      if (PutPVAValues(pvaTriggerPV) == 1) {
        fprintf(stderr, "Error, unable to set value of %s\n", control_name->triggerPV);
        exit(1);
      }
    }
    firstTime = 0;
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (runControlPingWhileSleep(0.01, control_name)) {
        fprintf(stderr, "Problem pinging the run control record.\n");
        exit(1);
      }
      strcpy(rcParam.message, "sddspvasaverestore data taking done");
      rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Unable tp write status message and alarm severity\n");
        exit(1);
      }
    }
#endif
  } while (daemon);
  if (linkname)
    free(linkname);
  if (prevInput)
    free(prevInput);
}

long InputfileModified(char *inputFile, char *prevInput, struct stat *input_stat) {
  struct stat filestat;

  if (inputFile && prevInput && strcmp(inputFile, prevInput) != 0)
    return 1;
  filestat = *input_stat;
  if (inputFile) {
    if (stat(inputFile, input_stat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputFile);
      exit(1);
    }
    if (input_stat->st_ctime != filestat.st_ctime) {
      return 1;
    }
  }
  return 0;
}

long ReadDataAndSave(CONTROL_NAME *control_name, PVA_OVERALL *pva, PVA_OVERALL *pvaDescriptPV, SDDS_DATASET *outTable, char *inputFile, char *outputFile, long firstTime, long daemon) {
  long i, rowcount = 0;
  char vectorOut[1024];
  char *TimeStamp, groupID[256], effectID[256];
  double RunStartTime, endTime;
  CHANNEL_INFO *channelInfo;

  channelInfo = control_name->channelInfo;
  sprintf(groupID, "%ld", control_name->group_id);
  sprintf(effectID, "%ld", control_name->effective_uid);

  if (control_name->descriptPV)
    read_char_waveform(&control_name->description, pvaDescriptPV);
  control_name->hasWaveform = 0;

  if (!firstTime) {
    SDDS_DATASET inSet;
    if (!SDDS_InitializeInput(&inSet, inputFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
    if (!SDDS_InitializeCopy(outTable, &inSet, outputFile, (char *)"w"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (0 >= SDDS_ReadPage(&inSet)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
    if (!WriteOutputLayout(outTable, &inSet))
      SDDS_Bomb((char *)"Error in writing output layout!");
    if (!SDDS_CopyPage(outTable, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_Terminate(&inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  /*write values to output file */
  getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, &TimeStamp);
  if (!SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", TimeStamp, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "StartTime", RunStartTime, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Time", RunStartTime, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "LoginID", control_name->userID, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "EffectiveUID", effectID, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "GroupID", groupID, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "SnapType", "Absolute", NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "RequestFile", inputFile, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "SnapshotFilename", outputFile, NULL) ||
      !SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "SnapshotDescription", control_name->description, NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  /*read data*/
  if (daemon) {
    freePVAGetReadings(pva);
    if (GetPVAValues(pva) == 1) {
      fprintf(stderr, "Error: Problem reading PVs\n");
      exit(1);
    }
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (control_name->valid[i]) {
      if (pva->isConnected[i] == false) {
        if (control_name->verbose)
          fprintf(stderr, "Warning: no connection for %s\n", control_name->ControlName[i]);
        else if (control_name->logFp)
          fprintf(control_name->logFp, "Warning: no connection for %s\n", control_name->ControlName[i]);
        free(control_name->ValueString[i]);
        SDDS_CopyString(&control_name->ValueString[i], "_NoConnection_");
        control_name->CAError[i] = 'y';
        continue;
      }
      if (channelInfo[i].length > 1) {
        control_name->hasWaveform = 1;
        if (channelInfo[i].dataType == SDDS_STRING) {
          channelInfo[i].waveformData = pva->pvaData[i].getData[0].stringValues;
        } else {
          channelInfo[i].waveformData = pva->pvaData[i].getData[0].values;
        }
        control_name->CAError[i] = 'n';
        free(control_name->ValueString[i]);
        if (!control_name->saveWaveformFile) {
          SDDS_CopyString(&control_name->ValueString[i], "WaveformPV");
        } else {
          char *tempName = NULL;
          tempName = getWaveformFileName(control_name, outputFile, control_name->IndirectName[i]);
          SDDS_CopyString(&control_name->ValueString[i], tempName);
          if (tempName)
            free(tempName);
        }
      } else {
        if (channelInfo[i].dataType == SDDS_STRING) {
          strcpy(control_name->ValueString[i], pva->pvaData[i].getData[0].stringValues[0]);
        } else {
          control_name->doubleValue[i] = pva->pvaData[i].getData[0].values[0];
        }
        control_name->CAError[i] = 'n';
        switch (channelInfo[i].dataType) {
        case SDDS_DOUBLE:
          if (channelInfo[i].isEnumPV)
            sprintf(control_name->ValueString[i], "%.0f", control_name->doubleValue[i]);
          else
            sprintf(control_name->ValueString[i], "%.15le", control_name->doubleValue[i]);
          break;
        case SDDS_FLOAT:
          sprintf(control_name->ValueString[i], "%.6le", control_name->doubleValue[i]);
          break;
        case SDDS_LONG64:
        case SDDS_ULONG64:
        case SDDS_LONG:
        case SDDS_ULONG:
        case SDDS_SHORT:
        case SDDS_USHORT:
        case SDDS_CHARACTER:
          sprintf(control_name->ValueString[i], "%.0f", control_name->doubleValue[i]);
          break;
        case SDDS_STRING:
          if (strchr(control_name->ValueString[i], ' ') != NULL) {
            /*add quote before and after string values, to make the output results be the same
                        as burtrb (otherwise, it is not necessary) */
            strslide(control_name->ValueString[i], 1);
            control_name->ValueString[i][0] = '\"';
            strcat(control_name->ValueString[i], "\"");
          }
          break;
        default:
          fprintf(stderr, "Error: invalid datatype of PV %s for reading values\n", control_name->ControlName[i]);
          exit(1);
          break;
        }
      }
    }
  }

  rowcount = 0;
  getTimeBreakdown(&endTime, NULL, NULL, NULL, NULL, NULL, NULL);
  if (!SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "ElapsedTimeToSave", endTime - RunStartTime,
                          "ElapsedTimeToCAConnect", control_name->ca_connect_time,
                          "PendIOTime", control_name->pendIOTime,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i]) {
      if (pva->isConnected[i] == false) {
        if (!SDDS_SetRowValues(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, control_name->rowIndex[i],
                               "ControlName", control_name->ControlName[i],
                               "ValueString", control_name->ValueString[i],
                               "CAError", control_name->CAError[i],
                               "IndirectName", control_name->IndirectName[i],
                               "Count", 0,
                               "Lineage", "-", NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_SetRowValues(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, control_name->rowIndex[i],
                               "ControlName", control_name->ControlName[i],
                               "ValueString", control_name->ValueString[i],
                               "CAError", control_name->CAError[i],
                               "IndirectName", control_name->IndirectName[i],
                               "Count", control_name->channelInfo[i].length,
                               "Lineage", "-", NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (strlen(control_name->IndirectName[i]) && !is_blank(control_name->IndirectName[i]) && strcmp(control_name->IndirectName[i], "-")) {
        control_name->hasWaveform = 1;
        if (!outputFile && !control_name->waveformRootname) {
          fprintf(stderr, "The output root or the waveform rootname is not provided for saving waveform PV data (sddspvasaverestore).\n");
          exit(1);
        }
        if (control_name->waveformRootname) {
          if (control_name->waveformDir)
            sprintf(vectorOut, "%s/%s.%s", control_name->waveformDir,
                    control_name->waveformRootname, control_name->IndirectName[i]);
          else
            sprintf(vectorOut, "%s.%s", control_name->waveformRootname, control_name->IndirectName[i]);
        } else if (outputFile)
          sprintf(vectorOut, "%s.%s", outputFile, control_name->IndirectName[i]);
        if (control_name->logFp && !control_name->oneWaveformFile)
          fprintf(control_name->logFp, "writing to %s vector file.\n", vectorOut);

        if (!control_name->oneWaveformFile) {
          WriteVectorFile(control_name->ControlName[i], vectorOut, control_name->channelInfo[i].waveformData,
                          control_name->channelInfo[i].length, control_name->channelInfo[i].dataType,
                          TimeStamp, RunStartTime);
        }
      }

      rowcount++;
    }
  }
  if (control_name->hasWaveform && control_name->oneWaveformFile) {
    WriteOneVectorFile(control_name, outputFile, TimeStamp, RunStartTime);
  }
  if (!SDDS_WritePage(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_Terminate(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long SetPVValue(CONTROL_NAME *control_name, PVA_OVERALL *pva) {
  long i, j;
  for (i = 0; i < pva->numPVs; i++) {
    if (control_name->valid[i]) {
      if (control_name->channelInfo[i].length > 1) {
        if (PrepPut(pva, i, control_name->channelInfo[i].stringValue, control_name->channelInfo[i].length) == 1) {
          fprintf(stderr, "ERROR: problem setting PV values\n");
          exit(1);
        }
        for (j = 0; j < control_name->channelInfo[i].length; j++) {
          free(control_name->channelInfo[i].stringValue[j]);
        }
        free(control_name->channelInfo[i].stringValue);
        control_name->channelInfo[i].stringValue = NULL;
      } else {
        if (PrepPut(pva, i, control_name->ValueString[i]) == 1) {
          fprintf(stderr, "ERROR: problem setting PV values\n");
          exit(1);
        }
      }
    }
  }
  if (PutPVAValues(pva) == 1) {
    fprintf(stderr, "ERROR: problem setting PV values\n");
    exit(1);
  }
  for (i = 0; i < pva->numPVs; i++) {
    if (control_name->valid[i]) {
      if (pva->isConnected[i] == false) {
        fprintf(stderr, "Error, no connection for %s\n", control_name->ControlName[i]);
        if (control_name->logFp)
          fprintf(control_name->logFp, "Error, no connection for %s\n", control_name->ControlName[i]);
        control_name->CAError[i] = 'y';
      } else {
        control_name->CAError[i] = 'n';
      }
    }
  }

  /*new FPGA sectors 19,20,21,22 are installed behond a gateway, to make sure that gateway excute the setting command before exiting
    ca_pend_event() is required, H. Shang, 5/24/2013*/
  usleepSystemIndependent(.5 * 1000000);

  //FIX THIS
  /*
  if (control_name->verify)
    {
      VS = (char **)malloc(sizeof(char *) * control_name->variables);
      DV = (struct dbr_ctrl_double *)malloc(sizeof(struct dbr_ctrl_double) * control_name->variables);
      for (i = 0; i < control_name->variables; i++)
        {
          VS[i] = (char *)malloc(sizeof(char) * 250);
          if (control_name->valid[i] && control_name->CAError[i] == 'n')
            {
              caErrors = 0;
              //No waveform checking yet
              if (control_name->channelInfo[i].length == 1)
                {
                  if (control_name->channelInfo[i].dataType == SDDS_STRING)
                    {
                      if (ca_get(DBR_STRING, channelInfo[i].channelID, VS[i]) != ECA_NORMAL)
                        caErrors = 1;
                    }
                  else
                    {
                      if (ca_get(DBR_CTRL_DOUBLE,
                                 channelInfo[i].channelID, &DV[i]) != ECA_NORMAL)
                        caErrors = 1;
                    }
                  if (caErrors)
                    {
                      fprintf(stderr, "Error, unable to get value of %s\n", control_name->ControlName[i]);
                      if (control_name->logFp)
                        fprintf(control_name->logFp, "Error, unable to get value of %s\n", control_name->ControlName[i]);
                      control_name->CAError[i] = 'y';
                    }
                }
            }
        }

      if (ca_pend_io(control_name->pendIOTime) != ECA_NORMAL)
        {
          for (i = 0; i < control_name->variables; i++)
            {
              if (control_name->valid[i] && control_name->CAError[i] == 'n')
                {
                  if (ca_state(channelInfo[i].channelID) != cs_conn)
                    {
                      control_name->CAError[i] = 'y';
                      fprintf(stderr, "Error: Can not read value of %s\n", control_name->ControlName[i]);
                      if (control_name->logFp)
                        fprintf(control_name->logFp, "Error: Can not read value of %s\n", control_name->ControlName[i]);
                    }
                }
            }
        }

      for (i = 0; i < control_name->variables; i++)
        {
          if (control_name->valid[i] && control_name->CAError[i] == 'n')
            {
              if (channelInfo[i].length == 1)
                {
                  switch (channelInfo[i].dataType)
                    {
                    case SDDS_DOUBLE:
                      if (channelInfo[i].isEnumPV)
                        {
                          sprintf(VS[i], "%.0f", DV[i].value);
                        }
                      else
                        {
                          sprintf(VS[i], "%.13le", DV[i].value);
                          sprintf(control_name->ValueString[i], "%.13le", atof(control_name->ValueString[i]));
                          if ((strcmp(control_name->ValueString[i], VS[i]) != 0) && (DV[i].precision > 0))
                            {
                              sprintf(format, "%%.%dlf", (dbr_short_t)DV[i].precision);
                              sprintf(VS[i], format, DV[i].value);
                              sprintf(control_name->ValueString[i], format, atof(control_name->ValueString[i]));
                            }
                        }
                      break;
                    case SDDS_FLOAT:
                      sprintf(VS[i], "%.3le", DV[i].value);
                      sprintf(control_name->ValueString[i], "%.3le", atof(control_name->ValueString[i]));
                      if ((strcmp(control_name->ValueString[i], VS[i]) != 0) && (DV[i].precision > 0))
                        {
                          sprintf(format, "%%.%dlf", (dbr_short_t)DV[i].precision);
                          sprintf(VS[i], format, DV[i].value);
                          sprintf(control_name->ValueString[i], format, atof(control_name->ValueString[i]));
                        }
                      break;
                    case SDDS_LONG64:
                    case SDDS_ULONG64:
                    case SDDS_LONG:
                    case SDDS_ULONG:
                    case SDDS_SHORT:
                    case SDDS_USHORT:
                    case SDDS_CHARACTER:
                      sprintf(VS[i], "%.0f", DV[i].value);
                      break;
                    case SDDS_STRING:
                      break;
                    default:
                      fprintf(stderr, "Error: invalid datatype of PV %s for reading values\n", control_name->ControlName[i]);
                      exit(1);
                      break;
                    }
                  if (strcmp(control_name->ValueString[i], VS[i]) != 0)
                    {
                      fprintf(stderr, "Warning: PV did not accept value change (%s)\n", control_name->ControlName[i]);
                      fprintf(stderr, "         SCR value=%s, current value=%s\n", control_name->ValueString[i], VS[i]);
                      if (control_name->logFp)
                        {
                          fprintf(control_name->logFp, "Warning: PV did not accept value change (%s)\n", control_name->ControlName[i]);
                          fprintf(control_name->logFp, "         SCR value=%s, current value=%s\n", control_name->ValueString[i], VS[i]);
                        }
                    }
                }
            }
        }

      for (i = 0; i < control_name->variables; i++)
        {
          if (VS[i])
            {
              free(VS[i]);
            }
        }
      free(VS);
      free(DV);
    }
  */
  return 1;
}

long WriteOneVectorFile(CONTROL_NAME *control_name, char *outputFile, char *TimeStamp, double RunStartTime) {
  char vectorOut[1024];
  SDDS_TABLE outTable;
  int32_t *index, i, j;

  if (control_name->waveformRootname) {
    if (control_name->waveformDir)
      sprintf(vectorOut, "%s/%s.waveformDir", control_name->waveformDir, control_name->waveformRootname);
    else
      sprintf(vectorOut, "%s.waveform", control_name->waveformRootname);
  } else if (outputFile)
    sprintf(vectorOut, "%s.waveform", outputFile);

  if (control_name->logFp)
    fprintf(control_name->logFp, "writing to %s vector file.\n", outputFile);
  if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 1, NULL, NULL, vectorOut) ||
      !SDDS_DefineSimpleParameter(&outTable, "Time", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&outTable, "TimeStamp", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(&outTable, "WaveformPV", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(&outTable, "Index", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(&outTable, "Waveform", NULL, SDDS_STRING))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i] && control_name->channelInfo[i].length > 1 && control_name->CAError[i] == 'n') {
      index = (int32_t *)malloc(sizeof(*index) * control_name->channelInfo[i].length);
      for (j = 0; j < control_name->channelInfo[i].length; j++)
        index[j] = j;
      if (!SDDS_StartPage(&outTable, control_name->channelInfo[i].length))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME,
                          index, control_name->channelInfo[i].length, "Index") ||
          !SDDS_SetParameters(&outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "TimeStamp", TimeStamp,
                              "Time", RunStartTime,
                              "WaveformPV", control_name->ControlName[i],
                              NULL))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (control_name->channelInfo[i].dataType == SDDS_STRING) {
        if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, control_name->channelInfo[i].waveformData, control_name->channelInfo[i].length, "Waveform"))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      } else {
        if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_BY_NAME, (double *)(control_name->channelInfo[i].waveformData), control_name->channelInfo[i].length, "Waveform"))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (!SDDS_WritePage(&outTable))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

      free(index);
    }
  }
  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long WriteVectorFile(char *waveformPV, char *filename, void *waveformData, long waveformLength,
                     long waveformDataType, char *TimeStamp, double RunStartTime) {
  SDDS_TABLE outTable;
  int32_t *index, i;

  if (!waveformPV || !filename || !waveformData || !TimeStamp)
    return 1;
  if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 1, NULL, NULL, filename) ||
      !SDDS_DefineSimpleParameter(&outTable, "Time", NULL, SDDS_DOUBLE) ||
      !SDDS_DefineSimpleParameter(&outTable, "TimeStamp", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(&outTable, "WaveformPV", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleColumn(&outTable, "Index", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(&outTable, "Waveform", NULL, waveformDataType))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_WriteLayout(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  index = (int32_t *)malloc(sizeof(*index) * waveformLength);
  for (i = 0; i < waveformLength; i++)
    index[i] = i;
  if (!SDDS_StartTable(&outTable, waveformLength) ||
      !SDDS_SetColumn(&outTable, SDDS_BY_NAME,
                      waveformData, waveformLength, "Waveform") ||
      !SDDS_SetColumn(&outTable, SDDS_BY_NAME,
                      index, waveformLength, "Index") ||
      !SDDS_SetParameters(&outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "TimeStamp", TimeStamp,
                          "Time", RunStartTime,
                          "WaveformPV", waveformPV,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (waveformDataType == SDDS_STRING) {
    if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME, waveformData, waveformLength, "Waveform"))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  } else {
    if (!SDDS_SetColumnFromDoubles(&outTable, SDDS_BY_NAME, (double *)waveformData, waveformLength, "Waveform"))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (!SDDS_WriteTable(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_Terminate(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(index);
  return 1;
}

#ifdef USE_RUNCONTROL
int runControlPingWhileSleep(double sleepTime, CONTROL_NAME *control_name) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
    switch (rcParam.status) {
    case RUNCONTROL_ABORT:
      if (control_name->verbose)
        fprintf(stderr, "Run control sddspvasaverestore aborted.\n");
      if (control_name->logFp) {
        fprintf(control_name->logFp, "Run control sddspvasaverestore aborted.\n");
        fclose(control_name->logFp);
      }
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      if (control_name->verbose)
        fprintf(stderr, "Run control sddspvasaverestore timed out.\n");
      if (control_name->logFp) {
        fprintf(control_name->logFp, "Run control sddspvasaverestore timed out.\n");
        fclose(control_name->logFp);
      }
      strcpy(rcParam.message, "Sddspvasaverestore timed-out");
      runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
      exit(1);
      break;
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      if (control_name->verbose)
        fprintf(stderr, "Communications error with runcontrol record.\n");
      if (control_name->logFp)
        fprintf(control_name->logFp, "Communications error with runcontrol record.\n");
      return (RUNCONTROL_ERROR);
    default:
      if (control_name->verbose)
        fprintf(stderr, "Unknown run control error code.\n");
      if (control_name->logFp)
        fprintf(control_name->logFp, "Unknown run control error code.\n");
      break;
    }
    oag_ca_pend_event(MIN(timeLeft, rcParam.pingInterval), &sigint);
    if (sigint)
      exit(1);
    timeLeft -= rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void interrupt_handler(int sig) {
  exit(1);
}

void sigint_interrupt_handler(int sig) {
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
  sigint = 1;
  return;
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
      switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
      case RUNCONTROL_OK:
        break;
      case RUNCONTROL_ERROR:
        fprintf(stderr, "Error exiting run control.\n");
        break;
      default:
        fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
        break;
      }
    }
#endif
    ca_task_exit();
  }
}

void GetUserInformation(CONTROL_NAME *control_name) {
#if !defined(_WIN32)
  struct passwd *pp;
  int effective_uid;
  /* uid - person who logged in ... effective uid person as */
  /* defined by "set-user-ID" (if done at all)              */
  effective_uid = geteuid();

  if ((pp = getpwuid(effective_uid))) {
    sprintf(control_name->userID, "%s (%s)", pp->pw_name, pp->pw_gecos);
    control_name->effective_uid = pp->pw_uid;
    control_name->group_id = pp->pw_gid;
  } else {
    fprintf(stderr, "Unable to get user information.\n");
    exit(1);
  }
#else
  sprintf(control_name->userID, "");
  control_name->effective_uid = 0;
  control_name->group_id = 0;
#endif
}

long SDDS_UnsetDupicateRows(SDDS_TABLE *inSet) {
  long rows = 0, i;
  int32_t *rowFlag;
  char **ControlName = NULL;
  KEYED_EQUIVALENT **keyGroup;
  long keyGroups = 0, index;

  if (!(rows = SDDS_CountRowsOfInterest(inSet)))
    SDDS_Bomb((char *)"Zero Rows defined in input file.");
  if (!(ControlName = (char **)SDDS_GetColumn(inSet, (char *)"ControlName")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  keyGroup = MakeSortedKeyGroups(&keyGroups, SDDS_STRING, ControlName, rows);
  rowFlag = (int32_t *)malloc(sizeof(*rowFlag) * rows);
  for (i = 0; i < rows; i++)
    rowFlag[i] = 0;
  for (i = 0; i < keyGroups; i++) {
    if ((index = match_string(keyGroup[i]->equivalent[0]->stringKey, ControlName, rows, EXACT_MATCH)) >= 0)
      rowFlag[index] = 1;
  }
  for (i = 0; i < keyGroups; i++) {
    free(keyGroup[i]->equivalent);
    free(keyGroup[i]);
  }
  free(keyGroup);
  for (i = 0; i < rows; i++)
    free(ControlName[i]);
  free(ControlName);
  if (!SDDS_AssertRowFlags(inSet, SDDS_FLAG_ARRAY, rowFlag, (int64_t)rows))
    return 0;
  free(rowFlag);
  return 1;
}

void connection_callback(struct connection_handler_args event) {
  NewPVs--;
  return;
}

char *read_char_waveform(char **stringval, PVA_OVERALL *pva) {
  long length;

  if (*stringval)
    free(*stringval);
  *stringval = NULL;
  freePVAGetReadings(pva);
  if (GetPVAValues(pva) == 1) {
    fprintf(stderr, "Unable to read the character waveform value!\n");
    exit(1);
  }
  length = strlen(pva->pvaData[0].getData[0].stringValues[0]) + 1;
  *stringval = (char *)malloc(sizeof(char) * length);
  strcpy(*stringval, pva->pvaData[0].getData[0].stringValues[0]);
  return *stringval;
}

long setup_other_caconnections(CONTROL_NAME *control_name, PVA_OVERALL *pvaDescriptPV, PVA_OVERALL *pvaTriggerPV, PVA_OVERALL *pvaOutputFilePV, PVA_OVERALL *pvaInputFilePV) {
  if (control_name->descriptPV) {
    allocPVA(pvaDescriptPV, 1);
    epics::pvData::shared_vector<std::string> descName(1);
    epics::pvData::shared_vector<std::string> descProvider(1);
    descName[0] = control_name->descriptPV;
    descProvider[0] = control_name->descriptPV_provider;
    pvaDescriptPV->pvaChannelNames = freeze(descName);
    pvaDescriptPV->pvaProvider = freeze(descProvider);
    ConnectPVA(pvaDescriptPV, 2.0);
    if (GetPVAValues(pvaDescriptPV) == 1) {
      exit(1);
    }
  }
  if (control_name->triggerPV) {
    allocPVA(pvaTriggerPV, 1);
    epics::pvData::shared_vector<std::string> pvasaveName(1);
    epics::pvData::shared_vector<std::string> pvasaveProvider(1);
    pvasaveName[0] = control_name->triggerPV;
    pvasaveProvider[0] = control_name->triggerPV_provider;
    pvaTriggerPV->pvaChannelNames = freeze(pvasaveName);
    pvaTriggerPV->pvaProvider = freeze(pvasaveProvider);
    ConnectPVA(pvaTriggerPV, 2.0);
    if (GetPVAValues(pvaTriggerPV) == 1) {
      exit(1);
    }
  }

  if (control_name->outputFilePV) {
    allocPVA(pvaOutputFilePV, 1);
    epics::pvData::shared_vector<std::string> outputName(1);
    epics::pvData::shared_vector<std::string> outputProvider(1);
    outputName[0] = control_name->outputFilePV;
    outputProvider[0] = control_name->outputFilePV_provider;
    pvaOutputFilePV->pvaChannelNames = freeze(outputName);
    pvaOutputFilePV->pvaProvider = freeze(outputProvider);
    ConnectPVA(pvaOutputFilePV, 2.0);
    if (GetPVAValues(pvaOutputFilePV) == 1) {
      exit(1);
    }
  }
  if (control_name->inputFilePV) {
    allocPVA(pvaInputFilePV, 1);
    epics::pvData::shared_vector<std::string> inputName(1);
    epics::pvData::shared_vector<std::string> inputProvider(1);
    inputName[0] = control_name->inputFilePV;
    inputProvider[0] = control_name->inputFilePV_provider;
    pvaInputFilePV->pvaChannelNames = freeze(inputName);
    pvaInputFilePV->pvaProvider = freeze(inputProvider);
    ConnectPVA(pvaInputFilePV, 2.0);
    if (GetPVAValues(pvaInputFilePV) == 1) {
      exit(1);
    }
  }
  if ((control_name->descriptPV) && (pvaDescriptPV->numNotConnected)) {
    fprintf(stdout, "Exiting due to PV connection error for %s\n", control_name->descriptPV);
    return (1);
  }
  if ((control_name->triggerPV) && (pvaTriggerPV->numNotConnected)) {
    fprintf(stdout, "Exiting due to PV connection error for %s\n", control_name->triggerPV);
    return (1);
  }
  if ((control_name->outputFilePV) && (pvaOutputFilePV->numNotConnected)) {
    fprintf(stdout, "Exiting due to PV connection error for %s\n", control_name->outputFilePV);
    return (1);
  }
  if ((control_name->inputFilePV) && (pvaInputFilePV->numNotConnected)) {
    fprintf(stdout, "Exiting due to PV connection error for %s\n", control_name->inputFilePV);
    return (1);
  }
  if (control_name->descriptPV) {
    if ((pvaDescriptPV->pvaData[0].scalarType != epics::pvData::pvString) ||
        (pvaDescriptPV->pvaData[0].fieldType != epics::pvData::scalar)) {
      fprintf(stderr, "the description PV should be a character-waveform PV\n");
      return (1);
    }
  }
  if (control_name->outputFilePV) {
    if ((pvaOutputFilePV->pvaData[0].scalarType != epics::pvData::pvString) ||
        (pvaOutputFilePV->pvaData[0].fieldType != epics::pvData::scalar)) {
      fprintf(stderr, "the output file PV should be a character-waveform PV\n");
      return (1);
    }
  }
  if (control_name->inputFilePV) {
    if ((pvaInputFilePV->pvaData[0].scalarType != epics::pvData::pvString) ||
        (pvaInputFilePV->pvaData[0].fieldType != epics::pvData::scalar)) {
      fprintf(stderr, "the input file PV should be a character-waveform PV\n");
      return (1);
    }
  }
  return (0);
}
