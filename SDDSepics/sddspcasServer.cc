/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// fileDescriptorManager.process(delay);
// (the name of the global symbol has leaked in here)
//

//
// Example EPICS CA server
//
#include <complex>
#include "sddspcasServer.h"
#include "mdb.h"
#include "SDDS.h"

//
// static list of pre-created PVs
//
pvInfo *exServer::pvList;
uint32_t exServer::pvListNElem;

//
// exServer::exServer()
//
exServer::exServer(const char *const pvPrefix,
                   uint32_t aliasCount, bool scanOnIn,
                   bool asyncScan, long numInputs, char **input, char *PVFile1,
                   char *PVFile2, double rate) : pTimerQueue(0), simultAsychIOCount(0u),
                                                 scanOn(scanOnIn) {
  uint32_t i;
  exPV *pPV;
  pvInfo *pPVI;
  pvInfo *pPVAfter;
  char pvAlias[256];
  const char *const pNameFmtStr = "%.100s%.40s";
  const char *const pAliasFmtStr = "%.100s%.40s%u";
  double tmp;

  pvListNElem = 0;
  pvList = NULL;

  this->inputfile = input;
  this->inputfiles = numInputs;
  this->masterPVFile = PVFile1;
  this->pcasPVFile = PVFile2;

  //Read PV input file
  this->ReadPVInputFile();

  //Read master PV file
  if (this->masterPVFile != NULL)
    this->ReadMasterPVFile(pvPrefix);

  //Read master sddspcas PV file
  if (this->pcasPVFile != NULL)
    this->ReadMasterSDDSpcasPVFile(pvPrefix);

  //Create PVs
  pvList = new pvInfo[pvListNElem];
  for (unsigned int n = 0; n < pvListNElem; n++) {
    if (this->Types) {
      if (strcmp("char", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumInt8);
      } else if (strcmp("uchar", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumUint8);
      } else if (strcmp("short", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumInt16);
      } else if (strcmp("ushort", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumUint16);
      } else if (strcmp("int", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumInt32);
      } else if (strcmp("uint", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumUint32);
      } else if (strcmp("float", this->Types[n]) == 0) {
        pvList[n].setType(aitEnumFloat32);
      } else if (strcmp("string", this->Types[n]) == 0) {
        if ((this->elementCount) && (this->elementCount[n] > 1)) {
          fprintf(stderr, "Multiple elements with String PVs are not supported in sddspcas yet\n");
          exit(1);
        }
        pvList[n].setType(aitEnumString);
      }
    }
    pvList[n].setName(this->ControlName[n]);
    pvList[n].setIndex(n);
    pvList[n].setScanPeriod(rate);
    if (this->hopr) {
      if ((this->lopr) && (this->hopr[n] < this->lopr[n])) {
        fprintf(stderr, "warning: swapping HOPR and LOPR values for %s\n", this->ControlName[n]);
        tmp = this->lopr[n];
        this->lopr[n] = this->hopr[n];
        this->hopr[n] = tmp;
      }
      pvList[n].setHopr(this->hopr[n]);
    }
    if (this->lopr) {
      pvList[n].setLopr(this->lopr[n]);
    }
    if (this->ReadbackUnits) {
      pvList[n].setUnits(this->ReadbackUnits[n]);
    }
    if (this->elementCount) {
      if (this->elementCount[n] > 1) {
        pvList[n].setElementCount(this->elementCount[n]);
      }
    }
  }

  pPVAfter = &exServer::pvList[pvListNElem];

  exPV::initFT();

  if (asyncScan) {
    uint32_t timerPriotity;
    epicsThreadBooleanStatus etbs = epicsThreadLowestPriorityLevelAbove(
      epicsThreadGetPrioritySelf(), &timerPriotity);
    if (etbs != epicsThreadBooleanStatusSuccess) {
      timerPriotity = epicsThreadGetPrioritySelf();
    }
    this->pTimerQueue = &epicsTimerQueueActive::allocate(false, timerPriotity);
  }

  //
  // pre-create all of the simple PVs that this server will export
  //
  for (unsigned int z = 0; z < pvListNElem; z++) {
    pPVI = getPVInfo(z);
    pPV = pPVI->createPV(*this, true, scanOnIn);

    if (!pPV) {
      fprintf(stderr, "Unable to create new PV \"%s\"\n",
              pPVI->getName());
    }

    //
    // Install canonical (root) name
    //
    sprintf(pvAlias, pNameFmtStr, pvPrefix, pPVI->getName());
    this->installAliasName(*pPVI, pvAlias);

    //
    // Install numbered alias names
    //
    for (i = 0u; i < aliasCount; i++) {
      sprintf(pvAlias, pAliasFmtStr, pvPrefix,
              pPVI->getName(), i);
      this->installAliasName(*pPVI, pvAlias);
    }
  }

  //
  // Install create on-the-fly PVs
  // into the PV name hash table
  //
  /*
    sprintf ( pvAlias, pNameFmtStr, pvPrefix, billy.getName() );
    this->installAliasName ( billy, pvAlias );
    sprintf ( pvAlias, pNameFmtStr, pvPrefix, bloaty.getName() );
    this->installAliasName ( bloaty, pvAlias );
  */

  //Append master sddspcas PV file
  if (this->pcasPVFile != NULL)
    this->AppendMasterSDDSpcasPVFile(pvPrefix);
}

