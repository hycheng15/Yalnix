#include <comp421/hardware.h>
#include <comp421/yalnix.h>
#include <comp421/loadinfo.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	while(1)
	{	
		TracePrintf(6, "I'm in idle now!\n");
		Pause();
	}
}