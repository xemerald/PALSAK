/**
 * @file TEST_NTP.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2023-01-10
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/SPTIME.h"

/* Main function, entry */
void main( void )
{
	timeval_s tv;
	uint      inc = 0;
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
	SYSTIME_SERVICE_START();
/* */
	Print("\r\nPress any key to start timer");
	Print("\r\nthen press 'q' to quit\r\n");
	NTPConnect( "140.112.2.189", DEFAULT_NTP_UDP_PORT );
/* */
	Getch();
	while( 1 ) {
		if ( Kbhit() && Getch() == 'q' )
			break;
	/* */
		Delay2(100);
		NTPProcess();
		if ( inc % 5 == 0 ) {
			SysTimeGet( &tv );
			Print("\r\nNow is %ld.%.6ld", tv.tv_sec, ((long)tv.tv_frac * 15625) / 1024);
		}
		inc++;
	/* */
		if ( inc % 3000 == 0 )
			SysTimeToHWTime( 8 );
	}
	SYSTIME_SERVICE_STOP();

	return;
}
