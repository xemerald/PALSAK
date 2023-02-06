

#ifndef __PALSAK_NTP_H__
#define __PALSAK_NTP_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"

/*
 *
 */
void SysTimeInit( const int );
void SysTimeService( const long );
void SysTimeGet( struct timeval * );
void SysTimeToHWTime( const int );

int NTPConnect( const char *, const uint );
int NTPProcess( uint );


#ifdef __cplusplus
}
#endif
#endif
