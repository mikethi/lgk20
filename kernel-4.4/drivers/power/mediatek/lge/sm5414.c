/*
* sm5414.c -- Silicon Mitus SM5414 Charger IC driver
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
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/power_supply.h>
#include <mtk_charger_intf.h>

/* Registers */
#define SM5414_INT1 (0x00)
#define SM5414_INT2 (0x01)
#define SM5414_INT3 (0x02)
#define SM5414_INTMSK1 (0x03)
#define SM5414_INTMSK2 (0x04)
#define SM5414_INTMSK3 (0x05)
#define SM5414_STATUS (0x06)
#define SM5414_CTRL (0x07)
#define SM5414_VBUSCTRL (0x08)
#define SM5414_CHGCTRL1 (0x09)
#define SM5414_CHGCTRL2 (0x0A)
#define SM5414_CHGCTRL3 (0x0B)
#define SM5414_CHGCTRL4 (0x0C)
#define SM5414_CHGCTRL5 (0x0D)

/* Bit Mask & Shift */
#define SM5414_INT1_VBUSOVP_MASK (0x80)
#define SM5414_INT1_VBUSOVP_SHIFT (7)
#define SM5414_INT1_VBUSUVLO_MASK (0x40)
#define SM5414_INT1_VBUSUVLO_SHIFT (6)
#define SM5414_INT1_VBUSINOK_MASK (0x20)
#define SM5414_INT1_VBUSINOK_SHIFT (5)
#define SM5414_INT1_AICL_MASK (0x10)
#define SM5414_INT1_AICL_SHIFT (4)
#define SM5414_INT1_VBUSLIMIT_MASK (0x08)
#define SM5414_INT1_VBUSLIMIT_SHIFT (3)
#define SM5414_INT1_BATOVP_MASK (0x04)
#define SM5414_INT1_BATOVP_SHIFT (2)
#define SM5414_INT1_THEMSHDN_MASK (0x02)
#define SM5414_INT1_THEMSHDN_SHIFT (1)
#define SM5414_INT1_THEMREG_MASK (0x01)
#define SM5414_INT1_THEMREG_SHIFT (0)

#define SM5414_INT2_FASTTMROFF_MASK (0x80)
#define SM5414_INT2_FASTTMROFF_SHIFT (7)
#define SM5414_INT2_NOBAT_MASK (0x40)
#define SM5414_INT2_NOBAT_SHIFT (6)
#define SM5414_INT2_WEAKBAT_MASK (0x20)
#define SM5414_INT2_WEAKBAT_SHIFT (5)
#define SM5414_INT2_OTGFAIL_MASK (0x10)
#define SM5414_INT2_OTGFAIL_SHIFT (4)
#define SM5414_INT2_PRETMROFF_MASK (0x08)
#define SM5414_INT2_PRETMROFF_SHIFT (3)
#define SM5414_INT2_CHGRSTF_MASK (0x04)
#define SM5414_INT2_CHGRSTF_SHIFT (2)
#define SM5414_INT2_DONE_MASK (0x02)
#define SM5414_INT2_DONE_SHIFT (1)
#define SM5414_INT2_TOPOFF_MASK (0x01)
#define SM5414_INT2_TOPOFF_SHIFT (0)

#define SM5414_INT3_VSYSOK_MASK (0x08)
#define SM5414_INT3_VSYSOK_SHIFT (3)
#define SM5414_INT3_VSYSNG_MASK (0x04)
#define SM5414_INT3_VSYSNG_SHIFT (2)
#define SM5414_INT3_VSYSOLP_MASK (0x02)
#define SM5414_INT3_VSYSOLP_SHIFT (1)
#define SM5414_INT3_DISLIMIT_MASK (0x01)
#define SM5414_INT3_DISLIMIT_SHIFT (0)

#define SM5414_INTMSK1_VBUSOVPM_MASK (0x80)
#define SM5414_INTMSK1_VBUSOVPM_SHIFT (7)
#define SM5414_INTMSK1_VBUSUVLOM_MASK (0x40)
#define SM5414_INTMSK1_VBUSUVLOM_SHIFT (6)
#define SM5414_INTMSK1_VBUSINOKM_MASK (0x20)
#define SM5414_INTMSK1_VBUSINOKM_SHIFT (5)
#define SM5414_INTMSK1_AICLM_MASK (0x10)
#define SM5414_INTMSK1_AICLM_SHIFT (4)
#define SM5414_INTMSK1_VBUSLIMITM_MASK (0x08)
#define SM5414_INTMSK1_VBUSLIMITM_SHIFT (3)
#define SM5414_INTMSK1_BATOVPM_MASK (0x04)
#define SM5414_INTMSK1_BATOVPM_SHIFT (2)
#define SM5414_INTMSK1_THEMSHDNM_MASK (0x02)
#define SM5414_INTMSK1_THEMSHDNM_SHIFT (1)
#define SM5414_INTMSK1_THEMREGM_MASK (0x01)
#define SM5414_INTMSK1_THEMREGM_SHIFT (0)

#define SM5414_INTMSK2_FASTTMROFFM_MASK (0x80)
#define SM5414_INTMSK2_FASTTMROFFM_SHIFT (7)
#define SM5414_INTMSK2_NOBATM_MASK (0x40)
#define SM5414_INTMSK2_NOBATM_SHIFT (6)
#define SM5414_INTMSK2_WEAKBATM_MASK (0x20)
#define SM5414_INTMSK2_WEAKBATM_SHIFT (5)
#define SM5414_INTMSK2_OTGFAILM_MASK (0x10)
#define SM5414_INTMSK2_OTGFAILM_SHIFT (4)
#define SM5414_INTMSK2_PRETMROFFM_MASK (0x08)
#define SM5414_INTMSK2_PRETMROFFM_SHIFT (3)
#define SM5414_INTMSK2_CHGRSTFM_MASK (0x04)
#define SM5414_INTMSK2_CHGRSTFM_SHIFT (2)
#define SM5414_INTMSK2_DONEM_MASK (0x02)
#define SM5414_INTMSK2_DONEM_SHIFT (1)
#define SM5414_INTMSK2_TOPOFFM_MASK (0x01)
#define SM5414_INTMSK2_TOPOFFM_SHIFT (0)

