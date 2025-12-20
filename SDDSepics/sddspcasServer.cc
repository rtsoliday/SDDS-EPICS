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
#include <string>
#include <vector>
#include <complex>
#include <errno.h>
#include <float.h>
#include "sddspcasServer.h"
#include "mdb.h"
#include "SDDS.h"

static std::string trimWhitespace(const std::string &in) {
  size_t first = 0;
  while (first < in.size() && (in[first] == ' ' || in[first] == '\t' || in[first] == '\r' || in[first] == '\n')) {
    first++;
  }
  size_t last = in.size();
  while (last > first && (in[last - 1] == ' ' || in[last - 1] == '\t' || in[last - 1] == '\r' || in[last - 1] == '\n')) {
    last--;
  }
  return in.substr(first, last - first);
}

static void parseEnumStatesCsv(const char *pvName, const char *csv, std::vector<std::string> &outStates) {
  outStates.clear();
  if (!csv) {
    fprintf(stderr, "error: EnumStrings for %s is NULL\n", pvName ? pvName : "(unknown)");
    exit(1);
  }

  const std::string csvStr(csv);

  /* Do not allow quoting/escaping. */
  if (csvStr.find('"') != std::string::npos || csvStr.find('\'') != std::string::npos) {
    fprintf(stderr, "error: EnumStrings for %s must not contain quotes\n", pvName ? pvName : "(unknown)");
    exit(1);
  }

  size_t start = 0;
  while (start <= csvStr.size()) {
    size_t comma = csvStr.find(',', start);
    std::string token;
    if (comma == std::string::npos) {
      token = csvStr.substr(start);
      start = csvStr.size() + 1;
    } else {
      token = csvStr.substr(start, comma - start);
      start = comma + 1;
    }

    token = trimWhitespace(token);
    if (token.empty()) {
      fprintf(stderr, "error: EnumStrings for %s contains an empty state (consecutive commas or leading/trailing comma)\n", pvName ? pvName : "(unknown)");
      exit(1);
    }
    outStates.push_back(token);
  }

  if (outStates.size() < 1) {
    fprintf(stderr, "error: EnumStrings for %s must specify at least one state\n", pvName ? pvName : "(unknown)");
    exit(1);
  }
  if (outStates.size() > 16) {
    fprintf(stderr, "error: EnumStrings for %s specifies %lu states; CA enums support up to 16\n", pvName ? pvName : "(unknown)", (unsigned long)outStates.size());
    exit(1);
  }
}

