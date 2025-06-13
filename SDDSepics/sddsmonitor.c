/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program sddsmonitor.c
 * reads a sddsfile with a string columns defined as device names, (read) message,
 * and a name into which the value is written. The target name forms
 * columns in the output file.
 * the output file also has an elapsed time column
 * and a start time parameter.
 * Output page lists all variables at each time step.
 * Version 1.1 added the time of day column
 * Version 1.2 (April 94) added single shot and other time options
 * Version 1.3 (Oct. 94) changed time and interval options, added glitch options
 $Log: not supported by cvs2svn $
 Revision 1.22  2009/12/04 16:49:57  soliday
 Improved the last changes.

 Revision 1.21  2009/12/03 22:13:59  soliday
 Updated by moving the needed definitions from alarm.h and alarmString.h
 into this file directly. This was needed because of a problem with
 EPICS Base 3.14.11. Andrew Johnson said that the values for these
 definitions will not change in the future.

 Revision 1.20  2007/05/07 19:29:38  soliday
 Fixed problem using the dailyfiles option with the glitch option.

 Revision 1.19  2007/04/30 20:17:19  soliday
 Updated code to exit if it is unable to close the SDDS file at midnight before
 opening a new one.

 Revision 1.18  2006/12/14 22:25:49  soliday
 Updated a bunch of programs because SDDS_SaveLayout is now called by
 SDDS_WriteLayout and it is no longer required to be called directly.
 Also the AutoCheckMode is turned off by default now so I removed calls to
 SDDS_SetAutoCheckMode that would attempt to turn it off. It is now up to
 the programmer to turn it on in new programs until debugging is completed
 and then remove the call to SDDS_SetAutoCheckMode.

 Revision 1.17  2006/11/20 22:02:36  soliday
 Updated the usage message.

 Revision 1.16  2006/10/20 15:21:08  soliday
 Fixed problems seen by linux-x86_64.

 Revision 1.15  2006/07/06 15:46:37  soliday
 Updated to reduce the load on the runcontrol IOC.

 Revision 1.14  2005/11/08 22:05:04  soliday
 Added support for 64 bit compilers.

 Revision 1.13  2005/07/11 20:17:38  soliday
 Fixed problem exiting the program when the glitch option is used.

 Revision 1.12  2005/06/29 19:20:28  soliday
 Fixed problem with last change.

 Revision 1.11  2005/06/28 16:16:33  shang
 made ControlName as column description

 Revision 1.10  2005/04/21 15:15:34  soliday
 Added checking to ensure runControlPingWhileSleep commands returned without
 an error.

 Revision 1.9  2005/03/17 19:12:32  soliday
 Added runcontrol support to sddsmonitor

 Revision 1.8  2005/03/17 17:08:01  soliday
 Reindented code.

 Revision 1.7  2004/11/04 16:34:53  shang
 the watchInput feature now watches both the name of the final link-to file and the state of the input file.

 Revision 1.6  2004/09/27 16:19:00  soliday
 Added missing ca_task_exit call.

 Revision 1.5  2004/07/19 17:39:37  soliday
 Updated the usage message to include the epics version string.

 Revision 1.4  2004/07/15 19:38:34  soliday
 Replace sleep commands with ca_pend_event commands because Epics/Base 3.14.x
 has an inactivity timer that was causing disconnects from PVs when the
 log interval time was too large.

 Revision 1.3  2003/12/19 16:34:20  soliday
 Copied last change to another spot in the code where the CA catch up problem occurs.

 Revision 1.2  2003/12/18 20:32:33  soliday
 Fixed an issue where the logger would try to catch up after CA problems stopped

 Revision 1.1  2003/08/27 19:51:17  soliday
 Moved into subdirectory

 Revision 1.64  2003/04/21 18:40:23  soliday
 Increased the accuracy of the time interval.

 Revision 1.63  2003/03/04 19:13:11  soliday
 Changed the default pend time to 10 seconds.

 Revision 1.62  2002/10/31 15:47:25  soliday
 It now converts the old ezca option into a pendtimeout value.

 Revision 1.61  2002/10/15 21:52:19  soliday
 Modified sddsmonitor so that it does not use ezca anymore.

 Revision 1.60  2002/08/14 20:00:35  soliday
 Added Open License

 Revision 1.59  2002/07/01 21:12:11  soliday
 Fsync is no longer the default so it is now set after the file is opened.

 Revision 1.58  2002/04/08 17:08:32  borland
 Added required extra argument to MakeDailyGenerationFilename() calls.

 Revision 1.57  2001/12/13 17:17:31  borland
 No longer issues warning if -comment is used with -append.

 Revision 1.56  2001/10/16 21:06:55  shang
 replaced st_mtime by st_ctime to make it work for linking files too and fixed the problem that
 -enforceTimeLimt did not work properly for -append mode

 Revision 1.55  2001/10/05 16:23:36  shang
 fixed the problem that program did not sleep for timeInterval when
 tests failed at the beginning.

 Revision 1.54  2001/10/02 20:54:41  shang
 modified so that it flushes data when inhibited

 Revision 1.53  2001/10/02 17:16:12  shang
 modified main() such that it flushes data when the tests fail.

 Revision 1.52  2001/09/10 21:02:10  soliday
 Fixed Linux compiler warnings.

 Revision 1.51  2001/07/23 19:39:16  borland
 Moved inhibit PV check to right after the argument parsing so no output
 files are created.

 Revision 1.50  2001/07/23 19:19:00  borland
 Code for inhibit-PV check prior to main loop was revised so that the
 program exits if it doesn't have permission to run.  sddslogger and
 sddsalarmlog already behave this way.

 Revision 1.49  2001/07/10 16:21:26  soliday
 Added the -watchInput option so that the program with exit if the inputfile
 is changed.

 Revision 1.48  2001/05/03 19:53:45  soliday
 Standardized the Usage message.

 Revision 1.47  2000/06/20 15:57:44  borland
 Fixed some incorrect information about alarm triggeringn in the usage message.

 Revision 1.46  2000/04/20 15:58:59  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.45  2000/04/19 15:51:50  soliday
 Removed some solaris compiler warnings.

 Revision 1.44  2000/03/30 17:27:51  soliday
 Modified the usage statement so that it wasn't too long.

 Revision 1.43  2000/03/30 02:47:14  borland
 Added 'topage' qualifier to append option, so that program can append to
 last page in file or (as before) append a new page.

 Revision 1.42  2000/03/08 17:13:55  soliday
 Removed compiler warnings under Linux.

 Revision 1.41  1999/09/17 22:12:48  soliday
 This version now works with WIN32

 Revision 1.40  1999/08/25 17:56:32  borland
 Rewrote code for data logging inhibiting.  Collected code in SDDSepics.c

 Revision 1.39  1999/08/24 13:42:33  borland
 Fixed another message (about PV inhibit).

 Revision 1.38  1999/08/23 14:43:45  borland
 Add/modified -inhibitPV option:
 For sddsstatmon, added this feature.
 For others, modified the usage message and verbose message.

 Revision 1.37  1999/08/17 18:56:27  soliday
 Added -inhibitPV option

 Revision 1.36  1998/12/16 21:54:31  borland
 Initialize generationsRowLimit and generationsTimeLimit to zero.

 Revision 1.35  1998/10/28 18:31:11  borland
 Ordinary generation filenames are now made in such a way that names are
 never reused during a single run.
 Also, commented out usleep code as Solaris 5.6 has usleep.

 Revision 1.34  1998/09/09 15:37:22  borland
 Added -dailyFiles option that permits internal generation of OAG-style
 data logging filenames.  A new file is started whenever a new day
 starts.  The filenames are of the form
 <outputName>-<YYYY>-<JJJ>-<MMDD>.<NNNN>
 where <outputName> is given on the commandline (as output file),
 YYYY is the year, JJJ is julian day, MMDD is month/day, and NNNN is
 the generation number.

 Revision 1.33  1998/06/15 15:02:50  borland
 Fixed bug in last version that resulted in the first generation file having
 only one row for time limited generations.

 Revision 1.32  1998/06/15 14:54:35  borland
 Fixed bug in last version.  When no rowlimit or time limit was given for
 generations, was behaving as if rowlimit=1 was given.

 Revision 1.31  1998/06/15 14:36:44  borland
 Change the usage message to reflect new capabilities.

 Revision 1.30  1998/06/15 14:32:11  borland
 Dummy change made so I can amplify the last change message:
 Added new capabilities to -generation mode.  Can now create a new
 generation when the current file reaches a specified number of rows
 or after a specified amount of time has elapsed.  Not implemented
 for glitch or trigger modes.

 Revision 1.29  1998/06/15 14:25:04  borland
 Added dead-band feature to sddsmonitor.  Required changing getScalarMonitorData,
 which is used to read data from SDDS file specifying how to scalar logging.

 Revision 1.28  1997/09/26 21:50:14  borland
 Added alarm-based trigger of circular buffer dumps.

 Revision 1.27  1997/07/23 14:28:53  borland
 Added PostTrigger column to glitch mode output to allow determining
 which samples are before/after the trigger.

 Revision 1.26  1997/05/30 14:53:39  borland
 Added -offsetTimeOfDay option.  When given, causes TimeOfDay data to be
 offset by 24 hours if more than half of the time of the run will fall
 in the following day.  This permits starting a 24+ hour job before midnight
 with the TimeOfDay values starting at negative numbers, then running 0
 to 24 in the following day.  An identical job started just after midnight
 will have the same TimeOfDay values as the first.

 Revision 1.25  1997/05/02 15:39:18  borland
 Added a flag variable to prevent repeated zeroing of buffer list when
 conditions repeatedly fail in glitch mode.  Added extra code to check
 for need to flush file (previously wouldn't do it until the next trigger).

 Revision 1.24  1997/04/22 16:02:54  borland
 Fixed problems with trigger arming and after buffer pending condition.

 Revision 1.23  1997/04/21 17:09:17  borland
 Added conditions feature to glitch/trigger mode.

 Revision 1.22  1997/04/02 23:30:13  borland
 Changed the way update time is kept track of to solve bug when there
 are conditions on the data acquisition.

 Revision 1.21  1997/02/28 20:27:44  borland
 Updated usage message.

 Revision 1.20  1997/02/28 20:24:55  borland
 Improved glitch/trigger modes.  Holdoff now works better, has auto-holdoff
 mode that guarantees filled after-buffer.  Output of after-buffer is done
 all at once, not one row at a time.  Added option for disabling resetting
 of baseline after a glitch.

 Revision 1.19  1997/02/27 22:15:26  borland
 Added call to turn off checks inside SDDS routines for better performance.

 Revision 1.18  1997/02/04 22:43:19  borland
 Rewrote glitch/trigger code to use data structures instead of many separate
 arrays.

 Revision 1.17  1997/01/31 16:09:57  borland
 Rewrote glitch/trigger code to allow use of glitches and triggers together.
 Also added sign and filter-fraction qualifiers to glitch.

 Revision 1.16  1997/01/25 00:18:03  borland
 Added multiple channel glitch or trigger capability.

 Revision 1.15  1996/12/13 20:10:33  borland
 Removed recalculation of total time that I put in as part of enforcing
 the time limit.

 Revision 1.14  1996/12/13 19:51:49  borland
 Added -enforceTimeLimit option and code to implement enforced limit on
 running time, equal to the value given with the -time option.

 Revision 1.13  1996/11/22 21:53:03  borland
 PageTimeStamp is now updated for trigger/glitch mode.

 Revision 1.12  1996/08/21 17:09:48  borland
 Fixed bug in

 Revision 1.11  1996/06/13 19:46:32  borland
 Improved function of -singleShot option by providing control of whether
 prompts go to stderr or stdout.  Also added "Done." message before program
 exits.

 * Revision 1.10  1996/02/28  21:14:31  borland
 * Added mode argument to getScalarMonitorData() for control of units control.
 * sddsmonitor and sddsstatmon now have -getUnits option. sddswmonitor and
 * sddsvmonitor now get units from EPICS for any scalar that has a blank
 * units string.
 *
 * Revision 1.9  1996/02/14  05:07:55  borland
 * Switched from obsolete scan_item_list() routine to scanItemList(), which
 * offers better response to invalid/ambiguous qualifiers.
 *
 * Revision 1.8  1996/02/10  06:30:32  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.7  1996/02/07  18:31:55  borland
 * Brought up-to-date with new time routines; now uses common setup routines
 * for time data in SDDS output.
 *
 * Revision 1.6  1996/01/02  18:38:02  borland
 * Added -comment and -generations options to monitor and logging programs;
 * added support routines in SDDSepics.c .
 *
 * Revision 1.5  1995/12/03  01:30:11  borland
 * Removed SDDS_CheckFile() routine and put it in SDDSepics.c .
 *
 * Revision 1.4  1995/11/13  16:22:52  borland
 * Cleaned up some parsing statements to check for presence of data before
 * scanning.  Using new form of SDDS_UpdatePage, based on -updateInterval
 * option given by user.
 *
 * Revision 1.3  1995/11/09  03:22:37  borland
 * Added copyright message to source files.
 *
 * Revision 1.2  1995/11/04  04:45:37  borland
 * Added new program sddsalarmlog to Makefile.  Changed routine
 * getTimeBreakdown in SDDSepics, and modified programs to use the new
 * version; it computes the Julian day and the year now.
 *
 * Revision 1.1  1995/09/25  20:15:40  saunders
 * First release of SDDSepics applications.
 *
 */

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <alarm.h>
/* If you get an error compiling this next few lines it
   means that the number or elements in alarmSeverityString
   and/or alarmStatusString have to change to match those
   in EPICS/Base */
#define COMPILE_TIME_ASSERT(expr) char constraint[expr]
COMPILE_TIME_ASSERT(ALARM_NSEV == 4);
COMPILE_TIME_ASSERT(ALARM_NSTATUS == 22);
const char *alarmSeverityString[] = {
  "NO_ALARM",
  "MINOR",
  "MAJOR",
  "INVALID"};
const char *alarmStatusString[] = {
  "NO_ALARM",
  "READ",
  "WRITE",
  "HIHI",
  "HIGH",
  "LOLO",
  "LOW",
  "STATE",
  "COS",
  "COMM",
  "TIMEOUT",
  "HWLIMIT",
  "CALC",
  "SCAN",
  "LINK",
  "SOFT",
  "BAD_SUB",
  "UDF",
  "DISABLE",
  "SIMM",
  "READ_ACCESS",
  "WRITE_ACCESS"};

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

#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#define CLO_INTERVAL 0
#define CLO_STEPS 1
#define CLO_VERBOSE 2
#define CLO_UPDATEINTERVAL 3
#define CLO_SINGLE_SHOT 4
#define CLO_TIME 5
#define CLO_APPEND 6
#define CLO_PRECISION 7
#define CLO_PROMPT 8
#define CLO_ONCAERROR 9
#define CLO_ERASE 10
#define CLO_GLITCH 11
#define CLO_TRIGGER 12
#define CLO_PENDIOTIME 13
#define CLO_EZCATIMING 14
#define CLO_NOEZCA 15
#define CLO_CONDITIONS 16
#define CLO_GENERATIONS 17
#define CLO_COMMENT 18
#define CLO_GETUNITS 19
#define CLO_ENFORCETIMELIMIT 20
#define CLO_OFFSETTIMEOFDAY 21
#define CLO_ALARMTRIGGER 22
#define CLO_CIRCULARBUFFER 23
#define CLO_HOLDOFFTIME 24
#define CLO_AUTOHOLDOFF 25
#define CLO_DAILY_FILES 26
#define CLO_INHIBIT 27
#define CLO_WATCHINPUT 28
#define CLO_RUNCONTROLPV 29
#define CLO_RUNCONTROLDESC 30
#define CLO_STOPPV 31
#define COMMANDLINE_OPTIONS 32
static char *commandline_option[COMMANDLINE_OPTIONS] = {
  "interval", "steps", "verbose", "updateinterval", "singleshot", "time",
  "append", "precision", "prompt", "oncaerror", "erase", "glitch", "trigger",
  "pendiotime", "ezcatiming", "noezca", "conditions", "generations",
  "comment", "getunits", "enforcetimelimit", "offsettimeofday",
  "alarmtrigger", "circularbuffer", "holdofftime", "autoholdoff",
  "dailyfiles", "inhibitpv", "watchInput", "runControlPV",
  "runControlDescription", "stopPV"};

#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2
static char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double"};

#define GLITCH_HOLDOFF 0x0001U
#define GLITCH_AUTOHOLD 0x0002U
#define GLITCH_BEFORE 0x0004U
#define GLITCH_AFTER 0x0008U
#define GLITCH_FRACTION 0x00010000U
#define GLITCH_DELTA 0x00020000U
#define GLITCH_BASELINE 0x00040000U
#define GLITCH_MESSAGE 0x00080000U
#define GLITCH_FILTERFRAC 0x00100000U
#define GLITCH_NORESET 0x00200000U
char *glitchUsage = "-glitch=<controlname>[,message=<string>]{,delta=<value>|,fraction=<value>}[,before=<number>][,after=<number>][,{baseline=<number> | filterFraction=<value>}][,sign={+ | -}][,noReset][,{autoHoldoff | holdoff=<seconds>}]";

#define GLITCH_BEFORE_DEFAULT 100
#define GLITCH_AFTER_DEFAULT 1
#define GLITCH_BASELINE_DEFAULT 1

#define TRIGGER_HOLDOFF GLITCH_HOLDOFF
#define TRIGGER_AUTOHOLD GLITCH_AUTOHOLD
#define TRIGGER_BEFORE GLITCH_BEFORE
#define TRIGGER_AFTER GLITCH_AFTER
#define TRIGGER_LEVEL 0x00010000U
#define TRIGGER_SLOPE 0x00020000U
#define TRIGGER_MESSAGE 0x00040000U
#define TRIGGER_AUTOARM 0x00080000U
static char *triggerUsage = "-trigger=<controlName>,level=<value>[,message=<string>][,slope={+ | -}][,before=<number>][,after=<number>][,autoArm][,{autoHoldoff | holdoff=<seconds>}]";

typedef struct
{
  short triggerMode, triggerArmed;
  unsigned long flags;
  char *controlName, *message, glitchSign, triggerSlope;
  double baselineValue, holdoff;
  double triggerLevel, delta, fraction, filterFrac;
  long parameterIndex;
} GLITCH_REQUEST;

#define TRIGGER_BEFORE_DEFAULT 100
#define TRIGGER_AFTER_DEFAULT 1

#define ALARMTRIG_HOLDOFF 0x0001U
#define ALARMTRIG_AUTOHOLD 0x0002U
#define ALARMTRIG_CONTROLNAME 0x0004U
#define ALARMTRIG_STATUSLIST 0x0008U
#define ALARMTRIG_NOTSTATUSLIST 0x0010U
#define ALARMTRIG_SEVERITYLIST 0x0020U
#define ALARMTRIG_NOTSEVERITYLIST 0x0040U

static char *alarmTriggerUsage0 = "-alarmTrigger=<controlName>[,status=<statusNames>][,notStatus=<statusNames>][,severity=<sevNames>][,notSeverity=<sevNames>][,{autoHoldoff | holdoff=<seconds>}]\n";
static char alarmTriggerUsage[1024];

typedef struct
{
  unsigned long flags;
  char *controlName;
  /* user's comma-separated list of severities/status for alarm conditions: */
  char *severityNameList, *notSeverityNameList;
  char *statusNameList, *notStatusNameList;
  /* array of indices corresponding to severityNameList and notSeverityNameList */
  long *severityIndex, severityIndices;
  /* array of indices corresponding to statusNameList and notStatusNameList */
  long *statusIndex, statusIndices;
  double holdoff;
  short triggered, severity, status;        /* set by callback routine */
  long alarmParamIndex, severityParamIndex; /* parameter indices for SDDS output */
  chid channelID;
  /* used with channel access routines to give index via callback: */
  long usrValue;
} ALARMTRIG_REQUEST;

typedef struct datanode {
  unsigned short hasdata;
  long step; /* fixed columns*/
  double ElapsedTime, EpochTime, TimeOfDay, DayOfMonth;
  double *values;
  struct datanode *next;
} DATANODE, *PDATANODE;

DATANODE *firstNode, *Node, *glitchNode;

#define ONCAERROR_USEZERO 0
#define ONCAERROR_SKIPROW 1
#define ONCAERROR_EXIT 2
#define ONCAERROR_OPTIONS 3
static char *oncaerror_options[ONCAERROR_OPTIONS] = {
  "usezero",
  "skiprow",
  "exit"};

#define NAuxiliaryColumnNames 5
static char *AuxiliaryColumnNames[NAuxiliaryColumnNames] = {
  "Step",
  "Time",
  "TimeOfDay",
  "DayOfMonth",
  "CAerrors",
};

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

static char *USAGE1 = "sddsmonitor <SDDSinputfile> <SDDSoutputfile>\n\
 [-erase | -append[=recover][,toPage]] | \n\
  -generations[=digits=<integer>][,delimiter=<string>][,rowlimit=<number>][,timelimit=<secs>] | \n\
  -dailyFiles]\n\
 [-steps=<integer-value> | -time=<real-value>[,<time-units>]] [-interval=<real-value>[,<time-units>]] | \n\
 [-enforceTimeLimit] [-offsetTimeOfDay] [-watchInput]\n\
 -updateinterval=<integer-value> [-verbose] [-singleshot{=noprompt|stdout}] [-precision={single|double}]\n\
 -oncaerror={usezero|skiprow|exit} [-pendIOtime=<value>]\n\
 [-glitch=<controlname>[,message=<string>]{,delta=<value>|,fraction=<value>}[,before=<number>][,after=<number>][,{baseline=<number>|filterFraction=<value>}][,sign={+|-}][,noReset][,{autoHoldoff|holdoff=<seconds>}]]\n\
 [-trigger=<controlName>,level=<value>[,message=<string>][,slope={+|-}][,before=<number>][,after=<number>][,autoArm][,{autoHoldoff|holdoff=<seconds>}]]\n\
 [-alarmTrigger=<controlName>[,status=<statusNames>][,notStatus=<statusNames>][,severity=<sevNames>][,notSeverity=<sevNames>][,{autoHoldoff|holdoff=<seconds>}]]\n\
 [-circularBuffer=[before=<number>,][after=<number>] [-holdoffTime=<seconds>] [-autoHoldoff]\n\
 [-conditions=<filename>,{allMustPass|oneMustPass}[,touchOutput][,retakeStep]]\n\
 [-comment=<parameterName>,<text>]\n\
 [-getUnits={force|ifBlank|ifNoneGiven}]\n\
 [-inhibitPV=name=<name>[,pendIOTime=<seconds>][,waitTime=<seconds>]]\n\
 [-stopPV=<controlName>[,pendIOTime=<seconds>]]\n\n\
 [-runControlPV=string=<string>,pingTimeout=<value>\n\
 [-runControlDescription=string=<string>]\n\n\
Writes values of process variables or devices to a binary SDDS file.\n";
static char *USAGE2 = "<SDDSinputfile>    defines the process variables or device names to be read;\n\
                   One column must be \"Device\" or \"ControlName\" (they have the same effect),\n\
                   columns \"ReadbackName\", \"Message\", \"ScaleFactor\", \"ReadbackUnits\",\n\
                   and \"DeadBand\" are optional.\n\
<SDDSoutputfile>   Contains the data read from the process variables.\n\
erase              The output file is erased before execution.\n\
append             Appends new values in a new SDDS page in the output file.\n\
                   Optionally recovers garbled SDDS data.  The 'toPage' qualifier\n\
                   results in appending to the last data page, rather than creation\n\
                   of a new page.\n\
generations        The output is sent to the file <SDDSoutputfile>-<N>, where <N> is\n\
                   the smallest positive integer such that the file does not already \n\
                   exist.   By default, four digits are used for formatting <N>, so that\n\
                   the first generation number is 0001.  If a row limit or time limit\n\
                   is given, a new file is started when the given limit is reached.\n\
dailyFiles         The output is sent to the file <SDDSoutputfile>-YYYY-JJJ-MMDD.<N>,\n\
                   where YYYY=year, JJJ=Julian day, and MMDD=month+day.  A new file is\n\
                   started after midnight.\n\
interval           desired interval in seconds for each reading step;\n\
                   valid time units are seconds, minutes, hours, or days.\n\
steps              number of reads for each process variable or device.\n\
time               total time (in seconds) for monitoring;\n\
                   valid time units are seconds, minutes, hours, or days.\n\
enforceTimeLimit   Enforces the time limit given with the -time option, even if\n\
                   the expected number of samples has not been taken.\n";
static char *USAGE3 = "offsetTimeOfDay    Adjusts the starting TimeOfDay value so that it corresponds\n\
                   to the day for which the bulk of the data is taken.  Hence, a\n\
                   26 hour job started at 11pm would have initial time of day of\n\
                   -1 hour and final time of day of 25 hours.\n\
updateinterval     number of read steps before each output file update.\n\
                   The default is 1.\n\
verbose            prints out a message when data is taken.\n\
singleshot         single shot read initiated by a <cr> key press; time_interval is disabled.\n\
                   By default, a prompt is issued to stderr.  The qualifiers may be used to\n\
                   remove the prompt or have it go to stdout.\n\
precision          specify single (default) or double precision for PV data.\n\
oncaerror          action taken when a channel access error occurs. Default is\n\
                   to use zeroes for values.\n\
pendIOtime         sets the maximum time to wait for return of each value.\n";
static char *USAGE4 = "glitch             records buffer of PV readback values whenever the glitch PV or \n\
                   device changes by some value.  If control name is a device, then \"message\"\n\
                   should be specified. The glitch is triggered if the control \n\
                   variable changes by \"delta\" or a \"fraction\" with respect to an exponential\n\
                   average from \"baseline\" number of readings or using \"filterFraction\" as the\n\
                   fraction of the new reading to add accumulate. \n\
                   \"before\" and \"after\" give the number of readings recorded in a page\n\
                   before and after the glitch is triggered.  Normally, the baseline computation\n\
                   is reset after each glitch; giving the \"noReset\" qualifier prevents this.\n\
                   The autoHoldOff qualifier can be used to guarantee that the after-trigger samples\n\
                   are all acquired before a new trigger is recognized.\n\
trigger            similar to glitch, except buffered data is recorded when the named PV exceeds\n\
                   the given \"level\" with the given \"slope\".  This is analogous to an oscilloscope\n\
                   trigger.\n\
alarmTrigger       Similar to -glitch and -trigger, but alarm callbacks are used to trigger saving of\n\
                   data.  Data is saved when the alarm severity of the given channel becomes equal to one of\n\
                   the values given with the severity qualifier (defaults to all values) with the status\n\
                   equal to one of values given with the status qualifier (defaults to all values).\n\
                   Alternatively, you may specify the trigger condition by exclusion using the notSeverity\n\
                   and/or notStatus qualifiers.  E.g., severity=(MAJOR,MINOR,INVALID) is equivalent to \n\
                   notSeverity=NO_ALARM. Most uses employ only the severity or notSeverity qualifier.\n";
static char *USAGE5 = "circularBuffer     Set how many samples to keep before and after the triggering event.\n\
                   Replaces the before and after qualifiers on -trigger and -glitch. This option must be used\n\
                   after the -trigger and -glitch options.\n\
holdoffTime        Set the number of seconds to wait after a trigger or alarm before accepting new triggers\n\
                   or alarms. \n\
autoHoldoff        Sets holdoff time so that the after-trigger samples are guaranteed to be collected before\n\
                   a new trigger or alarm is accepted.\n\
conditions         Names an SDDS file containing PVs to read and limits on each PV that must\n\
                   be satisfied for data to be taken and logged.  The file is like the main\n\
                   input file, but has numerical columns LowerLimit and UpperLimit.\n\
comment            Gives the parameter name for a comment to be placed in the SDDS output file,\n\
                   along with the text to be placed in the file.  \n\
getUnits           Gets the units of quantities from EPICS.  'force' means ignore the ReadbackUnits\n\
                   data in the input, if any.  'ifBlank' means attempt to get units for any quantity\n\
                   that has a blank string given for units.  'ifNoneGiven' (default) means get units\n\
                   for all quantities, but only if no ReadbackUnits column is given in the file.\n\
inhibitPV          Checks this PV prior to each sample.  If the value is nonzero, then data\n\
                   collection is inhibited.  None of the conditions-related or other PVs are\n\
                   polled.\n\
stopPV             Specifies a stop PV name. Program will exit when PV value is true (non-zero).\n";
static char *USAGE6 = "runControlPV       specifies a runControl PV name.\n\
runControlDescription\n\
                   specifies a runControl PV description record.\n\n\
Program by Louis Emery, ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

#ifndef USE_RUNCONTROL
static char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#else
static char *USAGE_WARNING = "";
#endif

#ifdef USE_RUNCONTROL
static char *runControlPVUsage = "-runControlPV=string=<string>\n";
static char *runControlDescUsage = "-runControlDescription=string=<string>\n";
#endif

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status;
  RUNCONTROL_INFO rcInfo;
} RUNCONTROL_PARAM;
RUNCONTROL_PARAM rcParam;
#endif

