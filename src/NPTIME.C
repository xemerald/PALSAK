/**
 * @file NPTIME.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2023-02-06
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/NPTIME.h"

/*
 * Byte order conversions
 */
#define HTONS_FP(__X) (htonl((__X)))
#define NTOHS_FP(__X) (ntohl((__X)))
/*
 *
 */
static time_t _mktime( uint, uint, uint, uint, uint, uint );
static ulong  frac2usec( const ulong );
static ulong  usec2frac( const ulong );
static long   get_compensate_avg( long [] );

/* */
#define INTERNAL_BUF_SIZE  64
static char InternalBuffer[INTERNAL_BUF_SIZE];
/* */
static volatile int NTPSock = -1;
/* */
static struct timeval _SoftSysTime;
const struct timeval far *SoftSysTime = &_SoftSysTime;
/* */
static BYTE      PollIntervalPow;
static BYTE      WriteToRTC;
static long      CompensateUSec;
static int       RmCompensateUSec;
static uint      CorrectTimeStep;
static long      TimeResidualUsec;
static long      WriteRTCCountDown;
static TIME_DATE TimeDateSetting;

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
	_SoftSysTime.tv_usec = 250000L;
	PollIntervalPow      = MIN_INTERVAL_POW;
	WriteToRTC           = 0;
	CompensateUSec       = 0L;
	RmCompensateUSec     = 0;
	CorrectTimeStep      = (uint)ONE_CLOCK_STEP_USEC;
	TimeResidualUsec     = 0L;
	WriteRTCCountDown    = 0L;
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
		sub word ptr WriteRTCCountDown, cx
		mov ax, word ptr WriteRTCCountDown
		sbb word ptr WriteRTCCountDown+2, 0
		mov dx, word ptr WriteRTCCountDown+2
		or dx, dx
		jg EPOCH_CHECK
		jne REAL_WRITE_RTC
		or ax, ax
		ja EPOCH_CHECK
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
		mov count_step_epoch, 2000
		jmp STEP_RESIDUAL
	}
SELECT_COMPENSATE:
	_asm {
		cmp ax, RmCompensateUSec
		jle INC_CX
		neg ax
		cmp ax, RmCompensateUSec
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
		mov ax, word ptr TimeResidualUsec
		mov dx, word ptr TimeResidualUsec+2
		or ax, dx
		jz REAL_ADJS
		or dx, dx
		js NEG_RESIDUAL_CHECK
		cmp dx, 0
		jg ASSIGN_POS_RESIDUAL
		jne ZERO_RESIDUAL
		cmp word ptr TimeResidualFrac, 250
		jbe ZERO_RESIDUAL
	}
ASSIGN_POS_RESIDUAL:
	_asm {
		add cx, 250
		sub word ptr TimeResidualUsec, 250
		sbb word ptr TimeResidualUsec+2, 0
		jmp REAL_ADJS
	}
NEG_RESIDUAL_CHECK:
	_asm {
		cmp dx, 0xFFFF
		jg ZERO_RESIDUAL
		jne ASSIGN_NEG_RESIDUAL
		cmp word ptr TimeResidualFrac, 0xFF06
		jae ZERO_RESIDUAL
	}
ASSIGN_NEG_RESIDUAL:
	_asm {
		sub cx, 250
		add word ptr TimeResidualUsec, 250
		adc word ptr TimeResidualUsec+2, 0
		jmp REAL_ADJS
	}
ZERO_RESIDUAL:
	_asm {
		add cx, word ptr TimeResidualFrac
		mov word ptr TimeResidualUsec, 0
		mov word ptr TimeResidualUsec+2, 0
	}
/* Keep the clock step forward */
REAL_ADJS:
	_asm {
		mov ax, word ptr _SoftSysTime+4
		mov dx, word ptr _SoftSysTime+6
		add ax, cx
		adc dx, 0
	}
