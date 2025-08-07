/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <generated/autoconf.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>
#include <linux/device.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/sched.h>
#include <linux/writeback.h>
#include <linux/seq_file.h>
#include <linux/power_supply.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/reboot.h>
#ifdef CONFIG_LGE_PM
#include <linux/of.h>
#endif
#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
#include <linux/power/charger_controller.h>
#endif
#ifdef CONFIG_LGE_PM_INVALID_CHARGER
#include <linux/switch.h>
#endif

#include <mt-plat/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>
#include <mach/upmu_hw.h>
#include <mt-plat/mtk_boot.h>
#include <mt-plat/charger_type.h>
#include <pmic.h>

#include "mtk_charger_intf.h"

void __attribute__((weak)) fg_charger_in_handler(void)
{
	pr_notice("%s not defined\n", __func__);
}

static enum charger_type g_chr_type;
static bool ignore_usb;

#ifdef CONFIG_LGE_PM_INVALID_CHARGER
static struct switch_dev invalid_charger;
#endif

#ifdef CONFIG_MTK_FPGA
/*****************************************************************************
 * FPGA
 ******************************************************************************/
int hw_charging_get_charger_type(void)
{
	return STANDARD_HOST;
}

#else
/*****************************************************************************
 * EVB / Phone
 ******************************************************************************/

static const char * const mtk_chg_type_name[] = {
	"Charger Unknown",
	"Standard USB Host",
	"Charging USB Host",
	"Non-standard Charger",
	"Standard Charger",
	"Apple 2.1A Charger",
	"Apple 1.0A Charger",
	"Apple 0.5A Charger",
	"Wireless Charger",
};

static void dump_charger_name(enum charger_type type)
{
	switch (type) {
	case CHARGER_UNKNOWN:
	case STANDARD_HOST:
	case CHARGING_HOST:
	case NONSTANDARD_CHARGER:
	case STANDARD_CHARGER:
	case APPLE_2_1A_CHARGER:
	case APPLE_1_0A_CHARGER:
	case APPLE_0_5A_CHARGER:
		pr_err("%s: charger type: %d, %s\n", __func__, type,
			mtk_chg_type_name[type]);
		break;
	default:
		pr_err("%s: charger type: %d, Not Defined!!!\n", __func__,
			type);
		break;
	}
}

/************************************************/
/* Power Supply
*************************************************/

struct mt_charger {
	struct device *dev;
	struct power_supply_desc chg_desc;
	struct power_supply_config chg_cfg;
	struct power_supply *chg_psy;
	struct power_supply_desc ac_desc;
	struct power_supply_config ac_cfg;
	struct power_supply *ac_psy;
	struct power_supply_desc usb_desc;
	struct power_supply_config usb_cfg;
	struct power_supply *usb_psy;
	bool chg_online; /* Has charger in or not */
	enum charger_type chg_type;
#ifdef CONFIG_LGE_PM
	struct delayed_work floated_retry_work;
	unsigned int floated_retry_ms;
	int floated;

	bool is_kpoc;

	int fastchg;
	int typec;

	struct delayed_work power_changed_work;
	int current_max;
	int voltage_max;
#endif
#ifdef CONFIG_LGE_PM_INVALID_CHARGER
	struct switch_dev *invalid_chg_sdev;
#endif

};

#ifdef CONFIG_LGE_PM
static void mt_charger_enable_chr_type_det(struct mt_charger *mtk_chg)
{
#ifdef CONFIG_MTK_EXTERNAL_CHARGER_TYPE_DETECT
#if CONFIG_MTK_GAUGE_VERSION == 30
	static struct charger_device *primary_charger;

	if (!primary_charger)
		primary_charger = get_charger_by_name("primary_chg");

	charger_dev_enable_chg_type_det(primary_charger, true);
#else
	mtk_chr_enable_chr_type_det(true);
#endif
#else
	mtk_pmic_enable_chr_type_det(true);
#endif
}

static bool mt_charger_floated_ignore(struct mt_charger *mtk_chg)
{
	if (mtk_chg->chg_type != NONSTANDARD_CHARGER)
		return true;
	if (mtk_chg->fastchg)
		return true;
	if (mtk_chg->typec)
		return true;

	return false;
}

