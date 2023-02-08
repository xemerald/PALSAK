/**
 * @file SYSTIME.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2023-02-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __PALSAK_SYSTIME_H__
#define __PALSAK_SYSTIME_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* The checking result of func. */
#define SYSTIME_SUCCESS   0
#define SYSTIME_ERROR    -1
#define SYSTIME_WARNING  -2
/*
 *
 */
extern const struct timeval far *SoftSysTime;
/*
 *
 */
void SysTimeInit( const int, const long );
void SysTimeService( void );
void SysTimeGet( struct timeval * );
void SysTimeToHWTime( const int );

int  NTPConnect( const char *, const uint );
int  NTPProcess( void );
void NTPClose( void );

#ifdef __cplusplus
}
#endif
#endif
