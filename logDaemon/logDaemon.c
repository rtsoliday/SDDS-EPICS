/* 
 *
 * $Log: not supported by cvs2svn $
 * Revision 1.2  2004/11/18 15:42:44  soliday
 * No longer accepts tags that are not in the correct order.
 *
 * Revision 1.1  2003/08/27 19:45:47  soliday
 * Moved to a subdirectory of SDDSepics.
 *
 * Revision 1.19  2003/07/19 01:46:41  borland
 * Builds with epics base R3.14.  Still builds with earlier versions, also.
 *
 * Revision 1.18  2002/07/24 23:13:05  shang
 * modified to log data into daily files
 *
 * Revision 1.17  2000/12/01 18:16:55  soliday
 * It now flushes the data after writing it.
 *
 * Revision 1.16  2000/06/26 17:01:26  soliday
 * It now ignores extra line feeds in the log.sourceId file.
 *
 * Revision 1.15  2000/05/17 15:32:15  soliday
 * Removed Linux compiler warnings.
 *
 * Revision 1.14  2000/05/17 14:53:37  soliday
 * Removed Solaris compiler warnings.
 *
 * Revision 1.13  1996/10/23 19:46:47  saunders
 * Improved error checking in both client library and server. Added
 * testLogger utility to automatically test basic functionality of client
 * library and server. Greatly reduced stack usage by client library
 * and removed redundant string copying.
 *
 * Revision 1.12  1996/10/22 21:34:57  saunders
 * A number of improvements resulting from design review.
 * 1. Don't stop config file matching on first match. Log the message
 *    to all valid matches.
 *
 * 2. In logDaemonValidateSourceId:
 *    Don't allow a subset of tagList to be considered valid.
 *
 * 3. Make LOG_ENT one big buffer of size MAX_UDP_SIZE, and make secs,
 *    usecs, and tag name/value pairs an array of ptrs. This allows
 *    tag values to use almost all of UDP packet if desired.
 *
 * 4. Add logArray() to library.
 *
 * Revision 1.11  1996/05/30 21:27:57  saunders
 * Fixed bug whereby -p command line option was being ignored.
 *
 * Revision 1.10  1996/03/27 21:21:45  saunders
 * V2.0 of logDaemon ready and tested.
 *
 * Revision 1.9  1996/03/26 00:46:52  saunders
 * Upgrade to arbitrary tag fields complete except for cleaning up
 * comments, and format of email message.
 *
 * Revision 1.8  1996/03/24 21:47:58  saunders
 * Added logMessage acknowledgement. Changed time output to one double field.
 *
 * Revision 1.7  1996/03/22 16:16:21  saunders
 * Incorporated use of syslog facility to report startup/operation errors.
 *
 * Revision 1.6  1996/03/21 22:55:34  saunders
 * Close to completion of logDaemon upgrade (tagged message fields).
 *
 * Revision 1.5  1996/03/05 20:40:09  saunders
 * Removed stat of self and uid/gid setting at startup time.
 *
 * Revision 1.4  1996/02/11  20:34:54  saunders
 * Renamed time field from nsec, to usec.
 *
 * Revision 1.3  1996/02/06  23:57:21  saunders
 * Removed use of lseek to maintain row count. New SDDS_SetNoRowCounts allows
 * me to disable row count.
 *
 * Revision 1.2  1996/02/06  23:14:04  saunders
 * Log file generation names now fixed width and padded with 0's.
 * This makes sorting them much easier.
 *
 * Revision 1.1.1.1  1995/12/13 22:05:47  saunders
 * Imported Files
 * */
/*************************************************************************
 * FILE:      logDaemon.c
 * Author:    Claude Saunders and Jim Kowalkowski
 * 
 * Purpose:   General purpose message log server. Supports an ascii
 *            log file format as well as an SDDS log file format.
 *            Runs as a single-threaded server, processing incoming
 *            UDP packets with the correct header. Messages may have
 *            an arbitrary number of fields, specified when the log
 *            session is opened.
 *
 *************************************************************************/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <syslog.h>
#include <ellLib.h>

#undef DEBUG
#include "Debug.h"
#include "logDaemonConfig.h"
#include "BSlib.h"
#include "mdb.h"

#ifdef SDDS
#  include <SDDS.h>
#  define SDDS_MODE 0
#  define TEXT_MODE 1
#endif

/****** Data Type Declarations ********/

/* Each node is one tag name/value pair */
/****************************************/
typedef struct {
  ELLNODE nodePad;
  char tag[STDBUF];
  char value[MAX_MESSAGE_SIZE];
} TAGNODE;

/* Each node contains sourceId, and associated list of tag names. */
/******************************************************************/
typedef struct {
  ELLNODE nodePad;
  char sourceId[STDBUF]; /* source of log message */
  ELLLIST tagList;
} SOURCETAGNODE;

/* An instance of the TYPE node is created for each log file or   */
/* mail recipient given in the server configuration file.         */
/******************************************************************/
typedef struct {
  ELLNODE nodePad;
  char sourceId[STDBUF];        /* source of log message (class of user) */
  SOURCETAGNODE *sourceTagNode; /* ptr to sourceId node which we will    */
                                /*  use to create SDDS column names.     */
  ELLLIST tagList;
  char area;       /* area is log or mail */
  int log_file_fd; /* file descriptor for log file */
#ifdef SDDS
  SDDS_TABLE table;
  long row_count; /* SDDS row count */
  off_t h_offset; /* byte offset of end of SDDS header */
#endif
  off_t size;     /* log file size in bytes */
  char *log_mail; /* log address */
  char *log_file; /* log file name */
} TYPENODE;

/* The LOG_ENT struct holds the information for a single log transaction. */
/**************************************************************************/
typedef struct _log_entry {
  SOURCETAGNODE *sourceTagNode;
  char field_buffer[MAX_MESSAGE_SIZE]; /* full log message */
  char *secs;                          /* ptrs to subfields of msg */
  char *usecs;
  char *tagValues[MAX_MESSAGE_SIZE];
  int numTagValues;
} LOG_ENT;

/****** Local Function Prototypes ********/
void logDaemonReadConfig();
void logDaemonStartNewLog();
void logDaemonRequestReadConfig(int sig);
void logDaemonOpenLogfile(TYPENODE *typeNode);
void logDaemonAppendPageToLogFile(TYPENODE *typeNode);
void logDaemonCloseLogfile(TYPENODE *typeNode);
void logDaemonFindMatchingEntries();
void logDaemonAddDefaultTypeNode(char *sourceId);
void logDaemonUpdateTypeList(char *sourceId);
SOURCETAGNODE *logDaemonFindSourceTagNode(char *sourceId);
int logDaemonValidateSourceId(char *sourceIdTagString);
int logDaemonMainLoop(int sockfd);
int logDaemonWriteMessage(TYPENODE *typeNode);
int logDaemonSendMail(TYPENODE *typeNode);
int logDaemonNewGeneration(TYPENODE *typeNode);
void logDaemonExampleConfig();
#ifdef SDDS
int logDaemonGetSDDSHeaderOffset(SDDS_TABLE *table, off_t *h_offset);
int logDaemonWriteSDDSHeader(SDDS_TABLE *table, TYPENODE *typeNode);
#endif

/****** Global Variables ************/
/*  Log daemon configuration data   */
char log_service_id[STDBUF]; /* "name" of the log daemon service */
char *config_file;           /* config filename       */
char *log_home;              /* home dir for logfiles */
char *log_savedir;           /* dir for logfile save  */
off_t log_maxsize;           /* max logfile size      */
int log_port;                /* udp port number       */
#ifdef SDDS
int mode; /* SDDS or regular */
#endif
double startHour;

/* Environment variable strings corresponding to above config data */
char *logger_id;
char *logger_port, *logger_config;
char *logger_home, *logger_savedir, *logger_maxsize;

/* Assorted vars */
char argv0[STDBUF];        /* executable name */
int Refresh = 0;           /* flag indicating a daemon refresh occurred */
int requestReadConfig = 0; /* flag requesting server reconfiguration */

/* The four main data structures maintained during execution  */
/**************************************************************/
ELLLIST typeList;      /* main configuration type list */
ELLLIST sourceIdList;  /* list of sourceId->tagList associations */
LOG_ENT log_ent;       /* space for data from an incoming log entry */
TYPENODE **matchArray; /* Holds array of ptrs to TYPENODES which match
				   incoming log entry. Stored from location
				   zero with a NULL ptr terminating array.
				   Dynamically resized if needed. */

