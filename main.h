/*
  This code is a modification of the original Eventlog to Syslog Script written by
  Curtis Smith of Purdue University. The original copyright notice can be found below.
  
  The original program was modified by Sherwin Faria for Rochester Institute of Technology
  in July 2009 to provide bug fixes and add several new features. Additions include
  the ability to ignore specific events, add the event timestamp to outgoing messages,
  a service status file, and compatibility with the new Vista/2k8 Windows Events service.

     Sherwin Faria
	 Rochester Institute of Technology
	 Information & Technology Services Bldg. 10
	 1 Lomb Memorial Drive
	 Rochester, NY 14623 U.S.A.
	 
	Send all comments, suggestions, or bug reports to:
		sherwin.faria@gmail.com
*/
 
/*
  Copyright (c) 1998-2007, Purdue University
  All rights reserved.

  Redistribution and use in source and binary forms are permitted provided
  that:

  (1) source distributions retain this entire copyright notice and comment,
      and
  (2) distributions including binaries display the following acknowledgement:

         "This product includes software developed by Purdue University."

      in the documentation or other materials provided with the distribution
      and in all advertising materials mentioning features or use of this
      software.

  The name of the University may not be used to endorse or promote products
  derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.

  This software was developed by:
     Curtis Smith

     Purdue University
     Engineering Computer Network
     465 Northwestern Avenue
     West Lafayette, Indiana 47907-2035 U.S.A.

  Send all comments, suggestions, or bug reports to:
     software@ecn.purdue.edu

*/

/* Basic include files */
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <dhcpcsdk.h>
#include <lm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

/* Macros */
#define COUNT_OF(x)	(sizeof(x)/sizeof(*x))

/* Constants */
#define ERRMSG_SZ	256
#define MAX_IGNORED_EVENTS	256
#define HOSTNAME_SZ 64

#define QUERY_SZ	1024
#define QUERY_LIST_SZ  524288
#define ERR_FAIL	-1
#define ERR_CONTINUE 3
#define SOURCE_SZ	128
#define PLUGIN_SZ	32

/* Compatibility */
#define in_addr_t	unsigned long

typedef struct EVENT_LIST EventList;


extern BOOL ProgramUseIPAddress;
extern char ProgramHostName[256];
extern char ProgramExePath[MAX_PATH];

/* Prototypes */
int     CheckForWindowsEvents();
char*   CollapseExpandMessage(char * message);
int     EventlogCreate(char * name);
char*   EventlogNext( int log, int * level);
void    EventlogsClose(void);
int     EventlogsOpen(void);
char*   FormatLibraryMessage(char * message_file, DWORD event_id, char ** string_array);
void    GetError(DWORD err_num, char * message, int len);
void    GetErrorW(DWORD err_num, WCHAR * message, int len);
char*   GetTimeStamp(void);
char*   GetUsername(SID * sid);
char*   GetWinEvent(char * log, int recNum, int event_id);
int     LogStart(void);
void    LogStop(void);
void    Log(int level, char * message, ...);
char*   LookupMessageFile(char * logtype, char * source, DWORD eventID);
int     MainLoop(void);
int     RegistryGather(void);
BOOL    WINAPI ShutdownConsole(DWORD dwCtrlType);
int		EndsWith(const LPWSTR str, const LPWSTR suffix);
int		StartsWith(const LPWSTR str, const LPWSTR suffix);