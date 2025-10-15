/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* file: pvMultList.c
 * purpose: routines for multiplying lists/ranges to make PV names.
 * M. Borland, 1995.
 */
#include "mdb.h"
#include "SDDS.h"
#include "pvMultList.h"
#include <stdlib.h>

PV_VALUE *excludePVsFromLists(PV_VALUE *PVvalue, long PVs, long  *kept, char **excludePatterns, long excludeCount)
{
  long j;
  PV_VALUE *newPV = (PV_VALUE *)tmalloc(sizeof(*newPV) * PVs);
  *kept = 0;
  for (j = 0; j<excludeCount; j++) {
    if (has_wildcards(excludePatterns[j]) && strchr(excludePatterns[j], '-'))
        excludePatterns[j] = expand_ranges(excludePatterns[j]);
  }
  for (j = 0; j < PVs; j++) {
    int excluded = 0;
    for (long k = 0; k < excludeCount; k++) {
      if (wild_match(PVvalue[j].name, excludePatterns[k])) {
        excluded = 1;
        break;
      }
    }
    if (!excluded) {
      /* Move entry into new array */
      newPV[*kept] = PVvalue[j];
      /* Prevent double-free of moved pointers */
      PVvalue[j].name = NULL;
      PVvalue[j].value = NULL;
      *kept += 1;
    } else {
      /* Free excluded entry now since we'll discard original array */
      if (PVvalue[j].name)
        free(PVvalue[j].name);
      if (PVvalue[j].value)
        free(PVvalue[j].value);
      PVvalue[j].name = PVvalue[j].value = NULL;
    }
  }
  /* Free original container and update pointers/count */
  free(PVvalue);
  PVvalue = newPV;
  PVs = *kept;
  if (PVs == 0) {
    fprintf(stderr, "Error: all PVs were excluded by -exclude filters\n");
    exit(EXIT_FAILURE);
  }
  return newPV;
}

void multiplyWithList(PV_VALUE **PVvalue, long *PVvalues, TERM_LIST *List, long listEntries) {
  long newPVvalues, i, j, k;
  PV_VALUE *newPVvalue;

  if (!*PVvalues) {
    *PVvalues = listEntries;
    *PVvalue = tmalloc(sizeof(**PVvalue) * listEntries);
    for (i = 0; i < listEntries; i++) {
      SDDS_CopyString(&(*PVvalue)[i].name, List[i].string);
      if (List[i].flags & VALUE_GIVEN)
        SDDS_CopyString(&(*PVvalue)[i].value, List[i].value);
      else
        (*PVvalue)[i].value = 0;
    }
  } else {
    newPVvalues = *PVvalues * listEntries;
    newPVvalue = tmalloc(sizeof(*newPVvalue) * (newPVvalues));
    for (i = k = 0; i < *PVvalues; i++) {
      for (j = 0; j < listEntries; j++, k++) {
        newPVvalue[k].name = tmalloc(sizeof(*newPVvalue[k].name) *
                                     (strlen(List[j].string) + strlen((*PVvalue)[i].name) + 1));
        strcpy(newPVvalue[k].name, (*PVvalue)[i].name);
        strcat(newPVvalue[k].name, List[j].string);
        if (List[j].flags & VALUE_GIVEN)
          SDDS_CopyString(&newPVvalue[k].value, List[j].value);
        else
          SDDS_CopyString(&newPVvalue[k].value, (*PVvalue)[i].value);
      }
    }
    for (i = 0; i < *PVvalues; i++) {
      free((*PVvalue)[i].name);
      if ((*PVvalue)[i].value)
        free((*PVvalue)[i].value);
    }
    free(*PVvalue);
    *PVvalue = newPVvalue;
    *PVvalues = newPVvalues;
  }
}

void multiplyWithRange(PV_VALUE **PVvalue, long *PVvalues, long begin, long end, long interval, char *format) {
  long newPVvalues, value, i, j, k;
  PV_VALUE *newPVvalue;
  char buffer[256];

  if (!*PVvalues) {
    *PVvalue = tmalloc(sizeof(**PVvalue) * (end - begin + 1));
    for (value = begin, j = k = 0; value <= end; value++, j++) {
      if (j % interval)
        continue;
      sprintf(buffer, format, value);
      SDDS_CopyString(&(*PVvalue)[k].name, buffer);
      (*PVvalue)[k].value = 0;
      k++;
    }
    *PVvalues = k;
  } else {
    newPVvalue = tmalloc(sizeof(*newPVvalue) * (*PVvalues * (end - begin + 1)));
    newPVvalues = 0;
    for (i = k = 0; i < *PVvalues; i++) {
      for (value = begin, j = 0; value <= end; value++, j++) {
        if (j % interval)
          continue;
        sprintf(buffer, format, value);
        newPVvalue[k].name = tmalloc(sizeof(*newPVvalue[k].name) *
                                     (strlen(buffer) + strlen((*PVvalue)[i].name) + 1));
        strcpy(newPVvalue[k].name, (*PVvalue)[i].name);
        strcat(newPVvalue[k].name, buffer);
        newPVvalue[k].value = (*PVvalue)[i].value;
        k++;
      }
    }
    newPVvalues = k;
    for (i = 0; i < *PVvalues; i++) {
      free((*PVvalue)[i].name);
      /* if ((*PVvalue)[i].value) free((*PVvalue)[i].value); */
    }
    free(*PVvalue);
    *PVvalue = newPVvalue;
    *PVvalues = newPVvalues;
  }
}