//
// exServer::~exServer()
//
exServer::~exServer() {
  pvInfo *pPVI;
  pvInfo *pPVAfter =
    &exServer::pvList[NELEMENTS(exServer::pvList)];

  //
  // delete all pre-created PVs (eliminate bounds-checker warnings)
  //
  for (pPVI = exServer::pvList; pPVI < pPVAfter; pPVI++) {
    pPVI->deletePV();
  }

  this->stringResTbl.traverse(&pvEntry::destroy);
}

//
// exServer::installAliasName()
//
void exServer::installAliasName(pvInfo &info, const char *pAliasName) {
  pvEntry *pEntry;

  pEntry = new pvEntry(info, *this, pAliasName);
  if (pEntry) {
    int resLibStatus;
    resLibStatus = this->stringResTbl.add(*pEntry);
    if (resLibStatus == 0) {
      return;
    } else {
      delete pEntry;
    }
  }
  fprintf(stderr,
          "Unable to enter PV=\"%s\" Alias=\"%s\" in PV name alias hash table\n",
          info.getName(), pAliasName);
}

//
// More advanced pvExistTest() isn't needed so we forward to
// original version. This avoids sun pro warnings and speeds
// up execution.
//
pvExistReturn exServer::pvExistTest(const casCtx &ctx, const caNetAddr &, const char *pPVName) {
  return this->pvExistTest(ctx, pPVName);
}

//
// exServer::pvExistTest()
//
pvExistReturn exServer::pvExistTest // X aCC 361
  (const casCtx &ctxIn, const char *pPVName) {
  //
  // lifetime of id is shorter than lifetime of pName
  //
  stringId id(pPVName, stringId::refString);
  pvEntry *pPVE;

  //
  // Look in hash table for PV name (or PV alias name)
  //
  pPVE = this->stringResTbl.lookup(id);
  if (!pPVE) {
    return pverDoesNotExistHere;
  }

  pvInfo &pvi = pPVE->getInfo();

  //
  // Initiate async IO if this is an async PV
  //
  if (pvi.getIOType() == excasIoSync) {
    return pverExistsHere;
  } else {
    if (this->simultAsychIOCount >= maxSimultAsyncIO) {
      return pverDoesNotExistHere;
    }

    this->simultAsychIOCount++;

    exAsyncExistIO *pIO;
    pIO = new exAsyncExistIO(pvi, ctxIn, *this);
    if (pIO) {
      return pverAsyncCompletion;
    } else {
      return pverDoesNotExistHere;
    }
  }
}

//
// exServer::pvAttach()
//
pvAttachReturn exServer::pvAttach // X aCC 361
  (const casCtx &ctx, const char *pName) {
  //
  // lifetime of id is shorter than lifetime of pName
  //
  stringId id(pName, stringId::refString);
  exPV *pPV;
  pvEntry *pPVE;

  pPVE = this->stringResTbl.lookup(id);
  if (!pPVE) {
    return S_casApp_pvNotFound;
  }

  pvInfo &pvi = pPVE->getInfo();

  //
  // If this is a synchronous PV create the PV now
  //
  if (pvi.getIOType() == excasIoSync) {
    pPV = pvi.createPV(*this, false, this->scanOn);
    if (pPV) {
      return *pPV;
    } else {
      return S_casApp_noMemory;
    }
  }
  //
  // Initiate async IO if this is an async PV
  //
  else {
    if (this->simultAsychIOCount >= maxSimultAsyncIO) {
      return S_casApp_postponeAsyncIO;
    }

    this->simultAsychIOCount++;

    exAsyncCreateIO *pIO =
      new exAsyncCreateIO(pvi, *this, ctx, this->scanOn);
    if (pIO) {
      return S_casApp_asyncCompletion;
    } else {
      return S_casApp_noMemory;
    }
  }
}

