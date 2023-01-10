/**
 * @file FILE.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2023-01-10
 *
 * @copyright Copyright (c) 2023
 *
 */
/* */
#include <stdio.h>
#include <string.h>
/* */
#include "./include/u7186EX/7186e.h"

/**
 * @brief
 *
 * @param fileptr
 * @param mark
 * @param order
 * @param start
 * @return char*
 */
ulong FileSeek( const FILE_DATA far *fileptr, const char mark, const uint order, ulong start )
{
	uint      _order = 1;
	char far *c_ptr;

/* */
	if ( fileptr ) {
		c_ptr  = AddFarPtrLong(fileptr->addr, start);
		while ( start++ < (fileptr->size - 1) ) {
			if ( *c_ptr == mark && _order++ == order )
				return start;
			c_ptr = AddFarPtrLong(c_ptr, 1);
		}
	}


	return 0;
}

/**
 * @brief Get the target string from file
 *
 * @param fileptr
 * @param key
 * @param def
 * @param dest
 * @param dest_size
 * @return char*
 */
char *GetFileStr( const FILE_DATA far *fileptr, const char *key, const char *def, char *dest, const size_t dest_size )
{
	ulong scan_pos = 0;
	char  buf[32]  = { 0 };

/* */
	if ( !fileptr )
		goto def_return;
/* */
	memset(dest, 0, dest_size);
/* */
	while ( scan_pos < fileptr->size ) {
	/* To find the 1st '*' from scan_pos */
		if ( !(scan_pos = FileSeek( fileptr, '*', 1, scan_pos )) )
			goto def_return;  /* Cannot find '*' */
	/* */
		sscanf(AddFarPtrLong(fileptr->addr, scan_pos), "%31s", buf);
	/* */
		if ( !(strncmp(key, buf, strlen(key))) ) {
		/* To find the 1st '=' from scan_pos */
			if ( !(scan_pos = FileSeek( fileptr, '=', 1, scan_pos )) )
				goto def_return;  /* Cannot find '=' */
		/* */
			buf[0] = '\0';
			sscanf(AddFarPtrLong(fileptr->addr, scan_pos), "%31s", buf);
		/* From this point we will use the scan_pos to store the result length, 'cause the scaning position isn't used any more */
			if ( !(scan_pos = strlen(buf)) )
				goto def_return;  /* Cannot find string */
		/* */
			strncpy(dest, buf, scan_pos >= dest_size ? (dest_size - 1) : scan_pos);
			break;
		}
	}

normal_return:
	return dest;
def_return:
	strcpy(dest, def);
	goto normal_return;
}
