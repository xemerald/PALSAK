
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/SPTIME.h"

/*
 * Byte order conversions
 */
#define HTONS_FP(__X) (htonl((__X)))
#define NTOHS_FP(__X) (ntohl((__X)))
/*
 *
 */
static time_t _mktime( uint, uint, uint, uint, uint, uint );
static uint   nfrac2lfrac( const ulong );
static ulong  lfrac2nfrac( const uint );
static long   get_compensate_avg( long [] );

/* */
#define INTERNAL_BUF_SIZE  64
static char InternalBuffer[INTERNAL_BUF_SIZE];
/* */
static volatile int MainSock = -1;
/* */
static timeval_s _SoftSysTime;
const timeval_s far *SoftSysTime = &_SoftSysTime;
/* */
static BYTE      PollIntervalPow;
static BYTE      WriteToRTC;
static long      CompensateFrac;
static int       RmCompensateFrac;
static uint      CorrectTimeStep;
static long      TimeResidualFrac;
static uint      WriteRTCCountDown;
static TIME_DATE TimeDateSetting;
/* */
static COUNTDOWNTIMER NTPProcessTimer;

/**
 * @brief
 *
 * @param timezone
 */
void SysTimeInit( const int timezone )
{
	TIME_DATE timedate;

/* */
	GetTimeDate(&timedate);
/* */
	_SoftSysTime.tv_sec
		= _mktime( timedate.year, timedate.month, timedate.day, timedate.hour, timedate.minute, timedate.sec ) - ((long)timezone * 3600);
	_SoftSysTime.tv_frac = 16384;
	PollIntervalPow      = MIN_INTERVAL_POW;
	WriteToRTC           = 0;
	CompensateFrac       = 0L;
	RmCompensateFrac     = 0;
	CorrectTimeStep      = ONE_CLOCK_STEP_FRAC;
	TimeResidualFrac     = 0L;
	WriteRTCCountDown    = 0;
/* */
	T_CountDownTimerStart(&NTPProcessTimer, 0);
/* Flush the internal buffer */
	memset(InternalBuffer, 0, INTERNAL_BUF_SIZE);

	return;
}

/**
 * @brief
 *
 */
void SysTimeService( void )
{
	static int count_step_epoch = STEP_TIMES_IN_EPOCH;

/* */
WRITE_RTC_CHECK:
	_asm {
		mov cx, CorrectTimeStep
		cmp byte ptr WriteToRTC, 0
		je EPOCH_CHECK
		sub WriteRTCCountDown, cx
		jz REAL_WRITE_RTC
		jnc EPOCH_CHECK
	}
REAL_WRITE_RTC:
	_asm {
		push ds
		push offset TimeDateSetting
		call far ptr SetTimeDate
		add sp, 4
		mov byte ptr WriteToRTC, 0
	}
/* */
EPOCH_CHECK:
	_asm {
		dec count_step_epoch
		mov ax, count_step_epoch
		or ax, ax
		jnz SELECT_COMPENSATE
		mov count_step_epoch, STEP_TIMES_IN_EPOCH
		jmp STEP_RESIDUAL
	}
SELECT_COMPENSATE:
	_asm {
		cmp ax, RmCompensateFrac
		jle INC_CX
		neg ax
		cmp ax, RmCompensateFrac
		jl STEP_RESIDUAL
		dec cx
		jmp STEP_RESIDUAL
	}
INC_CX:
	_asm {
		inc cx
	}
/* If there is some residual only in sub-second, step or slew it! */
STEP_RESIDUAL:
	_asm {
		mov ax, word ptr TimeResidualFrac
		or ax, word ptr TimeResidualFrac+2
		jz REAL_ADJS
		mov ax, word ptr TimeResidualFrac
		mov dx, word ptr TimeResidualFrac+2
		or dx, dx
		js NEG_RESIDUAL_CHECK
		cmp dx, 0
		jg ASSIGN_POS_RESIDUAL
		jne ZERO_RESIDUAL
		cmp ax, ABS_HALF_CLOCK_STEP
		jbe ZERO_RESIDUAL
	}
ASSIGN_POS_RESIDUAL:
	_asm {
		add cx, ABS_HALF_CLOCK_STEP
		sub word ptr TimeResidualFrac, ABS_HALF_CLOCK_STEP
		sbb word ptr TimeResidualFrac+2, 0
		jmp REAL_ADJS
	}
NEG_RESIDUAL_CHECK:
	_asm {
		cmp dx, 0xFFFF
		jg ZERO_RESIDUAL
		jne ASSIGN_NEG_RESIDUAL
		cmp ax, 0xFFF0
		jae ZERO_RESIDUAL
	}
ASSIGN_NEG_RESIDUAL:
	_asm {
		sub cx, ABS_HALF_CLOCK_STEP
		add word ptr TimeResidualFrac, ABS_HALF_CLOCK_STEP
		adc word ptr TimeResidualFrac+2, 0
		jmp REAL_ADJS
	}
ZERO_RESIDUAL:
	_asm {
		mov word ptr TimeResidualFrac, 0
		mov word ptr TimeResidualFrac+2, 0
		add cx, ax
	}
/* Keep the clock step forward */
REAL_ADJS:
	_asm {
		add word ptr _SoftSysTime+4, cx
		adc word ptr _SoftSysTime, 0
		adc word ptr _SoftSysTime+2, 0
	}

	return;
}