size_t matchArraySize; /* number of ptrs allocated for matchArray */
size_t nextFreeMatch;  /* next available element to store ptr */

extern int errno;

#ifdef SDDS
char usage[] = "\n[-m <mode>] text or SDDS\n[-i <server_id>] server name which can be referenced by client\n[-f <config_file>] server config file name\n[-p <log_port>] server UDP port number\n[-r] start log file at beginning even if it exists \n[-h <home_dir>] dir for log files \n[-o <log_generations_dir>] dir to which previous log files are copied\n[-s <max_log_size_in_bytes>] max size before copied to <log_generations_dir>\n[-e] Print example config file to stdout\n";
#else
char usage[] = "\n[-i <server_id>] server name which can be referenced by client\n[-f <config_file>] server config file name\n[-p <log_port>] server UDP port number\n[-r] start log file at beginning even if it exists \n[-h <home_dir>] dir for log files \n[-o <log_generations_dir>] dir to which previous log files are copied\n[-s <max_log_size_in_bytes>] max size before copied to <log_generations_dir>\n[-e] Print example config file to stdout\n";
#endif

char mess_format[] = "usage: %s %s\n\n";

/*************************************************************************
 * FUNCTION : main()
 * PURPOSE  : Read environment variables and/or command line options.
 *            Put process into server mode (detach from parent, etc...)
 *            Read the configuration file and open log files
 *            Enter main loop for processing incoming log messages
 *
 *             arguments: 
 *             -m log file mode (text or SDDS) -->only #ifdef SDDS
 *             -i log daemon service name (so client broadcast can find 
 *                                         given server)
 *             -f configuration file name
 *             -p logger port number
 *             -r refresh all logs
 *             -h home directory of logger
 *             -o directory for log file generations
 *             -s max size of log files in bytes
 *             -e print example of config file to stdout
 *  
 *             environment variables (see logDaemonConfig.h for real names):
 *             LOGGER_ID = name of this service (so client broadcast can 
 *                                                find given server)
 *             LOGGER_PORT  = UDP port to read log messages from
 *             LOGGER_CONFIG = name of logger configuration file
 *             LOGGER_HOME  = logger home directory
 *             LOGGER_SAVEDIR = directory for log file generations
 *             LOGGER_MAXSIZE = max size of a log file in bytes
 *
 *             main loop:
 *             while (1) {
 *                get UDP packet
 *                if (re-read of config file requested via signal) 
 *                   logDaemonReadConfig();
 *                if (broadcast_type and server id matches) {
 *                   validate sourceId/tagList
 *                   acknowledge broadcast
 *                } else {
 *                   extract log fields and write to file and/or email
 *                   acknowledge log entry
 *                }
 *             }
 * 
 *             incoming log message contains the following fields:
 *                1) time stamp of message in secondsAfterEpoch
 *                2) microsecond part of time stamp
 *                3) sourceId of message (EPICS, SDDS, etc...)
 *                4) tag value string
 *                ...
 *                n) tag value string
 *
 *             The sourceId is used to identify the order of tag values
 *             expected. The sourceId and tag values are then matched
 *             against specifications in the config file to determine
 *             what log file should be written and/or who should receive
 *             email. If no config file entry matches, a default log file
 *             named "<sourceId>/YYYY-JJJ-MMDD.log" is used.
 *             (YYYY-JJJ-MM-DD is the current date)
 ************************************************************************/
int main(int argc, char *argv[]) {
  extern char *optarg;
  int opt;
  /* struct stat stat_buf;*/
  int sockfd;

  /* Initialize syslog */
  openlog(argv[0], LOG_PID, LOG_USER);

  /* store executable name in global for other functions */
  strcpy(argv0, argv[0]);

  /*
  if (stat(argv[0],&stat_buf) < 0) {
     syslog(LOG_ERR,"could not stat self: %m");
  }
  
  if (setuid(stat_buf.st_uid) < 0) {
    syslog(LOG_ERR,"Warning:Cannot set uid to logD owner: %m");
  }
  
  if (setgid(stat_buf.st_gid) < 0) {
    syslog(LOG_ERR,"Warning:Cannot set gid to logD group: %m");
  }
*/

#ifdef SDDS
  mode = SDDS_MODE;
#endif
  strcpy(log_service_id, "*");
  strcat(log_service_id, DEF_LOGSERVER_ID);
  config_file = DEF_CONFIGFILE;
  log_port = DEF_LOGPORT;
  log_home = DEF_HOME;
  log_savedir = DEF_SAVEDIR;
  log_maxsize = DEF_LOGSIZE;
  Refresh = 0;

  /*-----------------------------------------------------------*/
  /* get environment variables if present */

  if ((logger_id = (char *)getenv(LOGGER_ID)) != (char *)NULL) {
    strcpy(log_service_id, "*");
    strcat(log_service_id, logger_id);
  }

  if ((logger_port = (char *)getenv(LOGGER_PORT)) != (char *)NULL) {
    log_port = (short)atoi(logger_port);
  }
  /* Validate UDP port number */
  if (log_port <= USER_RESERVED_UDP_PORT) {
    syslog(LOG_ERR, "invalid udp port number, must be > %d",
           USER_RESERVED_UDP_PORT);
    exit(1);
  }
  Debug("Server port is %d\n", log_port);

  if ((logger_config = (char *)getenv(LOGGER_CONFIG)) != (char *)NULL) {
    config_file = (char *)malloc(strlen(logger_config) + 1);
    strcpy(config_file, logger_config);
  }

  if ((logger_home = (char *)getenv(LOGGER_HOME)) != (char *)NULL) {
    log_home = (char *)malloc(strlen(logger_home) + 1);
    strcpy(log_home, logger_home);
  }

  if ((logger_savedir = (char *)getenv(LOGGER_SAVEDIR)) != (char *)NULL) {
    log_savedir = (char *)malloc(strlen(logger_savedir) + 1);
    strcpy(log_savedir, logger_savedir);
  }

  if ((logger_maxsize = (char *)getenv(LOGGER_MAXSIZE)) != (char *)NULL) {
    if ((log_maxsize = atoi(logger_maxsize)) == 0) {
      syslog(LOG_ERR, "invalid log max size entered, must be integer");
      exit(1);
    }
  }

  /* Read the command line options */
#ifdef SDDS
  while ((opt = getopt(argc, argv, "m:i:f:p:rs:o:h:e")) != EOF) {
    switch (opt) {
    case 'm':
      if (!strcmp(optarg, "text"))
        mode = TEXT_MODE;
      else if (!strcmp(optarg, "SDDS"))
        mode = SDDS_MODE;
      else {
        syslog(LOG_ERR, "illegal -m option, 'text' or 'SDDS' only");
        exit(1);
      }
      break;
    case 'i':
      strcpy(log_service_id, "*");
      strcat(log_service_id, optarg);
      break;
    case 'f':
      config_file = (char *)malloc(strlen(optarg) + 2);
      strcpy(config_file, optarg);
      break;
    case 'p':
      log_port = (short)atoi(optarg);
      printf("setting log port to: %d\n", log_port);
      break;
    case 'r':
      Refresh = 1;
      break;
    case 's':
      if ((log_maxsize = atoi(optarg)) == 0) {
        syslog(LOG_ERR, "Invalid log max size entered, must by integer");
        exit(1);
      }
      break;
    case 'o':
      log_savedir = (char *)malloc(strlen(optarg) + 2);
      strcpy(log_savedir, optarg);
      break;
    case 'h':
      log_home = (char *)malloc(strlen(optarg) + 2);
      strcpy(log_home, optarg);
      break;
    case 'e':
      logDaemonExampleConfig();
      exit(0);
    default:
      fprintf(stderr, mess_format, argv[0], usage);
      exit(1);
    }
  }
#else
  while ((opt = getopt(argc, argv, "i:f:p:rs:o:h:e")) != EOF) {
    switch (opt) {
    case 'i':
      strcpy(log_service_id, "*");
      strcat(log_service_id, optarg);
      break;
    case 'f':
      config_file = (char *)malloc(strlen(optarg) + 2);
      strcpy(config_file, optarg);
      break;
    case 'p':
      log_port = (short)atoi(optarg);
      break;
    case 'r':
      Refresh = 1;
      break;
    case 's':
      if ((log_maxsize = atoi(optarg)) == 0) {
        syslog(LOG_ERR, "Invalid log max size entered, must by integer");
        exit(1);
      }
      break;
    case 'o':
      log_savedir = (char *)malloc(strlen(optarg) + 2);
      strcpy(log_savedir, optarg);
      break;
    case 'h':
      log_home = (char *)malloc(strlen(optarg) + 2);
      strcpy(log_home, optarg);
      break;
    case 'e':
      logDaemonExampleConfig();
      exit(0);
    default:
      fprintf(stderr, mess_format, argv[0], usage);
      exit(1);
    }
  }
#endif

  /* Open UDP socket to receive messages */
  if ((sockfd = BSopenListenerUDP(log_port)) == -1) {
    syslog(LOG_ERR, "BSopenListenerUDP() can't create and bind socket: %m");
    exit(1);
  }

  /* Make myself a server process. */
  BSmakeServer();

  /* kill -HUP will request that config file and sourceId database be read
     upon receipt of the next log message.
  */
  signal(SIGHUP, logDaemonRequestReadConfig);

  if (chdir(log_home) < 0) {
    syslog(LOG_ERR, "cannot change to logDaemon home directory: %m");
  }
  if (access(log_savedir, 00) < 0) {
    if (errno == ENOENT) {
      if (mkdir(log_savedir, 0777) < 0) {
        syslog(LOG_ERR, "could not create logger save dir: %m");
      }
    } else {
      syslog(LOG_ERR, "access to savedir denied: %m");
    }
  }

  /* Initialize sourceId->tagList list. */
  ellInit(&sourceIdList);

  /* Initialize type list */
  ellInit(&typeList);

  /* Initialize the match array */
  matchArray = (TYPENODE **)malloc(sizeof(TYPENODE *) * CONFIG_BLOCK_SIZE);
  matchArray[0] = NULL;
  matchArraySize = CONFIG_BLOCK_SIZE;
  nextFreeMatch = 0;

  /* Read the config file and sourceId->tagList file */
  logDaemonReadConfig();
  startHour = getHourOfDay();
  /* Process incoming UDP packets */
  logDaemonMainLoop(sockfd);
  return (0);
}

