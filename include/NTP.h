

#ifndef __PALSAK_NTP_H__
#define __PALSAK_NTP_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 */
#include <time.h>

#include "./include/u7186EX/7186e.h"

/*
 *
 */
struct timeval *SysTimeInit( const int );
struct timeval *SysTimeStep( const long );
void SysTimeToHWTime( const int );

int NTPConnect( const char *, const uint );
int NTPSend( void );
int NTPRecv( const int );


#ifdef __cplusplus
}
#endif
#endif
