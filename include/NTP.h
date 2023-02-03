

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
struct timeval *SysTimeInit( const int );
struct timeval *SysTimeStep( const long );
void SysTimeToHWTime( const int );

int NTPConnect( const char *, const uint );
int NTPSend( void );
int NTPRecv( void );


#ifdef __cplusplus
}
#endif
#endif
