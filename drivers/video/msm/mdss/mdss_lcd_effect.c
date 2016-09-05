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
#include "mdss_lcd_effect.h"

/* MDSS Display panel */
#include "mdss_r63319.h"

static struct mdss_dsi_ctrl_pdata *get_rctrl_data(struct mdss_panel_data *pdata)
{
	if (!pdata || !pdata->next) {
		pr_err("%s: %s: Invalid panel data!\n", TAG, __func__);
		return NULL;
	}

	return container_of(pdata->next, struct mdss_dsi_ctrl_pdata,
			panel_data);
}

/**
 * Send an array of dsi_cmd_desc to the display panel
 */
static int mdss_lcd_effect_send_cmds(
		struct msm_fb_data_type *mfd,
		struct mdss_lcd_effect_data *lcd_data,
		int cnt)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct dcs_cmd_req cmdreq;
	struct mdss_panel_data *pdata;
	int ret;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (mfd->panel_power_on == false) {
		pr_err("%s: %s: LCD panel is powered off!\n", TAG, __func__);
		return -EPERM;
	}

	if (ctrl_pdata->shared_pdata.broadcast_enable &&
			ctrl_pdata->ndx == DSI_CTRL_0) {

		struct mdss_dsi_ctrl_pdata *rctrl_pdata = NULL;
		rctrl_pdata = get_rctrl_data(pdata);

		if (!rctrl_pdata) {
			pr_err("%s: %s: Right ctrl data NULL!\n", TAG, __func__);
			return -EFAULT;
		}
		ctrl_pdata = rctrl_pdata;
	}

	pr_debug("%s: %s: sending dsi cmds, len=%d\n", TAG, __func__, cnt);

	memset(&cmdreq, 0, sizeof(cmdreq));
	cmdreq.flags = CMD_REQ_COMMIT | CMD_CLK_CTRL;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	cmdreq.cmds = lcd_data->buf;
	cmdreq.cmds_cnt = cnt;
	ret = mdss_dsi_cmdlist_put(ctrl_pdata, &cmdreq);

	return ret;
}

/**
 * Copy HEAD CMDS to the buffer
 */
static struct dsi_cmd_desc *mdss_lcd_effect_copy_head_code(
		struct mdss_lcd_effect_data *lcd_data,
		int /*out*/*cnt)
{
	struct dsi_cmd_desc *head_cmds = lcd_data->head_data->cmds;
	int head_cnt = lcd_data->head_data->cnt; 

	pr_debug("%s: %s: copying head code to buffer, pos=%d, cnt=%d", TAG, __func__, *cnt, head_cnt);

	memcpy(lcd_data->buf + *cnt, head_cmds, head_cnt * sizeof (struct dsi_cmd_desc));
	*cnt += head_cnt;

	pr_debug("%s: %s: line=%d cnt=%d\n", TAG, __func__, __LINE__, *cnt);

	return (lcd_data->buf + head_cnt);
}

/**
 * Copy single EFFECT CMDS to the buffer
 */
static struct dsi_cmd_desc *mdss_lcd_effect_copy_effect_code(
		struct mdss_lcd_effect_data *lcd_data,
		int index,
		int level,
		int /*out*/*cnt)
{
	struct mdss_lcd_effect_effect *effects = lcd_data->effect_data->effect;
	struct dsi_cmd_desc *effect_cmds = effects[index].cmds;

	pr_debug("%s: %s: copying effect code to buffer, pos=%d, cnt=%d", TAG, __func__, *cnt, 1);

	memcpy(lcd_data->buf + *cnt, &effect_cmds[level], sizeof (struct dsi_cmd_desc));
	*cnt += 1; // single effect cmd

	pr_debug("%s: %s: line=%d cnt=%d\n", TAG, __func__, __LINE__, *cnt);

	return (lcd_data->buf + 1);
}

/**
 * Copy MODE CMDS to the buffer
 */
