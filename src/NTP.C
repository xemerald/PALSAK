
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"
#include "./include/NTP.h"

/*
 * Byte order conversions
 */
#define HTONS_FP(__X) (htonl((__X)))
#define NTOHS_FP(__X) (ntohl((__X)))
/*
 *
 */
#define ONE_EPOCH_USEC  1000000L
#define HALF_EPOCH_USEC  500000L
#define WRITERTC_COUNTDOWN_CHANNEL  7
/*
 *
 */
static time_t FetchHWTime( void );
static void   SetHWTime( time_t, ulong );
static time_t _mktime( uint, uint, uint, uint, uint, uint );
static ulong  frac2usec( ulong );
static ulong  usec2frac( ulong );

/* */
#define INTERNAL_BUF_SIZE  128
static char InternalBuffer[INTERNAL_BUF_SIZE];
/*
 * Time returns the time since the Epoch (00:00:00 UTC, January 1, 1970),
 * measured in seconds. If t is non-NULL, the return value is also stored
 * in the memory pointed to by t .
 */
static const ulong EpochDiff_Jan1970 = 86400UL * (365UL * 70 + 17);
/* */
static volatile int MainSock = -1;
static struct sockaddr_in TransmitAddr;
/* */
static struct timeval SoftSysTime;
static struct timeval TimeResidual;
static BYTE           WriteToRTC;
static TIME_DATE      TimeDateSetting;


/**
 * @brief
 *
 * @param timezone
 */
void SysTimeInit( const int timezone )
{
/* */
	SoftSysTime.tv_sec   = FetchHWTime() - (time_t)(timezone * 3600);
	SoftSysTime.tv_usec  = 100000L;
	TimeResidual.tv_sec  = 0L;
	TimeResidual.tv_usec = 0L;
	WriteToRTC           = 0;

	return;
}

/**
 * @brief
 *
 * @param step_usec
 */
void SysTimeService( const long step_usec )
{
	static ulong count_compensate = 0;
/* */
	ulong tmp;
	long  adjs = step_usec;

/* */
	if ( WriteToRTC ) {
		CountDownTimerReadValue(WRITERTC_COUNTDOWN_CHANNEL, &tmp);
		if ( tmp == 0 ) {
			SetTimeDate(&TimeDateSetting);
			TimerClose();
			WriteToRTC = 0;
		}
	}

/* If the residual is larger than one second, directly adjust it! */
	if ( TimeResidual.tv_sec ) {
		SoftSysTime.tv_sec  += TimeResidual.tv_sec;
		SoftSysTime.tv_usec += TimeResidual.tv_usec;
		TimeResidual.tv_sec  = 0L;
		TimeResidual.tv_usec = 0L;
	}
/* If the residual only in msecond or usecond, step or slew it! */
	if ( TimeResidual.tv_usec ) {
	/* */
		adjs = labs(adjs);
		if ( (adjs /= 2) == 0 )
			adjs = 1;
	/* */
		if ( TimeResidual.tv_usec < 0 )
			adjs = -adjs;
	/* */
		if ( labs(TimeResidual.tv_usec) <= labs(adjs) )
			adjs = TimeResidual.tv_usec;
	/* */
		TimeResidual.tv_usec -= adjs;
		adjs += step_usec;
	}
/* Keep the clock step forward */
	if ( adjs )
		SoftSysTime.tv_usec += adjs;
/* */
	if ( SoftSysTime.tv_usec >= ONE_EPOCH_USEC ) {
		SoftSysTime.tv_sec++;
		SoftSysTime.tv_usec -= ONE_EPOCH_USEC;
	}
	else if ( SoftSysTime.tv_usec < 0 ) {
		SoftSysTime.tv_sec--;
		SoftSysTime.tv_usec += ONE_EPOCH_USEC;
	}

	return;
}

/**
 * @brief
 *
 * @param sys_time
 */