#define SM5414_INTMSK3_VSYSOKM_MASK (0x08)
#define SM5414_INTMSK3_VSYSOKM_SHIFT (3)
#define SM5414_INTMSK3_VSYSNGM_MASK (0x04)
#define SM5414_INTMSK3_VSYSNGM_SHIFT (2)
#define SM5414_INTMSK3_VSYSOLPM_MASK (0x02)
#define SM5414_INTMSK3_VSYSOLPM_SHIFT (1)
#define SM5414_INTMSK3_DISLIMITM_MASK (0x01)
#define SM5414_INTMSK3_DISLIMITM_SHIFT (0)

#define SM5414_STATUS_VBUSOVP_MASK (0x80)
#define SM5414_STATUS_VBUSOVP_SHIFT (7)
#define SM5414_STATUS_VBUSUVLO_MASK (0x40)
#define SM5414_STATUS_VBUSUVLO_SHIFT (6)
#define SM5414_STATUS_TOPOFF_MASK (0x20)
#define SM5414_STATUS_TOPOFF_SHIFT (5)
#define SM5414_STATUS_VSYSOLP_MASK (0x10)
#define SM5414_STATUS_VSYSOLP_SHIFT (4)
#define SM5414_STATUS_DISLIMIT_MASK (0x08)
#define SM5414_STATUS_DISLIMIT_SHIFT (3)
#define SM5414_STATUS_THEMSHDN_MASK (0x04)
#define SM5414_STATUS_THEMSHDN_SHIFT (2)
#define SM5414_STATUS_BATDET_MASK (0x02)
#define SM5414_STATUS_BATDET_SHIFT (1)
#define SM5414_STATUS_SUSPEND_MASK (0x01)
#define SM5414_STATUS_SUSPEND_SHIFT (0)

#define SM5414_CTRL_ENCOMPARATOR_MASK (0x40)
#define SM5414_CTRL_ENCOMPARATOR_SHIFT (6)
#define SM5414_CTRL_RESET_MASK (0x08)
#define SM5414_CTRL_RESET_SHIFT (3)
#define SM5414_CTRL_SUSPEN_MASK (0x04)
#define SM5414_CTRL_SUSPEN_SHIFT (2)
#define SM5414_CTRL_CHGEN_MASK (0x02)
#define SM5414_CTRL_CHGEN_SHIFT (1)
#define SM5414_CTRL_ENBOOST_MASK (0x01)
#define SM5414_CTRL_ENBOOST_SHIFT (0)

#define SM5414_VBUSCTRL_VBUSLIMIT_MASK (0x3F)
#define SM5414_VBUSCTRL_VBUSLIMIT_SHIFT (0)

#define SM5414_CHGCTRL1_AICLTH_MASK (0x70)
#define SM5414_CHGCTRL1_AICLTH_SHIFT (4)
#define SM5414_CHGCTRL1_AUTOSTOP_MASK (0x08)
#define SM5414_CHGCTRL1_AUTOSTOP_SHIFT (3)
#define SM5414_CHGCTRL1_AICLEN_MASK (0x04)
#define SM5414_CHGCTRL1_AICLEN_SHIFT (2)
#define SM5414_CHGCTRL1_PRECHG_MASK (0x03)
#define SM5414_CHGCTRL1_PRECHG_SHIFT (0)

#define SM5414_CHGCTRL2_FASTCHG_MASK (0x3F)
#define SM5414_CHGCTRL2_FASTCHG_SHIFT (0)

#define SM5414_CHGCTRL3_BATREG_MASK (0xF0)
#define SM5414_CHGCTRL3_BATREG_SHIFT (4)
#define SM5414_CHGCTRL3_WEAKBAT_MASK (0x0F)
#define SM5414_CHGCTRL3_WEAKBAT_SHIFT (0)

#define SM5414_CHGCTRL4_TOPOFF_MASK (0x78)
#define SM5414_CHGCTRL4_TOPOFF_SHIFT (3)
#define SM5414_CHGCTRL4_DISLIMIT_MASK (0x07)
#define SM5414_CHGCTRL4_DISLIMIT_SHIFT (0)

#define SM5414_CHGCTRL5_VOTG_MASK (0x30)
#define SM5414_CHGCTRL5_VOTG_SHIFT (4)
#define SM5414_CHGCTRL5_FASTTIMER_MASK (0x0C)
#define SM5414_CHGCTRL5_FASTTIMER_SHIFT (2)
#define SM5414_CHGCTRL5_TOPOFFTIMER_MASK (0x03)
#define SM5414_CHGCTRL5_TOPOFFTIMER_SHIFT (0)

#define SM5414_INTMSK_NUM (3)

#define SM5414_VBUSLIMIT_UA_MIN (100000)
#define SM5414_VBUSLIMIT_UA_MAX (2050000)
#define SM5414_VBUSLIMIT_UA_STEP (50000)

#define SM5414_AICLTH_UV_MIN (4300000)
#define SM5414_AICLTH_UV_MAX (4900000)
#define SM5414_AICLTH_UV_STEP (100000)

#define SM5414_PRECHG_UA_MIN (150000)
#define SM5414_PRECHG_UA_MAX (450000)
#define SM5414_PRECHG_UA_STEP (100000)

#define SM5414_FASTCHG_UA_MIN (100000)
#define SM5414_FASTCHG_UA_MAX (2500000)
#define SM5414_FASTCHG_UA_STEP (50000)

#define SM5414_BATREG_UV_MIN (4100000)
#define SM5414_BATREG_UV_MAX (4475000)
#define SM5414_BATREG_UV_STEP (25000)

#define SM5414_WEAKBAT_UV_MIN (3000000)
#define SM5414_WEAKBAT_UV_MAX (3750000)
#define SM5414_WEAKBAT_UV_STEP (50000)

#define SM5414_TOPOFF_UA_MIN (100000)
#define SM5414_TOPOFF_UA_MAX (650000)
#define SM5414_TOPOFF_UA_STEP (50000)

#define SM5414_DISLIMIT_UA_MIN (2000000)
#define SM5414_DISLIMIT_UA_MAX (5000000)
#define SM5414_DISLIMIT_UA_STEP (500000)

#define SM5414_VOTG_UV_MIN (5000000)
#define SM5414_VOTG_UV_MAX (5200000)
#define SM5414_VOTG_UV_STEP (100000)

#define SM5414_AICL_UA_STEP (25000)

static u32 sm5414_fasttimer_min[] = {
	210,
	270,
	330,
};

static u32 sm5414_topofftimer_min[] = {
	10,
	20,
	30,
	45,
};

struct sm5414_init_data {
	u8 intmsk[SM5414_INTMSK_NUM];
	u32 aiclth;
	bool aiclen;
	u32 prechg;
	u32 batreg;
	u32 weakbat;
	u32 topoff;
	u32 dislimit;
	u32 votg;
	u32 fasttimer;
	u32 topofftimer;
};

