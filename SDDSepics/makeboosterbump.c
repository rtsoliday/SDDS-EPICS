/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* SCCS ID: @(#)rampload.c	1.5 5/5/94 */
/*************************************************************************
 * FILE:      rampload.c
 * Author:    Claude Saunders
 * 
 * Purpose:   Program for loading ramp tables to ramptable records on
 *            an IOC. The IOC record and device support software then
 *            transfers the table to the PSCU.
 *
 * Mods:      login mm/dd/yy: description
 *************************************************************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <string.h>
#include "cadef.h"
#include "mdb.h"
#include "scan.h"
#include "SDDS.h"
#include "mdb.h"

#define VERSION "V1.0"
#define MAX_RAMP_POINTS 4096

#define CLO_CORRECTOR_LIST 0
#define CLO_REFERENCE_RAMP_TABLE 1
#define CLO_RAMP_TEMPLATAE 2
#define CLO_OUT_RAMP_FILE 3
#define CLO_AMPLITUDE 4
#define CLO_REGION 5
#define CLO_DRYRUN 6
#define CLO_VERBOSE 7
#define CLO_DAILY_FILES 8
#define CLO_COEFFICIENTS 9
#define N_OPTIONS 10

char *option[N_OPTIONS] = {"correctorList", "referenceRampTable", "rampTemplate", "outRampFile", "amplitude", "region", "dryRun", "verbose", "dailyFiles", "coefficients"};

char *USAGE = "makeboosterbump <configFile> \n\
    [-correctorList=<list of correctors>] [-referenceRampTable=<ref ramp table file>] [-verbose] \n\
    [-rampTemplate=<ramp template file>] [-outRampFile=<log filename name>|-dailyFiles=dir=<dir>,rootname=<rootname>,extension=<extension>,hoursOnly] \n\
    [-amplitude=<value> or <list of values>] -regions=<ramp region 0,1,2,..., or 9 | start=<start-region>,end=<end-region>>\n\
    [-coefficients=<list of coefficients>\n\
correctorList       provide the list of correctors if <configFile> is not provided.\n\
                    eg, B1C0H, B1C1H, etc.\n\
                    if <configFile> is provided, correctorList option will be ignored.\n\
                    either correctorList or configFile has to be provided.\n\
referenceRampTable  the new ramp table to be loaded will be the amount to be changed plus the reference.\n\
                    for example, /home/helios/oagData/booster/ramps/correctors/lattices/default/HVCorr.ramp is normally used.\n\
                    if not provided, it will read the ramp table from PVs.\n\
rampTemplate        ramp template which the ramp changes based on, \n\
                    default is /home/helios/oagData/booster/ramps/correctors/lattices/default/ramp.sdds \n\
outRampFile         file name for saving the new ramp table.\n\
dailyFile           ignored if outRampFile provided. dailyFile provide directory name, rootname, and file extension for outRampFile\n\
                    the filename will be <directory>/<rootname>-YYYY-JJJ-MMDD:HHMMSS.<extension> \n\
amplitude           amount to change the ramp by, if not provided, the value will be read from \n\
                    <corrector>:CorrectionAI pvs.\n\
regions             required, region 0, 1, ..., or 9 of the ramp; default region 0. \n\
                    or give a range of regions, start region and end region.\n\
coefficients          give a list of coefficients for correctors. must be the same length as correctorList.\n\
dryRun              if provided, the ramp table will not be loaded.\n\
verbose             printout message.\n\
changeboosterramp is a utility to change the provided booster corrector ramps by given amplitudes..\n\n";

typedef struct
{
  double *refRampSetpoint, *rampSetpoint, rampAmplitude, *currentRampSetpoint, shuntCoef;
  chid ramp_setID, ramp_timeID, amp_ID, ramp_procID;
  short foundmatch;
} CORRECTOR_RAMP;

void readConfigFile(char *configFile, char ***corrList, long *correctors);
char *generateDailyFilename(char *dir, char *rootname, char *ext, long hoursOnly);

/*
#define HCorr_Coef 1.106
#define VCorr_Coef 0.919 */
/* HCorr_Coef and VCorr_Coef are from At 7 GeV, maximum angle for 19 A is
   H plane 1.106  mrad
   V plane 0.919  mrad
*/
/***2019-0301 CY claims that
CorrCoef =1.106(H) and 0.919 (V) --- maxim deflection of H- and V-correctors. 
(The H deflection is twice as list in parameters page. The V deflection is close. They 
should be: CorrCoef=0.594 and 0.919 ****/
/* redefine HCorr_Coef and VCorr_Ceof*/
/*changed VCorr_Coef from 0.919 to 0.923 again*/
#define HCorr_Coef 0.594
#define VCorr_Coef 0.923

static char *shuntCoefFile = "/home/helios/oagData/booster/ramps/correctors/correctorShuntCoeffs.sdds";

#define HOURS_ONLY 0x0001U

int main(int argc, char *argv[]) {
  SDDS_TABLE outTable, refTable, templateTable, shuntTable;
  char *configFile = NULL, *refFile = NULL, *templateFile = NULL, *logFile = NULL, *dailyFileDir, *dailyFileRootname, *dailyFileExt;
  char **corrList = NULL, *corrName, **shuntCorr = NULL;
  double *templateTime = NULL, *templateSetpoint = NULL, temp_value, maxsetpoint, *rampsetpoint = NULL, *shuntCoef = NULL;
  double *ampList = NULL;
  long region = 0, correctors = 0, pageUsed = 0, ampsProvided = 0;
  long ramp_rows = 0, template_rows = 0, rows = 0, verbose = 0;
  long i, j, i_arg, index, dryRun = 0, dailyFiles = 0, hoursOnly = 0;
  unsigned long code, dummyFlags;
  double *rampTime = NULL;
  CORRECTOR_RAMP *corrRamp = NULL;
  char tempStr[256];
  SCANNED_ARG *s_arg;
  OUTRANGE_CONTROL aboveRange, belowRange;
  double starttime, onevalue = 1.0, fixedValue = 55;
  chid eventProcID;
  double *coeff = NULL;
  int32_t regionStart = -1, regionEnd = -1, startIndex, endIndex, ncoefs = 0, shuntRows;
  double corrCoef = 1;

  SDDS_RegisterProgramName(argv[0]);
  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);
  dailyFileDir = dailyFileRootname = dailyFileExt = NULL;
  starttime = getTimeInSecs();
  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], "_");
      switch (match_string(s_arg[i_arg].list[0], option, N_OPTIONS, 0)) {
      case CLO_CORRECTOR_LIST:
        correctors = s_arg[i_arg].n_items - 1;
        corrList = (char **)malloc(sizeof(*corrList) * correctors);
        for (j = 0; j < correctors; j++)
          SDDS_CopyString(&corrList[j], s_arg[i_arg].list[j + 1]);
        break;
      case CLO_REFERENCE_RAMP_TABLE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -referenceRampTable syntax.");
        SDDS_CopyString(&refFile, s_arg[i_arg].list[1]);
        break;
      case CLO_RAMP_TEMPLATAE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -rampTemplate syntax.");
        SDDS_CopyString(&templateFile, s_arg[i_arg].list[1]);
        break;
      case CLO_OUT_RAMP_FILE:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb("invalid -outRampFile syntax.");
        SDDS_CopyString(&logFile, s_arg[i_arg].list[1]);
        break;
      case CLO_AMPLITUDE:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -amplitude syntax.");
        ampsProvided = s_arg[i_arg].n_items - 1;
        ampList = malloc(sizeof(*ampList) * ampsProvided);
        for (i = 0; i < ampsProvided; i++) {
          if (!get_double(&ampList[i], s_arg[i_arg].list[i + 1]))
            SDDS_Bomb("Invalid -amplitude values provided.");
        }
        break;
      case CLO_REGION:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -region syntax.");
        if (s_arg[i_arg].n_items == 2) {
          if (!get_long(&region, s_arg[i_arg].list[1]))
            SDDS_Bomb("Invalid -region provided.");
          if (region < 0 || region > 9) {
            fprintf(stderr, "Error invalid region %ld provided\n", region);
            exit(1);
          }
        } else {
          s_arg[i_arg].n_items--;
          if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                            "start", SDDS_LONG, &regionStart, 1, 0,
                            "end", SDDS_LONG, &regionEnd, 1, 0,
                            NULL))
            SDDS_Bomb("invalid -region syntax/value");
          if (regionStart < 0 || regionEnd < regionStart || regionEnd > 9) {
            fprintf(stderr, "start=%d, end=%d\n", regionStart, regionEnd);
            SDDS_Bomb("invalid -region start/end values");
          }
          s_arg[i_arg].n_items += 1;
        }
        break;
      case CLO_DRYRUN:
        dryRun = 1;
        break;
      case CLO_VERBOSE:
        verbose = 1;
        break;
      case CLO_DAILY_FILES:
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "directory", SDDS_STRING, &dailyFileDir, 1, 0,
                          "rootname", SDDS_STRING, &dailyFileRootname, 1, 0,
                          "extension", SDDS_STRING, &dailyFileExt, 1, 0,
                          "hoursOnly", -1, NULL, 0, HOURS_ONLY,
                          NULL))
          SDDS_Bomb("invalid -dailyFiles syntax/value");
        s_arg[i_arg].n_items += 1;
        dailyFiles = 1;
        if (dummyFlags & HOURS_ONLY)
          hoursOnly = 1;
        break;
      case CLO_COEFFICIENTS:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb("invalid -coefficients syntax.");
        ncoefs = s_arg[i_arg].n_items - 1;
        if (!(coeff = malloc(sizeof(*coeff) * ncoefs)))
          SDDS_Bomb("Memory allocation failure for coef!");
        for (i = 0; i < ncoefs; i++)
          if (!get_double(&coeff[i], s_arg[i_arg].list[i + 1]))
            SDDS_Bomb("invalid coefficient value provided.");
        break;
      default:
        fprintf(stderr, "Error: invalid option (%s) provided\n", s_arg[i_arg].list[0]);
        break;
      }
    } else {
      if (!configFile)
        configFile = s_arg[i_arg].list[0];
      else {
        fprintf(stderr, "Error (%s): too many filenames\n", s_arg[i_arg].list[0]);
        exit(1);
      }
    }
  }
  if (!logFile) {
    if (dailyFiles) {
      logFile = generateDailyFilename(dailyFileDir, dailyFileRootname, dailyFileExt, hoursOnly);
    }
  }

  if (!correctors && !configFile)
    SDDS_Bomb("Either -corrList or configFile has to be provided");

  if (correctors && configFile) {
    if (verbose)
      fprintf(stdout, "Warning -corrList is provided, configFile is ignored.\n");
  }
  if (ncoefs && ncoefs != correctors)
    SDDS_Bomb("number of coeffiicents should be the same as number of correctrors.");

  /* if (!refFile)
     SDDS_CopyString(&refFile, "/home/helios/oagData/booster/ramps/correctors/lattices/default/HVCorr.ramp"); */
  if (!templateFile)
    SDDS_CopyString(&templateFile, "/home/helios/oagData/booster/ramps/correctors/lattices/default/ramp.sdds");

  if (configFile) {
    /*read correctors from config file */
    if (verbose)
      fprintf(stdout, "reading correctors from config file %s\n", configFile);
    readConfigFile(configFile, &corrList, &correctors);
  }
  /*read shunt coefficients */
  if (!SDDS_InitializeInput(&shuntTable, shuntCoefFile) || SDDS_ReadTable(&shuntTable) < 0)
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!(shuntRows = SDDS_CountRowsOfInterest(&shuntTable))) {
    fprintf(stderr, "Error: no data found in shunt coeff file\n");
    exit(1);
  }
  if (!(shuntCorr = (char **)SDDS_GetColumn(&shuntTable, "CorrName")) ||
      !(shuntCoef = (double *)SDDS_GetColumnInDoubles(&shuntTable, "ShuntCoeff")) ||
      !(SDDS_Terminate(&shuntTable))) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (!(corrRamp = SDDS_Malloc(sizeof(*corrRamp) * correctors)))
    SDDS_Bomb("Memory allocation failure!");

  for (i = 0; i < correctors; i++) {
    corrRamp[i].foundmatch = 0;
    j = match_string(corrList[i], shuntCorr, shuntRows, EXACT_MATCH);
    if (j >= 0)
      corrRamp[i].shuntCoef = shuntCoef[j];
    else
      corrRamp[i].shuntCoef = 1.0;
  }

  /*free shunt data */
  SDDS_FreeStringArray(shuntCorr, shuntRows);
  free(shuntCorr);
  free(shuntCoef);

  /*read template file */
  if (verbose)
    fprintf(stdout, "Reading template file...\n");
  if (!SDDS_InitializeInput(&templateTable, templateFile) || SDDS_ReadTable(&templateTable) < 0)
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

  if (!(template_rows = SDDS_CountRowsOfInterest(&templateTable))) {
    fprintf(stderr, "Error: no data found in template table %s\n", templateFile);
    exit(1);
  }
  if (!(templateTime = SDDS_GetColumnInDoubles(&templateTable, "RampTime")) ||
      !(templateSetpoint = SDDS_GetColumnInDoubles(&templateTable, "RampSetpoint")) ||
      !(SDDS_Terminate(&templateTable))) {
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }
  if (verbose)
    fprintf(stdout, "searching pvs...\n");
  ca_task_initialize();
  if (ca_search("It:EG1:vme_eventLO", &eventProcID) != ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_search for gbl pvs.\n", argv[0]);
    return (1);
  }

  for (i = 0; i < correctors; i++) {
    sprintf(tempStr, "B:%s:source", corrList[i]);
    if (ca_search(tempStr, &corrRamp[i].ramp_setID) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_search for %s\n", argv[0], tempStr);
      exit(1);
    }
    sprintf(tempStr, "B:%s:time", corrList[i]);
    if (ca_search(tempStr, &corrRamp[i].ramp_timeID) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_search for %s\n", argv[0], tempStr);
      exit(1);
    }
    sprintf(tempStr, "B:%s:sub.PROC", corrList[i]);
    if (ca_search(tempStr, &corrRamp[i].ramp_procID) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_search for %s\n", argv[0], tempStr);
      exit(1);
    }
    corrRamp[i].refRampSetpoint = corrRamp[i].rampSetpoint = corrRamp[i].currentRampSetpoint = NULL;
    if (ampsProvided == 0) {
      /*if amplitude is not provided, read it from :CorrectionAI pv */
      sprintf(tempStr, "B:%s:CorrectionAI", corrList[i]);
      if (ca_search(tempStr, &corrRamp[i].amp_ID) != ECA_NORMAL) {
        fprintf(stderr, "%s: problem doing ca_search for %s\n", argv[0], tempStr);
        exit(1);
      }
    } else {
      if (i < ampsProvided)
        corrRamp[i].rampAmplitude = ampList[i];
      else
        corrRamp[i].rampAmplitude = ampList[ampsProvided - 1];
    }
  }
  if (ca_pend_io(30) != ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_search1 for some pvs.\n", argv[0]);
    exit(1);
  }

  if (verbose)
    fprintf(stdout, "Reading reference ramp...\n");
  /*read the reference ramp table for each corrector */
  if (refFile) {
    corrName = NULL;
    if (!SDDS_InitializeInput(&refTable, refFile))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    while (SDDS_ReadTable(&refTable) > 0) {
      if (!(rows = SDDS_CountRowsOfInterest(&refTable)))
        continue;
      if (!ramp_rows) {
        ramp_rows = rows;
        /*only read one time of RampTime since they are the same for all correctors */
        /*and it suppose to have 20 values, corresponding to 10 regions, each region correspond to two points */
        /*for example, region0 --point 0, 1; region 1 point: 2, 3; */
        if (!(rampTime = SDDS_GetColumnInDoubles(&refTable, "RampTime")))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      }
      if (ramp_rows != rows)
        SDDS_Bomb("Error: reference ramp table has different rows.");

      if (!(corrName = SDDS_GetParameterAsString(&refTable, "CorrName", NULL)))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if ((index = match_string(corrName, corrList, correctors, EXACT_MATCH)) >= 0) {
        /*reading ref ramp setpoint */
        corrRamp[index].foundmatch = 1;
        if (!(corrRamp[index].refRampSetpoint = SDDS_GetColumnInDoubles(&refTable, "RampSetpoint")))
          SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
        if (!(corrRamp[index].rampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)) ||
            !(corrRamp[index].currentRampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)))
          SDDS_Bomb("Memory allocation for ramp setpoint failure");
        pageUsed++;
      }
      free(corrName);
      corrName = NULL;
      if (pageUsed == correctors) /*all needed corrector have reference setpoint */
        break;
    }
    if (!SDDS_Terminate(&refTable))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    for (i = 0; i < correctors; i++) {
      if (!corrRamp[i].foundmatch) {
        fprintf(stdout, "Warning: %s did not find in the reference ramp file, its reference will be read from current table.\n", corrList[i]);
        if (!(corrRamp[i].refRampSetpoint = malloc(sizeof(double) * ramp_rows)) ||
            !(corrRamp[i].rampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)) ||
            !(corrRamp[i].currentRampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)))
          SDDS_Bomb("Memory allocation for ramp setpoint failure");
      }
    }
  } else {
    ramp_rows = 20;
    if (!(rampTime = malloc(sizeof(*rampTime) * ramp_rows)))
      SDDS_Bomb("Memory allocation for ramp time failure");

    for (i = 0; i < correctors; i++) {
      if (!(corrRamp[i].refRampSetpoint = malloc(sizeof(double) * ramp_rows)) ||
          !(corrRamp[i].rampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)) ||
          !(corrRamp[i].currentRampSetpoint = SDDS_Malloc(sizeof(double) * ramp_rows)))
        SDDS_Bomb("Memory allocation for ramp setpoint failure");
    }
  }

  if (!ampsProvided) {
    /*read amplituded from pv*/
    if (verbose)
      fprintf(stdout, "reading bump amplitude...\n");
    for (i = 0; i < correctors; i++) {
      if (ca_get(DBR_DOUBLE, corrRamp[i].amp_ID, &corrRamp[i].rampAmplitude) != ECA_NORMAL) {
        fprintf(stderr, "Error reading %s amplitude.\n", corrList[i]);
        exit(1);
      }
    }
    if (ca_pend_io(30) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem reading corrector ramp amplituded.\n", argv[0]);
      exit(1);
    }
  }
  if (verbose)
    fprintf(stdout, "check corrector connection...\n");
  for (i = 0; i < correctors; i++) {
    if ((ca_state(corrRamp[i].ramp_timeID) != cs_conn) ||
        (ca_state(corrRamp[i].ramp_setID) != cs_conn)) {
      fprintf(stderr, "%s: unable to connect to %s\n", argv[0], corrList[i]);
      return (1);
    }
    if (i == 0 && !refFile) {
      if (ca_array_get(DBR_DOUBLE, ramp_rows, corrRamp[i].ramp_timeID, rampTime) != ECA_NORMAL) {
        fprintf(stderr, "Error reading ramp time of %s \n", corrList[i]);
        exit(1);
      }
    }
    if (ca_array_get(DBR_DOUBLE, ramp_rows, corrRamp[i].ramp_setID, corrRamp[i].currentRampSetpoint) != ECA_NORMAL) {
      fprintf(stderr, "Error reading current ramp table of %s \n", corrList[i]);
      exit(1);
    }
  }
  if (ca_pend_io(30) != ECA_NORMAL) {
    fprintf(stderr, "%s: problem doing ca_array_get\n", argv[0]);
    return (1);
  }

  /*if not found in the  reference file, use current ramp setpoint as reference */
  for (i = 0; i < correctors; i++) {
    if (!corrRamp[i].foundmatch)
      memcpy(corrRamp[i].refRampSetpoint, corrRamp[i].currentRampSetpoint, sizeof(double) * ramp_rows);
  }
  /*compute bump */
  if (verbose)
    fprintf(stdout, "compute new ramp setpoint...\n");

  if (regionStart >= 0 && regionEnd > regionStart) {
    startIndex = 2 * regionStart;
    endIndex = 2 * regionEnd + 1;
  } else {
    startIndex = 2 * region;
    endIndex = 2 * region + 1;
  }

  for (i = 0; i < correctors; i++) {
    /* region0 start point is rampTime[0], stop points is rampTime[1];
         region region start point is rampTime[2*region], stop point is rampTime[2*region+1] */
    corrRamp[i].rampAmplitude /= corrRamp[i].shuntCoef;
    memcpy(corrRamp[i].rampSetpoint, corrRamp[i].refRampSetpoint, sizeof(double) * ramp_rows);
    for (j = startIndex; j <= endIndex; j++) {
      if (wild_match(corrList[i], "*H*"))
        corrCoef = HCorr_Coef;
      else if (wild_match(corrList[i], "*V*"))
        corrCoef = VCorr_Coef;
      else
        corrCoef = 1.0;
      temp_value = interpolate(templateSetpoint, templateTime, template_rows, rampTime[j], &belowRange, &aboveRange, 1, &code, 1);
      if (ncoefs)
        corrRamp[i].rampSetpoint[j] += temp_value * coeff[i] * 5 * corrRamp[i].rampAmplitude / corrCoef;
      else
        corrRamp[i].rampSetpoint[j] += temp_value * corrRamp[i].rampAmplitude / corrCoef;
      /*change the sign because the corrector connection was wrong*/
    }
  }
  free(templateTime);
  free(templateSetpoint);
  if (logFile) {
    if (verbose)
      fprintf(stdout, "writing data into log file %s\n", logFile);
    if (!SDDS_InitializeOutput(&outTable, SDDS_BINARY, 1, NULL, NULL, logFile))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (!SDDS_DefineSimpleParameter(&outTable, "CorrName", NULL, SDDS_STRING) ||
        !SDDS_DefineSimpleColumn(&outTable, "RampTime", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&outTable, "RampSetpoint", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&outTable, "RefRampSetpoint", NULL, SDDS_DOUBLE) ||
        !SDDS_DefineSimpleColumn(&outTable, "OldRampSetpoint", NULL, SDDS_DOUBLE) ||
        !SDDS_WriteLayout(&outTable))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);

    for (i = 0; i < correctors; i++) {
      if (!SDDS_StartTable(&outTable, ramp_rows))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
      if (!SDDS_SetParameters(&outTable, SDDS_BY_NAME | SDDS_PASS_BY_VALUE, "CorrName", corrList[i], NULL) ||
          !SDDS_SetColumn(&outTable, SDDS_BY_NAME, rampTime, ramp_rows, "RampTime") ||
          !SDDS_SetColumn(&outTable, SDDS_BY_NAME, corrRamp[i].rampSetpoint, ramp_rows, "RampSetpoint") ||
          !SDDS_SetColumn(&outTable, SDDS_BY_NAME, corrRamp[i].refRampSetpoint, ramp_rows, "RefRampSetpoint") ||
          !SDDS_SetColumn(&outTable, SDDS_BY_NAME, corrRamp[i].currentRampSetpoint, ramp_rows, "OldRampSetpoint") ||
          !SDDS_WritePage(&outTable))
        SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    }
    if (!SDDS_Terminate(&outTable))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  }

  if (!dryRun) {
    /*check monotonic increasing  of ramp time and find the max setpoint */
    if (verbose)
      fprintf(stdout, "loading new ramp table...ramprows=%ld\n", ramp_rows);
    for (j = 1; j < ramp_rows; j++) {
      if (rampTime[j] < rampTime[j - 1]) {
        fprintf(stderr, "%s, ramp time decreases at %ld point.\n", argv[0], j);
        exit(1);
      }
    }
    for (i = 0; i < correctors; i++) {
      rampsetpoint = corrRamp[i].rampSetpoint;
      maxsetpoint = rampsetpoint[0];
      for (j = 1; j < ramp_rows; j++) {
        if (fabs(rampsetpoint[j]) > maxsetpoint)
          maxsetpoint = fabs(rampsetpoint[j]);
      }
      if (maxsetpoint > 1) {
        fprintf(stderr, "max ramp setpoint of %s greater than 1 or less than -1.\n", corrList[i]);
        exit(1);
      }
    }
    for (i = 0; i < correctors; i++) {
      /* Write ramp table out to PV */
      if ((ca_state(corrRamp[i].ramp_timeID) != cs_conn) ||
          (ca_state(corrRamp[i].ramp_setID) != cs_conn)) {
        fprintf(stderr, "%s: unable to connect to %s\n", argv[0], corrList[i]);
        return (1);
      }
      if ((ca_array_put(DBR_DOUBLE, ramp_rows, corrRamp[i].ramp_timeID, rampTime) != ECA_NORMAL) ||
          (ca_array_put(DBR_DOUBLE, ramp_rows, corrRamp[i].ramp_setID, corrRamp[i].rampSetpoint) != ECA_NORMAL)) {
        fprintf(stderr, "%s: problem doing ca_array_put for %s\n", argv[0], corrList[i]);
        return (1);
      }
      if (ca_pend_io(15) != ECA_NORMAL) {
        fprintf(stderr, "%s: problem doing ca_array_put2 for %s\n", argv[0], corrList[i]);
        return (1);
      }
      if (ca_put(DBR_DOUBLE, corrRamp[i].ramp_procID, &onevalue) != ECA_NORMAL) {
        fprintf(stderr, "%s: problem doing ca_array_put3 for %s proc\n", argv[0], corrList[i]);
        return (1);
      }
      if (ca_pend_io(15) != ECA_NORMAL) {
        fprintf(stderr, "%s: problem doing ca_array_put for %s\n", argv[0], corrList[i]);
        return (1);
      }
    }
  }
  if (!dryRun) {
    if (ca_put(DBR_DOUBLE, eventProcID, &fixedValue) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_put for gbl pvs\n", argv[0]);
      ca_task_exit();
      return (1);
    }
    if (ca_pend_io(15) != ECA_NORMAL) {
      fprintf(stderr, "%s: problem doing ca_put for gbl pvs\n", argv[0]);
      ca_task_exit();
      return (1);
    }
  }
  ca_task_exit();
  SDDS_FreeStringArray(corrList, correctors);
  free(corrList);
  free(ampList);
  for (i = 0; i < correctors; i++) {
    free(corrRamp[i].rampSetpoint);
    free(corrRamp[i].refRampSetpoint);
    free(corrRamp[i].currentRampSetpoint);
  }
  free(corrRamp);
  if (rampTime)
    free(rampTime);
  if (refFile)
    free(refFile);
  if (templateFile)
    free(templateFile);
  if (logFile)
    free(logFile);
  free_scanargs(&s_arg, argc);
  if (dailyFileDir)
    free(dailyFileDir);
  if (dailyFileRootname)
    free(dailyFileRootname);
  if (dailyFileExt)
    free(dailyFileExt);
  if (coeff)
    free(coeff);
  if (verbose)
    fprintf(stdout, "Total time elapsed %lf seconds, exit\n", getTimeInSecs() - starttime);
  return (0);
}