/**
 * @brief
 *
 * @param sys_time
 */
void SysTimeGet( timeval_s far *sys_time )
{
	_asm cli;
	*sys_time = _SoftSysTime;
	_asm sti;

	return;
}

/**
 * @brief
 *
 * @param timezone
 * @param timer
 */
void SysTimeToHWTime( const int timezone )
{
	timeval_s  now_time;
	struct tm *brktime;

/* */
	_asm cli;
	now_time = _SoftSysTime;
	_asm sti;
/* Add to the next second */
	now_time.tv_sec += ((long)timezone * 3600) + 1;
/* Turn the frac to the frac between next second */
	now_time.tv_frac = ONE_EPOCH_FRAC - now_time.tv_frac;
/* */
	brktime = gmtime( &now_time.tv_sec );
	TimeDateSetting.year  = brktime->tm_year + 1900;
	TimeDateSetting.month = brktime->tm_mon + 1;
	TimeDateSetting.day   = brktime->tm_mday;
/* */
	TimeDateSetting.hour   = brktime->tm_hour;
	TimeDateSetting.minute = brktime->tm_min;
	TimeDateSetting.sec    = brktime->tm_sec;
/* */
	WriteRTCCountDown = now_time.tv_frac;
	WriteToRTC = 1;

	return;
}

/**
 * @brief
 *
 * @param host
 * @param port
 * @return int
 */