/*shang*/
/*************************************************************************
 * FUNCTION : logDaemonStartNewLog()
 * PURPOSE  : start new log files after midnight.
 *            Searches through typeList for instances of sourceId, remove the
 *            date tag (-YYYY-JJJ-MM-DD), and add current date tag to sourceId,
 *            overwrite the sourceId file using the new sourceIds and
 *            initialize logs by calling logDaemonReadConfig()
 * ARGS in  : none
 * ARGS out : none
 * GLOBAL   : modifiy the log_mail field of typeNode in typeList (global) 
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonStartNewLog() {
  TYPENODE *typeNode;
  char *sourceId;
  char defaultLogFile[STDBUF + 5];

  typeNode = (TYPENODE *)ellFirst(&typeList);
  /*  fprintf(stderr,"startHour=%f, hourNow=%f\n",startHour,getHourOfDay()); */
  while (typeNode != NULL) {
    sourceId = typeNode->sourceId;
    /*close old files */
    if (typeNode->area == 'l')
      logDaemonCloseLogfile(typeNode);
    free(typeNode->log_file);

    /*open new files with updated date*/
    /*strcpy(defaultLogFile,MakeDailyGenerationFilename(sourceId,0,"-",0));*/
    strcpy(defaultLogFile, sourceId);
    strcat(defaultLogFile, "/");
    strcat(defaultLogFile, MakeDailyGenerationFilename(NULL, 0, NULL, 0));
    strcat(defaultLogFile, ".log");
    typeNode->log_file = (char *)malloc(strlen(defaultLogFile) + 1);
    strcpy(typeNode->log_file, defaultLogFile);
    typeNode->log_file_fd = -1;
    typeNode->area = 'l';
    if (typeNode->area == 'l' && typeNode->sourceTagNode != NULL)
      logDaemonOpenLogfile(typeNode);
    typeNode = (TYPENODE *)ellNext((ELLNODE *)typeNode);
  }
}
/*shang*/

