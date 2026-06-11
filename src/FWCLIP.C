/**
 * @file FWCLIP.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2026-06-09
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
/* */
#include <u7186EX/7186e.h>
#include <u7186EX/Tcpip32.h>
/* */
#include <PALSAK.h>
#include <LEDINFO.h>

/* Main socket */
static volatile int SockRecv = -1;
static volatile int SockSend = -1;
/* Global address info, expecially for transmitting command */
static struct sockaddr_in TransmitAddr;
/* Input buffer */
static char RecvBuffer[RECVBUF_SIZE];
/* Output buffer */
static char PreBuffer[PREBUF_SIZE];
/*  */
static uchar SelectedSlot = 0;

/* */
static int  InitControlSocket( const char * );
static void SwitchFWSlot( const uint );
static int  TransmitCommand( const char * );
static int  TransmitDataRaw( const char *, int );
static void ForceFlushSocket( int );
static int  ProcSelectedSlot( const uchar );
static int  UploadFileData( const int, const FILE_DATA far * );
static void FatalError( void );
static int  ResetProgram( void );

/* If the init pin is connected when transmit command, the whole program will reset */
#define LOOP_TRANSMIT_COMMAND(_COMM) \
		while ( TransmitCommand( (_COMM) ) != NORMAL && (!ReadInitPin() || ResetProgram()) )

/**
 * @brief Main function, entry
 *
 */
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
	Delay2(5);
/* Wait until the network connection is on */
	SwitchFWSlot( 5000 );
/* Initialization for broadcasting network */
	if ( InitControlSocket( NULL ) == ERROR )
		goto err_return;
/* Start to upload firmware & batch file */
	if ( ProcSelectedSlot( SelectedSlot ) == ERROR )
		goto err_return;
/* Show the Good result on the 7-seg led */
	SHOW_GOOD_5DIGITLED( 1000 );
/* */
	ForceFlushSocket( SockRecv );

normal_return:
/* Close the sockets */
	closesocket(SockSend);
	closesocket(SockRecv);
/* Terminate the network interface */
	Nterm();
	return;
err_return:
/* Show the 'ERROR' on the 7-seg led */
	SHOW_ERROR_5DIGITLED( 2000 );
	goto normal_return;
}

/**
 * @brief The initialization process of control socket.
 *
 * @param dotted_ip
 * @return int
 * @retval 0 All of the socket we need are created.
 * @retval < 0 Something wrong when creating socket or setting up the operation mode.
 */