#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 100
#define DEFAULT_UPDATEINTERVAL 1

#define SS_PROMPT 1
#define SS_NOPROMPT 2
#define SS_STDOUTPROMPT 4
#define ANSWER_LENGTH 256

long IsGlitch(double value, double baseline, unsigned long flags,
              double fraction, double delta, char sign);
long DefineGlitchParameters(SDDS_DATASET *SDDSout, GLITCH_REQUEST *glitchRequest,
                            long glitchRequests, ALARMTRIG_REQUEST *alarmTrigRequest,
                            long alarmTrigRequests);
long *CSStringListsToIndexArray(long *arraySize, char *includeList,
                                char *excludeList, char **validItem, long validItems);
void makeAlarmTriggerUsageMessage(char *alarmTriggerUsage0, char *buffer);

ALARMTRIG_REQUEST *alarmTrigRequest;
void alarmTriggerEventHandler(struct event_handler_args event);
long setUpAlarmTriggerCallbacks(ALARMTRIG_REQUEST *alarmTrigRequest,
                                long alarmTrigChannels, double pendIOtime);
void startMonitorFile(SDDS_DATASET *output_page, char *outputfile, GLITCH_REQUEST *glitchRequest,
                      long glitchChannels, ALARMTRIG_REQUEST *alarmTrigRequest,
                      long alarmTrigChannels, long Precision, long n_variables,
                      char **ReadbackNames, char **DeviceNames, char **ReadbackUnits,
                      double *StartTime, double *StartDay, double *StartHour, double *StartJulianDay,
                      double *StartMonth, double *StartYear, char **TimeStamp,
                      char **commentParameter, char **commentText, long comments,
                      long updateInterval);

