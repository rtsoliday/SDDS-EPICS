/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddsvexperiment.c
 * purpose: conduct an experiment by varying any number of devices
 *          and reading back any number of devices.  This version
 *          is similar to sddsvmonitor, in that the devices to
 *          read are given in a file listing Rootnames and Attribute
 *          names.
 *
 * Michael Borland, 1995
 $Log: not supported by cvs2svn $
 Revision 1.7  2005/11/09 19:42:30  soliday
 Updated to remove compiler warnings on Linux

 Revision 1.6  2005/11/08 22:05:04  soliday
 Added support for 64 bit compilers.

 Revision 1.5  2004/09/10 14:37:57  soliday
 Changed the flag for oag_ca_pend_event to a volatile variable

 Revision 1.4  2004/07/23 16:27:42  soliday
 Improved signal support when using Epics/Base 3.14.6

 Revision 1.3  2004/07/19 17:39:38  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/16 21:24:40  soliday
 Replaced sleep commands with ca_pend_event commands because Epics/Base
 3.14.x has an inactivity timer that was causing disconnects from PVs
 when the log interval time was too large.

 Revision 1.1  2003/08/27 19:51:23  soliday
 Moved into subdirectory

 Revision 1.28  2002/11/01 21:03:59  soliday
 sddsvexperiment no longer uses ezca.

 Revision 1.27  2002/08/14 20:00:36  soliday
 Added Open License

 Revision 1.26  2001/05/03 19:53:47  soliday
 Standardized the Usage message.

 Revision 1.25  2000/10/16 21:48:04  soliday
 Removed tsDefs.h include statement.

 Revision 1.24  2000/04/20 15:59:27  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.23  2000/04/19 15:53:16  soliday
 Removed some solaris compiler warnings.

 Revision 1.22  2000/03/08 17:15:04  soliday
 Removed compiler warnings under Linux.

 Revision 1.21  1999/09/17 22:12:24  soliday
 This version now works with WIN32

 Revision 1.20  1997/09/26 21:51:19  borland
 Changed the way counter appending to system_calls is done.  Now uses
 default string of  " %ld" and *does not* pad the users command string.

 Revision 1.19  1996/06/13 22:08:46  borland
 Improved function of rootname substitution in system_call commands.

 * Revision 1.18  1996/06/13  21:39:13  borland
 * Fixed bug in substitution of output filename into system_call commands.
 * Also, now substitutes rootname rather than filename.
 *
 * Revision 1.17  1996/05/01  14:12:15  borland
 * Added exec_command namelist command (immediate command execution).
 *
 * Revision 1.16  1996/04/24  23:09:47  borland
 * Does binary SDDS output now.
 *
 * Revision 1.15  1996/03/08  23:39:33  borland
 * Added more signals to list of signals trapped for restoring initial
 * setpoints.
 *
 * Revision 1.14  1996/03/07  01:15:27  borland
 * Moved location of substep pause to outside the loop over the variables for
 * each counter.
 *
 * Revision 1.13  1996/02/10  06:29:45  borland
 * Added feature to variable command to support setting from a column of
 * values in an SDDS file.  Converted Time parameter/column from elapsed time to
 * time-since-epoch; added new ElapsedTime parameter/column.
 *
 * Revision 1.12  1996/02/07  18:49:52  borland
 * Brought up-to-date with new time routines and common SDDS time output format.
 *
 * Revision 1.11  1996/01/09  17:12:49  borland
 * Fixed bug in ramping when user function was given; added more output for
 * ramping when dry run mode is used.
 *
 * Revision 1.10  1996/01/04  00:22:39  borland
 * Added ganged ramping to experiment programs.
 *
 * Revision 1.9  1995/12/11  21:24:18  borland
 * Added TimeStamp parameter; removed second linefeed from percentage complete
 * printout.
 *
 * Revision 1.8  1995/12/05  21:24:31  borland
 * Upgraded ramping feature to remove bugs (code basically the same as
 * for sddsexperiment).  Fixed suffix column name feature; the SDDS column
 * SuffixColumnName in the suffix file gives the column names for the
 * output file.
 *
 * Revision 1.7  1995/12/03  01:30:51  borland
 * Fixed a typo in a printout message.
 *
 * Revision 1.6  1995/11/15  21:59:00  borland
 * Changed interrupt handlers to have proper arguments for non-Solaris machines.
 *
 * Revision 1.5  1995/11/15  01:32:31  borland
 * Fixed problems with use of new time routines.
 *
 * Revision 1.4  1995/11/15  00:04:23  borland
 * Modified to use time routines in SDDSepics.c .
 *
 * Revision 1.3  1995/11/14  04:34:06  borland
 * Added conditionals to support Solaris requirement for function arguments
 * to signal().
 *
 * Revision 1.2  1995/11/09  03:22:42  borland
 * Added copyright message to source files.
 *
 * Revision 1.1  1995/09/25  20:15:49  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#define TYPE_CHID 7
#include "sddsvexperiment.h"
#include "rpn.h"
#include <stdlib.h>
#include <time.h>
#include <signal.h>
/*#include <tsDefs.h>*/
#include <cadef.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#else
#  include <unistd.h>
#endif

void interrupt_handler(int);
void dummy_interrupt_handler(int);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

static long dev_timeout_errors = 0;
static long total_limit_errors = 0;

#define CLO_ECHO_INPUT 0
#define CLO_DRY_RUN 1
#define CLO_SUMMARIZE 2
#define CLO_VERBOSE 3
#define CLO_SUFFIX_FILE 4
#define CLO_ROOTNAME_FILE 5
#define CLO_EZCATIMING 6
#define CLO_DESCRIBEINPUT 7
#define CLO_PENDIOTIME 8
#define COMMANDLINE_OPTIONS 9
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "echoinput",
  "dryrun",
  "summarize",
  "verbose",
  "suffixfile",
  "rootnamefile",
  "ezcatiming",
  "describeinput",
  "pendiotimeout",
};

static char *USAGE = "sddsvexperiment <inputFile> <outputFile>\n\
  [-suffixFile=<filename>] [-rootnameFile=<filename>]\n\
  [-echoinput] [-dryrun] [-summarize] [-verbose[=very]]\n\
  [-pendIOtime=<seconds>]\n\
  [-describeInput]\n\n\
sddsvexperiment varies process variables and measures\n\
process variables, recording the results in a SDDS file or files.\n\
Program by Michael Borland.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  char *PV_name, *units;
  long rootnameIndex, columnIndex, number_to_average, include_standard_deviation, include_sigma;
  long StDevColumnIndex, SigmaColumnIndex;
  double sum, sum_sqr, average_value, standard_deviation, lower_limit, upper_limit;
  long number_of_values;
  chid PVchid;
} MEASUREMENT_DATA;

typedef struct
{
  double original_value, step_size, value;
  long parameterIndex;
  double *valueFromFile;
  long valuesFromFile;
  VARIABLESPEC VariableSpec;
} VARIABLE_DATA;

typedef struct
{
  FILE *fp;
  long call_number, parameterIndex;
  SYSTEMCALLSPEC SystemCallSpec;
} SYSTEMCALL_DATA;

typedef struct
{
  long index, limit;
  long Variables, SystemCalls;
  long substeps;
  double substep_pause;
  long counter_changed;
  VARIABLE_DATA **VariableData;
  SYSTEMCALL_DATA **SystemCallData;
} COUNTER;

#define FL_DRY_RUN 1
#define FL_VERBOSE 2
#define FL_VERYVERBOSE 4

VARIABLE_DATA *add_variable(VARIABLE_DATA *Variable, long Variables, VARIABLESPEC *NewVariable,
                            long verbose, double pendIOtime);
SYSTEMCALL_DATA *add_system_call(SYSTEMCALL_DATA *SystemCallData, long SystemCalls, SYSTEMCALLSPEC *NewSystemCall);
void summarize_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                          MEASUREMENT_DATA *Measurement, long Measurements,
                          SYSTEMCALL_DATA *SystemCall, long SystemCalls);
