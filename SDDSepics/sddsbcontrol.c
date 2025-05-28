/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/

/* program sddsbcontrol.c
   $Log: not supported by cvs2svn $
   Revision 1.7  2006/07/05 18:24:02  soliday
   Found another spot where runcontrol was being pinged too fast.

   Revision 1.6  2006/06/29 21:15:31  soliday
   Changed the runcontrol pinging so that it only pings every 2 seconds.

   Revision 1.5  2004/09/10 14:37:55  soliday
   Changed the flag for oag_ca_pend_event to a volatile variable

   Revision 1.4  2004/07/22 22:05:17  soliday
   Improved signal support when using Epics/Base 3.14.6

   Revision 1.3  2004/07/19 17:39:36  soliday
   Updated the usage message to include the epics version string.

   Revision 1.2  2004/07/16 21:24:39  soliday
   Replaced sleep commands with ca_pend_event commands because Epics/Base
   3.14.x has an inactivity timer that was causing disconnects from PVs
   when the log interval time was too large.

   Revision 1.1  2003/08/27 19:51:07  soliday
   Moved into subdirectory

   Revision 1.6  2003/04/17 21:01:10  soliday
   Fixed issue when built without runcontrol.

   Revision 1.5  2003/04/17 16:51:35  soliday
   It now updates the run control message PV.

   Revision 1.4  2002/11/27 16:22:05  soliday
   Fixed issues on Windows when built without runcontrol

   Revision 1.3  2002/11/19 22:33:03  soliday
   Altered to work with the newer version of runcontrol.

   Revision 1.2  2002/11/15 21:45:55  soliday
   Fixed some typos after converting from bcontrol

   Revision 1.1  2002/11/14 22:28:06  soliday
   Added sddsbcontrol and fixed a problem with sddsexperiment

   *
   */

#include "mdb.h"
#include "SDDS.h"
#include "SDDSepics.h"
#include <signal.h>

#ifdef USE_RUNCONTROL
#  include "libruncontrol.h"
#endif

#define NSAVES 50
#define NCOEFFS 5
#define RCTIMEOUT 30000
#define PENDIOTIME 10.0
#define TRUE 1
#define FALSE 0
#define FAIL -1

typedef struct
{
  long BMslope;
  long QFslope;
  long QDslope;
  long SFslope;
  long SDslope;
  long BMzero;
  long QFzero;
  long QDzero;
  long SFzero;
  long SDzero;
} status;

typedef struct
{
  double BM;
  double QF;
  double QD;
  double SF;
  double SD;
} magdoubles;

typedef struct
{
  long BM;
  long QF;
  long QD;
  long SF;
  long SD;
} maglongs;

typedef struct
{
  char *PVname;
  chid PVchid;
} PVDATA;

typedef struct
{
  PVDATA BMslopePushedPV;
  PVDATA BMzeroPushedPV;
  PVDATA BMslopeStatPV;
  PVDATA BMzeroStatPV;

  PVDATA QFslopePushedPV;
  PVDATA QFzeroPushedPV;
  PVDATA QFslopeStatPV;
  PVDATA QFzeroStatPV;

  PVDATA QDslopePushedPV;
  PVDATA QDzeroPushedPV;
  PVDATA QDslopeStatPV;
  PVDATA QDzeroStatPV;

  PVDATA SFslopePushedPV;
  PVDATA SFzeroPushedPV;
  PVDATA SFslopeStatPV;
  PVDATA SFzeroStatPV;

  PVDATA SDslopePushedPV;
  PVDATA SDzeroPushedPV;
  PVDATA SDslopeStatPV;
  PVDATA SDzeroStatPV;

  PVDATA slopeAvgsPV;
  PVDATA slopeDerivGainPV;
  PVDATA zeroAvgsPV;
  PVDATA zeroDerivGainPV;
  PVDATA slopeGainPV;
  PVDATA zeroGainPV;
  PVDATA lockoutPV;

  PVDATA BMlockoutPV;
  PVDATA QFlockoutPV;
  PVDATA QDlockoutPV;
  PVDATA SFlockoutPV;
  PVDATA SDlockoutPV;

  PVDATA BMslopeDemandPV;
  PVDATA BMslopePV;
  PVDATA BMslopeErrorPV;
  PVDATA BMzeroDemandPV;
  PVDATA BMzeroPV;
  PVDATA BMzeroErrorPV;

  PVDATA QFslopeDemandPV;
  PVDATA QFslopePV;
  PVDATA QFslopeErrorPV;
  PVDATA QFzeroDemandPV;
  PVDATA QFzeroPV;
  PVDATA QFzeroErrorPV;

  PVDATA QDslopeDemandPV;
  PVDATA QDslopePV;
  PVDATA QDslopeErrorPV;
  PVDATA QDzeroDemandPV;
  PVDATA QDzeroPV;
  PVDATA QDzeroErrorPV;

  PVDATA SFslopeDemandPV;
  PVDATA SFslopePV;
  PVDATA SFslopeErrorPV;
  PVDATA SFzeroDemandPV;
  PVDATA SFzeroPV;
  PVDATA SFzeroErrorPV;

  PVDATA SDslopeDemandPV;
  PVDATA SDslopePV;
  PVDATA SDslopeErrorPV;
  PVDATA SDzeroDemandPV;
  PVDATA SDzeroPV;
  PVDATA SDzeroErrorPV;

  PVDATA BMafgAmplitudePV;
  PVDATA QFafgAmplitudePV;
  PVDATA QDafgAmplitudePV;
  PVDATA SFafgAmplitudePV;
  PVDATA SDafgAmplitudePV;

  PVDATA BMafgSwapPV;
  PVDATA QFafgSwapPV;
  PVDATA QDafgSwapPV;
  PVDATA SFafgSwapPV;
  PVDATA SDafgSwapPV;

  PVDATA BMafgNewtPV;
  PVDATA QFafgNewtPV;
  PVDATA QDafgNewtPV;
  PVDATA SFafgNewtPV;
  PVDATA SDafgNewtPV;

  PVDATA BMafgErrcPV;
  PVDATA QFafgErrcPV;
  PVDATA QDafgErrcPV;
  PVDATA SFafgErrcPV;
  PVDATA SDafgErrcPV;

  PVDATA BMstartRampPV;
  PVDATA QFstartRampPV;
  PVDATA QDstartRampPV;
  PVDATA SFstartRampPV;
  PVDATA SDstartRampPV;

  PVDATA PSAFG100SyncPV;

  int BMslopePushed;
  int BMzeroPushed;
  int BMslopeStat;
  int BMzeroStat;

  int QFslopePushed;
  int QFzeroPushed;
  int QFslopeStat;
  int QFzeroStat;

  int QDslopePushed;
  int QDzeroPushed;
  int QDslopeStat;
  int QDzeroStat;

  int SFslopePushed;
  int SFzeroPushed;
  int SFslopeStat;
  int SFzeroStat;

  int SDslopePushed;
  int SDzeroPushed;
  int SDslopeStat;
  int SDzeroStat;

  int BMlockout;
  int QFlockout;
  int QDlockout;
  int SFlockout;
  int SDlockout;

  double lockoutRange;

  double BMslopeDemand;
  double QFslopeDemand;
  double QDslopeDemand;
  double SFslopeDemand;
  double SDslopeDemand;
  double BMzeroDemand;
  double QFzeroDemand;
  double QDzeroDemand;
  double SFzeroDemand;
  double SDzeroDemand;

  double BMstartRamp;
  double QFstartRamp;
  double QDstartRamp;
  double SFstartRamp;
  double SDstartRamp;

  double BMdeltaAmp;
  double QFdeltaAmp;
  double QDdeltaAmp;
  double SFdeltaAmp;
  double SDdeltaAmp;
} BCONTROL;

volatile int sigint = 0;
char rcHandle[256];
long updatedPV = 0;
char *runControlPV = {"B:PSBcontrolRC"};
#ifdef USE_RUNCONTROL
RUNCONTROL_INFO rcInfo;
#endif

