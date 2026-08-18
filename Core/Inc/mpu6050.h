/**
  ******************************************************************************
  * @file    mpu6050.h
  * @brief   MPU6050 6-axis IMU (I2C1, shared with OLED) driver header - accelerometer only
  ******************************************************************************
  */
#ifndef __MPU6050_H
#define __MPU6050_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* 连接(10DOF模块): VCC_IN->5V(或3.3V) GND->GND SCL->PB6 SDA->PB7 (与OLED并联同总线)
   3.3V输出/FSYNC/INTA/DRDY 全部悬空, AD0悬空或接地 -> 地址0x68 */
#define MPU_I2C         hi2c1
#define MPU_I2C_ADDR    (0x68 << 1)

uint8_t mpu_init(void);                       /* return 1 if OK */
void    mpu_calibrate(void);                  /* sample offset while flat */
void    mpu_get_tilt(float *fx, float *fy);   /* tilt force in g, low-passed */
float   mpu_get_pitch(void);                  /* deg, for display */

#ifdef __cplusplus
}
#endif

#endif /* __MPU6050_H */
