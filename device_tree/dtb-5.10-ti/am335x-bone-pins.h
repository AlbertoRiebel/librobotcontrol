/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AM335x pin definitions for BeagleBone Blue librobotcontrol DTS
 *
 * These constants define the pad configuration register addresses
 * for the AM335x SoC, relative to the SCM (System Control Module)
 * pad multiplexing register base at offset 0x800.
 *
 * Used by am335x-boneblue.dts for librobotcontrol pin groups that
 * are not present in the official TI DTS (servo, motor, encoder pins).
 *
 * NOTE: MCASP0_* pins (AHCLKR, AHCLKX, ACLKR, FSR, AXR1, etc.) are
 * NOT defined here because they already exist in the official header
 * at dt-bindings/pinctrl/am33xx.h. Redefining them here caused
 * "redefined" warnings and, worse, this header originally had their
 * values SWAPPED relative to the official ones (AHCLKX/AHCLKR mixed
 * up), which pointed GPIO3_17/GPIO3_20 at the wrong physical pins.
 * Always prefer the official header's values for any symbol it
 * already defines.
 *
 * Reference: AM335x Technical Reference Manual, Section 9 (Control Module)
 */
#ifndef _AM335X_BONE_PINS_H
#define _AM335X_BONE_PINS_H

/* GPMC */
#define AM335X_PIN_GPMC_AD0             0x800
#define AM335X_PIN_GPMC_AD1             0x804
#define AM335X_PIN_GPMC_AD2             0x808
#define AM335X_PIN_GPMC_AD3             0x80c
#define AM335X_PIN_GPMC_AD4             0x810
#define AM335X_PIN_GPMC_AD5             0x814
#define AM335X_PIN_GPMC_AD6             0x818
#define AM335X_PIN_GPMC_AD7             0x81c
#define AM335X_PIN_GPMC_AD10            0x828
#define AM335X_PIN_GPMC_AD11            0x82c
#define AM335X_PIN_GPMC_AD12            0x830
#define AM335X_PIN_GPMC_AD13            0x834
#define AM335X_PIN_GPMC_A0              0x840
#define AM335X_PIN_GPMC_A5              0x854
#define AM335X_PIN_GPMC_A6              0x858
#define AM335X_PIN_GPMC_A7              0x85c
#define AM335X_PIN_GPMC_A8              0x860
#define AM335X_PIN_GPMC_WAIT0           0x870
#define AM335X_PIN_GPMC_WPN             0x874
#define AM335X_PIN_GPMC_CSN0            0x87c
#define AM335X_PIN_GPMC_CSN1            0x880
#define AM335X_PIN_GPMC_CSN2            0x884
#define AM335X_PIN_GPMC_CSN3            0x888
#define AM335X_PIN_GPMC_CLK             0x88c
#define AM335X_PIN_GPMC_ADVN_ALE        0x890
#define AM335X_PIN_GPMC_OEN_REN         0x894

/* LCD */
#define AM335X_PIN_LCD_DATA0            0x8a0
#define AM335X_PIN_LCD_DATA1            0x8a4
#define AM335X_PIN_LCD_DATA2            0x8a8
#define AM335X_PIN_LCD_DATA3            0x8ac
#define AM335X_PIN_LCD_DATA4            0x8b0
#define AM335X_PIN_LCD_DATA5            0x8b4
#define AM335X_PIN_LCD_DATA6            0x8b8
#define AM335X_PIN_LCD_DATA7            0x8bc
#define AM335X_PIN_LCD_DATA8            0x8c0
#define AM335X_PIN_LCD_DATA9            0x8c4
#define AM335X_PIN_LCD_DATA10           0x8c8
#define AM335X_PIN_LCD_DATA12           0x8d0
#define AM335X_PIN_LCD_DATA13           0x8d4
#define AM335X_PIN_LCD_DATA14           0x8d8
#define AM335X_PIN_LCD_DATA15           0x8dc
#define AM335X_PIN_LCD_DATA16           0x8e0
#define AM335X_PIN_LCD_DATA17           0x8e4
#define AM335X_PIN_LCD_DATA18           0x8e8
#define AM335X_PIN_LCD_DATA19           0x8ec

/* SPI0 (used for I2C1 and UART2 alt functions, NOT SPI1) */
#define AM335X_PIN_SPI0_SCLK            0x950
#define AM335X_PIN_SPI0_D0              0x954
#define AM335X_PIN_SPI0_D1              0x958
#define AM335X_PIN_SPI0_CS0             0x95c
#define AM335X_PIN_SPI0_CS1             0x960

/* XDMA / misc */
#define AM335X_PIN_XDMA_EVENT_INTR1     0x9b4

#endif /* _AM335X_BONE_PINS_H */