int runControlPingWhileSleep(double sleepTime);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

volatile int sigint = 0;

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_DATASET output_page;

  char *inputfile, *outputfile, answer[ANSWER_LENGTH], *origOutputFile = NULL, *inputlink;
  long n_variables, enforceTimeLimit;
  char **DeviceNames = NULL, **ReadMessages = NULL, **ReadbackNames = NULL, **ReadbackUnits = NULL;
  chid *DeviceCHID = NULL;
  long *ReadbackIndex = NULL;
  char **commentParameter = NULL, **commentText = NULL;
  long comments, offsetTimeOfDay;
  double *values, *baselineValues = NULL, pendIOtime, *DeadBands;
  long i, j, i_arg;
  char *TimeStamp, *PageTimeStamp;
  double TimeInterval, ElapsedTime, EpochTime, StartHour, RunTime;
  double TimeOfDay, DayOfMonth, HourNow, LastHour;
  long Step, outputRow, NStep, updateInterval, updateCountdown, newFileCountdown = 0;
  int64_t initialOutputRow;
  long verbose, singleShot, generations;
  int32_t generationsDigits, generationsRowLimit;
  long dailyFiles;
  unsigned long dummyFlags, appendFlags;
  char *generationsDelimiter;
  double *ScaleFactors;
  long steps_set, totalTimeSet;
  double TotalTime, generationsTimeLimit, generationsTimeStop;
  long append, appendToPage = 0, recover = 0, erase;
  SDDS_DATASET originalfile_page;
  double StartTime, StartJulianDay, StartDay, StartYear, StartMonth, RunStartTime;
  char **ColumnNames;
  int32_t NColumnNames;
  long Precision, CAerrorsColumnPresent = 0;
  char *precision_string = NULL, *oncaerror;
  long TimeUnits, oncaerrorindex, CAerrors, readCode, optionCode;
  double holdoffTime, defaultHoldoffTime;
  long holdoffCount, triggerAllowed, autoHoldoff;

  unsigned long getUnits;

  GLITCH_REQUEST *glitchRequest;
  char **glitchControlName, **glitchControlMessage = NULL;
  double *glitchValue = NULL;
  chid *glitchCHID = NULL;
  long glitchChannels, iGlitch, iAlarm, baselineStarted;
  int32_t glitchBefore, glitchAfter, glitchBaseline;
  long row = 0, triggerCount, glitchBeforeCounter, glitchAfterCounter;
  long writeBeforeBuffer, afterBufferPending, bufferlength;

  long alarmTrigChannels;

  short *triggered = NULL; /* array of trigger states for glitch/trigger channels */
  short *alarmed = NULL;   /* array of alarm states (0 or 1) for alarm trigger channels */

  char **CondDeviceName = NULL, **CondReadMessage = NULL, *CondFile;
  double *CondScaleFactor = NULL, *CondLowerLimit = NULL, *CondUpperLimit = NULL, *CondHoldoff = NULL;
  chid *CondCHID = NULL;
  long conditions, SDDSFlushNeeded = 0, lastConditionsFailed;
  unsigned long CondMode = 0;
  double *CondDataBuffer = NULL;

  long inhibit = 0;
  int32_t InhibitWaitTime = 5;
  double InhibitPendIOTime = 10;
  chid inhibitID;
  PV_VALUE InhibitPV;

  long useStop = 0;
  double stopPendIOTime = 10;
  chid stopID;
  PV_VALUE stopPV;

  struct stat input_filestat;
  int watchInput = 0;
  double timeout;
  long retries;

  double SampleTimeToWait, SampleTimeToExecute;
  double PingTime;

  pendIOtime = 10.0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5, USAGE6);
    fprintf(stderr, "%s", USAGE_WARNING);
    return (1);
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

  makeAlarmTriggerUsageMessage(alarmTriggerUsage0, alarmTriggerUsage);
  alarmTrigRequest = NULL;
  alarmTrigChannels = 0;

  verbose = offsetTimeOfDay = 0;
  inputfile = outputfile = inputlink = NULL;
  TimeInterval = DEFAULT_TIME_INTERVAL;
  NStep = DEFAULT_STEPS;
  totalTimeSet = steps_set = 0;
  append = appendToPage = erase = generations = dailyFiles = 0;
  singleShot = 0;
  oncaerrorindex = ONCAERROR_USEZERO;
  Precision = PRECISION_SINGLE;
  updateInterval = DEFAULT_UPDATEINTERVAL;
  CondFile = NULL;
  commentParameter = commentText = NULL;
  comments = getUnits = 0;
  enforceTimeLimit = 0;
  autoHoldoff = 0;
  defaultHoldoffTime = 0;

  glitchBefore = GLITCH_BEFORE_DEFAULT;
  glitchAfter = GLITCH_AFTER_DEFAULT;
  glitchRequest = NULL;
  glitchChannels = baselineStarted = 0;

  generationsRowLimit = 0;
  generationsTimeLimit = 0;
  initialOutputRow = 0;

#ifdef USE_RUNCONTROL
  rcParam.PV = rcParam.Desc = NULL;
  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  rcParam.pingInterval = 2;
  rcParam.pingTimeout = 10;
  rcParam.alarmSeverity = NO_ALARM;