struct sm5414 {
	struct i2c_client *client;
	struct device *dev;
	struct mutex lock;

	struct delayed_work done_dwork;
	bool done;

	/* charger class */
	struct charger_device *chg_dev;
	struct charger_properties chg_props;

	/* device tree */
	bool is_polling_mode;
	int irq_gpio;
	struct sm5414_init_data init_data;

	/* debugfs */
	struct dentry *debugfs;
	u32 debug_addr;
};

static struct sm5414_init_data sm5414_init_data_default = {
	.intmsk = {0xFF, 0xFF, 0xFF},
	.aiclth = 4500000,	/* 4.5 V */
	.aiclen = true,		/* enable */
	.prechg = 450000,	/* 450 mA */
	.batreg = 4200000,	/* 4.2 V */
	.weakbat = 3600000,	/* 3.6 V */
	.topoff = 100000,	/* 100 mA */
	.dislimit = 4000000,	/* 4.0 A */
	.votg = 5000000, 	/* 5.0 V */
	.fasttimer = 0, 	/* disable */
	.topofftimer = 45,	/* 45 minutes */
};

static u8 sm5414_handle_reserved(u8 reg, u8 data)
{
	switch (reg) {
	case SM5414_INTMSK3:
		data |= 0xF0;
		break;
	case SM5414_CTRL:
		data &= 0x4F;
		break;
	case SM5414_VBUSCTRL:
		data &= 0x3F;
		break;
	case SM5414_CHGCTRL1:
		data &= 0x7F;
		break;
	case SM5414_CHGCTRL2:
		data &= 0x3F;
		break;
	case SM5414_CHGCTRL4:
		data &= 0x7F;
		break;
	case SM5414_CHGCTRL5:
		data &= 0x3F;
		break;
	}

	return data;
}

static int sm5414_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int rc;

	rc = i2c_smbus_read_byte_data(client, reg);
	if (rc < 0)
		dev_err(&client->dev, "failed to read. rc=%d\n", rc);

	*data = (rc & 0xFF);

	return rc < 0 ? rc : 0;
}

static int sm5414_write(struct i2c_client *client, u8 reg, u8 data)
{
	int rc;

	rc = i2c_smbus_write_byte_data(client, reg, data);
	if (rc < 0)
		dev_err(&client->dev, "failed to write. rc=%d\n", rc);

	return rc;
}

static int sm5414_masked_write(struct i2c_client *client, u8 reg, u8 data, u8 mask)
{
	struct sm5414 *chip = i2c_get_clientdata(client);
	u8 tmp;
	int rc = 0;

	mutex_lock(&chip->lock);

	rc = sm5414_read(client, reg, &tmp);
	if (rc < 0)
		goto out;

	tmp = (data & mask) | (tmp & (~mask));
	tmp = sm5414_handle_reserved(reg, tmp);
	rc = sm5414_write(client, reg, tmp);

out:
	mutex_unlock(&chip->lock);
	return rc;
}

static int sm5414_set_suspen(struct sm5414 *chip, bool en)
{
	u8 data = en ? 1 : 0;

	return sm5414_masked_write(chip->client, SM5414_CTRL,
			data << SM5414_CTRL_SUSPEN_SHIFT,
			SM5414_CTRL_SUSPEN_MASK);
}

static int sm5414_get_suspen(struct sm5414 *chip)
{
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CTRL, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CTRL_SUSPEN_MASK;
	data >>= SM5414_CTRL_SUSPEN_SHIFT;

	return data;
}

static int sm5414_set_chgen(struct sm5414 *chip, bool en)
{
	u8 data = en ? 1 : 0;

	return sm5414_masked_write(chip->client, SM5414_CTRL,
			data << SM5414_CTRL_CHGEN_SHIFT,
			SM5414_CTRL_CHGEN_MASK);
}

static int sm5414_get_chgen(struct sm5414 *chip)
{
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CTRL, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CTRL_CHGEN_MASK;
	data >>= SM5414_CTRL_CHGEN_SHIFT;

	return data;
}

static int sm5414_set_enboost(struct sm5414 *chip, bool en)
{
	u8 data = en ? 1 : 0;

	return sm5414_masked_write(chip->client, SM5414_CTRL,
			data << SM5414_CTRL_ENBOOST_SHIFT,
			SM5414_CTRL_ENBOOST_MASK);
}

static int sm5414_get_enboost(struct sm5414 *chip)
{
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CTRL, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CTRL_ENBOOST_MASK;
	data >>= SM5414_CTRL_ENBOOST_SHIFT;

	return data;
}

static int sm5414_set_vbuslimit(struct sm5414 *chip, u32 uA)
{
	u8 data;

	if (uA < SM5414_VBUSLIMIT_UA_MIN)
		uA = SM5414_VBUSLIMIT_UA_MIN;
	if (uA > SM5414_VBUSLIMIT_UA_MAX)
		uA = SM5414_VBUSLIMIT_UA_MAX;

	data = (uA - SM5414_VBUSLIMIT_UA_MIN) / SM5414_VBUSLIMIT_UA_STEP;
	if (data > 0x27)
		data = 0x27;

	return sm5414_masked_write(chip->client, SM5414_VBUSCTRL,
			data << SM5414_VBUSCTRL_VBUSLIMIT_SHIFT,
			SM5414_VBUSCTRL_VBUSLIMIT_MASK);
}

static u32 sm5414_get_vbuslimit(struct sm5414 *chip)
{
	u32 uA;
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_VBUSCTRL, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_VBUSCTRL_VBUSLIMIT_MASK;
	data >>= SM5414_VBUSCTRL_VBUSLIMIT_SHIFT;

	uA = SM5414_VBUSLIMIT_UA_MIN + (data * SM5414_VBUSLIMIT_UA_STEP);
	if (uA > SM5414_VBUSLIMIT_UA_MAX)
		uA = SM5414_VBUSLIMIT_UA_MAX;

	return uA;
}

static int sm5414_set_aiclth(struct sm5414 *chip, u32 uV)
{
	u8 data;

	if (uV < SM5414_AICLTH_UV_MIN)
		uV = SM5414_AICLTH_UV_MIN;
	if (uV > SM5414_AICLTH_UV_MAX)
		uV = SM5414_AICLTH_UV_MAX;

	data = (uV - SM5414_AICLTH_UV_MIN) / SM5414_AICLTH_UV_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL1,
			data << SM5414_CHGCTRL1_AICLTH_SHIFT,
			SM5414_CHGCTRL1_AICLTH_MASK);
}

