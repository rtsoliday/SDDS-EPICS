/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 *     National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 *     Operator of Los Alamos National Laboratory.
 * EPICS BASE Versions 3.13.7
 * and higher are distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "sddspcasServer.h"
#include "gddApps.h"

#define myPI 3.14159265358979323846

//
// SUN C++ does not have RAND_MAX yet
//
#if !defined(RAND_MAX)
//
// Apparently SUN C++ is using the SYSV version of rand
//
#  if 0
#    define RAND_MAX INT_MAX
#  else
#    define RAND_MAX SHRT_MAX
#  endif
#endif

//
// special gddDestructor guarantees same form of new and delete
//
class exVecDestructor : public gddDestructor {
  virtual void run(void *);
};

//
// exVectorPV::maxDimension()
//
uint32_t exVectorPV::maxDimension() const {
  return 1u;
}

//
// exVectorPV::maxBound()
//
aitIndex exVectorPV::maxBound(uint32_t dimension) const // X aCC 361
{
  if (dimension == 0u) {
    return this->info.getElementCount();
  } else {
    return 0u;
  }
}

//
// exVectorPV::scan
//
void exVectorPV::scan() {
  caStatus status;
  double radians;
  smartGDDPointer pDD;
  double limit;
  exVecDestructor *pDest;
  int gddStatus;

  //
  // update current time (so we are not required to do
  // this every time that we write the PV which impacts
  // throughput under sunos4 because gettimeofday() is
  // slow)
  //
  this->currentTime = epicsTime::getCurrent();

  if (this->info.getType() == aitEnumString) {
    pDD = new gddAtomic(gddAppType_value, aitEnumString, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumUint8) {
    pDD = new gddAtomic(gddAppType_value, aitEnumUint8, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumInt8) {
    pDD = new gddAtomic(gddAppType_value, aitEnumInt8, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumUint16) {
    pDD = new gddAtomic(gddAppType_value, aitEnumUint16, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumInt16) {
    pDD = new gddAtomic(gddAppType_value, aitEnumInt16, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumUint32) {
    pDD = new gddAtomic(gddAppType_value, aitEnumUint32, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumInt32) {
    pDD = new gddAtomic(gddAppType_value, aitEnumInt32, 1u, this->info.getElementCount());
  } else if (this->info.getType() == aitEnumFloat32) {
    pDD = new gddAtomic(gddAppType_value, aitEnumFloat32, 1u, this->info.getElementCount());
  } else {
    pDD = new gddAtomic(gddAppType_value, aitEnumFloat64, 1u, this->info.getElementCount());
  }

  if (!pDD.valid()) {
    return;
  }

  //
  // smart pointer class manages reference count after this point
  //
  gddStatus = pDD->unreference();
  assert(!gddStatus);

  if (this->info.getType() == aitEnumString) {
    const aitString *pCF;
    char newValue[AIT_FIXED_STRING_SIZE];
    aitString *pF, *pFE;
    pF = new aitString[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        return;
      } else {
        sprintf(newValue, "");
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumUint8) {
    const aitUint8 *pCF;
    unsigned char newValue;
    aitUint8 *pF, *pFE;
    pF = new aitUint8[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumInt8) {
    const aitInt8 *pCF;
    char newValue;
    aitInt8 *pF, *pFE;
    pF = new aitInt8[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumUint16) {
    const aitUint16 *pCF;
    unsigned char newValue;
    aitUint16 *pF, *pFE;
    pF = new aitUint16[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumInt16) {
    const aitInt16 *pCF;
    char newValue;
    aitInt16 *pF, *pFE;
    pF = new aitInt16[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumUint32) {
    const aitUint32 *pCF;
    unsigned char newValue;
    aitUint32 *pF, *pFE;
    pF = new aitUint32[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumInt32) {
    const aitInt32 *pCF;
    char newValue;
    aitInt32 *pF, *pFE;
    pF = new aitInt32[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0;
      }
      *(pF++) = newValue;
    }
  } else if (this->info.getType() == aitEnumFloat32) {
    const aitFloat32 *pCF;
    float newValue;
    aitFloat32 *pF, *pFE;
    pF = new aitFloat32[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      radians = (rand() * 2.0 * myPI) / RAND_MAX;
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0.0f;
      }
      if ((double)this->info.getScanPeriod() > 0) {
        newValue += (float)(sin(radians) / 10.0);
      }
      limit = (double)this->info.getHopr();
      newValue = (float)tsMin((double)newValue, limit);
      limit = (double)this->info.getLopr();
      newValue = (float)tsMax((double)newValue, limit);
      *(pF++) = newValue;
    }
  } else {
    const aitFloat64 *pCF;
    double newValue;
    aitFloat64 *pF, *pFE;
    pF = new aitFloat64[this->info.getElementCount()];
    if (!pF) {
      return;
    }
    pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return;
    }
    pDD->putRef(pF, pDest);
    pCF = NULL;
    if (this->pValue.valid()) {
      if (this->pValue->dimension() == 1u) {
        const gddBounds *pB = this->pValue->getBounds();
        if (pB[0u].size() == this->info.getElementCount()) {
          pCF = *this->pValue;
        }
      }
    }
    pFE = &pF[this->info.getElementCount()];
    while (pF < pFE) {
      radians = (rand() * 2.0 * myPI) / RAND_MAX;
      if (pCF) {
        newValue = *pCF++;
      } else {
        newValue = 0.0f;
      }
      if ((double)this->info.getScanPeriod() > 0) {
        newValue += (double)(sin(radians) / 10.0);
      }
      limit = (double)this->info.getHopr();
      newValue = tsMin(newValue, limit);
      limit = (double)this->info.getLopr();
      newValue = tsMax(newValue, limit);
      *(pF++) = newValue;
    }
  }

  status = this->update(*pDD);
  if (status != S_casApp_success) {
    errMessage(status, "vector scan update failed\n");
  }
}

//
// exVectorPV::updateValue ()
//
// NOTES:
// 1) This should have a test which verifies that the
// incoming value in all of its various data types can
// be translated into a real number?
// 2) We prefer to unreference the old PV value here and
// reference the incomming value because this will
// result in value change events each retaining an
// independent value on the event queue. With large arrays
// this may result in too much memory consumtion on
// the event queue.
//
caStatus exVectorPV::updateValue(const gdd &value) {
  //
  // Check bounds of incoming request
  // (and see if we are replacing all elements -
  // replaceOk==true)
  //
  // Perhaps much of this is unnecessary since the
  // server lib checks the bounds of all requests
  //
  if (value.isAtomic()) {
    if (value.dimension() != 1u) {
      return S_casApp_badDimension;
    }
    const gddBounds *pb = value.getBounds();
    if (pb[0u].first() != 0u) {
      return S_casApp_outOfBounds;
    } else if (pb[0u].size() > this->info.getElementCount()) {
      return S_casApp_outOfBounds;
    }
  } else if (!value.isScalar()) {
    //
    // no containers
    //
    return S_casApp_outOfBounds;
  }

  //
  // Create a new array data descriptor
  // (so that old values that may be referenced on the
  // event queue are not replaced)
  //

  if (this->info.getType() == aitEnumString) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumString,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitString *pF = new aitString[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      //*pF[i].string = NULL;
      //*pF[i].len = 0;
      //pF[i] = (const aitString*)NULL;
      //sprintf(pF[i], "");
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumUint8) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumUint8,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitUint8 *pF = new aitUint8[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumInt8) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumInt8,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitInt8 *pF = new aitInt8[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumUint16) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumUint16,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitUint16 *pF = new aitUint16[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumInt16) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumInt16,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitInt16 *pF = new aitInt16[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumUint32) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumUint32,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitUint32 *pF = new aitUint32[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumInt32) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumInt32,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitInt32 *pF = new aitInt32[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else if (this->info.getType() == aitEnumFloat32) {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumFloat32,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitFloat32 *pF = new aitFloat32[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0.0f;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);

    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  } else {
    smartGDDPointer pNewValue(new gddAtomic(gddAppType_value, aitEnumFloat64,
                                            1u, this->info.getElementCount()));
    if (!pNewValue.valid()) {
      return S_casApp_noMemory;
    }
    gddStatus gdds = pNewValue->unreference();
    assert(!gdds);
    aitFloat64 *pF = new aitFloat64[this->info.getElementCount()];
    if (!pF) {
      return S_casApp_noMemory;
    }
    uint32_t count = this->info.getElementCount();
    for (uint32_t i = 0u; i < count; i++) {
      pF[i] = 0.0f;
    }
    exVecDestructor *pDest = new exVecDestructor;
    if (!pDest) {
      delete[] pF;
      return S_casApp_noMemory;
    }
    pNewValue->putRef(pF, pDest);
    gdds = pNewValue->put(&value);
    if (gdds) {
      return S_cas_noConvert;
    }
    this->pValue = pNewValue;
  }

  return S_casApp_success;
}

//
// exVecDestructor::run()
//
// special gddDestructor guarantees same form of new and delete
//
void exVecDestructor::run(void *pUntyped) {
  aitFloat32 *pf = reinterpret_cast<aitFloat32 *>(pUntyped);
  delete[] pf;
}
