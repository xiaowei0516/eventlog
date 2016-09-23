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

// Include files //
#include "main.h"
#include "log.h"
#include "syslog.h"
#include "check.h"

int IGNORED_LINES = 0;
int XPATH_COUNT = 0;

// Facility conversion table //
static struct {
	char * name;
	int id;
} FacilityTable[] = {
	{ "auth", SYSLOG_AUTH },
	{ "authpriv", SYSLOG_AUTHPRIV },
	{ "cron", SYSLOG_CRON },
	{ "daemon", SYSLOG_DAEMON },
	{ "ftp", SYSLOG_FTP },
	{ "kern", SYSLOG_KERN },
	{ "local0", SYSLOG_LOCAL0 },
	{ "local1", SYSLOG_LOCAL1 },
	{ "local2", SYSLOG_LOCAL2 },
	{ "local3", SYSLOG_LOCAL3 },
	{ "local4", SYSLOG_LOCAL4 },
	{ "local5", SYSLOG_LOCAL5 },
	{ "local6", SYSLOG_LOCAL6 },
	{ "local7", SYSLOG_LOCAL7 },
	{ "lpr", SYSLOG_LPR },
	{ "mail", SYSLOG_MAIL },
	{ "news", SYSLOG_NEWS },
	{ "ntp", SYSLOG_NTP },
	{ "security", SYSLOG_SECURITY },
	{ "user", SYSLOG_USER },
	{ "uucp", SYSLOG_UUCP }
};






XPathList* CreateXPath(char * plugin, char * source, char * query) {
    XPathList* newXPath = (XPathList*)malloc(sizeof(XPathList));
    if (newXPath != NULL){
		newXPath->plugin = plugin;
        newXPath->source = source;
        newXPath->query = query;
        newXPath->next = NULL;
    }
    
    if (LogInteractive)
        Log(LOG_INFO, "evtsys Creating XPATH:  %s | %s", source, query);

    return newXPath;
}

XPathList* AddXPath(XPathList* xpathList, char * plugin, char * source, char * query) {
    XPathList* newXPath = CreateXPath(plugin, source, query);
    if (newXPath != NULL) {
        newXPath->next = xpathList;
    }

    return newXPath;
}

void DeleteXPath(XPathList* oldXPath) {
    if (oldXPath->next != NULL) {
        DeleteXPath(oldXPath->next);
    }

    if (LogInteractive)
        Log(LOG_INFO, "Deleting %s", oldXPath->source);
    
    if (oldXPath->source != NULL)
        free(oldXPath->source);

    if (oldXPath->query != NULL)
		free(oldXPath->query);

	if (oldXPath->plugin != NULL)
		free(oldXPath->plugin);

    free(oldXPath);
}




// Check for IncludeOnly flag //
int CheckSyslogIncludeOnly()
{
	// Store new value //
	SyslogIncludeOnly = TRUE;

	// Success //
	return 0;
}

int CheckSyslogTag(char * arg)
{
	if(strlen(arg) > sizeof(SyslogTag)-1) {
		Log(LOG_ERROR, "Syslog tag too long: \"%s\"", arg);
		return 1;
	}
	
	SyslogIncludeTag = TRUE;
	strncpy_s(SyslogTag, sizeof(SyslogTag), arg, _TRUNCATE);

	return 0;
}

// Check for new Crimson Log Service //
int CheckForWindowsEvents()
{
	HKEY hkey = NULL;
    BOOL winEvents = FALSE;

	// Check if the new Windows Events Service is in use //
	// If so we will use the new API's to sift through events // 打开一个指定的键
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\WINEVT\\Channels\\ForwardedEvents", 0, KEY_READ, &hkey) != ERROR_SUCCESS)
		winEvents = FALSE;
	else
		winEvents = TRUE;
		
	if (hkey)
		RegCloseKey(hkey);

	// A level of 1 (Critical) is not valid in this process prior //
	// to the new Windows Events. Set level to 2 (Error) //
	if (winEvents == FALSE && SyslogLogLevel == 1)
		SyslogLogLevel = 2;

    return winEvents;
}