#pragma once
#ifndef _CONFIG_H
#define _CONFIG_H

#define DAEMON_VERSION_STRING "1.1"

#define MAX_DOCKS 8
#define MAX_CHANNELS 8

#define BAUD_RATE 115200

#define PID 0x08C3 //9220
#define VID 0x16D0 //1003
#define FLOTILLA_PORT 9395

#define PID_FILE_PATH "/var/run/flotilla.pid"
#define LOG_FILE_PATH "/var/log/flotilla.log"

#define NOTIFY_REQUEST_HOST "discover.flotil.la"
#define NOTIFY_REQUEST_PORT "80"
#define NOTIFY_INTERVAL 3600  // 3600 = 1hr
#define NOTIFY_ENABLE

//#define _DISCOVERY_DEBUG
//#define DEBUG_SCAN_FOR_HOST

#ifndef TRUE
#define TRUE  (1==1)
#endif
#ifndef FALSE
#define FALSE (1==0)
#endif

#endif