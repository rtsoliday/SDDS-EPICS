/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include "cadef.h"

#define HIGH 19.
#define ONTIME 0.
#define OFFTIME 236.439589
#define ENDTIME 250.
#define SDDSID "SDDS1"
#define FILETEMPLET "/tmp/setbcorr.XXXXXX"
#define NSCANNERS 10
#define BOOSTERHARMONIC 432
#define LIGHTWAITTIME 10
#define NAMESIZE 34
#define NCOR 40
static int ncor=NCOR;
static char cornames[NCOR][NAMESIZE]= {
  "B1C0","B1C1","B1C2","B1C3","B1C4","B1C5","B1C6","B1C7","B1C8","B1C9",
  "B2C0","B2C1","B2C2","B2C3","B2C4","B2C5","B2C6","B2C7","B2C8","B2C9",
  "B3C0","B3C1","B3C2","B3C3","B3C4","B3C5","B3C6","B3C7","B3C8","B3C9",
  "B4C0","B4C1","B4C2","B4C3","B4C4","B4C5","B4C6","B4C7","B4C8","B4C9"
};

struct BTIMING {
  double boosterduration;
  double boosterep;
  double boosterip;
  double boosternturns;
  double boostertrigger;
  double bpmdelay;
  double bpmduration;
  double bpmoffset;
  double bpmstart;
  double bpmtime;
  double dipolezeroref;
  double goaround;
  double pscusampletime;
  double crampstart;
  double crampstartdelay;
  double drampstartdelay;
  double rampstartoffset;
  double rampstartref;
  double rffreq;
  long averagernturns;
  long averagerweight;
  long bpmn;
  long bpmm;
};

static struct pvs {
  char name_newt[255];
  char name_swap[255];
  char name_pact[255];
  char name_stat[255];
  char name_sevr[255];
  char name_CurrentAO[255];
  chid chid_newt;
  chid chid_swap;
  chid chid_pact;
  chid chid_stat;
  chid chid_sevr;
  chid chid_CurrentAO;
  short value_newt;
  short value_swap;
  short value_pact;
  short value_stat;
  short value_sevr;
  char enum_value_stat[256];
  char enum_value_sevr[256];
};

static struct timingpvs {
  char name_delaycount[255];
  char name_cyclecount[255];
  chid chid_delaycount;
  chid chid_cyclecount;
  long value_delaycount;
  long value_cyclecount;
};

static struct moretimingpvs {
  char name_Hp8657RefFreq[255];
  char name_LastBunch2BoosterIp[255];
  char name_Trig2BsIp[255];
  char name_StartRamp2BsIp[255];
  char name_Ddg4chan6[255];
  char name_ZeroRefAO[255];
  char name_Ddg8chan6[255];
  char name_Ddg3chan6[255];
  char name_Ddg3chan3[255];
  char name_BoosterRampTurnsBsCC[255];
  char name_memscan_wt_ao[255];
  chid chid_Hp8657RefFreq;
  chid chid_LastBunch2BoosterIp;
  chid chid_Trig2BsIp;
  chid chid_StartRamp2BsIp;
  chid chid_Ddg4chan6;
  chid chid_ZeroRefAO;
  chid chid_Ddg8chan6;
  chid chid_Ddg3chan6;
  chid chid_Ddg3chan3;
  chid chid_BoosterRampTurnsBsCC;
  chid chid_memscan_wt_ao;
  double value_Hp8657RefFreq;
  double value_LastBunch2BoosterIp;
  double value_Trig2BsIp;
  double value_StartRamp2BsIp;
  double value_Ddg4chan6;
  double value_ZeroRefAO;
  double value_Ddg8chan6;
  double value_Ddg3chan6;
  double value_Ddg3chan3;
  double value_BoosterRampTurnsBsCC;
  double value_memscan_wt_ao;
};

struct pvs pvinfo[80];

void usage(void);
int setforegroundall(void);
int getbpmtime(double *bpmtime, double *bpmduration);
int gettiming(struct BTIMING *btiming);
int setbcorr(char *corname,double val, double rampbias, int usebpmtime,
             int save, int append, int rampload, int setfg, char *filename);
char *timestamp(void);
void writerampheader(FILE *file);
int writerampdata(FILE *file, double val, char *corname, double rampbias,
                  double bpmtime, double bpmduration, int usebpmtime, char *timestamp);

