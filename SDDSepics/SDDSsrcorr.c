#include "mdb.h"
#include "SDDS.h"
#include "SDDSsrcorr.h"

#define corrs 7
char *hcorr[corrs] = {"A:H1", "A:H2", "A:H4", "B:H4", "B:H3", "B:H2", "B:H1"};
char *vcorr[corrs] = {"A:V1", "A:V2", "A:V4", "B:V4", "B:V3", "B:V2", "B:V1"};
char *Coord[2] = {"X", "Y"};

char *corrType[corrTypes] = {"DP", "vector", "dynamic", "scalar", "plain"};

char *sourceMode[sourceModes] = {"Operation", "Maintenance"};

#define writeBPMSetPt2RM 0
#define bpmSetPtVectorMode 1
#define enableWriteCorr2RM 2
#define reloadCorr2RM 3
#define corrVectorMode 4
#define writeCorr2RM 5
#define corrModePVs 6
chid corrModeID[corrModePVs];
char *hcorrModePV[corrModePVs] = {"DP:EnableWriteXbpmSetPt2RMC", "DP:XbpmSetPtVectorModeC.VAL",
                                  "DP:EnableWriteHCor2RMC", "DP:ReloadHCor2RMC",
                                  "DP:HCorrVectorModeC.VAL", "DP:WriteHCor2RMC"};
char *vcorrModePV[corrModePVs] = {"DP:EnableWriteYbpmSetPt2RMC", "DP:YbpmSetPtVectorModeC.VAL",
                                  "DP:EnableWriteVCor2RMC", "DP:ReloadVCor2RMC",
                                  "DP:VCorrVectorModeC.VAL", "DP:WriteVCor2RMC"};
char **corrModePV;

static char *CUsectorFile = "/home/helios/oagData/sr/XAXSTF/sectors.sdds";
void set_source_mode(char *plane, char *sourceModeInput, long dryRun, double pendIOTime) {
  SDDS_DATASET SDDSin;
  int32_t *sector = NULL, *CUflag = NULL, *CUsector = NULL, channels = 0;
  int32_t rows, i, j, CUsectors = 0;
  char pvName[256], Plane[256];
  chid *sourceID;
  if (strcmp(plane, "h") == 0)
    sprintf(Plane, "H");
  else if (strcmp(plane, "v") == 0)
    sprintf(Plane, "V");
  else
    SDDS_Bomb("Invalid plane provided!");

  if (!SDDS_InitializeInput(&SDDSin, CUsectorFile))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  if (SDDS_ReadPage(&SDDSin) <= 0) {
    fprintf(stderr, "Error reading ID sector file %s\n", CUsectorFile);
    exit(1);
  }
  rows = SDDS_RowCount(&SDDSin);
  CUsector = malloc(sizeof(*CUsector) * rows);
  if (!(sector = SDDS_GetColumnInLong(&SDDSin, "Sector")) ||
      !(CUflag = SDDS_GetColumnInLong(&SDDSin, "CUflag")) ||
      !SDDS_Terminate(&SDDSin))
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors | SDDS_EXIT_PrintErrors);
  for (i = 0; i < rows; i++) {
    if (CUflag[i]) {
      CUsector[CUsectors] = sector[i];
      CUsectors++;
    }
  }
  sourceID = malloc(sizeof(*sourceID) * (40 * corrs + CUsectors));
  for (i = 0; i < 40; i++) {
    for (j = 0; j < corrs; j++) {
      if (strcmp(plane, "h") == 0)
        sprintf(pvName, "S%d%s:ControlSrcBO", i + 1, hcorr[j]);
      else
        sprintf(pvName, "S%d%s:ControlSrcBO", i + 1, vcorr[j]);
      if (ca_search(pvName, &sourceID[channels]) != ECA_NORMAL) {
        fprintf(stderr, "error: problem doing search for %s\n", pvName);
        ca_task_exit();
        exit(1);
      }
      channels++;
    }
  }
  for (i = 0; i < CUsectors; i++) {
    sprintf(pvName, "S%dC:%s1:ControlSrcBO", CUsector[i], Plane);
    if (ca_search(pvName, &sourceID[channels]) != ECA_NORMAL) {
      fprintf(stderr, "error: problem doing search for %s\n", pvName);
      ca_task_exit();
      exit(1);
    }
    channels++;
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "error1: problem doing search for some source channels\n");
    exit(1);
  }
  if (!dryRun) {
    for (i = 0; i < channels; i++) {
      if (ca_put(DBR_STRING, sourceID[i], sourceModeInput) != ECA_NORMAL) {
        fprintf(stderr, "error2: problem doing put for some source channels\n");
        exit(1);
      }
    }
    if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
      fprintf(stderr, "error3: problem doing put for some source channels\n");
      exit(1);
    }
  }
  free(sector);
  free(CUflag);
  free(CUsector);
  free(sourceID);
}

