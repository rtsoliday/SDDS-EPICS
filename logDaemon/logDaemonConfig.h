
#define LOGFILE_PERM 0644           /* logfile permissions */
#define USER_RESERVED_UDP_PORT 5000 /* user port range above 5K  */
#define STDBUF 1024                 /* standard char buffer size */
#define MAX_MAIL_RECIPIENTS 250     /* max # mail addrs in config file */
#define CONFIG_BLOCK_SIZE 1000      /* Memory for storing list of \
         log-message/config-entry matches is                      \
         allocated in chunks this big. This                       \
         denotes storage for 1000 pointers. */

/* Size limits on log messages. (Message Header + Message) < UDP limit */
#define MAX_UDP_SIZE 2000 /* 2048 minus space for IP header */
#define MAX_HEADER_SIZE 5 /* unique byte plus 4 char length */
#define MAX_MESSAGE_SIZE MAX_UDP_SIZE - MAX_HEADER_SIZE
#define FIELD_DELIMITER "~" /* unique char for field separation */

#ifndef DEF_CONFIGFILE
#  define DEF_CONFIGFILE "log.config" /* default config file */
#endif
#ifndef DEF_SOURCEIDFILE
#  define DEF_SOURCEIDFILE "log.sourceId" /* default sourceId file */
#endif
#ifndef DEF_LOGSERVER_ID
#  define DEF_LOGSERVER_ID "STDLOG"
#endif
#ifndef DEF_SOURCE_ID
#  define DEF_SOURCE_ID "LOGMSG"
#endif
#ifndef DEF_BCAST_ACK
#  define DEF_BCAST_ACK "*ACK"
#endif
#ifndef DEF_BCAST_NAK
#  define DEF_BCAST_NAK "*NAK"
#endif
#ifndef DEF_LOGMSG_ACK
#  define DEF_LOGMSG_ACK "\bACK"
#endif
#ifndef DEF_LOGPORT
#  define DEF_LOGPORT 5332 /* default logdaemon udp port */
#endif
#ifndef DEF_HOME
#  define DEF_HOME "."
#endif
#ifndef DEF_SAVEDIR
#  define DEF_SAVEDIR "./save"
#endif
#ifndef DEF_LOGSIZE
#  define DEF_LOGSIZE 20000000
#endif

/***************************************************/
/** Environment Variable names may be changed here */
/***************************************************/
#ifndef LOGGER_ID
#  define LOGGER_ID "LOG_SERVER_ID"
#endif
#ifndef LOGGER_PORT
#  define LOGGER_PORT "LOG_PORT"
#endif
#ifndef LOGGER_HOST
#  define LOGGER_HOST "LOG_HOST"
#endif
#ifndef LOGGER_CONFIG
#  define LOGGER_CONFIG "LOG_CONFIG"
#endif
#ifndef LOGGER_HOME
#  define LOGGER_HOME "LOG_HOME"
#endif
#ifndef LOGGER_SAVEDIR
#  define LOGGER_SAVEDIR "LOG_SAVEDIR"
#endif
#ifndef LOGGER_MAXSIZE
#  define LOGGER_MAXSIZE "LOG_MAXSIZE"
#endif
/***************************************************/
