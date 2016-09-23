#include "main.h"
#include "eventlog.h"
#include "log.h"
#include "syslog.h"
#include "ver.h"
#include "check.h"

// Main program //
int main(int argc, char ** argv)
{
	int status;
	status = MainLoop();
	// Success //
	return status;
}

