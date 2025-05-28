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
 Revision 1.9  2002/11/01 21:03:59  soliday
 sddsvexperiment no longer uses ezca.

 Revision 1.8  2002/08/14 20:00:37  soliday
 Added Open License

 Revision 1.7  1997/09/26 21:51:18  borland
 Changed the way counter appending to system_calls is done.  Now uses
 default string of  " %ld" and *does not* pad the users command string.

# Revision 1.6  1996/05/01  14:12:19  borland
# Added exec_command namelist command (immediate command execution).
#
# Revision 1.5  1996/03/07  01:15:54  borland
# Changed default for units for variables from "unknown" to NULL.
#
# Revision 1.4  1996/02/10  06:29:48  borland
# Added feature to variable command to support setting from a column of
# values in an SDDS file.  Converted Time parameter/column from elapsed time to
# time-since-epoch; added new ElapsedTime parameter/column.
#
# Revision 1.3  1996/01/04  00:22:41  borland
# Added ganged ramping to experiment programs.
#
# Revision 1.2  1995/11/09  03:22:44  borland
# Added copyright message to source files.
#
# Revision 1.1  1995/09/25  20:15:52  saunders
# First release of SDDSepics applications.
#
 */
#include "namelist.h"

#namelist variable struct/VariableSpec
        STRING PV_name = NULL;
        STRING parameter_name = NULL;
        STRING readback_name = NULL;
        STRING symbol = NULL;
        STRING units = NULL;
        STRING function = NULL;
        STRING values_file = NULL;
        STRING values_file_column = NULL;
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
        chid PVchid = NULL;
#end                

#namelist execute struct/ExecuteSpec
        double post_change_pause = 0;
        double intermeasurement_pause = 0;
        double rollover_pause = 0;
        long post_change_key_wait = 0;
        long allowed_timeout_errors = 1;
        long allowed_limit_errors = 1;
        double outlimit_pause = 0.1;
        double ramp_pause = 0.25;
        long ramp_steps = 10;
#end

#namelist erase struct/EraseSpec
        long variable_definitions = 1;
        long measurement_definitions = 1;
#end

#namelist suffix_list struct/SuffixList
        STRING filename = NULL;
#end

#namelist rootname_list struct/RootnameList
        STRING filename = NULL;
#end

#namelist list_control_quantities struct/ListControlSpec
        STRING filename = NULL;
#end

#namelist system_call struct/SystemCallSpec
        STRING command = NULL;
        long index_number = 0;
        long index_limit = 0;
        double post_command_pause = 0;
        double pre_command_pause = 0;
        long append_counter = 0;
        STRING counter_format = " %ld";
        long call_before_setting = 0;
        long call_before_measuring = 1;
        STRING counter_parameter_name = NULL;
#end 

#namelist exec_command struct/ExecCommandSpec
        STRING command = NULL;
#end

