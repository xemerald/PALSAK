/**
 * @file AGENT.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/PALSAK.h"
#include "./include/LEDINFO.h"

/* EEPROM Block 0 filling */
static BYTE BlockZero[EEPROM_SET_TOTAL_LENGTH];
/* Main socket */
static volatile int SockRecv;
static volatile int SockSend;
/* Global address info, expecially for broadcasting command */
static struct sockaddr_in TransmitAddr;
/* Input buffer */
static char RecvBuffer[RECVBUF_SIZE];
/* */
static int   InitBroadcastNetwork( void );
static int   RecvBlockZeroData( const uint );
static char *RecvCommand( const uint );
static int   SwitchCommand( const char * );
static int   BroadcastResp( char *, const uint   );
static int   EnrichBlockZero( void );
static int   EnrichSurveyResp( void );
static int   WriteDefSetting( void );
static int   CheckConsistency_Serial( void );
static int   CheckConsistency_CValue( void );
static int   OverrideFactory_Serial( void );
static int   OverrideFactory_CValue( void );

/**
 * @brief
 *
 */
void main( void )
{
	int   ret = 0;
	char *comm;
/* */
	InitLib();
	Init5DigitLed();
/* Initialization for network interface library */
	if ( NetStart() < 0 )
		return;
/* Wait for the network interface ready, it might be shoter */
	YIELD();
	Delay2(5);
/* Initialization for network interface library */
	if ( InitBroadcastNetwork() < 0 ) {
		SHOW_ERROR_5DIGITLED();
		Delay2(2000);
		return;
	}
/* */
	if ( (comm = RecvCommand( 250 )) ) {
	/* */
		memset(BlockZero, 0xff, EEPROM_SET_TOTAL_LENGTH);
	/* */
		switch ( SwitchCommand( comm ) ) {
		case AGENT_COMMAND_WBLOCK0:
		/* Send a byte to notify the master that here is ready for receiving data */
			sendto(SockSend, RecvBuffer, 1, MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr));
		/* */
			while ( RecvBlockZeroData( 250 ) != NORMAL )
				if ( ++ret >= NETWORK_OPERATION_RETRY )
					goto err_return;
		/* */
			if ( EnrichBlockZero() || WriteDefSetting() )
				goto err_return;
			break;
		case AGENT_COMMAND_CHECKCON:
		/* */
			if ( EnrichSurveyResp() )
				goto err_return;
			BroadcastResp( RecvBuffer, 250 );
			break;
		case AGENT_COMMAND_CORRECT:
		/* */
			if ( EnrichBlockZero() )
				goto err_return;
			if ( !strncmp(comm, "serial", 6) )
				OverrideFactory_Serial();
			else if ( !strncmp(comm, "cvalue", 6) )
				OverrideFactory_CValue();
			else
				goto err_return;
		/* */
			break;
		case AGENT_COMMAND_DHCP:
			break;
		case AGENT_COMMAND_QUIT:
			break;
		default:
		/* Unknown command from master */
			goto err_return;
		}
	}
	else {
		goto err_return;
	}

	SHOW_GOOD_5DIGITLED();
	Delay2(2000);

normal_return:
/* Close the sockets */
	closesocket(SockRecv);
	closesocket(SockSend);
/* Terminate the network interface */
	Nterm();
	return;
err_return:
/* If go into error condition, there will an "Error" reps. And show the 'ERROR' on the 7-seg led */
	strcat(RecvBuffer, "\rError\n");
	BroadcastResp( RecvBuffer, 250 );
	SHOW_ERROR_5DIGITLED();
	Delay2(2000);
	goto normal_return;
}

/**
 * @brief
 *
 * @return int
 */
