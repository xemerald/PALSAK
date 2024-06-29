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
/* */
#define BUTTONS_SERVICE_START() \
		{ SetRtsActive_1(); InstallUserTimer1Function_ms(50, ButtonService); }
#define BUTTONS_SERVICE_STOP() \
		{ StopUserTimer1Fun(); SetRtsInactive_1(); }

/*
 *
 */
extern volatile uchar InitPressCount;
extern volatile uchar CtsPressCount;

/*
 *
 */
void  ButtonService( void );
void  ButtonServiceInit( void );
uchar GetInitButtonPressCount( void );
uchar GetCtsButtonPressCount( void );

#ifdef __cplusplus
}
#endif
#endif