void SysTimeGet( struct timeval *sys_time )
{
	_asm cli
	*sys_time = SoftSysTime;
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

	_asm cli
	now_time = SoftSysTime;
	_asm sti
/* */
	SetHWTime( now_time.tv_sec + timezone * 3600, now_time.tv_usec );

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
/* Create the UDP socket */
	if ( (MainSock = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		return ERROR;

/* Addressing for master socket */
	memset(&TransmitAddr, 0, sizeof(TransmitAddr));
	TransmitAddr.sin_family = AF_INET;
	TransmitAddr.sin_addr.s_addr = inet_addr((char *)host);
	TransmitAddr.sin_port = htons(port);
/* Initialize the connection */
	if ( connect(MainSock, (struct sockaddr*)&TransmitAddr, sizeof(TransmitAddr)) < 0 ) {
	/* Close the opened socket */
		closesocket(MainSock);
		return ERROR;
	}

	return NORMAL;
}

/**
 * @brief
 *
 * @return int
 */
int NTPSend( void )
{
	struct timeval tv1;

/* Send to the server */
	memset(InternalBuffer, 0, 61);
/* 00 001 011 - leap, ntp ver, client.  See RFC 1361. */
	InternalBuffer[0] = (0 << 6) | (1 << 3) | 3;
/* Get the local sent time - Originate Timestamp */
	_asm cli
	tv1 = SoftSysTime;
	_asm sti
	*(ulong *)&InternalBuffer[40] = HTONS_FP( tv1.tv_sec + EpochDiff_Jan1970 );
	*(ulong *)&InternalBuffer[44] = HTONS_FP( usec2frac( tv1.tv_usec ) );
/* Send to the server */
	if ( send(MainSock, InternalBuffer, 48, 0) <= 0 )
		return ERROR;

	return NORMAL;
}

/**
 * @brief Synchronize the time.  See RFC 1361.
 *
 * @param timezone
 * @return int
 */
int NTPRecv( void )
{
	static long offset_avg = 0;
	static char recv_times = 0;
/* */
	struct timeval offset;
	struct timeval tv1, tv2, tv3, tv4;
	long offset_f;

/* Read from the server */
	if ( recv(MainSock, InternalBuffer, 60, 0) <= 0 ) {
		return ERROR;
	}
	else {
	/* Get the local received timestamp */
		_asm cli
		tv4 = SoftSysTime;
		_asm sti
		tv4.tv_sec += EpochDiff_Jan1970;
	/* Get the local transmitted timestamp */
		tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
		tv1.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
	/* Get the remote receive timestamp */
		tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
		tv2.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
	/* Get the remote transmit timestamp */
		tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
		tv3.tv_usec = frac2usec( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
	/* Calculate the time offset */
		offset.tv_sec  = (tv2.tv_sec - tv1.tv_sec) + (tv3.tv_sec - tv4.tv_sec);
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
				offset.tv_sec++;
				offset.tv_usec -= ONE_EPOCH_USEC;
			}
			else {
				offset.tv_sec--;
				offset.tv_usec += ONE_EPOCH_USEC;
			}
		}
	/* Set the time directly or keep the adjustment */
		Print("\r\nOffset is %ld %ld", offset.tv_sec, offset.tv_usec);
	/* Disable the ISR */
		_asm cli
		TimeResidual = offset;
		_asm sti
	/* */
		if ( recv_times ) {
			offset_f = offset.tv_usec + offset.tv_sec * ONE_EPOCH_USEC;
			offset_avg = offset_avg ? (offset_avg * 9 + offset_f) / 10 : offset_f;
		}
	/* */
		if ( recv_times > 10 ) {

		}
		else {
			recv_times++;
		}
		Print("\r\noffset_avg is %ld", offset_avg);
	}

	return NORMAL;
}

/**
 * @brief Get the Hardware System Time (unix timestamp)
 *
 * @return time_t
 */
static time_t FetchHWTime( void )
{
	TIME_DATE timedate;

/* */
	GetTimeDate(&timedate);

	return _mktime(
		timedate.year, timedate.month, timedate.day,
		timedate.hour, timedate.minute, timedate.sec
	);
}

/**
 * @brief Set the Hardware System Time
 *
 * @param sec
 * @param usec
 * @param timer
 */
static void SetHWTime( time_t sec, ulong usec )
{
	struct tm *brktime;

/* */
	++sec;
	usec = (ONE_EPOCH_USEC - usec) / 1000;
/* */
	brktime = gmtime( &sec );
	TimeDateSetting.year  = brktime->tm_year + 1900;
	TimeDateSetting.month = brktime->tm_mon + 1;
	TimeDateSetting.day   = brktime->tm_mday;
/* */
	TimeDateSetting.hour   = brktime->tm_hour;
	TimeDateSetting.minute = brktime->tm_min;
	TimeDateSetting.sec    = brktime->tm_sec;
/* */
	TimerOpen();
	CountDownTimerStart(WRITERTC_COUNTDOWN_CHANNEL, usec);
	WriteToRTC = 1;

	return;
}

/**
 * @brief turn the broken time structure into calendar time(UTC)
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
		year--;
	}

	return ((((time_t)(year / 4 - year / 100 + year / 400 + 367 * mon / 12 + day) + (ulong)year * 365 - 719499) * 24 + hour) * 60 + min) * 60 + sec;
}

/**
 * @brief Convert from fraction of a second to microsecond
 *
 * @param frac
 * @return ulong: microsecond.
 */
static ulong frac2usec( ulong frac )
{
	return ((((frac >> 16) & 0x0000ffff) * 15625) >> 10) + (((frac & 0x0000ffff) * 15625) >> 26) + (frac & 0x1);
}

/**
 * @brief
 *
 * @param usec it must smaller than 1 minion.
 * @return ulong
 */
static ulong usec2frac( ulong usec )
{
	return (((usec & 0x000fffff) << 12) / 15625 + 1) << 14;
}