static int InitBroadcastNetwork( void )
{
	char optval = 1;
	struct sockaddr_in _addr;

/* Close the previous sockets for following process */
	closesocket(SockSend);
	closesocket(SockRecv);
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
	if ( (SockRecv = socket(PF_INET, SOCK_DGRAM, 0)) < 0 )
		return ERROR;
/* Set the socket to reuse the address */
	if ( setsockopt(SockRecv, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 )
		return ERROR;
/* Bind the receiving socket to the port number 54321 */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	_addr.sin_port = htons(CONTROL_PORT);
	if ( bind(SockRecv, (struct sockaddr *)&_addr, sizeof(struct sockaddr)) < 0 )
		return ERROR;
/* Set the timeout of receiving socket to 0.25 sec. */
	SOCKET_RXTOUT(SockRecv, 250);

/* Set the transmitting address info */
	memset(&_addr, 0, sizeof(struct sockaddr));
	_addr.sin_family = AF_INET;
	_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	_addr.sin_port = htons(LISTEN_PORT);
	TransmitAddr = _addr;

	return NORMAL;
}

/**
 * @brief
 *
 * @param msec
 * @return int
 */
static int RecvBlockZeroData( const uint msec )
{
	int   ret = 0;
	int   fromlen = sizeof(struct sockaddr);
	int   remain = EEPROM_SET_TOTAL_LENGTH + 2;  /* Add another two bytes for CRC16 */
	BYTE *bufptr = (BYTE *)RecvBuffer;
	struct sockaddr_in _addr;

/* */
	if ( CRC16_MakeTable() )
		return ERROR;
	CRC16_Reset();
/* Flush the input buffer */
	memset(bufptr, 0, RECVBUF_SIZE);
	do {
	/* Show the "-L-" message on the 7-seg led */
		SHOW_2DASH_5DIGITLED( 0 );
		Show5DigitLedSeg(3, 0x0e);
	/* Receiving the command from the master & show the "-O-" message */
		if ( (ret = recvfrom(SockRecv, (char *)bufptr, remain, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
			Show5DigitLed(3, 0x00);
			Delay2(msec);
		}
		else {
			CRC16_AddDataN(bufptr, ret);
			bufptr += ret;
			remain -= ret;
		}
	} while ( remain );
/* */
	if ( !(ret = CRC16_Read()) ) {
		memcpy(BlockZero, RecvBuffer, EEPROM_SET_TOTAL_LENGTH);
		RecvBuffer[0] = ACK;
	}
	else {
		RecvBuffer[0] = NAK;
	}
/* Broadcasting the command to others */
	sendto(SockSend, RecvBuffer, 1, MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr));

	return RecvBuffer[0] == ACK ? NORMAL : ERROR;
}

/**
 * @brief
 *
 * @param msec
 * @return int
 */
static char *RecvCommand( const uint msec )
{
	int   ret = 0;
	int   fromlen = sizeof(struct sockaddr);
	int   remain = RECVBUF_SIZE - 1;
	char *result = NULL;
	char *bufptr = RecvBuffer;
	struct sockaddr_in _addr;

/* Flush the input buffer */
	memset(bufptr, 0, RECVBUF_SIZE);
	do {
	/* Show the "-L-" message on the 7-seg led */
		SHOW_2DASH_5DIGITLED( 0 );
		Show5DigitLedSeg(3, 0x0e);
		Delay2(msec);
	/* Receiving the command from the master & show the "-O-" message */
		if ( (ret = recvfrom(SockRecv, bufptr, remain, 0, (struct sockaddr *)&_addr, &fromlen)) <= 0 ) {
			Show5DigitLed(3, 0x00);
			Delay2(msec);
		}
		else {
			bufptr += ret;
			remain -= ret;
		/* Once received the carriage return, exit the loop */
			if ( *(bufptr - 1) == '\r' ) {
				result = RecvBuffer;
				break;
			}
		}
	} while ( 1 );
/* Replace the carriage return with the null terminator */
	*(bufptr - 1) = '\0';

	return result;
}

/**
 * @brief
 *
 * @return int
 */
static int SwitchCommand( const char *comm )
{
	int i;
/***/
#define X(a, b, c) b,
	char *agent_comm[] = {
		AGENT_COMMANDS_TABLE
	};
#undef X
/***/
#define X(a, b, c) c,
	int comm_len[] = {
		AGENT_COMMANDS_TABLE
	};
#undef X

/* Trim the input string from left */
	for ( ; isspace(*comm) && *comm; comm++ );
/* Switch the function by input command */
	for ( i = 0; i < AGENT_COMMAND_COUNT; i++ ) {
		if ( !strncmp(comm, agent_comm[i], comm_len[i]) ) {
		/* */
			for ( comm += comm_len[i]; isspace(*comm) && *comm; comm++ );
		/* */
			break;
		}
	}

	return i;
}

/**
 * @brief
 *
 * @param resp
 * @return int
 */
static int BroadcastResp( char *resp, const uint msec )
{
/* Broadcasting the command to others */
	sendto(SockSend, resp, strlen(resp), MSG_DONTROUTE, (struct sockaddr *)&TransmitAddr, sizeof(TransmitAddr));
	Delay2(msec);

	return NORMAL;
}

/**
 * @brief
 *
 * @return int
 */
static int EnrichBlockZero( void )
{
	WORD  l_opmode;
	WORD *r_opmode = (WORD *)&BlockZero[EEPROM_OPMODE_ADDR];

/* Those information should always be keep, include the device serial & correction value */
	if (
		!EE_MultiRead(0, EEPROM_SERIAL_ADDR, EEPROM_SERIAL_LENGTH, (char *)&BlockZero[EEPROM_SERIAL_ADDR]) &&
		!EE_MultiRead(0, EEPROM_CVALUE_ADDR, EEPROM_CVALUE_LENGTH, (char *)&BlockZero[EEPROM_CVALUE_ADDR])
	) {
	/* Then fetch the DHCP setting & keep it */
		if ( !EE_MultiRead(0, EEPROM_OPMODE_ADDR, EEPROM_OPMODE_LENGTH, (char *)&l_opmode) ) {
		/*
		 * 'cause the value stored within EEPROM in Big-Endian and the program is under Little-Endian.
		 * Here, we need a swap for the words first then continue the operation.
		 */
			SWAP_WORD_ASM( l_opmode );
			SWAP_WORD_ASM( *r_opmode );
		/* Just in case, turn the recv. block zero setting to disable DHCP function */
			*r_opmode &= ~OPMODE_BITS_MODE_DHCP;
		/* Write the local DHCP setting to the recv. block zero setting */
			*r_opmode |= l_opmode & OPMODE_BITS_MODE_DHCP;
		/* After the operaion, swap the word back */
			SWAP_WORD_ASM( *r_opmode );

			return NORMAL;
		}
	}

	return ERROR;
}

/**
 * @brief
 *
 * @return char
 */
static int EnrichSurveyResp( void )
{
	char *bufptr;
	BYTE *cvptr;
	BYTE  serial[EEPROM_SERIAL_LENGTH];
	BYTE  cvalue[EEPROM_CVALUE_LENGTH];

/* Read from EEPROM block 1 where factory setting within */
	if (
		!EE_MultiRead(1, EEPROM_SERIAL_ADDR, EEPROM_SERIAL_LENGTH, (char *)serial) &&
		!EE_MultiRead(1, EEPROM_CVALUE_ADDR, EEPROM_CVALUE_LENGTH, (char *)cvalue)
	) {
	/* Find the last position of the message */
		for ( bufptr = RecvBuffer; *bufptr; bufptr++ );
	/* */
		cvptr = &BlockZero[EEPROM_CVALUE_ADDR];
		sprintf(
			bufptr,
			"\rPalert Serial=%.5u:%.5u\n"
			"\rCorr. Values(1g)=%.2x-%.2x-%.2x-%.2x-%.2x-%.2x:%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n"
			"\rCorr. Values(0g)=%.2x-%.2x-%.2x-%.2x-%.2x-%.2x:%.2x-%.2x-%.2x-%.2x-%.2x-%.2x\n",
			(BlockZero[EEPROM_SERIAL_ADDR] << 8) + BlockZero[EEPROM_SERIAL_ADDR + 1],
			(serial[0] << 8) + serial[1],
			cvptr[0], cvptr[1], cvptr[2], cvptr[3], cvptr[4], cvptr[5],
			cvalue[0], cvalue[1], cvalue[2], cvalue[3], cvalue[4], cvalue[5],
			cvptr[6], cvptr[7], cvptr[8], cvptr[9], cvptr[10], cvptr[11],
			cvalue[6], cvalue[7], cvalue[8], cvalue[9], cvalue[10], cvalue[11]
		);

		return NORMAL;
	}

	return ERROR;
}

/**
 * @brief
 *
 * @return int
 */
static int WriteDefSetting( void )
{
	int   i;
	BYTE *dataptr;

/* */
	EE_WriteEnable();
	for ( i = 0x00, dataptr = BlockZero; i < EEPROM_SET_END_ADDR; i += 0x10, dataptr += 0x10 ) {
		if ( EE_MultiWrite(0, i, 0x10, (char *)dataptr) ) {
			EE_WriteProtect();
			return ERROR;
		}
	}
	EE_WriteProtect();

	return NORMAL;
}

/**
 * @brief
 *
 * @return int
 */
static int CheckConsistency_Serial( void )
{
	BYTE serial[EEPROM_SERIAL_LENGTH];

/* */
	if ( !EE_MultiRead(1, EEPROM_SERIAL_ADDR, EEPROM_SERIAL_LENGTH, (char *)serial) ) {
	/* */
		if ( BlockZero[EEPROM_SERIAL_ADDR] == serial[0] && BlockZero[EEPROM_SERIAL_ADDR + 1] == serial[1] )
			return 1;
		else
			return 0;
	/* */
	}

	return ERROR;
}

/**
 * @brief
 *
 * @return int
 */
static int CheckConsistency_CValue( void )
{
	BYTE cvalue[EEPROM_CVALUE_LENGTH];

/* */
	if ( !EE_MultiRead(1, EEPROM_CVALUE_ADDR, EEPROM_CVALUE_LENGTH, (char *)cvalue) ) {
		if ( !memcmp(&BlockZero[EEPROM_CVALUE_ADDR], cvalue, EEPROM_CVALUE_LENGTH) )
			return 1;
		else
			return 0;
	}

	return ERROR;
}

/**
 * @brief
 *
 * @return int
 */
static int OverrideFactory_Serial( void )
{
/* */
	EE_WriteEnable();
	if ( EE_MultiWrite(1, EEPROM_SERIAL_ADDR, EEPROM_SERIAL_LENGTH, (char *)&BlockZero[EEPROM_SERIAL_ADDR]) ) {
		EE_WriteProtect();
		return ERROR;
	}
	EE_WriteProtect();

	return NORMAL;
}

/**
 * @brief
 *
 * @return int
 */
static int OverrideFactory_CValue( void )
{
/* */
	EE_WriteEnable();
	if ( EE_MultiWrite(1, EEPROM_CVALUE_ADDR, EEPROM_CVALUE_LENGTH, (char *)&BlockZero[EEPROM_CVALUE_ADDR]) ) {
		EE_WriteProtect();
		return ERROR;
	}
	EE_WriteProtect();

	return NORMAL;
}