//
// exServer::setDebugLevel ()
//
void exServer::setDebugLevel(uint32_t level) {
  this->caServer::setDebugLevel(level);
}

//
// exServer::createTimer ()
//
class epicsTimer &exServer::createTimer() {
  if (this->pTimerQueue) {
    return this->pTimerQueue->createTimer();
  } else {
    return this->caServer::createTimer();
  }
}

//
// pvInfo::createPV()
//
exPV *pvInfo::createPV(exServer &cas,
                       bool preCreateFlag, bool scanOn) {
  if (this->pPV) {
    return this->pPV;
  }

  exPV *pNewPV;

  //
  // create an instance of the appropriate class
  // depending on the io type and the number
  // of elements
  //
  if (this->elementCount == 1u) {
    switch (this->ioType) {
    case excasIoSync:
      pNewPV = new exScalarPV(cas, *this, preCreateFlag, scanOn);
      break;
    case excasIoAsync:
      pNewPV = new exAsyncPV(cas, *this, preCreateFlag, scanOn);
      break;
    default:
      pNewPV = NULL;
      break;
    }
  } else {
    if (this->ioType == excasIoSync) {
      pNewPV = new exVectorPV(cas, *this, preCreateFlag, scanOn);
    } else {
      pNewPV = NULL;
    }
  }

  //
  // load initial value (this is not done in
  // the constructor because the base class's
  // pure virtual function would be called)
  //
  // We always perform this step even if
  // scanning is disable so that there will
  // always be an initial value
  //
  if (pNewPV) {
    this->pPV = pNewPV;
    pNewPV->scan();
  }

  return pNewPV;
}

//
// exServer::show()
//
void exServer::show(uint32_t level) const {
  //
  // server tool specific show code goes here
  //
  this->stringResTbl.show(level);

  //
  // print information about ca server library
  // internals
  //
  this->caServer::show(level);
}

//
// exAsyncExistIO::exAsyncExistIO()
//
exAsyncExistIO::exAsyncExistIO(const pvInfo &pviIn, const casCtx &ctxIn,
                               exServer &casIn) : casAsyncPVExistIO(ctxIn), pvi(pviIn),
                                                  timer(casIn.createTimer()), cas(casIn) {
  this->timer.start(*this, 0.00001);
}

//
// exAsyncExistIO::~exAsyncExistIO()
//
exAsyncExistIO::~exAsyncExistIO() {
  this->cas.removeIO();
  this->timer.destroy();
}

//
// exAsyncExistIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncExistIO::expire(const epicsTime & /*currentTime*/) {
  //
  // post IO completion
  //
  this->postIOCompletion(pvExistReturn(pverExistsHere));
  return noRestart;
}

//
// exAsyncCreateIO::exAsyncCreateIO()
//
exAsyncCreateIO::exAsyncCreateIO(pvInfo &pviIn, exServer &casIn,
                                 const casCtx &ctxIn, bool scanOnIn) : casAsyncPVAttachIO(ctxIn), pvi(pviIn),
                                                                       timer(casIn.createTimer()),
                                                                       cas(casIn), scanOn(scanOnIn) {
  this->timer.start(*this, 0.00001);
}

//
// exAsyncCreateIO::~exAsyncCreateIO()
//
exAsyncCreateIO::~exAsyncCreateIO() {
  this->cas.removeIO();
  this->timer.destroy();
}

//
// exAsyncCreateIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncCreateIO::expire(const epicsTime & /*currentTime*/) {
  exPV *pPV;

  pPV = this->pvi.createPV(this->cas, false, this->scanOn);
  if (pPV) {
    this->postIOCompletion(pvAttachReturn(*pPV));
  } else {
    this->postIOCompletion(pvAttachReturn(S_casApp_noMemory));
  }
  return noRestart;
}