void setup_corr_mode_ca(char *plane, double pendIOTime) {
  long i;
  if (strcmp(plane, "h") == 0 || strcmp(plane, "H") == 0) {
    corrModePV = hcorrModePV;
  } else if (strcmp(plane, "v") == 0 || strcmp(plane, "V") == 0) {
    corrModePV = vcorrModePV;
  } else
    SDDS_Bomb("setup_corr_mode_ca(Error): invalid plane given!");
  for (i = 0; i < corrModePVs; i++) {
    if (ca_search(corrModePV[i], &corrModeID[i]) != ECA_NORMAL) {
      fprintf(stderr, "Error searching for mode pv %s\n", corrModePV[i]);
      exit(1);
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    for (i = 0; i < corrModePVs; i++) {
      if (ca_state(corrModeID[i]) != ECA_NORMAL) {
        fprintf(stderr, "Error: %s not connected\n", corrModePV[i]);
      }
    }
    exit(1);
  }
}

void set_corr_mode(char *plane, char *corrTypeInput, long dryRun, double pendIOTime, long unified) {
  long corrTypeIndex = -1, enable = 0, vector = 0, enableWrite = 0, i;
  long value[corrModePVs];
  setup_corr_mode_ca(plane, pendIOTime);
  /*set corrector mode pvs has to be one in sequential mode, can not run it in parallel */
  corrTypeIndex = match_string(corrTypeInput, corrType, corrTypes, EXACT_MATCH);
  if (dryRun)
    return;
  switch (corrTypeIndex) {
  case 0:
  case 1:
    /*DP or vector*/
    vector = enable = enableWrite = 1;
    break;
  case 2:
  case 3:
  case 4:
    vector = enable = enableWrite = 0;
    break;
  }
  if (unified) {
    /*in unified mode, bpm setpoints are not transmitted from datapool */
    value[0] = 0;
    value[1] = 0;
  } else {
    value[0] = enable;
    value[1] = vector;
  }
  value[2] = enable;
  value[3] = enableWrite;
  value[4] = vector;
  value[5] = enableWrite;
  if (vector == 0 || unified) {
    /*set the pvs in parallel*/
    for (i = 0; i < corrModePVs; i++) {
      if (ca_put(DBR_LONG, corrModeID[i], &value[i]) != ECA_NORMAL) {
        fprintf(stderr, "Error setting value for %s\n", corrModePV[i]);
        exit(1);
      }
    }
  } else {
    /*has to set the pvs in sequential mode */
    if (ca_put(DBR_LONG, corrModeID[0], &value[0]) != ECA_NORMAL) {
      fprintf(stderr, "Error setting value for %s\n", corrModePV[0]);
      exit(1);
    }
    if (ca_put(DBR_LONG, corrModeID[1], &value[1]) != ECA_NORMAL) {
      fprintf(stderr, "Error setting value for %s\n", corrModePV[1]);
      exit(1);
    }
    ca_pend_event(0.1);
    if (ca_put(DBR_LONG, corrModeID[2], &value[2]) != ECA_NORMAL) {
      fprintf(stderr, "Error setting value for %s\n", corrModePV[2]);
      exit(1);
    }

    ca_pend_event(0.1);
    if (enableWrite) {
      if (ca_put(DBR_LONG, corrModeID[3], &value[3]) != ECA_NORMAL) {
        fprintf(stderr, "Error setting value for %s\n", corrModePV[3]);
        exit(1);
      }
      ca_pend_event(0.1);
    }
    if (ca_put(DBR_LONG, corrModeID[4], &value[4]) != ECA_NORMAL) {
      fprintf(stderr, "Error setting value for %s\n", corrModePV[4]);
      exit(1);
    }
    ca_pend_event(0.1);
    if (enableWrite) {
      if (ca_put(DBR_LONG, corrModeID[5], &value[5]) != ECA_NORMAL) {
        fprintf(stderr, "Error setting value for %s\n", corrModePV[5]);
        exit(1);
      }
    }
  }
  if (ca_pend_io(pendIOTime) != ECA_NORMAL) {
    fprintf(stderr, "Error setting value for mode pvs\n");
    exit(1);
  }
}
