/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program sddscontrollaw2.c
 * reads a sddsfile with one table.
 * The names of columns of doubles are the system readouts (x)
 * the row labels are the names of the actuators (u).
 * In ascii mode the table looks like the gain matrix K in 
 * equation u = - K x or delta_u = - K x.
 * the control law regulates the system readouts to zero or
 * to present values.
 * Add additional submatrix for doing feedforward on
 * other actuators:
 * delta_u = K1 x
 * where K1 might be designed to create the negative correction
 * executed by a parallel orbit correction system, such as the real-time orbit
 * feedback system at APS. The K1 x quantity may be optionally passed through
 * a time filter specified in the file or at the command line.
 * The K and K1 matrices may be packaged in the same input matrix SDDS file,
 * with the rows of the K1 part distinguished from the rest with a flag.
 schematic of iterations:
 0) before iterations:
 a) read control variables initial values.
 b) read readback variable initial values. 
 Take averages values if specified on the command line.
 these are not used in a correction. There is
 a reason to take the data, but I don't remember.
 1) each iteration:
 a) ping runControlPV
 b) read readback variable value. Take averages if requested.
 Do despiking if requested.
 c) calculate statistics of readback variables.
 d) read control variable values.
 e) calculate statistics of control variables.
 f) read test PV values.
 g) determine whether test PVs are in range. Despiking
 is done on a specified subset beforehand if requested.
 h) For out-of-range condition, sleep required time (i.e. 
 the value of the usual time interval or another specified value),
 and ping runcontrol while sleeping. Return to step 1a)
 i) For In-range condition, calculate the deltas for the
 control PV from the correction matrix and the readback
 PV values.
 Also, if averaging is on, skip the iteration
 after the in-range condition has just occurred.
 j) make corrections for the deltas if a delta limit
 is specified.
 k) calculate statistics for control variable deltas.
 l) send new control variable setpoints to IOCs.
 m) print stats if requested.
 n) write to output file and to stats file if requested.
 o) sleep for the remainder of the specified time interval 
 while pinging runcontrol PV.

 * Output table lists all variables at each time step.
 * L. Emery ANL
 * Version 2 4/23/95 Now uses ezca
 * 10/29/95 13:51:40:  added runControl library calls
 
 $Log: not supported by cvs2svn $
 Revision 1.25  2010/11/30 20:25:26  soliday
 Added internal CA.Client.Exception handling with an _exit() call to avoid
 calling EPICS atexit functions that can lock the program up.

 Revision 1.24  2010/06/07 22:10:01  shang
 fixed the problem of the not-enough waiting time for the second step after resuming from testing failure.

 Revision 1.23  2010/06/07 19:15:48  shang
 fixed the bug that it does not wait requested time after resuming from test failure.

 Revision 1.22  2006/10/20 15:21:08  soliday
 Fixed problems seen by linux-x86_64.

 Revision 1.21  2006/06/08 21:45:04  soliday
 Updated to reduce the run control calls.

 Revision 1.20  2006/03/16 00:59:22  emery
 Removed statements that reduced the loopParam.interval by
 some amount when averaging of readings was requested.
 Removed commented-out statements about other time adjustments
 that are no longer useful to string along.

 Revision 1.19  2005/11/09 19:42:30  soliday
 Updated to remove compiler warnings on Linux

 Revision 1.18  2005/11/08 22:05:03  soliday
 Added support for 64 bit compilers.

 Revision 1.17  2004/09/10 14:37:56  soliday
 Changed the flag for oag_ca_pend_event to a volatile variable

 Revision 1.16  2004/08/30 14:44:33  shang
 the despike threshold will only be ramped in the beginning (if requested) and when the
 despike threshold ramp pv is set to 1.

 Revision 1.15  2004/08/23 17:39:27  soliday
 Fixed usage message issue on WIN32

 Revision 1.14  2004/07/29 20:31:16  soliday
 Cleaned up vxWorks compiler warnings.

 Revision 1.13  2004/07/29 20:10:56  soliday
 Fixed some issues so that it would compile again for vxWorks.

 Revision 1.12  2004/07/27 15:08:26  shang
 tested despike ramping threshold and modified it to work with any given ramping
 steps.

 Revision 1.11  2004/07/26 16:22:14  shang
 fixed bugs in checking despike ramping arguments

 Revision 1.10  2004/07/23 21:54:04  shang
 removed unnecessary printing statements

 Revision 1.9  2004/07/23 21:50:57  shang
 allocated memory for rampThrehsoldPVInfo.count which is needed by setup ca connections.

 Revision 1.8  2004/07/23 20:51:21  shang
 replaced thresholdRampPV within despike option by rampThresholdPV because
 the former conflicts with threshold option in despike option.

 Revision 1.7  2004/07/23 16:27:23  soliday
 Fixed problem with oag_ca_pend_event.

 Revision 1.6  2004/07/23 14:50:17  soliday
 Added Hairong's despiking changes.

 Revision 1.5  2004/07/23 14:16:35  soliday
 Improved signal support when using Epics/Base 3.14.6

 Revision 1.4  2004/07/19 20:02:33  soliday
 Prior to setting the waveform data it checks each element of the waveform
 for a NaN value. If found it will abort the execution of the program.

 Revision 1.3  2004/07/19 17:39:36  soliday
 Updated the usage message to include the epics version string.

 Revision 1.2  2004/07/16 21:25:23  soliday
 Shang added additional despike options. I also replaced the sleep commands
 with ca_pend_event commands because Epics/Base 3.14.x has an inactivity
 timer that was causing disconnects from PVs when the log interval time was
 too large.

 Revision 1.1  2003/08/27 19:51:10  soliday
 Moved into subdirectory

 Revision 1.137  2003/07/28 15:46:30  soliday
 ALRM field only changes once when it goes out of range.

 Revision 1.136  2003/07/22 15:59:07  soliday
 Changed the alarm mode for out-of-range.

 Revision 1.135  2003/07/10 21:00:40  soliday
 Fixed the IOC alarm state when this program exits on the IOC.

 Revision 1.134  2003/07/01 19:34:43  soliday
 On the IOC the alarm state goes to major alarm when this program stops.

 Revision 1.133  2003/02/25 22:24:53  shang
 removed the extra ca_pend_io() statement after ca_search()

 Revision 1.132  2003/02/21 21:41:38  shang
 removed averaging for testing

 Revision 1.131  2003/02/21 00:02:28  shang
 add ca_pend_io(pendIOTime) after ca_search in SetupRawCAConnection for save. The previous
 version should work too, the failure might be caused by the system.

 Revision 1.130  2003/02/13 18:26:52  soliday
 Updated to solve a problem on vxWorks when this program exits with an error.

 Revision 1.129  2003/01/29 21:35:08  soliday
 This version adds the direct db access ability on the IOC.

 Revision 1.128  2003/01/29 16:11:50  soliday
 Made some improvements but it is still not finished for the IOC.

 Revision 1.127  2003/01/07 16:09:05  soliday
 Modified the code so it does not need to use the vxWorks delete hook.

 Revision 1.126  2002/11/27 16:22:05  soliday
 Fixed issues on Windows when built without runcontrol

 Revision 1.125  2002/11/19 22:33:03  soliday
 Altered to work with the newer version of runcontrol.

 Revision 1.124  2002/11/18 14:47:24  soliday
 This version seems to work better in the IOCs

 Revision 1.123  2002/11/11 20:14:08  soliday
 Minor modification from last change.

 Revision 1.122  2002/11/11 19:30:36  soliday
 Removed the ca callback commands.

 Revision 1.121  2002/11/05 15:49:18  soliday
 Fixed a problem with runcontrol

 Revision 1.120  2002/11/04 20:48:24  soliday
 There were issues running runcontrol because it is still using ezca calls.

 Revision 1.119  2002/10/31 15:47:24  soliday
 It now converts the old ezca option into a pendtimeout value.

 Revision 1.118  2002/10/16 19:55:04  soliday
 Changed sddscontrollaw, sddsfeedforward, and sddspvtest so that they
 no longer use ezca.

 Revision 1.117  2002/09/09 18:44:04  soliday
 Fixed a few bugs found during IOC testing.

 Revision 1.116  2002/08/14 20:00:32  soliday
 Added Open License

 Revision 1.115  2002/08/14 19:51:34  soliday
 Added a threshold pv.

 Revision 1.114  2002/08/07 00:11:01  soliday
 Committed after studies time. This has improved vxWorks functions.

 Revision 1.113  2002/08/06 14:28:35  soliday
 Added additional debug statements when the DEBUGTIMES flag is used.

 Revision 1.112  2002/08/05 14:14:50  soliday
 Changed the glitch logging memory to a circular buffer.

 Revision 1.111  2002/08/02 18:39:23  soliday
 Fixed a problem on vxWorks where a spawned task may not be deleted properly at exit

 Revision 1.110  2002/07/31 20:30:13  soliday
 Do not do a ca_flush_io at exit in vxWorks.

 Revision 1.109  2002/07/30 12:38:17  soliday
 Removed some problems related to the waveform options.

 Revision 1.108  2002/07/24 20:45:40  shang
 added -searchPath feature

 Revision 1.107  2002/07/17 20:37:47  soliday
 Changed the -messagepv option to -launcherpv and added the ability to set the
 alarm status.

 Revision 1.106  2002/07/17 18:15:02  soliday
 Added the -messagePV option.

 Revision 1.105  2002/07/08 20:56:14  soliday
 Committed changes after studies period.

 Revision 1.104  2002/07/02 16:45:31  soliday
 Removed some compiler warnings on Windows.

 Revision 1.103  2002/07/01 15:56:54  soliday
 Added the ability to log to the glitch file when a tests value is out of range.

 Revision 1.102  2002/06/28 16:42:49  soliday
 Updated the glitch logging function.

 Revision 1.101  2002/06/26 18:37:28  soliday
 Added the ability to specify PV names for -interval and -average options.

 Revision 1.100  2002/06/13 19:40:24  soliday
 Added a brief mode to the statistics.

 Revision 1.99  2002/05/07 19:30:06  jba
 Changes for cross only build.

 Revision 1.98  2002/04/30 03:35:59  shang
 made setpoint bpm vector working, updated readback->old at each step, set
 the control->delta to zero when out of range. added -mode=integral|proportional
 to -auxiliary option, and separate it from the control (integral or proportional)
 mode.

 Revision 1.97  2002/04/17 22:20:14  soliday
 Added one missing free_scanargs.

 Revision 1.96  2002/04/17 00:53:35  shang
 fixed bug in -proportional mode

 Revision 1.95  2002/04/10 14:56:51  soliday
 Removed some memory leaks related to the scanargs function.

 Revision 1.94  2002/04/09 21:39:03  soliday
 Shang added some missing free statements for the loopParam

 Revision 1.93  2002/04/03 20:32:49  soliday
 Fixed a small problem with vxWorks.

 Revision 1.92  2002/04/03 20:30:04  soliday
 Fixed a problem with timeouts.

 Revision 1.91  2002/04/02 16:13:21  soliday
 Fixed a timeout problem

 Revision 1.90  2002/04/01 21:59:25  soliday
 Fixed a problem with the channelInfo variable not being allocated.

 Revision 1.89  2002/04/01 18:05:12  soliday
 Added a missing option to MakeDailyGenerationFilename

 Revision 1.88  2002/03/22 23:01:59  soliday
 Changed argument for free_scanargs

 Revision 1.87  2002/03/21 23:09:14  soliday
 Fixed some bugs for vxWorks.

 Revision 1.86  2002/03/18 19:04:48  soliday
 Committed Shang's waveform changes and my vxWorks changes.

 Revision 1.85  2002/02/26 17:13:15  shang
 fixed the bug that the controlName of correctors was not setup if -actuator
 options is provided but the definition file is not provided.

 Revision 1.84  2002/02/18 19:41:25  soliday
 Disabled the fsync command for the output files.

 Revision 1.83  2002/01/30 19:58:11  soliday
 Made changes needed by WIN32

 Revision 1.82  2002/01/22 16:22:41  shang
 added -waveforms option, which had been tested in real SR system

 Revision 1.81  2001/11/20 14:02:40  borland
 Added option to obtain the gain from a PV.

 Revision 1.80  2001/10/28 14:26:42  borland
 Revised setupInputFile() to use SDDS_AssertColumnFlags(), which makes it
 much faster to read in large arrays.

 Revision 1.79  2001/09/10 19:02:41  soliday
 Removed Solaris compiler warning.

 Revision 1.78  2001/09/10 18:46:05  soliday
 Fixed Linux warning messages.

 Revision 1.77  2001/08/29 02:26:58  shang
 add PVOffsets option

 Revision 1.76  2001/05/15 14:35:56  emery
 Added additional initialization and tests for overlap compensation.

 Revision 1.75  2001/05/15 13:36:59  borland
 Fixed bugs introduced when overlap compenstation was added.  Runs without
 overlap compensation would sometimes fail due to memory access errors
 (uninitialized variables).

 Revision 1.74  2001/05/14 21:19:21  emery
 Changed the overlap compensation functionality. We now
 expect a direct response matrix connecting the correctors used
 to the RT "FF" bpm setpoint. Changed the option name from
 overlapCompensation to auxiliaryOutput since RT ff bpms are
 a type of output.

 Revision 1.73  2001/05/03 19:53:44  soliday
 Standardized the Usage message.

 Revision 1.72  2000/12/06 21:03:06  soliday
 Made changes needed for WIN32.

 Revision 1.71  2000/11/29 23:20:25  borland
 Fixed problems with offset and hold present values introduced in recent
 revisions.

 Revision 1.70  2000/11/27 20:12:29  borland
 Fixed a looping construction problem in setPVs and readPVs that may have
 affected performance.

 Revision 1.69  2000/11/27 17:09:56  borland
 Fixed memory leaks that repeated with each iteration.

 Revision 1.68  2000/11/23 10:30:49  emery
 Added possibility of specifying a secondary correction
 matrix which to be used in a feedfroward fashion.
 A digital filter may be specified. Output of filtered
 correctors is available.

 Revision 1.67  2000/07/26 23:34:03  emery
 Made applyDeltaLimit run correctly when proportional control
 is requested.

 Revision 1.66  2000/07/26 22:41:52  emery
 Fixes a bug where loopParam->integral was set to 1 instead of 0 when
 proportional control was specified in the command line.
 Changed the values of targetTime and timeLeft when
 the targetTime was passed the present clock time (this could
 happened when the process is suspended for some time).

 Revision 1.65  2000/07/18 19:40:45  emery
 Made arguments of pingWhileSleep equal to
 zero when a very small value was used.
 Inside pingWhileSleep skipped the sleep
 function if the argument is zero.

 Revision 1.64  2000/07/09 16:11:01  borland
 Fixed bug that resulted in very rapid running of iterations after a suspension.

 Revision 1.63  2000/06/21 12:04:16  emery
 Improved the implementation of the interval value
 to force the time duration of each iteration cycle to
 be exactly the value entered. Previously the interval value
 was simply the sleep period after each iteration,
 ignoring the time overhead of doing ca connections
 and writing to files, etc.

 Revision 1.62  2000/04/20 15:58:14  soliday
 Fixed WIN32 definition of usleep.

 Revision 1.61  2000/04/19 15:50:36  soliday
 Removed some solaris compiler warnings.

 Revision 1.60  2000/03/09 16:26:56  soliday
 Fixed a bug with it trying to close the
 testValues file even if it was never opened.

 Revision 1.59  2000/03/08 17:13:19  soliday
 Removed compiler warnings under Linux.

 Revision 1.58  1999/09/17 22:11:51  soliday
 This version now works with WIN32

 Revision 1.57  1999/09/17 20:31:23  emery
 Added configuration file for selective despiking of PVs.

 Revision 1.56  1999/08/09 16:12:20  emery
 Corrected error in generating daily file names in sddscontrollaw.c.

 Revision 1.55  1999/08/06 20:29:57  emery
 Added -generations and -dailyFiles options.
 Added -readbcakLimit option which give the user
 a way to specify the maximum and minimum values
 that readback PVs can have for the purposes of
 calculating a correction.

 Revision 1.54  1998/10/27 23:28:01  emery
 Changed the logic for skipping an iteration step
 to include the case when values are out of range and
 when averaging is requested.

 Revision 1.53  1998/07/03 02:32:52  emery
 Corrected a bug that was introduced when I tried to simplify
 passing the readback and control structures as argument.
 I went back to using the CORRECTOR structure with
 pointer elements to readback and control.

 Revision 1.52  1998/06/30 13:11:57  emery
 Added glitch logging triggered on readback rms
 and control delta rms.
 Added adjustable ping timeout for the run control record.

 Revision 1.51  1998/05/19 22:13:18  emery
 Added default: statements to switch statements.

 Revision 1.50  1998/05/19 22:04:12  emery
 Added fprintf message before default: exit(1); code.

 Revision 1.49  1998/05/19 21:57:23  emery
 Repositioned a badly placed #endif statement which completed the
 #ifdef #endif structure outside of a for loop.

 Revision 1.48  1998/02/24 18:25:42  emery
 changed the option name testCASecurity to CASecurityTest
 to avoid conflict with testValues option.

 Revision 1.47  1998/02/19 22:09:55  emery
 Added option -testCASecurity which checks the write
 permission of the control PVs. The control PVs are not updated
 if at least one PV has write access denied. An alarm is
 sent to the runcontrol PV, and the string "CA access denied"
 is written to runcontrol message PV.

 Revision 1.46  1997/08/18 21:53:15  emery
 Put in a few more call to getTimeInSecs() so that
 reading or setting PV messages get an accurate time stamp.
 Moved the pingWhileSleep(0.1) call after the printf
 calls in the control law loop.

 Revision 1.45  1997/07/30 20:38:26  emery
 Added argument timeStamp to logActuator call.
 Added timeStamp parameter to controlLogFile.
 Put logActuator call outside if dryrun "if" block.

 Revision 1.44  1997/02/28 21:45:11  emery
 Reduced ping timeout factor from 4 to 2.

 Revision 1.43  1997/02/25 20:15:31  borland
 Added -offsets option to allow specifying a file of values at which to
 hold the readbacks.

 Revision 1.42  1997/02/19 19:45:37  borland
 Added better error reporting for CA errors.

 Revision 1.41  1997/01/17 17:22:10  emery
 Added retries to the opening of files.

 Revision 1.40  1996/12/07 09:06:17  emery
 Added a runControlLogMessage call with
 "Terminated by signal" when the server exits.
 Unfortunately, the description runcontrol PV is set
 to this value only briefly since a runControlExit
 call occurs upon exiting sddscontrollaw.

 Revision 1.39  1996/12/02 05:39:13  emery
 The usleep function hangs up with an argument of 0 sec.
 The sleep time for PingWhileSleep was changed
 from 0.0 to 0.1 sec, so that the usleep function does not hang up.

 Revision 1.38  1996/11/21 00:08:26  emery
 Added control variable log.
 In server mode, the runcontrol is exitied and re-initialized
 with the new run control PV and description.
 Added a NULL to the argument list of logArgument call.

 Revision 1.37  1996/11/18 22:23:56  epics
 Added include for alarm.h

 Revision 1.36  1996/11/14 21:16:07  emery
 Changed location where the server is started.
 Return n when ezcaEndGroup returns an error code.
 Make the sddscontrollaw exit when ezca or ca errors are
 detected.
 Added serverMode variable to InitializeData.

 Revision 1.35  1996/11/11 23:56:09  emery
 Put testing of runControlPing return codes into
 runControlPingwhileSleep function to make the main
 routine a little more streamlined.

 Revision 1.34  1996/11/10 18:55:52  emery
 Re-organized an if statement in setupTestsFile.
 Added initialization of STATS structures, and more
 initialization of test structure.

 Revision 1.33  1996/11/10 17:50:46  emery
 Major re-writting again using data structures.
 Not tested with beam.

 Revision 1.32  1996/11/07 01:07:11  emery
 Almost total re-write of sddscontrollaw with structuring
 into function with large number of arguments. Not tested
 with real beam.

 Revision 1.31  1996/10/08 08:45:28  emery
 Added despiking to a subset of test values. To enable
 despiking of test PVs, the -despike option for the regular
 readback variables should be used, and
 the tests file should have a Despike long column
 defined with a non-zero value for those PVs to despike.
 Despiking parameters (average, neighbors, passes) are the
 same as that of the readback variables.

 Revision 1.30  1996/10/06 17:42:40  borland
 Added printout of how many spikes are removed.

 Revision 1.29  1996/10/06 17:28:23  borland
 Added output of stats for despiked readbacks.

 Revision 1.28  1996/10/05 09:25:36  emery
 Added statistics logging file. Added Action limit, which isn't tested.

 Revision 1.27  1996/10/05 00:32:38  borland
 Added despiking algorithm. NOT tested.

 Revision 1.26  1996/09/23 18:52:49  emery
 Fixed bug in calling logDaemon: corrected the spelling of the
 tag "Action".

 Revision 1.25  1996/09/21 01:41:07  emery
 Fixed bug where the block of a for loop for the controlValue
 calculation wasn't bracketed.

 Revision 1.24  1996/09/21 01:07:04  emery
 Fixed bug that was introduced  in the last version
 where "factor" wasn't multiplying the deltas
 for the delta limit option.

 Revision 1.23  1996/09/19 20:42:24  emery
 Changed all run control minor alarms to major ones.

 Revision 1.22  1996/09/18 18:26:40  emery
 Added a nice table of statistics with verbose option.

 Revision 1.21  1996/09/12 11:22:19  emery
 Removed a semi-colon from the end of an "if" statement
 testing the syntax of the -deltaLimit option
 which caused the body of the if statement to be ignored.

 * Revision 1.20  1996/07/05  21:21:23  emery
 * Added averaging of readbacks before making a correction.
 *
 * Revision 1.19  1996/05/24  04:18:55  emery
 * Added calls to logDaemon.
 *
 * Revision 1.18  1996/05/23  23:20:48  borland
 * Added -deltaLimit option.
 *
 * Revision 1.17  1996/05/13  15:44:10  emery
 * Removed some unnecessary run control log message with minor alarm
 * severity.
 *
 * Revision 1.16  1996/05/13  02:26:52  emery
 * Added the use of MINOR_ALARM for out-of-range PVs.
 *
 * Revision 1.15  1996/05/08  00:48:03  emery
 * reformatted the usage message for a shorter line.
 *
 * Revision 1.14  1996/05/03  05:18:45  emery
 * Made all the error and warning messages print to stderr.
 *
 * Revision 1.13  1996/03/22  06:02:48  sr
 * Increased ping timeout interval to 4 times the controllaw
 * time interval to reduce the number of timeouts caused
 * by network slowdowns.
 *
 * Revision 1.12  1996/02/15  20:47:19  emery
 * Added more checking on return codes from calls to runcontrol libraries.
 *
 * Revision 1.11  1996/02/14  05:07:53  borland
 * Switched from obsolete scan_item_list() routine to scanItemList(), which
 * offers better response to invalid/ambiguous qualifiers.
 *
 * Revision 1.10  1996/02/10  06:30:30  borland
 * Converted Time column/parameter in output from elapsed time to time-since-
 * epoch.
 *
 * Revision 1.9  1996/02/07  18:49:54  borland
 * Brought up-to-date with new time routines and common SDDS time output format.
 *
 * Revision 1.8  1995/12/02  00:30:10  emery
 * Forgot to enclose previous fix inside and "if ( runControlDesc ) {}"block.
 *
 * Revision 1.7  1995/12/01  23:12:44  emery
 * Added test values stderr output for out-of-range variable in -verbose mode.
 * Added missing #ifdef USE_RUNCONTROL and #endif statements around some runcontrol calls.
 *
 * Revision 1.6  1995/11/15  21:58:54  borland
 * Changed interrupt handlers to have proper arguments for non-Solaris machines.
 *
 * Revision 1.5  1995/11/15  21:57:04  emery
 * Added realloc to some string variables passed to channel access
 * in order to avoid the array bound error in purify compilation.
 *
 * Revision 1.4  1995/11/15  00:03:19  borland
 * Modified to use time routines in SDDSepics.c .
 *
 * Revision 1.3  1995/11/09  03:22:32  borland
 * Added copyright message to source files.
 *
 * Revision 1.2  1995/11/07  01:59:52  emery
 * Added runControl capability. runControl is a facility that allows
 * registration of applications using EPICS PVs as a database.
 *
 * Revision 1.1  1995/09/25  20:15:32  saunders
 * First release of SDDSepics applications.
 *
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "matlib.h"
#include "SDDS.h"
#include <time.h>
#if !defined(vxWorks)
#  include <sys/timeb.h>
#endif
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
#include <epicsVersion.h>
#ifdef USE_RUNCONTROL
#  include <libruncontrol.h>
#endif

#ifdef USE_LOGDAEMON
#  include <logDaemonLib.h>
#endif

void interrupt_handler(int sig);
void interrupt_handler2(int sig);
void sigint_interrupt_handler(int sig);

#if defined(vxWorks)
/*#define FLOAT_MATH 1*/
#endif
#define CLO_GAIN 0
#define CLO_TIME_INTERVAL 1
#define CLO_STEPS 2
#define CLO_UPDATE_INTERVAL 3
#define CLO_INTEGRAL 4
#define CLO_PROPORTIONAL 5
#define CLO_VERBOSE 6
#define CLO_CONTROL_QUANTITY_DEFINITION 7
#define CLO_HOLD_PRESENT_VALUES 8
#define CLO_DRY_RUN 9
#define CLO_TEST_VALUES 10
#define CLO_WARNING 11
#define CLO_EZCATIMING 12
#define CLO_UNGROUPEZCACALLS 13
#define CLO_RUNCONTROLPV 14
#define CLO_RUNCONTROLDESC 15
#define CLO_ACTUATORCOL 16
#define CLO_DELTALIMIT 17
#define CLO_ACTIONLIMIT 18
#define CLO_AVERAGE 19
#define CLO_STATISTICS 20
#define CLO_DESPIKE 21
#define CLO_SERVERMODE 22
#define CLO_CONTROLLOG 23
#define CLO_OFFSETS 24
#define CLO_TESTCASECURITY 25
#define CLO_GLITCHLOG 26
#define CLO_GENERATIONS 27
#define CLO_DAILY_FILES 28
#define CLO_READBACK_LIMIT 29
#define CLO_OVERLAP_COMPENSATION 30
#define CLO_PVOFFSETS 31
#define CLO_WAVEFORMS 32
#define CLO_INFINITELOOP 33
#define CLO_PENDIOTIME 34
#define CLO_LAUNCHERPV 35
#define CLO_SEARCHPATH 36
#define CLO_THRESHOLD_RAMP 37
#define CLO_POST_CHANGE_EXECUTION 38
#define CLO_FILTERFILE 39
#define CLO_TRIGGERPV 40
#define CLO_ENDOFLOOPPV 41
#define COMMANDLINE_OPTIONS 42

#define CLO_READBACKWAVEFORM 0
#define CLO_OFFSETWAVEFORM 1
#define CLO_ACTUATORWAVEFORM 2
#define CLO_FFSETPOINTWAVEFORM 3
#define CLO_TESTWAVEFORM 4
#define WAVEFORMOPTIONS 5

#define DEFAULT_AVEINTERVAL 1 /* one second default time interval */

#define LIMIT_VALUE 0x0001U
#define LIMIT_MINVALUE 0x0002U
#define LIMIT_MAXVALUE 0x0004U
#define LIMIT_MINMAXVALUE 0x0008U
#define LIMIT_FILE 0x0010U

#define DEFAULT_GAIN 0.1
#define DEFAULT_COMPENSATION_GAIN 1.0
#define DEFAULT_TIME_INTERVAL 1.0
#define DEFAULT_STEPS 100
#define DEFAULT_UPDATE_INTERVAL 1
#define DEFAULT_TIMEOUT_ERRORS 5

#define DESPIKE_AVERAGEOF 0x0001U
#define DESPIKE_STARTTHRESHOLD 0x0002U
#define DESPIKE_ENDTHRESHOLD 0x0004U
#define DESPIKE_STEPSTHRESHOLD 0x0008U
#define DESPIKE_THRESHOLD_RAMP_PV 0x0010U

typedef struct
{
#if defined(DBAccess)
  DBADDR channelID;
#else
  chid channelID;
#endif
  int32_t waveformLength;
  double value, *waveformData;
  /* used with channel access routines to give index via callback: */
  long usrValue, flag, *count;
} CHANNEL_INFO;

typedef struct
{
  char *file;       /* not necessarily used in all instances */
  char *searchPath; /*point to the searchPath of loopParam */
  int64_t n;
  int32_t *despike, *waveformIndex, valueIndexes;
  short *waveformMatchFound, *setFlag;
  char **controlName, **symbolicName;
  CHANNEL_INFO *channelInfo;
  double **value, *setpoint, *error, *initial, *old, **delta;
  MATRIX *history; /* array of past values arranged in a matrix form */
  MATRIX *historyFiltered;
  long integral; /*the default is integral */
  /*integral (integral=1): control->value[0] +=control->delta[0],
    proportional(integral=0): control->value[0]=control->delta[0] */
} CONTROL_NAME;

typedef struct
{
  unsigned long flag;
  char *file, **controlName;
  char *searchPath; /*point to the searchPath of loopParam */
  long n, *index;
  double *value, *minValue, *maxValue;
  double singleValue, singleMinValue, singleMaxValue;
} LIMITS;

#ifdef USE_RUNCONTROL
typedef struct
{
  char *PVparam, *DescParam;
  char *PV, *Desc;
  double pingInterval;
  float pingTimeout;
  char message[39], handle[256];
  long alarmSeverity;
  int status, init;
#  if !defined(vxWorks)
  RUNCONTROL_INFO rcInfo;
#  endif
} RUNCONTROL_PARAM;
#endif

/*for datastrobe trigger */
typedef struct
{
  char *PV;
  chid channelID;
  double currentValue;
  short triggered, datalogged, initialized;
  /* used with channel access routines to give index via callback: */
  long usrValue;
  long trigStep; /* trigStep: the Step where trigger occurs */
  long datastrobeMissed;
} DATASTROBE_TRIGGER;
void datastrobeTriggerEventHandler(struct event_handler_args event);
long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger);

typedef struct
{
  long *step, steps, integral, holdPresentValues, dryRun, compensationIntegral;
  long updateInterval, n; /*n is the number of readbacks */
  long briefStatistics;
  double interval, gain, compensationGain, *elapsedTime, *epochTime;
  char *searchPath;
  char *offsetFile;
  char *offsetPVFile;
  char **offsetPV, *gainPV, *intervalPV, *averagePV, *launcherPV[5], *endOfLoopPV;
  char *postChangeExec;
  short triggerProvided;
  DATASTROBE_TRIGGER trigger;
  CHANNEL_INFO *channelInfo, gainPVInfo, intervalPVInfo, averagePVInfo, launcherPVInfo[5], endOfLoopPVInfo;
  double *offsetPVvalue;
} LOOP_PARAM;

typedef struct
{
  char **waveformFile;
  char *searchPath; /*point to the searchPath of loopParam */
  long waveforms, waveformFiles;
  char **waveformPV, **writeWaveformPV;
  CHANNEL_INFO *channelInfo, *writeChannelInfo;
  int32_t **readbackIndex, *waveformLength;
  double **waveformData;
  double *readbackValue; /*the index is consistent with that of readbacks */
} WAVE_FORMS;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  char *coefFile;
  char *definitionFile; /*definition file for controls and readbacks */
  /* READBACK_NAME *readback; */
  CONTROL_NAME *control, *readback;
  char *actuator;
  MATRIX *K;
  MATRIX *aCoef;
  MATRIX *bCoef;
} CORRECTION;

typedef struct
{
  unsigned long flag;
  int32_t neighbors, passes, averageOf, countLimit, stepsThreshold;
  long reramp;
  double threshold, startThreshold, endThreshold, deltaThreshold;
  /* file which give despiking flag for readback PVs in matrix */
  char *file, **controlName, **symbolicName, *thresholdPV, *rampThresholdPV;
  char *searchPath; /*point to the searchPath of loopParam */
  int32_t n, *despike;
  CHANNEL_INFO thresholdPVInfo, rampThresholdPVInfo;
} DESPIKE_PARAM;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  long n, *outOfRange;
  /* despikeValues: number of PVs to despike */
  int32_t despikeValues, *despike, *despikeIndex, *writeToFile, writeToGlitchLogger;
  char **controlName;
  CHANNEL_INFO *channelInfo;
  double *value, *min, *max, *sleep, *holdOff, *reset, longestSleep, longestHoldOff, longestReset;
  char *glitchLog;
} TESTS;

/*defined for multiple waveform tests*/
typedef struct
{
  long waveformTests, testFiles;
  char **testFile;  /*file names of waveform tests */
  char *searchPath; /*point to the searchPath of loopParam */
  char **waveformPV, ***DeviceName;
  CHANNEL_INFO *channelInfo;
  int32_t *waveformLength, **outOfRange, *testFailed;
  int32_t **index;
  double **waveformData, **min, **max, **sleep, *waveformSleep, **holdOff, *waveformHoldOff;
  /*despikeValues: number of PVs to despike */
  int32_t *despikeValues, **despike, **despikeIndex, **writeToFile;
  long longestSleep, longestHoldOff;
  short holdOffPresent, **ignore;
} WAVEFORM_TESTS;

typedef struct
{
  long level, counter;
} BACKOFF;

typedef struct
{
  double RMS, ave, MAD, largest;
  char *largestName, msg[256];
} STATS;

typedef struct
{
  long n;
  double n2;
  double interval;
} AVERAGE_PARAM;

typedef struct
{
  char *file;
  char *searchPath; /*point to the searchPath of loopParam */
  double readbackRMSthreshold, controlRMSthreshold;
  int32_t rows;
  long avail_rows, glitched, row_pointer;
} GLITCH_PARAM;

typedef struct
{
  long doGenerations, daily;
  int32_t digits, rowLimit;
  double timeStop, timeLimit;
  char *delimiter;
} GENERATIONS;

long oag_ca_pend_event(double timeout, volatile int *flag);
void SetupRawCAConnection(char **PVname, CHANNEL_INFO *channelInfo, long n, double pendIOTime);
void initializeWaveforms(WAVE_FORMS *wave_forms);
void initializeTestWaveforms(WAVEFORM_TESTS *waveform_test);
long isSorted(int32_t *index, long n);
void SortBPMsByIndex(char ***PV, int32_t *index, long n);
void setupWaveforms(WAVE_FORMS *wave_forms, CONTROL_NAME *readback, long verbose, double pendIOTime);
long ReadWaveformData(WAVE_FORMS *wave_forms, double pendIOTime);
void setupWaveformTests(WAVEFORM_TESTS *waveform_tests, CONTROL_NAME *readback, CONTROL_NAME *control, double interval, long verbose, double pendIOTime);
long CheckWaveformTest(WAVEFORM_TESTS *waveform_tests, BACKOFF *backoff, LOOP_PARAM *loopParam, DESPIKE_PARAM *despikeParam, long verbose, long warning, double pendIOTime);
void cleanupWaveforms(WAVE_FORMS *wave_forms);
void cleanupTestWaveforms(WAVEFORM_TESTS *waveform_tests);
long WriteWaveformData(WAVE_FORMS *controlWaveforms, CONTROL_NAME *control, double pendIOTime);
void CleanUpCorrections(CORRECTION *correction);
void CleanUpOthers(DESPIKE_PARAM *despike, TESTS *tests, LOOP_PARAM *loopParam);
void CleanUpLimits(LIMITS *limits);

void setupDeltaLimitFile(LIMITS *delta);
void setupReadbackLimitFile(LIMITS *readbackLimits);
void setupActionLimitFile(LIMITS *action);
void setupInputFile(CORRECTION *correction);
void assignControlName(CORRECTION *correction, char *defitionFile);
void matchUpControlNames(LIMITS *limits, char **controlName, long n);
void setupTestsFile(TESTS *tests, double interval, long verbose, double pendIOTime); /*interval is the correction interval */
void setupDespikeFile(DESPIKE_PARAM *despikeParam, CONTROL_NAME *readback, long verbose);
void setupOutputFile(SDDS_TABLE *outputPage, char *file, CORRECTION *correction, TESTS *test, LOOP_PARAM *loopParam, char *timeStamp, WAVEFORM_TESTS *waveform_tests, long verbose);
void setupStatsFile(SDDS_TABLE *statsPage, char *statsFile, LOOP_PARAM *loopParam, char *timeStamp, long verbose);
void setupGlitchFile(SDDS_TABLE *glitchPage, GLITCH_PARAM *glitchParam, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, char *timeStamp, long verbose);
void getInitialValues(CORRECTION *correction, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime);
long initializeHistories(CORRECTION *compensation);
long getReadbackValues(CONTROL_NAME *readback, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *offsetWaveforms, long verbose, double pendIOTime);
long checkActionLimit(LIMITS *action, CONTROL_NAME *readback);
void adjustReadbacks(CONTROL_NAME *readback, LIMITS *readbackLimits, DESPIKE_PARAM *despikeParam, STATS *readbackAdjustedStats, long verbose);
long getControlDevices(CONTROL_NAME *control, STATS *controlStats, LOOP_PARAM *loopParam, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime);
#ifdef USE_RUNCONTROL
void checkRCParam(SDDS_TABLE *inputPage, char *file);
#endif
void despikeTestValues(TESTS *test, DESPIKE_PARAM *despikeParam, long verbose);
long checkOutOfRange(TESTS *test, BACKOFF *backoff, STATS *readbackStats, STATS *readbackAdjustedStats, STATS *controlStats, LOOP_PARAM *loopParam, double timeOfDay, long verbose, long warning);
void apply_filter(CORRECTION *correction, long verbose);
void controlLaw(long skipIteration, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, long verbose, double pendIOTime);
/* return factor which was applied to force the control
   under the limit */
