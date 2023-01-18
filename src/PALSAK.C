/**
 * @file PALSAK.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2022-12-22
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"
#include "./include/FTP.h"
#include "./include/FILE.h"
#include "./include/LEDINFO.h"

/* Main socket */
static volatile int SockRecv = -1;
static volatile int SockSend = -1;
/* Global address info, expecially for transmitting command */
static struct sockaddr_in TransmitAddr;
/* Command buffer */
static char CommBuffer[COMMBUF_SIZE];
/* Input buffer */
static char RecvBuffer[RECVBUF_SIZE];
/* Output buffer */
static char PreBuffer[PREBUF_SIZE];
/* Workflow flag */
static uchar WorkflowFlag = WORKFLOW_1;
/* */
static char  FTPHost[24] = { 0 };
static uint  FTPPort     = 21;
static char  FTPUser[24] = { 0 };
static char  FTPPass[24] = { 0 };
static char  FTPPath[32] = { 0 };

/* */
static int  InitControlSocket( const char * );
static int  InitDHCP( const uint );
static void SwitchWorkflow( const uint );

static int TransmitCommand( const char * );
static int TransmitDataByCommand( const char *, int );
static void ForceFlushSocket( int );
static char *ExtractResponse( char *, const uint );

static int GetPalertMac( const uint );
static int GetPalertNetworkSetting( const uint );
static int SetPalertNetwork( const uint );
static int CheckPalertDisk( const int, const uint );
static int UploadPalertFirmware( const uint );
static int AgentCommand( const char *, const uint );

static int UploadFileData( const int, const FILE_DATA far * );

static int CheckFirmwareVer( char *, const uint );
static int DownloadFirmware( const char * );

static int ReadFileFTPInfo( const FILE_DATA far * );
static int ReadFileBlockZero( const FILE_DATA far *, BYTE far *, size_t );

static int ConvertMask( ulong );

