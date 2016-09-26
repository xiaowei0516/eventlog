#include <mbstring.h>
#include "main.h"
#include "eventlog.h"



// List of registry items to store //
struct RegistryData {
	char * name;			// Name of key			//
	DWORD key_type;			// Type of key			//
	void * value;			// Value of key			//
	DWORD size;				// Size of key			//
	int required;			// Key is required		//
};


// Location of eventlog keys //
static char RegistryEventlogKeyPath[] = "SYSTEM\\CurrentControlSet\\Services\\Eventlog";



int RegistryGather()
{
	DWORD size;
	HKEY registry_handle;
	char name[EVENTLOG_NAME_SZ];  /*128*/
	int errnum;
	int index;
	if (RegOpenKey(HKEY_LOCAL_MACHINE, RegistryEventlogKeyPath, &registry_handle)) {
		printf( "Cannot initialize access to registry: \"%s\"", RegistryEventlogKeyPath);
		return 1;
	}

	index = 0;
	while (1) {

		
		size = sizeof(name);
		errnum = RegEnumKey(registry_handle, index, name, size);
		if (errnum == ERROR_NO_MORE_ITEMS)
			break;

		if (errnum) {
			printf("Cannot enumerate registry key: \"%s\"", RegistryEventlogKeyPath);
			break;
		}
		
		if (EventlogCreate(name))
			break;
		index++;
	}
	
	RegCloseKey(registry_handle);
	return errnum != ERROR_NO_MORE_ITEMS;
}


