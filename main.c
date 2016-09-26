#include "main.h"
#include "eventlog.h"



BOOL ServiceIsRunning = TRUE;
// Main program //
int main(int argc, char ** argv)
{
	int status;
	status = MainLoop();
	return status;
}

