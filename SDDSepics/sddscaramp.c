/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
  $Log: not supported by cvs2svn $
  Revision 1.6  2006/04/04 21:44:17  soliday
  Xuesong Jiao modified the error messages if one or more PVs don't connect.
  It will now print out all the non-connecting PV names.

  Revision 1.5  2005/11/08 22:05:03  soliday
  Added support for 64 bit compilers.

  Revision 1.4  2004/07/19 17:39:36  soliday
  Updated the usage message to include the epics version string.

  Revision 1.3  2004/07/15 21:22:46  soliday
  Replaced sleep commands with ca_pend_event commands because Epics/Base 3.14.x
  has an inactivity timer that was causing disconnects from PVs when the
  log interval time was too large.

  Revision 1.2  2004/03/10 22:15:00  borland
  Removed debugging printouts.  Added -controlNameColumn option to allow using
  a non-standard column for PV names.

  Revision 1.1  2003/08/27 19:51:08  soliday
  Moved into subdirectory

  Revision 1.12  2002/10/17 21:45:04  soliday
  sddswget and sddswput no longer use ezca.

  Revision 1.11  2002/10/17 20:13:05  soliday
  sddscaramp.c no longer uses ezca.

  Revision 1.10  2002/08/14 20:00:32  soliday
  Added Open License

  Revision 1.9  2001/05/03 19:53:44  soliday
  Standardized the Usage message.

  Revision 1.8  2000/12/06 21:03:09  soliday
  Made changes needed for WIN32.

  Revision 1.7  2000/11/20 16:52:35  soliday
  Fixed some warning messages.

  Revision 1.6  1998/12/16 21:50:20  soliday
  EzcaPut command now works for string data

  Revision 1.5  1998/12/16 15:28:19  soliday
  Fixed bug when ValueString contains non numeric data.

  Revision 1.4  1998/12/15 15:18:07  soliday
  Added the ability to use numeric types in the Value column.

  Revision 1.3  1998/12/14 20:16:56  borland
  Removed option to use "Value" column and made it more explicit that
  the code only accepts string data.  Added CVS log entry place marker.

*/

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

#include <stdlib.h>
#ifndef _WIN32
#  include <sys/time.h>
#endif
#include <sys/types.h>
#include <time.h>
#include <signal.h>
/*#include <tsDefs.h>*/
#include <cadef.h>
#include <errno.h>
#include <string.h>

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"

#define CA_RAMPTO 0
#define CA_DATACOLUMN 1
#define CA_VERBOSE 2
#define CA_PENDIOTIME 3
#define CA_CONTROLNAMECOLUMN 4
#define CA_RESEND_FINAL 5
#define COMMANDLINE_OPTIONS 6
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "rampTo",
  "dataColumn",
  "verbose",
  "pendiotime",
  "controlNameColumn",
  "resendFinal",
};

#define NUMERICAL_YES 0
#define NUMERICAL_NO 1
#define NUMERICAL_OPTIONS 2
static char *numerical_option[NUMERICAL_OPTIONS] = {
  "y",
  "n"};

typedef struct
{
  char *name;
  double final_value;
  double current_value;
  char *final_string;
  double increment;
  long isNumber;
  chid PVchid;
} PVDATA;

typedef struct
{
  char *filename;
  int32_t steps;
  short differentialMode;
  double TimeInterval, percentage, differentialFactor;
} RAMPTO;

static char *USAGE = "sddscaramp [-rampTo=<filename>,steps=<number>,pause=<secs>[,percentage=<value>][,differential=<factor>]]...\n\
[-controlNameColumn=<string>] [-dataColumn=<columnName>] [-verbose]\n\
[-pendIOtime=<seconds>] [-resendFinal] \n\n\
Takes output file from burtrb or sddssnapshot and ramps the PVs\n\
from their current settings to those defined in the file.\n\
There must be a column named \"ControlName\", \"Device\", or \"DeviceName\"\n\
that contains the process variable name; alternatively, you may name the column\n\
using the -controlNameColumn option.  There must also be a column named \n\
\"ValueString\", \"Value\", or a -dataColumn specified.  This\n\
column contains the final value for the corresponding process variable.\n\
The \"IsNumerical\" column is optional; if present, this character column\n\
should contain \"y\" or \"n\".  If this column does not exist, it is assumed\n\
that the values are numerical.\n\
Program by Robert Soliday.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

