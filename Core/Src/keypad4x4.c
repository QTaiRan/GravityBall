/**
  ******************************************************************************
  * @file    keypad4x4.c
  * @brief   4x4 matrix keypad driver with debounce
  *
  * Wiring (user spec):
  *   key 1 = pin4+pin5, key 2 = pin3+pin5, ...  key 16 = pin1+pin8
  *   Pin1-4 = rows, Pin5-8 = columns
  *   Pin1 -> PA4(rows), Pin2 -> PA3, Pin3 -> PA2, Pin4 -> PA1
  *   Pin5 -> PA5(cols), Pin6 -> PA6, Pin7 -> PA7, Pin8 -> PB0
  *
  * Layout:  [ 1][ 2][ 3][ 4]
  *          [ 5][ 6][ 7][ 8]
  *          [ 9][10][11][12]
  *          [13][14][15][16]
  ******************************************************************************
  */
#include "keypad4x4.h"

/* rowIdx: 0 = pin4(PA1), 1 = pin3(PA2), 2 = pin2(PA3), 3 = pin1(PA4) */
static const GPIO_TypeDef *row_port[4] = { GPIOA, GPIOA, GPIOA, GPIOA };
static const uint16_t      row_pin[4]  = { GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4 };
/* colIdx: 0 = pin5(PA5), 1 = pin6(PA6), 2 = pin7(PA7), 3 = pin8(PB0) */
static const GPIO_TypeDef *col_port[4] = { GPIOA, GPIOA, GPIOA, GPIOB };
static const uint16_t      col_pin[4]  = { GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7, GPIO_PIN_0 };

static uint8_t last_key, stable_key, stable_cnt, prev_stable;
static uint8_t raw_hold;

/* 按键映射表 [行索引][列索引] -> 按键编号
   行索引: 0=Pin4(PA1)  1=Pin3(PA2)  2=Pin2(PA3)  3=Pin1(PA4)
   列索引: 0=Pin5(PA5)  1=Pin6(PA6)  2=Pin7(PA7)  3=Pin8(PB0) */
static const uint8_t key_map[4][4] = {
  {  1,  5,  9, 13 },   /* Pin4 行: 键1,5,9,13 */
  {  2,  6, 10, 14 },   /* Pin3 行: 键2,6,10,14 */
  {  3,  7, 11, 15 },   /* Pin2 行: 键3,7,11,15 */
  {  4,  8, 12, 16 },   /* Pin1 行: 键4,8,12,16 */
};

void keypad_init(void)
{
  GPIO_InitTypeDef gpio = {0};
  uint8_t i;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  for (i = 0; i < 4; i++)
  {
    gpio.Pin   = row_pin[i];
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init((GPIO_TypeDef *)row_port[i], &gpio);
    HAL_GPIO_WritePin((GPIO_TypeDef *)row_port[i], row_pin[i], GPIO_PIN_SET);
  }

  gpio.Pin   = col_pin[0] | col_pin[1] | col_pin[2];   /* PA5 PA6 PA7 */
  gpio.Mode  = GPIO_MODE_INPUT;
  gpio.Pull  = GPIO_PULLUP;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio);
  gpio.Pin   = col_pin[3];                             /* PB0 */
  HAL_GPIO_Init(GPIOB, &gpio);

  last_key = stable_key = prev_stable = stable_cnt = raw_hold = 0;
}

void keypad_scan(void)
{
  uint8_t r, c;
  uint8_t found = 0;

  for (r = 0; r < 4 && !found; r++)
  {
    HAL_GPIO_WritePin((GPIO_TypeDef *)row_port[r], row_pin[r], GPIO_PIN_RESET);
    for (c = 0; c < 4; c++)
    {
      if (HAL_GPIO_ReadPin((GPIO_TypeDef *)col_port[c], col_pin[c]) == GPIO_PIN_RESET)
      {
        found = key_map[r][c];
        break;
      }
    }
    HAL_GPIO_WritePin((GPIO_TypeDef *)row_port[r], row_pin[r], GPIO_PIN_SET);
  }

  raw_hold = found;

  /* debounce: key must be stable for 3 consecutive scans */
  if (found == stable_key) stable_cnt++;
  else { stable_key = found; stable_cnt = 0; }

  if (stable_cnt >= 3)
  {
    if (stable_key != prev_stable)
    {
      prev_stable = stable_key;
      last_key = stable_key;
    }
  }
}

uint8_t keypad_get_key(void)
{
  uint8_t k = last_key;
  last_key = 0;
  return k;
}

uint8_t keypad_get_hold(void)
{
  return raw_hold;
}