static void mt_charger_floated_retry(struct work_struct *work)
{
	struct mt_charger *mtk_chg = container_of(to_delayed_work(work),
			struct mt_charger, floated_retry_work);

	if (mt_charger_floated_ignore(mtk_chg))
		return;

	mt_charger_enable_chr_type_det(mtk_chg);

	if (mt_charger_floated_ignore(mtk_chg))
		return;

	mtk_chg->floated = 1;

	pr_info("floated charger detected.\n");

#ifdef CONFIG_LGE_PM_INVALID_CHARGER
	if (mtk_chg->invalid_chg_sdev)
		switch_set_state(mtk_chg->invalid_chg_sdev, 1);
#endif

	power_supply_changed(mtk_chg->usb_psy);
}

static void mt_charger_floated_trigger(struct mt_charger *mtk_chg)
{
	if (!mtk_chg->floated_retry_ms)
		return;

	if (mtk_chg->chg_type != NONSTANDARD_CHARGER)
		cancel_delayed_work(&mtk_chg->floated_retry_work);

	if (mt_charger_floated_ignore(mtk_chg))
		return;

	/* retry charger type detection */
	pr_info("floated charger. retry after %dms\n",
			mtk_chg->floated_retry_ms);
	schedule_delayed_work(&mtk_chg->floated_retry_work,
			msecs_to_jiffies(mtk_chg->floated_retry_ms));
}

static void mt_charger_floated_init(struct mt_charger *mtk_chg)
{
	INIT_DELAYED_WORK(&mtk_chg->floated_retry_work,
			mt_charger_floated_retry);
	mtk_chg->floated = 0;
}

static void mt_charger_floated_parse_dt(struct mt_charger *mtk_chg)
{
	struct device_node *node = mtk_chg->dev->of_node;
	int ret = 0;

	ret = of_property_read_u32(node, "floated-retry-ms",
			&mtk_chg->floated_retry_ms);
	if (ret)
		mtk_chg->floated_retry_ms = 0;
}

static bool mt_charger_is_kpoc(struct mt_charger *mtk_chg)
{
	int boot_mode = get_boot_mode();

	if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
		return true;
	}

	return false;
}

static void mt_charger_fastchg(struct mt_charger *mtk_chg, int fastchg)
{
	if (mtk_chg->fastchg == fastchg)
		return;

	pr_info("%s: fast charger: %d\n", __func__, fastchg);

	mtk_chg->fastchg = fastchg;

	/* clear incompatible_chg if fastchg set */
	if (mtk_chg->fastchg)
		mtk_chg->floated = 0;

	power_supply_changed(mtk_chg->usb_psy);
}

static void mt_charger_changed(struct mt_charger *mtk_chg,
			       enum charger_type chg_type)
{
	switch (chg_type) {
	case STANDARD_HOST:
	case CHARGING_HOST:
		power_supply_changed(mtk_chg->usb_psy);
		break;
	case NONSTANDARD_CHARGER:
	case STANDARD_CHARGER:
	case APPLE_2_1A_CHARGER:
	case APPLE_1_0A_CHARGER:
	case APPLE_0_5A_CHARGER:
		power_supply_changed(mtk_chg->ac_psy);
		break;
	default:
		break;
	}
}

static void mt_charger_power_changed(struct work_struct *work)
{
	struct mt_charger *mtk_chg = container_of(to_delayed_work(work),
			struct mt_charger, power_changed_work);
	int voltage_max = mtk_chg->voltage_max / 1000;
	int current_max = mtk_chg->current_max  / 1000;
	int power_max = voltage_max * current_max / 1000;

	pr_info("%s: type:%d, power:%dmW, voltage:%dmV, current:%dmA\n",
			__func__, mtk_chg->chg_type,
			power_max, voltage_max, current_max);

	mt_charger_changed(mtk_chg, mtk_chg->chg_type);
}

static void mt_charger_current_max(struct mt_charger *mtk_chg, int current_max)
{
	if (mtk_chg->chg_type == CHARGER_UNKNOWN)
		return;

	if (mtk_chg->current_max == current_max)
		return;

	mtk_chg->current_max = current_max;

	schedule_delayed_work(&mtk_chg->power_changed_work,
			msecs_to_jiffies(100));
}

