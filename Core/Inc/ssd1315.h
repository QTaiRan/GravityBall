/**
  ******************************************************************************
  * @file    ssd1315.h
  * @brief   SSD1315 / SSD1306 128x64 OLED (I2C) driver header
  ******************************************************************************
  */
#ifndef __SSD1315_H
#define __SSD1315_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

#define OLED_WIDTH      128
#define OLED_HEIGHT     64

/* 连接: VDD->3.3V GND->GND SCK(SCL)->PB6 SDA->PB7 (纯I2C模块,地址0x3C) */
#define OLED_I2C        hi2c1
#define OLED_I2C_ADDR   (0x3C << 1)

void oled_init(void);
void oled_clear(void);
void oled_show(void);
void oled_set_pixel(int x, int y, uint8_t color);
void oled_fill_rect(int x, int y, int w, int h, uint8_t color);
void oled_draw_rect(int x, int y, int w, int h, uint8_t color);
void oled_draw_circle(int cx, int cy, int r, uint8_t color);
void oled_fill_circle(int cx, int cy, int r, uint8_t color);
void oled_draw_char(int x, int y, char c, uint8_t color);
void oled_draw_char_big(int x, int y, char c, uint8_t color);
void oled_draw_str(int x, int y, const char *s, uint8_t color);
void oled_draw_str_big(int x, int y, const char *s, uint8_t color);
void oled_draw_num(int x, int y, int32_t v, uint8_t color);

#ifdef __cplusplus
}
#endif

#endif /* __SSD1315_H */
