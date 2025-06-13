/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: SDDSepics.c
 * routines for SDDS EPICS applications
 * M. Borland, 1995
 $Log: not supported by cvs2svn $
 Revision 1.18  2010/04/15 20:18:57  soliday
 Added oag_ca_exception_handler to capture CA.Client.Exceptions that previously
 were just printed to stdout and did not cause the program to exit with an
 error state.

 Revision 1.17  2008/06/30 21:21:51  shang
 added readScalarValuesByType and moved the INFOBUF definition from SDDSepics.c to SDDSepics.h

 Revision 1.16  2008/06/03 16:28:23  soliday
 Fixed Win32 problem with ushort.

 Revision 1.15  2007/12/07 22:28:53  shang
 added CaTypeToDataType() to convert the ca type to SDDS data type.

 Revision 1.14  2007/04/03 15:26:38  soliday
 Undid last change because it may have caused problems.

 Revision 1.13  2007/03/29 20:07:40  soliday
 Fixed a problem with ReadWaveforms. The length argument is the longest a waveform
 can be, but sometimes other waveforms are shorter. This used to cause it
 to fail.

 Revision 1.12  2006/10/31 21:06:36  jiaox
 Changed a typo in previous version.

 Revision 1.11  2006/10/30 22:09:26  shang
 added ushort and ulong type, and fixed the size of SDDS_LONG which should be size of int32_t

 Revision 1.10  2006/04/05 14:45:33  jiaox
 Fixed a typo from last change.

 Revision 1.9  2006/04/04 21:44:17  soliday
 Xuesong Jiao modified the error messages if one or more PVs don't connect.
 It will now print out all the non-connecting PV names.

 Revision 1.8  2005/11/08 22:05:03  soliday
 Added support for 64 bit compilers.

 Revision 1.7  2004/09/10 14:37:55  soliday
 Changed the flag for oag_ca_pend_event to a volatile variable

 Revision 1.6  2004/07/23 16:26:56  soliday
 Fixed problem with oag_ca_pend_event.

 Revision 1.5  2004/07/22 22:00:49  soliday
 Added oag_ca_pend_event function.

 Revision 1.4  2004/07/15 21:35:40  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base 3.14.x
 has an inactivity timer that was causing disconnects from PVs when the
 log interval time was too large.

 Revision 1.3  2004/05/13 18:17:29  soliday
 Added the ability to define time related arrays.

 Revision 1.2  2004/01/03 19:35:17  borland
 sddswmonitor no longer requires the WaveformLength parameter in the input
 file.  If it is there, it is used.  If not, the waveform length is determined
 from the waveform PVs.

 Revision 1.1  2003/08/27 19:51:04  soliday
 Moved into subdirectory

 Revision 1.73  2003/06/17 20:11:01  soliday
 Added support for AIX.

 Revision 1.72  2003/03/14 00:55:27  shang
 fixed a bug in WriteScalarValues() where the caErrors was not counted when
 error occurs in setting pv values.

 Revision 1.71  2003/03/14 00:48:14  shang
 replace "reading" by "writing" in WriteScalarPVs()

 Revision 1.70  2002/11/27 16:22:04  soliday
 Fixed issues on Windows when built without runcontrol

 Revision 1.69  2002/11/26 22:07:07  soliday
 ezca has been 100% removed from all SDDS Epics programs.

 Revision 1.68  2002/10/31 20:43:49  soliday
 sddsexperiment no longer uses ezca.

 Revision 1.67  2002/10/30 22:55:49  soliday
 sddswmonitor no longer uses ezca

 Revision 1.66  2002/10/21 15:41:08  soliday
 Improved some of the CA routines so that they can handle an IOC going down.

 Revision 1.65  2002/10/17 21:45:03  soliday
 sddswget and sddswput no longer use ezca.

 Revision 1.64  2002/10/17 18:32:57  soliday
 BPS16monitor.c and sddsvmonitor.c no longer use ezca.

 Revision 1.63  2002/10/15 19:13:40  soliday
 Modified so that sddslogger, sddsglitchlogger, and sddsstatmon no longer
 use ezca.

 Revision 1.62  2002/10/08 18:17:40  soliday
 Fixed a ezca bug

 Revision 1.61  2002/10/08 16:10:52  soliday
 Improved the ezca response after calls to ezcaStartGroup

 Revision 1.60  2002/09/30 20:38:49  soliday
 In the doRampPVs I changed the ezca timings to work better.

 Revision 1.59  2002/09/24 20:05:24  soliday
 Added epicsSecondsWestOfUTC

 Revision 1.58  2002/08/27 15:30:07  soliday
 Removed functions that have been moved to mdbcommon

 Revision 1.57  2002/08/14 20:00:31  soliday
 Added Open License

 Revision 1.56  2002/05/07 19:30:06  jba
 Changes for cross only build.

 Revision 1.55  2002/05/07 18:51:00  soliday
 Removed rogue ezca references when not using ezca.

 Revision 1.54  2002/04/17 22:19:35  soliday
 Created some non-ezca functions to replace ezca functions. Hopefully all the
 ezca functions will be replaced in the future.

 Revision 1.53  2002/04/02 20:41:04  shang
 fixed a bug in ReadWaveforms()

 Revision 1.52  2002/04/01 18:03:39  soliday
 vxWorks doesn't do any thing with checkGenerationFileLocks.

 Revision 1.51  2002/03/29 19:59:13  shang
 added getHourMinuteSecond() function and checkGenerationFileLocks(char *match_date) function,
 modified MakeDailyGenerationFilename()

 Revision 1.50  2002/03/25 23:24:24  shang
 added break; statement to each case of the switch() in SizeOfDataType() and
 DataTypeToEzcaType() functions

 Revision 1.49  2002/03/25 20:47:01  shang
 modified ReadWaveforms() such that it will allocate memory for waveformData
 only if it has not been allocated.

 Revision 1.48  2002/03/21 23:45:26  soliday
 Added the if defined(USE_EZCA) statement around functions that use ezca.

 Revision 1.47  2002/03/18 19:04:47  soliday
 Committed Shang's waveform changes and my vxWorks changes.

 Revision 1.46  2001/01/31 17:04:19  borland
 When getting waveform input data, now properly uses the desired type for
 waveforms if no data is in the file.

 Revision 1.45  2000/11/29 19:48:25  borland
 Added getWaveformMonitorData().

 Revision 1.44  2000/11/27 20:12:56  borland
 Fixed a loop construction problem in writeScalarValues that may have reduced
 performance.

 Revision 1.43  2000/04/20 15:58:03  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.42  2000/04/19 15:54:13  soliday
 Added Borland C support.

 Revision 1.41  2000/03/08 17:12:49  soliday
 Removed compiler warnings under Linux.

 Revision 1.40  1999/09/17 22:11:20  soliday
 This version now works with WIN32

 Revision 1.39  1999/08/25 17:56:31  borland
 Rewrote code for data logging inhibiting.  Collected code in SDDSepics.c

 Revision 1.38  1999/05/18 14:54:50  borland
 Made getScalarMonitorNames more forgiving when NULL pointers are passed
 in argument list.

 Revision 1.37  1999/04/30 21:35:56  borland
 Now asserts scale factor=1 when a scale factor of 0 is read in.

 Revision 1.36  1999/04/15 13:27:54  borland
 Removed use of the device library.

 Revision 1.35  1998/11/18 14:32:38  borland
 In getUnits(), now ignore return value from ezcaEndGroup(), as it is
 misleading when the units are undefined.

 Revision 1.34  1998/10/28 18:31:10  borland
 Ordinary generation filenames are now made in such a way that names are
 never reused during a single run.
 Also, commented out usleep code as Solaris 5.6 has usleep.

 Revision 1.33  1998/09/09 15:35:43  borland
 Added getHourOfDay() and MakeDailyGenerationFilename() routines.

 Revision 1.32  1998/07/26 15:22:44  borland
 The file-already-active message from MakeGenerationFilename is now more
 explicit about the name of the file it finds.

 Revision 1.31  1998/06/15 14:25:02  borland
 Added dead-band feature to sddsmonitor.  Required changing getScalarMonitorData,
 which is used to read data from SDDS file specifying how to scalar logging.

 Revision 1.30  1998/06/11 15:23:11  borland
 Fixed a memory leak associated with reading scalar PVs.

 Revision 1.29  1998/05/21 16:44:32  borland
 Added ReadAverageScalarValues, WriteScalarValues, and CheckCAWritePermission.
 Minor changes to ReadScalarValues.

 Revision 1.28  1997/07/31 18:43:46  borland
 Added conditions holdoff to conditions routines.

 Revision 1.27  1997/02/27 23:18:48  borland
 Moved ezcaFree outside of test in readScalarValues, to fix a memory leak.

 Revision 1.26  1997/02/04 22:13:08  borland
 Added error trapping and better messages for group reads.

 Revision 1.25  1996/12/07 16:47:56  borland
 Put conditional in usleep so that it doesn't wait forever for small time
 intervals.

 Revision 1.24  1996/12/02 05:00:22  emery
 Putting back the usleep function written by C. Saunders.

 Revision 1.23  1996/11/22 20:59:23  borland
 Now uses clock_gettime() in place of ftime() for solaris.

 Revision 1.22  1996/11/21 00:25:30  emery
 Removed non-SUN4 special case for function getTimeInSecs.

 Revision 1.21  1996/10/30 20:45:28  emery
 Removed the specially-made usleep function
 when it was realized that solaris has this
 function available.

 Revision 1.20  1996/09/30 19:59:39  borland
 Added Solaris version of usleep using setitimer (from Saunders).

 Revision 1.19  1996/06/23 20:13:48  borland
 Fixed bug in getConditionsData; wasn't setting scaleFactor pointer to NULL.

 Revision 1.18  1996/06/13 19:16:48  borland
 Added SDDS_Terminate call to ReadValueFiles to prevent running out of
 file descriptors.

 * Revision 1.17  1996/04/25  16:07:32  borland
 * Now flags problems in scalar monitor input files.
 *
 * Revision 1.16  1996/04/24  23:11:09  borland
 * Reacts better when ezcaGetUnits returns error messages; basically,
 * it assumes there are no units and doesn't print an error.
 *
 * Revision 1.15  1996/03/07  00:36:09  borland
 * No longer prints ezca error messages when units can't be found.
 *
 * Revision 1.14  1996/02/28  21:14:28  borland
 * Added mode argument to getScalarMonitorData() for control of units control.
 * sddsmonitor and sddsstatmon now have -getUnits option. sddswmonitor and
 * sddsvmonitor now get units from EPICS for any scalar that has a blank
 * units string.
 *
 * Revision 1.13  1996/02/10  06:30:26  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.12  1996/02/07  18:30:12  borland
 * Added option for omitting ReadbackName column from monitor input.  Added
 * choice of delimiter for generation filenames. Added routines DefineLoggingTimeParameters
 * and DefineLoggingTimeDetail for common definition of time entries in SDDS output.
 *
 * Revision 1.11  1996/02/07  18:25:18  borland
 * Upgraded time functions to include fraction motnth calculationplus start-of-year
 * time function for Solaris.
 *
 * Revision 1.10  1996/01/09  17:12:44  borland
 * Fixed bug in ramping when user function was given; added more output for
 * ramping when dry run mode is used.
 *
 * Revision 1.9  1996/01/04  00:22:28  borland
 * Added ganged ramping to experiment programs.
 *
 * Revision 1.8  1996/01/02  18:37:55  borland
 * Added -comment and -generations options to monitor and logging programs;
 * added support routines in SDDSepics.c .
 *
 * Revision 1.7  1995/12/03  01:28:31  borland
 * Added CheckFile and RecoverFile procedures for use by programs that do
 * file appends.
 *
 * Revision 1.6  1995/11/15  00:00:48  borland
 * Added conditional compilation for SOLARIS compatibility (usleep and other
 * time routines).
 *
 * Revision 1.5  1995/11/14  04:35:03  borland
 * Added usleep() substitute for Solaris, along with makefile changes to
 * link these to sdds*experiment.  Corrected nonANSI use of sprintf() in
 * MV20image2sdds.c .
 *
 * Revision 1.4  1995/11/09  03:22:28  borland
 * Added copyright message to source files.
 *
 * Revision 1.3  1995/11/04  04:45:33  borland
 * Added new program sddsalarmlog to Makefile.  Changed routine
 * getTimeBreakdown in SDDSepics, and modified programs to use the new
 * version; it computes the Julian day and the year now.
 *
 * Revision 1.2  1995/10/15  21:17:58  borland
 * Fixed bug in getScalarMonitorData (wasn't initializing *scaleFactor).
 * Fixed bug in sdds[vw]monitor argument parsing for -onCAerror option (had
 * exit outside of if branch).
 *
 * Revision 1.1  1995/09/25  20:15:27  saunders
 * First release of SDDSepics applications.
 *
 */

#ifdef _WIN32
#  include <winsock.h>
#  include <io.h>
#  include <sys/locking.h>
#else
#  include <unistd.h>
#endif

#include "mdb.h"
#include "SDDS.h"
#include "SDDSepics.h"

/*** Replace the ca_client_context::vSignal exception procedure in 
     base/src/ca/ca_client_context.cpp with this one. The difference is 
     that the message is printed to stderr instead of stdout and after 
     the first exception it will exit the program with a error value so 
     that tcl can catch it. ***/

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

  if ((severityInt != 1) && (severityInt != 3)) {
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
    /*I am using _exit instead of exit because some EPICS atexit functions
        are stopping the program from exiting.*/
    _exit(1);
  } else {
    fprintf(stdout, "CA.Client.Exception................\n");
    fprintf(stdout, "    %s: \"%s\"\n",
            severity[severityInt],
            ca_message(args.stat));

    if (args.ctx) {
      fprintf(stdout, "    Context: \"%s\"\n", args.ctx);
    }
    if (args.chid) {
      pName = (char *)ca_name(args.chid);
      fprintf(stdout, "    Channel: \"%s\"\n", pName);
      fprintf(stdout, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
    }
  }
}

/* wrapper for ca_pend_event that allows breaking out early if the
   flag is set from a signal handler. Unlike ca_pend_event, a timeout
   of zero will not be converted to infinity. */
long oag_ca_pend_event(double timeout, volatile int *flag) {
  long status = ECA_NORMAL;

  if (timeout <= 0)
    ca_poll();
  while (timeout > 0) {
    if (*flag)
      return (status);
    if (timeout > 1) {
      status = ca_pend_event(1);
      timeout -= 1;
    } else {
      status = ca_pend_event(timeout);
      timeout = 0;
    }
  }

  return (status);
}

long ReadScalarValuesByType(char **deviceName, char **readMessage, long *dataType,
                            double *scaleFactor, void *value, long n,
                            chid *cid, double pendIOTime) {
  long i, caErrors, pend = 0, type;

  caErrors = 0;
  if (!n)
    return 0;
  if (!deviceName || !value || !cid) {
    fprintf(stderr, "Error: null pointer passed to ReadScalarValues()\n");
    return (-1);
  }

  for (i = 0; i < n; i++) {
    if (readMessage && readMessage[i][0] != 0) {
      fprintf(stderr, "Devices (devSend) no longer supported");
      return (-1);
    } else {
      if (!cid[i]) {
        pend = 1;
        if (ca_search(deviceName[i], &(cid[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", deviceName[i]);
          return (-1);
        }
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      /* exit(1); */
    }
  }
  for (i = 0; i < n; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (dataType)
        type = DataTypeToCaType(dataType[i]);
      else
        type = DBR_DOUBLE;
      if (type == DBR_STRING) {
        ((char **)value)[i] = (char *)malloc(sizeof(char) * 40);
        if (ca_get(type, cid[i], ((char **)value)[i]) != ECA_NORMAL) {
          ((char **)value)[i][0] = 0;
          caErrors++;
          fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
        }
      } else {
        if (ca_get(type, cid[i], (char *)value + i) != ECA_NORMAL) {
          ((char *)value)[i] = 0;
          caErrors++;
          fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
        }
      }
    } else {
      fprintf(stderr, "PV %s is not connected\n", deviceName[i]);
      if (dataType[i] == SDDS_STRING)
        ((char **)value)[i][0] = 0;
      else
        ((char *)value)[i] = 0;
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
    /* exit(1);*/
  }
  if (scaleFactor)
    for (i = 0; i < n; i++) {
      if (dataType[i] != SDDS_STRING && dataType[i] != SDDS_CHARACTER)
        ((double *)value)[i] *= scaleFactor[i];
    }
  return caErrors;
}

long ReadScalarValues(char **deviceName, char **readMessage,
                      double *scaleFactor, double *value, long n,
                      chid *cid, double pendIOTime) {
  long i, caErrors, pend = 0;

  caErrors = 0;
  if (!n)
    return 0;
  if (!deviceName || !value || !cid) {
    fprintf(stderr, "Error: null pointer passed to ReadScalarValues()\n");
    return (-1);
  }

  for (i = 0; i < n; i++) {
    if (readMessage && readMessage[i][0] != 0) {
      fprintf(stderr, "Devices (devSend) no longer supported");
      return (-1);
    } else {
      if (!cid[i]) {
        pend = 1;
        if (ca_search(deviceName[i], &(cid[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", deviceName[i]);
          return (-1);
        }
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      /* exit(1); */
    }
  }
  for (i = 0; i < n; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (ca_get(DBR_DOUBLE, cid[i], value + i) != ECA_NORMAL) {
        value[i] = 0;
        caErrors++;
        fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
      }
    } else {
      fprintf(stderr, "PV %s is not connected\n", deviceName[i]);
      value[i] = 0;
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
    /* exit(1);*/
  }
  if (scaleFactor)
    for (i = 0; i < n; i++)
      value[i] *= scaleFactor[i];
  return caErrors;
}

long ReadAverageScalarValues(char **deviceName, char **readMessage,
                             double *scaleFactor, double *value, long n,
                             long averages, double timeInterval,
                             chid *cid, double pendIOTime) {
  int caErrors = 0;
  long i, j;
  double *sum;

  if (!averages) {
    fprintf(stderr, "Error: variable \"averages\" is zero in function ReadAverageScalarValues.\n");
    exit(1);
  }
  if (!(sum = (double *)malloc(n * sizeof(double)))) {
    fprintf(stderr, "Memory allocation failure in ReadAverageScalarValues\n");
    exit(1);
  }
  for (i = 0; i < n; i++)
    sum[i] = 0;

  for (i = 0; i < averages; i++) {
    if ((caErrors = ReadScalarValues(deviceName, readMessage, scaleFactor, value, n, cid, pendIOTime)) != 0)
      return caErrors;
    for (j = 0; j < n; j++)
      sum[j] += value[j];
    if (i) {
      if (timeInterval > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(timeInterval);
      } else {
        ca_poll();
      }
    }
  }
  for (j = 0; j < n; j++)
    value[j] = sum[j] / averages;
  free(sum);
  return caErrors;
}

long SetValues(SDDS_TABLE *page, long row, char **ReadbackName,
               double *value, long n_variables, long Precision) {
  long j;

  switch (Precision) {
  case PRECISION_SINGLE:
    for (j = 0; j < n_variables; j++) {
      if (!SDDS_SetRowValues(page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, row,
                             ReadbackName[j], (float)value[j], NULL)) {
        fprintf(stderr, "problem setting column %s, row %ld\n", ReadbackName[j], row);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
    }
    break;
  case PRECISION_DOUBLE:
    for (j = 0; j < n_variables; j++) {
      if (!SDDS_SetRowValues(page, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, row,
                             ReadbackName[j], value[j], NULL)) {
        fprintf(stderr, "problem setting column %s, row %ld\n", ReadbackName[j], row);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
    }
    break;
  default:
    bomb("Unknown precision encountered.\n", NULL);
  }
  return (0);
}

long SetValuesFromIndex(SDDS_TABLE *page, long row, long *ReadbackIndex,
                        double *value, long n_variables, long Precision) {
  long j;

  switch (Precision) {
  case PRECISION_SINGLE:
    for (j = 0; j < n_variables; j++) {
      if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                             ReadbackIndex[j], (float)value[j], -1)) {
        fprintf(stderr, "problem setting column %ld, row %ld\n", ReadbackIndex[j], row);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
    }
    break;
  case PRECISION_DOUBLE:
    for (j = 0; j < n_variables; j++) {
      if (!SDDS_SetRowValues(page, SDDS_BY_INDEX | SDDS_PASS_BY_VALUE, row,
                             ReadbackIndex[j], value[j], -1)) {
        fprintf(stderr, "problem setting column %ld, row %ld\n", ReadbackIndex[j], row);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        return 1;
      }
    }
    break;
  default:
    bomb("Unknown precision encountered.\n", NULL);
  }
  return (0);
}

long SizeOfDataType(long dataType) {
  long result = 0;

  switch (dataType) {
  case SDDS_SHORT:
    result = sizeof(short);
    break;
  case SDDS_LONG:
    result = sizeof(int32_t);
    break;
  case SDDS_LONG64:
    result = sizeof(int64_t);
    break;
  case SDDS_USHORT:
    result = sizeof(unsigned short);
    break;
  case SDDS_ULONG:
    result = sizeof(uint32_t);
    break;
  case SDDS_ULONG64:
    result = sizeof(uint64_t);
    break;
  case SDDS_FLOAT:
    result = sizeof(float);
    break;
  case SDDS_DOUBLE:
    result = sizeof(double);
    break;
  case SDDS_LONGDOUBLE:
    result = sizeof(long double);
    break;
  case SDDS_CHARACTER:
    result = sizeof(char);
    break;
  case SDDS_STRING:
    result = sizeof(char *);
    break;
  default:
    bomb("Bad data type passed to SizeOfDataType.", NULL);
    break;
  }
  return (result);
}

long DataTypeToCaType(long dataType) {
  long result = 0;
  switch (dataType) {
  case SDDS_SHORT:
  case SDDS_USHORT:
    result = DBR_SHORT;
    break;
  case SDDS_LONG:
  case SDDS_ULONG:
  case SDDS_LONG64:
  case SDDS_ULONG64:
    result = DBR_LONG;
    break;
  case SDDS_FLOAT:
    result = DBR_FLOAT;
    break;
  case SDDS_DOUBLE:
    result = DBR_DOUBLE;
    break;
  case SDDS_LONGDOUBLE:
    result = DBR_DOUBLE;
    break;
  case SDDS_CHARACTER:
    result = DBR_CHAR;
    break;
  case SDDS_STRING:
    result = DBR_STRING;
    break;
  default:
    bomb("Bad data type passed to DataTypeToCaType.", NULL);
    break;
  }
  return (result);
}

long CaTypeToDataType(long caType) {
  long result = 0;
  switch (caType) {
  case DBF_STRING:
    result = SDDS_STRING;
    break;
  case DBF_ENUM:
    result = SDDS_STRING;
    break;
  case DBF_DOUBLE:
    result = SDDS_DOUBLE;
    break;
  case DBF_FLOAT:
    result = SDDS_FLOAT;
    break;
  case DBF_LONG:
    result = SDDS_LONG;
    break;
  case DBF_SHORT:
    result = SDDS_SHORT;
    break;
  case DBF_CHAR:
    result = SDDS_CHARACTER;
    break;
  default:
    bomb("Bad ca data type passed to CaTypeToDataType.", NULL);
    break;
  }
  return (result);
}

long ReadWaveforms(char **readbackName, void **waveform, long length,
                   long waveforms, int32_t *readbackDataType, chid *cid, double pendIOTime) {
  long i, j, numBuffers = 0, pend = 0;
  long caErrors = 0;
  char **buffer = NULL;

  if (!waveform) {
    fprintf(stderr, "error: NULL waveform array passed to ReadWaveforms\n");
    exit(1);
  }
  if (!cid) {
    fprintf(stderr, "error: NULL pointer passed to ReadWaveforms\n");
    exit(1);
  }

  /*bufferSize = 40 * length; */
  buffer = tmalloc(sizeof(char *) * waveforms);

  for (i = 0; i < waveforms; i++) {
    if (!waveform[i])
      waveform[i] = tmalloc(SizeOfDataType(readbackDataType[i]) * length);
    if (!cid[i]) {
      pend = 1;
      if (ca_search(readbackName[i], &(cid[i])) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", readbackName[i]);
        exit(1);
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (i = 0; i < waveforms; i++) {
        if (ca_state(cid[i]) != cs_conn)
          fprintf(stderr, "%s not connected\n", readbackName[i]);
      }
    }
  }
  for (i = 0; i < waveforms; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (readbackDataType[i] == SDDS_STRING) {
        buffer[numBuffers] = tmalloc(sizeof(char) * (40 * length));
        if (ca_array_get(DBR_STRING, length, cid[i], buffer[numBuffers]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading %s\n", readbackName[i]);
          exit(1);
        }
        numBuffers++;
      } else {
        if (ca_array_get(DataTypeToCaType(readbackDataType[i]), length, cid[i], waveform[i]) != ECA_NORMAL) {
          fprintf(stderr, "error: problem reading %s\n", readbackName[i]);
          exit(1);
        }
      }
    } else {
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
  }
  numBuffers = 0;
  for (i = 0; i < waveforms; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (readbackDataType[i] == SDDS_STRING) {
        for (j = 0; j < length; j++)
          ((char **)waveform[i])[j] = (char *)&(buffer[numBuffers][40 * j]);
        numBuffers++;
      }
    }
  }
  free(buffer);
  if (caErrors)
    printf("%ld CA errors reading waveforms\n", caErrors);
  return caErrors;
}

#include <time.h>

static char *precision_option[PRECISION_OPTIONS] = {
  "single",
  "double",
};

long identifyPrecision(char *precision) {
  return match_string(precision, precision_option, PRECISION_OPTIONS, 0);
}

long getScalarMonitorData(char ***DeviceName, char ***ReadMessage,
                          char ***ReadbackName, char ***ReadbackUnits,
                          double **scaleFactor, double **DeadBands,
                          long *variables, char *inputFile, unsigned long mode,
                          chid **DeviceCHID, double pendIOtime) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName, *MessageColumnName = NULL;
  short UnitsDefined, MessageDefined, ScaleFactorDefined, DeadBandDefined;
  long i, noReadbackName, type;
  INFOBUF *info;
  *DeviceName = NULL;
  if (ReadMessage)
    *ReadMessage = NULL;
  if (ReadbackName)
    *ReadbackName = NULL;
  if (ReadbackUnits)
    *ReadbackUnits = NULL;
  if (scaleFactor)
    *scaleFactor = NULL;
  if (DeadBands)
    *DeadBands = NULL;
  if (DeviceCHID)
    *DeviceCHID = NULL;
  *variables = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  noReadbackName = 1;
  if (ReadbackName) {
    noReadbackName = 0;
    if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "ReadbackName", NULL, SDDS_STRING, NULL)))
      noReadbackName = 1;
  }

  MessageDefined = 0;
  if (ReadMessage) {
    MessageDefined = 1;
    if (!(MessageColumnName =
            SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                            "ReadMessage", "Message", "ReadMsg", NULL))) {
      MessageDefined = 0;
    }
    if (MessageDefined)
      SDDS_Bomb("Messages (devSend) no longer supported.");
  }

  UnitsDefined = 0;
  if (ReadbackUnits) {
    switch (SDDS_CheckColumn(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      UnitsDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ReadbackUnits\".\n");
      exit(1);
      break;
    }
    if (!UnitsDefined) {
      switch (SDDS_CheckColumn(&inSet, "ReadbackUnit", NULL, SDDS_STRING, NULL)) {
      case SDDS_CHECK_OKAY:
        UnitsDefined = 1;
        break;
      case SDDS_CHECK_NONEXISTENT:
        break;
      case SDDS_CHECK_WRONGTYPE:
      case SDDS_CHECK_WRONGUNITS:
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
        printf("Something wrong with column \"ReadbackUnit\".\n");
        exit(1);
        break;
      }
    }
  }

  ScaleFactorDefined = 0;
  if (scaleFactor) {
    switch (SDDS_CheckColumn(&inSet, "ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      ScaleFactorDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"ScaleFactor\".\n");
      exit(1);
      break;
    }
  }

  DeadBandDefined = 0;
  if (DeadBands) {
    switch (SDDS_CheckColumn(&inSet, "DeadBand", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
    case SDDS_CHECK_OKAY:
      DeadBandDefined = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    case SDDS_CHECK_WRONGTYPE:
    case SDDS_CHECK_WRONGUNITS:
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      printf("Something wrong with column \"DeadBand\".\n");
      exit(1);
      break;
    }
  }

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;

  if (!(*variables = SDDS_CountRowsOfInterest(&inSet)))
    bomb("Zero rows defined in input file.\n", NULL);

  *DeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (noReadbackName)
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  else
    *ReadbackName = (char **)SDDS_GetColumn(&inSet, "ReadbackName");
  if (ControlNameColumnName)
    free(ControlNameColumnName);

  if (MessageDefined)
    *ReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
  else if (ReadMessage) {
    *ReadMessage = (char **)malloc(sizeof(char *) * (*variables));
    for (i = 0; i < *variables; i++) {
      (*ReadMessage)[i] = (char *)malloc(sizeof(char));
      (*ReadMessage)[i][0] = 0;
    }
  }

  *DeviceCHID = (chid *)malloc(sizeof(chid) * (*variables));
  for (i = 0; i < *variables; i++)
    (*DeviceCHID)[i] = NULL;

  if (ReadbackUnits) {
    if (!(mode & GET_UNITS_FORCED) && UnitsDefined)
      *ReadbackUnits = (char **)SDDS_GetColumn(&inSet, "ReadbackUnits");
    else {
      *ReadbackUnits = (char **)malloc(sizeof(char *) * (*variables));
      for (i = 0; i < *variables; i++) {
        (*ReadbackUnits)[i] = (char *)malloc(sizeof(char) * (8 + 1));
        (*ReadbackUnits)[i][0] = 0;
      }
    }
    if (mode & GET_UNITS_FORCED || (mode & GET_UNITS_IF_NONE && !UnitsDefined) || mode & GET_UNITS_IF_BLANK) {
      info = (INFOBUF *)malloc(sizeof(INFOBUF) * (*variables));
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_search((*DeviceName)[i], &((*DeviceCHID)[i])) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for %s\n", (*DeviceName)[i]);
            exit(1);
          }
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for some channels\n");
        for (i = 0; i < *variables; i++) {
          if (ca_state((*DeviceCHID)[i]) != cs_conn)
            fprintf(stderr, "%s not connected\n", (*DeviceName)[i]);
        }
        /*  exit(1);*/
      }
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_state((*DeviceCHID)[i]) == cs_conn) {
            type = ca_field_type((*DeviceCHID)[i]);
            if ((type == DBF_STRING) || (type == DBF_ENUM))
              continue;
            if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                             (*DeviceCHID)[i], &info[i]) != ECA_NORMAL) {
              fprintf(stderr, "error: unable to read units.\n");
              exit(1);
            }
          }
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading units for some channels\n");
        /* exit(1);*/
      }
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_state((*DeviceCHID)[i]) == cs_conn) {
            type = ca_field_type((*DeviceCHID)[i]);
            switch (type) {
            case DBF_CHAR:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].c.units);
              break;
            case DBF_SHORT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].i.units);
              break;
            case DBF_LONG:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].l.units);
              break;
            case DBF_FLOAT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].f.units);
              break;
            case DBF_DOUBLE:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].d.units);
              break;
            default:
              break;
            }
          }
        }
      }
      free(info);
    }
  }

  if (scaleFactor && ScaleFactorDefined) {
    if (!(*scaleFactor = SDDS_GetColumnInDoubles(&inSet, "ScaleFactor")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < *variables; i++) {
      if ((*scaleFactor)[i] == 0) {
        fprintf(stderr, "Warning: scale factor for %s is 0.  Set to 1.\n",
                (*DeviceName)[i]);
        (*scaleFactor)[i] = 1;
      }
    }
  }

  if (DeadBands && DeadBandDefined &&
      !(*DeadBands = SDDS_GetColumnInDoubles(&inSet, "DeadBand")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_ReadPage(&inSet) > 0)
    fprintf(stderr, "Warning: extra pages in input file %s---they are ignored\n", inputFile);

  SDDS_Terminate(&inSet);
  return 1;
}

long getConditionsData(char ***DeviceName, char ***ReadMessage,
                       double **scaleFactor, double **LowerLimit,
                       double **UpperLimit, double **HoldoffTime,
                       long *variables, char *inputFile) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName = NULL, *MessageColumnName = NULL;
  short MessageDefined, ScaleFactorDefined, HoldoffDefined;
  long i;

  *DeviceName = *ReadMessage = NULL;
  *variables = 0;
  *LowerLimit = *UpperLimit = NULL;
  *HoldoffTime = NULL;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }

  MessageDefined = 1;
  if (!(MessageColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ReadMessage", "Message", "ReadMsg", NULL))) {
    MessageDefined = 0;
  }

  ScaleFactorDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    ScaleFactorDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"ScaleFactor\".\n");
    exit(1);
    break;
  }

  if (0 >= SDDS_ReadPage(&inSet)) {
    if (ControlNameColumnName)
      free(ControlNameColumnName);
    if (MessageColumnName)
      free(MessageColumnName);
    return 0;
  }

  if (!(*variables = SDDS_CountRowsOfInterest(&inSet)))
    SDDS_Bomb("Zero rows defined in input file.\n");

  *DeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);

  *ReadMessage = NULL;
  if (MessageDefined)
    *ReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
  else {
    *ReadMessage = (char **)malloc(sizeof(char *) * (*variables));
    for (i = 0; i < *variables; i++) {
      (*ReadMessage)[i] = (char *)malloc(sizeof(char));
      (*ReadMessage)[i][0] = 0;
    }
  }

  *scaleFactor = NULL;
  if (ScaleFactorDefined &&
      !(*scaleFactor = SDDS_GetColumnInDoubles(&inSet, "ScaleFactor")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  switch (SDDS_CheckColumn(&inSet, "LowerLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"LowerLimit\".\n");
    exit(1);
    break;
  }
  switch (SDDS_CheckColumn(&inSet, "UpperLimit", NULL, SDDS_ANY_NUMERIC_TYPE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  case SDDS_CHECK_NONEXISTENT:
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    fprintf(stderr, "Something wrong with column \"UpperLimit\".\n");
    exit(1);
    break;
  }

  if (!(*LowerLimit = SDDS_GetColumnInDoubles(&inSet, "LowerLimit")) ||
      !(*UpperLimit = SDDS_GetColumnInDoubles(&inSet, "UpperLimit")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  HoldoffDefined = 0;
  switch (SDDS_CheckColumn(&inSet, "HoldoffTime", NULL, SDDS_ANY_NUMERIC_TYPE, NULL)) {
  case SDDS_CHECK_OKAY:
    HoldoffDefined = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    break;
  case SDDS_CHECK_WRONGTYPE:
  case SDDS_CHECK_WRONGUNITS:
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    printf("Something wrong with column \"HoldoffTime\".\n");
    exit(1);
    break;
  }
  if (HoldoffDefined &&
      !(*HoldoffTime = SDDS_GetColumnInDoubles(&inSet, "HoldoffTime")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  SDDS_Terminate(&inSet);
  if (ControlNameColumnName)
    free(ControlNameColumnName);
  if (MessageColumnName)
    free(MessageColumnName);
  return 1;
}

long getWaveformMonitorData(char ***readbackPV, char ***readbackName, char ***readbackUnits,
                            char ***rmsPVPrefix,
                            double **readbackOffset, double **readbackScale,
                            int32_t **readbackDataType, long *readbacks,
                            long *waveformLength, char *inputFile, long desiredDataType) {
  SDDS_DATASET inSet;
  long code, dataTypeColumnGiven, pvIndex;

  *readbacks = 0;
  *readbackPV = NULL;
  *readbackName = NULL;
  if (readbackUnits)
    *readbackUnits = NULL;
  if (rmsPVPrefix)
    *rmsPVPrefix = NULL;
  if (readbackDataType)
    *readbackDataType = NULL;
  if (readbackOffset)
    *readbackOffset = NULL;
  if (readbackScale)
    *readbackScale = NULL;
  *waveformLength = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if ((SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "WaveformPV", NULL, SDDS_STRING, stderr)) ||
      (SDDS_CHECK_OKAY != SDDS_CheckColumn(&inSet, "WaveformName", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing data in waveform input file");
    return 0;
  }

  if (0 >= SDDS_ReadPage(&inSet)) {
    SDDS_SetError("No data in file");
    return 0;
  }

  *waveformLength = 0;
  if (SDDS_CheckParameter(&inSet, "WaveformLength", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&inSet, "WaveformLength", waveformLength)) {
      SDDS_SetError("Problem getting WaveformLength data in waveform input file");
      return 0;
    }
  }

  if (!(*readbacks = SDDS_CountRowsOfInterest(&inSet))) {
    SDDS_SetError("Zero rows defined in input file.\n");
    return 0;
  }

  if (!(*readbackPV = (char **)SDDS_GetColumn(&inSet, "WaveformPV")) ||
      !(*readbackName = (char **)SDDS_GetColumn(&inSet, "WaveformName"))) {
    SDDS_SetError("Unable to find WaveformPV or WaveformName columns");
    return 0;
  }

  if (readbackOffset && readbackScale) {
    if ((code = SDDS_CheckColumn(&inSet, "ReadbackOffset", NULL, SDDS_ANY_NUMERIC_TYPE,
                                 NULL)) == SDDS_CHECK_OKAY) {
      if (!(*readbackOffset = SDDS_GetColumnInDoubles(&inSet, "ReadbackOffset")) ||
          !(*readbackScale = SDDS_GetColumnInDoubles(&inSet, "ReadbackScale"))) {
        SDDS_SetError("Unable to get ReadbackOffset and ReadbackScale data");
        return 0;
      }
    } else if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_SetError("Problem with data type of ReadbackOffset");
      return 0;
    }
  }

  if (readbackUnits) {
    if ((code = SDDS_CheckColumn(&inSet, "ReadbackUnits", NULL, SDDS_STRING, NULL)) == SDDS_CHECK_OKAY) {
      if (!(*readbackUnits = (char **)SDDS_GetColumn(&inSet, "ReadbackUnits")))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      dataTypeColumnGiven = 1;
    } else if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_SetError("Problem with data type of ReadbackUnits");
      return 0;
    }
  }

  if (rmsPVPrefix) {
    if ((code = SDDS_CheckColumn(&inSet, "rmsPVPrefix", NULL, SDDS_STRING, NULL)) == SDDS_CHECK_OKAY) {
      if (!(*rmsPVPrefix = (char **)SDDS_GetColumn(&inSet, "rmsPVPrefix")))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
  }

  if (readbackDataType) {
    switch (SDDS_CheckColumn(&inSet, "DataType", NULL, SDDS_STRING, NULL)) {
    case (SDDS_CHECK_OKAY):
      dataTypeColumnGiven = 1;
      break;
    case (SDDS_CHECK_NONEXISTENT):
      dataTypeColumnGiven = 0;
      break;
    default:
      SDDS_SetError("Problem with type of DataType column");
      return 0;
    }
    if (!dataTypeColumnGiven) {
      if (!SDDS_VALID_TYPE(desiredDataType))
        desiredDataType = SDDS_FLOAT;
      if (!(*readbackDataType = (int32_t *)malloc(sizeof(int32_t) * (*readbacks)))) {
        SDDS_SetError("memory allocation failure");
        return 0;
      }
      for (pvIndex = 0; pvIndex < *readbacks; pvIndex++)
        (*readbackDataType)[pvIndex] = desiredDataType;
    } else {
      char **readbackDataTypeStrings;
      if (!(readbackDataTypeStrings = SDDS_GetColumn(&inSet, "DataType"))) {
        SDDS_SetError("Problem getting data for DataType column");
        return 0;
      }
      if (!(*readbackDataType = (int32_t *)malloc(sizeof(int32_t) * (*readbacks)))) {
        SDDS_SetError("memory allocation failure");
        return 0;
      }
      for (pvIndex = 0; pvIndex < *readbacks; pvIndex++) {
        (*readbackDataType)[pvIndex] = SDDS_IdentifyType(readbackDataTypeStrings[pvIndex]);
        free(readbackDataTypeStrings[pvIndex]);
      }
      free(readbackDataTypeStrings);
    }
  }

  if (!SDDS_Terminate(&inSet)) {
    SDDS_SetError("problem termining input file");
    return 0;
  }
  return 1;
}

static long verbosity;

void setVerbosity(long verbosityInput) {
  verbosity = verbosityInput;
}

long Read16BitWaveforms(unsigned short **waveform, long waveformLength,
                        CHANNEL_ASSIGNMENT *assignment, long PVs, double pendIOtime) {
  long i;
  static long *maxValue = NULL;

  if (!maxValue)
    maxValue = tmalloc(sizeof(*maxValue) * 2 * PVs);

  if (verbosity)
    fprintf(stderr, "Reading waveforms...");

  for (i = 0; i < PVs; i++) {
    if (!assignment[i].PVchid) {
      if (ca_search(assignment[i].PVname, &(assignment[i].PVchid)) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", assignment[i].PVname);
        return (0);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < PVs; i++) {
      if (ca_state(assignment[i].PVchid) != cs_conn)
        fprintf(stderr, "%s not connected\n", assignment[i].PVname);
    }
    return (0);
  }
  for (i = 0; i < PVs; i++) {
    if (ca_state(assignment[i].PVchid) == cs_conn) {
      if (ca_array_get(DBR_SHORT, waveformLength, assignment[i].PVchid, (short *)waveform[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", assignment[i].PVname);
        return (0);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    return (0);
  }
  if (verbosity)
    fprintf(stderr, "done.\n");
  return 1;
}

void addScalarParameters(SDDS_TABLE *outTable, char ***processVariable,
                         char ***parameter, long *parameters,
                         char *file, long dataType,
                         chid **PVchid, double pendIOtime) {
  SDDS_TABLE inTable;
  long i, rows;
  char **PVname, **readbackName = NULL, *controlColumn, *readbackColumn = NULL;
  char **unitsBuffer;

  if (!SDDS_InitializeInput(&inTable, file))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(controlColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "ControlName", "DeviceName", NULL)) ||
      !(readbackColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "ReadbackName", NULL)))
    bomb("unable to find control or readback columns in scalar file", NULL);

  *parameter = *processVariable = NULL;
  *PVchid = NULL;
  *parameters = 0;
  while (SDDS_ReadTable(&inTable) >= 1) {
    if ((rows = SDDS_CountRowsOfInterest(&inTable)) == 0)
      continue;
    if (!(PVname = (char **)SDDS_GetColumn(&inTable, controlColumn)) ||
        !(readbackName = (char **)SDDS_GetColumn(&inTable, readbackColumn)))
      bomb("unable to get data column from scalar file", NULL);
    *parameter = SDDS_Realloc(*parameter, sizeof(**parameter) * (*parameters + rows));
    *processVariable = SDDS_Realloc(*processVariable, sizeof(**processVariable) * (*parameters + rows));
    *PVchid = SDDS_Realloc(*PVchid, sizeof(chid) * (*parameters + rows));
    for (i = 0; i < rows; i++) {
      (*processVariable)[i + *parameters] = PVname[i];
      (*parameter)[i + *parameters] = readbackName[i];
      (*PVchid)[i + *parameters] = NULL;
    }
    *parameters += rows;
    free(PVname);
    free(readbackName);
  }
  unitsBuffer = tmalloc(sizeof(*unitsBuffer) * (*parameters));
  getUnits(*processVariable, unitsBuffer, *parameters, *PVchid, pendIOtime);
  for (i = 0; i < *parameters; i++)
    if (!SDDS_DefineParameter(outTable, (*parameter)[i], NULL,
                              unitsBuffer[i], NULL, NULL, dataType, NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

long getUnits(char **PVname, char **buffer, long PVs, chid *PVchid, double pendIOtime) {
  long type, i;
  INFOBUF *info = NULL;
  if (verbosity)
    fprintf(stderr, "Getting units...");
  info = (INFOBUF *)malloc(sizeof(INFOBUF) * (PVs));
  for (i = 0; i < PVs; i++) {
    buffer[i] = tmalloc(sizeof(**buffer) * 256);
    buffer[i][0] = 0;
    if (!PVchid[i]) {
      if (ca_search(PVname[i], &(PVchid[i])) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", PVname[i]);
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < PVs; i++) {
      if (ca_state(PVchid[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", PVname[i]);
    }
    /*  exit(1);*/
  }
  for (i = 0; i < PVs; i++) {
    if (ca_state(PVchid[i]) == cs_conn) {
      type = ca_field_type(PVchid[i]);
      if ((type == DBF_STRING) || (type == DBF_ENUM))
        continue;
      if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                       PVchid[i], &info[i]) != ECA_NORMAL) {
        fprintf(stderr, "error: unable to read units.\n");
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    /*  exit(1);*/
  }
  for (i = 0; i < PVs; i++) {
    if (ca_state(PVchid[i]) == cs_conn) {
      type = ca_field_type(PVchid[i]);
      switch (type) {
      case DBF_CHAR:
        free(buffer[i]);
        SDDS_CopyString(&(buffer[i]), info[i].c.units);
        break;
      case DBF_SHORT:
        free(buffer[i]);
        SDDS_CopyString(&(buffer[i]), info[i].i.units);
        break;
      case DBF_LONG:
        free(buffer[i]);
        SDDS_CopyString(&(buffer[i]), info[i].l.units);
        break;
      case DBF_FLOAT:
        free(buffer[i]);
        SDDS_CopyString(&(buffer[i]), info[i].f.units);
        break;
      case DBF_DOUBLE:
        free(buffer[i]);
        SDDS_CopyString(&(buffer[i]), info[i].d.units);
        break;
      default:
        break;
      }
    }
  }
  if (info)
    free(info);
  if (verbosity)
    fprintf(stderr, "done.\n");
  return (1);
}

CHANNEL_ASSIGNMENT *makeAssignments(char *assignmentFile, long *assignments) {
  SDDS_TABLE inTable;
  long i, rows;
  static CHANNEL_ASSIGNMENT default_assignment[4] = {
    {"destWF0", "channel0", NULL, NULL, 0, 1, NULL},
    {"destWF1", "channel1", NULL, NULL, 0, 1, NULL},
    {"destWF2", "channel2", NULL, NULL, 0, 1, NULL},
    {"destWF3", "channel3", NULL, NULL, 0, 1, NULL},
  };
  CHANNEL_ASSIGNMENT *assignment;
  char *controlColumn, *readbackColumn = NULL, *unitsColumn, *offsetColumn, *factorColumn, *analysesColumn;
  char **PV, **signal = NULL, **units, **analyses;
  double *offset, *factor;

  if (!assignmentFile)
    return default_assignment;

  if (!SDDS_InitializeInput(&inTable, assignmentFile))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!(controlColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "ControlName", "DeviceName", NULL)) ||
      !(readbackColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "ReadbackName", NULL)))
    bomb("unable to find control or readback columns in channel assignment file", NULL);

  unitsColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "ReadbackUnits", NULL);
  offsetColumn = SDDS_FindColumn(&inTable, FIND_NUMERIC_TYPE, "ConversionOffset", "Offset",
                                 "ReadbackOffset", NULL);
  factorColumn = SDDS_FindColumn(&inTable, FIND_NUMERIC_TYPE, "ConversionFactor", "Factor",
                                 "ReadbackFactor", NULL);
  analysesColumn = SDDS_FindColumn(&inTable, FIND_SPECIFIED_TYPE, SDDS_STRING, "Analyses",
                                   "analyses", NULL);

  assignment = NULL;
  *assignments = 0;
  units = analyses = NULL;
  offset = NULL;
  factor = NULL;
  while (SDDS_ReadTable(&inTable) > 0) {
    if (!(rows = SDDS_CountRowsOfInterest(&inTable)))
      continue;
    if (!(PV = SDDS_GetColumn(&inTable, controlColumn)) ||
        !(signal = SDDS_GetColumn(&inTable, readbackColumn)) ||
        (unitsColumn && !(units = SDDS_GetColumn(&inTable, unitsColumn))) ||
        (offsetColumn && !(offset = SDDS_GetColumnInDoubles(&inTable, offsetColumn))) ||
        (analysesColumn && !(analyses = SDDS_GetColumn(&inTable, analysesColumn))) ||
        (factorColumn && !(factor = SDDS_GetColumnInDoubles(&inTable, factorColumn))))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    assignment = SDDS_Realloc(assignment, sizeof(*assignment) * (*assignments + rows));
    for (i = 0; i < rows; i++) {
      assignment[i].PVname = PV[i];
      assignment[i].PVchid = NULL;
      /*
            if (strncmp(PV[i], "dest", 4)!=0 || strncmp(PV[i]+strlen(PV[i])-2, "WF", 2)!=0 ) {
            fprintf(stderr, "warning: PV name %s is not of the expected \"dest...WF\" format\n",
            PV[i]);
            }
          */
      assignment[i + *assignments].signalName = signal[i];
      assignment[i + *assignments].convertedUnits = unitsColumn ? units[i] : NULL;
      assignment[i + *assignments].offset = offsetColumn ? offset[i] : 0;
      assignment[i + *assignments].factor = factorColumn ? factor[i] : 1;
      assignment[i + *assignments].analyses = analysesColumn ? analyses[i] : NULL;
      if (assignment[i + *assignments].analyses) {
        /* append a space to the line to aid parsing later */
        assignment[i + *assignments].analyses = SDDS_Realloc(assignment[i + *assignments].analyses,
                                                             (strlen(assignment[i + *assignments].analyses) + 2) *
                                                               sizeof(*assignment[i + *assignments].analyses));
        strcat(assignment[i + *assignments].analyses, " ");
      }
    }
    free(PV);
    free(signal);
    if (units)
      free(units);
    if (offset)
      free(offset);
    if (factor)
      free(factor);
    if (analyses)
      free(analyses);
    *assignments += rows;
  }

  free(controlColumn);
  free(readbackColumn);
  if (unitsColumn)
    free(unitsColumn);
  if (factorColumn)
    free(factorColumn);
  if (analysesColumn)
    free(analysesColumn);

  if (SDDS_ReadTable(&inTable) > 1)
    fprintf(stderr, "warning: assignments file has more than one page---only the first is used\n");
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  return assignment;
}

long PassesConditions(char **deviceName, char **readMessage, double *scaleFactor,
                      double *data, double *lowerLimit, double *upperLimit,
                      double *holdoffTime,
                      long conditions, unsigned long mode,
                      chid *cid, double pendIOTime) {
  long passed, CAerrors, i, tested;
  double maxHoldoff;

  if (!conditions)
    return 1;

  CAerrors = ReadScalarValues(deviceName, readMessage, scaleFactor, data, conditions, cid, pendIOTime);
  if (CAerrors &&
      (mode & MUST_PASS_ALL || CAerrors == conditions))
    return 0;

  passed = tested = 0;
  maxHoldoff = 0;
  for (i = 0; i < conditions; i++) {
    if (lowerLimit[i] < upperLimit[i]) {
      tested++;
      if (data[i] < lowerLimit[i] || data[i] > upperLimit[i]) {
        if (holdoffTime && holdoffTime[i] > maxHoldoff)
          maxHoldoff = holdoffTime[i];
      } else
        passed++;
    }
  }
  if ((mode & MUST_PASS_ALL && tested != passed) ||
      (mode & MUST_PASS_ONE && !passed)) {
    if (maxHoldoff > 0) {
      /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
      ca_pend_event(maxHoldoff);
    } else {
      ca_poll();
    }
    return 0;
  }
  return passed;
}

#define CONDITION_MODES 4
unsigned long IdentifyConditionMode(char **qualifier, long qualifiers) {
  long i, index;
  static char *modeName[CONDITION_MODES] = {"allmustpass", "onemustpass", "touchoutput", "retakestep"};
  static unsigned long modeCode[CONDITION_MODES] = {MUST_PASS_ALL, MUST_PASS_ONE, TOUCH_OUTPUT, RETAKE_STEP};
  unsigned long mode;

  mode = 0;
  for (i = 0; i < qualifiers; i++) {
    if ((index = match_string(qualifier[i], modeName, CONDITION_MODES, 0)) < 0)
      return 0;
    mode |= modeCode[index];
  }
  return mode;
}

long SDDS_CheckFile(char *filename) {
  SDDS_DATASET dataset;
  long code;
  if (!SDDS_InitializeInput(&dataset, filename))
    return 0;
  while ((code = SDDS_ReadPage(&dataset)) > 0) {
  }
  SDDS_Terminate(&dataset);
  if (code == -1)
    return 1;
  return 0;
}

long SDDS_RecoverFile(char *filename, unsigned long mode) {
  char buffer[1024];
  if (!fexists(filename))
    return 0;
  if (mode & RECOVERFILE_VERBOSE)
    fprintf(stderr,
            "warning: file %s is corrupted--reconstructing before appending--some data may be lost.\n",
            filename);
  sprintf(buffer, "sddsconvert -recover -nowarnings %s", filename);
  system(buffer);
  return 1;
}

void ProcessCommentOption(char **argv, int argc,
                          char ***parameter, char ***text, long *commentsPtr) {
  long comments;

  if (argc != 2 || strlen(argv[0]) == 0)
    SDDS_Bomb("invalid -comment syntax--beware of unescaped commas in the text");
  comments = (*commentsPtr += 1);
  if (!((*parameter) = SDDS_Realloc(*parameter, sizeof(**parameter) * comments)) ||
      !((*text) = SDDS_Realloc(*text, sizeof(**text) * comments)))
    SDDS_Bomb("memory allocation failure");

  (*parameter)[comments - 1] = argv[0];
  (*text)[comments - 1] = argv[1];
}

long SetCommentParameters(SDDS_DATASET *dataset, char **parameter, char **text,
                          long comments) {
  long i;

  for (i = 0; i < comments; i++) {
    if (!SDDS_SetParameters(dataset, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            parameter[i], text[i], NULL))
      return 0;
  }
  return 1;
}

static long PVsToRamp = 0, MaxPVsToRamp = 0;
static char **PVToRamp = NULL;
static double *RampPVTo = NULL;
static double *RampPVFrom = NULL;
static double *RampPVStep = NULL;
static chid *RampPVCHID = NULL;

void addRampPV(char *PV, double rampTo, unsigned long mode, chid cid, double pendIOtime) {
  if (mode & (RAMPPV_NEWGROUP | RAMPPV_CLEAR))
    PVsToRamp = 0;
  if (mode & RAMPPV_CLEAR)
    return;
  if (PVsToRamp >= MaxPVsToRamp) {
    MaxPVsToRamp += 10;
    PVToRamp = SDDS_Realloc(PVToRamp, sizeof(*PVToRamp) * MaxPVsToRamp);
    RampPVTo = SDDS_Realloc(RampPVTo, sizeof(*RampPVTo) * MaxPVsToRamp);
    RampPVFrom = SDDS_Realloc(RampPVFrom, sizeof(*RampPVFrom) * MaxPVsToRamp);
    RampPVStep = SDDS_Realloc(RampPVStep, sizeof(*RampPVStep) * MaxPVsToRamp);
    RampPVCHID = SDDS_Realloc(RampPVCHID, sizeof(*RampPVCHID) * MaxPVsToRamp);
  }
  PVToRamp[PVsToRamp] = PV;
  RampPVTo[PVsToRamp] = rampTo;
  RampPVCHID[PVsToRamp] = cid;
  if (RampPVCHID[PVsToRamp] == NULL) {
    if (ca_search(PV, &(RampPVCHID[PVsToRamp])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", PV);
      exit(1);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      if (ca_state(RampPVCHID[PVsToRamp]) != cs_conn)
        fprintf(stderr, "%s not connected\n", PV);
      exit(1);
    }
  }
  PVsToRamp++;
}

void doRampPVs(long steps, double pause, short verbose, double pendIOtime) {
  long i, step, needRamp = 0;

  if (!PVsToRamp) {
    fprintf(stderr, "Warning: doRampPVs called with no ramps set up\n");
    return;
  }
  if (steps < 2)
    steps = 1;
  for (i = 0; i < PVsToRamp; i++) {
    if (ca_get(DBR_DOUBLE, RampPVCHID[i], RampPVFrom + i) != ECA_NORMAL) {
      fprintf(stderr, "Error: unable to get value for %s for ramping\n", PVToRamp[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "Error: unable to get value for %s for ramping\n", PVToRamp[i]);
    exit(1);
  }
  for (i = 0; i < PVsToRamp; i++) {
    if (RampPVFrom[i] != RampPVTo[i]) {
      needRamp = 1;
      if (verbose)
        printf("Ramping %s from %e to %e\n", PVToRamp[i], RampPVFrom[i], RampPVTo[i]);
    }
  }
  if (!needRamp) {
    /*all current values = ramp to values , no need to ramp*/
    return;
  }
  for (i = 0; i < PVsToRamp; i++) {
    if (steps > 1)
      RampPVStep[i] = (RampPVTo[i] - RampPVFrom[i]) / (steps - 1);
    else
      RampPVFrom[i] = RampPVTo[i];
  }
  if (verbose)
    fflush(stdout);
  for (step = 0; step < steps; step++) {
    for (i = 0; i < PVsToRamp; i++) {
      if (ca_put(DBR_DOUBLE, RampPVCHID[i], RampPVFrom + i) != ECA_NORMAL) {
        fprintf(stderr, "Error: unable to put value for %s for ramping\n", PVToRamp[i]);
        exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "Error: unable to put value for %s for ramping\n", PVToRamp[i]);
      exit(1);
    }
    if (pause > 0) {
      /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
      ca_pend_event(pause);
    } else {
      ca_poll();
    }

    if (step < steps - 1)
      for (i = 0; i < PVsToRamp; i++)
        RampPVFrom[i] += RampPVStep[i];
    else
      for (i = 0; i < PVsToRamp; i++)
        RampPVFrom[i] = RampPVTo[i];
  }
  if (verbose) {
    printf("Done ramping.\n");
    fflush(stdout);
  }
}

void showRampPVs(FILE *fp, double pendIOtime) {
  long i;

  if (!PVsToRamp)
    return;
  for (i = 0; i < PVsToRamp; i++) {
    if (ca_get(DBR_DOUBLE, RampPVCHID[i], RampPVFrom + i) != ECA_NORMAL) {
      fprintf(stderr, "Error: unable to get value for %s for ramping\n", PVToRamp[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "Error: unable to get value for %s for ramping\n", PVToRamp[i]);
    exit(1);
  }
  for (i = 0; i < PVsToRamp; i++)
    fprintf(fp, "    %s from %e to %e\n", PVToRamp[i], RampPVFrom[i], RampPVTo[i]);
}

long DefineLoggingTimeParameters(SDDS_DATASET *dataset) {
  if (0 > SDDS_DefineParameter(dataset, "TimeStamp", NULL, NULL,
                               "Time stamp for file", "%28s", SDDS_STRING, NULL) ||
      0 > SDDS_DefineParameter(dataset, "PageTimeStamp", NULL, NULL,
                               "Time stamp for page", "%28s", SDDS_STRING, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartTime", NULL, "s",
                               "Time since start of epoch", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(dataset, "YearStartTime", NULL, "s",
                               "Time since epoch of start of year", NULL, SDDS_DOUBLE, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartYear", NULL, NULL,
                               "Year when file was started", NULL, SDDS_SHORT, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartJulianDay", NULL, NULL,
                               "Julian day when file was started", NULL, SDDS_SHORT, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartMonth", NULL, NULL,
                               "Month when file was started", NULL, SDDS_SHORT, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartDayOfMonth", NULL, NULL,
                               "Day of month when file was started", NULL, SDDS_SHORT, NULL) ||
      0 > SDDS_DefineParameter(dataset, "StartHour", NULL, NULL,
                               "Hour when file was started", NULL, SDDS_DOUBLE, NULL))
    return 0;
  return 1;
}

long DefineLoggingTimeDetail(SDDS_DATASET *dataset, unsigned long mode) {
  int size = SDDS_DOUBLE;
  if (mode & TIMEDETAIL_LONGDOUBLE) {
    size = SDDS_LONGDOUBLE;
  }
  if (mode & TIMEDETAIL_COLUMNS) {
    if ((mode & TIMEDETAIL_EXTRAS) &&
        (0 > SDDS_DefineColumn(dataset, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, 0) ||
         0 > SDDS_DefineColumn(dataset, "CAerrors", NULL, NULL, "Channel access errors for this row",
                               NULL, SDDS_LONG, 0) ||
         0 > SDDS_DefineColumn(dataset, "Time", NULL, "s", "Time since start of epoch", NULL, size, 0)))
      return 0;
    if (0 > SDDS_DefineColumn(dataset, "TimeOfDay", NULL,
                              "h", "Hour of the day", NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(dataset, "DayOfMonth", NULL,
                              "days", "Day of the month", NULL, SDDS_FLOAT, 0))
      return 0;
    return 1;
  }
  if (mode & TIMEDETAIL_ARRAYS) {
    if ((mode & TIMEDETAIL_EXTRAS) &&
        (0 > SDDS_DefineArray(dataset, "Step", NULL, NULL,
                              "Step number",
                              NULL, SDDS_LONG, 0, 1, NULL) ||
         0 > SDDS_DefineArray(dataset, "CAerrors", NULL, NULL,
                              "Channel access errors for this row",
                              NULL, SDDS_LONG, 0, 1, NULL) ||
         0 > SDDS_DefineArray(dataset, "Time", NULL, "s",
                              "Time since start of epoch",
                              NULL, size, 0, 1, NULL)))
      return 0;
    if (0 > SDDS_DefineArray(dataset, "TimeOfDay", NULL, "h",
                             "Hour of the day",
                             NULL, SDDS_FLOAT, 0, 1, NULL) ||
        0 > SDDS_DefineArray(dataset, "DayOfMonth", NULL, "days",
                             "Day of the month",
                             NULL, SDDS_FLOAT, 0, 1, NULL))
      return 0;
    return 1;
  }
  if ((mode & TIMEDETAIL_EXTRAS) &&
      (0 > SDDS_DefineParameter(dataset, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, NULL) ||
       0 > SDDS_DefineParameter(dataset, "CAerrors", NULL, NULL, "Channel access errors for this page",
                                NULL, SDDS_LONG, NULL) ||
       0 > SDDS_DefineParameter(dataset, "Time", NULL, "s", "Time since start of epoch", NULL, size, 0)))
    return 0;
  if (0 > SDDS_DefineParameter(dataset, "TimeOfDay", NULL,
                               "h", "Hour of the day", NULL, SDDS_FLOAT, NULL) ||
      0 > SDDS_DefineParameter(dataset, "DayOfMonth", NULL,
                               "days", "Day of the month", NULL, SDDS_FLOAT, NULL))
    return 0;
  return 1;
}

double *ReadValuesFileData(long *values, char *filename, char *column) {
  SDDS_DATASET dataset;
  double *data, *buffer;
  long i, rows, totalRows;

  if (!filename) {
    SDDS_SetError("NULL filename in ReadValuesFileData");
    return NULL;
  }
  if (!column) {
    SDDS_SetError("NULL filename in ReadValuesFileData");
    return NULL;
  }
  if (!SDDS_InitializeInput(&dataset, filename))
    return NULL;

  buffer = NULL;
  totalRows = 0;
  while (SDDS_ReadPage(&dataset) > 0) {
    if (!(rows = SDDS_CountRowsOfInterest(&dataset)))
      continue;
    if (!(data = SDDS_GetColumnInDoubles(&dataset, column)))
      return NULL;
    if (!(buffer = SDDS_Realloc(buffer, sizeof(*buffer) * (rows + totalRows)))) {
      SDDS_SetError("Memory allocation failure");
      return NULL;
    }
    for (i = 0; i < rows; i++)
      buffer[i + totalRows] = data[i];
    totalRows += rows;
    free(data);
  }
  if (!totalRows) {
    SDDS_SetError("No data in file");
  }
  *values = totalRows;
  if (!SDDS_Terminate(&dataset))
    return NULL;
  return buffer;
}

long WriteScalarValues(char **PVs, double *value,
                       short *writeFlag, long n,
                       chid *cid, double pendIOTime) {
  long caErrors = 0;
  long i, nWritten;

  if (!value) {
    fprintf(stderr, "Error: array \"value\" is NULL in function WriteScalarValues.\n");
    exit(1);
  }
  if (!cid) {
    fprintf(stderr, "Error: array \"cid\" is NULL in function WriteScalarValues.\n");
    exit(1);
  }
  for (i = nWritten = 0; i < n; i++) {
    if (writeFlag && !writeFlag[i])
      continue;
    nWritten++;
  }
  if (!nWritten)
    return 0;
  for (i = 0; i < n; i++) {
    if (writeFlag && !writeFlag[i])
      continue;
    if (ca_state(cid[i]) == cs_conn) {
      if (ca_put(DBR_DOUBLE, cid[i], value + i) != ECA_NORMAL) {
        fprintf(stderr, "error: problem writing %s\n", PVs[i]);
        caErrors++;
      }
    }
  }
  ca_pend_io(pendIOTime);
  for (i = 0; i < n; i++) {
    if (writeFlag && !writeFlag[i])
      continue;
    if (ca_state(cid[i]) != cs_conn) {
      caErrors++;
    }
  }

  return caErrors;
}

long CheckCAWritePermission(char **PVs, long n, chid *cid) {
  long caDenied = 0;
  long i;
  for (i = 0; i < n; i++) {
    if (!ca_write_access(cid[i])) {
      fprintf(stderr, "No write access to %s\n", PVs[i]);
      caDenied++;
    }
  }
  return caDenied;
}

/* compares value[i] to baseline[i].  If the difference for any i is more than
 * deadBand[i], then returns 1.  Otherwise, returns 0.
 */

long OutsideDeadBands(double *value, double *baseline, double *deadBand, long n) {
  if (n <= 0)
    return 0;
  while (n--) {
    if (fabs(*value++ - *baseline++) > *deadBand++)
      return 1;
  }
  return 0;
}

#if !defined(vxWorks)
long QueryInhibitDataCollection(PV_VALUE InhibitPV, chid *inhibitID, double pendIOtime, long connect) {
  if (connect) {
    if (ca_search(InhibitPV.name, inhibitID) != ECA_NORMAL ||
        ca_pend_io(pendIOtime) != ECA_NORMAL)
      return 1; /* will generally cause an exit */
    return 0;
  }
  if (ca_get(DBR_DOUBLE, *inhibitID, &InhibitPV.numericalValue) != ECA_NORMAL ||
      ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "Problem getting value for inhibit PV %s\n",
            InhibitPV.name);
    return 0; /* will not inhibit data collection */
  }
  return InhibitPV.numericalValue != 0;
}

long ConnectPV(PV_VALUE pv, chid *pvID, double pendIOtime) {
  if (ca_search(pv.name, pvID) != ECA_NORMAL || ca_pend_io(pendIOtime) != ECA_NORMAL) {
    return 1; /* will generally cause an exit */
  }
  return 0;
}

long QueryPVDoubleValue(PV_VALUE *pv, chid *pvID, double pendIOtime) {
  if (ca_get(DBR_DOUBLE, *pvID, &(pv->numericalValue)) != ECA_NORMAL || ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "Problem getting value for PV %s\n", pv->name);
    return 1;
  }
  /*fprintf(stdout, "Retrieved PV %s: %f\n", pv->name, pv->numericalValue); */
  return 0;
}
#endif

#if !defined(vxWorks)
long epicsSecondsWestOfUTC(void) {
#  if defined(__APPLE__)
  struct tm *tm;
  time_t clock = 0;
#  endif
  tzset();
#  if defined(_WIN32)
  return (_timezone);
#  endif
#  if defined(__APPLE__)
  tm = localtime(&clock);
  return (-1 * tm->tm_gmtoff);
#  endif
#  if !defined(_WIN32) && !defined(__APPLE__)
  return (__timezone);
#  endif
}
#endif

/*added two more columns -- ReadbackSymbol and UnitsPV, and also returns the ReadbackDataType to handle with
  string PVs (shang, June 2008).*/
long getScalarMonitorDataModified(char ***DeviceName, char ***ReadMessage,
                                  char ***ReadbackName, char ***ReadbackUnits, char ***ReadbackSymbol,
                                  double **scaleFactor, double **DeadBands, long **ReadbackDataType,
                                  long *variables, char *inputFile, unsigned long mode,
                                  chid **DeviceCHID, double pendIOtime) {
  SDDS_DATASET inSet;
  char *ControlNameColumnName, *MessageColumnName = NULL, **UnitsPV = NULL;
  short hasUnits = 0, hasMessage = 0, hasFactor = 0, hasDeadBands = 0, hasSymbol = 0, unitsPVDefined = 0, hasReadback = 0;
  long i, type;
  INFOBUF *info = NULL;
  chid *unitsCHID = NULL;

  *DeviceName = NULL;
  if (ReadMessage)
    *ReadMessage = NULL;
  if (ReadbackName)
    *ReadbackName = NULL;
  if (ReadbackUnits)
    *ReadbackUnits = NULL;
  if (scaleFactor)
    *scaleFactor = NULL;
  if (DeadBands)
    *DeadBands = NULL;
  if (DeviceCHID)
    *DeviceCHID = NULL;
  if (ReadbackDataType)
    *ReadbackDataType = NULL;
  if (ReadbackSymbol)
    *ReadbackSymbol = NULL;

  *variables = 0;

  if (!SDDS_InitializeInput(&inSet, inputFile))
    return 0;

  if (!(ControlNameColumnName =
          SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                          "ControlName", "Device", "DeviceName", NULL))) {
    fprintf(stderr, "Error: ControlName and Device columns both missing or not string type\n");
    exit(1);
  }
  if ((SDDS_CHECK_OKAY == SDDS_CheckColumn(&inSet, (char *)"ReadbackName", NULL, SDDS_STRING, NULL)))
    hasReadback = 1;
  if (ReadMessage && (MessageColumnName = SDDS_FindColumn(&inSet, FIND_SPECIFIED_TYPE, SDDS_STRING,
                                                          "ReadMessage", "Message", "ReadMsg", NULL)))
    hasMessage = 1;
  if (ReadbackSymbol && SDDS_CheckColumn(&inSet, (char *)"ReadbackSymbol", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    hasSymbol = 1;
  if (ReadbackUnits && SDDS_CheckColumn(&inSet, (char *)"ReadbackUnits", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK)
    hasUnits = 1;
  if (scaleFactor && SDDS_CheckColumn(&inSet, (char *)"ScaleFactor", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OK)
    hasFactor = 1;
  if (DeadBands && SDDS_CheckColumn(&inSet, (char *)"DeadBand", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OKAY)
    hasDeadBands = 1;
  if (SDDS_CheckColumn(&inSet, (char *)"UnitsPV", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY)
    unitsPVDefined = 1;

  if (0 >= SDDS_ReadPage(&inSet))
    return 0;

  if (!(*variables = SDDS_CountRowsOfInterest(&inSet)))
    bomb((char *)"Zero rows defined in input file.\n", NULL);
  if (hasSymbol && !(*ReadbackSymbol = (char **)SDDS_GetColumn(&inSet, (char *)"ReadbackSymbol")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  *DeviceName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
  if (ReadbackName) {
    if (!hasReadback)
      *ReadbackName = (char **)SDDS_GetColumn(&inSet, ControlNameColumnName);
    else
      *ReadbackName = (char **)SDDS_GetColumn(&inSet, (char *)"ReadbackName");
  }
  if (ControlNameColumnName)
    free(ControlNameColumnName);

  if (ReadMessage) {
    if (hasMessage)
      *ReadMessage = (char **)SDDS_GetColumn(&inSet, MessageColumnName);
    else {
      *ReadMessage = (char **)malloc(sizeof(char *) * (*variables));
      for (i = 0; i < *variables; i++) {
        (*ReadMessage)[i] = (char *)malloc(sizeof(char));
        (*ReadMessage)[i][0] = 0;
      }
    }
  }

  *DeviceCHID = (chid *)malloc(sizeof(chid) * (*variables));
  for (i = 0; i < *variables; i++)
    (*DeviceCHID)[i] = NULL;

  if (ReadbackUnits) {
    info = (INFOBUF *)malloc(sizeof(INFOBUF) * (*variables));
    if (hasUnits)
      *ReadbackUnits = (char **)SDDS_GetColumn(&inSet, (char *)"ReadbackUnits");
    else {
      *ReadbackUnits = (char **)malloc(sizeof(char *) * (*variables));
      for (i = 0; i < *variables; i++) {
        (*ReadbackUnits)[i] = (char *)malloc(sizeof(char) * (256 + 1));
        (*ReadbackUnits)[i][0] = 0;
      }
    }
  }
  for (i = 0; i < *variables; i++) {
    if (ca_search((*DeviceName)[i], &((*DeviceCHID)[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", (*DeviceName)[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < *variables; i++) {
      if (ca_state((*DeviceCHID)[i]) != cs_conn)
        fprintf(stderr, "%s not connected\n", (*DeviceName)[i]);
    }
    /*  exit(1);*/
  }
  if (unitsPVDefined && ReadbackUnits) {
    UnitsPV = (char **)SDDS_GetColumn(&inSet, (char *)"UnitsPV");
    unitsCHID = (chid *)malloc(sizeof(chid) * (*variables));
    for (i = 0; i < *variables; i++) {
      (unitsCHID)[i] = NULL;
      if (!SDDS_StringIsBlank(UnitsPV[i])) {
        if (ca_search(UnitsPV[i], &(unitsCHID[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", UnitsPV[i]);
          exit(1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      exit(1);
    }
  }

  if (ReadbackDataType)
    *ReadbackDataType = (long *)malloc(sizeof(**ReadbackDataType) * (*variables));
  if (ReadbackDataType || ReadbackUnits) {
    for (i = 0; i < *variables; i++) {
      if (ca_state((*DeviceCHID)[i]) == cs_conn) {
        type = ca_field_type((*DeviceCHID)[i]);
        if (ReadbackDataType) {
          if ((type == DBF_STRING) || (type == DBF_ENUM))
            (*ReadbackDataType)[i] = SDDS_STRING;
          else
            (*ReadbackDataType)[i] = SDDS_DOUBLE;
        }
        if (ReadbackUnits) {
          if ((type == DBF_STRING) || (type == DBF_ENUM))
            continue;
          if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                           (*DeviceCHID)[i], &info[i]) != ECA_NORMAL) {
            fprintf(stderr, "error: unable to read units.\n");
            exit(1);
          }
          if (UnitsPV && !SDDS_StringIsBlank(UnitsPV[i])) {
            if (ca_get(DBF_STRING, unitsCHID[i], (*ReadbackUnits)[i]) != ECA_NORMAL) {
              fprintf(stderr, "error: problem reading values for %s\n", UnitsPV[i]);
              exit(1);
            }
          }
          if (UnitsPV)
            free(UnitsPV[i]);
        }
      }
    }
    if (UnitsPV)
      free(UnitsPV);
    if (ReadbackUnits) {
      if (ca_pend_io(pendIOtime) != ECA_NORMAL)
        fprintf(stderr, "error: problem reading units for some channels\n");
      for (i = 0; i < *variables; i++) {
        if (!(mode & GET_UNITS_IF_BLANK) || SDDS_StringIsBlank((*ReadbackUnits)[i])) {
          if (ca_state((*DeviceCHID)[i]) == cs_conn) {
            type = ca_field_type((*DeviceCHID)[i]);
            switch (type) {
            case DBF_CHAR:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].c.units);
              break;
            case DBF_SHORT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].i.units);
              break;
            case DBF_LONG:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].l.units);
              break;
            case DBF_FLOAT:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].f.units);
              break;
            case DBF_DOUBLE:
              free((*ReadbackUnits)[i]);
              SDDS_CopyString(&((*ReadbackUnits)[i]), info[i].d.units);
              break;
            default:
              break;
            }
          }
        }
      }
    }
  }
  if (info)
    free(info);

  if (scaleFactor && hasFactor) {
    if (!(*scaleFactor = SDDS_GetColumnInDoubles(&inSet, (char *)"ScaleFactor")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < *variables; i++) {
      if ((*scaleFactor)[i] == 0) {
        fprintf(stderr, "Warning: scale factor for %s is 0.  Set to 1.\n",
                (*DeviceName)[i]);
        (*scaleFactor)[i] = 1;
      }
    }
  }

  if (DeadBands && hasDeadBands &&
      !(*DeadBands = SDDS_GetColumnInDoubles(&inSet, (char *)"DeadBand")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (SDDS_ReadPage(&inSet) > 0)
    fprintf(stderr, "Warning: extra pages in input file %s---they are ignored\n", inputFile);

  SDDS_Terminate(&inSet);
  if (unitsCHID)
    free(unitsCHID);

  return 1;
}

long ReadMixScalarValues(char **deviceName, char **readMessage,
                         double *scaleFactor, double *value, long *dataType, char **strData, long n,
                         chid *cid, double pendIOTime) {
  long i, caErrors, pend = 0;

  caErrors = 0;
  if (!n)
    return 0;
  if (!deviceName || !value || !cid) {
    fprintf(stderr, "Error: null pointer passed to ReadScalarValues()\n");
    return (-1);
  }

  for (i = 0; i < n; i++) {
    if (readMessage && readMessage[i][0] != 0) {
      fprintf(stderr, "Devices (devSend) no longer supported");
      return (-1);
    } else {
      if (!cid[i]) {
        pend = 1;
        if (ca_search(deviceName[i], &(cid[i])) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", deviceName[i]);
          return (-1);
        }
      }
    }
  }
  if (pend) {
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      /* exit(1); */
    }
  }
  for (i = 0; i < n; i++) {
    if (ca_state(cid[i]) == cs_conn) {
      if (dataType[i] != SDDS_STRING) {
        if (ca_get(DBR_DOUBLE, cid[i], value + i) != ECA_NORMAL) {
          value[i] = 0;
          caErrors++;
          fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
        }
      } else {
        if (ca_get(DBR_STRING, cid[i], strData[i]) != ECA_NORMAL) {
          caErrors++;
          fprintf(stderr, "error: problem reading %s\n", deviceName[i]);
        }
      }
    } else {
      fprintf(stderr, "PV %s is not connected\n", deviceName[i]);
      value[i] = 0;
      caErrors++;
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
  }
  if (scaleFactor)
    for (i = 0; i < n; i++)
      value[i] *= scaleFactor[i];
  return caErrors;
}