CARRY_CHECK:
	_asm {
		cmp dx, 15
		jg CARRY_PROC
		jne FINAL_PROC
		cmp ax, 16960
		jb FINAL_PROC
	}
CARRY_PROC:
	_asm {
		add word ptr _SoftSysTime, 1
		adc word ptr _SoftSysTime+2, 0
		sub ax, 16960
		sbb dx, 15
	}
FINAL_PROC:
	_asm {
		mov word ptr _SoftSysTime+4, ax
		mov word ptr _SoftSysTime+6, dx
	}

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
	struct timeval now_time;
	struct tm *brktime;

/* */
	if ( WriteToRTC )
		return;
/* */
	_asm cli;
	now_time = _SoftSysTime;
	_asm sti;
/* Add to the next second */
	now_time.tv_sec += ((long)timezone * 3600) + 1;
/* Turn the usec to the usec between next second */
	WriteRTCCountDown = ONE_EPOCH_USEC - now_time.tv_usec - 1;
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
	if ( (NTPSock = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		return SYSTIME_ERROR;
/* Set the timeout of receiving socket to 0.5 sec. */
	SOCKET_RXTOUT(NTPSock, 250);

/* Addressing for master socket */
	memset(&_addr, 0, sizeof(_addr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = inet_addr((char *)host);
	_addr.sin_port = htons(port);
/* Initialize the connection */
	if ( connect(NTPSock, (struct sockaddr*)&_addr, sizeof(_addr)) < 0 ) {
	/* Close the opened socket */
		closesocket(NTPSock);
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
	static long  last_sec = 0;
	static uchar i_compensate = 0;
	static uchar first_time = 1;
/* */
	struct timeval offset;
	struct timeval tv1, tv2, tv3, tv4;

/* Check the processing interval */
	_asm cli;
	tv1.tv_sec = _SoftSysTime.tv_sec;
	_asm sti;
	if ( (tv1.tv_sec - last_sec) < (long)(1 << (int)PollIntervalPow) )
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
	*(ulong *)&InternalBuffer[44] = HTONS_FP( usec2frac( tv1.tv_usec ) );
/* Send to the server */
	if ( send(NTPSock, InternalBuffer, 48, 0) <= 0 )
		return SYSTIME_WARNING;
/* Read from the server */
	if ( recv(NTPSock, InternalBuffer, INTERNAL_BUF_SIZE, 0) <= 0 )
		return SYSTIME_WARNING;
/* Get the local received timestamp */
	_asm cli;
	tv4 = _SoftSysTime;
	_asm sti;
/* Checking part */
	//if ( (tv1.tv_usec = NTOHS_FP( *(ulong *)&InternalBuffer[28] )) & FRAC_RANDOM_FILL ^ FRAC_RANDOM_FILL )
		//return SYSTIME_ERROR;
	tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
	tv1.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
/* Get the remote receive timestamp */
	tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
	tv2.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
/* Get the remote transmit timestamp */
	tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
	tv3.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
/* Calculate the time offset */
	offset.tv_sec  = (tv2.tv_sec - tv1.tv_sec) + (tv3.tv_sec - (tv4.tv_sec + EPOCH_DIFF_JAN1970));
	offset.tv_usec = ((tv2.tv_usec - tv1.tv_usec) + (tv3.tv_usec - tv4.tv_usec)) / 2;
	if ( offset.tv_sec & 0x1 )
		offset.tv_usec += offset.tv_sec < 0 ? -HALF_EPOCH_USEC : HALF_EPOCH_USEC;
	offset.tv_sec /= 2;
/* Deal with the different sign condition */
	if ( offset.tv_sec && (offset.tv_sec ^ offset.tv_usec) & 0x80000000 ) {
		if ( offset.tv_sec < 0 ) {
			++offset.tv_sec;
			offset.tv_usec -= ONE_EPOCH_USEC;
		}
		else {
			--offset.tv_sec;
			offset.tv_usec += ONE_EPOCH_USEC;
		}
	}
/* Maybe also need to calculate the transmitting delta... */
/* Flush the internal buffer for next time usage */
	memset(InternalBuffer, 0, INTERNAL_BUF_SIZE);
/* If the residual is larger than one second, directly adjust it! */
	if ( offset.tv_sec ) {
	/* Disable the ISR */
		_asm cli;
		_SoftSysTime.tv_sec += offset.tv_sec;
		_asm sti;
	}
/* Otherwise keep the adjustment in residual */
	if ( offset.tv_usec ) {
	/* Disable the ISR */
		_asm cli;
		TimeResidualUsec = offset.tv_usec;
		_asm sti;
	}

/* */
	if ( !first_time ) {
	/* */
		compensate[i_compensate++] = offset.tv_usec + offset.tv_sec * ONE_EPOCH_USEC;
		if ( i_compensate >= COMPENSATE_CANDIDATE_NUM ) {
		/* */
			i_compensate  = 0;
			compensate[1] = get_compensate_avg( compensate );
			compensate[2] = labs(CompensateUSec) << 1;
		/* Get the frequency different, if it is zero we should check reduce the interval to half */
			if ( !(compensate[0] = compensate[1] / (long)(1 << (int)PollIntervalPow)) )
				compensate[0] = compensate[1] / (long)(1 << (int)(PollIntervalPow - 1));
		/* */
			if ( !CompensateUSec && (labs(compensate[0] - CompensateUSec) > compensate[2] && compensate[2] > 200) ) {
				PollIntervalPow  = MIN_INTERVAL_POW;
				CorrectTimeStep  = (uint)ONE_CLOCK_STEP_USEC;
				RmCompensateUSec = 0;
				CompensateUSec   = 0L;
				first_time       = 1;
			}
			else {
			/* Just in case & avoid the CorrectTimeStep whould be too large or being negative */
				if ( labs(CompensateUSec += compensate[0]) >= HALF_EPOCH_USEC )
					return SYSTIME_ERROR;
			/* */
				compensate[3]    = CompensateUSec / STEP_TIMES_IN_EPOCH;
				CorrectTimeStep  = (uint)(ONE_CLOCK_STEP_USEC + compensate[3]);
				RmCompensateUSec = (int)(CompensateUSec - STEP_TIMES_IN_EPOCH * compensate[3]);
			/* */
				if ( labs(compensate[1]) <= (long)(8 << (int)PollIntervalPow) ) {
					if ( PollIntervalPow < MAX_INTERVAL_POW )
						++PollIntervalPow;
				}
				else if ( labs(compensate[1]) > (long)(16 << (int)PollIntervalPow) || labs(compensate[1]) > FIVE_MSEC_USEC ) {
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
	Print("\r\nOffset:     %ld sec, %ld usec.", offset.tv_sec, offset.tv_usec);
	Print("\r\nPolling:    %u sec.", 1 << PollIntervalPow);
	Print("\r\nFrequency:  %+ld ppm.", CompensateUSec);
#endif
/* */
	last_sec = tv4.tv_sec;

	return SYSTIME_SUCCESS;
}

/**
 * @brief
 *
 */
void NTPClose( void )
{
/* */
	if ( NTPSock >= 0 )
		closesocket(NTPSock);
/* */
	NTPSock = -1;
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
static ulong frac2usec( const ulong frac )
{
	return ((((frac >> 16) & 0x0000ffff) * 15625) >> 10) + ((((frac & 0x0000ffff) + ONE_USEC_FRAC) * 15625) >> 26);
}

/**
 * @brief Convert from microsecond to fraction of a second
 *
 * @param usec it must smaller than 1 minion.
 * @return ulong
 */
static ulong usec2frac( const ulong usec )
{
	return (((((((usec & 0x000fffff) << 12) + 4095) / 125) << 7) / 125) << 7) | FRAC_RANDOM_FILL;
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