#endif

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (optionCode = match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
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
      case CLO_UPDATEINTERVAL:
        if (s_arg[i_arg].n_items != 2 ||
            !(get_long(&updateInterval, s_arg[i_arg].list[1])))
          bomb("no value given for option -updateinterval", NULL);
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
      case CLO_ERASE:
        erase = 1;
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
          return (1);
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
          return (1);
        }
        switch (oncaerrorindex = match_string(oncaerror, oncaerror_options, ONCAERROR_OPTIONS, 0)) {
        case ONCAERROR_USEZERO:
        case ONCAERROR_SKIPROW:
        case ONCAERROR_EXIT:
          break;
        default:
          printf("unrecognized oncaerror option given: %s\n", oncaerror);
          return (1);
        }
        break;
      case CLO_GLITCH:
      case CLO_TRIGGER:
        glitchRequest = SDDS_Realloc(glitchRequest, sizeof(*glitchRequest) * (glitchChannels + 1));

        glitchRequest[glitchChannels].message = (char *)malloc(sizeof(char) * 64);
        glitchRequest[glitchChannels].message[0] = 0;
        glitchRequest[glitchChannels].glitchSign = 0;
        glitchRequest[glitchChannels].triggerSlope = '+';
        glitchRequest[glitchChannels].delta = glitchRequest[glitchChannels].fraction =
          glitchRequest[glitchChannels].holdoff = 0;
        glitchRequest[glitchChannels].filterFrac = 1;

        glitchBaseline = GLITCH_BASELINE_DEFAULT;
        switch (optionCode) {
        case CLO_GLITCH:
          glitchRequest[glitchChannels].triggerMode = 0;
          if ((s_arg[i_arg].n_items -= 2) < 0 ||
              !scanItemList(&glitchRequest[glitchChannels].flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                            "fraction", SDDS_DOUBLE, &glitchRequest[glitchChannels].fraction, 1, GLITCH_FRACTION,
                            "delta", SDDS_DOUBLE, &glitchRequest[glitchChannels].delta, 1, GLITCH_DELTA,
                            "before", SDDS_LONG, &glitchBefore, 1, GLITCH_BEFORE,
                            "after", SDDS_LONG, &glitchAfter, 1, GLITCH_AFTER,
                            "baseline", SDDS_LONG, &glitchBaseline, 1, GLITCH_BASELINE,
                            "message", SDDS_STRING, &glitchRequest[glitchChannels].message, 1, GLITCH_MESSAGE,
                            "holdoff", SDDS_DOUBLE, &glitchRequest[glitchChannels].holdoff, 1, GLITCH_HOLDOFF,
                            "sign", SDDS_CHARACTER, &glitchRequest[glitchChannels].glitchSign, 1, 0,
                            "filterfraction", SDDS_DOUBLE, &glitchRequest[glitchChannels].filterFrac, 1, GLITCH_FILTERFRAC,
                            "noreset", -1, NULL, 0, GLITCH_NORESET,
                            "autoholdoff", -1, NULL, 0, GLITCH_AUTOHOLD,
                            NULL) ||
              ((glitchRequest[glitchChannels].flags & GLITCH_FRACTION) && (glitchRequest[glitchChannels].flags & GLITCH_DELTA)) ||
              (!(glitchRequest[glitchChannels].flags & GLITCH_FRACTION) && !(glitchRequest[glitchChannels].flags & GLITCH_DELTA)) ||
              (glitchRequest[glitchChannels].flags & GLITCH_FILTERFRAC && glitchRequest[glitchChannels].flags & GLITCH_BASELINE) ||
              glitchBaseline <= 0 || glitchRequest[glitchChannels].filterFrac > 1 || glitchRequest[glitchChannels].filterFrac <= 0 ||
              !strlen(glitchRequest[glitchChannels].controlName = s_arg[i_arg].list[1]) ||
              glitchBefore <= 0 || glitchAfter <= 0 ||
              (glitchRequest[glitchChannels].glitchSign &&
               !(glitchRequest[glitchChannels].glitchSign == '+' || glitchRequest[glitchChannels].glitchSign == '-')))
            bomb("bad -glitch syntax", glitchUsage);
          if (glitchRequest[glitchChannels].flags & GLITCH_BASELINE)
            glitchRequest[glitchChannels].filterFrac = 1. / glitchBaseline;
          break;
        default:
          glitchRequest[glitchChannels].triggerMode = 1;
          if ((s_arg[i_arg].n_items -= 2) < 0 ||
              !scanItemList(&glitchRequest[glitchChannels].flags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                            "level", SDDS_DOUBLE, &glitchRequest[glitchChannels].triggerLevel, 1, TRIGGER_LEVEL,
                            "slope", SDDS_CHARACTER, &glitchRequest[glitchChannels].triggerSlope, 1, TRIGGER_SLOPE,
                            "before", SDDS_LONG, &glitchBefore, 1, TRIGGER_BEFORE,
                            "after", SDDS_LONG, &glitchAfter, 1, TRIGGER_AFTER,
                            "message", SDDS_STRING, &glitchRequest[glitchChannels].message, 1, TRIGGER_MESSAGE,
                            "holdoff", SDDS_DOUBLE, &glitchRequest[glitchChannels].holdoff, 1, TRIGGER_HOLDOFF,
                            "autoarm", -1, NULL, 0, TRIGGER_AUTOARM,
                            "autoholdoff", -1, NULL, 0, TRIGGER_AUTOHOLD,
                            NULL) ||
              !(glitchRequest[glitchChannels].flags & TRIGGER_LEVEL) || (glitchRequest[glitchChannels].triggerSlope != '+' && glitchRequest[glitchChannels].triggerSlope != '-'))
            bomb("bad -trigger syntax", triggerUsage);
          if (!strlen(glitchRequest[glitchChannels].controlName = s_arg[i_arg].list[1]))
            bomb("bad -trigger syntax", triggerUsage);
          /* the variables glitchBefore and glitchAfter are used for -trigger too */
          if (!(glitchRequest[glitchChannels].flags & TRIGGER_BEFORE))
            glitchBefore = TRIGGER_BEFORE_DEFAULT;
          if (!(glitchRequest[glitchChannels].flags & TRIGGER_AFTER))
            glitchAfter = TRIGGER_AFTER_DEFAULT;
        }
        glitchChannels++;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax", NULL);
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
        generationsRowLimit = 0;
        generationsTimeLimit = 0;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &generationsDigits, 1, 0,
                          "delimiter", SDDS_STRING, &generationsDelimiter, 1, 0,
                          "rowlimit", SDDS_LONG, &generationsRowLimit, 1, 0,
                          "timelimit", SDDS_DOUBLE, &generationsTimeLimit, 1, 0,
                          NULL) ||
            generationsDigits < 1)
          SDDS_Bomb("invalid -generations syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DAILY_FILES:
        generationsDigits = 4;
        generationsDelimiter = ".";
        dailyFiles = 1;
        if (s_arg[i_arg].n_items != 1)
          SDDS_Bomb("invalid -dailyFiles syntax");
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
      case CLO_ENFORCETIMELIMIT:
        enforceTimeLimit = 1;
        break;
      case CLO_OFFSETTIMEOFDAY:
        offsetTimeOfDay = 1;
        break;
      case CLO_ALARMTRIGGER:
        alarmTrigRequest = SDDS_Realloc(alarmTrigRequest,
                                        sizeof(*alarmTrigRequest) * (alarmTrigChannels + 1));
        alarmTrigRequest[alarmTrigChannels].controlName =
          alarmTrigRequest[alarmTrigChannels].statusNameList =
            alarmTrigRequest[alarmTrigChannels].notStatusNameList =
              alarmTrigRequest[alarmTrigChannels].severityNameList =
                alarmTrigRequest[alarmTrigChannels].notSeverityNameList = NULL;
        alarmTrigRequest[alarmTrigChannels].flags = 0;
        alarmTrigRequest[alarmTrigChannels].holdoff = 0;

        if ((s_arg[i_arg].n_items -= 2) < 0 ||
            !scanItemList(&alarmTrigRequest[alarmTrigChannels].flags,
                          s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "status", SDDS_STRING, &alarmTrigRequest[alarmTrigChannels].statusNameList, 1, 0,
                          "notstatus", SDDS_STRING, &alarmTrigRequest[alarmTrigChannels].notStatusNameList, 1, 0,
                          "severity", SDDS_STRING, &alarmTrigRequest[alarmTrigChannels].severityNameList, 1, 0,
                          "notseverity", SDDS_STRING, &alarmTrigRequest[alarmTrigChannels].notSeverityNameList, 1, 0,
                          "holdoff", SDDS_DOUBLE, &alarmTrigRequest[alarmTrigChannels].holdoff, 1,
                          ALARMTRIG_HOLDOFF,
                          "autoholdoff", -1, NULL, 1, ALARMTRIG_AUTOHOLD,
                          NULL) ||
            (alarmTrigRequest[alarmTrigChannels].statusNameList &&
             alarmTrigRequest[alarmTrigChannels].notStatusNameList) ||
            (alarmTrigRequest[alarmTrigChannels].severityNameList &&
             alarmTrigRequest[alarmTrigChannels].notSeverityNameList) ||
            !strlen(alarmTrigRequest[alarmTrigChannels].controlName = s_arg[i_arg].list[1]) ||
            alarmTrigRequest[alarmTrigChannels].holdoff < 0 ||
            (alarmTrigRequest[alarmTrigChannels].flags & ALARMTRIG_HOLDOFF &&
             alarmTrigRequest[alarmTrigChannels].flags & ALARMTRIG_AUTOHOLD))
          bomb("invalid -alarmTrigger usage/values (sddsmonitor)", alarmTriggerUsage);
        if (!(alarmTrigRequest[alarmTrigChannels].statusIndex =
                CSStringListsToIndexArray(&alarmTrigRequest[alarmTrigChannels].statusIndices,
                                          alarmTrigRequest[alarmTrigChannels].statusNameList,
                                          alarmTrigRequest[alarmTrigChannels].notStatusNameList,
                                          (char **)alarmStatusString,
                                          ALARM_NSTATUS))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          bomb("invalid -alarmTrigger usage---problem with status lists (sddsmonitor)", alarmTriggerUsage);
        }
        if (!(alarmTrigRequest[alarmTrigChannels].severityIndex =
                CSStringListsToIndexArray(&alarmTrigRequest[alarmTrigChannels].severityIndices,
                                          alarmTrigRequest[alarmTrigChannels].severityNameList,
                                          alarmTrigRequest[alarmTrigChannels].notSeverityNameList,
                                          (char **)alarmSeverityString,
                                          ALARM_NSEV))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          bomb("invalid -alarmTrigger usage---problem with severity lists (sddsmonitor)", alarmTriggerUsage);
        }
        alarmTrigChannels += 1;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_CIRCULARBUFFER:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "before", SDDS_LONG, &glitchBefore, 1, 0,
                          "after", SDDS_LONG, &glitchAfter, 1, 0,
                          NULL) ||
            glitchBefore < 1 || glitchAfter < 1)
          SDDS_Bomb("invalid -circularBuffer syntax/values");
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_HOLDOFFTIME:
        if (s_arg[i_arg].n_items != 2 ||
            sscanf(s_arg[i_arg].list[1], "%lf", &defaultHoldoffTime) != 1 ||
            defaultHoldoffTime < 0)
          SDDS_Bomb("invalid -holdoffTime syntax/value");
        break;
      case CLO_AUTOHOLDOFF:
        autoHoldoff = 1;
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
      case CLO_STOPPV:
        stopPendIOTime = 10;
        s_arg[i_arg].n_items -= 2;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "pendiotime", SDDS_DOUBLE, &stopPendIOTime, 1, 0,
                          NULL)) {
          SDDS_Bomb("invalid -stopPV syntax/values");
        } else {
          stopPV.name = s_arg[i_arg].list[1];
          if (!stopPV.name || !strlen(stopPV.name) || stopPendIOTime <= 0) {
            SDDS_Bomb("invalid -stopPV syntax/values");
          }
        }
        useStop = 1;
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_WATCHINPUT:
        watchInput = 1;
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &rcParam.pingTimeout, 1, 0,
                          NULL) ||
            (!rcParam.PV))
          bomb("bad -runControlPV syntax", runControlPVUsage);
        rcParam.pingTimeout *= 1000;
        if (rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          return (1);
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "string", SDDS_STRING, &rcParam.Desc, 1, 0,
                          NULL) ||
            (!rcParam.Desc))
          bomb("bad -runControlDesc syntax", runControlDescUsage);
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      default:
        printf("unrecognized option given: %s.\n", s_arg[i_arg].list[0]);
        return (1);
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputfile)
        outputfile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }

#ifdef USE_RUNCONTROL
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
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      return (1);
    }
  }