/*************************************************************************
 * FUNCTION : logDaemonRequestReadConfig()
 * PURPOSE  : SIGHUP signal handler which sets a flag so that the
 *            logDaemonMainLoop will re-read the config file and
 *            sourceId->tagList file upon receipt of the next message.
 * ARGS in  : sig - signal number
 * ARGS out : none
 * GLOBAL   : sets requestReadConfig flag
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonRequestReadConfig(int sig) {
  requestReadConfig = 1;

#if defined linux || defined SOLARIS
  signal(SIGHUP, logDaemonRequestReadConfig);
#endif
}

/*************************************************************************
 * FUNCTION : logDaemonReadConfig()
 * PURPOSE  : Reads the configuration file and sets up typeList in memory
 *            which contains the log files and mail id's for all (sourceId,
 *            tagValue(s)) specified in the config file. If no config 
 *            file entry exists, a "catch-all" entry is made. Also reads
 *            the "log.sourceId" file which initializes sourceIdList.
 *            This list specifies the valid tag names for a given sourceId.
 * ARGS in  : none
 * ARGS out : none
 * GLOBAL   : Allocates and manipulates global sourceIdList and typeList.
 *            Log files specified in config file are also opened.
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonReadConfig() {
  int i, j, k; /* counters for strtok loops */
  char *p, *r; /* ptrs for strtok loops */
  char *string;
  FILE *config_fp;                  /* configuration file */
  FILE *sourceId_fp;                /* sourceId->tagList file */
  char buffer[MAX_MESSAGE_SIZE];    /* buffer for config file entry */
  char tagValBuf[MAX_MESSAGE_SIZE]; /* for processing tag/Value field */
  char tagName[STDBUF], defaultLogFile[STDBUF + 5];
  char tagPattern[MAX_MESSAGE_SIZE];
  int invalidEntry = 0;
  SOURCETAGNODE *sourceTagNode;
  TAGNODE *tagNode;
  TYPENODE *typeNode, *typeNode2;
  size_t typeListLength, numBlocks;

  /* Reset the typeList linked list. */
  typeNode = (TYPENODE *)ellFirst(&typeList);
  while (typeNode != NULL) {
    if (typeNode->area == 'l')
      logDaemonCloseLogfile(typeNode);
    free(typeNode->log_mail);
    free(typeNode->log_file);
    ellFree(&typeNode->tagList);
    typeNode = (TYPENODE *)ellNext((ELLNODE *)typeNode);
  }
  ellFree(&typeList);
  ellInit(&typeList);

  Debug("freed up typeList\n", NULL);

  /* Also reset the sourceId->tagList linked list. */
  sourceTagNode = (SOURCETAGNODE *)ellFirst(&sourceIdList);
  while (sourceTagNode != NULL) {
    ellFree(&sourceTagNode->tagList);
    sourceTagNode = (SOURCETAGNODE *)ellNext((ELLNODE *)sourceTagNode);
  }
  ellFree(&sourceIdList);
  ellInit(&sourceIdList);

  Debug("freed up sourceIdList\n", NULL);

  /* Open and read the sourceId->tagList database */
  if ((sourceId_fp = fopen(DEF_SOURCEIDFILE, "r")) == NULL) {
    Debug("no sourceId->tagList database found\n", NULL);
  } else {
    Debug("scanning sourceId->tagList database\n", NULL);
    /* for each line in file */
    for (j = 0; (fgets(buffer, sizeof(buffer), sourceId_fp) != NULL);) {
      /* for each field in line */
      for (i = 0, p = strtok(buffer, "~"); p; p = strtok(NULL, "~\n"), i++) {
        if (i == 0) {
          if (strlen(p) == 1)
            continue;
          sourceTagNode = (SOURCETAGNODE *)malloc(sizeof(SOURCETAGNODE));
          strcpy(sourceTagNode->sourceId, p);
          ellInit(&sourceTagNode->tagList);
          ellAdd(&sourceIdList, (ELLNODE *)sourceTagNode);
        } else {
          tagNode = (TAGNODE *)malloc(sizeof(TAGNODE));
          strcpy(tagNode->tag, p);
          ellAdd(&(sourceTagNode->tagList), (ELLNODE *)tagNode);
        }
      }
    }
    fclose(sourceId_fp);
  }

  if ((config_fp = fopen(config_file, "r")) == NULL) {
    Debug("no config file %s, default log files only\n", config_file);

  } else {
    Debug("Reading config file\n", 0);

    for (j = 0; (fgets(buffer, sizeof(buffer), config_fp) != NULL);) {
      switch (buffer[0]) { /* skip all empty lines and comments */
      case '#':
      case '\n':
      case '\t':
        continue;
      }
      Debug("j=%d\n", j);

      invalidEntry = 0;
      typeNode = (TYPENODE *)malloc(sizeof(TYPENODE));
      typeNode->log_mail = NULL;
      typeNode->log_file = NULL;

      typeNode->log_file_fd = -1;
      typeNode->sourceTagNode = NULL;
      ellInit(&(typeNode->tagList));

      /* Process the fields on a config entry */
      for (i = 0, p = strtok(buffer, "~"); p && !invalidEntry;
           p = strtok(NULL, "~\n"), i++) {
        switch (i) {
        case 0: /* field one (sourceId) */
          Debug("Got sourceId (%s)\n", p);
          if ((int)strlen(p) < STDBUF) {
            strcpy(typeNode->sourceId, p);

            /* Find entry in sourceIdList, and save ptr to it. */
            typeNode->sourceTagNode = logDaemonFindSourceTagNode(p);

            /* If not found, don't worry. The "typeNode->sourceTagNode" ptr
	       will be set when the logOpen() call gets new sourceId in.
	     */
          } else {
            syslog(LOG_ERR, "invalid sourceId %s in config file", p);
            invalidEntry = 1;
            continue;
          }
          break;

        case 1: /* field two (area of log or mail) */
          Debug("Got area (%s)\n", p);

          if (!strcmp(p, "log")) { /* log area */
            typeNode->area = 'l';
          } else if (!strcmp(p, "mail")) { /* mail area */
            typeNode->area = 'm';
          } else { /* error in config file entry */
            syslog(LOG_ERR, "invalid area %s in config file", p);
            invalidEntry = 1;
            continue;
          }
          break;

        case 2: /* field three (mail id's or file name) */
          Debug("Got file/mail (%s)\n", p);
          string = p;
          Debug("\tlog/mail thing (%s)\n", string);
          typeNode->log_mail = (char *)malloc(strlen(string) + 1);
          strcpy(typeNode->log_mail, string);
          fprintf(stderr, "%s\n", typeNode->log_mail);
          if ((typeNode->area == 'l') && (typeNode->sourceTagNode != NULL)) {
            strcpy(defaultLogFile, typeNode->sourceId);
            strcat(defaultLogFile, "/");
            strcat(defaultLogFile, MakeDailyGenerationFilename(NULL, 0, NULL, 0));
            strcat(defaultLogFile, ".log");
            typeNode->log_file = (char *)malloc(strlen(defaultLogFile) + 1);
            strcpy(typeNode->log_file, defaultLogFile);
            logDaemonOpenLogfile(typeNode);
            fprintf(stderr, "%s %s\n", typeNode->sourceId, typeNode->log_file);
          }
          break;

        default: /* trailing fields specify tag=value */
          Debug("Got tag value (%s)\n", p);
          strcpy(tagValBuf, p);
          strcat(tagValBuf, "\n");
          for (k = 0, r = strtok(tagValBuf, "="); r; r = strtok(NULL, "=\n"), k++) {
            if (k == 0) {
              if ((int)strlen(r) < STDBUF) {
                strcpy(tagName, r);
              } else {
                syslog(LOG_ERR, "invalid tagName: too long");
                continue;
              }
            } else if (k == 1) {
              strcpy(tagPattern, r);
            }
          }
          if (k != 2) {
            syslog(LOG_ERR, "invalid tagName/Pattern field");
            continue;
          }
          /* NOTE: Could check validity of tag names given here. Ie.
	     check that they match against existing sourceIdList.
	     This, though, will prevent someone from entering a 
	     config file line for a sourceId which isn't yet submitted 
	     by a logOpen() call. I choose instead to ignore invalid
	     tag names at the time a log message arrives and is matched
	     against typeList.
	     */
          tagNode = (TAGNODE *)malloc(sizeof(TAGNODE));
          strcpy(tagNode->tag, tagName);
          strcpy(tagNode->value, tagPattern);
          ellAdd(&(typeNode->tagList), (ELLNODE *)tagNode);
        }
      } /* end foreach field in line */

      if (i < 3 || invalidEntry) {
        syslog(LOG_ERR, "invalid config file entry");
        if (typeNode->log_mail)
          free(typeNode->log_mail);
        if (typeNode->log_file)
          free(typeNode->log_file);
        free(typeNode);
        continue;
      }

      j++;
      ellAdd(&typeList, (ELLNODE *)typeNode);

    } /* end foreach line in config file */

    fclose(config_fp);
  }

  /* Scan typeList, adding a "catch-all" entry if there isn't one already. */
  typeNode = (TYPENODE *)ellFirst(&typeList);
  while (typeNode != NULL) {
    typeNode2 = (TYPENODE *)ellFirst(&typeList);

    while (typeNode2 != NULL) {
      /* see if there is already a "catch-all" entry */
      if (!strcmp(typeNode2->sourceId, typeNode->sourceId) &&
          ellCount(&(typeNode2->tagList)) == 0)
        break;
      typeNode2 = (TYPENODE *)ellNext((ELLNODE *)typeNode2);
    }
    if (typeNode2 == NULL) /* there isn't, so add one */
      logDaemonAddDefaultTypeNode(typeNode->sourceId);

    typeNode = (TYPENODE *)ellNext((ELLNODE *)typeNode);
  }

  /* Scan the sourceIdList, adding a "catch-all" entry to the typeList,
     if there isn't one already.
  */
  sourceTagNode = (SOURCETAGNODE *)ellFirst(&sourceIdList);
  while (sourceTagNode != NULL) {
    typeNode2 = (TYPENODE *)ellFirst(&typeList);

    while (typeNode2 != NULL) {
      /* see if there is already a "catch-all" entry */
      if (!strcmp(typeNode2->sourceId, sourceTagNode->sourceId) &&
          ellCount(&(typeNode2->tagList)) == 0)
        break;
      typeNode2 = (TYPENODE *)ellNext((ELLNODE *)typeNode2);
    }
    if (typeNode2 == NULL) /* there isn't, so add one */
      logDaemonAddDefaultTypeNode(sourceTagNode->sourceId);

    sourceTagNode = (SOURCETAGNODE *)ellNext((ELLNODE *)sourceTagNode);
  }

  /* Reallocate matchArray if we have a big config file. */
  typeListLength = (size_t)ellCount(&typeList);
  if (typeListLength > matchArraySize) {
    numBlocks = typeListLength / CONFIG_BLOCK_SIZE + 1;
    free(matchArray);
    matchArray =
      (TYPENODE **)malloc(numBlocks * sizeof(TYPENODE *) * CONFIG_BLOCK_SIZE);
    matchArraySize = numBlocks * CONFIG_BLOCK_SIZE;
    matchArray[0] = NULL;
  }
}

/*************************************************************************
 * FUNCTION : logDaemonOpenLogfile()
 * PURPOSE  : Open log file and initialize typeNode fields which
 *            keep track of size, etc...
 * ARGS in  : typeNode - ptr to node in typeList
 * ARGS out : typeNode - fields initialized
 * GLOBAL   : Opens file 
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonOpenLogfile(TYPENODE *typeNode) {
  struct stat stat_buf;

#ifdef SDDS
  SDDS_TABLE *table;
  if (access(typeNode->sourceId, 00) < 0) {
    if (errno == ENOENT) {
      if (mkdir(typeNode->sourceId, 0777) < 0) {
        syslog(LOG_ERR, "could not create logger dir: %m");
      }
    } else {
      syslog(LOG_ERR, "access to log dir denied: %m");
    }
  }
  if (mode == TEXT_MODE) {
    if (Refresh) {
      unlink(typeNode->log_file);
    }
    if ((typeNode->log_file_fd =
           open(typeNode->log_file,
                O_WRONLY | O_APPEND | O_CREAT, LOGFILE_PERM)) < 0) {
      syslog(LOG_ERR, "cannot open log file %s: %m", typeNode->log_file);
    }

    if (stat(typeNode->log_file, &stat_buf) < 0) {
      syslog(LOG_ERR, "could not stat log file: %m");
      stat_buf.st_size = 0;
    }
    typeNode->size = stat_buf.st_size;
    Debug("text logfile opened\n", NULL);

  } else { /* mode == SDDS_MODE */
    int len;

    table = &(typeNode->table);
    typeNode->size = 0;
    if (access(typeNode->log_file, F_OK) == 0) { /* file exists */
      /* Move log file to save directory as a new generation */
      /*logDaemonNewGeneration(typeNode);*/
      /*append*/
      logDaemonAppendPageToLogFile(typeNode);
    } else {
      logDaemonWriteSDDSHeader(table, typeNode);
      typeNode->log_file_fd = fileno(table->layout.fp);

      /* this call used to get header length */
      logDaemonGetSDDSHeaderOffset(table, &(typeNode->h_offset));
      Debug("SDDS logfile opened with header offset %ld\n",
            typeNode->h_offset);

      len = 0;
      typeNode->size = typeNode->h_offset + len;
      typeNode->row_count = 0;
    }
  }
