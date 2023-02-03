#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"
#include "./include/NTP.h"


void MyTimerFun(void)
{
	struct timeval *tv;

	tv = SysTimeStep( 50000 );
	Print("\r\nNow Time is %ld.%.6ld", tv->tv_sec, tv->tv_usec);

	return;
}

/* Main function, entry */
void main( void )
{
/* Initialization for u7186EX's general library */
	InitLib();
	Init5DigitLed();
/* Re-initialization for network interface library */
	if ( NetStart() < 0 )
		return;
/* Wait for the network interface ready, it might be shoter */
	YIELD();
	Delay(5);
/* */
	SysTimeInit( 8 );

	Print("\r\nPress any key to start timer");
	Print("\r\nthen Press 'q' to quit\r\n");

	Getch();
	InstallUserTimer1Function_us(500000, MyTimerFun);
	//NTPConnect( "140.112.2.189", 123 );

	while( 1 ) {
		if ( Kbhit() && Getch() == 'q' )
			break;
	}

	StopUserTimer1Fun();

	return;
}
