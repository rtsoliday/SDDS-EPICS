/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * $Log: not supported by cvs2svn $
 * Revision 1.9  2008/06/30 21:21:52  shang
 * added readScalarValuesByType and moved the INFOBUF definition from SDDSepics.c to SDDSepics.h
 *
 * Revision 1.8  2007/12/07 22:28:54  shang
 * added CaTypeToDataType() to convert the ca type to SDDS data type.
 *
 * Revision 1.7  2006/10/20 15:21:07  soliday
 * Fixed problems seen by linux-x86_64.
 *
 * Revision 1.6  2005/11/08 22:05:03  soliday
 * Added support for 64 bit compilers.
 *
 * Revision 1.5  2004/09/10 14:37:55  soliday
 * Changed the flag for oag_ca_pend_event to a volatile variable
 *
 * Revision 1.4  2004/07/22 22:00:49  soliday
 * Added oag_ca_pend_event function.
 *
 * Revision 1.3  2004/07/19 17:39:35  soliday
 * Updated the usage message to include the epics version string.
 *
 * Revision 1.2  2004/05/13 18:17:29  soliday
 * Added the ability to define time related arrays.
 *
 * Revision 1.1  2003/08/27 19:51:05  soliday
 * Moved into subdirectory
 *
 * Revision 1.38  2002/11/26 22:22:07  soliday
 * ezca has been 100% removed from all SDDS Epics programs.
 *
 * Revision 1.37  2002/10/31 20:43:49  soliday
 * sddsexperiment no longer uses ezca.
 *
 * Revision 1.36  2002/10/30 22:55:50  soliday
 * sddswmonitor no longer uses ezca
 *
 * Revision 1.35  2002/10/17 21:45:04  soliday
 * sddswget and sddswput no longer use ezca.
 *
 * Revision 1.34  2002/10/17 18:32:58  soliday
 * BPS16monitor.c and sddsvmonitor.c no longer use ezca.
 *
 * Revision 1.33  2002/10/15 19:13:40  soliday
 * Modified so that sddslogger, sddsglitchlogger, and sddsstatmon no longer
 * use ezca.
 *
 * Revision 1.32  2002/09/24 20:05:25  soliday
 * Added epicsSecondsWestOfUTC
 *
 * Revision 1.31  2002/08/27 15:30:07  soliday
 * Removed functions that have been moved to mdbcommon
 *
 * Revision 1.30  2002/08/14 20:00:31  soliday
 * Added Open License
 *
 * Revision 1.29  2002/05/07 19:30:06  jba
 * Changes for cross only build.
 *
 * Revision 1.28  2002/05/07 18:51:00  soliday
 * Removed rogue ezca references when not using ezca.
 *
 * Revision 1.27  2002/04/17 22:19:36  soliday
 * Created some non-ezca functions to replace ezca functions. Hopefully all the
 * ezca functions will be replaced in the future.
 *
 * Revision 1.26  2002/03/29 19:57:40  shang
 * added char *getHourMinuteSecond() func and void checkGenereationFileLocks(char *match_date)
 * defined USE_TIMETAG constant and modified declaration of MakeDailyGenerationFilename(..)
 *
 * Revision 1.25  2002/03/18 19:04:48  soliday
 * Committed Shang's waveform changes and my vxWorks changes.
 *
 * Revision 1.24  2000/11/29 19:48:25  borland
 * Added getWaveformMonitorData().
 *
 * Revision 1.23  2000/10/16 21:48:03  soliday
 * Removed tsDefs.h include statement.
 *
 * Revision 1.22  2000/04/19 15:54:36  soliday
 * Fixed some prototypes by changing () to (void).
 *
 * Revision 1.21  2000/03/08 17:12:59  soliday
 * Removed compiler warnings under Linux.
 *
 * Revision 1.20  1999/09/17 22:11:31  soliday
 * This version now works with WIN32
 *
 * Revision 1.19  1999/08/25 17:56:31  borland
 * Rewrote code for data logging inhibiting.  Collected code in SDDSepics.c
 *
 * Revision 1.18  1999/05/18 14:56:37  borland
 * Removed include for libdev.h
 *
 * Revision 1.17  1999/03/12 18:30:43  borland
 * No changes.  Spurious need to commit due to CVS mixing up the commenting
 * of update notes.
 *
 * Revision 1.16  1998/11/18 14:33:58  borland
 * Brought up to date with latest version of MakeGenerationFilename().
 *
 * Revision 1.15  1998/09/09 15:35:44  borland
 * Added getHourOfDay() and MakeDailyGenerationFilename() routines.
 *
 * Revision 1.14  1998/07/30 16:53:16  borland
 * Control over data type of each waveform (short, long, float, double, string,
 * character) added by B. Dolin.
 *
 * Revision 1.13  1998/06/15 14:25:03  borland
 * Added dead-band feature to sddsmonitor.  Required changing getScalarMonitorData,
 * which is used to read data from SDDS file specifying how to scalar logging.
 *
 * Revision 1.12  1998/05/21 16:42:30  borland
 * Added three new prototypes plus an ifdef to prevent multiple interpretations
 * of the file.
 *
 * Revision 1.11  1997/07/31 18:43:49  borland
 * Added conditions holdoff to conditions routines.
 *
 * Revision 1.10  1996/02/28  21:14:29  borland
 * Added mode argument to getScalarMonitorData() for control of units control.
 * sddsmonitor and sddsstatmon now have -getUnits option. sddswmonitor and
 * sddsvmonitor now get units from EPICS for any scalar that has a blank
 * units string.
 *
 * Revision 1.9  1996/02/10  06:30:27  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.8  1996/02/07  18:30:34  borland
 * Updated prototypes to match SDDSepics.c
 *
 * Revision 1.7  1996/01/09  17:12:45  borland
 * Fixed bug in ramping when user function was given; added more output for
 * ramping when dry run mode is used.
 *
 * Revision 1.6  1996/01/04  00:22:32  borland
 * Added ganged ramping to experiment programs.
 *
 * Revision 1.5  1996/01/02  18:37:57  borland
 * Added -comment and -generations options to monitor and logging programs;
 * added support routines in SDDSepics.c .
 *
 * Revision 1.4  1995/12/03  01:28:33  borland
 * Added CheckFile and RecoverFile procedures for use by programs that do
 * file appends.
 *
 * Revision 1.3  1995/11/09  03:22:29  borland
 * Added copyright message to source files.
 *
 * Revision 1.2  1995/11/04  04:45:35  borland
 * Added new program sddsalarmlog to Makefile.  Changed routine
 * getTimeBreakdown in SDDSepics, and modified programs to use the new
 * version; it computes the Julian day and the year now.
 *
 * Revision 1.1  1995/09/25  20:15:30  saunders
 * First release of SDDSepics applications.
 * */

