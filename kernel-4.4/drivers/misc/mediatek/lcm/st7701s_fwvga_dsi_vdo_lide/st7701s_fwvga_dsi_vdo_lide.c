/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define LOG_TAG "LCM"

#ifndef BUILD_LK
#include <linux/string.h>
#include <linux/kernel.h>
#endif

#include "lcm_drv.h"

#ifdef BUILD_LK
#include <platform/upmu_common.h>
#include <platform/mt_gpio.h>
#include <platform/mt_i2c.h>
#include <platform/mt_pmic.h>
#include <string.h>
#elif defined(BUILD_UBOOT)
#include <asm/arch/mt_gpio.h>
#else
/*#include <mach/mt_pm_ldo.h>*/
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#endif
#endif
#ifdef CONFIG_MTK_LEGACY
#include <cust_gpio_usage.h>
#endif
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_LEGACY)
#include <cust_i2c.h>
#endif
#endif
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#ifdef BUILD_LK
#define LCM_LOGI(string, args...)  dprintf(0, "[LK/"LOG_TAG"]"string, ##args)
#define LCM_LOGD(string, args...)  dprintf(1, "[LK/"LOG_TAG"]"string, ##args)
#else
#define LCM_LOGI(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#define LCM_LOGD(fmt, args...)  pr_debug("[KERNEL/"LOG_TAG"]"fmt, ##args)
#endif
static const unsigned int BL_MIN_LEVEL = 20;
static LCM_UTIL_FUNCS lcm_util;
#define SET_RESET_PIN(v)        (lcm_util.set_reset_pin((v)))
#define SET_GPIO_PIN(gpio,value)   (lcm_util.set_gpio_out(gpio,value))
#define SET_GPIO_DIR(gpio,dir)	 (lcm_util.set_gpio_dir(gpio,dir))
#define GPIO_OUT_ONE   1
#define GPIO_OUT_ZERO  0
#define MDELAY(n)               (lcm_util.mdelay(n))
#define UDELAY(n)               (lcm_util.udelay(n))

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define wrtie_cmd(cmd)										lcm_util.dsi_write_cmd(cmd)
#define write_regs(addr, pdata, byte_nums)					lcm_util.dsi_write_regs(addr, pdata, byte_nums)
#define read_reg(cmd)										lcm_util.dsi_dcs_read_lcm_reg(cmd)
#define read_reg_v2(cmd, buffer, buffer_size)   			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define set_gpio_lcd_enp(cmd) \
		lcm_util.set_gpio_lcd_enp_bias(cmd)

#ifndef BUILD_LK
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
/* #include <linux/jiffies.h> */
/* #include <linux/delay.h> */
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#endif



#define LCM_DSI_CMD_MODE				0
#define FRAME_WIDTH					(480)
#define FRAME_HEIGHT					(960)

#define LCM_PHYSICAL_WIDTH									(61880)
#define LCM_PHYSICAL_HEIGHT									(123750)

#define REGFLAG_DELAY             								0xFE
#define REGFLAG_END_OF_TABLE      							0xFC   // END OF REGISTERS MARKER



#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif


struct LCM_setting_table {
	unsigned int cmd;
	unsigned char count;
	unsigned char para_list[64];
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] ={
	/*{REGFLAG_DELAY, 10, {}},
    {0x28, 0, {}},
	{REGFLAG_DELAY, 20, {}},
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC2, 2,{0x07, 0x06}}, //1 Dot
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x00} },
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x11} },
	{0xC0,  1, {0x07} },
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x13} },

	{0xeb,  1, {0x12} },
	{REGFLAG_DELAY, 20, {}},
	{0xe8,  1, {0x01} },
	{0xeb,  1, {0x1e} },
	{REGFLAG_DELAY, 120, {}},
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x10, 0, {}},
	{REGFLAG_DELAY, 120, {}},
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x13} },
	{0xeb,  1, {0x00} },
	{0xe8,  1, {0x00} },
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
*/
	{0x28,0,{}},
	{0x10,0,{}},
	{REGFLAG_DELAY, 120, {}}

};