static void mt_charger_voltage_max(struct mt_charger *mtk_chg, int voltage_max)
{
	if (mtk_chg->chg_type == CHARGER_UNKNOWN)
		return;

	if (mtk_chg->voltage_max == voltage_max)
		return;

	mtk_chg->voltage_max = voltage_max;

	schedule_delayed_work(&mtk_chg->power_changed_work,
			msecs_to_jiffies(100));
}
#endif

static int mt_charger_online(struct mt_charger *mtk_chg)
{
#ifdef CONFIG_LGE_PM_CHARGERLOGO
	/* do not power-off here */
	return 0;
#else /* MediaTek */
	int ret = 0;

	int boot_mode = 0;

	if (!mtk_chg->chg_online) {
		boot_mode = get_boot_mode();
		if (boot_mode == KERNEL_POWER_OFF_CHARGING_BOOT ||
		    boot_mode == LOW_POWER_OFF_CHARGING_BOOT) {
			pr_err("%s: Unplug Charger/USB\n", __func__);
			kernel_power_off();
		}
	}

	return ret;
#endif
}

/************************************************/
/* Power Supply Functions
*************************************************/

static int mt_charger_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		break;
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_FASTCHG:
		val->intval = mtk_chg->fastchg;
		break;
	case POWER_SUPPLY_PROP_FASTCHG_SUPPORT:
		val->intval = is_fastchg_supported() ? 1 : 0;
		break;
	case POWER_SUPPLY_PROP_INCOMPATIBLE_CHG:
		val->intval = mtk_chg->floated;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = 0;
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = pmic_get_vbus() * 1000;
		break;
#endif
	default:
		return -EINVAL;
	}

	return 0;
}


static int mt_charger_set_property(struct power_supply *psy,
	enum power_supply_property psp, const union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

#ifdef CONFIG_LGE_PM
	pr_debug("%s\n", __func__);
#else /* MediaTek */
	pr_info("%s\n", __func__);
#endif

	if (!mtk_chg) {
		pr_err("%s: no mtk chg data\n", __func__);
		return -EINVAL;
	}

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		mtk_chg->chg_online = val->intval;
		mt_charger_online(mtk_chg);
		return 0;
	case POWER_SUPPLY_PROP_CHARGE_TYPE:
#ifdef CONFIG_LGE_PM
		if (mtk_chg->chg_type == val->intval)
			return 0;
#endif
		mtk_chg->chg_type = val->intval;
		g_chr_type = val->intval;
		break;
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_FASTCHG:
		mt_charger_fastchg(mtk_chg, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		mt_charger_current_max(mtk_chg, val->intval);
		return 0;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		mt_charger_voltage_max(mtk_chg, val->intval);
		return 0;
#endif
	default:
		return -EINVAL;
	}

	dump_charger_name(mtk_chg->chg_type);

#ifdef CONFIG_LGE_PM
	mt_charger_floated_trigger(mtk_chg);

	if (mtk_chg->chg_type == CHARGER_UNKNOWN) {
#ifdef CONFIG_LGE_PM_INVALID_CHARGER
		if (mtk_chg->invalid_chg_sdev)
			switch_set_state(mtk_chg->invalid_chg_sdev, 0);
#endif
		mtk_chg->floated = 0;
		mtk_chg->fastchg = 0;
		mtk_chg->typec = 0;
		mtk_chg->current_max = 500000;
		mtk_chg->voltage_max = 5000000;
	}

	/* usb */
	if (!ignore_usb && !mtk_chg->is_kpoc) {
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			mt_usb_connect();
		else
			mt_usb_disconnect();
	}
#else /* MediaTek */
	if (!ignore_usb) {
		/* usb */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST) ||
			(mtk_chg->chg_type == NONSTANDARD_CHARGER))
			mt_usb_connect();
		else
			mt_usb_disconnect();
	}
#endif

	mtk_charger_int_handler();
	fg_charger_in_handler();

	power_supply_changed(mtk_chg->ac_psy);
	power_supply_changed(mtk_chg->usb_psy);

	return 0;
}

