/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* file: sddsexperiment.nl
 * purpose: namelist definitions for sddsexperiment.c
 *
 * Michael Borland, 1993 
 $Log: not supported by cvs2svn $
 Revision 1.1  2003/08/27 19:51:12  soliday
 Moved into subdirectory

 Revision 1.16  2003/02/24 22:24:08  borland
 Found simple experiments did not work on Linux.  Problem appeared to
 be related to having the CHID variables in the input namelist structures.
 I moved the CHID variables to the VARIBLE_DATA and MEASUREMENT_DATA
 structures.  I also no longer expect a NULL chid to indicate an
 unconnected PV.  This seemed unnecessary since we only try to connect
 to each variable or measurement once.

 Revision 1.15  2002/12/11 01:19:45  shang
 added pre_experiment and post_experiment to execute namelist

 Revision 1.14  2002/11/14 22:28:07  soliday
 Added sddsbcontrol and fixed a problem with sddsexperiment

 Revision 1.13  2002/10/31 20:43:50  soliday
 sddsexperiment no longer uses ezca.

 Revision 1.12  2002/08/14 20:00:34  soliday
 Added Open License

 Revision 1.11  2000/11/15 07:41:32  emery
 Added flag skip_first_pause in execute command.
 This allows immediate measurement when the first data
 points doesn't require any change to the PVs.

 Revision 1.10  2000/07/18 19:57:15  borland
 Changes by D. Blachowicz:  Added to system_call command ability to scan
 values from stdout of the UNIX command and put them in the main sdds output
 file.

 Revision 1.9  1997/02/25 18:17:01  borland
 Changed default counter format for system calls to compensate another
 change in the code.

# Revision 1.8  1996/05/01  14:12:12  borland
# Added exec_command namelist command (immediate command execution).
#
# Revision 1.7  1996/03/07  00:36:54  borland
# Changed default units for measurements and variables from "unknown" to
# blank string.
#
# Revision 1.6  1996/02/10  06:29:47  borland
# Added feature to variable command to support setting from a column of
# values in an SDDS file.  Converted Time parameter/column from elapsed time to
# time-since-epoch; added new ElapsedTime parameter/column.
#
# Revision 1.5  1996/02/07  18:50:45  borland
# Added capability to load measurement requests from sddsmonitor input files.
#
# Revision 1.4  1996/01/04  00:22:36  borland
# Added ganged ramping to experiment programs.
#
# Revision 1.3  1995/12/03  01:31:39  borland
# Added ramping feature as per sddsvexperiment; removed support for devices;
# added fflush(stdout) calls after informational printouts.
#
# Revision 1.2  1995/11/09  03:22:36  borland
# Added copyright message to source files.
#
# Revision 1.1  1995/09/25  20:15:38  saunders
# First release of SDDSepics applications.
#
 */
#include "namelist.h"

#namelist variable struct/VariableSpec
        STRING control_name = NULL;
        STRING readback_name = NULL;
        STRING set_message = NULL;
        STRING readback_message = NULL;
        STRING column_name = NULL;
        STRING symbol = NULL;
        STRING units = NULL;
        STRING function = NULL;
        STRING values_file = NULL;
        STRING values_file_column = NULL;
	STRING knob_file_name = NULL;
	long is_knob = 0;
        long index_number = 0;
        long index_limit = 0;
        long substeps = 1;
        long relative_to_original = 0;
        double substep_pause = 0;
        double initial_value = 0;
        double final_value = 0;
        double range_multiplier = 1;
        double readback_tolerance = 0;
        double readback_pause = 0.1;
        long readback_attempts = 10;
        long reset_to_original = 1;
	double delta_limit = 0;
	double delta_pause = 1;
#end                

#namelist measurement_file struct/MeasurementFileSpec
        STRING filename = NULL;
        long number_to_average = 1;
        long include_standard_deviation = 0;
        long include_sigma = 0;
        long include_maximum = 0;
        long include_minimum = 0;
        double lower_limit = 0;
        double upper_limit = 0;
        long limits_all = 0;;
#end

#namelist measurement struct/MeasurementSpec
        STRING control_name = NULL;
        STRING read_message = NULL;
        STRING column_name = NULL;
        STRING symbol = NULL;
        STRING units = NULL;
        long number_to_average = 1;
        long include_standard_deviation = 0;
        long include_sigma = 0;
        long include_maximum = 0;
        long include_minimum = 0;
        double lower_limit = 0;
        double upper_limit = 0;
        long limits_all = 0;;
#end

#namelist execute struct/ExecuteSpec
        STRING outputfile = NULL;
        long measure_before_first_change = 0;
        double post_change_pause = 0;
        long skip_first_pause = 0;
        double intermeasurement_pause = 0;
        double rollover_pause = 0;
        long post_change_key_wait = 0;
        long allowed_timeout_errors = 1;
        long allowed_limit_errors = 1;
        double outlimit_pause = 0.1;
        long repeat_readings = 1;
        double post_reading_pause = 0.1;
        double ramp_pause = 0.25;
        long ramp_steps = 10;
	long cycles = 1;
        STRING pre_experiment = NULL;
        STRING post_experiment = NULL;
#end

#namelist erase struct/EraseSpec
        long variable_definitions = 1;
        long measurement_definitions = 1;
#end

#namelist list_control_quantities struct/ListControlSpec
        STRING filename = NULL;
#end

#namelist system_call struct/SystemCallSpec
        STRING command = NULL;
        STRING output_column_list = NULL;
        STRING output_type_list = NULL;
        long index_number = 0;
        long index_limit = 0;
        double post_command_pause = 0;
        double pre_command_pause = 0;
        long append_counter = 0;
        STRING counter_format = " %ld";
        long call_before_setting = 0;
        long call_before_measuring = 1;
        long call_between_measuring = 0;
        long call_before_cycle = 0;
	long call_after_cycle = 0;
	long call_after_experiment = 0;
        STRING counter_column_name = NULL;
#end 

#namelist exec_command struct/ExecCommandSpec
        STRING command = NULL;
#end


