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
#include "../include/u7186EX/7186e.h"
/* */
#include "../include/BUTTONS.h"

/* */
volatile uchar InitPressCount;
volatile uchar InitPressLastCount;
volatile uchar CtsPressCount;
volatile uchar CtsPressLastCount;
/* */
static uchar InitPinStatus;
static uchar CtsPinStatus;
static uchar BothPressStatus;
static uchar LastBothPressStatus;

/**
 * @brief
 *
 */
void ButtonService( void )
{
/* When the red button is pressed, the init. pin will be closed */
	if ( ReadInitPin() ) {
		InitPinStatus = PIN_IS_CLOSE;
	}
	else if ( InitPinStatus == PIN_IS_CLOSE ) {
		InitPressCount += BothPressStatus ? 0 : 1;
		InitPinStatus = PIN_IS_OPEN;
	}

/* When the black button is pressed, the cts. pin will be opened */
	if ( !GetCtsStatus_1() ) {
		CtsPinStatus = PIN_IS_OPEN;
	}
	else if ( CtsPinStatus == PIN_IS_OPEN ) {
		CtsPressCount += BothPressStatus ? 0 : 1;
		CtsPinStatus = PIN_IS_CLOSE;
	}

	if ( InitPinStatus == PIN_IS_CLOSE && CtsPinStatus == PIN_IS_OPEN )
		BothPressStatus = BUTTON_IS_PRESS;
	else if ( BothPressStatus && InitPinStatus == PIN_IS_OPEN && CtsPinStatus == PIN_IS_CLOSE )
		BothPressStatus = BUTTON_IS_RELEASE;

	return;
}

/**
 * @brief
 *
 */
void InitButtonService( void )
{
/* The normal stated of init. pin is open */
	InitPressCount = InitPressLastCount = 0;
	InitPinStatus  = PIN_IS_OPEN;
/* The normal stated of cts. pin is close */
	CtsPressCount = CtsPressLastCount = 0;
	CtsPinStatus  = PIN_IS_CLOSE;
/* */
	LastBothPressStatus = BothPressStatus = BUTTON_IS_RELEASE;

	return;
}

/**
 * @brief Get the Init Button Press Count
 *
 * @return uchar
 */
uchar GetInitButtonPressCount( void )
{
	uchar result = InitPressCount;

	if ( result > InitPressLastCount )
		InitPressLastCount = result - InitPressLastCount;
	else if ( result < InitPressLastCount )
		InitPressLastCount = (0xff - InitPressLastCount) + result + 1;
	else
		return 0;
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
	uchar result = CtsPressCount;

	if ( result > CtsPressLastCount )
		CtsPressLastCount = result - CtsPressLastCount;
	else if ( result < CtsPressLastCount )
		CtsPressLastCount = (0xff - CtsPressLastCount) + result + 1;
	else
		return 0;
/* */
	_asm {
		mov al, byte ptr CtsPressLastCount
		xchg al, byte ptr result
		mov byte ptr CtsPressLastCount, al
	}

	return result;
}

/**
 * @brief
 *
 * @return uchar
 */
uchar IsBothButtonPress( void )
{
/* */
	if ( !LastBothPressStatus && BothPressStatus ) {
		LastBothPressStatus = BothPressStatus;
		return BUTTON_IS_PRESS;
	}
	else if ( !BothPressStatus ) {
		LastBothPressStatus = BothPressStatus;
	}

	return BUTTON_IS_RELEASE;
}