void exServer::ReadPVInputFile() {
  SDDS_DATASET SDDS_input;
  int hoprFound = 0, loprFound = 0, unitsFound = 0, elementCountFound = 0, typeFound = 0;
  int i, j;
  uint32_t rows;
  this->ControlName = NULL;
  this->ReadbackUnits = NULL;
  this->Types = NULL;
  this->hopr = NULL;
  this->lopr = NULL;
  this->elementCount = NULL;
  char **cn = NULL;
  char **ru = NULL;
  char **ty = NULL;
  double *ho = NULL;
  double *lo = NULL;
  uint32_t *ec = NULL;
  //Read PV input file
  for (i = 0; i < this->inputfiles; i++) {
    hoprFound = 0;
    loprFound = 0;
    unitsFound = 0;
    elementCountFound = 0;
    typeFound = 0;
    if (!SDDS_InitializeInput(&SDDS_input, this->inputfile[i])) {
      fprintf(stderr, "error: Unable to read SDDS input file\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE,
                                SDDS_STRING, "ControlName") == -1) {
      fprintf(stderr, "error: string column ControlName does not exist\n");
      exit(1);
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Hopr") >= 0) {
      hoprFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_NUMERIC_TYPE, (char *)"Lopr") >= 0) {
      loprFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE,
                                SDDS_STRING, "ReadbackUnits") >= 0) {
      unitsFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_INTEGER_TYPE, (char *)"ElementCount") >= 0) {
      elementCountFound = 1;
    }
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE,
                                SDDS_STRING, "Type") >= 0) {
      typeFound = 1;
    }
    if (SDDS_ReadPage(&SDDS_input) != 1) {
      fprintf(stderr, "error: Unable to read SDDS file\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if ((rows = SDDS_RowCount(&SDDS_input)) < 1) {
      fprintf(stderr, "error: No rows found in SDDS file");
      exit(1);
    }
    pvListNElem += rows;
    if (!(cn = (char **)SDDS_GetColumn(&SDDS_input, (char *)"ControlName"))) {
      fprintf(stderr, "error: Unable to read SDDS input file\n");
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    this->ControlName = (char **)trealloc(this->ControlName, pvListNElem * sizeof(*this->ControlName));
    for (j = 0; j < rows; j++) {
      this->ControlName[pvListNElem - rows + j] = cn[j];
    }
    free(cn);

    this->hopr = (double *)trealloc(this->hopr, pvListNElem * sizeof(*this->hopr));
    if (hoprFound) {
      if ((ho = SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Hopr")) == NULL) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->hopr[pvListNElem - rows + j] = ho[j];
      }
      free(ho);
    } else {
      for (j = 0; j < rows; j++) {
        this->hopr[pvListNElem - rows + j] = DBL_MAX;
      }
    }

    this->lopr = (double *)trealloc(this->lopr, pvListNElem * sizeof(*this->lopr));
    if (loprFound) {
      if ((lo = SDDS_GetColumnInDoubles(&SDDS_input, (char *)"Lopr")) == NULL) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->lopr[pvListNElem - rows + j] = lo[j];
      }
      free(lo);
    } else {
      for (j = 0; j < rows; j++) {
        this->lopr[pvListNElem - rows + j] = -DBL_MAX;
      }
    }

    this->ReadbackUnits = (char **)trealloc(this->ReadbackUnits, pvListNElem * sizeof(*this->ReadbackUnits));
    if (unitsFound) {
      if (!(ru = (char **)SDDS_GetColumn(&SDDS_input, (char *)"ReadbackUnits"))) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->ReadbackUnits[pvListNElem - rows + j] = ru[j];
      }
      free(ru);
    } else {
      for (j = 0; j < rows; j++) {
        this->ReadbackUnits[pvListNElem - rows + j] = (char *)malloc(2 * sizeof(char));
        sprintf(this->ReadbackUnits[pvListNElem - rows + j], " ");
      }
    }

    this->elementCount = (uint32_t *)trealloc(this->elementCount, pvListNElem * sizeof(*this->elementCount));
    if (elementCountFound) {
      if ((ec = (uint32_t *)SDDS_GetColumnInLong(&SDDS_input, (char *)"ElementCount")) == NULL) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->elementCount[pvListNElem - rows + j] = ec[j];
      }
      free(ec);
    } else {
      for (j = 0; j < rows; j++) {
        this->elementCount[pvListNElem - rows + j] = 1;
      }
    }

    this->Types = (char **)trealloc(this->Types, pvListNElem * sizeof(*this->Types));
    if (typeFound) {
      if (!(ty = (char **)SDDS_GetColumn(&SDDS_input, (char *)"Type"))) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->Types[pvListNElem - rows + j] = ty[j];
      }
      free(ty);
    } else {
      for (j = 0; j < rows; j++) {
        this->Types[pvListNElem - rows + j] = (char *)malloc(7 * sizeof(char));
        sprintf(this->Types[pvListNElem - rows + j], "double");
      }
    }
    SDDS_Terminate(&SDDS_input);
  }
}

