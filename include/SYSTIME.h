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
#define SYSTIME_WARNING  -1
#define SYSTIME_ERROR    -2
/* */
#define ONE_EPOCH_USEC       1000000L /* 1000000 usec = 1 sec */
#define HALF_EPOCH_USEC      500000L  /* 500000 usec = 0.5 sec */
#define ONE_USEC_FRAC        4295L
#define HALF_USEC_FRAC       2147L
#define FRAC_RANDOM_FILL     0x00002827
/*
 * The time between the Unix Timestamp Base Epoch (00:00:00 UTC, January 1, 1970) &
 * NTP Base Epoch (00:00:00 UTC, January 1, 1900) measured in seconds.
 */
#define EPOCH_DIFF_JAN1970   2208988800UL
/* */
#define ONE_CLOCK_STEP_USEC  500L     /* One step for clock in usec & it should not larger than 50000 */
#define ABS_HALF_CLOCK_STEP  250L     /* Half step for clock in usec & it should be always larger than 0 */
#define STEP_TIMES_IN_EPOCH  2000L    /* ABS(ONE_EPOCH_USEC / CLOCK_STEP_USEC) */
#define L_CLOCK_PRECISION    -10      /* Precision of the local clock, in seconds to the nearest power of two */
/* */
#define MIN_INTERVAL_POW     5
#define MAX_INTERVAL_POW     9
/* */
#define COMPENSATE_CANDIDATE_NUM 5
/* */
#define SYSTIME_INSTALL_TICKTIMER_FUNC(__FUNC) \
		InstallUserTimer1Function_us(ONE_CLOCK_STEP_USEC * 10, (__FUNC))
/*
 *
 */
extern const struct timeval far *SoftSysTime;
/*
 *
 */
void SysTimeInit( const int );
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