void initVariables(BCONTROL *bc);
void interrupt_handler(int sig);
void sigint_interrupt_handler(int sig);
void rc_interrupt_handler();
void quitfunc(char *message, char *qualifier);
int initProcessVariables(BCONTROL *bc);
void EventHandler(struct event_handler_args event);
void checkToggleButton(long buttonPushed, long *status, char *name, short verbose);
void getLoopStatus(status *loop, BCONTROL *bc);
int runControlPingWhileSleep(double sleepTime, double pingInterval);
long processError(double demand, double measured, double *pastRawError, double *pastProcError,
                  long nsaves, double derivGain, long navg, double lockoutRange);
double calculateError(double demand, double measured, double lockoutRange, long *locked);
void fitLine(double *yarray, double *slope, double *intercept, long fitStart, long fitEnd);
void updatePast(double latest, double *past, double npoints);
void endstopAmplitude(double *amplitude);
void endstopStartRamp(double *startRamp);
long loadNewAmplitude(magdoubles ampl, maglongs *load, BCONTROL *bc);

int main(int argc, char *argv[]) {
  long updated, try
    , slopeAvgs, zeroAvgs, i;
  long BMslopeLock, BMzeroLock;
  long QFslopeLock, QFzeroLock;
  long QDslopeLock, QDzeroLock;
  long SFslopeLock, SFzeroLock;
  long SDslopeLock, SDzeroLock;
  double slopeGain, zeroGain, slopeDerivGain, zeroDerivGain;
  double BMzero, QFzero, QDzero, SFzero, SDzero;
  double BMslope, QFslope, QDslope, SFslope, SDslope;
  double BMpastRawSlope[NSAVES], QFpastRawSlope[NSAVES];
  double QDpastRawSlope[NSAVES], SFpastRawSlope[NSAVES], SDpastRawSlope[NSAVES];
  double BMpastRawZero[NSAVES], QFpastRawZero[NSAVES];
  double QDpastRawZero[NSAVES], SFpastRawZero[NSAVES], SDpastRawZero[NSAVES];
  double BMpastProcZero[NSAVES], QFpastProcZero[NSAVES];
  double QDpastProcZero[NSAVES], SFpastProcZero[NSAVES], SDpastProcZero[NSAVES];
  double BMpastProcSlope[NSAVES], QFpastProcSlope[NSAVES];
  double QDpastProcSlope[NSAVES], SFpastProcSlope[NSAVES], SDpastProcSlope[NSAVES];

  status loop;
  magdoubles ampl;
  maglongs load;
  BCONTROL bc;

  for (i = 0; i < NSAVES; i++) {
    BMpastRawSlope[i] = QFpastRawSlope[i] = QDpastRawSlope[i] = SFpastRawSlope[i] = SDpastRawSlope[i] = 0;
    BMpastRawZero[i] = QFpastRawZero[i] = QDpastRawZero[i] = SFpastRawZero[i] = SDpastRawZero[i] = 0;
    BMpastProcSlope[i] = QFpastProcSlope[i] = QDpastProcSlope[i] = SFpastProcSlope[i] = SDpastProcSlope[i] = 0;
    BMpastProcZero[i] = QFpastProcZero[i] = QDpastProcZero[i] = SFpastProcZero[i] = SDpastProcZero[i] = 0;
  }

  fprintf(stdout, "\nBooster Ramp Stability Control Program\n");
  fprintf(stdout, "--------------------------------------\n");
  fprintf(stdout, "Compiled on %s, %s\n", __DATE__, EPICS_VERSION_STRING);

#ifdef EPICS313
  ca_task_initialize();
#else
  if (ca_context_create(ca_enable_preemptive_callback) != ECA_NORMAL) {
    fprintf(stderr, "Unable to initiate channel access context\n");
    return (1);
  }
#endif
#if defined(USE_RUNCONTROL)

  /* cp_str(&runControlPV, "B:PSBcontrolRC");*/
  switch (runControlInit(runControlPV, "Bcontrol", RCTIMEOUT, rcHandle, &rcInfo, PENDIOTIME)) {
  case RUNCONTROL_DENIED:
    quitfunc("runControl permission denied", argv[0]);
    break;
  case RUNCONTROL_ERROR:
    quitfunc("unable to communicate with runControl", argv[0]);
    break;
  case RUNCONTROL_OK:
    fprintf(stdout, "registered with runControl (%s)\n", argv[0]);
    break;
  default:
    quitfunc("unknown runControl error during initialization", argv[0]);
    break;
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

  initVariables(&bc);

  initProcessVariables(&bc);

  /* initialize arrays*/
  loop.BMslope = 0;
  loop.QFslope = 0;
  loop.QDslope = 0;
  loop.SFslope = 0;
  loop.SDslope = 0;
  loop.BMzero = 0;
  loop.QFzero = 0;
  loop.QDzero = 0;
  loop.SFzero = 0;
  loop.SDzero = 0;
  load.BM = 0;
  load.QF = 0;
  load.QD = 0;
  load.SF = 0;
  load.SD = 0;

  if ((ca_add_event(DBR_DOUBLE, bc.BMslopePV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.QFslopePV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.QDslopePV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.SFslopePV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.SDslopePV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.BMzeroPV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.QFzeroPV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.QDzeroPV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.SFzeroPV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL) ||
      (ca_add_event(DBR_DOUBLE, bc.SDzeroPV.PVchid, EventHandler, NULL, NULL) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: unable to setup callback for PVs\n");
    return (1);
  }
  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: unable to setup callback for PVs\n");
    return (1);
  }

  /****************/
  /* loop forever */
  /****************/

  while (1) {

    if (sigint) {
      return (1);
    }

#ifdef USE_RUNCONTROL
    runControlLogMessage(rcHandle, "Running", NO_ALARM, &rcInfo);
#endif
    load.BM = load.SF = load.SD = load.QF = load.QD = FALSE;
    BMslopeLock = BMzeroLock = 0;
    QFslopeLock = QFzeroLock = 0;
    SFslopeLock = SFzeroLock = 0;
    SDslopeLock = SDzeroLock = 0;
    QDslopeLock = QDzeroLock = 0;
    updated = 0;
    try
      = -1;
    /* wait for fit parameters to be updated */
    while ((updated + try) < 6) {
      /* read in required values */
      if ((ca_get(DBR_DOUBLE, bc.BMslopeDemandPV.PVchid, &bc.BMslopeDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.QFslopeDemandPV.PVchid, &bc.QFslopeDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.QDslopeDemandPV.PVchid, &bc.QDslopeDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.SFslopeDemandPV.PVchid, &bc.SFslopeDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.SDslopeDemandPV.PVchid, &bc.SDslopeDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.BMzeroDemandPV.PVchid, &bc.BMzeroDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.QFzeroDemandPV.PVchid, &bc.QFzeroDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.QDzeroDemandPV.PVchid, &bc.QDzeroDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.SFzeroDemandPV.PVchid, &bc.SFzeroDemand) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.SDzeroDemandPV.PVchid, &bc.SDzeroDemand) != ECA_NORMAL) ||
          (ca_get(DBR_LONG, bc.slopeAvgsPV.PVchid, &slopeAvgs) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.slopeDerivGainPV.PVchid, &slopeDerivGain) != ECA_NORMAL) ||
          (ca_get(DBR_LONG, bc.zeroAvgsPV.PVchid, &zeroAvgs) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.zeroDerivGainPV.PVchid, &zeroDerivGain) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.slopeGainPV.PVchid, &slopeGain) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.zeroGainPV.PVchid, &zeroGain) != ECA_NORMAL) ||
          (ca_get(DBR_DOUBLE, bc.lockoutPV.PVchid, &bc.lockoutRange) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_get for PVs\n");
        return (1);
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_get for PVs\n");
        return (1);
      }
      updated = updatedPV;
      updatedPV = 0;

      getLoopStatus(&loop, &bc);
      try
        ++;
    }
    /* read latest fit parameters */
    if ((ca_get(DBR_DOUBLE, bc.BMslopePV.PVchid, &BMslope) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QFslopePV.PVchid, &QFslope) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QDslopePV.PVchid, &QDslope) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SFslopePV.PVchid, &SFslope) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SDslopePV.PVchid, &SDslope) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.BMzeroPV.PVchid, &BMzero) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QFzeroPV.PVchid, &QFzero) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QDzeroPV.PVchid, &QDzero) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SFzeroPV.PVchid, &SFzero) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SDzeroPV.PVchid, &SDzero) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.BMafgAmplitudePV.PVchid, &(ampl.BM)) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QFafgAmplitudePV.PVchid, &(ampl.QF)) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QDafgAmplitudePV.PVchid, &(ampl.QD)) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SFafgAmplitudePV.PVchid, &(ampl.SF)) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SDafgAmplitudePV.PVchid, &(ampl.SD)) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.BMstartRampPV.PVchid, &bc.BMstartRamp) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QFstartRampPV.PVchid, &bc.QFstartRamp) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.QDstartRampPV.PVchid, &bc.QDstartRamp) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SFstartRampPV.PVchid, &bc.SFstartRamp) != ECA_NORMAL) ||
        (ca_get(DBR_DOUBLE, bc.SDstartRampPV.PVchid, &bc.SDstartRamp) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc.BMlockoutPV.PVchid, &bc.BMlockout) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc.QFlockoutPV.PVchid, &bc.QFlockout) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc.QDlockoutPV.PVchid, &bc.QDlockout) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc.SDlockoutPV.PVchid, &bc.SDlockout) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc.SFlockoutPV.PVchid, &bc.SFlockout) != ECA_NORMAL)) {
      fprintf(stderr, "sddsbcontrol: error: problem doing ca_get for PVs\n");
      return (1);
    }
    if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: problem doing ca_get for PVs\n");
      return (1);
    }

    slopeAvgs = slopeAvgs < 2 ? 2 : slopeAvgs;
    zeroAvgs = zeroAvgs < 2 ? 2 : zeroAvgs;
    /* normalize gain to averaging time */
    slopeGain /= slopeAvgs;
    zeroGain /= zeroAvgs;
    slopeDerivGain *= (double)slopeAvgs / 2.0 + 1;
    zeroDerivGain *= (double)zeroAvgs / 2.0 + 1;
    if (!bc.BMlockout) {
      /**** BMslope ****/
      BMslopeLock = processError(bc.BMslopeDemand, BMslope, BMpastRawSlope, BMpastProcSlope,
                                 NSAVES, slopeDerivGain, slopeAvgs, bc.lockoutRange);
      if (loop.BMslope && !BMslopeLock && load.BM != FAIL) {
        ampl.BM += BMpastProcSlope[0] * slopeGain;
        endstopAmplitude(&(ampl.BM));
        load.BM = TRUE;
      } else
        load.BM = FALSE;

      /**** BMzero ****/
      BMzeroLock = processError(bc.BMzeroDemand, BMzero, BMpastRawZero, BMpastProcZero,
                                NSAVES, zeroDerivGain, zeroAvgs, bc.lockoutRange);
      if (loop.BMzero && !BMzeroLock)
        bc.BMstartRamp -= BMpastProcZero[0] * zeroGain;
      endstopStartRamp(&bc.BMstartRamp);
    }

    if (!bc.QFlockout) {
      /**** QFslope ****/
      QFslopeLock = processError(bc.QFslopeDemand, QFslope, QFpastRawSlope, QFpastProcSlope,
                                 NSAVES, slopeDerivGain, slopeAvgs, bc.lockoutRange);
      if (loop.QFslope && !QFslopeLock && load.QF != FAIL) {
        ampl.QF += QFpastProcSlope[0] * slopeGain;
        endstopAmplitude(&(ampl.QF));
        load.QF = TRUE;
      } else {
        load.QF = FALSE;
      }

      /**** QFzero ****/
      QFzeroLock = processError(bc.QFzeroDemand, QFzero, QFpastRawZero, QFpastProcZero,
                                NSAVES, zeroDerivGain, zeroAvgs, bc.lockoutRange);
      if (loop.QFzero && !QFzeroLock)
        bc.QFstartRamp -= QFpastProcZero[0] * zeroGain;
      endstopStartRamp(&bc.QFstartRamp);
    }

    if (!bc.QDlockout) {
      /**** QDslope ****/
      QDslopeLock = processError(bc.QDslopeDemand, QDslope, QDpastRawSlope, QDpastProcSlope,
                                 NSAVES, slopeDerivGain, slopeAvgs, bc.lockoutRange);
      if (loop.QDslope && !QDslopeLock && load.QD != FAIL) {
        ampl.QD += QDpastProcSlope[0] * slopeGain;
        endstopAmplitude(&(ampl.QD));
        load.QD = TRUE;
      } else
        load.QD = FALSE;

      /**** QDzero ****/
      QDzeroLock = processError(bc.QDzeroDemand, QDzero, QDpastRawZero, QDpastProcZero,
                                NSAVES, zeroDerivGain, zeroAvgs, bc.lockoutRange);
      if (loop.QDzero && !QDzeroLock)
        bc.QDstartRamp -= QDpastProcZero[0] * zeroGain;
      endstopStartRamp(&bc.QDstartRamp);
    }
    if (!bc.SFlockout) {
      /**** SFslope ****/
      SFslopeLock = processError(bc.SFslopeDemand, SFslope, SFpastRawSlope, SFpastProcSlope,
                                 NSAVES, slopeDerivGain, slopeAvgs, bc.lockoutRange);
      if (loop.SFslope && !SFslopeLock && load.SF != FAIL) {
        ampl.SF += SFpastProcSlope[0] * slopeGain;
        endstopAmplitude(&(ampl.SF));
        load.SF = TRUE;
      } else
        load.SF = FALSE;

      /**** SFzero ****/
      SFzeroLock = processError(bc.SFzeroDemand, SFzero, SFpastRawZero, SFpastProcZero,
                                NSAVES, zeroDerivGain, zeroAvgs, bc.lockoutRange);
      if (loop.SFzero && !SFzeroLock)
        bc.SFstartRamp -= SFpastProcZero[0] * zeroGain;
      endstopStartRamp(&bc.SFstartRamp);
    }
    if (!bc.SDlockout) {
      /**** SDslope ****/
      SDslopeLock = processError(bc.SDslopeDemand, SDslope, SDpastRawSlope, SDpastProcSlope,
                                 NSAVES, slopeDerivGain, slopeAvgs, bc.lockoutRange);
      if (loop.SDslope && !SDslopeLock && load.SD != FAIL) {
        ampl.SD += SDpastProcSlope[0] * slopeGain;
        endstopAmplitude(&(ampl.SD));
        load.SD = TRUE;
      } else
        load.SD = FALSE;

      /**** SDzero ****/
      SDzeroLock = processError(bc.SDzeroDemand, SDzero, SDpastRawZero, SDpastProcZero,
                                NSAVES, zeroDerivGain, zeroAvgs, bc.lockoutRange);
      if (loop.SDzero && !SDzeroLock)
        bc.SDstartRamp -= SDpastProcZero[0] * zeroGain;
      endstopStartRamp(&bc.SDstartRamp);
    }
    /* load new amplitudes */
    if (!bc.BMlockout || !bc.SFlockout || !bc.SDlockout || !bc.QFlockout || !bc.QDlockout)
      loadNewAmplitude(ampl, &load, &bc);

    if (!bc.BMlockout) {
      bc.BMlockout = BMzeroLock | BMslopeLock;
      if (load.BM == FAIL)
        loop.BMslope = FALSE;
      if ((ca_put(DBR_DOUBLE, bc.BMslopeErrorPV.PVchid, BMpastProcSlope) != ECA_NORMAL) ||
          (ca_put(DBR_DOUBLE, bc.BMzeroErrorPV.PVchid, BMpastProcZero) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for BM slope/zero error PVs\n");
        return (1);
      }
      if (loop.BMzero && !BMzeroLock) {
        if (ca_put(DBR_DOUBLE, bc.BMstartRampPV.PVchid, &bc.BMstartRamp) != ECA_NORMAL) {
          fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for %s\n", bc.BMstartRampPV.PVname);
          return (1);
        }
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for PVs\n");
        return (1);
      }
    }
    if (!bc.QFlockout) {
      bc.QFlockout = QFzeroLock | QFslopeLock;
      if (load.QF == FAIL)
        loop.QFslope = FALSE;
      if ((ca_put(DBR_DOUBLE, bc.QFslopeErrorPV.PVchid, QFpastProcSlope) != ECA_NORMAL) ||
          (ca_put(DBR_DOUBLE, bc.QFzeroErrorPV.PVchid, QFpastProcZero) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for QF slope/error PVs\n");
        return (1);
      }
      if (loop.QFzero && !QFzeroLock) {
        if (ca_put(DBR_DOUBLE, bc.QFstartRampPV.PVchid, &bc.QFstartRamp) != ECA_NORMAL) {
          fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for %s\n", bc.QFstartRampPV.PVname);
          return (1);
        }
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for PVs\n");
        return (1);
      }
    }

    if (!bc.QDlockout) {
      bc.QDlockout = QDzeroLock | QDslopeLock;
      if (load.QD == FAIL)
        loop.QDslope = FALSE;
      if ((ca_put(DBR_DOUBLE, bc.QDslopeErrorPV.PVchid, QDpastProcSlope) != ECA_NORMAL) ||
          (ca_put(DBR_DOUBLE, bc.QDzeroErrorPV.PVchid, QDpastProcZero) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for QD slope/zero error PVs\n");
        return (1);
      }
      if (loop.QDzero && !QDzeroLock) {
        if (ca_put(DBR_DOUBLE, bc.QDstartRampPV.PVchid, &bc.QDstartRamp) != ECA_NORMAL) {
          fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for %s\n", bc.QDstartRampPV.PVname);
          return (1);
        }
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for PVs\n");
        return (1);
      }
    }
    if (!bc.SFlockout) {
      bc.SFlockout = SFzeroLock | SFslopeLock;
      if (load.SF == FAIL)
        loop.SFslope = FALSE;
      if ((ca_put(DBR_DOUBLE, bc.SFslopeErrorPV.PVchid, SFpastProcSlope) != ECA_NORMAL) ||
          (ca_put(DBR_DOUBLE, bc.SFzeroErrorPV.PVchid, SFpastProcZero) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for slope/zero PVs\n");
        return (1);
      }
      if (loop.SFzero && !SFzeroLock) {
        if (ca_put(DBR_DOUBLE, bc.SFstartRampPV.PVchid, &bc.SFstartRamp) != ECA_NORMAL) {
          fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for %s\n", bc.SFstartRampPV.PVname);
          return (1);
        }
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for PVs\n");
        return (1);
      }
    }
    if (!bc.SDlockout) {
      bc.SDlockout = SDzeroLock | SDslopeLock;
      if (load.SD == FAIL)
        loop.SDslope = FALSE;
      if ((ca_put(DBR_DOUBLE, bc.SDslopeErrorPV.PVchid, SDpastProcSlope) != ECA_NORMAL) ||
          (ca_put(DBR_DOUBLE, bc.SDzeroErrorPV.PVchid, SDpastProcZero) != ECA_NORMAL)) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for SD slope/zero error PVs\n");
        return (1);
      }
      if (loop.SDzero && !SDzeroLock) {
        if (ca_put(DBR_DOUBLE, bc.SDstartRampPV.PVchid, &bc.SDstartRamp) != ECA_NORMAL) {
          fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for %s\n", bc.SDstartRampPV.PVname);
          return (1);
        }
      }
      if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: problem doing ca_put for PVs\n");
        return (1);
      }
    }
  }
  return (0);
}