static int mt_ac_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 0;
		/* Force to 1 in all charger type */
		if (mtk_chg->chg_type != CHARGER_UNKNOWN)
			val->intval = 1;
		/* Reset to 0 if charger type is USB */
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 0;
		break;
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = mtk_chg->current_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = mtk_chg->voltage_max;
		break;
#endif
	default:
		return -EINVAL;
	}

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
	chgctrl_charger_property_override(psp, val);
#endif

	return 0;
}

static int mt_usb_get_property(struct power_supply *psy,
	enum power_supply_property psp, union power_supply_propval *val)
{
	struct mt_charger *mtk_chg = power_supply_get_drvdata(psy);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if ((mtk_chg->chg_type == STANDARD_HOST) ||
			(mtk_chg->chg_type == CHARGING_HOST))
			val->intval = 1;
		else
			val->intval = 0;
		break;
#ifdef CONFIG_LGE_PM
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = mtk_chg->current_max;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = mtk_chg->voltage_max;
		break;
#else /* MediaTek */
	case POWER_SUPPLY_PROP_CURRENT_MAX:
		val->intval = 500000;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
		val->intval = 5000000;
		break;
#endif
	default:
		return -EINVAL;
	}

#ifdef CONFIG_LGE_PM_CHARGER_CONTROLLER
	chgctrl_charger_property_override(psp, val);
#endif

	return 0;
}

static enum power_supply_property mt_charger_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
#ifdef CONFIG_LGE_PM
	POWER_SUPPLY_PROP_FASTCHG,
	POWER_SUPPLY_PROP_FASTCHG_SUPPORT,
	POWER_SUPPLY_PROP_INCOMPATIBLE_CHG,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
#endif
};

static enum power_supply_property mt_ac_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
#ifdef CONFIG_LGE_PM
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
#endif
};

static enum power_supply_property mt_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_CURRENT_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
};

static int mt_charger_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mt_charger *mt_chg = NULL;

	mt_chg = devm_kzalloc(&pdev->dev, sizeof(struct mt_charger), GFP_KERNEL);
	if (!mt_chg)
		return -ENOMEM;

	mt_chg->dev = &pdev->dev;
	mt_chg->chg_online = false;
	mt_chg->chg_type = CHARGER_UNKNOWN;

#ifdef CONFIG_LGE_PM
	mt_charger_floated_parse_dt(mt_chg);
	mt_charger_floated_init(mt_chg);

	mt_chg->is_kpoc = mt_charger_is_kpoc(mt_chg);

	mt_chg->fastchg = 0;
	mt_chg->typec = 0;

	INIT_DELAYED_WORK(&mt_chg->power_changed_work,
			mt_charger_power_changed);
	mt_chg->current_max = 500000;
	mt_chg->voltage_max = 5000000;
#endif

	mt_chg->chg_desc.name = "charger";
	mt_chg->chg_desc.type = POWER_SUPPLY_TYPE_UNKNOWN;
	mt_chg->chg_desc.properties = mt_charger_properties;
	mt_chg->chg_desc.num_properties = ARRAY_SIZE(mt_charger_properties);
	mt_chg->chg_desc.set_property = mt_charger_set_property;
	mt_chg->chg_desc.get_property = mt_charger_get_property;
	mt_chg->chg_cfg.drv_data = mt_chg;

	mt_chg->ac_desc.name = "ac";
	mt_chg->ac_desc.type = POWER_SUPPLY_TYPE_MAINS;
	mt_chg->ac_desc.properties = mt_ac_properties;
	mt_chg->ac_desc.num_properties = ARRAY_SIZE(mt_ac_properties);
	mt_chg->ac_desc.get_property = mt_ac_get_property;
	mt_chg->ac_cfg.drv_data = mt_chg;

	mt_chg->usb_desc.name = "usb";
	mt_chg->usb_desc.type = POWER_SUPPLY_TYPE_USB;
	mt_chg->usb_desc.properties = mt_usb_properties;
	mt_chg->usb_desc.num_properties = ARRAY_SIZE(mt_usb_properties);
	mt_chg->usb_desc.get_property = mt_usb_get_property;
	mt_chg->usb_cfg.drv_data = mt_chg;

	mt_chg->chg_psy = power_supply_register(&pdev->dev,
		&mt_chg->chg_desc, &mt_chg->chg_cfg);
	if (IS_ERR(mt_chg->chg_psy)) {
		dev_err(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->chg_psy));
		ret = PTR_ERR(mt_chg->chg_psy);
		return ret;
	}

	mt_chg->ac_psy = power_supply_register(&pdev->dev, &mt_chg->ac_desc,
		&mt_chg->ac_cfg);
	if (IS_ERR(mt_chg->ac_psy)) {
		dev_err(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->ac_psy));
		ret = PTR_ERR(mt_chg->ac_psy);
		goto err_ac_psy;
	}

	mt_chg->usb_psy = power_supply_register(&pdev->dev, &mt_chg->usb_desc,
		&mt_chg->usb_cfg);
	if (IS_ERR(mt_chg->usb_psy)) {
		dev_err(&pdev->dev, "Failed to register power supply: %ld\n",
			PTR_ERR(mt_chg->usb_psy));
		ret = PTR_ERR(mt_chg->usb_psy);
		goto err_usb_psy;
	}

