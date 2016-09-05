/*
 *  LCD Effect control
 *
 * Copyright (c) 2016, Michal Chvíla <electrydev@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LCD_EFFECT_H__
#define __LCD_EFFECT_H__

#include <linux/moduleparam.h>
#include <linux/gpio.h>
#include "mdss_dsi.h"
#include "mdss_fb.h"
#include "mdss_dsi_cmd.h"

#define TAG "lcd_effect"

/**
 * EFFECT
 */
struct mdss_lcd_effect_effect {
	const char *name;			/* name of effect */
	const int max_level;			/* max avail level */
	int level;				/* current level id */
	struct dsi_cmd_desc *cmds; 		/* commands for each level */
};

/**
 * MODE
 */
struct mdss_lcd_effect_mode {
	const char *name;			/* name of mode */
	const int bl_gpio;			/* enable bl_outdoor gpio? */
	struct dsi_cmd_desc *cmds;		/* list of commands */
	const int cnt;				/* count of commands */
};

/**
 * CMD DATA
 */
struct mdss_lcd_effect_cmd_data {
	struct dsi_cmd_desc *cmds;		/* list of commands */
	int cnt;				/* count of commands */
};

/**
 * EFFECT DATA
 */
struct mdss_lcd_effect_effect_data {
	struct mdss_lcd_effect_effect *effect;	/* all supported effects */
	const int supported_effect;		/* total count of supported effects */
};

/**
 * MODE DATA
 */
struct mdss_lcd_effect_mode_data {
	struct mdss_lcd_effect_mode *mode;	/* all supported modes */
	const int supported_mode;		/* total count of supported modes */
	int current_mode;			/* current selected mode */
};

/**
 * DATA
 */
struct mdss_lcd_effect_data {
	struct mdss_lcd_effect_cmd_data *head_data;	/* head commands */

	struct mdss_lcd_effect_effect_data *effect_data;/* effect data */
	struct mdss_lcd_effect_mode_data *mode_data;	/* mode data */

	struct dsi_cmd_desc *buf;			/* allocated buffer */
	int buf_size;					/* buffer size */
};

/* Sysfs */
int mdss_lcd_effect_create_sysfs(struct msm_fb_data_type *mfd);

/* Allocate buffer */
int mdss_lcd_effect_malloc_buf(struct mdss_lcd_effect_data *lcd_data);

#endif
