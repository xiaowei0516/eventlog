#include "main.h"
#include "eventlog.h"
#include "log.h"

#include "syslog.h"
#include "check.h"

/* Number of eventlogs */
#define EVENTLOG_SZ		32
/* Size of buffer */
#define EVENTLOG_BUF_SZ	(10*1024)
/* Number of strings in formatted message */
#define EVENTLOG_ARRAY_SZ	96

/* Eventlog descriptor */
struct Eventlog {
	char name[EVENTLOG_NAME_SZ];	/* Name of eventlog   128*/
	HANDLE handle;					/* Handle to eventlog	*/
	char buffer[EVENTLOG_BUF_SZ];	/* Message buffer	 10K	*/
	int count;			/* Number of messages left	*/
	int pos;			/* Current position in buffer	*/
	int recnum;			/* Next record number		*/
};

/* List of eventlogs */
static struct Eventlog EventlogList[EVENTLOG_SZ];   /*32*/
int EventlogCount = 0;



/* Create new eventlog descriptor
    创建新的描述符
    统计eventlog
*/
int EventlogCreate(char * name)
{
      printf("count:%d,  name--->%s\n", EventlogCount, name);
	if (EventlogCount == EVENTLOG_SZ) {  
		Log(LOG_ERROR, "Too many eventlogs: %d", EVENTLOG_SZ);
		return 1;
	}
	/* Store new name */
	strncpy_s(EventlogList[EventlogCount].name, sizeof(EventlogList[EventlogCount].name), name, _TRUNCATE);

	/* Increment count */
	EventlogCount++;
	/* Success */
	return 0;
}


/* Close eventlog */
static void EventlogClose(int log)
{
	/* Close log */
	CloseEventLog(EventlogList[log].handle);
	EventlogList[log].handle = NULL;
}

/* Close eventlogs */
void EventlogsClose()
{
	int i;

	/* Loop until list depleted */
	for (i = 0; i < EventlogCount; i++)
		if (EventlogList[i].handle)
			EventlogClose(i);

	/* Reset count */
	EventlogCount = 0;
}

/* Open event log */
static int EventlogOpen(int log)
{
	DWORD count;
	DWORD oldest;

	/* Reset all indicators */
	EventlogList[log].count = 0;   /*剩余message 数量*/
	EventlogList[log].pos = 0;       /*当前在buffer 中的位置*/
	EventlogList[log].recnum = 1;  /*下一次记录的数量*/

      printf("EventlogList[%d]:--> %s\n", log, EventlogList[log].name);
	/* Open log 
          Windows 日志：
          应用程序 对应于OpenEventLog（NULL,"Application"）
          安全 对应于OpenEventLog（NULL,"Security"）
          setup
          系统 对应于OpenEventLog（NULL,"System"）

	*/
	EventlogList[log].handle = OpenEventLog(NULL, EventlogList[log].name);
	if (EventlogList[log].handle == NULL) {
		Log(LOG_ERROR|LOG_SYS, "Cannot open event log: \"%s\"", EventlogList[log].name);
		return 1;
	}

	/* Get number of records to skip */
	if (GetNumberOfEventLogRecords(EventlogList[log].handle, &count) == 0) {
		Log(LOG_ERROR|LOG_SYS, "Cannot get record count for event log: \"%s\"", EventlogList[log].name);
		return 1;
	}

	/* Get oldest record number */
	if (GetOldestEventLogRecord(EventlogList[log].handle, &oldest) == 0 && count != 0) {
		Log(LOG_ERROR|LOG_SYS, "Cannot get oldest record number for event log: \"%s\"", EventlogList[log].name);
		return 1;
	}

	/* Store record of next event */
	EventlogList[log].recnum = oldest + count;
	if (EventlogList[log].recnum == 0)
		EventlogList[log].recnum = 1; /* ?? */

	/* Success */
	return 0;
}

/* Open event logs */
int EventlogsOpen()
{
	int i;

	/* Open the log files */
	for (i = 0; i < EventlogCount; i++)
		if (EventlogOpen(i))
			break;

	/* Check for errors */
	if (i != EventlogCount) {
		EventlogsClose();
		return 1;
	}

	/* Success */
	return 0;
}

