/**
 * @file FTP.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2022-12-21
 *
 * @copyright Copyright (c) 2022
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
/* */
#include "./include/u7186EX/7186e.h"
#include "./include/u7186EX/Tcpip32.h"
/* */
#include "./include/FTP.h"
#include "./include/LEDINFO.h"

/* */
static int ConnectTCP( const char *, uint );
static int ReceiveTCP( const int, const int );
static int ParseFTPResp( void );
static int ProcessPASVResp( void );

/* External variable for TCP/IP error condition. NOTICE! it is different from standard errno.h. */
extern int errno;
/* */
#define INTERNAL_BUF_SIZE  256
static char InternalBuffer[INTERNAL_BUF_SIZE];
/* */
static volatile int MainSock = -1;
static volatile int PasvSock = -1;
/* */
#define SAFE_CLOSE_SOCKET(__SOCK) \
	{if ( (__SOCK) >= 0 ) { \
		closesocket((__SOCK)); \
		(__SOCK) = -1; \
	}}

/**
 * @brief
 *
 * @param host example: "192.168.255.1"
 * @param port 0~65535
 * @param user example: "Admin", 20 bytes max.
 * @param password example: "pass", 20 bytes max.
 * @return int
 * @retval FTP_SUCCESS(0) - Success.
 * @retval FTP_ERROR(-1)  - Error, can not connect to the FTP server
 */
int FTPConnect( const char *host, const uint port, const char *user, const char *password )
{
	ulong sendtime;
	ulong timeout = 2000;

/* Check is there existed main socket, just in case. */
	SAFE_CLOSE_SOCKET( MainSock );
/* */
	if ( (MainSock = ConnectTCP( host, port )) >= 0 ) {
		do {
			if ( ReceiveTCP( MainSock, TCP_CONNECT_TIMEOUT ) >= 0 ) {
				sendtime = GetTimeTicks();
			/* */
				switch ( ParseFTPResp() ) {
				case 220:
				/* Resp: 220 Welcome... */
					sprintf(InternalBuffer, "USER %s\r\n", user);
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
					break;
				case 331:
				/* Resp: 331 Please specify the password. */
				/*
				 * The time to pass the identification is much longer than other commands.
				 * Therefore, should increase the timeout of waiting for passing identification.
				 */
					timeout += 3000;
					sprintf(InternalBuffer, "PASS %s\r\n", password);
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
					break;
				case 230:
				/* Resp: 230 Login successfull. */
					timeout -= 3000;
					strcpy(InternalBuffer, "TYPE I\r\n");
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
					break;
				case 200:
				/* Resp: 200 Switching to Binary mode. */
					return FTP_SUCCESS;  /* Successful return */
				default:
					goto err_return;
				}
			}
		/* Timeout condition */
			else if ( errno == ETIMEDOUT ) {
				if ( (GetTimeTicks() - sendtime) > timeout ) {
					SHOW_TIMEOUT_5DIGITLED( 500 );
					goto err_return;
				}
			}
		/* Other Ethernet communication error */
			else {
				goto err_return;
			}
		} while ( 1 );
	}

err_return:
	SAFE_CLOSE_SOCKET( MainSock );
	YIELD();
	Delay2(1);

	return FTP_ERROR;
}

/**
 * @brief
 *
 * @param path
 * @param pattern
 * @param buf
 * @param buflen
 * @return int
 */
