#include <stdio.h>
#include "main.h"
#include "eventlog.h"



int ascii2utf8(char * message, int level);

/* Main eventlog monitoring loop */
int MainLoop()
{
	char * output = NULL;
	int level;
	int log;
 
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
		Sleep(5000);
	}

	/* Service is stopped */
	printf( "Eventlog to Syslog Service Stopped");
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
	printf("---->%s\n", utf8_message);


	//print -- utf8_message

	// Send result to syslog server //
	return 0;
}

