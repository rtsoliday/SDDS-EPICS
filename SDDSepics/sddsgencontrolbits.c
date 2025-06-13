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
 *  Revision 1.35 2013/01/25 shang
 *  fixed the Index column output of controlbits file which gave wrong value because the index variable type was defined as long instead of int32_t.  
 *
 *  Revision 1.34 2013/01/24 shang
 *  fixed core dump error due to reading the "long" type data of sdds file which should int32_t instead of long, and freed big memory leaks (90%).
 *
 *  Revision 1.33 2013/01/24 shang
 *  fixed problem in generating sample bits when the turn marker interval is greater 324, where the sample points were lost after 310 time slots.
 *
 *  Revision 1.32  2010/07/14 22:22:35  shang
 *  made the sample bit to be zero everywhere if the samples per bunch is zero.
 *
 *  Revision 1.31  2010/02/01 17:47:35  shang
 *  removed printout statements
 *
 *  Revision 1.30  2010/01/21 22:28:40  shang
 *  changed to set the samples bits before turn marker offset to 0 only for non 324 interval, and single-bunch sample mode
 *
 *  Revision 1.29  2010/01/21 22:01:29  shang
 *  commented out the part of setting the sample bits to zero before turn marker offset since it is not correct.
 *
 *  Revision 1.28  2010/01/05 20:31:31  shang
 *  added -turnMarkerInterval option so that user can change the turn marker interval instead of fixed value of 324. The default value is 324.
 *
 *  Revision 1.27  2009/08/21 21:34:59  shang
 *  fixed a bug in computing RAM waveform from control bits where a "+" should be used instead of "*"
 *
 *  Revision 1.26  2009/08/05 21:22:05  shang
 *  made extensive changes to treat 4 different receivers as requested by the hardware
 *
 *  Revision 1.25  2009/06/19 20:52:07  shang
 *  modified to generate the control bits by receiver for scope too.
 *
 *  Revision 1.24  2009/06/10 22:05:27  shang
 *  fixed a bug in computing RAM control bits which cut off the sample points in the end of array and changed the computation of scope control bits from RAM to use the trigger offset only.
 *
 *  Revision 1.23  2009/03/25 18:05:25  shang
 *  fixed a bug in parsing samplemode option
 *
 *  Revision 1.22  2009/03/24 15:31:12  shang
 *  the plane mode, commutation mode, sample mode and accumulation mode now are case-insensitive.
 *
 *  Revision 1.21  2009/03/13 18:44:34  shang
 *  fixed a bug in computing the scope control bits from control RAM, there was one sample difference.
 *
 *  Revision 1.20  2008/02/29 16:39:57  shang
 *  added -accumulatorMode option so that it can be independent of -planeMode.
 *
 *  Revision 1.19  2007/04/04 19:29:20  shang
 *  changed the x plane bit to 0 and y plane bit to 1 to be consistent with Eric's FPGA firmwave.
 *
 *  Revision 1.18  2007/03/29 15:41:27  emery
 *  Made flag variables uint32_t types.
 *  Added ca_pend_io after setRAMWaveformPV ca_search.
 *  Use %lu format for waveform debug printout.
 *
 *  Revision 1.17  2007/03/28 22:39:54  emery
 *  Updated usage, comments.
 *  Replaced variable P0Ofset with scopeTriggerIndexOffset
 *  and made its default value 0.
 *  Removed the exit statement following an error for the CA array put.
 *
 *  Revision 1.16  2007/01/23 16:24:33  emery
 *  Included changed from Shang who converted the RAM contents array to
 *  unsigned long (uint32_t) type.
 *  Removed scope flags that are no longer operational.
 *  Added more description on scope bits.
 *
 *  Revision 1.15  2006/12/15 23:34:23  emery
 *  changed transition dead time input from units of ns to units of time cycles.
 *  Variable transitionDeadTime converted to long.
 *  Fixed sample bits for receiver 2 and 3.
 *  Forgot to write values of CommutationNegate to any output
 *  file in previous version.
 *  Added comments for turn marker setting.
 *
 *  Revision 1.14  2006/08/24 21:19:11  emery
 *  Added commutationNegate flag and array. Updated
 *  the commutation flag bits for the scope waveform readback.
 *  Moved some statements ahead or behind by a few lines
 *  in order to better fit the context.
 *  For continuous bunch pattern, disabled the sampling
 *  extension, effectively making samples per bunch always 1
 *  for continuous mode.
 *
 *  Revision 1.13  2006/08/08 17:13:07  emery
 *  Updated the scope bit flags.
 *  Made the accumulator RAM bit equal to the plane switch bit.
 *  Eliminated the scope commutation switch flag.
 *
 *  Revision 1.12  2006/08/05 06:04:37  emery
 *  Removed a debugging statement used in binary string calculation.
 *
 *  Revision 1.11  2006/08/05 06:02:49  emery
 *  Added string column RAMWaveformBinaryString to RAM waveform file.
 *  This is a string of 32 characters of 0's and 1's corresponding to the
 *  RAM long integer.
 *
 *  Revision 1.10  2006/08/04 06:29:10  emery
 *  All receivers get the same flag.
 *
 *  Revision 1.9  2006/08/03 16:43:47  emery
 *  Now does hybrid mode preset patterns.
 *  Gets BpmGroupBuckets and BpmGroupMode column data from preset pattern
 *  (for determining hybrid mode). Added arguments to
 *  ObtainInformationFromBunchPatternTable.
 *  Fixed -sampleMode usage message.
 *
 *  Revision 1.8  2006/05/29 23:04:14  emery
 *  Added a MODULO macro that does modulo of a negative number.
 *  Use double array for a copy of the RAM waveform to write
 *  to file and to EPICS.
 *
 *  Revision 1.7  2006/05/24 15:57:37  emery
 *  Changed location of file of preset bunch patterns.
 *  Changed evaluation of flags from pow functions to bit rotation <<.
 *
 *  Revision 1.6  2006/05/02 18:24:26  soliday
 *  Fixed problems on WIN32
 *
 *  Revision 1.5  2006/04/24 21:22:12  emery
 *  Indented some lines, removed index variable from function
 *  ObtainInformationFromPresetTable
 *
 *  Revision 1.4  2006/04/14 17:42:26  emery
 *  Added receiver selection option. Updated RAM control bits.
 *
 *  Revision 1.3  2006/03/30 20:36:54  emery
 *  Changed variables with "epics" substring to names with RAM substring.
 *  Rewrote usage message as three separate usages, one for each input
 *  data mode.
 *
 *  Revision 1.2  2005/12/07 19:43:50  shang
 *  added the ability to generate control bits from an input control RAM file.
 *
 *  Revision 1.1  2005/11/30 17:15:41  shang
 *  first version, for generating control bits of epics and scope waveform record.
 *

 */
#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include "match_string.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <cadef.h>

/* this will do for A not more negative than -B */
#define MODULO(A, B) (((A) >= 0) ? ((A) % (B)) : (((A) + (B)) % (B)))

#define SET_RAM_WAVEFORMPV 0
#define SET_SCOPE_WAVEFORMPV 1
#define SET_PLANE_MODE 2
#define SET_COMMUTATION_MODE 3
#define SET_SAMPLE_MODE 4
#define SET_SAMPLES_PER_BUNCH 5
#define SET_TURN_MARKER_OFFSET 6
#define SET_TRANSITION_DEAD_TIME 7
#define SET_RAM_ARRAY_LENGTH 8
#define SET_SCOPE_ARRAY_LENGTH 9
#define SET_BUNCH_PATTERN_FILE 10
#define SET_PRESET_RAM_CONFIGURATION 11
#define SET_TURNS_PERWRAP 12
#define SET_SCOPE_TRIGGER_INDEX_OFFSET 13
#define SET_SETRAM_WAVEFORMPV 14
#define SET_CONTROLRAM_FILE 15
#define SET_COMMENT 16
#define SET_RECEIVER 17
#define SET_ACCUMULATOR_MODE 18
#define SET_TURN_MARKER_INTERVAL 19
#define SET_SECOND_SAMPLE_BUNCH 20
#define N_OPTIONS 21
char *option[N_OPTIONS] = {
  "RAMWaveformPV",
  "scopeWaveformPV",
  "planeMode",
  "commutationMode",
  "sampleMode",
  "samplesPerBunch",
  "turnMarkerOffset",
  "transitionDeadTime",
  "RAMArrayLength",
  "scopeArrayLength",
  "bunchPatternFile",
  "presetRAMconfiguration",
  "turnsPerWrap",
  "scopeTriggerIndexOffset",
  "setRAMWaveformPV",
  "controlRAMFile",
  "comment",
  "receiver",
  "accumulatorMode",
  "turnMarkerInterval",
  "secondSampleBunch",
};

static char *USAGE1 = "sddsgencontrolbits <inputRAMFile> <outputFile> \n\
          [-setRAMWaveformPV=<string>] [-controlRAMFile=<filename>] [-receiver=0|1|2|3] \n\
sddsgencontrolbits {-RAMWaveformPV=<string> [-scopeArrayLength=<int>] | \n\
          -scopeWaveformPV=<string> -turnsPerWrap=<int> [-RAMArrayLength=<string>] | \n\
          -RAMWaveformPV=<string> -scopeWaveformPV=<string> } <outputFile> \n\
          [-setRAMWaveformPV=<string>] [-controlRAMFile=<filename>] \n\
sddsgencontrolbits  [-RAMArrayLength=<string>] [-scopeArrayLength=<int>] [-receiver=0|1|2|3] \n\
          {-presetRAMconfiguration=<string>,file=<filename> | \n\
           -planeMode=<string> -commutationMode=<string>] \n\
           -sampleMode={single|continuous|bunchPattern=<string>} \n\
           -bunchPatternFile=<filename> -samplesPerBunch=<int> \n\
           -turnMarkerOffset=<int> -transitionDeadTime=<int> } \n\
           [-scopeTriggerIndexOffset=<int>] [-receiver=0|1|2|3] \n\
           [-setRAMWaveformPV=<string>] [-controlRAMFile=<filename>] \n\
           [-accumulatorMode=<string>] [-turnMarkerInterval=<integer>]\n\
           [-secondsSampleBunch=delay=<int>,samplesPerBunch=<int>] \n\
Creates a file of RAM control bits from an array of 32-bit long integers.\n\
Optionally writes to a RAM acquisition control waveform PV.\n\
Each usage represent one of three ways to supply input data:\n\
1) input file of RAM data,\n\
2) RAM or scope readback waveform PV, or\n\
3) RAM configuration parameters for a bunch pattern.\n";
static char *USAGE2 = "\n\n<inputRAMFile>      file containing the values of a RAM waveform record. The data \n\
                    is an array of unsigned long integer.\n\
\n\
-RAMWaveformPV      RAM waveform PV name. \n\
-scopeWaveformPV    digital scope waveform PV name. \n\
-RAMArrayLength     the length of control RAM waveform PV needed for generating\n\
                    control bits from bunch pattern.\n\
-scopeArrayLength   the length of digital scope waveform PV needed for generating\n\
                    control bits from bunch pattern.\n\
-turnsPerWrap       number of turns in each wrap; \n\
                    \"Simulated\" scope readback control bits can normally be generated \n\
                    from RAM long integer data. However for the inverse generation the quantity turnsPerWrap \n\
                    is required. This option is required for the -scopeWaveformPV input method.\n\
\n\
-presetRAMconfiguration  specifies a particular configuration from the file of RAM configuration presets.\n\
                    A preset configuration consists of Label, PlaneMode, CommutationMode, \n\
                    SampleMode, BunchPattern, SamplesPerBunch, TurnMarkerOffset, and TransitionDeadTime. \n\
                    The preset string value must match one of the values of Label of the preset file. \n\
                    The default file is /home/helios/oagData/sr/BPMcontrolRAM/presetPatterns. \n\
                    If this option is provided, then the planeMode, commutationMode and sampleMode \n\
                    options will be overridden. \n";
