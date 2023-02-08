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
#include "./include/SYSTIME.h"

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
static void SetNetworkConfig( uint );
static int  InitControlSocket( const char * );
static int  InitDHCP( const uint );
static void SwitchWorkflow( const uint );
static int  SwitchDHCPorStatic( const uint );

static int TransmitCommand( const char * );
static int TransmitDataByCommand( const char *, int );
static void ForceFlushSocket( int );
static char *ExtractResponse( char *, const uint );

static int GetPalertMac( const uint );
static int GetPalertNetworkSetting( const uint );
static int SetPalertNetwork( const uint );
static int CheckServerConnect( const uint );
static int CheckPalertDisk( const int, const uint );
static int UploadPalertFirmware( const uint );
static int AgentCommand( const char *, const uint );

static int UploadFileData( const int, const FILE_DATA far * );

static int CheckFirmwareVer( char *, const uint );
static int DownloadFirmware( const char * );

static int ReadFileFTPInfo( const FILE_DATA far * );
static int ReadFileBlockZero( const FILE_DATA far *, BYTE far *, size_t );

static void ParseNetinfoToRoller( char *, const BYTE [], const BYTE [], const BYTE [] );
static int  ConvertMask( ulong );
static int  ConnectTCP( const char *, uint );

static void TimerFunc( void );
static void FatalError( const int );
static int  ResetProgram( void );

#define LOOP_TRANSMIT_COMMAND(_COMM) \
		while ( TransmitCommand( (_COMM) ) != NORMAL && (!ReadInitPin() || ResetProgram()) )

/* Main function, entry */
void main( void )
{
/* Initialization for u7186EX's general library */
	InitLib();
	Init5DigitLed();
	SetNetworkConfig( NETWORK_DEFAULT );
/* Initialization for network interface library */
	if ( NetStart() < 0 )
		return;
/* Wait for the network interface ready, it might be shorter */
	YIELD();
	Delay2(5);
/* Wait until the network connection is on, each func will wait for around 5 sec.(0.312 * 16)*/
	SwitchWorkflow( 312 );

/* If it shows the UPD flag (Workflow 0), just return after finishing */
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
		Delay2(1000);
		goto normal_return;
	}

/* If it shows the CHK_CN flag, just return after finishing */
	if ( WorkflowFlag & STRATEGY_CHK_CN ) {
	/* */
		SetNetworkConfig( NETWORK_TEMPORARY );
	/* */
		if ( SwitchDHCPorStatic( 400 ) == ERROR )
			goto err_return;
	/* */
		if ( bUseDhcp && InitDHCP( 400 ) == ERROR )
			goto err_return;
	/* */
		if ( CheckServerConnect( 1000 ) == ERROR )
			goto err_return;

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
		Delay2(1000);
	/* If the resetting result is still error, just give it up */
		if ( CheckPalertDisk( RESET, 1000 ) == ERROR )
			goto err_return;
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay2(1000);
	/* Since we reset the disk, we should upload the firmware again */
		WorkflowFlag |= STRATEGY_UPL_FW;
	}
	else {
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay2(1000);
	}