int FTPListDir( const char *path, const char *pattern, char *buf, const int buflen )
{
	int  ret;
	char * const fence = buf + buflen;
	uchar data_in = 0;
	ulong sendtime;

/* Check is there existed passive socket, just in case. */
	SAFE_CLOSE_SOCKET( PasvSock );
/* First, change the remote directory */
	sprintf(InternalBuffer, "CWD %s\r\n", path);
	send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
	sendtime = GetTimeTicks();
/* */
	while ( MainSock >= 0 ) {
		if ( data_in && PasvSock >= 0 ) {
			if ( (ret = ReceiveTCP( PasvSock, TCP_CONNECT_TIMEOUT )) > 0 ) {
				ret = (buf + ret) < fence ? ret : ((fence - buf) - 1);
				memcpy(buf, InternalBuffer, ret);
				buf += ret;
				continue;
			}
		/* Although we close the passive socket here, we should keep the socket number to indicate it already finish the process */
			closesocket(PasvSock);
			data_in = 0;
			*buf = '\0';
		}
		else if ( ReceiveTCP( MainSock, TCP_CONNECT_TIMEOUT ) >= 0 ) {
			sendtime = GetTimeTicks();
		/* */
			switch ( ParseFTPResp() ) {
			case 250:
			/* Resp: 250 Directory successfully changed. */
				if ( PasvSock < 0 ) {
					strcpy(InternalBuffer, "PASV\r\n");
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				}
				else {
				/* Finish the process */
					PasvSock = -1;
					return FTP_SUCCESS;
				}
				break;
			case 227:
			/* Resp: 227 Enter Passive Mode. (ip1,ip2,ip3,ip4,port1,port2) */
				if ( ProcessPASVResp() >= 0 ) {
					sprintf(InternalBuffer, "NLST %s\r\n", pattern);
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				}
				else {
					goto err_return;
				}
				break;
			case 150:
			/* Resp: 150 Here comes the directory listing. */
				data_in = 1;
				break;
			case 226:
			/* Resp: 226 Directory send OK. */
				strcpy(InternalBuffer, "CWD ~\r\n");
				send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				break;
			default:
				goto err_return;
			}
		}
	/* Timeout condition */
		else if ( errno == ETIMEDOUT ) {
			if ( (GetTimeTicks() - sendtime) > TCP_CONNECT_TIMEOUT ) {
				SHOW_TIMEOUT_5DIGITLED( 500 );
				goto err_return;
			}
		}
	/* Other Ethernet communication error */
		else {
			goto err_return;
		}
	}

err_return:
	SAFE_CLOSE_SOCKET( PasvSock );
	YIELD();
	Delay2(1);

	return FTP_ERROR;
}

/**
 * @brief
 *
 * @param path
 * @param rfname
 * @param lfname
 * @param disk
 * @return int
 */
int FTPRetrFile( const char *path, const char *rfname, const char *lfname, uint disk )
{
	int   ret;
	ulong data_in = 0;
	ulong data_write = 0;
	ulong sendtime;
	FILE_DATA fdata;

/* External variable for setting the file datatime mode to auto-writing mode */
	OS7_FileDateTimeMode = 2;
/* Check is there existed passive socket, just in case. */
	SAFE_CLOSE_SOCKET( PasvSock );
/* First, change the remote directory */
	sprintf(InternalBuffer, "CWD %s\r\n", path);
	send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
	sendtime = GetTimeTicks();
/* */
	while ( MainSock >= 0 ) {
		if ( data_in && PasvSock >= 0 ) {
			if ( (ret = ReceiveTCP( PasvSock, TCP_CONNECT_TIMEOUT )) > 0 ) {
				OS7_WriteFile(disk, InternalBuffer, ret);
				ShowProg5DigitsLed( (data_write += ret), data_in );
				continue;
			}
		/* */
			memset(fdata.fname, 0, sizeof(fdata.fname));
			memcpy(fdata.fname, lfname, (ret = strlen(lfname)) > sizeof(fdata.fname) ? sizeof(fdata.fname) : ret );
			OS7_CloseWriteFile(disk, &fdata);
			ShowProg5DigitsLed( data_in, data_in );
			Delay2(100);
		/* */
			closesocket(PasvSock);
			PasvSock = -1;
			data_in = 0;
		}
		else if ( ReceiveTCP( MainSock, TCP_CONNECT_TIMEOUT ) >= 0 ) {
			sendtime = GetTimeTicks();
		/* */
			switch ( ParseFTPResp() ) {
			case 250:
			/* Resp: 250 Directory successfully changed. */
				if ( !data_write ) {
					sprintf(InternalBuffer, "SIZE %s\r\n", rfname);
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				}
				else {
				/* Finish the process */
					return FTP_SUCCESS;
				}
				break;
			case 213:
			/* Resp: 213 FILE_SIZE */
				sscanf(InternalBuffer, "%*d %lu", &data_write);
			/* */
				strcpy(InternalBuffer, "PASV\r\n");
				send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				break;
			case 227:
			/* Resp: 227 Enter Passive Mode. (ip1,ip2,ip3,ip4,port1,port2) */
				if ( ProcessPASVResp() >= 0 ) {
					sprintf(InternalBuffer, "RETR %s\r\n", rfname);
					send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				}
				else {
					goto err_return;
				}
				break;
			case 150:
			/* Resp: 150 Opening BINARY mode data connection for... */
				data_in = data_write;
				data_write = 0;
				OS7_OpenWriteFile(disk);
				ShowProg5DigitsLed( data_write, data_in );
				Delay2(100);
				break;
			case 226:
			/* Resp: 226 Transfer complete. */
				strcpy(InternalBuffer, "CWD ~\r\n");
				send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
				break;
			default:
				goto err_return;
			}
		}
	/* Timeout condition */
		else if ( errno == ETIMEDOUT ) {
			if ( (GetTimeTicks() - sendtime) > TCP_CONNECT_TIMEOUT ) {
				SHOW_TIMEOUT_5DIGITLED( 500 );
				goto err_return;
			}
		}
	/* Other Ethernet communication error */
		else {
			goto err_return;
		}
	};

err_return:
	SAFE_CLOSE_SOCKET( PasvSock );
	YIELD();
	Delay2(1);

	return FTP_ERROR;
}

