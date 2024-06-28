/**
 * @file FTP.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2022-12-22
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __PALSAK_FTP_H__
#define __PALSAK_FTP_H__

#ifdef __cplusplus
extern "C" {
#endif
/* */
#include "./include/u7186EX/7186e.h"
/* The checking result of func. */
#define FTP_SUCCESS   0
#define FTP_ERROR    -1
#define FTP_WARNING  -2
/* TCP Connection timeout, unit is mseconds */
#define TCP_CONNECT_TIMEOUT  5000L

int FTPConnect( const char *, const uint, const char *, const char * );
int FTPListDir( const char *, const char *, char *, const int );
int FTPRetrFile( const char *, const char *, const char *, uint );
void FTPClose( void );

#ifdef __cplusplus
}
#endif
#endif
