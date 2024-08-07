/**
 * @file LEDINFO.C
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @date 2022-12-22
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <string.h>
#include <ctype.h>
/* */
#include "./include/u7186EX/7186e.h"
/* */
#include "./include/PALSAK.h"

/* */
#define DISPLAY_CONTENT_MAX    32
#define DISPLAY_CHAR_PER_PAGE  4
/* Mark for seq. */
const BYTE SeqMark[] = { 0x01, 0x48, 0x49, 0x36, 0x60, 0x18, 0x0c, 0x42 };
/* */
uint ContentLength = 0;
uint ContentPages = 0;
/* */
static BYTE DisplayContent[DISPLAY_CONTENT_MAX] = { 0x00 };

/**
 * @brief
 *
 * @param data_1
 * @param data_2
 * @param data_3
 * @param data_4
 * @param data_5
 * @param delay_msec
 */
void ShowAll5DigitLedSeg( BYTE data_1, BYTE data_2, BYTE data_3, BYTE data_4, BYTE data_5, uint delay_msec )
{
	Show5DigitLedSeg(1, data_1);
	Show5DigitLedSeg(2, data_2);
	Show5DigitLedSeg(3, data_3);
	Show5DigitLedSeg(4, data_4);
	Show5DigitLedSeg(5, data_5);
/* */
	if ( delay_msec )
		Delay2(delay_msec);

	return;
}

/**
 * @brief Showing the progressing percentage on the 7-seg led.
 *
 * @param nblock The accumulated blocks number.
 * @param totalblocks The total blocks number.
 */
void ShowProg5DigitsLed( ulong nblock, const ulong totalblocks )
{
	ulong fval;
/*
 * Compute the percentage of progress, ival stand for integer part of value,
 * fval stand for fraction part of value.
 */
	nblock *= 100L;
	fval    = nblock;
	if ( totalblocks ) {
		nblock = (ulong)(nblock / totalblocks);
		fval = (ulong)(fval * 100L / totalblocks) - nblock * 100L;
	}
	else {
		nblock = 0;
		fval = 0;
	}

/* Display progressing percentage on the 7-seg led */
	if ( nblock < 100 ) {
		Show5DigitLedSeg(1, 0);
		Show5DigitLed(2, nblock >= 10 ? nblock / 10 : 16);
		Show5DigitLedWithDot(3, nblock % 10);
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

	return;
}

/**
 * @brief
 *
 * @param page
 */
void ShowContent5DigitsLedPage( uint page )
{
/* Fisrt, display the page mark */
	page %= ContentPages;
	Show5DigitLedSeg(1, SeqMark[page]);
/* */
	page *= DISPLAY_CHAR_PER_PAGE;
/* Every 4 chars will be display on the same page */
	Show5DigitLedSeg(2, (int)DisplayContent[page++]);
	Show5DigitLedSeg(3, (int)DisplayContent[page++]);
	Show5DigitLedSeg(4, (int)DisplayContent[page++]);
	Show5DigitLedSeg(5, (int)DisplayContent[page  ]);

	return;
}

/**
 * @brief
 *
 * @param msec
 * @param start_pos
 */
void ShowContent5DigitsLedRoller( uint start_pos )
{
/* From Led-1 to Led-5 */
	Show5DigitLedSeg(1, (int)DisplayContent[start_pos++ % ContentLength]);
	Show5DigitLedSeg(2, (int)DisplayContent[start_pos++ % ContentLength]);
	Show5DigitLedSeg(3, (int)DisplayContent[start_pos++ % ContentLength]);
	Show5DigitLedSeg(4, (int)DisplayContent[start_pos++ % ContentLength]);
	Show5DigitLedSeg(5, (int)DisplayContent[start_pos   % ContentLength]);

	return;
}

/**
 * @brief Set the Roller Content object
 *
 * @param src
 * @param src_len
 * @return BYTE*
 */
BYTE *SetDisplayContent( const BYTE *src, uint src_len )
{
/* */
	src_len = src_len > DISPLAY_CONTENT_MAX ? DISPLAY_CONTENT_MAX : src_len;
/* */
	memcpy(DisplayContent, src, src_len);
	ContentLength = src_len;
	ContentPages = (src_len / DISPLAY_CHAR_PER_PAGE) + (src_len % DISPLAY_CHAR_PER_PAGE ? 1 : 0);

	return DisplayContent;
}

/**
 * @brief
 *
 * @param src
 * @return BYTE*
 */
BYTE *EncodeAddrDisplayContent( const char *src )
{
	BYTE *dest = DisplayContent;
	uint  rlen = 0;

/* Skip all the dot and colon */
	while ( *src && rlen < DISPLAY_CONTENT_MAX ) {
	/* Turn the digit to number include 'a' to 'f' (for HEX). And other char will be set to space */
		switch ( *src ) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			*dest = ShowData[*src - '0'];
			break;
		case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
		case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
			*dest = ShowData[(tolower(*src) - 'a') + 0x0a];
			break;
		case '-':
			*dest = ShowData[0x11];
			break;
		case '.': case ':':
			*dest = 0x80;
			break;
		default:
			*dest = 0x00;
			break;
		}
	/* */
		src++;
	/* Check the next char. If it is dot or colon, here will add the dot display. And skip it */
		if ( (*src == '.' || *src == ':') && *dest != 0x80 ) {
			*dest |= 0x80;
			src++;
		}
	/* */
		dest++;
		rlen++;
	}
/* */
	ContentLength = rlen;
	ContentPages = (rlen / DISPLAY_CHAR_PER_PAGE) + (rlen % DISPLAY_CHAR_PER_PAGE ? 1 : 0);

	return DisplayContent;
}
