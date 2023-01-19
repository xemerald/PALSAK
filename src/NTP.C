
#include<stdlib.h>
#include<stdio.h>
#include<ctype.h>
#include<dos.h>
#include<time.h>

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
/*
 * Byte order conversions
 */
#define HTONS_FP(__X) (htonl((__X)))
#define NTOHS_FP(__X) (ntohl((__X)))
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
static TIME_DATE SoftSysTime;
/*
 * Synchronize the time.  See RFC 1361.
 */
struct timeval tv1, tv4;

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
	int i;
	time_t tmp_time;

/* Send to the server */
	for (i = 0; i < 61; i++)
		InternalBuffer[i] = 0;
/* 00 001 011 - leap, ntp ver, client.  See RFC 1361. */
	InternalBuffer[0] = (0 << 6) | (1 << 3) | 3;
/* Get the local sent time - Originate Timestamp */
	tv1.tv_sec = FetchSystemTime() + EpochDiff; //(double)iH*60*60+iM*60+iS;
/* Send to the server */
	*(ulong *)&InternalBuffer[40] = HTONS_FP(tmp_time);
	*(ulong *)&InternalBuffer[44] = 0L;
	tv1.tv_usec = *TimeTicks;
/* Send to the server */
	if ( send(MainSock, InternalBuffer, 48, 0) <= 0 )
		return ERROR;

	return NORMAL;
}

/**
 * @brief
 *
 * @param timezone
 * @return int
 */
int NTPRecv( const int timezone )
{
	time_t tfinal;
	time_t offset_ti, offset_tf;
	struct timeval tv2, tv3;

/* Read from the server */
	if ( recv(MainSock, InternalBuffer, 60, 0) <= 0 ) {
		return ERROR;
	}
	else {
	/* Get the local received time */
		tv4.tv_usec = *TimeTicks;
		tv4.tv_sec  = FetchSystemTime() + EpochDiff;
	/* Get the remote Receive Timestamp */
		tv2.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[32] );
		tv2.tv_usec = FRAC_TO_USEC( NTOHS_FP( *(ulong *)&InternalBuffer[36] ) );
	/* Get the remote Transmit Timestamp */
		tv3.tv_sec  = NTOHS_FP( *(ulong *)&InternalBuffer[40] );
		tv3.tv_usec = FRAC_TO_USEC( NTOHS_FP( *(ulong *)&InternalBuffer[44] ) );
	/* Calculate the time offset */
		offset_ti = (tv2.tv_sec + tv3.tv_sec - tv1.tv_sec - tv4.tv_sec) >> 1;
		offset_tf = (tv2.tv_usec + tv3.tv_usec - tv1.tv_usec - tv4.tv_usec) >> 1;
	/* Calculate the new time */
		tfinal = tv4.tv_sec + offset_ti + (offset_tf / 1000000) + (4 * 3600) - EpochDiff + (timezone * 3600);
	/* Set the time */
		SetSystemTime( tfinal );
	}

	return NORMAL;
}

/**
 * @brief
 *
 * @return time_t
 */
static time_t FetchSystemTime( void )
{
	struct time d_time;
	struct date d_date;

/* */
	GetTimeDate(&SoftSysTime);
/* Time part */
	d_time.ti_hour = SoftSysTime.hour;
	d_time.ti_hund = 0;
	d_time.ti_min  = SoftSysTime.minute;
	d_time.ti_sec  = SoftSysTime.sec;
/* Date part */
	d_date.da_year = SoftSysTime.year;
	d_date.da_mon  = SoftSysTime.month;
	d_date.da_day  = SoftSysTime.day;

	return dostounix(&d_date, &d_time);
}

/**
 * @brief Set the System Time
 *
 * @param val
 */
static void SetSystemTime( time_t val )
{
	struct time d_time;
	struct date d_date;

/* */
	unixtodos(val, &d_date, &d_time);
/* */
	SoftSysTime.year   = d_date.da_year;
	SoftSysTime.month  = d_date.da_mon;
	SoftSysTime.day    = d_date.da_day;
/* */
	SoftSysTime.hour   = d_time.ti_hour;
	SoftSysTime.minute = d_time.ti_min;
	SoftSysTime.sec    = d_time.ti_sec;
/* */
	SetTimeDate(&SoftSysTime);

	return;
}
