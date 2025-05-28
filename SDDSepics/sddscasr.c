/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 *  $Log: not supported by cvs2svn $
 *  Revision 1.48  2010/04/15 20:19:29  soliday
 *  Added oag_ca_exception_handler to capture CA.Client.Exceptions that previously
 *  were just printed to stdout and did not cause the program to exit with an
 *  error state.
 *
 *  Revision 1.47  2010/04/15 15:25:05  shang
 *  added three parameters ElaspedTimeToCAConnect, ElaspedTimeToSave, and PendIOTime to output file to record the channel access time and input pend io time.
 *
 *  Revision 1.46  2010/03/10 23:11:11  shang
 *  refined the index type of the waveform to int32_t instead of long to be consistent with the SDDS_LONG type, this fixed the problem on 64bit machine.
 *
 *  Revision 1.45  2009/03/18 18:12:32  shang
 *  added option of storing the waveform file name as the value of waveform pvs in the outputfile, and the default value is "WaveformPV"
 *
 *  Revision 1.44  2009/02/10 20:40:50  shang
 *  modified to exit with warning message if an empty file provided.
 *
 *  Revision 1.43  2009/01/21 20:55:50  shang
 *  added extension for waveform file name.
 *
 *  Revision 1.42  2009/01/19 22:01:25  shang
 *  corrected the printout message for writing waveform file.
 *
 *  Revision 1.41  2009/01/19 20:18:44  shang
 *  removed the debug statements in DataTypeToCaType statements.
 *
 *  Revision 1.40  2009/01/19 20:13:39  shang
 *  added "onefile" option to -waveform option to read/write wavefroms from/to one file.
 *
 *  Revision 1.39  2006/12/14 22:25:49  soliday
 *  Updated a bunch of programs because SDDS_SaveLayout is now called by
 *  SDDS_WriteLayout and it is no longer required to be called directly.
 *  Also the AutoCheckMode is turned off by default now so I removed calls to
 *  SDDS_SetAutoCheckMode that would attempt to turn it off. It is now up to
 *  the programmer to turn it on in new programs until debugging is completed
 *  and then remove the call to SDDS_SetAutoCheckMode.
 *
 *  Revision 1.38  2005/11/08 22:34:39  soliday
 *  Updated to remove compiler warnings on Linux.
 *
 *  Revision 1.37  2005/11/08 22:05:03  soliday
 *  Added support for 64 bit compilers.
 *
 *  Revision 1.36  2005/07/18 23:28:44  shang
 *  fixed a freeing freed memory bug
 *
 *  Revision 1.35  2005/07/18 22:10:36  shang
 *  fixed a bug that the values of character pvs were not transformed into ValueString column
 *
 *  Revision 1.34  2005/04/05 00:09:57  borland
 *  Fixed error in previous change.
 *
 *  Revision 1.33  2005/04/05 00:08:14  borland
 *  No longer bomb when input file is of zero length (no rows).
 *
 *  Revision 1.32  2005/02/09 15:05:11  shang
 *  moved the checking if output is provided after processFilenames
 *
 *  Revision 1.31  2005/02/09 15:03:00  shang
 *  made the pipe option work with save
 *
 *  Revision 1.30  2005/01/31 19:46:06  shang
 *  it now takes both - and white spaces of IndirectName as scalar pv.
 *
 *  Revision 1.29  2005/01/10 16:32:16  shang
 *  the Time parameter will overwrited to double type if it exists in the input file; the input
 *  file now is not allowed to be overwritten.
 *
 *  Revision 1.28  2004/11/04 19:57:49  shang
 *  removed the debug printing statments
 *
 *  Revision 1.27  2004/11/04 16:36:48  shang
 *  added Time parameter to the output file and uses the libaries watchInput functions
 *  for checking the input file state.
 *
 *  Revision 1.26  2004/09/27 15:49:41  soliday
 *  Updated the section that parses the arguments.
 *
 *  Revision 1.25  2004/09/25 17:27:36  borland
 *  Added debugging statements and fixed problem with hanging at exit by
 *  adding ca_task_exit() call.
 *
 *  Revision 1.24  2004/09/10 14:37:56  soliday
 *  Changed the flag for oag_ca_pend_event to a volatile variable
 *
 *  Revision 1.23  2004/08/10 16:58:58  shang
 *  set the outputfile pv to empty string before taking data
 *
 *  Revision 1.22  2004/08/10 12:56:47  shang
 *  added rowIndex member to Control_Name structure to fix the row mismatch bug
 *  occurred in writing to output files.
 *
 *  Revision 1.21  2004/07/22 22:05:17  soliday
 *  Improved signal support when using Epics/Base 3.14.6
 *
 *  Revision 1.20  2004/07/19 17:39:36  soliday
 *  Updated the usage message to include the epics version string.
 *
 *  Revision 1.19  2004/07/16 21:24:39  soliday
 *  Replaced sleep commands with ca_pend_event commands because Epics/Base
 *  3.14.x has an inactivity timer that was causing disconnects from PVs
 *  when the log interval time was too large.
 *
 *  Revision 1.18  2004/04/26 19:41:04  shang
 *  only the error messages will be printed out if -verbose option is given.
 *
 *  Revision 1.17  2004/04/15 22:38:37  shang
 *  replace countExist (a bug) by LineageExist for define Lineage column
 *
 *  Revision 1.16  2004/04/15 22:15:18  shang
 *  added Lineage and Count columns to output file to be able to be used by burtwb
 *
 *  Revision 1.15  2004/03/24 23:32:49  shang
 *  made changes such that the input filename will be overwritten by inputFilePV's
 *  vale, and if only one filename is provided with inputFilePV provided, the filename
 *  will be given to output root; moved the ca connection setup in arguments parsing
 *  statments to where the parsing is done.
 *
 *  Revision 1.14  2004/03/16 20:57:16  shang
 *  fixed the cast problem in usleep()
 *
 *  Revision 1.13  2004/03/16 17:00:40  shang
 *  fixed the bug in usleep time and changed the output format for different datatypes
 *
 *  Revision 1.12  2004/03/16 15:21:16  shang
 *  added -inputFilePV and -description=pv options
 *
 *  Revision 1.11  2004/03/04 19:32:45  shang
 *  removed the quotes for string PVs before restoring values
 *
 *  Revision 1.10  2004/03/01 19:43:49  shang
 *  added quote for string values that have spaces.
 *
 *  Revision 1.9  2004/02/26 19:54:52  shang
 *  changed the precision of the numeric values to be consistent with the result of
 *  burtrb and made the waveform option work in save option also so that one can
 *  still use pipe option when waveform PV exists
 *
 *  Revision 1.8  2004/02/05 20:32:52  soliday
 *  Updated usage message so that it would compile on Windows.
 *
 *  Revision 1.7  2003/11/14 16:14:43  shang
 *  set the pingTimeout of runcontrol to be 10 seconds longer than pendIOTime if it
 *  is shorter than the pendIOTime to avoid ping timeout during CA connection;
 *  added runcontrolExit() to abort action; added signal(SIGINT, interrupt_handler) to
 *  respond to the kill command.
 *
 *  Revision 1.6  2003/11/12 23:27:11  shang
 *  added -numeric option for getting numerical values of enum pvs. added
 *  run control ping right before CA connection to help avoiding runcontrol time out.
 *
 *  Revision 1.5  2003/10/30 19:34:46  shang
 *  changed the outputFilePV from string scalar pv to character waveform PV
 *
 *  Revision 1.4  2003/09/26 19:30:18  shang
 *  fixed a bug that tried to get the waveform rootname from a parameter which
 *  may not exist.
 *
 *  Revision 1.3  2003/09/12 19:37:22  shang
 *  made -pipe option work, save and restore waveform pv in the same way as sddswget and sddswput,
 *  added -waveform option to provide the rootname and directory of waveform file, in case they
 *  are different from the input file.
 *
 *  Revision 1.18  2003/08/05 18:05:27  shang
 *  set the ValueString to "_NoConnection_" if PV does not exist or
 *  "_NoValueAvailable_" if the value of PV can not be read
 *
 *  Revision 1.17  2003/08/05 14:35:43  shang
 *  changed the parameter type of EffectiveUID and GroupID from long to string to
 *  be consistent with snapshot files.
 *
 *  Revision 1.16  2003/07/30 17:39:39  shang
 *  added more RequestFile and SnapshotFilename parameters to output file to make
 *  it consistent with snapshot files
 *
 *  Revision 1.15  2003/07/29 21:06:34  shang
 *  fixed the segmentation bug when -outputFilePV is not given
 *
 *  Revision 1.14  2003/07/29 19:49:20  shang
 *  added outputFilePV option to store the output file rootname
 *
 *  Revision 1.13  2003/07/25 22:31:07  shang
 *  write outputFile name to semaphoreFile
 *
 *  Revision 1.12  2003/07/25 21:22:55  shang
 *  fixed the bug in writing output with -daemon mode
 *
 *  Revision 1.11  2003/07/25 19:52:43  shang
 *  removed the warning message
 *
 *  Revision 1.10  2003/07/25 19:17:26  shang
 *  added -pipe option and fixed several bugs
 *
 *  Revision 1.9  2003/07/15 21:32:35  shang
 *  added checking for IndirectName column for restoring
 *
 *  Revision 1.8  2003/07/14 19:37:10  shang
 *  fixed the bug that segmenation fault occurred when the runcontrol record about is pressed
 *  in Linux system
 *
 *  Revision 1.7  2003/07/11 14:21:17  soliday
 *  Added missing ifdef statement.
 *
 *  Revision 1.6  2003/07/10 21:27:02  soliday
 *  Updated to work with WIN32
 *
 *  Revision 1.5  2003/07/10 21:08:06  soliday
 *  Added Shang's changes.
 *
 *  Revision 1.4  2003/07/09 19:09:43  soliday
 *  Added support for WIN32
 *
 *  Revision 1.3  2003/07/03 19:56:05  shang
 *  added pendIOTime option
 *
 *  Revision 1.2  2003/07/03 16:21:29  shang
 *  replaced sddscasave by sddscasr
 *
 *  Revision 1.1  2003/07/01 22:42:42  shang
 *  an alternative version of casave with more features and efficiency
 *
 alternative programs for casave/carestore with more features and expecting more efficiency  SHANG
 *
 */
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <alarm.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
#include <signal.h>
#include <alarm.h>

