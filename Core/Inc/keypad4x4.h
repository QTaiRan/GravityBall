/**
  ******************************************************************************
  * @file    keypad4x4.h
  * @brief   4x4 matrix keypad driver header
  ******************************************************************************
  */
#ifndef __KEYPAD4X4_H
#define __KEYPAD4X4_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

void    keypad_init(void);
void    keypad_scan(void);                 /* call ~50Hz */
uint8_t keypad_get_key(void);              /* returns 1..16 or 0 (edge only) */
uint8_t keypad_get_hold(void);             /* returns 1..16 or 0 (raw held) */

#ifdef __cplusplus
}
#endif

#endif /* __KEYPAD4X4_H */