void readConfigFile(char *configFile, char ***corrList, long *correctors) {
  SDDS_DATASET configData;
  int32_t rows = 0;
  char *parName = NULL;
  *corrList = NULL;
  *correctors = 0;

  if (!SDDS_InitializeInput(&configData, configFile))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  while (SDDS_ReadTable(&configData) > 0) {
    if (!(parName = SDDS_GetParameterAsString(&configData, "NameType", NULL)))
      SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
    if (strcmp(parName, "CorrectorNames") == 0) {
      rows = SDDS_CountRowsOfInterest(&configData);
      if (!rows) {
        fprintf(stderr, "readConfigFile error1: no correctors found in config file %s\n", configFile);
        exit(1);
      }
      *correctors = rows;
      if (!(*corrList = (char **)SDDS_GetColumn(&configData, "Name"))) {
        fprintf(stderr, "readConfigFile error2: error reading corrector names in config file %s\n", configFile);
        exit(1);
      }
      break;
    }
  }
  if (!SDDS_Terminate(&configData))
    SDDS_PrintErrors(stderr, SDDS_EXIT_PrintErrors | SDDS_VERBOSE_PrintErrors);
  if (!(*correctors)) {
    fprintf(stderr, "readConfigFile error1: no correctors found in config file %s\n", configFile);
    exit(1);
  }
  return;
}

char *generateDailyFilename(char *dir, char *rootname, char *ext, long hoursOnly) {
  char buffer[1024], filename[1024];
  char *name = NULL;
  time_t now;
  struct tm *now1;

  now = time(NULL);
  now1 = localtime(&now);
  if (hoursOnly)
    strftime(buffer, 1024, "%Y-%j-%m%d-%H", now1);
  else
    strftime(buffer, 1024, "%Y-%j-%m%d-%H%M%S", now1);
  if (rootname && strlen(rootname)) {
    sprintf(filename, "%s-%s", rootname, buffer);
    sprintf(buffer, filename);
  }
  if (dir && strlen(dir)) {
    sprintf(filename, "%s/%s", dir, buffer);
    sprintf(buffer, filename);
  }
  if (ext && strlen(ext)) {
    sprintf(filename, "%s%s", buffer, ext);
    sprintf(buffer, filename);
  }

  if (!(name = malloc(sizeof(*name) * (strlen(buffer) + 1)))) {
    fprintf(stderr, "Memory allocation failure for generating daily filename.\n");
    exit(1);
  }
  return strcpy(name, buffer);
}
