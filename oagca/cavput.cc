/**
 * @file cavput.cc
 * @brief Send values to EPICS process variables using Channel Access.
 *
 * cavput performs vector puts to one or more process variables.  It supports
 * ramping, delta mode, and other features useful for scripting accelerator
 * operations.
 *
 * @section Usage
 * ```
 * cavput [-list=<string>[=<value>][,<string>[=<value>]...]]
 *        [-delist=<pattern>[,<pattern]...]
 *        [-range=begin=<integer>,end=<integer>[,format=<string>][,interval=<integer>]]
 *        [-pendIoTime=<seconds>]
 *        [-dryRun]
 *        [-deltaMode[=factor=<value>]]
 *        [-ramp=step=<n>,pause=<sec>]
 *        [-numerical]
 *        [-charArray]
 *        [-blunderAhead[=silently]]
 *        [-provider={ca|pva}]
 * ```
 *
 * @section Options
 * | Option          | Description                                                                                   |
 * |-----------------|-----------------------------------------------------------------------------------------------|
 * | `-list`         | Specify PV name components with optional values.                                               |
 * | `-range`        | Generate PV names from a range.                                                              |
 * | `-pendIoTime`   | Maximum time to wait for connections and I/O. Default is 1.0s.                                |
 * | `-deltaMode`    | Send values as deltas from current PV values; optional factor (defaults to 1).                |
 * | `-ramp`         | Ramp to the target value in steps (default steps=1); optional pause between steps (default 0.1s). |
 * | `-numerical`    | Force conversion to numeric mode.                                                             |
 * | `-charArray`    | Force string mode for array values.                                                           |
 * | `-blunderAhead` | Continue puts on errors; `silently` suppresses error messages.                                |
 * | `-dryRun`       | Display operations without performing I/O.                                                    |
 * | `-ezcaTiming`   | Obsolete; use `-pendIoTime` instead.                                                         |
 * | `-noGroups`     | Obsolete; no effect.                                                                           |
 * | `-provider`     | Choose `ca` (Channel Access) or `pva` (PVAccess) provider.                                     |
 *
 * @subsection Incompatibilities
 * - `-ezcaTiming` cannot be used with `-pendIoTime`; use `-pendIoTime` exclusively.  
 * - `-deltaMode` and `-ramp` require numeric, scalar PVs; incompatible with `-charArray`.  
 * - `-numerical` and `-charArray` are mutually exclusive.                                 
 *
 * @copyright
 *   - (c) 2002 The University of Chicago, as Operator of Argonne National Laboratory.
 *   - (c) 2002 The Regents of the University of California, as Operator of Los Alamos National Laboratory.
 *
 * @license
 * This file is distributed under the terms of the Software License Agreement
 * found in the file LICENSE included with this distribution.
 *
 * @authors
 * M. Borland, R. Soliday
 */

#include <complex>
#include <iostream>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#  include <winsock.h>
#else
#  include <unistd.h>
#endif

#include <cadef.h>
#include <epicsVersion.h>
#if (EPICS_VERSION > 3)
#  include "pvaSDDS.h"
#  include <boost/algorithm/string.hpp>
#endif

#include "mdb.h"
#include "scan.h"
#include "match_string.h"
#include "SDDS.h"
#include "pvMultList.h"

#define CLO_LIST 0
#define CLO_RANGE 1
#define CLO_PENDIOTIME 2
#define CLO_DRYRUN 3
/* obsolete options for commandline compatibility */
#define CLO_EZCATIMING 4
#define CLO_NOGROUPS 5
#define CLO_DELTAMODE 6
#define CLO_NUMERICAL 7
#define CLO_BLUNDERAHEAD 8
#define CLO_RAMP 9
#define CLO_PROVIDER 10
#define CLO_CHARARRAY 11
#define CLO_DELIST 12
#define COMMANDLINE_OPTIONS 13
char *commandline_option[COMMANDLINE_OPTIONS] = {
  (char *)"list",
  (char *)"range",
  (char *)"pendiotime",
  (char *)"dryrun",
  (char *)"ezcatiming",
  (char *)"nogroups",
  (char *)"deltamode",
  (char *)"numerical",
  (char *)"blunderahead",
  (char *)"ramp",
  (char *)"provider",
  (char *)"chararray",
  (char *)"delist",
};