static char *USAGE3 = "-planeMode          the plane switch mode with values x, y, xy1, or xy2, which stands for \n\
                    x plane, y plane, switch x/y per turn, or switch x/y every two turns. \n\
                    (each turn has 324 time slots.) Either one or 4 modes has to be provided, if only one mode is provided,\n\
                    then 4 receivers will take the plane mode as provided.\n\
-accumulatorMode   the accumulation switch mode with values x, y, xy1, or xy2, which stands for \n\
                    x plane, y plane, switch x/y per turn, or switch x/y every two turns. \n\
                    (each turn has 324 time slots.) If not provided, it will be the same as plane switch.\n\
                    Either one or 4 modes has to be provided, if only one mode is provided,\n\
                    then 4 receivers will take the accumulator mode as provided.\n\
-commutationMode    commutation switch mode with possible values of a, b, ab1, and ab2, \n\
                    which stands for 0 degree, 180 degrees, switch between 0 and 180 every turn, \n\
                    or switch  between 0 and 180 every two turns. \n\
-sampleMode         sample mode with values continuous, single or pattern.\n\
                    If \"pattern\" is selected, then the value of suboption bunchPattern must be given\n\
                    and must match up with a bunch pattern defined in the preset bunch pattern file.\n\
                    Either one or 4 modes has to be provided, if only one mode is provided,\n\
                    then 4 receivers will take the sample mode as provided.\n\
-bunchPatternFile   file that contains the properties of preset bunch patterns. \n\
-transitionDeadTime dead time length in units of timing slots for which samples will not be taken. \n\
                    Either 1 or 4 values should be provided, if only 1 value\n\
                    is provided, then all receivers will have the same transistionDeadTime value.\n\
-turnMarkerOffset   the offset of turn marker. The first turn marker starts from this offset, and \n\
                    the others are spaced 324 time steps. \n\
-samplesPerBunch    number of consectuive samples to be collected per bunch. Presently it \n\
                    is the same for all bunches. Either 1 or 4 values should be provided, if only 1 value\n\
                    is provided, then all receivers will have the same samplesPerBunch value.\n\
-secondSampleBunch  adding second sample bunch, delay is the distance from first sample bunch, and samples per bunch gives \
                    the number of consecutive samples per bunch.\n\
-turnMarkerInterval turn marker interval, default is 324.\n\
-scopeTriggerIndexOffset  provides the offset between scope trigger and wrap marker,\n\
                    which is normally set at 2048 by PV S42B:scope:gtr:numberPTS.\n\n";

static char *USAGE4 = "The output options are the same in the three usages.\n\
<outputFile>        file of the separated control bits of a RAM waveform record. \n\
                    Columns are Index, PlaneSwitch, CommutationSwitch, Sample, \n\
                    TurnMarker, P0Marker, WrapMarker, all short integers except for Index, \n\
                    which is long integer.\n\
                    The digital scope readback waveform record is simulated and is \n\
                    included as the second data page. \n\
-setRAMWaveformPV   if provided, the control RAM long integer data will be written to this PV. \n\
                    Note that when used with -RAMWaveformPV input with the same PVs, \n\
                    the same values are read and written to the same PV. \n\
-controlRAMFile     if provided, the control RAM long integer data will be written to this file. \n\
-comment            comment string for output files.\n\
-receiver           obsolete. now 4 receivers are included already. \n\
Program by H. Shang, ANL\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

/* used bits 
   the bits of RAM record (does not have P0 marker)
   Bit 0 Plane switch for receiver 0
   Bit 1 Accumulator selection for receiver 0
   Bit 2 "Use this sample" for receiver 0
   Bit 3 Plane switch for receiver 1
   Bit 4 Accumulator selection for receiver 1
   Bit 5 "Use this sample" for receiver 1
   Bit 6 Plane switch for receiver 2
   Bit 7 Accumulator selection for receiver 2
   Bit 8 "Use this sample" for receiver 2
   Bit 9 Plane switch for receiver 3
   Bit 10 Accumulator selection for receiver 3
   Bit 11 "Use this sample" for receiver 3

   Bit 14 Commutation negate
   Bit 15 Commutation switch
   Bit 28 Save this sample for scope
   Bit 29 Self-test gate
   Bit 30 Turn Marker
   Bit 31 Wrap Marker

   the bits of scope record
   The 16 bits are the same as the 16 LSB of the RAM
   Bit 0 Plane switch for receiver 0
   Bit 1 Accumulator selection for receiver 0
   Bit 2 "Use this sample" for receiver 0
   Bit 3 Plane switch for receiver 1
   Bit 4 Accumulator selection for receiver 1
   Bit 5 "Use this sample" for receiver 1
   Bit 6 Plane switch for receiver 2
   Bit 7 Accumulator selection for receiver 2
   Bit 8 "Use this sample" for receiver 2
   Bit 9 Plane switch for receiver 3
   Bit 10 Accumulator selection for receiver 3
   Bit 11 "Use this sample" for receiver 3

   Bit 14 Commutation negate
   Bit 15 Commutation switch

*/
/* the RAM bit value in 16-digit */
#define RAM_PLANE_SWITCH_FLAG0 0x0001
#define RAM_ACCUMULATOR_FLAG0 0x0002
#define RAM_SAMPLE_FLAG0 0x0004
#define RAM_COMMUTATION_NEGATE_FLAG 0x4000
#define RAM_COMMUTATION_SWITCH_FLAG 0x8000
#define RAM_SAVE_SAMPLE_FLAG 0x10000000
#define RAM_SELF_TEST_FLAG 0x20000000
#define RAM_TURN_MARKER_FLAG 0x40000000
#define RAM_WRAP_MARKER_FLAG 0x80000000

/* The ram has 4 receivers, the plane switch, accumulator, and sample bit 
   for receiver 0 are as above, for receiver 1 (bit 3, 4, 5) are as following */
#define RAM_PLANE_SWITCH_FLAG1 0x0008
#define RAM_ACCUMULATOR_FLAG1 0x0010
#define RAM_SAMPLE_FLAG1 0x0020

/* the plane switch, accumulator, and sample bit for receiver 2 (bit 6, 7, 8) */
#define RAM_PLANE_SWITCH_FLAG2 0x0040
#define RAM_ACCUMULATOR_FLAG2 0x0080
#define RAM_SAMPLE_FLAG2 0x0100

/* the plane switch, accumulator, and sample bit for receiver 3 (bit 9, 10, 11) */
#define RAM_PLANE_SWITCH_FLAG3 0x0200
#define RAM_ACCUMULATOR_FLAG3 0x0400
#define RAM_SAMPLE_FLAG3 0x0800

#define SCOPE_PLANE_SWITCH_FLAG 0x0001
#define SCOPE_ACCUMULATOR_FLAG 0x0002
#define SCOPE_SAMPLE_FLAG 0x0004
/* These flags may be used at a later time. */
#define SCOPE_PLANE_SWITCH_FLAG0 0x0001
#define SCOPE_ACCUMULATOR_FLAG0 0x0002
#define SCOPE_SAMPLE_FLAG0 0x0004
#define SCOPE_PLANE_SWITCH_FLAG1 0x0008
#define SCOPE_ACCUMULATOR_FLAG1 0x0010
#define SCOPE_SAMPLE_FLAG1 0x0020
#define SCOPE_PLANE_SWITCH_FLAG2 0x0040
#define SCOPE_ACCUMULATOR_FLAG2 0x0080
#define SCOPE_SAMPLE_FLAG2 0x0100
#define SCOPE_PLANE_SWITCH_FLAG3 0x0200
#define SCOPE_ACCUMULATOR_FLAG3 0x0400
#define SCOPE_SAMPLE_FLAG3 0x0800
#define SCOPE_COMMUTATION_NEGATE_FLAG 0x4000
#define SCOPE_COMMUTATION_SWITCH_FLAG 0x8000

typedef struct
{
  long arrayLength;
  short **planeSwitch, **accumulator, **sample, *commutationNegate, *commutationSwitch, *turnMarker, *P0Marker, *wrapMarker, *saveSample, *selfTest;
} CONTROL_BITS;
static long planeModes = 4;
static char *planeModeList[4] = {"x", "y", "xy1", "xy2"};

static long commutationModes = 4;
static char *commutationModeList[4] = {"a", "b", "ab1", "ab2"};

#define SAMPLE_CONTINOUS 0
#define SAMPLE_SINGLE 1
#define SAMPLE_BUNCHPATTERN 2
static long sampleModes = 3;
static char *sampleModeList[3] = {"continuous", "single", "bunchpattern"};

/*second sample bunches, only add samples; all others stay unchanged */

void initializeControlBits(CONTROL_BITS *controlBit);
void allocateControlBits(CONTROL_BITS *controlBit, long length, short type);
void freeControlBits(CONTROL_BITS *controlBit, short type);
void ObtainInformationFromPresetTable(char *presetFile, char *presetLabel, long *planeMode, long *accumulatorMode,
                                      long *commutationMode, long *sampleMode, char **bunchPattern,
                                      long *samplesPerBunch, long *turnMarkerOffset,
                                      long *transitionDeadTime);
void ObtainInformationFromBunchPatternTable(char *bunchFile, char *bunchPattern,
                                            long *multiplicity, long *multiplets,
                                            long *multipletStart, long *multipletSpacing,
                                            long *hybridMode, long *bpmGroupBuckets);
void ReadInputRAM(char *inputRAMFile, long *waveformLength, uint32_t **waveform,
                  char **comment, long *planeMode, long *accumulatorMode,
                  long *commutationMode, long *sampleMode, char **bunchPattern,
                  long *samplesPerBunch, long *turnMarkerOffset,
                  long *transitionDeadTime, long *turnMarkerInterval);

#define RAM_RECORD 0
#define SCOPE_RECORD 1
void GenerateControlBitsFromWaveform(uint32_t *waveform, long length, CONTROL_BITS *controlBits, short type, long *turnsPerWrap, long *turnMarkerOffset);
void GenerateControlBitsFromPreset(long *planeMode, long *accumulatorMode, long cummutationMode, long *sampleMode,
                                   long *samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                                   long transitionDeadTime,
                                   char **bunchPattern, char *bunchFile,
                                   long RAMArrayLength, CONTROL_BITS *RAM,
                                   long scopeArrayLength, CONTROL_BITS *scope,
                                   long *turnsPerWrap, long scopeTriggerIndexOffset);
void GenerateReceiverBitsFromPreset(long planeMode, long accumulatorMode, long commutationMode, long sampleMode,
                                    long samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                                    long transitionDeadTime,
                                    char *bunchPattern, char *bunchFile,
                                    long RAMArrayLength, short *planeSwitch, short *accumulator, short *sampleSwitch);
void SetupOutputFile(SDDS_DATASET *SDDSout, char *outputFile);
void WriteToOutputFile(SDDS_DATASET *SDDSout, CONTROL_BITS *controlBits, short type,
                       long *planeMode, long *accumulatorMode, long commutationMode, long *sampleMode,
                       char **bunchPattern, long *samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                       long transitionDeadTime, long turnsPerWrap, char *comment, long ramInput);
void GenerateScopeControlBitsFromRAM(CONTROL_BITS *RAM, CONTROL_BITS *scope, long scopeTriggerIndexOffset,
                                     long *turnsPerWrap, long *turnMarkerOffset, long turnMarkerInterval);
void GenerateRAMControlBitsFromScope(CONTROL_BITS *scope, CONTROL_BITS *RAM, long scopeTriggerIndexOffset,
                                     long *turnsPerWrap, long *turnMarkerOffset, long turnMarkerInterval);

void add_second_sample(CONTROL_BITS *record, long secondSampleDelay, long secondSamplesPerBunch, long turnMarkerInterval);

