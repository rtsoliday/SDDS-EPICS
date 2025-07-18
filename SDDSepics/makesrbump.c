/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program: sddswput.c
 * purpose: reads a file and sends out waveform data to PVs
 * input: a file like that generated by sddswget.
 *      Parameters: WaveformPV (string)
 *      Columns: Waveform (float or double)
 * 
 * Michael Borland, 2001.
 */
#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "SDDSepics.h"
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

#ifdef _WIN32
#  include <windows.h>
#  define usleep(usecs) Sleep(usecs / 1000)
#  define sleep(sec) Sleep(sec * 1000)
#else
#  include <unistd.h>
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

#define CLO_PENDIOTIME 0
#define CLO_BPM_LIST 1
#define CLO_DELTA_LIST 2
#define CLO_DRY_RUN 3
#define CLO_VERBOSE 4
#define CLO_WAVEFORM_FILE 5
#define CLO_REFERENCE_MATRIX 6
#define CLO_CORR_PREFIX 7
#define CLO_CORR_SUFFIX 8
#define CLO_BPM_PREFIX 9
#define CLO_BPM_SUFFIX 10
#define CLO_DELTA_LIMIT 11
#define CLO_RAMP_INTERVAL 12
#define CLO_CHANGE_RF 13
#define CLO_TWISS_FILE 14
#define CLO_REFERENCE_FILE 15
#define CLO_REVIEW 16
#define COMMANDLINE_OPTIONS 17

char *commandline_option[COMMANDLINE_OPTIONS] = {"pendiotime", "BPMList", "deltaList", "dryRun", "verbose",
                                                 "waveformFile", "referenceMatrix", "corrPrefix", "corrSuffix",
                                                 "bpmPrefix", "bpmSuffix", "deltaLimit", "rampInterval", "changeRF",
                                                 "twissFile", "referenceFile", "review"};

static char *USAGE1 = "makesrbump <inputfile> <outputfile> \n\
    [-pendIOtime=<seconds>] -bpmList=<list of bpm names> -deltaList=<list of bpm setpoint change>] \n\
    [-dryRun] [-verbose] [-waveformFile=<filename>] [-corrPrefix=<string] [-corrSuffix=<string>] \n\
    [-bpmPrefix=<string>] [-bpmSuffix=<string>] [-deltaLimit=<value>] [-rampInterval=<seconds>] [-review] \n\n\
bpmList            required, list of bpms to change the setpoint (for creating a bump) \n\
deltaList          required, list of bpm setpoint changes \n\
dryRun             if provided in dryRun mode, the bpm or corrector setupoint will not be changed.\n\
<inputFile>        required, inverse matrix file for computing corrector changes from bpm changes.\n\
<outputFile>       optional, if provided, the bpm setpoint changes will be written to output file for review purpose.\n\
waveformFile       if provided, the waveform PV (should be delta waveform) will be updated also while \n\
                   the scalar bpm and correctors are being changed.\n\
referenceMatrix    optional, for computing in-bound bpm setpoint changes from the corrector changes.\n\
corrPrefix         prefix for corrector pv names.\n\
corrSuffix         suffix for corrector pv names.\n\
bpmPrefix          prefix for bpm pv names.\n\
bpmSuffix          suffix for bpm pv names.\n\
deltaLimit         the maximum limit changes allowed. (default is 5Amps for corrector) \n\
rampInterval       the interval between ramp steps if the maximum change is greater than the deltaLimit. (default is 0.5 seconds) \n\
changeRF           if provided, the RF frequency will be changed, this is needed for BM source and x plane steering.\n\
twissFile          provide twissFile for change RF frequency, default is /home/helios/oagData/sr/lattices/default/aps.twi.\n\
referenceFile      the reference value of bpm setpoints, if not provided, it will be read from ioc.\n\
verbose            print out the message \n\
review             if provided, only generate the output file for review purpose, no CA connections.\n\
Program by H. Shang.\n\
Link date: "__DATE__
                      " "__TIME__
                      ", SVN revision: " SVN_VERSION ", " EPICS_VERSION_STRING "\n";

typedef struct
{
  double *waveformValue;
  char *waveformPV;
  long *index;
  int32_t waveformRows;
  chid waveformCID;
  short isCorrector, isDelta;
} WAVEFORM_DATA;

/*matrix is names * m */
typedef struct {
  char **name;
  char **pv_name;
  int32_t names, m;
  double **matrix;
  double *delta, *prevValue, *currentValue;
  chid *channelID;
} CORRBPM_MAT;

#define FPINFO stdout

volatile int sigint = 0;

void init_corr_bpm_mat(CORRBPM_MAT *corr_mat, CORRBPM_MAT *bpm_mat);
void allocate_corr_bpm_mat_memory(CORRBPM_MAT *corr_bpm_mat, int32_t m);
long readwaveformfile(char *waveformFile, long *waveforms, WAVEFORM_DATA **waveformData,
                      CORRBPM_MAT *corr_mat, CORRBPM_MAT *bpm_mat,
                      char *corrPrefix, char *corrSuffix, char *bpmPrefix, char *bpmSuffix);