/*
static struct LCM_setting_table lcm_deep_sleep_mode_out_setting[] = {
	{0x11, 0, {}},
	{REGFLAG_DELAY, 60, {}},
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC2, 2,{0x07, 0x02}}, //1 Dot
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x00} },
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x11} },
	{0xC0,  1, {0x87} },
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x13} },
	{0xeb,  1, {0x00} },
	{REGFLAG_DELAY, 80, {}},

	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	{0x29, 0, {}},
	{REGFLAG_DELAY, 20, {}},

	{REGFLAG_END_OF_TABLE, 0x00, {}}
};*/
static struct LCM_setting_table lcm_initialization_setting[] = {
/*	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC6, 1,{0x07}},
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	{0x11,  0, {0x00}},
	{REGFLAG_DELAY, 120, {}},
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC0, 2,{0x77, 0x00}},
	{0xC1, 2,{0x12, 0x02}},
	{0xC2, 2,{0x07, 0x02}},
	{0xCC, 1,{0x30}},
#if 0
	{0xB0, 16, {0x00,0x10,0x1B,0x0F,0x14,0x08,0x0D,0x08,0x08,0x25,0x06,0x15,0x13,0xE6,0x2C,0x11} },
	{0xB1, 16, {0x00,0x10,0x1B,0x0F,0x14,0x08,0x0E,0x08,0x08,0x25,0x04,0x11,0x0F,0x27,0x2C,0x11} },
#else	//gamma 2.5
	{0xB0, 16, {0x00,0x0E,0x1C,0x0F,0x12,0x07,0x0C,0x08,0x07,0x25,0x06,0x15,0x13,0x24,0x28,0x08} },
	{0xB1, 16, {0x00,0x0E,0x16,0x0F,0x14,0x09,0x0E,0x08,0x08,0x26,0x07,0x17,0x14,0x23,0x28,0x0B} },
#endif
	{0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x11} },
	{0xB0,  1, {0x5d} },
	{0xB1,  1, {0x12} }, //12
	{0xB2, 1,{0x87}},
	{0xB3, 1,{0x80}},
	{0xB5, 1,{0x49}},
	{0xB7, 1,{0x89}},
	{0xB8, 1,{0x21}},
	{0xC1, 1,{0x78}},
	{0xC2, 1,{0x78}},
	{0xD0, 1,{0x88}},
	{REGFLAG_DELAY, 10, {}},
	{0xE0, 3,{0x00, 0x00, 0x02}},
	{0xE1, 11,{0x0A,0x96,0x0C,0x96,0x0B,0x96,0x0D,0x96,0x00,0x44,0x44}},
	{0xE2, 13,{0x33,0x33,0x44,0x44,0xCF,0x96,0xD1,0x96,0xD0,0x96,0xD2,0x96,0x00}},
	{0xE3, 4,{0x00, 0x00, 0x33, 0x33}},
	{0xE4, 2,{0x44, 0x44}},
	{0xE5, 16,{0x0C,0xD0,0x86,0x86,0x0E,0xD2,0x86,0x86,0x10,0xD4,0x86,0x86,0x12,0xD6,0x86,0x86}},
	{0xE6, 4,{0x00, 0x00, 0x33, 0x33}},
	{0xE7, 2,{0x44, 0x44}},
	{0xE8, 16,{0x0D,0xD1,0x86,0x86,0x0F,0xD3,0x86,0x86,0x11,0xD5,0x86,0x86,0x13,0xD7,0x86,0x86}},
	{0xEB, 7,{0x02,0x01,0x4E,0x4E,0xEE,0x44,0x00}},
	{0xEC, 2,{0x00, 0x00}},
	{0xED, 16,{0xFF,0xF1,0x04,0x56,0x72,0x3F,0xFF,0xFF,0xFF,0xFF,0xF3,0x27,0x65,0x40,0x1F,0xFF}},

	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	{0x29,  0, {0x00}},
	{REGFLAG_DELAY, 10, {}},
	#if 1
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC2, 2,{0x00, 0x02}}, //1 Dot
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x12} },
	{0xd1, 1,{0x81}},
	{0xd2, 1,{0x0c}},

	{REGFLAG_DELAY, 120, {}},

	{0xd1,  1, {0x01} }, //Return

	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x10}},
	{0xC2, 2,{0x07, 0x02}}, //Column
	{0xFF, 5,{0x77, 0x01, 0x00, 0x00, 0x00}},
	#endif
	{REGFLAG_END_OF_TABLE, 0x00, {} } */
 /* ST7701 Initial Code For BOE5.45IPS(BV055FGE-N40-8Q01)					   */
 {0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x10} },
 {0xC0,  2, {0x77, 0x00} },
 {0xC1,  2, {0x0A, 0x02} },
 {0xC2,  2, {0x07, 0x00} },
 {0xC7,  1, {0x04} },
 {0xCC,  1, {0x30} },
 {0xB0, 16, {0x80, 0x0D, 0x14, 0x0D, 0x11, 0x07, 0x43, 0x08, 0x07, 0x5C, 0x03, 0x11, 0x0F, 0x24, 0x2D, 0x1F} },
 {0xB1, 16, {0x40, 0x0D, 0x15, 0x0D, 0x11, 0x07, 0x43, 0x08, 0x08, 0x5C, 0x04, 0x12, 0x11, 0x25, 0x2E, 0x1F} },
 {0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x11} },
 {0xB0,  1, {0x93} },
 {0xB1,  1, {0x31} },
 {0xB2,  1, {0x07} },
 {0xB3,  1, {0x80} },
 {0xB5,  1, {0x4E} },
 {0xB7,  1, {0x87} },
 {0xB8,  1, {0x20} },
 {0xBB,  1, {0x03} },
 {0xC1,  1, {0x08} },
 {0xC2,  1, {0x08} },
 {0xD0,  1, {0x88} },
 {REGFLAG_DELAY, 20, {}},
 {0xE0,  3, {0x00, 0x00, 0x02} },
 {0xE1, 11, {0x05, 0x8C, 0x00, 0x00, 0x04, 0x8C, 0x00, 0x00, 0x00, 0x20, 0x20} },
 {0xE2, 13, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
 {0xE3,  4, {0x00, 0x00, 0x33, 0x33} },
 {0xE4,  2, {0x22, 0x00} },
 {0xE5, 16, {0x09, 0xD5, 0xBB, 0x8C, 0x07, 0xD5, 0xBB, 0x8C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
 {0xE6,  4, {0x00, 0x00, 0x33, 0x00} },
 {0xE7,  2, {0x22, 0x00} },
 {0xE8, 16, {0x08, 0xD5, 0xBB, 0x8C, 0x06, 0xD5, 0xBB, 0x8C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} },
 {0xEB,  7, {0x02, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00} },
 {0xEC,  2, {0x00, 0x00} },
 //{0xED, 16, {0xAA, 0x54, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x45, 0xAA} },
 {0xED, 16, {0xAB, 0x54, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xB0, 0x45, 0xBA} },
 {0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x00} },
 {0x35,  1, {0x00} },
 {0x36,  1, {0x10} },
 {0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x13} },
 {0xEF,  1, {0x08} },
 {0xFF,  5, {0x77, 0x01, 0x00, 0x00, 0x00} },
 {0x11,  0, {0x00}},
 {REGFLAG_DELAY, 120, {}},
 {0x29,  0, {0x00}},
 {REGFLAG_DELAY, 20, {}},
 {REGFLAG_END_OF_TABLE, 0x00, {} }


 };

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
	unsigned int i;
	unsigned cmd;

	for (i = 0; i < count; i++) {
		cmd = table[i].cmd;

		switch (cmd) {

		case REGFLAG_DELAY:
			if (table[i].count <= 10)
				MDELAY(table[i].count);
			else
				MDELAY(table[i].count);
			break;

		case REGFLAG_END_OF_TABLE:
			break;

		default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
		}
	}
}


