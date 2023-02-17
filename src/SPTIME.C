
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
#define LFRAC_TO_NFRAC(__LFRAC) (((ulong)(__LFRAC) << 16) | FRAC_RANDOM_FILL)
#define NFRAC_TO_LFRAC(__NFRAC) ((uint)(((__NFRAC) >> 16) & 0x0000ffff))
/*
 *
 */
static time_t _mktime( uint, uint, uint, uint, uint, uint );
static long   get_compensate_avg( long [COMPENSATE_CANDIDATE_NUM] );

/* */
#define INTERNAL_BUF_SIZE  64
static char InternalBuffer[INTERNAL_BUF_SIZE];
/* */
static volatile int NTPSock = -1;
/* */
static ulong _SoftTimeBase;
static uint  _SoftTimeSec;
static uint  _SoftTimeFrac;
const  ulong far *SoftTimeBase = &_SoftTimeBase;
const  uint  far *SoftTimeSec  = &_SoftTimeSec;
const  uint  far *SoftTimeFrac = &_SoftTimeFrac;
/* */
static BYTE      PollIntervalPow;
static BYTE      WriteToRTC;
static long      CompensateFrac;
static int       RmCompensateFrac;
static uint      CorrectTimeStep;
static long      TimeResidualFrac;
static uint      WriteRTCCountDown;
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
	_SoftTimeBase
		= _mktime( timedate.year, timedate.month, timedate.day, timedate.hour, timedate.minute, timedate.sec ) - ((long)timezone * 3600);
	_SoftTimeSec      = 0;
	_SoftTimeFrac     = 16384;
	PollIntervalPow   = MIN_INTERVAL_POW;
	WriteToRTC        = 0;
	CompensateFrac    = 0L;
	RmCompensateFrac  = 0;
	CorrectTimeStep   = ONE_CLOCK_STEP_FRAC;
	TimeResidualFrac  = 0L;
	WriteRTCCountDown = 0;
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
		mov dx, word ptr TimeResidualFrac+2
		or ax, dx
		jz REAL_ADJS
		or dx, dx
		js NEG_RESIDUAL_CHECK
		cmp dx, 0
		jg ASSIGN_POS_RESIDUAL
		jne ZERO_RESIDUAL
		cmp word ptr TimeResidualFrac, ABS_HALF_CLOCK_STEP
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
		cmp word ptr TimeResidualFrac, 0xFFF0
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
		add cx, word ptr TimeResidualFrac
		mov word ptr TimeResidualFrac, 0
		mov word ptr TimeResidualFrac+2, 0
	}
/* Keep the clock step forward */
REAL_ADJS:
	_asm {
		add _SoftTimeFrac, cx
		adc _SoftTimeSec, 0
	}
/* Maybe we still need to deal with _SoftTimeSec carrying condition here... */
	return;
}

/**
 * @brief
 *
 * @param tvs
 */
void SysTimeGet( timeval_s far *tvs )
{
_asm {
		les bx, dword ptr tvs
		mov ax, _SoftTimeSec
		mov dx, _SoftTimeFrac
		mov cx, _SoftTimeSec
		mov word ptr es:[bx+4], dx
		cmp dx, HALF_EPOCH_FRAC
		jae ADD_TIMEBASE
		mov ax, cx
	}
ADD_TIMEBASE:
	_asm {
		xor dx, dx
		add ax, word ptr _SoftTimeBase
		adc dx, word ptr _SoftTimeBase+2
		mov word ptr es:[bx], ax
		mov word ptr es:[bx+2], dx
	}

	return;
}

/**
 * @brief
 *
 * @param timezone
 */