int main(int argc, char *argv[]) {
  int i;
  int nspec=0;
  double bpmduration;
  double rampbias=0.;

  int append=0, usebpmtime=0, rampload=1, setfg=1;
  int save=0, quiet=0, valspecified=0;
  double bpmtime, val=0.0;
  char filename[BUFSIZ]="";
  char corname[NAMESIZE]="";

  /* Parse command line */
  if (argc < 2) {
    usage();
    return(1);
  }
  ca_task_initialize();
  for (i=1; i < argc; i++) {
    if (argv[i][0] == '-') {
      switch (argv[i][1]) {
      case 'a':
        append = 1;
        break;
      case 'b':
        if (getbpmtime(&bpmtime, &bpmduration)) {
          fprintf(stderr, "*** setbcorr: Could not get BPM time\n");
          return(1);
        }
        usebpmtime = 1;
        break;
      case 'f':
        if (setforegroundall()) {
          ca_task_exit();
          return(1);
        } else {
          ca_task_exit();
          return(0);
        }
      case 'h':
      case '?':
        usage();
        return(0);
      case 'n':
        switch (argv[i][2]) {
        case 'r':
          rampload = 0;
          break;
        case 's':
          setfg = 0;
          break;
        default:
          fprintf(stderr, "*** setbcorr: Invalid option: %s\n", argv[i]);
          return(1);
        }
        break;
      case 'o':
        i++;
        if (i >= argc) {
          usage();
          return(1);
        }
        strcpy(filename, argv[i]);
        save = 1;
        break;
      case 'q':
        quiet = 1;
        break;
      default:
        if (nspec == 1) {
          nspec++;
          val = atof(argv[i]);
          valspecified = 1;
        }
        else {
          fprintf(stderr, "*** setbcorr: Invalid option: %s\n", argv[i]);
          usage();
          return(1);
        }
      }
    } else if(nspec == 0) {
      nspec++;
      strncpy(corname, argv[i], NAMESIZE);
      corname[NAMESIZE-1] = '\0';
    } else if(nspec == 1) {
      nspec++;
      val=atof(argv[i]);
      valspecified = 1;
    } else {
      usage();
      return(1);
    }
  }

  /* Check command line information */
  if (!(*corname)) {
    fprintf(stderr, "*** No corrector name specified\n");
    usage();
    return(1);
  }
  if (!valspecified) {
    fprintf(stderr, "*** No corrector value specified\n");
    usage();
    return(1);
  }
  if (save && !(*filename)) {
    fprintf(stderr, "*** Save filename is blank\n");
    usage();
    return(1);
  }
  if (append && !save) {
    fprintf(stderr, "Must specify \"-o filename\" for -a\n");
    usage();
    return(1);
  }

  /* Set it */
  if (setbcorr(corname,val,rampbias,usebpmtime,save,append,rampload,
               setfg,filename)) {
    if (!quiet) {
      ca_task_exit();   
      return(1);
    }
  }
  ca_task_exit();   
  return(0);
}

void usage(void)
{
  printf("Sets a single Booster rampRAMPTABLE value, the CurrentAO value,\n");
  printf("  and sets the AFG foreground\n");
  printf("Usage is: "
         "setbcorr [Options] corrector-name value\n");
  printf("  Options:\n");
  printf("    -a filename  Append ramp table to filename\n");
  printf("    -b           Make ramp go to zero after BPM interval\n");
  printf("    -o filename  Save ramp table in filename\n");
  printf("    -fg          Load AFG foreground only\n");
  printf("    -h           Help\n");
  printf("    -nr          Skip ramp load\n");
  printf("    -ns          Skip setting AFG foreground\n");
  printf("    -q           Quiet (no error messages)\n");
}

