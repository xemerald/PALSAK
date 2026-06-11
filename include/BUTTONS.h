/**
 * @file BUTTONS.h
 * @author your name (you@domain.com)
 * @brief
 * @date 2024-06-29
 *
 * @copyright Copyright (c) 2024
 *
 */
#ifndef __PALSAK_BUTTONS_H__
#define __PALSAK_BUTTONS_H__

#ifdef __cplusplus
extern "C" {
#endif
/*
 *
 */
#include "./u7186EX/7186e.h"

/* The status of pin */
#define PIN_IS_OPEN   0
#define PIN_IS_CLOSE  1

/* The status of button */
#define BUTTON_IS_RELEASE  0
#define BUTTON_IS_PRESS    1

/**
 * @name External variables
 *
 */
extern volatile uchar InitPressCount;
extern volatile uchar InitPressLastCount;
extern volatile uchar CtsPressCount;
extern volatile uchar CtsPressLastCount;

/* */
#define BUTTONS_LASTCOUNT_RESET() \
		{ InitPressLastCount = InitPressCount; CtsPressLastCount = CtsPressLastCount; }

/**
 * @name
 *
 */
void  ButtonService( void );
void  InitButtonService( void );
uchar GetInitButtonPressCount( void );
uchar GetCtsButtonPressCount( void );
uchar IsBothButtonPress( void );

/* */
#define START_BUTTONS_SERVICE() \
		{ SetRtsActive_1(); InstallUserTimer0Function_ms(50, ButtonService); }
#define STOP_BUTTONS_SERVICE() \
		{ StopUserTimer0Fun(); SetRtsInactive_1(); }

#ifdef __cplusplus
}
#endif
#endif