#else
  if (Refresh) {
    unlink(typeNode->log_file);
  }
  if ((typeNode->log_file_fd =
         open(typeNode->log_file,
              O_WRONLY | O_APPEND | O_CREAT, LOGFILE_PERM)) < 0) {
    syslog(LOG_ERR, "cannot open log file %s: %m", typeNode->log_file);
  }

  if (stat(typeNode->log_file, &stat_buf) < 0) {
    syslog(LOG_ERR, "could not stat log file: %m");
    stat_buf.st_size = 0;
  }
  typeNode->size = stat_buf.st_size;
  Debug("text logfile opened\n", NULL);
#endif
}

/*************************************************************************
 * FUNCTION : logDaemonCloseLogfile()
 * PURPOSE  : Close up log file.
 * ARGS in  : typeNode - ptr to node in typeList
 * ARGS out : none
 * GLOBAL   : closes file
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonCloseLogfile(TYPENODE *typeNode) {
  if (typeNode->log_file_fd == -1)
    return;
#ifdef SDDS
  if (mode == TEXT_MODE) {
    close(typeNode->log_file_fd);
  } else { /* mode == SDDS_MODE */
    if (!SDDS_Terminate(&(typeNode->table))) {
      syslog(LOG_ERR, "unable to close log file %s: %m", typeNode->log_file);
      /* SDDS_PrintErrors(stderr,
               (SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors)); */
    }
  }
#else
  close(typeNode->log_file_fd);
#endif
}

/*************************************************************************
 * FUNCTION : logDaemonFindMatchingEntries()
 * PURPOSE  : Does a linear search of typeList, attempting to find
 *            all entries that match the current log message (contained
 *            in log_ent). First, the sourceId in typeList must match
 *            the log message sourceId. Then, each tag value in the
 *            matching typeNode must match the tag value from the log
 *            message. As each complete match is found, the matching
 *            typeNode pointer is stored in the next slot of the global
 *            matchArray.
 * ARGS in  : none
 * ARGS out : none
 * GLOBAL   : Loads elements of matchArray with ptrs to matching typeNodes.
 *            Last element is set to NULL to indicate no more matches.
 * RETURNS  : nada
 ************************************************************************/
void logDaemonFindMatchingEntries() {
  TYPENODE *typeNode;
  TAGNODE *tagNode, *msgTagNode;
  int misMatch;
  int nextTagValue;
  char *tagValue;

  Debug("search for matching entries in typeList\n", NULL);

  /* Initialize match array */
  matchArray[0] = NULL;
  nextFreeMatch = 0;

  typeNode = (TYPENODE *)ellFirst(&typeList);
  while (typeNode != NULL) {
    misMatch = 0;
    Debug("checking sourceId: %s\n", typeNode->sourceId);
    if (!strcmp(log_ent.sourceTagNode->sourceId, typeNode->sourceId)) {
      tagNode = (TAGNODE *)ellFirst(&(typeNode->tagList));
      nextTagValue = 0;
      while (tagNode != NULL && !misMatch) {
        msgTagNode = (TAGNODE *)ellFirst(&(log_ent.sourceTagNode->tagList));
        while (nextTagValue < log_ent.numTagValues && !misMatch &&
               msgTagNode != NULL) {
          /* If tag names match, but values don't, give up on this typeNode */
          /* NOTE: This implies that I am ignoring invalid tag names in
                   the typeNode->tagList. Ie. someone has specified a tag
                   name in the config file which isn't defined for that
                   sourceId. It's harmless, so I choose to ignore it.
          */
          tagValue = log_ent.tagValues[nextTagValue++];
          if (!strcmp(msgTagNode->tag, tagNode->tag) &&
              strcmp(tagValue, tagNode->value)) {
            misMatch = 1;
            continue;
          }
          msgTagNode = (TAGNODE *)ellNext((ELLNODE *)msgTagNode);
        }
        tagNode = (TAGNODE *)ellNext((ELLNODE *)tagNode);
      }
      /* Made it through all matching without setting misMatch. */
      if (!misMatch) {
        matchArray[nextFreeMatch++] = typeNode;
        matchArray[nextFreeMatch] = NULL;
      }
    }
    typeNode = (TYPENODE *)ellNext((ELLNODE *)typeNode);
  }
}

/*************************************************************************
 * FUNCTION : logDaemonValidateSourceId()
 * PURPOSE  : Takes string from broadcast which contains sourceId and
 *            the tag names, and compares that against the existing
 *            database of sourceId->tagList entries. If not found,
 *            a new sourceId->tagList entry is added to memory, and
 *            the file log.sourceId.
 * ARGS in  : sourceIdTagString - null terminated string of format:
 *                   "sourceId~tag1~tag2~...~tagn"
 * ARGS out : nothing
 * GLOBAL   : (maybe) adds to sourceId->tagList database
 * RETURNS  : -1 - invalid sourceId (given tag list doesn't match known)
 *             0 - valid sourceId
 *             1 - given sourceId is new (added to memory, and file)
 ************************************************************************/
int logDaemonValidateSourceId(char *sourceIdTagString) {
  char *p;
  int i, j;
  char arg1[MAX_UDP_SIZE + 1];
  FILE *sourceId_fp;
  SOURCETAGNODE *sourceTagNode = NULL;
  TAGNODE *tagNode;
  int numMatches = 0;

  arg1[0] = '\0';
  strcpy(arg1, sourceIdTagString);

#ifdef DEBUG
  /* Print out sourceId data structure
  sourceTagNode = (SOURCETAGNODE *)ellFirst(&sourceIdList);
  while (sourceTagNode != NULL) {
    printf("sourceId: %s\n",sourceTagNode->sourceId);
    tagNode = (TAGNODE *)ellFirst(&sourceTagNode->tagList);
    while (tagNode != NULL) {
      printf("  tag: %s\n",tagNode->tag);
      tagNode = (TAGNODE *)ellNext((ELLNODE*)tagNode);
    }
    sourceTagNode = (SOURCETAGNODE *)ellNext((ELLNODE*)sourceTagNode);
  }
  */
#endif

  Debug("validating sourceId: %s\n", arg1);
  /* Check to see if we already have given sourceId in database. */
  for (i = 0, p = strtok(arg1, "~"); p; p = strtok(NULL, "~\n"), i++) {
    if (i == 0) { /* search for sourceId */
      sourceTagNode = logDaemonFindSourceTagNode(p);
      if (sourceTagNode == NULL) { /* didn't find it, so add new one */
        Debug("new sourceId, adding to database\n", NULL);
        /* First to file */
        if ((sourceId_fp = fopen(DEF_SOURCEIDFILE, "a")) == NULL) {
          syslog(LOG_ERR, "unable to append sourceId file: %m");
          exit(1);
        }
        fputs(sourceIdTagString, sourceId_fp);
        fputs("\n", sourceId_fp);
        fclose(sourceId_fp);

        /* Then to data structure */
        sourceTagNode = (SOURCETAGNODE *)malloc(sizeof(SOURCETAGNODE));
        sourceTagNode->sourceId[0] = '\0';
        strcpy(sourceTagNode->sourceId, p);
        ellInit(&sourceTagNode->tagList);
        ellAdd(&sourceIdList, (ELLNODE *)sourceTagNode);
        while ((p = strtok(NULL, "~\n")) != NULL) {
          tagNode = (TAGNODE *)malloc(sizeof(TAGNODE));
          tagNode->tag[0] = '\0';
          strcpy(tagNode->tag, p);
          ellAdd(&(sourceTagNode->tagList), (ELLNODE *)tagNode);
        }
        return (1);
      }
    } else { /* validate tag list */
      tagNode = (TAGNODE *)ellFirst(&sourceTagNode->tagList);
      j = 1;
      while (tagNode != NULL) {
        if (j == i) {
          if (!strcmp(tagNode->tag, p)) {
            numMatches++;
            break;
          }
        }
        tagNode = (TAGNODE *)ellNext((ELLNODE *)tagNode);
        j++;
      }
      if (tagNode == NULL) {
        Debug("invalid sourceId given\n", NULL);
        return (-1);
      }
    }
  }
  if (numMatches < ellCount(&sourceTagNode->tagList)) {
    Debug("invalid sourceId given\n", NULL);
    return (-1);
  }
  Debug("valid sourceId\n", NULL);
  return (0);
}

