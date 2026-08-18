# BALL BLASTER - STM32F103C8T6 重力感应平衡球游戏

用 MPU6050 重力感应控制小球躲避障碍物的 OLED 小游戏。
倾斜开发板 = 小球滚动方向，4x4 矩阵键盘操作菜单和辅助微调，无源蜂鸣器播放音效。

## 硬件接线

| 外设 | 引脚 | STM32 引脚 | 备注 |
|------|------|-----------|------|
| OLED SSD1315 (I2C) | SCK (=SCL) | **PB6** | 0.96 寸 128x64，纯 I2C 模块，地址 0x3C |
| | SDA | **PB7** | |
| | VDD/GND | 3.3V / GND | |
| MPU6050 (10DOF) | SCL | **PB10** | 10DOF 模块加速度计部分 |
| | SDA | **PB11** | |
| | VCC_IN/GND | 5V(或3.3V) / GND | 板载 LDO 稳压，注意共地 |
| | 3.3V/FSYNC/INTA/DRDY | 悬空 | 不需要连接 |
| 无源蜂鸣器 | IN | **PA0** (TIM2_CH1) | 9012 三极管驱动，PWM 2k-5kHz |
| 4x4 矩阵键盘 | Pin1~Pin4 | **PA4, PA3, PA2, PA1** | Pin1=PA4, Pin2=PA3, Pin3=PA2, Pin4=PA1 |
| | Pin5~Pin8 | **PA5, PA6, PA7, PB0** | Pin5=PA5, Pin6=PA6, Pin7=PA7, Pin8=PB0 |
| 板载 LED | | **PC13** | 游戏中常亮，Game Over 闪烁 |

> 键盘按键与引脚对应关系：
> ```
> Pin4+Pin5=1   Pin4+Pin6=5   Pin4+Pin7=9    Pin4+Pin8=13
> Pin3+Pin5=2   Pin3+Pin6=6   Pin3+Pin7=10   Pin3+Pin8=14
> Pin2+Pin5=3   Pin2+Pin6=7   Pin2+Pin7=11   Pin2+Pin8=15
> Pin1+Pin5=4   Pin1+Pin6=8   Pin1+Pin7=12   Pin1+Pin8=16
> ```

## 操作说明

| 按键 | 主菜单 | 游戏中 | 暂停 | 游戏结束 | 校准 |
|------|--------|--------|------|----------|------|
| 2 / 8 | 上移 / 下移 | 上 / 下微调 | - | - | - |
| 4 / 6 | - | 左 / 右微调 | - | - | - |
| 5 | 确认 | 暂停 | 继续 | 重新开始 | 开始校准 |
| 16 | - | 退出到菜单 | 退出到菜单 | 回菜单 | 取消 |
| 其他键 | 无功能 | 无功能 | 无功能 | 无功能 | 无功能 |

## 玩法

- 倾斜开发板控制小球，键盘可辅助"吹"小球
- 躲避从四面飞来的障碍物（实心=从上落下，空心=从左/右水平移动，点阵=水平移动）
- 收集 5 角星的**金币**（+10 分），每 50 分升一级：障碍更多、更快
- 3 条命（困难模式 1 条），被撞到会清空障碍并进入 2.2 秒无敌闪烁
- 游戏结束显示最高分（断电清零）

## 菜单

- **START**：开始游戏。首次游玩建议先在菜单做一次 **CALIBRATE**
- **CALIBRATE**：把开发板平放在桌上，按 5 采样 64 次完成零偏校准（断电失效，每次开机建议校准）
- **DIFFICULTY**：EASY / NORMAL / HARD 循环切换
- **SOUND**：音效开关

## 构建与烧录

1. STM32CubeIDE 打开 `D:\ST\STM32CubeIDEworkspace` 工作区（或直接导入 `GravityBall` 项目）
2. 编译 Debug 配置（已用 headless 构建验证：0 errors / 0 warnings，Flash 24.4KB）
3. ST-Link 下载 `Debug/GravityBall.elf`（或 `STM32CubeProgrammer` 烧录 `Debug/GravityBall.hex`）

## 常见问题

- **开机显示 MPU6050 ERR**：检查 VCC_IN 是否供电（建议 5V）、SCL/SDA 是否接在 PB10/PB11、是否与开发板共地
- **屏幕不亮**：OLED 模块一般默认地址 0x3C；若为 0x3D，改 `Core/Inc/ssd1315.h` 中 `OLED_I2C_ADDR`
- **画面方向反了**：改 `Core/Src/ssd1315.c` 初始化中的 `0xA1`/`0xC8`（`0xA0`/`0xC0` 反向）
- **蜂鸣器不响**：确认是无源蜂鸣器（有源蜂鸣器不适用），检查 PA0 到 9012 基极的接线
- **球不受控**：先 CALIBRATE；若仍异常，可加大 `Core/Src/mpu6050.c` 里的死区 `0.02f`
- **操控方向反了**：改 `Core/Src/mpu6050.c` 顶部的 `MPU_FLIP_X` / `MPU_FLIP_Y`（左右反设 1，上下反设 1）
- **手感**：调 `Core/Src/game.c` 中 `update_ball()` 的 `330.0f`（灵敏度）和 `0.955f`（摩擦）

## 文件结构

```
Core/Src/
  main.c        HAL 初始化 (App_I2C1/2_Init, App_TIM2_Init) + 主循环
  ssd1315.c     OLED 驱动：framebuffer + 脏页增量刷新 + 5x7 字体 + 图形原语
  mpu6050.c     加速度计驱动：±2g 原始值 -> 倾角力，64 次采样校准，低通滤波
  keypad4x4.c   4x4 矩阵键盘扫描 + 消抖
  buzzer.c      TIM2 PWM 变频音效（菜单/吃金币/升级/受伤/游戏结束）
  game.c        状态机：Splash -> 菜单 -> 校准 -> 游戏中 -> 暂停 -> Game Over
```

## 说明

- 所有外设初始化都写在 `main.c` 的 USER CODE 区内，与 CubeMX 生成代码兼容；
  `GravityBall.ioc` 已同步配置好 I2C1/I2C2/TIM2/键盘引脚，如需在 CubeMX 里改引脚，
  生成后把 `MX_I2C1_Init` 等改名或删掉旧的 `App_*_Init` 即可。
- 未使用的硬件（W25Q128、DHT11、HMC5883L、BMP180、额外 LED）已预留 I2C/SPI 总线，可自行扩展