#ifndef SDDSEPICS_H_INCLUDED
#define SDDSEPICS_H_INCLUDED 1
//#include <time.h>

#include <cadef.h>
#include <epicsVersion.h>
//#include "SDDS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
  char *PVname, *signalName, *convertedUnits, *analyses;
  double offset, factor;
  chid PVchid;
} CHANNEL_ASSIGNMENT;

typedef union {
  struct dbr_sts_string s;
  struct dbr_ctrl_enum e;
  struct dbr_ctrl_char c;
  struct dbr_ctrl_short i;
  struct dbr_ctrl_long l;
  struct dbr_ctrl_float f;
  struct dbr_ctrl_double d;
} INFOBUF;

void oag_ca_exception_handler(struct exception_handler_args args);
long oag_ca_pend_event(double timeout, volatile int *flag);
long ReadScalarValues(char **deviceName, char **readMessage,
                      double *scaleFactor, double *value, long n,
                      chid *cid, double pendIOTime);
long ReadScalarValuesByType(char **deviceName, char **readMessage, long *dataType,
                            double *scaleFactor, void *value, long n,
                            chid *cid, double pendIOTime);
long ReadAverageScalarValues(char **deviceName, char **readMessage,
                             double *scaleFactor, double *value, long n,
                             long averages, double timeInterval,
                             chid *cid, double pendIOTime);
long WriteScalarValues(char **PVs, double *value, short *writeFlag, long n,
                       chid *cid, double pendIOTime);
long CheckCAWritePermission(char **PVs, long n, chid *cid);
long PassesConditions(char **deviceName, char **readMessage, double *scaleFactor,
                      double *data, double *lowerLimit, double *upperLimit,
                      double *holdoffTime,
                      long conditions, unsigned long mode,
                      chid *cid, double pendIOTime);
long getScalarMonitorData(char ***DeviceName, char ***ReadMessage,
                          char ***ReadbackName, char ***ReadbackUnits,
                          double **scaleFactors, double **deadBands,
                          long *variables, char *inputFile, unsigned long mode,
                          chid **DeviceCHID, double pendIOTime);
void addScalarParameters(SDDS_TABLE *outTable, char ***processVariable, char ***parameter,
                         long *parameters, char *file, long dataType,
                         chid **PVchid, double pendIOtime);