static void lcm_set_util_funcs(const LCM_UTIL_FUNCS * util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
		memset(params, 0, sizeof(LCM_PARAMS));

		params->type   = LCM_TYPE_DSI;
		params->width  = FRAME_WIDTH;
		params->height = FRAME_HEIGHT;

		//enable tearing-free
		params->dbi.te_mode 				= LCM_DBI_TE_MODE_VSYNC_ONLY;
		params->dbi.te_edge_polarity		= LCM_POLARITY_RISING;
		params->dsi.mode   					= SYNC_PULSE_VDO_MODE ;

		// DSI
		/* Command mode setting */
		params->dsi.LANE_NUM				= LCM_TWO_LANE;
		//The following defined the fomat for data coming from LCD engine.
		params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

		// Video mode setting
		params->dsi.PS=LCM_PACKED_PS_24BIT_RGB888;

		params->dsi.vertical_sync_active				= 2;//2
		params->dsi.vertical_backporch					= 16;//12
		params->dsi.vertical_frontporch					= 12;//54
		params->dsi.vertical_active_line				= FRAME_HEIGHT;

		params->dsi.horizontal_sync_active				= 10;//8
		params->dsi.horizontal_backporch				= 26;//60
		params->dsi.horizontal_frontporch				= 26;//60
		params->dsi.horizontal_active_pixel				= FRAME_WIDTH;


		params->dsi.PLL_CLOCK = 200; //this value must be in MTK suggested table

	//	params->dsi.cont_clock = 1;
		params->dsi.ssc_disable							= 1;
		params->dsi.ssc_range							= 4;

		//ESD CHECK
		params->dsi.esd_check_enable = 1;
	//params->dsi.noncont_clock  	= 1;
	//	params->dsi.noncont_clock_period                = 1;
		params->dsi.customization_esd_check_enable = 1;
		params->dsi.lcm_esd_check_table[0].cmd			= 0x0a;
		params->dsi.lcm_esd_check_table[0].count		= 1;
		params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9c;

		}