static u32 sm5414_get_aiclth(struct sm5414 *chip)
{
	u32 uV;
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL1, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL1_AICLTH_MASK;
	data >>= SM5414_CHGCTRL1_AICLTH_SHIFT;

	uV = SM5414_AICLTH_UV_MIN + (data * SM5414_AICLTH_UV_STEP);
	if (uV > SM5414_AICLTH_UV_MAX)
		uV = SM5414_AICLTH_UV_MAX;

	return uV;
}

static int sm5414_set_autostop(struct sm5414 *chip, bool en)
{
	u8 data = en ? 1 : 0;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL1,
			data << SM5414_CHGCTRL1_AUTOSTOP_SHIFT,
			SM5414_CHGCTRL1_AUTOSTOP_MASK);
}

static bool sm5414_get_autostop(struct sm5414 *chip)
{
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL1, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL1_AUTOSTOP_MASK;
	data >>= SM5414_CHGCTRL1_AUTOSTOP_SHIFT;

	return data ? true : false;
}

static int sm5414_set_aiclen(struct sm5414 *chip, bool en)
{
	u8 data = en ? 1 : 0;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL1,
			data << SM5414_CHGCTRL1_AICLEN_SHIFT,
			SM5414_CHGCTRL1_AICLEN_MASK);
}

static int sm5414_set_prechg(struct sm5414 *chip, u32 uA)
{
	u8 data;

	if (uA < SM5414_PRECHG_UA_MIN)
		uA = SM5414_PRECHG_UA_MIN;
	if (uA > SM5414_PRECHG_UA_MAX)
		uA = SM5414_PRECHG_UA_MAX;

	data = (uA - SM5414_PRECHG_UA_MIN) / SM5414_PRECHG_UA_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL1,
			data << SM5414_CHGCTRL1_PRECHG_SHIFT,
			SM5414_CHGCTRL1_PRECHG_MASK);
}

static int sm5414_set_fastchg(struct sm5414 *chip, u32 uA)
{
	u8 data;

	if (uA < SM5414_FASTCHG_UA_MIN)
		uA = SM5414_FASTCHG_UA_MIN;
	if (uA > SM5414_FASTCHG_UA_MAX)
		uA = SM5414_FASTCHG_UA_MAX;

	data = (uA - SM5414_FASTCHG_UA_MIN) / SM5414_FASTCHG_UA_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL2,
			data << SM5414_CHGCTRL2_FASTCHG_SHIFT,
			SM5414_CHGCTRL2_FASTCHG_MASK);
}

static u32 sm5414_get_fastchg(struct sm5414 *chip)
{
	u32 uA;
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL2, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL2_FASTCHG_MASK;
	data >>= SM5414_CHGCTRL2_FASTCHG_SHIFT;

	uA = SM5414_FASTCHG_UA_MIN + (data * SM5414_FASTCHG_UA_STEP);
	if (uA > SM5414_FASTCHG_UA_MAX)
		uA = SM5414_FASTCHG_UA_MAX;

	return uA;
}

static int sm5414_set_batreg(struct sm5414 *chip, u32 uV)
{
	u8 data;

	if (uV < SM5414_BATREG_UV_MIN)
		uV = SM5414_BATREG_UV_MIN;
	if (uV > SM5414_BATREG_UV_MAX)
		uV = SM5414_BATREG_UV_MAX;

	data = (uV - SM5414_BATREG_UV_MIN) / SM5414_BATREG_UV_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL3,
			data << SM5414_CHGCTRL3_BATREG_SHIFT,
			SM5414_CHGCTRL3_BATREG_MASK);
}

static u32 sm5414_get_batreg(struct sm5414 *chip)
{
	u32 uV;
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL3, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL3_BATREG_MASK;
	data >>= SM5414_CHGCTRL3_BATREG_SHIFT;

	uV = SM5414_BATREG_UV_MIN + (data * SM5414_BATREG_UV_STEP);
	if (uV > SM5414_BATREG_UV_MAX)
		uV = SM5414_BATREG_UV_MAX;

	return uV;
}

static int sm5414_set_weakbat(struct sm5414 *chip, u32 uV)
{
	u8 data;

	if (uV < SM5414_WEAKBAT_UV_MIN)
		uV = SM5414_WEAKBAT_UV_MIN;
	if (uV > SM5414_WEAKBAT_UV_MAX)
		uV = SM5414_WEAKBAT_UV_MAX;

	data = (uV - SM5414_WEAKBAT_UV_MIN) / SM5414_WEAKBAT_UV_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL3,
			data << SM5414_CHGCTRL3_WEAKBAT_SHIFT,
			SM5414_CHGCTRL3_WEAKBAT_MASK);
}

static int sm5414_set_topoff(struct sm5414 *chip, u32 uA)
{
	u8 data;

	if (uA < SM5414_TOPOFF_UA_MIN)
		uA = SM5414_TOPOFF_UA_MIN;
	if (uA > SM5414_TOPOFF_UA_MAX)
		uA = SM5414_TOPOFF_UA_MAX;

	data = (uA - SM5414_TOPOFF_UA_MIN) / SM5414_TOPOFF_UA_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL4,
			data << SM5414_CHGCTRL4_TOPOFF_SHIFT,
			SM5414_CHGCTRL4_TOPOFF_MASK);
}

static u32 sm5414_get_topoff(struct sm5414 *chip)
{
	u32 uA;
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL4, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL4_TOPOFF_MASK;
	data >>= SM5414_CHGCTRL4_TOPOFF_SHIFT;

	uA = SM5414_TOPOFF_UA_MIN + (data * SM5414_TOPOFF_UA_STEP);
	if (uA > SM5414_TOPOFF_UA_MAX)
		uA = SM5414_TOPOFF_UA_MAX;

	return uA;
}

static int sm5414_set_dislimit(struct sm5414 *chip, u32 uA)
{
	u8 data = 0;

	if (uA < SM5414_DISLIMIT_UA_MIN)
		uA = 0;
	if (uA > SM5414_DISLIMIT_UA_MAX)
		uA = SM5414_DISLIMIT_UA_MAX;

	if (uA)
		data = (uA - SM5414_DISLIMIT_UA_MIN) / SM5414_DISLIMIT_UA_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL4,
			data << SM5414_CHGCTRL4_DISLIMIT_SHIFT,
			SM5414_CHGCTRL4_DISLIMIT_MASK);
}