void SysTimeToHWTime( const int timezone )
{
	timeval_s  now_time;
	struct tm *brktime;

/* */
	if ( WriteToRTC )
		return;
/* */
	SysTimeGet( &now_time );
/* Add to the next second */
	now_time.tv_sec += ((long)timezone * 3600) + 1;
/* Turn the frac to the frac between next second */
	WriteRTCCountDown = ONE_EPOCH_FRAC - now_time.tv_frac;
/* */
	brktime = gmtime( (time_t *)&now_time.tv_sec );
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
	static uchar i_compensate = 0;
	static uchar first_time = 1;
/* */
	long offset_sec;
	long offset_frac;
	timeval_s tv1, tv2, tv3, tv4;

/* Check the processing interval */
	if ( !first_time && _SoftTimeSec < (1 << (uint)PollIntervalPow) )
		return SYSTIME_SUCCESS;
/* 00 001 011 - leap, ntp ver, client. Ref. RFC 1361. */
	InternalBuffer[0] = (0 << 6) | (1 << 3) | 3;
/* Polling interval */
	InternalBuffer[2] = (char)PollIntervalPow;
/* Clock precision */
	InternalBuffer[3] = L_CLOCK_PRECISION;
/* Get the local sent time - Originate Timestamp */
	SysTimeGet( &tv1 );
	*(ulong *)&InternalBuffer[40] = HTONS_FP( tv1.tv_sec + EPOCH_DIFF_JAN1970 );
	*(ulong *)&InternalBuffer[44] = HTONS_FP( LFRAC_TO_NFRAC( tv1.tv_frac ) );
/* Send to the server */
	if ( send(NTPSock, InternalBuffer, 48, 0) <= 0 )
		return SYSTIME_WARNING;
/* Read from the server */
	if ( recv(NTPSock, InternalBuffer, INTERNAL_BUF_SIZE, 0) <= 0 )
		return SYSTIME_WARNING;
/* Get the local received timestamp */
	SysTimeGet( &tv4 );
/* Checking part */
	//if ( (tv1.tv_usec = NTOHS_FP( *(ulong *)&InternalBuffer[28] )) & FRAC_RANDOM_FILL ^ FRAC_RANDOM_FILL )
		//return SYSTIME_ERROR;
	tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
	tv1.tv_frac = NFRAC_TO_LFRAC( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
/* Get the remote receive timestamp */
	tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
	tv2.tv_frac = NFRAC_TO_LFRAC( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
/* Get the remote transmit timestamp */
	tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
	tv3.tv_frac = NFRAC_TO_LFRAC( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
/* Calculate the time offset */
	offset_sec  = (long)(tv2.tv_sec - tv1.tv_sec) + (long)(tv3.tv_sec - (tv4.tv_sec + EPOCH_DIFF_JAN1970));
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
/* If the residual is larger than one second, directly adjust the time base! */
	if ( offset_sec )
		_SoftTimeBase += offset_sec;
/* Otherwise keep the adjustment in residual */
	if ( offset_frac ) {
	/* Disable the ISR before copy to the TimeResidualFrac */
		_asm {
			mov ax, word ptr offset_frac
			mov dx, word ptr offset_frac+2
			cli
			mov word ptr TimeResidualFrac, ax
			mov word ptr TimeResidualFrac+2, dx
			sti
		}
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
		/* Get the frequency different, if it is zero we should check reduce the interval to half */
			if ( !(compensate[0] = compensate[1] / (long)(1 << (int)PollIntervalPow)) )
				compensate[0] = compensate[1] / (long)(1 << (int)(PollIntervalPow - 1));
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
				if ( labs(compensate[1]) <= (long)(1 << (int)PollIntervalPow) ) {
					if ( PollIntervalPow < MAX_INTERVAL_POW )
						++PollIntervalPow;
				}
				else if ( labs(compensate[1]) > (long)(1 << (int)PollIntervalPow) || labs(compensate[1]) > FIVE_MSEC_FRACTION ) {
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
/* Change the time base */
	_asm {
		xor ax, ax
		xchg ax, _SoftTimeSec
		add word ptr _SoftTimeBase, ax
		adc word ptr _SoftTimeBase+2, 0
	}

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
 * @brief Get the compensate average
 *
 * @param compensate
 * @return long
 */
static long get_compensate_avg( long compensate[COMPENSATE_CANDIDATE_NUM] )
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
