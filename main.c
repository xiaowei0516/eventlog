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
#include "eventlog.h"
#include "log.h"
#include "syslog.h"
#include "getopt.h"
#include "ver.h"
#include "check.h"
#include "winevent.h"
#include "wsock.h"

// Main program //

// Program variables //
static BOOL ProgramDebug = FALSE;
static BOOL ProgramInstall = FALSE;
static BOOL ProgramUninstall = FALSE;
static BOOL ProgramIncludeOnly = FALSE;
static char * ProgramName;
static char * ProgramSyslogFacility = NULL;
static char * ProgramSyslogLogHosts = NULL;
static char * ProgramSyslogPort = NULL;
static char * ProgramSyslogQueryDhcp = NULL;
static char * ProgramSyslogInterval = NULL;
static char * ProgramSyslogTag = NULL;
static char * ProgramLogLevel = NULL;

BOOL ProgramUseIPAddress = FALSE;
char ProgramHostName[256];
char ProgramExePath[MAX_PATH];



// Main program //
int main(int argc, char ** argv)
{
	int status;

    // Get and store executable path //  获取当前进程已加载模块的文件的完整路径
    //该模块必须由当前进程加载。
    if (!GetModuleFileName(NULL, ProgramExePath, sizeof(ProgramExePath))) {
        Log(LOG_ERROR, "Unable to get path to my executable");
        return 1;
    }
	 //获取可执行程序evtsys的完整路径
	printf("program:%s\n",ProgramExePath);  
	
	status = MainLoop();
	// Success //
	return status;
}

