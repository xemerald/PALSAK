/**
 * @file FILE.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2023-01-10
 *
 * @copyright Copyright (c) 2023
 *
 */
#ifndef __PALSAK_FILE_H__
#define __PALSAK_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif
/* */
#include "./include/u7186EX/7186e.h"
/* */
ulong FileSeek( const FILE_DATA far *, const char, const uint, ulong );
char *GetFileStr( const FILE_DATA far *, const char *, const char *, char *, const size_t );

#ifdef __cplusplus
}
#endif
#endif