int main(int argc, char **argv) {
  SCANNED_ARG *s_arg;
  SDDS_TABLE SDDSout;
  chid RAMChID, scopeChID, setRAMChID;
  char *outputFile, *inputRAMFile, *presetLabel, **bunchPattern, *RAMWaveformPV, *scopeWaveformPV, *presetFile, *bunchPatternFile, *comment, *controlRAMFile, *setRAMWaveformPV;
  long *planeMode, *accumulatorMode, commutationMode, *sampleMode, i, j, i_arg, RAMArrayLength, scopeArrayLength, turnMarkerOffset, *samplesPerBunch, turnsPerWrap, scopeTriggerIndexOffset, receiver, ramInput = 0;
  uint32_t planeFlag, commutationNegateFlag, commutationFlag, sampleFlag, turnMarkerFlag, wrapMarkerFlag, accumulatorFlag, saveSampleFlag, selfTestFlag;
  uint32_t *RAMWaveform, *scopeWaveform;
  int32_t *indexColumn;
  char **RAMWaveformBinaryString, *tmpStr;
  CONTROL_BITS RAM, scope;
  unsigned long dummyFlags = 0;
  long transitionDeadTime, turnMarkerInterval = 324;
  short inputRAMFileProvided = 0;
  int32_t secondSampleDelay = 141, secondSamplesPerBunch = 30, secondSampleGiven = 0;

  setRAMWaveformPV = NULL;
  scopeTriggerIndexOffset = 0; /* originally the time slot difference between P0 and wrap marker was 4 */
  RAMWaveform = scopeWaveform = NULL;
  RAMWaveformBinaryString = NULL;
  transitionDeadTime = 10;
  turnMarkerOffset = 0;
  samplesPerBunch = NULL;
  RAMArrayLength = 3888;   /*default ioc */
  scopeArrayLength = 4096; /* 324 * 12 time slots */
  inputRAMFile = outputFile = RAMWaveformPV = scopeWaveformPV = NULL;
  bunchPattern = NULL;
  indexColumn = NULL;
  presetLabel = NULL;
  initializeControlBits(&RAM);
  initializeControlBits(&scope);
  commutationMode = 0;
  planeMode = accumulatorMode = sampleMode = NULL;
  /* 4 receivers */
  samplesPerBunch = tmalloc(sizeof(*samplesPerBunch) * 4);
  bunchPattern = tmalloc(sizeof(*bunchPattern) * 4);
  planeMode = tmalloc(sizeof(*planeMode) * 4);
  accumulatorMode = tmalloc(sizeof(*accumulatorMode) * 4);
  sampleMode = tmalloc(sizeof(*sampleMode) * 4);
  for (i = 0; i < 4; i++) {
    bunchPattern[i] = NULL;
    planeMode[i] = accumulatorMode[i] = sampleMode[i] = -1;
    samplesPerBunch[i] = 5;
  }

  presetFile = "/home/helios/oagData/sr/BPMcontrolRAM/presetConfigurations";
  bunchPatternFile = "/home/helios/oagData/sr/bunchPatterns/presetPatterns";
  turnsPerWrap = 4;
  comment = controlRAMFile = NULL;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2) {
    fprintf(stderr, "%s%s%s%s\n", USAGE1, USAGE2, USAGE3, USAGE4);
    exit(1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case SET_RECEIVER:
        /*obsolete*/
        break;
      case SET_SETRAM_WAVEFORMPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -setRAMWaveformPV syntax", NULL);
        setRAMWaveformPV = s_arg[i_arg].list[1];
        break;
      case SET_RAM_WAVEFORMPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -RAMWaveformPV syntax", NULL);
        RAMWaveformPV = s_arg[i_arg].list[1];
        break;
      case SET_SCOPE_WAVEFORMPV:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -scopeWaveformPV syntax", NULL);
        scopeWaveformPV = s_arg[i_arg].list[1];
        break;
      case SET_COMMENT:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -comment syntax", NULL);
        SDDS_CopyString(&comment, s_arg[i_arg].list[1]);
        break;
      case SET_CONTROLRAM_FILE:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -controlRAMFile syntax", NULL);
        controlRAMFile = s_arg[i_arg].list[1];
        break;
      case SET_PLANE_MODE:
        if (s_arg[i_arg].n_items == 2) {
          if ((planeMode[0] = planeMode[1] = planeMode[2] = planeMode[3] = match_string(str_tolower(s_arg[i_arg].list[1]), planeModeList, planeModes, EXACT_MATCH)) < 0)
            SDDS_Bomb("invalid planeMode given, either 1 or 4 modes should be provided!");
        } else if (s_arg[i_arg].n_items == 5) {
          for (i = 0; i < 4; i++)
            if ((planeMode[i] = match_string(str_tolower(s_arg[i_arg].list[i + 1]), planeModeList, planeModes, EXACT_MATCH)) < 0)
              SDDS_Bomb("invalid planeMode given!");
        } else
          bomb("invalid -planeMode syntax, either one or four modes should be provided", NULL);
        break;
      case SET_ACCUMULATOR_MODE:
        if (s_arg[i_arg].n_items == 2) {
          if ((accumulatorMode[0] = accumulatorMode[1] = accumulatorMode[2] = accumulatorMode[3] = match_string(str_tolower(s_arg[i_arg].list[1]), planeModeList, planeModes, EXACT_MATCH)) < 0)
            SDDS_Bomb("invalid accumulatorMode given!");
        } else if (s_arg[i_arg].n_items == 5) {
          for (i = 0; i < 4; i++)
            if ((accumulatorMode[i] = match_string(str_tolower(s_arg[i_arg].list[i + 1]), planeModeList, planeModes, EXACT_MATCH)) < 0)
              SDDS_Bomb("invalid accumulatorMode given, either 1 or 4 modes should be provided!");
        } else
          bomb("invalid -accumulatorMode syntax, either 1 or 4 modes should be provided", NULL);
        break;
      case SET_COMMUTATION_MODE:
        if (s_arg[i_arg].n_items != 2)
          bomb("invalid -commutationMode syntax", NULL);
        if ((commutationMode = match_string(str_tolower(s_arg[i_arg].list[1]), commutationModeList, commutationModes, EXACT_MATCH)) < 0)
          SDDS_Bomb("invalid commutationMode given!");
        break;
      case SET_SAMPLE_MODE:
        if (s_arg[i_arg].n_items < 2)
          bomb("invalid -sampleMode syntax", NULL);
        receiver = i = 0;
        while (i < s_arg[i_arg].n_items - 1 && receiver < 4) {
          SDDS_CopyString(&tmpStr, s_arg[i_arg].list[i + 1]);
          if ((sampleMode[receiver] = match_string(str_tolower(tmpStr), sampleModeList, sampleModes, 0)) < 0)
            SDDS_Bomb("Invalid -sampleMode given!");
          if (sampleMode[receiver] == SAMPLE_BUNCHPATTERN) {
            bunchPattern[receiver] = strchr(s_arg[i_arg].list[i + 1], '=');
            *bunchPattern[receiver]++ = 0;
          }
          i++;
          receiver++;
        }
        if (receiver == 1) {
          sampleMode[1] = sampleMode[2] = sampleMode[3] = sampleMode[0];
          bunchPattern[1] = bunchPattern[2] = bunchPattern[3] = bunchPattern[0];
        } else if (receiver != 4)
          SDDS_Bomb("Invalid -sampleMode given, either 1 or 4 modes should be provided.");
        break;
      case SET_SAMPLES_PER_BUNCH:
        if (s_arg[i_arg].n_items == 2) {
          if (!get_long(&samplesPerBunch[0], s_arg[i_arg].list[1]))
            SDDS_Bomb("invalid value given for option -samplesPerBunch");
          samplesPerBunch[1] = samplesPerBunch[2] = samplesPerBunch[3] = samplesPerBunch[0];
        } else if (s_arg[i_arg].n_items == 5) {
          for (i = 0; i < 4; i++)
            if (!get_long(&samplesPerBunch[i], s_arg[i_arg].list[i + 1]))
              SDDS_Bomb("invalid value given for option -samplesPerBunch");
        } else
          SDDS_Bomb("Invalid -samplesPerBunch syntax, either 1 or 4 values should be provided!");
        break;
      case SET_TURN_MARKER_OFFSET:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -turnMarkerOffset syntax!");
        if (!get_long(&turnMarkerOffset, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -turnMarkerOffset");
        break;
      case SET_TURN_MARKER_INTERVAL:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -turnMarkerInterval syntax!");
        if (!get_long(&turnMarkerInterval, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -turnMarkerInterval");
        break;
      case SET_TRANSITION_DEAD_TIME:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -transitionDeadTime syntax!");
        if (!get_long(&transitionDeadTime, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -transitionDeadTime");
        break;
      case SET_RAM_ARRAY_LENGTH:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -RAMArrayLength syntax!");
        if (!get_long(&RAMArrayLength, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -RAMArrayLength");
        break;
      case SET_SCOPE_ARRAY_LENGTH:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -scopeArrayLength syntax!");
        if (!get_long(&scopeArrayLength, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -scopeArrayLength");
        break;
      case SET_BUNCH_PATTERN_FILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -presetBunchPatternFile syntax!");
        bunchPatternFile = s_arg[i_arg].list[1];
        break;
      case SET_PRESET_RAM_CONFIGURATION:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("Invalid -presetRAMconfiguration syntax!");
        presetLabel = s_arg[i_arg].list[1];
        if (s_arg[i_arg].n_items == 3) {
          s_arg[i_arg].n_items -= 2;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 2, &s_arg[i_arg].n_items, 0,
                            "file", SDDS_STRING, &presetFile, 1, 0, NULL))
            SDDS_Bomb("invalid -presetRAMconfiguration syntax/values");
          s_arg[i_arg].n_items += 2;
        }
        break;
      case SET_TURNS_PERWRAP:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -turnsPerWrap syntax!");
        if (!get_long(&turnsPerWrap, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -turnsPerWrap");
        break;
      case SET_SCOPE_TRIGGER_INDEX_OFFSET:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("Invalid -scopeTriggerIndexOffset syntax!");
        if (!get_long(&scopeTriggerIndexOffset, s_arg[i_arg].list[1]))
          SDDS_Bomb("invalid value given for option -scopeTriggerIndexOffset");
        break;
      case SET_SECOND_SAMPLE_BUNCH:
        s_arg[i_arg].n_items -= 1;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "delay", SDDS_LONG, &secondSampleDelay, 1, 0,
                          "samplesPerBunch", SDDS_LONG, &secondSamplesPerBunch, 1, 0,
                          NULL))
          SDDS_Bomb("invalid -secondSampleBunch syntax/values");
        s_arg[i_arg].n_items += 1;
        secondSampleGiven = 1;
        break;
      default:
        fprintf(stderr, "unknown option - %s .\n", s_arg[i_arg].list[0]);
        break;
      }
    } else {
      if (!inputRAMFile)
        inputRAMFile = s_arg[i_arg].list[0];
      else if (!outputFile) {
        inputRAMFileProvided = 1;
        outputFile = s_arg[i_arg].list[0];
      } else
        SDDS_Bomb("Too many files provided.");
    }
  }
  for (i = 0; i < 4; i++)
    if (accumulatorMode[i] < 0)
      accumulatorMode[i] = planeMode[i];

  /* if only one file is supplied as argument, then it is an output file */
  if (!outputFile)
    outputFile = inputRAMFile;
  /* no files provided at all! */
  if (!outputFile)
    SDDS_Bomb("Output file is not provided.");
  if (presetLabel)
    ObtainInformationFromPresetTable(presetFile, presetLabel, planeMode, accumulatorMode, &commutationMode,
                                     sampleMode, bunchPattern, samplesPerBunch,
                                     &turnMarkerOffset, &transitionDeadTime);
  if (!inputRAMFileProvided && !RAMWaveformPV && (planeMode[0] < 0 || commutationMode < 0 || sampleMode[0] < 0))
    SDDS_Bomb("Nether input RAM file, waveform PV nor valid RAM configuration mode properties provided!");

  if (setRAMWaveformPV || RAMWaveformPV || scopeWaveformPV) {
#ifdef EPICS313
    ca_task_initialize();
#else
    if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
      fprintf(stderr, "Unable to initiate channel access context\n");
      return (1);
    }
#endif
    if (setRAMWaveformPV) {
      if (ca_search(setRAMWaveformPV, &setRAMChID) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", setRAMWaveformPV);
        exit(1);
      }
      if (ca_pend_io(30.0) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for some channels\n");
        exit(1);
      }
    }
  }

  /* Three modes of input data:
     1) data from input file of RAM waveform 
     2) data from epics PV
     3) data generated from bunch pattern and other parameters
  */
  if (turnMarkerOffset > turnMarkerInterval) {
    fprintf(stdout, "Warning: the turn maker offset is bigger than turn marker interval, so the turn-by-turn data can not tell which is P0.\n");
    turnMarkerOffset = turnMarkerOffset % turnMarkerInterval;
  }
  if (inputRAMFileProvided) {
    ReadInputRAM(inputRAMFile, &RAMArrayLength, &RAMWaveform,
                 &comment, planeMode, accumulatorMode, &commutationMode, sampleMode, bunchPattern,
                 samplesPerBunch, &turnMarkerOffset, &transitionDeadTime, &turnMarkerInterval);
    allocateControlBits(&RAM, RAMArrayLength, RAM_RECORD);
    allocateControlBits(&scope, scopeArrayLength, SCOPE_RECORD);
    GenerateControlBitsFromWaveform(RAMWaveform, RAMArrayLength, &RAM, RAM_RECORD,
                                    &turnsPerWrap, &turnMarkerOffset);
    GenerateScopeControlBitsFromRAM(&RAM, &scope, scopeTriggerIndexOffset, &turnsPerWrap, &turnMarkerOffset, turnMarkerInterval);
    if (!comment)
      SDDS_CopyString(&comment, "Control RAM was obtained from input RAM file.");
  }
  /* Second input method */
  else if (RAMWaveformPV || scopeWaveformPV) {
    if (RAMWaveformPV)
      if (ca_search(RAMWaveformPV, &RAMChID) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", RAMWaveformPV);
        exit(1);
      }
    if (scopeWaveformPV)
      if (ca_search(scopeWaveformPV, &scopeChID) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", scopeWaveformPV);
        exit(1);
      }
    if (ca_pend_io(30.0) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for variable channels\n");
      exit(1);
    }
    if (RAMWaveformPV) {
      RAMArrayLength = ca_element_count(RAMChID);
      RAMWaveform = (uint32_t *)calloc(sizeof(*RAMWaveform), RAMArrayLength);
      RAMWaveformBinaryString = (char **)calloc(sizeof(char *), RAMArrayLength);
      for (i = 0; i < RAMArrayLength; i++) {
        /* Binary string will be 32 characters of either 0 or 1. 
                 We need a null character at the end */
        RAMWaveformBinaryString[i] = (char *)calloc(sizeof(char), 33);
      }

      if (ca_array_get(DBR_LONG, RAMArrayLength, RAMChID, RAMWaveform) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", RAMWaveformPV);
        exit(1);
      }
    }
    if (scopeWaveformPV) {
      scopeArrayLength = ca_element_count(scopeChID);
      scopeWaveform = (uint32_t *)calloc(sizeof(*scopeWaveform), scopeArrayLength);
      if (ca_array_get(DBR_LONG, scopeArrayLength, scopeChID, scopeWaveform) != ECA_NORMAL) {
        fprintf(stderr, "error: problem reading %s\n", scopeWaveformPV);
        exit(1);
      }
    }
    if (ca_pend_io(30.0) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for some channels\n");
      exit(1);
    }
    allocateControlBits(&RAM, RAMArrayLength, RAM_RECORD);
    allocateControlBits(&scope, scopeArrayLength, SCOPE_RECORD);

    /* switching is not elegant but is clear */
    if (RAMWaveformPV && !scopeWaveformPV) {
      /* a value for turnsPerWrap and turnMarkerOffset is returned */
      GenerateControlBitsFromWaveform(RAMWaveform, RAMArrayLength, &RAM, RAM_RECORD,
                                      &turnsPerWrap, &turnMarkerOffset);
      GenerateScopeControlBitsFromRAM(&RAM, &scope, scopeTriggerIndexOffset, &turnsPerWrap, &turnMarkerOffset, turnMarkerInterval);
    }

    if (!RAMWaveformPV && scopeWaveformPV) {
      /* no value returned for turnsPerWrap and turnMarkerOffset */
      GenerateControlBitsFromWaveform(scopeWaveform, scopeArrayLength, &scope, SCOPE_RECORD,
                                      &turnsPerWrap, &turnMarkerOffset);
      GenerateRAMControlBitsFromScope(&scope, &RAM, scopeTriggerIndexOffset, &turnsPerWrap, &turnMarkerOffset, turnMarkerInterval);
    }

    if (RAMWaveformPV && scopeWaveformPV) {
      /* a value for turnsPerWrap and turnMarkerOffset is returned */
      GenerateControlBitsFromWaveform(RAMWaveform, RAMArrayLength, &RAM, RAM_RECORD,
                                      &turnsPerWrap, &turnMarkerOffset);
      GenerateControlBitsFromWaveform(scopeWaveform, scopeArrayLength, &scope, SCOPE_RECORD,
                                      &turnsPerWrap, &turnMarkerOffset);
    }

    if (!comment)
      SDDS_CopyString(&comment, "Control RAM was obtained from waveform PV.");
  }
  /* use preset RAM configuration */
  else {
    /*to make sure that the array length is at least or greater than 3888, which is the RAM EPICS waveform length */
    /*  for (i=1;i<200;i++) {
          RAMArrayLength = turnsPerWrap*turnMarkerInterval * i;
          if (RAMArrayLength>=3888)
          break;
          } */
    GenerateControlBitsFromPreset(planeMode, accumulatorMode, commutationMode, sampleMode,
                                  samplesPerBunch, turnMarkerOffset, turnMarkerInterval,
                                  transitionDeadTime, bunchPattern, bunchPatternFile,
                                  RAMArrayLength, &RAM, scopeArrayLength, &scope,
                                  &turnsPerWrap, scopeTriggerIndexOffset);
    /*RAMArrayLength = 3888; */
    if (!comment)
      SDDS_CopyString(&comment, "Control RAM was generated from preset pattern.");
  }

  /* From here on these lines are common to all three input methods. */
  if (secondSampleGiven) {
    /*added second sample bunch */
    add_second_sample(&RAM, secondSampleDelay, secondSamplesPerBunch, turnMarkerInterval);
    add_second_sample(&scope, secondSampleDelay, secondSamplesPerBunch, turnMarkerInterval);
  }

  if (setRAMWaveformPV || controlRAMFile) {
    planeFlag = 1;
    accumulatorFlag = 2;
    sampleFlag = 4;
    commutationNegateFlag = 1 << 14;
    commutationFlag = 1 << 15;
    saveSampleFlag = 1 << 28;
    selfTestFlag = 1 << 29;
    turnMarkerFlag = 1 << 30;
    wrapMarkerFlag = 1 << 31;
    if (controlRAMFile)
      indexColumn = malloc(sizeof(*indexColumn) * RAM.arrayLength);
    if (!inputRAMFileProvided) {
      RAMWaveform = (uint32_t *)malloc(sizeof(*RAMWaveform) * RAM.arrayLength);
      RAMWaveformBinaryString = (char **)malloc(sizeof(char *) * RAM.arrayLength);
      for (i = 0; i < RAMArrayLength; i++) {
        /* Binary string will be 32 characters of either 0 or 1. 
                 We need a null character at the end */
        RAMWaveformBinaryString[i] = (char *)calloc(sizeof(char), 33);
      }
    }
    for (i = 0; i < RAM.arrayLength; i++) {
      if (controlRAMFile)
        indexColumn[i] = i;
      if (!inputRAMFileProvided) {
        /* Sum the base flags for the four receivers */
        RAMWaveform[i] = RAM.planeSwitch[0][i] * planeFlag +
                         accumulatorFlag * RAM.accumulator[0][i] + sampleFlag * RAM.sample[0][i] + RAM.planeSwitch[1][i] * 8 + RAM.accumulator[1][i] * 16 + RAM.sample[1][i] * 32 + RAM.planeSwitch[2][i] * 64 + RAM.accumulator[2][i] * 128 + RAM.sample[2][i] * 256 + RAM.planeSwitch[3][i] * 512 + RAM.accumulator[3][i] * 1024 + RAM.sample[3][i] * 2048 + commutationNegateFlag * RAM.commutationNegate[i] + commutationFlag * RAM.commutationSwitch[i] + saveSampleFlag * RAM.saveSample[i] + selfTestFlag * RAM.selfTest[i] + turnMarkerFlag * RAM.turnMarker[i] + wrapMarkerFlag * RAM.wrapMarker[i];
        for (j = 0; j < 32; j++) {
          /* j is binary digit */
          RAMWaveformBinaryString[i][31 - j] = (RAMWaveform[i] >> j & 0x1) ? '1' : '0';
        }
      }

      /* fprintf(stderr,"RAMWaveform[%ld] %lu RAM.wrapMarker[%ld] %lu \n", i, RAMWaveform[i], i, RAM.wrapMarker[i]); */
    }
    if (setRAMWaveformPV) {
      if (ca_array_put(DBR_LONG, RAM.arrayLength,
                       setRAMChID, RAMWaveform) != ECA_NORMAL) {
        fprintf(stderr, "error: problem setting %s\n", setRAMWaveformPV);
      }
      if (ca_pend_io(30.0) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for some channels\n");
        exit(1);
      }
    }
    if (controlRAMFile) {
      SDDS_DATASET ramOut;
      char *timeStamp;
      double time;
      getTimeBreakdown(&time, NULL, NULL, NULL, NULL, NULL, &timeStamp);
      if (!SDDS_InitializeOutput(&ramOut, SDDS_BINARY, 0, NULL, NULL, controlRAMFile))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_DefineSimpleParameter(&ramOut, "Time", NULL, SDDS_DOUBLE) ||
          !SDDS_DefineSimpleParameter(&ramOut, "TimeStamp", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleParameter(&ramOut, "WaveformPV", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleParameter(&ramOut, "Description", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleParameter(&ramOut, "Comment", NULL, SDDS_STRING) ||
          !SDDS_DefineSimpleColumn(&ramOut, "Index", NULL, SDDS_LONG) ||
          !SDDS_DefineSimpleColumn(&ramOut, "Waveform", NULL, SDDS_ULONG) ||
          !SDDS_DefineSimpleColumn(&ramOut, "WaveformBinaryString", NULL, SDDS_STRING))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_WriteLayout(&ramOut))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!SDDS_StartPage(&ramOut, RAM.arrayLength) ||
          !SDDS_SetParameters(&ramOut, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                              "TimeStamp", timeStamp,
                              "Time", time,
                              "Comment", comment, NULL) ||
          !SDDS_SetColumn(&ramOut, SDDS_BY_NAME,
                          RAMWaveform, RAM.arrayLength, "Waveform") ||
          !SDDS_SetColumn(&ramOut, SDDS_BY_NAME,
                          RAMWaveformBinaryString, RAM.arrayLength, "WaveformBinaryString") ||
          !SDDS_SetColumn(&ramOut, SDDS_BY_NAME,
                          indexColumn, RAM.arrayLength, "Index"))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      if (!setRAMWaveformPV) {
        if (!SDDS_SetParameters(&ramOut, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "WaveformPV", "cf:AcqControlSetWF", NULL))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      } else {
        if (!SDDS_SetParameters(&ramOut, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                                "WaveformPV", setRAMWaveformPV, NULL))
          SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      }
      if (!SDDS_WritePage(&ramOut) || !SDDS_Terminate(&ramOut))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    }
    if (indexColumn)
      free(indexColumn);
  }
  if (RAMWaveformBinaryString) {
    for (i = 0; i < RAMArrayLength; i++)
      free(RAMWaveformBinaryString[i]);
    free(RAMWaveformBinaryString);
  }
  if (setRAMWaveformPV || RAMWaveformPV || scopeWaveformPV)
    ca_task_exit();
  SetupOutputFile(&SDDSout, outputFile);
  if (inputRAMFile || RAMWaveformPV)
    ramInput = 1;
  WriteToOutputFile(&SDDSout, &RAM, RAM_RECORD, planeMode, accumulatorMode, commutationMode, sampleMode,
                    bunchPattern, samplesPerBunch, turnMarkerOffset, turnMarkerInterval, transitionDeadTime,
                    turnsPerWrap, comment, ramInput);
  WriteToOutputFile(&SDDSout, &scope, SCOPE_RECORD, planeMode, accumulatorMode, commutationMode, sampleMode,
                    bunchPattern, samplesPerBunch, turnMarkerOffset, turnMarkerInterval, transitionDeadTime,
                    turnsPerWrap, comment, ramInput);
  if (!SDDS_Terminate(&SDDSout))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  freeControlBits(&RAM, RAM_RECORD);
  freeControlBits(&scope, SCOPE_RECORD);
  free_scanargs(&s_arg, argc);
  if (bunchPattern)
    free(bunchPattern);
  if (RAMWaveform)
    free(RAMWaveform);
  if (scopeWaveform)
    free(scopeWaveform);
  if (comment)
    free(comment);

  return 0;
}

void ObtainInformationFromPresetTable(char *presetFile, char *presetLabel, long *planeMode,
                                      long *accumulatorMode,
                                      long *commutationMode, long *sampleMode, char **bunchPattern,
                                      long *samplesPerBunch, long *turnMarkerOffset,
                                      long *transitionDeadTime) {
  SDDS_DATASET presetData;
  int32_t rowIndex, rows = 0, *flag = NULL, i;
  char **label = NULL;
  void *data = NULL;

  if (!presetLabel || !presetFile)
    return;
  if (!SDDS_InitializeInput(&presetData, presetFile))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_ReadPage(&presetData) < 0)
    SDDS_Bomb("Unable to read preset data.");
  if (!(rows = SDDS_CountRowsOfInterest(&presetData)))
    SDDS_Bomb("No data in preset file.");
  if (!(label = (char **)SDDS_GetColumn(&presetData, "Label")))
    SDDS_Bomb("Can not obtain Label from preset data.");
  if ((rowIndex = match_string(presetLabel, label, rows, EXACT_MATCH)) < 0) {
    fprintf(stderr, "preset %s is not found in preset file %s.\n", presetLabel, presetFile);
    exit(1);
  }
  for (i = 0; i < rows; i++)
    free(label[i]);
  free(label);
  flag = (int32_t *)calloc(sizeof(*flag), rows);
  flag[rowIndex] = 1;
  if (!SDDS_AssertRowFlags(&presetData, SDDS_FLAG_ARRAY, flag, (int64_t)rows))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  free(flag);
  if (!(data = (char **)SDDS_GetColumn(&presetData, "PlaneMode")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if ((planeMode[0] = planeMode[1] = planeMode[2] = planeMode[3] = match_string(str_tolower(((char **)data)[0]), planeModeList, planeModes, EXACT_MATCH)) < 0) {
    fprintf(stderr, "Invalid plane mode provided for %s preset in file %s.\n", presetLabel, presetFile);
    exit(1);
  }
  free(((char **)data)[0]);
  free(data);

  if (!(data = SDDS_GetColumn(&presetData, "CommutationMode")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if ((*commutationMode = match_string(str_tolower(((char **)data)[0]), commutationModeList, commutationModes, EXACT_MATCH)) < 0) {
    fprintf(stderr, "Invalid commutation mode provided for %s preset in file %s.\n", presetLabel, presetFile);
    exit(1);
  }
  free(((char **)data)[0]);
  free((char **)data);

  if (!(data = SDDS_GetColumn(&presetData, "SampleMode")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if ((sampleMode[0] = sampleMode[1] = sampleMode[2] = sampleMode[3] = match_string(str_tolower(((char **)data)[0]), sampleModeList, sampleModes, 0)) < 0) {
    fprintf(stderr, "Invalid sample mode provided for %s preset in file %s.\n", presetLabel, presetFile);
    exit(1);
  }
  free(((char **)data)[0]);
  free((char **)data);

  if (!(data = SDDS_GetColumn(&presetData, "BunchPattern")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  SDDS_CopyString(&bunchPattern[0], ((char **)data)[0]);
  bunchPattern[1] = bunchPattern[2] = bunchPattern[3] = bunchPattern[0];
  free(((char **)data)[0]);
  free((char **)data);

  if (!(data = SDDS_GetColumn(&presetData, "SamplesPerBunch")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  samplesPerBunch[0] = samplesPerBunch[1] = samplesPerBunch[2] = samplesPerBunch[3] = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&presetData, "TurnMarkerOffset")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *turnMarkerOffset = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&presetData, "TransitionDeadTime")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *transitionDeadTime = ((int32_t *)data)[0];
  free((int32_t *)data);
  if (SDDS_CheckColumn(&presetData, "AccumulatorMode", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY) {
    if (!(data = (char **)SDDS_GetColumn(&presetData, "AccumulatorMode")))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if ((accumulatorMode[0] = accumulatorMode[1] = accumulatorMode[2] = accumulatorMode[3] = match_string(str_tolower(((char **)data)[0]), planeModeList, planeModes, EXACT_MATCH)) < 0) {
      fprintf(stderr, "Invalid plane mode provided for %s preset in file %s.\n", presetLabel, presetFile);
      exit(1);
    }
    free(((char **)data)[0]);
    free(data);
  } else
    accumulatorMode[0] = accumulatorMode[1] = accumulatorMode[2] = accumulatorMode[3] = planeMode[0];
  if (!SDDS_Terminate(&presetData))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return;
}

void initializeControlBits(CONTROL_BITS *controlBit) {
  controlBit->arrayLength = 0;
  controlBit->commutationNegate = controlBit->commutationSwitch = controlBit->turnMarker = controlBit->P0Marker = controlBit->wrapMarker = controlBit->saveSample = controlBit->selfTest = NULL;
  controlBit->planeSwitch = controlBit->sample = controlBit->accumulator = NULL;
}

void allocateControlBits(CONTROL_BITS *controlBit, long length, short type) {
  long i;

  if (length <= 0)
    return;
  controlBit->arrayLength = length;
  /*4 receivers differ by plane, accumulator and sample bits */
  controlBit->planeSwitch = (short **)malloc(sizeof(*controlBit->planeSwitch) * 4);
  controlBit->accumulator = (short **)malloc(sizeof(*controlBit->accumulator) * 4);
  controlBit->sample = (short **)malloc(sizeof(*controlBit->sample) * 4);
  for (i = 0; i < 4; i++) {
    controlBit->planeSwitch[i] = (short *)calloc(sizeof(**controlBit->planeSwitch), length);
    controlBit->accumulator[i] = (short *)calloc(sizeof(**controlBit->accumulator), length);
    controlBit->sample[i] = (short *)calloc(sizeof(**controlBit->sample), length);
  }
  controlBit->commutationNegate = (short *)calloc(sizeof(*controlBit->commutationNegate), length);
  controlBit->commutationSwitch = (short *)calloc(sizeof(*controlBit->commutationSwitch), length);
  controlBit->turnMarker = (short *)calloc(sizeof(*controlBit->turnMarker), length);
  controlBit->wrapMarker = (short *)calloc(sizeof(*controlBit->wrapMarker), length);

  if (type == SCOPE_RECORD) {
    controlBit->P0Marker = (short *)calloc(sizeof(*controlBit->P0Marker), length);
    /* scope does not these three bits */
    controlBit->saveSample = controlBit->selfTest = controlBit->accumulator[0];
  } else {
    controlBit->P0Marker = controlBit->turnMarker;
    controlBit->saveSample = (short *)calloc(sizeof(*controlBit->saveSample), length);
    controlBit->selfTest = (short *)calloc(sizeof(*controlBit->selfTest), length);
    for (i = 0; i < length; i++)
      controlBit->saveSample[i] = 1;
  }
}

void freeControlBits(CONTROL_BITS *controlBit, short type) {
  long i;
  if (controlBit->arrayLength) {
    for (i = 0; i < 4; i++) {
      free(controlBit->sample[i]);
      free(controlBit->planeSwitch[i]);
      free(controlBit->accumulator[i]);
    }
    free(controlBit->planeSwitch);
    free(controlBit->commutationNegate);
    free(controlBit->commutationSwitch);
    free(controlBit->turnMarker);
    free(controlBit->sample);
    free(controlBit->wrapMarker);
    if (type == SCOPE_RECORD)
      free(controlBit->P0Marker);
    else {
      free(controlBit->saveSample);
      free(controlBit->selfTest);
    }
  }
}

void GenerateControlBitsFromWaveform(uint32_t *waveform, long length, CONTROL_BITS *controlBits,
                                     short type, long *turnsPerWrap, long *turnMarkerOffset) {
  long i, turns = 0, markerOffset = 0, RAMWrapIndex = -1, j;
  /*sample, plane and accumulator bits differ by receiver, j is the receiver here */
  for (j = 0; j < 4; j++) {
    for (i = 0; i < length; i++) {
      switch (type) {
      case RAM_RECORD:
        switch (j) {
        case 0:
          controlBits->planeSwitch[j][i] = (waveform[i] & RAM_PLANE_SWITCH_FLAG0) / RAM_PLANE_SWITCH_FLAG0;
          controlBits->accumulator[j][i] = (waveform[i] & RAM_ACCUMULATOR_FLAG0) / RAM_ACCUMULATOR_FLAG0;
          controlBits->sample[j][i] = (waveform[i] & RAM_SAMPLE_FLAG0) / RAM_SAMPLE_FLAG0;
          break;
        case 1:
          controlBits->planeSwitch[j][i] = (waveform[i] & RAM_PLANE_SWITCH_FLAG1) / RAM_PLANE_SWITCH_FLAG1;
          controlBits->accumulator[j][i] = (waveform[i] & RAM_ACCUMULATOR_FLAG1) / RAM_ACCUMULATOR_FLAG1;
          controlBits->sample[j][i] = (waveform[i] & RAM_SAMPLE_FLAG1) / RAM_SAMPLE_FLAG1;
          break;
        case 2:
          controlBits->planeSwitch[j][i] = (waveform[i] & RAM_PLANE_SWITCH_FLAG2) / RAM_PLANE_SWITCH_FLAG2;
          controlBits->accumulator[j][i] = (waveform[i] & RAM_ACCUMULATOR_FLAG2) / RAM_ACCUMULATOR_FLAG2;
          controlBits->sample[j][i] = (waveform[i] & RAM_SAMPLE_FLAG2) / RAM_SAMPLE_FLAG2;
          break;
        case 3:
          controlBits->planeSwitch[j][i] = (waveform[i] & RAM_PLANE_SWITCH_FLAG3) / RAM_PLANE_SWITCH_FLAG3;
          controlBits->accumulator[j][i] = (waveform[i] & RAM_ACCUMULATOR_FLAG3) / RAM_ACCUMULATOR_FLAG3;
          controlBits->sample[j][i] = (waveform[i] & RAM_SAMPLE_FLAG3) / RAM_SAMPLE_FLAG3;
          break;
        }
        break;
      case SCOPE_RECORD:
        switch (j) {
        case 0:
          controlBits->planeSwitch[j][i] = (waveform[i] & SCOPE_PLANE_SWITCH_FLAG0) / SCOPE_PLANE_SWITCH_FLAG0;
          controlBits->accumulator[j][i] = (waveform[i] & SCOPE_ACCUMULATOR_FLAG0) / SCOPE_ACCUMULATOR_FLAG0;
          controlBits->sample[j][i] = (waveform[i] & SCOPE_SAMPLE_FLAG0) / SCOPE_SAMPLE_FLAG0;
          break;
        case 1:
          controlBits->planeSwitch[j][i] = (waveform[i] & SCOPE_PLANE_SWITCH_FLAG1) / SCOPE_PLANE_SWITCH_FLAG1;
          controlBits->accumulator[j][i] = (waveform[i] & SCOPE_ACCUMULATOR_FLAG1) / SCOPE_ACCUMULATOR_FLAG1;
          controlBits->sample[j][i] = (waveform[i] & SCOPE_SAMPLE_FLAG1) / SCOPE_SAMPLE_FLAG1;
          break;
        case 2:
          controlBits->planeSwitch[j][i] = (waveform[i] & SCOPE_PLANE_SWITCH_FLAG2) / SCOPE_PLANE_SWITCH_FLAG2;
          controlBits->accumulator[j][i] = (waveform[i] & SCOPE_ACCUMULATOR_FLAG2) / SCOPE_ACCUMULATOR_FLAG2;
          controlBits->sample[j][i] = (waveform[i] & SCOPE_SAMPLE_FLAG2) / SCOPE_SAMPLE_FLAG2;
          break;
        case 3:
          controlBits->planeSwitch[j][i] = (waveform[i] & SCOPE_PLANE_SWITCH_FLAG3) / SCOPE_PLANE_SWITCH_FLAG3;
          controlBits->accumulator[j][i] = (waveform[i] & SCOPE_ACCUMULATOR_FLAG3) / SCOPE_ACCUMULATOR_FLAG3;
          controlBits->sample[j][i] = (waveform[i] & SCOPE_SAMPLE_FLAG3) / SCOPE_SAMPLE_FLAG3;
          break;
        }
        break;
      }
    }
  }
  for (i = 0; i < length; i++) {
    switch (type) {
    case RAM_RECORD:
      /* this converts RAM to control bits. The control RAM has flags for all receivers
             but the control bits file has flags for one receiver.
             Thus the variable receiver selects one receiver. */
      controlBits->commutationNegate[i] = (waveform[i] & RAM_COMMUTATION_NEGATE_FLAG) / RAM_COMMUTATION_NEGATE_FLAG;
      controlBits->commutationSwitch[i] = (waveform[i] & RAM_COMMUTATION_SWITCH_FLAG) / RAM_COMMUTATION_SWITCH_FLAG;
      controlBits->turnMarker[i] = (waveform[i] & RAM_TURN_MARKER_FLAG) / RAM_TURN_MARKER_FLAG;
      controlBits->wrapMarker[i] = (waveform[i] & RAM_WRAP_MARKER_FLAG) / RAM_WRAP_MARKER_FLAG;
      controlBits->saveSample[i] = (waveform[i] & RAM_SAVE_SAMPLE_FLAG) / RAM_SAVE_SAMPLE_FLAG;
      controlBits->selfTest[i] = (waveform[i] & RAM_SELF_TEST_FLAG) / RAM_SELF_TEST_FLAG;

      if (controlBits->turnMarker[i] && RAMWrapIndex < 0) {
        /*  RAMWrapIndex<0 means wrap marker not found yet */
        if (turns < 1)
          markerOffset = i;
        turns++;
      }
      if (controlBits->wrapMarker[i] && RAMWrapIndex < 0) {
        RAMWrapIndex = i;
      }
      break;
    case SCOPE_RECORD:
      controlBits->commutationNegate[i] = (waveform[i] & SCOPE_COMMUTATION_NEGATE_FLAG) / SCOPE_COMMUTATION_NEGATE_FLAG;
      controlBits->commutationSwitch[i] = (waveform[i] & SCOPE_COMMUTATION_SWITCH_FLAG) / SCOPE_COMMUTATION_SWITCH_FLAG;
      break;
    default:
      SDDS_Bomb("Unknown type for generating control bits.");
      break;
    }
  }
  if (type == RAM_RECORD) {
    if (markerOffset < 323)
      *turnMarkerOffset = markerOffset;
    else
      *turnMarkerOffset = 0;
    *turnsPerWrap = turns;
    /* P0 marker is made the same as turn marker in the RAM contents, ie. no distinction */
    controlBits->P0Marker = controlBits->turnMarker;
  }
}

void SetupOutputFile(SDDS_DATASET *SDDSout, char *outputFile) {
  if (!SDDS_InitializeOutput(SDDSout, SDDS_BINARY, 0, NULL, NULL, outputFile))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_DefineSimpleParameter(SDDSout, "Comment", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "Description", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "RecordType", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "PlaneMode0", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "PlaneMode1", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "PlaneMode2", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "PlaneMode3", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "AccumulatorMode0", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "AccumulatorMode1", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "AccumulatorMode2", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "AccumulatorMode3", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "CommutationMode", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SampleMode0", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SampleMode1", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SampleMode2", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SampleMode3", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "BunchPattern0", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "BunchPattern1", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "BunchPattern2", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "BunchPattern3", NULL, SDDS_STRING) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SamplesPerBunch0", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SamplesPerBunch1", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SamplesPerBunch2", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "SamplesPerBunch3", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "TurnMarkerOffset", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "TurnMarkerInterval", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "TransitionDeadTime", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "ArrayLength", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleParameter(SDDSout, "TurnsPerWrap", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Index", NULL, SDDS_LONG) ||
      !SDDS_DefineSimpleColumn(SDDSout, "PlaneSwitch0", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "PlaneSwitch1", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "PlaneSwitch2", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "PlaneSwitch3", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Accumulator0", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Accumulator1", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Accumulator2", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Accumulator3", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Sample0", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Sample1", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Sample2", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "Sample3", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "CommutationNegate", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "CommutationSwitch", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "TurnMarker", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "WrapMarker", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "SaveSample", NULL, SDDS_SHORT) ||
      !SDDS_DefineSimpleColumn(SDDSout, "SelfTest", NULL, SDDS_SHORT))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WriteLayout(SDDSout))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
}

void WriteToOutputFile(SDDS_DATASET *SDDSout, CONTROL_BITS *controlBits, short type,
                       long *planeMode, long *accumulatorMode, long commutationMode, long *sampleMode,
                       char **bunchPattern, long *samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                       long transitionDeadTime, long turnsPerWrap, char *comment, long ramInput) {
  int32_t i, *index;
  char tmpBuffer[128];
  index = (int32_t *)malloc(sizeof(*index) * controlBits->arrayLength);
  for (i = 0; i < controlBits->arrayLength; i++)
    index[i] = i;
  if (!SDDS_StartPage(SDDSout, controlBits->arrayLength))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (ramInput) {
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "Description",
                            "Control bits are obtained from waveform pv or input RAM file, the planeMode, commutationMode etc. are not available.", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "Description",
                            "Control bits are generated from preset patterns", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  for (i = 0; i < 4; i++) {
    sprintf(tmpBuffer, "BunchPattern%d", i);
    if (bunchPattern[i]) {
      if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpBuffer, bunchPattern[i], NULL))
        SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    } else if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, tmpBuffer, "", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }
  if (planeMode[0] == -1) {
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "PlaneMode0", "",
                            "PlaneMode1", "",
                            "PlaneMode2", "",
                            "PlaneMode3", "",
                            "AccumulatorMode0", "",
                            "AccumulatorMode1", "",
                            "AccumulatorMode2", "",
                            "AccumulatorMode3", "",
                            "CommutationMode", "",
                            "SampleMode0", "",
                            "SampleMode1", "",
                            "SampleMode2", "",
                            "SampleMode3", "", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  } else {
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "PlaneMode0", planeModeList[planeMode[0]],
                            "PlaneMode1", planeModeList[planeMode[1]],
                            "PlaneMode2", planeModeList[planeMode[2]],
                            "PlaneMode3", planeModeList[planeMode[3]],
                            "AccumulatorMode0", planeModeList[accumulatorMode[0]],
                            "AccumulatorMode1", planeModeList[accumulatorMode[1]],
                            "AccumulatorMode2", planeModeList[accumulatorMode[2]],
                            "AccumulatorMode3", planeModeList[accumulatorMode[3]],
                            "CommutationMode", commutationModeList[commutationMode],
                            "SampleMode0", sampleModeList[sampleMode[0]],
                            "SampleMode1", sampleModeList[sampleMode[1]],
                            "SampleMode2", sampleModeList[sampleMode[2]],
                            "SampleMode3", sampleModeList[sampleMode[3]], NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  }

  switch (type) {
  case RAM_RECORD:
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "RecordType", "RAM", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  case SCOPE_RECORD:
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "RecordType", "scope", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  default:
    if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                            "RecordType", "", NULL))
      SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    break;
  }
  if (!SDDS_SetParameters(SDDSout, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE,
                          "SamplesPerBunch0", samplesPerBunch[0],
                          "SamplesPerBunch1", samplesPerBunch[1],
                          "SamplesPerBunch2", samplesPerBunch[2],
                          "SamplesPerBunch3", samplesPerBunch[3],
                          "TurnMarkerOffset", turnMarkerOffset,
                          "TurnMarkerInterval", turnMarkerInterval,
                          "TransitionDeadTime", transitionDeadTime,
                          "ArrayLength", controlBits->arrayLength,
                          "Comment", comment,
                          "TurnsPerWrap", turnsPerWrap, NULL))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if (!SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, index, controlBits->arrayLength, "Index") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->planeSwitch[0], controlBits->arrayLength, "PlaneSwitch0") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->planeSwitch[1], controlBits->arrayLength, "PlaneSwitch1") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->planeSwitch[2], controlBits->arrayLength, "PlaneSwitch2") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->planeSwitch[3], controlBits->arrayLength, "PlaneSwitch3") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->accumulator[0], controlBits->arrayLength, "Accumulator0") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->accumulator[1], controlBits->arrayLength, "Accumulator1") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->accumulator[2], controlBits->arrayLength, "Accumulator2") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->accumulator[3], controlBits->arrayLength, "Accumulator3") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->sample[0], controlBits->arrayLength, "Sample0") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->sample[1], controlBits->arrayLength, "Sample1") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->sample[2], controlBits->arrayLength, "Sample2") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->sample[3], controlBits->arrayLength, "Sample3") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->commutationNegate, controlBits->arrayLength, "CommutationNegate") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->commutationSwitch, controlBits->arrayLength, "CommutationSwitch") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->turnMarker, controlBits->arrayLength, "TurnMarker") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->wrapMarker, controlBits->arrayLength, "WrapMarker") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->saveSample, controlBits->arrayLength, "SaveSample") ||
      !SDDS_SetColumn(SDDSout, SDDS_SET_BY_NAME, controlBits->selfTest, controlBits->arrayLength, "SelfTest"))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_WritePage(SDDSout))
    SDDS_PrintErrors(stdout, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  free(index);
}