/*get the corrector matrix for give bpms only, other bpms are zero from irm (inverse matrix) */
long setupCorrectorMatrix(char *filename, CORRBPM_MAT *corr_mat, char **bpm, long bpms, char *corrPrefix, char *corrSuffix);
/*get inbound bpms from correctors and response matrix (RM)*/
long setupBPMmatrix(char *filename, CORRBPM_MAT *bpm_mat, CORRBPM_MAT *corr_mat, char **bpm, long bpms, double *deltaList, char *bpmPrefix, char *bpmSuffix);

void free_waveform_memory(long waveforms, WAVEFORM_DATA *waveformData);
void free_corr_bpm_mat_memory(CORRBPM_MAT *corr_bpm_mat);
void setupChannelAccessConnection(CORRBPM_MAT *bpm_mat, CORRBPM_MAT *corr_mat, long waveforms, WAVEFORM_DATA *waveformData,
                                  char *corrPrefix, char *corrSuffix, char *bpmPrefix, char *bpmSuffix, double pendIOTime, long changeRF, chid *RFchind, double *currentRF);
double computeDelta(CORRBPM_MAT *corr_mat, long bpms, double *deltaList, CORRBPM_MAT *bpm_mat);
double computeRFdelta(CORRBPM_MAT *corr_mat, char *twissFile);

