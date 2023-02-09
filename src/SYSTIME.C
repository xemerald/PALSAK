
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/SYSTIME.h"

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
/*
 * Time returns the time since the Epoch (00:00:00 UTC, January 1, 1970),
 * measured in seconds. If t is non-NULL, the return value is also stored
 * in the memory pointed to by t .
 */
static const ulong EpochDiff_Jan1970 = 86400UL * (365UL * 70 + 17);
/* */
static volatile int MainSock = -1;
/* */
static struct timeval _SoftSysTime;
const struct timeval far *SoftSysTime = &_SoftSysTime;
/* */
static struct timeval TimeResidual;
static uchar          PollIntervalPower;
static BYTE           WriteToRTC;
static BYTE           CompensateReady;
static long           CompensateUSec;
static TIME_DATE      TimeDateSetting;
/* */
static COUNTDOWNTIMER WriteRTCTimer;
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
	_SoftSysTime.tv_usec = 250000L;
	TimeResidual.tv_sec  = 0L;
	TimeResidual.tv_usec = 0L;
	PollIntervalPower    = MIN_INTERVAL_POWER;
	WriteToRTC           = 0;
	CompensateReady      = 0;
	CompensateUSec       = 0L;
/* */
	T_CountDownTimerStart(&WriteRTCTimer, 0);
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
	static long correct_timestep  = 0L;
	static long remain_compensate = 0L;
	static ulong count_one_epoch  = 0L;
/* */
	long adjs;

/* */
	if ( WriteToRTC ) {
		if ( T_CountDownTimerIsTimeUp(&WriteRTCTimer) ) {
			SetTimeDate(&TimeDateSetting);
			WriteToRTC = 0;
		}
	}
/* */
	if ( CompensateReady ) {
		if ( (count_one_epoch += ONE_CLOCK_STEP_USEC) >= ONE_EPOCH_USEC ) {
			adjs               = CompensateUSec / STEP_TIMES_IN_EPOCH;
			correct_timestep   = ONE_CLOCK_STEP_USEC + adjs;
			remain_compensate += CompensateUSec - STEP_TIMES_IN_EPOCH * adjs;
			count_one_epoch    = 0L;
		}
	}
	else if ( correct_timestep != ONE_CLOCK_STEP_USEC ) {
		correct_timestep = ONE_CLOCK_STEP_USEC;
	}

/* If the residual is larger than one second, directly adjust it! */
	if ( TimeResidual.tv_sec ) {
		_SoftSysTime.tv_sec  += TimeResidual.tv_sec;
		_SoftSysTime.tv_usec += TimeResidual.tv_usec;
		TimeResidual.tv_sec   = 0L;
		TimeResidual.tv_usec  = 0L;
	}
/* If the residual only in sub-second, step or slew it! */
	if ( TimeResidual.tv_usec ) {
	/* */
		adjs = ABS_HALF_CLOCK_STEP;
	/* */
		if ( labs(TimeResidual.tv_usec) <= adjs )
			adjs = TimeResidual.tv_usec;
		else if ( TimeResidual.tv_usec < 0 )
			adjs = -adjs;
	/* */
		TimeResidual.tv_usec -= adjs;
	}
	else {
		adjs = 0;
	}
/* */
	if ( remain_compensate ) {
		if ( remain_compensate < 0 ) {
			--adjs;
			++remain_compensate;
		}
		else {
			++adjs;
			--remain_compensate;
		}
	}
/* Keep the clock step forward */
	if ( (adjs += correct_timestep) )
		_SoftSysTime.tv_usec += adjs;
/* */
	if ( _SoftSysTime.tv_usec >= ONE_EPOCH_USEC ) {
		++_SoftSysTime.tv_sec;
		_SoftSysTime.tv_usec -= ONE_EPOCH_USEC;
	}
	else if ( _SoftSysTime.tv_usec < 0 ) {
		--_SoftSysTime.tv_sec;
		_SoftSysTime.tv_usec += ONE_EPOCH_USEC;
	}

	return;
}

/**
 * @brief
 *
 * @param sys_time
 */
