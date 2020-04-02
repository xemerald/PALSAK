#include <stdio.h>
#include <u7186EX\7186e.h>
#include <u7186EX\Tcpip32.h>

/* Address to accept any incoming messages */
#define	INADDR_ANY        0x00000000
/* Address to send to all hosts */
#define	INADDR_BROADCAST  0xffffffff
/* The port used to listen the broadcast messages from palert */
#define	LISTEN_PORT   54321
/* The port used to send the control messages */
#define	CONTROL_PORT  23
/* The size of the buffer used to received the data from broadcast */
#define BUFSIZE          256
/* String length of those responses from the other palert */
#define MAC_STRING       17  /* Include the null-terminator */
#define IPV4_STRING      16  /* Include the null-terminator */
#define DABSIZE_STRING    3  /* Include the null-terminator */
#define RSVSIZE_STRING    4  /* Include the null-terminator */
/* Workflow type define */
#define STRATEGY_MAC     0x01
#define STRATEGY_UPL     0x02
/* The checking result of func. */
#define NORMAL   0
#define ERROR   -1
/* The functional mode of "CheckPalertDisk()" */
#define CHECK  0
#define RESET  1
/* The default(normal) disk size partition of the palert */
#define DISKA_SIZE   3
#define DISKB_SIZE   4
#define RESERVE_SIZE 0
/* The response from the other palert when receiving data bytes */
#define ACK   6
#define NAK  21
/* The macro to showing " - - " on the 7-seg led */
#define SHOW_2DASH_5DIGITLED() \
		Show5DigitLedSeg(1, 0); \
		Show5DigitLedSeg(2, 1); \
		Show5DigitLedSeg(3, 0); \
		Show5DigitLedSeg(4, 1); \
		Show5DigitLedSeg(5, 0);

/* Main socket */
static volatile int SockRecv;
static volatile int SockSend;

/* Global address info, expecially for broadcasting command */
static struct sockaddr_in BroadcastAddr;

/* Input buffer */
static char InBuffer[BUFSIZE];

/* Workflow flag */
static unsigned int WorkflowFlag = STRATEGY_MAC;

/* Internal functions' prototype */
static int NetworkInit( const int, const int );
static int WaitNetworkConnect( void );

static int BroadcastCommand( const char * );
static int BroadcastDataByCommand( char far *, unsigned long );
static char *ExtractResponse( char *, const unsigned int );

static void ShowMac5DigitsLed( const char * const, const unsigned int, unsigned int );
static void ShowCheck5DigitsLed( const int, const unsigned int );
static void ShowProg5DigitsLed( const unsigned int, const unsigned int, const unsigned int );

static int GetPalertMac( const unsigned int );
static int CheckPalertDisk( const int, const unsigned int );
static int UploadPalertFirmware( const unsigned int );

static int UploadFileData( const FILE_DATA far * );

/* Main function, entry */
void main( void )
{
	int ret;

/* Initialization for u7186EX's general library */
	InitLib();
/* Wait for the network interface ready, it might be shoter */
	Delay(1200);
/* Initialization for network interface library */
	if ( NetworkInit( LISTEN_PORT, CONTROL_PORT ) < 0 ) return;
/* Wait until the network connection is on */
	WaitNetworkConnect();

/* Checking the segment of palert disk */
	ret = CheckPalertDisk( CHECK, 4000 );
/* Show the checking result on the 7-seg led */
	ShowCheck5DigitsLed( ret, 4000 );

/* If the previous result is error, then resetting the segment of palert disk */
	if ( ret == ERROR ) {
		ret = CheckPalertDisk( RESET, 4000 );
	/* Show the checking result on the 7-seg led */
		ShowCheck5DigitsLed( ret, 4000 );
	/* If the resetting result is still error, just give it up */
		if ( ret == ERROR ) return;
		WorkflowFlag |= STRATEGY_UPL;
	}
/*
	Otherwise, after resetting disk, we need to upload firmware.
	Or user force to upload firmware by connecting the ethernet
	wire after 15 seconds from power-on.
*/
	if ( WorkflowFlag & STRATEGY_UPL ) {
	/* Start to upload firmware & batch file */
		ret = UploadPalertFirmware( 2000 );
	/* Show the checking result on the 7-seg led */
		ShowCheck5DigitsLed( ret, 4000 );
	/* If the result is error, just exit! */
		if ( ret == ERROR ) return;
	}
/* If if shows the MAC flag, just get the MAC and show it */
	if ( WorkflowFlag & STRATEGY_MAC ) {
	/* Show the MAC of the Palert */
		GetPalertMac( 4000 );
	}

/* Close the sockets */
	closesocket(SockRecv);
	closesocket(SockSend);
/* Terminate the network interface */
	Nterm();

	return;
}