static int sm5414_set_votg(struct sm5414 *chip, u32 uV)
{
	u8 data;

	if (uV < SM5414_VOTG_UV_MIN)
		uV = SM5414_VOTG_UV_MIN;
	if (uV > SM5414_VOTG_UV_MAX)
		uV = SM5414_VOTG_UV_MAX;

	data = (uV - SM5414_VOTG_UV_MIN) / SM5414_VOTG_UV_STEP;

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL5,
			data << SM5414_CHGCTRL5_VOTG_SHIFT,
			SM5414_CHGCTRL5_VOTG_MASK);
}

static int sm5414_set_fasttimer(struct sm5414 *chip, u32 min)
{
	u8 data;

	for (data = 0; data < ARRAY_SIZE(sm5414_fasttimer_min); data++) {
		if (sm5414_fasttimer_min[data] == min)
			break;
	}

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL5,
			data << SM5414_CHGCTRL5_FASTTIMER_SHIFT,
			SM5414_CHGCTRL5_FASTTIMER_MASK);
}

static u32 sm5414_get_fasttimer(struct sm5414 *chip)
{
	u8 data;
	int rc;

	rc = sm5414_read(chip->client, SM5414_CHGCTRL5, &data);
	if (rc < 0)
		return rc;

	data &= SM5414_CHGCTRL5_FASTTIMER_MASK;
	data >>= SM5414_CHGCTRL5_FASTTIMER_SHIFT;

	if (data >= ARRAY_SIZE(sm5414_fasttimer_min))
		return 0;

	return sm5414_fasttimer_min[data];
}

static int sm5414_set_topofftimer(struct sm5414 *chip, u32 min)
{
	u8 data;

	for (data = 0; data < ARRAY_SIZE(sm5414_topofftimer_min); data++) {
		if (sm5414_topofftimer_min[data] == min)
			break;
	}

	return sm5414_masked_write(chip->client, SM5414_CHGCTRL5,
			data << SM5414_CHGCTRL5_TOPOFFTIMER_SHIFT,
			SM5414_CHGCTRL5_TOPOFFTIMER_MASK);
}

static void sm5414_done_work(struct work_struct *work)
{
	struct sm5414 *chip = container_of(to_delayed_work(work),
			struct sm5414, done_dwork);

	if (!chip->done)
		return;

	if (!chip->chg_dev)
		return;

	if (chip->chg_dev->is_polling_mode)
		return;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_EOC);
}

static void sm5414_irq_notify_done(struct sm5414 *chip)
{
	if (chip->done)
		return;

	chip->done = true;

	schedule_delayed_work(&chip->done_dwork, msecs_to_jiffies(50));
}

static void sm5414_irq_notify_chgrstf(struct sm5414 *chip)
{
	if (!chip->done)
		return;

	chip->done = false;
	if (cancel_delayed_work(&chip->done_dwork))
		return;

	if (!chip->chg_dev)
		return;

	if (chip->chg_dev->is_polling_mode)
		return;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
}

static void sm5414_irq_notify_fasttmroff(struct sm5414 *chip)
{
	if (chip->done) {
		chip->done = false;
		cancel_delayed_work(&chip->done_dwork);
	}

	if (!chip->chg_dev)
		return;

	charger_dev_notify(chip->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
}

static const char *sm5414_irq_msg[] = {
	/* INT1 */
	"Thermal regulation threshold detected",	/* THEMREG */
	"Thermal shutdown detected",			/* THEMSHDN */
	"BATREG OVP detected",				/* BATOVP */
	"VBUS Input Current Limit detected",		/* VBUSLIMIT */
	"AICL threshold detected",			/* AICL */
	"Valid VBUS detected",				/* VBUSINOK */
	"VBUS falling UVLO event detected",		/* VBUSUVLO */
	"VBUS OVP event detected",			/* VBUSOVP */
	/* INT2 */
	"Top-Off threshold detected",			/* TOPOFF */
	"Top-Off charge timer expired",			/* DONE */
	"Charger restart detected",			/* CHGRSTF */
	"Pre-charge timer expired",			/* PRETMROFF */
	"Boost failed detected due to overload",	/* OTGFAIL */
	"Weak Battery threshold detected",		/* WEAKBAT */
	"No Battery threshold detected",		/* NOBAT */
	"Fast-charge timer expired",			/* FASTTMROFF */
	/* INT3 */
	"Current Limit threshold detected",		/* DISLIMIT */
	"VSYS Overload condition debounced detected",	/* VSYSOLP */
	"VSYS falling 3.4V detected",			/* VSYSNG */
	"VSYS rising 3.6V detected",			/* VSYSOK */
};

static void sm5414_irq_dump_msg(struct sm5414 *chip, u8 int1, u8 int2, u8 int3)
{
	u32 data = (int1 << 0) | (int2 << 8) | (int3 << 16);
	int i;

	for (i = 0; i < ARRAY_SIZE(sm5414_irq_msg); i++) {
		if (!(BIT(i) & data))
			continue;
		if (sm5414_irq_msg[i] == NULL)
			continue;

		dev_notice(chip->dev, "%s\n", sm5414_irq_msg[i]);
	}
}

static irqreturn_t sm5414_irq_handler(int irq, void *data)
{
	struct sm5414 *chip = data;
	u8 int1 = 0, int2 = 0, int3 = 0;
	int rc;

	dev_info(chip->dev, "sm5414_irq_handler\n");

	rc = sm5414_read(chip->client, SM5414_INT1, &int1);
	if (rc)
		dev_err(chip->dev, "failed to read INT1\n");

	rc = sm5414_read(chip->client, SM5414_INT2, &int2);
	if (rc)
		dev_err(chip->dev, "failed to read INT2\n");

	rc = sm5414_read(chip->client, SM5414_INT3, &int3);
	if (rc)
		dev_err(chip->dev, "failed to read INT3\n");

	dev_notice(chip->dev, "irq: 0x%02x 0x%02x 0x%02x\n", int1, int2, int3);

	/* mask out masked interrupts */
	int1 &= ~chip->init_data.intmsk[0];
	int2 &= ~chip->init_data.intmsk[1];
	int3 &= ~chip->init_data.intmsk[2];

	sm5414_irq_dump_msg(chip, int1, int2, int3);

	if (int2 & SM5414_INT2_DONE_MASK)
		sm5414_irq_notify_done(chip);

	if (int2 & SM5414_INT2_CHGRSTF_MASK)
		sm5414_irq_notify_chgrstf(chip);

	if (int2 & SM5414_INT2_FASTTMROFF_MASK)
		sm5414_irq_notify_fasttmroff(chip);

	if (int1 & (SM5414_INT1_VBUSOVP_MASK |
			SM5414_INT1_VBUSUVLO_MASK |
			SM5414_INT1_BATOVP_MASK |
			SM5414_INT1_THEMSHDN_MASK |
			SM5414_INT1_THEMREG_MASK))
		chip->done = false;

	return IRQ_HANDLED;
}

/* charger class interface */
static int sm5414_plug_out(struct charger_device *chg_dev)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	chip->done = false;

	return 0;
}