/* This simulates what the FPGA scope would produce and return if the
   ram contents were loaded and the scope were armed. */
void GenerateScopeControlBitsFromRAM(CONTROL_BITS *RAM, CONTROL_BITS *scope, long scopeTriggerIndexOffset,
                                     long *turnsPerWrap, long *turnMarkerOffset, long turnMarkerInterval) {
  long RAMWrapIndex = -1, i, offset, period, index, markerOffset = 0, lastP0Index, j;
  /* The P0 timing slot is 2052 in scope, which corresponding to the wrap position of RAM*/
  /* the scope wrap marker is at 2048, 2048 + scopeTriggerIndexOffset is the P0 time slot */
  RAMWrapIndex = *turnsPerWrap * turnMarkerInterval - 2;
  /* offset = 2048 + scopeTriggerIndexOffset - RAMWrapIndex;*/
  offset = scopeTriggerIndexOffset;
  if (*turnMarkerOffset > 0)
    markerOffset = *turnMarkerOffset % turnMarkerInterval + 1;
  else if (*turnMarkerOffset < 0)
    markerOffset = turnMarkerInterval + *turnMarkerOffset % turnMarkerInterval + 1;
  /*  offset = 2048 + offset - RAMWrapIndex;*/
  period = RAMWrapIndex + 2;
  if (offset == 0)
    offset = 1;
  for (j = 0; j < 4; j++) {
    for (i = 0; i < scope->arrayLength; i++) {
      index = (offset + i - 1) % period;
      scope->planeSwitch[j][i] = RAM->planeSwitch[j][index];
      scope->accumulator[j][i] = RAM->accumulator[j][index];
      scope->sample[j][i] = RAM->sample[j][index];
    }
  }
  for (i = 0; i < scope->arrayLength; i++) {
    index = (offset + i - 1) % period;
    scope->commutationNegate[i] = RAM->commutationNegate[index];
    scope->commutationSwitch[i] = RAM->commutationSwitch[index];
    scope->turnMarker[i] = RAM->turnMarker[index];
    if (i < 2048)
      scope->wrapMarker[i] = 0;
    else
      scope->wrapMarker[i] = 1;
  }
  lastP0Index = 0;
  for (i = markerOffset + 1; i < scope->arrayLength; i++) {
    if (scope->turnMarker[i]) {
      scope->P0Marker[i - markerOffset - 1] = 1;
      lastP0Index = i - markerOffset - 1;
    }
  }
  if (markerOffset) {
    for (i = scope->arrayLength - 1 - markerOffset; i < scope->arrayLength; i++) {
      if ((i - lastP0Index) % turnMarkerInterval == 0)
        scope->P0Marker[i] = 1;
    }
  }
}