static int InitControlSocket( const char *dotted_ip )
{
	char optval = 1;

/* Close the previous sockets for following process */
	closesocket(SockSend);
	closesocket(SockRecv);
/* Flush the address struct */
	memset(&TransmitAddr, 0, sizeof(struct sockaddr));
/* */
	if ( dotted_ip ) {
	/* Terminate the network interface first */
		Nterm();
	/* We should set the Mask to zero, let all the packet skip the routing table */
		TransmitAddr.sin_addr.s_addr = 0L;
		SetMask((uchar *)&TransmitAddr.sin_addr.s_addr);
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
	if (
		(SockSend = socket(PF_INET, SOCK_DGRAM, 0)) < 0 ||
	/* Set the socket to reuse the address */
		setsockopt(SockSend, SOL_SOCKET, SO_DONTROUTE, &optval, sizeof(optval)) < 0 ||
	/* Set the broadcast ability */
		setsockopt(SockSend, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval)) < 0 ||
	/* Create the receiving socket */
		(SockRecv = dotted_ip ? SockSend : socket(PF_INET, SOCK_DGRAM, 0)) < 0 ||
	/* Set the socket to reuse the address */
		setsockopt(SockRecv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0
	) {
		return ERROR;
	}
/* Bind the receiving socket to the port number 54321 or 12345 */
	TransmitAddr.sin_family = AF_INET;
	TransmitAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	TransmitAddr.sin_port = htons(dotted_ip ? CONTROL_BIND_PORT : LISTEN_PORT);
	if ( bind(SockRecv, (struct sockaddr *)&TransmitAddr, sizeof(struct sockaddr)) < 0 )
		return ERROR;
/* Set the timeout of receiving socket to 0.05 sec. */
	SOCKET_RXTOUT(SockRecv, 50);

/* Finally, really set the transmitting address info */
	TransmitAddr.sin_family = AF_INET;
	TransmitAddr.sin_addr.s_addr = dotted_ip ? inet_addr((char *)dotted_ip) : htonl(INADDR_BROADCAST);
	TransmitAddr.sin_port = htons(CONTROL_PORT);

	return NORMAL;
}

/**
 * @brief
 *
 * @param msec
 */
static void SwitchFWSlot( const uint msec )
{
	uchar slot        = FWCLIP_SLOT_0;
	uint  delay_msec  = 0;
	BYTE  display_seg = 0x02;

/* Show the " S.XX " message on the 7-seg led */
	ShowAll5DigitLedSeg( 0x00, ShowData[0x05] | 0x80, ShowData[slot / 10], ShowData[slot % 10], 0x00, 0 );
/*
 * External variables for network linking status :
 * After testing, when network is real connected,
 * this number should be 0x40(64) or 0x01.
 */
	while ( !bEthernetLinkOk ) {
	/* */
		if ( !(delay_msec % 100) ) {
			Show5DigitLedSeg(1, display_seg);
			Show5DigitLedSeg(5, display_seg);
		/* */
			if ( !(display_seg = (display_seg << 1) & ~0x80) )
				display_seg = 0x02;
		}
	/* Detect the init. pin condition for switching to the updating firmware func. */
		if ( ReadInitPin() && slot != FWCLIP_SLOT_COUNT ) {
		/* */
			Show5DigitLed(3, 0x0f);
			Show5DigitLed(4, 0x0f);
		/* */
			slot = FWCLIP_SLOT_COUNT;
		}
	/* Increase the times of waiting network connection every 500 msec */
		if ( ++delay_msec >= msec && slot != FWCLIP_SLOT_COUNT ) {
		/* */
			slot = ++slot % FWCLIP_SLOT_COUNT;
		/* Show the " S.XX " message on the 7-seg led */
			Show5DigitLed(3, slot / 10);
			Show5DigitLed(4, slot % 10);
		/* */
			delay_msec = 0;
		}
	/* */
		Delay2(1);
	}
/* */
	SelectedSlot = slot;

	return;
}

/**
 * @brief Transmitting control command.
 *
 * @param comm The pointer to the string of command.
 * @return int
 * @retval ERROR(-1) - It didn't receive the response properly.
 * @retval NORMAL(0) - It has been processed properly.
 */
static int TransmitCommand( const char *comm )
{
	char comm_buf[COMMBUF_SIZE];

/* Show the '-S-' message on the 7-seg led */
	SHOW_2DASH_5DIGITLED( 0, ShowData[0x05], 0 );
/* Appending the '\r' to the input command */
	sprintf(comm_buf, "%s\r", comm);
/* Flush the receiving buffer from client, just in case */
	while ( recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE, 0, NULL, NULL) > 0 );

/* Transmitting the command to others */
	if ( TransmitDataRaw( comm_buf, strlen(comm_buf) ) )
		return ERROR;

/* Show the '-L-' message on the 7-seg led */
	Show5DigitLedSeg(3, 0x0e);
	Delay2(250);

	return NORMAL;
}

/**
 * @brief Transmitting raw data bytes through sending socket.
 *
 * @param data The pointer to the data beginning.
 * @param data_length The length of data bytes.
 * @return int
 * @retval ERROR(-1) - It didn't receive the response properly.
 * @retval NORMAL(0) - It has been received properly.
 */
