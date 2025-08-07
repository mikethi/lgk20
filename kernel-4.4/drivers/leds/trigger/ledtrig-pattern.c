/*
 * LED Kernel Timer Trigger
 *
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/ctype.h>
#include <linux/leds.h>

static ssize_t led_pattern_id_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", led_cdev->pattern_id);
}

static ssize_t led_pattern_id_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	unsigned long pattern_id;
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &pattern_id);
	if (ret)
		return ret;

	if (led_cdev->pattern_set)
		led_cdev->pattern_set(led_cdev, &pattern_id);

	led_cdev->pattern_id = pattern_id;

	return size;
}

static DEVICE_ATTR(pattern_id, 0644, led_pattern_id_show, led_pattern_id_store);

static void pattern_trig_activate(struct led_classdev *led_cdev)
{
	int rc;

	led_cdev->trigger_data = NULL;

	rc = device_create_file(led_cdev->dev, &dev_attr_pattern_id);
	if (rc)
		return;

	if (system_state == SYSTEM_BOOTING)
		led_cdev->pattern_id = LED_PATTERN_POWER_ON;

	if (led_cdev->pattern_set)
		led_cdev->pattern_set(led_cdev, &led_cdev->pattern_id);
	led_cdev->activated = true;

	return;
}

static void pattern_trig_deactivate(struct led_classdev *led_cdev)
{
	if (led_cdev->activated) {
		device_remove_file(led_cdev->dev, &dev_attr_pattern_id);
		led_cdev->activated = false;
	}

	/* Stop blinking */
	led_set_brightness(led_cdev, LED_OFF);
}

static struct led_trigger pattern_led_trigger = {
	.name     = "pattern",
	.activate = pattern_trig_activate,
	.deactivate = pattern_trig_deactivate,
};

static int __init pattern_trig_init(void)
{
	return led_trigger_register(&pattern_led_trigger);
}

static void __exit pattern_trig_exit(void)
{
	led_trigger_unregister(&pattern_led_trigger);
}

module_init(pattern_trig_init);
module_exit(pattern_trig_exit);

MODULE_AUTHOR("LG Electronics <anonymous@lge.com>");
MODULE_DESCRIPTION("LGE Pattern LED trigger");
MODULE_LICENSE("GPL");