/* This attempts a reconstruction of RAM contents based on
   a scope waveform. The scope data is 16 bits while the RAM data is 32 bits,
   so there would or could be some missing data. */
void GenerateRAMControlBitsFromScope(CONTROL_BITS *scope, CONTROL_BITS *RAM, long scopeTriggerIndexOffset,
                                     long *turnsPerWrap, long *turnMarkerOffset, long turnMarkerInterval) {
  long RAMWrapIndex, i, offset, period, index, turns = 0, turnIndex = 0, j;
  /*the P0 timing slot is 2052 in scope, which corresponding to the wrap position of RAM*/
  RAMWrapIndex = *turnsPerWrap * turnMarkerInterval - 2;
  period = RAMWrapIndex + 2;
  /*valid scope time slots starts from 2048, the wrap marker is at 2048 + P0 Offset,
    the valid period starts from 2048+scopeTriggerIndexOffset +2 since wraperIndex = period-2 */
  /* while RAM control ram period starts from 0 */
  offset = 2048 + scopeTriggerIndexOffset + 2;
  for (i = 0; i < RAM->arrayLength; i++) {
    index = i % period + offset;
    RAM->commutationNegate[i] = scope->commutationNegate[index];
    RAM->commutationSwitch[i] = scope->commutationSwitch[index];
    RAM->turnMarker[i] = scope->turnMarker[index];
    if (i && (period - i % period) == 2)
      RAM->wrapMarker[i] = 1;
    else
      RAM->wrapMarker[i] = 0;
    if (RAM->turnMarker[i] && i < RAMWrapIndex) {
      if (turns < 1)
        turnIndex = i;
      turns++;
    }
  }
  for (j = 0; j < 4; j++) {
    for (i = 0; i < RAM->arrayLength; i++) {
      index = i % period + offset;
      RAM->planeSwitch[j][i] = scope->planeSwitch[j][index];
      RAM->sample[j][i] = scope->sample[j][index];
      RAM->accumulator[j][i] = scope->accumulator[j][index];
    }
  }
  if (turnIndex < turnMarkerInterval - 1)
    *turnMarkerOffset = turnIndex;
  else
    *turnMarkerOffset = 0;
}