#endif
  atexit(rc_interrupt_handler);

  if (inhibit) {
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 1)) {
      fprintf(stderr, "error: problem doing search for Inhibit PV %s\n", InhibitPV.name);
      return (1);
    }
    if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
      if (verbose)
        fprintf(stderr, "Data collection inhibited---not starting\n");
      return (0);
    }
  }

  if (useStop) {
    if (ConnectPV(stopPV, &stopID, stopPendIOTime)) {
      fprintf(stderr, "error: problem doing search for stop PV %s\n", stopPV.name);
      return (1);
    }
  }

  /* assert defaults for autoholdoff and holdoff time */
  for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
    if (autoHoldoff)
      glitchRequest[iGlitch].flags |= TRIGGER_AUTOHOLD;
    if (defaultHoldoffTime > 0 && !(glitchRequest[iGlitch].flags & TRIGGER_HOLDOFF)) {
      glitchRequest[iGlitch].holdoff = defaultHoldoffTime;
      glitchRequest[iGlitch].flags |= TRIGGER_HOLDOFF;
    }
  }
  for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
    if (autoHoldoff)
      alarmTrigRequest[iAlarm].flags |= ALARMTRIG_AUTOHOLD;
    if (defaultHoldoffTime > 0 && !(alarmTrigRequest[iAlarm].flags & ALARMTRIG_HOLDOFF)) {
      alarmTrigRequest[iAlarm].holdoff = defaultHoldoffTime;
      alarmTrigRequest[iAlarm].flags |= ALARMTRIG_HOLDOFF;
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
      return (1);
    }
  }

  if (totalTimeSet) {
    if (steps_set) {
      printf("Option totalTime supersedes option steps\n");
    }
    NStep = TotalTime / TimeInterval + 1;
  } else {
    enforceTimeLimit = 0;
  }
  if (append && erase)
    SDDS_Bomb("-append and -erase are incompatible options");
  if (append && (generations || dailyFiles))
    SDDS_Bomb("-append and -generations/dailyFiles are incompatible options");
  if (erase && (generations || dailyFiles))
    SDDS_Bomb("-erase and -generations/dailyFiles are incompatible options");
  if (generations && dailyFiles)
    SDDS_Bomb("-generations and -dailyFiles are incompatible");
  if (append && (glitchChannels || alarmTrigChannels))
    SDDS_Bomb("-append and -glitch/-trigger/-alarmTrigger are incompatible");

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    if (runControlPingWhileSleep(0)) {
      fprintf(stderr, "Problem pinging the run control record.\n");
      return (1);
    }
    strcpy(rcParam.message, "Running");
    rcParam.status = runControlLogMessage(rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
    if (rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      return (1);
    }
  }
