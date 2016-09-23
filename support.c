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
#include "main.h"
#include "check.h"
#include "syslog.h"
#include <string.h>

/* Number of libaries to load */
#define LIBRARY_SZ		4

/* Length of a file path */
#define PATH_SZ			1024

/* Length of a SID */
#define SID_SZ			256

/* Length of a registry key */
#define KEY_SZ			256

/* Length of a message format */
#define FORMAT_SZ		65535



/* Get error message */
void GetError(DWORD err_num, char * message, int len)
{
	/* Attempt to get the message text */
	if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err_num, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), message, len, NULL) == 0)
		_snprintf_s(message, len, _TRUNCATE,
			"(Error %u)",
			err_num
		);
}


/* Create Timestamp from current local time */
char * GetTimeStamp()
{
	time_t t = time(NULL);
	struct tm stm;
	char result[16];
	static char timestamp[] = "Mmm dd hh:mm:ss";

	/* Format timestamp string */ 
	if (localtime_s(&stm, &t) == 0) {
		strftime(result, sizeof(result), "%b %d %H:%M:%S", &stm); 
		if (result[4] == '0') /* Replace leading zero with a space on */ 
			result[4] = ' ';  /* single digit days so we comply with the RFC */ 
	} else 
		result[0] = '\0'; 
	strncpy_s(timestamp, sizeof(timestamp), result, _TRUNCATE);

	return timestamp;
}

/* Get user name string for a SID */
char * GetUsername(SID * sid)
{
	DWORD domain_len;
	DWORD name_len;
	HRESULT last_err;
	SID_NAME_USE snu;
	char domain[DNLEN+1];
	char id[32];
	char name[UNLEN+1];
	int c;

	static char result[SID_SZ];

	/* Initialize lengths for return buffers */
	name_len = sizeof(name);
	domain_len = sizeof(domain);

	/* Convert SID to name and domain */
	if (LookupAccountSid(NULL, sid, name, &name_len, domain, &domain_len, &snu) == 0) {

		/* Check last error */
		last_err = GetLastError();

		/* Could not convert - make printable version of numbers */
		_snprintf_s(result, sizeof(result), _TRUNCATE,
			"S-%u",
			sid->Revision
		);
		for (c = 0; c < COUNT_OF(sid->IdentifierAuthority.Value); c++)
			if (sid->IdentifierAuthority.Value[c]) {
				_snprintf_s(id, sizeof(id), _TRUNCATE,
					"-%u",
					sid->IdentifierAuthority.Value[c]
				);
				strncat_s(result, sizeof(result), id, _TRUNCATE);
			}
		for (c = 0; c < sid->SubAuthorityCount; c++) {
			_snprintf_s(id, sizeof(id), _TRUNCATE,
				"-%u",
				sid->SubAuthority[c]
			);
			strncat_s(result, sizeof(result), id, _TRUNCATE);
		}

		/* Show error (skipping RPC errors) */
		if (last_err != RPC_S_SERVER_TOO_BUSY)
			printf("Cannot find SID for \"%s\"", result);
	} else
		_snprintf_s(result, sizeof(result), _TRUNCATE,
			"%s\\%s",
			domain,
			name
		);

	/* Result result */
	return result;
}



/* Look up message file key */
char * LookupMessageFile(char * logtype, char * source, DWORD eventID)
{
	HKEY hkey;
	DWORD key_size;
	DWORD key_type;
	LONG status;
	char key[KEY_SZ];
	char key_value[KEY_SZ];

	static char result[PATH_SZ];

	/* Get key to registry */
	_snprintf_s(key, sizeof(key), _TRUNCATE,
		"SYSTEM\\CurrentControlSet\\Services\\Eventlog\\%s\\%s",
		logtype,
		source
	);
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ, &hkey) != ERROR_SUCCESS) {
		printf("Cannot find message file key for \"%s\"", key);
		return NULL;
	}

	/* Get message file from registry */
	key_size = sizeof(key_value);
	status = RegQueryValueEx(hkey, "EventMessageFile", NULL, &key_type, key_value, &key_size);
	RegCloseKey(hkey);
	if (status != ERROR_SUCCESS) {
		printf("Cannot find key value \"EventMessageFile\": \"%s\"", key);
		return NULL;
	}

	/* Expand any environmental strings */
	if (key_type == REG_EXPAND_SZ) {
		if (ExpandEnvironmentStrings(key_value, result, sizeof(result)) == 0) {
			printf("Cannot expand string: \"%s\"", key_value);
			return NULL;
		}
	} else
		strncpy_s(result, sizeof(result), key_value, _TRUNCATE);
	key_value[0] = '\0';

	return result;
}

/* Message file storage */
struct MessageFile {
	char path[PATH_SZ];		/* Path to message file		*/
	HINSTANCE library;		/* Handle to library		*/
};

/* List of message files */
static struct MessageFile MessageFile[LIBRARY_SZ];
static int MessageFileCount;