void SysTimeGet( struct timeval far *sys_time )
{
	_asm cli
	*sys_time = _SoftSysTime;
	_asm sti

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
	_asm cli
	now_time = _SoftSysTime;
	_asm sti
/* */
	now_time.tv_sec += ((long)timezone * 3600) + 1;
/* Turn the usec to the msec to next second */
	now_time.tv_usec = (ONE_EPOCH_USEC - now_time.tv_usec) / 1000;
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
	T_CountDownTimerStart(&WriteRTCTimer, now_time.tv_usec);
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
 * @brief Synchronize the time.  See RFC 1361.
 *
 * @return int
 */
int NTPProcess( void )
{
	static long  compensate[COMPENSATE_CANDIDATE_NUM];
	static uchar ind_compensate = 0;
	static uchar first_time = 1;
/* */
	struct timeval offset;
	struct timeval tv1, tv2, tv3, tv4;

/* Check the processing interval */
	if ( !T_CountDownTimerIsTimeUp(&NTPProcessTimer) )
		return SYSTIME_SUCCESS;
/* 00 001 011 - leap, ntp ver, client.  See RFC 1361. */
	InternalBuffer[0] = (0 << 6) | (1 << 3) | 3;
/* Polling interval */
	InternalBuffer[2] = (char)PollIntervalPower;
/* Clock precision */
	InternalBuffer[3] = L_CLOCK_PRECISION;
/* Get the local sent time - Originate Timestamp */
	_asm cli
	tv1 = _SoftSysTime;
	_asm sti
/* */
	*(ulong *)&InternalBuffer[40] = HTONS_FP( tv1.tv_sec + EpochDiff_Jan1970 );
	*(ulong *)&InternalBuffer[44] = HTONS_FP( usec2frac( tv1.tv_usec ) );
/* Send to the server */
	if ( send(MainSock, InternalBuffer, 48, 0) <= 0 )
		return SYSTIME_ERROR;
/* Read from the server */
	if ( recv(MainSock, InternalBuffer, 60, 0) <= 0 )
		return SYSTIME_ERROR;
/* Get the local received timestamp */
	_asm cli
	tv4 = _SoftSysTime;
	_asm sti
/* Get the local transmitted timestamp */
	tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
	tv1.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
/* Get the remote receive timestamp */
	tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
	tv2.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
/* Get the remote transmit timestamp */
	tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
	tv3.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
/* Debug information */
	Print("\r\nTv2 %ld usec, Tv3 %ld usec.", tv2.tv_usec, tv3.tv_usec);
/* Calculate the time offset */
	offset.tv_sec  = (tv2.tv_sec - tv1.tv_sec) + (tv3.tv_sec - (tv4.tv_sec + EpochDiff_Jan1970));
	offset.tv_usec = ((tv2.tv_usec - tv1.tv_usec) + (tv3.tv_usec - tv4.tv_usec)) / 2;
	if ( offset.tv_sec & 0x1 ) {
		if ( offset.tv_sec < 0 )
			offset.tv_usec -= HALF_EPOCH_USEC;
		else
			offset.tv_usec += HALF_EPOCH_USEC;
	}
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
/* Set the time directly or keep the adjustment */
/* Disable the ISR */
	_asm cli
	TimeResidual = offset;
	_asm sti
/* */
	if ( !first_time ) {
	/* */
		compensate[ind_compensate++] = offset.tv_usec + offset.tv_sec * ONE_EPOCH_USEC;
		if ( ind_compensate >= COMPENSATE_CANDIDATE_NUM ) {
		/* */
			ind_compensate = 0;
			compensate[1] = get_compensate_avg( compensate );
			compensate[0] = compensate[1] / (long)(1 << (ulong)PollIntervalPower);
			compensate[2] = labs(CompensateUSec);
		/* */
			if ( CompensateReady && (labs(offset_f - CompensateUSec) > compensate[2] && compensate[2] > 100) ) {
				PollIntervalPower = MIN_INTERVAL_POWER;
				CompensateReady   = 0;
				CompensateUSec    = 0L;
				first_time        = 1;
			}
			else if ( CompensateReady ) {
			/* */
				if ( !compensate[0] )
					compensate[0] = compensate[1] / (long)(1 << (ulong)(PollIntervalPower - 1));
			/* */
				_asm cli
				CompensateUSec += compensate[0];
				_asm sti
			/* */
				compensate[0] = labs(compensate[0]);
				if ( compensate[0] < COMPENSATE_CANDIDATE_NUM * 2 ) {
					if ( PollIntervalPower < MAX_INTERVAL_POWER )
						++PollIntervalPower;
				}
				else if ( compensate[0] > COMPENSATE_CANDIDATE_NUM * 4 ) {
					if ( PollIntervalPower > MIN_INTERVAL_POWER )
						--PollIntervalPower;
				}
			}
			else {
				CompensateUSec  = offset_f;
				CompensateReady = 1;
			}
		}
	}
	else {
		first_time = 0;
	}
/* Debug information */
	Print("\r\nTime offset is %ld sec %ld usec.", offset.tv_sec, offset.tv_usec);
	Print("\r\nCompensate is %ld usec.", CompensateUSec);
	Print("\r\nPolling interval is %u sec.", 1 << PollIntervalPower);
/* */
	T_CountDownTimerStart(&NTPProcessTimer, 1000UL << (ulong)PollIntervalPower);

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
static ulong frac2usec( const ulong frac )
{
	return frac / ONE_USEC_FRAC + (frac % ONE_USEC_FRAC > HALF_USEC_FRAC);
}

/**
 * @brief Convert from microsecond to fraction of a second
 *
 * @param usec it must smaller than 1 minion.
 * @return ulong
 */
static ulong usec2frac( const ulong usec )
{
	return usec * ONE_USEC_FRAC;
}

/**
 * @brief Get the compensate average
 *
 * @param compensate
 * @return long
 */
static long get_compensate_avg( long compensate[] )
{
	int  i;
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