#if defined(_WIN32)
#  include <winsock.h>
#  include <process.h>
#  include <io.h>
#  include <sys/locking.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#  include <pwd.h>
#endif

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

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include "SDDSepics.h"

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
#define CLO_CASAVE 4
#define CLO_CARESTORE 5
#define CLO_PENDIOTIME 6
#define CLO_LOGFILE 7
#define CLO_RUNCONTROLPV 8
#define CLO_RUNCONTROLDESC 9
#define CLO_PIDFILE 10
#define CLO_CASAVEPV 11
#define CLO_INTERVAL 12
#define CLO_UNIQUE 13
#define CLO_PIPE 14
#define CLO_DESCRIPTION 15
#define CLO_OUTPUTFILEPV 16
#define CLO_WAVEFORM 17
#define CLO_NUMERICAL 18
#define CLO_INPUTFILEPV 19
#define CLO_ADD 20
#define CLO_DRYRUN 21
#define COMMANDLINE_OPTIONS 22

#define ONE_WAVEFORM_FILE 0x0001UL
#define SAVE_WAVEFORM_FILE 0x0002UL
#define FULL_WAVEFORM_FILENAME 0x0004UL

typedef struct
{
  chid channelID;
  short isEnumPV;
  long length; /*length=1. means scalar pv */
  char charValue;
  long dataType;
  char *bufferCopy;
  char **stringValue;
  void *waveformData;
  /* used with channel access routines to give index via callback: */
  long usrValue, flag, *count;
} CHANNEL_INFO;

typedef struct
{
  char **ControlName;
  long variables, validCount; /*variables is the total number of pv, validCount is the number of valid pvs */
  long *rowIndex;             /* rowIndex is the row position of PV in the input file */
  char **IndirectName;        /*same as ControlName for vectors, or "-" for scalar pvs */
  char **ValueString;         /*the value of pvs are saved as string, input/output column is ValueString */
  char *CAError;              /*y or n for CAerror, the corresponding column name is CAError */
  CHANNEL_INFO *channelInfo;
  short *newFlag; /*indicates if the PV is new or old */
  short *valid;   /*indicates if the PV is still valid, if not, do nothing about it */
  double pendIOTime;
  short verbose, casave, carestore, numerical, verify, add;
  FILE *logFp;    /*file pointer for logfile to log the print out infomation*/
  char *logFile;  /*logfile to log the print out infomation*/
  char *casavePV; /*monitor pv to turn on/off sddscasr */
  chid casavePV_ID, outputFilePV_ID, descriptPV_ID, inputFilePV_ID;
  double interval;
  short unique;
  char *inputFilePV;  /*a char waveform pv which contains the input filename */
  char *descriptPV;   /*a char waveform pv which contains the description for SnapshotDescription parameter */
  char *description;  /* write to output file SnapshotDescription parameter */
  char *outputFilePV; /*store the output file name*/
  /*variables for login id, group id ,etc. */
  char userID[256];
  long effective_uid, group_id;
  char *waveformRootname, *waveformExtension; /*rootname for waveform pv file for restoring option, in case it is different from the input file name, default is the SnapshotFilename par in the input file*/
  char *waveformDir;                          /* directory of the waveform file for restoring option, default is [pwd] */
  double *doubleValue;                        /*for keep the value of enumerical pvs if -numerical option is provided. */
  short waveformRootGiven, oneWaveformFile, hasWaveform, saveWaveformFile, fullname;
  long waveformDataType;
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

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV={string=<string>|parameter=<string>}\n";
static char *runControlDescUsage = "-runControlDescription={string=<string>|parameter=<string>}\n";
#endif

static int SignalEventArrived, NewPVs;
volatile int sigint = 0;

void SignalEventHandler(int i);
void InitializeData(CONTROL_NAME *control_name);
long UpdateDataInfo(CONTROL_NAME *control_name, long variables, char **ControlName,
                    char **ValueString, char **IndirectName, char *inputFile);
long GetWaveformValueFromFile(char *directory, char *rootname, char *waveformPV, CHANNEL_INFO *channelInfo);
long GetWaveformValuesFromOneFile(CONTROL_NAME *control_name);
long ReadPVValue(CONTROL_NAME *control_name, long add, char *outputFile);
long ReadInputAndMakeCAConnections(CONTROL_NAME *control_name, char *inputFile, char *outputFile, SDDS_DATASET *outTable);
long SetupCAConnection(CONTROL_NAME *control_name);
void FreeDataMemory(CONTROL_NAME *control_name);
void sddscasaverestore(CONTROL_NAME *control_name, char *inputFile, char *outputRoot,
                       long dailyFiles, char *semaphoreFile, long daemon, long add, long dryRun);

long ReadDataAndSave(CONTROL_NAME *control_name, SDDS_DATASET *outTable, char *inputFile, char *outputFile, long firstTime);
long InputfileModified(char *inputFile, char *prevInput, struct stat *input_stat);
long WriteVectorFile(char *waveformPV, char *filename, void *waveformData, long waveformLength,
                     long waveformDataType, char *TimeStamp, double RunStartTime);
long WriteOneVectorFile(CONTROL_NAME *control_name, char *outputFile, char *TimeStamp, double RunStartTime);
long SetPVValue(CONTROL_NAME *control_name, long dryRun);
long WriteOutputLayout(SDDS_DATASET *outTable, SDDS_TABLE *inTable);
void GetUserInformation(CONTROL_NAME *control_name);
long SDDS_UnsetDupicateRows(SDDS_TABLE *inSet);
void connection_callback(struct connection_handler_args event);
char *read_char_waveform(char **stringval, chid charWFID);
void setup_other_caconnections(CONTROL_NAME *control_name);
char *getWaveformFileName(CONTROL_NAME *control_name, char *outputFile, char *waveformPV);

char *commandline_option[COMMANDLINE_OPTIONS] = {
  "daemon",
  "semaphore",
  "dailyFiles",
  "verbose",
  "save",
  "restore",
  "pendIOTime",
  "logFile",
  "runControlPV",
  "runControlDescription",
  "pidFile",
  "casavePV",
  "interval",
  "unique",
  "pipe",
  "description",
  "outputFilePV",
  "waveform",
  "numerical",
  "inputFilePV",
  "add",
  "dryRun",
};
char *USAGE1 = "sddscasr <inputfile> <outputRoot> [-verbose]\n\
[-daemon] [-dailyFiles] [-semaphore=<filename>] [-save] [-restore[=verify]] [-logFile=] \n\
[-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>,pingInterval=<value>] \n\
[-runControlDescription={string=<string>|parameter=<string>}] [-unique] [-outputFilePV=<pvname>] \n\
[-inputFilePV=<pvname>] [-add] [-dryRun] \n\
[-pidFile=<pidFile>] [-casavePV=<string>] [-interval=<seconds>] [-pipe=[input|output]] \n\
[-waveform=[rootname=<string>][,directory=<string>],onefile][,saveWaveformFile[,fullname]] [-numerical] [-description=<string>|pv=<pvname>] \n \
inputFile      inputFile name (SDDS file). \n\
outputRoot     output file or root if -dailyFiles option is specified. \n\
-verbose       print out messages. \n\
-dailyFiles    output file name is attached by current date. \n\
-semaphore     specify the flag file for indicating CA connection completence. \n\
               the current outputFile name is written to semaphore file. \n";
char *USAGE2 = "-daemon        run in daemon mode, that is, stay in background and start running whenever \n\
               changes in the input detected or signal event arrived until terminating \n\
               signal received. \n\
-save          read the values of process variables given in the input file and write \n\
               to the output file. \n\
-restore       set the values of process variables given in the input file. \n\
               -save and -restore can not be given in the same time. \n\
               if -restore option is provide, the outputRoot is not needed.\n\
               If verify is requested, then it will read the values from the PVs\n\
               after they are set to verify they values were changed.\n\
-add           only work for restore option, write PV in delta mode, i.e new_value=current_value+provided_value in input file.\n\
-dryRun        pretend setting pvs, but only print out the pv values to be set \n\
-logFile       file for logging the printout information. \n\
-pendIOTime    specifies maximum time to wait for connections and\n\
               return values.  Default is 30.0s \n\
-pidFile       provide the file to write the PID into.\n\
-interval      the interval (in seconds) of checking monitor pv or the sleeping time \n\
               if no signal handling arrived. \n\
-casavePV      a monitor pv to turn on/off sddscasr. When the pv changed from 0 to 1, \n\
               sddssave make a save and change the pv back to 0. \n\
-outputFilePV  a charachter-waveform pv to store the output file name with full path. \n\
-inputFilePV   a charachter-waveform pv which has the input file name with full path. \n\
               it overwrites the inputFile from command line, for example, if inputFilePV is \n\
               provided, the inputFile will be obtained from this PV. If inputFile is also \n\
               given from from command line, but outputRoot is not provided, then the value of \n\
               inputFile from the command line is given to outputRoot. \n\
-unique        remove all duplicates but the first occurrence of each PV from the input file \n\
               to prevent channel access errors when setting pv values. \n";
char *USAGE3 = "-pipe          input/output data to the pipe.\n\
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
this is an alternative version of casave/carestore, \n\
program by Hairong Shang, ANL\n\
Link date: "__DATE__
               " "__TIME__
               ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

/*****************************************************************************
 *
 * Main routine for casave and carestore
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
  long daemon = 0, add=0, dryRun=0;
  char *semaphoreFile, *inputFile, *outputRoot, *pidFile, *tmpstr;
  FILE *fp = NULL;
  unsigned long pipeFlags = 0;

  semaphoreFile = NULL;
  inputFile = outputRoot = pidFile = NULL;
  control_name = calloc(sizeof(*control_name), 1);
  /*initialize rcParam */
#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = rcParam.PVparam = rcParam.DescParam = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10.0;
  rcParam.alarmSeverity = NO_ALARM;
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

  SDDS_RegisterProgramName("sddscasr");
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s%s\n", USAGE1, USAGE2, USAGE3);
    exit(1);
  }
  /*initialize data */
  InitializeData(control_name);