long getUnits(char **PVname, char **buffer, long PVs, chid *PVchid, double pendIOtime);
long Read16BitWaveforms(unsigned short **waveform, long waveformLength, CHANNEL_ASSIGNMENT *assignment, long PVs, double pendIOtime);
long ReadWaveforms(char **readbackName, void **waveform, long length, long waveforms, int32_t *readbackDataType, chid *cid, double pendIOTime);
void addRampPV(char *PV, double rampTo, unsigned long mode, chid cid, double pendIOtime);
void doRampPVs(long steps, double pause, short verbose, double pendIOtime);
void showRampPVs(FILE *fp, double pendIOtime);

long SetValues(SDDS_TABLE *page, long row, char **ReadbackName, double *value, long n_variables, long Precision);
long SetValuesFromIndex(SDDS_DATASET *page, long row, long *ReadbackIndex, double *values, long n_variables, long Precision);
long DataTypeToCaType(long dataType);
long CaTypeToDataType(long caType);

long getScalarMonitorDataModified(char ***DeviceName, char ***ReadMessage,
                                  char ***ReadbackName, char ***ReadbackUnits, char ***ReadbackSymbol,
                                  double **scaleFactor, double **DeadBands, long **ReadbackDataType,
                                  long *variables, char *inputFile, unsigned long mode,
                                  chid **DeviceCHID, double pendIOtime);

long ReadMixScalarValues(char **deviceName, char **readMessage,
                         double *scaleFactor, double *value, long *scalarDataType, char **strScalarData, long n,
                         chid *cid, double pendIOTime);

long identifyPrecision(char *precision);
#define PRECISION_SINGLE 0
#define PRECISION_DOUBLE 1
#define PRECISION_OPTIONS 2

long identifyDataType(char *dataType);

#define GET_UNITS_IF_NONE 0x0001U
#define GET_UNITS_IF_BLANK 0x0002U
#define GET_UNITS_FORCED 0x0004U
long getWaveformMonitorData(char ***readbackPV, char ***readbackName, char ***readbackUnits,
                            char ***rmsPVPrefix,
                            double **readbackOffset, double **readbackScale,
                            int32_t **readbackDataType, long *readbacks,
                            long *waveformLength, char *inputFile, long desiredDataType);
long getConditionsData(char ***DeviceName, char ***ReadMessage,
                       double **scaleFactor, double **lowerLimit,
                       double **upperLimit, double **holdoffTime,
                       long *variables, char *inputFile);
long OutsideDeadBands(double *value, double *baseline, double *deadBand, long n);

#define MUST_PASS_ALL 0x0001UL
#define MUST_PASS_ONE 0x0002UL
#define TOUCH_OUTPUT 0x0004UL
#define RETAKE_STEP 0x0008UL
unsigned long IdentifyConditionMode(char **qualifier, long qualifiers);

CHANNEL_ASSIGNMENT *makeAssignments(char *assignmentFile, long *assignments);

void setVerbosity(long verbosityInput);

long SDDS_CheckFile(char *filename);
long SDDS_RecoverFile(char *filename, unsigned long mode);
#define RECOVERFILE_VERBOSE 0x1UL

#define DEFAULT_GENERATIONS_DIGITS 4
#define USE_TIMETAG 0x0010U
long SetCommentParameters(SDDS_DATASET *dataset, char **parameter, char **text, long comments);
void ProcessCommentOption(char **argv, int argc,
                          char ***parameter, char ***text, long *commentsPtr);

#define RAMPPV_NEWGROUP 0x0001UL
#define RAMPPV_CLEAR 0x0002UL

long DefineLoggingTimeDetail(SDDS_DATASET *dataset, unsigned long mode);
#define TIMEDETAIL_COLUMNS 0x0001UL
#define TIMEDETAIL_EXTRAS 0x0002UL
#define TIMEDETAIL_ARRAYS 0x0004UL
#define TIMEDETAIL_LONGDOUBLE 0x0008UL
long DefineLoggingTimeParameters(SDDS_DATASET *dataset);

double *ReadValuesFileData(long *values, char *filename, char *column);

long SizeOfDataType(long dataType);

long epicsSecondsWestOfUTC(void);

#if !defined(vxWorks)
#  include "../../oagca/pvMultList.h"
long QueryInhibitDataCollection(PV_VALUE InhibitPV, chid *inhibitID, double pendIOtime, long connect);
long ConnectPV(PV_VALUE pv, chid *pvID, double pendIOtime);
long QueryPVDoubleValue(PV_VALUE *pv, chid *pvID, double pendIOtime);
#endif

#endif
#ifdef __cplusplus
}
#endif
