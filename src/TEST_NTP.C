#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"
#include "./include/SPTIME.h"

/* Main function, entry */
void main( void )
{
	timeval_s tv;
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
	Int1cFlag = 0;
	Int9Flag = 0;
	SysTimeInit( 8 );
	SYSTIME_INSTALL_TICKTIMER_FUNC( SysTimeService );

	Print("\r\nPress any key to start timer");
	Print("\r\nthen Press 'q' to quit\r\n");
	NTPConnect( "140.112.2.189", DEFAULT_NTP_UDP_PORT );

	Getch();
	while( 1 ) {
		if ( Kbhit() && Getch() == 'q' )
			break;
		Delay2(100);
		NTPProcess();
		if ( inc % 5 == 0 ) {
			SysTimeGet( & tv );
			Print("\r\nNow time is %ld.%.4ld", tv.tv_sec, ((long)tv.tv_frac * 625) / 4096);
		}
		inc++;
		if ( inc % 3000 == 0 ) {
			SysTimeToHWTime( 8 );
		}
	}

	StopUserTimer1Fun();

	return;
}