/* Main function, entry */
void main( void )
{
/* Initialization for u7186EX's general library */
	InitLib();
	Init5DigitLed();
/* Initialization for network interface library */
	if ( NetStart() < 0 )
		return;
/* Wait for the network interface ready, it might be shorter */
	YIELD();
	Delay(5);
/* Wait until the network connection is on, each func will wait for around 5 sec.(0.312 * 16)*/
	SwitchWorkflow( 312 );

/* If it shows the UPD flag (Workflow 0), just return after finishing  */
	if ( WorkflowFlag & STRATEGY_UPD_FW ) {
	/* */
		if ( bUseDhcp && InitDHCP( 400 ) == ERROR )
			goto err_return;
	/* */
		if ( ReadFileFTPInfo( GetFileInfoByName_AB(DISKA, "ftp_info.ini") ) == ERROR )
			goto err_return;
	/* */
		if ( CheckFirmwareVer( PreBuffer, 2000 ) == ERROR || (strlen(PreBuffer) && DownloadFirmware( PreBuffer ) == ERROR) )
			goto err_return;
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
		goto normal_return;
	}

/* Initialization for broadcasting network */
	if ( InitControlSocket( NULL ) == ERROR )
		goto err_return;
/*
 * Checking the segment of palert disk. If the result is not consisten with expectation,
 * then resetting the segment of palert disk
 */
	if ( CheckPalertDisk( CHECK, 1000 ) == ERROR ) {
	/* Show the ERROR result on the 7-seg led */
		SHOW_ERROR_5DIGITLED();
		Delay(1000);
	/* If the resetting result is still error, just give it up */
		if ( CheckPalertDisk( RESET, 1000 ) == ERROR )
			goto err_return;
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
	/* Since we reset the disk, we should upload the firmware again */
		WorkflowFlag |= STRATEGY_UPL_FW;
	}
	else {
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
	}
/*
 * Fetching the network setting of palert & save it.
 */
	if ( WorkflowFlag & STRATEGY_GET_NET ) {
		if ( GetPalertNetworkSetting( 1000 ) == ERROR )
			goto err_return;
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
	}
/*
 * Otherwise, after resetting disk, we need to upload firmware.
 * Or user force to upload firmware by connecting the ethernet
 * wire within second stage.
 */
	if ( WorkflowFlag & STRATEGY_UPL_FW ) {
	/* Start to upload firmware & batch file */
		if ( UploadPalertFirmware( 2000 ) == ERROR )
			goto err_return;
	/* Show the Good result on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
	}
/* */
	if ( WorkflowFlag & STRATEGY_SET_NET ) {
	/* Set the network of the Palert with saved setting */
		if ( SetPalertNetwork( 400 ) == ERROR )
			goto err_return;
	/* Show the Good result on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay(1000);
	}
/* */
	if ( WorkflowFlag & STRATEGY_SET_EEP ) {
		if ( UploadFileData( DISK_RAM, GetFileInfoByName_AB(DISKA, "AGENT.EXE") ) )
			goto err_return;
		if ( AgentCommand( "setdef", 2000 ) == ERROR )
			goto err_return;
	/* */
		ForceFlushSocket( SockRecv );
	}
/* */
	if ( WorkflowFlag & STRATEGY_CHK_EEP ) {
		if ( UploadFileData( DISK_RAM, GetFileInfoByName_AB(DISKA, "AGENT.EXE") ) )
			goto err_return;
		if ( AgentCommand( "check", 2000 ) == ERROR )
			goto err_return;
	/* */
		ForceFlushSocket( SockRecv );
	}
/* If it shows the MAC flag, just get the MAC and show it */
	if ( WorkflowFlag & STRATEGY_CHK_MAC ) {
	/* Show the MAC of the Palert */
		if ( GetPalertMac( 2000 ) == ERROR )
			goto err_return;
	}

normal_return:
/* Close the sockets */
	closesocket(SockSend);
	closesocket(SockRecv);
/* Terminate the network interface */
	Nterm();
	return;
err_return:
/* Show the 'ERROR' on the 7-seg led */
	SHOW_ERROR_5DIGITLED();
	Delay(2000);
	goto normal_return;
}

/**
 * @brief The initialization process of control socket.
 *
 * @param dotted
 * @return int
 * @retval 0 All of the socket we need are created.
 * @retval < 0 Something wrong when creating socket or setting up the operation mode.
 */
static int InitControlSocket( const char *dotted )
{
	char optval = 1;
	struct sockaddr_in _addr;

/* Close the previous sockets for following process */
	closesocket(SockSend);
	closesocket(SockRecv);
/* Wait for the network interface ready, it might be shorter */
	YIELD();
	Delay(5);
/* External variables for broadcast setting: Setup for accepting broadcast packet */
	bAcceptBroadcast = 1;

/* Create the sending socket */
	if ( (SockSend = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		return ERROR;
/* Set the socket to reuse the address */
	if ( setsockopt(SockSend, SOL_SOCKET, SO_DONTROUTE, &optval, sizeof(optval)) < 0 )
		return ERROR;
/* Set the broadcast ability */
	if ( setsockopt(SockSend, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0 )
		return ERROR;

/* Create the receiving socket */
	SockRecv = dotted != NULL ? SockSend : socket(PF_INET, SOCK_DGRAM, 0);
	if ( SockRecv < 0 )
		return ERROR;

/* Set the socket to reuse the address */
	if ( setsockopt(SockRecv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 )
		return ERROR;
/* Bind the receiving socket to the port number 54321 */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	_addr.sin_port = htons(dotted != NULL ? 12345 : LISTEN_PORT);
	if ( bind(SockRecv, (struct sockaddr *)&_addr, sizeof(struct sockaddr)) < 0 )
		return ERROR;
/* Set the timeout of receiving socket to 0.25 sec. */
	SOCKET_RXTOUT(SockRecv, 250);

/* Set the transmitting address info */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = dotted != NULL ? inet_addr((char *)dotted) : htonl(INADDR_BROADCAST);
	_addr.sin_port = htons(CONTROL_PORT);
	TransmitAddr = _addr;

	return NORMAL;
}

/**
 * @brief
 *
 * @param msec
 * @return int
 */
static int InitDHCP( const uint msec )
{
	int   i;
	uchar trycount = 0;
/* Two external struct but not listed in header file, therefore we should declare here */
	extern struct NETDATA *NetHost, *NetGateway;

/* Turn off the network interface for resetting */
	Nterm();
/* Using DHCP, we should set the IP, Mask and Gateway to 0 before initialization for network interface */
	*(long *)NetHost->Iaddr.c =
	*(long *)NetHost->Imask.c =
	*(long *)NetGateway->Iaddr.c = 0L;
	DhcpLeaseTime = 0L;
/* Call for linking DHCP library */
	Install_DHCP();
/* Re-initialization for network interface library */
	if ( NetStart() < 0 )
		return ERROR;
/* Wait for the network interface ready, it might be shoter */
	YIELD();
	Delay(5);
/* */
	do {
		YIELD();
		if ( nets[0].DHCPserver && DHCPget(0, DhcpLeaseTime) >= 0 ) {
		/* Show the fetched IP on the 7-seg led roller once */
			sprintf(
				PreBuffer, "%u.%u.%u.%u-%u  %u.%u.%u.%u  ",
				NetHost->Iaddr.c[0], NetHost->Iaddr.c[1], NetHost->Iaddr.c[2], NetHost->Iaddr.c[3],
				ConvertMask( *(long *)NetHost->Imask.c ),
				NetGateway->Iaddr.c[0], NetGateway->Iaddr.c[1], NetGateway->Iaddr.c[2], NetGateway->Iaddr.c[3]
			);
			EncodeAddrDisplayContent( PreBuffer );
		/* */
			for ( i = 0; i < ContentLength; i++ ) {
				ShowContent5DigitsLedRoller( i );
				Delay(msec);
			}

			return NORMAL;
		}
		Delay(msec);
	} while ( ++trycount < NETWORK_OPERATION_RETRY );

	return ERROR;
}

/**
 * @brief
 *
 */
static void SwitchWorkflow( const uint msec )
{
	uint num = 0;
	uint delay_msec = msec;

/* Fetch the saved IP from EEPROM */
	GetIp((uchar *)RecvBuffer);
	sprintf(PreBuffer, "%u.%u.%u.%u  ", (uchar)RecvBuffer[0], (uchar)RecvBuffer[1], (uchar)RecvBuffer[2], (uchar)RecvBuffer[3]);
	EncodeAddrDisplayContent( PreBuffer );
/* Show the "-0-" message on the 7-seg led */
	SHOW_2DASH_5DIGITLED( 0 );
/*
 * External variables for network linking status :
 * After testing, when network is real connected,
 * this number should be 0x40(64) or 0x01.
 */
	while ( bEthernetLinkOk == 0x00 ) {
	/* Detect the init. pin condition for switching to the updating firmware func. */
		if ( ReadInitPin() && WorkflowFlag != WORKFLOW_0 ) {
		/* */
			num = 0;
			delay_msec = msec;
			WorkflowFlag = WORKFLOW_0;
		}
	/* Increase the times of waiting network connection every 500 msec */
		if ( ++delay_msec >= msec ) {
		/* */
			if ( (num && !(num % 0x10)) || WorkflowFlag == WORKFLOW_0 ) {
			/* */
				switch ( WorkflowFlag ) {
				case WORKFLOW_1:
					WorkflowFlag = WORKFLOW_2;
					SHOW_2DASH_5DIGITLED( 1 );
					break;
				case WORKFLOW_2:
					WorkflowFlag = WORKFLOW_3;
					SHOW_2DASH_5DIGITLED( 2 );
					break;
				case WORKFLOW_3:
					WorkflowFlag = WORKFLOW_4;
					SHOW_2DASH_5DIGITLED( 3 );
					break;
				case WORKFLOW_4:
					WorkflowFlag = WORKFLOW_5;
					SHOW_2DASH_5DIGITLED( 4 );
					break;
				case WORKFLOW_5:
					WorkflowFlag = WORKFLOW_1;
					SHOW_2DASH_5DIGITLED( 0 );
					break;
				case WORKFLOW_0:
				/* After display saved IP address over two times, turn on DHCP function */
					if ( !bUseDhcp && num > (ContentLength << 1) ) {
						bUseDhcp = 1;
					/* Set the 'dHCP.  ' message to roller buffer */
						PreBuffer[0] = 0x3d;
						PreBuffer[1] = 0x37;
						PreBuffer[2] = 0x4e;
						PreBuffer[3] = 0xe7;
						PreBuffer[4] = 0x00;
						PreBuffer[5] = 0x00;
						SetDisplayContent( (BYTE *)PreBuffer, 6 );
					}
				/* No break here */
				default:
					break;
				}
			}
		/* 7-seg display part */
			if ( WorkflowFlag == WORKFLOW_0 )
			/* Show the IP address or 'dHCP.  ' on the 7-seg led roller */
				ShowContent5DigitsLedRoller( num );
			else
			/* Show the '-0-' to '-f-' message on the 7-seg led each loop */
				Show5DigitLed(3, num % 0x10);
		/* */
			num++;
			delay_msec = 0;
		}
	/* */
		Delay(1);
	}
/* One more waiting for stablization of the connection */
	Delay(msec);

	return;
}

/*
 *  TransmitCommand() - Transmitting control command.
 *  argument:
 *    comm - The pointer to the string of command.
 *  return:
 *    ERROR(-1) - It didn't receive the response properly.
 *    NORMAL(0) - It has been processed properly.
 */
static int TransmitCommand( const char *comm )
{
	int   ret = 0;
	uchar trycount = 0;

	struct sockaddr_in _addr;
	int fromlen = sizeof(struct sockaddr);

/* Show the '-S-' message on the 7-seg led */
	SHOW_2DASH_5DIGITLED( 0 );
	Show5DigitLed(3, 5);
/* Flush the receiving buffer from client, just in case */
	while ( recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE, 0, (struct sockaddr *)&_addr, &fromlen) > 0 );
/* Appending the '\r' to the input command */
	sprintf(CommBuffer, "%s\r", comm);
/* Transmitting the command to others */
	if ( sendto(SockSend, CommBuffer, strlen(CommBuffer), MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr)) <= 0 )
		return ERROR;
	Delay(250);

/* Flush the input buffer */
	memset(RecvBuffer, 0, RECVBUF_SIZE);
/* Show the '-L-' message on the 7-seg led */
	Show5DigitLedSeg(3, 0x0e);
	Delay(250);
/* Receiving the response from the palert */
	while ( (ret = recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE - 1, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
		if ( ++trycount >= NETWORK_OPERATION_RETRY )
			return ERROR;
	}
	RecvBuffer[ret] = '\0';

	return NORMAL;
}

/*
 *  TransmitDataByCommand() - Transmitting data bytes by command line.
 *  argument:
 *    data        - The pointer to the data beginning.
 *    data_length - The length of data bytes.
 *  return:
 *    ERROR(-1) - It didn't receive the response properly.
 *    NORMAL(0) - It has been received properly.
 */
static int TransmitDataByCommand( const char *data, int data_length )
{
	int   ret = 0;
	uchar trycount = 0;

	struct sockaddr_in _addr;
	int fromlen = sizeof(struct sockaddr);

/* Flush the receiving buffer from client, just in case */
	while ( SOCKET_HASDATA(SockRecv) )
		recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE, 0, (struct sockaddr *)&_addr, &fromlen);
/* Sending the data bytes by command line method */
	if ( sendto(SockSend, (char *)data, data_length, MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr)) <= 0 )
		return ERROR;

/* Flush the input buffer */
	memset(RecvBuffer, 0, RECVBUF_SIZE);
/* Receiving the response from the palert */
	while ( (ret = recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
		if ( ++trycount >= NETWORK_OPERATION_RETRY )
			return ERROR;
	}
	RecvBuffer[ret] = '\0';

	return NORMAL;
}

/**
 * @brief
 *
 * @param sock
 */
static void ForceFlushSocket( int sock )
{
	uchar i;
	int   fromlen = sizeof(struct sockaddr);
	struct sockaddr_in _addr;

/* Show 'FLUSH.' on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x0e);
	Show5DigitLedSeg(3, 0x3e);
	Show5DigitLed(4, 0x05);
	Show5DigitLedSeg(5, 0xb7);
/* Flush the receiving buffer from client, just in case */
	for ( i = 0; i < NETWORK_OPERATION_RETRY; i++ )
		while ( recvfrom(sock, RecvBuffer, RECVBUF_SIZE, MSG_OOB, (struct sockaddr *)&_addr, &fromlen) > 0 );

	return;
}

/*
 *  ExtractResponse() - Extract the real data from the whole response.
 *  argument:
 *    buffer - The pointer to the whole raw response.
 *    length - The length of the real data in string.
 *  return:
 *    NULL  - It didn't find out the data.
 *    !NULL - The pointer to the real data string.
 */
static char *ExtractResponse( char far *buffer, const uint length )
{
	char far *pos = NULL;

	if ( buffer != NULL ) {
	/* Find out where is the '=', and skip all the following space or tab. */
		pos = strchr(buffer, '=');
		if ( pos ) {
			for ( pos += 1; isspace(*pos); pos++ );
		/* Appending a null-terminator after the data. */
			for ( buffer = pos; *buffer && !isspace(*buffer); buffer++ );
			if ( buffer > (pos + length) )
				buffer = pos + length;
			*buffer = '\0';
		}
	}

	return pos;
}

/*
 *  GetPalertMac() - Get the MAC address of Palert(u7186EX) on the other
 *                   end of the ethernet cable. Then show it on the 7-seg
 *                   led.
 *  argument:
 *    msec - The waiting delay of display in msecond.
 *  return:
 *    NORMAL(0) - The MAC address of Palert has been requested successfully.
 *    ERROR(-1) - Something wrong when requesting.
 */
static int GetPalertMac( const uint msec )
{
	uint  page = 0;
	uint  delay_msec = msec;
	char *pos;

/* Send out the MAC address request command */
	while ( TransmitCommand( "mac" ) != NORMAL );
/* Extract the MAC address from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, MAC_STRING )) == NULL )
		return ERROR;

/* Parsing the MAC address with hex format into Display content buffer */
	EncodeAddrDisplayContent( pos );
/* Show on the 7-seg led */
	do {
		if ( ++delay_msec >= msec ) {
			ShowContent5DigitsLedPage( page++ );
			delay_msec = 0;
		}
		Delay(1);
	} while ( !ReadInitPin() );

	return NORMAL;
}

/**
 * @brief Get the Palert Network Setting & save to EEPROM block 2
 *
 * @param msec
 * @return int
 */
static int GetPalertNetworkSetting( const uint msec )
{
	char *pos;

/* Send out the IP address request command */
	while ( TransmitCommand( "ip" ) != NORMAL );
/* Extract the IP address from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the IP address to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[0], (uchar *)&PreBuffer[1], (uchar *)&PreBuffer[2], (uchar *)&PreBuffer[3]);

/* Send out the Mask request command */
	while ( TransmitCommand( "mask" ) != NORMAL );
/* Extract the Mask from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the Mask to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[4], (uchar *)&PreBuffer[5], (uchar *)&PreBuffer[6], (uchar *)&PreBuffer[7]);

/* Send out the Gateway address request command */
	while ( TransmitCommand( "gateway" ) != NORMAL );
/* Extract the Gateway address from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the Gateway address to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[8], (uchar *)&PreBuffer[9], (uchar *)&PreBuffer[10], (uchar *)&PreBuffer[11]);

/* */
	EE_WriteEnable();
	if ( EE_MultiWrite(EEPROM_NETWORK_SET_BLOCK, EEPROM_NETWORK_SET_ADDR, EEPROM_NETWORK_SET_LENGTH, PreBuffer) ) {
		EE_WriteProtect();
		return ERROR;
	}
	EE_WriteProtect();
/* Show 'F. nEt.' on the 7-seg led */
	Show5DigitLedWithDot(1, 0x0f);
	Show5DigitLedSeg(2, 0x00);
	Show5DigitLedSeg(3, 0x15);
	Show5DigitLed(4, 0x0e);
	Show5DigitLedSeg(5, 0x91);
	Delay(msec);

	return NORMAL;
}

/**
 * @brief Set the Palert Network
 *
 * @param msec
 * @return int
 */
static int SetPalertNetwork( const uint msec )
{
	uint  seq = 0;
	uint  delay_msec = msec;
	char *pos;
	char *str_ptr = PreBuffer + EEPROM_NETWORK_SET_LENGTH + 1;

/* Read from EEPROM block 2 where the saved network setting within */
	if ( !EE_MultiRead(EEPROM_NETWORK_SET_BLOCK, EEPROM_NETWORK_SET_ADDR, EEPROM_NETWORK_SET_LENGTH, PreBuffer) ) {
	/* Show 'U.PLUG.' on the 7-seg led */
		Show5DigitLedSeg(1, 0xbe);
		Show5DigitLedSeg(2, 0x67);
		Show5DigitLedSeg(3, 0x0e);
		Show5DigitLedSeg(4, 0x3e);
		Show5DigitLedSeg(5, 0xde);
	/* */
		while ( bEthernetLinkOk != 0x00 )
			Delay(1);
	/* Show the fetched IP on the 7-seg led roller once */
		sprintf(
			str_ptr, "%u.%u.%u.%u-%u  %u.%u.%u.%u  ",
			(BYTE)PreBuffer[0], (BYTE)PreBuffer[1], (BYTE)PreBuffer[2], (BYTE)PreBuffer[3],
			ConvertMask( *(long *)(PreBuffer + 4) ),
			(BYTE)PreBuffer[8], (BYTE)PreBuffer[9], (BYTE)PreBuffer[10], (BYTE)PreBuffer[11]
		);
		EncodeAddrDisplayContent( str_ptr );
	/* */
		while ( bEthernetLinkOk == 0x00 ) {
		/* */
			if ( ++delay_msec >= msec ) {
				ShowContent5DigitsLedRoller( seq++ );
				delay_msec = 0;
			}
		/* */
			if ( ReadInitPin() )
				return NORMAL;
			Delay(1);
		}
	/* Send out the IP address request command for following connection */
		while ( TransmitCommand( "ip" ) != NORMAL );
		Print("%d %s\n\r", __LINE__, RecvBuffer);
	/* Extract the IP address from the raw response & connect to it */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
		if ( InitControlSocket( pos ) == ERROR )
			return ERROR;
	/* */
		sprintf(str_ptr, "ip %u.%u.%u.%u", (BYTE)PreBuffer[0], (BYTE)PreBuffer[1], (BYTE)PreBuffer[2], (BYTE)PreBuffer[3]);
		while ( TransmitCommand( str_ptr ) != NORMAL );
		Print("%d %s\n\r", __LINE__, RecvBuffer);
	/* Show 'S. iP.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x05);
		Show5DigitLedSeg(2, 0x00);
		Show5DigitLedSeg(3, 0x04);
		Show5DigitLedSeg(4, 0xe7);
		Show5DigitLedSeg(5, 0x00);
		Delay(msec);
	/* Send out the IP address request command for rechecking */
		while ( TransmitCommand( "ip" ) != NORMAL );
		Print("%d %s\n\r", __LINE__, RecvBuffer);
	/* Extract the IP address from the raw response */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
	/* Parsing the IP address to bytes & compare it with storage data */
		sscanf(pos, "%hu.%hu.%hu.%hu", (BYTE *)&str_ptr[0], (BYTE *)&str_ptr[1], (BYTE *)&str_ptr[2], (BYTE *)&str_ptr[3]);
		if ( memcmp(&PreBuffer[0], &str_ptr[0], 4) )
			return ERROR;

	/* */
		sprintf(str_ptr, "mask %u.%u.%u.%u", (BYTE)PreBuffer[4], (BYTE)PreBuffer[5], (BYTE)PreBuffer[6], (BYTE)PreBuffer[7]);
		while ( TransmitCommand( str_ptr ) != NORMAL );
	/* Show 'S.MASk.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x05);
		Show5DigitLedSeg(2, 0x76);
		Show5DigitLed(3, 0x0a);
		Show5DigitLed(4, 0x05);
		Show5DigitLedSeg(5, 0xb7);
		Delay(msec);
	/* Send out the Mask request command for rechecking */
		while ( TransmitCommand( "mask" ) != NORMAL );
	/* Extract the Mask from the raw response */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
	/* Parsing the Mask to bytes & compare it with storage data */
		sscanf(pos, "%hu.%hu.%hu.%hu", (BYTE *)&str_ptr[0], (BYTE *)&str_ptr[1], (BYTE *)&str_ptr[2], (BYTE *)&str_ptr[3]);
		if ( memcmp(&PreBuffer[4], &str_ptr[0], 4) )
			return ERROR;

	/* */
		sprintf(str_ptr, "gateway %u.%u.%u.%u", (BYTE)PreBuffer[8], (BYTE)PreBuffer[9], (BYTE)PreBuffer[10], (BYTE)PreBuffer[11]);
		while ( TransmitCommand( str_ptr ) != NORMAL );
	/* Show 'S.GAtE.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x05);
		Show5DigitLedSeg(2, 0x5e);
		Show5DigitLed(3, 0x0a);
		Show5DigitLedSeg(4, 0x11);
		Show5DigitLedWithDot(5, 0x0e);
		Delay(msec);
	/* Send out the Gateway address request command */
		while ( TransmitCommand( "gateway" ) != NORMAL );
	/* Extract the Gateway address from the raw response */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
	/* Parsing the Gateway address to bytes & compare it with storage data */
		sscanf(pos, "%hu.%hu.%hu.%hu", (BYTE *)&str_ptr[0], (BYTE *)&str_ptr[1], (BYTE *)&str_ptr[2], (BYTE *)&str_ptr[3]);
		if ( memcmp(&PreBuffer[8], &str_ptr[0], 4) )
			return ERROR;
	/* */
		if ( InitControlSocket( NULL ) == ERROR )
			return ERROR;

		return NORMAL;
	}

	return ERROR;
}

/*
 *  CheckPalertDisk() - Checking the disk size of Palert on the other
 *                      end of the ethernet cable and show on the
 *                      7-seg led.
 *  argument:
 *    mode - There are two kinds of mode, Check(0) & Reset(1).
 *    msec - The waiting delay of display in msecond.
 *  return:
 *    NORMAL(0) - The disk size of the Palert is correct.
 *    ERROR(-1) - The disk size of the Palert is wrong or something
 *                happened when requesting.
 */
static int CheckPalertDisk( const int mode, const uint msec )
{
/*
 * It would be some problem when using seperated variables (for disk size) instead an array,
 * I think it might be caused by the near & far pointer difference.
 */
	char *pos;

/* Function switch between "reset" & "check", then end out the request command */
	while ( TransmitCommand( mode == RESET ? "disksize 3 4" : "disksize" ) != NORMAL );
/* Extract the DiskA size from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, DABSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[0]);
/* Show it on the 7-seg led */
	Show5DigitLed(1, PreBuffer[0]);
	Show5DigitLedSeg(2, 1);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING + 1;

/* Extract the DiskB size from the raw response */
	if ( (pos = ExtractResponse( pos, DABSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[1]);
/* Show it on the 7-seg led */
	Show5DigitLed(3, PreBuffer[1]);
	Show5DigitLedSeg(4, 1);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING + 1;

/* Extract the Reserved size from the raw response */
	if ( (pos = ExtractResponse( pos, RSVSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[2]);
/* Show it on the 7-seg led */
	Show5DigitLed(5, PreBuffer[2]);
/* Display for "msec" msec. */
	Delay(msec);

/* Check the size */
	if ( (uchar)PreBuffer[0] != DISKA_SIZE || (uchar)PreBuffer[1] != DISKB_SIZE || (uchar)PreBuffer[2] != RESERVE_SIZE )
		return ERROR;

	return NORMAL;
}

/*
 *  UploadPalertFirmware() - Upload the firmware of Palert store in disk b &
 *                           the auto execute batch file to the Palert on the
 *                           other end of the ethernet cable.
 *  argument:
 *    msec - The waiting delay of display in msecond.
 *  return:
 *    NORMAL(0) - The uploading process is successful.
 *    ERROR(-1) - Something happened when uploading.
 */
static int UploadPalertFirmware( const uint msec )
{
/* Show 'FLASH.' on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x0e);
	Show5DigitLed(3, 0x0a);
	Show5DigitLed(4, 0x05);
	Show5DigitLedSeg(5, 0xb7);
	Delay(msec);
/* Flushing the disk a */
	while ( TransmitCommand( "del /y" ) != NORMAL );
/* Show 'del. A' on the 7-seg led */
	Show5DigitLed(1, 0x0d);
	Show5DigitLed(2, 0x0e);
	Show5DigitLedSeg(3, 0x8e);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0a);
	Delay(msec);
/* Flushing the disk b */
	while ( TransmitCommand( "delb /y" ) != NORMAL);
/* Show 'del. b' on the 7-seg led */
	Show5DigitLed(1, 0x0d);
	Show5DigitLed(2, 0x0e);
	Show5DigitLedSeg(3, 0x8e);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0b);
	Delay(msec);

/* Start to upload the firmware */
	if ( UploadFileData( DISKA, GetFileInfoByNo_AB(DISKB, 0) ) )
		return ERROR;
/* Show 'Fin. F' on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x04);
	Show5DigitLedSeg(3, 0x95);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0f);
	Delay(msec);

/* Start to upload the auto batch file */
	if ( UploadFileData( DISKA, GetFileInfoByName_AB(DISKA, "autoexec.bat") ) )
		return ERROR;
/* Show 'Fin. b' on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x04);
	Show5DigitLedSeg(3, 0x95);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0b);
	Delay(msec);

	return NORMAL;
}

/**
 * @brief
 *
 * @param comm
 * @param msec
 * @return int
 */
static int AgentCommand( const char *comm, const uint msec )
{
	char *pos;
	char *data  = PreBuffer;
	char *_data = data + 32;
	uint  page;
	uchar i = 0;

/* Show 'F. b.0. ' on the 7-seg led */
	Show5DigitLedWithDot(1, 0x0f);
	Show5DigitLedSeg(2, 0x00);
	Show5DigitLedWithDot(3, 0x0b);
	Show5DigitLedWithDot(4, 0x00);
	Show5DigitLedSeg(5, 0x00);
/* */
	if ( ReadFileBlockZero( GetFileInfoByName_AB(DISKA, "block_0.ini"), (BYTE *)PreBuffer, PREBUF_SIZE ) == ERROR )
		return ERROR;
	Delay(250);
/* Execute the remote agent */
	while ( TransmitCommand( "runr" ) != NORMAL );
/* Send the Block zero data to the agent */
	do {
		if ( TransmitDataByCommand( PreBuffer, EEPROM_SET_TOTAL_LENGTH + 2 ) == NORMAL ) {
		/* Show the '-S-' message on the 7-seg led */
			if ( RecvBuffer[0] == ACK || RecvBuffer[0] == 0 ) {
				break;
			}
		/* If receiving "Not ack", retry three times */
			else if ( RecvBuffer[0] == NAK ) {
				if ( ++i < NETWORK_OPERATION_RETRY ) {
					Delay(250);
					continue;
				}
			}
		}
	/* Something wrong, goto error return */
		return ERROR;
	} while ( 1 );
/* Sending the command to remote agent */
	while ( TransmitCommand( comm ) != NORMAL );
	if ( !strncmp(comm, "setdef", 6) ) {
	/* Show 'S. def.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x05);
		Show5DigitLedSeg(2, 0x00);
		Show5DigitLed(3, 0x0d);
		Show5DigitLed(4, 0x0e);
		Show5DigitLedWithDot(5, 0x0f);
	}
	else if ( !strncmp(comm, "check", 5) ) {
	/* Show 'C. Con.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x0c);
		Show5DigitLedSeg(2, 0x00);
		Show5DigitLed(3, 0x0c);
		Show5DigitLedSeg(4, 0x1d);
		Show5DigitLedSeg(5, 0x95);
	}
	else {
	/* Unknown command */
		return ERROR;
	}
/* */
	Delay(250);

/* Extract the remote palert serial from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, PSERIAL_STRING )) == NULL )
		return ERROR;
/* Show the serial on the 7-seg led */
	Show5DigitLed(1, pos[0] - '0');
	Show5DigitLed(2, pos[1] - '0');
	Show5DigitLed(3, pos[2] - '0');
	Show5DigitLed(4, pos[3] - '0');
	Show5DigitLedWithDot(5, pos[4] - '0');
	Delay(msec);
/* Compare the applied serial & factory serial */
	sscanf(pos, "%5s:%5s", data, _data);
	if ( strncmp(data, _data, 5) ) {
	/* Show 'S.diff.' on the 7-seg led */
		Show5DigitLedWithDot(1, 0x05);
		Show5DigitLed(2, 0x0d);
		Show5DigitLedSeg(3, 0x04);
		Show5DigitLed(4, 0x0f);
		Show5DigitLedWithDot(5, 0x0f);
		Delay(msec);
	}
/* Move the pointer to the next data */
	pos += PSERIAL_STRING + 1;

/* Extract the remote 1g & 0g corr value from the raw response */
	for ( i = 0; i < 2; i++ ) {
		if ( (pos = ExtractResponse( pos, CVALUE_STRING )) == NULL )
			return ERROR;
	/* Parse the value to strings for comparasion */
		sscanf(pos, "%17s:%17s", data, _data);
		if ( strncmp(data, _data, 17) ) {
		/* Show 'CX.dif.' on the 7-seg led */
			Show5DigitLed(1, 0x0c);
			Show5DigitLedWithDot(2, i == 0 ? 0x01 : 0x00);
			Show5DigitLed(3, 0x0d);
			Show5DigitLedSeg(4, 0x04);
			Show5DigitLedWithDot(5, 0x0f);
			Delay(msec);
		/* Parse the value to bytes & rearrange for display */
			sscanf(
				pos, "%2hx-%2hx-%2hx-%2hx-%2hx-%2hx:%2hx-%2hx-%2hx-%2hx-%2hx-%2hx",
				&data[0], &data[1], &data[2], &data[3], &data[4], &data[5],
				&data[6], &data[7], &data[8], &data[9], &data[10], &data[11]
			);
			sprintf(
				_data, "%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x%2.2x:%2.2x",
				(BYTE)data[0], (BYTE)data[6], (BYTE)data[1], (BYTE)data[7], (BYTE)data[2], (BYTE)data[8],
				(BYTE)data[3], (BYTE)data[9], (BYTE)data[4], (BYTE)data[10], (BYTE)data[5], (BYTE)data[11]
			);
		/* */
			EncodeAddrDisplayContent( _data );
		/* Real show on the 7-seg led */
			page = 0;
			do {
				ShowContent5DigitsLedPage( page++ );
				Delay(msec);
			} while ( page < ContentPages );
		}
	/* Move the pointer to the next data */
		pos += CVALUE_STRING + 1;
	}

	return NORMAL;
}

/*
 *  UploadFileData() - Parsing the file that pointed by the pointer and
 *                     uploading to the Palert on the other end of ethernet
 *                     wire by command line method.
 *  argument:
 *    disk    - The uploadind target disk.
 *    fileptr - The waiting delay of display in msecond.
 *  return:
 *    NORMAL(0) - The uploading process is successful.
 *    ERROR(-1) - Something happened when uploading or file is not existed.
 */
static int UploadFileData( const int disk, const FILE_DATA far *fileptr )
{
	uint  tmp;
	uint  block;
	uint  blockall;
	ulong addrindex = 0;
	BYTE *out_ptr = (BYTE *)PreBuffer;

/* Checking this opened file is file or not, and check the size of this file */
	if ( fileptr == NULL || fileptr->mark != 0x7188 || fileptr->size <= 0 )
		return ERROR;
/* Initialize the CRC16 table for following CRC16 computation */
	if ( CRC16_MakeTable() )
		return ERROR;
/* Send out the uploading request command */
	while ( TransmitCommand( disk == DISKA ? "load" : disk == DISKB ? "loadb" : "loadr" ) != NORMAL );
/* Start to show the progress and waiting for 150 ms */
	ShowProg5DigitsLed( 0, blockall );
	Delay(150);
/* Setting the output buffer to zero first */
	memset(out_ptr, 0, 260);
/*
 * Go through all blocks:
 * And compute the total block number, each block should be 256 bytes
 */
	for ( block = 0, blockall = ((fileptr->size + 255) >> 8) + 1; block < blockall; block++ ) {
	/* Other blocks except for first one */
		if ( block ) {
		/*
		 * The header(2 bytes) of other blocks(except for the last block) should be 00 & 01 (in decimal),
		 * and the last block should be sizelo & 00 (in decimal)
		 */
			tmp = fileptr->size & 0xff;
			out_ptr[0] = block == (blockall - 1) ? tmp : 0;
			out_ptr[1] = out_ptr[0] ? 0 : 1;
		/* Just copy all file data in binary byte by byte, one block should consist 256 bytes */
			memcpy(out_ptr + 2, AddFarPtrLong(fileptr->addr, addrindex), out_ptr[0] ? tmp : 256);
			addrindex += 256;
		}
	/* First block */
		else {
		/* The header(2 bytes) of the first block should be 29 & 00 (in decimal)*/
			out_ptr[0] = 29;
			/* out_ptr[1] = 0; */
		/* Following is name of the file and the length is limited under 12 bytes */
			memcpy(&out_ptr[2], fileptr->fname, 12);
		/* The file size part, the high part should be seperate into high & low again */
			/* out_ptr[14] = 0; */
			out_ptr[15] = fileptr->size & 0xff;
			tmp = (fileptr->size >> 8) & 0xffff;
			out_ptr[16] = tmp & 0xff;
			out_ptr[17] = tmp >> 8;
		/* The date information of the file */
			/* out_ptr[18] = 0; */
			out_ptr[19] = fileptr->year;
			/* out_ptr[20] = 0; */
			out_ptr[21] = fileptr->month;
			/* out_ptr[22] = 0; */
			out_ptr[23] = fileptr->day;
			/* out_ptr[24] = 0; */
			out_ptr[25] = fileptr->hour;
			/* out_ptr[26] = 0; */
			out_ptr[27] = fileptr->minute;
			/* out_ptr[28] = 0; */
			out_ptr[29] = fileptr->sec;
		/* All other bytes are zero... */
		}
	/* CRC16 computing part, using build-in function */
		CRC16_Reset();
		CRC16_AddDataN(out_ptr, 258);
		tmp = CRC16_Read();
		out_ptr[258] = tmp >> 8;
		out_ptr[259] = tmp & 0xff;
	/* Sending by the command line method */
		tmp = 0;
		while ( 1 ) {
			if ( TransmitDataByCommand( (char *)out_ptr, 260 ) == NORMAL ) {
				if ( RecvBuffer[0] == ACK || RecvBuffer[0] == 0 ) {
					break;
				}
			/* If receiving "Not ack", retry three times */
				else if ( RecvBuffer[0] == NAK ) {
					if ( ++tmp < NETWORK_OPERATION_RETRY ) {
						Delay(250);
						continue;
					}
				}
			}
		/* Sending the carrier return then return error */
			while ( TransmitCommand( "" ) != NORMAL );
			return ERROR;
		}
	/* Show the progress on the 7-seg led */
		ShowProg5DigitsLed( block + 1, blockall );
	}
/* For finishing, last for 100 ms */
	Delay(150);
/* Sending the carrier return */
	while ( TransmitCommand( "" ) != NORMAL );

	return NORMAL;
}

/**
 * @brief
 *
 * @param new_name
 * @param msec
 * @return int
 */
static int CheckFirmwareVer( char *new_name, const uint msec )
{
	char  result = ERROR;
	char *rname  = NULL;

/* */
	new_name[0] = '\0';
	if ( FTPConnect( FTPHost, FTPPort, FTPUser, FTPPass ) == FTP_SUCCESS ) {
		if ( FTPListDir( FTPPath, "plt*.exe", RecvBuffer, RECVBUF_SIZE ) == FTP_SUCCESS ) {
		/* Here, we can access the FTP server, therefore the return should be normal at lease */
			result = NORMAL;
			if ( GetFileName_AB(DISKB, 0, new_name) < 0 ) {
			/* Show '00000' on the 7-seg led */
				Show5DigitLed(1, 0);
				Show5DigitLed(2, 0);
				Show5DigitLed(3, 0);
				Show5DigitLed(4, 0);
				Show5DigitLed(5, 0);
			}
			else {
			/* Show existed version number on the 7-seg led */
				Show5DigitLed(1, new_name[3] - '0');
				Show5DigitLed(2, new_name[4] - '0');
				Show5DigitLed(3, new_name[5] - '0');
				Show5DigitLed(4, new_name[6] - '0');
				Show5DigitLed(5, new_name[7] - '0');
			}
			Delay(msec);
		/* Scan the list to find the firmware newer than we have */
			for ( rname = strtok(RecvBuffer, "\r\n"); rname; rname = strtok(NULL, "\r\n") ) {
				if ( !strlen(new_name) || strncmp(rname, new_name, 8) > 0 ) {
				/* We got a candidate, turn the return to larger than zero */
					memcpy(new_name, rname, 12);
					result = 1;
				}
			}
		}
	}
/* If we got a candidate, then show it on the 7-seg led */
	if ( result > 0 ) {
		Show5DigitLedSeg(1, 0x00);
		Show5DigitLedSeg(2, 0x11);
		Show5DigitLedSeg(3, 0x9d);
		Show5DigitLedSeg(4, 0x00);
		Show5DigitLedSeg(5, 0x00);
		Delay(msec);
	/* Show new version number on the 7-seg led */
		Show5DigitLed(1, new_name[3] - '0');
		Show5DigitLed(2, new_name[4] - '0');
		Show5DigitLed(3, new_name[5] - '0');
		Show5DigitLed(4, new_name[6] - '0');
		Show5DigitLed(5, new_name[7] - '0');
		Delay(msec);
	/* */
		result = NORMAL;
	}
/* Otherwise, flush the filename buffer & close the connection */
	else {
		new_name[0] = '\0';
		FTPClose();
	}

	return result;
}

/**
 * @brief
 *
 * @param target_name
 * @return int
 */
static int DownloadFirmware( const char *target_name )
{
	char result = ERROR;

/* First, check the target_name is not null */
	if ( target_name != NULL && strlen(target_name) ) {
		if ( GetFileNo_AB(DISKB) )
			OS7_DeleteAllFile(DISKB);
		if ( FTPRetrFile( FTPPath, target_name, target_name, DISKB ) == FTP_SUCCESS )
			result = NORMAL;
	}
/* Just close the connection */
	FTPClose();

	return result;
}

/**
 * @brief
 *
 * @param fileptr
 * @return int
 */
static int ReadFileFTPInfo( const FILE_DATA far *fileptr )
{
/* */
	if ( fileptr != NULL ) {
		GetFileStr( fileptr, "REMOTE_FTP_HOST", "127.0.0.1", FTPHost, sizeof(FTPHost) );
		GetFileStr( fileptr, "REMOTE_FTP_PORT", "21", FTPUser, sizeof(FTPUser) );
		FTPPort = atoi(FTPUser);
		GetFileStr( fileptr, "REMOTE_FTP_USER", "USER", FTPUser, sizeof(FTPUser) );
		GetFileStr( fileptr, "REMOTE_FTP_PASS", "PASS", FTPPass, sizeof(FTPPass) );
		GetFileStr( fileptr, "REMOTE_FTP_PATH", "/home", FTPPath, sizeof(FTPPath) );

		return NORMAL;
	}

	return ERROR;
}

/**
 * @brief
 *
 * @param fileptr
 * @param dest
 * @param dest_size
 * @return int
 */
static int ReadFileBlockZero( const FILE_DATA far *fileptr, BYTE far *dest, size_t dest_size )
{
	ulong scan_pos = 0;
	BYTE far * const endptr = dest + EEPROM_SET_TOTAL_LENGTH;

/* */
	if ( dest_size < (EEPROM_SET_TOTAL_LENGTH + 2) )
		return ERROR;
/* */
	memset(dest, 0xff, EEPROM_SET_TOTAL_LENGTH);
/* */
	if ( fileptr != NULL && !CRC16_MakeTable() ) {
		CRC16_Reset();
		dest_size = 0;
		while ( scan_pos < fileptr->size && dest < endptr ) {
		/* To find the 1st '*' from scan_pos */
			if ( !(scan_pos = FileSeek( fileptr, '*', 1, scan_pos )) )
				break;  /* Cannot find '*' */
		/* */
			if (
				sscanf(
					(const char far *)AddFarPtrLong(fileptr->addr, scan_pos),
					"%hi %hi %hi %hi %hi %hi %hi %hi",
					dest, dest + 1, dest + 2, dest + 3, dest + 4, dest + 5, dest + 6, dest + 7
				) == EEPROM_BYTE_PER_LINE
			) {
			/* */
				dest += EEPROM_BYTE_PER_LINE;
				dest_size += EEPROM_BYTE_PER_LINE;
			}
		}
	/* */
		if ( dest_size ) {
			dest -= dest_size;
			CRC16_AddDataN(dest, EEPROM_SET_TOTAL_LENGTH);
			scan_pos = CRC16_Read();
			dest[EEPROM_SET_TOTAL_LENGTH] = (scan_pos >> 8) & 0xff;
			dest[EEPROM_SET_TOTAL_LENGTH + 1] = scan_pos & 0xff;

			return NORMAL;
		}
	}

	return ERROR;
}

/**
 * @brief
 *
 * @param mask
 * @return int
 */
static int ConvertMask( ulong mask )
{
	uchar result = 0;

	while ( mask ) {
		mask &= mask - 1;
		result++;
	}

	return result;
}