int NTPConnect( const char *host, const uint port )
{
	struct sockaddr_in _addr;

/* Create the UDP socket */
	if ( (MainSock = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		return SYSTIME_ERROR;
/* Set the timeout of receiving socket to 0.5 sec. */
	SOCKET_RXTOUT(MainSock, 500);

/* Addressing for master socket */
	memset(&_addr, 0, sizeof(_addr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = inet_addr((char *)host);
	_addr.sin_port = htons(port);
/* Initialize the connection */
	if ( connect(MainSock, (struct sockaddr*)&_addr, sizeof(_addr)) < 0 ) {
	/* Close the opened socket */
		closesocket(MainSock);
		return SYSTIME_ERROR;
	}

	return SYSTIME_SUCCESS;
}

/**
 * @brief Synchronize the time by modified SNTP protocol. Ref. RFC 1361.
 *
 * @return int
 */
int NTPProcess( void )
{
	static long  compensate[COMPENSATE_CANDIDATE_NUM];
	static uchar i_compensate = 0;
	static uchar first_time = 1;
/* */
	long offset_sec;
	long offset_frac;
	timeval_s tv1, tv2, tv3, tv4;

/* Check the processing interval */
	if ( !T_CountDownTimerIsTimeUp(&NTPProcessTimer) )
		return SYSTIME_SUCCESS;
/* 00 001 011 - leap, ntp ver, client. Ref. RFC 1361. */
	InternalBuffer[0] = (0 << 6) | (1 << 3) | 3;
/* Polling interval */
	InternalBuffer[2] = (char)PollIntervalPow;
/* Clock precision */
	InternalBuffer[3] = L_CLOCK_PRECISION;
/* Get the local sent time - Originate Timestamp */
	_asm cli;
	tv1 = _SoftSysTime;
	_asm sti;
	*(ulong *)&InternalBuffer[40] = HTONS_FP( tv1.tv_sec + EPOCH_DIFF_JAN1970 );
	*(ulong *)&InternalBuffer[44] = HTONS_FP( lfrac2nfrac( tv1.tv_frac ) );
/* Send to the server */
	if ( send(MainSock, InternalBuffer, 48, 0) <= 0 )
		return SYSTIME_WARNING;
/* Read from the server */
	if ( recv(MainSock, InternalBuffer, INTERNAL_BUF_SIZE, 0) <= 0 )
		return SYSTIME_WARNING;
/* Get the local received timestamp */
	_asm cli;
	tv4 = _SoftSysTime;
	_asm sti;
/* Checking part */
	//if ( (tv1.tv_usec = NTOHS_FP( *(ulong *)&InternalBuffer[28] )) & FRAC_RANDOM_FILL ^ FRAC_RANDOM_FILL )
		//return SYSTIME_ERROR;
	tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
	tv1.tv_frac = nfrac2lfrac( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
/* Get the remote receive timestamp */
	tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
	tv2.tv_frac = nfrac2lfrac( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
/* Get the remote transmit timestamp */
	tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
	tv3.tv_frac = nfrac2lfrac( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
/* Calculate the time offset */
	offset_sec  = (tv2.tv_sec - tv1.tv_sec) + (tv3.tv_sec - (tv4.tv_sec + EPOCH_DIFF_JAN1970));
	offset_frac = ((long)((ulong)tv2.tv_frac - (ulong)tv1.tv_frac) + (long)((ulong)tv3.tv_frac - (ulong)tv4.tv_frac)) / 2;
	if ( offset_sec & 0x1 )
		offset_frac += offset_sec < 0 ? -HALF_EPOCH_FRAC : HALF_EPOCH_FRAC;
	offset_sec /= 2;
/* Deal with the different sign condition */
	if ( offset_sec && (offset_sec ^ offset_frac) & 0x80000000 ) {
		if ( offset_sec < 0 ) {
			++offset_sec;
			offset_frac -= ONE_EPOCH_FRAC;
		}
		else {
			--offset_sec;
			offset_frac += ONE_EPOCH_FRAC;
		}
	}
/* Maybe also need to calculate the transmitting delta... */
/* Flush the internal buffer for next time usage */
	memset(InternalBuffer, 0, INTERNAL_BUF_SIZE);
/* If the residual is larger than one second, directly adjust it! */
	if ( offset_sec ) {
	/* Disable the ISR */
		_asm cli;
		_SoftSysTime.tv_sec += offset_sec;
		_asm sti;
	}
/* Otherwise keep the adjustment in residual */
	if ( offset_frac ) {
	/* Disable the ISR */
		_asm cli;
		TimeResidualFrac = offset_frac;
		_asm sti;
	}

/* */
	if ( !first_time ) {
	/* */
		compensate[i_compensate++] = offset_frac + offset_sec * ONE_EPOCH_FRAC;
		if ( i_compensate >= COMPENSATE_CANDIDATE_NUM ) {
		/* */
			i_compensate  = 0;
			compensate[1] = get_compensate_avg( compensate );
			compensate[2] = labs(CompensateFrac) << 1;
		/* */
			if ( !(compensate[0] = compensate[1] / (long)(1 << (ulong)PollIntervalPow)) )
				compensate[0] = compensate[1] / (long)(1 << (ulong)(PollIntervalPow - 1));
		/* */
			if ( !CompensateFrac && (labs(compensate[0] - CompensateFrac) > compensate[2] && compensate[2] > 20) ) {
				PollIntervalPow  = MIN_INTERVAL_POW;
				CorrectTimeStep  = ONE_CLOCK_STEP_FRAC;
				RmCompensateFrac = 0;
				CompensateFrac   = 0L;
				first_time       = 1;
			}
			else {
			/* Just in case & avoid the CorrectTimeStep whould be too large or being negative */
				if ( labs(CompensateFrac += compensate[0]) >= HALF_EPOCH_FRAC )
					return SYSTIME_ERROR;
			/* */
				compensate[3]    = CompensateFrac / STEP_TIMES_IN_EPOCH;
				CorrectTimeStep  = (uint)(ONE_CLOCK_STEP_FRAC + compensate[3]);
				RmCompensateFrac = (int)(CompensateFrac - compensate[3] * STEP_TIMES_IN_EPOCH);
			/* */
				if ( labs(compensate[0]) <= 1 ) {
					if ( PollIntervalPow < MAX_INTERVAL_POW )
						++PollIntervalPow;
				}
				else if ( labs(compensate[0]) > 2 ) {
					if ( PollIntervalPow > MIN_INTERVAL_POW )
						--PollIntervalPow;
				}
			}
		}
	}
	else {
		first_time = 0;
	}
/* Debug information */
#ifdef __SYSTIME__DEBUG__
	Print("\r\nOffset:     %ld sec, %ld(%ld) usec.", offset_sec, (offset_frac * 15625 / 1024), offset_frac);
	Print("\r\nPolling:    %u sec.", 1 << PollIntervalPow);
	Print("\r\nFrequency:  %+ld(%+ld) ppm.", (CompensateFrac * 15625 / 1024), CompensateFrac);
#endif
/* */
	T_CountDownTimerStart(&NTPProcessTimer, 1000UL << (ulong)PollIntervalPow);

	return SYSTIME_SUCCESS;
}

/**
 * @brief
 *
 */
void NTPClose( void )
{
/* */
	if ( MainSock >= 0 ) {
		closesocket(MainSock);
		MainSock = -1;
	}
/* */
	YIELD();

	return;
}

/**
 * @brief Turn the broken time structure into calendar time(UTC)
 *
 * @param year
 * @param mon
 * @param day
 * @param hour
 * @param min
 * @param sec
 * @return ulong
 */
static time_t _mktime( uint year, uint mon, uint day, uint hour, uint min, uint sec )
{
	if ( 0 >= (int)(mon -= 2) ) {
	/* Puts Feb last since it has leap day */
		mon += 12;
		--year;
	}

	return ((((time_t)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + (ulong)year * 365 - 719499) * 24 + hour) * 60 + min) * 60 + sec;
}

/**
 * @brief Convert from fraction of a second to microsecond
 *
 * @param frac
 * @return ulong: microsecond.
 */
static uint nfrac2lfrac( const ulong frac )
{
	return (uint)((frac >> 16) & 0x0000ffff);
}

/**
 * @brief Convert from local fraction to ntp long fraction of a second
 *
 * @param lfrac
 * @return ulong
 */
static ulong lfrac2nfrac( const uint lfrac )
{
	return ((ulong)lfrac << 16) | FRAC_RANDOM_FILL;
}

/**
 * @brief Get the compensate average
 *
 * @param compensate
 * @return long
 */
static long get_compensate_avg( long compensate[] )
{
	register int i;
	int  i_st, i_nd;
	long result;
	long _max;

/* */
	for ( i = 0, result = 0; i < COMPENSATE_CANDIDATE_NUM; i++ )
		result += compensate[i];
	result /= COMPENSATE_CANDIDATE_NUM;
/* */
	for ( i = 1, i_st = 0, i_nd = 1; i < COMPENSATE_CANDIDATE_NUM; i++ ) {
		_max = labs(compensate[i] - result);
		if ( _max > labs(compensate[i_st] - result) ) {
			i_nd = i_st;
			i_st = i;
		}
		else if ( _max > labs(compensate[i_nd] - result) ) {
			i_nd = i;
		}
	}
/* */
	for ( i = 0, result = 0; i < COMPENSATE_CANDIDATE_NUM; i++ ) {
		if ( i != i_st && i != i_nd )
			result += compensate[i];
	}
	result /= COMPENSATE_CANDIDATE_NUM - 2;

	return result;
}
