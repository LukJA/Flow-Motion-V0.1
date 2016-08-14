#pragma once

#define STM32F4xx

#define FATFS_USE_SDIO 0
#define FATFS_SPI SPI1
#define FATFS_SPI_PINSPACK TM_SPI_PinsPack_1

#define FATFS_CS_PORT GPIOB
#define FATFS_CS_PIN GPIO_PIN_5

#define CTRL_REG1 0x20
#define CTRL_REG2 0x21
#define CTRL_REG3 0x22
#define CTRL_REG4 0x23
#define CTRL_REG5 0x24
#define CTRL_REG6 0x25

#define INT1_CFG 0x30
#define INT1_SRC 0x31
#define INT1_THS 0x32
#define INT1_DURATION 0x33

#define CLICK_CFG 0x38
#define CLICK_SRC 0x39
#define CLICK_THS 0x3A

#define TIME_LIMIT 0x3B
#define TIME_LATENCY 0x3C
#define TIME_WINDOW 0x3D


#define R_LED_PIN GPIO_PIN_1
#define G_LED_PIN GPIO_PIN_2
#define B_LED_PIN GPIO_PIN_3
#define LED_PORT GPIOA

#define X_AXIS 0x1
#define Y_AXIS 0x2
#define Z_AXIS 0x3
#define INT1_PIN GPIO_PIN_7
#define INT1_PORT GPIOB

#define LIS3DH_ADDR							0B0011000