/* Get the next eventlog message */
char * EventlogNext(int log, int * level)
{
	BOOL reopen = FALSE;
	DWORD errnum;
	DWORD needed;
	DWORD loglevel;
	EVENTLOGRECORD * event;
	char * cp;
	char * current;
	char * formatted_string;
	char * message_file;
	char * source;
	char * string_array[EVENTLOG_ARRAY_SZ];
	char * username;
	char hostname[HOSTNAME_SZ];
	char defmsg[ERRMSG_SZ];
	int event_id;
	int i;
	char *index;

	static char message[SYSLOG_DEF_SZ-17];
	static char tstamped_message[SYSLOG_DEF_SZ];

	/* Initialize array to prevent memory exceptions with bad message definitions */
	for(i = 0; i < EVENTLOG_ARRAY_SZ; i++)
		string_array[i]="*";

	/* Are there any records left in buffer */
	while (EventlogList[log].pos == EventlogList[log].count) {

		/* Reset input position */
		EventlogList[log].count = 0;
		EventlogList[log].pos = 0;

		/* Read a record */
		needed = 0;
		if (ReadEventLog(EventlogList[log].handle, EVENTLOG_FORWARDS_READ | EVENTLOG_SEEK_READ, EventlogList[log].recnum, EventlogList[log].buffer, sizeof(EventlogList[log].buffer), &EventlogList[log].count, &needed) == 0) {

			/* Check error */
			errnum = GetLastError();
			switch (errnum) {

			/* Message too large... skip over */
			case ERROR_INSUFFICIENT_BUFFER:
				Log(LOG_WARNING, "Eventlog message size too large: \"%s\": %u bytes", EventlogList[log].name, needed);
				EventlogList[log].recnum++;
				break;

			/* Eventlog corrupted (?)... Reopen */
			case ERROR_EVENTLOG_FILE_CORRUPT:
				Log(LOG_INFO, "Eventlog was corrupted: \"%s\"", EventlogList[log].name);
				reopen = TRUE;
				break;

			/* Eventlog files are clearing... Reopen */
			case ERROR_EVENTLOG_FILE_CHANGED:
				Log(LOG_INFO, "Eventlog was cleared: \"%s\"", EventlogList[log].name);
				reopen = TRUE;
				break;

			/* Record not available (yet) */
			case ERROR_INVALID_PARAMETER:
				return NULL;

			/* Normal end of eventlog messages */
			case ERROR_HANDLE_EOF:
				return NULL;

			/* Eventlog probably closing down */
			case RPC_S_UNKNOWN_IF:
				return NULL;

			/* Unknown condition */
			default:
				Log(LOG_ERROR|LOG_SYS, "Eventlog \"%s\" returned error: ", EventlogList[log].name);
				ServiceIsRunning = FALSE;
				return NULL;
			}

			/* Process reopen */
			if (reopen) {
				EventlogClose(log);
				if (EventlogOpen(log)) {
					ServiceIsRunning = FALSE;
					return NULL;
				}
				reopen = FALSE;
			}
		}
	}

	/* Increase record number */
	EventlogList[log].recnum++;

	/* Get position into buffer */
	current = EventlogList[log].buffer + EventlogList[log].pos;

	/* Get pointer to current event record */
	event = (EVENTLOGRECORD *) current;             /*格式化数据*/

	/* Advance position */
	EventlogList[log].pos += event->Length;         /*下一次读取日志的位置*/

	/* Get source and event id */
	source = current + sizeof(*event);                /*跳过头部*/
	event_id = (int) HRESULT_CODE(event->EventID);    /*EventID 日志记录标识*/

	
	/* Check number of strings */
	if (event->NumStrings > COUNT_OF(string_array)) {  /*StringOffset  描述字符串的偏移量
													     NumStrings    log中string的数量，如果合并就是整个log string*/

		/* Too many strings */
		Log(LOG_WARNING, "Eventlog message has too many strings to print message: \"%s\": %u strings", EventlogList[log].name, event->NumStrings);
		formatted_string = NULL;
	} else {

		/* Convert strings to arrays */
		cp = current + event->StringOffset;    /*存储每个string*/
		for (i = 0; i < event->NumStrings; i++) {
			string_array[i] = cp;
			while (*cp++ != '\0');
		}

		message_file = LookupMessageFile(EventlogList[log].name, source, event->EventID);

		if (message_file == NULL) {
			/* Cannot load resources */
			formatted_string = NULL;
		} else {
			/* Format eventlog message */
			formatted_string = FormatLibraryMessage(message_file, event->EventID, string_array);
		}
	}

	/* Create a default message if resources or formatting didn't work */
	if (formatted_string == NULL) {
		_snprintf_s(defmsg, sizeof(defmsg), _TRUNCATE,
			"(Facility: %u, Status: %s)",
			HRESULT_FACILITY(event->EventID),
			FAILED(event->EventID) ? "Failure" : "Success"
		);
		formatted_string = defmsg;
	}

	/* replace every space in source by underscores */
	index = source;
	while( *index ) {
		if( *index == ' ' ) {
			*index = '_';
		}
		index++;
	}

	/* Format source and event ID number */
	
	_snprintf_s(message, sizeof(message), _TRUNCATE,  "%s: %u: ",   source,  HRESULT_CODE(event->EventID));
	

	/* Convert user */
	if (event->UserSidLength > 0) {
		username = GetUsername((SID *) (current + event->UserSidOffset));
		if (username) {
			strncat_s(message, sizeof(message), username, _TRUNCATE);
			strncat_s(message, sizeof(message), ": ", _TRUNCATE);
		}
	}

	/* Add formatted string to base message */
	strncat_s(message, sizeof(message), formatted_string, _TRUNCATE);

	/* Select syslog level */
	switch (event->EventType) {
		case EVENTLOG_ERROR_TYPE:
			loglevel = SYSLOG_ERR;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;
		case EVENTLOG_WARNING_TYPE:
			loglevel = SYSLOG_WARNING;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;
		case EVENTLOG_INFORMATION_TYPE:
			loglevel = SYSLOG_NOTICE;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;
		case EVENTLOG_AUDIT_SUCCESS:
            strncat_s(message, sizeof(message), "AUDIT_SUCCESS ", _TRUNCATE);
			loglevel = SYSLOG_NOTICE;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;
		case EVENTLOG_AUDIT_FAILURE:
            strncat_s(message, sizeof(message), "AUDIT_FAILURE ", _TRUNCATE);
			loglevel = SYSLOG_ERR;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;

		/* Everything else */
		case EVENTLOG_SUCCESS:
		default:
			loglevel = SYSLOG_NOTICE;
			*level = SYSLOG_BUILD(SyslogFacility, loglevel);
			break;
	}

	/* If event is not being ignored, make sure it is severe enough to be logged */
	if (SyslogLogLevel != 0)
		if (SyslogLogLevel < loglevel-1)
			return NULL;

	/* Add hostname for RFC compliance (RFC 3164) */
	/* if -a then use the fqdn bound to our IP address. If none, use the IP address */
	if (ProgramUseIPAddress == TRUE) {
		strcpy_s(hostname, HOSTNAME_SZ, ProgramHostName);
	} else {
		if (ExpandEnvironmentStrings("%COMPUTERNAME%", hostname, COUNT_OF(hostname)) == 0) {
			strcpy_s(hostname, COUNT_OF(hostname), "HOSTNAME_ERR");
			Log(LOG_ERROR|LOG_SYS, "Cannot expand %COMPUTERNAME%");
	    }
    }
	
	/* Query and Add timestamp from EventLog, add hostname, */
	/* and finally the message to the string */
	_snprintf_s(tstamped_message, sizeof(tstamped_message), _TRUNCATE,
		"%s %s %s",
		TimeToString(event->TimeGenerated),
		hostname,
		message
	);

	printf("-------------->%s\n", tstamped_message);

	/* Return formatted message */
	return tstamped_message;
}

/* Format Timestamp from EventLog */
char * TimeToString(DWORD dw)
{
	time_t tt;
	struct tm stm;
	char result[16];
	static char formatted_result[] = "Mmm dd hh:mm:ss";

	tt = (time_t) dw;
	
	/* Format timestamp string */
	if (localtime_s(&stm, &tt) == 0) {
		strftime(result, sizeof(result), "%b %d %H:%M:%S", &stm);
		if (result[4] == '0') /* Replace leading zero with a space for */
			result[4] = ' ';  /* single digit days so we comply with the RFC */
	} else
		result[0] = '\0';

	strncpy_s(formatted_result, sizeof(result), result, _TRUNCATE);
	
	return formatted_result;
}