static struct dsi_cmd_desc *mdss_lcd_effect_copy_mode_code(
		struct mdss_lcd_effect_data *lcd_data,
		int index,
		int /*out*/*cnt)
{
	struct mdss_lcd_effect_mode *mode = &lcd_data->mode_data->mode[index];
	struct dsi_cmd_desc *mode_cmds = mode->cmds;
	int mode_cnt = mode->cnt;
	int i;

	int current_mode = lcd_data->mode_data->current_mode;

	pr_debug("%s: %s: line=%d mode_cnt=%d curr_mode=%d\n", TAG, __func__, __LINE__, mode_cnt, current_mode);

	if (current_mode == 0) {
		pr_debug("%s: %s: current is custom mode\n", TAG, __func__);

		// Set mode
		memcpy(lcd_data->buf + *cnt, mode_cmds, mode_cnt * sizeof (struct dsi_cmd_desc));
		*cnt += mode_cnt;

		// Update all effects
		for (i = 0; i < lcd_data->effect_data->supported_effect; i++) {
			mdss_lcd_effect_copy_effect_code(lcd_data, i, lcd_data->effect_data->effect[i].level, /*out*/cnt);
		}
	} else {
		pr_debug("%s: %s: current is [%s]\n", TAG, __func__, mode->name);

		// Reset all effects
		for (i = 0; i < lcd_data->effect_data->supported_effect; i++) {
			mdss_lcd_effect_copy_effect_code(lcd_data, i, /*reset*/0, /*out*/cnt);
		}

		// Set mode
		memcpy(lcd_data->buf + *cnt, mode_cmds, mode_cnt * sizeof (struct dsi_cmd_desc));
		*cnt += mode_cnt;
	}

	return (lcd_data->buf + mode_cnt);
}

/**
 * Set bl_outdoor gpio value
 */
static int mdss_lcd_effect_set_bl_gpio(struct msm_fb_data_type *mfd, int level)
{
	struct mdss_dsi_ctrl_pdata *ctrl_pdata = NULL;
	struct mdss_panel_data *pdata;

	pdata = dev_get_platdata(&mfd->pdev->dev);
	ctrl_pdata = container_of(pdata, struct mdss_dsi_ctrl_pdata, panel_data);

	if (mfd->panel_power_on == false) {
		pr_err("%s: %s: LCD panel is powered off!\n", TAG, __func__);
		return -EPERM;
	}

	if (ctrl_pdata->shared_pdata.broadcast_enable &&
			ctrl_pdata->ndx == DSI_CTRL_0) {
		struct mdss_dsi_ctrl_pdata *rctrl_pdata = NULL;
		rctrl_pdata = get_rctrl_data(pdata);
		if (!rctrl_pdata) {
			pr_err("%s: %s: Right ctrl data NULL!\n", TAG, __func__);
			return -EFAULT;
		}
		ctrl_pdata = rctrl_pdata;
	}

	if (!gpio_is_valid(ctrl_pdata->bl_outdoor_gpio)) {
		pr_err("%s: %s: bl_outdoor_gpio [%d] invalid, level=%d\n",
				TAG, __func__, ctrl_pdata->bl_outdoor_gpio, level);
		return -EINVAL;
	}

	gpio_set_value(ctrl_pdata->bl_outdoor_gpio, level);

	pr_info("%s: %s: bl_outdoor_gpio set level=%d\n",
			TAG, __func__, level);

	return 0;
}


/**
 * Set LCD Effect
 *  copy head & effect code and sends dsi cmds to the display panel
 */
static int mdss_lcd_effect_set_effect(
		struct msm_fb_data_type *mfd,
		struct mdss_lcd_effect_data *lcd_data,
		int index,
		int level)
{
	struct mdss_lcd_effect_effect_data *effect_data = lcd_data->effect_data;
	int ret, cnt = 0;

	if (index < 0 || index >= effect_data->supported_effect) {
		pr_err("%s: %s: index [%d] is invalid, available range is 0-%d!\n",
				TAG, __func__, index, effect_data->supported_effect - 1);
		return -EINVAL;
	}

	if (level < 0 || level >= effect_data->effect[index].max_level) {
		pr_err("%s: %s: level [%d] for mode [%s] is invalid, available range is 0-%d!\n",
				TAG, __func__, level,
				effect_data->effect[index].name,
				effect_data->effect[index].max_level - 1);
		return -EINVAL;
	}

	mdss_lcd_effect_copy_head_code(lcd_data, /*out*/&cnt);
	mdss_lcd_effect_copy_effect_code(lcd_data, index, level, /*out*/&cnt);

	ret = mdss_lcd_effect_send_cmds(mfd, lcd_data, cnt);
	if (!ret || ret == -EPERM) {
		effect_data->effect[index].level = level;
		pr_info("%s: %s: name [%s] level [%d] success\n",
				TAG, __func__, effect_data->effect[index].name, level);

		ret = 0;
	} else {
		pr_err("%s: %s: Failed to set LCD Effect index [%d] level [%d], ret=%d!\n",
				TAG, __func__, index, level, ret);
	}

	return ret;
}

/**
 * Set LCD Mode
 *  copy head & mode code and send dsi cmds to the display panel
 */