double applyDeltaLimit(LIMITS *delta, LOOP_PARAM *loopParam, CONTROL_NAME *control, long verbose, long warning);
void calcControlDeltaStats(CONTROL_NAME *control, STATS *controlStats, STATS *controlDeltaStats);
void writeToOutputFile(char *outputFile, SDDS_TABLE *outputPage, long *outputRow, LOOP_PARAM *loopParam, CORRECTION *correction, TESTS *test, WAVEFORM_TESTS *waveform_tests);
void readOffsetValues(double *offset, long n, char **controlName, char *offsetFile, char *searchPath);
void readOffsetPV(char **offsetPVs, long n, char **controlName, char *PVOffsetFile, char *searchPath);
void writeToStatsFile(char *statsFile, SDDS_TABLE *statsPage, long *statsRow, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, STATS *controlStats, STATS *controlDeltaStats);
void writeToGlitchFile(GLITCH_PARAM *glitchParam, SDDS_TABLE *glitchPage, long *glitchRow, LOOP_PARAM *loopParam, STATS *readbackAdjustedStats, STATS *controlDeltaStats, CORRECTION *correction, CORRECTION *overlapCompensation, TESTS *test);

void FreeEverything(void);

#if !defined(vxWorks)
void commandServer(int sig);
void serverExit(int sig);
void exitIfServerRunning(void);
void setupServer(void);
#endif

long parseArguments(char ***argv,
                    int *argc,
                    CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *param,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    char **statsFile,
                    long *serverMode,
                    char **commandFile,
                    char **outputFile,
                    char **controlLogFile,
                    long *testCASecurity,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    char **compensationOutputFile,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests,
                    long firstTime,
                    long *verbose,
                    long *warning,
                    double *pendIOTime,
                    char *commandline_option[COMMANDLINE_OPTIONS],
                    char *waveformOption[WAVEFORMOPTIONS]);

void initializeStatsData(STATS *stats);
void initializeData(CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests);

void setupData(CORRECTION *correction,
               LIMITS *delta,
               LIMITS *readbackLimits,
               LIMITS *action,
               LOOP_PARAM *loopParam,
               DESPIKE_PARAM *despikeParam,
               TESTS *test,
               CORRECTION *overlapCompensation,
               WAVE_FORMS *readbackWaveforms,
               WAVE_FORMS *controlWaveforms,
               WAVE_FORMS *offsetWaveforms,
               WAVE_FORMS *ffSetpointWaveforms,
               WAVEFORM_TESTS *waveform_test,
               GLITCH_PARAM *glitchParam,
               long verbose,
               double pendIOTime);

void logActuator(char *controlLogFile, CONTROL_NAME *control, char *timeStamp);

/* There is an added argument of file pointer to M. Borland function 
   found in sddsplot.c */
long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc, FILE *fp);
int countArguments(char *s);
long readPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, long n, AVERAGE_PARAM *aveParam, double pendIOTime);

long getAveragePVs(char **PVs, double *value, long number, long averages, double timeInterval);
long setPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, long n, double pendIOTime);
long runPostChangeExec(char *scriptName, CONTROL_NAME *control, long verbose);
long CheckCAWritePermissionMod(char **PVs, CHANNEL_INFO *channelInfo, long n);
int runControlPingWhileSleep(double sleepTime);
long setStringPV(char *PV, char *value, CHANNEL_INFO channelInfo, long exitOnError);
long setEnumPV(char *PV, long value, CHANNEL_INFO channelInfo, long exitOnError);
long readEnumPV(char *PV, long *value, CHANNEL_INFO channelInfo, double pendIoTime, long exitOnError);
void oag_ca_exception_handler(struct exception_handler_args args);

/* need to be global because some quantities are used
   in signal and interrupt handlers */
typedef struct
{
  CORRECTION correction, overlapCompensation;
  DESPIKE_PARAM despikeParam;
  TESTS test;
  LIMITS delta, action, readbackLimits;
  WAVE_FORMS readbackWaveforms, controlWaveforms;
  WAVE_FORMS offsetWaveforms, ffSetpointWaveforms;
  WAVEFORM_TESTS waveform_tests;
  char *outputFile, *statsFile, *controlLogFile;
  SDDS_TABLE outputPage, statsPage, glitchPage;
#ifdef USE_RUNCONTROL
  RUNCONTROL_PARAM rcParam;
#endif
  LOOP_PARAM loopParam;
  GLITCH_PARAM glitchParam;
#ifdef USE_LOGDAEMON
  LOGHANDLE logHandle;
  long useLogDaemon;
#endif
  char *pidFile;
  long reparseFromFile;
  int32_t *sortIndex;
  int argc;
  char ***argv;
  volatile int sigint;
} SDDSCONTROLLAW_GLOBAL;
SDDSCONTROLLAW_GLOBAL *sddscontrollawGlobal;


#if defined(vxWorks)
int sddsIocControlLaw(char *name, char *options)
#else
int main(int argc, char **argv)
#endif
{
#if defined(vxWorks)
  int argc;
  char **argv = NULL;
  char *buffer = NULL;
#endif
  SCANNED_ARG *s_arg = NULL;
  SDDS_TABLE compensationOutputPage;
  CONTROL_NAME *control = NULL, *controlComp = NULL, *readback = NULL, *readbackComp = NULL;
  char *outputRoot = NULL, *compensationOutputFile = NULL;
  long i, outputRow, compensationOutputRow, statsRow, glitchRow, firstTime, icontrol;
  long skipIteration = 0, outOfRange, prevOutOfRange = 0, waveformOutOfRange, testOutOfRange;
  char *timeStamp = NULL;
  double startTime, startHour, timeOfDay, targetTime, timeLeft, sleepTime = 0;
  BACKOFF backoff;
  AVERAGE_PARAM aveParam;
  STATS readbackStats, readbackDeltaStats, readbackAdjustedStats;
  STATS controlStats, controlDeltaStats;
  STATS readbackCompStats, readbackCompDeltaStats, readbackCompAdjustedStats;
  STATS controlCompStats, controlCompDeltaStats;
  long serverMode;
  long testCASecurity, caWriteError;
  double HourNow = 0, LastHour;
  long newFileCountdown = 0;
  GENERATIONS generations;
  double factor;
  double startReadTime = 0, endReadTime = 0, averageTime = 0, previousAverage = 0;
  long pre_n = 1;
#if defined(DBAccess)
  long microSeconds;
#endif

  char *commandline_option[COMMANDLINE_OPTIONS] = {
    "gain",
    "interval",
    "steps",
    "updateInterval",
    "integral",
    "proportional",
    "verbose",
    "controlQuantityDefinition",
    "holdPresentValues",
    "dryRun",
    "testValues",
    "warning",
    "ezca",
    "ungroupEzca",
    "runControlPV",
    "runControlDescription",
    "actuatorColumn",
    "deltaLimit",
    "actionLimit",
    "average",
    "statistics",
    "despike",
    "servermode",
    "controlLogFile",
    "offsets",
    "CASecurityTest",
    "glitchlogfile",
    "generations",
    "dailyFiles",
    "readbackLimit",
    "auxiliaryOutput",
    "PVOffsets",
    "waveforms",
    "infiniteLoop",
    "pendIOTime",
    "launcherPV",
    "searchPath",
    "thresholdRamp",
    "postChangeExecution",
    "filterFile",
    "triggerPV",
    "endOfLoopPV"
  };
  char *waveformOption[WAVEFORMOPTIONS] = {
    "readback", "offset", "actuator", "ffSetpoint", "test"};
#if !defined(vxWorks)
  char *USAGE1 = "sddscontrollaw <inputfile> [-searchPath=<dir-path>] [-actuatorColumn=<string>]\n\
       [<outputfile>] [-infiniteLoop] [-pendIOTime] \n\
       [-filterFile=<file>]\n\
       [-generations[=digits=<integer>][,delimiter=<string>][,rowlimit=<number>][,timelimit=<secs>] | \n\
       -dailyFiles] [-controlQuantityDefinition=<file>]\n\
       [-gain={<real-value>|PVname=<name>}]\n\
       {[-interval={<real-value>|PVname=<name>}] | -triggerPV=<PVname>} [-steps=<integer=value>]\n\
       [-updateInterval=<integer=value>]\n\
       [{-integration | -proportional}]\n\
       [-holdPresentValues] [-offsets=<offsetFile>] [-PVOffsets=<filename>] \n\
       [-endOfLoopPV=<pvName>]\n\
       [-average={<number>|PVname=<name>}[,interval=<seconds>]] \n\
       [-despike[=neighbors=<integer>][,passes=<integer>][,averageOf=<integer>][,threshold=<value>][,pvthreshold=<pvname][,file=<filename>][,countLimit=<integer>][,startThreshold=<value>,endThreshold=<value>,stepsThreshold=<integer>][,rampThresholdPV=<string>]]\n\
       [-deltaLimit={value=<value>|file=<filename>}]\n\
       [-readbackLimit={value=<value>|minValue=<value>,maxValue=<value>|file=<filename>}]\n\
       [-actionLimit={value=<value>|file=<filename>}]\n\
       [-testValues=<SDDSfile>] [-statistics=<filename>[,mode=<full|brief>]] \n\
       [-auxiliaryOutput=matrixFile=<file>,controlQuantityDefinition=<file>,\n\
          filterFile=<file>],controlLogFile=<file>[,mode=<integral|proportional>] \n\
       [-runControlPV={string=<string>|parameter=<string>},pingTimeout=<value>\n\
       [-runControlDescription={string=<string>|parameter=<string>}]\n\
       [-launcherPV=<pvname>]\n\
       [-verbose] [-dryRun] [-warning]\n\
       [-servermode=pid=<file>,command=<file>]\n\
       [-controlLogFile=<file>] \n\
       [-glitchLogFile=file=<string>,[readbackRmsThreshold=<value>][,controlRmsThreshold=<value>][,rows=<integer]]\n\
       [-CASecurityTest] [-waveforms=<filename>,<type>] [-postChangeExecution=<string>] \n\n";
  char *USAGE2 = "Perform simple feedback on APS control system process variables using ca calls.\n\
<inputfile>    gain matrix in sdds format\n\
<searchPath>   the directory path for the input files.\n\
actuatorColumn String column in the input file to be used for\n\
               actuator names. The default is the first string column\n\
               of the input file.\n\
<outputfile>   optional file in which the readback and\n\
               control variables are printed at every step.\n\
<filterFile>   optional file which provides the filtering information to be applied to actuators.\n\
               the file must contain DeviceName, a0, a1,..., b0, b1,... columns\n";
  char *USAGE3 = "\
generations    The output is sent to the file <SDDSoutputfile>-<N>, where <N> is\n\
               the smallest positive integer such that the file does not already \n\
               exist. By default, four digits are used for formatting <N>, so that\n\
               the first generation number is 0001.  If a row limit or time limit\n\
               is given, a new file is started when the given limit is reached.\n\
dailyFiles     The output is sent to the file <SDDSoutputfile>-YYYY-JJJ-MMDD.<N>,\n\
               where YYYY=year, JJJ=Julian day, and MMDD=month+day.  A new file is\n\
               started after midnight.\n\
controlQuantityDefinition\n\
               a cross-reference file which matches the simplified\n\
               names used in the inputfile to PV names.\n\
               Column names are SymbolicName and controlName.\n\
gain           quantity multiplying the inputfile matrix.\n\
               If the gain matrix is the inverse response matrix\n\
               then this should be less than one.\n\
interval       time interval between each correction.\n\
triggerPV      Names a process variable that must change to start each\n\
               cycle.\n\
steps          total number of corrections.\n\
postChangeExecution run given execution after changing the setpoints.\n";
char *USAGE4 = "\
updateInterval number of steps between each outputfile updates.\n\
integral | proportional\n\
               switch the control law to either integral or proportional\n\
               control;  use integral control for orbit correction; \n\
               the default control law is integral control.\n\
holdPresentValues\n\
               causes regulation of the readback variables to the\n\
               values at the start of the running.\n\
offsets        Gives a file of offset values to be subtracted from the\n\
               readback values to obtain the values to be held to zero.\n\
               Expected to contain at least two columns: ControlName and Offset. \n\
PVOffsets      Gives a file of offset PVs to be read and their values are \n\
               subtracted from the readback values of the primary readbacks.\n\
               Expected to contain at least two columns: ControlName (the names \n\
               of the primary readbacks), OffsetControlName (the name of \n\
               the offset PVs). \n\
auxiliaryOutput reads in an additional matrix to calculate values for PVs  \n\
               (not necessarily corrector PVs) based \n\
               on the correction that is being done. The formula \n\
               y = (gain) M (delta_C) is used for integral mode and \n\
               y = (gain) M (C) for proportional mode. Time filtering \n\
               is available through a filter file with \n\
               a0, b0, a1, etc, coefficients. \n\
               A control quantity definition file for the matrix \n\
               is available as option. The default mode is integral, if \n\
               mode=proportional is given, proportional control will be applied. \n\
verbose        prints extra information.\n\
dryRun         readback variables are read, but the control variables\n\
               aren't changed.\n";
char *USAGE5 = "\
average        number of readbacks to average before making a correction.\n\
               Default interval is one second. Units of the specified \n\
               interval is seconds. The total time for averaging will\n\
               not add to the time between corrections.\n\
testValues     sdds format file containing minimum and maximum values\n\
               of PV's specifying a range outside of which the feedback\n\
               is temporarily suspended. Column names are ControlName,\n\
               MinimumValue, MaximumValue. Optional column names are \n\
               SleepTime, ResetTime, Despike, and GlitchLog.\n\
deltaLimit     Specifies maximum change made in one step for any actuator,\n\
               either as a single value or as a column of values (column name\n\
               DeltaName) from a file. The calculated change vector is scaled\n\
               to enforce these limits, if necessary.\n\
readbackLimit  Specifies the maximum negative and positive error to\n\
               recognize for each PV. The values can be specified\n\
               as a single value, as two values, or as columns of\n\
               values in a file (clumns should be minValue and maxValue).\n\
actionLimit    Specifies minimum values in readback before any action is\n\
               taken, either as a single value or as a column of values\n\
               (column name ActionLimit) from a file.\n\
warning        prints warning messages.\n\
pendIoTime     specifies maximum time to wait for connections and\n\
               return values.  Default is 30.0s \n\
runControlPV   specifies a string parameter in <inputfile> whose value\n\
               is a runControl PV name.\n\
runControlDescription\n\
               specifies a string parameter in <inputfile> whose value\n\
               is a runControl PV description record.\n";
char *USAGE6 = "\
despike        specifies despiking of readback variables,\n\
               under the assumption\n\
               that consecutive readbacks are nearest neighbors. If a file is\n\
               specified, then the PVs with column Despike value of 0 will not\n\
               get despiked. If the testsValues file has a Despike column defined,\n\
               the test PVs with a nonzero value for Despike will get despiked\n\
               with the same option parameters. If countLimit is greater than 0\n\
               and there are more than N readings outside the despiking threshold,\n\
               then no despiking is done.\n\
               If startThreshold, endThreshold and stepsThreshold \n\
               are provided, then ramp the despiking threshold from startThreshold to \n\
               endThreshold over stepsThreshold correction steps. It then remains at \n\
               endThreshold, unless the threshold pv is given with -despike.  \n\
               In that case, the threshold can be changed via the PV.\n\
               If rampThresholdPV is provided, the threshold will be reramped whenever \n\
               the value of rampThresholdPV is no-zero and it is reset to 0 after reramping.\n";
char *USAGE7 = "\
servermode     allows one to change the commandline options while the program is\n\
               running. Program reads the command file for new options whenever\n\
               SIGUSR1 is received, and exits when SIGUSR2 is received.\n\
               The process id is stored in file specified by pid.\n\
controlLogFile At each change of actuators, the old and new values of the actuators\n\
               are written to this file. The previous instance of the file \n\
               is over-written at the same time.\n\
glitchLogFile  Readback and control data values\n\
               are written at each glitch event defined by the\n\
               by the RMS thresholds specified as sub-options.\n\
CASecurityTest checks the channel access write permission of the control PVs.\n\
               If at least one PV has a write restriction then the program suspends\n\
               the runcontrol record.\n";
char *USAGE8 = "\
waveforms=<filename>,<type> the waveform file name, and the type of\n\
               waveforms. <type>=readback, offset, actuator or ffSetpoint or test.\n\
               The waveform file contains \"WaveformPV\" parameter, \n\
               \"DeviceName\" and \"Index\" columns, which is the index of DeviceName in \n\
               the vector. Note that for actuators, if the reading and writing name are\n\
               different, two parameters \"ReadWaveformPV\" and \"WriteWaveformPV\" \n\
               should be given. For testing waveforms, there are two additional required columns: \n\
               MaximumValue and MinimumValue, and one optional short column - Ignore: \n\
               which set the flags of whether ignore the pvs in the waveform. If Ignore column \n\
               does not exist, then the readbacks and controls will consider to be testing pvs \n\
               in the waveforms. \n\n\
Program by Louis Emery, ANL\n\
Link date: " __DATE__ " " __TIME__ ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";
#  ifndef USE_RUNCONTROL
  char *USAGE_WARNING = "** note ** Program is not compiled with run control.\n";
#  else
  char *USAGE_WARNING = "";
  double lastRCPingTime = 0.0;
#  endif
#endif
  int pingSkipIteration = 0;
  double pendIOTime;
  long verbose, warning, rampDone;
  char *commandFile;
#if defined(vxWorks)
  double wait = 0;
#endif
#ifdef DEBUGTIMES
  int debugTimes = 0;
  double debugTime[100];
  for (i = 0; i < 99; i++)
    debugTime[i] = 0;
#endif
  outputRoot = NULL;
  caWriteError = 0;
  rampDone = 0;

#if !defined(vxWorks)
  SDDS_RegisterProgramName("sddscontrollaw");
#endif

  SDDS_CheckDatasetStructureSize(sizeof(SDDS_DATASET));
#if defined(vxWorks)
  if ((buffer = malloc(sizeof(char) * (strlen(options) + 1 + 15))) == NULL) {
    fprintf(stderr, "can't malloc buffer\n");
    return (1);
  }
  sprintf(buffer, "sddscontrollaw %s", options);
  argc = parse_string(&argv, buffer);
  if (buffer)
    free(buffer);
  buffer = NULL;
#  if defined(TASKVAR)
  taskVarInit();
  if (taskVarAdd(0, (int *)&sddscontrollawGlobal) != OK) {
    fprintf(stderr, "can't taskVarAdd sddscontrollawGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
  if ((sddscontrollawGlobal = (SDDSCONTROLLAW_GLOBAL *)calloc(1, sizeof(SDDSCONTROLLAW_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddscontrollawGlobal\n");
    for (i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
    return (1);
  }
#  else
  if ((sddscontrollawGlobal = (SDDSCONTROLLAW_GLOBAL *)calloc(1, sizeof(SDDSCONTROLLAW_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddscontrollawGlobal\n");
    return (1);
  }
#  endif
#else
  if ((sddscontrollawGlobal = (SDDSCONTROLLAW_GLOBAL *)calloc(1, sizeof(SDDSCONTROLLAW_GLOBAL))) == NULL) {
    fprintf(stderr, "can't malloc sddscontrollawGlobal\n");
    return (1);
  }
#endif
#if defined(vxWorks)

  sddscontrollawGlobal->argc = argc;
  sddscontrollawGlobal->argv = &argv;

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

#if !defined(DBAccess)
#  ifdef EPICS313
  ca_task_initialize();
#  else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#  endif
  ca_add_exception_event(oag_ca_exception_handler, NULL);
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc == 1) {
#if defined(vxWorks)
    fprintf(stderr, "invalid sddscontrollaw command line\n");
#else
    fprintf(stderr, "%s%s%s%s%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4, USAGE5, USAGE6, USAGE7, USAGE8);
    fprintf(stderr, "%s", USAGE_WARNING);
#endif
    free_scanargs(&s_arg, argc);
    FreeEverything();
    return (1);
  }
  free_scanargs(&s_arg, argc);

  firstTime = 1;
  initializeStatsData(&readbackStats);
  initializeStatsData(&readbackDeltaStats);
  initializeStatsData(&readbackAdjustedStats);
  initializeStatsData(&controlDeltaStats);

  initializeStatsData(&readbackCompStats);
  initializeStatsData(&readbackCompDeltaStats);
  initializeStatsData(&readbackCompAdjustedStats);
  initializeStatsData(&controlCompStats);

  sddscontrollawGlobal->waveform_tests.longestHoldOff = 1;

  controlStats.msg[0] = 0;
  if (parseArguments(&argv,
                     &argc,
                     &sddscontrollawGlobal->correction,
                     &sddscontrollawGlobal->loopParam,
                     &aveParam,
                     &sddscontrollawGlobal->delta,
                     &sddscontrollawGlobal->readbackLimits,
                     &sddscontrollawGlobal->action,
                     &sddscontrollawGlobal->despikeParam,
                     &sddscontrollawGlobal->glitchParam,
                     &sddscontrollawGlobal->test,
                     &sddscontrollawGlobal->statsFile,
                     &serverMode,
                     &commandFile,
                     &sddscontrollawGlobal->outputFile,
                     &sddscontrollawGlobal->controlLogFile,
                     &testCASecurity,
                     &generations,
                     &sddscontrollawGlobal->overlapCompensation,
                     &compensationOutputFile,
                     &sddscontrollawGlobal->readbackWaveforms,
                     &sddscontrollawGlobal->controlWaveforms,
                     &sddscontrollawGlobal->offsetWaveforms,
                     &sddscontrollawGlobal->ffSetpointWaveforms,
                     &sddscontrollawGlobal->waveform_tests,
                     firstTime,
                     &verbose,
                     &warning,
                     &pendIOTime,
                     commandline_option,
                     waveformOption)) {
    fprintf(stderr, "Problem with parsing arguments.\n");
    FreeEverything();
    return (1);
  }
  /*set search path */
  if (sddscontrollawGlobal->loopParam.searchPath)
    setSearchPath(sddscontrollawGlobal->loopParam.searchPath);
  setupData(&sddscontrollawGlobal->correction,
            &sddscontrollawGlobal->delta,
            &sddscontrollawGlobal->readbackLimits,
            &sddscontrollawGlobal->action,
            &sddscontrollawGlobal->loopParam,
            &sddscontrollawGlobal->despikeParam,
            &sddscontrollawGlobal->test,
            &sddscontrollawGlobal->overlapCompensation,
            &sddscontrollawGlobal->readbackWaveforms,
            &sddscontrollawGlobal->controlWaveforms,
            &sddscontrollawGlobal->offsetWaveforms,
            &sddscontrollawGlobal->ffSetpointWaveforms,
            &sddscontrollawGlobal->waveform_tests,
            &sddscontrollawGlobal->glitchParam,
            verbose,
            pendIOTime);
  readback = sddscontrollawGlobal->correction.readback;
  control = sddscontrollawGlobal->correction.control;
  if (sddscontrollawGlobal->overlapCompensation.file) {
    readbackComp = sddscontrollawGlobal->overlapCompensation.readback;
    controlComp = sddscontrollawGlobal->overlapCompensation.control;
    if (sddscontrollawGlobal->overlapCompensation.file && (readbackComp->n != control->n)) {
      FreeEverything();
      SDDS_Bomb("Size of auxiliary output file is not compatible with correction file.");
    }
  }

  if (!sddscontrollawGlobal->correction.file) {
    FreeEverything();
    SDDS_Bomb("input filename not given");
  }
  if (sddscontrollawGlobal->loopParam.updateInterval > sddscontrollawGlobal->loopParam.steps)
    sddscontrollawGlobal->loopParam.updateInterval = sddscontrollawGlobal->loopParam.steps;
  if (aveParam.n > 1) {
    if (sddscontrollawGlobal->loopParam.interval < (aveParam.n - 1) * aveParam.interval) {
      fprintf(stderr, "Time interval between corrections (%e) is too short for the averaging requested (%ldx%e).\n", sddscontrollawGlobal->loopParam.interval, (aveParam.n - 1), aveParam.interval);
      FreeEverything();
      return (1);
    }
  }

  if (serverMode && (sddscontrollawGlobal->outputFile || sddscontrollawGlobal->loopParam.holdPresentValues)) {
    FreeEverything();
    SDDS_Bomb("Server mode is incompatible with output file logging and holdPresentValues.");
  }

  firstTime = 0;

  if (serverMode) {
#if !defined(vxWorks)
    /* if a server is already running in this directory
         then exit since we don't want two sddscontrollaw running */
    exitIfServerRunning();
    /* if a server is not already running then start one */
    setupServer();
#endif
  }

#ifdef USE_LOGDAEMON
#  ifdef USE_RUNCONTROL
  sddscontrollawGlobal->useLogDaemon = 0;
  if (sddscontrollawGlobal->rcParam.PV) {
    switch (logOpen(&sddscontrollawGlobal->logHandle, NULL, "controllawAudit", "Instance Action")) {
    case LOG_OK:
      sddscontrollawGlobal->useLogDaemon = 1;
      break;
    case LOG_INVALID:
      fprintf(stderr, "warning: logDaemon rejected given sourceId and tagList. logDaemon messages not logged.\n");
      break;
    case LOG_ERROR:
      fprintf(stderr, "warning: unable to open connection to logDaemon. logDaemon messages not logged.\n");
      break;
    case LOG_TOOBIG:
      fprintf(stderr, "warning: one of the string supplied is too long. logDaemon messages not logged.\n");
      break;
    default:
      fprintf(stderr, "warning: unrecognized return code from logOpen.\n");
      break;
    }
  }
#  endif
#endif

#ifdef USE_RUNCONTROL
  if (sddscontrollawGlobal->rcParam.PV) {
    sddscontrollawGlobal->rcParam.handle[0] = (char)0;
    sddscontrollawGlobal->rcParam.Desc = (char *)SDDS_Realloc(sddscontrollawGlobal->rcParam.Desc, 41 * sizeof(char));
    sddscontrollawGlobal->rcParam.PV = (char *)SDDS_Realloc(sddscontrollawGlobal->rcParam.PV, 41 * sizeof(char));
    sddscontrollawGlobal->rcParam.status = runControlInit(sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc, sddscontrollawGlobal->rcParam.pingTimeout, sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo), pendIOTime);
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Error initializing run control.\n");
      if (sddscontrollawGlobal->rcParam.status == RUNCONTROL_DENIED) {
        fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
      }
      FreeEverything();
      return (1);
    }
    sddscontrollawGlobal->rcParam.init = 1;
    lastRCPingTime = getTimeInSecs();

    strcpy(sddscontrollawGlobal->rcParam.message, "Running");
    sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      FreeEverything();
      return (1);
    }
  }
#endif

  /*******************************\
   * define table for output files *
   \*******************************/
  getTimeBreakdown(&startTime, NULL, &startHour, NULL, NULL, NULL, &timeStamp);

  if (!serverMode) {
    if (generations.doGenerations) {
      if (!generations.daily) {
        sddscontrollawGlobal->outputFile = MakeGenerationFilename(outputRoot = sddscontrollawGlobal->outputFile, generations.digits, generations.delimiter, NULL);
      } else {
        /* when daily files are requested then the variable outputRoot
	         contains the root part of the output file names.
	       */
        sddscontrollawGlobal->outputFile = MakeDailyGenerationFilename(outputRoot = sddscontrollawGlobal->outputFile, generations.digits, generations.delimiter, 0);
      }
      HourNow = getHourOfDay();
    }
    setupOutputFile(&sddscontrollawGlobal->outputPage, sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
    if (sddscontrollawGlobal->overlapCompensation.file && compensationOutputFile) {
      fprintf(stderr, "Setting up file %s.\n", compensationOutputFile);
      setupOutputFile(&compensationOutputPage, compensationOutputFile, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
    }
  }

  setupStatsFile(&sddscontrollawGlobal->statsPage, sddscontrollawGlobal->statsFile, &sddscontrollawGlobal->loopParam, timeStamp, verbose);

  setupGlitchFile(&sddscontrollawGlobal->glitchPage, &sddscontrollawGlobal->glitchParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, timeStamp, verbose);
  /*********************\
   * Initial values    *
   \*********************/
  getInitialValues(&sddscontrollawGlobal->correction, &aveParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->readbackWaveforms, &sddscontrollawGlobal->controlWaveforms, verbose, pendIOTime);
  if (sddscontrollawGlobal->correction.coefFile)
    initializeHistories(&sddscontrollawGlobal->correction);

  if (sddscontrollawGlobal->overlapCompensation.file) {
    /*here, controlWaveforms is corresponding to readbackComp, and
         ffSetpointWaveforms is corresponding to controlComp */
    getInitialValues(&sddscontrollawGlobal->overlapCompensation, &aveParam, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->controlWaveforms, &sddscontrollawGlobal->ffSetpointWaveforms, verbose, pendIOTime);
    initializeHistories(&sddscontrollawGlobal->overlapCompensation);
  }

  getTimeBreakdown(&startTime, NULL, NULL, NULL, NULL, NULL, NULL);
  targetTime = startTime;
  /*********************\
   * Iteration loop    *
   \*********************/
  if (warning || verbose) {
    backoff.level = 1;
    backoff.counter = 0;
  }

  if (verbose)
    fprintf(stderr, "Starting loop of %ld steps.\n", sddscontrollawGlobal->loopParam.steps);
  outputRow = 0;             /* row is incremented only when writing to the 
				   output file (i.e. successful iteration) */
  compensationOutputRow = 0; /* row is incremented only when writing to the 
				   compensation output file (i.e. successful iteration) */
  statsRow = 0;
  glitchRow = 0;
  testOutOfRange = 0;
  outOfRange = 0;
  waveformOutOfRange = 0;
  if (generations.rowLimit)
    newFileCountdown = generations.rowLimit;
  /* row numbering in SDDS pages starts at 0 */

  if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
    setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Iterations started", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
    setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 1);
#if !defined(DBAccess)
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "pendIOerror: unable to put PV values\n");
      FreeEverything();
      exit(1);
    }
#endif
  }

  if (sddscontrollawGlobal->loopParam.triggerProvided && !sddscontrollawGlobal->loopParam.trigger.initialized) {
    fprintf(stderr, "Waiting for trigger connection event\n");
    while (!sddscontrollawGlobal->loopParam.trigger.initialized) {
#ifdef USE_RUNCONTROL
      if (sddscontrollawGlobal->rcParam.PV) {
        if (runControlPingWhileSleep(0.0)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          FreeEverything();
          return (1);
        }
      } else
        oag_ca_pend_event(1.0, &(sddscontrollawGlobal->sigint));
#else
    oag_ca_pend_event(1.0, &(sddscontrollawGlobal->sigint));
#endif
    }
  }
  
  for (sddscontrollawGlobal->loopParam.step[0] = 0; sddscontrollawGlobal->loopParam.step[0] < sddscontrollawGlobal->loopParam.steps; sddscontrollawGlobal->loopParam.step[0]++) {
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[0] = getTimeInSecs();
#endif

    if (sddscontrollawGlobal->loopParam.triggerProvided) {
      if (verbose)
        fprintf(stderr, "Waiting for trigger event\n");
      while (!sddscontrollawGlobal->loopParam.trigger.triggered) {
#ifdef USE_RUNCONTROL
        if (sddscontrollawGlobal->rcParam.PV) {
          if (runControlPingWhileSleep(0.0)) {
            fprintf(stderr, "Problem pinging the run control record.\n");
            FreeEverything();
            return (1);
          }
        } else
          oag_ca_pend_event(0.001, &(sddscontrollawGlobal->sigint));
#else
        oag_ca_pend_event(0.001, &(sddscontrollawGlobal->sigint));
#endif
      }
      if (verbose)
        fprintf(stderr, "Trigger event received\n");
      sddscontrollawGlobal->loopParam.trigger.triggered = 0;
    }
    
    if (sddscontrollawGlobal->sigint) {
      FreeEverything();
      return (1);
    }

#if defined(vxWorks)
    if (sddscontrollawGlobal->loopParam.launcherPV[2]) {
      readPVs(&sddscontrollawGlobal->loopParam.launcherPV[2], &wait, &sddscontrollawGlobal->loopParam.launcherPVInfo[2], 1, NULL, pendIOTime);
      if (wait > .5) {
        if (wait > 1.5) {
          FreeEverything();
          return (1);
        }
        setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Task suspended", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
#  if !defined(DBAccess)
        if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
          fprintf(stderr, "pendIOerror: unable to put PV values\n");
          FreeEverything();
          exit(1);
        }
#  endif
        if (taskSuspend(0) == ERROR) {
          FreeEverything();
          return (1);
        }
        continue;
      }
    }
#endif
    if (sddscontrollawGlobal->loopParam.averagePV) {
      pre_n = aveParam.n;
      readPVs(&sddscontrollawGlobal->loopParam.averagePV, &aveParam.n2, &sddscontrollawGlobal->loopParam.averagePVInfo, 1, NULL, pendIOTime);
      aveParam.n = round(aveParam.n2);
    }
    if (sddscontrollawGlobal->loopParam.intervalPV) {
      readPVs(&sddscontrollawGlobal->loopParam.intervalPV, &sddscontrollawGlobal->loopParam.interval, &sddscontrollawGlobal->loopParam.intervalPVInfo, 1, NULL, pendIOTime);
    }
    if (sddscontrollawGlobal->despikeParam.rampThresholdPV && sddscontrollawGlobal->despikeParam.startThreshold != sddscontrollawGlobal->despikeParam.endThreshold) {
      readEnumPV(sddscontrollawGlobal->despikeParam.rampThresholdPV, &sddscontrollawGlobal->despikeParam.reramp, sddscontrollawGlobal->despikeParam.rampThresholdPVInfo, pendIOTime, 0);
      if (sddscontrollawGlobal->despikeParam.reramp) {
        if (verbose)
          fprintf(stderr, "Re-ramp despike threshold.\n");
        sddscontrollawGlobal->despikeParam.reramp = 0;
        sddscontrollawGlobal->despikeParam.threshold = sddscontrollawGlobal->despikeParam.startThreshold - sddscontrollawGlobal->despikeParam.deltaThreshold;
        setEnumPV(sddscontrollawGlobal->despikeParam.rampThresholdPV, sddscontrollawGlobal->despikeParam.reramp, sddscontrollawGlobal->despikeParam.rampThresholdPVInfo, 0);
        rampDone = 0;
      }
    }
    if (fabs(sddscontrollawGlobal->despikeParam.threshold - sddscontrollawGlobal->despikeParam.endThreshold) <= 1e-6)
      rampDone = 1;
    /*the endThreshold is smaller than the start threshold and deltaThreshold is negative */
    if (sddscontrollawGlobal->despikeParam.flag & DESPIKE_STARTTHRESHOLD && !outOfRange && !rampDone) {
      sddscontrollawGlobal->despikeParam.threshold += sddscontrollawGlobal->despikeParam.deltaThreshold;
      if (sddscontrollawGlobal->despikeParam.threshold < sddscontrollawGlobal->despikeParam.endThreshold)
        sddscontrollawGlobal->despikeParam.threshold = sddscontrollawGlobal->despikeParam.endThreshold;
      if (sddscontrollawGlobal->despikeParam.thresholdPV) {
        /* send the ramped value to the despiking PV */
        setPVs(&sddscontrollawGlobal->despikeParam.thresholdPV, &sddscontrollawGlobal->despikeParam.threshold, &sddscontrollawGlobal->despikeParam.thresholdPVInfo, 1, pendIOTime);
      }
    }

    if (verbose)
      fprintf(stderr, "Despike threshold %f\n", sddscontrollawGlobal->despikeParam.threshold);
    if (sddscontrollawGlobal->despikeParam.thresholdPV) {
      readPVs(&sddscontrollawGlobal->despikeParam.thresholdPV, &sddscontrollawGlobal->despikeParam.threshold, &sddscontrollawGlobal->despikeParam.thresholdPVInfo, 1, NULL, pendIOTime);
    }
    if (sddscontrollawGlobal->despikeParam.threshold < 0)
      sddscontrollawGlobal->despikeParam.threshold = 0;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[1] = getTimeInSecs();
#endif
    if ((sddscontrollawGlobal->loopParam.intervalPV) || (sddscontrollawGlobal->loopParam.averagePV)) {
      if ((pre_n > 1) && (!sddscontrollawGlobal->loopParam.intervalPV))
        sddscontrollawGlobal->loopParam.interval += (pre_n - 1) * aveParam.interval;
      if (aveParam.n > 1) {
        if (sddscontrollawGlobal->loopParam.interval < (aveParam.n - 1) * aveParam.interval) {
          fprintf(stderr, "Time interval between corrections (%e) is too short for the averaging requested (%ldx%e).\n", sddscontrollawGlobal->loopParam.interval, (aveParam.n - 1), aveParam.interval);
          FreeEverything();
          return (1);
        }
      }
    }
    targetTime += sddscontrollawGlobal->loopParam.interval; /* time at which the loop should
								   return to the beginning */
    /* pingSkipIteration is used to detect if the program was suspended. If it was we do not want to use old data collected
         before it was suspended so we will skip this iteration */
    pingSkipIteration = 0;
#ifdef USE_RUNCONTROL
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        /* ping once at the beginning */
        if (runControlPingWhileSleep(0.0)) {
          fprintf(stderr, "Problem pinging the run control record.\n");
          FreeEverything();
          return (1);
        }
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }
#endif
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    if (!(sddscontrollawGlobal->loopParam.step[0] % 2))
      startReadTime = sddscontrollawGlobal->loopParam.elapsedTime[0];
    else
      endReadTime = sddscontrollawGlobal->loopParam.elapsedTime[0];
    previousAverage = averageTime;
    if (sddscontrollawGlobal->loopParam.step[0]) {
      if (sddscontrollawGlobal->loopParam.step[0] == 1)
        averageTime = fabs(endReadTime - startReadTime);
      else
        averageTime = 0.1 * (fabs(endReadTime - startReadTime)) + 0.9 * previousAverage;
      if (verbose)
        fprintf(stderr, "average interval time: %f\n\n", averageTime);
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[2] = getTimeInSecs();
#endif
    if (getReadbackValues(readback, &aveParam, &sddscontrollawGlobal->loopParam, &readbackStats, &readbackDeltaStats, &sddscontrollawGlobal->readbackWaveforms, &sddscontrollawGlobal->offsetWaveforms, verbose, pendIOTime)) {
      FreeEverything();
      SDDS_Bomb("Error code return from getReadbackValues.");
    }
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[3] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->overlapCompensation.file) {
      if (getReadbackValues(readbackComp, &aveParam, &sddscontrollawGlobal->loopParam, &readbackCompStats, &readbackCompDeltaStats, &sddscontrollawGlobal->controlWaveforms, NULL, verbose, pendIOTime)) {
        FreeEverything();
        SDDS_Bomb("Error code return from getReadbackValues.");
      }
    }

    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    timeOfDay = startHour + sddscontrollawGlobal->loopParam.elapsedTime[0] / 3600.0;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[4] = getTimeInSecs();
#endif

    adjustReadbacks(readback, &sddscontrollawGlobal->readbackLimits, &sddscontrollawGlobal->despikeParam, &readbackAdjustedStats, verbose);
    for (i = 0; i < readback->n; i++)
      if (isnan(readback->value[0][i]))
        fprintf(stderr, "Error the value of PV %s (index=%ld) is not a number (nan)\n", readback->controlName[i], i);
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[5] = getTimeInSecs();
#endif
#ifdef USE_RUNCONTROL
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        runControlPingWhileSleep(0.0);
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }
#endif

    /* The compensation vector readbackComp doesn't need any adjustment, such
         as despiking or applying a limit. Besides, the despikin file and limit
         files are defined for bpms only.
       */
    /*
         if (overlapCompensation.file) {
         adjustReadbacks( readbackComp, 
         &readbackLimits,
         &despikeParam,
         &readbackCompAdjustedStats, verbose);
         }
       */
    if ((skipIteration = checkActionLimit(&sddscontrollawGlobal->action, readback)) != 0) {
      if (warning || verbose)
        fprintf(stderr, "Readback values are less than the action limit. Skipping correction.\n");
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[6] = getTimeInSecs();
#endif

    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
    if (getControlDevices(control, &controlStats, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->controlWaveforms, verbose, pendIOTime)) {
      FreeEverything();
      SDDS_Bomb("Error code return from getControlDevices");
    }
    sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[7] = getTimeInSecs();
#endif
    if (sddscontrollawGlobal->overlapCompensation.file) {
      if (getControlDevices(controlComp, &controlCompStats, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->ffSetpointWaveforms, verbose, pendIOTime)) {
        FreeEverything();
        SDDS_Bomb("Error code return from getControlDevices");
      }
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[8] = getTimeInSecs();
#endif
#ifdef USE_RUNCONTROL
    if (sddscontrollawGlobal->rcParam.PV) {
      if (getTimeInSecs() >= lastRCPingTime + 2) {
        lastRCPingTime = getTimeInSecs();
        runControlPingWhileSleep(0.0);
        if ((getTimeInSecs() - lastRCPingTime) > 5.0)
          pingSkipIteration = 1;
      }
    }
#endif

    /********************\*
       * test values        *
     \*********************/

    if (sddscontrollawGlobal->test.file || sddscontrollawGlobal->waveform_tests.waveformTests) {
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      if (sddscontrollawGlobal->test.file) {
        if (verbose)
          fprintf(stderr, "Reading plain test devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
        readPVs(sddscontrollawGlobal->test.controlName, sddscontrollawGlobal->test.value, sddscontrollawGlobal->test.channelInfo, sddscontrollawGlobal->test.n, NULL, pendIOTime);
      }
#ifdef USE_RUNCONTROL
      if (sddscontrollawGlobal->rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0);
          if ((getTimeInSecs() - lastRCPingTime) > 5.0)
            pingSkipIteration = 1;
        }
      }
#endif
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[9] = getTimeInSecs();
#endif
      despikeTestValues(&sddscontrollawGlobal->test, &sddscontrollawGlobal->despikeParam, verbose);
      prevOutOfRange = outOfRange;

#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[10] = getTimeInSecs();
#endif
      if (sddscontrollawGlobal->test.file)
        testOutOfRange = checkOutOfRange(&sddscontrollawGlobal->test, &backoff, &readbackStats, &readbackAdjustedStats, &controlStats, &sddscontrollawGlobal->loopParam, timeOfDay, verbose, warning);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[11] = getTimeInSecs();
#endif
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      waveformOutOfRange = CheckWaveformTest(&sddscontrollawGlobal->waveform_tests, &backoff, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->despikeParam, verbose, warning, pendIOTime);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[12] = getTimeInSecs();
#endif
      if (waveformOutOfRange || testOutOfRange) {
        /*if out of range, set the control delta to 0 */
        outOfRange = 1;
        for (icontrol = 0; icontrol < control->n; icontrol++) {
          control->delta[0][icontrol] = 0;
          control->old[icontrol] = control->value[0][icontrol];
        }
        calcControlDeltaStats(control, &controlStats, &controlDeltaStats);
      } else
        outOfRange = 0;
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[13] = getTimeInSecs();
#endif
      if (outOfRange) {
        if (prevOutOfRange == 1) {
          if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Out-of-range test variable(s)", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 1, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 1);
#if !defined(DBAccess)
            if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
              fprintf(stderr, "pendIOerror: unable to put PV values\n");
              FreeEverything();
              exit(1);
            }
#endif
          }
        }
#ifdef USE_RUNCONTROL
        if (sddscontrollawGlobal->rcParam.PV) {
          /* This messages must be re-written at every iteration
		     because the runcontrolMessage can be written by another process
		     such as the tcl interface that runs sddscontrollaw */
          strcpy(sddscontrollawGlobal->rcParam.message, "Out-of-range test variable(s)");
          sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
          if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            FreeEverything();
            return (1);
          }
#  ifdef USE_LOGDAEMON
          if (!prevOutOfRange && sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
            switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "Out-of-range", NULL)) {
            case LOG_OK:
              break;
            case LOG_ERROR:
            case LOG_TOOBIG:
              fprintf(stderr, "warning: something wrong in calling logArguments.\n");
              break;
            default:
              fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
              break;
            }
          }
#  endif
        }
#endif
      } else {
        if (prevOutOfRange && (warning || verbose)) {
          fprintf(stderr, "Iterations restarted.\n");
          /* reset backoff counter */
          backoff.counter = 0;
          backoff.level = 1;
        }
        if (sddscontrollawGlobal->loopParam.launcherPV[0] && !caWriteError) {
          if (prevOutOfRange == 0) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Iterations re-started", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 1);
#if !defined(DBAccess)
            if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
              fprintf(stderr, "pendIOerror: unable to put PV values\n");
              FreeEverything();
              exit(1);
            }
#endif
          }
        }