#define PROVIDER_CA 0
#define PROVIDER_PVA 1
#define PROVIDER_COUNT 2
char *providerOption[PROVIDER_COUNT] = {
  (char *)"ca",
  (char *)"pva",
};

static char *USAGE = (char *)"cavput [-list=<string>[=<value>][,<string>[=<value>]...]]\n\
[-delist=<pattern>[,<pattern>...]]\n\
[-range=begin=<integer>,end=<integer>[,format=<string>][,interval=<integer>]]\n\
[-pendIoTime=<seconds>] [-dryRun] [-deltaMode[=factor=<value>]] [-ramp=step=<n>,pause=<sec>] \n\
[-numerical] [-charArray] [-blunderAhead[=silently]]\n\
[-provider={ca|pva}]\n\n\
-list           specifies PV name string components\n\
-delist        Exclude PVs that match any of the comma-separated glob patterns (supports * and ?).\n\
-range          specifies range of integers and format string\n\
-pendIoTime     specifies maximum time to wait for connections and\n\
                return values.  Default is 1.0s \n\
-dryRun         specifies showing PV names and values but\n\
                not sending to IOCs.\n\
-deltaMode      specifies that values are deltas from present values in\n\
                PVs.\n\
-numerical      forces conversion of the value to a number and sending as\n\
                a number.  Default for -deltaMode.\n\
-charArray      Use when passing a string to a char array PV.\n\
-ramp           ramp to the value by given steps (default is 1) with given pause \n\
		(default is 0.1 seconds) between steps. \n\
-blunderAhead   specifies that the program should blunder ahead with all the\n\
                puts it can do, even if some PVs don't connect.\n\
-provider       defaults to CA (channel access) calls.\n\n\
Program by Michael Borland and Robert Soliday, ANL/APS (" EPICS_VERSION_STRING ", " __DATE__ ")\n";

/* example of usage:
   # set all SR quads to zero
   cavput -list=S -range=begin=1,end=40,format=%ld 
   -list=AQ,BQ -range=begin=1,end=5 -list=:CurrentAI=0
   # set slow beam history for S1A bpms and 4080 samples
   cavput -list=S:bpm -range=begin=1,end=40,format=%ld -list=:SlowBh:
   -list=MI=0,sizeAO=4080,modeBO=1,enableBO=1
*/

#define DO_BLUNDERAHEAD 0x0001U
#define BLUNDER_SILENTLY 0x0002U
void oag_ca_exception_handler(struct exception_handler_args args);

#if (EPICS_VERSION > 3)
long PrepPVAPutValues(PVA_OVERALL *pva, PV_VALUE *PVvalue, short charArray);
#endif