long make_system_call(char *command, long call_number, char *counter_format,
                      char *buffer, long buffer_length);
void make_calls_for_counters(COUNTER *counter, long counters, long rolled, long flags, long before_or_after);
void make_calls_for_counter(COUNTER *counter, long flags, long before_or_after);
void do_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                   SYSTEMCALL_DATA *SystemCall, long SystemCalls, char *inputfile,
                   char *outputfile, char *rootnameFile, char *suffixFile,
                   long flags, long summarize, double pendIOtime);
void set_variable_values(COUNTER *counter, long counters, long flags, long allowed_timeout_errors,
                         long counter_rolled_over, double pendIOtime);
void reset_initial_variable_values(COUNTER *counter, long counters, long flags, double pendIOtime);
void take_measurements(MEASUREMENT_DATA *Measurement, long Measurements,
                       double intermeasurement_pause, long flags, long allowed_timeout_errors, long allowed_limit_errors,
                       double outlimit_pause, double pendIOtime);
long advance_counters(COUNTER *counter, long counters, long *counter_rolled_over);
void output_control_quantities_list(char *filename, VARIABLE_DATA *Variable, long Variables,
                                    MEASUREMENT_DATA *Measurement, long Measurements);
long within_tolerance(double value1, double value2, double tolerance);
void setupOutputFile(SDDS_TABLE *outTable, char *outputFile, long *outputColumns, char ***rootname,
                     long *rootnames, MEASUREMENT_DATA **MeasurementData, long *measurements,
                     VARIABLE_DATA *VariableData, long Variables,
                     SYSTEMCALL_DATA *SystemCallData, long SystemCalls,
                     char *suffixFile, char *rootnameFile, double pendIOtime);
long readSuffixFile(char *suffixFile, char ***Suffix, char ***ColumnNameSuffix,
                    char **IncludeStDev, char **IncludeSigma, double **LowerLimit, double **UpperLimit,
                    int32_t **NumberToAverage);
long readRootnameFile(char *rootnameFile, char ***rootname);
void setupForInitialRamping(COUNTER *counter, long counters, unsigned flags, double pendIOtime);

#define AFTER_MEASURING 0x01
#define BEFORE_MEASURING 0x02
#define AFTER_SETTING 0x04
#define BEFORE_SETTING 0x08

static double globalRampPause = 0.25;
static long globalRampSteps = 10;
static unsigned long globalFlags = 0;

static char *outputRootname = NULL;
volatile int sigint = 0;

int main(int argc, char **argv) {
  char *inputFile, *outputFile, *rootnameFile, *suffixFile, *ptr;
  VARIABLE_DATA *VariableData;
  MEASUREMENT_DATA *Measurement;
  SYSTEMCALL_DATA *SystemCallData;
  long Measurements, Variables, SystemCalls;
  FILE *fpin;
  double pendIOtime = 10.0, timeout;
  long retries;
  long i_arg, echo_input, summarize, verbose, do_experiment_flags;
  SCANNED_ARG *s_arg;
  char s[SDDS_MAXLINE];

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
  atexit(rc_interrupt_handler);

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  inputFile = outputFile = rootnameFile = suffixFile = NULL;
  VariableData = NULL;
  Measurement = NULL;
  SystemCallData = NULL;
  Variables = Measurements = SystemCalls = 0;
  echo_input = verbose = do_experiment_flags = summarize = 0;

  rpn(getenv("RPN_DEFNS"));
  if (rpn_check_error())
    exit(1);

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_ECHO_INPUT:
        echo_input = 1;
        break;
      case CLO_SUMMARIZE:
        summarize = 1;
        break;
      case CLO_DRY_RUN:
        do_experiment_flags |= FL_DRY_RUN;
        break;
      case CLO_VERBOSE:
        if (s_arg[i_arg].n_items == 1)
          do_experiment_flags |= FL_VERBOSE;
        else if (strncmp(s_arg[i_arg].list[1], "very", strlen(s_arg[i_arg].list[1])) == 0)
          do_experiment_flags |= FL_VERYVERBOSE;
        else
          SDDS_Bomb("invalid -verbose syntax");
        break;
      case CLO_ROOTNAME_FILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -rootnameFile syntax");
        rootnameFile = s_arg[i_arg].list[1];
        break;
      case CLO_SUFFIX_FILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -suffixFile syntax");
        suffixFile = s_arg[i_arg].list[1];
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
      case CLO_DESCRIBEINPUT:
        show_namelists_fields(stdout, namelist_pointer, namelist_name, n_namelists);
        if (argc == 2)
          exit(0);
        break;
      default:
        SDDS_Bomb("unrecognized option given");
        break;
      }
    } else {
      if (!inputFile)
        inputFile = s_arg[i_arg].list[0];
      else if (!outputFile)
        outputFile = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }

  if (!inputFile)
    SDDS_Bomb("input filename not given");
  if (!outputFile)
    SDDS_Bomb("output filename not given");
  if (fexists(outputFile))
    SDDS_Bomb("output file already exists");

  fpin = fopen_e(inputFile, "r", FOPEN_EXIT_ON_ERROR);

  set_namelist_processing_flags(STICKY_NAMELIST_DEFAULTS);
  set_print_namelist_flags(PRINT_NAMELIST_COMPACT | PRINT_NAMELIST_NODEFAULTS);

  while (get_namelist(s, SDDS_MAXLINE, fpin)) {
    scan_namelist(&namelist_text, s);
    process_namelists(namelist_pointer, namelist_name, n_namelists, &namelist_text);
    switch (match_string(namelist_text.group_name, namelist_name, n_namelists, EXACT_MATCH)) {
    case EXECUTE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &execute);
        fflush(stdout);
      }
      if (!rootnameFile)
        SDDS_Bomb("you must give the rootname file on the command line or in the input file");
      if (!suffixFile)
        SDDS_Bomb("you must give the suffix file on the command line or in the input file");
      globalRampPause = ExecuteSpec.ramp_pause;
      globalRampSteps = ExecuteSpec.ramp_steps;
      globalFlags = do_experiment_flags;
      fflush(stdout);
      if (!(outputRootname = malloc(sizeof(*outputRootname) * (strlen(outputFile) + 1))))
        SDDS_Bomb("memory allocation failure");
      strcpy(outputRootname, outputFile);
      if ((ptr = strchr(outputRootname, '.')))
        *ptr = 0;
      do_experiment(&ExecuteSpec, VariableData, Variables, SystemCallData, SystemCalls,
                    inputFile, outputFile, rootnameFile, suffixFile, do_experiment_flags, summarize,
                    pendIOtime);
      exit(0);
      break;
    case VARIABLE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &variable);
        fflush(stdout);
      }
      if (!(VariableData = add_variable(VariableData, Variables, &VariableSpec, verbose, pendIOtime)))
        SDDS_Bomb("unable to add variable");
      Variables++;
      VariableSpec.values_file = NULL;
      VariableSpec.values_file_column = NULL;
      break;
    case ERASE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &erase);
        fflush(stdout);
      }
      if (EraseSpec.variable_definitions)
        Variables = 0;
      if (EraseSpec.measurement_definitions)
        Measurements = 0;
      break;
    case LIST_CONTROL_QUANTITIES_NAMELIST:
      if (!ListControlSpec.filename)
        SDDS_Bomb("a filename must be given for list_control_quantities");
      output_control_quantities_list(ListControlSpec.filename,
                                     VariableData, Variables,
                                     Measurement, Measurements);
      break;
    case SUFFIX_LIST_NAMELIST:
      if (!SuffixList.filename)
        SDDS_Bomb("a filename must be given for suffix_list");
      if (suffixFile)
        fprintf(stderr, "warning: suffix_list namelist ignored as -suffix option was given\n");
      else
        suffixFile = SuffixList.filename;
      break;
    case ROOTNAME_LIST_NAMELIST:
      if (!RootnameList.filename)
        SDDS_Bomb("a filename must be given for rootname_list");
      if (rootnameFile)
        fprintf(stderr, "warning: rootname_list namelist ignored as -rootname option was given\n");
      else
        rootnameFile = RootnameList.filename;
      break;
    case SYSTEM_CALL_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &system_call);
        fflush(stdout);
      }
      if (!(SystemCallData = add_system_call(SystemCallData, SystemCalls, &SystemCallSpec)))
        SDDS_Bomb("unable to add SystemCall");
      SystemCalls++;
      break;
    case EXEC_COMMAND_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &exec_command);
        fflush(stdout);
      }
      system(ExecCommandSpec.command);
      break;
    default:
      fprintf(stderr, "error: namelist %s is unknown\n", namelist_text.group_name);
      exit(1);
      break;
    }
  }
  if (dev_timeout_errors)
    printf("%ld device timeout errors occured\n", dev_timeout_errors);
  if (total_limit_errors)
    printf("%ld limit errors occured\n", total_limit_errors);
  return 0;
}