/*
 * Fetching the network setting of palert & save it.
 */
	if ( WorkflowFlag & STRATEGY_GET_NET ) {
		if ( GetPalertNetworkSetting( 1000 ) == ERROR )
			goto err_return;
	/* Show the 'Good' on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay2(1000);
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
		Delay2(1000);
	}
/* */
	if ( WorkflowFlag & STRATEGY_SET_NET ) {
	/* Set the network of the Palert with saved setting */
		if ( SetPalertNetwork( 400 ) == ERROR )
			goto err_return;
	/* Show the Good result on the 7-seg led */
		SHOW_GOOD_5DIGITLED();
		Delay2(1000);
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
	Delay2(2000);
	goto normal_return;
}

/**
 * @brief Set the Network Config from EEPROM
 *
 */
static void SetNetworkConfig( uint set )
{
/* */
	if ( set == NETWORK_TEMPORARY )
		set = EEPROM_NETWORK_TMP_ADDR;
	else if ( set == NETWORK_DEFAULT )
		set = EEPROM_NETWORK_DEF_ADDR;
	else
		return;
/* */
	if ( !EE_MultiRead(EEPROM_NETWORK_SET_BLOCK, set, EEPROM_NETWORK_SET_LENGTH, PreBuffer) ) {
		SetIp((uchar *)&PreBuffer[0]);
		SetMask((uchar *)&PreBuffer[4]);
		SetGateway((uchar *)&PreBuffer[8]);
	}

	return;
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
/* */
	if ( dotted != NULL ) {
	/* Terminate the network interface first */
		Nterm();
	/* We should set the Mask to zero, let all the packet skip the routing table */
		_addr.sin_addr.s_addr = 0L;
		SetMask((uchar *)&_addr.sin_addr.s_addr);
	/* Initialization for network interface library */
		if ( NetStart() < 0 )
			return ERROR;
	}
/* Wait for the network interface ready, it might be shorter */
	YIELD();
	Delay2(5);
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
/* Bind the receiving socket to the port number 54321 or 12345 */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	_addr.sin_port = htons(dotted != NULL ? CONTROL_BIND_PORT : LISTEN_PORT);
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
	Delay2(5);
/* */
	do {
		YIELD();
		if ( nets[0].DHCPserver && DHCPget(0, DhcpLeaseTime) >= 0 ) {
		/* Show the fetched IP on the 7-seg led roller once */
			ParseNetinfoToRoller( PreBuffer, NetHost->Iaddr.c, NetHost->Imask.c, NetGateway->Iaddr.c );
		/* */
			for ( i = 0; i < ContentLength; i++ ) {
				ShowContent5DigitsLedRoller( i );
				Delay2(msec);
			}

			return NORMAL;
		}
		Delay2(msec);
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
		/* Fetch the saved IP information from EEPROM */
			GetIp((uchar *)&RecvBuffer[0]);
			GetMask((uchar *)&RecvBuffer[4]);
			GetGateway((uchar *)&RecvBuffer[8]);
		/* Show the saved IP information on the 7-seg led roller once */
			ParseNetinfoToRoller( PreBuffer, (BYTE *)&RecvBuffer[0], (BYTE *)&RecvBuffer[4], (BYTE *)&RecvBuffer[8] );
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
					WorkflowFlag = WORKFLOW_6;
					SHOW_2DASH_5DIGITLED( 5 );
					break;
				case WORKFLOW_6:
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
		Delay2(1);
	}
/* One more waiting for stablization of the connection */
	Delay2(msec);

	return;
}

/**
 * @brief
 *
 * @param msec
 * @return int
 */
static int SwitchDHCPorStatic( const uint msec )
{
	uint seq = 0;
	uint delay_msec = msec;

/* Show 'U.PLUG.' on the 7-seg led */
	ShowAll5DigitLedSeg( 0xbe, 0x67, 0x0e, 0x3e, 0xde );
/* Wait until ethernet unplug */
	while ( bEthernetLinkOk != 0x00 )
		Delay2(1);

/* Fetch the saved IP information from EEPROM */
	GetIp((uchar *)&RecvBuffer[0]);
	GetMask((uchar *)&RecvBuffer[4]);
	GetGateway((uchar *)&RecvBuffer[8]);
/* Show the saved IP information on the 7-seg led roller */
	ParseNetinfoToRoller( PreBuffer, (BYTE *)&RecvBuffer[0], (BYTE *)&RecvBuffer[4], (BYTE *)&RecvBuffer[8] );
/* Wait until ethernet plug in */
	while ( bEthernetLinkOk == 0x00 ) {
	/* */
		if ( ++delay_msec >= msec ) {
			ShowContent5DigitsLedRoller( seq++ );
			delay_msec = 0;
		}
	/* After display saved IP address over two times, turn on DHCP function */
		if ( !bUseDhcp && seq > (ContentLength << 1) ) {
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
		Delay2(1);
	}

/* Re-initialization for network interface library when using static IP */
	if ( !bUseDhcp ) {
		Nterm();
		if ( NetStart() < 0 )
			return ERROR;
	}

	return NORMAL;
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
		FatalError(2000);
	Delay2(250);

/* Flush the input buffer */
	memset(RecvBuffer, 0, RECVBUF_SIZE);
/* Show the '-L-' message on the 7-seg led */
	Show5DigitLedSeg(3, 0x0e);
	Delay2(250);
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
		FatalError(2000);

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
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x0e, 0x3e, ShowData[0x05], 0xb7 );
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
	LOOP_TRANSMIT_COMMAND( "mac" );
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
		Delay2(1);
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
	LOOP_TRANSMIT_COMMAND( "ip" );
/* Extract the IP address from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the IP address to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[0], (uchar *)&PreBuffer[1], (uchar *)&PreBuffer[2], (uchar *)&PreBuffer[3]);

/* Send out the Mask request command */
	LOOP_TRANSMIT_COMMAND( "mask" );
/* Extract the Mask from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the Mask to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[4], (uchar *)&PreBuffer[5], (uchar *)&PreBuffer[6], (uchar *)&PreBuffer[7]);

/* Send out the Gateway address request command */
	LOOP_TRANSMIT_COMMAND( "gateway" );
/* Extract the Gateway address from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
		return ERROR;
/* Parsing the Gateway address to bytes */
	sscanf(pos, "%hu.%hu.%hu.%hu", (uchar *)&PreBuffer[8], (uchar *)&PreBuffer[9], (uchar *)&PreBuffer[10], (uchar *)&PreBuffer[11]);

/* */
	EE_WriteEnable();
	if ( EE_MultiWrite(EEPROM_NETWORK_SET_BLOCK, EEPROM_NETWORK_TMP_ADDR, EEPROM_NETWORK_SET_LENGTH, PreBuffer) ) {
		EE_WriteProtect();
		return ERROR;
	}
	EE_WriteProtect();
/* Show 'F. nEt.' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f] | 0x80, 0x00, 0x15, ShowData[0x0e], 0x91 );
	Delay2(msec);

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
	if ( !EE_MultiRead(EEPROM_NETWORK_SET_BLOCK, EEPROM_NETWORK_TMP_ADDR, EEPROM_NETWORK_SET_LENGTH, PreBuffer) ) {
	/* Show 'U.PLUG.' on the 7-seg led */
		ShowAll5DigitLedSeg( 0xbe, 0x67, 0x0e, 0x3e, 0xde );
	/* */
		while ( bEthernetLinkOk != 0x00 )
			Delay2(1);
	/* Show the fetched IP on the 7-seg led roller once */
		ParseNetinfoToRoller( str_ptr, (BYTE *)&PreBuffer[0], (BYTE *)&PreBuffer[4], (BYTE *)&PreBuffer[8] );
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
			Delay2(1);
		}
	/* Send out the IP address request command for following connection */
		LOOP_TRANSMIT_COMMAND( "ip" );
	/* Extract the IP address from the raw response & connect to it */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
		if ( InitControlSocket( pos ) == ERROR )
			return ERROR;
	/* One more carriage return to flush the broadcast record */
		LOOP_TRANSMIT_COMMAND( "" );
	/* */
		sprintf(str_ptr, "ip %u.%u.%u.%u", (BYTE)PreBuffer[0], (BYTE)PreBuffer[1], (BYTE)PreBuffer[2], (BYTE)PreBuffer[3]);
		LOOP_TRANSMIT_COMMAND( str_ptr );
	/* Show 'S. iP.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x05] | 0x80, 0x00, 0x04, 0xe7, 0x00 );
		Delay2(msec);
	/* Send out the IP address request command for rechecking */
		LOOP_TRANSMIT_COMMAND( "ip" );
	/* Extract the IP address from the raw response */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
	/* Parsing the IP address to bytes & compare it with storage data */
		sscanf(pos, "%hu.%hu.%hu.%hu", (BYTE *)&str_ptr[0], (BYTE *)&str_ptr[1], (BYTE *)&str_ptr[2], (BYTE *)&str_ptr[3]);
		if ( memcmp(&PreBuffer[0], &str_ptr[0], 4) )
			return ERROR;

	/* */
		sprintf(str_ptr, "mask %u.%u.%u.%u", (BYTE)PreBuffer[4], (BYTE)PreBuffer[5], (BYTE)PreBuffer[6], (BYTE)PreBuffer[7]);
		LOOP_TRANSMIT_COMMAND( str_ptr );
	/* Show 'S.MASk.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x05] | 0x80, 0x76, ShowData[0x0a], ShowData[0x05], 0xb7 );
		Delay2(msec);
	/* Send out the Mask request command for rechecking */
		LOOP_TRANSMIT_COMMAND( "mask" );
	/* Extract the Mask from the raw response */
		if ( (pos = ExtractResponse( RecvBuffer, IPV4_STRING )) == NULL )
			return ERROR;
	/* Parsing the Mask to bytes & compare it with storage data */
		sscanf(pos, "%hu.%hu.%hu.%hu", (BYTE *)&str_ptr[0], (BYTE *)&str_ptr[1], (BYTE *)&str_ptr[2], (BYTE *)&str_ptr[3]);
		if ( memcmp(&PreBuffer[4], &str_ptr[0], 4) )
			return ERROR;

	/* */
		sprintf(str_ptr, "gateway %u.%u.%u.%u", (BYTE)PreBuffer[8], (BYTE)PreBuffer[9], (BYTE)PreBuffer[10], (BYTE)PreBuffer[11]);
		LOOP_TRANSMIT_COMMAND( str_ptr );
	/* Show 'S.GAtE.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x05] | 0x80, 0x5e, ShowData[0x0a], 0x11, ShowData[0x0e] | 0x80 );
		Delay2(msec);
	/* Send out the Gateway address request command */
		LOOP_TRANSMIT_COMMAND( "gateway" );
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

/**
 * @brief
 *
 * @param msec
 * @return int
 */
static int CheckServerConnect( const uint msec )
{
	int sock = -1;

/* Reading the config file for servers information */
	if ( ReadFileBlockZero( GetFileInfoByName_AB(DISKA, "block_0.ini"), (BYTE *)PreBuffer, PREBUF_SIZE ) == ERROR )
		return ERROR;
/* */
	if ( ReadFileFTPInfo( GetFileInfoByName_AB(DISKA, "ftp_info.ini") ) == ERROR )
		return ERROR;

/* Show 'ntP.' on the 7-seg led */
	ShowAll5DigitLedSeg( 0x00, 0x15, 0x11, 0xe7, 0x00 );
/* Start the system time service */
	SysTimeInit( 8, 500 );
	InstallUserTimer1Function_us(5000, TimerFunc);
	Delay2(msec);
/* NTP server connection test */
	sprintf(RecvBuffer, "%u.%u.%u.%u", (BYTE)PreBuffer[41], (BYTE)PreBuffer[43], (BYTE)PreBuffer[45], (BYTE)PreBuffer[47]);
	if ( NTPConnect( RecvBuffer, 123 ) == ERROR || (NTPProcess() == ERROR && NTPProcess() == ERROR) ) {
		SHOW_ERROR_5DIGITLED();
	}
	else {
	/* If we can get the offset from ntp server, then write it into HW(RTC) timer */
		SHOW_GOOD_5DIGITLED();
		Delay2(1000);
		SysTimeToHWTime( 8 );
	}
/* This one second delay is for waiting RTC write-in */
	Delay2(1000);
/* Close the system time service & ntp connection */
	StopUserTimer1Fun();
	NTPClose();

/* Show 'tCP.0.' on the 7-seg led */
	ShowAll5DigitLedSeg( 0x00, 0x11, ShowData[0x0c], 0xe7, ShowData[0x00] | 0x80 );
	Delay2(msec);
/* TCP server 0 connection test */
	sprintf(RecvBuffer, "%u.%u.%u.%u", (BYTE)PreBuffer[28], (BYTE)PreBuffer[29], (BYTE)PreBuffer[30], (BYTE)PreBuffer[31] );
	if ( (sock = ConnectTCP( RecvBuffer, 502 )) == ERROR ) {
		SHOW_ERROR_5DIGITLED();
	}
	else {
		SHOW_GOOD_5DIGITLED();
	}
	Delay2(msec);
	closesocket(sock);
/* Show 'tCP.1.' on the 7-seg led */
	ShowAll5DigitLedSeg( 0x00, 0x11, ShowData[0x0c], 0xe7, ShowData[0x01] | 0x80 );
	Delay2(msec);
/* TCP server 1 connection test */
	sprintf(RecvBuffer, "%u.%u.%u.%u", (BYTE)PreBuffer[32], (BYTE)PreBuffer[33], (BYTE)PreBuffer[34], (BYTE)PreBuffer[35] );
	if ( (sock = ConnectTCP( RecvBuffer, 502 )) == ERROR ) {
		SHOW_ERROR_5DIGITLED();
	}
	else {
		SHOW_GOOD_5DIGITLED();
	}
	Delay2(msec);
	closesocket(sock);

/* Show 'FtP.' on the 7-seg led */
	ShowAll5DigitLedSeg( 0x00, ShowData[0x0f], 0x11, 0xe7, 0x00 );
	Delay2(msec);
/* FW(FTP) server connection test by using the checking firmware function */
	if ( CheckFirmwareVer( PreBuffer, 2000 ) == ERROR ) {
		SHOW_ERROR_5DIGITLED();
	}
	else {
		SHOW_GOOD_5DIGITLED();
	}
	Delay2(msec);
	FTPClose();

	return NORMAL;
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
	LOOP_TRANSMIT_COMMAND( mode == RESET ? "disksize 3 4" : "disksize" );
/* Extract the DiskA size from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, DABSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[0]);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING + 1;

/* Extract the DiskB size from the raw response */
	if ( (pos = ExtractResponse( pos, DABSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[1]);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING + 1;

/* Extract the Reserved size from the raw response */
	if ( (pos = ExtractResponse( pos, RSVSIZE_STRING )) == NULL )
		return ERROR;
/* Parsing the size with integer */
	sscanf(pos, "%hu", (uchar *)&PreBuffer[2]);
/* Show it on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[PreBuffer[0]], 0x01, ShowData[PreBuffer[1]], 0x01, ShowData[PreBuffer[2]] );
/* Display for "msec" msec. */
	Delay2(msec);

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
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x0e, ShowData[0x0a], ShowData[0x05], 0xb7 );
	Delay2(msec);
/* Flushing the disk a */
	LOOP_TRANSMIT_COMMAND( "del /y" );
/* Show 'del. A' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0d], ShowData[0x0e], 0x8e, 0x00, ShowData[0x0a] );
	Delay2(msec);
/* Flushing the disk b */
	LOOP_TRANSMIT_COMMAND( "delb /y" );
/* Show 'del. b' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0d], ShowData[0x0e], 0x8e, 0x00, ShowData[0x0b] );
	Delay2(msec);

/* Start to upload the firmware */
	if ( UploadFileData( DISKA, GetFileInfoByNo_AB(DISKB, 0) ) )
		return ERROR;
/* Show 'Fin. F' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x04, 0x95, 0x00, ShowData[0x0f] );
	Delay2(msec);

/* Start to upload the auto batch file */
	if ( UploadFileData( DISKA, GetFileInfoByName_AB(DISKA, "autoexec.bat") ) )
		return ERROR;
/* Show 'Fin. b' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x04, 0x95, 0x00, ShowData[0x0b] );
	Delay2(msec);

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
	ShowAll5DigitLedSeg( ShowData[0x0f] | 0x80, 0x00, ShowData[0x0b] | 0x80, ShowData[0x00] | 0x80, 0x00 );
/* */
	if ( ReadFileBlockZero( GetFileInfoByName_AB(DISKA, "block_0.ini"), (BYTE *)PreBuffer, PREBUF_SIZE ) == ERROR )
		return ERROR;
	Delay2(msec);
/* Execute the remote agent */
	LOOP_TRANSMIT_COMMAND( "runr" );
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
					Delay2(250);
					continue;
				}
			}
		}
	/* Something wrong, goto error return */
		return ERROR;
	} while ( 1 );