int main(int argc, char **argv) {
  int32_t beginRange, endRange, rangeInterval, rampSteps;
  long dryRun;
  long PVs, j, i_arg, numerical, iramp;
  unsigned long blunderAhead;
  PV_VALUE *PVvalue;
  TERM_LIST *List;
  unsigned long flags, dummyFlags;
  SCANNED_ARG *s_arg;
  char *ptr, *rangeFormat;
  double pendIOTime;
  chid *channelID = NULL;
  short deltaMode, doRamp, charArray=0;
  double *presentValue, deltaFactor, rampPause;
  long providerMode = 0;
  /* Exclusion filters */
  char **excludePatterns = NULL;
  long excludeCount = 0;
#if (EPICS_VERSION > 3)
  PVA_OVERALL pva;
  double *rampDeltas = NULL;
#endif

  argc = scanargs(&s_arg, argc, argv);
  if (argc < 2)
    bomb(NULL, USAGE);

  PVvalue = NULL;
  presentValue = NULL;
  List = NULL;
  PVs = dryRun = 0;
  pendIOTime = 1.0;
  deltaMode = numerical = 0;
  deltaFactor = 1;
  blunderAhead = 0;
  rampSteps = 1;
  rampPause = 0.1;
  doRamp = 0;

  for (i_arg = 1; i_arg < argc; i_arg++) {
    if (s_arg[i_arg].arg_type == OPTION) {
      delete_chars(s_arg[i_arg].list[0], (char *)"_");
      switch (match_string(s_arg[i_arg].list[0], commandline_option, COMMANDLINE_OPTIONS, 0)) {
      case CLO_LIST:
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -list syntax (cavput)");
        List = (TERM_LIST *)trealloc(List, sizeof(*List) * (s_arg[i_arg].n_items - 1));
        for (j = 1; j < s_arg[i_arg].n_items; j++) {
          List[j - 1].flags = 0;
          if ((ptr = strchr(s_arg[i_arg].list[j], '=')))
            *ptr++ = 0;
          List[j - 1].string = s_arg[i_arg].list[j];
          if (ptr) {
            List[j - 1].flags |= VALUE_GIVEN;
            List[j - 1].value = ptr;
          }
        }
        multiplyWithList(&PVvalue, &PVs, List, s_arg[i_arg].n_items - 1);
        break;
      case CLO_RANGE:
        s_arg[i_arg].n_items--;
        rangeFormat = NULL;
        rangeInterval = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "begin", SDDS_LONG, &beginRange, 1, BEGIN_GIVEN,
                          "end", SDDS_LONG, &endRange, 1, END_GIVEN,
                          "interval", SDDS_LONG, &rangeInterval, 1, INTERVAL_GIVEN,
                          "format", SDDS_STRING, &rangeFormat, 1, FORMAT_GIVEN,
                          NULL) ||
            !(flags & BEGIN_GIVEN) || !(flags & END_GIVEN) || beginRange > endRange ||
            (flags & FORMAT_GIVEN && SDDS_StringIsBlank(rangeFormat)))
          SDDS_Bomb((char *)"invalid -range syntax/values");
        if (!rangeFormat)
          rangeFormat = (char *)"%ld";
        multiplyWithRange(&PVvalue, &PVs, beginRange, endRange, rangeInterval, rangeFormat);
        s_arg[i_arg].n_items++;
        break;
      case CLO_PENDIOTIME:
        if (s_arg[i_arg].n_items != 2)
          bomb((char *)"wrong number of items for -pendIoTime", NULL);
        if (sscanf(s_arg[i_arg].list[1], "%lf", &pendIOTime) != 1 ||
            pendIOTime <= 0)
          SDDS_Bomb((char *)"invalid -pendIoTime value (cavget)");
        break;
      case CLO_DRYRUN:
        dryRun = 1;
        break;
      case CLO_EZCATIMING:
        fprintf(stderr, "warning (cavput): -ezcaTiming option is obsolete. Use -pendIoTime\n");
        break;
      case CLO_NOGROUPS:
        fprintf(stderr, "warning (cavput): -noGroups option is obsolete.\n");
        break;
      case CLO_DELTAMODE:
        deltaMode = 1;
        s_arg[i_arg].n_items--;
        if (!scanItemList(&dummyFlags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "factor", SDDS_DOUBLE, &deltaFactor, 1, 0,
                          NULL))
          SDDS_Bomb((char *)"invalid -deltaMode syntax/values");
        s_arg[i_arg].n_items++;
        break;
      case CLO_NUMERICAL:
        numerical = 1;
        break;
      case CLO_CHARARRAY:
        charArray = 1;
        break;
      case CLO_DELIST: {
        if (s_arg[i_arg].n_items < 2)
          SDDS_Bomb((char *)"invalid -exclude syntax (cavput)");
        long add = s_arg[i_arg].n_items - 1;
        excludePatterns = (char **)trealloc(excludePatterns, sizeof(*excludePatterns) * (excludeCount + add));
        for (j = 1; j < s_arg[i_arg].n_items; j++) {
          excludePatterns[excludeCount++] = s_arg[i_arg].list[j];
        }
        break;
      }
      case CLO_BLUNDERAHEAD:
        s_arg[i_arg].n_items--;
        if (!scanItemList(&blunderAhead, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "silently", -1, NULL, 0, BLUNDER_SILENTLY,
                          NULL))
          SDDS_Bomb((char *)"invalid -blunderAhead syntax/values");
        blunderAhead |= DO_BLUNDERAHEAD;
        s_arg[i_arg].n_items++;
        break;
      case CLO_RAMP:
        s_arg[i_arg].n_items--;
        rangeFormat = NULL;
        rangeInterval = 1;
        if (!scanItemList(&flags, s_arg[i_arg].list + 1, &s_arg[i_arg].n_items, 0,
                          "steps", SDDS_LONG, &rampSteps, 1, 0,
                          "pause", SDDS_DOUBLE, &rampPause, 1, 0, NULL) ||
            (rampSteps <= 1) || (rampPause < 0))
          SDDS_Bomb((char *)"invalid -ramp syntax/values");
        doRamp = 1;
        s_arg[i_arg].n_items++;
        break;
      case CLO_PROVIDER:
        if (s_arg[i_arg].n_items != 2)
          SDDS_Bomb((char *)"no value given for option -provider");
        if ((providerMode = match_string(s_arg[i_arg].list[1], providerOption,
                                         PROVIDER_COUNT, 0)) < 0)
          SDDS_Bomb((char *)"invalid -provider");
        break;
      default:
        bomb((char *)"unknown option (cavput)", NULL);
        break;
      }
    } else
      bomb((char *)"unknown option", NULL);
  }

  /* Apply exclusion filters before validating values or performing operations */
  if (excludeCount > 0 && PVs > 0) {
    long kept = 0;
    PVvalue = excludePVsFromLists(PVvalue, PVs, &kept, excludePatterns, excludeCount);
    PVs = kept;
  }

  for (j = 0; j < PVs; j++)
    if (!PVvalue[j].value) {
      fprintf(stderr, "Error: no value given for %s\n", PVvalue[j].name);
      exit(EXIT_FAILURE);
    }

  if (dryRun) {
    for (j = 0; j < PVs; j++) {
      if (strncmp(PVvalue[j].name, "pva://", 6) == 0) {
        PVvalue[j].name += 6;
        printf("%32s %s\n", PVvalue[j].name, PVvalue[j].value);
        PVvalue[j].name -= 6;
      } else if (strncmp(PVvalue[j].name, "ca://", 5) == 0) {
        PVvalue[j].name += 5;
        printf("%32s %s\n", PVvalue[j].name, PVvalue[j].value);
        PVvalue[j].name -= 5;
      } else {
        printf("%32s %s\n", PVvalue[j].name, PVvalue[j].value);
      }
    }
  } else {
#if (EPICS_VERSION > 3)
      //Allocate memory for pva structure
      allocPVA(&pva, PVs, 0);
      //List PV names
      epics::pvData::shared_vector<std::string> names(pva.numPVs);
      epics::pvData::shared_vector<std::string> provider(pva.numPVs);
      for (j = 0; j < pva.numPVs; j++) {
        if (strncmp(PVvalue[j].name, "pva://", 6) == 0) {
          PVvalue[j].name += 6;
          names[j] = PVvalue[j].name;
          PVvalue[j].name -= 6;
          provider[j] = "pva";
        } else if (strncmp(PVvalue[j].name, "ca://", 5) == 0) {
          PVvalue[j].name += 5;
          names[j] = PVvalue[j].name;
          PVvalue[j].name -= 5;
          provider[j] = "ca";
        } else {
          names[j] = PVvalue[j].name;
          if (providerMode == PROVIDER_PVA) {
            provider[j] = "pva";
          } else {
            provider[j] = "ca";
          }
        }
      }
      pva.pvaChannelNames = freeze(names);
      pva.pvaProvider = freeze(provider);
      //Connect to PVs
      ConnectPVA(&pva, pendIOTime);

      for (j = 0; j < PVs; j++) {
        if (!pva.isConnected[j]) {
          if (!(blunderAhead & BLUNDER_SILENTLY))
            fprintf(stderr, "Channel not connected: %s\n", PVvalue[j].name);
          if (!blunderAhead && (doRamp || deltaMode))
            exit(EXIT_FAILURE);
          continue;
        }
      }
      //Is there a way to check write access rights? If so, add it here.

      //Get the current values. This is mostly done to prepopulate the PVstructure which makes it easier to change the values later.
      if (GetPVAValues(&pva) == 1) {
        return (EXIT_FAILURE);
      }

      //Do not allow ramp or delta modes for non-scalar data
      if (doRamp || deltaMode) {
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j]) {
            if (pva.pvaData[j].fieldType != epics::pvData::scalar) {
              fprintf(stderr, "error (cavput): %s is not a scalar type PV, delta and ramp options are only available for scalar type PVs\n", PVvalue[j].name);
              return (EXIT_FAILURE);
            }
          }
        }
      }
      //Extract PV values given on the commandline and place them in the pva data structure
      if (PrepPVAPutValues(&pva, PVvalue, charArray)) {
        return (EXIT_FAILURE);
      }

      //Modify the put value because if it was a delta value
      if (deltaMode) {
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j] && (pva.pvaData[j].fieldType == epics::pvData::scalar) && pva.pvaData[j].numeric) {
            pva.pvaData[j].putData[0].values[0] = pva.pvaData[j].putData[0].values[0] * deltaFactor + pva.pvaData[j].getData[0].values[0];
          }
        }
      }

      if (doRamp) {
        double *fValues;
        //Calculate ramp step sizes
        rampDeltas = (double *)malloc(sizeof(double) * PVs);
        fValues = (double *)malloc(sizeof(double) * PVs);
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j] && (pva.pvaData[j].fieldType == epics::pvData::scalar) && pva.pvaData[j].numeric) {
            fValues[j] = pva.pvaData[j].putData[0].values[0];
            rampDeltas[j] = (pva.pvaData[j].putData[0].values[0] - pva.pvaData[j].getData[0].values[0]) / rampSteps;
            pva.pvaData[j].putData[0].values[0] = pva.pvaData[j].getData[0].values[0];
          }
        }
        //Put ramp values
        for (iramp = 1; iramp <= rampSteps; iramp++) {
          for (j = 0; j < PVs; j++) {
            if (pva.isConnected[j] && (pva.pvaData[j].fieldType == epics::pvData::scalar) && pva.pvaData[j].numeric) {
              if (iramp == rampSteps) {
                pva.pvaData[j].putData[0].values[0] = fValues[j];
              } else {
                pva.pvaData[j].putData[0].values[0] = pva.pvaData[j].putData[0].values[0] + rampDeltas[j];
              }
              pva.pvaData[j].numPutElements = 1;
            }
          }
          if (PutPVAValues(&pva) == 1) {
            return (EXIT_FAILURE);
          }
          if ((rampPause > 0) && (iramp < rampSteps)) {
            usleepSystemIndependent(rampPause * 1000000);
          }
        }
        free(rampDeltas);
      } else {
        //Put values
        if (PutPVAValues(&pva) == 1) {
          return (EXIT_FAILURE);
        }
      }
      //Free memory and exit
      if (PVvalue) {
        for (j = 0; j < PVs; j++) {
          if (PVvalue[j].name)
            free(PVvalue[j].name);
          if (PVvalue[j].value)
            free(PVvalue[j].value);
        }
        free(PVvalue);
      }
      freePVA(&pva);
      if (List)
        free(List);
      free_scanargs(&s_arg, argc);
      return (EXIT_SUCCESS);