void GenerateReceiverBitsFromPreset(long planeMode, long accumulatorMode, long commutationMode, long sampleMode,
                                    long samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                                    long transitionDeadTime,
                                    char *bunchPattern, char *bunchFile,
                                    long RAMArrayLength, short *planeSwitch, short *accumulator, short *sampleSwitch) {
  long i, j, deadTimeSlots, multiplicity, multiplets, multipletStart, multipletSpacing, count, hybridMode, bpmGroupBuckets, bucket, offset, firstSampleMarker, buckets;
  /* long emptyBuckets; */
  short bit1, bit11, bit12, bit2, bit21, bit22, *sampleMaskP = NULL, *sampleMaskC = NULL, *sample = NULL, *bucketList = NULL;

  deadTimeSlots = (int)(transitionDeadTime);
  sampleMaskP = calloc(sizeof(*sampleMaskP), RAMArrayLength);
  sampleMaskC = calloc(sizeof(*sampleMaskC), RAMArrayLength);
  sample = calloc(sizeof(*sample), RAMArrayLength);
  for (i = 0; i < RAMArrayLength; i++) {
    bit1 = ((int)MODULO(i + deadTimeSlots - turnMarkerOffset, 2 * turnMarkerInterval) / turnMarkerInterval);
    bit11 = ((int)MODULO(i + deadTimeSlots, 2 * turnMarkerInterval) / turnMarkerInterval);
    bit12 = ((int)(i / turnMarkerInterval)) % 2;
    bit2 = ((int)MODULO(i + deadTimeSlots - turnMarkerOffset, 4 * turnMarkerInterval) / turnMarkerInterval);
    bit21 = ((int)MODULO(i + deadTimeSlots, 4 * turnMarkerInterval) / (2 * turnMarkerInterval));
    bit22 = ((int)(i / (2 * turnMarkerInterval))) % 2;
    switch (planeMode) {
    case 0:
      /* x plane, originally this bit is 1, but changed to 0 to be consistent with Eric's FPGA firmwave*/
      if (planeSwitch)
        planeSwitch[i] = 0;
      sampleMaskP[i] = 1;
      break;
    case 1:
      /* y plane, originally this bit is 0, but changed to 1 to be consistent with Eric's FPGA firmwave */
      if (planeSwitch)
        planeSwitch[i] = 1;
      sampleMaskP[i] = 1;
      break;
    case 2:
      /* xy1 */
      if (planeSwitch)
        planeSwitch[i] = bit1;
      if (bit11 == bit12)
        sampleMaskP[i] = 1;
      break;
    case 3:
      /* xy2 */
      if (planeSwitch)
        planeSwitch[i] = bit2;
      if (bit21 == bit22)
        sampleMaskP[i] = 1;
      break;
    default:
      SDDS_Bomb("invalid plane mode provided.");
      break;
    }
    if (accumulator) {
      switch (accumulatorMode) {
      case 0:
        /* x plane, originally this bit is 1, but changed to 0 to be consistent with Eric's FPGA firmwave*/
        accumulator[i] = 0;
        break;
      case 1:
        /* y plane, originally this bit is 0, but changed to 1 to be consistent with Eric's FPGA firmwave */
        accumulator[i] = 1;
        break;
      case 2:
        /* xy1 */
        accumulator[i] = bit1;
        break;
      case 3:
        /* xy2 */
        accumulator[i] = bit2;
        break;
      default:
        SDDS_Bomb("invalid plane mode provided.");
        break;
      }
    }
    if (sampleSwitch) {
      switch (commutationMode) {
      case 0:
        /* no commutation, no need to set commuationSwitch since it was initialized to 0 */
        sampleMaskC[i] = 1;
        break;
      case 1:
        sampleMaskC[i] = 1;
        break;
      case 2:
        /* ab1 */
        if (bit11 == bit12)
          sampleMaskC[i] = 1;
        break;
      case 3:
        /* ab2 */
        if (bit21 == bit22)
          sampleMaskC[i] = 1;
        break;
      default:
        SDDS_Bomb("Invalid commutation mode provided.");
        break;
      }
      if (samplesPerBunch) {
        switch (sampleMode) {
        case 0:
          /* continuous */
          if (sampleMaskP[i] && sampleMaskC[i])
            sample[i] = 1;
          break;
        case 1:
          /* single mode: sample at each turn */
          /* for single trigger we assume we don't have to apply sampleMakeP or sampleMaskC
                     so we ignore them */
          sample[i] = (i % turnMarkerInterval) ? 0 : 1;
          break;
        case 2:
          /*pattern */
          if (!bunchPattern || !bunchFile)
            SDDS_Bomb("Bunch pattern or file is not provided from prest pattern sample mode.");
          /* will process separately */
          break;
        default:
          SDDS_Bomb("Invalid sample mode provided.");
          break;
        }
      }
    }
  }
  if (!sampleSwitch) {
    free(sample);
    sample = NULL;
    free(sampleMaskP);
    sampleMaskP = NULL;
    free(sampleMaskC);
    sampleMaskC = NULL;
    return;
  }
  if (sampleMode == SAMPLE_BUNCHPATTERN) {
    /* bunch pattern sample*/
    hybridMode = 0;
    ObtainInformationFromBunchPatternTable(bunchFile, bunchPattern,
                                           &multiplicity, &multiplets,
                                           &multipletStart, &multipletSpacing,
                                           &hybridMode, &bpmGroupBuckets);

    /* emptyBuckets = multipletSpacing - multiplicity; */
    /* this test will fail if we have a bunch pattern that is not symmetric */
    /* so we remove it.
         if (buckets != 4 * turnMarkerInterval)
         SDDS_Bomb("Something wrong with bucket parameters, the total number of buckets is not 1296!");
      */
    buckets = 4 * turnMarkerInterval;
    bucketList = calloc(sizeof(*bucketList), (buckets + multipletStart + multipletSpacing));
    if (hybridMode) {
      /* bpmGroupBuckets is only used with hybrid mode */
      for (j = 0; j < bpmGroupBuckets; j++)
        bucketList[j] = 1;
    }

    /* count = multipletStart;
         for (i = 0; i < multiplets; i++) {
         for (j=0; j<multiplicity; j++)
         bucketList[j + count] = 1;
         count += multiplicity + emptyBuckets ;
         } */
    /*the above method only works for 324 turn marker interval, when the turn marker interval is greater,
        it will not sample the buckets that are beyond 1296.
      */
    count = multipletStart;
    for (i = 0; i < buckets; i++) {
      if (i % multipletSpacing == 0) {
        for (j = 0; j < multiplicity; j++)
          bucketList[j + i + count] = 1;
      }
    }
    /* if one of a group of four bucket has a bunch, then sample */
    count = 0;
    for (bucket = 0; bucket < buckets; bucket += 4) {
      if (bucketList[bucket] || bucketList[bucket + 1] ||
          bucketList[bucket + 2] || bucketList[bucket + 3]) {
        sample[count] = 1;
      }
      count++;
    }
    /* count value now should be 324 */
    /* duplicate the above sample by 324 interval */
    for (i = turnMarkerInterval; i < RAMArrayLength; i++) {
      sample[i] = sample[i % turnMarkerInterval];
      /* Apply deadtime filter before turnMarker offset. 
             Do not sample within some time of the transition,
             if there is a transition */
    }
    for (i = 0; i < RAMArrayLength; i++) {
      if (!sampleMaskP[i] || !sampleMaskC[i])
        sample[i] = 0;
    }
    free(bucketList);
    bucketList = NULL;
  }
  free(sampleMaskP);
  sampleMaskP = NULL;
  free(sampleMaskC);
  sampleMaskC = NULL;
  offset = 0;
  if (turnMarkerOffset > 0)
    /* shift by turnMarkerOffset */
    offset = turnMarkerOffset % turnMarkerInterval;
  else if (turnMarkerOffset < 0)
    /* shift by RAMArrayLength - turnMarkerOffset */
    offset = turnMarkerInterval + turnMarkerOffset % turnMarkerInterval;
  if (offset) {
    for (i = 0; i < RAMArrayLength - offset; i++)
      sampleSwitch[i + offset] = sample[i];
    for (i = 0; i < offset; i++)
      sampleSwitch[i] = sample[(RAMArrayLength - offset) + i];
  } else {
    memcpy(sampleSwitch, sample, sizeof(*sample) * RAMArrayLength);
  }

  free(sample);
  sample = NULL;

  /* Extend sampling using samplesPerBunch */
  /* should be used only for single bunchm, and bunch pattern */
  /* For continuous mode, the samples should stop at the transition
     of the plane and commutation switches. */

  if (sampleMode == SAMPLE_BUNCHPATTERN || sampleMode == SAMPLE_SINGLE) {
    firstSampleMarker = -1;
    for (i = 0; i < RAMArrayLength; i++) {
      if (sampleSwitch[i]) {
        if (firstSampleMarker < 0)
          firstSampleMarker = i;
        for (j = 1; j < samplesPerBunch; j++) {
          if (i >= RAMArrayLength - 1)
            sampleSwitch[++i - RAMArrayLength] = 1;
          else
            sampleSwitch[++i] = 1;
        }
      }
    }
  }
  /* remove sample points before the turn marker offset, so that turn marker offset should be smaller than the turn marker interval (turnMarkerInterval) */
  if (turnMarkerInterval != 324 && sampleMode == SAMPLE_SINGLE) {
    for (i = 0; i < turnMarkerOffset; i++)
      sampleSwitch[i] = 0;
  }
}

