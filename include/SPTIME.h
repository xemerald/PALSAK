/**
 * @file SPTIME.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2023-02-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef __PALSAK_SPTIME_H__
#define __PALSAK_SPTIME_H__

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
#define ONE_EPOCH_FRAC       65536  /* 65536 frac = 1 sec */
#define HALF_EPOCH_FRAC      32768  /* 32768 frac = 0.5 sec */
#define FRAC_RANDOM_FILL     0x0000F070
/*
 * The time between the Unix Timestamp Base Epoch (00:00:00 UTC, January 1, 1970) &
 * NTP Base Epoch (00:00:00 UTC, January 1, 1900) measured in seconds.
 */
#define EPOCH_DIFF_JAN1970   2208988800UL
/* */
#define ONE_CLOCK_STEP_FRAC  32     /* One step for clock in frac & it should not larger than 256 & less than 2 */
#define ABS_HALF_CLOCK_STEP  16     /* Half step for clock in frac & it should be always larger than 0 */
#define STEP_TIMES_IN_EPOCH  2048   /* ABS(ONE_EPOCH_FRAC / ONE_CLOCK_STEP_FRAC) */
#define L_CLOCK_PRECISION    -10    /* Precision of the local clock, in seconds to the nearest power of two */
/* */
#define FIVE_MSEC_FRACTION   ((5 << 16) / 1000)
#define MIN_INTERVAL_POW     5
#define MAX_INTERVAL_POW     9
/* */
#define COMPENSATE_CANDIDATE_NUM 5
/* */
#define DEFAULT_NTP_UDP_PORT  123
/* */
#define SYSTIME_INSTALL_TICKTIMER_FUNC(__FUNC) \
		InstallUserTimer1Function_us((ulong)ONE_CLOCK_STEP_FRAC * 78125 / 512 + 1, (__FUNC))
#define SYSTIME_STOP_TICKTIMER_FUNC() \
		StopUserTimer1Fun();
/*
 *
 */
typedef struct {
	ulong tv_sec;   /* seconds */
	uint  tv_frac;  /* fraction of second */
} timeval_s;

/*
 *
 */
extern const ulong far *SoftTimeBase;
extern const uint  far *SoftTimeSec;
extern const uint  far *SoftTimeFrac;
/*
 *
 */
void SysTimeInit( const int );
void SysTimeService( void );
void SysTimeGet( timeval_s * );
void SysTimeToHWTime( const int );

int  NTPConnect( const char *, const uint );
int  NTPProcess( void );
void NTPClose( void );

#ifdef __cplusplus
}
#endif
#endif