int setforegroundall(void) {
  int iplane,icor,index=-1, i, wait;
  char corname[80][NAMESIZE];
  short setswapval=1;

  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      if (iplane == 0) 
        sprintf(corname[index],"%sH",cornames[icor]);
      else 
        sprintf(corname[index],"%sV",cornames[icor]);
    }
  }

  /* Search for PVs */
  index = -1;
  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      sprintf(pvinfo[index].name_newt,"%s:rampRAMPTABLE.NEWT",corname[index]);
      sprintf(pvinfo[index].name_swap,"%s:rampRAMPTABLE.SWAP",corname[index]);
      sprintf(pvinfo[index].name_pact,"%s:rampRAMPTABLE.PACT",corname[index]);
      sprintf(pvinfo[index].name_stat,"%s:rampRAMPTABLE.STAT",corname[index]);
      sprintf(pvinfo[index].name_sevr,"%s:rampRAMPTABLE.SEVR",corname[index]);
      if ((ca_search(pvinfo[index].name_newt, &pvinfo[index].chid_newt)!=ECA_NORMAL) ||
          (ca_search(pvinfo[index].name_swap, &pvinfo[index].chid_swap)!=ECA_NORMAL) ||
          (ca_search(pvinfo[index].name_pact, &pvinfo[index].chid_pact)!=ECA_NORMAL) ||
          (ca_search(pvinfo[index].name_stat, &pvinfo[index].chid_stat)!=ECA_NORMAL) ||
          (ca_search(pvinfo[index].name_sevr, &pvinfo[index].chid_sevr)!=ECA_NORMAL)) {
        fprintf(stderr,"problem doing ca_search for %s:rampRAMPTABLE\n", corname[index]);
        return(1);
      }
    }
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_search\n");
    return(1);
  }

  /* Get NEWT PV values */
  index = -1;
  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      if (ca_get(DBR_SHORT, pvinfo[index].chid_newt, &pvinfo[index].value_newt)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_newt);
        return(1);
      }
    }
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_get\n");
    return(1);
  }

  /* Set SWAP PV values to 1 */
  index = -1;
  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      if (pvinfo[index].value_newt == 0)
        continue;
      if (ca_put(DBR_SHORT, pvinfo[index].chid_swap, &setswapval)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_put for %s\n", pvinfo[index].name_swap);
        return(1);
      }
    }
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_put\n");
    return(1);
  }

  /* Wait for all NEWT PV values to equal 0 */
  for (i=0; i <= LIGHTWAITTIME; i++) {
    index = -1;
    for (iplane=0; iplane < 2; iplane++) {
      for (icor=0; icor < ncor; icor++) {
        index++;
        if (pvinfo[index].value_newt == 0)
          continue;
        if (ca_get(DBR_SHORT, pvinfo[index].chid_newt, &pvinfo[index].value_newt)!=ECA_NORMAL) {
          fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_newt);
          return(1);
        }
      }
    }
    if (ca_pend_io(10)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get\n");
      return(1);
    }
    wait = 0;
    index = -1;
    for (iplane=0; iplane < 2; iplane++) {
      for (icor=0; icor < ncor; icor++) {
        index++;
        if (pvinfo[index].value_newt == 0)
          continue;
        wait = 1;
        if (i == LIGHTWAITTIME) {
          fprintf(stderr, "Light for %s did not turn Black\n", pvinfo[index].name_newt);
          return(1);
        }
        break;
      }
    }
    if (wait == 0)
      break;
    ca_pend_event(1);
  }

  /* Wait for all PACT PV values to equal 0 */
  for (i=0; i <= LIGHTWAITTIME; i++) {
    index = -1;
    for (iplane=0; iplane < 2; iplane++) {
      for (icor=0; icor < ncor; icor++) {
        index++;
        if (ca_get(DBR_SHORT, pvinfo[index].chid_pact, &pvinfo[index].value_pact)!=ECA_NORMAL) {
          fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_pact);
          return(1);
        }
      }
    }
    if (ca_pend_io(10)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get\n");
      return(1);
    }
    wait = 0;
    index = -1;
    for (iplane=0; iplane < 2; iplane++) {
      for (icor=0; icor < ncor; icor++) {
        index++;
        if (pvinfo[index].value_pact == 0)
          continue;
        wait = 1;
        if (i == LIGHTWAITTIME) {
          fprintf(stderr, "%s did not turn to 0\n", pvinfo[index].name_pact);
          return(1);
        }
        break;
      }
    }
    if (wait == 0)
      break;
    ca_pend_event(1);
  }

  /* Check STAT and SEVR PV values */
  index = -1;
  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      if (ca_get(DBR_SHORT, pvinfo[index].chid_stat, &pvinfo[index].value_stat)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_stat);
        return(1);
      }
      if (ca_get(DBR_SHORT, pvinfo[index].chid_sevr, &pvinfo[index].value_sevr)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_sevr);
        return(1);
      }
      if (ca_get(DBR_STRING, pvinfo[index].chid_stat, &pvinfo[index].enum_value_stat)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_stat);
        return(1);
      }
      if (ca_get(DBR_STRING, pvinfo[index].chid_sevr, &pvinfo[index].enum_value_sevr)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvinfo[index].name_sevr);
        return(1);
      }
    }
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_get\n");
    return(1);
  }
  index = -1;
  for (iplane=0; iplane < 2; iplane++) {
    for (icor=0; icor < ncor; icor++) {
      index++;
      if ((pvinfo[index].value_stat != 0) || (pvinfo[index].value_sevr != 0)) {
        fprintf(stdout, "%s ALARM: %s(%d) %s(%d)\n", corname[index], 
                pvinfo[index].enum_value_stat, pvinfo[index].value_stat, 
                pvinfo[index].enum_value_sevr, pvinfo[index].value_sevr);
      }
    }
  }

  return(0);
}