VARIABLE_DATA *add_variable(VARIABLE_DATA *VariableData, long Variables, VARIABLESPEC *NewVariable,
                            long verbose, double pendIOtime) {
  long i, highest_index_number;
  double readsetpoint, midpoint, spread;

  if (!NewVariable->PV_name)
    SDDS_Bomb("PV_name not given for variable");
  if (!NewVariable->parameter_name || SDDS_StringIsBlank(NewVariable->parameter_name))
    SDDS_CopyString(&NewVariable->parameter_name, NewVariable->PV_name);

  if (NewVariable->range_multiplier != 1) {
    midpoint = (NewVariable->initial_value + NewVariable->final_value) / 2;
    spread = NewVariable->range_multiplier * (NewVariable->final_value - NewVariable->initial_value) / 2;
    NewVariable->initial_value = midpoint - spread;
    NewVariable->final_value = midpoint + spread;
  }
  highest_index_number = 0;
  for (i = 0; i < Variables; i++)
    if (VariableData[i].VariableSpec.index_number > highest_index_number)
      highest_index_number = VariableData[i].VariableSpec.index_number;
  if (NewVariable->index_number < highest_index_number || NewVariable->index_number > (highest_index_number + 1)) {
    fprintf(stderr, "error: index_number %ld used out of order\n", NewVariable->index_number);
    exit(1);
  }

  VariableData = trealloc(VariableData, sizeof(*VariableData) * (Variables + 1));

  VariableData[Variables].valuesFromFile = 0;
  VariableData[Variables].valueFromFile = NULL;
  for (i = 0; i < Variables; i++) {
    if (VariableData[i].VariableSpec.index_number == NewVariable->index_number) {
      NewVariable->index_limit = VariableData[i].VariableSpec.index_limit;
      break;
    }
  }
  if (!NewVariable->values_file) {
    if (i == Variables && !NewVariable->index_limit) {
      fprintf(stderr, "error: index_limit not defined for variable %s\n", NewVariable->PV_name);
      exit(1);
    }
  } else {
    if (i == Variables)
      NewVariable->index_limit = 0; /* don't care what was entered, as no other linked index */
    if (!NewVariable->values_file_column) {
      fprintf(stderr, "error; values_file_column not defined for variable %s\n",
              NewVariable->PV_name);
      exit(1);
    }
    if (!(VariableData[Variables].valueFromFile =
            ReadValuesFileData(&VariableData[Variables].valuesFromFile,
                               NewVariable->values_file, NewVariable->values_file_column))) {
      fprintf(stderr, "Error: problem reading column %s from file %s for variable %s\n",
              NewVariable->values_file_column, NewVariable->values_file,
              NewVariable->PV_name);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (NewVariable->index_limit &&
        NewVariable->index_limit != VariableData[Variables].valuesFromFile) {
      fprintf(stderr, "Error: previously used index %ld has a limit of %ld, but\n",
              NewVariable->index_number, NewVariable->index_limit);
      fprintf(stderr, "       file %s (column %s) has %ld values.\n",
              NewVariable->values_file, NewVariable->values_file_column,
              VariableData[Variables].valuesFromFile);
    }
    NewVariable->index_limit = VariableData[Variables].valuesFromFile;
  }

  if (NewVariable->index_limit > 1)
    VariableData[Variables].step_size =
      (NewVariable->final_value - NewVariable->initial_value) / (NewVariable->index_limit - 1);
  else
    VariableData[Variables].step_size = 0;
  if (NewVariable->substeps < 1)
    NewVariable->substeps = 1;
  if (NewVariable->substep_pause < 0)
    NewVariable->substep_pause = 0;
  /* storing original value of variable */
  if (NewVariable->PVchid == NULL) {
    if (ca_search(NewVariable->PV_name, &(NewVariable->PVchid)) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", NewVariable->PV_name);
      exit(1);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      fprintf(stderr, "%s not connected\n", NewVariable->PV_name);
      exit(1);
    }
  }
  if (ca_state(NewVariable->PVchid) != cs_conn) {
    fprintf(stderr, "error getting setpoint of %s--aborting\n", NewVariable->PV_name);
    exit(1);
  }
  if (ca_get(DBR_DOUBLE, NewVariable->PVchid, &readsetpoint) != ECA_NORMAL) {
    fprintf(stderr, "error getting setpoint of %s--aborting\n", NewVariable->PV_name);
    exit(1);
  }

  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error getting setpoint of %s--aborting\n", NewVariable->PV_name);
    exit(1);
  }
  VariableData[Variables].original_value = readsetpoint;
  if (SDDS_StringIsBlank(NewVariable->units))
    getUnits(&NewVariable->PV_name, &NewVariable->units, 1, &NewVariable->PVchid, pendIOtime);

  printf("Storing original value of %s: %e \n",
         NewVariable->PV_name, VariableData[Variables].original_value);
  fflush(stdout);

  VariableData[Variables].VariableSpec = *NewVariable;
  return (VariableData);
}

SYSTEMCALL_DATA *add_system_call(SYSTEMCALL_DATA *SystemCallData, long SystemCalls, SYSTEMCALLSPEC *NewSystemCall) {
  SystemCallData = trealloc(SystemCallData, sizeof(*SystemCallData) * (SystemCalls + 1));
  SystemCallData[SystemCalls].call_number = 0;
  SystemCallData[SystemCalls].SystemCallSpec = *NewSystemCall;
  if (SystemCallData[SystemCalls].SystemCallSpec.call_before_setting)
    SystemCallData[SystemCalls].SystemCallSpec.call_before_measuring = 1;
  return (SystemCallData);
}

void summarize_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                          MEASUREMENT_DATA *Measurement, long Measurements,
                          SYSTEMCALL_DATA *SystemCall, long SystemCalls) {
  long i;
  printf("\n\nSummary of experiment:\n");
  printf("  settling time between changes and measurements:  post_change_pause = %f s\n", Execute->post_change_pause);
  printf("  pause interval between repeated measurements:    intermeasurement_pause = %f s\n", Execute->intermeasurement_pause);
  printf("\n%ld %s been defined:\n", Variables, Variables > 1 ? "variables have" : "variable has");
  printf("PV_name    index_number    index_limit    initial_value     final_value\n");
  for (i = 0; i < Variables; i++)
    printf("%-21s    %-12ld    %-11ld    %-13.6e    %-13.6e\n",
           Variable[i].VariableSpec.PV_name, Variable[i].VariableSpec.index_number,
           Variable[i].VariableSpec.index_limit, Variable[i].VariableSpec.initial_value,
           Variable[i].VariableSpec.final_value);

  printf("\n%ld %s been defined:\n", Measurements, Measurements > 1 ? "measurements have" : "measurement has");
  printf("PV_name      number_to_average      include_standard_deviation include_sigma\n");
  for (i = 0; i < Measurements; i++)
    printf("%-21s      %-17ld      %-26s %-15s\n",
           Measurement[i].PV_name,
           Measurement[i].number_to_average,
           Measurement[i].include_standard_deviation ? "yes" : "no",
           Measurement[i].include_sigma ? "yes" : "no");
  putchar('\n');

  printf("\n%ld %s been defined:\n", SystemCalls, SystemCalls > 1 ? "system salls have" : "system call has");
  for (i = 0; i < SystemCalls; i++)
    printf("%s\n",
           SystemCall[i].SystemCallSpec.command);

  fflush(stdout);
}

