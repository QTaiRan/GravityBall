/**
  ******************************************************************************
  * @file    buzzer.c
  * @brief   Passive buzzer sound effects - TIM2 CH1 PWM on PA0
  *          TIM2 clock = 72MHz, PSC=71 -> 1MHz tick, ARR=1MHz/freq
  ******************************************************************************
  */
#include "buzzer.h"
#include "main.h"

static uint32_t tone_end_ms;

void buzzer_init(void)
{
  tone_end_ms = 0;
}

void buzzer_on(uint16_t freq)
{
  uint32_t arr;
  if (freq < 500) freq = 500;
  if (freq > BUZZER_MAX_FREQ) freq = BUZZER_MAX_FREQ;
  arr = 1000000u / freq;
  if (arr > 65535) arr = 65535;
  BUZZER_TIM.Instance->ARR = arr - 1;
  BUZZER_TIM.Instance->CCR1 = (arr - 1) / 2;
}

void buzzer_off(void)
{
  BUZZER_TIM.Instance->CCR1 = 0;
}

void buzzer_tone(uint16_t freq, uint16_t ms)
{
  buzzer_on(freq);
  tone_end_ms = HAL_GetTick() + ms;
}

void buzzer_tick(void)
{
  if (tone_end_ms && HAL_GetTick() >= tone_end_ms)
  {
    tone_end_ms = 0;
    buzzer_off();
  }
}

static void beep_seq(const uint16_t *seq, uint16_t n)
{
  uint16_t i;
  for (i = 0; i < n; i++)
  {
    if (seq[i])
      buzzer_tone(seq[i], seq[i + 1]);
    else
      buzzer_off();
    i++;
    HAL_Delay(seq[i]);
    buzzer_off();
    HAL_Delay(20);
  }
}

void sfx_menu_move(void)  { buzzer_tone(2500, 25); }
void sfx_menu_ok(void)    { buzzer_tone(3000, 60); }
void sfx_coin(void)       { buzzer_tone(4000, 40); }

void sfx_levelup(void)
{
  uint16_t s[] = { 2000, 60, 3000, 60, 4000, 80 };
  beep_seq(s, 3);
}

void sfx_hit(void)
{
  uint16_t s[] = { 2800, 60, 2000, 60, 1500, 80 };
  beep_seq(s, 3);
}

void sfx_gameover(void)
{
  uint16_t s[] = { 2000, 120, 0, 80, 1500, 120, 0, 80, 1000, 250 };
  beep_seq(s, 5);
}

void sfx_ready(void)
{
  uint16_t s[] = { 3500, 50, 3500, 50, 4500, 90 };
  beep_seq(s, 3);
}