void GenerateControlBitsFromPreset(long *planeMode, long *accumulatorMode, long commutationMode, long *sampleMode,
                                   long *samplesPerBunch, long turnMarkerOffset, long turnMarkerInterval,
                                   long transitionDeadTime,
                                   char **bunchPattern, char *bunchFile,
                                   long RAMArrayLength, CONTROL_BITS *RAM,
                                   long scopeArrayLength, CONTROL_BITS *scope,
                                   long *turnsPerWrap, long scopeTriggerIndexOffset) {
  long i, j, deadTimeSlots, sameAs;
  short bit1, bit2;
  /* short bit11, bit12, bit21, bit22; */

  /* if (commutationMode==3)
   *turnsPerWrap = 4;
   else if (commutationMode==2)
   *turnsPerWrap = 2;
   else 
   *turnsPerWrap = 4; */

  /* transitionDeadTime used to be a double and with units ns */
  /* Now it has the same meaning as local variable deadTimeslot */
  deadTimeSlots = (int)(transitionDeadTime);
  allocateControlBits(RAM, RAMArrayLength, RAM_RECORD);
  allocateControlBits(scope, scopeArrayLength, SCOPE_RECORD);

  /* generate controlbits for the 4 receivers */
  for (i = 0; i < 4; i++) {
    if (i == 0) {
      GenerateReceiverBitsFromPreset(planeMode[0], accumulatorMode[0], commutationMode, sampleMode[0],
                                     samplesPerBunch[0], turnMarkerOffset, turnMarkerInterval,
                                     transitionDeadTime,
                                     bunchPattern[0],
                                     bunchFile,
                                     RAMArrayLength, RAM->planeSwitch[0], RAM->accumulator[0], RAM->sample[0]);
    } else {
      sameAs = -1;
      for (j = 0; j < i; j++) {
        if (planeMode[i] == planeMode[j] && accumulatorMode[i] == accumulatorMode[j] &&
            sampleMode[i] == sampleMode[j]) {
          if (sampleMode[i] == SAMPLE_BUNCHPATTERN && strcmp(bunchPattern[i], bunchPattern[j]) == 0 && samplesPerBunch[i] == samplesPerBunch[j]) {
            sameAs = j;
            break;
          } else if (sampleMode[i] == SAMPLE_SINGLE && samplesPerBunch[i] == samplesPerBunch[j]) {
            sameAs = j;
            break;
          } else if (sampleMode[i] == SAMPLE_CONTINOUS) {
            sameAs = j;
          }
        }
      }
      if (sameAs >= 0) {
        memcpy(RAM->planeSwitch[i], RAM->planeSwitch[sameAs], sizeof(short) * RAMArrayLength);
        memcpy(RAM->accumulator[i], RAM->accumulator[sameAs], sizeof(short) * RAMArrayLength);
        memcpy(RAM->sample[i], RAM->sample[sameAs], sizeof(short) * RAMArrayLength);
      } else {
        GenerateReceiverBitsFromPreset(planeMode[i], accumulatorMode[i], commutationMode, sampleMode[i],
                                       samplesPerBunch[i], turnMarkerOffset, turnMarkerInterval,
                                       transitionDeadTime,
                                       bunchPattern[i],
                                       bunchFile,
                                       RAMArrayLength, RAM->planeSwitch[i], RAM->accumulator[i], RAM->sample[i]);
      }
    }
  }
  /* generate other common control bits */
  for (i = 0; i < RAMArrayLength; i++) {
    bit1 = ((int)MODULO(i + deadTimeSlots - turnMarkerOffset, 2 * turnMarkerInterval) / turnMarkerInterval);
    /* bit11 = ((int) MODULO( i + deadTimeSlots, 2*turnMarkerInterval ) / turnMarkerInterval); */
    /* bit12 = ((int)(i / turnMarkerInterval)) % 2; */
    bit2 = ((int)MODULO(i + deadTimeSlots - turnMarkerOffset, 4 * turnMarkerInterval) / (2 * turnMarkerInterval));
    /* bit21 = ((int) MODULO( i + deadTimeSlots, 4*turnMarkerInterval ) / (2*turnMarkerInterval)); */
    /* bit22 = ((int)(i / (2*turnMarkerInterval)))%2; */
    /* The FPGA accumulator bit must be the same as the rf section
         plane bit. A delay for the accumulator will have to be included later. */
    /*  RAM->accumulator[i] = RAM->planeSwitch[i] ; */

    switch (commutationMode) {
    case 0:
      /* no commutation, no need to set commuationSwitch since it was initialized to 0 */
      break;
    case 1:
      RAM->commutationSwitch[i] = 1;
      break;
    case 2:
      /* ab1 */
      RAM->commutationSwitch[i] = bit1;
      break;
    case 3:
      /* ab2 */
      RAM->commutationSwitch[i] = bit2;
      break;
    default:
      SDDS_Bomb("Invalid commutation mode provided.");
      break;
    }
    RAM->commutationNegate[i] = RAM->commutationSwitch[i];
  }
  for (i = 0; i < RAMArrayLength; i++) {
    /* wrap maker at end of sequence length -2 */
    if (((i + 2) % (*turnsPerWrap * turnMarkerInterval)) == 0)
      RAM->wrapMarker[i] = 1;

    /* The turn marker (time when accumulator data is sent to orbit 
         averaging stage, i.e. not the same thing as P0 offset) can 
         be at any position as long as they are spaced
         by 324. That time slot can have beam present or not. 
      */

    /* For single bunch, the turn marker should probably
         be sometime after the single bunch for quicker data in
         principle, though a 3.68 usec delay is not critical for
         most application.
         For 24 bunches, the turn marker should probably
         come after the 24th bunch after the P0 marker.
         For continuous beam, the turn marker can be placed
         during the switch dead time so no data additional need be lost.
         For hybrid mode, place the turn marker after the 56 bunches.
         It is best then to configure all of this in the presetfile. */
    if ((i - turnMarkerOffset) % turnMarkerInterval == 0)
      RAM->turnMarker[i] = 1;
  }
  /* generate scope control bits from RAM */
  GenerateScopeControlBitsFromRAM(RAM, scope, scopeTriggerIndexOffset, turnsPerWrap, &turnMarkerOffset, turnMarkerInterval);
  return;
}