static COUNTER *counter_global = NULL;
static long counters_global;
static SDDS_TABLE SDDS_table;
static long table_written = 0, table_active = 0, Index_columnIndex, ElapsedTime_columnIndex, Rootname_columnIndex, EpochTime_columnIndex;

void do_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *VariableData, long Variables,
                   SYSTEMCALL_DATA *SystemCallData, long SystemCalls, char *inputFile,
                   char *outputFile, char *rootnameFile, char *suffixFile,
                   long flags, long summarize, double pendIOtime) {
  long i, j, counters, columns, rootnames;
  COUNTER *counter = NULL;
  char *date, **rootname;
  double StartHour, TimeOfDay, ElapsedTime, StartTime, EpochTime;
  long step, data_points, counter_rolled_over, Measurements;
  MEASUREMENT_DATA *Measurement = NULL;

  if (!Variables && !SystemCalls)
    SDDS_Bomb("no variables or system calls defined");
  counters = 0;
  for (i = 0; i < Variables; i++) {
    if (VariableData[i].VariableSpec.index_number >= counters)
      counters = VariableData[i].VariableSpec.index_number + 1;
  }
  for (i = 0; i < SystemCalls; i++) {
    if (SystemCallData[i].SystemCallSpec.index_number >= counters)
      counters = SystemCallData[i].SystemCallSpec.index_number + 1;
  }
  if (!(counter = (COUNTER *)calloc(counters, sizeof(*counter))))
    SDDS_Bomb("memory allocation failure (do_experiment)");
  data_points = 1;
  for (i = 0; i < counters; i++) {
    counter[i].index = counter[i].limit = counter[i].Variables = counter[i].SystemCalls =
      counter[i].substeps = counter[i].substep_pause = 0;
    counter[i].VariableData = NULL;
    counter[i].SystemCallData = NULL;
  }
  for (i = 0; i < Variables; i++) {
    if ((j = VariableData[i].VariableSpec.index_number) < 0 || j >= counters)
      SDDS_Bomb("programming problem--variable index number out of range (do_experiment)");
    if (counter[j].limit && counter[j].limit != VariableData[i].VariableSpec.index_limit)
      SDDS_Bomb("programming problem--variable index_limit has unexpected value (do_experiment)");
    counter[j].limit = VariableData[i].VariableSpec.index_limit;
    counter[j].VariableData = trealloc(counter[j].VariableData,
                                       (counter[j].Variables + 1) * sizeof(*counter[j].VariableData));
    counter[j].VariableData[counter[j].Variables] = VariableData + i;
    counter[j].Variables += 1;
  }

  for (i = 0; i < SystemCalls; i++) {
    if ((j = SystemCallData[i].SystemCallSpec.index_number) < 0 || j >= counters)
      SDDS_Bomb("system call index number out of range (do_experiment)");
    counter[j].SystemCallData = trealloc(counter[j].SystemCallData,
                                         (counter[j].SystemCalls + 1) * sizeof(*counter[j].SystemCallData));
    counter[j].SystemCallData[counter[j].SystemCalls] = SystemCallData + i;
    counter[j].SystemCalls += 1;
    if (counter[j].limit <= 0 &&
        (counter[j].limit = SystemCallData[i].SystemCallSpec.index_limit) <= 0)
      SDDS_Bomb("system call counter limit invalid");
  }

  setupOutputFile(&SDDS_table, outputFile, &columns, &rootname, &rootnames, &Measurement, &Measurements,
                  VariableData, Variables, SystemCallData, SystemCalls, suffixFile, rootnameFile, pendIOtime);

  for (i = 0; i < counters; i++) {
    if (counter[i].limit == 0) {
      fprintf(stderr, "error: counter %ld has no limit specified\n", i);
      exit(1);
    }
    data_points *= counter[i].limit;
    if (counter[i].Variables) {
      counter[i].substeps = counter[i].VariableData[0]->VariableSpec.substeps;
      counter[i].substep_pause = counter[i].VariableData[0]->VariableSpec.substep_pause;
    }
  }

  setupForInitialRamping(counter, counters, flags, pendIOtime);

  if (flags & FL_VERYVERBOSE) {
    for (i = 0; i < counters; i++) {
      printf("index %ld has a limit of %ld, %ld variables, and %ld system calls\n",
             i, counter[i].limit, counter[i].Variables,
             counter[i].SystemCalls);
      if (counter[i].substeps != 1)
        printf("  %ld substeps will be taken at intervals of %f seconds\n",
               counter[i].substeps, counter[i].substep_pause);
      for (j = 0; j < counter[i].Variables; j++) {
        if (!counter[i].VariableData[j]->valueFromFile)
          printf("    %s, from %e to %e\n",
                 counter[i].VariableData[j]->VariableSpec.PV_name,
                 counter[i].VariableData[j]->VariableSpec.initial_value,
                 counter[i].VariableData[j]->VariableSpec.final_value);
        else
          printf("    %s, %ld values of %s from file %s\n",
                 counter[i].VariableData[j]->VariableSpec.PV_name,
                 counter[i].VariableData[j]->valuesFromFile,
                 counter[i].VariableData[j]->VariableSpec.values_file_column,
                 counter[i].VariableData[j]->VariableSpec.values_file);
      }
      for (j = 0; j < counter[i].SystemCalls; j++)
        printf("    \"%s\" (%s measurements, %s variable setting)\n",
               counter[i].SystemCallData[j]->SystemCallSpec.command,
               counter[i].SystemCallData[j]->SystemCallSpec.call_before_measuring ? "before" : "after",
               counter[i].SystemCallData[j]->SystemCallSpec.call_before_setting ? "before" : "after");
    }
    printf("A total of %ld data points will be taken\n", data_points);
    fflush(stdout);
  }

  if (summarize)
    summarize_experiment(Execute, VariableData, Variables, Measurement, Measurements, SystemCallData,
                         SystemCalls);
  step = 0;
  counter_rolled_over = 1;
  counter_global = counter;
  counters_global = counters;

  getTimeBreakdown(&StartTime, NULL, &StartHour, NULL, NULL, NULL, &date);

  if (!(flags & FL_DRY_RUN)) {
    if (flags & FL_VERYVERBOSE) {
      printf("ramping PVs to initial values\n");
      fflush(stdout);
    }
    doRampPVs(globalRampSteps, globalRampPause, flags & (FL_VERYVERBOSE | FL_VERBOSE), pendIOtime);
  } else if (flags & FL_VERYVERBOSE) {
    printf("pretending to ramp PVs to initial values:\n");
    showRampPVs(stdout, pendIOtime);
    fflush(stdout);
  }

  do {
    if (sigint)
      exit(1);

    if (!SDDS_StartTable(&SDDS_table, rootnames)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    make_calls_for_counters(counter, counters, counter_rolled_over, flags, BEFORE_SETTING | BEFORE_MEASURING);
    set_variable_values(counter, counters, flags, Execute->allowed_timeout_errors, counter_rolled_over, pendIOtime);
    make_calls_for_counters(counter, counters, counter_rolled_over, flags, AFTER_SETTING | BEFORE_MEASURING);
    if (counter_rolled_over && (Execute->rollover_pause > 0)) {
      oag_ca_pend_event(Execute->rollover_pause, &sigint);
    }
    if (Execute->post_change_pause > 0) {
      oag_ca_pend_event(Execute->post_change_pause, &sigint);
    }
    if (Execute->post_change_key_wait) {
      printf("Press <return> to continue\7\7\n");
      fflush(stdout);
      getchar();
    }
    take_measurements(Measurement, Measurements, Execute->intermeasurement_pause,
                      flags, Execute->allowed_timeout_errors, Execute->allowed_limit_errors,
                      Execute->outlimit_pause, pendIOtime);
    /*
        ElapsedTime = getTimeInSecs() - StartTime;
      */
    getTimeBreakdown(&EpochTime, NULL, NULL, NULL, NULL, NULL, &date);
    ElapsedTime = EpochTime - StartTime;
    TimeOfDay = StartHour + ElapsedTime / 3600.0;
    if (!SDDS_SetParameters(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "Step", step, "ElapsedTime", ElapsedTime, "Time", EpochTime,
                            "TimeOfDay", TimeOfDay, "TimeStamp", date, NULL)) {
      SDDS_SetError("Unable to set time and step parameter values in SDDS data set");
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    /* loop over variables */
    for (i = 0; i < Variables; i++) {
      if (!SDDS_SetParameters(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              VariableData[i].parameterIndex,
                              VariableData[i].value, -1)) {
        SDDS_SetError("Unable to set variable value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    for (i = 0; i < rootnames; i++) {
      if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i,
                             Index_columnIndex, i,
                             ElapsedTime_columnIndex, ElapsedTime,
                             EpochTime_columnIndex, EpochTime,
                             Rootname_columnIndex, rootname[i], -1)) {
        SDDS_SetError("Unable to set columns values in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    /* loop over measurements */
    for (i = 0; i < Measurements; i++) {
      if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                             Measurement[i].rootnameIndex,
                             Measurement[i].columnIndex,
                             Measurement[i].average_value, -1)) {
        SDDS_SetError("Unable to set measurement value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (Measurement[i].include_standard_deviation &&
          !SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                             Measurement[i].rootnameIndex,
                             Measurement[i].StDevColumnIndex,
                             Measurement[i].standard_deviation, -1)) {
        SDDS_SetError("Unable to set standard deviation value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (Measurement[i].include_sigma &&
          !SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                             Measurement[i].rootnameIndex,
                             Measurement[i].SigmaColumnIndex,
                             Measurement[i].standard_deviation / sqrt((double)Measurement[i].number_of_values),
                             -1)) {
        SDDS_SetError("Unable to set standard deviation value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    make_calls_for_counters(counter, counters, counter_rolled_over, flags, AFTER_SETTING | AFTER_MEASURING);
    /* loop over system calls*/
    for (i = 0; i < SystemCalls; i++) {
      if (SystemCallData[i].SystemCallSpec.counter_parameter_name &&
          !SDDS_SetParameters(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE,
                              SystemCallData[i].parameterIndex,
                              SystemCallData[i].call_number - 1, -1)) {
        SDDS_SetError("Unable to set system call counter value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
    }
    if (SDDS_NumberOfErrors() || !SDDS_WriteTable(&SDDS_table))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    step++;
    if (flags & FL_VERBOSE) {
      printf("* Experiment is %.2f%% done.\n", (100.0 * step) / data_points);
      fflush(stdout);
    }
  } while (advance_counters(counter, counters, &counter_rolled_over) >= 0);
  if (!SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  table_written = 1;
  table_active = 0;
  reset_initial_variable_values(counter, counters, flags, pendIOtime);
  free(counter);
  counter = counter_global = NULL;
  if (Measurement)
    free(Measurement);
}

void set_variable_values(COUNTER *counter, long counters, long flags, long allowed_timeout_errors, long counter_rolled_over, double pendIOtime) {
  long i, j, k, step;
  double readback;

  for (i = 0; i < counters; i++) {
    if (!counter[i].Variables)
      continue;
    for (step = 0; step < counter[i].VariableData[0]->VariableSpec.substeps; step++) {
      for (j = 0; j < counter[i].Variables; j++) {
        if (counter[i].VariableData[j]->valueFromFile) {
          if (counter[i].index >= counter[i].VariableData[j]->valuesFromFile) {
            fprintf(stderr, "Error: counter %ld index (%ld) exceeds size of values array (%ld) for variable %s (file %s, column %s)\n",
                    i, counter[i].index, counter[i].VariableData[j]->valuesFromFile,
                    counter[i].VariableData[j]->VariableSpec.PV_name,
                    counter[i].VariableData[j]->VariableSpec.values_file,
                    counter[i].VariableData[j]->VariableSpec.values_file_column);
            exit(1);
          }
          counter[i].VariableData[j]->value =
            counter[i].VariableData[j]->valueFromFile[counter[i].index];
        } else {
          counter[i].VariableData[j]->value =
            counter[i].VariableData[j]->VariableSpec.initial_value +
            (counter[i].index - 1) * counter[i].VariableData[j]->step_size +
            (step + 1) * (counter[i].VariableData[j]->step_size / counter[i].substeps);
        }
        if (counter[i].VariableData[j]->VariableSpec.function) {
          push_num(counter[i].VariableData[j]->original_value);
          push_num(counter[i].VariableData[j]->value);
          counter[i].VariableData[j]->value = rpn(counter[i].VariableData[j]->VariableSpec.function);
          rpn_clear();
        } else if (counter[i].VariableData[j]->VariableSpec.relative_to_original)
          counter[i].VariableData[j]->value +=
            counter[i].VariableData[j]->original_value;
        if (flags & FL_VERYVERBOSE) {
          printf("%s %s to %.10e\n",
                 flags & FL_DRY_RUN ? "Pretending to set " : "Setting ",
                 counter[i].VariableData[j]->VariableSpec.PV_name,
                 counter[i].VariableData[j]->value);
          fflush(stdout);
        }
        if (!(flags & FL_DRY_RUN)) {
          if (ca_state(counter[i].VariableData[j]->VariableSpec.PVchid) != cs_conn) {
            fprintf(stderr, "Error trying to put value for %s\n",
                    counter[i].VariableData[j]->VariableSpec.PV_name);
            exit(1);
          }
          if (ca_put(DBR_DOUBLE, counter[i].VariableData[j]->VariableSpec.PVchid,
                     &counter[i].VariableData[j]->value) != ECA_NORMAL) {
            fprintf(stderr, "Error trying to put value for %s\n",
                    counter[i].VariableData[j]->VariableSpec.PV_name);
            exit(1);
          }
          if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
            fprintf(stderr, "Error trying to put value for %s\n",
                    counter[i].VariableData[j]->VariableSpec.PV_name);
            exit(1);
          }
          if (counter[i].VariableData[j]->VariableSpec.readback_attempts > 0 &&
              counter[i].VariableData[j]->VariableSpec.readback_tolerance > 0) {
            for (k = 0; k < counter[i].VariableData[j]->VariableSpec.readback_attempts; k++) {
              if (ca_get(DBR_DOUBLE, counter[i].VariableData[j]->VariableSpec.PVchid, &readback) != ECA_NORMAL)
                continue;
              if (ca_pend_io(pendIOtime) != ECA_NORMAL)
                continue;
              if (within_tolerance(readback, counter[i].VariableData[j]->value,
                                   counter[i].VariableData[j]->VariableSpec.readback_tolerance))
                break;
              oag_ca_pend_event(counter[i].VariableData[j]->VariableSpec.readback_pause, &sigint);
            }
            if (k == counter[i].VariableData[j]->VariableSpec.readback_attempts) {
              fprintf(stderr,
                      "Variable %s did not go to setpoint within specified tolerance in allotted time\n",
                      counter[i].VariableData[j]->VariableSpec.PV_name);
              fprintf(stderr, "Setpoint was %.10e, readback was %.10e\n",
                      counter[i].VariableData[j]->value, readback);
              /*
                            reset_initial_variable_values(counter_global, counters_global, flags, pendIOtime);
                          */
              exit(1);
            }
          }
        }
        if (dev_timeout_errors >= allowed_timeout_errors)
          SDDS_Bomb("too many timeout errors");
      }
      oag_ca_pend_event(counter[i].substep_pause, &sigint);
    }
  }
}

void reset_initial_variable_values(COUNTER *counter, long counters, long flags, double pendIOtime) {
  long i, j;

  if (flags & FL_VERYVERBOSE) {
    if (flags & FL_DRY_RUN)
      puts("Pretending to reset variables to original values.");
    else
      puts("Reseting variables to original values.");
  }
  addRampPV(NULL, 0.0, RAMPPV_CLEAR, NULL, pendIOtime);
  for (i = 0; i < counters; i++) {
    for (j = 0; j < counter[i].Variables; j++) {
      if (!counter[i].VariableData[j]->VariableSpec.reset_to_original)
        continue;
      addRampPV(counter[i].VariableData[j]->VariableSpec.PV_name,
                counter[i].VariableData[j]->original_value, 0,
                counter[i].VariableData[j]->VariableSpec.PVchid, pendIOtime);
    }
  }
  if (!(flags & FL_DRY_RUN)) {
    doRampPVs(globalRampSteps, globalRampPause, 1, pendIOtime);
  } else {
    if (flags & FL_VERYVERBOSE) {
      showRampPVs(stdout, pendIOtime);
      fflush(stdout);
    }
  }
}

void take_measurements(MEASUREMENT_DATA *Measurement, long Measurements, double intermeasurement_pause, long flags, long allowed_timeout_errors,
                       long allowed_limit_errors, double outlimit_pause, double pendIOtime) {
  long i, limit_errors;
  static double *readback = NULL;
  double variance;

  for (i = 0; i < Measurements; i++)
    Measurement[i].sum = Measurement[i].sum_sqr = Measurement[i].number_of_values = 0;
  limit_errors = 0;
  readback = SDDS_Realloc(readback, sizeof(*readback) * Measurements);
  do {
    for (i = 0; i < Measurements; i++)
      if (Measurement[i].number_of_values < Measurement[i].number_to_average)
        break;
    if (i == Measurements)
      break;
    for (i = 0; i < Measurements; i++) {
      readback[i] = 0;
      if (Measurement[i].PVchid == NULL) {
        if (ca_search(Measurement[i].PV_name,
                      &(Measurement[i].PVchid)) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for %s\n", Measurement[i].PV_name);
          exit(1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      for (i = 0; i < Measurements; i++) {
        if (ca_state(Measurement[i].PVchid) != cs_conn)
          fprintf(stderr, "%s not connected\n", Measurement[i].PV_name);
      }
      exit(1);
    }
    for (i = 0; i < Measurements; i++) {
      if (ca_state(Measurement[i].PVchid) != cs_conn) {
        fprintf(stderr, "error getting setpoint of %s--aborting\n", Measurement[i].PV_name);
        exit(1);
      }
      if (ca_get(DBR_DOUBLE, Measurement[i].PVchid, readback + i) != ECA_NORMAL) {
        fprintf(stderr, "error getting setpoint of %s--aborting\n", Measurement[i].PV_name);
        exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      exit(1);
    }
    for (i = 0; i < Measurements; i++) {
      if (Measurement[i].number_of_values >= Measurement[i].number_to_average)
        continue;
      if (Measurement[i].lower_limit != Measurement[i].upper_limit &&
          (Measurement[i].lower_limit > readback[i] ||
           Measurement[i].upper_limit < readback[i])) {
        /*                i--; */
        limit_errors++;
        if (limit_errors >= allowed_limit_errors) {
          fprintf(stderr, "error: too many limit errors\n");
          abort();
        }
        oag_ca_pend_event(outlimit_pause, &sigint);
        break;
      }
      /*
            if (dev_timeout_errors>=allowed_timeout_errors) {
            fprintf(stderr, "error: too many timeout errors\n"); 
            abort();
            }
          */
    }
    if (i != Measurements)
      continue;
    for (i = 0; i < Measurements; i++) {
      Measurement[i].sum += readback[i];
      Measurement[i].sum_sqr += readback[i] * readback[i];
      Measurement[i].number_of_values += 1;
    }
    oag_ca_pend_event(intermeasurement_pause, &sigint);
  } while (1);
  total_limit_errors += limit_errors;
  for (i = 0; i < Measurements; i++) {
    if (Measurement[i].number_of_values < 1)
      SDDS_Bomb("the impossible happened--no values read for a device");
    Measurement[i].average_value = Measurement[i].sum / Measurement[i].number_of_values;
    Measurement[i].standard_deviation = 0;
    if (Measurement[i].number_of_values > 1) {
      variance = (Measurement[i].sum_sqr - pow(Measurement[i].sum, 2.0) / Measurement[i].number_of_values) /
                 (Measurement[i].number_of_values - 1);
      if (variance <= 0)
        Measurement[i].standard_deviation = 0;
      else
        Measurement[i].standard_deviation = sqrt(variance);
    }
    if (flags & FL_VERYVERBOSE) {
      printf("  measurement of %s:  %.10e +/- %.10e\n",
             Measurement[i].PV_name,
             Measurement[i].average_value,
             Measurement[i].standard_deviation / sqrt(1.0 * Measurement[i].number_of_values));
      fflush(stdout);
    }
  }
}

long advance_counters(COUNTER *counter, long counters, long *counter_rolled_over) {
  long i;

  *counter_rolled_over = 0;
  for (i = 0; i < counters; i++)
    counter[i].counter_changed = 0;

  for (i = 0; i < counters; i++)
    if (counter[i].index != (counter[i].limit - 1))
      break;
  if (i == counters)
    return (-1);

  for (i = 0; i < counters; i++) {
    if (counter[i].index < (counter[i].limit - 1)) {
      counter[i].index++;
      counter[i].counter_changed = 1;
      break;
    } else {
      counter[i].index = 0;
      counter[i].counter_changed = 1;
      *counter_rolled_over = 1;
    }
  }
  return (i);
}

void output_control_quantities_list(char *filename, VARIABLE_DATA *Variable, long Variables,
                                    MEASUREMENT_DATA *Measurement, long Measurements) {
  long i;
  SDDS_TABLE SDDS_table;
  long iControlName = 0, iControlUnits = 0;

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, 1,
                             "Control quantities list from sddsvexperiment",
                             "Control quantities list from sddsvexperiment",
                             filename) ||
      (iControlName =
         SDDS_DefineColumn(&SDDS_table, "ControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0 ||
      (iControlUnits =
         SDDS_DefineColumn(&SDDS_table, "ControlUnits", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&SDDS_table) || !SDDS_StartTable(&SDDS_table, Measurements + Variables))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < Variables; i++) {
    if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i,
                           iControlName, Variable[i].VariableSpec.PV_name,
                           iControlUnits, Variable[i].VariableSpec.units, -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  for (i = 0; i < Measurements; i++) {
    if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, i + Variables,
                           iControlName, Measurement[i].PV_name,
                           iControlUnits, Measurement[i].units, -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (!SDDS_WriteTable(&SDDS_table) || !SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void interrupt_handler(int sig) {
  exit(1);
}

void dummy_interrupt_handler(int sig) {
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
  signal(SIGINT, dummy_interrupt_handler);
  signal(SIGTERM, dummy_interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, dummy_interrupt_handler);
#endif
#ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#endif
  if (table_active && !table_written) {
    if (!SDDS_WriteTable(&SDDS_table) || !SDDS_Terminate(&SDDS_table))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (ca) {
    if (counter_global)
      reset_initial_variable_values(counter_global, counters_global, FL_VERYVERBOSE | globalFlags, 10.0);
    ca_task_exit();
  }
}

long within_tolerance(double value1, double value2, double tolerance) {
  if (fabs(value1 - value2) <= tolerance)
    return (1);
  return (0);
}

void make_calls_for_counters(COUNTER *counter, long counters, long rolled, long flags, long before_or_after) {
  long i;
  for (i = 0; i < counters; i++)
    if (counter[i].counter_changed || rolled)
      make_calls_for_counter(counter + i, flags, before_or_after);
}

void make_calls_for_counter(COUNTER *counter, long flags, long before_or_after) {
  long j;
  static char buffer[1024];

  for (j = 0; j < counter->SystemCalls; j++) {
#if defined(DEBUG)
    printf("system call %ld for next counter:  %s (%s)\n",
           j,
           counter->SystemCallData[j]->SystemCallSpec.command,
           counter->SystemCallData[j]->SystemCallSpec.call_first ? "before" : "after");
    printf("  presently in >%s< stage\n",
           before_or_after == AFTER_SETTING ? "after" : "before");
#endif
    if (((before_or_after & AFTER_SETTING) && counter->SystemCallData[j]->SystemCallSpec.call_before_setting) ||
        ((before_or_after & BEFORE_SETTING) && !counter->SystemCallData[j]->SystemCallSpec.call_before_setting) ||
        ((before_or_after & AFTER_MEASURING) && counter->SystemCallData[j]->SystemCallSpec.call_before_measuring) ||
        ((before_or_after & BEFORE_MEASURING) && !counter->SystemCallData[j]->SystemCallSpec.call_before_measuring))
      continue;
    oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.pre_command_pause, &sigint);
    if (flags & FL_VERYVERBOSE) {
      printf("%s command %s\n",
             flags & FL_DRY_RUN ? "Pretending to execute" : "Executing",
             counter->SystemCallData[j]->SystemCallSpec.command);
      fflush(stdout);
    }
    if (!(flags & FL_DRY_RUN)) {
      if (!make_system_call(counter->SystemCallData[j]->SystemCallSpec.command,
                            counter->SystemCallData[j]->SystemCallSpec.append_counter ? counter->SystemCallData[j]->call_number : -1,
                            counter->SystemCallData[j]->SystemCallSpec.counter_format,
                            buffer, 1024))
        SDDS_Bomb("system call failed");
    }
    counter->SystemCallData[j]->call_number++;
    oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.post_command_pause, &sigint);
  }
}

long make_system_call(char *command, long call_number, char *counter_format,
                      char *buffer, long buffer_length) {
  char format[100];
  static char *buffer1 = NULL;

  buffer1 = SDDS_Realloc(buffer1, sizeof(*buffer1) * buffer_length);

  if (call_number >= 0) {
    strcpy(format, "%s");
    strcat(format, counter_format);
    sprintf(buffer1, format, command, call_number);
  } else
    strcpy(buffer1, command);
  replace_string(buffer, buffer1, "%s", outputRootname);

  if (!SDDS_StringIsBlank(buffer))
    system(buffer);
  return (1);
}

void setupOutputFile(SDDS_TABLE *outTable, char *outputFile, long *outputColumns,
                     char ***rootname, long *rootnames,
                     MEASUREMENT_DATA **MeasurementData, long *Measurements,
                     VARIABLE_DATA *VariableData, long Variables,
                     SYSTEMCALL_DATA *SystemCallData, long SystemCalls,
                     char *suffixFile, char *rootnameFile, double pendIOtime)
/* The measurements file has the following columns:
   string: Suffix, ColumnNameSuffix [optional],
   long: NumberToAverage [optional], 
   character: IncludeStDev, IncludeSigma [both optional]
   double: LowerLimit, UpperLimit [both optional]
*/
{
  long i, suffixes, imeas, iroot, isuffix, columnIndex, StDevColumnIndex = 0, SigmaColumnIndex = 0;
  char **Suffix, **ColumnNameSuffix, *IncludeStDev, *IncludeSigma;
  int32_t *NumberToAverage;
  double *LowerLimit, *UpperLimit;
  /*    char s[SDDS_MAXLINE], unitsBuffer[SDDS_MAXLINE];*/
  char *s[1], *unitsBuffer[1];
  double StartTime, StartHour;
  char *date;
  chid cid;

  s[0] = malloc(sizeof(char) * SDDS_MAXLINE);
  if (!(*rootnames = readRootnameFile(rootnameFile, rootname)))
    SDDS_Bomb("unable to read rootnames");
  if (!(suffixes = readSuffixFile(suffixFile, &Suffix, &ColumnNameSuffix,
                                  &IncludeStDev, &IncludeSigma, &LowerLimit, &UpperLimit,
                                  &NumberToAverage)))
    SDDS_Bomb("unable to read suffixes");

  getTimeBreakdown(&StartTime, NULL, &StartHour, NULL, NULL, NULL, &date);

  if (!SDDS_InitializeOutput(outTable, SDDS_BINARY, 0, NULL, NULL, outputFile) ||
      SDDS_DefineParameter(outTable, "Step", NULL, NULL, "Step number", NULL, SDDS_LONG, NULL) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "TimeStamp", NULL, NULL, "date and time string for present step",
                           NULL, SDDS_STRING, NULL) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "ElapsedTime", NULL, "s", "Elapsed time", NULL, SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, NULL) < 0 ||
      SDDS_DefineParameter(&SDDS_table, "TimeOfDay", NULL, "h", NULL, NULL, SDDS_DOUBLE, NULL) < 0 ||
      (Index_columnIndex = SDDS_DefineColumn(outTable, "Index", NULL, NULL, NULL, NULL, SDDS_LONG, 0)) < 0 ||
      (ElapsedTime_columnIndex = SDDS_DefineColumn(outTable, "ElapsedTime", NULL, "s", NULL, NULL, SDDS_DOUBLE, 0)) < 0 ||
      (EpochTime_columnIndex = SDDS_DefineColumn(outTable, "Time", NULL, "s", NULL, NULL, SDDS_DOUBLE, 0)) < 0 ||
      (Rootname_columnIndex = SDDS_DefineColumn(outTable, "Rootname", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0)
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  for (i = 0; i < Variables; i++) {
    if (VariableData[i].VariableSpec.PV_name) {
      if ((VariableData[i].parameterIndex =
             SDDS_DefineParameter(outTable, VariableData[i].VariableSpec.parameter_name,
                                  VariableData[i].VariableSpec.symbol, VariableData[i].VariableSpec.units,
                                  NULL, NULL, SDDS_DOUBLE, NULL)) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  }

  for (i = 0; i < SystemCalls; i++) {
    if (SystemCallData[i].SystemCallSpec.counter_parameter_name) {
      if ((SystemCallData[i].parameterIndex =
             SDDS_DefineParameter(outTable, SystemCallData[i].SystemCallSpec.counter_parameter_name,
                                  NULL, NULL, "system call counter", NULL, SDDS_LONG, NULL)) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  }

  *Measurements = suffixes * (*rootnames);
  *MeasurementData = tmalloc(sizeof(**MeasurementData) * (*Measurements));
  for (isuffix = imeas = 0; isuffix < suffixes; isuffix++) {
    sprintf(s[0], "%s%s", (*rootname)[0], Suffix[isuffix]);
    cid = NULL;
    if (!getUnits((char **)s, (char **)unitsBuffer, 1, &cid, pendIOtime)) {
      fprintf(stderr, "error: unable to get units for %s\n", s[0]);
      exit(1);
    }
    if ((columnIndex = SDDS_DefineColumn(outTable, ColumnNameSuffix ? ColumnNameSuffix[isuffix] : Suffix[isuffix],
                                         NULL, unitsBuffer[0], NULL, NULL, SDDS_DOUBLE, 0)) < 0)
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (NumberToAverage && NumberToAverage[isuffix] <= 1 &&
        ((IncludeStDev && IncludeStDev[isuffix] == 'y') ||
         (IncludeSigma && IncludeSigma[isuffix] == 'y')))
      SDDS_Bomb("must average more than 1 point to compute sigma or standard deviation");
    if (NumberToAverage && NumberToAverage[isuffix] > 1) {
      if (IncludeStDev && IncludeStDev[isuffix] == 'y') {
        sprintf(s[0], "StDev%s", ColumnNameSuffix ? ColumnNameSuffix[isuffix] : Suffix[isuffix]);
        if ((StDevColumnIndex = SDDS_DefineColumn(outTable, s[0], NULL, unitsBuffer[0], NULL, NULL, SDDS_DOUBLE, 0)) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (IncludeSigma && IncludeSigma[isuffix] == 'y') {
        sprintf(s[0], "Sigma%s", ColumnNameSuffix ? ColumnNameSuffix[isuffix] : Suffix[isuffix]);
        if ((SigmaColumnIndex = SDDS_DefineColumn(outTable, s[0], NULL, unitsBuffer[0], NULL, NULL, SDDS_DOUBLE, 0)) < 0)
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
    for (iroot = 0; iroot < *rootnames; iroot++, imeas++) {
      (*MeasurementData)[imeas].rootnameIndex = iroot;
      (*MeasurementData)[imeas].columnIndex = columnIndex;
      (*MeasurementData)[imeas].StDevColumnIndex = -1;
      (*MeasurementData)[imeas].SigmaColumnIndex = -1;
      (*MeasurementData)[imeas].PVchid = NULL;
      sprintf(s[0], "%s%s", (*rootname)[iroot], Suffix[isuffix]);
      SDDS_CopyString(&(*MeasurementData)[imeas].PV_name, s[0]);
      SDDS_CopyString(&(*MeasurementData)[imeas].units, unitsBuffer[0]);
      if (NumberToAverage) {
        (*MeasurementData)[imeas].number_to_average = NumberToAverage[isuffix];
        if (IncludeStDev) {
          (*MeasurementData)[imeas].include_standard_deviation = IncludeStDev[isuffix] == 'y';
          (*MeasurementData)[imeas].StDevColumnIndex = StDevColumnIndex;
        } else
          (*MeasurementData)[imeas].include_standard_deviation = 0;
        if (IncludeSigma) {
          (*MeasurementData)[imeas].include_sigma = IncludeSigma[isuffix] == 'y';
          (*MeasurementData)[imeas].SigmaColumnIndex = SigmaColumnIndex;
        } else
          (*MeasurementData)[imeas].include_sigma = 0;
      } else {
        (*MeasurementData)[imeas].number_to_average = 1;
        (*MeasurementData)[imeas].include_standard_deviation = 0;
      }
      if (LowerLimit) {
        (*MeasurementData)[imeas].lower_limit = LowerLimit[isuffix];
        (*MeasurementData)[imeas].upper_limit = UpperLimit[isuffix];
      } else
        (*MeasurementData)[imeas].lower_limit = (*MeasurementData)[imeas].upper_limit = 0;
    }
  }
  if (NumberToAverage)
    free(NumberToAverage);
  if (IncludeStDev)
    free(IncludeStDev);
  if (IncludeSigma)
    free(IncludeSigma);
  if (LowerLimit)
    free(LowerLimit);
  if (UpperLimit)
    free(UpperLimit);

  if (!SDDS_WriteLayout(outTable))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  table_active = 1;
  table_written = 0;
  free(s[0]);
}

long readSuffixFile(char *suffixFile, char ***Suffix, char ***ColumnNameSuffix,
                    char **IncludeStDev, char **IncludeSigma, double **LowerLimit, double **UpperLimit,
                    int32_t **NumberToAverage) {
  SDDS_TABLE inTable;
  long code = 0, suffixes = 0;

  if (!SDDS_InitializeInput(&inTable, suffixFile) || (code = SDDS_ReadTable(&inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (code == 0 || (suffixes = SDDS_CountRowsOfInterest(&inTable)) < 1)
    SDDS_Bomb("no data in measurement suffix file");
  if (!((*Suffix) = SDDS_GetColumn(&inTable, "Suffix"))) {
    fprintf(stderr, "error: required column \"Suffix\" not found in file %s\n", suffixFile);
    exit(1);
  }

  *ColumnNameSuffix = NULL;
  *IncludeStDev = *IncludeSigma = NULL;
  *LowerLimit = *UpperLimit = NULL;
  *NumberToAverage = NULL;

  if ((code = SDDS_CheckColumn(&inTable, "SuffixColumnName", NULL, SDDS_STRING, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "SuffixColumnName", NULL, SDDS_STRING, stderr);
      exit(1);
    }
  } else if (!((*ColumnNameSuffix) = SDDS_GetColumn(&inTable, "SuffixColumnName")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((code = SDDS_CheckColumn(&inTable, "IncludeStDev", NULL, SDDS_CHARACTER, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "IncludeStDev", NULL, SDDS_CHARACTER, stderr);
      exit(1);
    }
  } else if (!((*IncludeStDev) = SDDS_GetColumn(&inTable, "IncludeStDev")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((code = SDDS_CheckColumn(&inTable, "IncludeSigma", NULL, SDDS_CHARACTER, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "IncludeSigma", NULL, SDDS_CHARACTER, stderr);
      exit(1);
    }
  } else if (!((*IncludeSigma) = SDDS_GetColumn(&inTable, "IncludeSigma")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((code = SDDS_CheckColumn(&inTable, "NumberToAverage", NULL, SDDS_LONG, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "NumberToAverage", NULL, SDDS_LONG, stderr);
      exit(1);
    }
  } else if (!((*NumberToAverage) = SDDS_GetColumn(&inTable, "NumberToAverage")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((code = SDDS_CheckColumn(&inTable, "LowerLimit", NULL, SDDS_DOUBLE, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "LowerLimit", NULL, SDDS_DOUBLE, stderr);
      exit(1);
    }
  } else if (!((*LowerLimit) = SDDS_GetColumn(&inTable, "LowerLimit")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if ((code = SDDS_CheckColumn(&inTable, "UpperLimit", NULL, SDDS_DOUBLE, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "UpperLimit", NULL, SDDS_DOUBLE, stderr);
      exit(1);
    }
  } else if (!((*UpperLimit) = SDDS_GetColumn(&inTable, "UpperLimit")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if ((LowerLimit && !UpperLimit) || (!LowerLimit && UpperLimit))
    SDDS_Bomb("file contains one of LowerLimit and UpperLimit columns--must have both or neither");

  if (SDDS_ReadTable(&inTable) > 1)
    fprintf(stderr, "warning: file %s has multiple pages--only the first is used!\n",
            suffixFile);
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return suffixes;
}

long readRootnameFile(char *rootnameFile, char ***rootname) {
  SDDS_TABLE inTable;
  long rootnames = 0, code = 0;

  if (!SDDS_InitializeInput(&inTable, rootnameFile) || (code = SDDS_ReadTable(&inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (code == 0 || (rootnames = SDDS_CountRowsOfInterest(&inTable)) < 1)
    SDDS_Bomb("no data in measurement rootname file");
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

void setupForInitialRamping(COUNTER *counter, long counters, unsigned flags, double pendIOtime) {
  long i, j;
  double value;

  addRampPV(NULL, 0.0, RAMPPV_CLEAR, NULL, pendIOtime);
  for (i = 0; i < counters; i++) {
    for (j = 0; j < counter[i].Variables; j++) {
      if (counter[i].VariableData[j]->valueFromFile)
        value = counter[i].VariableData[j]->valueFromFile[0];
      else
        value = counter[i].VariableData[j]->VariableSpec.initial_value;
      if (counter[i].VariableData[j]->VariableSpec.function) {
        push_num(counter[i].VariableData[j]->original_value);
        push_num(value);
        value = rpn(counter[i].VariableData[j]->VariableSpec.function);
        rpn_clear();
      } else if (counter[i].VariableData[j]->VariableSpec.relative_to_original)
        value += counter[i].VariableData[j]->original_value;
      addRampPV(counter[i].VariableData[j]->VariableSpec.PV_name,
                value, 0,
                counter[i].VariableData[j]->VariableSpec.PVchid, pendIOtime);
    }
  }
}