int getbpmtime(double *bpmtime, double *bpmduration) {
  struct BTIMING btiming;
  int err;
    
  err = gettiming(&btiming);
  *bpmtime = btiming.bpmtime;
  *bpmduration = btiming.bpmduration;
  return err;
}

int gettiming(struct BTIMING *btiming)
     /* crampstart is for correctors */
     /* rampstartref, crampstart, boosterip, bpmstart are relative to LastBunch */
     /* crampstartdelay, dipolezeroref, boostertrigger, rampstartoffset are intervals */
     /* bpmtime is relative to crampstart */     
{
  int i;
  long n[NSCANNERS],m[NSCANNERS];
  static char id[NSCANNERS][6]={"1_8","9_16","17_24","25_32","33_40",
                                "41_48","49_56","57_64","65_72","73_80"};
  struct timingpvs pvtiming[NSCANNERS];
  struct moretimingpvs pvdata;

  for(i=0; i < NSCANNERS; i++) {
    sprintf(pvtiming[i].name_delaycount, "B:bpmT%s:Delay_count_li", id[i]);
    sprintf(pvtiming[i].name_cyclecount, "B:bpmT%s:Cycle_count_li", id[i]);
    if ((ca_search(pvtiming[i].name_delaycount, &pvtiming[i].chid_delaycount)!=ECA_NORMAL) ||
        (ca_search(pvtiming[i].name_cyclecount, &pvtiming[i].chid_cyclecount)!=ECA_NORMAL)) {
      fprintf(stderr,"problem doing ca_search for %s and/or %s\n", pvtiming[i].name_delaycount, pvtiming[i].name_cyclecount);
      return(1);     
    }
  }
  sprintf(pvdata.name_Hp8657RefFreq,"BRF:S:Hp8657RefFreq");
  sprintf(pvdata.name_LastBunch2BoosterIp,"It:Bs:LastBunch2BoosterIp");
  sprintf(pvdata.name_Trig2BsIp,"It:Bs:Trig2BsIp");
  sprintf(pvdata.name_StartRamp2BsIp,"It:Bs:StartRamp2BsIp");
  sprintf(pvdata.name_Ddg4chan6,"It:Ddg4chan6.DLY");
  sprintf(pvdata.name_ZeroRefAO,"B:BM:ZeroRefAO");
  sprintf(pvdata.name_Ddg8chan6,"It:Ddg8chan6.DLY");
  sprintf(pvdata.name_Ddg3chan6,"It:Ddg3chan6.DLY");
  sprintf(pvdata.name_Ddg3chan3,"It:Ddg3chan3.DLY");
  sprintf(pvdata.name_BoosterRampTurnsBsCC,"Mt:BoosterRampTurnsBsCC");
  sprintf(pvdata.name_memscan_wt_ao,"B:bpm:memscan_wt_ao");

  if ((ca_search(pvdata.name_Hp8657RefFreq, &pvdata.chid_Hp8657RefFreq)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_LastBunch2BoosterIp, &pvdata.chid_LastBunch2BoosterIp)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_Trig2BsIp, &pvdata.chid_Trig2BsIp)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_StartRamp2BsIp, &pvdata.chid_StartRamp2BsIp)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_Ddg4chan6, &pvdata.chid_Ddg4chan6)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_ZeroRefAO, &pvdata.chid_ZeroRefAO)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_Ddg8chan6, &pvdata.chid_Ddg8chan6)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_Ddg3chan6, &pvdata.chid_Ddg3chan6)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_Ddg3chan3, &pvdata.chid_Ddg3chan3)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_BoosterRampTurnsBsCC, &pvdata.chid_BoosterRampTurnsBsCC)!=ECA_NORMAL) ||
      (ca_search(pvdata.name_memscan_wt_ao, &pvdata.chid_memscan_wt_ao)!=ECA_NORMAL)) {
    fprintf(stderr, "problem doing ca_search for delay pvs.\n");
    return(1);     
  }
  if (ca_pend_io(30)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_search for bpm timing pvs.\n");
    return(1);
  }

  for(i=0; i < NSCANNERS; i++) {
    if ((ca_get(DBR_LONG, pvtiming[i].chid_delaycount, &pvtiming[i].value_delaycount)!=ECA_NORMAL) ||
        (ca_get(DBR_LONG, pvtiming[i].chid_cyclecount, &pvtiming[i].value_cyclecount)!=ECA_NORMAL)) {
      fprintf(stderr, "problem doing ca_get for %s and/or %s\n", pvtiming[i].name_delaycount, pvtiming[i].name_cyclecount);
      return(1);
    }
  }
  if ((ca_get(DBR_DOUBLE, pvdata.chid_Hp8657RefFreq, &pvdata.value_Hp8657RefFreq)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_LastBunch2BoosterIp, &pvdata.value_LastBunch2BoosterIp)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_Trig2BsIp, &pvdata.value_Trig2BsIp)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_StartRamp2BsIp, &pvdata.value_StartRamp2BsIp)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_Ddg4chan6, &pvdata.value_Ddg4chan6)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_ZeroRefAO, &pvdata.value_ZeroRefAO)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_Ddg8chan6, &pvdata.value_Ddg8chan6)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_Ddg3chan6, &pvdata.value_Ddg3chan6)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_Ddg3chan3, &pvdata.value_Ddg3chan3)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_BoosterRampTurnsBsCC, &pvdata.value_BoosterRampTurnsBsCC)!=ECA_NORMAL) ||
      (ca_get(DBR_DOUBLE, pvdata.chid_memscan_wt_ao, &pvdata.value_memscan_wt_ao)!=ECA_NORMAL)) {
    fprintf(stderr, "problem doing ca_get\n");
    return(1);
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_get\n");
    return(1);
  }
  for(i=0; i < NSCANNERS; i++) {
    n[i] = pvtiming[i].value_delaycount;
    m[i] = pvtiming[i].value_cyclecount;
  }
  btiming->rffreq = pvdata.value_Hp8657RefFreq;
  btiming->goaround = (double)BOOSTERHARMONIC / pvdata.value_Hp8657RefFreq * 1000.;
  btiming->boosterip = pvdata.value_LastBunch2BoosterIp;
  btiming->boostertrigger = pvdata.value_Trig2BsIp / 1000.;
  btiming->rampstartoffset = pvdata.value_StartRamp2BsIp;
  btiming->rampstartref = btiming->boosterip - pvdata.value_StartRamp2BsIp;
  btiming->crampstartdelay = pvdata.value_Ddg4chan6;
  btiming->dipolezeroref = pvdata.value_ZeroRefAO;
  btiming->crampstart = btiming->rampstartref + btiming->crampstartdelay;
  btiming->drampstartdelay = pvdata.value_Ddg8chan6;
  btiming->bpmoffset = pvdata.value_Ddg3chan6;
  btiming->pscusampletime = pvdata.value_Ddg3chan3 / 1000. + btiming->boosterip - btiming->boostertrigger;

  /* Calculate BPM values */
  btiming->bpmn = (n[0]);
  btiming->bpmm = (m[0]);
  btiming->bpmdelay = (double)(4*n[0])*(btiming->goaround);
  btiming->bpmduration = (double)(4*m[0])*(btiming->goaround);
  btiming->bpmstart = btiming->boosterip - btiming->boostertrigger + btiming->bpmoffset + btiming->bpmdelay;
  btiming->bpmtime = btiming->bpmstart + .5 * (btiming->bpmduration) - btiming->crampstart;

  btiming->boosternturns = pvdata.value_BoosterRampTurnsBsCC;
  btiming->averagerweight = pvdata.value_memscan_wt_ao + .5;
  btiming->averagernturns = pow(2.,(double)btiming->averagerweight) + .5;

  /* Calculate duration and extraction values */
  btiming->boosterduration = btiming->boosternturns * (btiming->goaround);
  btiming->boosterep = btiming->boosterip + btiming->boosterduration;

  return(0);
}