static int mdss_lcd_effect_set_mode(
		struct msm_fb_data_type *mfd,
		struct mdss_lcd_effect_data *lcd_data,
		int index)
{
	struct mdss_lcd_effect_mode_data *mode_data = lcd_data->mode_data;
	int ret, previous_mode, cnt = 0;

	if (index < 0 || index >= mode_data->supported_mode) {
		pr_err("%s: %s: mode [%d] is invalid, available range is 0-%d!\n",
				TAG, __func__, index, mode_data->supported_mode - 1);
		return -EINVAL;
	}

	previous_mode = mode_data->current_mode;
	mode_data->current_mode = index;

	mdss_lcd_effect_copy_head_code(lcd_data, /*out*/&cnt);
	mdss_lcd_effect_copy_mode_code(lcd_data, index, /*out*/&cnt);

	ret = mdss_lcd_effect_send_cmds(mfd, lcd_data, cnt);
	if (!ret || ret == -EPERM) {
		pr_info("%s: %s: name [%s] index [%d] success\n",
				TAG, __func__, mode_data->mode[index].name, index);

		// update bl_outdoor gpio
		mdss_lcd_effect_set_bl_gpio(mfd, mode_data->mode[index].bl_gpio);

		ret = 0;
	} else {
		mode_data->current_mode = previous_mode;

		pr_err("%s: %s: Failed to set LCD Mode index [%d], ret=%d!\n",
				TAG, __func__, index, ret);
	}

	return ret;
}


/**
 * LCD Panel name
 *   get: Return panel name as a string, defined by it's header.
 */
static ssize_t mdss_lcd_effect_sysfs_get_panel_name(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", LCD_PANEL_NAME);
}

/**
 * LCD Supported effect
 *   get: Return list of supported LCD effects (index name) defined by panel's header.
 */
static ssize_t mdss_lcd_effect_sysfs_get_supported_effect(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mdss_lcd_effect_effect_data *effect_data = mdss_lcd_effect_data.effect_data;
	ssize_t ret = 0;
	int i;

	for (i = 0; i < effect_data->supported_effect; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE, "%d %s\n", i, effect_data->effect[i].name);
	}

	return ret;
}

/**
 * LCD Supported mode
 *   get: Return list of supported LCD modes (index name) defined by panel's header.
 */
static ssize_t mdss_lcd_effect_sysfs_get_supported_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mdss_lcd_effect_mode_data *mode_data = mdss_lcd_effect_data.mode_data;
	ssize_t ret = 0;
	int i;

	for (i = 0; i < mode_data->supported_mode; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE, "%d %s\n", i, mode_data->mode[i].name);
	}

	return ret;
}

/**
 * LCD Effect
 *   get: Return lcd effect status (index name level/max_level)
 *   set: Set lcd effect
 */
static ssize_t mdss_lcd_effect_sysfs_get_effect(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mdss_lcd_effect_effect_data *effect_data = mdss_lcd_effect_data.effect_data;
	ssize_t ret = 0;
	int i;

	for (i = 0; i < effect_data->supported_effect; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE, "%d %s %d/%d\n", i,
					effect_data->effect[i].name,
					effect_data->effect[i].level,
					effect_data->effect[i].max_level - 1);
	}

	return ret;
}

static ssize_t mdss_lcd_effect_sysfs_set_effect(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	char *tmp_index, *tmp_level = (char *)buf;
	int index, level;

	tmp_index = strsep(&tmp_level, " ");

	if (kstrtoint(tmp_index, 0, &index) || kstrtoint(tmp_level, 0, &level))
		return -EINVAL;

	if (index < 0 || index >= mdss_lcd_effect_data.effect_data->supported_effect) {
		pr_err("%s: %s: Trying to set unsupported effect, index=%d!\n", TAG, __func__, index);
		return -EINVAL;
	}

	if (level < 0 || level >= mdss_lcd_effect_data.effect_data->effect[index].max_level) {
		pr_err("%s: %s: Trying to set invalid level for [%s], level=%d!\n",
				TAG, __func__, mdss_lcd_effect_data.effect_data->effect[index].name, level);
		return -EINVAL;
	}

	if (mdss_lcd_effect_data.mode_data->current_mode != 0) {
		pr_err("%s: %s: Trying to set LCD Effect [%s] while not in custom_mode, ignoring!\n",
				TAG, __func__, mdss_lcd_effect_data.mode_data->mode[index].name);
		return -EINVAL;
	}

	mdss_lcd_effect_set_effect(mfd, &mdss_lcd_effect_data, index, level);

	return count;
}

/**
 * LCD Mode
 *   get: Return current LCD Mode (index name)
 *   set: Set lcd mode
 */
