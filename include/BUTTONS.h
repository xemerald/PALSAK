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
#include "./include/u7186EX/7186e.h"

/* The checking result of func. */
#define PIN_IS_OPEN   0
#define PIN_IS_CLOSE  1

/*
 *
 */
extern volatile uchar InitPressCount;
extern volatile uchar InitPressLastCount;
extern volatile uchar CtsPressCount;
extern volatile uchar CtsPressLastCount;

/* */
#define BUTTONS_LASTCOUNT_RESET() \
		{ InitPressLastCount = InitPressCount; CtsPressLastCount = CtsPressLastCount; }

/*
 *
 */
void  ButtonService( void );
void  ButtonServiceInit( void );
uchar GetInitButtonPressCount( void );
uchar GetCtsButtonPressCount( void );

/* */
#define BUTTONS_SERVICE_START() \
		{ SetRtsActive_1(); InstallUserTimer0Function_ms(50, ButtonService); }
#define BUTTONS_SERVICE_STOP() \
		{ StopUserTimer0Fun(); SetRtsInactive_1(); }

#ifdef __cplusplus
}
#endif
#endif