char *timestamp(void) {
  /* Gets current time and puts it in a static array    */
  /* The calling program should copy it to a safe place */
  /*    e.g. strcpy(savetime,timestamp());              */
  struct timeval tv;
  struct timezone tz;
  time_t clock;
  static char time[26];

  gettimeofday(&tv,&tz);
  clock=tv.tv_sec;
  strcpy(time,ctime(&clock));
  time[24]='\0';
  return time;
}

void writerampheader(FILE *file) {
  fprintf(file,"%s\n",SDDSID);
  fprintf(file,
          "&description text=\"Booster Ramp Table\""
          " contents=\"booster ramp\" &end\n");
  fprintf(file,
          "&parameter name=TimeStamp type=string &end\n");
  fprintf(file,
          "&parameter name=BPMTime type=double &end\n");
  fprintf(file,
          "&parameter name=BPMDuration type=double &end\n");
  fprintf(file,
          "&parameter name=ControlName symbol=ControlName type=string &end\n");
  fprintf(file,
          "&parameter name=ControlType symbol=ControlType type=string &end\n");
  fprintf(file,
          "&parameter name=RampBias symbol=RampBias type=double &end\n");
  fprintf(file,
          "&parameter name=CorrName type=string &end\n");
  fprintf(file,
          "&parameter name=CorrCurrentAO type=string &end\n");
  fprintf(file,
          "&column name=RampTime symbol=RampTime type=double &end\n");
  fprintf(file,
          "&column name=RampSetpoint symbol=RampSetpoint type=double &end\n");
  fprintf(file,
          "&data mode=\"ascii\" &end\n");
}