static int sm5414_plug_in(struct charger_device *chg_dev)
{
	/* when cable plug in detected, this function will be called */
	/* TODO : implement */
	return 0;
}

static int sm5414_enable_charging(struct charger_device *chg_dev, bool en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	if (!en)
		chip->done = false;

	return sm5414_set_chgen(chip, en);
}

static int sm5414_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	int enabled = sm5414_get_chgen(chip);

	if (enabled < 0) {
		*en = false;
		return enabled;
	}

	*en = enabled ? true : false;

	return 0;
}

static int sm5414_get_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u32 cur = sm5414_get_fastchg(chip);

	if (cur < 0) {
		*uA = 0;
		return cur;
	}

	*uA = cur;

	return 0;
}

static int sm5414_set_charging_current(struct charger_device *chg_dev, u32 uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_fastchg(chip, uA);
}

static int sm5414_get_min_charging_current(struct charger_device *chg_dev, u32 *uA)
{
	*uA = SM5414_FASTCHG_UA_MIN;
	return 0;
}

static int sm5414_get_constant_voltage(struct charger_device *chg_dev, u32 *uV)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u32 vol = sm5414_get_batreg(chip);

	if (vol < 0) {
		*uV = 0;
		return vol;
	}

	*uV = vol;

	return 0;
}

static int sm5414_set_constant_voltage(struct charger_device *chg_dev, u32 uV)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_batreg(chip, uV);
}

static int sm5414_get_input_current(struct charger_device *chg_dev, u32 *uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u32 cur = sm5414_get_vbuslimit(chip);

	if (cur < 0) {
		*uA = 0;
		return cur;
	}

	*uA = cur;

	return 0;
}

static int sm5414_set_input_current(struct charger_device *chg_dev, u32 uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_vbuslimit(chip, uA);
}

static int sm5414_get_min_input_current(struct charger_device *chg_dev, u32 *uA)
{
	*uA = SM5414_VBUSLIMIT_UA_MIN;
	return 0;
}

static int sm5414_get_eoc_current(struct charger_device *chg_dev, u32 *uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u32 cur = sm5414_get_topoff(chip);

	if (cur < 0) {
		*uA = 0;
		return cur;
	}

	*uA = cur;

	return 0;
}

static int sm5414_set_eoc_current(struct charger_device *chg_dev, u32 uA)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_topoff(chip, uA);
}

static int sm5414_do_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static int sm5414_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_aiclth(chip, uV);
}

static int sm5414_is_safety_timer_enabled(struct charger_device *chg_dev, bool *en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	int timer;

	timer = sm5414_get_fasttimer(chip);
	if (timer < 0) {
		*en = false;
		return timer;
	}

	*en = (timer != INT_MAX) ? true : false;

	return 0;
}

static int sm5414_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	int timer = chip->init_data.fasttimer;

	/* use longest min if no init_data */
	if (!timer)
		timer = sm5414_fasttimer_min[2];
	if (!en)
		timer = 0;

	return sm5414_set_fasttimer(chip, timer);
}

static int sm5414_enable_termination(struct charger_device *chg_dev, bool en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	return sm5414_set_autostop(chip, en);
}

static int sm5414_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	if (en)
		sm5414_set_suspen(chip, false);
	return sm5414_set_enboost(chip, en);
}

static int sm5414_reset_eoc_state(struct charger_device *chg_dev)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	chip->done = false;

	return 0;
}

static int sm5414_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	const u8 status_err = SM5414_STATUS_VBUSOVP_MASK |
			SM5414_STATUS_VBUSUVLO_MASK |
			SM5414_STATUS_SUSPEND_MASK;
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u8 status = 0;
	int rc;

	*done = false;

	rc = sm5414_read(chip->client, SM5414_STATUS, &status);
	if (rc) {
		dev_err(chip->dev, "failed to read STATUS\n");
		return rc;
	}

	if (status & status_err)
		return 0;

	if (status & SM5414_STATUS_TOPOFF_MASK)
		*done = chip->done;

	return 0;
}

static int sm5414_dump_register(struct charger_device *chg_dev)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	u32 vbuslimit = sm5414_get_vbuslimit(chip);
	u32 fastchg = sm5414_get_fastchg(chip);
	u32 batreg = sm5414_get_batreg(chip);
	u32 topoff = sm5414_get_topoff(chip);
	u32 aiclth = sm5414_get_aiclth(chip);
	bool autostop = sm5414_get_autostop(chip);
	u8 status = 0;
	int rc;

	rc = sm5414_read(chip->client, SM5414_STATUS, &status);
	if (rc)
		dev_err(chip->dev, "failed to read STATUS\n");

	dev_info(chip->dev, "VBUSLIMIT: %umA, FASTCHG: %umA, BATREG: %umV\n",
			vbuslimit / 1000, fastchg / 1000, batreg / 1000);
	dev_info(chip->dev, "TOPOFF: %umA, AICLTH: %umV, AUTOSTOP: %s\n",
			topoff / 1000, aiclth / 1000, autostop ? "Yes" : "No");
	dev_info(chip->dev, "STATUS: 0x%02x%s%s%s%s%s\n", status,
			status & SM5414_STATUS_VBUSOVP_MASK ? " VBUSOVP" : "",
			status & SM5414_STATUS_VBUSUVLO_MASK ? " VBUSUVLO" : "",
			status & SM5414_STATUS_SUSPEND_MASK ? " SUSPEND" : "",
			status & SM5414_STATUS_TOPOFF_MASK ? " TOPOFF" : "",
			chip->done ? " DONE" : "");

	return 0;
}

#ifdef CONFIG_LGE_PM
static int sm5414_input_suspend(struct charger_device *chg_dev, bool en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);

	if (en)
		chip->done = false;

	return sm5414_set_suspen(chip, en);
}

