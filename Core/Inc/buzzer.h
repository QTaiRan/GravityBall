/**
  ******************************************************************************
  * @file    buzzer.h
  * @brief   Passive buzzer (TIM2 CH1 / PA0 PWM) sound effect driver header
  ******************************************************************************
  */
#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 连接: 蜂鸣器 IN -> PA0 (9012 三极管驱动), VCC -> 5V/3.3V */
#define BUZZER_TIM          htim2
#define BUZZER_TIM_CH       TIM_CHANNEL_1
#define BUZZER_MAX_FREQ     5000   /* 无源蜂鸣器有效频段 2k-5kHz */

void buzzer_init(void);
void buzzer_on(uint16_t freq);
void buzzer_off(void);
void buzzer_tone(uint16_t freq, uint16_t ms);  /* non-blocking */
void buzzer_tick(void);                        /* call in main loop */

void sfx_menu_move(void);
void sfx_menu_ok(void);
void sfx_coin(void);
void sfx_levelup(void);
void sfx_hit(void);
void sfx_gameover(void);
void sfx_ready(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