/*************************************************************************
 * FUNCTION : logDaemonAddDefaultTypeNode()
 * PURPOSE  : Adds a fully general node to typeList for the given sourceId.
 *            Fully general means this node will match any list of tag values.
 *            If this sourceId has been seen by the logDaemon already, then
 *            go ahead and use the tag name information to open the file.
 * ARGS in  : sourceId - string
 * ARGS out : nothing
 * GLOBAL   : allocates and adds a node to end of typeList
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonAddDefaultTypeNode(char *sourceId) {
  TYPENODE *typeNode;
  char defaultLogFile[STDBUF + 5];
  size_t typeListLength, numBlocks;

  typeNode = (TYPENODE *)malloc(sizeof(TYPENODE));
  ellInit(&(typeNode->tagList));
  typeNode->sourceTagNode = NULL;
  typeNode->log_file_fd = -1;
  typeNode->area = 'l';
  strcpy(typeNode->sourceId, sourceId);

  /* Find associated sourceTagNode, and set ptr to it. */
  typeNode->sourceTagNode = logDaemonFindSourceTagNode(sourceId);
  typeNode->log_mail = (char *)malloc(strlen(sourceId) + 1);
  strcpy(typeNode->log_mail, sourceId);
  /* Set default log file name */
  strcpy(defaultLogFile, sourceId);
  strcat(defaultLogFile, "/");
  strcat(defaultLogFile, MakeDailyGenerationFilename(NULL, 0, NULL, 0));
  strcat(defaultLogFile, ".log");
  typeNode->log_file = (char *)malloc(strlen(defaultLogFile) + 1);
  strcpy(typeNode->log_file, defaultLogFile);

  if (typeNode->area == 'l' && typeNode->sourceTagNode != NULL)
    logDaemonOpenLogfile(typeNode);

  ellAdd(&typeList, (ELLNODE *)typeNode);

  /* Reallocate matchArray if new node on typeList tips us over size. */
  typeListLength = (size_t)ellCount(&typeList);
  if (typeListLength > matchArraySize) {
    numBlocks = typeListLength / CONFIG_BLOCK_SIZE + 1;
    free(matchArray);
    matchArray =
      (TYPENODE **)malloc(numBlocks * sizeof(TYPENODE *) * CONFIG_BLOCK_SIZE);
    matchArraySize = numBlocks * CONFIG_BLOCK_SIZE;
    matchArray[0] = NULL;
  }
}

/*************************************************************************
 * FUNCTION : logDaemonFindSourceTagNode()
 * PURPOSE  : Linear search of sourceIdList, matching on sourceId field.
 * ARGS in  : sourceId - string
 * ARGS out : nothing
 * GLOBAL   : nothing
 * RETURNS  : matching sourceTagNode or NULL
 ************************************************************************/
SOURCETAGNODE *logDaemonFindSourceTagNode(char *sourceId) {
  SOURCETAGNODE *sourceTagNode;
  sourceTagNode = (SOURCETAGNODE *)ellFirst(&sourceIdList);
  while (sourceTagNode != NULL) {
    if (!strcmp(sourceTagNode->sourceId, sourceId))
      break;
    sourceTagNode = (SOURCETAGNODE *)ellNext((ELLNODE *)sourceTagNode);
  }
  return (sourceTagNode);
}

/*************************************************************************
 * FUNCTION : logDaemonUpdateTypeList()
 * PURPOSE  : Searches through typeList for instances of sourceId. If found,
 *            initialize the sourceTagNode field, and open the log file.
 *            If not found, then create a default entry. This function
 *            is used whenever the logDaemon receives a new sourceId in
 *            the logOpen() call.
 * ARGS in  : sourceId - sourceId string
 * ARGS out : nothing
 * GLOBAL   : modifies global typeList
 * RETURNS  : nothing
 ************************************************************************/
void logDaemonUpdateTypeList(char *sourceId) {
  TYPENODE *typeNode;
  int entryFound = 0;

  typeNode = (TYPENODE *)ellFirst(&typeList);
  while (typeNode != NULL) {
    if (!strcmp(typeNode->sourceId, sourceId)) {
      typeNode->sourceTagNode = logDaemonFindSourceTagNode(sourceId);
      if (typeNode->area == 'l' && typeNode->sourceTagNode != NULL)
        logDaemonOpenLogfile(typeNode);
      entryFound = 1;
    }
    typeNode = (TYPENODE *)ellNext((ELLNODE *)typeNode);
  }
  if (!entryFound)
    logDaemonAddDefaultTypeNode(sourceId);
}

/*************************************************************************
 * FUNCTION : logDaemonMainLoop()
 * PURPOSE  : Reads data from a UDP port. The first byte identifies
 *            either a broadcast, or a log message. If the serverId
 *            in the incoming broadcast matches our serverId, and the
 *            given sourceId is new or deemed valid, then ACK back.
 *            If the given sourceId is deemed invalid, NAK back.
 *
 *            If the incoming data is a log message, extract the fields, 
 *            and write to the appropriate log files and/or fork a process 
 *            to send some email.
 * ARGS in  : sockfd - open file descriptor of UDP port
 * ARGS out : nothing
 * GLOBAL   : Writes out to log files and sends mail.
 * RETURNS  : nothing
 ************************************************************************/
int logDaemonMainLoop(int sockfd) {
  int i;
  SOURCETAGNODE *sourceTagNode;
  int nextFreeByte;
  int nextTagValue;
  int buflen;
  char *p;
  char inbuffer[MAX_UDP_SIZE + 1];
  char ackbuffer[STDBUF];
  size_t acklen = 0;
  BSDATA bsData;
  int validCode;
  size_t nextMatch;

  Debug("This servers id is %s\n", log_service_id);
  while (1) {
    startHour = getHourOfDay();
    if ((buflen = BSreadUDP(sockfd, &bsData, -1, inbuffer, MAX_UDP_SIZE)) == -1) {
      syslog(LOG_ERR, "BSreadUDP error: %m");
      exit(1);
    }
    if (getHourOfDay() < startHour)
      logDaemonStartNewLog();
    startHour = getHourOfDay();
    /* Process SIGHUP request now. */
    if (requestReadConfig) {
      requestReadConfig = 0;
      Debug("kill -HUP received, reconfiguring server...\n", NULL);
      logDaemonReadConfig();
      Debug("...server reconfigured.\n", NULL);
    }

    inbuffer[buflen] = '\0'; /* make sure it is terminated */

    Debug("inbuffer: %s\n", inbuffer);
    switch (inbuffer[0]) {

    /*** broadcast for log daemon received ***/
    case '*': {
      ackbuffer[0] = '\0';
      if (!strncmp(inbuffer, log_service_id, strlen(log_service_id))) {
        Debug("Responding to broadcast for %s\n", log_service_id + 1);
        p = inbuffer;
        while (*p != '~')
          p++;
        p++;
        validCode = logDaemonValidateSourceId(p);
        if (getHourOfDay() < startHour)
          logDaemonStartNewLog();
        startHour = getHourOfDay();
        if (validCode == -1) { /* invalid sourceId */
          strcpy(ackbuffer, DEF_BCAST_NAK);
          acklen = strlen(DEF_BCAST_NAK);
        } else if (validCode == 0) { /* valid sourceId */
          strcpy(ackbuffer, DEF_BCAST_ACK);
          acklen = strlen(DEF_BCAST_ACK);
        } else if (validCode == 1) { /* new sourceId */
          p = strtok(p, "~");        /* extract sourceId */
          logDaemonUpdateTypeList(p);
          strcpy(ackbuffer, DEF_BCAST_ACK);
          acklen = strlen(DEF_BCAST_ACK);
        }
        if (BSwriteUDP(sockfd, &bsData, ackbuffer, acklen) != acklen)
          syslog(LOG_ERR, "Response to broadcast for %s failed: %m",
                 log_service_id + 1);
      }
      break;
    }

    /*** log message received ***/
    case '\b': {
      Debug("read %d bytes from port\n", buflen);

      log_ent.field_buffer[0] = '\0';
      nextFreeByte = 0;
      nextTagValue = 0;

      p = inbuffer;
      for (i = 0, p = strtok(++p, "~"); p; i++) {
        switch (i) {
        case 0: /* field zero (message length) */
          Debug("Got length (%s)\n", p);
          if (atoi(p) != buflen) {
            syslog(LOG_ERR, "length field != received buffer size");
            p = (char *)NULL;
            continue;
          }
          break;
        case 1: /* field one (secs) */
          Debug("Got secs (%s)\n", p);
          strcpy(&(log_ent.field_buffer[nextFreeByte]), p);
          log_ent.secs = &(log_ent.field_buffer[nextFreeByte]);
          nextFreeByte += strlen(p) + 1;
          break;
        case 2: /* field two (usecs) */
          Debug("Got usecs (%s)\n", p);
          strcpy(&(log_ent.field_buffer[nextFreeByte]), p);
          log_ent.usecs = &(log_ent.field_buffer[nextFreeByte]);
          nextFreeByte += strlen(p) + 1;
          break;
        case 3: /* field three (sourceId) */
          Debug("Got sourceId (%s)\n", p);
          /* Find entry in sourceIdList, and use it to store tag values. */
          sourceTagNode = logDaemonFindSourceTagNode(p);
          log_ent.sourceTagNode = sourceTagNode;
          if (sourceTagNode == NULL) {
            syslog(LOG_ERR, "fatal error, unknown sourceId received: %s", p);
            exit(1);
          }
          break;
        default:
          Debug("Got tag value (%s)\n", p);
          strcpy(&(log_ent.field_buffer[nextFreeByte]), p);
          log_ent.tagValues[nextTagValue++] =
            &(log_ent.field_buffer[nextFreeByte]);
          nextFreeByte += strlen(p) + 1;
        }
        if (getHourOfDay() < startHour)
          logDaemonStartNewLog();
        startHour = getHourOfDay();
        p = strtok(NULL, "~\n");
      }
      log_ent.numTagValues = nextTagValue;

      /* Load matchArray with all typeNodes that match this log entry */
      logDaemonFindMatchingEntries();

      /* Scan through matchArray, either logging to file, or sending email. */
      nextMatch = 0;
      while (matchArray[nextMatch] != NULL) {
        switch (matchArray[nextMatch]->area) {
        case 'l':
          logDaemonWriteMessage(matchArray[nextMatch]);
          break;
        case 'm':
          logDaemonSendMail(matchArray[nextMatch]);
          break;
        default:
          break;
        }
        nextMatch++;
      }

      acklen = strlen(DEF_LOGMSG_ACK);
      if (BSwriteUDP(sockfd, &bsData, DEF_LOGMSG_ACK, acklen) != acklen) {
        syslog(LOG_ERR, "BSwriteUDP error: %m");
        exit(1);
      }
      break;
    }
    default: {
      syslog(LOG_ERR, "unknown first byte received from client");
    }
    } /* end switch */
  }   /* end while(1) */
}

