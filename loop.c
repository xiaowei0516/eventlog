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

/* Include files */
#include <stdio.h>
#include "main.h"
#include "check.h"
#include "eventlog.h"
#include "log.h"
#include "service.h"
#include "syslog.h"
#include "ver.h"

int ascii2utf8(char * message, int level);

/* Main eventlog monitoring loop */
int MainLoop()
{
	char * output = NULL;
	HKEY hkey = NULL;
	int level;
	int log;
	int stat_counter = 0;
	FILE *fp = NULL;

    
 
    /* Gather eventlog names */
    if (RegistryGather())
		return 1;
     /* Open all eventlogs */
    if (EventlogsOpen())
		return 1;
    

	/* Loop while service is running */
	while (ServiceIsRunning)
      {
		/* Process records */
		for (log = 0; log < EventlogCount; log++) 
		{
                    while ((output = EventlogNext( log, &level))) 
			{
                         if (output != NULL)
				{
					if (ascii2utf8(output, level))
					{
						ServiceIsRunning = FALSE;
						break;
					}
                          }			
                    }
		 }


		/*打开文件*/
		/* Sleep five seconds */
		Sleep(5000);
	}

	/* Service is stopped */
	Log(LOG_INFO, "Eventlog to Syslog Service Stopped");
      EventlogsClose();
	/* Success */
	return 0;
}


// Send a message to the syslog server //
int ascii2utf8(char * message, int level)
{
	char error_message[SYSLOG_DEF_SZ];   /*1024*/
	WCHAR utf16_message[SYSLOG_DEF_SZ];  /*1024*/
	char utf8_message[SYSLOG_DEF_SZ];        /*1024*/

	// Write priority level //
	_snprintf_s(error_message, sizeof(error_message), _TRUNCATE,
		"<%d>%s",
		level,
		message
	);

	// convert from ansi/cp850 codepage (local system codepage) to utf8, widely used codepage on unix systems //
	MultiByteToWideChar(CP_ACP, 0, error_message, -1, utf16_message, SYSLOG_DEF_SZ);
	WideCharToMultiByte(CP_UTF8, 0, utf16_message, -1, utf8_message, SYSLOG_DEF_SZ, NULL, NULL);


	//print -- utf8_message

	// Send result to syslog server //
	return 0;
}