void ObtainInformationFromBunchPatternTable(char *bunchFile, char *bunchPattern0,
                                            long *multiplicity, long *multiplets,
                                            long *multipletStart, long *multipletSpacing,
                                            long *hybridMode, long *bpmGroupBuckets) {
  SDDS_DATASET bunchData;
  int32_t rowIndex, rows = 0, *flag = NULL, i;
  char **label = NULL;
  void *data = NULL;
  char **stringData = NULL;

  if (!bunchPattern0 || !bunchFile)
    return;
  if (!SDDS_InitializeInput(&bunchData, bunchFile))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_ReadPage(&bunchData) < 0)
    SDDS_Bomb("Unable to read bunch pattern data.");
  if (!(rows = SDDS_CountRowsOfInterest(&bunchData)))
    SDDS_Bomb("No data in preset file.");
  if (!(label = (char **)SDDS_GetColumn(&bunchData, "Label")))
    SDDS_Bomb("Can not obtain Label from preset data.");
  if ((rowIndex = match_string(bunchPattern0, label, rows, EXACT_MATCH)) < 0) {
    fprintf(stderr, "preset %s is not found in preset file %s.\n", bunchPattern0, bunchFile);
    exit(1);
  }
  for (i = 0; i < rows; i++)
    free(label[i]);
  free(label);
  flag = (int32_t *)calloc(sizeof(*flag), rows);
  flag[rowIndex] = 1;
  if (!SDDS_AssertRowFlags(&bunchData, SDDS_FLAG_ARRAY, flag, (int64_t)rows))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  free(flag);
  if (!(data = SDDS_GetColumn(&bunchData, "Multiplicity")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *multiplicity = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&bunchData, "Multiplets")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *multiplets = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&bunchData, "MultipletStart")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *multipletStart = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&bunchData, "MultipletSpacing")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *multipletSpacing = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(data = SDDS_GetColumn(&bunchData, "BpmGroupBuckets")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  *bpmGroupBuckets = ((int32_t *)data)[0];
  free((int32_t *)data);

  if (!(stringData = SDDS_GetColumn(&bunchData, "BpmGroupMode")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!strcmp(stringData[0], "None"))
    *hybridMode = 0;
  else
    *hybridMode = 1;
  free((char *)stringData[0]);
  free((char **)stringData);

  if (!SDDS_Terminate(&bunchData))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  return;
}

void ReadInputRAM(char *inputRAMFile, long *waveformLength, uint32_t **waveform,
                  char **comment, long *planeMode, long *accumulatorMode,
                  long *commutationMode, long *sampleMode, char **bunchPattern,
                  long *samplesPerBunch, long *turnMarkerOffset,
                  long *transitionDeadTime, long *turnMarkerInterval) {
  SDDS_DATASET ramData;
  long rows = 0, i;
  char tmpBuffer[128];
  char *plane, *commutation, *sample;

  plane = commutation = sample = NULL;
  *waveform = NULL;
  if (!SDDS_InitializeInput(&ramData, inputRAMFile))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_ReadPage(&ramData) < 0)
    SDDS_Bomb("Unable to read RAM input data.");
  if (!(rows = SDDS_CountRowsOfInterest(&ramData)))
    SDDS_Bomb("No data in input RAM file.");
  *waveformLength = rows;
  if (!(*waveform = (uint32_t *)SDDS_GetColumn(&ramData, "Waveform")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  /* These parameters, if present, are read in sfrom the input RAM file
     so that they could be transferred to the output files.
  */
  if (SDDS_CheckParameter(&ramData, "Comment", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK &&
      !SDDS_GetParameter(&ramData, "Comment", comment))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_CheckParameter(&ramData, "PlaneMode", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "PlaneMode", &plane))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    planeMode[0] = planeMode[1] = planeMode[2] = planeMode[3] = match_string(str_tolower(plane), planeModeList, planeModes, EXACT_MATCH);
    free(plane);
  } else if (SDDS_CheckParameter(&ramData, "PlaneMode0", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    for (i = 0; i < 4; i++) {
      sprintf(tmpBuffer, "PlaneMode%ld", i);
      if (!SDDS_GetParameter(&ramData, tmpBuffer, &plane))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      planeMode[i] = match_string(str_tolower(plane), planeModeList, planeModes, EXACT_MATCH);
      free(plane);
    }
  }
  if (SDDS_CheckParameter(&ramData, "TurnMarkerInterval", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "TurnMarkerInterval", turnMarkerInterval))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (SDDS_CheckParameter(&ramData, "AccumulatorMode", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "AccumulatorMode", &plane))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    accumulatorMode[0] = accumulatorMode[1] = accumulatorMode[2] = accumulatorMode[3] = match_string(str_tolower(plane), planeModeList, planeModes, EXACT_MATCH);
    free(plane);
  } else if (SDDS_CheckParameter(&ramData, "AccumulatorMode0", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    for (i = 0; i < 4; i++) {
      sprintf(tmpBuffer, "AccumulatorMode%ld", i);
      if (!SDDS_GetParameter(&ramData, tmpBuffer, &plane))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      accumulatorMode[i] = match_string(str_tolower(plane), planeModeList, planeModes, EXACT_MATCH);
      free(plane);
    }
  }

  if (SDDS_CheckParameter(&ramData, "CommutationMode", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "CommutationMode", &commutation))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    *commutationMode = match_string(str_tolower(commutation), commutationModeList, commutationModes, EXACT_MATCH);
    free(commutation);
  }
  if (SDDS_CheckParameter(&ramData, "SampleMode", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "SampleMode", &sample))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    *sampleMode = match_string(str_tolower(sample), sampleModeList, sampleModes, EXACT_MATCH);
    free(sample);
  } else if (SDDS_CheckParameter(&ramData, "SampleMode0", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    for (i = 0; i < 4; i++) {
      sprintf(tmpBuffer, "SampleMode%ld", i);
      if (!SDDS_GetParameter(&ramData, tmpBuffer, &sample))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      sampleMode[i] = match_string(str_tolower(sample), sampleModeList, sampleModes, EXACT_MATCH);
      free(plane);
    }
  }
  if (SDDS_CheckParameter(&ramData, "BunchPattern", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "BunchPattern", &bunchPattern[0]))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    bunchPattern[1] = bunchPattern[2] = bunchPattern[3] = bunchPattern[0];
  } else if (SDDS_CheckParameter(&ramData, "BunchPattern0", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OK) {
    for (i = 0; i < 4; i++) {
      sprintf(tmpBuffer, "BunchPattern%ld", i);
      if (!SDDS_GetParameter(&ramData, tmpBuffer, &bunchPattern[i]))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }
  if (SDDS_CheckParameter(&ramData, "SamplesPerBunch", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK) {
    if (!SDDS_GetParameter(&ramData, "SamplesPerBunch", &samplesPerBunch[0]))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    samplesPerBunch[1] = samplesPerBunch[2] = samplesPerBunch[3] = samplesPerBunch[0];
  } else if (SDDS_CheckParameter(&ramData, "SamplesPerBunch0", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK) {
    for (i = 0; i < 4; i++) {
      sprintf(tmpBuffer, "SamplesPerBunch%ld", i);
      if (!SDDS_GetParameter(&ramData, tmpBuffer, &samplesPerBunch[i]))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
  }
  if (SDDS_CheckParameter(&ramData, "TurnMarkerOffset", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK &&
      !SDDS_GetParameter(&ramData, "TurnMarkerOffset", turnMarkerOffset))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (SDDS_CheckParameter(&ramData, "TransistionDeadTime", NULL, SDDS_LONG, NULL) == SDDS_CHECK_OK &&
      !SDDS_GetParameter(&ramData, "TransistionDeadTime", transitionDeadTime))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!SDDS_Terminate(&ramData))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
}

void add_second_sample(CONTROL_BITS *record, long secondSampleDelay, long secondSamplesPerBunch, long turnMarkerInterval) {
  long i, j, k;
  long firstSamplePos;

  for (i = 0; i < 4; i++) {
    j = 0;
    firstSamplePos = 0;
    while (j < record->arrayLength) {
      if (record->sample[i][j]) {
        if (!firstSamplePos)
          firstSamplePos = j;
        j += secondSampleDelay;
        for (k = 0; k < secondSamplesPerBunch; k++) {
          if (j < record->arrayLength)
            record->sample[i][j] = 1;
          else
            break;
          j++;
        }
      } else {
        j++;
      }
    }
    /*check if there should be second sample bunch before the first sample*/
    if (firstSamplePos > (turnMarkerInterval - secondSampleDelay - secondSamplesPerBunch)) {
      j = firstSamplePos - (turnMarkerInterval - secondSampleDelay - secondSamplesPerBunch) + 1;
      k = 0;
      while (j >= 0 && k < secondSamplesPerBunch) {
        record->sample[i][j] = 1;
        j--;
        k++;
      }
    }
  }
}