static ssize_t mdss_lcd_effect_sysfs_get_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
	int current_mode = mdss_lcd_effect_data.mode_data->current_mode;

	return snprintf(buf, PAGE_SIZE, "%d %s\n",
		current_mode,
		mdss_lcd_effect_data.mode_data->mode[current_mode].name
	);
}

static ssize_t mdss_lcd_effect_sysfs_set_mode(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct fb_info *fbi = dev_get_drvdata(dev);
	struct msm_fb_data_type *mfd = (struct msm_fb_data_type *)fbi->par;
	unsigned long index;

	if (kstrtoul(buf, 0, &index))
		return -EINVAL;

	if (index < 0 || index >= mdss_lcd_effect_data.mode_data->supported_mode) {
		pr_err("%s: %s: Trying to set unsupported lcd mode, index=%lu!\n",
				TAG, __func__, index);
		return -EINVAL;
	}

	mdss_lcd_effect_set_mode(mfd, &mdss_lcd_effect_data, index);

	return count;
}

static DEVICE_ATTR(lcd_panel_name, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_sysfs_get_panel_name, NULL);
static DEVICE_ATTR(lcd_supported_effect, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_sysfs_get_supported_effect, NULL);
static DEVICE_ATTR(lcd_supported_mode, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_sysfs_get_supported_mode, NULL);
static DEVICE_ATTR(lcd_effect, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_sysfs_get_effect, mdss_lcd_effect_sysfs_set_effect);
static DEVICE_ATTR(lcd_mode, S_IRUGO | S_IWUSR | S_IWGRP,
	mdss_lcd_effect_sysfs_get_mode, mdss_lcd_effect_sysfs_set_mode);

static struct attribute *mdss_lcd_effect_attrs[] = {
	&dev_attr_lcd_panel_name.attr,
	&dev_attr_lcd_supported_effect.attr,
	&dev_attr_lcd_supported_mode.attr,
	&dev_attr_lcd_effect.attr,
	&dev_attr_lcd_mode.attr,
	NULL,
};

static struct attribute_group mdss_lcd_effect_attr_group = {
	.attrs = mdss_lcd_effect_attrs,
};

/**
 * Create sysfs group in fb
 *  called by mdss_fb_create_sysfs() in mdss_fb.c
 */
int mdss_lcd_effect_create_sysfs(struct msm_fb_data_type *mfd)
{
	int ret = 0;

	ret = sysfs_create_group(&mfd->fbi->dev->kobj, &mdss_lcd_effect_attr_group);
	if (ret)
		pr_err("%s: %s: sysfs group create fail, ret=%d\n",
				TAG, __func__, ret);

	pr_info("%s: %s: sysfs group create success\n", TAG, __func__);

	return ret;
}

/**
 * Get buf size to allocate
 */
static int mdss_lcd_effect_get_alloc_code_cnt(struct mdss_lcd_effect_data *lcd_data)
{
	int i, temp, cnttemp;
	int cnt = 0;

	// LCD Mode max cnt (1 at a time)
	cnttemp = 0;
	for (i = 0; i < lcd_data->mode_data->supported_mode; i++) {
		temp = lcd_data->mode_data->mode[i].cnt;
		cnttemp = (cnttemp > temp) ? cnttemp : temp;
	}
	cnt += cnttemp;

	// LCD Effect cnt (all at a time)
	cnttemp = 0;
	for (i = 0; i < lcd_data->effect_data->supported_effect; i++) {
		cnttemp += lcd_data->effect_data->effect[i].max_level;
	}
	cnt += cnttemp;

	pr_debug("%s: %s: cnt=%d\n", TAG, __func__, cnt);

	return cnt;
}

/**
 * Allocate lcd_effect buffer
 *  called by mdss_panel_parse_dt() in mdss_dsi_panel.c
 */
int mdss_lcd_effect_malloc_buf(struct mdss_lcd_effect_data *lcd_data)
{
	if (lcd_data->buf != NULL)
		return 0;

	lcd_data->buf_size = mdss_lcd_effect_get_alloc_code_cnt(lcd_data);
	lcd_data->buf = kmalloc(sizeof(struct dsi_cmd_desc) * lcd_data->buf_size, GFP_KERNEL);
	if (!lcd_data->buf) {
		pr_err("%s: %s: Failed to allocate lcd_effect buffer!\n",
				TAG, __func__);
		return -ENOMEM;
	}

	pr_info("%s: %s: Allocate lcd_effect buffer success\n",
				TAG, __func__);

	return 0;
}