/*************************************************************************
 * FUNCTION : logDaemonWriteMessage()
 * PURPOSE  : Uses information in typeNode to write out log_ent.
 *            If max log file size is exceeded, the log file is
 *            closed, moved to the save directory with a unique number
 *            suffix, and a fresh log file is opened.
 * ARGS in  : typeNode - node in typeList which matched log_ent
 * ARGS out : none
 * GLOBAL   : File written and/or moved.
 * RETURNS  : 0
 ************************************************************************/
int logDaemonWriteMessage(TYPENODE *typeNode) {
  int outlen, log_entry_len;
  char logbuff[MAX_MESSAGE_SIZE];
  double secs, usecs, Time;
  int nextTagValue = 0;

  Debug("Entering write_message\n", 0);

  /* Create the log file entry */
  secs = atof(log_ent.secs);
  usecs = atof(log_ent.usecs);
  Time = secs + (usecs / 1000000.0);

#ifdef SDDS
  if (mode == TEXT_MODE) {
    sprintf(logbuff, "%f~%s", Time, log_ent.sourceTagNode->sourceId);
    while (nextTagValue < log_ent.numTagValues) {
      strcat(logbuff, "~");
      strcat(logbuff, log_ent.tagValues[nextTagValue++]);
    }
    strcat(logbuff, "\n");
  } else { /* mode == SDDS_MODE */
    sprintf(logbuff, "%f", Time);
    while (nextTagValue < log_ent.numTagValues) {
      strcat(logbuff, " \"");
      strcat(logbuff, log_ent.tagValues[nextTagValue++]);
      strcat(logbuff, "\"");
    }
    strcat(logbuff, "\n");
    /* For SDDS format, we must update the row count */
    typeNode->row_count++;
  }
#else
  sprintf(logbuff, "%lf~%s", Time, log_ent.sourceTagNode->sourceId);
  while (nextTagValue < log_ent.numTagValues) {
    strcat(logbuff, "~");
    strcat(logbuff, log_ent.tagValues[nextTagValue++]);
  }
  strcat(logbuff, "\n");
#endif

  /* Write the log message out to appropriate log file */
  log_entry_len = strlen(logbuff);
#ifdef SDDS
  if ((outlen = fprintf(typeNode->table.layout.fp, "%s", logbuff)) < 0)
    syslog(LOG_ERR, "unable to write entry to log file: %m");
  fflush(typeNode->table.layout.fp);
#else
  if ((outlen = write(typeNode->log_file_fd, logbuff, log_entry_len)) < 0)
    syslog(LOG_ERR, "unable to write entry to log file: %m");
#endif
  typeNode->size += log_entry_len;
  if (typeNode->size >= log_maxsize) {
    Debug("NewGeneration: logfile size %ld\n", typeNode->size);
    Debug("NewGeneration: log_maxsize %ld\n", log_maxsize);
    logDaemonCloseLogfile(typeNode);
    logDaemonNewGeneration(typeNode);
    logDaemonOpenLogfile(typeNode);
  }
  return (0);
}

/*************************************************************************
 * FUNCTION : logDaemonSendMail()
 * PURPOSE  : Sends email to addr(s) specified in typeNode. Email
 *            consists of log message sourceId and tag/value pairs.
 * ARGS in  : typeNode - node in typeList
 * ARGS out : 
 * GLOBAL   : forks a process and execv's the /bin/mail program
 * RETURNS  : 
 ************************************************************************/
int logDaemonSendMail(TYPENODE *typeNode) {
  char mail_mess[MAX_MESSAGE_SIZE];
  int pfds[2];
  char *mailArr[MAX_MAIL_RECIPIENTS];
  int outlen, retval, i;
  char *p;
  TAGNODE *tagNode;
  int nextTagValue = 0;

  Debug("sending mail\n", 0);

  /* Build up body of email message */
  strcpy(mail_mess, "!!Message From logDaemon!!\n\n");
  strcat(mail_mess, "SourceId: ");
  strcat(mail_mess, typeNode->sourceId);

  tagNode = (TAGNODE *)ellFirst(&(typeNode->sourceTagNode->tagList));
  while (tagNode != NULL && nextTagValue < log_ent.numTagValues) {
    strcat(mail_mess, "\n");
    strcat(mail_mess, tagNode->tag);
    strcat(mail_mess, ": ");
    strcat(mail_mess, log_ent.tagValues[nextTagValue++]);
    tagNode = (TAGNODE *)ellNext((ELLNODE *)tagNode);
  }
  strcat(mail_mess, "\n");

  if (pipe(pfds) < 0) {
    syslog(LOG_ERR, "pipe call failed: %m");
    return (-1);
  }

  switch (fork()) {
  case -1:
    syslog(LOG_ERR, "cannot fork(): %m");
    return (-1);
  case 0: /* child */
    close(0);
    dup(pfds[0]);

    /* Split string of one or more space delimited e-mail addresses
       into separate string for execv call */
    mailArr[0] = "/bin/mail";
    for (i = 1, p = strtok(typeNode->log_mail, " "); p && i <= MAX_MAIL_RECIPIENTS;
         p = strtok(NULL, " \n"), i++)
      mailArr[i] = p;
    mailArr[i] = NULL;

    execv("/bin/mail", mailArr);
    syslog(LOG_ERR, "execv failed: %m");
    retval = -1;
    exit(0);
  }

  if ((outlen = write(pfds[1], mail_mess, strlen(mail_mess))) < 0) {
    syslog(LOG_ERR, "unable to write mail: %m");
    retval = -1;
  } else
    retval = 0;

  close(pfds[1]);
  /* used to have wait(0) here */
  return (retval);
}