static int TransmitDataRaw( const char *data, int data_length )
{
	int   ret;
	uchar trycount = 0;

/* Sending the data bytes by command line method */
	if ( sendto(SockSend, (char *)data, data_length, MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr)) <= 0 )
		FatalError();
/* Receiving the response from the other side */
	while ( (ret = recvfrom(SockRecv, RecvBuffer, RECVBUF_SIZE, 0, NULL, NULL)) <= 0 ) {
		if ( ++trycount >= NETWORK_OPERATION_RETRY )
			return ERROR;
	/* */
		Delay2(250 * (1 << trycount));
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
/* Show 'FLUSH.' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x0e, 0x3e, ShowData[0x05], 0xb7, 0 );
/* Directly flush the receiving buffer of the sock until it is empty */
	while ( recvfrom(sock, RecvBuffer, RECVBUF_SIZE, 0, NULL, NULL) > 0 )
		Delay2(250);
/* Flush the input buffer */
	memset(RecvBuffer, 0, RECVBUF_SIZE);

	return;
}

/**
 * @brief Process the selected firmware of Palert or just erase the flash.
 *
 * @param slot
 * @return int
 * @retval NORMAL(0) - The uploading process is successful.
 * @retval ERROR(-1) - Something happened when uploading.
 */
static int ProcSelectedSlot( const uchar slot )
{
/* */
	FILE_DATA *_slots[FWCLIP_SLOT_COUNT];
#define X(a, b) _slots[a] = b;
	FWCLIP_SLOTS_TABLE
#undef X

/* Show 'FLASH.' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], 0x0e, ShowData[0x0a], ShowData[0x05], 0xb7, 1000 );
	if ( slot < FWCLIP_SLOT_COUNT ) {
	/* Start to upload the firmware */
		if ( UploadFileData( DISK_PALSAK_FIRMWARE, _slots[slot] ) )
			return ERROR;
	/* Show 'Fin. F' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x0f], 0x04, 0x95, 0x00, ShowData[0x0f], 2000 );
	}
	else {
	/* Flushing the disk b */
		LOOP_TRANSMIT_COMMAND( "delb /y" );
	/* Show 'del. b' on the 7-seg led */
		ShowAll5DigitLedSeg( ShowData[0x0d], ShowData[0x0e], 0x8e, 0x00, ShowData[0x0b], 1000 );
	}

	return NORMAL;
}


/**
 * @brief Parsing the file that pointed by the pointer and uploading to the Palert on the other end of ethernet
 *        wire by command line method.
 *
 * @param disk The uploadind target disk.
 * @param fileptr The waiting delay of display in msecond.
 * @return int
 * @retval NORMAL(0) - The uploading process is successful.
 * @retval ERROR(-1) - Something happened when uploading or file is not existed.
 */
static int UploadFileData( const int disk, const FILE_DATA far *fileptr )
{
	uint  tmp;
	uint  block;
	uint  blockall;
	ulong addrindex = 0;
	BYTE far *out_ptr = (BYTE far *)PreBuffer;

	if (
	/* Checking this opened file is file or not, and check the size of this file */
		fileptr == NULL || fileptr->mark != 0x7188 || fileptr->size <= 0 ||
	/* Initialize the CRC16 table for following CRC16 computation */
		CRC16_MakeTable()
	) {
		return ERROR;
	}
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
			if ( !TransmitDataRaw( (char *)out_ptr, 260 ) ) {
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
/* For finishing, last for 150 ms */
	Delay2(150);
/* Sending the carrier return */
	LOOP_TRANSMIT_COMMAND( "" );

	return NORMAL;
}

/**
 * @brief
 *
 */
static void FatalError( void )
{
/* Show 'FAtAL.' on the 7-seg led */
	ShowAll5DigitLedSeg( ShowData[0x0f], ShowData[0x0a], 0x0f, ShowData[0x0a], 0x8e, 2000 );
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
/* Program start address. */
	((void (far *)(void))0xFFFF0000L)();

	return 0;
}
