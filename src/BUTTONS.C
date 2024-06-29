/**
 * @file BUTTONS.C
 * @author your name (you@domain.com)
 * @brief
 * @date 2024-06-29
 *
 * @copyright Copyright (c) 2024
 *
 */

/* */
#include "./include/u7186EX/7186e.h"
/* */
#include "./include/BUTTONS.h"

/* */
uchar InitPressCount;
uchar CtsPressCount;
/* */
static uchar InitPinStatus;
static uchar InitPressLastCount;
static uchar CtsPinStatus;
static uchar CtsPressLastCount;

/**
 * @brief
 *
 */
void ButtonService( void )
{
	if ( ReadInitPin() ) {
		InitPinStatus = PIN_IS_NOT_OPEN;
	}
	else if ( InitPinStatus ) {
		InitPressCount++;
		InitPinStatus = PIN_IS_OPEN;
	}

	if ( GetCtsStatus_1() ) {
		CtsPinStatus = PIN_IS_NOT_OPEN;
	}
	else if ( CtsPinStatus ) {
		CtsPressCount++;
		CtsPinStatus = PIN_IS_OPEN;
	}

	return;
}

/**
 * @brief
 *
 */
void ButtonServiceInit( void )
{
/* */
	InitPressCount = InitPressLastCount = 0;
	InitPinStatus  = PIN_IS_OPEN;
/* */
	CtsPressCount = CtsPressLastCount = 0;
	CtsPinStatus  = PIN_IS_OPEN;

	return;
}

/**
 * @brief Get the Init Button Press Count
 *
 * @return uchar
 */
uchar GetInitButtonPressCount( void )
{
	uchar result = 0;

	result = InitPressCount;
	if ( result > InitPressLastCount )
		InitPressLastCount = result - InitPressLastCount;
	else if ( result < InitPressLastCount )
		InitPressLastCount = (0xff - InitPressLastCount) + result + 1;
/* */
	_asm {
		mov al, byte ptr InitPressLastCount
		xchg al, byte ptr result
		mov byte ptr InitPressLastCount, al
	}

	return result;
}

/**
 * @brief Get the Cts Button Press Count
 *
 * @return uchar
 */
uchar GetCtsButtonPressCount( void )
{
	uchar result = 0;

	result = CtsPressCount;
	if ( result > CtsPressLastCount )
		CtsPressLastCount = result - CtsPressLastCount;
	else if ( result < CtsPressLastCount )
		CtsPressLastCount = (0xff - CtsPressLastCount) + result + 1;
/* */
	_asm {
		mov al, byte ptr CtsPressLastCount
		xchg al, byte ptr result
		mov byte ptr CtsPressLastCount, al
	}

	return result;
}