/*************************************************************************
 * FUNCTION : logDaemonNewGeneration()
 * PURPOSE  : rename current logfile using
 *            the next numerical suffix. 
 * ARGS in  : typeNode - node in typeList (contains file name & fd)
 * ARGS out : none
 * GLOBAL   : rename the log file with new name
 * RETURNS  : 0
 ************************************************************************/
int logDaemonNewGeneration(TYPENODE *typeNode) {
  struct dirent *dirp;
  DIR *dp;
  int max_generation;
  size_t logfilename_len;
  char errbuf[STDBUF];
  char newgeneration[STDBUF];
  char *ptr, *ptr1;

  Debug("file %s reached capacity\n", typeNode->log_file);
  /*
  if ((dp=opendir(log_savedir)) == NULL) {
    syslog(LOG_ERR,"unable to open save directory: %m");
    PERROR(errbuf);
  } */
  if ((dp = opendir(typeNode->sourceId)) == NULL) {
    syslog(LOG_ERR, "unable to open directory: %m");
    PERROR(errbuf);
  }
  max_generation = -1;
  /*get the filename only */
  SDDS_CopyString(&ptr, typeNode->log_file);
  ptr1 = strtok(ptr, "/");
  free(ptr);
  while (ptr1 != NULL) {
    SDDS_CopyString(&ptr, ptr1);
    ptr1 = strtok(NULL, "/");
  }

  logfilename_len = strlen(ptr);
  while ((dirp = readdir(dp)) != NULL) {
    if (strlen(dirp->d_name) > logfilename_len + 1 &&
        !strncmp(ptr, dirp->d_name, logfilename_len) &&
        atoi(&(dirp->d_name[logfilename_len + 1])) > max_generation) {
      max_generation = atoi(&(dirp->d_name[logfilename_len + 1]));
    }
  }
  if (closedir(dp) < 0)
    syslog(LOG_ERR, "unable to close directory: %m");

  snprintf(newgeneration, sizeof(newgeneration), "%s.%05d", ptr, max_generation + 1);

  /* Form destination path and rename file safely */
  {
    size_t dest_len = strlen(typeNode->sourceId) + 1 + strlen(newgeneration) + 1;
    char *dest_path = malloc(dest_len);
    if (dest_path) {
      snprintf(dest_path, dest_len, "%s/%s", typeNode->sourceId, newgeneration);
#ifdef DEBUG
      fprintf(stderr, "Renaming '%s' to '%s'\n", typeNode->log_file, dest_path);
#endif
      if (rename(typeNode->log_file, dest_path) != 0)
        syslog(LOG_ERR, "unable to rename log file %s to %s: %m", typeNode->log_file, dest_path);
      free(dest_path);
    } else {
      syslog(LOG_ERR, "malloc failed for dest_path: %m");
    }
  }
  return (0);
}

void logDaemonExampleConfig() {
  fprintf(stdout, "\n#Example log.config file");
  fprintf(stdout, "\n#-----------------------");
  fprintf(stdout, "\n# Fields are ~ separated, and as follows:");
  fprintf(stdout, "\n# sourceId~dest~destName~<tag1>=<val1>~<tagn>=<valn>");
  fprintf(stdout, "\n# where sourceId is string");
  fprintf(stdout, "\n#       dest is the string log or mail");
  fprintf(stdout, "\n#       destName is a log file name if dest is log,");
  fprintf(stdout, "\n#         or space delimited list of email addrs if");
  fprintf(stdout, "\n#         dest is mail.");
  fprintf(stdout, "\n#       <tag1>=<val1> specifies that tag value supplied");
  fprintf(stdout, "\n#         in message must match <val1> exactly. Any");
  fprintf(stdout, "\n#         tag names not given will match anything.");
  fprintf(stdout, "\n# Specify three log files for msgs from IOC sourceId");
  fprintf(stdout, "\nIOC~log~iocLogMinor.log~severity=Minor");
  fprintf(stdout, "\nIOC~log~iocLogMajor.log~severity=Major");
  fprintf(stdout, "\nIOC~log~iocLogOther.log");
  fprintf(stdout, "\n# Next, specify email for msg from any sourceId DOOM");
  fprintf(stdout, "\nDOOM~mail~me@aps.anl.gov you@aps.anl.gov");
}

#ifdef SDDS
/*************************************************************************
 * FUNCTION : logDaemonGetSDDSHeaderOffset()
 * PURPOSE  : Assumes you have just opened a new SDDS log file and
 *            written the header out. This function just finds the
 *            byte offset into the file.
 * ARGS in  : table - open SDDS_TABLE struct
 * ARGS out : h_offset - byte offset in file just after header
 * GLOBAL   : 
 * RETURNS  : 0
 ************************************************************************/
int logDaemonGetSDDSHeaderOffset(SDDS_TABLE *table, off_t *h_offset) {
  int fd = fileno(table->layout.fp);
  off_t header_offset;

  if ((header_offset = lseek(fd, 0, SEEK_CUR)) == -1) {
    syslog(LOG_ERR, "unable to lseek logfile header offset: %m");
    exit(1);
  }
  *h_offset = header_offset;
  return (0);
}

/*************************************************************************
 * FUNCTION : logDaemonWriteSDDSHeader()
 * PURPOSE  : Opens an SDDS file with name given in typeNode. A fixed
 *            parameter "sourceId" is defined. Then, one column for
 *            each tag name is defined.
 * ARGS in  : table - ptr to pre-allocated SDDS_TABLE struct
 *            typeNode - node in typeList (has file name and tagList)
 * ARGS out : none
 * GLOBAL   : Creates, opens, and writes SDDS file.
 * RETURNS  : 0
 ************************************************************************/
int logDaemonWriteSDDSHeader(SDDS_TABLE *table, TYPENODE *typeNode) {
  TAGNODE *tagNode;

  if (!SDDS_InitializeOutput(table, SDDS_ASCII, 1, "logDaemon", "logDaemon",
                             typeNode->log_file)) {
    syslog(LOG_ERR, "unable to open SDDS log file %s: %m",
           typeNode->log_file);
    /*
    SDDS_PrintErrors(stderr,(SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
    */
  }
  if (SDDS_DefineParameter(table, "sourceId", NULL, NULL, NULL, NULL,
                           SDDS_STRING, typeNode->sourceId) < 0) {
    syslog(LOG_ERR, "unable to define parameter %s: %m", "sourceId");
    /*
    SDDS_PrintErrors(stderr, (SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
    */
  }
  if (SDDS_DefineColumn(table, "Time", NULL, "s", NULL, NULL, SDDS_DOUBLE, 0) == -1) {
    syslog(LOG_ERR, "unable to define column %s: %m", "Time");
    /*
    SDDS_PrintErrors(stderr,(SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
    */
  }

  /* For each tag, define an SDDS column. */
  tagNode = (TAGNODE *)ellFirst(&(typeNode->sourceTagNode->tagList));
  while (tagNode != NULL) {
    if (SDDS_DefineColumn(table, tagNode->tag, NULL, NULL, NULL, NULL,
                          SDDS_STRING, 0) == -1) {
      syslog(LOG_ERR, "unable to define column %s: %m", tagNode->tag);
      /*
      SDDS_PrintErrors(stderr,(SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
      */
    }
    tagNode = (TAGNODE *)ellNext((ELLNODE *)tagNode);
  }

  if (!SDDS_SetNoRowCounts(table, 1)) {
    syslog(LOG_ERR, "unable to set no_row_counts");
    /*
    SDDS_PrintErrors(stderr,(SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
    */
  }
  if (!SDDS_WriteLayout(table)) {
    syslog(LOG_ERR, "unable to write SDDS log file header: %m");
    /*
    SDDS_PrintErrors(stderr,(SDDS_EXIT_PrintErrors|SDDS_VERBOSE_PrintErrors));
    */
  }
  return (0);
}

/*************************************************************************
 * FUNCTION : logDaemonAppendPageToLogFile()
 * PURPOSE  : open logFile in append to page mode (SDDS format)
 ************************************************************************/
void logDaemonAppendPageToLogFile(TYPENODE *typeNode) {
  int64_t initialRow = 0;

  if (!SDDS_InitializeAppendToPage(&(typeNode->table), typeNode->log_file, 1,
                                   &initialRow)) {
    syslog(LOG_ERR, "cannot append to log file %s: %m", typeNode->log_file);
  }
  typeNode->log_file_fd = fileno(typeNode->table.layout.fp);
  logDaemonGetSDDSHeaderOffset(&(typeNode->table), &(typeNode->size));
  typeNode->row_count = initialRow;
}

#endif
