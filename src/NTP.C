
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <dos.h>
#include <time.h>

/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"

/*
 * Convert from fraction of a second to microsecond
 * return: microsecond.
 */
#define FRAC_TO_USEC(__FRAC) \
		((long)(((long long)(__FRAC) * 1000000) >> 32))
#define USEC_TO_FRAC(__USEC) \
		((long)(((long long)(__USEC) << 32) / 1000000))
/*
 * Byte order conversions
 */
#define HTONS_FP(__X) (htonl((__X)))
#define NTOHS_FP(__X) (ntohl((__X)))

/*
 *
 */
static time_t FetchHWTime( void );
static void   SetHWTime( time_t );

/* */
#define INTERNAL_BUF_SIZE  1024
static char InternalBuffer[INTERNAL_BUF_SIZE];
/*
 * Time returns the time since the Epoch (00:00:00 UTC, January 1, 1970),
 * measured in seconds. If t is non-NULL, the return value is also stored
 * in the memory pointed to by t .
 */
static const ulong EpochDiff = (ulong)86400 * (365 * 70 + 17);
/* */
static volatile int MainSock = -1;
static struct sockaddr_in TransmitAddr;
/* */
static struct timeval SoftSysTime;
static long           Adjustment;

/**
 * @brief
 *
 * @return struct timeval*
 */
struct timeval *SysTimeInit( const int timezone )
{
/* */
	SoftSysTime.tv_sec  = FetchHWTime() - (timezone * 3600);
	SoftSysTime.tv_usec = 0L;
	Adjustment = 0;

	return &SoftSysTime;
}

/**
 * @brief
 *
 * @param usec
 * @return struct timeval*
 */
struct timeval *SysTimeStep( const long usec )
{
	long _usec = usec;

/* */
	if ( Adjustment ) {
	/* */
		_usec = labs(_usec);
		if ( (_usec /= 2) == 0 )
			_usec = 1;
	/* */
		if ( Adjustment < 0 )
			_usec = -_usec;
	/* */
		if ( labs(Adjustment) <= labs(_usec) )
			_usec = Adjustment;
	/* */
		Adjustment -= _usec;
		_usec += usec;
	}
/* */
	if ( _usec && (SoftSysTime.tv_usec += _usec) >= 1000000 ) {
		SoftSysTime.tv_sec  += SoftSysTime.tv_usec / 1000000;
		SoftSysTime.tv_usec %= 1000000;
	}

	return &SoftSysTime;
}

/**
 * @brief
 *
 * @return struct timeval
 */
void SysTimeGet( struct timeval *sys_time )
{
	_asm cli
	*sys_time = SoftSysTime
	_asm sti

	return;
}

/**
 * @brief
 *
 * @param timezone
 */
void SysTimeToHWTime( const int timezone )
{
/* */
	SetHWTime( SoftSysTime.tv_sec + (timezone * 3600) );

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
	tv1 = SoftSysTime;
	*(ulong *)&InternalBuffer[40] = HTONS_FP( tv1.tv_sec + EpochDiff );
	*(ulong *)&InternalBuffer[44] = HTONS_FP( USEC_TO_FRAC( tv1.tv_usec ) );
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
	long offset_usec;
	struct timeval tv1, tv2, tv3, tv4;

/* Read from the server */
	if ( recv(MainSock, InternalBuffer, 60, 0) <= 0 ) {
		return ERROR;
	}
	else {
	/* Get the local received timestamp */
		tv4 = SoftSysTime;
		tv4.tv_sec += EpochDiff;
	/* Get the local transmitted timestamp */
		tv1.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[24] );
		tv1.tv_usec = FRAC_TO_USEC( NTOHS_FP( *(ulong *)&InternalBuffer[28] ) );
	/* Get the remote receive timestamp */
		tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
		tv2.tv_usec = FRAC_TO_USEC( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
	/* Get the remote transmit timestamp */
		tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
		tv3.tv_usec = FRAC_TO_USEC( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
	/* Calculate the time offset */
		offset_usec  = (tv2.tv_usec - tv1.tv_usec) + (tv3.tv_usec - tv4.tv_usec);
		offset_usec += ((tv2.tv_sec - tv1.tv_sec) + (tv3.tv_sec - tv4.tv_sec)) * 1000000;
		offset_usec /= 2;
	/* Set the time directly or keep the adjustment */
		if ( labs(offset_usec) >= 1000000 ) {
			Adjustment = 0;
			SysTimeStep( offset_usec );
		}
		else {
			Adjustment = offset_usec;
		}
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
	struct time d_time;
	struct date d_date;
	TIME_DATE   timedate;

/* */
	GetTimeDate(&timedate);
/* Time part */
	d_time.ti_hour = timedate.hour;
	d_time.ti_hund = 0;
	d_time.ti_min  = timedate.minute;
	d_time.ti_sec  = timedate.sec;
/* Date part */
	d_date.da_year = timedate.year;
	d_date.da_mon  = timedate.month;
	d_date.da_day  = timedate.day;

	return dostounix(&d_date, &d_time);
}

/**
 * @brief Set the Hardware System Time
 *
 * @param val
 */
static void SetHWTime( time_t val )
{
	struct time d_time;
	struct date d_date;
	TIME_DATE   timedate;

/* */
	unixtodos(val, &d_date, &d_time);
/* */
	timedate.year   = d_date.da_year;
	timedate.month  = d_date.da_mon;
	timedate.day    = d_date.da_day;
/* */
	timedate.hour   = d_time.ti_hour;
	timedate.minute = d_time.ti_min;
	timedate.sec    = d_time.ti_sec;
/* */
	SetTimeDate(&timedate);

	return;
}