static int sm5414_is_input_suspended(struct charger_device *chg_dev, bool *en)
{
	struct sm5414 *chip = dev_get_drvdata(&chg_dev->dev);
	int suspended = sm5414_get_suspen(chip);

	if (suspended < 0) {
		*en = false;
		return suspended;
	}

	*en = suspended ? true : false;

	return 0;
}
#endif

static struct charger_ops sm5414_ops = {
	/* cable plug in/out */
	.plug_out = sm5414_plug_out,
	.plug_in = sm5414_plug_in,

	/* enable/disable charger */
	.enable = sm5414_enable_charging,
	.is_enabled = sm5414_is_charging_enabled,

	/* get/set charging current*/
	.get_charging_current = sm5414_get_charging_current,
	.set_charging_current = sm5414_set_charging_current,
	.get_min_charging_current = sm5414_get_min_charging_current,

	/* set cv */
	.get_constant_voltage = sm5414_get_constant_voltage,
	.set_constant_voltage = sm5414_set_constant_voltage,

	/* set input_current */
	.get_input_current = sm5414_get_input_current,
	.set_input_current = sm5414_set_input_current,
	.get_min_input_current = sm5414_get_min_input_current,

	/* set termination current */
	.get_eoc_current = sm5414_get_eoc_current,
	.set_eoc_current = sm5414_set_eoc_current,

	.event = sm5414_do_event,

	.set_mivr = sm5414_set_mivr,

	/* enable/disable charging safety timer */
	.is_safety_timer_enabled = sm5414_is_safety_timer_enabled,
	.enable_safety_timer = sm5414_enable_safety_timer,

	/* enable term */
	.enable_termination = sm5414_enable_termination,

	/* OTG */
	.enable_otg = sm5414_enable_otg,

	.reset_eoc_state = sm5414_reset_eoc_state,
	.is_charging_done = sm5414_is_charging_done,
	.dump_registers = sm5414_dump_register,

#ifdef CONFIG_LGE_PM
	.input_suspend = sm5414_input_suspend,
	.is_input_suspended = sm5414_is_input_suspended,
#endif
};

/* debugfs interface */
static int debugfs_get_data(void *data, u64 *val)
{
	struct sm5414 *chip = data;
	int rc;
	u8 temp;

	rc = sm5414_read(chip->client, chip->debug_addr, &temp);
	if (rc)
		return -EAGAIN;

	*val = temp;

	return 0;
}

static int debugfs_set_data(void *data, u64 val)
{
	struct sm5414 *chip = data;
	int rc;
	u8 temp;

	temp = (u8)val;
	rc = sm5414_write(chip->client, chip->debug_addr, temp);
	if (rc)
		return -EAGAIN;

	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(data_debugfs_ops,
	debugfs_get_data, debugfs_set_data, "0x%02llx\n");

const static int regs_to_dump[] = {
	SM5414_INTMSK1,
	SM5414_INTMSK2,
	SM5414_INTMSK3,
	SM5414_STATUS,
	SM5414_CTRL,
	SM5414_VBUSCTRL,
	SM5414_CHGCTRL1,
	SM5414_CHGCTRL2,
	SM5414_CHGCTRL3,
	SM5414_CHGCTRL4,
	SM5414_CHGCTRL5,
};

static int dump_debugfs_show(struct seq_file *m, void *start)
{
	struct sm5414 *chip = m->private;
	u8 data;
	int rc;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs_to_dump); i++) {
		rc = sm5414_read(chip->client, regs_to_dump[i], &data);
		if (rc) {
			seq_printf(m, "0x%02x=error\n", regs_to_dump[i]);
			continue;
		}

		seq_printf(m, "0x%02x=0x%02x\n", regs_to_dump[i], data);
	}

	return 0;
}

static int dump_debugfs_open(struct inode *inode, struct file *file)
{
	struct sm5414 *chip = inode->i_private;

	return single_open(file, dump_debugfs_show, chip);
}