/* Sending the command to remote agent */
	LOOP_TRANSMIT_COMMAND( comm );
	if ( !strncmp(comm, "setdef", 6) )
	/* Show 'S. def.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x05] | 0x80, 0x00, ShowData[0x0d], ShowData[0x0e], ShowData[0x0f] | 0x80 );
	else if ( !strncmp(comm, "check", 5) )
	/* Show 'C. Con.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x0c] | 0x80, 0x00, ShowData[0x0c], 0x1d, 0x95 );
	else
	/* Unknown command */
		return ERROR;
/* */
	Delay2(msec);

/* Extract the remote palert serial from the raw response */
	if ( (pos = ExtractResponse( RecvBuffer, PSERIAL_STRING )) == NULL )
		return ERROR;
/* Show the serial on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[pos[0] - '0'], ShowData[pos[1] - '0'], ShowData[pos[2] - '0'], ShowData[pos[3] - '0'], ShowData[pos[4] - '0'] | 0x80 );
	Delay2(msec);
/* Compare the applied serial & factory serial */
	sscanf(pos, "%5s:%5s", data, _data);
	if ( strncmp(data, _data, 5) ) {
	/* Show 'S.diff.' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x05] | 0x80, ShowData[0x0d], 0x04, ShowData[0x0f], ShowData[0x0f] | 0x80 );
		Delay2(msec);
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
			ShowAll5DigitLedSeg( ShowData[0x0c], ShowData[i == 0 ? 0x01 : 0x00] | 0x80, ShowData[0x0d], 0x04, ShowData[0x0f] | 0x80 );
			Delay2(msec);
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
				Delay2(msec);
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
	LOOP_TRANSMIT_COMMAND( disk == DISKA ? "load" : disk == DISKB ? "loadb" : "loadr" );
/* Start to show the progress and waiting for 150 ms */
	ShowProg5DigitsLed( 0, 0 );
	Delay2(150);
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
						Delay2(250);
						continue;
					}
				}
			}
		/* Sending the carrier return then return error */
			LOOP_TRANSMIT_COMMAND( "" );
			return ERROR;
		}
	/* Show the progress on the 7-seg led */
		ShowProg5DigitsLed( block + 1, blockall );
	}