void quitfunc(char *message, char *qualifier) {
  if (qualifier != NULL)
    fprintf(stderr, "error: %s (%s)\n", message, qualifier);
  else
    fprintf(stderr, "error: %s\n", message);
  exit(1);
}

void interrupt_handler(int sig) {
  exit(1);
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
#ifndef EPICS313
  if (ca_current_context() == NULL)
    ca = 0;
  if (ca)
    ca_attach_context(ca_current_context());
#endif
  if (ca) {
#ifdef USE_RUNCONTROL
    switch (runControlExit(rcHandle, &rcInfo)) {
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Error exiting run control.\n");
      break;
    default:
      fprintf(stderr, "Unrecognized error return code from runControlExit.\n");
      break;
    }
#endif
    ca_task_exit();
  }
}

void initVariables(BCONTROL *bc) {

  cp_str(&bc->BMslopePushedPV.PVname, "B:BM:SlopePushMBBO");
  cp_str(&bc->BMzeroPushedPV.PVname, "B:BM:ZeroPushMBBO");
  cp_str(&bc->BMslopeStatPV.PVname, "B:BM:SlopeStatMBBO");
  cp_str(&bc->BMzeroStatPV.PVname, "B:BM:ZeroStatMBBO");

  if ((ca_search(bc->BMslopePushedPV.PVname, &bc->BMslopePushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMzeroPushedPV.PVname, &bc->BMzeroPushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMslopeStatPV.PVname, &bc->BMslopeStatPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMzeroStatPV.PVname, &bc->BMzeroStatPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->QFslopePushedPV.PVname, "B:QF:SlopePushMBBO");
  cp_str(&bc->QFzeroPushedPV.PVname, "B:QF:ZeroPushMBBO");
  cp_str(&bc->QFslopeStatPV.PVname, "B:QF:SlopeStatMBBO");
  cp_str(&bc->QFzeroStatPV.PVname, "B:QF:ZeroStatMBBO");

  if ((ca_search(bc->QFslopePushedPV.PVname, &bc->QFslopePushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFzeroPushedPV.PVname, &bc->QFzeroPushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFslopeStatPV.PVname, &bc->QFslopeStatPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFzeroStatPV.PVname, &bc->QFzeroStatPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->QDslopePushedPV.PVname, "B:QD:SlopePushMBBO");
  cp_str(&bc->QDzeroPushedPV.PVname, "B:QD:ZeroPushMBBO");
  cp_str(&bc->QDslopeStatPV.PVname, "B:QD:SlopeStatMBBO");
  cp_str(&bc->QDzeroStatPV.PVname, "B:QD:ZeroStatMBBO");

  if ((ca_search(bc->QDslopePushedPV.PVname, &bc->QDslopePushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDzeroPushedPV.PVname, &bc->QDzeroPushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDslopeStatPV.PVname, &bc->QDslopeStatPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDzeroStatPV.PVname, &bc->QDzeroStatPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->SFslopePushedPV.PVname, "B:SF:SlopePushMBBO");
  cp_str(&bc->SFzeroPushedPV.PVname, "B:SF:ZeroPushMBBO");
  cp_str(&bc->SFslopeStatPV.PVname, "B:SF:SlopeStatMBBO");
  cp_str(&bc->SFzeroStatPV.PVname, "B:SF:ZeroStatMBBO");

  if ((ca_search(bc->SFslopePushedPV.PVname, &bc->SFslopePushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFzeroPushedPV.PVname, &bc->SFzeroPushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFslopeStatPV.PVname, &bc->SFslopeStatPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFzeroStatPV.PVname, &bc->SFzeroStatPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->SDslopePushedPV.PVname, "B:SD:SlopePushMBBO");
  cp_str(&bc->SDzeroPushedPV.PVname, "B:SD:ZeroPushMBBO");
  cp_str(&bc->SDslopeStatPV.PVname, "B:SD:SlopeStatMBBO");
  cp_str(&bc->SDzeroStatPV.PVname, "B:SD:ZeroStatMBBO");

  if ((ca_search(bc->SDslopePushedPV.PVname, &bc->SDslopePushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDzeroPushedPV.PVname, &bc->SDzeroPushedPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDslopeStatPV.PVname, &bc->SDslopeStatPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDzeroStatPV.PVname, &bc->SDzeroStatPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->slopeAvgsPV.PVname, "B:BconTorrAO");
  cp_str(&bc->slopeDerivGainPV.PVname, "B:BM:Value0AO");
  cp_str(&bc->zeroAvgsPV.PVname, "B:BM:Value1AO");
  cp_str(&bc->zeroDerivGainPV.PVname, "B:BM:Value2AO");
  cp_str(&bc->slopeGainPV.PVname, "B:BconSlopeGainAO");
  cp_str(&bc->zeroGainPV.PVname, "B:BconZeroGainAO");
  cp_str(&bc->lockoutPV.PVname, "B:BconLockoutAO");

  if ((ca_search(bc->slopeAvgsPV.PVname, &bc->slopeAvgsPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->slopeDerivGainPV.PVname, &bc->slopeDerivGainPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->zeroAvgsPV.PVname, &bc->zeroAvgsPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->zeroDerivGainPV.PVname, &bc->zeroDerivGainPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->slopeGainPV.PVname, &bc->slopeGainPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->zeroGainPV.PVname, &bc->zeroGainPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->lockoutPV.PVname, &bc->lockoutPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMlockoutPV.PVname, "B:BM:BconLockedMBBO");
  cp_str(&bc->QFlockoutPV.PVname, "B:QF:BconLockedMBBO");
  cp_str(&bc->QDlockoutPV.PVname, "B:QD:BconLockedMBBO");
  cp_str(&bc->SFlockoutPV.PVname, "B:SF:BconLockedMBBO");
  cp_str(&bc->SDlockoutPV.PVname, "B:SD:BconLockedMBBO");

  if ((ca_search(bc->BMlockoutPV.PVname, &bc->BMlockoutPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFlockoutPV.PVname, &bc->QFlockoutPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDlockoutPV.PVname, &bc->QDlockoutPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFlockoutPV.PVname, &bc->SFlockoutPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDlockoutPV.PVname, &bc->SDlockoutPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMslopeDemandPV.PVname, "B:BM:SlopeRefAO");
  cp_str(&bc->BMslopePV.PVname, "B:BM:RampSlopeAI");
  cp_str(&bc->BMslopeErrorPV.PVname, "B:BM:SlopeErrAO");
  cp_str(&bc->BMzeroDemandPV.PVname, "B:BM:ZeroRefAO");
  cp_str(&bc->BMzeroPV.PVname, "B:BM:RampZeroAI");
  cp_str(&bc->BMzeroErrorPV.PVname, "B:BM:ZeroErrAO");

  if ((ca_search(bc->BMslopeDemandPV.PVname, &bc->BMslopeDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMslopePV.PVname, &bc->BMslopePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMslopeErrorPV.PVname, &bc->BMslopeErrorPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMzeroDemandPV.PVname, &bc->BMzeroDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMzeroPV.PVname, &bc->BMzeroPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->BMzeroErrorPV.PVname, &bc->BMzeroErrorPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->QFslopeDemandPV.PVname, "B:QF:SlopeRefAO");
  cp_str(&bc->QFslopePV.PVname, "B:QF:RampSlopeAI");
  cp_str(&bc->QFslopeErrorPV.PVname, "B:QF:SlopeErrAO");
  cp_str(&bc->QFzeroDemandPV.PVname, "B:QF:ZeroRefAO");
  cp_str(&bc->QFzeroPV.PVname, "B:QF:RampZeroAI");
  cp_str(&bc->QFzeroErrorPV.PVname, "B:QF:ZeroErrAO");

  if ((ca_search(bc->QFslopeDemandPV.PVname, &bc->QFslopeDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFslopePV.PVname, &bc->QFslopePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFslopeErrorPV.PVname, &bc->QFslopeErrorPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFzeroDemandPV.PVname, &bc->QFzeroDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFzeroPV.PVname, &bc->QFzeroPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFzeroErrorPV.PVname, &bc->QFzeroErrorPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->QDslopeDemandPV.PVname, "B:QD:SlopeRefAO");
  cp_str(&bc->QDslopePV.PVname, "B:QD:RampSlopeAI");
  cp_str(&bc->QDslopeErrorPV.PVname, "B:QD:SlopeErrAO");
  cp_str(&bc->QDzeroDemandPV.PVname, "B:QD:ZeroRefAO");
  cp_str(&bc->QDzeroPV.PVname, "B:QD:RampZeroAI");
  cp_str(&bc->QDzeroErrorPV.PVname, "B:QD:ZeroErrAO");

  if ((ca_search(bc->QDslopeDemandPV.PVname, &bc->QDslopeDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDslopePV.PVname, &bc->QDslopePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDslopeErrorPV.PVname, &bc->QDslopeErrorPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDzeroDemandPV.PVname, &bc->QDzeroDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDzeroPV.PVname, &bc->QDzeroPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDzeroErrorPV.PVname, &bc->QDzeroErrorPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->SFslopeDemandPV.PVname, "B:SF:SlopeRefAO");
  cp_str(&bc->SFslopePV.PVname, "B:SF:RampSlopeAI");
  cp_str(&bc->SFslopeErrorPV.PVname, "B:SF:SlopeErrAO");
  cp_str(&bc->SFzeroDemandPV.PVname, "B:SF:ZeroRefAO");
  cp_str(&bc->SFzeroPV.PVname, "B:SF:RampZeroAI");
  cp_str(&bc->SFzeroErrorPV.PVname, "B:SF:ZeroErrAO");

  if ((ca_search(bc->SFslopeDemandPV.PVname, &bc->SFslopeDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFslopePV.PVname, &bc->SFslopePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFslopeErrorPV.PVname, &bc->SFslopeErrorPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFzeroDemandPV.PVname, &bc->SFzeroDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFzeroPV.PVname, &bc->SFzeroPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFzeroErrorPV.PVname, &bc->SFzeroErrorPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->SDslopeDemandPV.PVname, "B:SD:SlopeRefAO");
  cp_str(&bc->SDslopePV.PVname, "B:SD:RampSlopeAI");
  cp_str(&bc->SDslopeErrorPV.PVname, "B:SD:SlopeErrAO");
  cp_str(&bc->SDzeroDemandPV.PVname, "B:SD:ZeroRefAO");
  cp_str(&bc->SDzeroPV.PVname, "B:SD:RampZeroAI");
  cp_str(&bc->SDzeroErrorPV.PVname, "B:SD:ZeroErrAO");

  if ((ca_search(bc->SDslopeDemandPV.PVname, &bc->SDslopeDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDslopePV.PVname, &bc->SDslopePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDslopeErrorPV.PVname, &bc->SDslopeErrorPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDzeroDemandPV.PVname, &bc->SDzeroDemandPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDzeroPV.PVname, &bc->SDzeroPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDzeroErrorPV.PVname, &bc->SDzeroErrorPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMafgAmplitudePV.PVname, "B:BM:VoltageRT.GAIN");
  cp_str(&bc->QFafgAmplitudePV.PVname, "B:QF:VoltageRT.GAIN");
  cp_str(&bc->QDafgAmplitudePV.PVname, "B:QD:VoltageRT.GAIN");
  cp_str(&bc->SFafgAmplitudePV.PVname, "B:SF:VoltageRT.GAIN");
  cp_str(&bc->SDafgAmplitudePV.PVname, "B:SD:VoltageRT.GAIN");

  if ((ca_search(bc->BMafgAmplitudePV.PVname, &bc->BMafgAmplitudePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFafgAmplitudePV.PVname, &bc->QFafgAmplitudePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDafgAmplitudePV.PVname, &bc->QDafgAmplitudePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFafgAmplitudePV.PVname, &bc->SFafgAmplitudePV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDafgAmplitudePV.PVname, &bc->SDafgAmplitudePV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMafgSwapPV.PVname, "B:BM:VoltageRT.SWAP");
  cp_str(&bc->QFafgSwapPV.PVname, "B:QF:VoltageRT.SWAP");
  cp_str(&bc->QDafgSwapPV.PVname, "B:QD:VoltageRT.SWAP");
  cp_str(&bc->SFafgSwapPV.PVname, "B:SF:VoltageRT.SWAP");
  cp_str(&bc->SDafgSwapPV.PVname, "B:SD:VoltageRT.SWAP");

  if ((ca_search(bc->BMafgSwapPV.PVname, &bc->BMafgSwapPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFafgSwapPV.PVname, &bc->QFafgSwapPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDafgSwapPV.PVname, &bc->QDafgSwapPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFafgSwapPV.PVname, &bc->SFafgSwapPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDafgSwapPV.PVname, &bc->SDafgSwapPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMafgNewtPV.PVname, "B:BM:VoltageRT.NEWT");
  cp_str(&bc->QFafgNewtPV.PVname, "B:QF:VoltageRT.NEWT");
  cp_str(&bc->QDafgNewtPV.PVname, "B:QD:VoltageRT.NEWT");
  cp_str(&bc->SFafgNewtPV.PVname, "B:SF:VoltageRT.NEWT");
  cp_str(&bc->SDafgNewtPV.PVname, "B:SD:VoltageRT.NEWT");

  if ((ca_search(bc->BMafgNewtPV.PVname, &bc->BMafgNewtPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFafgNewtPV.PVname, &bc->QFafgNewtPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDafgNewtPV.PVname, &bc->QDafgNewtPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFafgNewtPV.PVname, &bc->SFafgNewtPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDafgNewtPV.PVname, &bc->SDafgNewtPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMafgErrcPV.PVname, "B:BM:VoltageRT.ERRC");
  cp_str(&bc->QFafgErrcPV.PVname, "B:QF:VoltageRT.ERRC");
  cp_str(&bc->QDafgErrcPV.PVname, "B:QD:VoltageRT.ERRC");
  cp_str(&bc->SFafgErrcPV.PVname, "B:SF:VoltageRT.ERRC");
  cp_str(&bc->SDafgErrcPV.PVname, "B:SD:VoltageRT.ERRC");

  if ((ca_search(bc->BMafgErrcPV.PVname, &bc->BMafgErrcPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFafgErrcPV.PVname, &bc->QFafgErrcPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDafgErrcPV.PVname, &bc->QDafgErrcPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFafgErrcPV.PVname, &bc->SFafgErrcPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDafgErrcPV.PVname, &bc->SDafgErrcPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->BMstartRampPV.PVname, "It:Bs:BmVafgTrigAO.VAL");
  cp_str(&bc->QFstartRampPV.PVname, "It:Bs:QfVafgTrigAO.VAL");
  cp_str(&bc->QDstartRampPV.PVname, "It:Bs:QdVafgTrigAO.VAL");
  cp_str(&bc->SFstartRampPV.PVname, "It:Bs:SfVafgTrigAO.VAL");
  cp_str(&bc->SDstartRampPV.PVname, "It:Bs:SdVafgTrigAO.VAL");

  if ((ca_search(bc->BMstartRampPV.PVname, &bc->BMstartRampPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QFstartRampPV.PVname, &bc->QFstartRampPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->QDstartRampPV.PVname, &bc->QDstartRampPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SFstartRampPV.PVname, &bc->SFstartRampPV.PVchid) != ECA_NORMAL) ||
      (ca_search(bc->SDstartRampPV.PVname, &bc->SDstartRampPV.PVchid) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  cp_str(&bc->PSAFG100SyncPV.PVname, "B:PSAFG100SyncBO");

  if (ca_search(bc->PSAFG100SyncPV.PVname, &bc->PSAFG100SyncPV.PVchid) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) problem doing search for PVs\n");
    exit(1);
  }

  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (initVariables) unable to setup connections for PVs\n");
    exit(1);
  }

  bc->BMslopePushed = 0;
  bc->BMzeroPushed = 0;
  bc->BMslopeStat = 0;
  bc->BMzeroStat = 0;

  bc->QFslopePushed = 0;
  bc->QFzeroPushed = 0;
  bc->QFslopeStat = 0;
  bc->QFzeroStat = 0;

  bc->QDslopePushed = 0;
  bc->QDzeroPushed = 0;
  bc->QDslopeStat = 0;
  bc->QDzeroStat = 0;

  bc->SFslopePushed = 0;
  bc->SFzeroPushed = 0;
  bc->SFslopeStat = 0;
  bc->SFzeroStat = 0;

  bc->SDslopePushed = 0;
  bc->SDzeroPushed = 0;
  bc->SDslopeStat = 0;
  bc->SDzeroStat = 0;

  bc->BMlockout = 0;
  bc->QFlockout = 0;
  bc->QDlockout = 0;
  bc->SFlockout = 0;
  bc->SDlockout = 0;

  bc->lockoutRange = 5.0;

  bc->BMslopeDemand = 0.0;
  bc->QFslopeDemand = 0.0;
  bc->QDslopeDemand = 0.0;
  bc->SFslopeDemand = 0.0;
  bc->SDslopeDemand = 0.0;
  bc->BMzeroDemand = 0.0;
  bc->QFzeroDemand = 0.0;
  bc->QDzeroDemand = 0.0;
  bc->SFzeroDemand = 0.0;
  bc->SDzeroDemand = 0.0;

  bc->BMstartRamp = 0.0;
  bc->QFstartRamp = 0.0;
  bc->QDstartRamp = 0.0;
  bc->SFstartRamp = 0.0;
  bc->SDstartRamp = 0.0;

  bc->BMdeltaAmp = 0.0;
  bc->QFdeltaAmp = 0.0;
  bc->QDdeltaAmp = 0.0;
  bc->SFdeltaAmp = 0.0;
  bc->SDdeltaAmp = 0.0;
}

int initProcessVariables(BCONTROL *bc) {
  if ((ca_put(DBR_LONG, bc->BMslopeStatPV.PVchid, &bc->BMslopeStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->BMslopePushedPV.PVchid, &bc->BMslopePushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->BMzeroStatPV.PVchid, &bc->BMzeroStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->BMzeroPushedPV.PVchid, &bc->BMzeroPushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->BMlockoutPV.PVchid, &bc->BMlockout) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) problem doing ca_put for PVs\n");
    exit(1);
  }
  if ((ca_put(DBR_LONG, bc->QFslopeStatPV.PVchid, &bc->QFslopeStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFslopePushedPV.PVchid, &bc->QFslopePushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFzeroStatPV.PVchid, &bc->QFzeroStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFzeroPushedPV.PVchid, &bc->QFzeroPushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFlockoutPV.PVchid, &bc->QFlockout) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) problem doing ca_put for PVs\n");
    exit(1);
  }
  if ((ca_put(DBR_LONG, bc->QDslopeStatPV.PVchid, &bc->QDslopeStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDslopePushedPV.PVchid, &bc->QDslopePushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDzeroStatPV.PVchid, &bc->QDzeroStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDzeroPushedPV.PVchid, &bc->QDzeroPushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDlockoutPV.PVchid, &bc->QDlockout) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) problem doing ca_put for PVs\n");
    exit(1);
  }
  if ((ca_put(DBR_LONG, bc->SFslopeStatPV.PVchid, &bc->SFslopeStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFslopePushedPV.PVchid, &bc->SFslopePushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFzeroStatPV.PVchid, &bc->SFzeroStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFzeroPushedPV.PVchid, &bc->SFzeroPushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFlockoutPV.PVchid, &bc->SFlockout) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) problem doing ca_put for PVs\n");
    exit(1);
  }
  if ((ca_put(DBR_LONG, bc->SDslopeStatPV.PVchid, &bc->SDslopeStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDslopePushedPV.PVchid, &bc->SDslopePushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDzeroStatPV.PVchid, &bc->SDzeroStat) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDzeroPushedPV.PVchid, &bc->SDzeroPushed) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDlockoutPV.PVchid, &bc->SDlockout) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) problem doing ca_put for PVs\n");
    exit(1);
  }
  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (initProcessVariables) unable to initialize PVs\n");
    exit(1);
  }
  return (0);
}

void EventHandler(struct event_handler_args event) {
  updatedPV++;
}

void getLoopStatus(status *loop, BCONTROL *bc) {
  /* see if the operator has toggled any on/off buttons */
  if ((ca_get(DBR_LONG, bc->BMslopePushedPV.PVchid, &bc->BMslopePushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->BMzeroPushedPV.PVchid, &bc->BMzeroPushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->QFslopePushedPV.PVchid, &bc->QFslopePushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->QFzeroPushedPV.PVchid, &bc->QFzeroPushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->QDslopePushedPV.PVchid, &bc->QDslopePushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->QDzeroPushedPV.PVchid, &bc->QDzeroPushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->SFslopePushedPV.PVchid, &bc->SFslopePushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->SFzeroPushedPV.PVchid, &bc->SFzeroPushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->SDslopePushedPV.PVchid, &bc->SDslopePushed) != ECA_NORMAL) ||
      (ca_get(DBR_LONG, bc->SDzeroPushedPV.PVchid, &bc->SDzeroPushed) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_get for PVs\n");
    exit(1);
  }
  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_get for PVs\n");
    exit(1);
  }

  /* act on any pushed buttons */
  checkToggleButton(bc->BMslopePushed, &(loop->BMslope), "BMslope", 1);
  checkToggleButton(bc->BMzeroPushed, &(loop->BMzero), "BMzero", 1);
  checkToggleButton(bc->QFslopePushed, &(loop->QFslope), "QFslope", 1);
  checkToggleButton(bc->QFzeroPushed, &(loop->QFzero), "QFzero", 1);
  checkToggleButton(bc->QDslopePushed, &(loop->QDslope), "QDslope", 1);
  checkToggleButton(bc->QDzeroPushed, &(loop->QDzero), "QDzero", 1);
  checkToggleButton(bc->SFslopePushed, &(loop->SFslope), "SFslope", 1);
  checkToggleButton(bc->SFzeroPushed, &(loop->SFzero), "SFzero", 1);
  checkToggleButton(bc->SDslopePushed, &(loop->SDslope), "SDslope", 1);
  checkToggleButton(bc->SDzeroPushed, &(loop->SDzero), "SDzero", 1);

  /* reset buttons to zero */
  if (bc->BMslopePushed) {
    bc->BMslopePushed = 0;
    if (ca_put(DBR_LONG, bc->BMslopePushedPV.PVchid, &bc->BMslopePushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->BMslopePushedPV.PVname);
      exit(1);
    }
  };
  if (bc->QFslopePushed) {
    bc->QFslopePushed = 0;
    if (ca_put(DBR_LONG, bc->QFslopePushedPV.PVchid, &bc->QFslopePushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->QFslopePushedPV.PVname);
      exit(1);
    }
  };
  if (bc->QDslopePushed) {
    bc->QDslopePushed = 0;
    if (ca_put(DBR_LONG, bc->QDslopePushedPV.PVchid, &bc->QDslopePushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->QDslopePushedPV.PVname);
      exit(1);
    }
  };
  if (bc->SFslopePushed) {
    bc->SFslopePushed = 0;
    if (ca_put(DBR_LONG, bc->SFslopePushedPV.PVchid, &bc->SFslopePushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->SFslopePushedPV.PVname);
      exit(1);
    }
  };
  if (bc->SDslopePushed) {
    bc->SDslopePushed = 0;
    if (ca_put(DBR_LONG, bc->SDslopePushedPV.PVchid, &bc->SDslopePushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->SDslopePushedPV.PVname);
      exit(1);
    }
  };
  if (bc->BMzeroPushed) {
    bc->BMzeroPushed = 0;
    if (ca_put(DBR_LONG, bc->BMzeroPushedPV.PVchid, &bc->BMzeroPushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->BMzeroPushedPV.PVname);
      exit(1);
    }
  };
  if (bc->QFzeroPushed) {
    bc->QFzeroPushed = 0;
    if (ca_put(DBR_LONG, bc->QFzeroPushedPV.PVchid, &bc->QFzeroPushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->QFzeroPushedPV.PVname);
      exit(1);
    }
  };
  if (bc->QDzeroPushed) {
    bc->QDzeroPushed = 0;
    if (ca_put(DBR_LONG, bc->QDzeroPushedPV.PVchid, &bc->QDzeroPushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->QDzeroPushedPV.PVname);
      exit(1);
    }
  };
  if (bc->SFzeroPushed) {
    bc->SFzeroPushed = 0;
    if (ca_put(DBR_LONG, bc->SFzeroPushedPV.PVchid, &bc->SFzeroPushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->SFzeroPushedPV.PVname);
      exit(1);
    }
  };
  if (bc->SDzeroPushed) {
    bc->SDzeroPushed = 0;
    if (ca_put(DBR_LONG, bc->SDzeroPushedPV.PVchid, &bc->SDzeroPushed) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for %s\n", bc->SDzeroPushedPV.PVname);
      exit(1);
    }
  };
  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for PVs\n");
    exit(1);
  }

  /* set up loop status lights */
  if ((ca_put(DBR_LONG, bc->BMslopeStatPV.PVchid, &(loop->BMslope)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->BMzeroStatPV.PVchid, &(loop->BMzero)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFslopeStatPV.PVchid, &(loop->QFslope)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QFzeroStatPV.PVchid, &(loop->QFzero)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDslopeStatPV.PVchid, &(loop->QDslope)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->QDzeroStatPV.PVchid, &(loop->QDzero)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFslopeStatPV.PVchid, &(loop->SFslope)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SFzeroStatPV.PVchid, &(loop->SFzero)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDslopeStatPV.PVchid, &(loop->SDslope)) != ECA_NORMAL) ||
      (ca_put(DBR_LONG, bc->SDzeroStatPV.PVchid, &(loop->SDzero)) != ECA_NORMAL)) {
    fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for PVs\n");
    exit(1);
  }
  if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (getLoopStatus) problem doing ca_put for PVs\n");
    exit(1);
  }
  /* added this sleep statement to "prevent" a race condition */
  runControlPingWhileSleep(2.0, 3.0);
}

void checkToggleButton(long buttonPushed, long *status, char *name, short verbose) {
  if (buttonPushed == TRUE) {
    if (*status == FALSE) {
      *status = TRUE;
    } else {
      *status = FALSE;
    }
  }
}

int runControlPingWhileSleep(double sleepTime, double pingInterval) {
  double timeLeft, timeSleep;

  timeLeft = sleepTime;
  do {
#if defined(USE_RUNCONTROL)
    switch (runControlPing(rcHandle, &rcInfo)) {
    case RUNCONTROL_ABORT:
      fprintf(stderr, "Run control application aborted.\n");
      exit(0);
      break;
    case RUNCONTROL_TIMEOUT:
      fprintf(stderr, "Run control application timed out.\n");
      exit(1);
      break;
    case RUNCONTROL_OK:
      break;
    case RUNCONTROL_ERROR:
      fprintf(stderr, "Run control error.\n");
      exit(1);
      break;
    default:
      fprintf(stderr, "Unknown run control error code.\n");
      exit(1);
      break;
    }
#endif
    timeSleep = timeLeft < pingInterval ? timeLeft : pingInterval;
    oag_ca_pend_event(timeSleep, &sigint);
    if (sigint)
      exit(1);
    timeLeft -= pingInterval;
  } while (timeLeft > 0);
#if defined(USE_RUNCONTROL)
  return (RUNCONTROL_OK);
#else
  return (0);
#endif
}

long processError(double demand, double measured, double *pastRawError, double *pastProcError,
                  long nsaves, double derivGain, long navg, double lockoutRange) {
  double rawError, mean, deriv, intercept, procError;
  long i, locked;

  rawError = calculateError(demand, measured, lockoutRange, &locked);
  updatePast(rawError, pastRawError, nsaves);
  fitLine(pastRawError, &deriv, &intercept, 0, navg);
  mean = 0;
  for (i = 0; i < navg; i++)
    mean += pastRawError[i];
  mean /= navg;
  procError = mean - derivGain * deriv;
  updatePast(procError, pastProcError, nsaves);
  return locked;
}

double calculateError(double demand, double measured, double lockoutRange, long *locked) {
  double error;
  error = demand - measured;
  if ((measured != 0.0) && (fabs(error) < (fabs(demand) * lockoutRange / 100.0)))
    *locked = 0;
  else {
    error = 0.0;
    *locked = 1;
  }
  return error;
}

void fitLine(double *yarray, double *slope, double *intercept, long fitStart, long fitEnd) {

  long i, npoints;
  double sumX, sumY, sumXY, sumX2;
  double meanX, meanY;

  npoints = fitEnd - fitStart;

  sumX = (double)npoints * ((double)npoints - 1) / 2.0;
  sumY = 0.0;
  sumXY = 0.0;
  sumX2 = 0.0;

  for (i = 0; i < npoints; i++) {
    sumY += yarray[i + fitStart];
    sumXY += (double)i * (double)yarray[i + fitStart];
    sumX2 += (double)i * (double)i;
  }

  meanX = sumX / npoints;
  meanY = sumY / npoints;

  *slope = (npoints * sumXY - sumX * sumY) / (npoints * sumX2 - sumX * sumX);

  if (*slope > 0.00001) {
    *intercept = meanY - *slope * meanX;
    *intercept /= -(*slope);
    *intercept += fitStart;
    if (*intercept <= 0.0)
      *intercept = 0.0;
  } else {
    *slope = 0.0;
    *intercept = 0.0;
  }
}

void updatePast(double latest, double *past, double npoints) {
  long i;

  for (i = npoints - 1; i > 0; i--)
    past[i] = past[i - 1];
  past[0] = latest;
}

void endstopAmplitude(double *amplitude) {
  double lowerLimit = 0.1;
  double upperLimit = 0.999;
  if (*amplitude < lowerLimit)
    *amplitude = lowerLimit;
  if (*amplitude > upperLimit)
    *amplitude = upperLimit;
}

void endstopStartRamp(double *startRamp) {
  double lowerLimit = -20.0;
  double upperLimit = 20.0;
  if (*startRamp < lowerLimit)
    *startRamp = lowerLimit;
  if (*startRamp > upperLimit)
    *startRamp = upperLimit;
}

long loadNewAmplitude(magdoubles ampl, maglongs *load, BCONTROL *bc) {
  /* return values: 0 (successful)  */
  /*                3 (rampload error) */

  long maxTries = 20;
  /* long swap = 1; */
  long mags, try
    , done;
  maglongs newt, err;

  newt.BM = 0;
  newt.QF = 0;
  newt.QD = 0;
  newt.SF = 0;
  newt.SD = 0;

  mags = 0;
  if (load->BM == TRUE) {
    if (ca_put(DBR_DOUBLE, bc->BMafgAmplitudePV.PVchid, &(ampl.BM)) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->BMafgAmplitudePV.PVname);
      exit(1);
    }
    mags++;
  }
  if (load->QF == TRUE) {
    if (ca_put(DBR_DOUBLE, bc->QFafgAmplitudePV.PVchid, &(ampl.QF)) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->QFafgAmplitudePV.PVname);
      exit(1);
    }
    mags++;
  }
  if (load->QD == TRUE) {
    if (ca_put(DBR_DOUBLE, bc->QDafgAmplitudePV.PVchid, &(ampl.QD)) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->QDafgAmplitudePV.PVname);
      exit(1);
    }
    mags++;
  }
  if (load->SF == TRUE) {
    if (ca_put(DBR_DOUBLE, bc->SFafgAmplitudePV.PVchid, &(ampl.SF)) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->SFafgAmplitudePV.PVname);
      exit(1);
    }
    mags++;
  }
  if (load->SD == TRUE) {
    if (ca_put(DBR_DOUBLE, bc->SDafgAmplitudePV.PVchid, &(ampl.SD)) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->SDafgAmplitudePV.PVname);
      exit(1);
    }
    mags++;
  }
  if (mags && ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for PVs\n");
    exit(1);
  }

  try
    = 0;
  done = 0;
  while (done != mags && try < maxTries) {
    done = 0;
    if (load->BM == TRUE) {
      if (ca_get(DBR_LONG, bc->BMafgNewtPV.PVchid, &(newt.BM)) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for %s\n", bc->BMafgSwapPV.PVname);
        exit(1);
      }
    }
    if (load->QF == TRUE) {
      if (ca_get(DBR_LONG, bc->QFafgNewtPV.PVchid, &(newt.QF)) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for %s\n", bc->QFafgSwapPV.PVname);
        exit(1);
      }
    }
    if (load->QD == TRUE) {
      if (ca_get(DBR_LONG, bc->QDafgNewtPV.PVchid, &(newt.QD)) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for %s\n", bc->QDafgSwapPV.PVname);
        exit(1);
      }
    }
    if (load->SF == TRUE) {
      if (ca_get(DBR_LONG, bc->SFafgNewtPV.PVchid, &(newt.SF)) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for %s\n", bc->SFafgSwapPV.PVname);
        exit(1);
      }
    }
    if (load->SD == TRUE) {
      if (ca_get(DBR_LONG, bc->SDafgNewtPV.PVchid, &(newt.SD)) != ECA_NORMAL) {
        fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for %s\n", bc->SDafgSwapPV.PVname);
        exit(1);
      }
    }
    if ((ca_get(DBR_LONG, bc->BMafgErrcPV.PVchid, &(err.BM)) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc->QFafgErrcPV.PVchid, &(err.QF)) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc->QDafgErrcPV.PVchid, &(err.QD)) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc->SFafgErrcPV.PVchid, &(err.SF)) != ECA_NORMAL) ||
        (ca_get(DBR_LONG, bc->SDafgErrcPV.PVchid, &(err.SD)) != ECA_NORMAL)) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for PVs\n");
      exit(1);
    }
    if (ca_pend_io(PENDIOTIME) != ECA_NORMAL) {
      fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_get for PVs\n");
      exit(1);
    }

    /* first verify that there is no rampload error */
    /* then check to see if the table has been loaded */

    if (err.BM) {
      if (load->BM)
        mags--;
      load->BM = FAIL;
    } else if (load->BM && newt.BM) {
      done++;
      load->BM = 0;
    }

    if (err.QF) {
      if (load->QF)
        mags--;
      load->QF = FAIL;
    } else if (load->QF && newt.QF) {
      done++;
      load->QF = 0;
    }

    if (err.QD) {
      if (load->QD)
        mags--;
      load->QD = FAIL;
    } else if (load->QD && newt.QD) {
      done++;
      load->QD = 0;
    }

    if (err.SF) {
      if (load->SF)
        mags--;
      load->SF = FAIL;
    } else if (load->SF && newt.SF) {
      done++;
      load->SF = 0;
    }

    if (err.SD) {
      if (load->SD)
        mags--;
      load->SD = FAIL;
    } else if (load->SD && newt.SD) {
      done++;
      load->SD = 0;
    }

    try
      ++;
    runControlPingWhileSleep(1.0, 2.0);
  }
  if (try == maxTries)
    return 3;

  /*if (newt.BM) {
    if (ca_put(DBR_LONG, bc->BMafgSwapPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->BMafgNewtPV.PVname);
    exit(1);
    }
    } 
    if (newt.QF) {
    if (ca_put(DBR_LONG, bc->QFafgSwapPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->QFafgNewtPV.PVname);
    exit(1);
    }
    } 
    if (newt.QD) {
    if (ca_put(DBR_LONG, bc->QDafgSwapPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->QDafgNewtPV.PVname);
    exit(1);
    }
    }
    if (newt.SF) {
    if (ca_put(DBR_LONG, bc->SFafgSwapPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->SFafgNewtPV.PVname);
    exit(1);
    }
    }
    if (newt.SD) {
    if (ca_put(DBR_LONG, bc->SDafgSwapPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->SDafgNewtPV.PVname);
    exit(1);
    }
    } 
    if ( mags != 0 ) {
    if (ca_put(DBR_LONG, bc->PSAFG100SyncPV.PVchid, &swap)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for %s\n", bc->PSAFG100SyncPV.PVname);
    exit(1);
    }
    } 
    if (ca_pend_io(PENDIOTIME)!=ECA_NORMAL) {
    fprintf(stderr, "sddsbcontrol: error: (loadNewAmplitude) problem doing ca_put for PVs\n");
    exit(1);
    }
  */
  return 0;
}
