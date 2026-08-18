/**
  ******************************************************************************
  * @file    mpu6050.c
  * @brief   MPU6050 driver (I2C2) - accelerometer tilt sensing for the game
  ******************************************************************************
  */
#include "mpu6050.h"
#include "main.h"

/* 轴方向修正：左右反向->1，上下反向->1（按模块安装方向调整） */
#define MPU_FLIP_X   1
#define MPU_FLIP_Y   0

#define MPU_REG_ACCEL_XOUT_H  0x3B
#define MPU_REG_PWR_MGMT_1    0x6B
#define MPU_REG_WHO_AM_I      0x75

/* raw offsets from calibration (in ±2g raw counts) */
static int16_t off_x, off_y, off_z;
static uint8_t inited = 0;
static float f_lx = 0.0f, f_ly = 0.0f;

static int8_t mpu_read(uint8_t reg, uint8_t *buf, uint8_t len)
{
  return HAL_I2C_Mem_Read(&MPU_I2C, MPU_I2C_ADDR, reg, 1, buf, len, 100) == HAL_OK;
}

static int8_t mpu_write(uint8_t reg, uint8_t val)
{
  return HAL_I2C_Mem_Write(&MPU_I2C, MPU_I2C_ADDR, reg, 1, &val, 1, 100) == HAL_OK;
}

uint8_t mpu_init(void)
{
  uint8_t id = 0;
  inited = 0;

  if (!mpu_read(MPU_REG_WHO_AM_I, &id, 1)) return 0;
  if ((id & 0x7E) != 0x68) return 0;      /* MPU6050/MPU6500 family */

  mpu_write(MPU_REG_PWR_MGMT_1, 0x00);    /* wake up */
  HAL_Delay(10);
  mpu_write(0x19, 0x00);                  /* sample rate = gyro rate */
  mpu_write(0x1A, 0x03);                  /* DLPF 44Hz */
  mpu_write(0x1B, 0x00);                  /* gyro full scale 250dps */
  mpu_write(0x1C, 0x00);                  /* accel full scale +-2g */

  off_x = off_y = off_z = 0;
  inited = 1;
  return 1;
}

void mpu_calibrate(void)
{
  uint8_t buf[6];
  int32_t sx = 0, sy = 0, sz = 0;
  uint16_t i;
  if (!inited) return;

  mpu_write(0x6B, 0x01);   /* reset then wake */
  HAL_Delay(50);
  mpu_write(0x6B, 0x00);
  HAL_Delay(10);

  for (i = 0; i < 64; i++)
  {
    if (mpu_read(MPU_REG_ACCEL_XOUT_H, buf, 6))
    {
      sx += (int16_t)((buf[0] << 8) | buf[1]);
      sy += (int16_t)((buf[2] << 8) | buf[3]);
      sz += (int16_t)((buf[4] << 8) | buf[5]);
    }
    HAL_Delay(2);
  }
  off_x = (int16_t)(sx / 64);
  off_y = (int16_t)(sy / 64);
  off_z = (int16_t)(sz / 64) - 16384;   /* remove +1g so flat == 0 */
}

void mpu_get_tilt(float *fx, float *fy)
{
  uint8_t buf[6];
  float ax, ay;

  *fx = 0.0f;
  *fy = 0.0f;
  if (!inited) return;

  if (mpu_read(MPU_REG_ACCEL_XOUT_H, buf, 6))
  {
    ax = (float)((int16_t)((buf[0] << 8) | buf[1]) - off_x) / 16384.0f;
    ay = (float)((int16_t)((buf[2] << 8) | buf[3]) - off_y) / 16384.0f;
  }
  else
  {
    ax = 0.0f;
    ay = 0.0f;
  }

  /* dead zone + saturation */
  if (ax > 0.02f) ax -= 0.02f; else if (ax < -0.02f) ax += 0.02f; else ax = 0.0f;
  if (ay > 0.02f) ay -= 0.02f; else if (ay < -0.02f) ay += 0.02f; else ay = 0.0f;
  if (ax > 1.0f) ax = 1.0f; else if (ax < -1.0f) ax = -1.0f;
  if (ay > 1.0f) ay = 1.0f; else if (ay < -1.0f) ay = -1.0f;

  /* low-pass */
  f_lx = f_lx * 0.7f + ax * 0.3f;
  f_ly = f_ly * 0.7f + ay * 0.3f;
  *fx = MPU_FLIP_X ? -f_lx : f_lx;
  *fy = MPU_FLIP_Y ? -f_ly : f_ly;
}

float mpu_get_pitch(void)
{
  uint8_t buf[6];
  float ax, az;
  float v;
  if (!inited) return 0.0f;
  if (!mpu_read(MPU_REG_ACCEL_XOUT_H, buf, 6)) return 0.0f;
  ax = (float)((int16_t)((buf[0] << 8) | buf[1]) - off_x);
  az = (float)((int16_t)((buf[4] << 8) | buf[5]) - off_z);
  if (az == 0.0f) return 0.0f;
  v = (ax / az) * 45.0f;   /* atan(x) ~= x for small tilt, 45/1 = 45deg */
  if (MPU_FLIP_X) v = -v;
  if (v > 80.0f) v = 80.0f; else if (v < -80.0f) v = -80.0f;
  return v;
}