/* For finishing, last for 100 ms */
	Delay2(150);
/* Sending the carrier return */
	LOOP_TRANSMIT_COMMAND( "" );

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
				ShowAll5DigitLedSeg( ShowData[0x00], ShowData[0x00], ShowData[0x00], ShowData[0x00], ShowData[0x00] );
			}
			else {
			/* Show existed version number on the 7-seg led */
				ShowAll5DigitLedSeg( ShowData[new_name[3] - '0'], ShowData[new_name[4] - '0'], ShowData[new_name[5] - '0'], ShowData[new_name[6] - '0'], ShowData[new_name[7] - '0'] );
			}
			Delay2(msec);
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
		new_name[12] = '\0';
		ShowAll5DigitLedSeg( 0x00, 0x11, 0x9d, 0x00, 0x00 );
		Delay2(msec);
	/* Show new version number on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[new_name[3] - '0'], ShowData[new_name[4] - '0'], ShowData[new_name[5] - '0'], ShowData[new_name[6] - '0'], ShowData[new_name[7] - '0'] );
		Delay2(msec);
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
 * @param host
 * @param port
 * @return int
 */
static int ConnectTCP( const char *host, uint port )
{
	extern int errno;

	int sock = -1;
	struct sockaddr_in hostaddr;
	ulong start_time;

/* */
	if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) >= 0 ) {
		YIELD();
		memset(&hostaddr, 0, sizeof(struct sockaddr_in));
		hostaddr.sin_family = AF_INET;
		hostaddr.sin_addr.s_addr = inet_addr((char *)host);
		hostaddr.sin_port = htons(port);
		YIELD();
	/* Set Non-blocking mode */
		SOCKET_NOBLOCK(sock);
	/* */
		if ( connect(sock, (struct sockaddr *)&hostaddr, sizeof(hostaddr)) ) {
			if ( errno == EINPROGRESS ) {
				start_time = GetTimeTicks();
				while ( !SOCKET_ISOPEN(sock) ) {
					if ( (GetTimeTicks() - start_time) >= TCP_CONNECT_TIMEOUT ) {
						SHOW_TIMEOUT_5DIGITLED();
						Delay2(500);
						goto err_return;
					}
					YIELD();
					Delay2(1);
				}
			}
			else {
				goto err_return;
			}
		}
	}
