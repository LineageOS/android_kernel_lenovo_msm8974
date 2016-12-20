/*
 * NXP TFA9890 audio amplifier driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _NXP_TFA9890_I2C_H_
#define _NXP_TFA9890_I2C_H_

#define sstrtoul(...) kstrtoul(__VA_ARGS__)

#define TFA9890_VTG_MIN_UV		1800000
#define TFA9890_VTG_MAX_UV		1800000
#define TFA9890_ACTIVE_LOAD_UA		10000 //15000
#define TFA9890_LPM_LOAD_UA		10

#define TFA9890_I2C_VTG_MIN_UV		1800000
#define TFA9890_I2C_VTG_MAX_UV		1800000
#define TFA9890_I2C_LOAD_UA		10000
#define TFA9890_I2C_LPM_LOAD_UA		10

extern int msm8974_quat_mi2s_clk_enable(bool enable);

struct tfa9890_i2c_platform_data {
	struct i2c_client *i2c_client;
	bool i2c_pull_up;
	bool regulator_en;
	u32 reset_flags;
	u32 irq_flags;
	unsigned int irq;
	unsigned int reset_gpio;
	unsigned int irq_gpio;

	int (*driver_opened)(void);
	void (*driver_closed)(void);
};

#endif