static aitEnum typeStringToAitEnum(const std::string &typeStr) {
  if (typeStr == "char")
    return aitEnumInt8;
  if (typeStr == "uchar")
    return aitEnumUint8;
  if (typeStr == "short")
    return aitEnumInt16;
  if (typeStr == "ushort")
    return aitEnumUint16;
  if (typeStr == "int")
    return aitEnumInt32;
  if (typeStr == "uint")
    return aitEnumUint32;
  if (typeStr == "float")
    return aitEnumFloat32;
  if (typeStr == "double" || typeStr.empty())
    return aitEnumFloat64;
  if (typeStr == "string")
    return aitEnumString;
  if (typeStr == "enum")
    return aitEnumEnum16;
  return aitEnumFloat64;
}

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
                                                 scanOn(scanOnIn),
                                                 pvPrefix(pvPrefix ? pvPrefix : ""),
                                                 scanRate(rate),
                                                 dynamicPvInfos() {
  uint32_t i;
  exPV *pPV;
  pvInfo *pPVI;
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
      } else if (strcmp("enum", this->Types[n]) == 0) {
        if ((this->elementCount) && (this->elementCount[n] > 1)) {
          fprintf(stderr, "error: Type=enum requires ElementCount=1 for %s\n", this->ControlName[n]);
          exit(1);
        }
        if (!this->EnumStrings || !this->EnumStrings[n] || this->EnumStrings[n][0] == '\0') {
          fprintf(stderr, "error: PV %s has Type=enum but EnumStrings is missing or empty\n", this->ControlName[n]);
          exit(1);
        }

        std::vector<std::string> states;
        parseEnumStatesCsv(this->ControlName[n], this->EnumStrings[n], states);
        pvList[n].setEnumStateStrings(states);
        pvList[n].setType(aitEnumEnum16);
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
  pvInfo *pPVAfter;
  if (exServer::pvList) {
    pPVAfter = &exServer::pvList[exServer::pvListNElem];
  } else {
    pPVAfter = NULL;
  }

  //
  // delete all pre-created PVs (eliminate bounds-checker warnings)
  //
  if (exServer::pvList) {
    for (pPVI = exServer::pvList; pPVI < pPVAfter; pPVI++) {
      pPVI->deletePV();
    }
    delete[] exServer::pvList;
    exServer::pvList = NULL;
    exServer::pvListNElem = 0;
  }

  for (size_t i = 0; i < this->dynamicPvInfos.size(); i++) {
    if (this->dynamicPvInfos[i]) {
      this->dynamicPvInfos[i]->deletePV();
      delete this->dynamicPvInfos[i];
    }
  }
  this->dynamicPvInfos.clear();

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
  int hoprFound = 0, loprFound = 0, unitsFound = 0, elementCountFound = 0, typeFound = 0, enumStringsFound = 0;
  int i;
  uint32_t j;
  uint32_t rows;
  this->ControlName = NULL;
  this->ReadbackUnits = NULL;
  this->Types = NULL;
  this->EnumStrings = NULL;
  this->hopr = NULL;
  this->lopr = NULL;
  this->elementCount = NULL;
  char **cn = NULL;
  char **ru = NULL;
  char **ty = NULL;
  char **es = NULL;
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
    enumStringsFound = 0;
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
    if (SDDS_VerifyColumnExists(&SDDS_input, FIND_SPECIFIED_TYPE,
                                SDDS_STRING, "EnumStrings") >= 0) {
      enumStringsFound = 1;
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

    this->EnumStrings = (char **)trealloc(this->EnumStrings, pvListNElem * sizeof(*this->EnumStrings));
    if (enumStringsFound) {
      if (!(es = (char **)SDDS_GetColumn(&SDDS_input, (char *)"EnumStrings"))) {
        fprintf(stderr, "error: Unable to read SDDS input file\n");
        SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
        exit(1);
      }
      for (j = 0; j < rows; j++) {
        this->EnumStrings[pvListNElem - rows + j] = es[j];
      }
      free(es);
    } else {
      for (j = 0; j < rows; j++) {
        this->EnumStrings[pvListNElem - rows + j] = NULL;
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

bool exServer::hasAlias(const char *pAliasName) const {
  if (!pAliasName)
    return false;
  stringId id(pAliasName, stringId::refString);
  pvEntry *pPVE = this->stringResTbl.lookup(id);
  return pPVE != NULL;
}

bool exServer::pvConflictsMasterFiles(const char *pvAliasMaster20) const {
  if (!pvAliasMaster20 || pvAliasMaster20[0] == '\0')
    return false;

  if (this->masterPVFile) {
    SDDS_DATASET SDDS_masterlist;
    long rows;
    char **rec_name = NULL;
    if (!SDDS_InitializeInput(&SDDS_masterlist, this->masterPVFile)) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", this->masterPVFile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return true;
    }
    if (SDDS_ReadPage(&SDDS_masterlist) == 1) {
      rows = SDDS_RowCount(&SDDS_masterlist);
      if (rows > 0 && (rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
        for (int m = 0; m < rows; m++) {
          if (rec_name[m] && strcmp(pvAliasMaster20, rec_name[m]) == 0) {
            for (int k = 0; k < rows; k++) {
              if (rec_name[k])
                free(rec_name[k]);
            }
            free(rec_name);
            SDDS_Terminate(&SDDS_masterlist);
            return true;
          }
        }
        for (int k = 0; k < rows; k++) {
          if (rec_name[k])
            free(rec_name[k]);
        }
        free(rec_name);
      }
    }
    SDDS_Terminate(&SDDS_masterlist);
  }

  if (this->pcasPVFile) {
    SDDS_DATASET SDDS_masterlist;
    long rows;
    char **rec_name = NULL;
    if (!SDDS_InitializeInput(&SDDS_masterlist, this->pcasPVFile)) {
      fprintf(stderr, "error: Unable to read SDDS file %s\n", this->pcasPVFile);
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      return true;
    }
    if (SDDS_ReadPage(&SDDS_masterlist) == 1) {
      rows = SDDS_RowCount(&SDDS_masterlist);
      if (rows > 0 && (rec_name = (char **)SDDS_GetColumn(&SDDS_masterlist, (char *)"rec_name"))) {
        for (int m = 0; m < rows; m++) {
          if (rec_name[m] && strcmp(pvAliasMaster20, rec_name[m]) == 0) {
            for (int k = 0; k < rows; k++) {
              if (rec_name[k])
                free(rec_name[k]);
            }
            free(rec_name);
            SDDS_Terminate(&SDDS_masterlist);
            return true;
          }
        }
        for (int k = 0; k < rows; k++) {
          if (rec_name[k])
            free(rec_name[k]);
        }
        free(rec_name);
      }
    }
    SDDS_Terminate(&SDDS_masterlist);
  }

  return false;
}

void exServer::appendMasterSddspcasPvFileForAliases(const std::vector<std::string> &pvAliasesMaster20) {
  if (!this->pcasPVFile)
    return;
  if (pvAliasesMaster20.empty())
    return;

  SDDS_DATASET SDDS_masterlist;
  char hostname[255];
  int64_t rowsPresent;
  gethostname(hostname, 255);

  if (!SDDS_InitializeAppendToPage(&SDDS_masterlist, this->pcasPVFile, 100, &rowsPresent)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    return;
  }
  if (!SDDS_LengthenTable(&SDDS_masterlist, (long)pvAliasesMaster20.size())) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
    SDDS_Terminate(&SDDS_masterlist);
    return;
  }

  for (size_t i = 0; i < pvAliasesMaster20.size(); i++) {
    if (!SDDS_SetRowValues(&SDDS_masterlist, SDDS_SET_BY_NAME | SDDS_PASS_BY_VALUE, rowsPresent,
                           "rec_name", (char *)pvAliasesMaster20[i].c_str(),
                           "host_name", hostname,
                           NULL)) {
      SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
      SDDS_Terminate(&SDDS_masterlist);
      return;
    }
    rowsPresent++;
  }

  if (!SDDS_UpdatePage(&SDDS_masterlist, FLUSH_TABLE)) {
    SDDS_PrintErrors(stderr, SDDS_VERBOSE_PrintErrors);
  }
  SDDS_Terminate(&SDDS_masterlist);
}

unsigned exServer::addPVs(const std::vector<SddspcasPvDef> &defs) {
  if (defs.empty())
    return 0u;

  const char *const pNameFmtStr40 = "%.100s%.40s";
  const char *const pNameFmtStr20 = "%.100s%.20s";
  char pvAlias40[256];
  char pvAlias20[256];

  unsigned added = 0u;
  std::vector<std::string> aliasesToAppend;
  aliasesToAppend.reserve(defs.size());

  for (size_t i = 0; i < defs.size(); i++) {
    const SddspcasPvDef &d = defs[i];
    if (d.controlName.empty())
      continue;

    sprintf(pvAlias40, pNameFmtStr40, this->pvPrefix.c_str(), d.controlName.c_str());
    sprintf(pvAlias20, pNameFmtStr20, this->pvPrefix.c_str(), d.controlName.c_str());

    if (strchr(pvAlias20, '.') != NULL) {
      fprintf(stderr, "warning: PV extensions are not allowed (%s); skipping\n", pvAlias20);
      continue;
    }

    if (this->hasAlias(pvAlias40)) {
      continue;
    }
    if (this->pvConflictsMasterFiles(pvAlias20)) {
      fprintf(stderr, "warning: PV %s conflicts with master PV lists; skipping\n", pvAlias20);
      continue;
    }

    pvInfo *pInfo = new pvInfo();
    if (!pInfo) {
      fprintf(stderr, "warning: out of memory adding PV %s\n", pvAlias40);
      continue;
    }

    char *nameDup = strdup(d.controlName.c_str());
    char *unitsDup = NULL;
    if (!d.units.empty())
      unitsDup = strdup(d.units.c_str());
    else
      unitsDup = strdup(" ");

    pInfo->setName(nameDup ? nameDup : (char *)"");
    pInfo->setUnits(unitsDup ? unitsDup : (char *)"");
    pInfo->setScanPeriod(this->scanRate);

    const aitEnum typeEnum = typeStringToAitEnum(d.type);
    if (typeEnum == aitEnumEnum16) {
      if (d.elementCount > 1) {
        fprintf(stderr, "warning: Type=enum requires ElementCount=1 for %s; skipping\n", d.controlName.c_str());
        delete pInfo;
        continue;
      }
      if (d.enumStrings.empty()) {
        fprintf(stderr, "warning: PV %s has Type=enum but EnumStrings is empty; skipping\n", d.controlName.c_str());
        delete pInfo;
        continue;
      }
      std::vector<std::string> states;
      parseEnumStatesCsv(d.controlName.c_str(), d.enumStrings.c_str(), states);
      pInfo->setEnumStateStrings(states);
    }
    if (typeEnum == aitEnumString && d.elementCount > 1) {
      fprintf(stderr, "warning: multiple elements with String PVs are not supported in sddspcas yet; skipping %s\n", d.controlName.c_str());
      delete pInfo;
      continue;
    }
    pInfo->setType(typeEnum);

    if (d.hopr != DBL_MAX)
      pInfo->setHopr(d.hopr);
    if (d.lopr != -DBL_MAX)
      pInfo->setLopr(d.lopr);
    if (d.elementCount > 1)
      pInfo->setElementCount(d.elementCount);

    exPV *pPV = pInfo->createPV(*this, true, this->scanOn);
    if (!pPV) {
      fprintf(stderr, "warning: Unable to create new PV \"%s\"\n", pvAlias40);
      delete pInfo;
      continue;
    }

    this->installAliasName(*pInfo, pvAlias40);
    this->dynamicPvInfos.push_back(pInfo);
    aliasesToAppend.push_back(std::string(pvAlias20));
    added++;
  }

  if (added && this->pcasPVFile) {
    this->appendMasterSddspcasPvFileForAliases(aliasesToAppend);
  }
  return added;
}