int writerampdata(FILE *file, double val, char *corname, double rampbias,
                  double bpmtime, double bpmduration, int usebpmtime, char *timestamp) {
  double zero=0.0;
  double scaledval1,scaledval2;
  double bpmstart,bpmend;
  double ontime=ONTIME, offtime=OFFTIME, endtime=ENDTIME;

  /* Write file */
  fprintf(file, "%s\n", timestamp);
  fprintf(file, "%f\n", bpmtime);
  fprintf(file, "%f\n", bpmduration);
  fprintf(file, "%s:rampRAMPTABLE\n", corname);
  fprintf(file, "pv\n");
  fprintf(file, "%f     ! Ramp bias\n", rampbias);
  fprintf(file, "%s\n", corname);
  fprintf(file, "%s:CurrentAO\n", corname);
  if (usebpmtime) {
    bpmstart = bpmtime - .5 *bpmduration;
    bpmend = bpmtime + .5 * bpmduration;
    scaledval1 = (bpmstart - ontime) / (offtime - ontime) * val;
    scaledval2 = (bpmend - ontime) / (offtime - ontime) * val;
    if (bpmstart <= ontime || bpmend >= offtime) {
      fprintf(stderr, "The BPMInterval is not between the OnTime and the OffTime for %s:\n  OnTime:   %f\n  BPMStart: %f\n  BPMTime: %f\n  BPMEND: %f\n  OFFTime:  %f\n", corname,ontime,bpmstart,bpmtime,bpmtime,offtime);
      fprintf(file, "2       ! number of rows\n");     /* Error with BPM time */
      fprintf(file, "%f %f\n", zero, zero);
      fprintf(file, "%f %f\n", endtime, zero);
    } else {     /* BPM time */
      if (ontime == zero) {
        fprintf(file, "5       ! number of rows\n");
      } else {
        fprintf(file, "6       ! number of rows\n");
        fprintf(file, "%f %f\n", zero, zero);
      }
      fprintf(file, "%f %f\n", ontime, zero);
      fprintf(file, "%f %f\n", bpmstart, scaledval1/HIGH);
      fprintf(file, "%f %f\n", bpmend, scaledval2/HIGH);
      fprintf(file, "%f %f\n", offtime, zero);
      fprintf(file, "%f %f\n", endtime, zero);
    }
  } else {      /* Full ramp */
    if (ontime == zero) {
      fprintf(file, "3       ! number of rows\n");
    } else {
      fprintf(file, "6       ! number of rows\n");
      fprintf(file, "%f %f\n", zero, zero);
    }
    fprintf(file, "%f %f\n", ontime, zero);
    fprintf(file, "%f %f\n", offtime, val/HIGH);
    fprintf(file, "%f %f\n", endtime, val/HIGH);
  }
  return(0);
}

