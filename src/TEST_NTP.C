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
	SysTimeStep( 500 );

	return;
}

/* Main function, entry */
void main( void )
{
	struct timeval tv;
	uint   inc   = 0;
/* Initialization for u7186EX's general library */
	InitLib();
	Init5DigitLed();
/* Re-initialization for network interface library */
	if ( NetStart() < 0 )
		return;
/* Wait for the network interface ready, it might be shoter */
	YIELD();
	Delay2(5);
/* */
	SysTimeInit( 8 );

	Print("\r\nPress any key to start timer");
	Print("\r\nthen Press 'q' to quit\r\n");
	NTPConnect( "140.112.2.189", 123 );

	Getch();
	InstallUserTimer1Function_us(5000, MyTimerFun);

	while( 1 ) {
		if ( Kbhit() && Getch() == 'q' )
			break;
		SysTimeGet( &tv );
		Print("\r\nNow time is %ld.%.6ld", tv.tv_sec, tv.tv_usec);
		Delay2(100);
		if ( inc % 320 == 0 ) {
			NTPSend();
			NTPRecv();
		}
		inc++;
		if ( inc % 3000 == 0 ) {
			SysTimeToHWTime( 8, 2 );
		}
	}

	StopUserTimer1Fun();

	return;
}
