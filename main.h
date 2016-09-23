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
#define HOSTNAME_SZ 64

#define ERR_FAIL	-1
#define SOURCE_SZ	128
#define PLUGIN_SZ	32

/* Compatibility */
#define in_addr_t	unsigned long



extern char ProgramExePath[MAX_PATH];

/* Prototypes */
char*   CollapseExpandMessage(char * message);
int     EventlogCreate(char * name);
char*   EventlogNext( int log, int * level);
void    EventlogsClose(void);
int     EventlogsOpen(void);
char*   FormatLibraryMessage(char * message_file, DWORD event_id, char ** string_array);
void    GetError(DWORD err_num, char * message, int len);
char*   GetTimeStamp(void);
char*   GetUsername(SID * sid);
char*   LookupMessageFile(char * logtype, char * source, DWORD eventID);
int     MainLoop(void);
int     RegistryGather(void);