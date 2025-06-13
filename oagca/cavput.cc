/*************************************************************************\
 * Copyright (c) 2002 The University of Chicago, as Operator of Argonne
 * National Laboratory.
 * Copyright (c) 2002 The Regents of the University of California, as
 * Operator of Los Alamos National Laboratory.
 * This file is distributed subject to a Software License Agreement found
 * in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* program: cavput.c
 * purpose: vector CA puts from command line
 * M. Borland, 1995.
 $Log: not supported by cvs2svn $
 Revision 1.21  2010/04/16 15:52:12  soliday
 Added internal CA.Client.Exception handling.

 Revision 1.20  2009/02/17 19:52:14  soliday
 Updated so that ca_task_exit is called even if it exits with an error
 because this is needed on linux so it doesn't segfault.

 Revision 1.19  2008/02/19 20:13:56  soliday
 Found a memory freeing bug.

 Revision 1.18  2006/10/19 22:19:28  soliday
 Added support for linux-x86_64.

 Revision 1.17  2005/11/08 22:04:35  soliday
 Added support for 64 bit compilers.

 Revision 1.16  2004/09/27 14:45:26  soliday
 Updated the section where the arguments are parsed.

 Revision 1.15  2004/09/25 15:11:47  borland
 Encountered problem with program hanging when setting large numbers of
 PVs on Linux.  It hung when doing free(channelID).  For some reason,
 freeing this first fixed the problem.

 Revision 1.14  2004/07/19 18:20:38  soliday
 Replaced sleep commands with ca_pend_event commands because
 Epics/Base 3.14.x has an inactivity timer that was causing disconnects
 from PVs when the log interval time was too large. Updated the usage
 message so that it displays the Epics version.

 Revision 1.13  2003/12/04 22:32:30  soliday
 If a channel is not connect and blunderAhead silently is set it will not
 notify the user.

 Revision 1.12  2003/09/02 21:20:49  soliday
 Cleaned up the code for Linux.

 Revision 1.11  2003/08/08 18:06:37  soliday
 Added missing ca_task_end call.

 Revision 1.10  2003/07/21 18:14:57  soliday
 Fixed issue with last change.

 Revision 1.9  2003/07/19 23:56:36  borland
 Moved a ca_pend_io() statement and added a very short usleep() call to
 get it to work on R3.14.  Doesn't make sense, but it works now.

 Revision 1.8  2002/11/26 22:34:01  soliday
 Removed references to ezca.

 Revision 1.7  2002/10/09 07:48:46  shang
 added -ramp option

 revision 1.6  2002/08/02 15:39:55;  author: jba
 Added license information
 
 Revision 1.5  2002/06/25 16:48:56  shang
 freed memory leaks

 Revision 1.4  2002/05/08 19:52:36  shang
 freed memory leaks

 Revision 1.3  2000/10/16 16:22:22  soliday
 Removed tsDefs.h include statement.

 Revision 1.2  2000/10/11 20:03:04  soliday
 Removed warnings on Solaris with gcc

 Revision 1.1.1.1  1999/03/11 22:23:32  borland
 Moved from ca area

 Revision 1.7  1996/09/21 21:00:59  borland
 Added -blunderAhead option to allow puts to proceed in spite of unconnected
 PVs.

 Revision 1.6  1996/05/03 23:39:54  borland
 Added error checking to ca_pend_io() call.  Added -numerical option.
 Reformatted code.

 * Revision 1.5  1996/05/03  02:18:09  borland
 * Added error checking and messages for case when no values are supplied.
 * Fixed format statement for -dryRun printout.
 *
 * Revision 1.4  1996/02/14  05:37:36  borland
 * Change from obsolete scan_item_list() routine to scanItemList(), which
 * offers better error detection and flagging.
 *
 * Revision 1.3  1996/01/04  21:50:03  borland
 * Added factor qualifier to -deltaMode option for multiplying offsets by
 * a number.
 *
 * Revision 1.2  1996/01/04  15:32:29  borland
 * Added -deltaMode option for sending delta values instead of absolute values.
 *
 * Revision 1.1  1995/09/18  20:00:29  saunders
 * Cavput and cawait (by Michael Borland) added and makefile modified as needed.
 *
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
#define COMMANDLINE_OPTIONS 11
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
};

#define PROVIDER_CA 0
#define PROVIDER_PVA 1
#define PROVIDER_COUNT 2
char *providerOption[PROVIDER_COUNT] = {
  (char *)"ca",
  (char *)"pva",
};

static char *USAGE = (char *)"cavput [-list=<string>[=<value>][,<string>[=<value>]...]]\n\
[-range=begin=<integer>,end=<integer>[,format=<string>][,interval=<integer>]]\n\
[-pendIoTime=<seconds>] [-deltaMode[=factor=<value>]] [-ramp=step=<n>,pause=<sec>] \n\
[-numerical] [-blunderAhead[=silently]]\n\
[-provider={ca|pva}]\n\n\
-list           specifies PV name string components\n\
-range          specifies range of integers and format string\n\
-pendIoTime     specifies maximum time to wait for connections and\n\
                return values.  Default is 1.0s \n\
-dryRun         specifies showing PV names and values but\n\
                not sending to IOCs.\n\
-deltaMode      specifies that values are deltas from present values in\n\
                PVs.\n\
-numerical      forces conversion of the value to a number and sending as\n\
                a number.  Default for -deltaMode.\n\
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
long PrepPVAPutValues(PVA_OVERALL *pva, PV_VALUE *PVvalue);
#endif

int main(int argc, char **argv) {
  int32_t beginRange, endRange, rangeInterval, rampSteps;
  long dryRun;
  long PVs, j, i_arg, status, numerical, iramp;
  unsigned long blunderAhead;
  PV_VALUE *PVvalue;
  TERM_LIST *List;
  unsigned long flags, dummyFlags;
  SCANNED_ARG *s_arg;
  char *ptr, *rangeFormat;
  double pendIOTime;
  chid *channelID = NULL;
  short deltaMode, doRamp;
  double *presentValue, deltaFactor, rampPause, rampValue, rampDelta;
  long providerMode = 0;
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

  for (j = 0; j < PVs; j++)
    if (!PVvalue[j].value) {
      fprintf(stderr, "Error: no value given for %s\n", PVvalue[j].name);
      exit(1);
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
    if (providerMode == PROVIDER_PVA) {
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
          provider[j] = "pva";
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
            exit(1);
          continue;
        }
      }
      //Is there a way to check write access rights? If so, add it here.

      //Get the current values. This is mostly done to prepopulate the PVstructure which makes it easier to change the values later.
      if (GetPVAValues(&pva) == 1) {
        return (1);
      }

      //Do not allow ramp or delta modes for non-scalar data
      if (doRamp || deltaMode) {
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j]) {
            if (pva.pvaData[j].fieldType != epics::pvData::scalar) {
              fprintf(stderr, "error (cavput): %s is not a scalar type PV, delta and ramp options are only available for scalar type PVs\n", PVvalue[j].name);
              return (1);
            }
          }
        }
      }

      //Extract PV values given on the commandline and place them in the pva data structure
      if (PrepPVAPutValues(&pva, PVvalue)) {
        return (1);
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
        //Calculate ramp step sizes
        rampDeltas = (double *)malloc(sizeof(double) * PVs);
        for (j = 0; j < PVs; j++) {
          if (pva.isConnected[j] && (pva.pvaData[j].fieldType == epics::pvData::scalar) && pva.pvaData[j].numeric) {
            rampDeltas[j] = (pva.pvaData[j].putData[0].values[0] - pva.pvaData[j].getData[0].values[0]) / rampSteps;
            pva.pvaData[j].putData[0].values[0] = pva.pvaData[j].getData[0].values[0];
          }
        }
        //Put ramp values
        for (iramp = 1; iramp <= rampSteps; iramp++) {
          for (j = 0; j < PVs; j++) {
            if (pva.isConnected[j] && (pva.pvaData[j].fieldType == epics::pvData::scalar) && pva.pvaData[j].numeric) {
              pva.pvaData[j].putData[0].values[0] = pva.pvaData[j].putData[0].values[0] + rampDeltas[j];
            }
          }
          if (PutPVAValues(&pva) == 1) {
            return (1);
          }
          if ((rampPause > 0) && (iramp < rampSteps)) {
            usleepSystemIndependent(rampPause * 1000000);
          }
        }
        free(rampDeltas);
      } else {
        //Put values
        if (PutPVAValues(&pva) == 1) {
          return (1);
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
      return (0);
#else
      fprintf(stderr, "-provider=pva is only available with newer versions of EPICS\n");
      return (1);
#endif
    } else {
      ca_task_initialize();
      ca_add_exception_event(oag_ca_exception_handler, NULL);
      if (!(channelID = (chid *)malloc(sizeof(*channelID) * PVs)))
        bomb((char *)"memory allocation failure (cavput)", NULL);
      for (j = 0; j < PVs; j++) {
        if (ca_search(PVvalue[j].name, channelID + j) != ECA_NORMAL) {
          fprintf(stderr, "error (cavput): problem doing search for %s\n",
                  PVvalue[j].name);
          ca_task_exit();
          exit(1);
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
              exit(1);
            }
          }
        }
        if (!(presentValue = (double *)malloc(sizeof(*presentValue) * PVs))) {
          fprintf(stderr, "error (cavput): memory allocation failure\n");
          ca_task_exit();
          exit(1);
        }
        for (j = 0; j < PVs; j++) {
          presentValue[j] = DBL_MAX;
          if (ca_state(channelID[j]) != cs_conn)
            continue;
          if (ca_get(DBR_DOUBLE, channelID[j], presentValue + j) != ECA_NORMAL) {
            fprintf(stderr, "error (cavput): problem doing get for %s\n",
                    PVvalue[j].name);
            ca_task_exit();
            exit(1);
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
            exit(1);
          }
          if (sscanf(PVvalue[j].value, "%le", &PVvalue[j].numericalValue) != 1) {
            fprintf(stderr, "error (cavput): value (%s) for %s is not numerical--can't use -deltaMode or -ramp\n",
                    PVvalue[j].value, PVvalue[j].name);
            ca_task_exit();
            exit(1);
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
                exit(1);
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
                exit(1);
              }
            }
            if (ca_put(DBR_DOUBLE, channelID[j], &PVvalue[j].numericalValue) != ECA_NORMAL) {
              if (!(blunderAhead & BLUNDER_SILENTLY))
                fprintf(stderr, "error (cavput): problem doing put for %s\n",
                        PVvalue[j].name);
              if (!blunderAhead) {
                ca_task_exit();
                exit(1);
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
                exit(1);
              }
            }
          }
          if ((status = ca_pend_io(pendIOTime)) != ECA_NORMAL) {
            fprintf(stderr, "Problem processing one or more puts (cavput):\n%s\n",
                    ca_message(status));
            ca_task_exit();
            exit(1);
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
        exit(1);
      }
      ca_pend_event(0.5);
#ifdef DEBUG
      fprintf(stderr, "ca_pend_io returned\n");
#endif

      ca_task_exit();
#ifdef DEBUG
      fprintf(stderr, "ca_task_exit returned\n");
#endif
    }
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

  return (0);
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
    _exit(1);
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
long PrepPVAPutValues(PVA_OVERALL *pva, PV_VALUE *PVvalue) {
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