/* Split message file string to individual file paths */
static void SplitMessageFiles(char * message_file)
{
	DWORD len;
	char * ip;
	char * op;

	/* Process each file, seperated by semicolons */
	MessageFileCount = 0;
	ip = message_file;
	do {

		/* Check library count */
		if (MessageFileCount == COUNT_OF(MessageFile)) {
			printf("Message file split into too many paths: %d paths", MessageFileCount);
			break;
		}

		/* Seperate paths */
		op = strchr(ip, ';');
		if (op)
			*op = '\0';

		/* Expand environment strings in path */
		len = ExpandEnvironmentStrings(ip, MessageFile[MessageFileCount].path, sizeof(MessageFile[MessageFileCount].path)-1);
		if (op)
			*op++ = ';';
		if (len == 0) {
			printf( "Cannot expand string: \"%s\"", ip);
			break;
		}
		MessageFileCount++;

		/* Go to next library */
		ip = op;
	} while (ip);
}

/* Load message file libraries */
static void LoadMessageFiles()
{
	HRESULT last_err;
	int i;

	/* Process each file */
	for (i = 0; i < MessageFileCount; i++) {

		/* Load library */
		MessageFile[i].library = LoadLibraryEx(MessageFile[i].path, NULL, LOAD_LIBRARY_AS_DATAFILE);
		if (MessageFile[i].library == NULL) {

			/* Check last error */
			last_err = GetLastError();

			/* Something other than a message file missing problem? */
			if (HRESULT_CODE(last_err) != ERROR_FILE_NOT_FOUND)
				printf( "Cannot load message file: \"%s\"", MessageFile[i].path);
		}
	}
}

/* Unload message file libraries */
static void UnloadMessageFiles()
{
	int i;

	/* Close libraries */
	for (i = 0; i < MessageFileCount; i++)
		if (MessageFile[i].library)
			FreeLibrary(MessageFile[i].library);
}

/* Format message */
static char * FormatMessageFiles(DWORD event_id, char ** string_array)
{
	DWORD last_err;
	HRESULT status;
	int i;

	static char result[FORMAT_SZ];

	/* Try each library until it works */
	for (i = 0; i < MessageFileCount; i++) {
		/* Format message */
		status = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE|FORMAT_MESSAGE_ARGUMENT_ARRAY, MessageFile[i].library, event_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), result, sizeof(result), (va_list*)string_array);
		result[FORMAT_SZ-1] = '\0';

		if (status)
			return result;
	}

	/* Check last error */
	last_err = GetLastError();

	/* Something other than a message file missing problem? */
	if (last_err != ERROR_MR_MID_NOT_FOUND) {
		printf( "Cannot format event ID %u", event_id);
	}

	/* Cannot format */
	return NULL;
}

/* Collapse white space and expand error numbers */
char * CollapseExpandMessage(char * message)
{
	BOOL skip_space = FALSE;
	DWORD errnum;
	char * ip;
	char * np;
	char * op;
	char ch;
	char errstr[ERRMSG_SZ];

	static char result[SYSLOG_DEF_SZ];

	/* Initialize */
	ip = message;
	op = result;
	np = NULL;

	/* Loop until buffer is full */
	while (op - result < sizeof(result)-1) {

		/* Are we at end of input */
		ch = *ip;
		if (ch == '\0') {

			/* Go to next input */
			ip = np;
			np = NULL;

			/* Is this end of input */
			if (ip == NULL)
				break;

			/* Start again */
			continue;
		} else {
			ip++;
		}

		/* Convert whitespace to "space" character */
		if (ch == '\r' || ch == '\t' || ch == '\n')
			ch = ' ';

		/* Is this a "space" character? */
		if (ch == ' ') {

			/* Skip if we're skipping spaces */
			if (skip_space)
				continue;

			/* Start skipping */
			skip_space = TRUE;
		} else {
			/* Stop skipping */
			skip_space = FALSE;
		}

		/* Is this a percent escape? */
		if (ch == '%') {

			/* Is the next character another percent? */
			if (*ip == '%') {

				/* Advance over percent */
				ip++;

				/* Convert input to error number */
				errnum = strtol(ip, &ip, 10);

				/* Format message */
				GetError(errnum, errstr, sizeof(errstr));

				/* Push error message as input */
				np = ip;
				ip = errstr;
				continue;
			}
		}

		/* Add character to output */
		*op++ = ch;
	}

	/* Terminate string */
	*op = '\0';

	/* Return result */
	return result;
}




/* Format library message */
char * FormatLibraryMessage(char * message_file, DWORD event_id, char ** string_array)
{
	static char * message;

	/* Split message file */
	SplitMessageFiles(message_file);

	/* Load files */
	LoadMessageFiles();

	/* Format message */
	message = FormatMessageFiles(event_id, string_array);

	/* Collapse white space */
	if (message)
		message = CollapseExpandMessage(message);

	/* Unload files */
	UnloadMessageFiles();

	/* Return result */
	return message;
}

