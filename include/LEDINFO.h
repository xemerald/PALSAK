/**
 * @file LEDINFO.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
 * @version 0.1
 * @date 2022-12-30
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifndef __LEDINFO_H__
#define __LEDINFO_H__

#ifdef __cplusplus
extern "C" {
#endif
/* */
#include "./include/u7186EX/7186e.h"

/* The macro to showing " - - " on the 7-seg led */
#define SHOW_2DASH_5DIGITLED(SEQ) \
		Show5DigitLedSeg(1, 0); \
		Show5DigitLedSeg(2, SeqMark[(SEQ)]); \
		Show5DigitLedSeg(3, 0); \
		Show5DigitLedSeg(4, SeqMark[(SEQ)]); \
		Show5DigitLedSeg(5, 0);

/* Display "Good." on the 7-seg led */
#define SHOW_GOOD_5DIGITLED() \
		Show5DigitLedSeg(1, 0x5e); \
		Show5DigitLedSeg(2, 0x1d); \
		Show5DigitLedSeg(3, 0x1d); \
		Show5DigitLed(4, 0x0d); \
		Show5DigitLedSeg(5, 0x80);

/* Display "Error." on the 7-seg led */
#define SHOW_ERROR_5DIGITLED() \
		Show5DigitLed(1, 0x0e); \
		Show5DigitLedSeg(2, 0x05); \
		Show5DigitLedSeg(3, 0x05); \
		Show5DigitLedSeg(4, 0x1d); \
		Show5DigitLedSeg(5, 0x85);
/* */
#define SHOW_TIMEOUT_5DIGITLED() \
		Show5DigitLedSeg(1, 0x00); \
		Show5DigitLedSeg(2, 0x91); \
		Show5DigitLedSeg(3, 0x9d); \
		Show5DigitLedSeg(4, 0x00); \
		Show5DigitLedSeg(5, 0x00);

/* Mark for seq. */
extern const BYTE SeqMark[];
/* */
extern uint ContentLength;
extern uint ContentPages;

/* */
void ShowProg5DigitsLed( ulong, const ulong );
void ShowContent5DigitsLedPage( uint );
void ShowContent5DigitsLedRoller( uint );
BYTE *SetDisplayContent( const BYTE *, uint );
BYTE *EncodeAddrDisplayContent( const char * );

#ifdef __cplusplus
}
#endif
#endif