#ifdef CONFIG_LGE_PM_INVALID_CHARGER
	mt_chg->invalid_chg_sdev = switch_dev_get_by_name("invalid_charger");
	if (!mt_chg->invalid_chg_sdev) {
		invalid_charger.name = "invalid_charger";
		ret = switch_dev_register(&invalid_charger);
		if (ret)
			dev_err(&pdev->dev, "Failed to register switch dev.\n");
		else
			mt_chg->invalid_chg_sdev = &invalid_charger;
	}
#endif

	platform_set_drvdata(pdev, mt_chg);
	device_init_wakeup(&pdev->dev, 1);

	pr_info("%s\n", __func__);
	return 0;

err_usb_psy:
	power_supply_unregister(mt_chg->ac_psy);
err_ac_psy:
	power_supply_unregister(mt_chg->chg_psy);
	return ret;
}

static int mt_charger_remove(struct platform_device *pdev)
{
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	power_supply_unregister(mt_charger->chg_psy);
	power_supply_unregister(mt_charger->ac_psy);
	power_supply_unregister(mt_charger->usb_psy);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mt_charger_suspend(struct device *dev)
{
	/* struct mt_charger *mt_charger = dev_get_drvdata(dev); */
	return 0;
}

static int mt_charger_resume(struct device *dev)
{
#ifdef CONFIG_LGE_PM
	/* do not need to notify */
#else
	struct platform_device *pdev = to_platform_device(dev);
	struct mt_charger *mt_charger = platform_get_drvdata(pdev);

	power_supply_changed(mt_charger->chg_psy);
	power_supply_changed(mt_charger->ac_psy);
	power_supply_changed(mt_charger->usb_psy);
#endif

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(mt_charger_pm_ops, mt_charger_suspend,
	mt_charger_resume);

static const struct of_device_id mt_charger_match[] = {
	{ .compatible = "mediatek,mt-charger", },
	{ },
};
static struct platform_driver mt_charger_driver = {
	.probe = mt_charger_probe,
	.remove = mt_charger_remove,
	.driver = {
		.name = "mt-charger-det",
		.owner = THIS_MODULE,
		.pm = &mt_charger_pm_ops,
		.of_match_table = mt_charger_match,
	},
};

/* Legacy api to prevent build error */
bool upmu_is_chr_det(void)
{
	if (upmu_get_rgs_chrdet())
		return true;

	return false;
}

/* Legacy api to prevent build error */
bool pmic_chrdet_status(void)
{
	if (upmu_is_chr_det())
		return true;

	pr_err("%s: No charger\n", __func__);
	return false;
}

enum charger_type mt_get_charger_type(void)
{
	return g_chr_type;
}

void charger_ignore_usb(bool ignore)
{
	ignore_usb = ignore;
}

static s32 __init mt_charger_det_init(void)
{
	return platform_driver_register(&mt_charger_driver);
}

static void __exit mt_charger_det_exit(void)
{
	platform_driver_unregister(&mt_charger_driver);
}

subsys_initcall(mt_charger_det_init);
module_exit(mt_charger_det_exit);

MODULE_DESCRIPTION("mt-charger-detection");
MODULE_AUTHOR("MediaTek");
MODULE_LICENSE("GPL v2");

#endif /* CONFIG_MTK_FPGA */
