/**
 * @file LEDINFO.h
 * @author Benjamin Ming Yang (b98204032@gmail.com) in Department of Geology of National Taiwan University
 * @brief
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
#define SHOW_2DASH_5DIGITLED(SEQ, DATA) \
		ShowAll5DigitLedSeg(0, SeqMark[(SEQ)], DATA, SeqMark[(SEQ)], 0)

/* Display "Good." on the 7-seg led */
#define SHOW_GOOD_5DIGITLED() \
		ShowAll5DigitLedSeg(0x5e, 0x1d, 0x1d, ShowData[0x0d], 0x80)

/* Display "Error." on the 7-seg led */
#define SHOW_ERROR_5DIGITLED() \
		ShowAll5DigitLedSeg(ShowData[0x0e], 0x05, 0x05, 0x1d, 0x85)

/* Display "t.o." on the 7-seg led */
#define SHOW_TIMEOUT_5DIGITLED() \
		ShowAll5DigitLedSeg(0x00, 0x91, 0x9d, 0x00, 0x00)

/* Mark for seq. */
extern const BYTE SeqMark[];
/* */
extern uint ContentLength;
extern uint ContentPages;

/* */
void  ShowAll5DigitLedSeg( BYTE, BYTE, BYTE, BYTE, BYTE );
void  ShowProg5DigitsLed( ulong, const ulong );
void  ShowContent5DigitsLedPage( uint );
void  ShowContent5DigitsLedRoller( uint );
BYTE *SetDisplayContent( const BYTE *, uint );
BYTE *EncodeAddrDisplayContent( const char * );

#ifdef __cplusplus
}
#endif
#endif