#ifdef USE_RUNCONTROL
        /* These messages must be re-written at every iteration
	         because the runcontrolMessage can be written by another process
	         such as the tcl interface that runs sddscontrollaw */
        if (sddscontrollawGlobal->rcParam.PV && !caWriteError) {
          if (sddscontrollawGlobal->loopParam.step[0] == 0)
            strcpy(sddscontrollawGlobal->rcParam.message, "Iterations started");
          else
            strcpy(sddscontrollawGlobal->rcParam.message, "Iterations re-started");
          sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
          if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
            fprintf(stderr, "Unable to write status message and alarm severity\n");
            FreeEverything();
            return (1);
          }
        }
        /* The "#if defined()" preprocessor command doesn't work in some compilers. */
#  ifdef USE_LOGDAEMON
        if (prevOutOfRange && sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
          switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "In-range", NULL)) {
          case LOG_OK:
            break;
          case LOG_ERROR:
          case LOG_TOOBIG:
            fprintf(stderr, "warning: something wrong in calling logArguments.\n");
            break;
          default:
            fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
            break;
          }
        }
#  endif
#endif
      }
    }

    /*********************\
       * control law       *
     \*********************/
    /* this skips an iteration if the previous step
         was out of range and averaging is requested. */
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[14] = getTimeInSecs();
#endif
    if ((!(outOfRange || ((aveParam.n > 1) && prevOutOfRange))) && !(pingSkipIteration)) {
      if (prevOutOfRange && (sddscontrollawGlobal->test.holdOff || sddscontrollawGlobal->waveform_tests.holdOffPresent)) {
        sleepTime = MAX(sddscontrollawGlobal->test.longestHoldOff, sddscontrollawGlobal->waveform_tests.longestHoldOff);
        if (verbose)
          fprintf(stderr, "Hold for %f secondes after tests passed.\n", sleepTime);
        /* targetTime += sleepTime; */
        /*recompute the target time with hold off */
        targetTime = getTimeInSecs() + sleepTime + sddscontrollawGlobal->loopParam.interval;
#if defined(DBAccess)
        microSeconds = (long)(1e6 * sleepTime);
        if (microSeconds > 0)
          usleepSystemIndependent(microSeconds);
#else
        oag_ca_pend_event(sleepTime, &(sddscontrollawGlobal->sigint));
#endif
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[15] = getTimeInSecs();
#endif
      sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
      if (verbose)
        fprintf(stderr, "Calling controlLaw function at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
      controlLaw(skipIteration, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, verbose, pendIOTime);

#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[16] = getTimeInSecs();
#endif
      /* for now compensation doesn't work with applyDeltaLimit */
      factor = applyDeltaLimit(&sddscontrollawGlobal->delta, &sddscontrollawGlobal->loopParam, control, verbose, warning);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[17] = getTimeInSecs();
#endif

      calcControlDeltaStats(control, &controlStats, &controlDeltaStats);
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[18] = getTimeInSecs();
#endif
      if (sddscontrollawGlobal->overlapCompensation.file) {
        if (factor < 1) {
          for (i = 0; i < controlComp->n; i++) {
            controlComp->delta[0][i] *= factor;
            controlComp->value[0][i] = controlComp->old[i] + controlComp->delta[0][i];
            /*for proportional mode, controlComp->old[i] is set to 0 */
          }
        }
        calcControlDeltaStats(controlComp, &controlCompStats, &controlCompDeltaStats);
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[19] = getTimeInSecs();
#endif

      if (!sddscontrollawGlobal->loopParam.dryRun) {
        sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;

        if (verbose) {
          fprintf(stderr, "Testing access of control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
        }
        caWriteError = 0;

#ifdef DEBUGTIMES
        if (debugTimes)
          debugTime[20] = getTimeInSecs();
#endif
        if (testCASecurity && (caWriteError = CheckCAWritePermissionMod(control->controlName, control->channelInfo, control->n))) {
          fprintf(stderr, "Write access denied to at least one PV.\n");

#ifdef USE_RUNCONTROL
          if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
            setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "CA Security denies access.", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
            setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 1, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 1);
#  if !defined(DBAccess)
            if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
              fprintf(stderr, "pendIOerror: unable to put PV values\n");
              FreeEverything();
              exit(1);
            }
#  endif
          }
          if (sddscontrollawGlobal->rcParam.PV) {
            /* This messages must be re-written at every iteration
		         because the runcontrolMessage can be written by another process
		         such as the tcl interface that runs sddscontrollaw */
            strcpy(sddscontrollawGlobal->rcParam.message, "CA Security denies access.");
            sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
            if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
              fprintf(stderr, "Unable to write status message and alarm severity\n");
              FreeEverything();
              return (1);
            }
#  ifdef USE_LOGDAEMON
            if (sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
              switch (logArguments(sddscontrollawGlobal->logHandle, sddscontrollawGlobal->rcParam.PV, "CA denied", NULL)) {
              case LOG_OK:
                break;
              case LOG_ERROR:
              case LOG_TOOBIG:
                fprintf(stderr, "warning: something wrong in calling logArguments.\n");
                break;
              default:
                fprintf(stderr, "warning: unrecognized return code from logArguments.\n");
                break;
              }
            }
#  endif
          }
#endif
        }
#ifdef DEBUGTIMES
        if (debugTimes)
          debugTime[21] = getTimeInSecs();
#endif
        if (!caWriteError) {
          sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
          if (verbose) {
            fprintf(stderr, "Setting control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
          }
          if (sddscontrollawGlobal->controlWaveforms.waveforms) {
            if (WriteWaveformData(&sddscontrollawGlobal->controlWaveforms, control, pendIOTime)) {
              FreeEverything();
              return (1);
            }
          } else {
            if (setPVs(control->controlName, control->value[0], control->channelInfo, control->n, pendIOTime)) {
              fprintf(stderr, "Error in setting PVs of the control variables.\n");
            }
          }
          if (sddscontrollawGlobal->loopParam.postChangeExec) {
            /*run post change execution */
            if (runPostChangeExec(sddscontrollawGlobal->loopParam.postChangeExec, control, verbose)) {
              fprintf(stderr, "Error in running post change execution %s\n", sddscontrollawGlobal->loopParam.postChangeExec);
              FreeEverything();
              return (1);
            }
          }
#ifdef DEBUGTIMES
          if (debugTimes)
            debugTime[22] = getTimeInSecs();
#endif
          if (sddscontrollawGlobal->overlapCompensation.file) {
            sddscontrollawGlobal->loopParam.elapsedTime[0] = (sddscontrollawGlobal->loopParam.epochTime[0] = getTimeInSecs()) - startTime;
            if (verbose) {
              fprintf(stderr, "Setting compensating control devices at %f seconds.\n", sddscontrollawGlobal->loopParam.elapsedTime[0]);
            }
            if (sddscontrollawGlobal->ffSetpointWaveforms.waveforms) {
              if (WriteWaveformData(&sddscontrollawGlobal->ffSetpointWaveforms, controlComp, pendIOTime)) {
                FreeEverything();
                return (1);
              }
            } else {
              if (setPVs(controlComp->controlName, controlComp->value[0], controlComp->channelInfo, controlComp->n, pendIOTime)) {
                fprintf(stderr, "Error in setting PVs of the compensation control variables.\n");
              }
            }
          }
#ifdef DEBUGTIMES
          if (debugTimes)
            debugTime[23] = getTimeInSecs();
#endif
        }
      }
      if (sddscontrollawGlobal->controlLogFile) {
        getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &timeStamp);
        logActuator(sddscontrollawGlobal->controlLogFile, control, timeStamp);
      }
#ifdef DEBUGTIMES
      if (debugTimes)
        debugTime[24] = getTimeInSecs();
#endif
      if (verbose) {
        fprintf(stderr, "Stats                     Average       rms        "
                        "mad     Largest\n"
                        "%s\n%s%s\n%s\n%s\n",
                readbackStats.msg, readbackAdjustedStats.msg, readbackDeltaStats.msg, controlStats.msg, controlDeltaStats.msg);
        if (sddscontrollawGlobal->overlapCompensation.file) {
          fprintf(stderr, "For compensation...\nStats                     "
                          "Average       rms        mad     Largest\n"
                          "%s\n%s\n%s\n%s\n",
                  readbackCompStats.msg, readbackCompDeltaStats.msg, controlCompStats.msg, controlCompDeltaStats.msg);
        }
      }

#ifdef USE_RUNCONTROL
      if (sddscontrollawGlobal->rcParam.PV) {
        if (getTimeInSecs() >= lastRCPingTime + 2) {
          lastRCPingTime = getTimeInSecs();
          runControlPingWhileSleep(0.0);
        }
      }
#endif
    }

    if (!serverMode) {
      LastHour = HourNow;
      HourNow = getHourOfDay();
      if (generations.doGenerations) {
        if ((generations.daily && HourNow < LastHour) || ((generations.timeLimit > 0 || generations.rowLimit) && !newFileCountdown)) {
          /* must start a new file */
          if (!SDDS_Terminate(&sddscontrollawGlobal->outputPage))
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          outputRow = 0;             /* row is incremented only when writing to the 
					   output file (i.e. successful iteration) */
          compensationOutputRow = 0; /* row is incremented only when writing to the 
						   compensation output file (i.e. successful iteration) */
          if (!generations.daily) {
            /* user-specified generation files */
            char *ptr = NULL;
            if (generations.rowLimit)
              newFileCountdown = generations.rowLimit;
            else
              newFileCountdown = -1;
            generations.timeStop = getTimeInSecs() + generations.timeLimit;
            sddscontrollawGlobal->outputFile = MakeGenerationFilename(outputRoot, generations.digits, generations.delimiter, ptr = sddscontrollawGlobal->outputFile);
            if (ptr)
              free(ptr);
          } else {
            /* "daily" log files with the OAG log name format */
            sddscontrollawGlobal->outputFile = MakeDailyGenerationFilename(outputRoot, generations.digits, generations.delimiter, 0);
          }
          fprintf(stderr, "Setting up file %s \n", sddscontrollawGlobal->outputFile);
          setupOutputFile(&sddscontrollawGlobal->outputPage, sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->loopParam, timeStamp, &sddscontrollawGlobal->waveform_tests, verbose);
          if (verbose) {
            fprintf(stderr, "New file %s started\n", sddscontrollawGlobal->outputFile);
          }
        }
      }

      writeToOutputFile(sddscontrollawGlobal->outputFile, &sddscontrollawGlobal->outputPage, &outputRow, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->test, &sddscontrollawGlobal->waveform_tests);
      if (sddscontrollawGlobal->overlapCompensation.file && compensationOutputFile) {
        writeToOutputFile(compensationOutputFile, &compensationOutputPage, &compensationOutputRow, &sddscontrollawGlobal->loopParam, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test, &sddscontrollawGlobal->waveform_tests);
      }
    }
    writeToStatsFile(sddscontrollawGlobal->statsFile, &sddscontrollawGlobal->statsPage, &statsRow, &sddscontrollawGlobal->loopParam, &readbackStats, &readbackDeltaStats, &controlStats, &controlDeltaStats);
    writeToGlitchFile(&sddscontrollawGlobal->glitchParam, &sddscontrollawGlobal->glitchPage, &glitchRow, &sddscontrollawGlobal->loopParam, &readbackAdjustedStats, &controlDeltaStats, &sddscontrollawGlobal->correction, &sddscontrollawGlobal->overlapCompensation, &sddscontrollawGlobal->test);

    /*******************************\
       * pause for iteration interval *
     \******************************/
    /* calculate time that is left to the iteration */
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[25] = getTimeInSecs();
#endif
    timeLeft = targetTime - getTimeInSecs();
    if (timeLeft < 0) {
      /* if the runcontrol PV had been paused for a long time, say, the target
	     time is no longer valid, since it is substantially in the past.
	     The target time should be reset to now.
	   */

      targetTime = getTimeInSecs();
      timeLeft = 0;
    }
    if (testOutOfRange)
      sleepTime = MAX(sddscontrollawGlobal->test.longestSleep, sddscontrollawGlobal->loopParam.interval);
    if (waveformOutOfRange)
      sleepTime = MAX(sddscontrollawGlobal->waveform_tests.longestSleep, sddscontrollawGlobal->loopParam.interval);
    sleepTime = MAX(sleepTime, sddscontrollawGlobal->loopParam.interval);
    /*if (outOfRange)
         fprintf( stderr, "Waiting for %f seconds.\n", sleepTime); */
#ifdef USE_RUNCONTROL
    if (sddscontrollawGlobal->rcParam.PV) {
      lastRCPingTime = getTimeInSecs();
      runControlPingWhileSleep(outOfRange ? MAX(sleepTime, sddscontrollawGlobal->loopParam.interval) : timeLeft);
    } else {
#  if defined(DBAccess)
      microSeconds = (long)(1e6 * (outOfRange ? sleepTime : timeLeft));
      if (microSeconds > 0)
        usleepSystemIndependent(microSeconds);
#  else
      oag_ca_pend_event((outOfRange ? sleepTime : timeLeft), &(sddscontrollawGlobal->sigint));
#  endif
    }
#else
#  if defined(DBAccess)
    microSeconds = (long)(1e6 * (outOfRange ? sleepTime : timeLeft));
    if (microSeconds > 0) {
#    if defined(vxWorks)
      taskDelay((int)(sysClkRateGet() * microSeconds / 1000000.0));
#    else
      usleepSystemIndependent(microSeconds);
#    endif
    }
#  else
    oag_ca_pend_event((outOfRange ? sleepTime : timeLeft), &(sddscontrollawGlobal->sigint));
#  endif
#endif
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[26] = getTimeInSecs();
#endif
    if (verbose)
      fprintf(stderr, "\n");
    if (sddscontrollawGlobal->reparseFromFile) {
      sddscontrollawGlobal->reparseFromFile = 0;
      if (parseArguments(&argv,
                         &argc,
                         &sddscontrollawGlobal->correction,
                         &sddscontrollawGlobal->loopParam,
                         &aveParam,
                         &sddscontrollawGlobal->delta,
                         &sddscontrollawGlobal->readbackLimits,
                         &sddscontrollawGlobal->action,
                         &sddscontrollawGlobal->despikeParam,
                         &sddscontrollawGlobal->glitchParam,
                         &sddscontrollawGlobal->test,
                         &sddscontrollawGlobal->statsFile,
                         &serverMode,
                         &commandFile,
                         &sddscontrollawGlobal->outputFile,
                         &sddscontrollawGlobal->controlLogFile,
                         &testCASecurity,
                         &generations,
                         &sddscontrollawGlobal->overlapCompensation,
                         &compensationOutputFile,
                         &sddscontrollawGlobal->readbackWaveforms,
                         &sddscontrollawGlobal->controlWaveforms,
                         &sddscontrollawGlobal->offsetWaveforms,
                         &sddscontrollawGlobal->ffSetpointWaveforms,
                         &sddscontrollawGlobal->waveform_tests,
                         firstTime,
                         &verbose,
                         &warning,
                         &pendIOTime,
                         commandline_option,
                         waveformOption)) {
        fprintf(stderr, "Problem parsing arguments. Forging ahead anyways.\n");
      }
      setupData(&sddscontrollawGlobal->correction,
                &sddscontrollawGlobal->delta,
                &sddscontrollawGlobal->readbackLimits,
                &sddscontrollawGlobal->action,
                &sddscontrollawGlobal->loopParam,
                &sddscontrollawGlobal->despikeParam,
                &sddscontrollawGlobal->test,
                &sddscontrollawGlobal->overlapCompensation,
                &sddscontrollawGlobal->readbackWaveforms,
                &sddscontrollawGlobal->controlWaveforms,
                &sddscontrollawGlobal->offsetWaveforms,
                &sddscontrollawGlobal->ffSetpointWaveforms,
                &sddscontrollawGlobal->waveform_tests,
                &sddscontrollawGlobal->glitchParam,
                verbose,
                pendIOTime);
#ifdef USE_RUNCONTROL
      /* exit runcontrol and re-init. */
      /* This is necessary in order to change the description field even
	     though the PV name hasn't changed. */
      if (sddscontrollawGlobal->rcParam.PV) {
        switch (runControlExit(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo))) {
        case RUNCONTROL_OK:
          sddscontrollawGlobal->rcParam.init = 0;
          break;
        case RUNCONTROL_ERROR:
          fprintf(stderr, "Error exiting run control.\n");
          sddscontrollawGlobal->rcParam.init = 0;
          FreeEverything();
          return (1);
        default:
          fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
          sddscontrollawGlobal->rcParam.init = 0;
          FreeEverything();
          return (1);
        }
        fprintf(stderr, "PV %s\nDesc %s\n", sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc);
        sddscontrollawGlobal->rcParam.status = runControlInit(sddscontrollawGlobal->rcParam.PV, sddscontrollawGlobal->rcParam.Desc, sddscontrollawGlobal->rcParam.pingTimeout, sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo), pendIOTime);
        if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
          fprintf(stderr, "Error initializing run control.\n");
          if (sddscontrollawGlobal->rcParam.status == RUNCONTROL_DENIED) {
            fprintf(stderr, "Another application instance is using the same runcontrol record,\n\tor has the same description string,\n\tor the runcontrol record has not been cleared from previous use.\n");
          }
          FreeEverything();
          return (1);
        }
        sddscontrollawGlobal->rcParam.init = 1;
      }
#endif
    }
    if (generations.doGenerations) {
      --newFileCountdown;
      if (generations.timeLimit > 0 && getTimeInSecs() > generations.timeStop)
        newFileCountdown = 0;
    }
#ifdef DEBUGTIMES
    if (debugTimes)
      debugTime[27] = getTimeInSecs();
    if (sddscontrollawGlobal->loopParam.step[0] == 19)
      debugTimes = 1;
    if (sddscontrollawGlobal->loopParam.step[0] == 20) {
      debugTimes = 0;
      for (i = 0; i < 28; i++) {
        if (debugTime[i] != 0)
          fprintf(stderr, "debug time #%ld = %f\n", i, debugTime[i] - debugTime[0]);
      }
    }
#endif
    if (sddscontrollawGlobal->loopParam.endOfLoopPV) {
      if (ca_state(sddscontrollawGlobal->loopParam.endOfLoopPVInfo.channelID) != cs_conn) {
        fprintf(stderr, "Warning: end-of-loop PV %s not connected\n", sddscontrollawGlobal->loopParam.endOfLoopPV);
      }
      if (ca_put(DBR_LONG, sddscontrollawGlobal->loopParam.endOfLoopPVInfo.channelID,
                 &(sddscontrollawGlobal->loopParam.step[0])) != ECA_NORMAL || 
          ca_pend_io(pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "Warning: problem writing to end-of-loop PV %s\n", sddscontrollawGlobal->loopParam.endOfLoopPV);
      }
    }
  }
  FreeEverything();
  return (0);
}

long readPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, long n, AVERAGE_PARAM *aveParam, double pendIOTime) {
  int i, j;
  long average;
  double interval;
#if defined(DBAccess)
  long num_elements = 1;
#endif

  if (!n)
    return 0;
  if (!aveParam) {
    average = 1;
    interval = 1;
  } else {
    average = aveParam->n;
    interval = aveParam->interval;
  }
  for (i = 0; i < n; i++) {
    channelInfo[i].waveformLength = 0;
    value[i] = 0.0;
  }
  while (average > 0) {
    *(channelInfo[0].count) = n;
    for (i = 0; i < n; i++) {
#if !defined(DBAccess)
      if (ca_state(channelInfo[i].channelID) != cs_conn) {
        fprintf(stderr, "Error, no connection for %s\n", PVs[i]);
        FreeEverything();
        exit(1);
      }
      if (ca_get(DBR_DOUBLE, channelInfo[i].channelID, &channelInfo[i].value) != ECA_NORMAL) {
#else
      if (dbGetField(&channelInfo[i].channelID, DBR_DOUBLE, &channelInfo[i].value, NULL, &num_elements, NULL)) {
#endif
        fprintf(stderr, "error: unable to get value for %s.\n", PVs[i]);
        FreeEverything();
        exit(1);
      }
      channelInfo[i].flag = 0;
    }
#if !defined(DBAccess)
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "pendIOerror: unable to get PV values\n");
      FreeEverything();
      exit(1);
    }
#endif
    *(channelInfo[0].count) = 0;
    average--;
    for (j = 0; j < n; j++)
      value[j] = channelInfo[j].value + value[j];
    if (average) {
#if defined(DBAccess)
      usleepSystemIndependent((long)(1e6 * interval));
#else
      oag_ca_pend_event(interval, &(sddscontrollawGlobal->sigint));
      if (sddscontrollawGlobal->sigint) {
        FreeEverything();
        exit(1);
      }
#endif
    }
  }
  if (aveParam && aveParam->n > 1) {
    for (i = 0; i < n; i++)
      value[i] = value[i] / aveParam->n;
  }
  for (i=0; i<n; i++)
    if (isnan(value[i]) || isinf(value[i])) {
      fprintf(stderr, "Error: value for %s is NaN or Inf\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
  return 0;
}

long CheckCAWritePermissionMod(char **PVs, CHANNEL_INFO *channelInfo, long n) {
#if defined(DBAccess)
  return 0;
#else
  long caDenied = 0;
  long i;
  for (i = 0; i < n; i++) {
    if (!ca_write_access(channelInfo[i].channelID)) {
      fprintf(stderr, "No write access to %s\n", PVs[i]);
      caDenied++;
    }
  }
  return caDenied;
#endif
}

long setPVs(char **PVs, double *value, CHANNEL_INFO *channelInfo, long n, double pendIOTime) {
#if defined(DRYRUN) || defined(NOCAPUT)
  return 0;
#else
  long j;
  *(channelInfo[0].count) = n;
  for (j=0; j<n; j++)
    if (isnan(value[j]) || isinf(value[j])) {
      fprintf(stderr, "Error: %s has invalid value (NaN or Inf)\n", PVs[j]);
      FreeEverything();
      exit(1);
    }
  
  for (j = 0; j < n; j++) {
    channelInfo[j].flag = 0;
#  if !defined(DBAccess)
    if (ca_state(channelInfo[j].channelID) != cs_conn)
      continue;
    if (ca_put(DBR_DOUBLE, channelInfo[j].channelID, &value[j]) != ECA_NORMAL) {
#  else
    if (dbPutField(&channelInfo[j].channelID, DBR_DOUBLE, &value[j], 1)) {
#  endif
      fprintf(stderr, "error: problem doing put for %s\n", PVs[j]);
      FreeEverything();
      exit(1);
    }
  }
#  if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "pendIOerror: unable to put PV values\n");
    FreeEverything();
    exit(1);
  }
#  endif
  *(channelInfo[0].count) = 0;

  return 0;
#endif
}

long runPostChangeExec(char *script, CONTROL_NAME *control, long verbose) {
  FILE *handle;
  char message[1024];
  char *setCommd = NULL;
#ifdef _WIN32
  FILE *f_tmp, *f_tmp1;
  char *tmpfile, *tmpComm;
  tmpfile = tmpComm = NULL;
#endif

  handle = NULL;

  setCommd = SDDS_Realloc(setCommd, sizeof(char) * (strlen(script) + 25));
  strcpy(setCommd, script);
  if (verbose)
    fprintf(stderr, "running post change command %s\n", setCommd);
#ifdef _WIN32
  cp_str(&tmpfile, tmpname(NULL));
  strcat(setCommd, " 2> ");
  strcat(setCommd, tmpfile);
  system(setCommd);
  if (!(f_tmp1 = fopen(tmpfile, "r"))) {
    fclose(f_tmp1);
    fprintf(stderr, "Error, can not open tmpfile");
    return 1;
  }
  if (fgets(message, sizeof(message), f_tmp1)) {
    if (strlen(message) && wild_match(message, "*error*")) {
      fprintf(stderr, "error message: %s\n", message);
      return 1;
    }
    fclose(f_tmp1);
    tmpComm = malloc(sizeof(char) * (strlen(tmpfile) + 20));
    cp_str(&tmpComm, "rm -f ");
    strcat(tmpComm, tmpfile);
    system(tmpComm);
    free(tmpComm);
    free(tmpfile);
    tmpComm = tmpfile = NULL;
  }
#else
  strcat(setCommd, " 2> /dev/stdout");
  handle = popen(setCommd, "r");
  free(setCommd);
  if (handle == NULL) {
    fprintf(stderr, "Error: popen call failed");
    return 1;
  }
  if (!feof(handle)) {
    if (fgets(message, sizeof(message), handle)) {
      if (strlen(message) && wild_match(message, "*error*")) {
        fprintf(stderr, "error message from varScript: %s\n", message);
        return 1;
      }
    }
  }
  pclose(handle);
#endif
  return 0;
}

long setStringPV(char *PV, char *value, CHANNEL_INFO channelInfo, long exitOnError) {
#if defined(DRYRUN)
  return 0;
#else
#  if !defined(DBAccess)
  if (ca_state(channelInfo.channelID) != cs_conn)
    return 1;
  if (ca_put(DBR_STRING, channelInfo.channelID, value) != ECA_NORMAL) {
#  else
  if (dbPutField(&channelInfo.channelID, DBR_STRING, value, 1)) {
#  endif
    fprintf(stderr, "error (ca_put): problem doing put for %s\n", PV);
    if (exitOnError) {
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
  return 0;
#endif
}

long setEnumPV(char *PV, long value, CHANNEL_INFO channelInfo, long exitOnError) {
#if defined(DRYRUN)
  return 0;
#else
#  if !defined(DBAccess)
  if (ca_state(channelInfo.channelID) != cs_conn) {
    return 1;
  }
  if (ca_put(DBR_LONG, channelInfo.channelID, &value) != ECA_NORMAL) {
#  else
  if (dbPutField(&channelInfo.channelID, DBR_LONG, &value, 1)) {
#  endif
    fprintf(stderr, "error (ca_put): problem doing put for %s\n", PV);
    if (exitOnError) {
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
  return 0;
#endif
}

void interrupt_handler2(int sig) {
  exit(1);
}

void sigint_interrupt_handler(int sig) {
  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);
#ifndef _WIN32
  signal(SIGQUIT, interrupt_handler);
#endif
  sddscontrollawGlobal->sigint = 1;
  return;
}

long readEnumPV(char *PV, long *value, CHANNEL_INFO channelInfo, double pendIOTime, long exitOnError) {
#if defined(DBAccess)
  long i = 1;
#endif
#if defined(DRYRUN)
  return 0;
#else
#  if !defined(DBAccess)
  if (ca_state(channelInfo.channelID) != cs_conn) {
    return 1;
  }
  if (ca_get(DBR_LONG, channelInfo.channelID, value) != ECA_NORMAL) {
#  else
  if (dbGetField(&channelInfo.channelID, DBR_LONG, value, NULL, &i, NULL)) {
#  endif
    fprintf(stderr, "error: unable to get value for %s\n", PV);
    if (exitOnError) {
      FreeEverything();
      exit(1);
    } else {
      return 1;
    }
  }
#  if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "pendIOerror: unable to get PV values\n");
    FreeEverything();
    exit(1);
  }
#  endif
  return 0;
#endif
}

#ifdef SUNOS4
void interrupt_handler()
#else
void interrupt_handler(int sig)
#endif
{
  FreeEverything();
  exit(1);
}

#ifdef USE_RUNCONTROL
/* returns value from same list of statuses as other runcontrol calls */
int runControlPingWhileSleep(double sleepTime) {
  double timeLeft;
#  if defined(DBAccess)
  long microSeconds;
#  endif
  timeLeft = sleepTime;
  do {
    sddscontrollawGlobal->rcParam.status = runControlPing(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo));
    switch (sddscontrollawGlobal->rcParam.status) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
      FreeEverything();
      exit(1);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      strcpy(sddscontrollawGlobal->rcParam.message, "Application timed-out");
      runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
      FreeEverything();
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

#  if defined(DBAccess)
    microSeconds = (long)(1e6 * MIN(timeLeft, sddscontrollawGlobal->rcParam.pingInterval));
    if (microSeconds > 0)
      usleepSystemIndependent(microSeconds);
#  else
    oag_ca_pend_event((MIN(timeLeft, sddscontrollawGlobal->rcParam.pingInterval)), &(sddscontrollawGlobal->sigint));
    if (sddscontrollawGlobal->sigint) {
      FreeEverything();
      exit(1);
    }
#  endif
    timeLeft -= sddscontrollawGlobal->rcParam.pingInterval;
  } while (timeLeft > 0);
  return (RUNCONTROL_OK);
}
#endif

void setupReadbackLimitFile(LIMITS *readbackLimits) {
  SDDS_TABLE readbackLimitPage;

  if (readbackLimits->file) {
    if (readbackLimits->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&readbackLimitPage, readbackLimits->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&readbackLimitPage, readbackLimits->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&readbackLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, "ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, "minValue", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "minValue");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&readbackLimitPage, "maxValue", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "maxValue");
      FreeEverything();
      exit(1);
    }
    readbackLimits->n = SDDS_CountRowsOfInterest(&readbackLimitPage);
    readbackLimits->controlName = (char **)SDDS_GetColumn(&readbackLimitPage, "ControlName");
    readbackLimits->minValue = (double *)SDDS_GetColumn(&readbackLimitPage, "minValue");
    readbackLimits->maxValue = (double *)SDDS_GetColumn(&readbackLimitPage, "maxValue");

    if (!SDDS_Terminate(&readbackLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    readbackLimits->n = 0;
    readbackLimits->controlName = NULL;
    readbackLimits->minValue = NULL;
    readbackLimits->maxValue = NULL;
  }
  return;
}

void setupDeltaLimitFile(LIMITS *delta) {
  SDDS_TABLE deltaLimitPage;

  if (delta->file) {
    if (delta->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&deltaLimitPage, delta->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&deltaLimitPage, delta->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&deltaLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&deltaLimitPage, "ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&deltaLimitPage, "DeltaLimit", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "DeltaLimit");
      FreeEverything();
      exit(1);
    }
    delta->n = SDDS_CountRowsOfInterest(&deltaLimitPage);
    delta->controlName = (char **)SDDS_GetColumn(&deltaLimitPage, "ControlName");
    delta->value = (double *)SDDS_GetColumn(&deltaLimitPage, "DeltaLimit");
    if (!SDDS_Terminate(&deltaLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    delta->n = 0;
    delta->controlName = NULL;
    delta->value = NULL;
  }
  return;
}

void setupActionLimitFile(LIMITS *action) {
  SDDS_TABLE actionLimitPage;

  if (action->file) {
    if (action->searchPath) {
      if (SDDS_InitializeInputFromSearchPath(&actionLimitPage, action->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&actionLimitPage, action->file)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadTable(&actionLimitPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&actionLimitPage, "ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&actionLimitPage, "ActionLimit", NULL, SDDS_DOUBLE, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ActionLimit");
      FreeEverything();
      exit(1);
    }
    action->n = SDDS_CountRowsOfInterest(&actionLimitPage);
    action->controlName = (char **)SDDS_GetColumn(&actionLimitPage, "ControlName");
    action->value = (double *)SDDS_GetColumn(&actionLimitPage, "ActionLimit");
    if (!SDDS_Terminate(&actionLimitPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    action->n = 0;
    action->controlName = NULL;
    action->value = NULL;
  }
  return;
}

void setupInputFile(CORRECTION *correction) {
  long i, aorder, border, subscript, j, k;
  int32_t columns, coefRows;
  long columnType;
  long *doubleColumnIndex;
  SDDS_TABLE inputPage, coefPage;
  char **name, **coefName = NULL, **coefDeviceNames = NULL;
  CONTROL_NAME *control, *readback;
  MATRIX *K, *A, *B;
  double **acoef, **bcoef;
  char *coefNameString = NULL;

  readback = correction->readback;
  control = correction->control;
  K = correction->K;
  control->file = correction->file;
  if (correction->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&inputPage, correction->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&inputPage, correction->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  name = (char **)SDDS_GetColumnNames(&inputPage, &columns);
  if ((readback->symbolicName = (char **)SDDS_Realloc(readback->symbolicName, (columns - 1) * sizeof(char *))) == NULL) {
    fprintf(stderr, "can't realloc readback->symbolicName\n");
    FreeEverything();
    exit(1);
  }
  if (readback->waveformMatchFound) {
    free(readback->waveformMatchFound);
  }
  if ((readback->waveformMatchFound = calloc(columns, sizeof(short))) == NULL) {
    fprintf(stderr, "can't calloc readback->waveformMatchFound\n");
    FreeEverything();
    exit(1);
  }

  if (0 > SDDS_ReadTable(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  control->n = SDDS_CountRowsOfInterest(&inputPage);
  if (control->waveformMatchFound) {
    free(control->waveformMatchFound);
  }
  if ((control->waveformMatchFound = calloc(control->n, sizeof(short))) == NULL) {
    fprintf(stderr, "can't calloc control->waveformMatchFound\n");
    FreeEverything();
    exit(1);
  }

  /* check existence of actuator column */
  if (correction->actuator) {
    switch (SDDS_CheckColumn(&inputPage, correction->actuator, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, correction->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", correction->actuator, correction->file);
      FreeEverything();
      exit(1);
    }
  }
  /* find numeric columns */
  if (!SDDS_SetColumnFlags(&inputPage, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!(doubleColumnIndex = (long *)calloc(1, (columns) * sizeof(long)))) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->n = 0;
  for (i = 0; i < columns; i++) {
    long index;
    if ((index = SDDS_GetColumnIndex(&inputPage, name[i])) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, index))) {
      doubleColumnIndex[readback->n] = i;
      SDDS_CopyString(&readback->symbolicName[readback->n], name[i]);
      if (!SDDS_AssertColumnFlags(&inputPage, SDDS_INDEX_LIMITS, index, index, 1L)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      readback->n++;
    } else if (columnType == SDDS_STRING && !(correction->actuator)) {
      SDDS_CopyString(&correction->actuator, name[i]);
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, correction->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
  }

  for (i = 0; i < columns; i++)
    free(name[i]);
  free(name);
  free(doubleColumnIndex);

  control->controlName = control->symbolicName;
  readback->controlName = readback->symbolicName;
  /*assign controlName to readback and controls */
  if (correction->definitionFile) {
    assignControlName(correction, correction->definitionFile);
  } else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  /*********************\
             * read matrix       *
   \********************/
  /*  m_alloc(&K, control->n, readback->n); */
  K->m = control->n;
  K->n = readback->n;
  K->a = (double **)SDDS_GetMatrixOfRows(&inputPage, &(control->n));
  if (!SDDS_Terminate(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  /************************ \
             * read coefficients file*
   \************************/
  if (correction->coefFile) {
    if ((coefNameString = (char *)malloc(sizeof(char) * 4)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if (correction->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&coefPage, correction->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&coefPage, correction->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    coefName = (char **)SDDS_GetColumnNames(&coefPage, &columns);
    if (0 > SDDS_ReadTable(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    coefRows = SDDS_CountRowsOfInterest(&coefPage);
    if (!(coefDeviceNames = (char **)SDDS_GetColumn(&coefPage, "DeviceName"))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    /*********************                      \
                 * read coefficients *
     \********************/
    /* find columns that start with letter "a" and determine the order of the filter */
    aorder = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "a%ld", &subscript)) {
          if (aorder < subscript)
            aorder = subscript;
        }
      }
    }
    /* find columns that start with letter "b" and determine the order of the filter */
    border = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "b%ld", &subscript)) {
          if (border < subscript)
            border = subscript;
        }
      }
    }
    /* I guess we free the strings as soon as we have no use for them */
    for (i = 0; i < columns; i++)
      free(coefName[i]);
    free(coefName);
    m_alloc(&A, (aorder + 1), control->n);
    correction->aCoef = A;
    m_zero(A);
    m_alloc(&B, (border + 1), control->n);
    correction->bCoef = B;
    m_zero(B);
    /* allocate arrays for history */
    m_alloc(&(control->history), (border + 1), control->n);
    m_alloc(&(control->historyFiltered), (aorder + 1), control->n);

    acoef = malloc(sizeof(*acoef) * (aorder + 1));
    bcoef = malloc(sizeof(*bcoef) * (border + 1));

    for (subscript = 0; subscript <= aorder; subscript++) {
      acoef[subscript] = NULL;
      sprintf(coefNameString, "a%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
        /*set it to be zero if not provided */
        acoef[subscript] = (double *)calloc(coefRows, sizeof(**acoef));
      } else {
        acoef[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }
    for (subscript = 0; subscript <= border; subscript++) {
      bcoef[subscript] = NULL;
      sprintf(coefNameString, "b%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
        /*set it to be zero if not provided */
        bcoef[subscript] = calloc(coefRows, sizeof(**bcoef));
      } else {
        bcoef[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }

    for (i = 0; i < control->n; i++) {
      j = match_string(control->symbolicName[i], coefDeviceNames, coefRows, EXACT_MATCH);
      if (j < 0) {
        /* if there is no match, then the filter is just unity. The other coefficients k>0 were initialized to 0. */
        A->a[0][i] = 1.0;
        B->a[0][i] = 1.0;
      } else {
        for (k = 0; k <= aorder; k++)
          A->a[k][i] = acoef[k][j];
        for (k = 0; k <= border; k++)
          B->a[k][i] = bcoef[k][j];
      }
    }
    for (k = 0; k <= aorder; k++)
      free(acoef[k]);
    free(acoef);
    acoef = NULL;
    for (k = 0; k <= border; k++)
      free(bcoef[k]);
    free(bcoef);
    bcoef = NULL;

    if (!SDDS_Terminate(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < coefRows; i++)
      free(coefDeviceNames[i]);
    free(coefDeviceNames);
    free(coefNameString);
  }
#ifdef USE_RUNCONTROL
  checkRCParam(&inputPage, correction->file);
#endif
}

void setupCompensationFiles(CORRECTION *compensation) {
  long i, rows;
  int32_t columns;
  long columnType;
  long *doubleColumnIndex;
  SDDS_TABLE inputPage, coefPage;
  char **name, **coefName, *coefNameString;
  CONTROL_NAME *control, *readback;
  MATRIX *K, *A, *B;
  long aorder, border, subscript;

  if (!compensation->file)
    return;

  readback = compensation->readback;
  control = compensation->control;

  K = compensation->K;
  A = compensation->aCoef;
  B = compensation->bCoef;
  control->file = compensation->file;
  if (compensation->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&inputPage, compensation->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&inputPage, compensation->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  name = (char **)SDDS_GetColumnNames(&inputPage, &columns);
  if ((readback->symbolicName = (char **)SDDS_Realloc(readback->symbolicName, columns * sizeof(char *))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (readback->waveformMatchFound) {
    free(readback->waveformMatchFound);
  }
  if ((readback->waveformMatchFound = calloc(columns, sizeof(short))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  if (0 > SDDS_ReadTable(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  control->n = SDDS_CountRowsOfInterest(&inputPage);
  if (control->waveformMatchFound) {
    free(control->waveformMatchFound);
  }
  if ((control->waveformMatchFound = calloc(control->n, sizeof(short))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  /* check existence of actuator column */
  if (compensation->actuator) {
    switch (SDDS_CheckColumn(&inputPage, compensation->actuator, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, compensation->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", compensation->actuator, compensation->file);
      FreeEverything();
      exit(1);
    }
  }
  /* find numeric columns */
  if (!SDDS_SetColumnFlags(&inputPage, 0)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if ((doubleColumnIndex = (long *)calloc(1, columns * sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->n = 0;
  for (i = 0; i < columns; i++) {
    if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&inputPage, i))) {
      doubleColumnIndex[readback->n] = i;
      SDDS_CopyString(&readback->symbolicName[readback->n], name[i]);
      SDDS_SetColumnsOfInterest(&inputPage, SDDS_MATCH_STRING, name[i], SDDS_OR);
      readback->n++;
    } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
      SDDS_CopyString(&compensation->actuator, name[i]);
      if (!(control->symbolicName = (char **)SDDS_GetColumn(&inputPage, compensation->actuator))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
  }

  /*assign control names */
  if (compensation->definitionFile)
    assignControlName(compensation, compensation->definitionFile);
  else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  /*********************\
             * read matrix       *
   \********************/
  /*  m_alloc(&K, control->n, readback->n); */
  K->m = control->n;
  K->n = readback->n;
  K->a = (double **)SDDS_GetMatrixOfRows(&inputPage, &(control->n));
  if (!SDDS_Terminate(&inputPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  /*************************\
             * read coefficients file*
   \************************/
  if (compensation->coefFile) {
    if (compensation->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&inputPage, compensation->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&coefPage, compensation->coefFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    coefName = (char **)SDDS_GetColumnNames(&coefPage, &columns);
    if (0 > SDDS_ReadTable(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    rows = SDDS_CountRowsOfInterest(&coefPage);
    if (rows != control->n) {
      FreeEverything();
      SDDS_Bomb("Number of rows in filter and compensation matrix files do not match.");
    }
    /*********************\
                 * read coefficients *
     \********************/
    /* find columns that start with letter "a" and determine the order of the filter */
    aorder = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "a%ld", &subscript)) {
          if (aorder < subscript)
            aorder = subscript;
        }
      } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
        compensation->actuator = coefName[i];
        if (!(control->symbolicName = (char **)SDDS_GetColumn(&coefPage, compensation->actuator))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
    }
    m_alloc(&A, (aorder + 1), control->n);
    compensation->aCoef = A;
    m_zero(A);
    if ((coefNameString = (char *)malloc(sizeof(char) * 4)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (subscript = 0; subscript <= aorder; subscript++) {
      sprintf(coefNameString, "a%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
      } else {
        free(A->a[subscript]);
        A->a[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }
    /* find columns that start with letter "b" and determine the order of the filter */
    border = 0;
    for (i = 0; i < columns; i++) {
      if (SDDS_NUMERIC_TYPE(columnType = SDDS_GetColumnType(&coefPage, i))) {
        if (sscanf(coefName[i], "b%ld", &subscript)) {
          if (border < subscript)
            border = subscript;
        }
      } else if (columnType == SDDS_STRING && !(compensation->actuator)) {
        compensation->actuator = coefName[i];
        if (!(control->symbolicName = (char **)SDDS_GetColumn(&coefPage, compensation->actuator))) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
    }
    m_alloc(&B, (border + 1), control->n);
    compensation->bCoef = B;
    m_zero(B);
    for (subscript = 0; subscript <= border; subscript++) {
      sprintf(coefNameString, "b%ld", subscript);
      if (!SDDS_FindColumn(&coefPage, FIND_NUMERIC_TYPE, coefNameString, NULL)) {
        fprintf(stderr, "warning: column %s doesn't exist\n", coefNameString);
      } else {
        free(B->a[subscript]);
        B->a[subscript] = SDDS_GetColumnInDoubles(&coefPage, coefNameString);
      }
    }

    /* allocate arrays for history */
    m_alloc(&control->history, (border + 1), control->n);
    m_alloc(&control->historyFiltered, (aorder + 1), control->n);
    if (!SDDS_Terminate(&coefPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    /* no filter file given */
  }
  for (i = 0; i < columns; i++)
    free(name[i]);
  free(name);
  free(doubleColumnIndex);
  return;
}

/*assign controlName for controls or readbacks */
void assignControlName(CORRECTION *correction, char *definitionFile) {
  long i, j, row_match, rows = 0;
  SDDS_DATASET definitionPage;
  char **def_symbolicName, **def_controlName;
  CONTROL_NAME *control, *readback;

  def_symbolicName = def_controlName = NULL;
  control = correction->control;
  readback = correction->readback;
  control->controlName = NULL;
  readback->controlName = NULL;

  if (!control->n && !readback->n)
    return;
  if (definitionFile) {
    /*get information from defitionFile */
    if (correction->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&definitionPage, definitionFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&definitionPage, definitionFile)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    if (SDDS_ReadPage(&definitionPage) < 0) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&definitionPage, "SymbolicName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "SymbolicName");
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&definitionPage, "ControlName", NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column %s.\n", "ControlName");
      FreeEverything();
      exit(1);
    }

    rows = SDDS_CountRowsOfInterest(&definitionPage);
    def_symbolicName = (char **)SDDS_GetColumn(&definitionPage, "SymbolicName");
    def_controlName = (char **)SDDS_GetColumn(&definitionPage, "ControlName");
    if (!SDDS_Terminate(&definitionPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    /*assign controlName */
    if (control->n) {
      if ((control->controlName = (char **)malloc(sizeof(char *) * control->n)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if (0 > (row_match = match_string(control->symbolicName[i], def_symbolicName, rows, EXACT_MATCH))) {
        fprintf(stderr, "Name %s doesn't exist in column %s of file %s.\n", control->symbolicName[i], "SymbolicName", correction->definitionFile);
        for (j = 0; j < i; j++) {
          free(control->controlName[j]);
        }
        free(control->controlName);
        control->controlName = NULL;
        FreeEverything();
        exit(1);
      }
      SDDS_CopyString(&control->controlName[i], def_controlName[row_match]);
    }
    if (readback->n) {
      if ((readback->controlName = (char **)malloc(sizeof(char *) * readback->n)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < readback->n; i++) {
      if (0 > (row_match = match_string(readback->symbolicName[i], def_symbolicName, rows, EXACT_MATCH))) {
        fprintf(stderr, "Name %s doesn't exist in column %s of file %s.\n", readback->symbolicName[i], "SymbolicName", correction->definitionFile);
        for (j = 0; j < i; j++) {
          free(readback->controlName[j]);
        }
        free(readback->controlName);
        readback->controlName = NULL;
        FreeEverything();
        exit(1);
      }
      SDDS_CopyString(&readback->controlName[i], def_controlName[row_match]);
    }
    /*free memory */
    for (i = 0; i < rows; i++) {
      free(def_symbolicName[i]);
      free(def_controlName[i]);
    }
    free(def_symbolicName);
    free(def_controlName);
  } else {
    control->controlName = control->symbolicName;
    readback->controlName = readback->symbolicName;
  }
  return;
}

void matchUpControlNames(LIMITS *limits, char **controlName, long n) {
  long i, row_match;

  if (limits->n == 0)
    return;
  if ((limits->index = (long *)malloc(sizeof(long) * (limits->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (limits->flag & LIMIT_FILE) {
    for (i = 0; i < limits->n; i++) {
      if (0 > (row_match = match_string(limits->controlName[i], controlName, n, EXACT_MATCH))) {
        fprintf(stderr, "Name %s in file %s doesn't match known control names.\n", limits->controlName[i], limits->file);
        FreeEverything();
        exit(1);
      }
      limits->index[i] = row_match;
    }
  }
  return;
}

void setupTestsFile(TESTS *test, double interval, long verbose, double pendIOTime) {
  long itest;
  SDDS_TABLE testValuesPage;
  long sleepTimeColumnPresent = 0, resetTimeColumnPresent = 0;
  long despikeColumnPresent = 0, holdOffTimeColumnPresent = 0;
  long holdOffIntervalColumnPresent = 0, sleepIntervalColumnPresent = 0, i;
  long glitchLogColumnPresent = 0;

  if (!test->file)
    return;
  if (test->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&testValuesPage, test->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&testValuesPage, test->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  switch (SDDS_CheckColumn(&testValuesPage, "ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MinimumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MinimumValue", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "MaximumValue", NULL, SDDS_DOUBLE, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "MaximumValue", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "SleepTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SleepTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "SleepIntervals", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    sleepIntervalColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    sleepIntervalColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SleepIntervals", test->file);
    FreeEverything();
    exit(1);
  }
  if (sleepTimeColumnPresent && sleepIntervalColumnPresent) {
    fprintf(stderr, "SleepTime Column and SleepIntervals column can not exist in the same time.\n");
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, "ResetTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    resetTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    resetTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "HoldOffTime", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    holdOffTimeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    holdOffTimeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&testValuesPage, "HoldOffIntervals", NULL, SDDS_DOUBLE, NULL)) {
  case SDDS_CHECK_OKAY:
    holdOffIntervalColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    holdOffIntervalColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ResetTime", test->file);
    FreeEverything();
    exit(1);
  }
  if (holdOffTimeColumnPresent && holdOffIntervalColumnPresent) {
    fprintf(stderr, "HoldOffTime column and HoldOffIntervals column can not exist in the same time.\n");
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, "Despike", NULL, SDDS_LONG, NULL)) {
  case SDDS_CHECK_OKAY:
    despikeColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    despikeColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", test->file);
    FreeEverything();
    exit(1);
  }

  switch (SDDS_CheckColumn(&testValuesPage, "GlitchLog", NULL, SDDS_CHARACTER, NULL)) {
  case SDDS_CHECK_OKAY:
    glitchLogColumnPresent = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    glitchLogColumnPresent = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "GlitchLog", test->file);
    FreeEverything();
    exit(1);
  }

  if (SDDS_ReadTable(&testValuesPage) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  test->n = SDDS_CountRowsOfInterest(&testValuesPage);
  if ((test->value = (double *)malloc(sizeof(double) * (test->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((test->outOfRange = (long *)malloc(sizeof(long) * (test->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose) {
    fprintf(stderr, "number of test values = %ld\n", test->n);
  }
  test->controlName = (char **)SDDS_GetColumn(&testValuesPage, "ControlName");
  test->min = SDDS_GetColumnInDoubles(&testValuesPage, "MinimumValue");
  test->max = SDDS_GetColumnInDoubles(&testValuesPage, "MaximumValue");
  if (sleepTimeColumnPresent)
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, "SleepTime");
  if (sleepIntervalColumnPresent) {
    test->sleep = SDDS_GetColumnInDoubles(&testValuesPage, "SleepIntervals");
    for (i = 0; i < test->n; i++)
      test->sleep[i] = test->sleep[i] * interval;
  }
  if (holdOffTimeColumnPresent)
    test->holdOff = SDDS_GetColumnInDoubles(&testValuesPage, "HoldOffTime");
  if (holdOffIntervalColumnPresent) {
    test->holdOff = SDDS_GetColumnInDoubles(&testValuesPage, "HoldOffIntervals");
    for (i = 0; i < test->n; i++)
      test->holdOff[i] = test->holdOff[i] * interval;
  }
  if (resetTimeColumnPresent)
    test->reset = SDDS_GetColumnInDoubles(&testValuesPage, "ResetTime");
  if (despikeColumnPresent) {
    test->despike = (int32_t *)SDDS_GetColumn(&testValuesPage, "Despike");
    /* make new array for test values that are to be despiked */
    /* the following arrays only need to be as long as the number
         of test variables with despike flag!=0, but this number
         is unknown in advance. */
    if ((test->despikeIndex = (int32_t *)malloc(sizeof(long) * test->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    test->despikeValues = 0;
    for (itest = 0; itest < test->n; itest++) {
      if (test->despike[itest]) {
        test->despikeIndex[test->despikeValues] = itest;
        test->despikeValues++;
      }
    }
  }
  if (glitchLogColumnPresent) {
    test->glitchLog = (char *)SDDS_GetColumn(&testValuesPage, "GlitchLog");
  }
  /*setup raw connection */
  if ((test->channelInfo = calloc(1, sizeof(CHANNEL_INFO) * test->n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for test PVs.\n");
  if ((test->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  SetupRawCAConnection(test->controlName, test->channelInfo, test->n, pendIOTime);
  if (verbose) {
    for (itest = 0; itest < test->n; itest++) {
      fprintf(stderr, "%3ld%30s\t%11.3f\t%11.3f", itest, test->controlName[itest], test->min[itest], test->max[itest]);
      if (sleepTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->sleep[itest]);
      if (resetTimeColumnPresent)
        fprintf(stderr, "\t%f sec\n", test->reset[itest]);
      fprintf(stderr, "\n");
    }
  }
  if (!SDDS_Terminate(&testValuesPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  return;
}

void setupDespikeFile(DESPIKE_PARAM *despikeParam, CONTROL_NAME *readback, long verbose) {

  SDDS_TABLE despikePage;
  long i, row_match, symbolicExist = 0;

  if (!despikeParam->file) {
    return;
  }
  if (despikeParam->searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&despikePage, despikeParam->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&despikePage, despikeParam->file)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  switch (SDDS_CheckColumn(&despikePage, "ControlName", NULL, SDDS_STRING, stderr)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "ControlName", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&despikePage, "Despike", NULL, SDDS_LONG, NULL)) {
  case SDDS_CHECK_OKAY:
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  switch (SDDS_CheckColumn(&despikePage, "SymbolicName", NULL, SDDS_STRING, NULL)) {
  case SDDS_CHECK_OKAY:
    symbolicExist = 1;
    break;
  case SDDS_CHECK_NONEXISTENT:
    symbolicExist = 0;
    break;
  default:
    fprintf(stderr, "Something wrong with column %s in file %s.\n", "SymolicName", despikeParam->file);
    FreeEverything();
    exit(1);
  }
  if (SDDS_ReadTable(&despikePage) < 0) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  despikeParam->n = SDDS_CountRowsOfInterest(&despikePage);
  if (verbose)
    fprintf(stderr, "Number of rows in file %s = %d\n", despikeParam->file, despikeParam->n);
  despikeParam->controlName = (char **)SDDS_GetColumn(&despikePage, "ControlName");
  despikeParam->despike = (int32_t *)SDDS_GetColumn(&despikePage, "Despike");
  if (symbolicExist)
    despikeParam->symbolicName = (char **)SDDS_GetColumn(&despikePage, "SymbolicName");
  else
    despikeParam->symbolicName = despikeParam->controlName;
  /* match up the control names from this file
     with the control names from the CONTROL_NAME structure.
     and transfer the value of the despike column */
  for (i = 0; i < readback->n; i++) /* initial value. Assume all readbacks to be despiked */
    readback->despike[i] = 1;
  for (i = 0; i < despikeParam->n; i++) {
    if (0 > (row_match = match_string(despikeParam->controlName[i], readback->controlName, readback->n, EXACT_MATCH))) {
      if (verbose) {
        fprintf(stderr, "Warning: Name %s in file %s doesn't match with any name.\n", despikeParam->controlName[i], despikeParam->file);
      }
    } else
      readback->despike[row_match] = despikeParam->despike[i];
  }

  if (!SDDS_Terminate(&despikePage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  return;
}

void setupOutputFile(SDDS_TABLE *outputPage, char *file, CORRECTION *correction, TESTS *test, LOOP_PARAM *loopParam, char *timeStamp, WAVEFORM_TESTS *waveform_tests, long verbose) {
  long i, j;
  CONTROL_NAME *control, *readback;

  readback = correction->readback;
  control = correction->control;
  if (file) {
    if (!SDDS_InitializeOutput(outputPage, SDDS_BINARY, 1,
                               "Output and control variables",
                               "Readback and control variables", file) ||
        (0 > SDDS_DefineParameter(outputPage, "Gain", "Gain", NULL,
                                  "Gain", NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(outputPage, "OverlapCompensationGain",
                                  "OverlapCompensationGain", NULL,
                                  "OverlapCompensationGain", NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(outputPage, "TimeStamp",
                                  "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(outputPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(outputPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(outputPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(outputPage);
    for (i = 0; i < readback->n; i++) {
      if (0 > SDDS_DefineColumn(outputPage, readback->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if (0 > SDDS_DefineColumn(outputPage, control->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    /* some tests PVs may be the same as those
         in the readback set or the control set. If there is 
         a case of identical column names, then the program will return an error.
         So far, we haven't encountered that situation.
       */
    if (test && test->n) {
      if ((test->writeToFile = (int32_t *)malloc(test->n * sizeof(int32_t))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (i = 0; i < test->n; i++) {
        if (match_string(test->controlName[i], readback->controlName, readback->n, EXACT_MATCH) >= 0 ||
            match_string(test->controlName[i], control->controlName, control->n, EXACT_MATCH) >= 0) {
          test->writeToFile[i] = 0;
        } else {
          test->writeToFile[i] = 1;
          if (0 > SDDS_DefineColumn(outputPage, test->controlName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            FreeEverything();
            exit(1);
          }
        }
      }
    }
    if (waveform_tests->waveformTests) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
          if (!waveform_tests->ignore[i][j]) {
            if (match_string(waveform_tests->DeviceName[i][j], readback->symbolicName, readback->n, EXACT_MATCH) >= 0 ||
                match_string(waveform_tests->DeviceName[i][j], control->symbolicName, control->n, EXACT_MATCH) >= 0) {
              waveform_tests->writeToFile[i][j] = 0;
            } else {
              waveform_tests->writeToFile[i][j] = 1;
              if (0 > SDDS_DefineColumn(outputPage, waveform_tests->DeviceName[i][j], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
                SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
                FreeEverything();
                exit(1);
              }
            }
          } else
            waveform_tests->writeToFile[i][j] = 0;
        }
      }
    }

    if (!SDDS_WriteLayout(outputPage) || !SDDS_StartTable(outputPage, loopParam->updateInterval)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "TimeStamp of output file %s: %s\n", file, timeStamp);
    if (!SDDS_SetParameters(outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, NULL) ||
        !SDDS_SetParameters(outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "Gain", loopParam->gain, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void setupGlitchFile(SDDS_TABLE *glitchPage, GLITCH_PARAM *glitchParam, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *overlapCompensation, char *timeStamp, long verbose) {
  long i;
  CONTROL_NAME *control, *readback;
  CONTROL_NAME *controlComp, *readbackComp;
  char *buffer = NULL;

  readback = correction->readback;
  control = correction->control;

  if (glitchParam->file) {
    if (!SDDS_InitializeOutput(glitchPage, SDDS_BINARY, 1,
                               "Readback and control variables at glitch",
                               "Readback and control variables at glitch", glitchParam->file) ||
        (0 > SDDS_DefineParameter(glitchPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "ReadbackRmsThreshold", "ReadbackRmsThreshold", NULL, "ReadbackRmsThreshold",
                                  NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "ControlRmsThreshold", "ControlRmsThreshold", NULL, "ControlRmsThreshold",
                                  NULL, SDDS_DOUBLE, NULL)) ||
        (0 > SDDS_DefineParameter(glitchPage, "GlitchRows", "GlitchRows", NULL, "GlitchRows",
                                  NULL, SDDS_LONG, NULL)) ||
        (0 > SDDS_DefineColumn(glitchPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(glitchPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(glitchPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(glitchPage);
    for (i = 0; i < readback->n; i++)
      if (0 > SDDS_DefineColumn(glitchPage, readback->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    for (i = 0; i < control->n; i++) {
      if (0 > SDDS_DefineColumn(glitchPage, control->symbolicName[i], NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      if ((buffer = malloc(sizeof(char) * (strlen(control->symbolicName[i]) + 6))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      sprintf(buffer, "%sDelta", control->symbolicName[i]);
      if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        free(buffer);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      free(buffer);
    }
    if (overlapCompensation->file) {
      readbackComp = overlapCompensation->readback;
      controlComp = overlapCompensation->control;
      for (i = 0; i < readbackComp->n; i++) {
        if ((buffer = malloc(sizeof(char) * (strlen(readbackComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer, "%sAuxil", readbackComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
      }
      for (i = 0; i < controlComp->n; i++) {
        if ((buffer = malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer, "%sAuxil", controlComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
        if ((buffer = malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 11))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer, "%sAuxilDelta", controlComp->symbolicName[i]);
        if (0 > SDDS_DefineColumn(glitchPage, buffer, NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
          free(buffer);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
      }
    }
    if (!SDDS_WriteLayout(glitchPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void setupStatsFile(SDDS_TABLE *statsPage, char *statsFile, LOOP_PARAM *loopParam, char *timeStamp, long verbose) {

  if (statsFile) {
    if (!SDDS_InitializeOutput(statsPage, SDDS_BINARY, 1,
                               "sddscontrollaw statistics of readback and control variables",
                               NULL, statsFile) ||
        (0 > SDDS_DefineParameter(statsPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(statsPage, "Step", NULL, NULL, "Step number",
                               NULL, SDDS_LONG, 0)) ||
        (0 > SDDS_DefineColumn(statsPage, "ElapsedTime", NULL, "s", "Time since start of run", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(statsPage, "Time", NULL, "s", "Time since start of epoch", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(statsPage);
    if (!loopParam->briefStatistics) {
      if (0 > SDDS_DefineColumn(statsPage, "readbackRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "readbackDeltaMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlRMS", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlAve", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlMAD", NULL,
                                NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaRMS", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaAve", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
          0 > SDDS_DefineColumn(statsPage, "controlDeltaMAD", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (0 > SDDS_DefineColumn(statsPage, "readbackLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackDeltaLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "readbackDeltaLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlLargest", NULL,
                              NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlLargestName", NULL,
                              NULL, NULL, NULL, SDDS_STRING, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlDeltaLargest", NULL, NULL, NULL, NULL, SDDS_FLOAT, 0) ||
        0 > SDDS_DefineColumn(statsPage, "controlDeltaLargestName", NULL, NULL, NULL, NULL, SDDS_STRING, 0)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_WriteLayout(statsPage) || !SDDS_StartTable(statsPage, loopParam->updateInterval)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "TimeStamp of stats file: %s\n", timeStamp);
    if (!SDDS_SetParameters(statsPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

void getInitialValues(CORRECTION *correction, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime) {

  CONTROL_NAME *control, *readback;
  long i;

  if (!correction->file)
    return;

  readback = correction->readback;
  control = correction->control;

  if (controlWaveforms && controlWaveforms->waveforms) {
    if (control->integral) {
      if (ReadWaveformData(controlWaveforms, pendIOTime)) {
        FreeEverything();
        exit(1);
      }
      for (i = 0; i < control->n; i++)
        control->value[0][i] = controlWaveforms->readbackValue[i];
    } else {
      for (i = 0; i < control->n; i++)
        control->value[0][i] = 0;
    }
  } else {
    /*setup connection */
    if ((control->channelInfo = calloc(1, sizeof(CHANNEL_INFO) * control->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->setFlag = malloc(sizeof(short) * control->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawCAConnection(control->controlName, control->channelInfo, control->n, pendIOTime);
    if (control->integral) {
      if (verbose)
        fprintf(stderr, "Reading initial values for %" PRId64 " control devices.\n", control->n);
      readPVs(control->controlName, control->value[0], control->channelInfo, control->n, NULL, pendIOTime);
    } else {
      for (i = 0; i < control->n; i++)
        control->value[0][i] = 0;
    }
  }

  if (verbose)
    fprintf(stderr, "Reading initial values for %" PRId64 " readback PVs.\n", readback->n);
  if (readbackWaveforms && readbackWaveforms->waveforms) {
    if (ReadWaveformData(readbackWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] = readbackWaveforms->readbackValue[i];
  } else {
    if ((readback->channelInfo = calloc(1, sizeof(CHANNEL_INFO) * readback->n)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawCAConnection(readback->controlName, readback->channelInfo, readback->n, pendIOTime);
    readPVs(readback->controlName, readback->value[0], readback->channelInfo, readback->n, aveParam, pendIOTime);
  }
  for (i = 0; i < readback->n; i++)
    if (isnan(readback->value[0][i]))
      fprintf(stderr, "i=%ld, name=%s, value=%f\n", i, readback->controlName[i], readback->value[0][i]);
  for (i = 0; i < control->n; i++) {
    control->initial[i] = control->value[0][i];
    control->old[i] = control->value[0][i];
  }
  for (i = 0; i < readback->n; i++) {
    if (!loopParam->offsetFile)
      readback->initial[i] = readback->value[0][i];
    readback->old[i] = readback->value[0][i];
  }
  if (loopParam->holdPresentValues) {
    if (!loopParam->offsetFile)
      for (i = 0; i < readback->n; i++)
        readback->setpoint[i] = readback->value[0][i];
    else
      for (i = 0; i < readback->n; i++)
        readback->setpoint[i] = readback->initial[i];
  }

  return;
}

long initializeHistories(CORRECTION *compensation) {
  long i, j;
  CONTROL_NAME *control;
  long aorder, border;
  double *initial;
  
  if (!compensation->coefFile)
    return 0;
  control = compensation->control;
  initial = control->initial; /* need this to initialize all history values */
  aorder = control->historyFiltered->n - 1;
  border = control->history->n - 1;

  if (!initial) {
    FreeEverything();
    SDDS_Bomb("Something wrong with initial values in call to initializeHistories");
  }
  
  /*   control->history->a[i][0] is supposed to be the x in the difference equation
      a0 y[n] + a1 y[n-1] + a2 y[n-2] + ... = b0 x[n]  + b1 x[n-1] + b2 x[n-2] + ...
   */
  for (j = 0; j < control->n; j++) {
    for (i = 0; i <= border; i++) {
      control->history->a[i][j] = initial[j];
    }
    for (i = 0; i <= aorder; i++)
      /* A reset filter signal command if one will ever be created, would have to 
         transfer present values to the a and b */
  /*   control->historyFiltered->a[i][0] is supposed to be the y in the difference equation
      a0 y[n] + a1 y[n-1] + a2 y[n-2] + ... = b0 x[n]  + b1 x[n-1] + b2 x[n-2] + ...
   */
      control->historyFiltered->a[i][j] =  initial[j];
  }
  return 0;
}

long getReadbackValues(CONTROL_NAME *readback, AVERAGE_PARAM *aveParam, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, WAVE_FORMS *readbackWaveforms, WAVE_FORMS *offsetWaveforms, long verbose, double pendIOTime) {
  int64_t i, largestIndex, maxIndex, minIndex;

  if (verbose) {
    fprintf(stderr, "Reading readback devices at %f seconds.\n", loopParam->elapsedTime[0]);
  }
  for (i = 0; i < readback->n; i++)
    readback->old[i] = readback->value[0][i];
  if (readbackWaveforms && readbackWaveforms->waveforms) {
    if (ReadWaveformData(readbackWaveforms, pendIOTime))
      return 1;
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] = readbackWaveforms->readbackValue[i];
  } else {
    readPVs(readback->controlName, readback->value[0], readback->channelInfo, readback->n, aveParam, pendIOTime);
  }
  if (offsetWaveforms && offsetWaveforms->waveforms) {
    if (ReadWaveformData(offsetWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++)
      readback->value[0][i] -= offsetWaveforms->readbackValue[i];
  } else {
    if (loopParam->offsetPVFile) {
      readPVs(loopParam->offsetPV, loopParam->offsetPVvalue, loopParam->channelInfo, readback->n, aveParam, pendIOTime);
      for (i = 0; i < readback->n; i++)
        readback->value[0][i] -= loopParam->offsetPVvalue[i];
    }
  }

  if (readbackStats) {
    readbackStats->RMS = standardDeviation(readback->value[0], readback->n);
    readbackStats->ave = arithmeticAverage(readback->value[0], readback->n);
    readbackStats->MAD = meanAbsoluteDeviation(readback->value[0], readback->n);
    index_min_max(&minIndex, &maxIndex, readback->value[0], readback->n);
    if ((readback->value[0])[maxIndex] > -(readback->value[0])[minIndex]) {
      largestIndex = maxIndex;
    } else {
      largestIndex = minIndex;
    }
    readbackStats->largest = readback->value[0][largestIndex];
    readbackStats->largestName = readback->controlName[largestIndex];
    sprintf(readbackStats->msg, "Readback devices        %10.3g %10.3g"
                                " %10.3g %10.3g (%s)",
            readbackStats->ave, readbackStats->RMS, readbackStats->MAD, readbackStats->largest, readbackStats->largestName);
  }
  if (readbackDeltaStats) {
    for (i = 0; i < readback->n; i++) {
      readback->delta[0][i] = readback->value[0][i] - readback->old[i];
    }
    readbackDeltaStats->RMS = standardDeviation(readback->delta[0], readback->n);
    readbackDeltaStats->ave = arithmeticAverage(readback->delta[0], readback->n);
    readbackDeltaStats->MAD = meanAbsoluteDeviation(readback->delta[0], readback->n);
    index_min_max(&minIndex, &maxIndex, readback->delta[0], readback->n);
    if (readback->delta[0][maxIndex] > -readback->delta[0][minIndex]) {
      largestIndex = maxIndex;
    } else {
      largestIndex = minIndex;
    }
    readbackDeltaStats->largest = readback->delta[0][largestIndex];
    readbackDeltaStats->largestName = readback->controlName[largestIndex];
    sprintf(readbackDeltaStats->msg, "Readback device deltas %10.3g %10.3g"
                                     " %10.3g %10.3g (%s)",
            readbackDeltaStats->ave, readbackDeltaStats->RMS, readbackDeltaStats->MAD, readbackDeltaStats->largest, readbackDeltaStats->largestName);
  }

  return (0);
}

long checkActionLimit(LIMITS *action, CONTROL_NAME *readback) {

  long skipIteration;
  long i, actionLimitRow;

  skipIteration = 0;
  if (action->flag & LIMIT_VALUE) {
    skipIteration = 1;
    for (i = 0; i < readback->n; i++) {
      if (fabs(readback->value[0][i]) > action->singleValue) {
        skipIteration = 0;
        break;
      }
    }
  } else if (action->flag & LIMIT_FILE) {
    skipIteration = 1;
    for (actionLimitRow = 0; actionLimitRow < action->n; actionLimitRow++) {
      if (fabs(readback->value[0][action->index[actionLimitRow]]) > action->value[actionLimitRow]) {
        skipIteration = 0;
        break;
      }
    }
  }
  return (skipIteration);
}

void adjustReadbacks(CONTROL_NAME *readback, LIMITS *limits, DESPIKE_PARAM *despikeParam, STATS *readbackAdjustedStats, long verbose) {
  long i, j, pos, restored, spikesDone;
  int64_t min_index, max_index, largest_index;
  double *tempArray;

  if (limits->flag & LIMIT_VALUE) {
    for (i = 0; i < readback->n; i++) {
      if (fabs(readback->value[0][i]) > limits->singleValue) {
        readback->value[0][i] = SIGN(readback->value[0][i]) * limits->singleValue;
      }
    }
  } else if (limits->flag & LIMIT_MINMAXVALUE) {
    for (i = 0; i < readback->n; i++) {
      if (readback->value[0][i] > limits->singleMaxValue) {
        readback->value[0][i] = limits->singleMaxValue;
      } else if (readback->value[0][i] < limits->singleMinValue) {
        readback->value[0][i] = limits->singleMinValue;
      }
    }
  } else if (limits->flag & LIMIT_FILE) {
    for (i = 0; i < limits->n; i++) {
      pos = limits->index[i];
      if (readback->value[0][pos] > limits->maxValue[i]) {
        readback->value[0][pos] = limits->maxValue[i];
      } else if (readback->value[0][pos] < limits->minValue[i]) {
        readback->value[0][pos] = limits->minValue[i];
      }
    }
  }

  if (readback->despike && despikeParam->threshold > 0) {
    if (despikeParam->file) {
      if ((tempArray = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (i = 0, j = 0; i < readback->n; i++) {
        if (readback->despike[i]) {
          tempArray[j] = readback->value[0][i];
          j++;
        }
      }
      if ((spikesDone = despikeData(tempArray, j, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
        if (verbose) {
          fprintf(stderr, "%ld spikes removed.\n", spikesDone);
        }
      }
      /* if some PV are not to be despiked, then restore their values
	     after despiking */
      restored = 0;
      for (i = 0, j = 0; i < readback->n; i++) {
        if (readback->despike[i]) {
          readback->value[0][i] = tempArray[j];
          j++;
        } else {
          /* leave readback->value[0][i] unchanged */
          restored++;
        }
      }
      if ((restored) && (verbose))
        fprintf(stderr, "%ld despiked values restored.\n", restored);
      free(tempArray);
    } else if ((spikesDone = despikeData(readback->value[0], readback->n, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
      if (verbose) {
        fprintf(stderr, "%ld spikes removed.\n", spikesDone);
      }
    }
  }

  if (readback->despike && (despikeParam->threshold > 0 || limits->flag)) {
    index_min_max(&min_index, &max_index, readback->value[0], readback->n);
    if (readback->value[0][max_index] > -readback->value[0][min_index]) {
      largest_index = max_index;
    } else {
      largest_index = min_index;
    }
    readbackAdjustedStats->ave = arithmeticAverage(readback->value[0], readback->n);
    readbackAdjustedStats->RMS = standardDeviation(readback->value[0], readback->n);
    readbackAdjustedStats->MAD = meanAbsoluteDeviation(readback->value[0], readback->n);
    readbackAdjustedStats->largest = readback->value[0][largest_index];
    readbackAdjustedStats->largestName = readback->controlName[largest_index];
    sprintf(readbackAdjustedStats->msg, "Adjusted readback devs  %10.3g %10.3g"
                                        " %10.3g %10.3g (%s)\n",
            readbackAdjustedStats->ave, readbackAdjustedStats->RMS, readbackAdjustedStats->MAD, readbackAdjustedStats->largest, readbackAdjustedStats->largestName);
  } else {
    readbackAdjustedStats->msg[0] = 0;
  }
  return;
}

long getControlDevices(CONTROL_NAME *control, STATS *controlStats, LOOP_PARAM *loopParam, WAVE_FORMS *controlWaveforms, long verbose, double pendIOTime) {
  /*long maxIndex, minIndex, largestIndex; */
  long i;
  if (!control->file || !control->integral)
    return 0;
  if (verbose)
    fprintf(stderr, "Reading control devices at %f seconds.\n", loopParam->elapsedTime[0]);
  if (controlWaveforms->waveforms) {
    if (ReadWaveformData(controlWaveforms, pendIOTime)) {
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < control->n; i++) {
      control->value[0][i] = controlWaveforms->readbackValue[i];
    }
  } else {
    readPVs(control->controlName, control->value[0], control->channelInfo, control->n, NULL, pendIOTime);
  }
  /*
     controlStats->RMS = standardDeviation( control->value[0], control->n);
     controlStats->ave = arithmeticAverage( control->value[0], control->n);
     controlStats->MAD = meanAbsoluteDeviation( control->value[0], control->n);
     index_min_max( &minIndex, &maxIndex, control->value[0], control->n);
     if ( control->value[maxIndex] > - control->value[0][minIndex] ) {
     largestIndex = maxIndex;
     }
     else {
     largestIndex = minIndex;
     }
     controlStats->largest = control->value[0][largestIndex];
     controlStats->largestName = control->controlName[largestIndex];
     sprintf( controlStats->msg, "Control devices         %10.3g %10.3g"
     " %10.3g %10.3g (%s)",
     controlStats->ave, controlStats->RMS, controlStats->MAD,
     controlStats->largest, controlStats->largestName);  */
  return (0);
}

#ifdef USE_RUNCONTROL
void checkRCParam(SDDS_TABLE *inputPage, char *file) {

  if (sddscontrollawGlobal->rcParam.PVparam) {
    switch (SDDS_CheckParameter(inputPage, sddscontrollawGlobal->rcParam.PVparam, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!SDDS_GetParameter(inputPage, sddscontrollawGlobal->rcParam.PVparam, sddscontrollawGlobal->rcParam.PV)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with parameter %s in file %s.\n", sddscontrollawGlobal->rcParam.PVparam, file);
      FreeEverything();
      exit(1);
    }
  }
  if (sddscontrollawGlobal->rcParam.DescParam) {
    switch (SDDS_CheckParameter(inputPage, sddscontrollawGlobal->rcParam.DescParam, NULL, SDDS_STRING, stderr)) {
    case SDDS_CHECK_OKAY:
      if (!SDDS_GetParameter(inputPage, sddscontrollawGlobal->rcParam.DescParam, sddscontrollawGlobal->rcParam.Desc)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      break;
    default:
      fprintf(stderr, "Something wrong with parameter %s in file %s.\n", sddscontrollawGlobal->rcParam.DescParam, file);
      FreeEverything();
      exit(1);
    }
  }
  return;
}
#endif

void despikeTestValues(TESTS *test, DESPIKE_PARAM *despikeParam, long verbose) {

  long i, spikesDone;
  double *despikeTestData;

  if ((despikeParam->threshold > 0) && test->despikeValues) {
    if ((despikeTestData = (double *)malloc(sizeof(double) * (test->despikeValues))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < test->despikeValues; i++) {
      despikeTestData[i] = test->value[test->despikeIndex[i]];
    }
    /* uses the same despike parameters as the readback values */
    if ((spikesDone = despikeData(despikeTestData, test->despikeValues, despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
      if (verbose) {
        fprintf(stderr, "%ld spikes from test variables removed.\n", spikesDone);
      }
    }
    for (i = 0; i < test->despikeValues; i++) {
      test->value[test->despikeIndex[i]] = despikeTestData[i];
    }
    free(despikeTestData);
  }
  return;
}

long checkOutOfRange(TESTS *test, BACKOFF *backoff, STATS *readbackStats, STATS *readbackAdjustedStats, STATS *controlStats, LOOP_PARAM *loopParam, double timeOfDay, long verbose, long warning) {

  long i, outOfRange;

  outOfRange = 0; /* flag to indicate an out-of-range PV */
  test->longestSleep = 0;
  test->longestReset = 0;
  test->writeToGlitchLogger = 0;
  for (i = 0; i < test->n; i++) {
    if (test->value[i] < test->min[i] || test->value[i] > test->max[i]) {
      test->outOfRange[i] = 1;
      outOfRange = 1;
      /* determine longest time of those tests that failed */
      if (test->sleep)
        test->longestSleep = MAX(test->longestSleep, test->sleep[i]);
      if (test->reset)
        test->longestReset = MAX(test->longestReset, test->reset[i]);
      if (test->holdOff)
        test->longestHoldOff = MAX(test->longestHoldOff, test->holdOff[i]);
      if (test->glitchLog && ((test->glitchLog[i] == 'y') || (test->glitchLog[i] == 'Y')))
        test->writeToGlitchLogger = 1;
    } else {
      test->outOfRange[i] = 0;
    }
  }
  if (outOfRange) {
    if (verbose || warning) {
      backoff->counter++;
      if (backoff->counter >= backoff->level) {
        backoff->counter = 0;
        if (backoff->level < 8)
          backoff->level *= 2;
        if (verbose) {
          fprintf(stderr, "Test variables out of range at %f seconds.\n", loopParam->elapsedTime[0]);
          fprintf(stderr, "Stats                     Average       rms        mad     Largest\n"
                          "%s\n%s%s\n\n",
                  readbackStats->msg, readbackAdjustedStats->msg, controlStats->msg);
        }
        fprintf(stderr, "Test variables out of range:\n");
        for (i = 0; i < test->n; i++) {
          if (test->outOfRange[i])
            fprintf(stderr, "\t%s\t%f\n", test->controlName[i], test->value[i]);
        }
        fprintf(stderr, "Waiting for %f seconds.\n", MAX(test->longestSleep, loopParam->interval));
        fflush(stderr);
      }
    }
  }
  return (outOfRange);
}

 /* FYI, it is only called one place, and after the matrix multiplication has been made to get a vector of corrector */
 void apply_filter(CORRECTION *correction, long verbose) {
  long i, k;
  CONTROL_NAME *control;
  MATRIX *A, *B;
  long aorder, border, sign;
  double *ptr;

  control = correction->control;
  /* filtering follows the equation 
     a0 y[k] + a1 y[k-1] + a2 y[k-2] = b0 x[k] + b1 x[k-1] + b2 x[k-2]
     where x is the raw data, i.e. control->history->a[0][i], also, controlComp->error[i]
     and y is the data to be output, i.e. control->historyFiltered->a[0][i], also controlComp->delta[0][i]
   */
  /* 0th column of history is x(n)        
     1th column of history is x(n-1), etc */
  /* Rotate history pointers. Throw out last but
     use the memory allocated for this one for the new readings */
  border = control->history->n - 1;
  ptr = control->history->a[border];
  for (k = border; k > 0; k--)
    control->history->a[k] = control->history->a[k - 1];
  control->history->a[0] = ptr;
  /* filter the control->error[i] quantity rather than 
     control->value[0][i] */
  for (i = 0; i < control->n; i++) {
    control->history->a[0][i] = control->delta[0][i];
  }

  if (verbose) {
    fprintf( stdout, "calling apply_filter...\n");
    for (i = 0; i < control->n; i++) {
      fprintf( stdout, "%s:\n", control->controlName[i]);
      for (k = 0; k <= border; k++)
        fprintf( stdout, "x[%ld]=%f  ", k, control->history->a[k][i]);
      fprintf( stdout, "\n");
    }
  }
  
  /* 0th column of history is y(n-1)        */
  /* 1th column of history is y(n-2), etc   */
  aorder = control->historyFiltered->n - 1;
  ptr = control->historyFiltered->a[aorder];
  for (k = aorder; k > 0; k--)
    control->historyFiltered->a[k] = control->historyFiltered->a[k - 1];
  control->historyFiltered->a[0] = ptr;
  /* elements controlComp->history->a[0][k] are to be determined next. */


 
  A = correction->aCoef;
  B = correction->bCoef;
  aorder = A->n - 1;
  border = B->n - 1;
  /* loop over correctors, and do a direct product of matrices */
  /* i.e.   a0 y[k] + a1 y[k-1] + a2 y[k-2] = b0 x[k] + b1 x[k-1] + b2 x[k-2] */
 
  for (i = 0; i < control->n; i++) {
    control->historyFiltered->a[0][i] = 0;
    for (k = 1; k <= aorder; k++) {
#ifdef FLOAT_MATH
      control->historyFiltered->a[0][i] -= ((float)A->a[k][i]) * ((float)control->historyFiltered->a[k][i]);
#else
      control->historyFiltered->a[0][i] -= A->a[k][i] * control->historyFiltered->a[k][i];
#endif
    }
    for (k = 0; k <= border; k++) {
#ifdef FLOAT_MATH
      control->historyFiltered->a[0][i] += ((float)B->a[k][i]) * ((float)control->history->a[k][i]);
#else
      control->historyFiltered->a[0][i] += B->a[k][i] * control->history->a[k][i];
#endif
    }
    control->historyFiltered->a[0][i] /= A->a[0][i];
    /*    if( verbose ) {
      fprintf(stderr,"after filtering %s   ", control->controlName[i]);
      for (k=0; k<aorder+1; k++) 
        fprintf(stderr,"a=%ld filtered=%11.6f ", k,  control->historyFiltered->a[k][i]);
      fprintf(stderr, "\n");
      for (k=0; k<border+1; k++)
        fprintf(stderr, "b=%ld history=%11.6f ", k,  control->history->a[k][i]);
      fprintf(stderr, "\n");
    } 
    */
    
    sign = 1; /* added sign variable for debugging */
    /* reevaluated  controlComp->delta[0] after filtering */
    control->delta[0][i] = sign * control->historyFiltered->a[0][i];
    control->value[0][i] = control->delta[0][i] + control->old[i];
  }
  if (verbose) {
    for (i = 0; i < control->n; i++) {
      fprintf( stdout, "%s:\n", control->controlName[i]);
      for (k = 0; k <= aorder; k++)
        fprintf( stdout, "y[%ld]=%f  ", k, control->historyFiltered->a[k][i]);
      fprintf( stdout, "\n");
    }
  }
  
  /*  fprintf(stderr,"%s initial[0]: %8.3f; old[0]: %8.3f; new[0]: %8.3f\n",
     control->controlName[0],
     control->initial[0], control->old[0], control->value[0][0]); */
}

void controlLaw(long skipIteration, LOOP_PARAM *loopParam, CORRECTION *correction, CORRECTION *compensation, long verbose, double pendIOTime) {
  long i, j, k;
  CONTROL_NAME *control, *controlComp, *readback, *readbackComp;
  MATRIX *K, *A, *B;
  long aorder, border, sign;
  double *ptr;
#ifdef FLOAT_MATH
  float accumulator;
#else
  double accumulator;
#endif

  readback = correction->readback;
  control = correction->control;
  readbackComp = compensation->readback;
  controlComp = compensation->control;

  K = correction->K;
  if (skipIteration) {
    /* make no change in correction */
    for (i = 0; i < control->n; i++) {
      control->old[i] = control->value[0][i];
      control->delta[0][i] = 0;
    }
  } else {
    for (i = 0; i < control->n; i++) {
      control->old[i] = control->value[0][i];
      if (!control->integral)
        /* for proportional mode the correction control values
	       is made proportional to the error readback. The values
	       in control->value[0][i] are set to zero so that the 
	       increment expression (-=) used later will not actually
	       make a relative change to the actuator values.
               This looks like a kludge; it may break some additional feature later.
	     */
        control->old[i] = control->value[0][i] = 0;
      if (loopParam->gainPV) {
        readPVs(&loopParam->gainPV, &loopParam->gain, &loopParam->gainPVInfo, 1, NULL, pendIOTime);
      }

      if (loopParam->holdPresentValues) {
        for (j = accumulator = 0; j < readback->n; j++) {
          /* for hold present values mode, correct relative to 
		     initial values stored in readback->initial[j]
		   */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * (((float)readback->value[0][j]) - ((float)readback->initial[j]));
#else
          accumulator += K->a[i][j] * (readback->value[0][j] - readback->initial[j]);
#endif
        }
      } else {
        for (j = accumulator = 0; j < readback->n; j++) {
          /* standard feedback using readback as error signal
		     to apply a change to control values
		   */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * ((float)readback->value[0][j]);
#else
          accumulator += K->a[i][j] * readback->value[0][j];
#endif
        }
      }
      control->value[0][i] -= accumulator * loopParam->gain;
      control->delta[0][i] = control->value[0][i] - control->old[i];
    }
    if (correction->coefFile)
      apply_filter(correction, verbose);
    if (verbose)
      fprintf(stderr, "%s initial[0]: %8.3f; old[0]: %8.3f; new[0]: %8.3f\n", control->controlName[0], control->initial[0], control->old[0], control->value[0][0]);
  }

  /* determine additional correction with auxiliary matrix */
  /* for now the sets of actuator control quantities for the two matrices should not and
     cannot intersect. Otherwise one of the values will be overwritten. */
  if (compensation->file) {
    K = compensation->K;
    if (skipIteration) {
      /* make no change in correction */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->old[i] = controlComp->value[0][i];
        controlComp->delta[0][i] = 0;
      }
    } else {
      /* the signal for compensation (controlComp->error) 
	     is the same as the output
	     from the previous correction (control->delta[0]) */
      for (j = 0; j < readbackComp->n; j++) {
        readbackComp->error[j] = control->delta[0][j];
      }

      for (i = 0; i < controlComp->n; i++) {
        controlComp->old[i] = controlComp->value[0][i];
        controlComp->delta[0][i] = 0.0;
        controlComp->error[i] = 0.0;
        if (!controlComp->integral)
          /* for proportional mode the correction control values
		   is made proportional to the error readback. The values
		   in control->value[0][i] are set to zero so that the 
		   increment expression (-=) used later will not actually
		   make a relative change to the actuator values.
		 */
          controlComp->value[0][i] = controlComp->old[i] = 0;
        for (j = accumulator = 0; j < readbackComp->n; j++) {
          /* cascade calculation using correction effort above
		     to apply a change to control values.
		     Also the flag holdPresentValues has no meaning here.
		   */
          /* + sign is the convention for feedforward compensation */
#ifdef FLOAT_MATH
          accumulator += ((float)K->a[i][j]) * ((float)readbackComp->error[j]);
#else
          accumulator += K->a[i][j] * readbackComp->error[j];
#endif
          /* controlComp->delta[0][i] may be modified by filtering below */
        }
        controlComp->delta[0][i] += accumulator * loopParam->compensationGain;
      }
    }
    if (compensation->coefFile) {
      /* filtering follows the equation 
	     a0 y[k] + a1 y[k-1] + a2 y[k-2] = b0 x[k] + b1 x[k-1] + b2 x[k-2]
	     where x is the raw data, i.e. controlComp->error[i]
	     and y is the data to be output, i.e. controlComp->delta[0][i]
	   */
      /* 0th column of history is x(n)        
	     1th column of history is x(n-1), etc */
      /* Rotate history pointers. Throw out last but
	     use the memory allocated for this one for the new readings */
      border = controlComp->history->n - 1;
      ptr = controlComp->history->a[border];
      for (k = border; k > 0; k--)
        controlComp->history->a[k] = controlComp->history->a[k - 1];
      controlComp->history->a[0] = ptr;
      /* filter the controlComp->error[i] quantity rather than 
	     controlComp->value[0][i] */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->history->a[0][i] = controlComp->delta[0][i];
      }

      /* 0th column of history is y(n-1)        */
      /* 1th column of history is y(n-2), etc   */
      aorder = controlComp->historyFiltered->n - 1;
      ptr = controlComp->historyFiltered->a[aorder];
      for (k = aorder; k > 0; k--)
        controlComp->historyFiltered->a[k] = controlComp->historyFiltered->a[k - 1];
      controlComp->historyFiltered->a[0] = ptr;
      /* elements controlComp->history->a[0][k] are to be determined next. */

      A = compensation->aCoef;
      B = compensation->bCoef;
      aorder = A->n - 1;
      border = B->n - 1;
      /* loop over correctors, and do a direct product of matrices */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->historyFiltered->a[0][i] = 0;
        for (k = 1; k <= aorder; k++) {
#ifdef FLOAT_MATH
          controlComp->historyFiltered->a[0][i] -= ((float)A->a[k][i]) * ((float)controlComp->historyFiltered->a[k][i]);
#else
          controlComp->historyFiltered->a[0][i] -= A->a[k][i] * controlComp->historyFiltered->a[k][i];
#endif
        }
        for (k = 0; k <= border; k++) {
#ifdef FLOAT_MATH
          controlComp->historyFiltered->a[0][i] += ((float)B->a[k][i]) * ((float)controlComp->history->a[k][i]);
#else
          controlComp->historyFiltered->a[0][i] += B->a[k][i] * controlComp->history->a[k][i];
#endif
        }
        controlComp->historyFiltered->a[0][i] /= A->a[0][i];
        if (verbose)
          fprintf(stderr, "%3ld   %11.6f %11.6f %11.6f    %11.6f %11.6f %11.6f\n", i,
                  controlComp->historyFiltered->a[0][i], controlComp->historyFiltered->a[1][i], controlComp->historyFiltered->a[2][i], controlComp->history->a[0][i], controlComp->history->a[1][i], controlComp->history->a[2][i]);
        sign = 1; /* added sign variable for debugging */
        /* reevaluated  controlComp->delta[0] after filtering */
        controlComp->delta[0][i] = sign * controlComp->historyFiltered->a[0][i];
        /*here should use old instead of initial; H. Shang 5-9-2017 */
        controlComp->value[0][i] = controlComp->delta[0][i] + controlComp->initial[i];
      }
      fprintf(stderr, "%s initial[0]: %8.3f old[0]: %8.3f new[0]: %8.3f\n", controlComp->controlName[0], controlComp->initial[0], controlComp->old[0], controlComp->value[0][0]);
    } else {
      /* no filter files given */
      for (i = 0; i < controlComp->n; i++) {
        controlComp->value[0][i] += controlComp->delta[0][i];
      }
    }
  }
  return;
}

double applyDeltaLimit(LIMITS *delta, LOOP_PARAM *loopParam, CONTROL_NAME *control, long verbose, long warning) {

  long i;
  double factor, trialFactor, change;

  factor = 1;
  if (delta->flag & LIMIT_VALUE) {
    if (control->integral) {
      for (i = 0; i < control->n; i++) {
        if ((change = fabs(control->value[0][i] - control->old[i])) > delta->singleValue) {
          trialFactor = delta->singleValue / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    } else {
      for (i = 0; i < control->n; i++) {
        if ((change = fabs(control->value[0][i])) > delta->singleValue) {
          trialFactor = delta->singleValue / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    }
  } else if (delta->flag & LIMIT_FILE) {
    if (control->integral) {
      for (i = 0; i < delta->n; i++) {
        if ((change = fabs(control->value[0][delta->index[i]] - control->old[delta->index[i]])) > delta->value[i]) {
          trialFactor = delta->value[i] / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    } else {
      for (i = 0; i < delta->n; i++) {
        if ((change = fabs(control->value[0][delta->index[i]])) > delta->value[i]) {
          trialFactor = delta->value[i] / change;
          if (trialFactor < factor)
            factor = trialFactor;
        }
      }
    }
  }

  if (factor < 1) {
    if (warning || verbose)
      fprintf(stderr, "Limiting changes by factor %.2g\n", factor);
    if (control->integral) {
      for (i = 0; i < control->n; i++) {
        control->delta[0][i] = (control->value[0][i] - control->old[i]) * factor;
        control->value[0][i] = control->old[i] + control->delta[0][i];
      }
    } else {
      for (i = 0; i < control->n; i++) {
        control->value[0][i] *= factor;
        control->delta[0][i] *= factor;
      }
    }
  }
  return factor;
}

void calcControlDeltaStats(CONTROL_NAME *control, STATS *controlStats, STATS *controlDeltaStats) {
  int64_t minIndex, maxIndex, largestIndex;

  /* calculate control stats */
  controlStats->RMS = standardDeviation(control->value[0], control->n);
  controlStats->ave = arithmeticAverage(control->value[0], control->n);
  controlStats->MAD = meanAbsoluteDeviation(control->value[0], control->n);
  index_min_max(&minIndex, &maxIndex, control->value[0], control->n);
  if (control->value[0][maxIndex] > -control->value[0][minIndex]) {
    largestIndex = maxIndex;
  } else {
    largestIndex = minIndex;
  }
  controlStats->largest = control->value[0][largestIndex];
  controlStats->largestName = control->controlName[largestIndex];
  sprintf(controlStats->msg, "Control devices         %10.3g %10.3g"
                             " %10.3g %10.3g (%s)",
          controlStats->ave, controlStats->RMS, controlStats->MAD, controlStats->largest, controlStats->largestName);

  /*calculate control delta stats */
  controlDeltaStats->RMS = standardDeviation(control->delta[0], control->n);
  controlDeltaStats->ave = arithmeticAverage(control->delta[0], control->n);
  controlDeltaStats->MAD = meanAbsoluteDeviation(control->delta[0], control->n);
  index_min_max(&minIndex, &maxIndex, control->delta[0], control->n);
  if (control->delta[0][maxIndex] > -control->delta[0][minIndex]) {
    largestIndex = maxIndex;
  } else {
    largestIndex = minIndex;
  }
  controlDeltaStats->largest = control->delta[0][largestIndex];
  controlDeltaStats->largestName = control->controlName[largestIndex];
  sprintf(controlDeltaStats->msg, "Control device deltas   %10.3g %10.3g"
                                  " %10.3g %10.3g (%s)",
          controlDeltaStats->ave, controlDeltaStats->RMS, controlDeltaStats->MAD, controlDeltaStats->largest, controlDeltaStats->largestName);
  return;
}

void writeToOutputFile(char *outputFile, SDDS_TABLE *outputPage, long *outputRow, LOOP_PARAM *loopParam, CORRECTION *correction, TESTS *test, WAVEFORM_TESTS *waveform_tests) {
  float readbackValuesSPrec, controlValuesSPrec, testValueSPrec;
  long i, j;
  CONTROL_NAME *control, *readback;

  readback = correction->readback;
  control = correction->control;
  if (outputFile) {
    /* first three columns */
    if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, "Step", loopParam->step[0], "Time", loopParam->epochTime[0], "ElapsedTime", loopParam->elapsedTime[0], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++) {
      readbackValuesSPrec = (float)readback->value[0][i];
      if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, readback->symbolicName[i], readbackValuesSPrec, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      controlValuesSPrec = (float)control->value[0][i];
      if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, control->symbolicName[i], controlValuesSPrec, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (test && test->file) {
      for (i = 0; i < test->n; i++) {
        if (test->writeToFile[i]) {
          testValueSPrec = (float)test->value[i];
          if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, test->controlName[i], testValueSPrec, NULL)) {
            SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
            FreeEverything();
            exit(1);
          }
        }
      }
    }
    if (waveform_tests->waveformTests) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
          if (waveform_tests->writeToFile[i][j]) {
            if (!SDDS_SetRowValues(outputPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *outputRow, waveform_tests->DeviceName[i][j], waveform_tests->waveformData[i][j], NULL)) {
              SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
              FreeEverything();
              exit(1);
            }
          }
        }
      }
    }

    if (!(((loopParam->step[0]) + 1) % loopParam->updateInterval)) {
      if (!SDDS_UpdatePage(outputPage, FLUSH_TABLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else if (loopParam->step[0] == (loopParam->steps) - 1) {
      if (!SDDS_UpdatePage(outputPage, FLUSH_TABLE)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    (*outputRow)++;
  }
  return;
}

void writeToStatsFile(char *statsFile, SDDS_TABLE *statsPage, long *statsRow, LOOP_PARAM *loopParam, STATS *readbackStats, STATS *readbackDeltaStats, STATS *controlStats, STATS *controlDeltaStats) {

  if (statsFile) {
    if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow, "Step", loopParam->step[0], "Time", loopParam->epochTime[0], "ElapsedTime", loopParam->elapsedTime[0], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!loopParam->briefStatistics) {
      if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow,
                             "readbackRMS", readbackStats->RMS,
                             "readbackAve", readbackStats->ave,
                             "readbackMAD", readbackStats->MAD,
                             "readbackDeltaRMS", readbackDeltaStats->RMS,
                             "readbackDeltaAve", readbackDeltaStats->ave,
                             "readbackDeltaMAD", readbackDeltaStats->MAD,
                             "controlRMS", controlStats->RMS,
                             "controlAve", controlStats->ave,
                             "controlMAD", controlStats->MAD,
                             "controlDeltaRMS", controlDeltaStats->RMS,
                             "controlDeltaAve", controlDeltaStats->ave,
                             "controlDeltaMAD", controlDeltaStats->MAD, NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    if (!SDDS_SetRowValues(statsPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *statsRow,
                           "readbackLargest", readbackStats->largest,
                           "readbackLargestName", readbackStats->largestName,
                           "readbackDeltaLargest", readbackDeltaStats->largest,
                           "readbackDeltaLargestName", readbackDeltaStats->largestName,
                           "controlLargest", controlStats->largest,
                           "controlLargestName", controlStats->largestName,
                           "controlDeltaLargest", controlDeltaStats->largest,
                           "controlDeltaLargestName", controlDeltaStats->largestName, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!((loopParam->step[0] + 1) % loopParam->updateInterval)) {
      if (!SDDS_UpdatePage(statsPage, FLUSH_TABLE) || !SDDS_FreeStringData(statsPage)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else if (loopParam->step[0] == loopParam->steps - 1) {
      if (!SDDS_UpdatePage(statsPage, FLUSH_TABLE) || !SDDS_FreeStringData(statsPage)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    (*statsRow)++;
  }
  return;
}

#include <time.h>

void writeToGlitchFile(GLITCH_PARAM *glitchParam, SDDS_TABLE *glitchPage, long *glitchRow, LOOP_PARAM *loopParam, STATS *readbackAdjustedStats, STATS *controlDeltaStats, CORRECTION *correction, CORRECTION *overlapCompensation, TESTS *test) {
  long i, n, index;
  CONTROL_NAME *control, *readback;
  CONTROL_NAME *controlComp = NULL, *readbackComp = NULL;
  char *buffer = NULL, *buffer2 = NULL;
  char *timeStamp = NULL;

  readback = correction->readback;
  control = correction->control;
  if (overlapCompensation->file) {
    readbackComp = overlapCompensation->readback;
    controlComp = overlapCompensation->control;
  }
  if (!glitchParam->file)
    return;
  if (glitchParam->avail_rows < glitchParam->rows)
    glitchParam->avail_rows++;
  glitchParam->glitched = 1;
  if ((controlDeltaStats->RMS <= glitchParam->controlRMSthreshold) && (readbackAdjustedStats->RMS <= glitchParam->readbackRMSthreshold) && (!test->writeToGlitchLogger)) {
    glitchParam->glitched = 0;
  }
  /* determine if glitch data should be written to a file */
  if (glitchParam->glitched == 0) {
    if (glitchParam->rows > 1) {
      glitchParam->row_pointer++;
      if (glitchParam->row_pointer == glitchParam->rows)
        glitchParam->row_pointer = 1;
      loopParam->step[glitchParam->row_pointer] = loopParam->step[0];
      loopParam->epochTime[glitchParam->row_pointer] = loopParam->epochTime[0];
      loopParam->elapsedTime[glitchParam->row_pointer] = loopParam->elapsedTime[0];
      for (n = 0; n < readback->n; n++)
        readback->value[glitchParam->row_pointer][n] = readback->value[0][n];
      for (n = 0; n < control->n; n++) {
        control->value[glitchParam->row_pointer][n] = control->value[0][n];
        control->delta[glitchParam->row_pointer][n] = control->delta[0][n];
      }
      if (overlapCompensation->file) {
        for (n = 0; n < readbackComp->n; n++)
          readbackComp->value[glitchParam->row_pointer][n] = readbackComp->value[0][n];
        for (n = 0; n < controlComp->n; n++) {
          controlComp->value[glitchParam->row_pointer][n] = controlComp->value[0][n];
          controlComp->delta[glitchParam->row_pointer][n] = controlComp->delta[0][n];
        }
      }
    }
    /*
         for (i=glitchParam->rows-1 ; i > 0 ; i--) {
         loopParam->step[i] = loopParam->step[i-1];
         loopParam->epochTime[i] = loopParam->epochTime[i-1];
         loopParam->elapsedTime[i] = loopParam->elapsedTime[i-1];
         for (n = 0; n < readback->n; n++)
         readback->value[i][n] = readback->value[i-1][n];
         for (n = 0; n < control->n; n++) {
         control->value[i][n] = control->value[i-1][n];
         control->delta[i][n] = control->delta[i-1][n];
         }
         if (overlapCompensation->file) {
         for (n = 0; n < readbackComp->n; n++)
         readbackComp->value[i][n] = readbackComp->value[i-1][n];
         for (n = 0; n < controlComp->n; n++) {
         controlComp->value[i][n] = controlComp->value[i-1][n];
         controlComp->delta[i][n] = controlComp->delta[i-1][n];
         }
         }
         }
       */
    return;
  }

  if (!SDDS_StartTable(glitchPage, loopParam->updateInterval * glitchParam->rows)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  getTimeBreakdown(NULL, NULL, NULL, NULL, NULL, NULL, &timeStamp);
  if (!SDDS_SetParameters(glitchPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, "TimeStamp", timeStamp, "ReadbackRmsThreshold", glitchParam->readbackRMSthreshold, "ControlRmsThreshold", glitchParam->controlRMSthreshold, "GlitchRows", glitchParam->rows, NULL)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }

  /* first three columns, as in output file  */
  for (n = glitchParam->avail_rows - 1; n >= 0; n--) {
    if ((glitchParam->rows < 2) || (n == 0))
      index = 0;
    else if (glitchParam->row_pointer < n)
      index = glitchParam->avail_rows - (n - glitchParam->row_pointer);
    else
      index = glitchParam->row_pointer - n + 1;
    if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, "Step", loopParam->step[index], "Time", loopParam->epochTime[index], "ElapsedTime", loopParam->elapsedTime[index], NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < readback->n; i++) {
      if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, readback->symbolicName[i], (float)readback->value[index][i], NULL)) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }
    for (i = 0; i < control->n; i++) {
      if ((buffer = malloc(sizeof(char) * (strlen(control->symbolicName[i]) + 6))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      sprintf(buffer, "%sDelta", control->symbolicName[i]);
      if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, control->symbolicName[i], (float)control->value[index][i], buffer, (float)control->delta[index][i], NULL)) {
        free(buffer);
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      free(buffer);
    }
    if (overlapCompensation->file) {
      for (i = 0; i < readbackComp->n; i++) {
        if ((buffer = malloc(sizeof(char) * (strlen(readbackComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer, "%sAuxil", readbackComp->symbolicName[i]);
        if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, buffer, (float)readbackComp->value[index][i], NULL)) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
      for (i = 0; i < controlComp->n; i++) {
        if ((buffer = malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 6))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer, "%sAuxil", controlComp->symbolicName[i]);
        if ((buffer2 = malloc(sizeof(char) * (strlen(controlComp->symbolicName[i]) + 11))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        sprintf(buffer2, "%sAuxilDelta", controlComp->symbolicName[i]);
        if (!SDDS_SetRowValues(glitchPage, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, *glitchRow, buffer, (float)controlComp->value[index][i], buffer2, (float)controlComp->delta[index][i], NULL)) {
          free(buffer);
          free(buffer2);
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        free(buffer);
        free(buffer2);
      }
    }
    (*glitchRow)++;
  }
  if (!SDDS_WritePage(glitchPage)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  /*
     if (!( ( (loopParam->step[0]) + 1)%loopParam->updateInterval)) {
     if (!SDDS_UpdatePage( glitchPage, FLUSH_TABLE)){
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
     FreeEverything();
     exit(1);
     }
     } else if (loopParam->step[0] == (loopParam->steps) - 1) {
     if (!SDDS_UpdatePage( glitchPage, FLUSH_TABLE)){
     SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
     FreeEverything();
     exit(1);
     }
     } 
   */
  /*
     for (i=glitchParam->rows-1 ; i > 0 ; i--) {
     loopParam->step[i] = loopParam->step[i-1];
     loopParam->epochTime[i] = loopParam->epochTime[i-1];
     loopParam->elapsedTime[i] = loopParam->elapsedTime[i-1];
     for (n = 0; n < readback->n; n++)
     readback->value[i][n] = readback->value[i-1][n];
     for (n = 0; n < control->n; n++) {
     control->value[i][n] = control->value[i-1][n];
     control->delta[i][n] = control->delta[i-1][n];
     }
     if (overlapCompensation->file) {
     for (n = 0; n < readbackComp->n; n++)
     readbackComp->value[i][n] = readbackComp->value[i-1][n];
     for (n = 0; n < controlComp->n; n++) {
     controlComp->value[i][n] = controlComp->value[i-1][n];
     controlComp->delta[i][n] = controlComp->delta[i-1][n];
     }
     }
     }
   */
  glitchParam->row_pointer = 0;
  glitchParam->avail_rows = 0;
  *glitchRow = 0;
  return;
}

#if !defined(vxWorks)
void serverExit(int sig) {
  char s[1024];
  sprintf(s, "rm %s", sddscontrollawGlobal->pidFile);
  system(s);
  fprintf(stderr, "Program terminated by signal.\n");
  if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
    setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Terminated by signal", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 1);
    setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 2, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 1);
#  if !defined(DBAccess)
    if (ca_pend_io(10.0) != ECA_NORMAL) {
      fprintf(stderr, "pendIOerror: unable to put PV values\n");
      FreeEverything();
      exit(1);
    }
#  endif
  }
#  ifdef USE_RUNCONTROL
  if (sddscontrollawGlobal->rcParam.PV) {
    strcpy(sddscontrollawGlobal->rcParam.message, "Terminated by signal");
    sddscontrollawGlobal->rcParam.status = runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, NO_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
    if (sddscontrollawGlobal->rcParam.status != RUNCONTROL_OK) {
      fprintf(stderr, "Unable to write status message and alarm severity\n");
      FreeEverything();
      exit(1);
    }
  }
#  endif
  FreeEverything();
  exit(0);
}

void commandServer(int sig) {
  sddscontrollawGlobal->reparseFromFile = 1;
#  ifndef _WIN32
  signal(SIGUSR1, commandServer);
#  endif
  return;
}

void exitIfServerRunning() {
  FILE *fp;
  int pid;
  char s[1024];

  if (!(fp = fopen(sddscontrollawGlobal->pidFile, "r"))) {
    fprintf(stderr, "No server file found--starting new server\n");
    return;
  }
  pid = -1;
  if (fscanf(fp, "%d", &pid) != 1 ||
#  if defined(_WIN32) || defined(vxWorks)
      0
#  else
      getpgid(pid) == -1
#  endif
  ) {
#  ifdef DEBUG
    fprintf(stderr, "Old server pid was %ld\n", pid);
    fprintf(stderr, "Removing old server PID file\n");
#  endif
    sprintf(s, "rm %s", sddscontrollawGlobal->pidFile);
    if (system(s)) {
      fprintf(stderr, "Problem with %s.\n", s);
    }
#  ifdef DEBUG
    fprintf(stderr, "Starting new server\n");
#  endif
    return;
  }
#  ifdef DEBUG
  fprintf(stderr, "Server %s already running\n", sddscontrollawGlobal->pidFile);
#  endif
  FreeEverything();
  exit(0);
}

void setupServer() {
#  if defined(vxWorks)
  FreeEverything();
  SDDS_Bomb("server mode not supported in vxWorks.");
#  else
  FILE *fp;
  long pid;
  char s[1024];

  pid = getpid();
  if (!sddscontrollawGlobal->pidFile) {
    FreeEverything();
    SDDS_Bomb("Pointer to pidFile is NULL detected in setupServer.");
  }
  fprintf(stderr, "pid file is %s.\n", sddscontrollawGlobal->pidFile);
  if (!(fp = fopen(sddscontrollawGlobal->pidFile, "w"))) {
    FreeEverything();
    SDDS_Bomb("Unable to open PID file");
  }
  fprintf(fp, "%ld\n", pid);
  fclose(fp);
  sprintf(s, "chmod g+w %s", sddscontrollawGlobal->pidFile);
  system(s);

#    ifndef _WIN32
  signal(SIGUSR1, commandServer);
  signal(SIGUSR2, serverExit);
#    endif
#  endif
  return;
}
#endif

long parseArguments(char ***argv,
                    int *argc,
                    CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    char **statsFile,
                    long *serverMode,
                    char **commandFile,
                    char **outputFile,
                    char **controlLogFile,
                    long *testCASecurity,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    char **compensationOutputFile,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests,
                    long firstTime,
                    long *verbose,
                    long *warning,
                    double *pendIOTime,
                    char *commandline_option[COMMANDLINE_OPTIONS],
                    char *waveformOption[WAVEFORMOPTIONS]) {
  FILE *fp = NULL;
  SCANNED_ARG *s_arg = NULL;
  long i, i_arg, infinite;
  long unsigned dummyFlags;
  double timeout;
  long retries;
  char *mode = NULL;
  char *mode_option[2] = {
    "integral",
    "proportional",
  };
  char *statistics_mode_option[2] = {
    "full",
    "brief",
  };

  infinite = 0;

  if (!firstTime) {
    if (!(fp = fopen(*commandFile, "r"))) {
      fprintf(stderr, "Command file %s not found.\n", *commandFile);
      return (1);
    }
    ReadMoreArguments(argv, argc, NULL, 0, fp);
  }
  *argc = scanargs(&s_arg, *argc, *argv);

  if (firstTime) {
    *verbose = 0;
    *warning = 0;
    *pendIOTime = 30.0;
    *outputFile = NULL;
    *statsFile = NULL;
    sddscontrollawGlobal->pidFile = NULL;
    *commandFile = NULL;
    *serverMode = 0;
    *controlLogFile = NULL;
    *compensationOutputFile = NULL;
    *testCASecurity = 0;
    initializeData(correction, loopParam, aveParam, delta, readbackLimits, action, despikeParam, glitchParam, test, generations, overlapCompensation, readbackWaveforms, controlWaveforms, offsetWaveforms, ffSetpointWaveforms, waveform_tests);
  }
  for (i_arg = 1; i_arg < *argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, UNIQUE_MATCH)) {
      case CLO_DAILY_FILES:
        /* daily files are a special case of generations option */
        if (generations->doGenerations && !generations->daily) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("Options -generation and -daily are incompatible.");
        }
        generations->doGenerations = 1;
        generations->daily = 1;
        generations->digits = 4;
        generations->delimiter = ".";
        break;
      case CLO_GENERATIONS:
        if (generations->doGenerations && generations->daily) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("Options -generation and -daily are incompatible.");
        }
        generations->doGenerations = 1;
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "digits", SDDS_LONG, &(generations->digits), 1, 0,
                          "delimiter", SDDS_STRING, &(generations->delimiter), 1, 0,
                          "rowlimit", SDDS_LONG, &(generations->rowLimit), 1, 0,
                          "timelimit", SDDS_DOUBLE, &(generations->timeLimit), 1, 0, NULL) ||
            generations->digits < 1) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("invalid -generations syntax/values");
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_GAIN:
        if (s_arg[i_arg].n_items != 2) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("invalid -gain syntax (too many qualifiers)");
        }
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -gain syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -gain syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->gainPV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for gain PV.\n");
            if ((loopParam->gainPVInfo.count = malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawCAConnection(&loopParam->gainPV, &loopParam->gainPVInfo, 1, *pendIOTime);
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -gain syntax (unknown qualifier)");
          }
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->gain) != 1) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -gain syntax/value");
          }
        }
        break;
      case CLO_TIME_INTERVAL:
        if (s_arg[i_arg].n_items != 2) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("invalid -interval syntax (too many qualifiers)");
        }
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -interval syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -interval syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->intervalPV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for interval PV.\n");
            if ((loopParam->intervalPVInfo.count = malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawCAConnection(&loopParam->intervalPV, &loopParam->intervalPVInfo, 1, *pendIOTime);
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -interval syntax (unknown qualifier)");
          }
          readPVs(&loopParam->intervalPV, &loopParam->interval, &loopParam->intervalPVInfo, 1, NULL, *pendIOTime);
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%lf", &loopParam->interval) != 1) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -interval syntax/value");
          }
        }
        break;
      case CLO_PENDIOTIME:
        if ((s_arg[i_arg].n_items < 2) || !(get_double(pendIOTime, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no number given for option -pendIOTime.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_STEPS:
        if ((s_arg[i_arg].n_items < 2) || !(get_long(&loopParam->steps, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no number given for option -steps.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_INFINITELOOP:
        infinite = 1;
        break;
      case CLO_UPDATE_INTERVAL:
        if ((s_arg[i_arg].n_items < 2) || !(get_long(&loopParam->updateInterval, s_arg[i_arg].list[1]))) {
          fprintf(stderr, "no integer given for option -updateInterval.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_INTEGRAL:
        loopParam->integral = 1;
        break;
      case CLO_PROPORTIONAL:
        loopParam->integral = 0;
        break;
      case CLO_UNGROUPEZCACALLS:
        /* Do nothing because this option is obsolete */
        break;
      case CLO_EZCATIMING:
        /* This option is obsolete */
        if (s_arg[i_arg].n_items != 3) {
          FreeEverything();
          SDDS_Bomb("wrong number of items for -ezcaTiming");
        }
        if (sscanf(s_arg[i_arg].list[1], "%lf", &timeout) != 1 ||
            sscanf(s_arg[i_arg].list[2], "%ld", &retries) != 1 ||
            timeout <= 0 || retries < 0) {
          FreeEverything();
          bomb("invalid -ezca values", NULL);
        }
        *pendIOTime = timeout * (retries + 1);
        break;
      case CLO_DRY_RUN:
        loopParam->dryRun = 1;
        break;
      case CLO_HOLD_PRESENT_VALUES:
        loopParam->holdPresentValues = 1;
        break;
      case CLO_OFFSETS:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -offsets syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->offsetFile, s_arg[i_arg].list[1]);
        break;
      case CLO_SEARCHPATH:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -searchPath syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->searchPath, s_arg[i_arg].list[1]);
        break;
      case CLO_PVOFFSETS:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -PVOffsets syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        SDDS_CopyString(&loopParam->offsetPVFile, s_arg[i_arg].list[1]);
        break;
      case CLO_WAVEFORMS:
        if (s_arg[i_arg].n_items != 3) {
          fprintf(stderr, "bad -waveforms syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (!strlen(s_arg[i_arg].list[1])) {
          fprintf(stderr, "no filename is given for -waveforms syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        switch (match_string(s_arg[i_arg].list[2], waveformOption, WAVEFORMOPTIONS, UNIQUE_MATCH)) {
        case CLO_READBACKWAVEFORM:
          if ((readbackWaveforms->waveformFile = SDDS_Realloc(readbackWaveforms->waveformFile, sizeof(char *) * (readbackWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&readbackWaveforms->waveformFile[readbackWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          readbackWaveforms->waveformFiles++;
          break;
        case CLO_OFFSETWAVEFORM:
          if ((offsetWaveforms->waveformFile = SDDS_Realloc(offsetWaveforms->waveformFile, sizeof(char *) * (offsetWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&offsetWaveforms->waveformFile[offsetWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          offsetWaveforms->waveformFiles++;
          break;
        case CLO_ACTUATORWAVEFORM:
          if ((controlWaveforms->waveformFile = SDDS_Realloc(controlWaveforms->waveformFile, sizeof(char *) * (controlWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&controlWaveforms->waveformFile[controlWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          controlWaveforms->waveformFiles++;
          break;
        case CLO_FFSETPOINTWAVEFORM:
          if ((ffSetpointWaveforms->waveformFile = SDDS_Realloc(ffSetpointWaveforms->waveformFile, sizeof(char *) * (ffSetpointWaveforms->waveformFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&ffSetpointWaveforms->waveformFile[ffSetpointWaveforms->waveformFiles], s_arg[i_arg].list[1]);
          ffSetpointWaveforms->waveformFiles++;
          break;
        case CLO_TESTWAVEFORM:
          if ((waveform_tests->testFile = SDDS_Realloc(waveform_tests->testFile, sizeof(char *) * (waveform_tests->testFiles + 1))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SDDS_CopyString(&waveform_tests->testFile[waveform_tests->testFiles], s_arg[i_arg].list[1]);
          waveform_tests->testFiles++;
          break;
        default:
          fprintf(stderr, "unknown \"%s\" syntax for the <type> of -waveforms.\n", s_arg[i_arg].list[2]);
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          exit(1);
          break;
        }
        break;
      case CLO_RUNCONTROLPV:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddscontrollawGlobal->rcParam.PVparam, 1, 0,
                          "string", SDDS_STRING, &sddscontrollawGlobal->rcParam.PV, 1, 0,
                          "pingTimeout", SDDS_FLOAT, &sddscontrollawGlobal->rcParam.pingTimeout, 1, 0, NULL) ||
            (!sddscontrollawGlobal->rcParam.PVparam && !sddscontrollawGlobal->rcParam.PV) ||
            (sddscontrollawGlobal->rcParam.PVparam && sddscontrollawGlobal->rcParam.PV)) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb("bad -runControlPV syntax", "-runControlPV={string=<string>|parameter=<string>}\n");
        }
        s_arg[i_arg].n_items += 1;
        sddscontrollawGlobal->rcParam.pingTimeout *= 1000;
        if (sddscontrollawGlobal->rcParam.pingTimeout < 0) {
          fprintf(stderr, "Ping timeout for run control PV should be >= 0.");
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          exit(1);
        }
#else
        fprintf(stderr, "runControl is not available. Option -runControlPV ignored.\n");
#endif
        break;
      case CLO_RUNCONTROLDESC:
#ifdef USE_RUNCONTROL
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "parameter", SDDS_STRING, &sddscontrollawGlobal->rcParam.DescParam, 1, 0,
                          "string", SDDS_STRING, &sddscontrollawGlobal->rcParam.Desc, 1, 0, NULL) ||
            (!sddscontrollawGlobal->rcParam.DescParam && !sddscontrollawGlobal->rcParam.Desc) ||
            (sddscontrollawGlobal->rcParam.DescParam && sddscontrollawGlobal->rcParam.Desc)) {
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb("bad -runControlDesc syntax", "-runControlDescription={string=<string>|parameter=<string>}\n");
        }
        s_arg[i_arg].n_items += 1;
#else
        fprintf(stderr, "runControl is not available. Option -runControlDescription ignored.\n");
#endif
        break;
      case CLO_STATISTICS:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(statsFile, s_arg[i_arg].list[1])) {
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          SDDS_Bomb("No file given in option -statistics");
        }
        s_arg[i_arg].n_items -= 2;
        if (s_arg[i_arg].n_items > 0 && (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "mode", SDDS_STRING, &mode, 1, 0, NULL))) {
          fprintf(stderr, "invalid -statistics syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (mode) {
          switch (match_string(mode, statistics_mode_option, 2, 0)) {
          case 0:
            loopParam->briefStatistics = 0;
            free(mode);
            break;
          case 1:
            loopParam->briefStatistics = 1;
            free(mode);
            break;
          default:
            fprintf(stderr, "invalid mode given for -statistics syntax/values.\n");
            s_arg[i_arg].n_items += 2;
            free_scanargs(&s_arg, *argc);
            free(mode);
            return (1);
          }
        } else {
          loopParam->briefStatistics = 0;
        }
        s_arg[i_arg].n_items += 2;
        break;
      case CLO_SERVERMODE:
#if !defined(vxWorks)
        if (firstTime) {
          *serverMode = 1;
          if ((s_arg[i_arg].n_items -= 1) < 0 || !scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0, "pid", SDDS_STRING, &sddscontrollawGlobal->pidFile, 1, 0, "command", SDDS_STRING, commandFile, 1, 0, NULL)) {
            s_arg[i_arg].n_items += 1;
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("bad -servermode syntax/value");
          }
          s_arg[i_arg].n_items += 1;
        } else {
          fprintf(stderr, "%s option ignored when a subsequent command is issued in server mode.\nThe option may have already been specified in the original command, and would stay in force.\n", s_arg[i_arg].list[0]);
        }
#endif
        break;
      case CLO_VERBOSE:
        *verbose = 1;
        fprintf(stderr, "Link date: %s %s\n", __DATE__, __TIME__);
        break;
      case CLO_WARNING:
        *warning = 1;
        break;
      case CLO_TESTCASECURITY:
        *testCASecurity = 1;
        break;
      case CLO_TEST_VALUES:
        if ((s_arg[i_arg].n_items < 2) || !strlen(s_arg[i_arg].list[1]) || !SDDS_CopyString(&test->file, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No file given in option -testValues.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_ACTUATORCOL:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(&correction->actuator, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No string given in option -actuatorColumn.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_CONTROLLOG:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(controlLogFile, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No string given in option -controlLogFile.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_CONTROL_QUANTITY_DEFINITION:
        if ((s_arg[i_arg].n_items < 2) || !SDDS_CopyString(&correction->definitionFile, s_arg[i_arg].list[1])) {
          fprintf(stderr, "No file given in option -controlQuantityDefinition.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_AVERAGE:
        if ((s_arg[i_arg].n_items -= 2) < 0 || !scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0, "interval", SDDS_DOUBLE, &aveParam->interval, 1, 0, NULL) || aveParam->interval <= 0) {
          s_arg[i_arg].n_items += 2;
          free_scanargs(&s_arg, *argc);
          FreeEverything();
          bomb("bad -average syntax", "-average={<number>|PVname=<name>}[,interval=<seconds>]\n");
        }
        s_arg[i_arg].n_items += 2;
        if (!tokenIsNumber(s_arg[i_arg].list[1])) {
          char *ptr;
          /* should be PVname=<name> */
          if (!(ptr = strchr(s_arg[i_arg].list[1], '='))) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -average syntax (no value for PV name)");
          }
          *ptr++ = 0;
          str_tolower(s_arg[i_arg].list[1]);
          if (strlen(ptr) == 0) {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -average syntax (pv name is empty)");
          }
          if (strncmp("pvname", s_arg[i_arg].list[1], strlen(s_arg[i_arg].list[1])) == 0) {
            SDDS_CopyString(&loopParam->averagePV, ptr);
            if (*verbose)
              fprintf(stderr, "Set up connections for average PV.\n");
            if ((loopParam->averagePVInfo.count = malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
            SetupRawCAConnection(&loopParam->averagePV, &loopParam->averagePVInfo, 1, *pendIOTime);
          } else {
            free_scanargs(&s_arg, *argc);
            FreeEverything();
            SDDS_Bomb("invalid -average syntax (unknown qualifier)");
          }
          readPVs(&loopParam->averagePV, &aveParam->n2, &loopParam->averagePVInfo, 1, NULL, *pendIOTime);
          aveParam->n = round(aveParam->n2);
        } else {
          if (sscanf(s_arg[i_arg].list[1], "%ld", &aveParam->n) != 1) {
            fprintf(stderr, "bad -average syntax.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        }
        if (aveParam->n <= 0) {
          FreeEverything();
          SDDS_Bomb("invalid -average value");
        }
        break;
      case CLO_DELTALIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&delta->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &delta->singleValue, 1, LIMIT_VALUE, "file", SDDS_STRING, &delta->file, 1, LIMIT_FILE, NULL) ||
            (delta->flag & LIMIT_VALUE && delta->flag & LIMIT_FILE) || (!(delta->flag & LIMIT_VALUE) && !(delta->flag & LIMIT_FILE))) {
          fprintf(stderr, "bad -deltaLimit syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_READBACK_LIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&readbackLimits->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &readbackLimits->singleValue, 1, LIMIT_VALUE,
                          "minValue", SDDS_DOUBLE, &readbackLimits->singleMinValue, 1, LIMIT_MINVALUE, "maxValue", SDDS_DOUBLE, &readbackLimits->singleMaxValue, 1, LIMIT_MAXVALUE, "file", SDDS_STRING, &readbackLimits->file, 1, LIMIT_FILE, NULL)) {
          fprintf(stderr, "bad -readbackLimits syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        if ((readbackLimits->flag & LIMIT_MINVALUE && !(readbackLimits->flag & LIMIT_MAXVALUE)) || (!(readbackLimits->flag & LIMIT_MINVALUE) && readbackLimits->flag & LIMIT_MAXVALUE)) {
          fprintf(stderr, "Bad -readbackLimits syntax/value. Need both minValue and maxValue specified.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((readbackLimits->flag & LIMIT_MINVALUE && readbackLimits->flag & LIMIT_MAXVALUE))
          readbackLimits->flag |= LIMIT_MINMAXVALUE;
        i = 0;
        if (readbackLimits->flag & LIMIT_VALUE)
          i++;
        if (readbackLimits->flag & LIMIT_FILE)
          i++;
        if (readbackLimits->flag & LIMIT_MINMAXVALUE)
          i++;
        if (i > 1) {
          fprintf(stderr, "bad -readbackLimits syntax/value.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        break;
      case CLO_ACTIONLIMIT:
        if ((s_arg[i_arg].n_items -= 1) < 0 ||
            !scanItemList(&action->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "value", SDDS_DOUBLE, &action->singleValue, 1, LIMIT_VALUE, "file", SDDS_STRING, &action->file, 1, LIMIT_FILE, NULL) ||
            (action->flag & LIMIT_VALUE && action->flag & LIMIT_FILE) || (!(action->flag & LIMIT_VALUE) && !(action->flag & LIMIT_FILE))) {
          fprintf(stderr, "bad -actionLimit syntax/value.\n");
          s_arg[i_arg].n_items += 1;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items += 1;
        break;
      case CLO_DESPIKE:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&despikeParam->flag, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "neighbors", SDDS_LONG, &despikeParam->neighbors, 1, 0,
                           "passes", SDDS_LONG, &despikeParam->passes, 1, 0,
                           "averageof", SDDS_LONG, &despikeParam->averageOf, 1, DESPIKE_AVERAGEOF,
                           "threshold", SDDS_DOUBLE, &despikeParam->threshold, 1, 0,
                           "startThreshold", SDDS_DOUBLE, &despikeParam->startThreshold, 1, DESPIKE_STARTTHRESHOLD,
                           "endThreshold", SDDS_DOUBLE, &despikeParam->endThreshold, 1, DESPIKE_ENDTHRESHOLD,
                           "stepsThreshold", SDDS_LONG, &despikeParam->stepsThreshold, 1, DESPIKE_STEPSTHRESHOLD,
                           "pvthreshold", SDDS_STRING, &despikeParam->thresholdPV, 1, 0,
                           "rampThresholdPV", SDDS_STRING, &despikeParam->rampThresholdPV, 1, DESPIKE_THRESHOLD_RAMP_PV,
                           "file", SDDS_STRING, &despikeParam->file, 1, 0, "countLimit", SDDS_LONG, &despikeParam->countLimit, 1, 0, NULL) ||
             despikeParam->neighbors < 2 || despikeParam->passes < 1 || despikeParam->averageOf < 2)) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items++;
        if (despikeParam->threshold < 0) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (despikeParam->flag & DESPIKE_STARTTHRESHOLD || despikeParam->flag & DESPIKE_ENDTHRESHOLD || despikeParam->flag & DESPIKE_STEPSTHRESHOLD) {
          if (!(despikeParam->flag & DESPIKE_STARTTHRESHOLD) || !(despikeParam->flag & DESPIKE_ENDTHRESHOLD) || !(despikeParam->flag & DESPIKE_STEPSTHRESHOLD)) {
            fprintf(stderr, "invalid -despike threshold ramp syntax/values, startThreshold, endThreshold and stepsThreshold should all be provided.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
          if (despikeParam->startThreshold < 0 || despikeParam->endThreshold < 0 || despikeParam->endThreshold > despikeParam->startThreshold || despikeParam->stepsThreshold < 2) {
            fprintf(stderr, "invalid -despike threshold ramp syntax/values, the value of startThreshold or endThreshold or stepsThreshold is not valid, they all should be positive and the endThreshold should be smaller than the startThreshold.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
          despikeParam->deltaThreshold = (despikeParam->endThreshold - despikeParam->startThreshold) / (despikeParam->stepsThreshold - 1.0);
          despikeParam->threshold = despikeParam->startThreshold - despikeParam->deltaThreshold;
        }
        if (despikeParam->flag & DESPIKE_THRESHOLD_RAMP_PV && !(despikeParam->flag & DESPIKE_STEPSTHRESHOLD)) {
          if (*verbose)
            fprintf(stderr, "rampThreasholdPV is ignored since threshold ramping is not specified.\n");

          free(despikeParam->rampThresholdPV);
          despikeParam->rampThresholdPV = NULL;
        }

        if (!(despikeParam->flag & DESPIKE_AVERAGEOF))
          despikeParam->averageOf = despikeParam->neighbors;
        if (despikeParam->averageOf > despikeParam->neighbors) {
          fprintf(stderr, "invalid -despike syntax/values.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (despikeParam->thresholdPV) {
          if (*verbose)
            fprintf(stderr, "Set up connections for despike threshold PV.\n");
          if ((despikeParam->thresholdPVInfo.count = malloc(sizeof(long))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SetupRawCAConnection(&despikeParam->thresholdPV, &despikeParam->thresholdPVInfo, 1, *pendIOTime);
          readPVs(&despikeParam->thresholdPV, &despikeParam->threshold, &despikeParam->thresholdPVInfo, 1, NULL, *pendIOTime);
        }
        if (despikeParam->rampThresholdPV) {
          if (*verbose)
            fprintf(stderr, "Set up connections for despike threshold Ramp PV %s.\n", despikeParam->rampThresholdPV);
          if ((despikeParam->rampThresholdPVInfo.count = malloc(sizeof(long))) == NULL) {
            fprintf(stderr, "memory allocation failure\n");
            FreeEverything();
            exit(1);
          }
          SetupRawCAConnection(&despikeParam->rampThresholdPV, &despikeParam->rampThresholdPVInfo, 1, *pendIOTime);
          setEnumPV(despikeParam->rampThresholdPV, despikeParam->reramp, despikeParam->rampThresholdPVInfo, 0);
        }
        break;
      case CLO_GLITCHLOG:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "file", SDDS_STRING, &glitchParam->file, 1, 0,
                           "readbackRmsThreshold", SDDS_DOUBLE, &glitchParam->readbackRMSthreshold, 1, 0,
                           "controlRmsThreshold", SDDS_DOUBLE, &glitchParam->controlRMSthreshold, 1, 0, "rows", SDDS_LONG, &glitchParam->rows, 1, 0, NULL) ||
             glitchParam->readbackRMSthreshold < 0 || glitchParam->controlRMSthreshold < 0 || glitchParam->rows < 1)) {
          fprintf(stderr, "invalid -glitchLogFile syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        s_arg[i_arg].n_items++;
        break;
      case CLO_OVERLAP_COMPENSATION:
        s_arg[i_arg].n_items--;
        if (s_arg[i_arg].n_items > 0 &&
            (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                           "matrixfile", SDDS_STRING, &overlapCompensation->file, 1, 0,
                           "controlquantitydefinition", SDDS_STRING, &overlapCompensation->definitionFile, 1, 0,
                           "coeffile", SDDS_STRING, &overlapCompensation->coefFile, 1, 0, "controllogfile", SDDS_STRING, compensationOutputFile, 1, 0, "gain", SDDS_DOUBLE, &loopParam->compensationGain, 1, 0, "mode", SDDS_STRING, &mode, 1, 0, NULL))) {
          fprintf(stderr, "invalid -auxiliaryOutput syntax/values.\n");
          s_arg[i_arg].n_items++;
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if (mode) {
          switch (match_string(mode, mode_option, 2, 0)) {
          case 0:
            loopParam->compensationIntegral = 1;
            free(mode);
            break;
          case 1:
            loopParam->compensationIntegral = 0;
            free(mode);
            break;
          default:
            fprintf(stderr, "invalid mode given for -auxiliaryOutput syntax/values.\n");
            s_arg[i_arg].n_items++;
            free_scanargs(&s_arg, *argc);
            free(mode);
            return (1);
          }
        }
        s_arg[i_arg].n_items++;
        break;
      case CLO_LAUNCHERPV:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -messagePV syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->launcherPV[0] = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[0], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[0], ".MSG");

        if ((loopParam->launcherPV[1] = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[1], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[1], ".ALRM");

        if ((loopParam->launcherPV[2] = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[2], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[2], ".WAIT");

        if ((loopParam->launcherPV[3] = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[3], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[3], ".VAL");

        if ((loopParam->launcherPV[4] = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 5)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->launcherPV[4], s_arg[i_arg].list[1]);
        strcat(loopParam->launcherPV[4], ".CMND");

        if (*verbose)
          fprintf(stderr, "Set up connections for message PV.\n");
        if ((loopParam->launcherPVInfo[0].count = malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[1].count = malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[2].count = malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[3].count = malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((loopParam->launcherPVInfo[4].count = malloc(sizeof(long))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        SetupRawCAConnection(&loopParam->launcherPV[0], &loopParam->launcherPVInfo[0], 1, *pendIOTime);
        SetupRawCAConnection(&loopParam->launcherPV[1], &loopParam->launcherPVInfo[1], 1, *pendIOTime);
        SetupRawCAConnection(&loopParam->launcherPV[2], &loopParam->launcherPVInfo[2], 1, *pendIOTime);
        SetupRawCAConnection(&loopParam->launcherPV[3], &loopParam->launcherPVInfo[3], 1, *pendIOTime);
        SetupRawCAConnection(&loopParam->launcherPV[4], &loopParam->launcherPVInfo[4], 1, *pendIOTime);
        break;
      case CLO_POST_CHANGE_EXECUTION:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -postChangeExecution syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->postChangeExec = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->postChangeExec, s_arg[i_arg].list[1]);
        break;
      case CLO_FILTERFILE:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -filterFilter syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((correction->coefFile = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1 + 4)) == NULL) {
          fprintf(stderr, "memory allocation failure for filter file\n");
          FreeEverything();
          exit(1);
        }
        strcpy(correction->coefFile, s_arg[i_arg].list[1]);
        break;
      case CLO_TRIGGERPV:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -triggerPV syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->trigger.PV = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1)) == NULL) {
          fprintf(stderr, "memory allocation failure for trigger PV\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->trigger.PV, s_arg[i_arg].list[1]);
        loopParam->triggerProvided = 1;
        setupDatastrobeTriggerCallbacks(&(loopParam->trigger));
        break;
      case CLO_ENDOFLOOPPV:
        if (s_arg[i_arg].n_items != 2) {
          fprintf(stderr, "bad -endOfLoopPV syntax\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
        if ((loopParam->endOfLoopPV = SDDS_Malloc(sizeof(char) * strlen(s_arg[i_arg].list[1]) + 1)) == NULL) {
          fprintf(stderr, "memory allocation failure for trigger PV\n");
          FreeEverything();
          exit(1);
        }
        strcpy(loopParam->endOfLoopPV, s_arg[i_arg].list[1]);
        if (*verbose)
          fprintf(stderr, "Set up connections for end-of-loop PV %s.\n", loopParam->endOfLoopPV);
        if ((loopParam->endOfLoopPVInfo.count = malloc(sizeof(long))) == NULL) {
              fprintf(stderr, "memory allocation failure\n");
              FreeEverything();
              exit(1);
            }
        SetupRawCAConnection(&loopParam->endOfLoopPV, &loopParam->endOfLoopPVInfo, 1, *pendIOTime);
        break;
      default:
        fprintf(stderr, "Unrecognized option %s given.\n", s_arg[i_arg].list[0]);
        free_scanargs(&s_arg, *argc);
        return (1);
      }
    } else {
      if (firstTime) {
        if (!correction->file) {
          if (!SDDS_CopyString(&correction->file, s_arg[i_arg].list[0])) {
            fprintf(stderr, "Problem reading input file.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        } else if (!*outputFile) {
          if (!SDDS_CopyString(outputFile, s_arg[i_arg].list[0])) {
            fprintf(stderr, "Problem reading output file.\n");
            free_scanargs(&s_arg, *argc);
            return (1);
          }
        } else {
          fprintf(stderr, "Too many filenames given: %s.\n", s_arg[i_arg].list[0]);
          free_scanargs(&s_arg, *argc);
          return (1);
        }
      } else {
        if (!SDDS_CopyString(&correction->file, s_arg[i_arg].list[0])) {
          fprintf(stderr, "Problem reading input file.\n");
          free_scanargs(&s_arg, *argc);
          return (1);
        }
      }
    }
  }

  if (loopParam->holdPresentValues && loopParam->offsetFile) {
    fprintf(stderr, "Can't hold present values and use offset file.\n");
    free_scanargs(&s_arg, *argc);
    return (1);
  }
  if (infinite)
    loopParam->steps = INT_MAX;
  free_scanargs(&s_arg, *argc);
  return (0);
}

void initializeStatsData(STATS *stats) {
  stats->RMS = stats->ave = stats->MAD = stats->largest = 0;
  stats->largestName = NULL;
  return;
}

/* initializes contents of data structures */
void initializeData(CORRECTION *correction,
                    LOOP_PARAM *loopParam,
                    AVERAGE_PARAM *aveParam,
                    LIMITS *delta,
                    LIMITS *readbackLimits,
                    LIMITS *action,
                    DESPIKE_PARAM *despikeParam,
                    GLITCH_PARAM *glitchParam,
                    TESTS *test,
                    GENERATIONS *generations,
                    CORRECTION *overlapCompensation,
                    WAVE_FORMS *readbackWaveforms,
                    WAVE_FORMS *controlWaveforms,
                    WAVE_FORMS *offsetWaveforms,
                    WAVE_FORMS *ffSetpointWaveforms,
                    WAVEFORM_TESTS *waveform_tests) {
  CONTROL_NAME *control, *controlComp, *readback, *readbackComp;

  if ((readback = calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readback->symbolicName = readback->controlName = NULL;
  readback->value = NULL;
  readback->initial = NULL;
  readback->setpoint = readback->error = NULL;
  readback->old = NULL;
  readback->delta = NULL;
  readback->despike = NULL;
  readback->file = NULL;
  readback->channelInfo = NULL;
  readback->waveformMatchFound = NULL;
  readback->waveformIndex = NULL;
  readback->valueIndexes = 0;
  correction->readback = readback;

  if ((control = (CONTROL_NAME *)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  control->channelInfo = NULL;
  control->symbolicName = control->controlName = NULL;
  control->value = NULL;
  control->initial = NULL;
  control->setpoint = control->error = NULL;
  control->old = NULL;
  control->delta = NULL;
  control->setFlag = NULL;
  control->despike = NULL;
  control->file = NULL;
  control->waveformMatchFound = NULL;
  control->waveformIndex = NULL;
  control->historyFiltered = NULL;
  control->history = NULL;
  control->valueIndexes = 0;
  correction->aCoef = NULL;
  correction->bCoef = NULL;
  correction->K = NULL;
  correction->control = control;
  correction->coefFile = NULL;

  if ((readbackComp = calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  readbackComp->symbolicName = readbackComp->controlName = NULL;
  readbackComp->value = NULL;
  readbackComp->initial = NULL;
  readbackComp->setpoint = readbackComp->error = NULL;
  readbackComp->old = NULL;
  readbackComp->delta = NULL;
  readbackComp->despike = NULL;
  readbackComp->file = NULL;
  readbackComp->channelInfo = NULL;
  readbackComp->waveformMatchFound = NULL;
  readbackComp->waveformIndex = NULL;
  readbackComp->valueIndexes = 0;
  overlapCompensation->file = overlapCompensation->definitionFile = NULL;
  overlapCompensation->readback = readbackComp;
  overlapCompensation->actuator = NULL;

  if ((controlComp = (CONTROL_NAME *)calloc(1, sizeof(CONTROL_NAME))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  controlComp->symbolicName = controlComp->controlName = NULL;
  controlComp->value = NULL;
  controlComp->initial = NULL;
  controlComp->setpoint = controlComp->error = NULL;
  controlComp->old = NULL;
  controlComp->delta = NULL;
  controlComp->despike = NULL;
  controlComp->file = NULL;
  controlComp->setFlag = NULL;
  controlComp->waveformMatchFound = NULL;
  controlComp->waveformIndex = NULL;
  controlComp->channelInfo = NULL;
  if ((controlComp->history = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((controlComp->historyFiltered = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  controlComp->valueIndexes = 0;
  overlapCompensation->control = controlComp;
  overlapCompensation->K = NULL;

  if ((correction->K = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  correction->K->m = 0;
  correction->K->a = NULL;
  /* if ((correction->aCoef = (MATRIX *) calloc(1, sizeof(MATRIX))) == NULL) {
     fprintf(stderr, "memory allocation failure\n");
     FreeEverything();
     exit(1);
     }
     if ((correction->bCoef = (MATRIX *) calloc(1, sizeof(MATRIX))) == NULL) {
     fprintf(stderr, "memory allocation failure\n");
     FreeEverything();
     exit(1);
     } */
  correction->file = NULL;
  correction->coefFile = correction->definitionFile = NULL;
  correction->actuator = NULL;
  if ((overlapCompensation->K = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  overlapCompensation->K->m = 0;
  overlapCompensation->K->a = NULL;
  if ((overlapCompensation->aCoef = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((overlapCompensation->bCoef = (MATRIX *)calloc(1, sizeof(MATRIX))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  overlapCompensation->file = NULL;
  overlapCompensation->coefFile = NULL;
  overlapCompensation->actuator = NULL;
  loopParam->steps = DEFAULT_STEPS;
  loopParam->integral = 1;
  loopParam->compensationIntegral = 1;
  loopParam->holdPresentValues = 0;
  loopParam->offsetFile = NULL;
  loopParam->searchPath = NULL;
  loopParam->offsetPVFile = NULL;
  loopParam->offsetPV = NULL;
  loopParam->channelInfo = NULL;
  loopParam->offsetPVvalue = NULL;
  loopParam->n = 0;
  loopParam->dryRun = 0;
  loopParam->epochTime = NULL;
  loopParam->elapsedTime = NULL;
  loopParam->step = NULL;
  loopParam->postChangeExec = NULL;
  loopParam->triggerProvided = 0;
  /* loopParam->dryRun =1; temporarily, for safe reason */

  loopParam->updateInterval = DEFAULT_UPDATE_INTERVAL;
  loopParam->gain = DEFAULT_GAIN;
  loopParam->compensationGain = DEFAULT_COMPENSATION_GAIN;
  loopParam->gainPV = NULL;
  loopParam->intervalPV = NULL;
  loopParam->averagePV = NULL;
  loopParam->launcherPV[0] = NULL;
  loopParam->launcherPV[1] = NULL;
  loopParam->launcherPV[2] = NULL;
  loopParam->launcherPV[3] = NULL;
  loopParam->launcherPV[4] = NULL;
  loopParam->endOfLoopPV = NULL;
  loopParam->interval = DEFAULT_TIME_INTERVAL;
  loopParam->briefStatistics = 0;
  aveParam->n = 1;
  aveParam->interval = DEFAULT_AVEINTERVAL;
  delta->flag = 0UL;
  delta->file = NULL;
  delta->singleValue = 0;
  delta->n = 0;
  delta->controlName = NULL;
  delta->value = NULL;
  readbackLimits->flag = 0UL;
  readbackLimits->file = NULL;
  readbackLimits->singleValue = 0;
  readbackLimits->singleMinValue = 0;
  readbackLimits->singleMaxValue = 0;
  readbackLimits->n = 0;
  readbackLimits->controlName = NULL;
  readbackLimits->minValue = NULL;
  readbackLimits->maxValue = NULL;
  action->flag = 0UL;
  action->file = NULL;
  action->singleValue = 0;
  action->n = 0;
  action->controlName = NULL;
  action->value = NULL;
  despikeParam->neighbors = 4;
  despikeParam->averageOf = 2;
  despikeParam->reramp = 0;
  despikeParam->passes = 1;
  despikeParam->threshold = 0;
  despikeParam->thresholdPV = NULL;
  despikeParam->rampThresholdPV = NULL;
  despikeParam->file = NULL;
  despikeParam->flag = 0;
  despikeParam->countLimit = 0;
  despikeParam->startThreshold = 0;
  despikeParam->endThreshold = 0;
  despikeParam->stepsThreshold = 0;
  despikeParam->deltaThreshold = 0.0;

  test->file = NULL;
  test->n = 0;
  test->value = test->min = test->sleep = test->reset = test->holdOff = NULL;
  test->despikeValues = 0;
  test->despike = test->despikeIndex = NULL;
  test->writeToFile = NULL;
  test->controlName = NULL;
  test->channelInfo = NULL;
  test->longestSleep = 1;
  test->longestReset = 0;
  test->longestHoldOff = 1;
  test->glitchLog = NULL;
  test->writeToGlitchLogger = 0;

  glitchParam->file = NULL;
  glitchParam->readbackRMSthreshold = 0;
  glitchParam->controlRMSthreshold = 0;
  glitchParam->rows = 1;
  glitchParam->avail_rows = 0;
  glitchParam->row_pointer = 0;
  glitchParam->glitched = 0;
  /* global variables are done here too. */
#ifdef USE_RUNCONTROL
  sddscontrollawGlobal->rcParam.PV = sddscontrollawGlobal->rcParam.Desc = sddscontrollawGlobal->rcParam.PVparam = sddscontrollawGlobal->rcParam.DescParam = NULL;

  /* pingInterval should be short enough so
     that operators get a response from an abort command */
  sddscontrollawGlobal->rcParam.pingInterval = 2;
  sddscontrollawGlobal->rcParam.pingTimeout = 10;
  sddscontrollawGlobal->rcParam.alarmSeverity = NO_ALARM;
  sddscontrollawGlobal->rcParam.init = 0;
#endif
  generations->doGenerations = 0;
  generations->daily = 0;
  generations->digits = DEFAULT_GENERATIONS_DIGITS;
  generations->rowLimit = 0;
  generations->delimiter = ".";
  generations->timeLimit = 0;

  initializeWaveforms(readbackWaveforms);
  initializeWaveforms(controlWaveforms);
  initializeWaveforms(offsetWaveforms);
  initializeWaveforms(ffSetpointWaveforms);
  initializeTestWaveforms(waveform_tests);
  correction->searchPath = delta->searchPath = readbackLimits->searchPath = action->searchPath =
    despikeParam->searchPath = test->searchPath = overlapCompensation->searchPath = readbackWaveforms->searchPath = controlWaveforms->searchPath = offsetWaveforms->searchPath = ffSetpointWaveforms->searchPath = waveform_tests->searchPath = glitchParam->searchPath = loopParam->searchPath = NULL;
  return;
}

void initializeTestWaveforms(WAVEFORM_TESTS *waveform_tests) {
  waveform_tests->waveformTests = waveform_tests->testFiles = 0;
  waveform_tests->DeviceName = NULL;
  waveform_tests->waveformPV = NULL;
  waveform_tests->testFile = NULL;
  waveform_tests->longestSleep = 1;
  waveform_tests->longestHoldOff = 1;
  waveform_tests->holdOffPresent = 0;
  waveform_tests->channelInfo = NULL;
  waveform_tests->waveformLength = waveform_tests->testFailed = NULL;
  waveform_tests->outOfRange = NULL;
  waveform_tests->index = NULL;
  waveform_tests->despikeValues = NULL;
  waveform_tests->despike = waveform_tests->despikeIndex = waveform_tests->writeToFile = NULL;
  waveform_tests->waveformData = waveform_tests->min = waveform_tests->max = waveform_tests->sleep = waveform_tests->holdOff = NULL;
  waveform_tests->waveformSleep = waveform_tests->waveformHoldOff = NULL;
  waveform_tests->ignore = NULL;
}

void initializeWaveforms(WAVE_FORMS *wave_forms) {
  wave_forms->waveformFile = NULL;
  wave_forms->waveforms = wave_forms->waveformFiles = 0;
  wave_forms->waveformPV = wave_forms->writeWaveformPV = NULL;
  wave_forms->waveformData = NULL;
  wave_forms->readbackIndex = NULL;
  wave_forms->waveformLength = NULL;
  wave_forms->readbackValue = NULL;
  wave_forms->channelInfo = NULL;
  wave_forms->writeChannelInfo = NULL;
}

void setupData(CORRECTION *correction,
               LIMITS *delta,
               LIMITS *readbackLimits,
               LIMITS *action,
               LOOP_PARAM *loopParam,
               DESPIKE_PARAM *despikeParam,
               TESTS *test, CORRECTION *overlapCompensation,
               WAVE_FORMS *readbackWaveforms,
               WAVE_FORMS *controlWaveforms,
               WAVE_FORMS *offsetWaveforms,
               WAVE_FORMS *ffSetpointWaveforms,
               WAVEFORM_TESTS *waveform_tests,
               GLITCH_PARAM *glitchParam,
               long verbose,
               double pendIOTime) {

  CONTROL_NAME *control, *controlComp = NULL, *readback, *readbackComp = NULL;
  long i;

  correction->searchPath = delta->searchPath = readbackLimits->searchPath = action->searchPath =
    despikeParam->searchPath = test->searchPath = overlapCompensation->searchPath = readbackWaveforms->searchPath = controlWaveforms->searchPath = offsetWaveforms->searchPath = ffSetpointWaveforms->searchPath = waveform_tests->searchPath = glitchParam->searchPath = loopParam->searchPath;

  setupInputFile(correction);
  setupReadbackLimitFile(readbackLimits);
  setupDeltaLimitFile(delta);
  setupActionLimitFile(action);
  setupCompensationFiles(overlapCompensation);

#ifdef USE_RUNCONTROL
  if (sddscontrollawGlobal->rcParam.pingTimeout == 0.0) {
    sddscontrollawGlobal->rcParam.pingTimeout = (float)(1000 * 2 * MAX(loopParam->interval, sddscontrollawGlobal->rcParam.pingInterval));
  }
#endif
  readback = correction->readback;
  control = correction->control;
  if (overlapCompensation->file) {
    readbackComp = overlapCompensation->readback;
    controlComp = overlapCompensation->control;
  }
  if (loopParam->step != NULL) {
    free(loopParam->step);
    free(loopParam->epochTime);
    free(loopParam->elapsedTime);
    for (i = 0; i < readback->valueIndexes; i++)
      if (readback->value[i] != NULL) {
        free(readback->value[i]);
        free(readback->delta[i]);
      }
    for (i = 0; i < control->valueIndexes; i++)
      if (control->value[i] != NULL) {
        free(control->value[i]);
        free(control->delta[i]);
      }
    for (i = 0; i < readbackComp->valueIndexes; i++)
      if (readbackComp->value[i] != NULL) {
        free(readbackComp->value[i]);
        free(readbackComp->delta[i]);
      }
    for (i = 0; i < controlComp->valueIndexes; i++)
      if (controlComp->value[i] != NULL) {
        free(controlComp->value[i]);
        free(controlComp->delta[i]);
      }
    free(readback->value);
    free(control->value);
    free(readbackComp->value);
    free(controlComp->value);
    free(readback->delta);
    free(control->delta);
    free(readbackComp->delta);
    free(controlComp->delta);
  }
  readback->valueIndexes = control->valueIndexes = glitchParam->rows;

  if ((loopParam->step = (long *)SDDS_Calloc(1, sizeof(long) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((loopParam->epochTime = (double *)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((loopParam->elapsedTime = (double *)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  if ((readback->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < glitchParam->rows; i++) {
    if ((readback->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if ((readback->initial = (double *)SDDS_Realloc(readback->initial, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->old = (double *)SDDS_Realloc(readback->old, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((readback->despike = (int32_t *)SDDS_Realloc(readback->despike, sizeof(double) * (readback->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }

  control->integral = loopParam->integral;
  if ((control->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((control->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < glitchParam->rows; i++) {
    if ((control->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (control->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((control->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (control->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if ((control->initial = (double *)SDDS_Realloc(control->initial, sizeof(double) * (control->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if ((control->old = (double *)SDDS_Realloc(control->old, sizeof(double) * (control->n))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (waveform_tests->testFiles)
    setupWaveformTests(waveform_tests, readback, control, loopParam->interval, verbose, pendIOTime);
  if (readbackWaveforms->waveformFiles)
    setupWaveforms(readbackWaveforms, readback, verbose, pendIOTime);
  if (offsetWaveforms->waveformFiles)
    setupWaveforms(offsetWaveforms, readback, verbose, pendIOTime);
  if (controlWaveforms->waveformFiles)
    setupWaveforms(controlWaveforms, control, verbose, pendIOTime);

  /* overlap compensation data */
  if (overlapCompensation->file) {
    readbackComp->valueIndexes = controlComp->valueIndexes = glitchParam->rows;
    if ((readbackComp->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < glitchParam->rows; i++) {
      if ((readbackComp->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readbackComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((readbackComp->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (readbackComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    if ((readbackComp->initial = (double *)SDDS_Realloc(readbackComp->initial, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->old = (double *)SDDS_Realloc(readbackComp->old, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->setpoint = (double *)SDDS_Realloc(readbackComp->setpoint, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readbackComp->error = (double *)SDDS_Realloc(readbackComp->error, sizeof(double) * (readbackComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    readbackComp->despike = NULL;

    controlComp->integral = loopParam->compensationIntegral;
    /*now the controlComp is array of bpm Setpoints, the readbackComp is the same as control */
    if (ffSetpointWaveforms->waveformFiles)
      setupWaveforms(ffSetpointWaveforms, controlComp, verbose, pendIOTime);
    if ((controlComp->value = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->delta = (double **)SDDS_Calloc(1, sizeof(double) * glitchParam->rows)) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    for (i = 0; i < glitchParam->rows; i++) {
      if ((controlComp->value[i] = (double *)SDDS_Calloc(1, sizeof(double) * (controlComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((controlComp->delta[i] = (double *)SDDS_Calloc(1, sizeof(double) * (controlComp->n))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
    }
    if ((controlComp->initial = (double *)SDDS_Realloc(controlComp->initial, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->old = (double *)SDDS_Realloc(controlComp->old, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->setpoint = (double *)SDDS_Realloc(controlComp->setpoint, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((controlComp->error = (double *)SDDS_Realloc(controlComp->error, sizeof(double) * (controlComp->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  if (loopParam->holdPresentValues || loopParam->offsetFile || loopParam->offsetPVFile) {
    if ((readback->setpoint = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((readback->error = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
  }
  matchUpControlNames(delta, control->controlName, control->n);
  matchUpControlNames(readbackLimits, readback->controlName, readback->n);
  matchUpControlNames(action, readback->controlName, readback->n);
  if (loopParam->offsetFile) {
    readOffsetValues(readback->initial, readback->n, readback->controlName, loopParam->offsetFile, loopParam->searchPath);
    /* Can't use hold present values and offset file command line options together
         but set holdPresentValues flag here because the function is equivalent
       */
    loopParam->holdPresentValues = 1;
  }
  if (loopParam->offsetPVFile) {
    /*get the offset PV name and rearrange it according to the order of readbacks */
    if ((loopParam->offsetPV = (char **)malloc(sizeof(char *) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((loopParam->offsetPVvalue = (double *)malloc(sizeof(double) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    readOffsetPV(loopParam->offsetPV, readback->n, readback->controlName, loopParam->offsetPVFile, loopParam->searchPath);
    loopParam->n = readback->n;
    /*setup connection for offset PVs */
    if ((loopParam->channelInfo = malloc(sizeof(*loopParam->channelInfo) * (readback->n))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if ((loopParam->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    SetupRawCAConnection(loopParam->offsetPV, loopParam->channelInfo, readback->n, pendIOTime);
  }
  setupTestsFile(test, loopParam->interval, verbose, pendIOTime);
  setupDespikeFile(despikeParam, readback, verbose);

  return;
}

void logActuator(char *file, CONTROL_NAME *control, char *timeStamp) {
  SDDS_TABLE outputPage;

  if (file) {
    if (!SDDS_InitializeOutput(&outputPage, SDDS_BINARY, 1,
                               "Control variables",
                               "Control variables", file) ||
        (0 > SDDS_DefineParameter(&outputPage, "TimeStamp", "TimeStamp", NULL, "TimeStamp",
                                  NULL, SDDS_STRING, NULL)) ||
        (0 > SDDS_DefineColumn(&outputPage, "ControlName", NULL,
                               NULL, "Control name", NULL, SDDS_STRING, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Value", NULL,
                               NULL, "Actual setpoint sent", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Old", NULL, NULL, "Previous setpoint sent", NULL, SDDS_DOUBLE, 0)) ||
        (0 > SDDS_DefineColumn(&outputPage, "Delta", NULL, NULL, "Change in setpoint", NULL, SDDS_DOUBLE, 0))) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    SDDS_DisableFSync(&outputPage);
    if (!SDDS_WriteLayout(&outputPage) || !SDDS_StartTable(&outputPage, control->n)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_SetParameters(&outputPage, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "TimeStamp", timeStamp, NULL) ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->controlName,
                        control->n, "ControlName") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->value[0], control->n, "Value") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->old, control->n, "Old") ||
        !SDDS_SetColumn(&outputPage, SDDS_BY_NAME, control->delta[0], control->n, "Delta")) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_UpdatePage(&outputPage, FLUSH_TABLE)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
    if (!SDDS_Terminate(&outputPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  return;
}

#define CL_BUFFER_SIZE 16384
long ReadMoreArguments(char ***argv, int *argc, char **commandlineArgv, int commandlineArgc, FILE *fp) {
  long i, l;
  char buffer[CL_BUFFER_SIZE];

  if (!fgets(buffer, CL_BUFFER_SIZE, fp) || SDDS_StringIsBlank(buffer))
    return 0;
  buffer[strlen(buffer) - 1] = 0;

  *argc = countArguments(buffer) + commandlineArgc;
  *argv = tmalloc((*argc + 1) * sizeof(**argv));
  for (i = 0; i < commandlineArgc; i++)
    SDDS_CopyString((*argv) + i, commandlineArgv[i]);

  while (((*argv)[i] = get_token_t(buffer, " "))) {
    l = strlen((*argv)[i]);
    if ((*argv)[i][0] == '"' && (*argv)[i][l - 1] == '"') {
      strslide((*argv)[i], -1);
      (*argv)[i][l - 2] = 0;
    }
    i++;
  }

  if (i != *argc) {
    FreeEverything();
    SDDS_Bomb("argument count problem in ReadMoreArguments");
  }
  return 1;
}

int countArguments(char *s) {
  int count;
  char *ptr, *copy;

  count = 0;
  cp_str(&copy, s);

  while ((ptr = get_token_t(copy, " "))) {
    count++;
    free(ptr);
  }
  free(copy);
  return (count);
}

void readOffsetValues(double *offset, long n, char **controlName, char *offsetFile, char *searchPath) {
  SDDS_DATASET SDDSin;
  char **inputName;
  double *inputOffset = 0;
  long rows = 0, i, j, found;

  inputName = NULL;
  if (searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&SDDSin, offsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&SDDSin, offsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  if (SDDS_ReadPage(&SDDSin) < 0 || (rows = SDDS_CountRowsOfInterest(&SDDSin)) <= 0 ||
      !(inputName = SDDS_GetInternalColumn(&SDDSin, "ControlName")) ||
      !(inputOffset = SDDS_GetInternalColumn(&SDDSin, "Offset"))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!controlName) {
    FreeEverything();
    SDDS_Bomb("problem with control name data (readOffsetValues)");
  }
  for (i = 0; i < n; i++) {
    for (j = found = 0; j < rows; j++) {
      if (strcmp(inputName[j], controlName[i]) == 0) {
        offset[i] = inputOffset[j];
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Error: no offset found for readback %s in file %s\n", controlName[i], offsetFile);
      FreeEverything();
      exit(1);
    }
  }
  /*
     for (i=0;i<rows;i++) 
     free(inputName[i]);
     free(inputName);
     free(inputOffset);
   */
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
}

void readOffsetPV(char **offsetPVs, long n, char **controlName, char *PVOffsetFile, char *searchPath) {
  SDDS_DATASET SDDSin;
  char **inputName, **inputOffsetPV = NULL;

  long rows = 0, i, j, found;

  inputName = NULL;
  if (searchPath) {
    if (!SDDS_InitializeInputFromSearchPath(&SDDSin, PVOffsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  } else {
    if (!SDDS_InitializeInput(&SDDSin, PVOffsetFile)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }

  if (SDDS_ReadPage(&SDDSin) < 0 || (rows = SDDS_CountRowsOfInterest(&SDDSin)) <= 0 ||
      !(inputOffsetPV = SDDS_GetInternalColumn(&SDDSin, "OffsetControlName")) ||
      !(inputName = SDDS_GetInternalColumn(&SDDSin, "ControlName"))) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
  if (!controlName) {
    FreeEverything();
    SDDS_Bomb("problem with control name data (readPVOffsets)");
  }
  for (i = 0; i < n; i++) {
    for (j = found = 0; j < rows; j++) {
      if (strcmp(inputName[j], controlName[i]) == 0) {
        if ((offsetPVs[i] = malloc(sizeof(char) * (strlen(inputOffsetPV[j]) + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        strcpy(offsetPVs[i], inputOffsetPV[j]);
        found = 1;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "Error: no offset PV found for readback %s in file %s\n", controlName[i], PVOffsetFile);
      FreeEverything();
      exit(1);
    }
  }
  /*
     for (i=0;i<rows;i++) {
     free(inputName[i]);
     free(inputOffsetPV[i]);
     }
     free(inputName);
     free(inputOffsetPV);
   */
  if (!SDDS_Terminate(&SDDSin)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    FreeEverything();
    exit(1);
  }
}

void setupWaveforms(WAVE_FORMS *wave_forms, CONTROL_NAME *CONTROL, long verbose, double pendIOTime) {
  long i, j, n, k, m, unmatched, samePV = 0;
  char **DeviceNames, **readback, **controlName;
  int32_t *index;
  SDDS_DATASET dataset;
  int32_t rows;

  rows = 0;
  DeviceNames = readback = NULL;
  index = NULL;
  if (!wave_forms->waveformFiles)
    return;
  readback = CONTROL->symbolicName;
  controlName = CONTROL->controlName;

  n = CONTROL->n;
  if ((wave_forms->readbackValue = calloc(n, sizeof(*wave_forms->readbackValue))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < wave_forms->waveformFiles; i++) {
    if (wave_forms->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&dataset, wave_forms->waveformFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&dataset, wave_forms->waveformFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    switch (SDDS_CheckColumn(&dataset, "DeviceName", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"DevicName\" column in file %s.\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "Index", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"Index\" column in file %s.\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }

    /*if only WaveformPV is given, then it is used for both reading and writing, otherwise
         two parameters "ReadbackWaveformPV" and "WriteWaveformPV" should be given. */
    switch (SDDS_CheckParameter(&dataset, "WaveformPV", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      samePV = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      if (SDDS_CheckParameter(&dataset, "WriteWaveformPV", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK ||
          SDDS_CheckParameter(&dataset, "ReadWaveformPV", NULL, SDDS_STRING, NULL) != SDDS_CHECK_OK) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      samePV = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with parameter \"WaveformPV\" in file %s\n", wave_forms->waveformFile[i]);
      FreeEverything();
      exit(1);
    }

    while (SDDS_ReadPage(&dataset) > 0) {
      m = wave_forms->waveforms;
      if ((wave_forms->waveformPV = SDDS_Realloc(wave_forms->waveformPV, sizeof(*wave_forms->waveformPV) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->channelInfo = SDDS_Realloc(wave_forms->channelInfo, sizeof(*wave_forms->channelInfo) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->readbackIndex = SDDS_Realloc(wave_forms->readbackIndex, sizeof(*wave_forms->readbackIndex) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformData = SDDS_Realloc(wave_forms->waveformData, sizeof(*wave_forms->waveformData) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformLength = SDDS_Realloc(wave_forms->waveformLength, sizeof(*wave_forms->waveformLength) * (m + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if (samePV) {
        if (!SDDS_GetParameter(&dataset, "WaveformPV", &wave_forms->waveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      } else {
        if ((wave_forms->writeWaveformPV = SDDS_Realloc(wave_forms->writeWaveformPV, sizeof(*wave_forms->writeWaveformPV) * (m + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((wave_forms->writeChannelInfo = SDDS_Realloc(wave_forms->writeChannelInfo, sizeof(*wave_forms->writeChannelInfo) * (m + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if (!SDDS_GetParameter(&dataset, "WriteWaveformPV", &wave_forms->writeWaveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
        if (!SDDS_GetParameter(&dataset, "ReadWaveformPV", &wave_forms->waveformPV[m])) {
          SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
          FreeEverything();
          exit(1);
        }
      }
      if (!(rows = SDDS_CountRowsOfInterest(&dataset))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      wave_forms->waveformLength[m] = rows;
      if ((wave_forms->readbackIndex[m] = malloc(sizeof(long) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((wave_forms->waveformData[m] = malloc(sizeof(double) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      DeviceNames = (char **)SDDS_GetColumn(&dataset, "DeviceName");
      index = SDDS_GetColumnInLong(&dataset, "Index");
      wave_forms->channelInfo[m].waveformLength = rows;
      wave_forms->channelInfo[m].waveformData = wave_forms->waveformData[m];
      if (!samePV) {
        wave_forms->writeChannelInfo[m].waveformLength = rows;
        wave_forms->writeChannelInfo[m].waveformData = wave_forms->waveformData[m];
      }

      /* sort the bpms by the index in waveform */
      SortBPMsByIndex(&DeviceNames, index, rows);
      for (j = 0; j < rows; j++) {
        k = match_string(DeviceNames[j], readback, n, EXACT_MATCH);
        if (k < 0)
          k = match_string(DeviceNames[j], controlName, n, UNIQUE_MATCH);
        wave_forms->readbackIndex[m][j] = k;
        if (k >= 0)
          CONTROL->waveformMatchFound[k] = 1;
        free(DeviceNames[j]);
      }
      free(DeviceNames);
      free(index);
      wave_forms->waveforms++;
    }
    if (!SDDS_Terminate(&dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  if ((wave_forms->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for reading waveform.\n");
  SetupRawCAConnection(wave_forms->waveformPV, wave_forms->channelInfo, wave_forms->waveforms, pendIOTime);
  if (samePV) {
    wave_forms->writeWaveformPV = wave_forms->waveformPV;
    wave_forms->writeChannelInfo = wave_forms->channelInfo;
  } else {
    if ((wave_forms->writeChannelInfo[0].count = malloc(sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    if (verbose)
      fprintf(stderr, "Set up connections for writinging waveform.\n");
    SetupRawCAConnection(wave_forms->writeWaveformPV, wave_forms->writeChannelInfo, wave_forms->waveforms, pendIOTime);
  }
  for (k = unmatched = 0; k < n; k++) {
    if (!CONTROL->waveformMatchFound[k]) {
      unmatched++;
      fprintf(stderr, "Error: column %s in the matrix has no match in the waveform files\n", readback[k]);
    }
  }
  if (unmatched) {
    FreeEverything();
    exit(1);
  }
}

long isSorted(int32_t *index, long n) {
  int i;
  if (!n)
    return 1;
  for (i = 0; i < n - 1; i++) {
    if (index[i + 1] < index[i])
      return 0;
  }
  return 1;
}

#if defined(linux) || defined(_WIN32) || defined(vxWorks) || defined(__APPLE__)
int Compare2Longs(const void *vindex1, const void *vindex2)
#else
long Compare2Longs(const void *vindex1, const void *vindex2)
#endif
{
  long i1, i2;

  i1 = *(long *)vindex1;
  i2 = *(long *)vindex2;
  if (sddscontrollawGlobal->sortIndex[i1] > sddscontrollawGlobal->sortIndex[i2]) {
    return 1;
  } else
    return 0;
}

/* sort the bpms by the index in waveform */
void SortBPMsByIndex(char ***PV, int32_t *index, long n) {
  long i, *sort;
  char **temp, **sortPV;
  sortPV = *PV;

  if (isSorted(index, n))
    return;
  sddscontrollawGlobal->sortIndex = index;
  if ((sort = malloc(sizeof(*sort) * n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  for (i = 0; i < n; i++)
    sort[i] = i;
  if ((temp = malloc(sizeof(*temp) * n)) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  qsort((void *)sort, n, sizeof(*sort), Compare2Longs);
  for (i = 0; i < n; i++) {
    SDDS_CopyString(&temp[i], sortPV[i]);
  }
  for (i = 0; i < n; i++) {
    free(sortPV[i]);
    SDDS_CopyString(&sortPV[i], temp[sort[i]]);
  }
  for (i = 0; i < n; i++)
    free(temp[i]);
  free(temp);
  free(sort);
}

long ReadWaveformData(WAVE_FORMS *wave_forms, double pendIOTime) {
  int32_t i, length, j, k, n;
  char **PVs;
  CHANNEL_INFO *channelInfo;

  length = 0;
  if (!wave_forms->waveforms)
    return 0;
  n = wave_forms->waveforms;
  PVs = wave_forms->waveformPV;
  channelInfo = wave_forms->channelInfo;
  *(channelInfo[0].count) = n;
  for (i = 0; i < n; i++) {
#if !defined(DBAccess)
    if (ca_state(channelInfo[i].channelID) != cs_conn) {
      fprintf(stderr, "Error, no connection for %s\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    if (ca_array_get(DBR_DOUBLE, channelInfo[i].waveformLength, channelInfo[i].channelID, channelInfo[i].waveformData) != ECA_NORMAL) {
#else
    if (dbGetField(&channelInfo[i].channelID, DBR_DOUBLE, channelInfo[i].waveformData, NULL, &channelInfo[i].waveformLength, NULL)) {
#endif
      fprintf(stderr, "error: unable to get value for %s.\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    channelInfo[i].flag = 0;
  }

#if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "pendIOerror: unable to get waveform PV values\n");
    FreeEverything();
    exit(1);
  }
#endif
  *(channelInfo[0].count) = 0;
  for (i = 0; i < n; i++) {
    wave_forms->waveformData[i] = channelInfo[i].waveformData;
    length = wave_forms->waveformLength[i];
    for (j = 0; j < length; j++) {
      k = wave_forms->readbackIndex[i][j];
      if (k >= 0)
        wave_forms->readbackValue[k] = wave_forms->waveformData[i][j];
    }
  }
  return 0;
}

void setupWaveformTests(WAVEFORM_TESTS *waveform_tests, CONTROL_NAME *readback, CONTROL_NAME *control, double interval, long verbose, double pendIOTime) {
  long i, n, k = 0, rows = 0, ignoreExist = 0, m, despikeColumnPresent = 0;
  SDDS_DATASET dataset;
  int32_t *index;
  int32_t *despike;
  short *ignore;
  char **DeviceName;
  double *max, *min, *sleep, *holdOff;
  long sleepTimeColumnPresent = 0, sleepIntervalColumnPresent = 0, row;
  long holdOffTimeColumnPresent = 0, holdOffIntervalColumnPresent = 0;

  DeviceName = NULL;
  max = min = sleep = holdOff = NULL;
  index = NULL;
  despike = NULL;

  if (!waveform_tests->testFiles)
    return;

  n = waveform_tests->testFiles;
  for (i = 0; i < n; i++) {
    if (waveform_tests->searchPath) {
      if (!SDDS_InitializeInputFromSearchPath(&dataset, waveform_tests->testFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    } else {
      if (!SDDS_InitializeInput(&dataset, waveform_tests->testFile[i])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
    }

    /*check the existence of required columns */
    switch (SDDS_CheckColumn(&dataset, "DeviceName", NULL, SDDS_STRING, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"DevicName\" column in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "Index", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with \"Index\" column in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "MinimumValue", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"MinimumValue\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "MaximumValue", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"MaximumValue\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "Ignore", NULL, SDDS_SHORT, NULL)) {
    case SDDS_CHECK_OKAY:
      ignoreExist = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      break;
    default:
      fprintf(stderr, "Something wrong with column \"Ignore\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "SleepTime", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      sleepTimeColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      sleepTimeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"SleepTime\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "SleepIntervals", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      sleepIntervalColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      sleepIntervalColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"SleepIntervals\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    if (sleepTimeColumnPresent && sleepIntervalColumnPresent) {
      fprintf(stderr, "SleepTime column and SleepIntervals can not exist in the same time in test file %s\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "HoldOffTime", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      holdOffTimeColumnPresent = 1;
      waveform_tests->holdOffPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      holdOffTimeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"HoldOffTime\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "HoldOffIntervals", NULL, SDDS_DOUBLE, NULL)) {
    case SDDS_CHECK_OKAY:
      holdOffIntervalColumnPresent = 1;
      waveform_tests->holdOffPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      holdOffIntervalColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with \"HoldOffIntervals\" in file %s.\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    if (holdOffTimeColumnPresent && holdOffIntervalColumnPresent) {
      fprintf(stderr, "HoldOffTime column and HoldOffIntervals can not exist in the same time in test file %s\n", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    switch (SDDS_CheckColumn(&dataset, "Despike", NULL, SDDS_LONG, NULL)) {
    case SDDS_CHECK_OKAY:
      despikeColumnPresent = 1;
      break;
    case SDDS_CHECK_NONEXISTENT:
      despikeColumnPresent = 0;
      break;
    default:
      fprintf(stderr, "Something wrong with column %s in file %s.\n", "Despike", waveform_tests->testFile[i]);
      FreeEverything();
      exit(1);
    }
    while (SDDS_ReadPage(&dataset) > 0) {
      k = waveform_tests->waveformTests;
      if ((waveform_tests->DeviceName = SDDS_Realloc(waveform_tests->DeviceName, sizeof(*waveform_tests->DeviceName) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformPV = SDDS_Realloc(waveform_tests->waveformPV, sizeof(*waveform_tests->waveformPV) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->channelInfo = SDDS_Realloc(waveform_tests->channelInfo, sizeof(*waveform_tests->channelInfo) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformLength = SDDS_Realloc(waveform_tests->waveformLength, sizeof(*waveform_tests->waveformLength) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->outOfRange = SDDS_Realloc(waveform_tests->outOfRange, sizeof(*waveform_tests->outOfRange) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->index = SDDS_Realloc(waveform_tests->index, sizeof(*waveform_tests->index) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->testFailed = SDDS_Realloc(waveform_tests->testFailed, sizeof(*waveform_tests->testFailed) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->waveformData = SDDS_Realloc(waveform_tests->waveformData, sizeof(*waveform_tests->waveformData) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->min = SDDS_Realloc(waveform_tests->min, sizeof(*waveform_tests->min) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->max = SDDS_Realloc(waveform_tests->max, sizeof(*waveform_tests->max) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->writeToFile = SDDS_Realloc(waveform_tests->writeToFile, sizeof(*waveform_tests->writeToFile) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->ignore = SDDS_Realloc(waveform_tests->ignore, sizeof(*waveform_tests->ignore) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if ((waveform_tests->despikeValues = SDDS_Realloc(waveform_tests->despikeValues, sizeof(*waveform_tests->despikeValues) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->despikeValues[k] = 0;
      if ((waveform_tests->waveformSleep = SDDS_Realloc(waveform_tests->waveformSleep, sizeof(*waveform_tests->waveformSleep) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformSleep[k] = 1;
      if ((waveform_tests->waveformHoldOff = SDDS_Realloc(waveform_tests->waveformHoldOff, sizeof(*waveform_tests->waveformHoldOff) * (k + 1))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformHoldOff[k] = 1;
      if (!SDDS_GetParameter(&dataset, "WaveformPV", &waveform_tests->waveformPV[k])) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      if (!(rows = SDDS_CountRowsOfInterest(&dataset))) {
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        FreeEverything();
        exit(1);
      }
      waveform_tests->waveformLength[k] = rows;
      waveform_tests->testFailed[k] = 0;
      if ((waveform_tests->writeToFile[k] = malloc(sizeof(**waveform_tests->writeToFile) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      if (despikeColumnPresent) {
        if ((waveform_tests->despikeIndex = SDDS_Realloc(waveform_tests->despikeIndex, sizeof(*waveform_tests->despikeIndex) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        if ((waveform_tests->despike = SDDS_Realloc(waveform_tests->despike, sizeof(*waveform_tests->despike) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        waveform_tests->despikeIndex[k] = NULL;
      }
      if (sleepTimeColumnPresent || sleepIntervalColumnPresent) {
        if ((waveform_tests->sleep = SDDS_Realloc(waveform_tests->sleep, sizeof(*waveform_tests->sleep) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
      }
      if (holdOffTimeColumnPresent || holdOffIntervalColumnPresent) {
        if ((waveform_tests->holdOff = SDDS_Realloc(waveform_tests->holdOff, sizeof(*waveform_tests->holdOff) * (k + 1))) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
      }
      if ((waveform_tests->waveformData[k] = malloc(sizeof(**waveform_tests->waveformData) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      waveform_tests->channelInfo[k].waveformLength = rows;
      waveform_tests->channelInfo[k].waveformData = waveform_tests->waveformData[k];

      DeviceName = (char **)SDDS_GetColumn(&dataset, "DeviceName");
      index = SDDS_GetColumnInLong(&dataset, "Index");
      if (ignoreExist)
        ignore = SDDS_GetColumn(&dataset, "Ignore");
      else {
        /*test value only contains readbacks and controls, ignore all others */
        if ((ignore = malloc(sizeof(*ignore) * rows)) == NULL) {
          fprintf(stderr, "memory allocation failure\n");
          FreeEverything();
          exit(1);
        }
        for (m = 0; m < rows; m++) {
          if (match_string(DeviceName[m], readback->symbolicName, readback->n, EXACT_MATCH) >= 0 ||
              match_string(DeviceName[m], control->symbolicName, control->n, EXACT_MATCH) >= 0)
            ignore[m] = 0;
          else
            ignore[m] = 1;
        }
      }
      min = SDDS_GetColumnInDoubles(&dataset, "MinimumValue");
      max = SDDS_GetColumnInDoubles(&dataset, "MaximumValue");
      if (sleepTimeColumnPresent)
        sleep = SDDS_GetColumnInDoubles(&dataset, "SleepTime");
      if (sleepIntervalColumnPresent) {
        sleep = SDDS_GetColumnInDoubles(&dataset, "SleepIntervals");
        for (row = 0; row < rows; row++)
          sleep[row] = sleep[row] * interval;
      }
      if (holdOffTimeColumnPresent)
        holdOff = SDDS_GetColumnInDoubles(&dataset, "HoldOffTime");
      if (holdOffIntervalColumnPresent) {
        holdOff = SDDS_GetColumnInDoubles(&dataset, "HoldOffIntervals");
        for (row = 0; row < rows; row++)
          holdOff[row] = holdOff[row] * interval;
      }
      if (despikeColumnPresent)
        despike = (int32_t *)SDDS_GetColumn(&dataset, "Despike");
      /*sort data by the index if it is not sorted */
      /*delete sorting the waveform index part -- we should make the waveform file is sorted by index */

      waveform_tests->index[k] = index;
      waveform_tests->DeviceName[k] = DeviceName;
      waveform_tests->max[k] = max;
      waveform_tests->min[k] = min;
      waveform_tests->ignore[k] = ignore;
      if (despikeColumnPresent)
        waveform_tests->despike[k] = despike;
      if (sleepTimeColumnPresent || sleepIntervalColumnPresent)
        waveform_tests->sleep[k] = sleep;
      if (holdOffTimeColumnPresent || holdOffIntervalColumnPresent)
        waveform_tests->holdOff[k] = holdOff;
    }
    if ((waveform_tests->outOfRange[k] = calloc(rows, sizeof(long))) == NULL) {
      fprintf(stderr, "memory allocation failure\n");
      FreeEverything();
      exit(1);
    }
    waveform_tests->waveformTests++;

    if (despikeColumnPresent) {
      if ((waveform_tests->despikeIndex[k] = (int32_t *)malloc(sizeof(int32_t) * rows)) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (m = 0; m < rows; m++) {
        if (waveform_tests->despike[k][m]) {
          waveform_tests->despikeIndex[k][waveform_tests->despikeValues[k]] = m;
          waveform_tests->despikeValues[k]++;
        }
      }
    }
    if (!SDDS_Terminate(&dataset)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      FreeEverything();
      exit(1);
    }
  }
  if ((waveform_tests->channelInfo[0].count = malloc(sizeof(long))) == NULL) {
    fprintf(stderr, "memory allocation failure\n");
    FreeEverything();
    exit(1);
  }
  if (verbose)
    fprintf(stderr, "Set up connections for testing waveform.\n");
  SetupRawCAConnection(waveform_tests->waveformPV, waveform_tests->channelInfo, waveform_tests->waveformTests, pendIOTime);
  if (verbose) {
    fprintf(stderr, "number of waveform tests =%ld\n", waveform_tests->waveformTests);
    for (i = 0; i < waveform_tests->waveformTests; i++)
      fprintf(stderr, "  %ld\t\t%s\n", i, waveform_tests->waveformPV[i]);
  }
}

long CheckWaveformTest(WAVEFORM_TESTS *waveform_tests, BACKOFF *backoff, LOOP_PARAM *loopParam, DESPIKE_PARAM *despikeParam, long verbose, long warning, double pendIOTime) {
  long i, j, outOfRange, tests, k, spikesDone;
  double *despikeTestData, **value, **max, **min;
  int32_t *despikeValues, **despikeIndex, *length, **outRange;
  CHANNEL_INFO *channelInfo;

  outOfRange = 0;
  waveform_tests->longestSleep = 0;
  if (!waveform_tests->waveformTests)
    return 0;
  tests = waveform_tests->waveformTests;
  despikeValues = waveform_tests->despikeValues;
  value = waveform_tests->waveformData;
  max = waveform_tests->max;
  min = waveform_tests->min;
  despikeIndex = waveform_tests->despikeIndex;
  length = waveform_tests->waveformLength;
  outRange = waveform_tests->outOfRange;
  channelInfo = waveform_tests->channelInfo;

  if (verbose)
    fprintf(stderr, "Reading waveform test devices at %f seconds.\n", loopParam->elapsedTime[0]);
  /*read test waveform data */
  *(channelInfo[0].count) = tests;
  for (i = 0; i < tests; i++) {
#if !defined(DBAccess)
    if (ca_state(channelInfo[i].channelID) != cs_conn) {
      fprintf(stderr, "Error, no connection for %s\n", waveform_tests->waveformPV[i]);
      FreeEverything();
      exit(1);
    }
    if (ca_array_get(DBR_DOUBLE, channelInfo[i].waveformLength, channelInfo[i].channelID, channelInfo[i].waveformData) != ECA_NORMAL) {
#else
    if (dbGetField(&channelInfo[i].channelID, DBR_DOUBLE, channelInfo[i].waveformData, NULL, &channelInfo[i].waveformLength, NULL)) {
#endif
      fprintf(stderr, "error: unable to get waveform PV %s.\n", waveform_tests->waveformPV[i]);
      FreeEverything();
      exit(1);
    }
    channelInfo[i].flag = 0;
  }

#if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "pendIOerror: unable to get waveform PV values\n");
    FreeEverything();
    exit(1);
  }
#endif
  *(channelInfo[0].count) = 0;
  for (i = 0; i < tests; i++) {
    waveform_tests->waveformSleep[i] = 0;
    waveform_tests->testFailed[i] = 0;
    if ((despikeParam->threshold > 0) && despikeValues[i]) {
      if ((despikeTestData = (double *)malloc(sizeof(double) * (despikeValues[i]))) == NULL) {
        fprintf(stderr, "memory allocation failure\n");
        FreeEverything();
        exit(1);
      }
      for (k = 0; k < despikeValues[i]; k++)
        despikeTestData[k] = value[i][despikeIndex[i][k]];
      /* uses the same despike parameters as the readback values */
      if ((spikesDone = despikeData(despikeTestData, despikeValues[i], despikeParam->neighbors, despikeParam->passes, despikeParam->averageOf, despikeParam->threshold, despikeParam->countLimit)) != 0) {
        if (verbose) {
          fprintf(stderr, "%ld spikes from %s waveform test removed.\n", spikesDone, waveform_tests->waveformPV[i]);
        }
      }
      for (k = 0; k < despikeValues[i]; k++)
        value[i][despikeIndex[i][k]] = despikeTestData[k];
      free(despikeTestData);
    }
    for (j = 0; j < length[i]; j++) {
      if (waveform_tests->ignore[i][j]) {
        outRange[i][j] = 0;
        continue;
      }
      if (value[i][j] < min[i][j] || value[i][j] > max[i][j]) {
        outRange[i][j] = 1;
        waveform_tests->testFailed[i] = 1;
        outOfRange = 1;
        if (waveform_tests->sleep)
          waveform_tests->waveformSleep[i] = MAX(waveform_tests->waveformSleep[i], waveform_tests->sleep[i][j]);
        if (waveform_tests->holdOff)
          waveform_tests->waveformHoldOff[i] = MAX(waveform_tests->waveformHoldOff[i], waveform_tests->holdOff[i][j]);
      } else
        outRange[i][j] = 0;
    }
    waveform_tests->longestSleep = (long)(MAX(waveform_tests->longestSleep, waveform_tests->waveformSleep[i]));
    waveform_tests->longestHoldOff = (long)(MAX(waveform_tests->longestHoldOff, waveform_tests->waveformHoldOff[i]));
  }
  if (outOfRange) {
    if (verbose || warning) {
      backoff->counter++;
      if (backoff->counter >= backoff->level) {
        backoff->counter = 0;
        if (backoff->level < 8)
          backoff->level *= 2;
      }
      if (verbose) {
        fprintf(stderr, "Waveform tests out of range:\n");
        for (i = 0; i < tests; i++) {
          if (waveform_tests->testFailed[i])
            fprintf(stderr, "\tWaveform %s test failed.\n", waveform_tests->waveformPV[i]);
          for (j = 0; j < length[i]; j++) {
            if (outRange[i][j])
              fprintf(stderr, "\t\t%s\t\t%g\n", waveform_tests->DeviceName[i][j], value[i][j]);
          }
        }
        fprintf(stderr, "Waiting for %f seconds.\n", MAX(waveform_tests->longestSleep, loopParam->interval));
      }
    }
  }
  return outOfRange;
}

void cleanupWaveforms(WAVE_FORMS *wave_forms) {
  long i;
  if (wave_forms && wave_forms->waveformFiles) {
    if (wave_forms->waveformFile) {
      for (i = 0; i < wave_forms->waveformFiles; i++) {
        if (wave_forms->waveformFile[i])
          free(wave_forms->waveformFile[i]);
      }
      free(wave_forms->waveformFile);
      wave_forms->waveformFile = NULL;
    }
  }
  if (wave_forms && wave_forms->waveforms) {
    for (i = 0; i < wave_forms->waveforms; i++) {
      if (wave_forms->waveformPV) {
        if (wave_forms->waveformPV[i])
          free(wave_forms->waveformPV[i]);
        wave_forms->waveformPV[i] = NULL;
      }
      if (wave_forms->writeWaveformPV)
        if (wave_forms->writeWaveformPV[i])
          free(wave_forms->writeWaveformPV[i]);
      if (wave_forms->readbackIndex)
        if (wave_forms->readbackIndex[i])
          free(wave_forms->readbackIndex[i]);
      if (wave_forms->waveformData)
        if (wave_forms->waveformData[i])
          free(wave_forms->waveformData[i]);
    }
    wave_forms->waveforms = 0;
    if (wave_forms->waveformData) {
      free(wave_forms->waveformData);
      wave_forms->waveformData = NULL;
    }
    if (wave_forms->writeWaveformPV && (wave_forms->writeWaveformPV != wave_forms->waveformPV)) {
      free(wave_forms->writeWaveformPV);
      wave_forms->writeWaveformPV = NULL;
    }
    if (wave_forms->waveformPV) {
      free(wave_forms->waveformPV);
      wave_forms->waveformPV = NULL;
    }
    if (wave_forms->waveformLength) {
      free(wave_forms->waveformLength);
      wave_forms->waveformLength = NULL;
    }
    if (wave_forms->writeChannelInfo && (wave_forms->writeChannelInfo != wave_forms->channelInfo)) {
      if (wave_forms->writeChannelInfo[0].count)
        free(wave_forms->writeChannelInfo[0].count);
      free(wave_forms->writeChannelInfo);
      wave_forms->writeChannelInfo = NULL;
    }
    if (wave_forms->channelInfo) {
      if (wave_forms->channelInfo[0].count)
        free(wave_forms->channelInfo[0].count);
      free(wave_forms->channelInfo);
      wave_forms->channelInfo = NULL;
    }
    if (wave_forms->readbackIndex) {
      free(wave_forms->readbackIndex);
      wave_forms->readbackIndex = NULL;
    }
    if (wave_forms->readbackValue) {
      free(wave_forms->readbackValue);
      wave_forms->readbackValue = NULL;
    }
  }
}

void cleanupTestWaveforms(WAVEFORM_TESTS *waveform_tests) {
  long i, j;
  if (waveform_tests->testFile) {
    for (i = 0; i < waveform_tests->testFiles; i++) {
      if (waveform_tests->testFile[i])
        free(waveform_tests->testFile[i]);
      waveform_tests->testFile[i] = NULL;
    }
    free(waveform_tests->testFile);
    waveform_tests->testFile = NULL;
  }
  if (waveform_tests->waveformTests) {
    if (waveform_tests->DeviceName) {
      for (i = 0; i < waveform_tests->waveformTests; i++) {
        if (waveform_tests->DeviceName[i]) {
          if (waveform_tests->waveformLength) {
            for (j = 0; j < waveform_tests->waveformLength[i]; j++) {
              if (waveform_tests->DeviceName[i][j])
                free(waveform_tests->DeviceName[i][j]);
            }
          }
          free(waveform_tests->DeviceName[i]);
        }
      }
      free(waveform_tests->DeviceName);
      waveform_tests->DeviceName = NULL;
    }
    for (i = 0; i < waveform_tests->waveformTests; i++) {
      if (waveform_tests->waveformPV)
        if (waveform_tests->waveformPV[i])
          free(waveform_tests->waveformPV[i]);
      if (waveform_tests->index)
        if (waveform_tests->index[i])
          free(waveform_tests->index[i]);
      if (waveform_tests->outOfRange)
        if (waveform_tests->outOfRange[i])
          free(waveform_tests->outOfRange[i]);
      if (waveform_tests->writeToFile)
        if (waveform_tests->writeToFile[i])
          free(waveform_tests->writeToFile[i]);
      if (waveform_tests->waveformData)
        if (waveform_tests->waveformData[i])
          free(waveform_tests->waveformData[i]);
      if (waveform_tests->min)
        if (waveform_tests->min[i])
          free(waveform_tests->min[i]);
      if (waveform_tests->max)
        if (waveform_tests->max[i])
          free(waveform_tests->max[i]);
      if (waveform_tests->ignore)
        if (waveform_tests->ignore[i])
          free(waveform_tests->ignore[i]);
      if (waveform_tests->sleep)
        if (waveform_tests->sleep[i])
          free(waveform_tests->sleep[i]);
      if (waveform_tests->holdOff)
        if (waveform_tests->holdOff[i])
          free(waveform_tests->holdOff[i]);
      if (waveform_tests->despike)
        if (waveform_tests->despike[i])
          free(waveform_tests->despike[i]);
      if (waveform_tests->despikeIndex)
        if (waveform_tests->despikeIndex[i])
          free(waveform_tests->despikeIndex[i]);
      if (waveform_tests->sleep)
        if (waveform_tests->sleep[i])
          free(waveform_tests->sleep[i]);
    }
    if (waveform_tests->ignore) {
      free(waveform_tests->ignore);
      waveform_tests->ignore = NULL;
    }
    if (waveform_tests->waveformPV) {
      free(waveform_tests->waveformPV);
      waveform_tests->waveformPV = NULL;
    }
    if (waveform_tests->waveformData) {
      free(waveform_tests->waveformData);
      waveform_tests->waveformData = NULL;
    }
    if (waveform_tests->channelInfo) {
      if (waveform_tests->channelInfo[0].count)
        free(waveform_tests->channelInfo[0].count);
      free(waveform_tests->channelInfo);
      waveform_tests->channelInfo = NULL;
    }
    if (waveform_tests->max) {
      free(waveform_tests->max);
      waveform_tests->max = NULL;
    }
    if (waveform_tests->min) {
      free(waveform_tests->min);
      waveform_tests->min = NULL;
    }
    if (waveform_tests->outOfRange) {
      free(waveform_tests->outOfRange);
      waveform_tests->outOfRange = NULL;
    }
    if (waveform_tests->index) {
      free(waveform_tests->index);
      waveform_tests->index = NULL;
    }
    if (waveform_tests->testFailed) {
      free(waveform_tests->testFailed);
      waveform_tests->testFailed = NULL;
    }
    if (waveform_tests->waveformLength) {
      free(waveform_tests->waveformLength);
      waveform_tests->waveformLength = NULL;
    }
    if (waveform_tests->sleep) {
      free(waveform_tests->sleep);
      waveform_tests->sleep = NULL;
    }
    if (waveform_tests->holdOff) {
      free(waveform_tests->holdOff);
      waveform_tests->holdOff = NULL;
    }
    if (waveform_tests->despike) {
      free(waveform_tests->despike);
      waveform_tests->despike = NULL;
    }
    if (waveform_tests->despikeIndex) {
      free(waveform_tests->despikeIndex);
      waveform_tests->despikeIndex = NULL;
    }
    if (waveform_tests->writeToFile) {
      free(waveform_tests->writeToFile);
      waveform_tests->writeToFile = NULL;
    }
    if (waveform_tests->despikeValues) {
      free(waveform_tests->despikeValues);
      waveform_tests->despikeValues = NULL;
    }
    if (waveform_tests->waveformSleep) {
      free(waveform_tests->waveformSleep);
      waveform_tests->waveformSleep = NULL;
    }
    if (waveform_tests->waveformHoldOff) {
      free(waveform_tests->waveformHoldOff);
      waveform_tests->waveformHoldOff = NULL;
    }
  }
  waveform_tests->waveformTests = 0;
}

long WriteWaveformData(WAVE_FORMS *controlWaveforms, CONTROL_NAME *control, double pendIOTime) {
  long i, n, length, j, k;
  char **PVs;
  CHANNEL_INFO *channelInfo;

  length = 0;
  if (!controlWaveforms->waveforms)
    return 0;
  n = controlWaveforms->waveforms;
  PVs = controlWaveforms->writeWaveformPV;
  channelInfo = controlWaveforms->writeChannelInfo;
  for (i = 0; i < n; i++) {
    length = controlWaveforms->waveformLength[i];
    for (j = 0; j < length; j++) {
      k = controlWaveforms->readbackIndex[i][j];
      controlWaveforms->waveformData[i][j] = 0;
      if (k >= 0) {
        if (isnan(control->value[0][k])) {
          fprintf(stderr, "error: NaN found after matrix-readback multiplication (if no NaN found in readbacks which should be printed before this error message, check the matrix). Execution aborted.\n");
          FreeEverything();
          exit(1);
        }
        controlWaveforms->waveformData[i][j] = control->value[0][k];
      }
    }
  }
#if defined(NOCAPUT)
  return (0);
#endif
  *(channelInfo[0].count) = n;
  for (i = 0; i < n; i++) {
    length = controlWaveforms->waveformLength[i];
    channelInfo[i].flag = 0;
#if !defined(DBAccess)
    if (ca_state(channelInfo[i].channelID) != cs_conn) {
      fprintf(stderr, "Error, no connection for %s\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
    if (ca_array_put(DBR_DOUBLE, length, channelInfo[i].channelID, controlWaveforms->waveformData[i]) != ECA_NORMAL) {
#else
    if (dbPutField(&channelInfo[i].channelID, DBR_DOUBLE, controlWaveforms->waveformData[i], length)) {
#endif
      fprintf(stderr, "error: unable to put waveform for %s.\n", PVs[i]);
      FreeEverything();
      exit(1);
    }
  }
#if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "pendIOerror: unable to put waveform PV values\n");
    FreeEverything();
    exit(1);
  }
#endif
  *(channelInfo[0].count) = 0;

  return 0;
}

void CleanUpCorrections(CORRECTION *correction) {
  int i;
  if (correction->file) {
    if (correction->K->a) {
      for (i = 0; i < correction->K->m; i++) {
        if (correction->K->a[i])
          free(correction->K->a[i]);
      }
      free(correction->K->a);
      correction->K->a = NULL;
    }
    if (correction->actuator) {
      free(correction->actuator);
      correction->actuator = NULL;
    }
    if (correction->control->symbolicName) {
      for (i = 0; i < correction->control->n; i++) {
        if (correction->control->symbolicName[i])
          free(correction->control->symbolicName[i]);
        correction->control->symbolicName[i] = NULL;
      }
      free(correction->control->symbolicName);
      correction->control->symbolicName = NULL;
    }
    if (correction->readback->symbolicName) {
      for (i = 0; i < correction->readback->n; i++) {
        if (correction->readback->symbolicName[i])
          free(correction->readback->symbolicName[i]);
        correction->readback->symbolicName[i] = NULL;
      }
      free(correction->readback->symbolicName);
      correction->readback->symbolicName = NULL;
    }
    if (correction->readback->channelInfo) {
      if (correction->readback->channelInfo[0].count)
        free(correction->readback->channelInfo[0].count);
      free(correction->readback->channelInfo);
    }
    if (correction->control->channelInfo) {
      if (correction->control->channelInfo[0].count)
        free(correction->control->channelInfo[0].count);
      free(correction->control->channelInfo);
    }
    if (correction->control->setFlag)
      free(correction->control->setFlag);
    if (correction->definitionFile) {
      if (correction->control->controlName) {
        for (i = 0; i < correction->control->n; i++) {
          if (correction->control->controlName[i])
            free(correction->control->controlName[i]);
        }
        free(correction->control->controlName);
        correction->control->controlName = NULL;
      }
      if (correction->readback->controlName) {
        for (i = 0; i < correction->readback->n; i++) {
          if (correction->readback->controlName[i])
            free(correction->readback->controlName[i]);
        }
        free(correction->readback->controlName);
        correction->readback->controlName = NULL;
      }
      free(correction->definitionFile);
      correction->definitionFile = NULL;
    }
    if (correction->control->despike)
      free(correction->control->despike);
    if (correction->control->waveformIndex)
      free(correction->control->waveformIndex);
    if (correction->control->waveformMatchFound)
      free(correction->control->waveformMatchFound);
    if (correction->control->value) {
      for (i = 0; i < correction->control->valueIndexes; i++) {
        if (correction->control->value[i])
          free(correction->control->value[i]);
      }
      free(correction->control->value);
    }
    if (correction->control->setpoint)
      free(correction->control->setpoint);
    if (correction->control->error)
      free(correction->control->error);
    if (correction->control->initial)
      free(correction->control->initial);
    if (correction->control->old)
      free(correction->control->old);
    if (correction->control->delta) {
      for (i = 0; i < correction->control->valueIndexes; i++) {
        if (correction->control->delta[i])
          free(correction->control->delta[i]);
      }
      free(correction->control->delta);
    }
    if (correction->readback->despike)
      free(correction->readback->despike);
    if (correction->readback->waveformIndex)
      free(correction->readback->waveformIndex);
    if (correction->readback->waveformMatchFound)
      free(correction->readback->waveformMatchFound);
    if (correction->readback->value) {
      for (i = 0; i < correction->readback->valueIndexes; i++) {
        if (correction->readback->value[i])
          free(correction->readback->value[i]);
      }
      free(correction->readback->value);
    }
    if (correction->readback->setpoint)
      free(correction->readback->setpoint);
    if (correction->readback->error)
      free(correction->readback->error);
    if (correction->readback->initial)
      free(correction->readback->initial);
    if (correction->readback->old)
      free(correction->readback->old);
    if (correction->readback->delta) {
      for (i = 0; i < correction->readback->valueIndexes; i++) {
        if (correction->readback->delta[i])
          free(correction->readback->delta[i]);
      }
      free(correction->readback->delta);
    }
    free(correction->file);
    correction->file = NULL;
  }
  if (correction->coefFile) {
    if (correction->aCoef->a) {
      for (i = 0; i < correction->aCoef->n; i++)
        if (correction->aCoef->a[i])
          free(correction->aCoef->a[i]);
      free(correction->aCoef->a);
      correction->aCoef->a = NULL;
    }

    if (correction->bCoef->a) {
      for (i = 0; i < correction->bCoef->n; i++)
        if (correction->bCoef->a[i])
          free(correction->bCoef->a[i]);
      free(correction->bCoef->a);
      correction->bCoef->a = NULL;
    }

    if (correction->control->history->a) {
      for (i = 0; i < correction->control->history->n; i++)
        if (correction->control->history->a[i])
          free(correction->control->history->a[i]);
      free(correction->control->history->a);
      correction->control->history->a = NULL;
    }
    if (correction->control->historyFiltered->a) {
      for (i = 0; i < correction->control->historyFiltered->n; i++)
        if (correction->control->historyFiltered->a[i])
          free(correction->control->historyFiltered->a[i]);
      free(correction->control->historyFiltered->a);
      correction->control->historyFiltered->a = NULL;
    }
    free(correction->coefFile);
    correction->coefFile = NULL;
  }
  if (correction->control) {
    if (correction->control->historyFiltered) {
      free(correction->control->historyFiltered);
      correction->control->historyFiltered = NULL;
    }
    if (correction->control->history) {
      free(correction->control->history);
      correction->control->history = NULL;
    }
  }
  if (correction->aCoef) {
    free(correction->aCoef);
    correction->aCoef = NULL;
  }
  if (correction->bCoef) {
    free(correction->bCoef);
    correction->bCoef = NULL;
  }
  if (correction->readback) {
    free(correction->readback);
    correction->readback = NULL;
  }
  if (correction->control) {
    free(correction->control);
    correction->control = NULL;
  }
  if (correction->K) {
    free(correction->K);
    correction->K = NULL;
  }
}

void CleanUpOthers(DESPIKE_PARAM *despike, TESTS *tests, LOOP_PARAM *loopParam) {
  long i;

  if (loopParam->offsetFile) {
    free(loopParam->offsetFile);
    loopParam->offsetFile = NULL;
  }
  if (loopParam->searchPath) {
    free(loopParam->searchPath);
    loopParam->searchPath = NULL;
  }
  if (loopParam->gainPV) {
    free(loopParam->gainPV);
    free(loopParam->gainPVInfo.count);
    loopParam->gainPV = NULL;
  }
  if (loopParam->intervalPV) {
    free(loopParam->intervalPV);
    free(loopParam->intervalPVInfo.count);
    loopParam->intervalPV = NULL;
  }
  if (loopParam->averagePV) {
    free(loopParam->averagePV);
    free(loopParam->averagePVInfo.count);
    loopParam->averagePV = NULL;
  }
  if (loopParam->launcherPV[0]) {
    free(loopParam->launcherPV[0]);
    free(loopParam->launcherPV[1]);
    free(loopParam->launcherPV[2]);
    free(loopParam->launcherPV[3]);
    free(loopParam->launcherPV[4]);
    free(loopParam->launcherPVInfo[0].count);
    free(loopParam->launcherPVInfo[1].count);
    free(loopParam->launcherPVInfo[2].count);
    free(loopParam->launcherPVInfo[3].count);
    free(loopParam->launcherPVInfo[4].count);
    loopParam->launcherPV[0] = NULL;
  }
  if (loopParam->step) {
    free(loopParam->step);
    loopParam->step = NULL;
  }
  if (loopParam->epochTime) {
    free(loopParam->epochTime);
    loopParam->epochTime = NULL;
  }
  if (loopParam->elapsedTime) {
    free(loopParam->elapsedTime);
    loopParam->elapsedTime = NULL;
  }
  if (loopParam->offsetPVFile) {
    if (loopParam->offsetPV) {
      for (i = 0; i < loopParam->n; i++) {
        if (loopParam->offsetPV[i])
          free(loopParam->offsetPV[i]);
      }
      free(loopParam->offsetPV);
      loopParam->offsetPV = NULL;
    }
    if (loopParam->channelInfo) {
      free(loopParam->channelInfo[0].count);
      free(loopParam->channelInfo);
      loopParam->channelInfo = NULL;
    }
    if (loopParam->offsetPVvalue) {
      free(loopParam->offsetPVvalue);
      loopParam->offsetPVvalue = NULL;
    }
    free(loopParam->offsetPVFile);
    loopParam->offsetPVFile = NULL;
  }
  if (loopParam->postChangeExec)
    free(loopParam->postChangeExec);
  if (despike->file) {
    if (despike->symbolicName && despike->symbolicName != despike->controlName) {
      for (i = 0; i < despike->n; i++)
        if (despike->symbolicName[i])
          free(despike->symbolicName[i]);
      free(despike->symbolicName);
      despike->symbolicName = NULL;
    }
    if (despike->controlName) {
      for (i = 0; i < despike->n; i++)
        if (despike->controlName[i])
          free(despike->controlName[i]);
      free(despike->controlName);
      despike->controlName = NULL;
    }
    if (despike->despike)
      free(despike->despike);
    if (despike->thresholdPV) {
      free(despike->thresholdPV);
      free(despike->thresholdPVInfo.count);
      despike->thresholdPV = NULL;
    }
    free(despike->file);
    despike->file = NULL;
  }
  if (tests->file) {
    if (tests->controlName) {
      for (i = 0; i < tests->n; i++)
        free(tests->controlName[i]);
      free(tests->controlName);
      tests->controlName = NULL;
    }
    if (tests->outOfRange)
      free(tests->outOfRange);
    if (tests->value)
      free(tests->value);
    if (tests->min)
      free(tests->min);
    if (tests->max)
      free(tests->max);
    if (tests->sleep)
      free(tests->sleep);
    if (tests->reset)
      free(tests->reset);
    if (tests->writeToFile)
      free(tests->writeToFile);
    if (tests->channelInfo) {
      if (tests->channelInfo[0].count)
        free(tests->channelInfo[0].count);
      free(tests->channelInfo);
    }
    if (tests->holdOff)
      free(tests->holdOff);
    if (tests->despike)
      free(tests->despike);
    if (tests->despikeIndex)
      free(tests->despikeIndex);
    if (tests->glitchLog)
      free(tests->glitchLog);
    free(tests->file);
    tests->file = NULL;
  }
}

void CleanUpLimits(LIMITS *limits) {
  long i;
  if (limits->file) {
    if (limits->controlName) {
      for (i = 0; i < limits->n; i++)
        if (limits->controlName[i])
          free(limits->controlName[i]);
      limits->n = 0;
      free(limits->controlName);
      limits->controlName = NULL;
    }
    if (limits->value) {
      free(limits->value);
      limits->value = NULL;
    }
    if (limits->minValue) {
      free(limits->minValue);
      limits->minValue = NULL;
    }
    if (limits->maxValue) {
      free(limits->maxValue);
      limits->maxValue = NULL;
    }
    if (limits->index) {
      free(limits->index);
      limits->index = NULL;
    }
  }
}

void SetupRawCAConnection(char **PVname, CHANNEL_INFO *channelInfo, long n, double pendIOTime) {
  long j, i;

  if (!channelInfo) {
    FreeEverything();
    SDDS_Bomb("Memory has not been allocated for channelID.\n");
  }
  for (i = 1; i < n; i++)
    channelInfo[i].count = channelInfo[0].count;
  *(channelInfo[0].count) = n;

  for (j = 0; j < n; j++) {
#if defined(DBAccess)
    if (dbNameToAddr(PVname[j], &channelInfo[j].channelID)) {
#else
    if (ca_search(PVname[j], &channelInfo[j].channelID) != ECA_NORMAL) {
#endif
      fprintf(stderr, "error: problem doing search for %s\n", PVname[j]);
      FreeEverything();
      exit(1);
    }
  }
#if !defined(DBAccess)
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "Unable to setup connections for \n");
    for (j = 0; j < n; j++)
      fprintf(stderr, "%s   ", PVname[j]);
    fprintf(stderr, "\n");
    FreeEverything();
    exit(1);
  }
#endif
}

void FreeEverything(void) {
  int ca = 1;
#if defined(vxWorks)
  int i;
#endif
  signal(SIGINT, interrupt_handler2);
  signal(SIGTERM, interrupt_handler2);
  signal(SIGILL, interrupt_handler2);
  signal(SIGABRT, interrupt_handler2);
  signal(SIGFPE, interrupt_handler2);
  signal(SIGSEGV, interrupt_handler2);
#ifndef _WIN32
  signal(SIGHUP, interrupt_handler2);
  signal(SIGQUIT, interrupt_handler2);
  signal(SIGTRAP, interrupt_handler2);
  signal(SIGBUS, interrupt_handler2);
#endif
#if !defined(DBAccess)
#  ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#  endif
#endif
#if defined(vxWorks)
  if (ca) {
    if (sddscontrollawGlobal->loopParam.launcherPV[0]) {
      setStringPV(sddscontrollawGlobal->loopParam.launcherPV[0], "Application exited.", sddscontrollawGlobal->loopParam.launcherPVInfo[0], 0);
      setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[1], 2, sddscontrollawGlobal->loopParam.launcherPVInfo[1], 0);
      setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[3], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[3], 0);
      setEnumPV(sddscontrollawGlobal->loopParam.launcherPV[4], 0, sddscontrollawGlobal->loopParam.launcherPVInfo[4], 0);
#  if !defined(DBAccess)
      if (ca_pend_io(10.0) != ECA_NORMAL) {
        fprintf(stderr, "pendIOerror: unable to put PV values\n");
      }
#  endif
    }
  }
#endif

  if (ca) {
#ifdef USE_RUNCONTROL
    if ((sddscontrollawGlobal->rcParam.init)) {
      if ((sddscontrollawGlobal->rcParam.PV) && (sddscontrollawGlobal->rcParam.status != RUNCONTROL_DENIED)) {
#  ifdef USE_LOGDAEMON
        if (sddscontrollawGlobal->useLogDaemon && sddscontrollawGlobal->rcParam.PV) {
          logClose(sddscontrollawGlobal->logHandle);
        }
#  endif
        strcpy(sddscontrollawGlobal->rcParam.message, "Application exited.");
        runControlLogMessage(sddscontrollawGlobal->rcParam.handle, sddscontrollawGlobal->rcParam.message, MAJOR_ALARM, &(sddscontrollawGlobal->rcParam.rcInfo));
        switch (runControlExit(sddscontrollawGlobal->rcParam.handle, &(sddscontrollawGlobal->rcParam.rcInfo))) {
        case RUNCONTROL_OK:
          break;
        case RUNCONTROL_ERROR:
          fprintf(stderr, "Error exiting run control.\n");
          break;
        default:
          fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
        }
      }
    }
#endif
#if !defined(DBAccess)
    ca_task_exit();
#endif
  }
  CleanUpCorrections(&(sddscontrollawGlobal->correction));
  CleanUpCorrections(&(sddscontrollawGlobal->overlapCompensation));
  CleanUpOthers(&(sddscontrollawGlobal->despikeParam), &(sddscontrollawGlobal->test), &(sddscontrollawGlobal->loopParam));
  CleanUpLimits(&(sddscontrollawGlobal->delta));
  CleanUpLimits(&(sddscontrollawGlobal->action));
  CleanUpLimits(&(sddscontrollawGlobal->readbackLimits));

  cleanupWaveforms(&(sddscontrollawGlobal->readbackWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->controlWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->offsetWaveforms));
  cleanupWaveforms(&(sddscontrollawGlobal->ffSetpointWaveforms));
  cleanupTestWaveforms(&(sddscontrollawGlobal->waveform_tests));

  if (sddscontrollawGlobal->outputFile) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->outputPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->outputFile);
    sddscontrollawGlobal->outputFile = NULL;
  }
  if (sddscontrollawGlobal->statsFile) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->statsPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->statsFile);
    sddscontrollawGlobal->statsFile = NULL;
  }
  if (sddscontrollawGlobal->glitchParam.file) {
    if (!SDDS_Terminate(&sddscontrollawGlobal->glitchPage)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    }
    free(sddscontrollawGlobal->glitchParam.file);
    sddscontrollawGlobal->glitchParam.file = NULL;
  }

  if (sddscontrollawGlobal->controlLogFile) {
    free(sddscontrollawGlobal->controlLogFile);
    sddscontrollawGlobal->controlLogFile = NULL;
  }
#if defined(vxWorks)
  if (*sddscontrollawGlobal->argv) {
    for (i = 0; i < sddscontrollawGlobal->argc; i++) {
      if ((*sddscontrollawGlobal->argv)[i]) {
        free((*sddscontrollawGlobal->argv)[i]);
        (*sddscontrollawGlobal->argv)[i] = NULL;
      }
    }
    free(*sddscontrollawGlobal->argv);
    *sddscontrollawGlobal->argv = NULL;
  }
#endif

  free(sddscontrollawGlobal);
  sddscontrollawGlobal = NULL;
#if defined(vxWorks)
#  if defined(TASKVAR)
  taskVarDelete(0, (int *)&sddscontrollawGlobal);
#  endif
#endif
}

#if !defined(DBAccess)
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

void oag_ca_exception_handler(struct exception_handler_args args) {
  char *pName;
  int severityInt;
  static const char *severity[] = {
    "Warning",
    "Success",
    "Error",
    "Info",
    "Fatal",
    "Fatal",
    "Fatal",
    "Fatal"};

  severityInt = CA_EXTRACT_SEVERITY(args.stat);

  fprintf(stderr, "CA.Client.Exception................\n");
  fprintf(stderr, "    %s: \"%s\"\n", severity[severityInt], ca_message(args.stat));

  if (args.ctx) {
    fprintf(stderr, "    Context: \"%s\"\n", args.ctx);
  }
  if (args.chid) {
    pName = (char *)ca_name(args.chid);
    fprintf(stderr, "    Channel: \"%s\"\n", pName);
    fprintf(stderr, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
  }
  fprintf(stderr, "This sometimes indicates an IOC that is hung up.\n");
  _exit(1);
}

#endif

void datastrobeTriggerEventHandler(struct event_handler_args event) {
  if (event.status != ECA_NORMAL) {
    fprintf(stderr, "Error received on data strobe PV\n");
    return;
  }
#ifdef DEBUG
  fprintf(stderr, "Received callback on data strobe PV\n");
#endif
  if (sddscontrollawGlobal->loopParam.trigger.initialized == 0) {
    /* the first callback is just the connection event */
    sddscontrollawGlobal->loopParam.trigger.initialized = 1;
    sddscontrollawGlobal->loopParam.trigger.datalogged = 1;
    return;
  }
  sddscontrollawGlobal->loopParam.trigger.currentValue = *((double *)event.dbr);
#ifdef DEBUG
  fprintf(stderr, "chid=%d, current=%f\n", (int)sddscontrollawGlobal->loopParam.trigger.channelID, sddscontrollawGlobal->loopParam.trigger.currentValue);
#endif
  sddscontrollawGlobal->loopParam.trigger.triggered = 1;
  if (!sddscontrollawGlobal->loopParam.trigger.datalogged)
    sddscontrollawGlobal->loopParam.trigger.datastrobeMissed++;
  sddscontrollawGlobal->loopParam.trigger.datalogged = 0;
  return;
}

long setupDatastrobeTriggerCallbacks(DATASTROBE_TRIGGER *datastrobeTrigger) {
  datastrobeTrigger->trigStep = -1;
  datastrobeTrigger->currentValue = 0;
  datastrobeTrigger->triggered = 0;
  datastrobeTrigger->datastrobeMissed = 0;
  datastrobeTrigger->usrValue = 1;
  datastrobeTrigger->datalogged = 0;
  datastrobeTrigger->initialized = 0;
  if (ca_search(datastrobeTrigger->PV, &datastrobeTrigger->channelID) != ECA_NORMAL) {
    fprintf(stderr, "error: search failed for trigger control name %s\n", datastrobeTrigger->PV);
    return 0;
  }
  ca_poll();
  if (ca_add_masked_array_event(DBR_DOUBLE, 1, datastrobeTrigger->channelID,
                                datastrobeTriggerEventHandler,
                                (void *)&datastrobeTrigger, (ca_real)0, (ca_real)0,
                                (ca_real)0, NULL, DBE_VALUE) != ECA_NORMAL) {
    fprintf(stderr, "error: unable to setup datastrobe callback for control name %s\n",
            datastrobeTrigger->PV);
    exit(1);
  }
#ifdef DEBUG
  fprintf(stderr, "Set up callback on data strobe PV\n");
#endif
  ca_poll();
  return 1;
}