/**
 * @brief
 *
 */
void FTPClose( void )
{
/* */
	if ( MainSock >= 0 ) {
		strcpy(InternalBuffer, "QUIT\r\n");
		send(MainSock, InternalBuffer, strlen(InternalBuffer), 0);
	/* */
		if ( ReceiveTCP( MainSock, TCP_CONNECT_TIMEOUT ) >= 0 ) {
		/* */
			switch ( ParseFTPResp() ) {
			case 221:
			/* Resp: 221 Goodbye. */
				break;
			default:
				break;
			}
		}
	}
/* */
	SAFE_CLOSE_SOCKET( MainSock );
	YIELD();
	Delay2(1);

	return;
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
						SHOW_TIMEOUT_5DIGITLED( 500 );
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
	return FTP_ERROR;
}

/**
 * @brief
 *
 * @param sock
 * @param timeout
 * @return int
 */
static int ReceiveTCP( const int sock, const int timeout )
{
	int result = FTP_ERROR;

/* */
	SOCKET_RXTOUT(sock, timeout);
	result = recv(sock, InternalBuffer, INTERNAL_BUF_SIZE, 0);
	SOCKET_RXTOUT(sock, TOUT_READ);
/* */
	if ( result < INTERNAL_BUF_SIZE )
		InternalBuffer[result < 0 ? 0 : result] = '\0';

	return result;
}

/**
 * @brief
 *
 * @return int
 */
static int ParseFTPResp( void )
{
	int result = 0;

	if ( sscanf(InternalBuffer, "%d %*s", &result) == 1 )
	/* Display "XXX" on the 7-seg led, last for 250 msec. */
		ShowAll5DigitLedSeg( 0x00, ShowData[InternalBuffer[0] - '0'], ShowData[InternalBuffer[1] - '0'], ShowData[InternalBuffer[2] - '0'] | 0x80, 0x00, 250 );

	return result;
}

/**
 * @brief
 *
 * @param pasv_resp
 * @return int
 */
static int ProcessPASVResp( void )
{
	char *host = NULL;
	char *pos = InternalBuffer;
	uchar tmp[2];

/* */
	for ( pos = InternalBuffer, tmp[0] = 0; *pos; pos++ ) {
		if ( *pos == '(' ) {
			host = pos + 1;
		}
		else if ( *pos == ',' ) {
			if ( ++tmp[0] >= 4 ) {
				*pos = '\0';
				pos++;
				break;
			}
			*pos = '.';
		}
	}
/* */
	sscanf(pos, "%hu,%hu", &tmp[0], &tmp[1]);

	return (PasvSock = ConnectTCP( host, (tmp[0] << 8) + tmp[1] ));
}