#else
      ca_task_initialize();
      ca_add_exception_event(oag_ca_exception_handler, NULL);
      if (!(channelID = (chid *)malloc(sizeof(*channelID) * PVs)))
        bomb((char *)"memory allocation failure (cavput)", NULL);
      for (j = 0; j < PVs; j++) {
        if (ca_search(PVvalue[j].name, channelID + j) != ECA_NORMAL) {
          fprintf(stderr, "error (cavput): problem doing search for %s\n",
                  PVvalue[j].name);
          ca_task_exit();
          exit(EXIT_FAILURE);
        }
#ifdef DEBUG
        fprintf(stderr, "ca_search initiated for %s\n", PVvalue[j].name);
#endif
      }
      ca_pend_io(pendIOTime);
#ifdef DEBUG
      fprintf(stderr, "ca_pend_io returned\n");
#endif
      if (doRamp || deltaMode) {
        for (j = 0; j < PVs; j++) {
          if (ca_state(channelID[j]) != cs_conn) {
            if (!(blunderAhead & BLUNDER_SILENTLY))
              fprintf(stderr, "error (cavput): no connection for %s\n",
                      PVvalue[j].name);
            if (!blunderAhead) {
              ca_task_exit();
              exit(EXIT_FAILURE);
            }
          }
        }
        if (!(presentValue = (double *)malloc(sizeof(*presentValue) * PVs))) {
          fprintf(stderr, "error (cavput): memory allocation failure\n");
          ca_task_exit();
          exit(EXIT_FAILURE);
        }
        for (j = 0; j < PVs; j++) {
          presentValue[j] = DBL_MAX;
          if (ca_state(channelID[j]) != cs_conn)
            continue;
          if (ca_get(DBR_DOUBLE, channelID[j], presentValue + j) != ECA_NORMAL) {
            fprintf(stderr, "error (cavput): problem doing get for %s\n",
                    PVvalue[j].name);
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
        }
        ca_pend_io(pendIOTime);
        for (j = 0; j < PVs; j++) {
          if (ca_state(channelID[j]) != cs_conn)
            continue;
          if (presentValue[j] == DBL_MAX) {
            fprintf(stderr, "error (cavput): no value returned in time for %s\n",
                    PVvalue[j].name);
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
          if (sscanf(PVvalue[j].value, "%le", &PVvalue[j].numericalValue) != 1) {
            fprintf(stderr, "error (cavput): value (%s) for %s is not numerical--can't use -deltaMode or -ramp\n",
                    PVvalue[j].value, PVvalue[j].name);
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
        }
        numerical = 1;
      }
      if (deltaMode) {
        for (j = 0; j < PVs; j++) {
          PVvalue[j].numericalValue = PVvalue[j].numericalValue * deltaFactor + presentValue[j];
        }
      }
      if (!doRamp) {
#ifdef DEBUG
        fprintf(stderr, "doing non-ramped puts\n");
#endif
        for (j = 0; j < PVs; j++) {
          if (ca_state(channelID[j]) != cs_conn) {
            if (!(blunderAhead & BLUNDER_SILENTLY))
              fprintf(stderr, "Channel not connected: %s\n", PVvalue[j].name);
            continue;
          }
#ifdef DEBUG
          fprintf(stderr, "%s is connected\n", PVvalue[j].name);
#endif
          if (ca_write_access(channelID[j]) == 0) {
            fprintf(stderr, "Write access denied on : %s\n", PVvalue[j].name);
          }
          if (!numerical) {
            if (ca_put(DBR_STRING, channelID[j], PVvalue[j].value) != ECA_NORMAL) {
              if (!(blunderAhead & BLUNDER_SILENTLY))
                fprintf(stderr, "error (cavput): problem doing put for %s\n",
                        PVvalue[j].name);
              if (!blunderAhead) {
                ca_task_exit();
                exit(EXIT_FAILURE);
              }
            }
#ifdef DEBUG
            fprintf(stderr, "ca_put of %s for %s done\n", PVvalue[j].value, PVvalue[j].name);
#endif
          } else {
            if (!deltaMode) {
              if (sscanf(PVvalue[j].value, "%le", &PVvalue[j].numericalValue) != 1) {
                fprintf(stderr,
                        "error (cavput): value (%s) for %s is not scannable as a number---can't use -numerical.\n",
                        PVvalue[j].value, PVvalue[j].name);
                ca_task_exit();
                exit(EXIT_FAILURE);
              }
            }
            if (ca_put(DBR_DOUBLE, channelID[j], &PVvalue[j].numericalValue) != ECA_NORMAL) {
              if (!(blunderAhead & BLUNDER_SILENTLY))
                fprintf(stderr, "error (cavput): problem doing put for %s\n",
                        PVvalue[j].name);
              if (!blunderAhead) {
                ca_task_exit();
                exit(EXIT_FAILURE);
              }
            }
#ifdef DEBUG
            fprintf(stderr, "ca_put of %f for %s done\n", PVvalue[j].numericalValue, PVvalue[j].name);
#endif
          }
        }
      } else {
        /*do ramp */
        for (iramp = 0; iramp < rampSteps; iramp++) {
          for (j = 0; j < PVs; j++) {
            rampDelta = (PVvalue[j].numericalValue - presentValue[j]) / rampSteps;
            rampValue = presentValue[j] + (iramp + 1) * rampDelta;
            if (ca_put(DBR_DOUBLE, channelID[j], &rampValue) != ECA_NORMAL) {
              if (!(blunderAhead & BLUNDER_SILENTLY))
                fprintf(stderr, "error (cavput): problem doing put for %s\n",
                        PVvalue[j].name);
              if (!blunderAhead) {
                ca_task_exit();
                exit(EXIT_FAILURE);
              }
            }
          }
          if ((status = ca_pend_io(pendIOTime)) != ECA_NORMAL) {
            fprintf(stderr, "Problem processing one or more puts (cavput):\n%s\n",
                    ca_message(status));
            ca_task_exit();
            exit(EXIT_FAILURE);
          }
          if (rampPause > 0) {
            ca_pend_event(rampPause);
          } else {
            ca_poll();
          }
        }
      }

      if ((status = ca_pend_io(pendIOTime)) != ECA_NORMAL) {
        fprintf(stderr, "Problem processing one or more puts (cavput):\n%s\n",
                ca_message(status));
        ca_task_exit();
        exit(EXIT_FAILURE);
      }
      ca_pend_event(0.5);
#ifdef DEBUG
      fprintf(stderr, "ca_pend_io returned\n");
#endif

      ca_task_exit();
#ifdef DEBUG
      fprintf(stderr, "ca_task_exit returned\n");
#endif

#endif
  }

#ifdef DEBUG
  fprintf(stderr, "starting to free memory\n");
#endif

  if (channelID)
    free(channelID);
#ifdef DEBUG
  fprintf(stderr, "free'd channelID\n");
#endif

  for (j = 0; j < PVs; j++) {
    if (PVvalue[j].name)
      free(PVvalue[j].name);
    /* if (PVvalue[j].value) free(PVvalue[j].value); THIS CAUSES PROBLEMS*/
    PVvalue[j].name = PVvalue[j].value = NULL;
  }
  free(PVvalue);
#ifdef DEBUG
  fprintf(stderr, "free'd PVvalue lists\n");
#endif

  if (presentValue)
    free(presentValue);

  free_scanargs(&s_arg, argc);
#ifdef DEBUG
  fprintf(stderr, "free_scanargs returned\n");
#endif

  if (List)
    free(List);
#ifdef DEBUG
  fprintf(stderr, "free'd List\n");
#endif

#ifdef DEBUG
  fprintf(stderr, "exiting main routine\n");
#endif

  return (EXIT_SUCCESS);
}

void oag_ca_exception_handler(struct exception_handler_args args) {
  char *pName;
  int severityInt;
  static const char *severity[] =
    {
      "Warning",
      "Success",
      "Error",
      "Info",
      "Fatal",
      "Fatal",
      "Fatal",
      "Fatal"};

  severityInt = CA_EXTRACT_SEVERITY(args.stat);

  if ((severityInt != 1) && (severityInt != 3)) {
    fprintf(stderr, "CA.Client.Exception................\n");
    fprintf(stderr, "    %s: \"%s\"\n",
            severity[severityInt],
            ca_message(args.stat));

    if (args.ctx) {
      fprintf(stderr, "    Context: \"%s\"\n", args.ctx);
    }
    if (args.chid) {
      pName = (char *)ca_name(args.chid);
      fprintf(stderr, "    Channel: \"%s\"\n", pName);
      fprintf(stderr, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
    }
    fprintf(stderr, "This sometimes indicates an IOC that is hung up.\n");
    _exit(EXIT_FAILURE);
  } else {
    fprintf(stdout, "CA.Client.Exception................\n");
    fprintf(stdout, "    %s: \"%s\"\n",
            severity[severityInt],
            ca_message(args.stat));

    if (args.ctx) {
      fprintf(stdout, "    Context: \"%s\"\n", args.ctx);
    }
    if (args.chid) {
      pName = (char *)ca_name(args.chid);
      fprintf(stdout, "    Channel: \"%s\"\n", pName);
      fprintf(stdout, "    Type: \"%s\"\n", dbr_type_to_text(args.type));
    }
  }
}

#if (EPICS_VERSION > 3)
long PrepPVAPutValues(PVA_OVERALL *pva, PV_VALUE *PVvalue, short charArray) {
  long i, j;
  for (j = 0; j < pva->numPVs; j++) {
    if (pva->isConnected[j]) {
      switch (pva->pvaData[j].fieldType) {
      case epics::pvData::scalar: {
        pva->pvaData[j].numPutElements = 1;
        if (pva->pvaData[j].numeric) {
          pva->pvaData[j].putData[0].values = (double *)malloc(sizeof(double));
          if (sscanf(PVvalue[j].value, "%le", &(pva->pvaData[j].putData[0].values[0])) != 1) {
            fprintf(stderr, "error (cavput): value (%s) for %s is not numerical\n", PVvalue[j].value, PVvalue[j].name);
            return (1);
          }
        } else {
          pva->pvaData[j].putData[0].stringValues = (char **)malloc(sizeof(char *));
          pva->pvaData[j].putData[0].stringValues[0] = (char *)malloc(sizeof(char) * (strlen(PVvalue[j].value) + 1));
          strcpy(pva->pvaData[j].putData[0].stringValues[0], trim_spaces(PVvalue[j].value));
        }
        break;
      }
      case epics::pvData::scalarArray: {
        char *ptr, *ptr2;
        size_t length = strlen(PVvalue[j].value);
        pva->pvaData[j].numPutElements = count_chars(PVvalue[j].value, ',') + 1;
        if (pva->pvaData[j].numeric) {
          std::string ss;
          if (length == 0) {
            pva->pvaData[j].numPutElements = 0;
          }
          if (charArray) {
            pva->pvaData[j].numPutElements = strlen(PVvalue[j].value) + 1;
            pva->pvaData[j].putData[0].values = (double *)malloc(sizeof(double) * pva->pvaData[j].numPutElements);
            for (i = 0; i < pva->pvaData[j].numPutElements - 1; i++) {
              pva->pvaData[j].putData[0].values[i] = (int)(PVvalue[j].value[i]);
            }
            pva->pvaData[j].putData[0].values[pva->pvaData[j].numPutElements - 1] = 0;
          } else {
            if (pva->pvaData[j].numPutElements > 0) {
              ptr = PVvalue[j].value;
              pva->pvaData[j].putData[0].values = (double *)malloc(sizeof(double) * pva->pvaData[j].numPutElements);
              for (i = 0; i < pva->pvaData[j].numPutElements; i++) {
                if ((ptr2 = strchr(ptr, ',')) != NULL) {
                  *ptr2 = 0;
                  ss = ptr;
                  ptr = ptr2 + 1;
                } else {
                  ss = ptr;
                }
                if (sscanf(ss.c_str(), "%le", &(pva->pvaData[j].putData[0].values[i])) != 1) {
                  fprintf(stderr, "error (cavput): value (%s) for %s is not numerical\n", ss.c_str(), PVvalue[j].name);
                  return (1);
                }
              }
            }
          }
        } else {
          std::string ss;
          if ((length == 0) && (pva->pvaData[j].scalarType != epics::pvData::pvString)) {
            pva->pvaData[j].numPutElements = 0;
          }
          if (pva->pvaData[j].numPutElements > 0) {
            ptr = PVvalue[j].value;
            pva->pvaData[j].putData[0].stringValues = (char **)malloc(sizeof(char *) * pva->pvaData[j].numPutElements);
            for (i = 0; i < pva->pvaData[j].numPutElements; i++) {
              if ((ptr2 = strchr(ptr, ',')) != NULL) {
                *ptr2 = 0;
                ss = ptr;
                ptr = ptr2 + 1;
              } else {
                ss = ptr;
              }
              boost::trim(ss);
              pva->pvaData[j].putData[0].stringValues[i] = (char *)malloc(sizeof(char) * (ss.length() + 1));
              strcpy(pva->pvaData[j].putData[0].stringValues[i], ss.c_str());
            }
          }
        }
        break;
      }
      case epics::pvData::structure: {
        if (pva->pvaData[j].pvEnumeratedStructure) {
          pva->pvaData[j].numPutElements = 1;
          pva->pvaData[j].putData[0].stringValues = (char **)malloc(sizeof(char *));
          pva->pvaData[j].putData[0].stringValues[0] = (char *)malloc(sizeof(char) * (strlen(PVvalue[j].value) + 1));
          strcpy(pva->pvaData[j].putData[0].stringValues[0], trim_spaces(PVvalue[j].value));
          pva->pvaData[j].putData[0].values = (double *)malloc(sizeof(double));
          pva->pvaData[j].putData[0].values = 0; //Set this to 0 for now. It isn't used for putting values.
        } else {
          std::cerr << "error (cavput): unexpected structure type " << std::endl;
          return (1);
        }
        break;
      }
      default: {
        std::cerr << "error (cavput): unexpected field type " << pva->pvaData[j].fieldType << std::endl;
        return (1);
      }
      }
    }
  }
  return (0);
}
#endif