static void lcm_init_power(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_init_power() enter\n");
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(20);

#else
	printk("[Kernel/LCM] lcm_init_power() enter\n");
	//lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
//	MDELAY(10);
#endif
}

static void lcm_suspend_power(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend_power() enter\n");
	//lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
//	MDELAY(20);

#else
	printk("[Kernel/LCM] lcm_suspend_power() enter\n");

	//lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ZERO);
//	MDELAY(20);

#endif
}

static void lcm_resume_power(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume_power() enter\n");
	lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	MDELAY(20);

#else
	printk("[Kernel/LCM] lcm_resume_power() enter\n");
	//lcm_set_gpio_output(GPIO_LCD_PWR_EN, GPIO_OUT_ONE);
	//MDELAY(20);

#endif
}

static void lcm_suspend(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_suspend() enter\n");

	lcm_set_gpio_output(GPIO_LCD_BL_EN,GPIO_OUT_ZERO);
	MDELAY(10);
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);

	//SET_RESET_PIN(0);
	//MDELAY(10);
#else
	printk("[Kernel/LCM] lcm_suspend() enter*************************\n");
	push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);

	printk("[Kernel/LCM] lcm_suspend() end###########################\n");

#endif

}


static unsigned int lcm_esd_check(void)
{
	return FALSE;
}

static void lcm_setbacklight(unsigned int level)
{
}

static unsigned int lcm_ata_check(unsigned char *buffer)
{
	return 0;
}

static void lcm_update(unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
}

static void *lcm_switch_mode(int mode)
{
	return NULL;
}

static void lcm_init(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_init() enter\n");
 push_table(lcm_deep_sleep_mode_in_setting, sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(120);

	// when phone initial , config output high, enable backlight drv chip
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	MDELAY(10);

	printf("[LK/LCM] lcm_init() end\n");
#else
	printk("[Kernel/LCM] lcm_init() enter\n");
		push_table( lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
		printk("[Kernel/LCM] lcm_init() end\n");
#endif

}

static void lcm_resume(void)
{
#ifdef BUILD_LK
	printf("[LK/LCM] lcm_resume() enter\n");

	SET_RESET_PIN(1);
	MDELAY(10);

	SET_RESET_PIN(0);
	MDELAY(10);

	SET_RESET_PIN(1);
	MDELAY(120);

	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

//	lcm_set_gpio_output(GPIO_LCD_BL_EN, GPIO_OUT_ONE);
	MDELAY(10);

#else
	printk("[Kernel/LCM] lcm_resume() enter lcm_init\n");
	//push_table(lcm_deep_sleep_mode_out_setting, sizeof(lcm_deep_sleep_mode_out_setting) / sizeof(struct LCM_setting_table), 1);
	push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);

	printk("[Kernel/LCM] lcm_resume() lcm_init end\n");
#endif
}

static unsigned int lcm_compare_id(void)
{
	return 1;
}

LCM_DRIVER st7701s_fwvga_vdo_lcm_drv_lide = {
	.name = "LIDE-ST7701S",
	.set_util_funcs = lcm_set_util_funcs,
	.get_params = lcm_get_params,
	.init = lcm_init,
	.suspend = lcm_suspend,
	.resume = lcm_resume,
	.compare_id = lcm_compare_id,
	.init_power = lcm_init_power,
	.resume_power = lcm_resume_power,
	.suspend_power = lcm_suspend_power,
	.esd_check = lcm_esd_check,
	.set_backlight = lcm_setbacklight,
	.ata_check = lcm_ata_check,
	.update = lcm_update,
	.switch_mode = lcm_switch_mode,
};

