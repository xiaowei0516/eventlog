#include "main.h"
#include "eventlog.h"
#include "syslog.h"
#include "ver.h"

BOOL ServiceIsRunning = TRUE;
// Main program //
int main(int argc, char ** argv)
{
	int status;
	status = MainLoop();
	// Success //
	return status;
}