int setbcorr(char *corname,double val, double rampbias, int usebpmtime,
             int save, int append, int rampload, int setfg, char *filename) {
  FILE *file;
  double bpmtime=0.,bpmduration=0.;
  char timestring[26];
  char *templet=FILETEMPLET;
  char lfilename[BUFSIZ];     /* Need a copy to avoid mktemp clobbering it */
  int err,returncode=0;
  struct pvs pvcor;
  int i;
  short setswapval=1;
  char string[BUFSIZ];


  /* Make temporary file */
  if (!save) {
    strcpy(lfilename, templet);
    mktemp(lfilename);
  } else {
    strcpy(lfilename, filename);
  }

  /* Open file */
  if (!append) 
    file=fopen(lfilename, "w");
  else 
    file=fopen(lfilename, "a");
  if (file == NULL) {
    fprintf(stderr, "Unable to open %s\n", lfilename);
    return(1);
  }

  /* Get timing */
  if (getbpmtime(&bpmtime, &bpmduration)) {
    fprintf(stderr, "Could not get BPM timing\n");
    return(1);
  }

  /* Get timestamp */
  strcpy(timestring, timestamp());

  /* Write the data */
  if (!append) 
    writerampheader(file);
  if (writerampdata(file, val, corname, rampbias,
                    bpmtime, bpmduration, usebpmtime, timestring)) {
    fclose(file);
    return(1);
  }
  fclose(file);

  sprintf(pvcor.name_newt, "%s:rampRAMPTABLE.NEWT", corname);
  sprintf(pvcor.name_pact, "%s:rampRAMPTABLE.PACT", corname);
  sprintf(pvcor.name_swap, "%s:rampRAMPTABLE.SWAP", corname);
  sprintf(pvcor.name_stat, "%s:rampRAMPTABLE.STAT", corname);
  sprintf(pvcor.name_sevr, "%s:rampRAMPTABLE.SEVR", corname);
  sprintf(pvcor.name_CurrentAO, "%s:CurrentAO",corname);
  if ((ca_search(pvcor.name_newt, &pvcor.chid_newt)!=ECA_NORMAL) ||
      (ca_search(pvcor.name_pact, &pvcor.chid_pact)!=ECA_NORMAL) ||
      (ca_search(pvcor.name_swap, &pvcor.chid_swap)!=ECA_NORMAL) ||
      (ca_search(pvcor.name_stat, &pvcor.chid_stat)!=ECA_NORMAL) ||
      (ca_search(pvcor.name_sevr, &pvcor.chid_sevr)!=ECA_NORMAL) ||
      (ca_search(pvcor.name_CurrentAO, &pvcor.chid_CurrentAO)!=ECA_NORMAL)) {
    fprintf(stderr,"problem doing ca_search\n");
    returncode = 1;
    goto QUIT;
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_search\n");
    returncode = 1;
    goto QUIT;
  }

  /* Run Rampload command */
  if (rampload) {
    sprintf(string, "rampload %s", lfilename);
    err = system(string);
    if (err>>8) {
      fprintf(stderr, "Could not run Rampload\n");
      returncode = 1;
      goto QUIT;
    }

    /* Wait for all NEWT PV values to equal 1 */
    for (i=0; i <= LIGHTWAITTIME; i++) {
      if (ca_get(DBR_SHORT, pvcor.chid_newt, &pvcor.value_newt)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_newt);
        returncode = 1;
        goto QUIT;
      }
      if (ca_pend_io(10) != ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get\n");
        returncode = 1;
        goto QUIT;
      }
      if (pvcor.value_newt == 1)
        break;
      if (i == LIGHTWAITTIME) {
        fprintf(stderr, "Light for %s did not turn Green\n", pvcor.name_newt);
        returncode = 1;
        goto QUIT;
      }
      ca_pend_event(1);
    }
    
    /* Wait for all PACT PV values to equal 0 */
    for (i=0; i <= LIGHTWAITTIME; i++) {
      if (ca_get(DBR_SHORT, pvcor.chid_pact, &pvcor.value_pact)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_pact);
        returncode = 1;
        goto QUIT;
      }
      if (ca_pend_io(10)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get\n");
        returncode = 1;
        goto QUIT;
      }
      if (pvcor.value_pact == 0)
        break;
      if (i == LIGHTWAITTIME) {
        fprintf(stderr, "%s did not turn to 0\n", pvcor.name_pact);
        returncode = 1;
        goto QUIT;
      }
      ca_pend_event(1);
    }
  }

  /* Set CurrentAO */
  if (ca_put(DBR_DOUBLE, pvcor.chid_CurrentAO, &val)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_put for %s\n", pvcor.name_CurrentAO);
    returncode = 1;
    goto QUIT;
  }
  if (ca_pend_io(10)!=ECA_NORMAL) {
    fprintf(stderr, "problem doing ca_put\n");
    returncode = 1;
    goto QUIT;
  }

  /* Run foreground load command */
  if (setfg) {
    if (ca_put(DBR_SHORT, pvcor.chid_swap, &setswapval)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_put for %s\n", pvcor.name_swap);
      returncode = 1;
      goto QUIT;
    }
    if (ca_pend_io(10)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_put\n");
      returncode = 1;
      goto QUIT;
    }

    /* Wait for all NEWT PV values to equal 0 */
    for (i=0; i <= LIGHTWAITTIME; i++) {
      if (ca_get(DBR_SHORT, pvcor.chid_newt, &pvcor.value_newt)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_newt);
        returncode = 1;
        goto QUIT;
      }
      if (ca_pend_io(10) != ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get\n");
        returncode = 1;
        goto QUIT;
      }
      if (pvcor.value_newt == 0)
        break;
      if (i == LIGHTWAITTIME) {
        fprintf(stderr, "Light for %s did not turn Black\n", pvcor.name_newt);
        returncode = 1;
        goto QUIT;
      }
      ca_pend_event(1);
    }
    
    /* Wait for all PACT PV values to equal 0 */
    for (i=0; i <= LIGHTWAITTIME; i++) {
      if (ca_get(DBR_SHORT, pvcor.chid_pact, &pvcor.value_pact)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_pact);
        returncode = 1;
        goto QUIT;
      }
      if (ca_pend_io(10)!=ECA_NORMAL) {
        fprintf(stderr, "problem doing ca_get\n");
        returncode = 1;
        goto QUIT;
      }
      if (pvcor.value_pact == 0)
        break;
      if (i == LIGHTWAITTIME) {
        fprintf(stderr, "%s did not turn to 0\n", pvcor.name_pact);
        returncode = 1;
        goto QUIT;
      }
      ca_pend_event(1);
    }
  }
  /* Check alarm */    
  if (rampload || setfg) {
    if (ca_get(DBR_SHORT, pvcor.chid_stat, &pvcor.value_stat)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_stat);
      returncode = 1;
      goto QUIT;
    }
    if (ca_get(DBR_SHORT, pvcor.chid_sevr, &pvcor.value_sevr)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_sevr);
      returncode = 1;
      goto QUIT;
    }
    if (ca_get(DBR_STRING, pvcor.chid_stat, &pvcor.enum_value_stat)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_stat);
      returncode = 1;
      goto QUIT;
    }
    if (ca_get(DBR_STRING, pvcor.chid_sevr, &pvcor.enum_value_sevr)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get for %s\n", pvcor.name_sevr);
      returncode = 1;
      goto QUIT;
    }
    if (ca_pend_io(10)!=ECA_NORMAL) {
      fprintf(stderr, "problem doing ca_get\n");
      returncode = 1;
      goto QUIT;
    }
    if ((pvcor.value_stat != 0) || (pvcor.value_sevr != 0)) {
      fprintf(stdout, "%s ALARM: %s(%d) %s(%d)\n", corname, 
              pvcor.enum_value_stat, pvcor.value_stat, 
              pvcor.enum_value_sevr, pvcor.value_sevr);
    }
  }
  /* Clean up */
 QUIT:    
  if (!save) {
    sprintf(string, "\\rm -f %s", lfilename);
    err = system(string);
    if (err>>8) {
      fprintf(stderr, "Could not delete temporary file %s\n", lfilename);
    }
  }
  return(returncode);
}