#endif

  if (!getScalarMonitorData(&DeviceNames, &ReadMessages, &ReadbackNames,
                            &ReadbackUnits, &ScaleFactors, &DeadBands,
                            &n_variables, inputfile, getUnits, &DeviceCHID, pendIOtime))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (CondFile) {
    if (!getConditionsData(&CondDeviceName, &CondReadMessage, &CondScaleFactor,
                           &CondLowerLimit, &CondUpperLimit, &CondHoldoff, &conditions,
                           CondFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (!(CondDataBuffer = (double *)malloc(sizeof(*CondDataBuffer) * conditions)))
      SDDS_Bomb("allocation failure");
    if (!(CondCHID = (chid *)malloc(sizeof(*CondCHID) * conditions)))
      SDDS_Bomb("allocation failure");
    for (i = 0; i < conditions; i++)
      CondCHID[i] = NULL;
  }

  if (!n_variables)
    fprintf(stderr, "error: no data in input file");

  if (!(values = (double *)malloc(sizeof(double) * n_variables)) ||
      !(baselineValues = (double *)malloc(sizeof(double) * n_variables)))
    SDDS_Bomb("memory allocation failure");

  if (generations)
    outputfile =
      MakeGenerationFilename(origOutputFile = outputfile, generationsDigits, generationsDelimiter,
                             NULL);
  if (dailyFiles)
    outputfile =
      MakeDailyGenerationFilename(origOutputFile = outputfile, generationsDigits, generationsDelimiter, 0);
  HourNow = getHourOfDay();

#ifdef DEBUG
  fprintf(stderr, "Output filename: %s\n", outputfile);
#endif

  /* define output page */
  /* check if output file already exists */
  /* output_previously_exists=0;*/
  if (fexists(outputfile)) {
    if (append) {
      /* output_previously_exists=1;*/
    } else if (!erase) {
      fprintf(stderr, "Error. File %s already exists.\n", outputfile);
      return (1);
    }
  } else
    /* isn't an append (even if requested) since file doesn't exist */
    append = 0;

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
      return (1);
    }
    ColumnNames = (char **)SDDS_GetColumnNames(&originalfile_page, &NColumnNames);
    for (i = 0; i < n_variables; i++) {
      if (-1 == match_string(ReadbackNames[i], ColumnNames, NColumnNames, EXACT_MATCH)) {
        printf("ReadbackName %s doesn't match any columns in output file.\n", ReadbackNames[i]);
        return (1);
      }
    }
    for (i = 0; i < NColumnNames; i++) {
      if (-1 == match_string(ColumnNames[i], ReadbackNames, n_variables, EXACT_MATCH)) {
        if (-1 == match_string(ColumnNames[i], AuxiliaryColumnNames, NAuxiliaryColumnNames, EXACT_MATCH)) {
          printf("Column %s in output doesn't match any ReadbackName value "
                 "in input file.\n",
                 ColumnNames[i]);
          return (1);
        }
      }
    }

    if ((readCode = SDDS_ReadPage(&originalfile_page)) != 1) {
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
        return (1);
      }
    } else {
      if (!SDDS_InitializeAppend(&output_page, outputfile) ||
          !SDDS_StartTable(&output_page, updateInterval) ||
          !SDDS_CopyParameters(&output_page, &originalfile_page) ||
          !SDDS_Terminate(&originalfile_page)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        return (1);
      }
    }
    CAerrorsColumnPresent = 0;
    if (SDDS_GetColumnIndex(&output_page, "CAerrors") >= 0)
      CAerrorsColumnPresent = 1;
    getTimeBreakdown(&RunStartTime, NULL, NULL, NULL, NULL, NULL, &TimeStamp);
    if (SDDS_CHECK_OKAY == SDDS_CheckParameter(&output_page, "PageTimeStamp", NULL, SDDS_STRING, NULL)) {
      /* SDDS_CopyString(&PageTimeStamp, makeTimeStamp(getTimeInSecs())); */
      SDDS_SetParameters(&output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "PageTimeStamp", TimeStamp, NULL);
    }
  } else {
    /* start with a new file */
    startMonitorFile(&output_page, outputfile, glitchRequest, glitchChannels,
                     alarmTrigRequest, alarmTrigChannels, Precision,
                     n_variables, ReadbackNames, DeviceNames, ReadbackUnits,
                     &StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                     &StartYear, &TimeStamp, commentParameter, commentText, comments,
                     updateInterval);
    RunStartTime = StartTime;
  }
  if (offsetTimeOfDay && totalTimeSet && (StartHour * 3600.0 + TotalTime - 24.0 * 3600.0) > 0.5 * TotalTime) {
    StartHour -= 24;
  }

  if (!(ReadbackIndex = malloc(sizeof(*ReadbackIndex) * n_variables))) {
    fprintf(stderr, "Memory allocation failure\n");
    return (1);
  }
  for (i = 0; i < n_variables; i++) {
    if ((ReadbackIndex[i] = SDDS_GetColumnIndex(&output_page, ReadbackNames[i])) < 0) {
      fprintf(stderr, "Unable to retrieve index for column %s\n", ReadbackNames[i]);
      return (1);
    }
  }
  SampleTimeToExecute = getTimeInSecs() - TimeInterval;

  PingTime = getTimeInSecs() - 2;

  if (!glitchChannels && !alarmTrigChannels) {
    /************\
       * steps loop 
     \*************/

    updateCountdown = updateInterval;
    if (generationsRowLimit)
      newFileCountdown = generationsRowLimit;
    else
      newFileCountdown = -1;
    generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
    for (Step = 0, outputRow = initialOutputRow; Step < NStep; Step++, outputRow++) {
      if (sigint) {
        return (1);
      }
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        if (PingTime + 2 < getTimeInSecs()) {
          PingTime = getTimeInSecs();
          if (runControlPingWhileSleep(0)) {
            fprintf(stderr, "Problem pinging the run control record.\n");
            return (1);
          }
        }
        /*
                strcpy( rcParam.message, "Running");
                rcParam.status = runControlLogMessage( rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
                if ( rcParam.status != RUNCONTROL_OK) {
                fprintf(stderr,"Unable to write status message and alarm severity\n");
                return(1);
                }
              */
      }
#endif

      RunTime = getTimeInSecs() - RunStartTime;
      ElapsedTime = getTimeInSecs() - StartTime;
      if (enforceTimeLimit && RunTime > TotalTime) {
        /* time's up */
        break;
      }

      /* Check stop PV. */
      if (useStop) {
        if (QueryPVDoubleValue(&stopPV, &stopID, stopPendIOTime)) {
          fprintf(stderr, "Problem pinging the stop PV.\n");
          fflush(stderr);
          break;
        }
        if (stopPV.numericalValue > 0) {
          if (verbose) {
            printf("Stop PV %s is nonzero, stopping monitor.\n", stopPV.name);
            fflush(stdout);
          }
          break;
        }
      }

      if (singleShot) {
        /* optionally prompt, then wait for user instruction */
        if (singleShot != SS_NOPROMPT) {
          fputs("Type <cr> to read, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
          fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        }
        fgets(answer, ANSWER_LENGTH, stdin);
        if (answer[0] == 'q' || answer[0] == 'Q') {
          SDDS_UpdatePage(&output_page, FLUSH_TABLE);
          SDDS_Terminate(&output_page);
          return (0);
        }
      } else if (Step) {
        /* execute wait to give the desired time interval */
        if ((SampleTimeToWait = TimeInterval - (getTimeInSecs() - SampleTimeToExecute)) < 0) {
          if (SampleTimeToWait < (TimeInterval * -2))
            SampleTimeToExecute = getTimeInSecs() - TimeInterval;
          SampleTimeToWait = 0;
        }
        if (SampleTimeToWait > 0) {
#ifdef USE_RUNCONTROL
          if ((rcParam.PV) && (SampleTimeToWait > 2)) {
            if (runControlPingWhileSleep(SampleTimeToWait)) {
              fprintf(stderr, "Problem pinging the run control record.\n");
              return (1);
            }
          } else {
            oag_ca_pend_event(SampleTimeToWait, &sigint);
          }
#else
          oag_ca_pend_event(SampleTimeToWait, &sigint);
#endif
        } else {
          ca_poll();
        }
        if (sigint)
          return (1);
      }
      SampleTimeToExecute += TimeInterval;
      LastHour = HourNow;
      HourNow = getHourOfDay();
      if ((dailyFiles && HourNow < LastHour) ||
          (generations && (generationsTimeLimit > 0 || generationsRowLimit) &&
           !newFileCountdown)) {
        /* must start a new file */
        if (!SDDS_Terminate(&output_page)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (generations) {
          /* user-specified generation files */
          char *ptr;
          if (generationsRowLimit)
            newFileCountdown = generationsRowLimit;
          else
            newFileCountdown = -1;
          generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
          updateCountdown = updateInterval;
          ptr = outputfile;
          outputfile =
            MakeGenerationFilename(origOutputFile, generationsDigits, generationsDelimiter,
                                   ptr = outputfile);
          if (ptr)
            free(ptr);
        } else
          /* "daily" log files with the OAG log name format */
          outputfile =
            MakeDailyGenerationFilename(origOutputFile, generationsDigits, generationsDelimiter, 0);
        if (verbose)
          fprintf(stderr, "New file %s started\n", outputfile);
        outputRow = 0;
        startMonitorFile(&output_page, outputfile, glitchRequest, glitchChannels,
                         alarmTrigRequest, alarmTrigChannels, Precision,
                         n_variables, ReadbackNames, DeviceNames, ReadbackUnits,
                         &StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                         &StartYear, &TimeStamp, commentParameter, commentText, comments,
                         updateInterval);
        /* free(outputfile); */
      }
      if (inhibit) {
        if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
          if (CondMode & TOUCH_OUTPUT)
            TouchFile(outputfile);
          if (verbose) {
            printf("Inhibit PV %s is nonzero.  Data collection inhibited.\n", InhibitPV.name);
            fflush(stdout);
          }
          if (SDDSFlushNeeded) {
            if (verbose) {
              printf("Flushing data\n");
              fflush(stdout);
            }
            if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            SDDSFlushNeeded = 0;
          }
          if (CondMode & RETAKE_STEP)
            Step--;
          outputRow--;
          if (InhibitWaitTime > 0) {
#ifdef USE_RUNCONTROL
            if ((rcParam.PV) && (InhibitWaitTime > 2)) {
              if (runControlPingWhileSleep(InhibitWaitTime)) {
                fprintf(stderr, "Problem pinging the run control record.\n");
                return (1);
              }
            } else {
              oag_ca_pend_event(InhibitWaitTime, &sigint);
            }
#else
            oag_ca_pend_event(InhibitWaitTime, &sigint);
#endif
          } else {
            ca_poll();
          }
          if (sigint)
            return (1);
          continue;
        }
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
        if (SDDSFlushNeeded) {
          if (verbose) {
            printf("tested failed, Flushing data\n");
            fflush(stdout);
          }
          if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          SDDSFlushNeeded = 0;
        }
        if (!Step) {
          /* execute wait to give the desired time interval */
          if (TimeInterval > 0) {
#ifdef USE_RUNCONTROL
            if ((rcParam.PV) && (TimeInterval > 2)) {
              if (runControlPingWhileSleep(TimeInterval)) {
                fprintf(stderr, "Problem pinging the run control record.\n");
                return (1);
              }
            } else {
              oag_ca_pend_event(TimeInterval, &sigint);
            }
#else
            oag_ca_pend_event(TimeInterval, &sigint);
#endif
          } else {
            ca_poll();
          }
          if (sigint)
            return (1);
        }
        if (CondMode & RETAKE_STEP)
          Step--;
        outputRow--;
        continue;
      }
      if ((CAerrors = ReadScalarValues(DeviceNames, ReadMessages, ScaleFactors,
                                       values, n_variables, DeviceCHID, pendIOtime)) != 0) {
        switch (oncaerrorindex) {
        case ONCAERROR_USEZERO:
          break;
        case ONCAERROR_SKIPROW:
          continue;
        case ONCAERROR_EXIT:
          return (1);
        }
      }
      if (outputRow && DeadBands &&
          !OutsideDeadBands(values, baselineValues, DeadBands, n_variables)) {
        if (verbose) {
          printf("Step %ld---changes not outside deadbands---continuing\n", Step);
          fflush(stdout);
        }
        outputRow--;
        continue;
      }

      ElapsedTime = (EpochTime = getTimeInSecs()) - StartTime;
      RunTime = EpochTime - RunStartTime;
      if (enforceTimeLimit && RunTime > TotalTime) {
        /* terminate after this pass through loop */
        NStep = Step;
      }
      TimeOfDay = StartHour + ElapsedTime / 3600.0;
      DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
      if (verbose) {
        printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      if (!SDDS_SetRowValues(&output_page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                             "Step", Step, "Time", EpochTime,
                             "TimeOfDay", (float)TimeOfDay, "DayOfMonth", (float)DayOfMonth, NULL))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (!append || CAerrorsColumnPresent) {
        if (!SDDS_SetRowValues(&output_page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, outputRow,
                               "CAerrors", CAerrors, NULL))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (SetValuesFromIndex(&output_page, outputRow, ReadbackIndex, values, n_variables, Precision)) {
        SDDS_Bomb("Something wrong with subroutine SetValuesFromIndex.");
      }
      if (DeadBands)
        /* use the last values as the new set of baseline values */
        SWAP_PTR(values, baselineValues);
      SDDSFlushNeeded = 1;
      --updateCountdown;
      --newFileCountdown;
      if (generationsTimeLimit > 0 && getTimeInSecs() > generationsTimeStop)
        newFileCountdown = 0;
      if (!updateCountdown || !newFileCountdown) {
        updateCountdown = updateInterval;
        if (verbose) {
          printf("Flushing data\n");
          fflush(stdout);
        }
        SDDSFlushNeeded = 0;
        if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (watchInput) {
          if (file_is_modified(inputfile, &inputlink, &input_filestat))
            break;
        }
      } else if (Step >= NStep - 1) {
        if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        break;
      }
    }
  } else {
    /* glitch/trigger and/or alarm trigger */
    /* make buffers for glitch option */
    bufferlength = glitchBefore + 1;
    Node = (DATANODE *)malloc(sizeof(DATANODE));
    Node->values = (double *)malloc(sizeof(double) * n_variables);
    Node->hasdata = 0;
    firstNode = Node;
    for (i = 1; i < bufferlength; i++) {
      Node->next = (DATANODE *)malloc(sizeof(DATANODE));
      Node = Node->next;
      Node->values = (double *)malloc(sizeof(double) * n_variables);
      Node->hasdata = 0;
    }
    Node->next = firstNode;
    Node = firstNode;
    glitchBeforeCounter = glitchAfterCounter = 0;
    writeBeforeBuffer = 0;
    for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
      glitchRequest[iGlitch].triggerArmed = 1;
    afterBufferPending = 0;

    /* make copies of control-names and messages for convenient use with CA routine */
    if (!(glitchControlName = malloc(sizeof(*glitchControlName) * glitchChannels)) ||
        !(glitchControlMessage = malloc(sizeof(*glitchControlMessage) * glitchChannels)) ||
        !(glitchValue = malloc(sizeof(*glitchValue) * glitchChannels)) ||
        !(glitchCHID = malloc(sizeof(*glitchCHID) * glitchChannels)) ||
        !(triggered = malloc(sizeof(*triggered) * glitchChannels)) ||
        !(alarmed = malloc(sizeof(*alarmed) * alarmTrigChannels)))
      SDDS_Bomb("memory allocation failure (glitch data copies)");
    for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
      glitchCHID[iGlitch] = NULL;
      if (!SDDS_CopyString(glitchControlName + iGlitch, glitchRequest[iGlitch].controlName) ||
          !SDDS_CopyString(glitchControlMessage + iGlitch, glitchRequest[iGlitch].message))
        SDDS_Bomb("memory allocation failure (glitch data copies)");
    }

    holdoffTime = holdoffCount = SDDSFlushNeeded = 0;
    lastConditionsFailed = 0;
    setUpAlarmTriggerCallbacks(alarmTrigRequest, alarmTrigChannels, pendIOtime);
    /************
     steps loop 
      ************/
    for (Step = 0; Step < NStep; Step++) {
      if (sigint) {
        return (1);
      }
#ifdef USE_RUNCONTROL
      if (rcParam.PV) {
        if (PingTime + 2 < getTimeInSecs()) {
          PingTime = getTimeInSecs();
          if (runControlPingWhileSleep(0)) {
            fprintf(stderr, "Problem pinging the run control record.\n");
            return (1);
          }
        }
        /*
                strcpy( rcParam.message, "Running");
                rcParam.status = runControlLogMessage( rcParam.handle, rcParam.message, NO_ALARM, &(rcParam.rcInfo));
                if ( rcParam.status != RUNCONTROL_OK) {
                fprintf(stderr,"Unable to write status message and alarm severity\n");
                return(1);
                }
              */
      }
#endif

      ElapsedTime = getTimeInSecs() - StartTime;
      RunTime = getTimeInSecs() - RunStartTime;
      if (enforceTimeLimit && RunTime > TotalTime) {
        /* time's up */
        break;
      }

      /* Check stop PV. */
      if (useStop) {
        if (QueryPVDoubleValue(&stopPV, &stopID, stopPendIOTime)) {
          fprintf(stderr, "Problem pinging the stop PV.\n");
          fflush(stderr);
          break;
        }

        if (stopPV.numericalValue > 0) {
          if (verbose) {
            printf("Stop PV %s has nonzero value, stopping monitor.\n", stopPV.name);
            fflush(stdout);
          }
          break;
        }
      }

      if (watchInput) {
        if (file_is_modified(inputfile, &inputlink, &input_filestat))
          break;
      }
      if (singleShot) {
        if (singleShot != SS_NOPROMPT) {
          fputs("Type <cr> to read, q to quit:\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
          fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
        }
        fgets(answer, ANSWER_LENGTH, stdin);
        if (answer[0] == 'q' || answer[0] == 'Q') {
          SDDS_Terminate(&output_page);
          return (0);
        }
      } else if (Step) {
        if ((SampleTimeToWait = TimeInterval - (getTimeInSecs() - SampleTimeToExecute)) < 0) {
          if (SampleTimeToWait < (TimeInterval * -2))
            SampleTimeToExecute = getTimeInSecs() - TimeInterval;
          SampleTimeToWait = 0;
        }
        if (SampleTimeToWait > 0) {
#ifdef USE_RUNCONTROL
          if ((rcParam.PV) && (SampleTimeToWait > 2)) {
            if (runControlPingWhileSleep(SampleTimeToWait)) {
              fprintf(stderr, "Problem pinging the run control record.\n");
              return (1);
            }
          } else {
            oag_ca_pend_event(SampleTimeToWait, &sigint);
          }
#else
          oag_ca_pend_event(SampleTimeToWait, &sigint);
#endif
        } else {
          ca_poll();
        }
        if (sigint)
          return (1);
      }
      SampleTimeToExecute += TimeInterval;
      LastHour = HourNow;
      HourNow = getHourOfDay();
      if ((dailyFiles && HourNow < LastHour) ||
          (generations && (generationsTimeLimit > 0 || generationsRowLimit) &&
           !newFileCountdown)) {
        /* must start a new file */
        if (!SDDS_Terminate(&output_page)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          return (1);
        }
        if (generations) {
          /* user-specified generation files */
          char *ptr;
          if (generationsRowLimit)
            newFileCountdown = generationsRowLimit;
          else
            newFileCountdown = -1;
          generationsTimeStop = getTimeInSecs() + generationsTimeLimit;
          updateCountdown = updateInterval;
          ptr = outputfile;
          outputfile =
            MakeGenerationFilename(origOutputFile, generationsDigits, generationsDelimiter,
                                   ptr = outputfile);
          if (ptr)
            free(ptr);
        } else
          /* "daily" log files with the OAG log name format */
          outputfile =
            MakeDailyGenerationFilename(origOutputFile, generationsDigits, generationsDelimiter, 0);
        if (verbose)
          fprintf(stderr, "New file %s started\n", outputfile);
        outputRow = 0;
        startMonitorFile(&output_page, outputfile, glitchRequest, glitchChannels,
                         alarmTrigRequest, alarmTrigChannels, Precision,
                         n_variables, ReadbackNames, DeviceNames, ReadbackUnits,
                         &StartTime, &StartDay, &StartHour, &StartJulianDay, &StartMonth,
                         &StartYear, &TimeStamp, commentParameter, commentText, comments,
                         updateInterval);
      }
      if (inhibit) {
        if (QueryInhibitDataCollection(InhibitPV, &inhibitID, InhibitPendIOTime, 0)) {
          if (CondMode & TOUCH_OUTPUT)
            TouchFile(outputfile);
          if (verbose) {
            printf("Inhibit PV %s is nonzero. Data collection inhibited\n", InhibitPV.name);
            fflush(stdout);
          }
          if (SDDSFlushNeeded) {
            if (verbose) {
              printf("Flushing data\n");
              fflush(stdout);
            }
            if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            SDDSFlushNeeded = 0;
          }
          if (!lastConditionsFailed) {
            Node = firstNode;
            glitchBeforeCounter = glitchAfterCounter = writeBeforeBuffer =
              afterBufferPending = holdoffCount = holdoffTime = 0;
            for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
              glitchRequest[iGlitch].triggerArmed = 1;
            for (i = 0; i < bufferlength; i++) {
              Node->hasdata = 0;
            }
          }
          lastConditionsFailed = 1;
          if (InhibitWaitTime > 0) {
#ifdef USE_RUNCONTROL
            if ((rcParam.PV) && (InhibitWaitTime > 2)) {
              if (runControlPingWhileSleep(InhibitWaitTime)) {
                fprintf(stderr, "Problem pinging the run control record.\n");
                return (1);
              }
            } else {
              oag_ca_pend_event(InhibitWaitTime, &sigint);
            }
#else
            oag_ca_pend_event(InhibitWaitTime, &sigint);
#endif
          } else {
            ca_poll();
          }
          if (sigint)
            return (1);
          continue;
        }
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
        if (SDDSFlushNeeded) {
          if (verbose) {
            printf("tested failed, Flushing data\n");
            fflush(stdout);
          }
          if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          SDDSFlushNeeded = 0;
        }
        if (!lastConditionsFailed) {
          Node = firstNode;
          glitchBeforeCounter = glitchAfterCounter = writeBeforeBuffer =
            afterBufferPending = holdoffCount = holdoffTime = 0;
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
            glitchRequest[iGlitch].triggerArmed = 1;
          for (i = 0; i < bufferlength; i++) {
            Node->hasdata = 0;
          }
        }
        lastConditionsFailed = 1;
        continue;
      }
      lastConditionsFailed = 0;

      CAerrors = ReadScalarValues(DeviceNames, ReadMessages, ScaleFactors,
                                  values, n_variables, DeviceCHID, pendIOtime);
      ElapsedTime = (EpochTime = getTimeInSecs()) - StartTime;
      RunTime = EpochTime - RunStartTime;
      if (enforceTimeLimit && RunTime > TotalTime) {
        break;
      }
      TimeOfDay = StartHour + ElapsedTime / 3600.0;
      DayOfMonth = StartDay + ElapsedTime / 3600.0 / 24.0;
      if (verbose) {
        printf("Step %ld. Reading devices at %f seconds.\n", Step, ElapsedTime);
        fflush(stdout);
      }
      /* write values to node */
      Node->step = Step;
      Node->ElapsedTime = ElapsedTime;
      Node->EpochTime = EpochTime;
      Node->TimeOfDay = TimeOfDay;
      Node->DayOfMonth = DayOfMonth;
      for (j = 0; j < n_variables; j++)
        Node->values[j] = values[j];
      Node->hasdata = 1;

      CAerrors = ReadScalarValues(glitchControlName, glitchControlMessage, NULL,
                                  glitchValue, glitchChannels, glitchCHID, pendIOtime);
      if (!CAerrors) {
        if (!baselineStarted) {
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
            glitchRequest[iGlitch].baselineValue = glitchValue[iGlitch];
          baselineStarted = 1;
        }
        if (glitchBeforeCounter < glitchBefore)
          glitchBeforeCounter++;
        if (verbose) {
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
            if (!glitchRequest[iGlitch].triggerMode) {
              printf("glitch quantity %s: baseline=%f slope=%c mode=%s (%f)  current=%f.\n",
                     glitchRequest[iGlitch].controlName,
                     glitchRequest[iGlitch].baselineValue,
                     glitchRequest[iGlitch].glitchSign ? glitchRequest[iGlitch].glitchSign : '*',
                     glitchRequest[iGlitch].flags & GLITCH_DELTA ? "delta" : "fractional",
                     glitchRequest[iGlitch].flags & GLITCH_DELTA ? glitchRequest[iGlitch].delta : glitchRequest[iGlitch].fraction,
                     glitchValue[iGlitch]);
            } else
              printf("Trigger %s:  level=%f slope=%c   current = %f\n",
                     glitchRequest[iGlitch].controlName,
                     glitchRequest[iGlitch].triggerLevel,
                     glitchRequest[iGlitch].triggerSlope, glitchValue[iGlitch]);
          }
          fflush(stdout);
          for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
            printf("alarm quantity %s:  triggered=%hd  severity=%hd\n",
                   alarmTrigRequest[iAlarm].controlName,
                   alarmTrigRequest[iAlarm].triggered,
                   alarmTrigRequest[iAlarm].severity);
          }
        }
        triggerCount = 0;
        triggerAllowed = 1;
        if (holdoffCount > 0) {
          holdoffCount--;
          if (verbose) {
            printf("Holding off trigger (%ld left)\n", holdoffCount);
            fflush(stdout);
          }
          triggerAllowed = 0;
        }
        for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
          triggered[iGlitch] = 0;
          if (!glitchRequest[iGlitch].triggerMode) {
            if (!triggerAllowed)
              continue;
            if (IsGlitch(glitchValue[iGlitch], glitchRequest[iGlitch].baselineValue,
                         glitchRequest[iGlitch].flags, glitchRequest[iGlitch].fraction,
                         glitchRequest[iGlitch].delta, glitchRequest[iGlitch].glitchSign)) {
              triggered[iGlitch] = 1;
              triggerCount++;
              if (verbose) {
                printf("Glitched on %s\n", glitchRequest[iGlitch].controlName);
                fflush(stdout);
              }
              if (!(glitchRequest[iGlitch].flags & GLITCH_NORESET))
                glitchRequest[iGlitch].baselineValue = glitchValue[iGlitch];
              else
                glitchRequest[iGlitch].baselineValue =
                  glitchValue[iGlitch] * glitchRequest[iGlitch].filterFrac +
                  (1 - glitchRequest[iGlitch].filterFrac) * glitchRequest[iGlitch].baselineValue;
              writeBeforeBuffer = 1;
            }
          } else {
            /* trigger mode */
            fflush(stdout);
            if (glitchRequest[iGlitch].triggerArmed) {
              if (!triggerAllowed)
                continue;
              if ((glitchRequest[iGlitch].triggerSlope == '+' && glitchValue[iGlitch] > glitchRequest[iGlitch].triggerLevel) ||
                  (glitchRequest[iGlitch].triggerSlope == '-' && glitchValue[iGlitch] < glitchRequest[iGlitch].triggerLevel)) {
                if (verbose) {
                  printf("Triggered on %s\n", glitchRequest[iGlitch].controlName);
                  fflush(stdout);
                }
                triggered[iGlitch] = 1;
                triggerCount++;
                glitchRequest[iGlitch].triggerArmed = 0;
                writeBeforeBuffer = 1;
              }
            } else {
              /* check for trigger re-arm */
              if ((glitchRequest[iGlitch].flags & TRIGGER_AUTOARM) ||
                  ((glitchRequest[iGlitch].triggerSlope == '+' && glitchValue[iGlitch] < glitchRequest[iGlitch].triggerLevel) ||
                   (glitchRequest[iGlitch].triggerSlope == '-' && glitchValue[iGlitch] > glitchRequest[iGlitch].triggerLevel))) {
                glitchRequest[iGlitch].triggerArmed = 1;
                triggered[iGlitch] = 0;
                if (verbose) {
                  printf("Trigger armed on channel %s.\n", glitchRequest[iGlitch].controlName);
                  fflush(stdout);
                }
              }
            }
          }
        }
        for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
          alarmed[iAlarm] = 0;
          if (triggerAllowed && alarmTrigRequest[iAlarm].triggered) {
            triggerCount++;
            alarmed[iAlarm] = 1;
            alarmTrigRequest[iAlarm].triggered = 0;
            writeBeforeBuffer = 1;
          }
        }

        /* act on trigger */
        if (writeBeforeBuffer && triggerCount) {
          glitchAfterCounter = 0;
          row = 0;
          glitchNode = Node;
          /* search for first valid node of the circular buffer after the glitch node*/
          Node = Node->next;
          while (!Node->hasdata) {
            Node = Node->next;
          }
          if (SDDSFlushNeeded) {
            if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            SDDSFlushNeeded = 0;
          }
          if (!SDDS_StartTable(&output_page, (glitchBeforeCounter + glitchAfter + 1)) ||
              !SDDS_CopyString(&PageTimeStamp, makeTimeStamp(getTimeInSecs())) ||
              !SDDS_SetParameters(&output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                  "PageTimeStamp", PageTimeStamp, NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
            if (!SDDS_SetParameters(&output_page, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    glitchRequest[iGlitch].parameterIndex,
                                    triggered[iGlitch], -1))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
            if (!SDDS_SetParameters(&output_page, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                                    alarmTrigRequest[iAlarm].severityParamIndex,
                                    alarmTrigRequest[iAlarm].severity,
                                    alarmTrigRequest[iAlarm].alarmParamIndex,
                                    alarmed[iAlarm], -1))
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }

          do {
            if (Node->hasdata) {
              if (verbose) {
                printf("Writing before-trigger values for step : %ld at time: %f  row : %ld \n", Node->step,
                       Node->ElapsedTime, row);
                fflush(stdout);
              }
              if (!SDDS_SetRowValues(&output_page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, row, "Step", Node->step,
                                     "Time", Node->EpochTime,
                                     "TimeOfDay", Node->TimeOfDay,
                                     "DayOfMonth", Node->DayOfMonth,
                                     "PostTrigger", (short)0,
                                     NULL)) /* terminate with NULL if SDDS_BY_NAME or with -1 if SDDS_BY_INDEX */
                SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
              if (SetValuesFromIndex(&output_page, row, ReadbackIndex, Node->values, n_variables, Precision)) {
                SDDS_Bomb("Something wrong with subroutine SetValuesFromIndex.");
              }
            }
            Node->hasdata = 0;
            Node = Node->next;
            row++;
          } while (Node != glitchNode->next);
          Node = glitchNode->next;
          if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          writeBeforeBuffer = glitchBeforeCounter = 0;
          afterBufferPending = glitchAfter ? 1 : 0;
          holdoffTime = holdoffCount = 0;
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
            if (triggered[iGlitch] &&
                glitchRequest[iGlitch].flags & (GLITCH_HOLDOFF + GLITCH_AUTOHOLD)) {
              if (glitchRequest[iGlitch].flags & GLITCH_AUTOHOLD) {
                holdoffCount = glitchAfter;
              } else {
                if (holdoffTime < glitchRequest[iGlitch].holdoff)
                  holdoffTime = glitchRequest[iGlitch].holdoff;
              }
            }
          }
          for (iAlarm = 0; iAlarm < alarmTrigChannels; iAlarm++) {
            if (alarmed[iAlarm] &&
                alarmTrigRequest[iAlarm].flags & (ALARMTRIG_HOLDOFF + ALARMTRIG_AUTOHOLD)) {
              if (alarmTrigRequest[iAlarm].flags & ALARMTRIG_AUTOHOLD) {
                holdoffCount = glitchAfter;
              } else {
                if (holdoffTime < alarmTrigRequest[iAlarm].holdoff)
                  holdoffTime = alarmTrigRequest[iAlarm].holdoff;
              }
            }
          }
          if (holdoffTime > holdoffCount * TimeInterval)
            holdoffCount = holdoffTime / TimeInterval;
        } else if (glitchAfterCounter < glitchAfter && afterBufferPending) {
          /* continue collecting data in circular buffer after trigger */
          if (verbose) {
            printf("Writing after-trigger values for step : %ld at time: %f \n", Node->step, Node->ElapsedTime);
            fflush(stdout);
          }
          if (!SDDS_SetRowValues(&output_page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, row, "Step", Node->step,
                                 "Time", Node->EpochTime,
                                 "TimeOfDay", Node->TimeOfDay,
                                 "DayOfMonth", Node->DayOfMonth,
                                 "PostTrigger", (short)1,
                                 NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          if (SetValuesFromIndex(&output_page, row, ReadbackIndex, Node->values, n_variables, Precision)) {
            SDDS_Bomb("Something wrong with subroutine SetValuesFromIndex.");
          }
          SDDSFlushNeeded = 1;
          /*
                    if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
                    SDDS_PrintErrors(stderr,SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors);
                  */
          Node->hasdata = 0;
          Node = Node->next;
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++) {
            if (!glitchRequest[iGlitch].triggerMode)
              glitchRequest[iGlitch].baselineValue =
                glitchValue[iGlitch] * glitchRequest[iGlitch].filterFrac +
                (1 - glitchRequest[iGlitch].filterFrac) * glitchRequest[iGlitch].baselineValue;
          }

          row++;
          glitchAfterCounter++;
        } else {
          /* no trigger yet */
          afterBufferPending = 0;
          for (iGlitch = 0; iGlitch < glitchChannels; iGlitch++)
            if (!glitchRequest[iGlitch].triggerMode)
              glitchRequest[iGlitch].baselineValue =
                glitchValue[iGlitch] * glitchRequest[iGlitch].filterFrac +
                (1 - glitchRequest[iGlitch].filterFrac) * glitchRequest[iGlitch].baselineValue;
          Node = Node->next;
          if (SDDSFlushNeeded && !SDDS_UpdatePage(&output_page, FLUSH_TABLE))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          SDDSFlushNeeded = 0;
        }
      }
    }
  }

  if (SDDSFlushNeeded) {
    if (verbose) {
      printf("Flushing data\n");
      fflush(stdout);
    }
    if (!SDDS_UpdatePage(&output_page, FLUSH_TABLE))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (!SDDS_Terminate(&output_page))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (singleShot && singleShot != SS_NOPROMPT) {
    fputs("Done.\n", singleShot == SS_STDOUTPROMPT ? stdout : stderr);
    fflush(singleShot == SS_STDOUTPROMPT ? stdout : stderr);
  }

#ifdef USE_RUNCONTROL
  if (rcParam.PV) {
    strcpy(rcParam.message, "Application completed normally.");
    runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
    switch (runControlExit(rcParam.handle, &(rcParam.rcInfo))) {
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error exiting run control.\n");
      return (1);
    default:
      fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
      return (1);
    }
  }
#endif

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
  if (ReadbackIndex)
    free(ReadbackIndex);
  if (commentParameter)
    free(commentParameter);
  if (commentText)
    free(commentText);
  if (baselineValues)
    free(baselineValues);
  free_scanargs(&s_arg, argc);

  return (0);
}