#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif
  ca_add_exception_event(oag_ca_exception_handler, NULL);

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
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -description syntax.");
        SDDS_CopyString(&tmpstr, s_arg[i_arg].list[1]);
        str_tolower(tmpstr);
        if ((strncmp("pv", tmpstr, 2) != 0))
          SDDS_CopyString(&control_name->description, s_arg[i_arg].list[1]);
        else {
          s_arg[i_arg].n_items -= 1;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "pv", SDDS_STRING, &control_name->descriptPV, 1, 0, NULL) ||
              !control_name->descriptPV)
            SDDS_Bomb("invalid -description syntax.");
          s_arg[i_arg].n_items += 1;
        }
        free(tmpstr);
        break;
      case CLO_PIPE:
        if (!processPipeOption(s_arg[i_arg].list + 1, s_arg[i_arg].n_items - 1, &pipeFlags))
          SDDS_Bomb("invalid -pipe syntax");
        break;
      case CLO_INTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&(control_name->interval), s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -interval");
        break;
      case CLO_PIDFILE:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -pidFile", NULL);
        SDDS_CopyString(&pidFile, s_arg[i_arg].list[1]);
        break;
      case CLO_CASAVEPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -casavePV", NULL);
        SDDS_CopyString(&control_name->casavePV, s_arg[i_arg].list[1]);
        break;
      case CLO_OUTPUTFILEPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -outputFilePV", NULL);
        SDDS_CopyString(&control_name->outputFilePV, s_arg[i_arg].list[1]);
        break;
      case CLO_INPUTFILEPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -inputFilePV", NULL);
        SDDS_CopyString(&control_name->inputFilePV, s_arg[i_arg].list[1]);
        break;
      case CLO_DAILYFILES:
        dailyFiles = 1;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_double(&(control_name->pendIOTime), s_arg[i_arg].list[1])))
          SDDS_Bomb("no value given for option -pendIOTime");
        break;
      case CLO_VERBOSE:
        control_name->verbose = 1;
        break;
      case CLO_CARESTORE:
        if (s_arg[i_arg].n_items == 1) {
          control_name->carestore = 1;
        } else if (s_arg[i_arg].n_items == 2) {
          char *verifyMode[1] = {"verify"};
          control_name->carestore = 1;
          if (match_string(s_arg[i_arg].list[1], verifyMode, 1, 0) == 0) {
            control_name->verify = 1;
          } else {
            bomb("unknown option for -restore", NULL);
          }
        } else {
          bomb("wrong number of items for -restore", NULL);
        }
        break;
      case CLO_CASAVE:
        control_name->casave = 1;
        break;
      case CLO_ADD:
	add = 1;
	break;
      case CLO_DRYRUN:
	dryRun=1;
	break;
      case CLO_DAEMON:
#if defined(_WIN32)
        bomb("-daemon is not available on WIN32", NULL);
#endif
        daemon = 1;
        break;
      case CLO_SEMAPHORE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -semaphore syntax.");
        semaphoreFile = s_arg[i_arg].list[1];
        break;
      case CLO_LOGFILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -semaphore syntax.");
        control_name->logFile = s_arg[i_arg].list[1];
        if (!(control_name->logFp = fopen(s_arg[i_arg].list[1], "w"))) {
          fprintf(stderr, "Can not open log file %s for writing.\n", s_arg[i_arg].list[1]);
          exit(1);
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
            (rcParam.PVparam && rcParam.PV))
          bomb("bad -runControlPV syntax", runControlPVUsage);
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          exit(1);
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
            (rcParam.DescParam && rcParam.Desc))
          bomb("bad -runControlDesc syntax", runControlDescUsage);
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
          fprintf(stderr, "Invalid -waveform option (sddscasr).\n");
          exit(1);
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
        fprintf(stderr, "unrecognized option: %s (sddscasr)\n",
                s_arg[i_arg].list[0]);
        exit(1);
        break;
      }
    } else {
      if (!inputFile)
        SDDS_CopyString(&inputFile, s_arg[i_arg].list[0]);
      else if (!outputRoot)
        SDDS_CopyString(&outputRoot, s_arg[i_arg].list[0]);
      else
        SDDS_Bomb("too many filenames given");
    }
  }
  setup_other_caconnections(control_name);
  if (control_name->inputFilePV) {
    if (inputFile && !outputRoot)
      SDDS_CopyString(&outputRoot, inputFile);
    read_char_waveform(&inputFile, control_name->inputFilePV_ID);
  }

  /* if (!control_name->waveformDir)
     SDDS_CopyString(&control_name->waveformDir,"."); */
  if (control_name->casave) {
    processFilenames("sddscasr", &inputFile, &outputRoot, pipeFlags, 1, &tmpfile_used);
    if (!outputRoot && !(control_name->outputFilePV) && !(pipeFlags & USE_STDOUT))
      SDDS_Bomb("Neither output file nor output file pv or pipe=out are given for -save!");
  }
  if (control_name->carestore) {
    if (inputFile && pipeFlags & USE_STDIN)
      SDDS_Bomb("Too many file names!");
    if (pipeFlags & USE_STDOUT || outputRoot)
      SDDS_Bomb("Invalid output argmument for restore!");
  }
  if (pipeFlags) {
    if (control_name->verbose && daemon)
      fprintf(stderr, "Warning, daemon mode is ignored with -pipe option!\n");
    daemon = 0;
  }
  if (pipeFlags & USE_STDOUT)
    dailyFiles = 0;
  if (control_name->casave && control_name->carestore) {
    fprintf(stderr, "casave and carestore can not exist in the same time (sddscasr).\n");
    exit(1);
  }
  if (!control_name->casave && !control_name->carestore) {
    fprintf(stderr, "one of -save and -restore has to be given (sddscasr).\n");
    exit(1);
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV && !rcParam.Desc) {
    fprintf(stderr, "the runControlDescription should not be null!");
    exit(1);
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
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      exit(1);
    }
  }
#endif
  atexit(rc_interrupt_handler);
  pid = getpid();
  if (pidFile) {
    if (!(fp = fopen(pidFile, "w")))
      bomb("unable to open PID file", NULL);
    fprintf(fp, "%ld\n", pid);
    fclose(fp);
  }
  GetUserInformation(control_name);
  /*read input and make CA connection */
  sddscasaverestore(control_name, inputFile, outputRoot, dailyFiles, semaphoreFile, daemon, add, dryRun);
  ca_task_exit();
#ifdef DEBUG
  fprintf(stderr, "Returned from sddscasaverestore\n");
#endif
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
  if (inputFile)
    free(inputFile);
  if (outputRoot)
    free(outputRoot);
#ifdef DEBUG
  fprintf(stderr, "Free'd filenames\n");
#endif

#ifdef DEBUG
  fprintf(stderr, "ca_task_exit returned\n");
#endif

  if (tmpfile_used && !replaceFileAndBackUp(inputFile, outputRoot))
    exit(1);

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
      SDDS_Bomb("Memory allocation error for control_name!");
  }
  control_name->doubleValue = NULL;
  control_name->numerical = 0;
  control_name->waveformRootname = NULL;
  control_name->waveformExtension = NULL;
  control_name->oneWaveformFile = 0;
  control_name->saveWaveformFile = 0;
  control_name->fullname = 0;
  control_name->hasWaveform = 0;
  control_name->waveformDataType = 0;
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
  control_name->verbose = control_name->casave = control_name->carestore = control_name->verify = 0;
  control_name->pendIOTime = 100.0; /*this is to prevent ioc connecting error when the input files is big*/
  control_name->casavePV = NULL;
  control_name->descriptPV = NULL;
  control_name->inputFilePV = NULL;
  control_name->waveformRootGiven = 0;
  control_name->rowIndex = NULL;
  control_name->ca_connect_time = 0;
}