void exServer::ReadMasterPVFile(const char *const pvPrefix) {
  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL;
  char pvAlias[256];
  const char *const pNameFmtStr = "%.100s%.20s";
  //Read master PV file
  if (!SDDS_InitializeInput(&SDDS_masterlist, this->masterPVFile)) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", this->masterPVFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", this->masterPVFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if ((rows = SDDS_RowCount(&SDDS_masterlist)) < 0) {
    fprintf(stderr, "error: No rows found in SDDS file %s\n", this->masterPVFile);
    exit(1);
  }
  if (rows == 0) {
    return;
  }
  if (!(rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", this->masterPVFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  SDDS_Terminate(&SDDS_masterlist);

  //Check for conflicts
  for (unsigned int n = 0; n < pvListNElem; n++) {
    sprintf(pvAlias, pNameFmtStr, pvPrefix, this->ControlName[n]);
    if (strchr(pvAlias, '.') != NULL) {
      fprintf(stderr, "error: PV extensions are not allowed (%s)\n", pvAlias);
      exit(1);
    }
    for (int m = 0; m < rows; m++) {
      if (strcmp(pvAlias, rec_name[m]) == 0) {
        fprintf(stderr, "error: %s already exists in %s\n", pvAlias, this->masterPVFile);
        exit(1);
      }
    }
  }
  for (int m = 0; m < rows; m++) {
    free(rec_name[m]);
  }
  free(rec_name);
}

void exServer::ReadMasterSDDSpcasPVFile(const char *const pvPrefix) {
  SDDS_DATASET SDDS_masterlist;
  long rows;
  char **rec_name = NULL, **host_name = NULL;
  char hostname[255];
  char pvAlias[256];
  const char *const pNameFmtStr = "%.100s%.20s";
  //Read master sddspcas PV file
  if (!SDDS_InitializeInput(&SDDS_masterlist, this->pcasPVFile)) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", this->pcasPVFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  if (SDDS_ReadPage(&SDDS_masterlist) != 1) {
    fprintf(stderr, "error: Unable to read SDDS file %s\n", this->pcasPVFile);
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }
  rows = SDDS_RowCount(&SDDS_masterlist);
  if (rows > 0) {
    if (!(rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", this->pcasPVFile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    if (!(host_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"host_name"))) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", this->pcasPVFile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
  }
  SDDS_Terminate(&SDDS_masterlist);
  //Check for conflicts
  if (rows > 0) {
    gethostname(hostname, 255);
    for (int m = 0; m < rows; m++) {
      if (strcmp(host_name[m], hostname) == 0) {
        fprintf(stderr, "error: %s listed as a hostname already running sddspcas in %s\n", hostname, this->pcasPVFile);
        exit(1);
      }
    }
    for (unsigned int n = 0; n < pvListNElem; n++) {
      sprintf(pvAlias, pNameFmtStr, pvPrefix, this->ControlName[n]);
      if (strchr(pvAlias, '.') != NULL) {
        fprintf(stderr, "error: PV extensions are not allowed (%s)\n", pvAlias);
        exit(1);
      }
      for (int m = 0; m < rows; m++) {
        if (strcmp(pvAlias, rec_name[m]) == 0) {
          fprintf(stderr, "error: %s already exists in %s\n", pvAlias, this->pcasPVFile);
          exit(1);
        }
      }
    }
    for (int m = 0; m < rows; m++) {
      free(rec_name[m]);
    }
    free(rec_name);
    for (int m = 0; m < rows; m++) {
      free(host_name[m]);
    }
    free(host_name);
  }
}

void exServer::AppendMasterSDDSpcasPVFile(const char *const pvPrefix) {
  SDDS_DATASET SDDS_masterlist;
  char hostname[255];
  int64_t rowsPresent;
  char pvAlias[256];
  const char *const pNameFmtStr = "%.100s%.20s";
  //Append master sddspcas PV file

  gethostname(hostname, 255);

  if (!SDDS_InitializeAppendToPage(&SDDS_masterlist, this->pcasPVFile, 100, &rowsPresent)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  if (!SDDS_LengthenTable(&SDDS_masterlist, pvListNElem)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  for (unsigned int n = 0; n < pvListNElem; n++) {
    sprintf(pvAlias, pNameFmtStr, pvPrefix, this->ControlName[n]);
    if (!SDDS_SetRowValues(&SDDS_masterlist, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rowsPresent, "rec_name", pvAlias, "host_name", hostname, NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      exit(1);
    }
    rowsPresent++;
  }

  if (!SDDS_UpdatePage(&SDDS_masterlist, FLUSH_TABLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    exit(1);
  }

  SDDS_Terminate(&SDDS_masterlist);
}