int main(int argc, char **argv) {
  SCANNED_ARG *scArg;
  long i_arg, i, j, verbose = 0, dryRun = 0, review = 0, k, changeRF = 0, rIndex;
  long waveforms = 0, bpms = 0, deltasProvided = 0, steps = 1, step;
  WAVEFORM_DATA *waveformData = NULL;
  char *waveformFile = NULL, **bpmList = NULL, *inputFile = NULL, *outputFile = NULL, *referenceMatrix = NULL, *twissFile = NULL;
  double startTime, *deltaList = NULL, *refValue = NULL;
  char *corrPrefix = NULL, *corrSuffix = NULL, *bpmPrefix = NULL, *bpmSuffix = NULL, *refFile = NULL, **refName = NULL;
  double deltaLimit = 5, rampInterval = 0.5, maxDelta, pendIOTime = 30.0, currentRF, deltaRF = 0, RFvalue;
  CORRBPM_MAT corr_mat, bpm_mat;
  SDDS_DATASET SDDSout, SDDSref;
  chid RFchid;
  int32_t refRows;

  SDDS_RegisterProgramName(argv[0]);

  argc = scanargs(&scArg, argc, argv);
  if (argc == 1) {
    fprintf(stderr, "%s\n", USAGE1);
    exit(1);
  }
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (scArg[i_arg].arg_type == OPTION) {
      delete_chars(scArg[i_arg].list[0], "_");
      switch (match_string(scArg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_PENDIOTIME:
        if (scArg[i_arg].n_items != 2 || sscanf(scArg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          bomb("invalid -pendIOtime syntax\n", NULL);
        break;
      case CLO_DRY_RUN:
        dryRun = 1;
        break;
      case CLO_REVIEW:
        review = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_BPM_LIST:
        bpms = scArg[i_arg].n_items - 1;
        bpmList = (char **)malloc(sizeof(*bpmList) * bpms);
        for (j = 0; j < bpms; j++)
          SDDS_CopyString(&bpmList[j], scArg[i_arg].list[j + 1]);
        break;
      case CLO_DELTA_LIST:
        deltasProvided = scArg[i_arg].n_items - 1;
        deltaList = malloc(sizeof(*deltaList) * deltasProvided);
        for (j = 0; j < deltasProvided; j++) {
          if (!get_double(&deltaList[j], scArg[i_arg].list[j + 1]))
            SDDS_Bomb("Invalid -delta pvalues provided.");
        }
        break;
      case CLO_REFERENCE_FILE:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -referenceFile syntax\n", NULL);
        refFile = scArg[i_arg].list[1];
        break;
      case CLO_WAVEFORM_FILE:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -waveformFile syntax\n", NULL);
        waveformFile = scArg[i_arg].list[1];
        break;
      case CLO_REFERENCE_MATRIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -referenceMatrix syntax\n", NULL);
        referenceMatrix = scArg[i_arg].list[1];
        break;
      case CLO_CORR_PREFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -corrPrefix syntax\n", NULL);
        corrPrefix = scArg[i_arg].list[1];
        break;
      case CLO_CORR_SUFFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -corrSuffix syntax\n", NULL);
        corrSuffix = scArg[i_arg].list[1];
        break;
      case CLO_BPM_PREFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -bpmPrefix syntax\n", NULL);
        bpmPrefix = scArg[i_arg].list[1];
        break;
      case CLO_BPM_SUFFIX:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -bpmSuffix syntax\n", NULL);
        bpmSuffix = scArg[i_arg].list[1];
        break;
      case CLO_DELTA_LIMIT:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -deltaLimit syntax\n", NULL);
        if (!get_double(&deltaLimit, scArg[i_arg].list[1]))
          bomb("invalid -deltaLimit value provided\n", NULL);
        break;
      case CLO_RAMP_INTERVAL:
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -rampInterval syntax\n", NULL);
        if (!get_double(&rampInterval, scArg[i_arg].list[1]))
          bomb("invalid -rampInterval value provided\n", NULL);
        break;
      case CLO_CHANGE_RF:
        /*for BM source and x plane, the RF frequency needs to be changed */
        changeRF = 1;
        break;
      case CLO_TWISS_FILE:
        /*twiss file for computing RF change */
        if (scArg[i_arg].n_items != 2)
          bomb("invalid -twissFile syntax\n", NULL);
        SDDS_CopyString(&twissFile, scArg[i_arg].list[1]);
        break;
      default:
        fprintf(stderr, "unrecognized option given: %s.\n", scArg[i_arg].list[0]);
        exit(1);
      }
    } else {
      if (!inputFile)
        inputFile = scArg[i_arg].list[0];
      else if (!outputFile)
        outputFile = scArg[i_arg].list[0];
      else
        bomb("too many filenames given", NULL);
    }
  }
  startTime = getTimeInSecs();
  if (!inputFile)
    SDDS_Bomb("Error: input file not provided");
  if (bpms <= 0)
    SDDS_Bomb("Error bpmList not provided");
  if (!deltasProvided)
    SDDS_Bomb("Error delta not provided");
  if (changeRF && !twissFile)
    SDDS_CopyString(&twissFile, "/home/helios/oagData/sr/lattices/default/aps.twi");
  if (deltasProvided < bpms) {
    deltaList = SDDS_Realloc(deltaList, sizeof(*deltaList) * bpms);
    for (i = deltasProvided; i < bpms; i++)
      deltaList[i] = deltaList[deltasProvided - 1];
  }
  waveforms = 0;
  ca_task_initialize();

  if (verbose)
    fprintf(stdout, "at %f reading matrix files...\n", getTimeInSecs() - startTime);
  init_corr_bpm_mat(&corr_mat, &bpm_mat);
  if (!setupCorrectorMatrix(inputFile, &corr_mat, bpmList, bpms, corrPrefix, corrSuffix) || !setupBPMmatrix(referenceMatrix, &bpm_mat, &corr_mat, bpmList, bpms, deltaList, bpmPrefix, bpmSuffix))
    exit(1);
  if (waveformFile) {
    if (readwaveformfile(waveformFile, &waveforms, &waveformData, &corr_mat, &bpm_mat,
                         corrPrefix, corrSuffix, bpmPrefix, bpmSuffix)) {
      fprintf(stderr, "Error in reading file/setup CA connections\n");
      free_waveform_memory(waveforms, waveformData);
      exit(1);
    }
  }
  if (verbose)
    fprintf(stdout, "at %f setup CA connection...\n", getTimeInSecs() - startTime);
  if (!review)
    setupChannelAccessConnection(&bpm_mat, &corr_mat, waveforms, waveformData, corrPrefix, corrSuffix, bpmPrefix, bpmSuffix, pendIOTime, changeRF, &RFchid, &currentRF);
  maxDelta = computeDelta(&corr_mat, bpms, deltaList, &bpm_mat);
  if (refFile) {
    /*if reference file provided, the delta changes will base on the reference value */
    if (!SDDS_InitializeInput(&SDDSref, refFile))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (SDDS_ReadPage(&SDDSref) <= 0)
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    refRows = SDDS_RowCount(&SDDSref);
    if (refRows > 0) {
      if (!(refName = (char **)SDDS_GetColumn(&SDDSref, "ControlName")) ||
          !(refValue = (double *)SDDS_GetColumnInDoubles(&SDDSref, "Value")) ||
          !SDDS_Terminate(&SDDSref))
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
      for (i = 0; i < bpm_mat.names; i++) {
        rIndex = match_string(bpm_mat.name[i], refName, refRows, EXACT_MATCH);
        if (rIndex >= 0)
          bpm_mat.prevValue[i] = refValue[rIndex];
      }
      for (i = 0; i < corr_mat.names; i++) {
        rIndex = match_string(corr_mat.name[i], refName, refRows, EXACT_MATCH);
        if (rIndex >= 0)
          corr_mat.prevValue[i] = refValue[rIndex];
      }
      SDDS_FreeStringArray(refName, refRows);
      free(refName);
      free(refValue);
    }
  }
  /* print the initial values  */
  if (verbose) {
    for (i = 0; i < corr_mat.names; i++) {
      fprintf(stdout, "Initial value of corr %s (%s) %lf\n", corr_mat.name[i], corr_mat.pv_name[i], corr_mat.prevValue[i]);
    }
  }
  if (changeRF)
    deltaRF = computeRFdelta(&corr_mat, twissFile);
  steps = 1;

  if (maxDelta > deltaLimit)
    steps = ceil(maxDelta / deltaLimit);

  if (verbose)
    fprintf(stdout, "steps %ld\n", steps);
  if (!review) {
    for (step = 0; step < steps; step++) {
      if (verbose) {
        fprintf(stdout, "step %ld at %f seconds ...\n", step, getTimeInSecs() - startTime);
        /*fprintf(stdout, "corrector values will be ...\n"); */
      }
      for (i = 0; i < corr_mat.names; i++) {
        corr_mat.currentValue[i] = corr_mat.prevValue[i] + (step + 1) * corr_mat.delta[i] / steps;
        if (verbose)
          fprintf(stdout, "%s %f\n", corr_mat.name[i], corr_mat.currentValue[i]);
        if (!dryRun) {
          if (ca_put(DBR_DOUBLE, corr_mat.channelID[i], &corr_mat.currentValue[i]) != ECA_NORMAL) {
            fprintf(stderr, "Error setting value for %s\n", corr_mat.pv_name[i]);
            exit(1);
          }
        }
      }
      if (bpm_mat.names && verbose)
        fprintf(stdout, "\nBPM values will be ...\n");
      for (i = 0; i < bpm_mat.names; i++) {
        bpm_mat.currentValue[i] = bpm_mat.prevValue[i] + (step + 1) * bpm_mat.delta[i] / steps;
        if (verbose)
          fprintf(stdout, "%s %f\n", bpm_mat.name[i], bpm_mat.currentValue[i]);
        if (!dryRun) {
          if (ca_put(DBR_DOUBLE, bpm_mat.channelID[i], &bpm_mat.currentValue[i]) != ECA_NORMAL) {
            fprintf(stderr, "Error setting value for %s\n", bpm_mat.pv_name[i]);
            exit(1);
          }
        }
      }
      /*compute waveform values, note it is delta */
      for (i = 0; i < waveforms; i++) {
        if (waveformData[i].isCorrector == 1) {
          for (j = 0; j < waveformData[i].waveformRows; j++) {
            /*corrector*/
            k = waveformData[i].index[j];
            if (k >= 0) {
              if (waveformData[i].isDelta)
                waveformData[i].waveformValue[j] = corr_mat.delta[k] / steps;
              else
                waveformData[i].waveformValue[j] = corr_mat.currentValue[k];
            }
          }
        } else if (waveformData[i].isCorrector == 0) {
          /*bpm*/
          for (j = 0; j < waveformData[i].waveformRows; j++) {
            k = waveformData[i].index[j];
            if (k >= 0) {
              if (waveformData[i].isDelta)
                waveformData[i].waveformValue[j] = bpm_mat.delta[k] / steps;
              else
                waveformData[i].waveformValue[j] = bpm_mat.currentValue[k];
              ;
            }
          }
        }
        if (waveformData[i].isCorrector != -1) {
          /* isCorrector=-1, means waveform does not match anything (corr or bpm), no need to update*/
          /*   if (verbose)
               fprintf(stdout, "set waveform %s\n",  waveformData[i].waveformPV); */
          if (!dryRun) {
            if (ca_array_put(DBR_DOUBLE, waveformData[i].waveformRows, waveformData[i].waveformCID, waveformData[i].waveformValue) != ECA_NORMAL) {
              fprintf(stderr, "Error setting waveform %s.\n", waveformData[i].waveformPV);
              exit(1);
            }
          }
        }
      }
      if (changeRF && !dryRun) {
        RFvalue = currentRF + deltaRF * (step + 1) / steps;
        if (ca_put(DBR_DOUBLE, RFchid, &RFvalue) != ECA_NORMAL) {
          fprintf(stderr, "Error setting value for BRF:S:Hp8657RefFreqAO\n");
          exit(1);
        }
      }

      if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing put for some channels\n");
        exit(1);
      }
      if (steps > 1)
        oag_ca_pend_event(rampInterval, &sigint);
    }
  }
  if (outputFile) {
    /*write bpm delta to output file */
    if (!SDDS_InitializeOutput(&SDDSout, SDDS_BINARY, 1, NULL, NULL, outputFile) ||
        !SDDS_DefineSimpleColumn(&SDDSout, "BPMName", NULL, SDDS_STRING) ||
        !SDDS_DefineSimpleColumn(&SDDSout, "delta", NULL, SDDS_DOUBLE) ||
        !SDDS_WriteLayout(&SDDSout) ||
        !SDDS_StartPage(&SDDSout, bpm_mat.names) ||
        !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, bpm_mat.name, bpm_mat.names, 0) ||
        !SDDS_SetColumn(&SDDSout, SDDS_SET_BY_INDEX | SDDS_PASS_BY_REFERENCE, bpm_mat.delta, bpm_mat.names, 1) ||
        !SDDS_WritePage(&SDDSout) ||
        !SDDS_Terminate(&SDDSout))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  ca_task_exit();
  free_waveform_memory(waveforms, waveformData);
  free_corr_bpm_mat_memory(&corr_mat);
  free_corr_bpm_mat_memory(&bpm_mat);
  free_scanargs(&scArg, argc);
  SDDS_FreeStringArray(bpmList, bpms);
  free(bpmList);
  if (deltaList)
    free(deltaList);
  if (twissFile)
    free(twissFile);
  if (verbose)
    fprintf(stdout, "at %f seconds done.\n", getTimeInSecs() - startTime);
  return 0;
}

void free_waveform_memory(long waveforms, WAVEFORM_DATA *waveformData) {
  long i;
  for (i = 0; i < waveforms; i++) {
    free(waveformData[i].waveformPV);
    if (waveformData[i].waveformValue)
      free(waveformData[i].waveformValue);
    if (waveformData[i].index)
      free(waveformData[i].index);
  }
  free(waveformData);
  waveformData = NULL;
}

long readwaveformfile(char *inputfile, long *waveforms, WAVEFORM_DATA **waveformData, CORRBPM_MAT *corr_mat, CORRBPM_MAT *bpm_mat,
                      char *corrPrefix, char *corrSuffix, char *bpmPrefix, char *bpmSuffix) {
  long j, pages, index;
  WAVEFORM_DATA *data = NULL;
  SDDS_TABLE SDDSin;
  short deltaWaveformExist = 0, deviceNameExist = 0, controlNameExist = 0, isCorrector = -1;
  char **DeviceName, **ControlName;
  int32_t isDelta = 1; /*set default waveform as delta waveform for SR steering*/

  /*waveform isCorrector 
    = 0 -- belongs to bpm; 
    = 1 belongs to correctors;
    = -1, do not match match any corrector or bpms, no need to update this waveform*/

  DeviceName = ControlName = NULL;

  *waveforms = 0;
  pages = 0;
  *waveformData = NULL;
  if (!SDDS_InitializeInput(&SDDSin, inputfile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  if ((SDDS_CHECK_OKAY != SDDS_CheckParameter(&SDDSin, "WaveformPV", NULL, SDDS_STRING, stderr))) {
    SDDS_SetError("Missing parameter Waveform in input file");
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (SDDS_CheckColumn(&SDDSin, "DeviceName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY)
    deviceNameExist = 1;
  if (SDDS_CheckColumn(&SDDSin, "ControlName", NULL, SDDS_STRING, NULL) == SDDS_CHECK_OKAY)
    controlNameExist = 1;

  if (SDDS_CheckParameter(&SDDSin, "IsDelta", NULL, SDDS_ANY_NUMERIC_TYPE, NULL) == SDDS_CHECK_OKAY)
    deltaWaveformExist = 1;

  if (!deviceNameExist && !controlNameExist) {
    fprintf(stderr, "Error: DeviceName and/or ControlName must exist in the waveform file!\n");
    exit(1);
  }
  pages = 0;
  while (SDDS_ReadPage(&SDDSin) > 0) {
    if (!(data = SDDS_Realloc(data, sizeof(*data) * (pages + 1))))
      SDDS_Bomb("memory allocation failure");
    if ((data[pages].waveformRows = SDDS_RowCount(&SDDSin)) <= 0)
      continue;
    data[pages].waveformPV = NULL;
    data[pages].index = NULL;
    data[pages].waveformValue = NULL;
    data[pages].isCorrector = -1;
    if (!(SDDS_GetParameter(&SDDSin, "WaveformPV", &data[pages].waveformPV)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    data[pages].waveformValue = malloc(sizeof(double) * data[pages].waveformRows);
    data[pages].index = malloc(sizeof(long) * data[pages].waveformRows);
    for (j = 0; j < data[pages].waveformRows; j++) {
      data[pages].index[j] = -1;
      data[pages].waveformValue[j] = 0;
    }
    if (deviceNameExist && !(DeviceName = (char **)SDDS_GetColumn(&SDDSin, "DeviceName")))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (controlNameExist && !(ControlName = (char **)SDDS_GetColumn(&SDDSin, "ControlName")))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (deltaWaveformExist && !SDDS_GetParameterAsLong(&SDDSin, "IsDelta", &isDelta))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    data[pages].isDelta = isDelta;
    isCorrector = -1;
    for (j = 0; j < data[pages].waveformRows; j++) {
      if (DeviceName) {
        index = match_string(DeviceName[j], corr_mat->name, corr_mat->names, EXACT_MATCH);
        if (index >= 0) {
          isCorrector = 1;
          data[pages].index[j] = index;
          data[pages].isCorrector = 1;
        }
      }
    }
    if (isCorrector == -1 && ControlName) {
      for (j = 0; j < data[pages].waveformRows; j++) {
        /*check if ControlName matches corrector names */

        index = match_string(ControlName[j], corr_mat->pv_name, corr_mat->names, EXACT_MATCH);
        if (index >= 0) {
          isCorrector = 1;
          data[pages].index[j] = index;
          data[pages].isCorrector = 1;
        }
      }
    }
    if (isCorrector == -1 && bpm_mat->names) {
      /*check if it has bpm */
      for (j = 0; j < data[pages].waveformRows; j++) {
        if (DeviceName) {
          index = match_string(DeviceName[j], bpm_mat->name, bpm_mat->names, EXACT_MATCH);
          if (index >= 0) {
            isCorrector = 0;
            data[pages].index[j] = index;
            data[pages].isCorrector = 0;
          }
        }
      }
      if (isCorrector == -1 && ControlName) {
        for (j = 0; j < data[pages].waveformRows; j++) {
          index = match_string(ControlName[j], bpm_mat->pv_name, bpm_mat->names, EXACT_MATCH);
          if (index >= 0) {
            isCorrector = 0;
            data[pages].index[j] = index;
            data[pages].isCorrector = 0;
          }
        }
      }
    }
    if (DeviceName) {
      SDDS_FreeStringArray(DeviceName, data[pages].waveformRows);
      free(DeviceName);
      DeviceName = NULL;
    }
    if (ControlName) {
      SDDS_FreeStringArray(ControlName, data[pages].waveformRows);
      free(ControlName);
      ControlName = NULL;
    }
    pages++;
  }
  if (!SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);

  *waveformData = data;
  *waveforms = pages;
  return (0);
}

void init_corr_bpm_mat(CORRBPM_MAT *corr_mat, CORRBPM_MAT *bpm_mat) {
  corr_mat->names = corr_mat->m = 0;
  corr_mat->name = NULL;
  corr_mat->matrix = NULL;
  corr_mat->delta = corr_mat->prevValue = corr_mat->currentValue = NULL;
  corr_mat->channelID = NULL;
  bpm_mat->names = bpm_mat->m = 0;
  bpm_mat->name = NULL;
  bpm_mat->matrix = NULL;
  bpm_mat->delta = bpm_mat->prevValue = bpm_mat->currentValue = NULL;
  bpm_mat->channelID = NULL;
}

void allocate_corr_bpm_mat_memory(CORRBPM_MAT *corr_bpm_mat, int32_t m) {
  long i;
  if (corr_bpm_mat->names <= 0)
    return;
  corr_bpm_mat->matrix = NULL;
  if (m > 0) {
    if (!(corr_bpm_mat->matrix = malloc(sizeof(double *) * corr_bpm_mat->names)))
      SDDS_Bomb("Memory allocation error for matrix");
    for (i = 0; i < corr_bpm_mat->names; i++)
      corr_bpm_mat->matrix[i] = malloc(sizeof(double) * m);
  }
  corr_bpm_mat->m = m;
  corr_bpm_mat->channelID = malloc(sizeof(chid) * corr_bpm_mat->names);
  corr_bpm_mat->prevValue = malloc(sizeof(double) * corr_bpm_mat->names);
  corr_bpm_mat->delta = malloc(sizeof(double) * corr_bpm_mat->names);
  corr_bpm_mat->currentValue = malloc(sizeof(double) * corr_bpm_mat->names);
  corr_bpm_mat->pv_name = malloc(sizeof(char *) * corr_bpm_mat->names);
}

void free_corr_bpm_mat_memory(CORRBPM_MAT *corr_bpm_mat) {
  long i;
  if (corr_bpm_mat->m > 0) {
    for (i = 0; i < corr_bpm_mat->names; i++) {
      free(corr_bpm_mat->name[i]);
      free(corr_bpm_mat->pv_name[i]);
      free(corr_bpm_mat->matrix[i]);
    }
    free(corr_bpm_mat->name);
    free(corr_bpm_mat->matrix);
    free(corr_bpm_mat->pv_name);
  }
  free(corr_bpm_mat->channelID);
  free(corr_bpm_mat->prevValue);
  free(corr_bpm_mat->currentValue);
  free(corr_bpm_mat->delta);
}

long setupCorrectorMatrix(char *filename, CORRBPM_MAT *corr_mat, char **bpm, long bpms, char *corrPrefix, char *corrSuffix) {
  SDDS_TABLE SDDSin;
  double **data = NULL;
  int32_t correctors = 0, i, j;
  char pvname[256];

  if (!SDDS_InitializeInput(&SDDSin, filename))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSin) <= 0) {
    fprintf(stderr, "Error reading irm matrix file %s\n", filename);
    return 0;
  }
  if ((correctors = SDDS_RowCount(&SDDSin)) <= 0) {
    fprintf(stderr, "Error irm matrix file %s is empty \n", filename);
    return 0;
  }
  corr_mat->names = correctors;
  /*allocate memory for corr_mat, there are correctors and bpms bpm */
  allocate_corr_bpm_mat_memory(corr_mat, bpms);
  data = malloc(sizeof(*data) * bpms);
  if (!(corr_mat->name = (char **)SDDS_GetColumn(&SDDSin, "ControlName")))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  for (i = 0; i < correctors; i++) {
    if (corrPrefix && corrSuffix)
      sprintf(pvname, "%s%s%s", corrPrefix, corr_mat->name[i], corrSuffix);
    else if (corrPrefix)
      sprintf(pvname, "%s%s", corrPrefix, corr_mat->name[i]);
    else if (corrSuffix)
      sprintf(pvname, "%s%s", corr_mat->name[i], corrSuffix);
    else
      sprintf(pvname, "%s", corr_mat->name[i]);
    SDDS_CopyString(&corr_mat->pv_name[i], pvname);
  }
  /*only read the matrix for bpm */
  for (i = 0; i < bpms; i++) {
    data[i] = NULL;
    if (!(data[i] = SDDS_GetColumnInDoubles(&SDDSin, bpm[i])))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  /*now transpose to corrector inverse matrix */
  for (i = 0; i < correctors; i++) {
    for (j = 0; j < bpms; j++) {
      corr_mat->matrix[i][j] = data[j][i];
    }
  }
  if (!SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < bpms; i++)
    free(data[i]);
  free(data);
  return 1;
}

long setupBPMmatrix(char *filename, CORRBPM_MAT *bpm_mat, CORRBPM_MAT *corr_mat, char **bpm, long bpms, double *deltaList, char *bpmPrefix, char *bpmSuffix) {
  SDDS_TABLE SDDSin;
  double **data = NULL;
  int32_t i, j;
  char pvname[256];

  if (filename) {
    if (!SDDS_InitializeInput(&SDDSin, filename))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    if (SDDS_ReadPage(&SDDSin) <= 0) {
      fprintf(stderr, "Error reading rm matrix file %s\n", filename);
      return 0;
    }
    if ((bpm_mat->names = SDDS_RowCount(&SDDSin)) <= 0) {
      fprintf(stderr, "Error rm matrix file %s is empty \n", filename);
      return 0;
    }
    if (!(bpm_mat->name = (char **)SDDS_GetColumn(&SDDSin, "BPMName")))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    allocate_corr_bpm_mat_memory(bpm_mat, corr_mat->names);
    for (i = 0; i < bpm_mat->names; i++) {
      if (bpmPrefix && bpmSuffix)
        sprintf(pvname, "%s%s%s", bpmPrefix, bpm_mat->name[i], bpmSuffix);
      else if (bpmPrefix)
        sprintf(pvname, "%s%s", bpmPrefix, bpm_mat->name[i]);
      else if (bpmSuffix)
        sprintf(pvname, "%s%s", bpm_mat->name[i], bpmSuffix);
      else
        sprintf(pvname, "%s", bpm_mat->name[i]);
      SDDS_CopyString(&bpm_mat->pv_name[i], pvname);
    }

    data = malloc(sizeof(*data) * corr_mat->names);
    for (i = 0; i < corr_mat->names; i++) {
      data[i] = NULL;
      if (!(data[i] = SDDS_GetColumnInDoubles(&SDDSin, corr_mat->name[i])))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    /*now transpose to bpm response matrix */
    for (i = 0; i < bpm_mat->names; i++) {
      for (j = 0; j < corr_mat->names; j++)
        bpm_mat->matrix[i][j] = data[j][i];
    }
    if (!SDDS_Terminate(&SDDSin))
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
    for (i = 0; i < corr_mat->names; i++)
      free(data[i]);
    free(data);
  } else {
    /*if no response matrix provided, then do not change anything for bpms;
      only change the correctors (might have bpms) in the input file */
    bpm_mat->names = 0;
    /* no response matrix provided for bpms, so use input bpms */
    /*bpm_mat->names = bpms;
    bpm_mat->name = bpm;
    allocate_corr_bpm_mat_memory(bpm_mat, 0);
    for (i=0; i<bpms; i++) {
      bpm_mat->delta[i] = deltaList[i];
      } */
  }
  return 1;
}

void setupChannelAccessConnection(CORRBPM_MAT *bpm_mat, CORRBPM_MAT *corr_mat, long waveforms, WAVEFORM_DATA *waveformData,
                                  char *corrPrefix, char *corrSuffix, char *bpmPrefix, char *bpmSuffix, double pendIOTime,
                                  long changeRF, chid *RFchid, double *currentRF) {
  long i;

  for (i = 0; i < corr_mat->names; i++) {
    if (ca_search(corr_mat->pv_name[i], &(corr_mat->channelID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", corr_mat->pv_name[i]);
      ca_task_exit();
      exit(1);
    }
  }
  for (i = 0; i < bpm_mat->names; i++) {
    if (ca_search(bpm_mat->pv_name[i], &(bpm_mat->channelID[i])) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", bpm_mat->pv_name[i]);
      ca_task_exit();
      exit(1);
    }
  }
  if (changeRF) {
    if (ca_search("BRF:S:Hp8657RefFreqAO", RFchid) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for BRF:S:Hp8657RefFreqAO=$deltaRF");
      ca_task_exit();
      exit(1);
    }
  }
  for (i = 0; i < waveforms; i++) {
    if (ca_search(waveformData[i].waveformPV, &(waveformData[i].waveformCID)) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", waveformData[i].waveformPV);
      ca_task_exit();
      exit(1);
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem doing search for some channels\n");
    for (i = 0; i < corr_mat->names; i++) {
      if (ca_state(corr_mat->channelID[i]) != cs_conn) {
        fprintf(stderr, "%s not connected\n", corr_mat->pv_name[i]);
      }
    }
    for (i = 0; i < bpm_mat->names; i++) {
      if (ca_state(bpm_mat->channelID[i]) != cs_conn) {
        fprintf(stderr, "%s not connected\n", bpm_mat->pv_name[i]);
      }
    }
    if (changeRF) {
      if (ca_state(*RFchid) != cs_conn)
        fprintf(stderr, "BRF:S:Hp8657RefFreqAO=$deltaRF not connected.\n");
    }
    for (i = 0; i < waveforms; i++) {
      if (ca_state(waveformData[i].waveformCID) != cs_conn)
        fprintf(stderr, "%s not connected\n", waveformData[i].waveformPV);
    }
    exit(1);
  }
  /*read initial value */
  for (i = 0; i < corr_mat->names; i++) {
    if (ca_get(DBR_DOUBLE, corr_mat->channelID[i], &corr_mat->prevValue[i]) != ECA_NORMAL) {
      fprintf(stderr, "Error reading corrector value %s\n", corr_mat->name[i]);
      exit(1);
    }
  }
  for (i = 0; i < bpm_mat->names; i++) {
    if (ca_get(DBR_DOUBLE, bpm_mat->channelID[i], &bpm_mat->prevValue[i]) != ECA_NORMAL) {
      fprintf(stderr, "Error reading bpm value %s\n", bpm_mat->name[i]);
      exit(1);
    }
  }
  if (changeRF) {
    if (ca_get(DBR_DOUBLE, *RFchid, currentRF) != ECA_NORMAL) {
      fprintf(stderr, "Error reading BRF:S:Hp8657RefFreqAO=$deltaRF\n");
      exit(1);
    }
  }
  for (i = 0; i < waveforms; i++) {
    if (waveformData[i].isDelta != 0 && waveformData[i].isCorrector != -1) {
      if (ca_array_get(DBR_DOUBLE, waveformData[i].waveformRows, waveformData[i].waveformCID, waveformData[i].waveformValue) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing reading for %s\n", waveformData[i].waveformPV);
        ca_task_exit();
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error: problem reading some channels\n");
    for (i = 0; i < corr_mat->names; i++) {
      if (ca_state(corr_mat->channelID[i]) != cs_conn) {
        fprintf(stderr, "%s not connected\n", corr_mat->pv_name[i]);
      }
    }
    for (i = 0; i < bpm_mat->names; i++) {
      if (ca_state(bpm_mat->channelID[i]) != cs_conn) {
        fprintf(stderr, "%s not connected\n", bpm_mat->pv_name[i]);
      }
    }
    exit(1);
  }
}

double computeDelta(CORRBPM_MAT *corr_mat, long bpms, double *deltaList, CORRBPM_MAT *bpm_mat) {
  long i, j;
  double maxDelta;
  maxDelta = 0;
  for (i = 0; i < corr_mat->names; i++) {
    corr_mat->delta[i] = 0;
    for (j = 0; j < bpms; j++)
      corr_mat->delta[i] += corr_mat->matrix[i][j] * deltaList[j];
    if (fabs(corr_mat->delta[i]) > maxDelta)
      maxDelta = fabs(corr_mat->delta[i]);
  }
  /*compute bpm delta if response matrix provided */
  if (bpm_mat->m > 0) {
    for (i = 0; i < bpm_mat->names; i++) {
      bpm_mat->delta[i] = 0;
      for (j = 0; j < bpm_mat->m; j++)
        bpm_mat->delta[i] += bpm_mat->matrix[i][j] * corr_mat->delta[j];
    }
  }

  return maxDelta;
}

double computeRFdelta(CORRBPM_MAT *corr_mat, char *twissFile) {
  SDDS_TABLE SDDSin;
  double *etax = NULL;
  char **name = NULL;
  int32_t rows;
  double deltaRF;
  long i, k;

  if (!SDDS_InitializeInput(&SDDSin, twissFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSin) <= 0) {
    fprintf(stderr, "Error reading irm matrix file %s\n", twissFile);
    return 0;
  }
  if ((rows = SDDS_RowCount(&SDDSin)) <= 0) {
    fprintf(stderr, "Error irm twiss file %s is empty \n", twissFile);
    return 0;
  }
  if (!(name = (char **)SDDS_GetColumn(&SDDSin, "ElementName")) ||
      !(etax = SDDS_GetColumnInDoubles(&SDDSin, "etax")))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (!SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  deltaRF = 0;
  for (i = 0; i < rows; i++) {
    if ((k = match_string(name[i], corr_mat->name, corr_mat->names, EXACT_MATCH)) >= 0) {
      deltaRF += corr_mat->delta[k] * etax[i];
    }
  }
  SDDS_FreeStringArray(name, rows);
  free(name);
  free(etax);
  deltaRF = -1.0 * 1296.0 / (1104.0 * 1104.0) * 2.99792458e8 * 0.00741e-3 * deltaRF;
  if (fabs(deltaRF) > 100) {
    fprintf(stderr, "Error: delta RF changes too much (%f), steering too much, quit!\n", deltaRF);
    exit(1);
  }
  return deltaRF;
}