long UpdateDataInfo(CONTROL_NAME *control_name, long variables, char **ControlName,
                    char **ValueString, char **IndirectName, char *inputFile) {
  long i;
  KEYED_EQUIVALENT **keyGroup; /*using qsort and binary search to find the existence */
  long keyGroups = 0, index, count, newpvs = 0, hasWaveform = 0;
  short *existed; /*indicates if the pv from input file is already contained in the data */

  if (!variables) {
    return 1;
  }
  if (!ControlName) {
    fprintf(stderr, "No controlname and IndirectName provided!");
    return 0;
  }
  if (control_name->carestore && !ValueString) {
    fprintf(stderr, "The ValueString column is missing or has not value in the input file, unable to restore settings!\n");
    return 0;
  }
  if (!control_name->variables) {
    /*the first time of reading input file, where the data is empty */
    control_name->variables = control_name->validCount = variables;
    control_name->ControlName = (char **)malloc(sizeof(*(control_name->ControlName)) * variables);
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
      control_name->channelInfo[i].waveformData = NULL;
      control_name->channelInfo[i].bufferCopy = NULL;
      control_name->IndirectName[i] = NULL;
      control_name->channelInfo[i].stringValue = NULL;
      if (control_name->carestore) {
        if (IndirectName && IndirectName[i])
          SDDS_CopyString(&control_name->IndirectName[i], IndirectName[i]);
        else
          SDDS_CopyString(&control_name->IndirectName[i], "-");
      }
      if (control_name->carestore) {
        if (!ValueString || !ValueString[i]) {
          fprintf(stderr, "The value is not provided for restoring pv values!\n");
          return 0;
        }

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
        SDDS_CopyString(&control_name->ValueString[i], ValueString[i]);
      } else
        control_name->ValueString[i] = (char *)malloc(sizeof(char) * 250);
    }
    if (control_name->carestore && control_name->hasWaveform && control_name->oneWaveformFile) {
      /* read waveform values from one file */
      GetWaveformValuesFromOneFile(control_name);
    }
  } else {
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
        if (control_name->carestore) {
          if (control_name->ValueString[index])
            free(control_name->ValueString[index]);
          if (control_name->IndirectName[index])
            free(control_name->IndirectName[index]);
          SDDS_CopyString(&control_name->ValueString[index], ValueString[i]);
          SDDS_CopyString(&control_name->IndirectName[index], IndirectName[i]);
        }
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
      control_name->rowIndex = SDDS_Realloc(control_name->rowIndex, sizeof(*control_name->rowIndex) * control_name->variables);
      control_name->ControlName = SDDS_Realloc(control_name->ControlName, sizeof(*control_name->ControlName) * control_name->variables);
      control_name->ValueString = SDDS_Realloc(control_name->ValueString, sizeof(*control_name->ValueString) * control_name->variables);
      control_name->IndirectName = SDDS_Realloc(control_name->IndirectName, sizeof(*control_name->IndirectName) * control_name->variables);
      control_name->channelInfo = SDDS_Realloc(control_name->channelInfo, sizeof(*control_name->channelInfo) * control_name->variables);
      control_name->CAError = SDDS_Realloc(control_name->CAError, sizeof(*control_name->CAError) * control_name->variables);
      control_name->newFlag = SDDS_Realloc(control_name->newFlag, sizeof(*control_name->newFlag) * control_name->variables);
      control_name->valid = SDDS_Realloc(control_name->valid, sizeof(*control_name->valid) * control_name->variables);
      control_name->doubleValue = SDDS_Realloc(control_name->doubleValue, sizeof(*control_name->doubleValue) * control_name->variables);
      for (i = 0; i < variables; i++) {
        if (existed[i])
          continue;
        /*add new pvs to the data */
        SDDS_CopyString(&control_name->ControlName[count], ControlName[i]);
        control_name->rowIndex[count] = i;
        control_name->IndirectName[count] = NULL;
        control_name->ValueString[count] = NULL;
        if (control_name->carestore) {
          if (IndirectName && IndirectName[i])
            SDDS_CopyString(&control_name->IndirectName[count], IndirectName[i]);
          else
            SDDS_CopyString(&control_name->IndirectName[count], "-");
        }
        control_name->channelInfo[count].waveformData = NULL;
        control_name->channelInfo[count].stringValue = NULL;
        control_name->channelInfo[count].bufferCopy = NULL;
        control_name->channelInfo[count].length = 1;
        if (control_name->carestore) {
          if (!ValueString || !ValueString[i]) {
            fprintf(stderr, "No value provided for restoring pv values!");
            return 0;
          }
          if (strlen(control_name->IndirectName[i]) && !is_blank(control_name->IndirectName[i]) && strcmp(control_name->IndirectName[i], "-") != 0) {
            hasWaveform = 1;
            if (!control_name->oneWaveformFile) {
              if (!GetWaveformValueFromFile(control_name->waveformDir, control_name->waveformRootname,
                                            control_name->ControlName[i],
                                            &control_name->channelInfo[i])) {
                fprintf(stderr, "Error in reading waveform values from file.\n");
                exit(1);
              }
            }
          }
          SDDS_CopyString(&control_name->ValueString[count], ValueString[i]);
        } else
          control_name->ValueString[count] = (char *)malloc(sizeof(char) * 250);
        control_name->valid[count] = 1;
        control_name->newFlag[count] = 1;
        count++;
      }
    }
    free(existed);
    if (hasWaveform)
      control_name->hasWaveform = 1;
    if (control_name->carestore && hasWaveform && control_name->oneWaveformFile) {
      /* read waveform values from one file */
      GetWaveformValuesFromOneFile(control_name);
    }
  }
  /*setup ca connections */
  if (control_name->logFp)
    fprintf(control_name->logFp, "%s, Connecting to CA...\n", getHourMinuteSecond());
  SetupCAConnection(control_name);
  if (control_name->logFp)
    fprintf(control_name->logFp, "%s, Connecting to CA done\n", getHourMinuteSecond());
  return 1;
}