int main(int argc, char **argv) {
  PVDATA *pvdata = NULL;
  RAMPTO *rampto = NULL;
  SDDS_DATASET SDDS_dataset;
  SCANNED_ARG *s_arg;
  long i_arg, *type, column_names, i, j, a, retval, verbose, resendFinal = 0;
  long rows, r, Numerical, numFilenames;
  unsigned long dummyFlags;
  double timeToExecute, timeToWait;
  char **columnFormat, **column_name, *s, numerical_string[40], *dataColumn, char_string[41];
  char *controlNameColumn;
  void **data;
  double pendIOtime = 10.0;

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  columnFormat = column_name = NULL;
  controlNameColumn = dataColumn = NULL;
  numFilenames = verbose = 0;
  Numerical = NUMERICAL_YES;
  rampto = tmalloc(sizeof(*rampto) * 20);

  /* Start reading command line options */
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CA_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2 || sscanf(s_arg[i_arg].list[1], "%lf", &pendIOtime) != 1 ||
            pendIOtime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CA_RAMPTO:
        if (s_arg[i_arg].n_items < 4)
          bomb("wrong number of items for -rampTo", NULL);
        SDDS_CopyString(&(rampto[numFilenames].filename), s_arg[i_arg].list[1]);
        s_arg[i_arg].n_items -= 2;
        rampto[numFilenames].steps = -1;
        rampto[numFilenames].TimeInterval = -1.0;
        rampto[numFilenames].percentage = 100;
        rampto[numFilenames].differentialMode = 0;
        rampto[numFilenames].differentialFactor = 0;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                          "steps", SDDS_LONG, &rampto[numFilenames].steps, 1, 0,
                          "pause", SDDS_DOUBLE, &rampto[numFilenames].TimeInterval, 1, 0,
                          "percentage", SDDS_DOUBLE, &rampto[numFilenames].percentage, 1, 1,
                          "differential", SDDS_DOUBLE, &rampto[numFilenames].differentialFactor, 1, 2,
                          NULL) ||
            rampto[numFilenames].steps <= 0 || rampto[numFilenames].TimeInterval < 0 ||
            rampto[numFilenames].percentage < 0 || rampto[numFilenames].percentage > 100)
          bomb("invalid -rampTo values", NULL);
        if (dummyFlags&2)
          rampto[numFilenames].differentialMode = 1;
        numFilenames++;
        s_arg[i_arg].n_items += 2;
        break;
      case CA_DATACOLUMN:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -dataColumn", NULL);
        dataColumn = s_arg[i_arg].list[1];
        break;
      case CA_CONTROLNAMECOLUMN:
        if (s_arg[i_arg].n_items != 2)
          bomb("wrong number of items for -controlNameColumn", NULL);
        controlNameColumn = s_arg[i_arg].list[1];
        break;
      case CA_VERBOSE:
        verbose = 1;
        break;
      case CA_RESEND_FINAL:
        resendFinal = 1;
        break;
      default:
        bomb("unrecognized option given", NULL);
        break;
      }
    }
  }
  /* Finished reading command line options */

  /* Start ramping procedure for each file, one at a time */
  for (a = 0; a < numFilenames; a++) {
    column_names = 3;
    column_name = tmalloc(sizeof(*column_name) * column_names);
    type = tmalloc(sizeof(*type) * column_names);
    data = tmalloc(sizeof(*data) * column_names);
    columnFormat = tmalloc(sizeof(*columnFormat) * column_names);

    /* Open file to get PV name and final value */
    if (!SDDS_InitializeInput(&SDDS_dataset, rampto[a].filename)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }

    if (!controlNameColumn) {
      if (!(column_name[0] = SDDS_FindColumn(&SDDS_dataset, FIND_SPECIFIED_TYPE, SDDS_STRING,
                                             "ControlName", "Device", "DeviceName", NULL))) {
        fprintf(stderr, "Error: ControlName and Device columns are both missing or not string type\n");
        exit(1);
      }
    } else {
      if (!(column_name[0] = SDDS_FindColumn(&SDDS_dataset, FIND_SPECIFIED_TYPE, SDDS_STRING,
                                             controlNameColumn, NULL))) {
        fprintf(stderr, "Error: %s column missing or not string type\n", controlNameColumn);
        exit(1);
      }
    }

    if (dataColumn == NULL) {
      if (!(column_name[1] = SDDS_FindColumn(&SDDS_dataset, FIND_ANY_TYPE,
                                             "ValueString", "Value", NULL))) {
        fprintf(stderr, "Error: ValueString and Value columns are both missing\n");
        exit(1);
      }
    } else {
      if (!(column_name[1] = SDDS_FindColumn(&SDDS_dataset, FIND_ANY_TYPE,
                                             dataColumn, NULL))) {
        fprintf(stderr, "Error: %s is missing\n", dataColumn);
        exit(1);
      }
    }

    if (!(column_name[2] = SDDS_FindColumn(&SDDS_dataset, FIND_SPECIFIED_TYPE,
                                           SDDS_CHARACTER, "IsNumerical", NULL))) {
      column_names = 2;
    }

    for (i = 0; i < column_names; i++) {
      if ((j = SDDS_GetColumnIndex(&SDDS_dataset, column_name[i])) < 0) {
        fprintf(stderr, "error: column %s does not exist\n", column_name[i]);
        exit(1);
      }
      if ((type[i] = SDDS_GetColumnType(&SDDS_dataset, j)) <= 0 ||
          !SDDS_GetColumnInformation(&SDDS_dataset, "format_string", columnFormat + i, SDDS_GET_BY_INDEX, j)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    rows = 0;
    retval = -1;
    while ((retval = SDDS_ReadPage(&SDDS_dataset)) > 0) {
      if ((r = SDDS_CountRowsOfInterest(&SDDS_dataset)) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      if (r) {
        rows += r;
        pvdata = SDDS_Realloc(pvdata, sizeof(*pvdata) * rows);

        for (i = 0; i < column_names; i++)
          if (!(data[i] = SDDS_GetInternalColumn(&SDDS_dataset, column_name[i]))) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
        for (j = rows - r; j < rows; j++) {
          pvdata[j].PVchid = NULL;
          pvdata[j].final_string = NULL;
          cp_str(&pvdata[j].name, ((char **)data[0])[j - rows + r]);
          switch (type[1]) {
          case SDDS_STRING:
            if (column_names == 3) {
              strncpy(numerical_string, ((char *)data[2]) + j - rows + r, 1);
              switch (Numerical = match_string(numerical_string, numerical_option, NUMERICAL_OPTIONS, 0)) {
              case NUMERICAL_YES:
                errno = 0;
                pvdata[j].final_value = strtod(((char **)data[1])[j - rows + r], &s);
                if (errno == ERANGE) {
                  fprintf(stderr, "error: strtod reports range error");
                  exit(1);
                }
                pvdata[j].isNumber = 1;
                break;
              default:
                cp_str(&pvdata[j].final_string, ((char **)data[1])[j - rows + r]);
                pvdata[j].isNumber = 0;
                break;
              }
            } else {
              if (tokenIsNumber(((char **)data[1])[j - rows + r])) {
                errno = 0;
                pvdata[j].final_value = strtod(((char **)data[1])[j - rows + r], &s);
                if (errno == ERANGE) {
                  fprintf(stderr, "error: strtod reports range error");
                  exit(1);
                }
                pvdata[j].isNumber = 1;
              } else {
                cp_str(&pvdata[j].final_string, ((char **)data[1])[j - rows + r]);
                pvdata[j].isNumber = 0;
              }
            }
            break;
          case SDDS_LONG64:
            pvdata[j].final_value = ((int64_t *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_ULONG64:
            pvdata[j].final_value = ((uint64_t *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_LONG:
            pvdata[j].final_value = ((int32_t *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_ULONG:
            pvdata[j].final_value = ((uint32_t *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_SHORT:
            pvdata[j].final_value = ((short *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_USHORT:
            pvdata[j].final_value = ((unsigned short *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_FLOAT:
            pvdata[j].final_value = ((float *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          case SDDS_DOUBLE:
            pvdata[j].final_value = ((double *)data[1])[j - rows + r];
            pvdata[j].isNumber = 1;
            break;
          default:
            bomb("Unsupported type, final value must be string, long, short, float, or double.", NULL);
            break;
          }
        }
      }
    }
    if (retval == 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!SDDS_Terminate(&SDDS_dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    /* Close file after getting PV name and final value */

    ca_task_initialize();
    /* Get current value of PV */
    for (j = 0; j < rows; j++) {
      if (!pvdata[j].PVchid) {
        if (ca_search(pvdata[j].name, &(pvdata[j].PVchid)) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", pvdata[j].name);
          exit(1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (j = 0; j < rows; j++) {
        if (ca_state(pvdata[j].PVchid) != cs_conn)
          fprintf(stderr, "%s not connected\n", pvdata[j].name);
      }
      exit(1);
    }
    for (j = 0; j < rows; j++) {
      if (pvdata[j].isNumber) {
        if (ca_state(pvdata[j].PVchid) == cs_conn) {
          if (ca_get(DBR_DOUBLE, pvdata[j].PVchid, &pvdata[j].current_value) != ECA_NORMAL) {
            fprintf(stderr, "error: problem reading %s\n", pvdata[j].name);
            exit(1);
          }
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading some channels\n");
      exit(1);
    }
    /* Finished getting current value of PV */

    if (rampto[a].differentialMode) {
      /* alter final values of numerical data to reflect offsetting from the present value */
      for (j = 0; j < rows; j++) {
        if (pvdata[j].isNumber) {
          pvdata[j].final_value = pvdata[j].current_value + rampto[a].differentialFactor*pvdata[j].final_value;
        }
      }
    }

    if (rampto[a].percentage != 100) {
      /* alter final values of numerical data to reflect the desired percentage */
      for (j = 0; j < rows; j++) {
        if (pvdata[j].isNumber) {
          pvdata[j].final_value = pvdata[j].current_value +
                                  (rampto[a].percentage / 100.0) * (pvdata[j].final_value - pvdata[j].current_value);
        }
      }
    }

    /* Calculate PV increment size */
    for (j = 0; j < rows; j++) {
      if (pvdata[j].isNumber) {
        pvdata[j].increment = (pvdata[j].final_value - pvdata[j].current_value) / rampto[a].steps;
      }
    }
    /* Finished calculating PV increment size */

    /* Initiate ramping */
    timeToExecute = 0;
    for (i = 0; i < rampto[a].steps; i++) {
      if (i != 0) {
        if ((timeToWait = rampto[a].TimeInterval - timeToExecute) < 0)
          timeToWait = 0;
        if (timeToWait > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(timeToWait);
        } else {
          ca_poll();
        }
      }
      timeToExecute = getTimeInSecs();

      for (j = 0; j < rows; j++) {
        if (pvdata[j].isNumber) {
          pvdata[j].current_value += pvdata[j].increment;
          if (ca_state(pvdata[j].PVchid) == cs_conn) {
            if (ca_put(DBR_DOUBLE, pvdata[j].PVchid, &pvdata[j].current_value) != ECA_NORMAL) {
              fprintf(stderr, "error: problem setting %s\n", pvdata[j].name);
              exit(1);
            }
            if (verbose) {
              fprintf(stdout, "%s = %lf\n", pvdata[j].name, pvdata[j].current_value);
            }
          }
        }
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem setting for some channels\n");
        exit(1);
      }
      if (verbose) {
        fprintf(stdout, "\n");
      }
      timeToExecute = getTimeInSecs() - timeToExecute;
    }
    if (rampto[a].steps > 0) {
      if (rampto[a].steps != 1) {
        if ((timeToWait = rampto[a].TimeInterval - timeToExecute) < 0)
          timeToWait = 0;
        if (timeToWait > 0) {
          /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
          ca_pend_event(timeToWait);
        } else {
          ca_poll();
        }
      }
      timeToExecute = getTimeInSecs();
      /*set the string pvs */
      for (j = 0; j < rows; j++) {
        if (!pvdata[j].isNumber) {
          strncpy(char_string, pvdata[j].final_string, 40);
          if (ca_state(pvdata[j].PVchid) == cs_conn) {
            if (ca_put(DBR_STRING, pvdata[j].PVchid, char_string) != ECA_NORMAL) {
              fprintf(stderr, "error: problem setting %s\n", pvdata[j].name);
              exit(1);
            }
            if (verbose)
              fprintf(stdout, "%s = %s\n", pvdata[j].name, char_string);
          }
        }
      }
      if (resendFinal) {
        /*resend the final value for numerica pvs */
        for (j = 0; j < rows; j++) {
          if (pvdata[j].isNumber) {
            if (ca_state(pvdata[j].PVchid) == cs_conn) {
              if (ca_put(DBR_DOUBLE, pvdata[j].PVchid, &pvdata[j].final_value) != ECA_NORMAL) {
                fprintf(stderr, "error: problem setting %s\n", pvdata[j].name);
                exit(1);
              }
              if (verbose)
                fprintf(stdout, "resend final setpoint %s = %lf\n", pvdata[j].name, pvdata[j].final_value);
            }
          }
        }
        if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
          fprintf(stderr, "error: problem setting for some channels\n");
          exit(1);
        }
      }
      timeToExecute = getTimeInSecs() - timeToExecute;
    }
    if (a < numFilenames - 1) {
      if ((timeToWait = rampto[a].TimeInterval - timeToExecute) < 0)
        timeToWait = 0;
      if (timeToWait > 0) {
        /* Wait inside ca_pend_event so the CA library does not disconnect us if we wait too long */
        ca_pend_event(timeToWait);
      } else {
        ca_poll();
      }
      if (verbose) {
        fprintf(stdout, "\n");
      }
    }

    /* Finished ramping */
    for (j = 0; j < 3; j++)
      free(column_name[j]);
    free(column_name);
    column_name = NULL;
    free(type);
    type = NULL;
    free(data);
    data = NULL;
    free(columnFormat);
    columnFormat = NULL;
    if (pvdata) {
      for (j = 0; j < rows; j++) {
        if (pvdata[j].name)
          free(pvdata[j].name);
        if (pvdata[j].final_string)
          free(pvdata[j].final_string);
      }
      free(pvdata);
      pvdata = NULL;
    }
  }
  if (rampto) {
    for (j = 0; j < 20; j++) {
      if (rampto[j].filename)
        free(rampto[j].filename);
    }
    free(rampto);
  }
  free_scanargs(&s_arg, argc);
  ca_task_exit();
  return (0);
}