static const struct file_operations dump_debugfs_ops = {
	.owner		= THIS_MODULE,
	.open		= dump_debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int create_debugfs_entries(struct sm5414 *chip)
{
	struct dentry *ent;

	chip->debugfs = debugfs_create_dir(chip->chg_props.alias_name, NULL);
	if (!chip->debugfs) {
		dev_err(chip->dev, "failed to create debugfs\n");
		return -ENODEV;
	}

	ent = debugfs_create_x32("addr", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, &chip->debug_addr);
	if (!ent)
		dev_err(chip->dev, "failed to create addr debugfs\n");

	ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO,
		chip->debugfs, chip, &data_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create data debugfs\n");

	ent = debugfs_create_file("dump", S_IFREG | S_IRUGO,
		chip->debugfs, chip, &dump_debugfs_ops);
	if (!ent)
		dev_err(chip->dev, "failed to create dump debugfs\n");

	return 0;
}

static int sm5414_charger_device_register(struct sm5414 *chip)
{
	int rc;

	chip->chg_props.alias_name = "sm5414";
	chip->chg_dev = charger_device_register("primary_chg", chip->dev, chip,
			&sm5414_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		rc = PTR_ERR(chip->chg_dev);
		return rc;
	}

	chip->chg_dev->is_polling_mode = chip->is_polling_mode;
	if (!gpio_is_valid(chip->irq_gpio) && !chip->is_polling_mode) {
		dev_warn(chip->dev, "force set polling mode for %s\n",
				chip->chg_props.alias_name);
		chip->chg_dev->is_polling_mode = true;
	}

	return 0;
}

static int sm5414_hw_init(struct sm5414 *chip)
{
	int rc = 0;

	if (!gpio_is_valid(chip->irq_gpio))
		goto irq_init_done;

	rc = devm_gpio_request_one(chip->dev, chip->irq_gpio,
			GPIOF_DIR_IN, "sm5414");
	if (rc) {
		dev_err(chip->dev, "failed to request gpio, rc=%d\n", rc);
		return rc;
	}

	rc = devm_request_threaded_irq(chip->dev, gpio_to_irq(chip->irq_gpio),
			NULL, sm5414_irq_handler,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "sm5414", chip);
	if (rc) {
		dev_err(chip->dev, "failed to request irq, rc=%d\n", rc);
		return rc;
	}
irq_init_done:

	rc = sm5414_write(chip->client, SM5414_INTMSK1, chip->init_data.intmsk[0]);
	if (rc < 0)
		dev_err(chip->dev, "failed to set intmsk1, rc=%d\n", rc);

	rc = sm5414_write(chip->client, SM5414_INTMSK2, chip->init_data.intmsk[1]);
	if (rc < 0)
		dev_err(chip->dev, "failed to set intmsk2, rc=%d\n", rc);

	rc = sm5414_write(chip->client, SM5414_INTMSK3, chip->init_data.intmsk[2]);
	if (rc < 0)
		dev_err(chip->dev, "failed to set intmsk3, rc=%d\n", rc);

	rc = sm5414_set_aiclth(chip, chip->init_data.aiclth);
	if (rc < 0)
		dev_err(chip->dev, "failed to set aiclth, rc=%d\n", rc);

	rc = sm5414_set_aiclen(chip, chip->init_data.aiclen);
	if (rc < 0)
		dev_err(chip->dev, "failed to set aiclen, rc=%d\n", rc);

	rc = sm5414_set_prechg(chip, chip->init_data.prechg);
	if (rc < 0)
		dev_err(chip->dev, "failed to set prechg, rc=%d\n", rc);

	rc = sm5414_set_batreg(chip, chip->init_data.batreg);
	if (rc < 0)
		dev_err(chip->dev, "failed to set batreg, rc=%d\n", rc);

	rc = sm5414_set_weakbat(chip, chip->init_data.weakbat);
	if (rc < 0)
		dev_err(chip->dev, "failed to set weakbat, rc=%d\n", rc);

	rc = sm5414_set_topoff(chip, chip->init_data.topoff);
	if (rc < 0)
		dev_err(chip->dev, "failed to set topoff, rc=%d\n", rc);

	rc = sm5414_set_dislimit(chip, chip->init_data.dislimit);
	if (rc < 0)
		dev_err(chip->dev, "failed to set dislimit, rc=%d\n", rc);

	rc = sm5414_set_votg(chip, chip->init_data.votg);
	if (rc < 0)
		dev_err(chip->dev, "failed to set votg, rc=%d\n", rc);

	rc = sm5414_set_fasttimer(chip, chip->init_data.fasttimer);
	if (rc < 0)
		dev_err(chip->dev, "failed to set fasttimer, rc=%d\n", rc);

	rc = sm5414_set_topofftimer(chip, chip->init_data.topofftimer);
	if (rc < 0)
		dev_err(chip->dev, "failed to set topofftimer, rc=%d\n", rc);

	return rc;
}

static int sm5414_parse_dt(struct sm5414 *chip)
{
	struct device_node *np = chip->dev->of_node;
	int rc = 0;

	if (!np)
		return -ENODEV;

	chip->is_polling_mode = of_property_read_bool(np, "polling_mode");
	chip->irq_gpio = of_get_named_gpio_flags(np, "irq-gpio", 0, NULL);

	/* read initial data */
	chip->init_data = sm5414_init_data_default;

	rc = of_property_read_u8_array(np, "intmsk", chip->init_data.intmsk,
			SM5414_INTMSK_NUM);
	if (rc)
		dev_info(chip->dev, "using default for intmsk\n");

	rc = of_property_read_u32(np, "aiclth", &chip->init_data.aiclth);
	if (rc)
		dev_info(chip->dev, "using default for aiclth\n");

	if (of_property_read_bool(np, "aicl-disable"))
		chip->init_data.aiclen = false;

	rc = of_property_read_u32(np, "prechg", &chip->init_data.prechg);
	if (rc)
		dev_info(chip->dev, "using default for prechg\n");

	rc = of_property_read_u32(np, "batreg", &chip->init_data.batreg);
	if (rc)
		dev_info(chip->dev, "using default for batreg\n");

	rc = of_property_read_u32(np, "weakbat", &chip->init_data.weakbat);
	if (rc)
		dev_info(chip->dev, "using default for weakbat\n");

	rc = of_property_read_u32(np, "topoff", &chip->init_data.topoff);
	if (rc)
		dev_info(chip->dev, "using default for topoff\n");

	rc = of_property_read_u32(np, "dislimit", &chip->init_data.dislimit);
	if (rc)
		dev_info(chip->dev, "using default for dislimit\n");

	rc = of_property_read_u32(np, "votg", &chip->init_data.votg);
	if (rc)
		dev_info(chip->dev, "using default for votg\n");

	rc = of_property_read_u32(np, "fasttimer", &chip->init_data.fasttimer);
	if (rc)
		dev_info(chip->dev, "using default for fasttimer\n");

	rc = of_property_read_u32(np, "topofftimer", &chip->init_data.topofftimer);
	if (rc)
		dev_info(chip->dev, "using default for topofftimer\n");

	return rc;
}

static int sm5414_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct sm5414 *chip;
	int rc;

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "failed to allocate memory\n");
		return -ENOMEM;
	}

	chip->client = client;
	chip->dev = &client->dev;
	i2c_set_clientdata(client, chip);

	mutex_init(&chip->lock);
	INIT_DELAYED_WORK(&chip->done_dwork, sm5414_done_work);

	rc = sm5414_parse_dt(chip);
	if (rc)
		return rc;

	rc = sm5414_hw_init(chip);
	if (rc)
		return rc;

	rc = sm5414_charger_device_register(chip);
	if (rc)
		return rc;

	create_debugfs_entries(chip);

	return rc;
}

static int sm5414_remove(struct i2c_client *client)
{
	struct sm5414 *chip = i2c_get_clientdata(client);

	charger_device_unregister(chip->chg_dev);

	return 0;
}

static void sm5414_shutdown(struct i2c_client *client)
{
	struct sm5414 *chip = i2c_get_clientdata(client);

	if (sm5414_get_enboost(chip) > 0)
		sm5414_set_enboost(chip, false);

	if (sm5414_get_suspen(chip))
		sm5414_set_suspen(chip, false);

	return;
}

static const struct of_device_id sm5414_of_match[] = {
	{
		.compatible = "sm,sm5414",
	},
	{},
};

static const struct i2c_device_id sm5414_i2c_id[] = {
	{
		.name = "sm5414",
		.driver_data = 0,
	},
	{},
};

static struct i2c_driver sm5414_driver = {
	.probe = sm5414_probe,
	.remove = sm5414_remove,
	.shutdown = sm5414_shutdown,
	.driver = {
		.name = "sm5414",
		.of_match_table = sm5414_of_match,
	},
	.id_table = sm5414_i2c_id,
};

static int __init sm5414_init(void)
{
	return i2c_add_driver(&sm5414_driver);
}

static void __exit sm5414_exit(void)
{
	i2c_del_driver(&sm5414_driver);
}

module_init(sm5414_init);
module_exit(sm5414_exit);