char *getWaveformFileName(CONTROL_NAME *control_name, char *outputFile, char *waveformPV) {
  char vectorOut[1024], *filename = NULL;
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
  /* SDDS_CopyString(&filename, vectorOut); */
  SDDS_CopyString(&filename, vectorOut);
  if (!control_name->fullname) {
    char *token = NULL;
    token = strtok(filename, "/");
    while (token != NULL) {
      filename = token;
      token = strtok(NULL, "/");
    }
  }
  return filename;
}
long ReadInputAndMakeCAConnections(CONTROL_NAME *control_name, char *inputFile, char *outputFile, SDDS_DATASET *outTable) {
  char **ControlName, **IndirectName;
  char **ValueString;
  SDDS_DATASET inSet;
  short valuestringExist = 0, IndirectNameExist = 0;
  long i;
  long variables;

  ControlName = IndirectName = ValueString = NULL;

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
  if (SDDS_CheckColumn(&inSet, "ControlName", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
    fprintf(stderr, "Column \"ControlName\" does not exist or something wrong with it in input file %s\n", inputFile);
    return 0;
  }
  if (SDDS_CheckColumn(&inSet, "IndirectName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    IndirectNameExist = 1;
  if (SDDS_CheckColumn(&inSet, "ValueString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    valuestringExist = 1;

  if (0 >= SDDS_ReadPage(&inSet)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    return 0;
  }
  if (control_name->unique && !SDDS_UnsetDupicateRows(&inSet))
    SDDS_Bomb("Error in unsetting duplicate rows of the input file");
  if (!(variables = SDDS_CountRowsOfInterest(&inSet))) {
    return 1;
  }
  if (!(ControlName = (char **)SDDS_GetColumn(&inSet, "ControlName")))
    SDDS_Bomb("No data provided in the input file.");
  if (IndirectNameExist)
    if (!(IndirectName = (char **)SDDS_GetColumn(&inSet, "IndirectName")))
      SDDS_Bomb("Unable to get the vaue of IndirectName column in the input file");
  if (valuestringExist) {
    if (!(ValueString = (char **)SDDS_GetColumn(&inSet, "ValueString")))
      SDDS_Bomb("Unable to get the vaue of ValueString column in the input file.");
    for (i = 0; i < variables; i++) {
      /* ca_put does not accept quotes, has to remove it before restore */
      if (ValueString[i][0] == '\"' && ValueString[i][strlen(ValueString[i]) - 1] == '\"') {
        strslide(ValueString[i], -1);
        ValueString[i][strlen(ValueString[i]) - 1] = '\0';
      }
    }
  }
  if (control_name->carestore && !valuestringExist)
    SDDS_Bomb("The ValueString column is missing in the input file, unable to restore pv values!");
  if (control_name->casave) {
    if (!SDDS_InitializeCopy(outTable, &inSet, outputFile, "w"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!WriteOutputLayout(outTable, &inSet))
      SDDS_Bomb("Error in writing output layout!");
    if (!SDDS_CopyPage(outTable, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!control_name->waveformRootGiven && control_name->carestore) {
    if (!SDDS_GetParameter(&inSet, "SnapshotFilename", &control_name->waveformRootname))
      SDDS_CopyString(&control_name->waveformRootname, inputFile);
  }
  if (!SDDS_Terminate(&inSet))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  /*update the data info from the input */
  if (!UpdateDataInfo(control_name, variables, ControlName, ValueString, IndirectName, inputFile))
    return 0;
  for (i = 0; i < variables; i++) {
    free(ControlName[i]);
    if (IndirectName && IndirectName[i])
      free(IndirectName[i]);
    if (ValueString && ValueString[i])
      free(ValueString[i]);
  }
  free(ControlName);
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

  if (SDDS_CheckColumn(inTable, "IndirectName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    indirectExist = 1;
  if (SDDS_CheckColumn(inTable, "ValueString", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    valuestringExist = 1;
  if (SDDS_CheckColumn(inTable, "CAError", NULL, SDDS_CHARACTER, NULL) == SDDS_CHECK_OK)
    CAErrorExist = 1;
  if (SDDS_CheckParameter(inTable, "TimeStamp", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    timestamp = 1;
  if (SDDS_CheckParameter(inTable, "StartTime", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    starttime = 1;
  if (SDDS_CheckParameter(inTable, "Time", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK) {
    long index, type;
    char *units = NULL;
    index = SDDS_GetParameterIndex(inTable, "Time");
    type = SDDS_GetParameterType(inTable, index);
    if (type != SDDS_DOUBLE) {
      if (!SDDS_ChangeParameterInformation(outTable, "type", "double",
                                           SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME,
                                           "Time")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (!SDDS_GetParameterInformation(inTable, "units", &units, SDDS_GET_BY_NAME, "Time"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!units || strcmp(units, "s")) {
      if (!SDDS_ChangeParameterInformation(outTable, "units", "s", SDDS_PASS_BY_STRING | SDDS_SET_BY_NAME, "Time")) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (units)
      free(units);
    timeFlag = 1;
  }
  if (SDDS_CheckParameter(inTable, "LoginID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    loginid = 1;
  if (SDDS_CheckParameter(inTable, "EffectiveUID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    effectiveid = 1;
  if (SDDS_CheckParameter(inTable, "GroupID", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    groupid = 1;
  if (SDDS_CheckParameter(inTable, "SnapType", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    snaptype = 1;
  if (SDDS_CheckParameter(inTable, "SnapshotDescription", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    description = 1;
  if (SDDS_CheckParameter(inTable, "SnapshotFilename", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    snapshot = 1;
  if (SDDS_CheckParameter(inTable, "RequestFile", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    request = 1;
  if (SDDS_CheckColumn(inTable, "Count", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK)
    countExist = 1;
  if (SDDS_CheckColumn(inTable, "Lineage", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    LineageExist = 1;
  if (SDDS_CheckParameter(inTable, "ElapsedTimeToCAConnect", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    elapsedTimeCAExists = 1;
  if (SDDS_CheckParameter(inTable, "ElapsedTimeToSave", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    elapsedTimeSaveExists = 1;
  if (SDDS_CheckParameter(inTable, "PendIOTime", NULL, SDDS_DOUBLE, NULL) == SDDS_CHECK_OK)
    pendiotimeExists = 1;
  if (!countExist && SDDS_DefineColumn(outTable, "Count", NULL, NULL, NULL, NULL, SDDS_LONG, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!LineageExist && SDDS_DefineColumn(outTable, "Lineage", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!indirectExist && SDDS_DefineColumn(outTable, "IndirectName", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!valuestringExist && SDDS_DefineColumn(outTable, "ValueString", NULL, NULL, NULL, NULL, SDDS_STRING, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!CAErrorExist && SDDS_DefineColumn(outTable, "CAError", NULL, NULL, NULL, NULL, SDDS_CHARACTER, 0) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!timestamp && SDDS_DefineParameter(outTable, "TimeStamp", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!starttime && SDDS_DefineParameter(outTable, "StartTime", NULL, NULL, NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!timeFlag && SDDS_DefineParameter(outTable, "Time", NULL, "s", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!snaptype && SDDS_DefineParameter(outTable, "SnapType", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!description && SDDS_DefineParameter(outTable, "SnapshotDescription", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!request && SDDS_DefineParameter(outTable, "RequestFile", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!snapshot && SDDS_DefineParameter(outTable, "SnapshotFilename", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!loginid && SDDS_DefineParameter(outTable, "LoginID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!effectiveid && SDDS_DefineParameter(outTable, "EffectiveUID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!groupid && SDDS_DefineParameter(outTable, "GroupID", NULL, NULL, NULL, NULL, SDDS_STRING, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!elapsedTimeCAExists && SDDS_DefineParameter(outTable, "ElapsedTimeToCAConnect", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!elapsedTimeSaveExists && SDDS_DefineParameter(outTable, "ElapsedTimeToSave", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!pendiotimeExists && SDDS_DefineParameter(outTable, "PendIOTime", NULL, "seconds", NULL, NULL, SDDS_DOUBLE, NULL) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return 1;
}

long SetupCAConnection(CONTROL_NAME *control_name) {
  long j, variables, validCount, newCount = 0, i;
  char **ControlName;
  CHANNEL_INFO *channelInfo;
  double starttime, endtime;

  ControlName = control_name->ControlName;
  channelInfo = control_name->channelInfo;

  variables = control_name->variables;
  validCount = 0;

  if (!control_name->channelInfo)
    SDDS_Bomb("Memory has not been allocated for channelID.\n");
  getTimeBreakdown(&starttime, NULL, NULL, NULL, NULL, NULL, NULL);
  for (j = 0; j < variables; j++) {
    if (control_name->valid[j]) {
      validCount++;
      if (control_name->newFlag[j]) {
        channelInfo[j].flag = 0;
        if (ca_search(ControlName[j], &(channelInfo[j].channelID)) != ECA_NORMAL) {
          if (control_name->verbose)
            fprintf(stderr, "error: problem doing search for %s\n",
                    control_name->ControlName[j]);
          else if (control_name->logFp)
            fprintf(control_name->logFp, "error: problem doing search for %s\n",
                    control_name->ControlName[j]);
          control_name->CAError[j] = 'y';
        } else
          control_name->CAError[j] = 'n';
        newCount++;
      }
    }
  }
  if (!newCount)
    return 0;
  if (ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
    if (control_name->verbose)
      fprintf(stderr, "Error, Unable to setup connections for some pvs during the timeout %f seconds.\n", control_name->pendIOTime);
    else if (control_name->logFp)
      fprintf(control_name->logFp, "Error, Unable to setup connections for some pvs during the timeout %f seconds.\n", control_name->pendIOTime);
  }
  getTimeBreakdown(&endtime, NULL, NULL, NULL, NULL, NULL, NULL);
  control_name->ca_connect_time = endtime - starttime;
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->CAError[i] == 'y') {
      if (control_name->logFp)
        fprintf(control_name->logFp, "Error: unable to set up connection for %s\n", control_name->ControlName[i]);
      else
        fprintf(stderr, "Error: unable to setup connection for %s PV\n", control_name->ControlName[i]);
      continue;
    }
    control_name->channelInfo[i].length = ca_element_count(channelInfo[i].channelID);
    control_name->channelInfo[i].isEnumPV = 0;
    if (control_name->newFlag[i]) {
      if ((control_name->channelInfo[i].length == 1) || control_name->casave) {
        switch (ca_field_type(control_name->channelInfo[i].channelID)) {
        case DBF_STRING:
          control_name->channelInfo[i].dataType = SDDS_STRING;
          break;
        case DBF_ENUM:
          control_name->channelInfo[i].isEnumPV = 1;
          if (control_name->numerical)
            control_name->channelInfo[i].dataType = SDDS_DOUBLE;
          else
            control_name->channelInfo[i].dataType = SDDS_STRING;
          break;
        case DBF_DOUBLE:
          control_name->channelInfo[i].dataType = SDDS_DOUBLE;
          break;
        case DBF_FLOAT:
          control_name->channelInfo[i].dataType = SDDS_FLOAT;
          break;
        case DBF_LONG:
          control_name->channelInfo[i].dataType = SDDS_LONG;
          break;
        case DBF_SHORT:
          control_name->channelInfo[i].dataType = SDDS_SHORT;
          break;
        case DBF_CHAR:
          control_name->channelInfo[i].dataType = SDDS_CHARACTER;
          break;
        case DBF_NO_ACCESS:
          fprintf(stderr, "Error: No access to PV %s\n", control_name->ControlName[i]);
          exit(1);
          break;
        default:
          /*fprintf(stderr, "Error: invalid datatype of PV %s\n",control_name->ControlName[i]);*/
          control_name->CAError[i] = 'y';
          /*
                    if (control_name->verbose)
                    fprintf(stderr,"Error: unable to set up connection for %s\n",control_name->ControlName[i]);
                    else if (control_name->logFp) 
                    fprintf(control_name->logFp,"Error: unable to setup connection for %s PV\n",control_name->ControlName[i]); */
          break;
        }
      }
    }
    if (control_name->newFlag[i] && control_name->casave) {
      if (control_name->channelInfo[i].length > 1) {
        SDDS_CopyString(&control_name->IndirectName[i], control_name->ControlName[i]);
        control_name->hasWaveform = 1;
        if (control_name->oneWaveformFile) {
          if (control_name->waveformDataType <= 0 || (SizeOfDataType(control_name->waveformDataType) <= SizeOfDataType(control_name->channelInfo[i].dataType)))
            control_name->waveformDataType = control_name->channelInfo[i].dataType;
        } else {
          control_name->channelInfo[i].waveformData = tmalloc(SizeOfDataType(control_name->channelInfo[i].dataType) * control_name->channelInfo[i].length);
        }
      } else
        SDDS_CopyString(&control_name->IndirectName[i], "-");
    }
  }
  if (control_name->casave && control_name->hasWaveform && control_name->oneWaveformFile) {
    for (i = 0; i < control_name->variables; i++) {
      if (control_name->channelInfo[i].length > 1) {
        if (control_name->channelInfo[i].waveformData)
          free(control_name->channelInfo[i].waveformData);
        control_name->channelInfo[i].dataType = control_name->waveformDataType;
        control_name->channelInfo[i].waveformData = tmalloc(SizeOfDataType(control_name->waveformDataType) * control_name->channelInfo[i].length);
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
    if (control_name->IndirectName[i])
      free(control_name->IndirectName[i]);
    if (control_name->ValueString[i])
      free(control_name->ValueString[i]);
    if (control_name->channelInfo[i].length > 1) {
      if (control_name->channelInfo[i].waveformData)
        free(control_name->channelInfo[i].waveformData);
      if (control_name->channelInfo[i].bufferCopy)
        free(control_name->channelInfo[i].bufferCopy);
      if (control_name->channelInfo[i].stringValue)
        for (j = 0; j < control_name->channelInfo[i].length; j++)
          free(control_name->channelInfo[i].stringValue[j]);
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
  free(control_name->IndirectName);
  free(control_name->ValueString);
  free(control_name->newFlag);
  free(control_name->valid);
  free(control_name->channelInfo);
  if (control_name->casavePV)
    free(control_name->casavePV);
  if (control_name->descriptPV)
    free(control_name->descriptPV);
  if (control_name->inputFilePV)
    free(control_name->inputFilePV);
}

long GetWaveformValuesFromOneFile(CONTROL_NAME *control_name) {
  char vectorFile[1024];
  SDDS_TABLE inSet;
  long index, i, j;
  long waveforms = 0, waveformDataType = 0;
  long *waveformLength = NULL;
  void **waveformData = NULL;
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
  if ((index = SDDS_GetColumnIndex(&inSet, "Waveform")) < 0) {
    fprintf(stderr, "column Waveform does not exist in waveform file %s\n", vectorFile);
    exit(1);
  }
  waveformDataType = SDDS_GetColumnType(&inSet, index);
  while (SDDS_ReadPage(&inSet) > 0) {
    if (!(waveformData = SDDS_Realloc(waveformData, sizeof(*waveformData) * (waveforms + 1))) ||
        !(waveformPV = SDDS_Realloc(waveformPV, sizeof(*waveformPV) * (waveforms + 1))) ||
        !(waveformLength = SDDS_Realloc(waveformLength, sizeof(*waveformLength) * (waveforms + 1))))
      SDDS_Bomb("memory allocation failure");
    if ((waveformLength[waveforms] = SDDS_RowCount(&inSet)) <= 0)
      continue;

    if (!(waveformData[waveforms] = SDDS_GetColumn(&inSet, "Waveform")) ||
        !SDDS_GetParameter(&inSet, "WaveformPV", waveformPV + waveforms))
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
      channelInfo->length = waveformLength[index];
      channelInfo->dataType = waveformDataType;
      if (waveformDataType == SDDS_STRING) {
        channelInfo->stringValue = (char **)malloc(sizeof(*(channelInfo->stringValue)) * channelInfo->length);
        for (j = 0; j < channelInfo->length; j++) {
          SDDS_CopyString(&(channelInfo->stringValue[j]), ((char **)(waveformData[index]))[j]);
          free(((char **)(waveformData[index]))[j]);
        }
        free((char **)(waveformData[index]));
        channelInfo->bufferCopy = (char *)malloc(sizeof(char) * 40 * (channelInfo->length));
        for (i = 0; i < channelInfo->length; i++) {
          for (j = 0; j < strlen(channelInfo->stringValue[i]); j++)
            channelInfo->bufferCopy[i * 40 + j] = channelInfo->stringValue[i][j];
          channelInfo->bufferCopy[i * 40 + j] = '\0';
        }
      } else {
        channelInfo->waveformData = malloc(SizeOfDataType(waveformDataType) * channelInfo->length);
        memcpy(channelInfo->waveformData, waveformData[index], SizeOfDataType(waveformDataType) * channelInfo->length);
      }
    }
  }
  free(waveformLength);
  for (i = 0; i < waveforms; i++) {
    free(waveformPV[i]);
    free(waveformData[i]);
  }
  free(waveformData);

  return 1;
}

long GetWaveformValueFromFile(char *directory, char *rootname, char *waveformPV, CHANNEL_INFO *channelInfo) {
  char vectorFile[1024];
  SDDS_TABLE inSet;
  long index, exist = 0, i, j;
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
    if (!SDDS_GetParameter(&inSet, "WaveformPV", &parameterName))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (strcmp(parameterName, waveformPV) != 0)
      continue;
    channelInfo->length = SDDS_CountRowsOfInterest(&inSet);
    if ((index = SDDS_GetColumnIndex(&inSet, "Waveform")) < 0) {
      fprintf(stderr, "\"Waveform\" column does not exist.!");
      exit(1);
    }
    channelInfo->dataType = SDDS_GetColumnType(&inSet, index);
    if (channelInfo->dataType == SDDS_STRING) {
      if (!(channelInfo->stringValue = (char **)SDDS_GetColumn(&inSet, "Waveform")))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      channelInfo->bufferCopy = (char *)malloc(sizeof(char) * 40 * (channelInfo->length));
      for (i = 0; i < channelInfo->length; i++) {
        for (j = 0; j < strlen(channelInfo->stringValue[i]); j++)
          channelInfo->bufferCopy[i * 40 + j] = channelInfo->stringValue[i][j];
        channelInfo->bufferCopy[i * 40 + j] = '\0';
      }
    } else if (!(channelInfo->waveformData = SDDS_GetColumn(&inSet, "Waveform")))
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

void sddscasaverestore(CONTROL_NAME *control_name, char *inputFile, char *outputRoot,
                       long dailyFiles, char *semaphoreFile, long daemon, long add, long dryRun) {
  char *outputFile = NULL;
  char *prevInput = NULL;
  char *linkname = NULL;
  long firstTime = 1, settozero = 0;
  struct stat input_stat;
  SDDS_DATASET outTable;

  if (semaphoreFile && fexists(semaphoreFile))
    remove(semaphoreFile);
  if (inputFile) {
    linkname = read_last_link_to_file(inputFile);
    if (get_file_stat(inputFile, linkname, &input_stat) != 0) {
      fprintf(stderr, "Problem getting modification time for %s\n", inputFile);
      exit(1);
    }
  }
  if (control_name->casave) {
    if (dailyFiles)
      outputFile = MakeSCRDailyTimeGenerationFilename(outputRoot);
    else
      outputFile = outputRoot;
  }
  if (!ReadInputAndMakeCAConnections(control_name, inputFile, outputFile, &outTable))
    SDDS_Bomb("Error in reading input file and make ca connections!");
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
    strcpy(rcParam.message, "sddscasr started");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable tp write status message and alarm severity\n");
      exit(1);
    }
  }
#endif
#if !defined(_WIN32)
  if (daemon && !control_name->casavePV)
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
            strcpy(rcParam.message, "sddscasr sleeping");
            rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
            if (rcParam.status != RUNCONTROL_OK) {
              fprintf(stderr, "Unable tp write status message and alarm severity\n");
              exit(1);
            }
          }
#endif
          if (control_name->casavePV) {
            if (ca_state(control_name->casavePV_ID) != cs_conn) {
              fprintf(stderr, "Error, no connection for %s\n", control_name->casavePV);
              exit(1);
            }
            if (ca_get(DBR_LONG, control_name->casavePV_ID, &SignalEventArrived) != ECA_NORMAL ||
                ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
              fprintf(stderr, "Error, unable to get value of %s\n", control_name->casavePV);
              exit(1);
            }
          }
          if (!SignalEventArrived)
            oag_ca_pend_event(control_name->interval, &sigint);
          if (sigint)
            exit(1);
        }
        if (semaphoreFile && fexists(semaphoreFile))
          remove(semaphoreFile);

        if (control_name->casave) {
          if (dailyFiles)
            outputFile = MakeSCRDailyTimeGenerationFilename(outputRoot);
          else
            outputFile = outputRoot;
        }
        /*set outputfile pv to an empty string */
        if (outputFile && control_name->outputFilePV) {
          char charData[1];
          charData[0] = 0;
          if (ca_array_put(DBR_CHAR,
                           ca_element_count(control_name->outputFilePV_ID),
                           control_name->outputFilePV_ID,
                           charData) != ECA_NORMAL ||
              ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
            fprintf(stderr, "Error, unable to set value of %s", control_name->outputFilePV);
            exit(1);
          }
        }
        if (inputFile)
          SDDS_CopyString(&prevInput, inputFile);
        if (control_name->inputFilePV)
          read_char_waveform(&inputFile, control_name->inputFilePV_ID);
        if (strcmp(inputFile, prevInput) != 0) {
          if (linkname)
            free(linkname);
          linkname = read_last_link_to_file(inputFile);
          get_file_stat(inputFile, linkname, &input_stat);
        }

        if (strcmp(inputFile, prevInput) != 0 ||
            file_is_modified(inputFile, &linkname, &input_stat) || control_name->carestore) {
          if (!ReadInputAndMakeCAConnections(control_name, inputFile, outputFile, &outTable))
            SDDS_Bomb("Error in reading input file");
        }
        /*
                if (InputfileModified(inputFile,prevInput,&input_stat) || control_name->carestore) { 
                if (!ReadInputAndMakeCAConnections(control_name,inputFile,outputFile,&outTable))
                SDDS_Bomb("Error in reading input file");
                }*/
        if (inputFile) {
          if (prevInput)
            free(prevInput);
          prevInput = NULL;
          SDDS_CopyString(&prevInput, inputFile);
        }
      }
    }
#ifdef USE_RUNCONTROL
    if (rcParam.PV) {
      if (runControlPingWhileSleep(0.01, control_name)) {
        fprintf(stderr, "Problem pinging the run control record.\n");
        exit(1);
      }
      strcpy(rcParam.message, "sddscasr taking data");
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
    if (control_name->casave) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, reading data ...\n", getHourMinuteSecond());
      if (!ReadDataAndSave(control_name, &outTable, inputFile, outputFile, firstTime))
        SDDS_Bomb("Error in reading data and saving into output file");
    }
    if (control_name->carestore) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, setting values ...\n", getHourMinuteSecond());
      if (add) {
	/*read current values, and add it to value provided by the input file */
	ReadPVValue(control_name, add, NULL);
      }
      if (!SetPVValue(control_name, dryRun))
        SDDS_Bomb("Error in setting pv data");
    }
    if (control_name->casave) {
      if (control_name->logFp)
        fprintf(control_name->logFp, "%s, wrote to file %s\n", getHourMinuteSecond(), outputFile);
    }
    if (control_name->carestore) {
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
      char charData[500];
      long i;
      sprintf(charData, "%s", outputFile);
      for (i = strlen(outputFile); i < ca_element_count(control_name->outputFilePV_ID); i++)
        charData[i] = '\0';
      if (ca_array_put(DBR_CHAR,
                       ca_element_count(control_name->outputFilePV_ID),
                       control_name->outputFilePV_ID,
                       charData) != ECA_NORMAL ||
          ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "Error1, unable to set value of %s, inputfile=%s\n", control_name->outputFilePV, inputFile);
        exit(1);
      }
    }
    if (outputFile && dailyFiles)
      free(outputFile);
    if (control_name->logFp)
      fflush(control_name->logFp);
    if (control_name->casavePV) {
      if (ca_put(DBR_LONG, control_name->casavePV_ID, &settozero) != ECA_NORMAL ||
          ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "Error, unable to set value of %s\n", control_name->casavePV);
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
      strcpy(rcParam.message, "sddscasr data taking done");
      rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Unable tp write status message and alarm severity\n");
        exit(1);
      }
    }
#endif
  } while (daemon);
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

long ReadPVValue(CONTROL_NAME *control_name, long add, char *outputFile) {
  long i, caErrors=0;
  CHANNEL_INFO *channelInfo;

  channelInfo = control_name->channelInfo;

  /*read data*/
  for (i = 0; i < control_name->variables; i++) {
    caErrors = 0;
    if (control_name->valid[i]) {
      if (ca_state(channelInfo[i].channelID) != cs_conn || control_name->CAError[i] == 'y') {
        if (control_name->verbose)
          fprintf(stderr, "Error, no connection for %s\n", control_name->ControlName[i]);
        else if (control_name->logFp)
          fprintf(control_name->logFp, "Error, no connection for %s\n", control_name->ControlName[i]);
        free(control_name->ValueString[i]);
        SDDS_CopyString(&control_name->ValueString[i], "_NoConnection_");
        control_name->CAError[i] = 'y';
        continue;
      }
      if (channelInfo[i].length > 1) {
        control_name->hasWaveform = 1;
        if (channelInfo[i].dataType == SDDS_STRING) {
          if (!channelInfo[i].bufferCopy)
            channelInfo[i].bufferCopy = tmalloc(sizeof(char) * (40 * channelInfo[i].length));
          if (ca_array_get(DBR_STRING, channelInfo[i].length, channelInfo[i].channelID,
                           channelInfo[i].bufferCopy) != ECA_NORMAL)
            caErrors = 1;
        } else {
          if (ca_array_get(DataTypeToCaType(channelInfo[i].dataType),
                           channelInfo[i].length,
                           channelInfo[i].channelID,
                           channelInfo[i].waveformData) != ECA_NORMAL)
            caErrors = 1;
        }
        if (caErrors) {
          if (control_name->verbose)
            fprintf(stderr, "Error, unable to get value of %s\n", control_name->ControlName[i]);
          else if (control_name->logFp)
            fprintf(control_name->logFp, "Error, unable to get value of %s\n", control_name->ControlName[i]);
          control_name->CAError[i] = 'y';
          free(control_name->ValueString[i]);
          SDDS_CopyString(&control_name->ValueString[i], "_NoValueAvailable_");
        } else {
          control_name->CAError[i] = 'n';
          free(control_name->ValueString[i]);
          if (!control_name->saveWaveformFile)
            SDDS_CopyString(&control_name->ValueString[i], "WaveformPV");
          else if (outputFile) 
	    SDDS_CopyString(&control_name->ValueString[i], getWaveformFileName(control_name, outputFile, control_name->IndirectName[i]));
        }
      } else {
        if (channelInfo[i].dataType == SDDS_STRING) {
          if (ca_get(DBR_STRING, channelInfo[i].channelID, control_name->ValueString[i]) != ECA_NORMAL)
            caErrors = 1;
        } else {
          if (ca_get(DBR_DOUBLE,
                     channelInfo[i].channelID, &control_name->doubleValue[i]) != ECA_NORMAL)
            caErrors = 1;
        }
        if (caErrors) {
          if (control_name->verbose)
            fprintf(stderr, "Error, unable to get value of %s\n", control_name->ControlName[i]);
          else if (control_name->logFp)
            fprintf(control_name->logFp, "Error, unable to get value of %s\n", control_name->ControlName[i]);
          if (control_name->ValueString[i])
            free(control_name->ValueString[i]);
          SDDS_CopyString(&control_name->ValueString[i], "_NoValueAvailable_");
          control_name->CAError[i] = 'y';
        } else
          control_name->CAError[i] = 'n';
      }
    }
  }

  if (ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
    for (i = 0; i < control_name->variables; i++) {
      if (control_name->valid[i] && control_name->CAError[i] == 'y') {
        if (control_name->verbose)
          fprintf(stderr, "Error: Can not read value of %s\n", control_name->ControlName[i]);
        else if (control_name->logFp)
          fprintf(control_name->logFp, "Error: Can not read value of %s\n", control_name->ControlName[i]);
      }
    }
  } else {
    for (i = 0; i < control_name->variables; i++) {
      if (channelInfo[i].length == 1) {
	if (add && channelInfo[i].dataType!=SDDS_STRING && control_name->ValueString[i]!=NULL)
	  control_name->doubleValue[i] += atof(control_name->ValueString[i]);
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
  return caErrors;
}


long ReadDataAndSave(CONTROL_NAME *control_name, SDDS_DATASET *outTable, char *inputFile, char *outputFile, long firstTime) {
  long i, rowcount = 0, j;
  char vectorOut[1024];
  char *TimeStamp, groupID[256], effectID[256];
  double RunStartTime, endTime;

  sprintf(groupID, "%ld", control_name->group_id);
  sprintf(effectID, "%ld", control_name->effective_uid);

  if (control_name->descriptPV)
    read_char_waveform(&control_name->description, control_name->descriptPV_ID);
  control_name->hasWaveform = 0;

  if (!firstTime) {
    SDDS_DATASET inSet;
    if (!SDDS_InitializeInput(&inSet, inputFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
    if (!SDDS_InitializeCopy(outTable, &inSet, outputFile, "w"))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (0 >= SDDS_ReadPage(&inSet)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      return 0;
    }
    if (!WriteOutputLayout(outTable, &inSet))
      SDDS_Bomb("Error in writing output layout!");
    if (!SDDS_CopyPage(outTable, &inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!SDDS_Terminate(&inSet))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  ReadPVValue(control_name, 0, outputFile);
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
  
  rowcount = 0;
  getTimeBreakdown(&endTime, NULL, NULL, NULL, NULL, NULL, NULL);
  if (!SDDS_SetParameters(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "ElapsedTimeToSave", endTime - RunStartTime,
                          "ElapsedTimeToCAConnect", control_name->ca_connect_time,
                          "PendIOTime", control_name->pendIOTime,
                          NULL))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i]) {
      if (!SDDS_SetRowValues(outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, control_name->rowIndex[i],
                             "ControlName", control_name->ControlName[i],
                             "ValueString", control_name->ValueString[i],
                             "CAError", control_name->CAError[i],
                             "IndirectName", control_name->IndirectName[i],
                             "Count", control_name->channelInfo[i].length,
                             "Lineage", "-", NULL))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (strlen(control_name->IndirectName[i]) && !is_blank(control_name->IndirectName[i]) && strcmp(control_name->IndirectName[i], "-")) {
        control_name->hasWaveform = 1;
        if (control_name->channelInfo[i].dataType == SDDS_STRING) {
          for (j = 0; j < control_name->channelInfo[i].length; j++)
            ((char **)control_name->channelInfo[i].waveformData)[j] = (char *)&(control_name->channelInfo[i].bufferCopy[40 * j]);
        }
        if (!outputFile && !control_name->waveformRootname) {
          fprintf(stderr, "The output root or the waveform rootname is not provided for saving waveform PV data (sddscasr).\n");
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

long SetPVValue(CONTROL_NAME *control_name, long dryRun) {
  long i, caErrors = 0;
  CHANNEL_INFO *channelInfo;
  char **VS;
  struct dbr_ctrl_double *DV;
  char format[20];
  channelInfo = control_name->channelInfo;

  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i]) {
      if (ca_state(channelInfo[i].channelID) != cs_conn) {
        fprintf(stderr, "Error, no connection for %s\n", control_name->ControlName[i]);
        if (control_name->logFp)
          fprintf(control_name->logFp, "Error, no connection for %s\n", control_name->ControlName[i]);
        control_name->CAError[i] = 'y';
        continue;
      }
      if (control_name->channelInfo[i].length > 1) {
        caErrors = 0;
	if (dryRun) {
	  fprintf(stdout, "pretending writing waveform PV %s\n", control_name->ControlName[i]);
	  control_name->CAError[i] = 'n';
	} else {
	  if (control_name->channelInfo[i].dataType == SDDS_STRING) {
	    if (ca_array_put(DataTypeToCaType(control_name->channelInfo[i].dataType),
			     control_name->channelInfo[i].length,
			     control_name->channelInfo[i].channelID,
			     control_name->channelInfo[i].bufferCopy) != ECA_NORMAL)
	      caErrors = 1;
	  } else {
	    if (ca_array_put(DataTypeToCaType(control_name->channelInfo[i].dataType),
			     control_name->channelInfo[i].length,
			     control_name->channelInfo[i].channelID,
			     control_name->channelInfo[i].waveformData) != ECA_NORMAL)
	      caErrors = 1;
	  }
	  if (caErrors) {
	    fprintf(stderr, "Error, unable to set value of %s\n", control_name->ControlName[i]);
	    if (control_name->logFp)
	      fprintf(control_name->logFp, "Error, unable to set value of %s\n", control_name->ControlName[i]);
	    control_name->CAError[i] = 'y';
	  } else
	    control_name->CAError[i] = 'n';
	}
      } else {
	if (dryRun) {
	  fprintf(stdout, "pretending writing value %s to pv %s\n", control_name->ValueString[i], control_name->ControlName[i]);
	  control_name->CAError[i] = 'n';
	} else {
	  if (ca_put(DBR_STRING, channelInfo[i].channelID, control_name->ValueString[i]) != ECA_NORMAL) {
	    fprintf(stderr, "Error, unable to set value of %s\n", control_name->ControlName[i]);
	    if (control_name->logFp)
	      fprintf(control_name->logFp, "Error, unable to set value of %s\n", control_name->ControlName[i]);
	    control_name->CAError[i] = 'y';
	  } else
	    control_name->CAError[i] = 'n';
	}
      }
    }
  }

  if (ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
    for (i = 0; i < control_name->variables; i++) {
      if (control_name->valid[i] && control_name->CAError[i] == 'n') {
        if (ca_state(channelInfo[i].channelID) != cs_conn) {
          control_name->CAError[i] = 'y';
          fprintf(stderr, "Error: Can not set value for %s\n", control_name->ControlName[i]);
          if (control_name->logFp)
            fprintf(control_name->logFp, "Error: Can not set value for %s\n", control_name->ControlName[i]);
        }
      }
    }
  }
  /*new FPGA sectors 19,20,21,22 are installed behond a gateway, to make sure that gateway excute the setting command before exiting
    ca_pend_event() is required, H. Shang, 5/24/2013*/
  oag_ca_pend_event(0.5, &sigint);
  if (sigint) {
    fprintf(stderr, "error: problem doing put for some channels.\n");
    exit(1);
  }

  if (control_name->verify) {
    VS = (char **)malloc(sizeof(char *) * control_name->variables);
    DV = (struct dbr_ctrl_double *)malloc(sizeof(struct dbr_ctrl_double) * control_name->variables);
    for (i = 0; i < control_name->variables; i++) {
      VS[i] = (char *)malloc(sizeof(char) * 250);
      if (control_name->valid[i] && control_name->CAError[i] == 'n') {
        caErrors = 0;
        /*No waveform checking yet*/
        if (control_name->channelInfo[i].length == 1) {
          if (control_name->channelInfo[i].dataType == SDDS_STRING) {
            if (ca_get(DBR_STRING, channelInfo[i].channelID, VS[i]) != ECA_NORMAL)
              caErrors = 1;
          } else {
            if (ca_get(DBR_CTRL_DOUBLE,
                       channelInfo[i].channelID, &DV[i]) != ECA_NORMAL)
              caErrors = 1;
          }
          if (caErrors) {
            fprintf(stderr, "Error, unable to get value of %s\n", control_name->ControlName[i]);
            if (control_name->logFp)
              fprintf(control_name->logFp, "Error, unable to get value of %s\n", control_name->ControlName[i]);
            control_name->CAError[i] = 'y';
          }
        }
      }
    }

    if (ca_pend_io(control_name->pendIOTime) != ECA_NORMAL) {
      for (i = 0; i < control_name->variables; i++) {
        if (control_name->valid[i] && control_name->CAError[i] == 'n') {
          if (ca_state(channelInfo[i].channelID) != cs_conn) {
            control_name->CAError[i] = 'y';
            fprintf(stderr, "Error: Can not read value of %s\n", control_name->ControlName[i]);
            if (control_name->logFp)
              fprintf(control_name->logFp, "Error: Can not read value of %s\n", control_name->ControlName[i]);
          }
        }
      }
    }

    for (i = 0; i < control_name->variables; i++) {
      if (control_name->valid[i] && control_name->CAError[i] == 'n') {
        if (channelInfo[i].length == 1) {
          switch (channelInfo[i].dataType) {
          case SDDS_DOUBLE:
            if (channelInfo[i].isEnumPV) {
              sprintf(VS[i], "%.0f", DV[i].value);
            } else {
              sprintf(VS[i], "%.13le", DV[i].value);
              sprintf(control_name->ValueString[i], "%.13le", atof(control_name->ValueString[i]));
              if ((strcmp(control_name->ValueString[i], VS[i]) != 0) && (DV[i].precision > 0)) {
                sprintf(format, "%%.%dlf", (dbr_short_t)DV[i].precision);
                sprintf(VS[i], format, DV[i].value);
                sprintf(control_name->ValueString[i], format, atof(control_name->ValueString[i]));
              }
            }
            break;
          case SDDS_FLOAT:
            sprintf(VS[i], "%.3le", DV[i].value);
            sprintf(control_name->ValueString[i], "%.3le", atof(control_name->ValueString[i]));
            if ((strcmp(control_name->ValueString[i], VS[i]) != 0) && (DV[i].precision > 0)) {
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
          if (strcmp(control_name->ValueString[i], VS[i]) != 0) {
            fprintf(stderr, "Warning: PV did not accept value change (%s)\n", control_name->ControlName[i]);
            fprintf(stderr, "         SCR value=%s, current value=%s\n", control_name->ValueString[i], VS[i]);
            if (control_name->logFp) {
              fprintf(control_name->logFp, "Warning: PV did not accept value change (%s)\n", control_name->ControlName[i]);
              fprintf(control_name->logFp, "         SCR value=%s, current value=%s\n", control_name->ValueString[i], VS[i]);
            }
          }
        }
      }
    }

    for (i = 0; i < control_name->variables; i++) {
      if (VS[i]) {
        free(VS[i]);
      }
    }
    free(VS);
    free(DV);
  }
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
      !SDDS_DefineSimpleColumn(&outTable, "Waveform", NULL, control_name->waveformDataType))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&outTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < control_name->variables; i++) {
    if (control_name->valid[i] && control_name->channelInfo[i].length > 1) {
      index = (int32_t *)malloc(sizeof(*index) * control_name->channelInfo[i].length);
      for (j = 0; j < control_name->channelInfo[i].length; j++)
        index[j] = j;
      if (!SDDS_StartPage(&outTable, control_name->channelInfo[i].length))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_SetColumn(&outTable, SDDS_BY_NAME,
                          control_name->channelInfo[i].waveformData, control_name->channelInfo[i].length, "Waveform") ||
          !SDDS_SetColumn(&outTable, SDDS_BY_NAME,
                          index, control_name->channelInfo[i].length, "Index") ||
          !SDDS_SetParameters(&outTable, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "TimeStamp", TimeStamp,
                              "Time", RunStartTime,
                              "WaveformPV", control_name->ControlName[i],
                              NULL) ||
          !SDDS_WritePage(&outTable))
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
        fprintf(stderr, "Run control sddscasr aborted.\n");
      if (control_name->logFp) {
        fprintf(control_name->logFp, "Run control sddscasr aborted.\n");
        fclose(control_name->logFp);
      }
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      if (control_name->verbose)
        fprintf(stderr, "Run control sddscasr timed out.\n");
      if (control_name->logFp) {
        fprintf(control_name->logFp, "Run control sddscasr timed out.\n");
        fclose(control_name->logFp);
      }
      strcpy(rcParam.message, "Sddscasr timed-out");
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
    SDDS_Bomb("Zero Rows defined in input file.");
  if (!(ControlName = (char **)SDDS_GetColumn(inSet, "ControlName")))
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

char *read_char_waveform(char **stringval, chid charWFID) {
  long length;

  if (*stringval)
    free(*stringval);
  *stringval = NULL;
  length = ca_element_count(charWFID);
  (*stringval) = malloc(sizeof(char *) * length);
  if (ca_array_get(DBR_CHAR, length, charWFID, *stringval) != ECA_NORMAL) {
    fprintf(stderr, "Unable to read the character waveform value!\n");
    exit(1);
  }
  ca_pend_io(10.0);
  if (strlen(*stringval) == length && (*stringval)[length - 1] != '\0') {
    *stringval = realloc(*stringval, sizeof(char) * (length + 1));
    (*stringval)[length] = '\0';
  }
  return *stringval;
}

void setup_other_caconnections(CONTROL_NAME *control_name) {
  long capendNeeded = 0;

  if (control_name->descriptPV) {
    if (ca_search(control_name->descriptPV, &control_name->descriptPV_ID) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", control_name->descriptPV);
      exit(1);
    }
    capendNeeded = 1;
  }
  if (control_name->casavePV) {
    if (ca_search(control_name->casavePV, &control_name->casavePV_ID) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", control_name->casavePV);
      exit(1);
    }
    capendNeeded = 1;
  }

  if (control_name->outputFilePV) {
    if (ca_search(control_name->outputFilePV, &control_name->outputFilePV_ID) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", control_name->outputFilePV);
      exit(1);
    }
    capendNeeded = 1;
  }
  if (control_name->inputFilePV) {
    if (ca_search(control_name->inputFilePV, &control_name->inputFilePV_ID) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", control_name->inputFilePV);
      exit(1);
    }
    capendNeeded = 1;
  }
  if (capendNeeded) {
    if (ca_pend_io(10.0) != ECA_NORMAL) {
      fprintf(stderr, "Unable to setup connections for %s.\n", control_name->descriptPV);
      exit(1);
    }
  }
  if (control_name->descriptPV) {
    if (ca_field_type(control_name->descriptPV_ID) != DBR_CHAR ||
        ca_element_count(control_name->descriptPV_ID) <= 1) {
      fprintf(stderr, "the description PV should be a character-waveform PV\n");
      exit(1);
    }
  }
  if (control_name->outputFilePV) {
    if (ca_field_type(control_name->outputFilePV_ID) != DBR_CHAR ||
        ca_element_count(control_name->outputFilePV_ID) <= 1) {
      fprintf(stderr, "the output file PV should be a character-waveform PV\n");
      exit(1);
    }
  }
  if (control_name->inputFilePV) {
    if (ca_field_type(control_name->inputFilePV_ID) != DBR_CHAR ||
        ca_element_count(control_name->inputFilePV_ID) <= 1) {
      fprintf(stderr, "the input file PV should be a character-waveform PV\n");
      exit(1);
    }
  }
}