/* External variables for broadcast setting */
extern int bAcceptBroadcast;

/*
*  NetworkInit() - The initialization process of network interface.
*  argument:
*    rport - The port number for receiving response.
*    sport - The port number for sending the command.
*  return:
*    0   - All of the socket we need are created.
*    < 0 - Something wrong when creating socket or setting up the operation mode.
*/
static int NetworkInit( const int rport, const int sport )
{
	int  ret = 0;
	char optval = 1;

	struct sockaddr_in _addr;

/* Initialization for network interface library */
	ret = NetStart();
	if ( ret < 0 ) goto err_return;

/* Setup for accepting broadcast packet */
	bAcceptBroadcast = 1;

/* Create the receiving socket */
	SockRecv = socket(PF_INET, SOCK_DGRAM, 0);
	if ( SockRecv < 0 ) goto err_return;

/* Set the socket to reuse the address */
	ret = setsockopt(SockRecv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	if ( ret < 0 ) goto err_return;

/* Bind the receiving socket to the broadcast port */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = PF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	_addr.sin_port = htons(rport);
	ret = bind(SockRecv, (struct sockaddr *)&_addr, sizeof(struct sockaddr));
	if ( ret < 0 ) goto err_return;

/* Set the timeout of receiving socket to 0.5 sec. */
	SOCKET_RXTOUT(SockRecv, 250);

/* Create the sending socket */
	ret = SockSend = socket(PF_INET, SOCK_DGRAM, 0);
	if ( SockSend < 0 ) goto err_return;

/* Set the socket to be able to broadcast */
	ret = setsockopt(SockSend, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
	if ( ret < 0 ) goto err_return;

/* Set the sending address info */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = PF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	_addr.sin_port = htons(sport);

	BroadcastAddr = _addr;
	return NORMAL;

/* Return for error */
err_return:
	Nterm();
	return ret;
}

/* External variables for network linking status */
extern volatile unsigned bEthernetLinkOk;

/*
*  WaitNetworkConnect() - Waiting for the network connection is ready.
*  argument:
*    None.
*  return:
*    0x40 - The connection is ready.
*/
static int WaitNetworkConnect( void )
{
	unsigned int num = 0;
	unsigned int ret;

/* Show the "- -" message on the 7-seg led */
	SHOW_2DASH_5DIGITLED();
/* After testing, when network is real connected, this number should be 0x40(64). */
	while ( (ret = bEthernetLinkOk) != 0x40 ) {
	/* Show the "-0-" to "-f-" message on the 7-seg led each loop */
		Show5DigitLed(3, num++);
		if ( num > 0x0f ) {
			num %= 0x10;
		/* After it shows 'f' on 7-seg led, it will go into forcing uploading mode */
			WorkflowFlag = STRATEGY_UPL;
		}
		Delay(800);
	}

	return ret;
}

/*
*  BroadcastCommand() - Broadcasting control command.
*  argument:
*    comm - The pointer to the string of command.
*  return:
*    <= 0 - It didn't receive the response properly.
*    > 0  - It has been received properly.
*/
static int BroadcastCommand( const char *comm )
{
	int  ret;
	char buffer[16];
	unsigned int retrycount = 0;

	struct sockaddr_in _addr;
	int fromlen = sizeof(struct sockaddr);

/* Appending the '\r' to the input command */
	memset(buffer, 0, 16);
	sprintf(buffer, "%s\r", comm);
	ret = strlen(buffer);

/* Show the "-S-" message on the 7-seg led */
	SHOW_2DASH_5DIGITLED();
	Show5DigitLed(3, 5);
/* Flush the receiving buffer from client, just in case */
	while ( SOCKET_HASDATA(SockRecv) )
		recvfrom(SockRecv, InBuffer, BUFSIZE, 0, (struct sockaddr *)&_addr, &fromlen);
/* Broadcasting the command to others */
	sendto(SockSend, buffer, ret, 0, (struct sockaddr *)&BroadcastAddr, sizeof(BroadcastAddr));
	Delay(250);

/* Flush the input buffer */
	memset(InBuffer, 0, BUFSIZE);

/* Show the "-L-" message on the 7-seg led */
	Show5DigitLedSeg(3, 0x0E);
	Delay(250);
/* Receiving the response from the palert */
	while ( (ret = recvfrom(SockRecv, InBuffer, BUFSIZE, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
		if ( retrycount++ >= 2 ) break;
	};

	return ret;
}

/*
*  BroadcastDataByCommand() - Broadcasting data bytes by command line.
*  argument:
*    data        - The pointer to the data beginning.
*    data_length - The length of data bytes.
*  return:
*    <= 0 - It didn't receive the response properly.
*    > 0  - It has been received properly.
*/
static int BroadcastDataByCommand( char far *data, unsigned long data_length )
{
	int ret;
	unsigned int retrycount = 0;

	struct sockaddr_in _addr;
	int fromlen = sizeof(struct sockaddr);

/* Flush the receiving buffer from client, just in case */
	while ( SOCKET_HASDATA(SockRecv) )
		recvfrom(SockRecv, InBuffer, BUFSIZE, 0, (struct sockaddr *)&_addr, &fromlen);

/* Sending the data bytes by command line method */
	sendto(SockSend, data, data_length, 0, (struct sockaddr *)&BroadcastAddr, sizeof(BroadcastAddr));

/* Flush the input buffer */
	memset(InBuffer, 0, BUFSIZE);

/* Receiving the response from the palert */
	while ( (ret = recvfrom(SockRecv, InBuffer, BUFSIZE, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
		if ( retrycount++ >= 2 ) break;
	};

	return ret;
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
static char *ExtractResponse( char *buffer, const unsigned int length )
{
	char *pos = NULL;

	if ( buffer != NULL ) {
	/* Find out where is the '=', and skip all the following space or tab. */
		pos = strchr(buffer, '=') + 1;
		if ( pos != NULL ) {
			for( ; *pos == ' ' || *pos == '\t'; pos++ );
		/* Appending a null-terminator after the data. */
			*(pos + length) = '\0';
		}
	}

	return pos;
}

/* Mark for seq. */
static const unsigned char Mark[3] = { 0x01, 0x48, 0x49 };

/*
*  ShowMac5DigitsLed() - Showing the derive MAC on the 7-seg led.
*  argument:
*    mac   - The pointer to the digit of MAC.
*    msec  - The waiting delay of display in msecond.
*    times - The times that the MAC will display ( <=0: Inf ).
*  return:
*    None.
*/
static void ShowMac5DigitsLed( const char * const mac, const unsigned int msec, unsigned int times )
{
	int   i, digit;
	const char *pos;

	do {
	/* Try to display the MAC address on the 7-seg led */
		pos = mac;
	/* Every 2 chars will be display on the same page */
		for ( i = 0; i < 3; i++ ) {
		/* Display the seq. mark */
			Show5DigitLedSeg(1, Mark[i]);
		/* Seperate one char into two digits */
			digit = (*pos >> 4) & 0xf;
			Show5DigitLed(2, digit);
			digit = *pos++ & 0xf;
			Show5DigitLedWithDot(3, digit);
		/* The next char */
			digit = (*pos >> 4) & 0xf;
			Show5DigitLed(4, digit);
			digit = *pos++ & 0xf;
			Show5DigitLedWithDot(5, digit);
		/* Display for "msec" msec. */
			Delay(msec);
		}
/* If the times is setted to 0, this will display as long as you shutdown this device */
	} while ( --times );

	return;
}

/*
*  ShowCheck5DigitsLed() - Showing the check result on the 7-seg led.
*  argument:
*    check_result - The result flag, NORMAL or ERROR.
*    msec         - The waiting delay of display in msecond.
*  return:
*    None.
*/
static void ShowCheck5DigitsLed( const int check_result, const unsigned int msec )
{
	switch ( check_result ) {
		case NORMAL:
		/* Display "Good." on the 7-seg led */
			Show5DigitLedSeg(1, 0x5e);
			Show5DigitLedSeg(2, 0x1d);
			Show5DigitLedSeg(3, 0x1d);
			Show5DigitLed(4, 0x0d);
			Show5DigitLedSeg(5, 0x80);
			break;
		case ERROR:
		/* Display "Error." on the 7-seg led */
			Show5DigitLed(1, 0x0e);
			Show5DigitLedSeg(2, 0x05);
			Show5DigitLedSeg(3, 0x05);
			Show5DigitLedSeg(4, 0x1d);
			Show5DigitLedSeg(5, 0x85);
			break;
	}
/* Display for "msec" msec. */
	Delay(msec);

	return;
}

/*
*  ShowProg5DigitsLed() - Showing the check result on the 7-seg led.
*  argument:
*    nblock      - The accumulated blocks number.
*    totalblocks - The total blocks number.
*    msec        - The waiting delay of display in msecond.
*  return:
*    None.
*/
static void ShowProg5DigitsLed( const unsigned int nblock, const unsigned int totalblocks, const unsigned int msec )
{
	unsigned int ival = nblock * 100;
	unsigned int fval = ival;
/*
	Compute the percentage of progress, ival stand for integer part of value,
	fval stand for fraction part of value.
*/
	if ( totalblocks > 0 ) {
		ival = (unsigned int)(ival / totalblocks);
		fval = (unsigned int)((unsigned long)fval * 100 / totalblocks) - ival * 100;
	}
	else {
		ival = 0;
		fval = 0;
	}

/* Display progressing percentage on the 7-seg led */
	if ( ival < 100 ) {
		Show5DigitLedSeg(1, 0);
		Show5DigitLed(2, ival >= 10 ? ival / 10 : 16);
		Show5DigitLedWithDot(3, ival % 10);
		Show5DigitLed(4, fval / 10);
		Show5DigitLed(5, fval % 10);
	}
	else {
		Show5DigitLed(1, 1);
		Show5DigitLed(2, 0);
		Show5DigitLedWithDot(3, 0);
		Show5DigitLed(4, 0);
		Show5DigitLed(5, 0);
	}
/* Display for "msec" msec. */
	Delay(msec);

	return;
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
static int GetPalertMac( const unsigned int msec )
{
	int   ret = 0;
	char  mac[6];
	char *pos;

/* Send out the MAC address request command */
	while ( (ret = BroadcastCommand( "mac" )) <= 0 );

/* Start to parse the response */
	InBuffer[ret] = '\0';
/* Extract the MAC address from the raw response */
	pos = ExtractResponse( InBuffer, MAC_STRING );
	if ( pos == NULL ) return ERROR;

/* Parsing the MAC address with hex format into six chars */
	sscanf(pos, "%x:%x:%x:%x:%x:%x",
		&mac[0], &mac[1], &mac[2],
		&mac[3], &mac[4], &mac[5]);

/* Show on the 7-seg led */
	ShowMac5DigitsLed( mac, msec, 0 );

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
static int CheckPalertDisk( const int mode, const unsigned int msec )
{
	int   ret = 0;
	int   size_a, size_b, size_r;
	char *pos;
/* Function switch between "reset" & "check" */
	const char *command = mode == RESET ? "disksize 3 4" : "disksize";

/* Send out the Disk size request command */
	while ( (ret = BroadcastCommand( command )) <= 0 );

/* Start to parse the response */
	InBuffer[ret] = '\0';
/* Extract the DiskA size from the raw response */
	pos = ExtractResponse( InBuffer, DABSIZE_STRING );
	if ( pos == NULL ) return -1;
/* Parsing the size with integer */
	sscanf(pos, "%d", &size_a);
/* Show it on the 7-seg led */
	Show5DigitLed(1, size_a);
	Show5DigitLedSeg(2, 1);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING;

/* Extract the DiskB size from the raw response */
	pos = ExtractResponse( pos + 1, DABSIZE_STRING );
	if ( pos == NULL ) return -1;
/* Parsing the size with integer */
	sscanf(pos, "%d", &size_b);
/* Show it on the 7-seg led */
	Show5DigitLed(3, size_b);
	Show5DigitLedSeg(4, 1);
/* Move the pointer to the next Disk size */
	pos += DABSIZE_STRING;

/* Extract the Reserved size from the raw response */
	pos = ExtractResponse( pos + 1, RSVSIZE_STRING );
	if ( pos == NULL ) return -1;
/* Parsing the size with integer */
	sscanf(pos, "%d", &size_r);
/* Show it on the 7-seg led */
	Show5DigitLed(5, size_r);
/* Display for "msec" msec. */
	Delay(msec);

/* Check the size */
	if ( size_a != DISKA_SIZE ||
		size_b != DISKB_SIZE ||
		size_r != RESERVE_SIZE )
	{
		ret = ERROR;
	}
	else {
		ret = NORMAL;
	}

	return ret;
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
static int UploadPalertFirmware( const unsigned int msec )
{
	FILE_DATA far *fp;

/* Show "FLASH." on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x0E);
	Show5DigitLed(3, 0x0a);
	Show5DigitLed(4, 0x05);
	Show5DigitLedSeg(5, 0xb7);
	Delay(msec);
/* Flushing the disk a */
	while ( BroadcastCommand( "del /y" ) <= 0 );
/* Show "del. A" on the 7-seg led */
	Show5DigitLed(1, 0x0d);
	Show5DigitLed(2, 0x0e);
	Show5DigitLedSeg(3, 0x8e);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0a);
	Delay(msec);
/* Flushing the disk b */
	while ( BroadcastCommand( "delb /y" ) <= 0 );
/* Show "del. b" on the 7-seg led */
	Show5DigitLed(1, 0x0d);
	Show5DigitLed(2, 0x0e);
	Show5DigitLedSeg(3, 0x8e);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0b);
	Delay(msec);

/* Start to upload the firmware */
	fp = GetFileInfoByNo_AB(DISKB, 0);
	if ( UploadFileData( fp ) ) goto err_return;
/* Show "Fin. F" on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x04);
	Show5DigitLedSeg(3, 0x95);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0f);
	Delay(msec);

/* Start to upload the auto batch file */
	fp = GetFileInfoByName_AB(DISKA, "autoexec.bat");
	if ( UploadFileData( fp ) ) goto err_return;
/* Show "Fin. b" on the 7-seg led */
	Show5DigitLed(1, 0x0f);
	Show5DigitLedSeg(2, 0x04);
	Show5DigitLedSeg(3, 0x95);
	Show5DigitLedSeg(4, 0x00);
	Show5DigitLed(5, 0x0b);
	Delay(msec);

	return NORMAL;

/* Return for error */
err_return:
	return ERROR;
}

/*
*  UploadFileData() - Parsing the file that pointed by the pointer and
*                     uploading to the Palert on the other end of ethernet
*                     wire by command line method.
*  argument:
*    fileptr - The waiting delay of display in msecond.
*  return:
*    NORMAL(0) - The uploading process is successful.
*    ERROR(-1) - Something happened when uploading or file is not existed.
*/
static int UploadFileData( const FILE_DATA far *fileptr )
{
	unsigned int i, block, retrycount;
	unsigned int crc16;
	unsigned long addrindex = 0;

/* Compute the total block number, each block should be 256 bytes */
	const unsigned int blockall = (((fileptr->size + 255) >> 8) + 1);
/* Seperate the size to highbyte and lowbyte */
	const unsigned int sizelo   = fileptr->size & 0xff;
	const unsigned int sizehi   = fileptr->size >> 8;

	char  outbuffer[260] = { 0 };

/* Checking this opened file is file or not, And the check the size of this file */
	if ( fileptr->mark != 0x7188 && fileptr->size <= 0 ) goto err_return;
/* Send out the upload request command */
	while ( BroadcastCommand( "load" ) <= 0 );
/* Initialize the CRC16 table for following CRC16 computation */
	if ( CRC16_MakeTable() ) goto err_return;
/* Start to show the progress and waiting for 250 ms */
	ShowProg5DigitsLed( 0, blockall, 250 );

/* Go through all blocks */
	for ( block = 0; block < blockall; block++ ) {
	/* Setting all bytes to zero first */
		memset(outbuffer, 0, sizeof(outbuffer));
	/* First block */
		if ( block == 0 ) {
		/* The header(2 bytes) of the first block should be 29 & 00 (in decimal)*/
			outbuffer[0] = 29;
			/* outbuffer[1] = 0; */
		/* Following is name of the file and the length is limited under 12 bytes */
			memcpy(&outbuffer[2], fileptr->fname, 12);
		/* The file size part, the high part should be seperate into high & low again */
			/* outbuffer[14] = 0; */
			outbuffer[15] = sizelo;
			outbuffer[16] = sizehi & 0xff;
			outbuffer[17] = sizehi >> 8;
		/* The date information of the file */
			/* outbuffer[18] = 0; */
			outbuffer[19] = fileptr->year;
			/* outbuffer[20] = 0; */
			outbuffer[21] = fileptr->month;
			/* outbuffer[22] = 0; */
			outbuffer[23] = fileptr->day;
			/* outbuffer[24] = 0; */
			outbuffer[25] = fileptr->hour;
			/* outbuffer[26] = 0; */
			outbuffer[27] = fileptr->minute;
			/* outbuffer[28] = 0; */
			outbuffer[29] = fileptr->sec;
		/* All other bytes are zero... */
		}
	/* Rest blocks */
		else {
		/* The header(2 bytes) of other blocks(ex. the last block) should be 00 & 01 (in decimal)*/
			if ( block < (blockall - 1) || sizelo == 0 )
				outbuffer[1] = 1;
		/* The header(2 bytes) of the last block should be sizelo & 00 (in decimal)*/
			else
				outbuffer[0] = sizelo;
		/* Just copy all file data in binary byte by byte, one block should consist 256 bytes */
			memcpy(outbuffer + 2, AddFarPtrLong(fileptr->addr, addrindex), outbuffer[0] ? sizelo : 256);
			addrindex += 256;
		}
	/* CRC16 computing part, using build-in function */
		CRC16_Reset();
		CRC16_AddDataN( outbuffer, 258 );
		crc16 = CRC16_Read();
		outbuffer[258] = crc16 >> 8;
		outbuffer[259] = crc16 & 0xff;
	/* Sending by the command line method */
		retrycount = 0;
		while ( 1 ) {
			if ( BroadcastDataByCommand( outbuffer, 260 ) > 0 ) {
				if ( InBuffer[0] == ACK || InBuffer[0] == 0 ) break;
			/* If receiving "Not ack", retry two times */
				else if ( InBuffer[0] == NAK ) {
					if ( retrycount++ <= 2 ) continue;
				}
			}
		/* Something wrong, goto error return */
			goto err_return;
		}
	/* Show the progress on the 7-seg led */
		ShowProg5DigitsLed( block + 1, blockall, 1 );
	}

/* Sending the carrier return */
	while ( BroadcastCommand( "" ) <= 0 );
	return NORMAL;

/* Return for error */
err_return:
/* Sending the carrier return */
	while ( BroadcastCommand( "" ) <= 0 );
	return ERROR;
}