long IsGlitch(double value, double baseline, unsigned long flags,
              double fraction, double delta, char sign) {
  double diff;
  diff = value - baseline;
  switch (sign) {
  case '+':
    if (diff <= 0)
      return (0);
    break;
  case '-':
    if (diff >= 0)
      return (0);
    diff = fabs(diff);
    break;
  default:
    diff = fabs(diff);
    break;
  }

  if ((flags & GLITCH_FRACTION && diff / (baseline + 1e-300) > fraction) ||
      diff > delta) {
    return (1);
  }
  return (0);
}

long DefineGlitchParameters(
  SDDS_DATASET *SDDSout,
  GLITCH_REQUEST *glitchRequest,
  long glitchRequests,
  ALARMTRIG_REQUEST *alarmTrigRequest,
  long alarmTrigRequests) {
  long i, j;
  char buffer[1024], descrip[1024];
  for (i = 0; i < glitchRequests; i++) {
    sprintf(buffer, "%sTriggered", glitchRequest[i].controlName);
    sprintf(descrip, "sddsmonitor glitch/trigger status of %s", glitchRequest[i].controlName);
    if ((glitchRequest[i].parameterIndex =
           SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
      return (0);
  }
  for (i = 0; i < alarmTrigRequests; i++) {
    sprintf(descrip, "sddsmonitor alarm-trigger status of %s",
            alarmTrigRequest[i].controlName);
    j = 0;
    while (1) {
      sprintf(buffer, "%sAlarmTrigger%ld", alarmTrigRequest[i].controlName, j);
      if (SDDS_GetParameterIndex(SDDSout, buffer) >= 0)
        j++;
      else
        break;
    }
    if ((alarmTrigRequest[i].alarmParamIndex =
           SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
      return (0);
    sprintf(buffer, "%sAlarmSeverity%ld", alarmTrigRequest[i].controlName, j);
    sprintf(descrip, "EPICS alarm severity of %s",
            alarmTrigRequest[i].controlName);
    if ((alarmTrigRequest[i].severityParamIndex =
           SDDS_DefineParameter(SDDSout, buffer, NULL, NULL, descrip, NULL, SDDS_SHORT, NULL)) < 0)
      return (0);
  }
  return (1);
}

void makeAlarmTriggerUsageMessage(char *alarmTriggerUsage0, char *buffer) {
  long i;
  strcpy(buffer, alarmTriggerUsage0);
  strcat(buffer, "where <statusNames> is a list made up of one or more of\n");
  for (i = 0; i < ALARM_NSEV; i++) {
    strcat(buffer, " ");
    strcat(buffer, alarmSeverityString[i]);
  }
  strcat(buffer, "\nand <sevNames> is a list made up of one or more of\n");
  for (i = 0; i < ALARM_NSTATUS; i++) {
    strcat(buffer, " ");
    strcat(buffer, alarmStatusString[i]);
  }
  strcat(buffer, "\nfor example, severity=(MAJOR,MINOR,INVALID) is equivalent to");
  strcat(buffer, "\nnotSeverity=NO_ALARM");
}

long *CSStringListsToIndexArray(
  long *arraySize,
  char *includeList,
  char *excludeList,
  char **validItem,
  long validItems) {
  long *returnValue = NULL;
  char *item;
  long returnValues = 0, matchIndex, i;
  short *valuePicked = NULL;

  if (!(valuePicked = calloc(validItems, sizeof(*valuePicked))) ||
      !(returnValue = calloc(validItems, sizeof(*returnValue)))) {
    SDDS_Bomb("Memory allocation failure (CSStringListsToArray)");
  }
  if (includeList && excludeList)
    return NULL;
  if (includeList) {
    while ((item = get_token(includeList))) {
      if ((matchIndex = match_string(item, validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_SetError("Invalid list entry (CSStringListsToArray)");
        return NULL;
      }
      valuePicked[matchIndex] = 1;
    }
  } else {
    for (i = 0; i < validItems; i++)
      valuePicked[i] = 1;
    if (excludeList) {
      while ((item = get_token(excludeList))) {
        if ((matchIndex = match_string(item, validItem, validItems, EXACT_MATCH)) < 0) {
          SDDS_SetError("Invalid list entry (CSStringListsToArray)");
          return NULL;
        }
        valuePicked[matchIndex] = 0;
      }
    }
  }
  for (i = returnValues = 0; i < validItems; i++) {
    if (valuePicked[i]) {
      if ((returnValue[returnValues++] = match_string(validItem[i], validItem, validItems, EXACT_MATCH)) < 0) {
        SDDS_Bomb("item not found in second search---this shouldn't happen.");
      }
    }
  }
  if (!returnValues)
    return NULL;
  *arraySize = returnValues;
  return returnValue;
}

long setUpAlarmTriggerCallbacks(
  ALARMTRIG_REQUEST *alarmTrigRequest,
  long alarmTrigChannels,
  double pendIOtime) {
  long i;

  if (!alarmTrigChannels)
    return (0);
  for (i = 0; i < alarmTrigChannels; i++) {
    alarmTrigRequest[i].severity = -1;
    alarmTrigRequest[i].triggered = 0;
    if (ca_search(alarmTrigRequest[i].controlName, &alarmTrigRequest[i].channelID) != ECA_NORMAL) {
      fprintf(stderr, "error: search failed for control name %s\n", alarmTrigRequest[i].controlName);
      return (0);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for the following channels:\n");
    for (i = 0; i < alarmTrigChannels; i++) {
      if (ca_state(alarmTrigRequest[i].channelID) != cs_conn) {
        fprintf(stderr, " %s\n", alarmTrigRequest[i].controlName);
      }
    }
  }
  for (i = 0; i < alarmTrigChannels; i++) {
    alarmTrigRequest[i].usrValue = i;
    if (ca_add_masked_array_event(DBR_TIME_STRING, 1, alarmTrigRequest[i].channelID,
                                  alarmTriggerEventHandler,
                                  (void *)&alarmTrigRequest[i].usrValue, (ca_real)0, (ca_real)0,
                                  (ca_real)0, NULL, DBE_ALARM) != ECA_NORMAL) {
      fprintf(stderr, "error: unable to setup alarm callback for control name %s\n",
              alarmTrigRequest[i].controlName);
      return (1);
    }
  }
  ca_poll();
  return (1);
}

void alarmTriggerEventHandler(struct event_handler_args event) {
  long index, i;
  struct dbr_time_double *dbrValue;

  index = *((long *)event.usr);
  dbrValue = (struct dbr_time_double *)event.dbr;
  alarmTrigRequest[index].status = (short)dbrValue->status;
  alarmTrigRequest[index].severity = (short)dbrValue->severity;
  for (i = 0; i < alarmTrigRequest[index].severityIndices; i++) {
    if (alarmTrigRequest[index].severity == alarmTrigRequest[index].severityIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].severityIndices)
    return;
  for (i = 0; i < alarmTrigRequest[index].severityIndices; i++) {
    if (alarmTrigRequest[index].severity == alarmTrigRequest[index].severityIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].severityIndices)
    return;
  for (i = 0; i < alarmTrigRequest[index].statusIndices; i++) {
    if (alarmTrigRequest[index].status == alarmTrigRequest[index].statusIndex[i])
      break;
  }
  if (i == alarmTrigRequest[index].statusIndices)
    return;
  alarmTrigRequest[index].triggered = 1;
}

void startMonitorFile(SDDS_DATASET *output_page, char *outputfile, GLITCH_REQUEST *glitchRequest,
                      long glitchChannels, ALARMTRIG_REQUEST *alarmTrigRequest,
                      long alarmTrigChannels, long Precision, long n_variables,
                      char **ReadbackNames, char **DeviceNames, char **ReadbackUnits,
                      double *StartTime, double *StartDay, double *StartHour, double *StartJulianDay,
                      double *StartMonth, double *StartYear, char **TimeStamp,
                      char **commentParameter, char **commentText, long comments,
                      long updateInterval) {
  long i;

  getTimeBreakdown(StartTime, StartDay, StartHour, StartJulianDay, StartMonth,
                   StartYear, TimeStamp);
  if (!SDDS_InitializeOutput(output_page, SDDS_BINARY, 1, NULL, NULL, outputfile) ||
      !DefineLoggingTimeParameters(output_page) ||
      !DefineLoggingTimeDetail(output_page, TIMEDETAIL_COLUMNS | TIMEDETAIL_EXTRAS) ||
      !DefineGlitchParameters(output_page, glitchRequest, glitchChannels,
                              alarmTrigRequest, alarmTrigChannels))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  SDDS_EnableFSync(output_page);
  switch (Precision) {
  case PRECISION_SINGLE:
    for (i = 0; i < n_variables; i++) {
      if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_FLOAT, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    break;
  case PRECISION_DOUBLE:
    for (i = 0; i < n_variables; i++) {
      if (SDDS_DefineColumn(output_page, ReadbackNames[i], NULL, ReadbackUnits ? ReadbackUnits[i] : NULL, DeviceNames[i], NULL, SDDS_DOUBLE, 0) < 0)
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    break;
  default:
    bomb("Unknown precision encountered.\n", NULL);
  }
  if ((glitchChannels || alarmTrigChannels) &&
      SDDS_DefineColumn(output_page, "PostTrigger", NULL, NULL, NULL, NULL, SDDS_SHORT, 0) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_DefineSimpleParameters(output_page, comments, commentParameter, NULL, SDDS_STRING) ||
      !SDDS_WriteLayout(output_page) ||
      !SDDS_StartTable(output_page, updateInterval))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_SetParameters(output_page, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "TimeStamp", *TimeStamp, "PageTimeStamp", *TimeStamp,
                          "StartTime", *StartTime, "StartYear", (short)*StartYear,
                          "YearStartTime", computeYearStartTime(*StartTime),
                          "StartJulianDay", (short)*StartJulianDay, "StartMonth", (short)*StartMonth,
                          "StartDayOfMonth", (short)*StartDay, "StartHour", (float)*StartHour,
                          NULL) ||
      !SetCommentParameters(output_page, commentParameter, commentText, comments))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
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
      rcParam.status = runControlExit(rcParam.handle, &(rcParam.rcInfo));
      if (rcParam.status != RUNCONTROL_OK) {
        fprintf(stderr, "Error during exiting run control.\n");
      }
    }
#endif
    ca_task_exit();
  }
}

#ifdef USE_RUNCONTROL
/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;

  timeLeft = sleepTime;
  do {
    rcParam.status = runControlPing(rcParam.handle, &(rcParam.rcInfo));
    switch (rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      strcpy(rcParam.message, "Application timed-out");
      runControlLogMessage(rcParam.handle, rcParam.message, MAJOR_ALARM, &(rcParam.rcInfo));
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
    oag_ca_pend_event((MIN(timeLeft, rcParam.pingInterval)), &sigint);
    if (sigint)
      exit(1);
    timeLeft -= rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif
