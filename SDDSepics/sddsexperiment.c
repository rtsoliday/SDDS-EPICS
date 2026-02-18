/**
 * @file sddsexperiment.c
 * @brief Vary devices and measure responses using EPICS PVs.
 *
 * sddsexperiment reads an SDDS input file describing devices to vary and
 * devices to measure. Values are written to an SDDS output file while
 * optional macros and comments may be supplied on the command line.
 *
 * @section Usage
 * ```
 * sddsexperiment <inputfile> [<outputfile-rootname|outputfilename>]
 *     [-echoinput] [-dryrun] [-summarize] [-verbose]
 *     [-pendIOtime=<seconds>] [-describeInput]
 *     [-macro=<tag>=<value>,...] [-comment=<string>]
 *     [-scalars=<filename>] [-ezcaTiming=<timeout>,<retries>]
 * ```
 *
 * @section Options
 * | Option          | Description |
 * |-----------------|-------------|
 * | `-echoinput`    | Echo commands as they are executed. |
 * | `-dryrun`       | Parse input without executing commands. |
 * | `-summarize`    | Summarize commands before execution. |
 * | `-verbose`      | Provide additional status output. |
 * | `-pendIOtime`   | Channel Access timeout in seconds. |
 * | `-describeInput`| Describe acceptable input file contents and exit. |
 * | `-macro`        | Perform macro substitutions on the input file. |
 * | `-comment`      | Comment text to store in the output file. |
 * | `-scalars`      | File containing additional scalar PVs. |
 * | `-ezcaTiming`   | Obsolete option for EZCA timing. |
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
 * M. Borland, R. Soliday, D. Blachowicz
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
#define TYPE_CHID 7
#include "sddsexperiment.h"
#include <ctype.h>
#include "rpn.h"
#include <time.h>
#include <signal.h>
/*#include <tsDefs.h>*/
#include <cadef.h>

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs)
#else
#  include <unistd.h>
#endif

void interrupt_handler(int);
void dummy_interrupt_handler(int);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();

/* this is the file pointer to which informational printouts
 * will be sent 
 */
#define FPINFO stdout

static long dev_timeout_errors = 0;
static long total_limit_errors = 0;

#define CLO_ECHO_INPUT 0
#define CLO_DRY_RUN 1
#define CLO_SUMMARIZE 2
#define CLO_VERBOSE 3
#define CLO_EZCATIMING 4
#define CLO_DESCRIBEINPUT 5
#define CLO_MACRO 6
#define CLO_COMMENT 7
#define CLO_PENDIOTIME 8
#define CLO_SCALARS 9
#define COMMANDLINE_OPTIONS 10
char *commandline_option[COMMANDLINE_OPTIONS] = {
  "echoinput",
  "dryrun",
  "summarize",
  "verbose",
  "ezcatiming",
  "describeinput",
  "macro",
  "comment",
  "pendiotimeout",
  "scalars"};

static char *USAGE = "sddsexperiment <inputfile> [<outputfile-rootname|outputfilename>]\n\
[-echoinput] [-dryrun] [-summarize] [-verbose] [-pendIOtime=<seconds>]\n\
[-describeInput] [-macro=<tag>=<value>,[...]] [-comment=<string value>] [-scalars=<filename>] \n\n\
sddsexperiment varies process variables and measures process variables, recording the results in a SDDS file or files.\n\
Program by M.Borland, R.Soliday, D.Blachowicz.\n\
Link date: "__DATE__
                     " "__TIME__
                     ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  double sum, sum_sqr;
  double average_value, standard_deviation, minimum, maximum;
  long number_of_values;
  chid PVchid;
  MEASUREMENTSPEC MeasurementSpec;
} MEASUREMENT_DATA;

typedef struct {
  char *knobName, **ControlName, **ReadbackName, **units;
  chid *PVchid, *RBPVchid;
  long PVs;
  double *initial_value, *value, *weight, gain, *lower_limit, *upper_limit;
} KNOB;
  
typedef struct
{
  double original_value, step_size, value, set_value, last_value;
  double *valueFromFile;
  long valuesFromFile;
  VARIABLESPEC VariableSpec;
  chid PVchid;
  chid RBPVchid;
  /*for knob pvs */
  KNOB knob;
} VARIABLE_DATA;

typedef struct
{
  long call_number, cycle_call_number;
  SYSTEMCALLSPEC SystemCallSpec;
} SYSTEMCALL_DATA;

typedef struct
{
  char **column;
  char **value;
  char **type;
  long *SDDS_type;
  long columns;
  long types;
} OUTPUT_COLUMN;

typedef struct
{
  long index, limit, first_set;
  long Variables, SystemCalls;
  long substeps;
  double substep_pause;
  long counter_changed;
  VARIABLE_DATA **VariableData;
  SYSTEMCALL_DATA **SystemCallData;
  OUTPUT_COLUMN *Output_Column;
} COUNTER;

#define FL_DRY_RUN 1
#define FL_VERBOSE 2
#define FL_VERYVERBOSE 4

void read_knob_file(VARIABLE_DATA *variable, VARIABLESPEC *NewVariable, double pendIOtime);
VARIABLE_DATA *add_variable(VARIABLE_DATA *Variable, long Variables, VARIABLESPEC *NewVariable, double pendIOtime);
SYSTEMCALL_DATA *add_system_call(SYSTEMCALL_DATA *SystemCallData, long SystemCalls, SYSTEMCALLSPEC *NewSystemCall);
MEASUREMENT_DATA *add_measurement(MEASUREMENT_DATA *MeasurementData, long Measurements, MEASUREMENTSPEC *NewMeasurement, double pendIOtime, int getUnitsNow);
void summarize_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                          MEASUREMENT_DATA *Measurement, long Measurements,
                          SYSTEMCALL_DATA *SystemCall, long SystemCalls);
char *compose_filename(char *template, char *rootname);

long make_system_call(OUTPUT_COLUMN *output_column, char *command, long call_number, char *counter_format,
                      char *buffer, long buffer_length);
void make_calls_for_counters(COUNTER *counter, long counters, long rolled, long flags, long before_or_after);
void make_calls_for_counter(COUNTER *counter, long flags, long before_or_after);
void make_cycle_calls(COUNTER *counter, long counters, long flags, long before_or_after);
void do_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                   MEASUREMENT_DATA *Measurement, long Measurements,
                   SYSTEMCALL_DATA *SystemCall, long SystemCalls, const char *inputfile, char *comment,
                   long flags, double pendIOtime, char *scalarFile);
void check_knob_settings(KNOB *knob, double tolerance, long temps, double pendIOtime);
void set_variable_values(COUNTER *counter, long counters, long flags, long allowed_timeout_errors,
                         long counter_rolled_over, double pendIOtime);
void reset_initial_variable_values(COUNTER *counter, long counters, long flags, double pendIOtime);
void take_measurements(MEASUREMENT_DATA *Measurement, long Measurements,
                       double intermeasurement_pause, long flags, long allowed_timeout_errors, long allowed_limit_errors,
                       double outlimit_pause, double pendIOtime, SYSTEMCALL_DATA *SystemCallData, long SystemCalls);
long advance_counters(COUNTER *counter, long counters, long *counter_rolled_over);
void output_control_quantities_list(char *filename, VARIABLE_DATA *Variable, long Variables,
                                    MEASUREMENT_DATA *Measurement, long Measurements);
long within_tolerance(double value1, double value2, double tolerance);
void setupForInitialRamping(COUNTER *counter, long counters, unsigned flags, double pendIOtime);
MEASUREMENT_DATA *add_file_measurement(MEASUREMENT_DATA *MeasurementData, long *Measurements,
                                       MEASUREMENTFILESPEC *MeasuremntFileSpec, double pendIOtime);
long readMeasurementFile(char *MeasurementFile, char ***ControlName, char ***ColumnName,
                         char **IncludeStDev, char **IncludeSigma, double **LowerLimit, double **UpperLimit,
                         char **LimitsAll, long **NumberToAverage, char ***ReadbackUnits, chid **PVchid,
                         double pendIOtime, short *ReadbackUnitsColumnFound);

static double globalRampPause = 0.25;
static long globalRampSteps = 10;
static unsigned long globalFlags;

#define AFTER_MEASURING 0x0001UL
#define BEFORE_MEASURING 0x0002UL
#define AFTER_SETTING 0x0004UL
#define BEFORE_SETTING 0x0008UL
#define BEFORE_CYCLE 0x0010UL
#define AFTER_CYCLE 0x0020UL
#define AFTER_EXPERIMENT 0x0040UL
/*#define NAMELIST_BUFLEN 65536 */

static char *outputRootname;
volatile int sigint = 0;

int main(int argc, char **argv) {
  char *inputfile, *ptr, **macroTag, **macroValue, *comment, *scalarFile;
  long macros;
  VARIABLE_DATA *VariableData;
  MEASUREMENT_DATA *Measurement;
  SYSTEMCALL_DATA *SystemCallData;
  long Measurements, Variables, SystemCalls;
  FILE *fpin;
  long i_arg, echo_input, summarize, verbose, do_experiment_flags;
  SCANNED_ARG *s_arg;
  char s[SDDS_MAXLINE];
  double pendIOtime = 10.0, timeout;
  long retries;

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
#if defined(DEBUG)
  fprintf(FPINFO, "scanargs done.\n");
#endif

  inputfile = outputRootname = comment = scalarFile = NULL;
  VariableData = NULL;
  Measurement = NULL;
  SystemCallData = NULL;
  Variables = Measurements = SystemCalls = 0;
  echo_input = verbose = do_experiment_flags = summarize = 0;
  macros = 0;
  macroTag = macroValue = NULL;

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
      case CLO_SCALARS:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -scalars syntax");
        scalarFile = s_arg[i_arg].list[1];
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
      case CLO_COMMENT:
        if (s_arg[i_arg].n_items > 2)
          SDDS_Bomb("invalid -comment syntax");
        if (s_arg[i_arg].n_items == 2)
          comment = s_arg[i_arg].list[1];
        break;
      case CLO_MACRO:
        macros = 0;
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -macro syntax");
        if (!(macroTag = SDDS_Realloc(macroTag, sizeof(*macroTag) * (macros + s_arg[i_arg].n_items))) ||
            !(macroValue = SDDS_Realloc(macroValue, sizeof(*macroValue) * (macros + s_arg[i_arg].n_items))))
          SDDS_Bomb("memory allocation failure (-macro)");
        else {
          long j;
          for (j = 0; j < s_arg[i_arg].n_items - 1; j++) {
            macroTag[macros] = s_arg[i_arg].list[j + 1];
            if (!(macroValue[macros] = strchr(macroTag[macros], '=')))
              SDDS_Bomb("invalid -macro syntax");
            macroValue[macros][0] = 0;
            macroValue[macros] += 1;
            macros++;
          }
        }
        break;
      default:
        SDDS_Bomb("unrecognized option given");
        break;
      }
    } else {
      if (!inputfile)
        inputfile = s_arg[i_arg].list[0];
      else if (!outputRootname)
        outputRootname = s_arg[i_arg].list[0];
      else
        SDDS_Bomb("too many filenames given");
    }
  }

#if defined(DEBUG)
  fprintf(FPINFO, "arg scanning done.\n");
#endif
  if (!inputfile)
    SDDS_Bomb("input filename not given");
  if (!outputRootname) {
    cp_str(&outputRootname, inputfile);
    if ((ptr = strrchr(outputRootname, '.')))
      *ptr = 0;
  }

  fpin = fopen_e(inputfile, "r", FOPEN_EXIT_ON_ERROR);

  set_namelist_processing_flags(STICKY_NAMELIST_DEFAULTS);
  set_print_namelist_flags(PRINT_NAMELIST_COMPACT | PRINT_NAMELIST_NODEFAULTS);

#if defined(DEBUG)
  fprintf(FPINFO, "timeout set\n");
#endif

  while (get_namelist(s, SDDS_MAXLINE, fpin)) {
    if (macros)
      substituteTagValue(s, SDDS_MAXLINE, macroTag, macroValue, macros);
#if defined(DEBUG)
    fprintf(FPINFO, "namelist gotten\n");
#endif
    if (scan_namelist(&namelist_text, s) == -1)
      SDDS_Bomb("namelist scanning error--can't continue\n");
#if defined(DEBUG)
    fprintf(FPINFO, "namelist scanned\n");
#endif
    process_namelists(namelist_pointer, namelist_name, n_namelists, &namelist_text);
#if defined(DEBUG)
    fprintf(FPINFO, "namelist processed\n");
#endif
    switch (match_string(namelist_text.group_name, namelist_name, n_namelists, EXACT_MATCH)) {
    case EXECUTE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &execute);
        fflush(stdout);
      }
      ExecuteSpec.outputfile = compose_filename(ExecuteSpec.outputfile, outputRootname);
      if (fexists(ExecuteSpec.outputfile)) {
        fprintf(stderr, "error: file %s already exists\n", ExecuteSpec.outputfile);
        exit(1);
      }
      if (strncmp(outputRootname, ExecuteSpec.outputfile, strlen(outputRootname))) {
        char *ptr;
        if (!(outputRootname = malloc(sizeof(*outputRootname) * (strlen(ExecuteSpec.outputfile) + 1))))
          SDDS_Bomb("memory allocation failure");
        strcpy(outputRootname, ExecuteSpec.outputfile);
        if ((ptr = strchr(outputRootname, '.')))
          *ptr = 0;
      }
      globalRampPause = ExecuteSpec.ramp_pause;
      globalRampSteps = ExecuteSpec.ramp_steps;
      globalFlags = do_experiment_flags;
      /* run pre_experiment script if any */
      if (ExecuteSpec.pre_experiment && strlen(ExecuteSpec.pre_experiment)) {
        if (do_experiment_flags & FL_VERBOSE)
          fprintf(stdout, "run pre-experiment script: %s \n", ExecuteSpec.pre_experiment);
        system(ExecuteSpec.pre_experiment);
      }
      if (summarize)
        summarize_experiment(&ExecuteSpec, VariableData, Variables, Measurement, Measurements,
                             SystemCallData, SystemCalls);
      do_experiment(&ExecuteSpec, VariableData, Variables, Measurement, Measurements,
                    SystemCallData, SystemCalls, inputfile, comment, do_experiment_flags, pendIOtime, scalarFile);
      /*run post_experiment script if any */
      if (ExecuteSpec.post_experiment && strlen(ExecuteSpec.post_experiment)) {
        if (do_experiment_flags & FL_VERBOSE)
          fprintf(stdout, "run post-experiment script: %s \n", ExecuteSpec.post_experiment);
        system(ExecuteSpec.post_experiment);
      }
      break;
    case VARIABLE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &variable);
        fflush(stdout);
      }
      if (!(VariableData = add_variable(VariableData, Variables, &VariableSpec, pendIOtime)))
        SDDS_Bomb("unable to add variable");
      Variables++;
      VariableSpec.values_file = NULL;
      VariableSpec.values_file_column = NULL;
      break;
    case MEASUREMENT_FILE_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &measurement_file);
        fflush(stdout);
      }
      if (!(Measurement = add_file_measurement(Measurement, &Measurements, &MeasurementFileSpec, pendIOtime)))
        SDDS_Bomb("unable to add measurement");
      break;
    case MEASUREMENT_NAMELIST:
      if (echo_input) {
        print_namelist(stdout, &measurement);
        fflush(stdout);
      }
      if (!(Measurement = add_measurement(Measurement, Measurements, &MeasurementSpec, pendIOtime, 1)))
        SDDS_Bomb("unable to add measurement");
      Measurements++;
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

  if (SystemCalls)
    free(SystemCallData);
  if (Variables)
    free(VariableData);
  if (Measurements)
    free(Measurement);

  if (dev_timeout_errors)
    fprintf(FPINFO, "%ld device timeout errors occurred\n", dev_timeout_errors);
  if (total_limit_errors)
    fprintf(FPINFO, "%ld limit errors occurred\n", total_limit_errors);
  free_scanargs(&s_arg, argc);
  return 0;
}

VARIABLE_DATA *add_variable(VARIABLE_DATA *VariableData, long Variables, VARIABLESPEC *NewVariable, double pendIOtime) {
  long i, highest_index_number;
  double readsetpoint=0, midpoint, spread;

#if defined(DEBUG)
  fprintf(FPINFO, "adding variable\n");
#endif
  VariableData = trealloc(VariableData, sizeof(*VariableData) * (Variables + 1));
  VariableData[Variables].PVchid = NULL;
  VariableData[Variables].RBPVchid = NULL;

  if (!NewVariable->control_name)
    SDDS_Bomb("control_name not given for variable");
  if (!NewVariable->column_name || SDDS_StringIsBlank(NewVariable->column_name))
    SDDS_CopyString(&NewVariable->column_name, NewVariable->control_name);
  if (NewVariable->readback_message && !SDDS_StringIsBlank(NewVariable->readback_message))
    SDDS_Bomb("readback_message field no longer accepted.");
  NewVariable->readback_message = NULL;
  if (NewVariable->set_message && !SDDS_StringIsBlank(NewVariable->set_message))
    SDDS_Bomb("set_message field no longer accepted.");
  NewVariable->set_message = NULL;
 
  for (i = 0; i < Variables; i++) {
    if (strcmp(NewVariable->control_name, VariableData[i].VariableSpec.control_name) == 0) {
      VariableData[Variables].PVchid = VariableData[i].PVchid;
      break;
    }
  }
  if (SDDS_StringIsBlank(NewVariable->units) && !NewVariable->is_knob)
    getUnits(&NewVariable->control_name, &NewVariable->units, 1, &(VariableData[Variables].PVchid), pendIOtime);
  
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
      fprintf(stderr, "error: index_limit not defined for variable %s\n", NewVariable->control_name);
      exit(1);
    }
  } else {
    if (i == Variables)
      NewVariable->index_limit = 0; /* don't care what was entered, as no other linked index */
    if (!NewVariable->values_file_column) {
      fprintf(stderr, "error; values_file_column not defined for variable %s\n",
              NewVariable->control_name);
      exit(1);
    }
    if (!(VariableData[Variables].valueFromFile =
            ReadValuesFileData(&VariableData[Variables].valuesFromFile,
                               NewVariable->values_file, NewVariable->values_file_column))) {
      fprintf(stderr, "Error: problem reading column %s from file %s for variable %s\n",
              NewVariable->values_file_column, NewVariable->values_file,
              NewVariable->control_name);
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
  
  if (NewVariable->is_knob) {
    /*read knobs */
    VariableData[Variables].knob.knobName = NewVariable->control_name;
    read_knob_file(&VariableData[Variables], NewVariable, pendIOtime);
  } else {
    /* storing original value of variable */
    if (!VariableData[Variables].PVchid) {
      if (ca_search(NewVariable->control_name, &(VariableData[Variables].PVchid)) != ECA_NORMAL) {
	fprintf(stderr, "error: problem doing search for0 %s\n", NewVariable->control_name);
	exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some variable channels\n");
      fprintf(stderr, "%s not connected4\n", NewVariable->control_name);
      exit(1);
    }
    if (ca_state(VariableData[Variables].PVchid) != cs_conn) {
      fprintf(stderr, "error getting setpoint of %s (no connection)--aborting\n", NewVariable->control_name);
      exit(1);
    }
    if (ca_get(DBR_DOUBLE, VariableData[Variables].PVchid, &readsetpoint) != ECA_NORMAL) {
      fprintf(stderr, "error getting setpoint of %s (ca_get error)--aborting\n", NewVariable->control_name);
      exit(1);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error getting setpoint of %s (no return in time)--aborting\n", NewVariable->control_name);
      exit(1);
    }
  }
  VariableData[Variables].original_value = readsetpoint;
  if (!NewVariable->is_knob) {
    fprintf(FPINFO, "Storing original value of %s: %e \n",
	    NewVariable->control_name, VariableData[Variables].original_value);
  } else {
    fprintf(FPINFO, "Storing original value of knob %s \n", NewVariable->control_name);
    for (i=0; i<VariableData[Variables].knob.PVs; i++) {
      fprintf(FPINFO, "Storing original value of %s: %e \n", VariableData[Variables].knob.ControlName[i],  VariableData[Variables].knob.initial_value[i]);
    }
  }
  VariableData[Variables].VariableSpec = *NewVariable;
  return (VariableData);
}

MEASUREMENT_DATA *add_measurement(MEASUREMENT_DATA *MeasurementData, long Measurements, MEASUREMENTSPEC *NewMeasurement, double pendIOtime, int getUnitsNow) {
  if (!NewMeasurement->control_name)
    SDDS_Bomb("no process variable name supplied for measurement");
  if (!NewMeasurement->column_name || SDDS_StringIsBlank(NewMeasurement->column_name))
    SDDS_CopyString(&NewMeasurement->column_name, NewMeasurement->control_name);
  if (NewMeasurement->lower_limit > NewMeasurement->upper_limit)
    SDDS_Bomb("invalid lower_limit and upper_limit for measurement");
  if (NewMeasurement->read_message && !SDDS_StringIsBlank(NewMeasurement->read_message))
    SDDS_Bomb("read_message field no longer accepted.");
  NewMeasurement->read_message = NULL;
  MeasurementData = trealloc(MeasurementData, sizeof(*MeasurementData) * (Measurements + 1));
  MeasurementData[Measurements].PVchid = NULL;
  if (getUnitsNow) {
    if (SDDS_StringIsBlank(NewMeasurement->units) &&
        !getUnits(&NewMeasurement->control_name, &NewMeasurement->units, 1, &(MeasurementData[Measurements].PVchid),
                  pendIOtime))
      SDDS_Bomb("unable to get units for quantity");
  }
  MeasurementData[Measurements].MeasurementSpec = *NewMeasurement;
  return (MeasurementData);
}

SYSTEMCALL_DATA *add_system_call(SYSTEMCALL_DATA *SystemCallData, long SystemCalls, SYSTEMCALLSPEC *NewSystemCall) {
  SystemCallData = trealloc(SystemCallData, sizeof(*SystemCallData) * (SystemCalls + 1));
  SystemCallData[SystemCalls].call_number = SystemCallData[SystemCalls].cycle_call_number = 0;
  SystemCallData[SystemCalls].SystemCallSpec = *NewSystemCall;
  if (SystemCallData[SystemCalls].SystemCallSpec.call_before_cycle || SystemCallData[SystemCalls].SystemCallSpec.call_after_cycle) {
    SystemCallData[SystemCalls].SystemCallSpec.call_before_setting = SystemCallData[SystemCalls].SystemCallSpec.call_before_measuring = SystemCallData[SystemCalls].SystemCallSpec.call_between_measuring = 0;
  } else {
    if (SystemCallData[SystemCalls].SystemCallSpec.call_before_setting)
      SystemCallData[SystemCalls].SystemCallSpec.call_before_measuring = 1;
    /*between measurement calls only*/
    if (SystemCallData[SystemCalls].SystemCallSpec.call_between_measuring)
      SystemCallData[SystemCalls].SystemCallSpec.call_before_measuring = SystemCallData[SystemCalls].SystemCallSpec.call_before_setting = 0;
  }
  return (SystemCallData);
}

void summarize_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *Variable, long Variables,
                          MEASUREMENT_DATA *Measurement, long Measurements,
                          SYSTEMCALL_DATA *SystemCall, long SystemCalls) {
  long i;
  fprintf(FPINFO, "\n\nSummary of experiment to be recorded in file %s:\n", Execute->outputfile);
  fprintf(FPINFO, "  settling time between changes and measurements:  post_change_pause = %f s\n", Execute->post_change_pause);
  fprintf(FPINFO, "  pause interval between repeated measurements:    intermeasurement_pause = %f s\n", Execute->intermeasurement_pause);
  fprintf(FPINFO, "\n%ld %s been defined:\n", Variables, Variables > 1 ? "variables have" : "variable has");
  fprintf(FPINFO, "control_name    index_number    index_limit    initial_value     final_value     values_file\n");
  for (i = 0; i < Variables; i++) {
    if (!Variable[i].VariableSpec.values_file)
      fprintf(FPINFO, "%-21s    %-12ld    %-11ld    %-13.6e    %-13.6e\n",
              Variable[i].VariableSpec.control_name, Variable[i].VariableSpec.index_number,
              Variable[i].VariableSpec.index_limit, Variable[i].VariableSpec.initial_value,
              Variable[i].VariableSpec.final_value);
    else
      fprintf(FPINFO, "%-21s    %-12ld    %-11ld    %-13s    %-13s\n",
              Variable[i].VariableSpec.control_name, Variable[i].VariableSpec.index_number,
              Variable[i].VariableSpec.index_limit,
              "N/A", "N/A");
  }

  fprintf(FPINFO, "\n%ld %s been defined:\n", Measurements, Measurements > 1 ? "measurements have" : "measurement has");
  fprintf(FPINFO, "control_name      number_to_average      include_standard_deviation include_sigma\n");
  for (i = 0; i < Measurements; i++)
    fprintf(FPINFO, "%-21s      %-17ld      %-26s %-15s\n",
            Measurement[i].MeasurementSpec.control_name,
            Measurement[i].MeasurementSpec.number_to_average,
            Measurement[i].MeasurementSpec.include_standard_deviation ? "yes" : "no",
            Measurement[i].MeasurementSpec.include_sigma ? "yes" : "no");
  putchar('\n');
  fflush(FPINFO);
}

static COUNTER *counter_global;
static long counters_global;
static SDDS_TABLE SDDS_table;
static long table_written = 0, table_active = 0;

void do_experiment(EXECUTESPEC *Execute, VARIABLE_DATA *VariableData, long Variables,
                   MEASUREMENT_DATA *Measurement, long Measurements,
                   SYSTEMCALL_DATA *SystemCallData, long SystemCalls,
                   const char *inputfile, char *comment, long flags, double pendIOtime, char *scalarFile) {
  long i, j, z, counters, columns, repeat, cycle, cycles, ik;
  COUNTER *counter;
  char *date;
  double StartHour, TimeOfDay, ElapsedTime, EpochTime, StartTime;
  static char s[1024], t[1024];
  long step, data_points;
  long counter_rolled_over, first_pause;
  char **column_list;
  long column_lists, k, columnFlag;
  char *column_string;
  char *column_token;
  short short_value;
  long long_value;
  float float_value;
  double double_value;
  char char_value;
  char *string_value;
  char **scalarPV, **scalarName, **scalarUnits, **scalarSymbol, **readMessage;
  long *scalarDataType, scalars, strScalarExist = 0;
  chid *scalarCHID;
  double *scalarData, *scalarFactor;
  char **strScalarData;

  scalars = 0;
  scalarPV = scalarName = scalarUnits = scalarSymbol = readMessage = NULL;
  scalarDataType = NULL;
  scalarData = scalarFactor = NULL;
  strScalarData = NULL;

  if (!Variables && !SystemCalls)
    SDDS_Bomb("no variables or system calls defined");

  column_string = NULL;
  column_token = NULL;
  counters = 0;
  short_value = 0;
  long_value = 0;
  float_value = 0;
  double_value = 0;
  /*char_value = NULL;*/
  string_value = NULL;
  cycles = Execute->cycles;

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
    counter[i].first_set = 1;
    counter[i].VariableData = NULL;
    counter[i].SystemCallData = NULL;
    counter[i].Output_Column = NULL;
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
    if (VariableData[i].valuesFromFile) {
      VariableData[i].last_value = VariableData[i].valueFromFile[0];
    } else {
      VariableData[i].last_value = VariableData[i].VariableSpec.initial_value;
    }
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
  data_points *= Execute->repeat_readings;
  if (Execute->measure_before_first_change)
    data_points += 1;
  
  setupForInitialRamping(counter, counters, flags, pendIOtime);

  if (flags & FL_VERYVERBOSE) {
    for (i = 0; i < counters; i++) {
      fprintf(FPINFO, "Index %ld has a limit of %ld, %ld variables, and %ld system calls\n",
              i, counter[i].limit, counter[i].Variables,
              counter[i].SystemCalls);
      if (counter[i].substeps != 1)
        fprintf(FPINFO, "  %ld substeps will be taken at intervals of %f seconds\n",
                counter[i].substeps, counter[i].substep_pause);
      for (j = 0; j < counter[i].Variables; j++) {
        if (!counter[i].VariableData[j]->valueFromFile)
          fprintf(FPINFO, "    %s, from %e to %e\n",
                  counter[i].VariableData[j]->VariableSpec.control_name,
                  counter[i].VariableData[j]->VariableSpec.initial_value,
                  counter[i].VariableData[j]->VariableSpec.final_value);
        else
          fprintf(FPINFO, "    %s, %ld values of %s from file %s\n",
                  counter[i].VariableData[j]->VariableSpec.control_name,
                  counter[i].VariableData[j]->valuesFromFile,
                  counter[i].VariableData[j]->VariableSpec.values_file_column,
                  counter[i].VariableData[j]->VariableSpec.values_file);
      }
      for (j = 0; j < counter[i].SystemCalls; j++)
        fprintf(FPINFO, "    \"%s\" (%s measurements, %s variable setting)\n",
                counter[i].SystemCallData[j]->SystemCallSpec.command,
                counter[i].SystemCallData[j]->SystemCallSpec.call_before_measuring ? "before" : "after",
                counter[i].SystemCallData[j]->SystemCallSpec.call_before_setting ? "before" : "after");
    }
    fprintf(FPINFO, "A total of %ld data points will be taken\n", data_points);
    fflush(FPINFO);
  }

  for (i = 0; i < counters; i++) {
    counter[i].Output_Column = NULL;
    for (j = 0; j < counter[i].SystemCalls; j++) {
      counter[i].Output_Column = trealloc(counter[i].Output_Column, sizeof(*counter[i].Output_Column) * (j + 1));
      counter[i].Output_Column[j].columns = 0;
      counter[i].Output_Column[j].types = 0;

      if (counter[i].SystemCallData[j]->SystemCallSpec.output_column_list) {
        column_string = counter[i].SystemCallData[j]->SystemCallSpec.output_column_list;
        counter[i].Output_Column[j].column = NULL;
        while (1) {
          column_token = strtok(column_string, " ");
          if (column_token == NULL)
            break;
          for (z = 0; z < counter[i].Output_Column[j].columns; z++) {
            if (strcmp(counter[i].Output_Column[j].column[z], column_token) == 0) {
              SDDS_Bomb("Multiple occurrence of the same column name in the SystemCall.");
              break;
            }
          }
          counter[i].Output_Column[j].column = trealloc(counter[i].Output_Column[j].column,
                                                        sizeof(*counter[i].Output_Column[j].column) * (counter[i].Output_Column[j].columns + 1));
          counter[i].Output_Column[j].column[counter[i].Output_Column[j].columns] =
            malloc(sizeof(counter[i].Output_Column[j].column[counter[i].Output_Column[j].columns]) * (strlen(column_token) + 1));
          strcpy(counter[i].Output_Column[j].column[counter[i].Output_Column[j].columns], column_token);

          counter[i].Output_Column[j].columns++;
          column_string = NULL;
        }
      }

      if (counter[i].SystemCallData[j]->SystemCallSpec.output_type_list) {
        column_string = counter[i].SystemCallData[j]->SystemCallSpec.output_type_list;
        counter[i].Output_Column[j].type = NULL;
        counter[i].Output_Column[j].SDDS_type = NULL;
        while (1) {
          column_token = strtok(column_string, " ");
          if (column_token == NULL)
            break;
          counter[i].Output_Column[j].type = trealloc(counter[i].Output_Column[j].type,
                                                      sizeof(*counter[i].Output_Column[j].type) * (counter[i].Output_Column[j].types + 1));
          counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = malloc(sizeof(*column_token));
          counter[i].Output_Column[j].SDDS_type = trealloc(counter[i].Output_Column[j].SDDS_type,
                                                           sizeof(*counter[i].Output_Column[j].SDDS_type) * (counter[i].Output_Column[j].types + 1));

          if (strcmp(column_token, "short") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "short";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_SHORT;
          } else if (strcmp(column_token, "long") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "long";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_LONG;
          } else if (strcmp(column_token, "float") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "float";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_FLOAT;
          } else if (strcmp(column_token, "double") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "double";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_DOUBLE;
          } else if (strcmp(column_token, "character") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "character";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_CHARACTER;
          } else if (strcmp(column_token, "string") == 0) {
            counter[i].Output_Column[j].type[counter[i].Output_Column[j].types] = "string";
            counter[i].Output_Column[j].SDDS_type[counter[i].Output_Column[j].types] = SDDS_STRING;
          } else {
            fprintf(stderr, "error: unknown column type: %s\n", column_token);
            exit(1);
          }
          counter[i].Output_Column[j].types++;
          column_string = NULL;
        }
      }
      if (counter[i].Output_Column[j].columns != counter[i].Output_Column[j].types)
        SDDS_Bomb("Number of output columns must be the same as number of output types.");
    }
  }

  columns = Variables + Measurements + 1;
  for (i = 0; i < Measurements; i++) {
    if (Measurement[i].MeasurementSpec.include_standard_deviation)
      columns += 1;
    if (Measurement[i].MeasurementSpec.include_sigma)
      columns += 1;
    if (Measurement[i].MeasurementSpec.include_maximum)
      columns += 1;
    if (Measurement[i].MeasurementSpec.include_minimum)
      columns += 1;
  }

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_BINARY, columns / 8 + 1, "data collected by sddsexperiment",
                             "sddsexperiment data", Execute->outputfile)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (flags & FL_VERYVERBOSE) {
    fprintf(FPINFO, "Output file initialized\n");
    fflush(FPINFO);
  }

  getTimeBreakdown(&StartTime, NULL, &StartHour, NULL, NULL, NULL, &date);
  if (comment) {
    if (SDDS_DefineParameter(&SDDS_table, "Comment", NULL, NULL, "comments for experiment",
                             NULL, SDDS_STRING, comment) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  if (SDDS_DefineParameter(&SDDS_table, "TimeStamp", NULL, NULL, "date and time of start of experiment",
                           NULL, SDDS_STRING, date) < 0 ||
      SDDS_DefineColumn(&SDDS_table, "ElapsedTime", NULL, "s", "Elapsed time since start of the experiment",
                        NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_table, "Time", NULL, "s", "Time since start of epoch",
                        NULL, SDDS_DOUBLE, 0) < 0 ||
      SDDS_DefineColumn(&SDDS_table, "TimeOfDay", NULL, "h", NULL, NULL, SDDS_DOUBLE, 0) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
#if defined(DEBUG)
  fprintf(FPINFO, "parameters initialized\n");
#endif

  for (i = 0; i < Variables; i++) {
    if (VariableData[i].VariableSpec.is_knob) {
      if (SDDS_DefineColumn(&SDDS_table, VariableData[i].VariableSpec.control_name,
			    "Knob", NULL,
			    NULL, NULL, SDDS_DOUBLE, 0) < 0) {
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
	exit(1);
      }
      
      for (ik=0; ik<VariableData[i].knob.PVs; ik++) {
	if (SDDS_DefineColumn(&SDDS_table, VariableData[i].knob.ControlName[ik],
			      VariableData[i].knob.ReadbackName ? VariableData[i].knob.ReadbackName[ik]: NULL, VariableData[i].knob.units[ik],
			      NULL, NULL, SDDS_DOUBLE, 0) < 0) {
	  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
	  exit(1);
	}
      }
    } else {
      if (VariableData[i].VariableSpec.control_name) {
	if (SDDS_DefineColumn(&SDDS_table, VariableData[i].VariableSpec.column_name,
			      VariableData[i].VariableSpec.symbol, VariableData[i].VariableSpec.units,
			      NULL, NULL, SDDS_DOUBLE, 0) < 0) {
	  SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
	  exit(1);
	}
      }
    }
  }
#if defined(DEBUG)
  fprintf(FPINFO, "variables initialized\n");
#endif

  for (i = 0; i < SystemCalls; i++) {
    if (!SystemCallData[i].SystemCallSpec.call_before_cycle && !SystemCallData[i].SystemCallSpec.call_after_cycle && SystemCallData[i].SystemCallSpec.counter_column_name) {
      if (SDDS_DefineColumn(&SDDS_table, SystemCallData[i].SystemCallSpec.counter_column_name,
                            NULL, NULL, "system call counter", NULL, SDDS_LONG, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  }
  column_lists = 0;
  column_list = NULL;
  for (i = 0; i < counters; i++) {
    for (j = 0; j < counter[i].SystemCalls; j++) {
      for (z = 0; z < counter[i].Output_Column[j].columns; z++) {
        /*printf("counter[i].Output_Column[j].column[z] = %s\n", counter[i].Output_Column[j].column[z]);*/
        columnFlag = 0;
        for (k = 0; k < column_lists; k++)
          if (strcmp(column_list[k], counter[i].Output_Column[j].column[z]) == 0)
            columnFlag = 1;

        if (!columnFlag) {
          if (SDDS_DefineColumn(&SDDS_table, counter[i].Output_Column[j].column[z],
                                NULL, NULL, NULL, NULL, counter[i].Output_Column[j].SDDS_type[z], 0) < 0) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            exit(1);
          }
          column_list = trealloc(column_list, sizeof(*column_list) * (column_lists + 1));
          column_list[column_lists] = counter[i].Output_Column[j].column[z];
          column_lists++;
        }
      }
    }
  }
  if (column_lists)
    free(column_list);
#if defined(DEBUG)
  fprintf(FPINFO, "system calls initialized\n");
#endif

  for (i = 0; i < Measurements; i++) {
#if defined(DEBUG)
    fprintf(FPINFO, "defining column for %s (%s)\n", Measurement[i].MeasurementSpec.control_name,
            Measurement[i].MeasurementSpec.column_name);
#endif
    if ((Measurement[i].MeasurementSpec.include_standard_deviation ||
         Measurement[i].MeasurementSpec.include_sigma ||
         Measurement[i].MeasurementSpec.include_maximum ||
         Measurement[i].MeasurementSpec.include_minimum) &&
        Measurement[i].MeasurementSpec.number_to_average < 2)
      SDDS_Bomb("number of averages must be >1 to compute sigma, standard deviation, minimum, or maximum.");
    if (SDDS_DefineColumn(&SDDS_table, Measurement[i].MeasurementSpec.column_name,
                          Measurement[i].MeasurementSpec.symbol, Measurement[i].MeasurementSpec.units,
                          NULL, NULL, SDDS_DOUBLE, 0) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (Measurement[i].MeasurementSpec.include_standard_deviation) {
      sprintf(s, "StDev%s", Measurement[i].MeasurementSpec.column_name);
      if (Measurement[i].MeasurementSpec.symbol)
        sprintf(t, "$gs$r[%s]", Measurement[i].MeasurementSpec.symbol);
      if (SDDS_DefineColumn(&SDDS_table, s, Measurement[i].MeasurementSpec.symbol ? t : NULL,
                            Measurement[i].MeasurementSpec.units, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (Measurement[i].MeasurementSpec.include_sigma) {
      sprintf(s, "Sigma%s", Measurement[i].MeasurementSpec.column_name);
      if (Measurement[i].MeasurementSpec.symbol)
        sprintf(t, "$gs$r[<%s>]", Measurement[i].MeasurementSpec.symbol);
      if (SDDS_DefineColumn(&SDDS_table, s, Measurement[i].MeasurementSpec.symbol ? t : NULL,
                            Measurement[i].MeasurementSpec.units, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (Measurement[i].MeasurementSpec.include_minimum) {
      sprintf(s, "Min%s", Measurement[i].MeasurementSpec.column_name);
      if (Measurement[i].MeasurementSpec.symbol)
        sprintf(t, "Min[%s]", Measurement[i].MeasurementSpec.symbol);
      if (SDDS_DefineColumn(&SDDS_table, s, Measurement[i].MeasurementSpec.symbol ? t : NULL,
                            Measurement[i].MeasurementSpec.units, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
    if (Measurement[i].MeasurementSpec.include_maximum) {
      sprintf(s, "Max%s", Measurement[i].MeasurementSpec.column_name);
      if (Measurement[i].MeasurementSpec.symbol)
        sprintf(t, "Max[%s]", Measurement[i].MeasurementSpec.symbol);
      if (SDDS_DefineColumn(&SDDS_table, s, Measurement[i].MeasurementSpec.symbol ? t : NULL,
                            Measurement[i].MeasurementSpec.units, NULL, NULL, SDDS_DOUBLE, 0) < 0) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
    }
  }
#if defined(DEBUG)
  fprintf(FPINFO, "measurements initialized\n");
#endif
  if (scalarFile) {
    if (!getScalarMonitorDataModified(&scalarPV, &readMessage, &scalarName,
                                      &scalarUnits, &scalarSymbol, &scalarFactor, NULL, &scalarDataType,
                                      &scalars, scalarFile, GET_UNITS_IF_BLANK, &scalarCHID, pendIOtime))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    scalarData = (double *)calloc(scalars, sizeof(*scalarData));
    if (!scalarUnits) {
      scalarUnits = (char **)tmalloc(sizeof(*scalarUnits) * scalars);
      getUnits(scalarPV, scalarUnits, scalars, scalarCHID, pendIOtime);
    }
    strScalarData = (char **)malloc(sizeof(*strScalarData) * scalars);
    if (flags & FL_VERYVERBOSE) {
      fprintf(FPINFO, "Define scalar parameters.\n");
      fflush(FPINFO);
    }

    for (i = 0; i < scalars; i++) {
      strScalarData[i] = NULL;
      if (scalarDataType[i] == SDDS_STRING) {
        strScalarExist = 1;
        strScalarData[i] = (char *)malloc(sizeof(char) * 40);
      }
      if (scalarSymbol) {
        if (!SDDS_DefineParameter(&SDDS_table, scalarName[i], scalarSymbol[i], scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_DefineParameter(&SDDS_table, scalarName[i], NULL, scalarUnits[i], NULL, NULL, scalarDataType[i], NULL))
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    }
  }
  if (!SDDS_WriteLayout(&SDDS_table)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  table_written = 0;
  table_active = 1;
  if (flags & FL_VERYVERBOSE) {
    fprintf(FPINFO, "Output header written.\n");
    fflush(FPINFO);
  }

  counter_global = counter;
  counters_global = counters;

  cycle = 0;
  do {
    step = 0;
    counter_rolled_over = 1;
    first_pause = 1;
    /*reset system call number */
    for (i = 0; i < counters; i++) {
      for (j = 0; j < counter[i].SystemCalls; j++) {
        counter[i].SystemCallData[j]->call_number = 0;
      }
    }
    if (!SDDS_StartTable(&SDDS_table, data_points)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    getTimeBreakdown(&StartTime, NULL, &StartHour, NULL, NULL, NULL, &date);
    if (!SDDS_SetParameters(&SDDS_table, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                            "TimeStamp", date, NULL))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (Execute->measure_before_first_change) {
      take_measurements(Measurement, Measurements, Execute->intermeasurement_pause,
                        flags, Execute->allowed_timeout_errors, Execute->allowed_limit_errors,
                        Execute->outlimit_pause, pendIOtime, SystemCallData, SystemCalls);
      ElapsedTime = (EpochTime = getTimeInSecs()) - StartTime;
      TimeOfDay = StartHour + ElapsedTime / 3600.0;
      if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                             "ElapsedTime", ElapsedTime, "Time", EpochTime, "TimeOfDay", TimeOfDay, NULL)) {
        SDDS_SetError("Unable to set time value in SDDS data set");
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      /* loop over measurements */
      for (i = 0; i < Measurements; i++) {
        if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                               Measurement[i].MeasurementSpec.column_name,
                               Measurement[i].average_value, NULL)) {
          SDDS_SetError("Unable to set measurement value in SDDS data set");
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
        if (Measurement[i].MeasurementSpec.include_standard_deviation) {
            sprintf(s, "StDev%s", Measurement[i].MeasurementSpec.column_name);
            if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                   s, Measurement[i].standard_deviation, NULL)) {
              SDDS_SetError("Unable to set measurement standard deviation value in SDDS data set");
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            }
        }
        if (Measurement[i].MeasurementSpec.include_sigma) {
          sprintf(s, "Sigma%s", Measurement[i].MeasurementSpec.column_name);
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                 s, Measurement[i].standard_deviation / sqrt((double)Measurement[i].number_of_values),
                                 NULL)) {
            SDDS_SetError("Unable to set measurement sigma value in SDDS data set");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
        if (Measurement[i].MeasurementSpec.include_minimum) {
          sprintf(s, "Min%s", Measurement[i].MeasurementSpec.column_name);
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                 s, Measurement[i].minimum, NULL)) {
            SDDS_SetError("Unable to set measurement minimum value in SDDS data set");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
        if (Measurement[i].MeasurementSpec.include_maximum) {
          sprintf(s, "Max%s", Measurement[i].MeasurementSpec.column_name);
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                 s, Measurement[i].maximum, NULL)) {
            SDDS_SetError("Unable to set measurement maximum value in SDDS data set");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
      }
      step++;
    }
    if (flags & FL_VERBOSE && cycles > 1) {
      fprintf(FPINFO, "\nstart cycle %ld\n", cycle);
      fflush(FPINFO);
    }
    if (!(flags & FL_DRY_RUN)) {
      if (flags & FL_VERYVERBOSE) {
        fprintf(FPINFO, "ramping PVs to initial values\n");
        fflush(FPINFO);
      }
      doRampPVs(globalRampSteps, globalRampPause, flags & (FL_VERYVERBOSE | FL_VERBOSE), pendIOtime);
    } else if (flags & FL_VERYVERBOSE) {
      fprintf(FPINFO, "pretending to ramp PVs to initial values:\n");
      showRampPVs(FPINFO, pendIOtime);
      fflush(FPINFO);
    }
    if (scalars) {
      if (flags & FL_VERBOSE) {
        fprintf(FPINFO, "read scalar pvs\n");
        fflush(FPINFO);
      }
      if ((!strScalarExist && ReadScalarValues(scalarPV, readMessage, scalarFactor,
                                               scalarData, scalars, scalarCHID, pendIOtime)) ||
          ReadMixScalarValues(scalarPV, readMessage, scalarFactor, scalarData, scalarDataType,
                              strScalarData, scalars, scalarCHID, pendIOtime)) {
        fprintf(stderr, "Error reading scalar pvs, exit\n");
        exit(1);
      }
      if (flags & FL_VERBOSE) {
        fprintf(FPINFO, "set scalar parameters\n");
        fflush(FPINFO);
      }
      for (i = 0; i < scalars; i++) {
        if (scalarDataType[i] == SDDS_STRING) {
          if (!SDDS_SetParameters(&SDDS_table, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                  scalarName[i], strScalarData[i], NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        } else {
          if (!SDDS_SetParameters(&SDDS_table, SDDS_BY_NAME | SDDS_PASS_BY_VALUE,
                                  scalarName[i], scalarData[i], NULL))
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
      }
    }
#ifdef DEBUG
    fprintf(FPINFO, "making calls before cycle(1)\n");
    fflush(FPINFO);
#endif
    make_cycle_calls(counter, counters, flags, BEFORE_CYCLE);
    do {
#ifdef DEBUG
      fprintf(FPINFO, "making calls for counters (1)\n");
      fflush(FPINFO);
#endif
      make_calls_for_counters(counter, counters, counter_rolled_over, flags, BEFORE_SETTING | BEFORE_MEASURING);
#ifdef DEBUG
      fprintf(FPINFO, "setting variable values\n");
      fflush(FPINFO);
#endif
      set_variable_values(counter, counters, flags, Execute->allowed_timeout_errors, counter_rolled_over, pendIOtime);
      if (counter_rolled_over && (Execute->rollover_pause > 0)) {
#ifdef DEBUG
        fprintf(FPINFO, "executing roll-over pause\n");
        fflush(FPINFO);
#endif
        oag_ca_pend_event(Execute->rollover_pause, &sigint);
      }
      if (Execute->post_change_pause > 0) {
        /* skip pause if requested in input file */
        if (!first_pause || !(Execute->skip_first_pause)) {
          if (flags & FL_VERBOSE) {
            fprintf(FPINFO, "Waiting %f seconds after changing values\n", Execute->post_change_pause);
            fflush(FPINFO);
          }
          oag_ca_pend_event(Execute->post_change_pause, &sigint);
        }
        first_pause = 0;
      }
      if (Execute->post_change_key_wait) {
        fprintf(FPINFO, "Press <return> to continue\7\7\n");
        fflush(FPINFO);
        getchar();
      }
#ifdef DEBUG
      fprintf(FPINFO, "making calls for counters (2)\n");
      fflush(FPINFO);
#endif
      make_calls_for_counters(counter, counters, counter_rolled_over, flags, AFTER_SETTING | BEFORE_MEASURING);
      for (repeat = 0; repeat < Execute->repeat_readings; repeat++) {

        if (sigint) {
          exit(1);
        }

#ifdef DEBUG
        fprintf(FPINFO, "taking measurements\n");
        fflush(FPINFO);
#endif
        take_measurements(Measurement, Measurements, Execute->intermeasurement_pause,
                          flags, Execute->allowed_timeout_errors, Execute->allowed_limit_errors,
                          Execute->outlimit_pause, pendIOtime, SystemCallData, SystemCalls);
        oag_ca_pend_event(Execute->post_reading_pause, &sigint);
        ElapsedTime = (EpochTime = getTimeInSecs()) - StartTime;
        TimeOfDay = StartHour + ElapsedTime / 3600.0;
#ifdef DEBUG
        fprintf(FPINFO, "setting row values\n");
        fflush(FPINFO);
#endif
        if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                               "ElapsedTime", ElapsedTime, "Time", EpochTime, "TimeOfDay", TimeOfDay, NULL)) {
          SDDS_SetError("Unable to set time value in SDDS data set");
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        }
#ifdef DEBUG
        fprintf(FPINFO, "setting values for variables\n");
        fflush(FPINFO);
#endif
        /* loop over variables */
        for (i = 0; i < Variables; i++) {
	  if  (VariableData[i].VariableSpec.is_knob) {
	    if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
				   VariableData[i].VariableSpec.column_name,
				   VariableData[i].value, NULL)) {
	      SDDS_SetError("Unable to set variable value in SDDS data set");
	      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
	    }
	    for (ik=0;  ik<VariableData[i].knob.PVs; ik++) {
	       if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
				      VariableData[i].knob.ControlName[ik],
				      VariableData[i].knob.value[ik], NULL)) {
		 SDDS_SetError("Unable to set variable value in SDDS data set");
		 SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
	       }
	    }
	  } else {
	    if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
				   VariableData[i].VariableSpec.column_name,
				   VariableData[i].value, NULL)) {
	      SDDS_SetError("Unable to set variable value in SDDS data set");
	      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
	    }
	  }
        }
#ifdef DEBUG
        fprintf(FPINFO, "setting values for measurements\n");
        fflush(FPINFO);
#endif
        /* loop over measurements */
        for (i = 0; i < Measurements; i++) {
          if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                 Measurement[i].MeasurementSpec.column_name,
                                 Measurement[i].average_value, NULL)) {
            SDDS_SetError("Unable to set measurement value in SDDS data set");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
          if (Measurement[i].MeasurementSpec.include_standard_deviation) {
            sprintf(s, "StDev%s", Measurement[i].MeasurementSpec.column_name);
            if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                   s, Measurement[i].standard_deviation, NULL)) {
              SDDS_SetError("Unable to set measurement standard deviation value in SDDS data set");
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            }
          }
          if (Measurement[i].MeasurementSpec.include_sigma) {
            sprintf(s, "Sigma%s", Measurement[i].MeasurementSpec.column_name);
            if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                   s, Measurement[i].standard_deviation / sqrt((double)Measurement[i].number_of_values),
                                   NULL)) {
              SDDS_SetError("Unable to set measurement sigma value in SDDS data set");
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            }
          }
          if (Measurement[i].MeasurementSpec.include_minimum) {
            sprintf(s, "Min%s", Measurement[i].MeasurementSpec.column_name);
            if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                   s, Measurement[i].minimum, NULL)) {
              SDDS_SetError("Unable to set measurement minimum value in SDDS data set");
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            }
          }
          if (Measurement[i].MeasurementSpec.include_maximum) {
            sprintf(s, "Max%s", Measurement[i].MeasurementSpec.column_name);
            if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                   s, Measurement[i].maximum, NULL)) {
              SDDS_SetError("Unable to set measurement maximum value in SDDS data set");
              SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
            }
          }
        }
#ifdef DEBUG
        fprintf(FPINFO, "setting values for system calls\n");
        fflush(FPINFO);
#endif
        /* loop over system calls*/
        for (i = 0; i < SystemCalls; i++) {
          if (SystemCallData[i].SystemCallSpec.counter_column_name &&
              !SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                 SystemCallData[i].SystemCallSpec.counter_column_name,
                                 SystemCallData[i].call_number - 1, NULL)) {
            SDDS_SetError("Unable to set system call counter value in SDDS data set");
            SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
          }
        }
        for (j = 0; j < counters; j++) {
          for (i = 0; i < counter[j].SystemCalls; i++) {
            for (z = 0; z < counter[j].Output_Column[i].columns; z++) {
              column_token = counter[j].Output_Column[i].type[z];
              if (strcmp(column_token, "short") == 0) {
                if (!(flags & FL_DRY_RUN)) {
                  if (!sscanf(counter[j].Output_Column[i].value[z], "%hd", &short_value)) {
                    fprintf(stderr, "error: a returned type does not match an assigned type:\
                                          \ncolumn = %s,\ntype = %s,\ncommand = %s,\ndata = %s\n",
                            counter[j].Output_Column[i].column[z], column_token,
                            counter[j].SystemCallData[i]->SystemCallSpec.command,
                            counter[j].Output_Column[i].value[z]);
                    exit(1);
                  }
                }
                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], short_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else if (strcmp(column_token, "long") == 0) {
                if (!(flags & FL_DRY_RUN)) {
                  if (!sscanf(counter[j].Output_Column[i].value[z], "%ld", &long_value)) {
                    fprintf(stderr, "error: a returned type does not match an assigned type:\
                                          \ncolumn = %s,\ntype = %s,\ncommand = %s,\ndata = %s\n",
                            counter[j].Output_Column[i].column[z], column_token,
                            counter[j].SystemCallData[i]->SystemCallSpec.command,
                            counter[j].Output_Column[i].value[z]);
                    exit(1);
                  }
                }
                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], long_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else if (strcmp(column_token, "float") == 0) {
                if (!(flags & FL_DRY_RUN)) {
                  if (!sscanf(counter[j].Output_Column[i].value[z], "%f", &float_value)) {
                    fprintf(stderr, "error: a returned type does not match an assigned type:\
                                          \ncolumn = %s,\ntype = %s,\ncommand = %s,\ndata = %s\n",
                            counter[j].Output_Column[i].column[z], column_token,
                            counter[j].SystemCallData[i]->SystemCallSpec.command,
                            counter[j].Output_Column[i].value[z]);
                    exit(1);
                  }
                }
                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], float_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else if (strcmp(column_token, "double") == 0) {
                if (!(flags & FL_DRY_RUN)) {
                  if (!sscanf(counter[j].Output_Column[i].value[z], "%lf", &double_value)) {
                    fprintf(stderr, "error: a returned type does not match an assigned type:\
                                          \ncolumn = %s,\ntype = %s,\ncommand = %s,\ndata = %s\n",
                            counter[j].Output_Column[i].column[z], column_token,
                            counter[j].SystemCallData[i]->SystemCallSpec.command,
                            counter[j].Output_Column[i].value[z]);
                    exit(1);
                  }
                }
                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], double_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else if (strcmp(column_token, "character") == 0) {
                if (!(flags & FL_DRY_RUN)) {
                  if (!sscanf(counter[j].Output_Column[i].value[z], "%c", &char_value)) {
                    fprintf(stderr, "error: a returned type does not match an assigned type:\
                                          \ncolumn = %s,\ntype = %s,\ncommand = %s,\ndata = %s\n",
                            counter[j].Output_Column[i].column[z], column_token,
                            counter[j].SystemCallData[i]->SystemCallSpec.command,
                            counter[j].Output_Column[i].value[z]);
                    exit(1);
                  }
                }
                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], char_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else if (strcmp(column_token, "string") == 0) {
                if (!(flags & FL_DRY_RUN))
                  string_value = strtok(counter[j].Output_Column[i].value[z], "\n");
                else
                  string_value = "";

                if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, step,
                                       counter[j].Output_Column[i].column[z], string_value, NULL)) {
                  SDDS_SetError("Unable to set systemcall column value in SDDS data set");
                  SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
                }
              } else {
                fprintf(stderr, "error: unknown column type: %s\n", column_token);
                exit(1);
              }
              free(counter[j].Output_Column[i].value[z]);
            }
          }
        }

        if (SDDS_NumberOfErrors())
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        step++;
      }
#ifdef DEBUG
      fprintf(FPINFO, "making calls for counters (3)\n");
      fflush(FPINFO);
#endif
      make_calls_for_counters(counter, counters, counter_rolled_over, flags, AFTER_SETTING | AFTER_MEASURING);
      if (flags & FL_VERBOSE) {
        fprintf(FPINFO, "* Experiment is %.2f%% done.\n", (100.0 * (step + cycle * data_points)) / (data_points * cycles));
        fflush(FPINFO);
      }
      if (!SDDS_UpdatePage(&SDDS_table, 0))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (flags & FL_VERBOSE)
        fprintf(FPINFO, "File updated\n");
      for (j = 0; j < counters; j++)
        if (counter[j].Output_Column)
          for (i = 0; i < counter[j].SystemCalls; i++)
            if (counter[j].Output_Column[i].columns)
              free(counter[j].Output_Column[i].value);
    } while (advance_counters(counter, counters, &counter_rolled_over) >= 0);
    /*reset counter*/
    for (i = 0; i < counters; i++)
      counter[i].index = 0;
#ifdef DEBUG
    fprintf(FPINFO, "making calls after cycle(1)\n");
    fflush(FPINFO);
#endif
    make_cycle_calls(counter, counters, flags, AFTER_CYCLE);
    cycle++;
  } while (cycle < cycles);

  for (j = 0; j < counters; j++)
    if (counter[j].Output_Column)
      for (i = 0; i < counter[j].SystemCalls; i++)
        if (counter[j].Output_Column[i].columns)
          for (z = 0; z < counter[j].Output_Column[i].columns; z++)
            free(counter[j].Output_Column[i].column[z]);
  if (scalars) {
    for (i = 0; i < scalars; i++) {
      if (scalarPV && scalarPV[i])
        free(scalarPV[i]);
      if (scalarName && scalarName[i])
        free(scalarName[i]);
      if (scalarUnits && scalarUnits[i])
        free(scalarUnits[i]);
      if (readMessage && readMessage[i])
        free(readMessage[i]);
      if (strScalarData && strScalarData[i])
        free(strScalarData[i]);
      if (scalarSymbol && scalarSymbol[i])
        free(scalarSymbol[i]);
    }
    if (scalarPV)
      free(scalarPV);
    if (scalarName)
      free(scalarName);
    if (scalarUnits)
      free(scalarUnits);
    if (readMessage)
      free(readMessage);
    if (strScalarData)
      free(strScalarData);
    if (scalarData)
      free(scalarData);
    if (scalarFactor)
      free(scalarFactor);
    if (scalarSymbol)
      free(scalarSymbol);
    if (scalarCHID)
      free(scalarCHID);
  }
#ifdef DEBUG
  fprintf(FPINFO, "writing table\n");
  fflush(FPINFO);
#endif
  if (!SDDS_Terminate(&SDDS_table))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  table_written = 1;
  table_active = 0;
#ifdef DEBUG
  fprintf(FPINFO, "resetting variable values\n");
  fflush(FPINFO);
#endif
  reset_initial_variable_values(counter, counters, flags, pendIOtime);
#ifdef DEBUG
    fprintf(FPINFO, "making calls after reset initial variables.\n");
    fflush(FPINFO);
#endif
  make_cycle_calls(counter, counters, flags, AFTER_EXPERIMENT);
  free(counter);
  counter_global = NULL;
  counters_global = 0;
}

char *compose_filename(char *template, char *rootname) {
  char *ptr;
  if (!template && !rootname)
    SDDS_Bomb("can't compose output filename");
  if (!template)
    return (rootname);
  if (!rootname || !strstr(template, "%s"))
    return (template);
  ptr = tmalloc(sizeof(*ptr) * (strlen(rootname) + strlen(template) + 1));
  sprintf(ptr, template, rootname);
  return (ptr);
}

void set_variable_values(COUNTER *counter, long counters, long flags, long allowed_timeout_errors, long counter_rolled_over, double pendIOtime) {
  long i, j, k, ik, step, outputs, rk, rampsteps=1;
  double readback, ramp_delta=0;
  static long maxOutputs = 0;
  static int *errorCode = NULL;
  static char **outputName = NULL;

#if defined(DEBUG)
  fprintf(FPINFO, "setting values for %ld counters\n", counters);
  fflush(FPINFO);
#endif
  for (i = outputs = 0; i < counters; i++) {
    outputs += counter[i].Variables;
  }
  if (outputs > maxOutputs) {
    maxOutputs = outputs;
    if (!(errorCode = SDDS_Realloc(errorCode, sizeof(*errorCode) * outputs)) ||
        !(outputName = SDDS_Realloc(outputName, sizeof(*outputName) * outputs))) {
      fprintf(stderr, "Memory allocation failure (set_variable_values)\n");
      exit(1);
    }
  }
  for (i = j = 0; i < counters; i++) {
#if defined(DEBUG)
    fprintf(FPINFO, "Counter %ld has %ld variables\n", i, counter[i].Variables);
    fflush(FPINFO);
#endif
    for (k = 0; k < counter[i].Variables; k++) {
      outputName[j++] = counter[i].VariableData[k]->VariableSpec.control_name;
    }
  }
  
  for (i = 0; i < counters; i++) {
    for (j = 0; j < counter[i].Variables; j++) {
	if (!counter[i].VariableData[j]->VariableSpec.is_knob && counter[i].VariableData[j]->VariableSpec.readback_name) {
	  if (!counter[i].VariableData[j]->RBPVchid) {
	    if (ca_search(counter[i].VariableData[j]->VariableSpec.readback_name,
			  &counter[i].VariableData[j]->RBPVchid) != ECA_NORMAL) {
	      fprintf(stderr, "error: problem doing search1 for %s\n",
		      counter[i].VariableData[j]->VariableSpec.readback_name);
	      exit(1);
	    }
	  }
	}
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some variable channels1\n");
    for (i = 0; i < counters; i++) {
      for (j = 0; j < counter[i].Variables; j++) {
	fprintf(stderr, "readback name %s\n", counter[i].VariableData[j]->VariableSpec.readback_name);
	if (!counter[i].VariableData[j]->VariableSpec.is_knob &&
	    counter[i].VariableData[j]->VariableSpec.readback_name && ca_state(counter[i].VariableData[j]->RBPVchid) != cs_conn)
          fprintf(stderr, "%s not connected1\n", counter[i].VariableData[j]->VariableSpec.readback_name);
      }
    }
    exit(1);
  }
  for (i = 0; i < counters; i++) {
    long firstStep;
    if (!counter[i].Variables)
      continue;
#ifdef DEBUG
    fprintf(FPINFO, "About to take %ld substeps for counter %ld\n",
            counter[i].VariableData[0]->VariableSpec.substeps, i);
    fflush(FPINFO);
#endif
    if (counter[i].index == 0)
      firstStep = counter[i].VariableData[0]->VariableSpec.substeps - 1;
    else
      firstStep = 0;
    for (step = firstStep; step < counter[i].VariableData[0]->VariableSpec.substeps; step++) {
#ifdef DEBUG
      fprintf(FPINFO, "Taking substep %ld for %ld variables\n", step, counter[i].Variables);
      fflush(FPINFO);
#endif
      for (j = 0; j < counter[i].Variables; j++) {
#ifdef DEBUG
        fprintf(FPINFO, "   * variable %ld\n", j);
        fflush(FPINFO);
#endif
        if (counter[i].VariableData[j]->valueFromFile) {
          if (counter[i].index >= counter[i].VariableData[j]->valuesFromFile) {
            fprintf(stderr, "Error: counter %ld index (%ld) exceeds size of values array (%ld) for variable %s (file %s, column %s)\n",
                    i, counter[i].index, counter[i].VariableData[j]->valuesFromFile,
                    counter[i].VariableData[j]->VariableSpec.control_name,
                    counter[i].VariableData[j]->VariableSpec.values_file,
                    counter[i].VariableData[j]->VariableSpec.values_file_column);
            exit(1);
          }
          counter[i].VariableData[j]->value =
            counter[i].VariableData[j]->valueFromFile[counter[i].index];
        } else
          counter[i].VariableData[j]->value =
            counter[i].VariableData[j]->VariableSpec.initial_value +
            (counter[i].index - 1) * counter[i].VariableData[j]->step_size +
            (step + 1) * (counter[i].VariableData[j]->step_size / counter[i].substeps);
        if (counter[i].VariableData[j]->VariableSpec.function) {
          push_num(counter[i].VariableData[j]->original_value);
          push_num(counter[i].VariableData[j]->value);
          counter[i].VariableData[j]->value = rpn(counter[i].VariableData[j]->VariableSpec.function);
          rpn_clear();
        } else if (counter[i].VariableData[j]->VariableSpec.relative_to_original)
          counter[i].VariableData[j]->value +=
            counter[i].VariableData[j]->original_value;
        if (flags & FL_VERBOSE) {
          fprintf(FPINFO, "%s %s to %e\n",
                  flags & FL_DRY_RUN ? "Pretending to set " : "Setting ",
                  counter[i].VariableData[j]->VariableSpec.control_name,
                  counter[i].VariableData[j]->value);
          fflush(FPINFO);
        }
        if (!(flags & FL_DRY_RUN)) {
          if (counter[i].counter_changed || counter[i].first_set) {
	    if (counter[i].VariableData[j]->VariableSpec.delta_limit>0 && fabs(counter[i].VariableData[j]->value - counter[i].VariableData[j]->last_value)>counter[i].VariableData[j]->VariableSpec.delta_limit) {
	      rampsteps = round(fabs(counter[i].VariableData[j]->value-counter[i].VariableData[j]->last_value)/counter[i].VariableData[j]->VariableSpec.delta_limit+0.4999);
	      ramp_delta = (counter[i].VariableData[j]->value - counter[i].VariableData[j]->last_value)/rampsteps;
	    } else {
	      rampsteps=1;
	      ramp_delta=0;
	      counter[i].VariableData[j]->last_value = counter[i].VariableData[j]->value;
	    }
	    if (flags & FL_VERBOSE) {
	      if (ramp_delta!=0) {
		fprintf(FPINFO, "Doing ca put: %s getting value %e, previous value %e\n",
			counter[i].VariableData[j]->VariableSpec.control_name,
			counter[i].VariableData[j]->value, counter[i].VariableData[j]->last_value);
	      } else {
		fprintf(FPINFO, "Doing ca put: %s getting value %e\n",
			counter[i].VariableData[j]->VariableSpec.control_name,
			counter[i].VariableData[j]->value);
	      }
	    }
	    
	    for (rk=0; rk<rampsteps;rk++) {
	      counter[i].VariableData[j]->set_value = counter[i].VariableData[j]->last_value + (rk+1)*ramp_delta;
	      if (flags & FL_VERBOSE) {
		if (rampsteps>1) {
		  fprintf(FPINFO, "Doing ca put: %s to %e in ramp step %ld\n",
			  counter[i].VariableData[j]->VariableSpec.control_name,
			  counter[i].VariableData[j]->set_value, rk);
		} else {
		  fprintf(FPINFO, "Doing ca put: %s to %e\n",
			  counter[i].VariableData[j]->VariableSpec.control_name,
			  counter[i].VariableData[j]->set_value);
		}
	      }
	      if (counter[i].VariableData[j]->VariableSpec.is_knob) {
		/*setting knob pvs*/
		for (ik=0; ik<counter[i].VariableData[j]->knob.PVs; ik++) {
		  counter[i].VariableData[j]->knob.value[ik] = counter[i].VariableData[j]->knob.initial_value[ik] +
		    counter[i].VariableData[j]->knob.gain * counter[i].VariableData[j]->set_value * counter[i].VariableData[j]->knob.weight[ik];
		  if (counter[i].VariableData[j]->knob.lower_limit && counter[i].VariableData[j]->knob.value[ik]<counter[i].VariableData[j]->knob.lower_limit[ik]) {
#ifdef DEBUG
		    fprintf(FPINFO, "%s exceeds its lower limit, set it to the lower limit\n", counter[i].VariableData[j]->knob.ControlName[ik]);
#endif
		    counter[i].VariableData[j]->knob.value[ik] = counter[i].VariableData[j]->knob.lower_limit[ik];
		  }
		  if (counter[i].VariableData[j]->knob.upper_limit && counter[i].VariableData[j]->knob.value[ik]>counter[i].VariableData[j]->knob.upper_limit[ik]) {
#ifdef DEBUG
		    fprintf(FPINFO, "%s exceeds its upper limit, set it to the upper limit\n", counter[i].VariableData[j]->knob.ContorlName[ik]);
#endif
		    counter[i].VariableData[j]->knob.value[ik] = counter[i].VariableData[j]->knob.upper_limit[ik];
		  }
		  if (flags & FL_VERBOSE) {
		    fprintf(FPINFO, "Doing ca put for knob PVs: setting %s to %e\n",
			    counter[i].VariableData[j]->knob.ControlName[ik],
			    counter[i].VariableData[j]->knob.value[ik]);
		  }
		  
		  if (ca_state(counter[i].VariableData[j]->knob.PVchid[ik]) != cs_conn) {
		    fprintf(stderr, "Error trying to put value for %s\n",
			    counter[i].VariableData[j]->knob.ControlName[ik]);
		    exit(1);
		  }
		  if (ca_put(DBR_DOUBLE, counter[i].VariableData[j]->knob.PVchid[ik],
			     &counter[i].VariableData[j]->knob.value[ik]) != ECA_NORMAL) {
		    fprintf(stderr, "Error trying to put value for %s\n",
			    counter[i].VariableData[j]->knob.ControlName[ik]);
		    exit(1);
		  }
		}
#ifdef DEBUG
		fprintf(FPINFO, "ca put done\n");
		fflush(FPINFO);
#endif
	      } else {
		if (ca_state(counter[i].VariableData[j]->PVchid) != cs_conn) {
		  fprintf(stderr, "Error trying to put value for %s\n",
			  counter[i].VariableData[j]->VariableSpec.control_name);
		  exit(1);
		}
		if (ca_put(DBR_DOUBLE, counter[i].VariableData[j]->PVchid,
			 &counter[i].VariableData[j]->set_value) != ECA_NORMAL) {
		  fprintf(stderr, "Error trying to put value for %s\n",
			  counter[i].VariableData[j]->VariableSpec.control_name);
		  exit(1);
		}
	      }
	      if (ramp_delta!=0) {
		/*wait for pause time for ramping purpose*/
		if (flags & FL_VERBOSE) {
		  fprintf(FPINFO, "Waiting %f seconds after ramping values\n", counter[i].VariableData[j]->VariableSpec.delta_pause);
		  fflush(FPINFO);
		}
		oag_ca_pend_event(counter[i].VariableData[j]->VariableSpec.delta_pause, &sigint);
	      }
	    }
            if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
              fprintf(stderr, "Error trying to put value for %s\n",
                      counter[i].VariableData[j]->VariableSpec.control_name);
              exit(1);
            }
	    counter[i].VariableData[j]->last_value =   counter[i].VariableData[j]->value;
#ifdef DEBUG
            fprintf(FPINFO, "ca put done\n");
            fflush(FPINFO);
#endif
          }
	  
        }
      }
      /* need some substep pause here!
           */
      oag_ca_pend_event(counter[i].substep_pause, &sigint);
#ifdef DEBUG
      fprintf(FPINFO, "Substep completed\n");
      fflush(FPINFO);
#endif
    }
#ifdef DEBUG
    fprintf(FPINFO, "Done with substeps\n");
    fflush(FPINFO);
#endif
    for (j = 0; j < counter[i].Variables; j++) {
      if (counter[i].VariableData[j]->VariableSpec.is_knob) {
	check_knob_settings(&(counter[i].VariableData[j]->knob),
			    counter[i].VariableData[j]->VariableSpec.readback_tolerance,
			    counter[i].VariableData[j]->VariableSpec.readback_attempts, pendIOtime);
	continue;
      }
      if (counter[i].VariableData[j]->VariableSpec.readback_attempts > 0 &&
          counter[i].VariableData[j]->VariableSpec.readback_tolerance > 0) {
        for (k = 0; k < counter[i].VariableData[j]->VariableSpec.readback_attempts; k++) {
	   if (ca_get(DBR_DOUBLE,
		      counter[i].VariableData[j]->VariableSpec.readback_name ? counter[i].VariableData[j]->RBPVchid : counter[i].VariableData[j]->PVchid,
		      &readback) != ECA_NORMAL)
	     continue;
          if (ca_pend_io(pendIOtime) != ECA_NORMAL)
            continue;
	  if (within_tolerance(readback, counter[i].VariableData[j]->value,
                               counter[i].VariableData[j]->VariableSpec.readback_tolerance))
            break;
          oag_ca_pend_event(counter[i].VariableData[j]->VariableSpec.readback_pause, &sigint);
	}
        if (k == counter[i].VariableData[j]->VariableSpec.readback_attempts) {
	  fprintf(stderr, "Variable %s did not go to setpoint within specified tolerance in allotted time\n",
		  counter[i].VariableData[j]->VariableSpec.control_name);
	  fprintf(stderr, "Setpoint was %e, readback was %e\n",
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
    if (counter[i].substeps > 1 && counter[i].substep_pause > 0) {
      oag_ca_pend_event(counter[i].substep_pause, &sigint);
    }
  }
}

void reset_initial_variable_values(COUNTER *counter, long counters, long flags, double pendIOtime) {
  long i, j, k;

  if (flags & FL_VERYVERBOSE) {
    if (flags & FL_DRY_RUN)
      puts("Pretending to reset variables to original values.");
    else
      puts("Resetting variables to original values.");
    fflush(FPINFO);
  }
 
  addRampPV(NULL, 0.0, RAMPPV_CLEAR, NULL, pendIOtime);
  for (i = 0; i < counters; i++) {
    for (j = 0; j < counter[i].Variables; j++) {
      if (!counter[i].VariableData[j]->VariableSpec.reset_to_original)
        continue;
      if (counter[i].VariableData[j]->VariableSpec.is_knob) {
	for (k=0; k<counter[i].VariableData[j]->knob.PVs;k++) {
	  addRampPV(counter[i].VariableData[j]->knob.ControlName[k],
		    counter[i].VariableData[j]->knob.initial_value[k], 0,
		    counter[i].VariableData[j]->knob.PVchid[k], pendIOtime);
	}
      } else {
	addRampPV(counter[i].VariableData[j]->VariableSpec.control_name,
		  counter[i].VariableData[j]->original_value, 0,
		  counter[i].VariableData[j]->PVchid, pendIOtime);
      }
    }
  }
  if (!(flags & FL_DRY_RUN))
    doRampPVs(globalRampSteps, globalRampPause, 1, pendIOtime);
  else {
    if (flags & FL_VERYVERBOSE) {
      showRampPVs(FPINFO, pendIOtime);
      fflush(FPINFO);
    }
  }
}

void take_measurements(MEASUREMENT_DATA *Measurement, long Measurements, double intermeasurement_pause, long flags, long allowed_timeout_errors,
                       long allowed_limit_errors, double outlimit_pause, double pendIOtime, SYSTEMCALL_DATA *SystemCallData, long SystemCalls) {
  long i, limit_errors, j;
  double variance;
  double *readback;

  readback = tmalloc(sizeof(*readback) * Measurements);
  for (i = 0; i < Measurements; i++) {
    Measurement[i].sum = Measurement[i].sum_sqr = Measurement[i].number_of_values = 0;
    Measurement[i].minimum = DBL_MAX;
    Measurement[i].maximum = -DBL_MAX;
  }
  limit_errors = 0;
  if (flags & FL_VERBOSE) {
    fprintf(FPINFO, "Taking measurements...\n");
    fflush(FPINFO);
  }
  do {
    for (i = 0; i < Measurements; i++)
      if (Measurement[i].number_of_values < Measurement[i].MeasurementSpec.number_to_average)
        break;
    if (i == Measurements)
      break;

    for (j = 0; j < SystemCalls; j++) {
      if (!SystemCallData[j].SystemCallSpec.call_between_measuring)
        continue;
      /*call system command between measurement */
      if (flags & FL_VERBOSE) {
        fprintf(FPINFO, "%s command %s between measurements\n",
                flags & FL_DRY_RUN ? "Pretend to execute" : "Executing",
                SystemCallData[j].SystemCallSpec.command);
        fflush(FPINFO);
      }
      system(SystemCallData[j].SystemCallSpec.command);
      if (SystemCallData[j].SystemCallSpec.post_command_pause)
        oag_ca_pend_event(SystemCallData[j].SystemCallSpec.post_command_pause, &sigint);
    }
#if defined(NO_EPICS)
    for (i = 0; i < Measurements; i++) {
      readback[i] = 0;
    }
#else
    for (i = 0; i < Measurements; i++) {
      if (Measurement[i].number_of_values >= Measurement[i].MeasurementSpec.number_to_average)
        continue;
      if (!Measurement[i].PVchid) {
        if (ca_search(Measurement[i].MeasurementSpec.control_name,
                      &(Measurement[i].PVchid)) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search2 for %s\n", Measurement[i].MeasurementSpec.control_name);
          exit(1);
        }
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some measurement channels\n");
      for (i = 0; i < Measurements; i++) {
        if (ca_state(Measurement[i].PVchid) != cs_conn)
          fprintf(stderr, "%s not connected2\n", Measurement[i].MeasurementSpec.control_name);
      }
      exit(1);
    }
    for (i = 0; i < Measurements; i++) {
      if (Measurement[i].number_of_values >= Measurement[i].MeasurementSpec.number_to_average)
        continue;
      if (ca_state(Measurement[i].PVchid) != cs_conn) {
        fprintf(stderr, "error getting value of %s (no connection)--aborting\n", Measurement[i].MeasurementSpec.control_name);
        exit(1);
      }
      if (ca_get(DBR_DOUBLE, Measurement[i].PVchid, readback + i) != ECA_NORMAL) {
        fprintf(stderr, "error getting value of %s (ca_get error)--aborting\n", Measurement[i].MeasurementSpec.control_name);
        exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some measurement channels\n");
      exit(1);
    }
#endif
    for (i = 0; i < Measurements; i++) {
      if (Measurement[i].MeasurementSpec.lower_limit == Measurement[i].MeasurementSpec.upper_limit ||
          !Measurement[i].MeasurementSpec.limits_all)
        continue;
      if (!(Measurement[i].MeasurementSpec.lower_limit > readback[i] ||
            Measurement[i].MeasurementSpec.upper_limit < readback[i]))
        continue;
      if (limit_errors >= allowed_limit_errors) {
        fprintf(stderr, "error: %s value out of limit.\n", Measurement[i].MeasurementSpec.control_name);
        fprintf(stderr, "error2: too many limit errors\n");
        abort();
      }
      oag_ca_pend_event(outlimit_pause, &sigint);
      break;
    }
    if (i != Measurements)
      continue;
    for (i = 0; i < Measurements; i++) {
      if (Measurement[i].number_of_values >= Measurement[i].MeasurementSpec.number_to_average)
        continue;
      if (Measurement[i].MeasurementSpec.lower_limit != Measurement[i].MeasurementSpec.upper_limit &&
          (Measurement[i].MeasurementSpec.lower_limit > readback[i] ||
           Measurement[i].MeasurementSpec.upper_limit < readback[i])) {
        /*                i--; */
        limit_errors++;
        if (limit_errors >= allowed_limit_errors) {
          fprintf(stderr, "error: %s value out of limit.\n", Measurement[i].MeasurementSpec.control_name);
          fprintf(stderr, "error1: too many limit errors\n");
          abort();
        }
        oag_ca_pend_event(outlimit_pause, &sigint);
        continue;
      }
      /*
            if (dev_timeout_errors>=allowed_timeout_errors) {
            fprintf(stderr, "error: too many timeout errors\n"); 
            abort();
            }
          */
      Measurement[i].sum += readback[i];
      Measurement[i].sum_sqr += readback[i] * readback[i];
      if (readback[i] < Measurement[i].minimum)
        Measurement[i].minimum = readback[i];
      if (readback[i] > Measurement[i].maximum)
        Measurement[i].maximum = readback[i];
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
      fprintf(FPINFO, "  averaged measurement of %s for %s: %e +/- %e\n",
              Measurement[i].MeasurementSpec.column_name,
              Measurement[i].MeasurementSpec.control_name,
              Measurement[i].average_value,
              Measurement[i].standard_deviation / sqrt(1.0 * Measurement[i].number_of_values));
      fflush(FPINFO);
    }
  }
  free(readback);
}

long advance_counters(COUNTER *counter, long counters, long *counter_rolled_over) {
  long i;

  *counter_rolled_over = 0;
  for (i = 0; i < counters; i++) {
    counter[i].counter_changed = 0;
    counter[i].first_set = 0;
  }

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
  long i,k, rowcount;
  SDDS_TABLE SDDS_table;
  long iControlName = 0, iControlMessage = 0, iSymbolicName = 0, iControlUnits = 0;

  if (!SDDS_InitializeOutput(&SDDS_table, SDDS_ASCII, 1,
                             "Control quantities list from sddsexperiment",
                             "Control quantities list from sddsexperiment",
                             filename) ||
      (iControlName =
         SDDS_DefineColumn(&SDDS_table, "ControlName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0 ||
      (iControlMessage =
         SDDS_DefineColumn(&SDDS_table, "ControlMessage", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0 ||
      (iSymbolicName =
         SDDS_DefineColumn(&SDDS_table, "SymbolicName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0 ||
      (iControlUnits =
         SDDS_DefineColumn(&SDDS_table, "ControlUnits", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(&SDDS_table) || !SDDS_StartTable(&SDDS_table, Measurements + Variables))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  rowcount = 0;
  for (i = 0; i < Variables; i++) {
    if (Variable[i].VariableSpec.is_knob) {
      for (k=0; k<Variable[i].knob.PVs; k++) {
	if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowcount,
				iControlName, Variable[i].knob.ControlName[k],
				iControlMessage, Variable[i].VariableSpec.set_message,
				iSymbolicName, Variable[i].knob.ReadbackName ?  Variable[i].knob.ReadbackName[k] :  Variable[i].knob.ControlName[k],
				iControlUnits, Variable[i].knob.units[k], -1))
	   SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
    } else {
      if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowcount,
			     iControlName, Variable[i].VariableSpec.control_name,
			     iControlMessage, Variable[i].VariableSpec.set_message,
			     iSymbolicName, Variable[i].VariableSpec.column_name,
			     iControlUnits, Variable[i].VariableSpec.units, -1))
	SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    rowcount++;
  }
  for (i = 0; i < Measurements; i++) {
    if (!SDDS_SetRowValues(&SDDS_table, SDDS_SET_BY_INDEX | SDDS_PASS_BY_VALUE, rowcount,
                           iControlName, Measurement[i].MeasurementSpec.control_name,
                           iControlMessage, Measurement[i].MeasurementSpec.read_message,
                           iSymbolicName, Measurement[i].MeasurementSpec.column_name,
                           iControlUnits, Measurement[i].MeasurementSpec.units, -1))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    rowcount++;
  }
  if (!SDDS_UpdatePage(&SDDS_table, 0) || !SDDS_Terminate(&SDDS_table))
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
    if (!SDDS_UpdatePage(&SDDS_table, 0) || !SDDS_Terminate(&SDDS_table))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (ca) {
    if (counter_global && counters_global)
      reset_initial_variable_values(counter_global, counters_global, FL_VERYVERBOSE, 10.0);
    ca_task_exit();
  }
}

long within_tolerance(double value1, double value2, double tolerance) {
  if (fabs(value1 - value2) <= tolerance)
    return (1);
  return (0);
}

void make_cycle_calls(COUNTER *counter, long counters, long flags, long before_or_after) {
  long i, j;
  static char buffer[1024];
  OUTPUT_COLUMN output_column;

  for (i = counters - 1; i >= 0; i--) {
    for (j = 0; j < counter->SystemCalls; j++) {
      if ((before_or_after & BEFORE_CYCLE && counter->SystemCallData[j]->SystemCallSpec.call_before_cycle) ||
          (before_or_after & AFTER_CYCLE && counter->SystemCallData[j]->SystemCallSpec.call_after_cycle) ||
	  (before_or_after & AFTER_EXPERIMENT && counter->SystemCallData[j]->SystemCallSpec.call_after_experiment)) {
        oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.pre_command_pause, &sigint);
        if (flags & FL_VERBOSE) {
          if (before_or_after & BEFORE_CYCLE)
            fprintf(FPINFO, "%s command %s\n",
                    flags & FL_DRY_RUN ? "Pretending to execute before cycle" : "Executing",
                    counter->SystemCallData[j]->SystemCallSpec.command);
          if (before_or_after & AFTER_CYCLE)
            fprintf(FPINFO, "%s command %s\n",
                    flags & FL_DRY_RUN ? "Pretending to execute after cycle" : "Executing",
                    counter->SystemCallData[j]->SystemCallSpec.command);
	   if (before_or_after & AFTER_EXPERIMENT)
            fprintf(FPINFO, "%s command %s\n",
                    flags & FL_DRY_RUN ? "Pretending to execute after cycle" : "Executing",
                    counter->SystemCallData[j]->SystemCallSpec.command);
	  
          fflush(FPINFO);
        }
        if (!(flags & FL_DRY_RUN)) {
          output_column = counter->Output_Column[j];
          if (!make_system_call(&output_column,
                                counter->SystemCallData[j]->SystemCallSpec.command,
                                counter->SystemCallData[j]->SystemCallSpec.append_counter ? counter->SystemCallData[j]->cycle_call_number : -1,
                                counter->SystemCallData[j]->SystemCallSpec.counter_format,
                                buffer, 1024))
            SDDS_Bomb("system call failed");
          counter->Output_Column[j] = output_column;
        }
        counter->SystemCallData[j]->cycle_call_number++;
        oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.post_command_pause, &sigint);
      }
    }
  }
}

void make_calls_for_counters(COUNTER *counter, long counters, long rolled, long flags, long before_or_after) {
  long i;
  for (i = counters - 1; i >= 0; i--)
    if (counter[i].counter_changed || rolled)
      make_calls_for_counter(counter + i, flags, before_or_after);
}

void make_calls_for_counter(COUNTER *counter, long flags, long before_or_after) {
  long j;
  static char buffer[1024];
  OUTPUT_COLUMN output_column;

  for (j = 0; j < counter->SystemCalls; j++) {
    if (counter->SystemCallData[j]->SystemCallSpec.call_before_cycle || counter->SystemCallData[j]->SystemCallSpec.call_after_cycle)
      continue;
    if (((before_or_after & AFTER_SETTING) && counter->SystemCallData[j]->SystemCallSpec.call_before_setting) ||
        ((before_or_after & BEFORE_SETTING) && !counter->SystemCallData[j]->SystemCallSpec.call_before_setting) ||
        ((before_or_after & AFTER_MEASURING) && counter->SystemCallData[j]->SystemCallSpec.call_before_measuring) ||
        ((before_or_after & BEFORE_MEASURING) && !counter->SystemCallData[j]->SystemCallSpec.call_before_measuring))
      continue;

    oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.pre_command_pause, &sigint);
    if (flags & FL_VERBOSE) {
      fprintf(FPINFO, "%s command %s\n",
              flags & FL_DRY_RUN ? "Pretending to execute" : "Executing",
              counter->SystemCallData[j]->SystemCallSpec.command);
      fflush(FPINFO);
    }
    if (!(flags & FL_DRY_RUN)) {
      output_column = counter->Output_Column[j];
      if (!make_system_call(&output_column,
                            counter->SystemCallData[j]->SystemCallSpec.command,
                            counter->SystemCallData[j]->SystemCallSpec.append_counter ? counter->SystemCallData[j]->call_number : -1,
                            counter->SystemCallData[j]->SystemCallSpec.counter_format,
                            buffer, 1024))
        SDDS_Bomb("system call failed");

      counter->Output_Column[j] = output_column;
    }

    counter->SystemCallData[j]->call_number++;
    oag_ca_pend_event(counter->SystemCallData[j]->SystemCallSpec.post_command_pause, &sigint);
  }
}

long make_system_call(OUTPUT_COLUMN *output_column, char *command, long call_number, char *counter_format,
                      char *buffer, long buffer_length) {
  char format[1024];
  static char *buffer1 = NULL;
  FILE *handle;
  char message[1024];
  char *message1;
  long status, line;

  buffer1 = SDDS_Realloc(buffer1, sizeof(*buffer1) * buffer_length);
#if defined(DEBUG)
  fprintf(FPINFO, "make_system_call: command=%s, call_number=%ld, counter_format=%s, buffer_length = %ld\n",
          command, call_number, counter_format, buffer_length);
  fflush(FPINFO);
#endif
  if (call_number >= 0) {
#if defined(DEBUG)
    fprintf(FPINFO, "composing system call command\n");
    fflush(FPINFO);
#endif
    strcpy(format, "%s");
    strcat(format, counter_format);
#if defined(DEBUG)
    fprintf(FPINFO, "format string is >%s<\n", format);
    fflush(FPINFO);
#endif
    sprintf(buffer1, format, command, call_number);
  } else
    strcpy(buffer1, command);
#if defined(DEBUG)
  fprintf(FPINFO, "command result is >%s<\n", buffer1);
  fflush(FPINFO);
#endif
  replace_string(buffer, buffer1, "%s", outputRootname);
#if defined(DEBUG)
  fprintf(FPINFO, "final command result is >%s<\n", buffer);
  fflush(FPINFO);
#endif

  if (!SDDS_StringIsBlank(buffer)) {
    if (!output_column->columns) {
#ifdef DEBUG
      fprintf(FPINFO, "doing system call\n");
      fflush(FPINFO);
#endif
      system(buffer);
    } else {
#ifdef DEBUG
      fprintf(FPINFO, "doing popen call\n");
      fflush(FPINFO);
#endif
      message1 = NULL;
      line = 0;
      output_column->value = NULL;
      handle = popen(buffer, "r");
      if (handle == NULL) {
        SDDS_Bomb("popen call failed");
        return (0);
      }
      if (feof(handle)) {
        SDDS_Bomb("no data returned from popen call");
        return (0);
      }

      while (fgets(message, sizeof(message), handle) != NULL) {
        message1 = strtok(message, "\n");
        output_column->value = trealloc(output_column->value, sizeof(*(output_column->value)) * (line + 1));
        output_column->value[line] = NULL;
        output_column->value[line] = malloc(sizeof(output_column->value[line]) * (strlen(message1) + 1));
        sprintf(output_column->value[line], "%s", message1);

        line++;
      }

      status = pclose(handle);
      if (status == -1) {
        SDDS_Bomb("pclose call failed");
        return (0);
      }
    }
  }

#if defined(DEBUG)
  fprintf(FPINFO, "system/popen call made\n");
  fflush(FPINFO);
#endif
  return (1);
}

void setupForInitialRamping(COUNTER *counter, long counters, unsigned flags, double pendIOtime) {
  long i, j, k;
  double value, value1;

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
      if (counter[i].VariableData[j]->VariableSpec.is_knob) {
	for (k=0; k<counter[i].VariableData[j]->knob.PVs;k++) {
	  value1 = value * counter[i].VariableData[j]->knob.gain * counter[i].VariableData[j]->knob.weight[k];
	  if (counter[i].VariableData[j]->VariableSpec.relative_to_original)
	    value1 += counter[i].VariableData[j]->knob.initial_value[k];
	  addRampPV(counter[i].VariableData[j]->knob.ControlName[k],
		    value1, 0, counter[i].VariableData[j]->knob.PVchid[k], pendIOtime);
	}
      } else {
	addRampPV(counter[i].VariableData[j]->VariableSpec.control_name,
		  value, 0, counter[i].VariableData[j]->PVchid, pendIOtime);
      }
    }
  }
}

MEASUREMENT_DATA *add_file_measurement(MEASUREMENT_DATA *MeasurementData, long *Measurements,
                                       MEASUREMENTFILESPEC *MeasurementFileSpec, double pendIOtime) {
  char **ControlName, **ColumnName, *IncludeStDev, *IncludeSigma, *LimitsAll, **Units;
  double *LowerLimit, *UpperLimit;
  long *NumberToAverage, measurements;
  MEASUREMENTSPEC *NewMeasurement;
  long i, j, ca=0, type, prevMeasurements;
  short *caList=NULL, ReadbackUnitsColumnFound;
  chid *PVchid;
  INFOBUF *info = NULL;

  if (!MeasurementFileSpec->filename)
    SDDS_Bomb("No filename for measurement_file");

  if ((measurements = readMeasurementFile(MeasurementFileSpec->filename, &ControlName,
                                          &ColumnName, &IncludeStDev,
                                          &IncludeSigma, &LowerLimit,
                                          &UpperLimit, &LimitsAll,
                                          &NumberToAverage, &Units, &PVchid,
                                          pendIOtime, &ReadbackUnitsColumnFound)) != 0) {
    /*
        Units = tmalloc(sizeof(*Units)*measurements);
        if (!getUnits(ControlName, Units, measurements))
        SDDS_Bomb("Unable to get units for measurements in file");
      */
    prevMeasurements = *Measurements;
    for (i = 0; i < measurements; i++) {
      NewMeasurement = tmalloc(sizeof(*NewMeasurement));
      NewMeasurement->control_name = ControlName[i];
      NewMeasurement->read_message = NewMeasurement->symbol = NewMeasurement->units = NULL;
      NewMeasurement->column_name = ColumnName[i];
      NewMeasurement->number_to_average = NumberToAverage ? NumberToAverage[i] : MeasurementFileSpec->number_to_average;
      NewMeasurement->include_standard_deviation = IncludeStDev ? (IncludeStDev[i] == 'y' ? 1 : 0) : MeasurementFileSpec->include_standard_deviation;
      NewMeasurement->include_sigma = IncludeSigma ? (IncludeSigma[i] == 'y' ? 1 : 0) : MeasurementFileSpec->include_sigma;
      NewMeasurement->lower_limit = MeasurementFileSpec->lower_limit;
      NewMeasurement->upper_limit = MeasurementFileSpec->upper_limit;
      if (LowerLimit)
        NewMeasurement->lower_limit = LowerLimit[i];
      if (UpperLimit)
        NewMeasurement->upper_limit = UpperLimit[i];
      NewMeasurement->limits_all = LimitsAll ? (LimitsAll[i] == 'y' ? 1 : 0) : MeasurementFileSpec->limits_all;
      NewMeasurement->units = Units[i];
      MeasurementData = add_measurement(MeasurementData, *Measurements, NewMeasurement, pendIOtime, 0);
      *Measurements += 1;
    }

    /* ReadbackUnits column exists but may have blank data that we can try to fix */
    if (ReadbackUnitsColumnFound) {
      for (i = 0; i < measurements; i++) {
        if (SDDS_StringIsBlank(MeasurementData[prevMeasurements + i].MeasurementSpec.units)) {
          if (ca == 0) {
            caList = calloc(measurements, sizeof(short));
          }
          caList[i] = 1;
          ca++;
          if (ca_search(MeasurementData[prevMeasurements + i].MeasurementSpec.control_name, &(MeasurementData[prevMeasurements + i].PVchid)) != ECA_NORMAL) {
            fprintf(stderr, "error: problem doing search for %s\n", MeasurementData[prevMeasurements + i].MeasurementSpec.control_name);
            exit(1);
          }
        }
      }
      if (ca) {
        if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for some channels\n");
          for (i = 0; i < measurements; i++) {
            if (caList[i]) {
              if (ca_state(MeasurementData[prevMeasurements + i].PVchid) != cs_conn)
                fprintf(stderr, "%s not connected\n", MeasurementData[prevMeasurements + i].MeasurementSpec.control_name);
            }
          }
        }
        info = (INFOBUF *)malloc(sizeof(INFOBUF) * (ca));
        j = -1;
        
        for (i = 0; i < measurements; i++) {
          if (caList[i]) {
            j++;
            if (ca_state(MeasurementData[prevMeasurements + i].PVchid) == cs_conn) {
              type = ca_field_type(MeasurementData[prevMeasurements + i].PVchid);
              if ((type == DBF_STRING) || (type == DBF_ENUM))
                continue;
              if (ca_array_get(dbf_type_to_DBR_CTRL(type), 1,
                               MeasurementData[prevMeasurements + i].PVchid, &info[j]) != ECA_NORMAL) {
                fprintf(stderr, "error: unable to read units.\n");
                exit(1);
              }
            }
          }
        }
        if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
          fprintf(stderr, "error: problem doing search for some channels\n");
        }
        j=-1;
        for (i = 0; i < measurements; i++) {
          if (caList[i]) {
            j++;
            if (ca_state(MeasurementData[prevMeasurements + i].PVchid) == cs_conn) {
              type = ca_field_type(MeasurementData[prevMeasurements + i].PVchid);
              switch (type) {
              case DBF_CHAR:
                free(MeasurementData[prevMeasurements + i].MeasurementSpec.units);
                SDDS_CopyString(&(MeasurementData[prevMeasurements + i].MeasurementSpec.units), info[j].c.units);
                break;
              case DBF_SHORT:
                free(MeasurementData[prevMeasurements + i].MeasurementSpec.units);
                SDDS_CopyString(&(MeasurementData[prevMeasurements + i].MeasurementSpec.units), info[j].i.units);
                break;
              case DBF_LONG:
                free(MeasurementData[prevMeasurements + i].MeasurementSpec.units);
                SDDS_CopyString(&(MeasurementData[prevMeasurements + i].MeasurementSpec.units), info[j].l.units);
                break;
              case DBF_FLOAT:
                free(MeasurementData[prevMeasurements + i].MeasurementSpec.units);
                SDDS_CopyString(&(MeasurementData[prevMeasurements + i].MeasurementSpec.units), info[j].f.units);
                break;
              case DBF_DOUBLE:
                free(MeasurementData[prevMeasurements + i].MeasurementSpec.units);
                SDDS_CopyString(&(MeasurementData[prevMeasurements + i].MeasurementSpec.units), info[j].d.units);
                break;
              default:
                break;
                
              }
            }
          }
        }
        if (info)
          free(info);
        if (caList)
          free(caList);
      }
    }    
  }
  return MeasurementData;
}

long readMeasurementFile(char *MeasurementFile, char ***ControlName, char ***ColumnName,
                         char **IncludeStDev, char **IncludeSigma, double **LowerLimit, double **UpperLimit,
                         char **LimitsAll, long **NumberToAverage, char ***ReadbackUnits, chid **PVchid,
                         double pendIOtime, short *ReadbackUnitsColumnFound) {
  SDDS_TABLE inTable;
  long code = 0, names = 0, i;

  if (!SDDS_InitializeInput(&inTable, MeasurementFile) || (code = SDDS_ReadTable(&inTable)) < 0)
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (code == 0 || (names = SDDS_CountRowsOfInterest(&inTable)) < 1)
    SDDS_Bomb("no data in measurement file");
  if (!((*ControlName) = SDDS_GetColumn(&inTable, "ControlName"))) {
    fprintf(stderr, "error: required column \"ControlName\" not found in file %s\n", MeasurementFile);
    exit(1);
  }

  *ColumnName = NULL;
  *IncludeStDev = *IncludeSigma = NULL;
  *LowerLimit = *UpperLimit = NULL;
  *NumberToAverage = NULL;
  *LimitsAll = NULL;
  *ReadbackUnits = NULL;
  *PVchid = NULL;

  if ((code = SDDS_CheckColumn(&inTable, "ReadbackName", NULL, SDDS_STRING, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "ReadbackName", NULL, SDDS_STRING, stderr);
      exit(1);
    }
    if (!(*ColumnName = SDDS_GetColumn(&inTable, "ControlName")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else if (!((*ColumnName) = SDDS_GetColumn(&inTable, "ReadbackName")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((code = SDDS_CheckColumn(&inTable, "LimitsAll", NULL, SDDS_CHARACTER, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "LimitsAll", NULL, SDDS_CHARACTER, stderr);
      exit(1);
    }
  } else if (!((*LimitsAll) = SDDS_GetColumn(&inTable, "LimitsAll")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  *PVchid = tmalloc(sizeof(chid) * names);
  for (i = 0; i < names; i++) {
    (*PVchid)[i] = NULL;
    if (ca_search((*ControlName)[i], &((*PVchid)[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search3 for %s\n", (*ControlName)[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search4 for some channels2\n");
    for (i = 0; i < names; i++) {
      fprintf(stderr, "%s \n",  (*ControlName)[i]);
      if (ca_state((*PVchid)[i]) != cs_conn)
        fprintf(stderr, "%s not connected3\n", (*ControlName)[i]);
    }
    exit(1);
  }
  *ReadbackUnitsColumnFound=1;
  if ((code = SDDS_CheckColumn(&inTable, "ReadbackUnits", NULL, SDDS_STRING, NULL)) != 0) {
    if (code != SDDS_CHECK_NONEXISTENT) {
      SDDS_CheckColumn(&inTable, "ReadbackUnits", NULL, SDDS_STRING, stderr);
      exit(1);
    } else {
      /* No ReadbackUnits column, try to get the values through CA calls */
      *ReadbackUnitsColumnFound=0;
      *ReadbackUnits = tmalloc(sizeof(ReadbackUnits) * names);
      if (!getUnits(*ControlName, *ReadbackUnits, names, *PVchid, pendIOtime))
        SDDS_Bomb("Unable to get units for measurements in file");
    }
  } else if (!((*ReadbackUnits) = SDDS_GetColumn(&inTable, "ReadbackUnits")))
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

  if (SDDS_ReadPage(&inTable) > 1) {
    fprintf(FPINFO, "warning: file %s has multiple pages--only the first is used!\n",
            MeasurementFile);
    fflush(FPINFO);
  }
  if (!SDDS_Terminate(&inTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  return names;
}

void read_knob_file(VARIABLE_DATA *variable, VARIABLESPEC *NewVariable, double pendIOtime) {
   
  SDDS_TABLE knobTable;
  char *knobName=NULL;
  long found=0, gainIndex=-1, lowerIndex=-1, upperIndex=-1, readbackIndex=-1, i;
  if (!SDDS_InitializeInput(&knobTable, NewVariable->knob_file_name)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  variable->knob.gain=1.0;
  gainIndex = SDDS_GetParameterIndex(&knobTable, "Gain");
  lowerIndex = SDDS_GetColumnIndex(&knobTable, "LowerLimit");
  upperIndex = SDDS_GetColumnIndex(&knobTable, "UpperLimit");
  readbackIndex = SDDS_GetColumnIndex(&knobTable, "ReadbackName");
  variable->knob.lower_limit = NULL;
  variable->knob.upper_limit = NULL;
  variable->knob.ReadbackName = NULL;
  variable->knob.RBPVchid=NULL;
  variable->knob.units=NULL;
  while (SDDS_ReadTable(&knobTable)>0) {
    if (!SDDS_GetParameter(&knobTable, "ControlName", &knobName))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (strcmp(NewVariable->control_name, knobName)!=0) {
      continue;
    }
    found=1;
    if ((variable->knob.PVs = SDDS_CountRowsOfInterest(&knobTable)) < 1) {
      fprintf(stderr, "error: no data in knob file.");
      exit(1);
    }
    variable->knob.ControlName=(char **)SDDS_GetColumn(&knobTable, "ControlName");
    variable->knob.weight = SDDS_GetColumnInDoubles(&knobTable, "Weight");
    if (lowerIndex>=0 && !(variable->knob.lower_limit = SDDS_GetColumnInDoubles(&knobTable,"LowerLimit")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (upperIndex>=0 && !(variable->knob.upper_limit = SDDS_GetColumnInDoubles(&knobTable, "UpperLimit")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (gainIndex>=0 && !SDDS_GetParameter(&knobTable, "Gain", &variable->knob.gain))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (readbackIndex>=0 && !(variable->knob.ReadbackName=(char **)SDDS_GetColumn(&knobTable, "ReadbackName")))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    variable->knob.PVchid = malloc(sizeof(chid)*variable->knob.PVs);
    variable->knob.initial_value = malloc(sizeof(double)*variable->knob.PVs);
    variable->knob.value = malloc(sizeof(double) * variable->knob.PVs);
    variable->knob.RBPVchid = malloc(sizeof(chid)*variable->knob.PVs);
    variable->knob.units = malloc(sizeof(*variable->knob.units)*variable->knob.PVs);
    for (i=0; i< variable->knob.PVs; i++) {
      if (ca_search(variable->knob.ControlName[i], &(variable->knob.PVchid[i])) != ECA_NORMAL) {
	fprintf(stderr, "error: problem doing search5 for %s\n",variable->knob.ControlName[i] );
	exit(1);
      }
      if (readbackIndex>=0 && ca_search(variable->knob.ReadbackName[i], &(variable->knob.RBPVchid[i])) != ECA_NORMAL) {
	fprintf(stderr, "error: problem doing search6 for %s\n",variable->knob.ReadbackName[i] );
	exit(1);
      }
      getUnits(&(variable->knob.ControlName[i]), &(variable->knob.units[i]), 1, &(variable->knob.PVchid[i]), pendIOtime);
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels in knob %s\n", NewVariable->control_name);
      exit(1);
    }
    for (i=0; i<variable->knob.PVs; i++) {
      if (ca_get(DBR_DOUBLE, variable->knob.PVchid[i], &(variable->knob.initial_value[i])) != ECA_NORMAL) {
	fprintf(stderr, "error getting setpoint of %s (ca_get error)--aborting\n", NewVariable->control_name);
	exit(1);
      }
    }
    if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
      fprintf(stderr, "error: problem reading some channels in knob %s\n", NewVariable->control_name);
      exit(1);
    }
    found=1;
    break;
  }
  if (!SDDS_Terminate(&knobTable))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!found) {
    fprintf(stderr,"Error knob %s not found in knob file %s.\n", NewVariable->control_name, NewVariable->knob_file_name);
    exit(1);
  }
}

void check_knob_settings(KNOB *knob,  double tolerance, long attempts, double pendIOtime) {
  double *readback=NULL;
  long i, k, done;
  if (!knob->ReadbackName)
    return;
  readback = malloc(sizeof(double)*knob->PVs);
  for (k=0; k<attempts; k++)  {
    done = 1;
    for (i=0; i<knob->PVs; i++) {
      if (ca_get(DBR_DOUBLE, knob->RBPVchid[i], &readback[i])!=ECA_NORMAL) {
	done=0;
	break;
      }
      if (ca_pend_io(pendIOtime) != ECA_NORMAL) {
	done=0;
	break;
      }
      if (!within_tolerance(readback[i], knob->value[i], tolerance)) {
	done=0;
	break;
      }
    }
    if (done)
      break;
  }
  if (k==attempts) {
    fprintf(stderr, "Some PVs in knob %s did not go to setpoint within specified tolerance in allotted time\n", knob->knobName);
    for (i=0; i<knob->PVs; i++) {
      if (!within_tolerance(readback[i], knob->value[i], tolerance)) {
	fprintf(stderr, "PV %s setpoint was %e, readback was %e \n", knob->ControlName[i],  knob->value[i], readback[i]);
      }
    }
    free(readback);
    exit(1);
  }
}