/* Must >= 3ms to make sure the following TCP/IP function works. */
	YIELD();
	Delay2(5);
	return sock;

/* Return for error */
err_return:
	closesocket(sock);
	YIELD();
	return ERROR;
}

/**
 * @brief
 *
 * @param ip
 * @param mask
 * @param gateway
 */
static void ParseNetinfoToRoller( char *dest, const BYTE ip[4], const BYTE mask[4], const BYTE gateway[4] )
{
	sprintf(
		dest, "%u.%u.%u.%u-%u  %u.%u.%u.%u  ",
		ip[0], ip[1], ip[2], ip[3],
		ConvertMask( *(long *)mask ),
		gateway[0], gateway[1], gateway[2], gateway[3]
	);
	EncodeAddrDisplayContent( dest );

	return;
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

/**
 * @brief
 *
 */
static void TimerFunc( void )
{
	SysTimeService();

	return;
}

/**
 * @brief
 *
 * @param msec
 */
static void FatalError( const int msec )
{
/* Show 'FAtAL.' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], ShowData[0x0a], 0x11, ShowData[0x0a], 0x8e );
/* Reset the network setting */
	SetNetworkConfig( NETWORK_DEFAULT );
	Delay2(msec);
/* Reset the system */
	ResetProgram();

	return;
}

/**
 * @brief
 *
 * @return int
 */
static int ResetProgram( void )
{
	((void (far *)(void))0xFFFF0000L)();  /* Program start address. */

	return 0;
}
