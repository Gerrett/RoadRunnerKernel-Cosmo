/*
 * Module to control max opp duty cycle
 *
 * Copyright (c) 2011 Texas Instrument
 * Contact: Eduardo Valentin <eduardo.valentin@ti.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo Valentin <eduardo.valentin@ti.com>");
MODULE_DESCRIPTION("Module to control max opp duty cycle");

#define NITRO_P(p, i)	((i) * (p) / 100)
enum omap4_duty_state {
	OMAP4_DUTY_NORMAL = 0,
	OMAP4_DUTY_HEATING,
	OMAP4_DUTY_COOLING_0,
	OMAP4_DUTY_COOLING_1,
};

/* state struct */
static unsigned int nitro_interval = 20000;
module_param(nitro_interval, int, 0);

static unsigned int nitro_percentage = 25;
module_param(nitro_percentage, int, 0);

static unsigned int nitro_rate = 1200000;
module_param(nitro_rate, int, 0);

static unsigned int cooling_rate = 1008000;
module_param(cooling_rate, int, 0);

static int heating_budget;
static int t_next_heating_end;

static struct workqueue_struct *duty_wq;
static struct delayed_work work_exit_cool;
static struct delayed_work work_exit_heat;
static struct delayed_work work_enter_heat;
static struct work_struct work_enter_cool0;
static struct work_struct work_enter_cool1;
static enum omap4_duty_state state;

/* protect our data */
static DEFINE_MUTEX(mutex_duty);

static void omap4_duty_enter_normal(void)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	pr_debug("%s enter at (%u)\n", __func__, policy->cur);
	state = OMAP4_DUTY_NORMAL;
	heating_budget = NITRO_P(nitro_percentage, nitro_interval);

	policy->max = nitro_rate;
	policy->user_policy.max = nitro_rate;
	cpufreq_update_policy(policy->cpu);
}

static void omap4_duty_exit_cool_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_normal();
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_cooling(unsigned int next_max,
					enum omap4_duty_state next_state)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(0);

	state = next_state;
	pr_debug("%s enter at (%u)\n", __func__, policy->cur);

	policy->max = next_max;
	policy->user_policy.max = next_max;
	cpufreq_update_policy(policy->cpu);

	queue_delayed_work(duty_wq, &work_exit_cool, msecs_to_jiffies(
		NITRO_P(100 - nitro_percentage, nitro_interval)));
}

static void omap4_duty_exit_heating_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(cooling_rate, OMAP4_DUTY_COOLING_0);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_c0_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(cooling_rate, OMAP4_DUTY_COOLING_0);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_c1_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_cooling(nitro_rate, OMAP4_DUTY_COOLING_1);
	mutex_unlock(&mutex_duty);
}

static void omap4_duty_enter_heating(void)
{
	pr_debug("%s enter at ()\n", __func__);
	state = OMAP4_DUTY_HEATING;

	t_next_heating_end = msecs_to_jiffies(heating_budget);
	queue_delayed_work(duty_wq, &work_exit_heat, t_next_heating_end);
	t_next_heating_end += jiffies;
}

static void omap4_duty_enter_heat_wq(struct work_struct *work)
{
	mutex_lock(&mutex_duty);
	pr_debug("%s enter at ()\n", __func__);

	omap4_duty_enter_heating();
	mutex_unlock(&mutex_duty);
}

static int omap4_duty_frequency_change(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct cpufreq_freqs *freqs = data;
	pr_debug("%s enter\n", __func__);

	/* We are interested only in POSTCHANGE transactions */
	if (val != CPUFREQ_POSTCHANGE)
		goto done;

	pr_debug("%s: freqs %u->%u state: %d\n", __func__, freqs->old,
							freqs->new, state);
	switch (state) {
	case OMAP4_DUTY_NORMAL:
		if (freqs->new == nitro_rate)
			queue_delayed_work(duty_wq, &work_enter_heat,
						msecs_to_jiffies(1));
		break;
	case OMAP4_DUTY_HEATING:
		if (freqs->new < nitro_rate) {
			heating_budget -= (t_next_heating_end - jiffies);
			if (heating_budget <= 0)
				queue_work(duty_wq, &work_enter_cool0);
			else
				queue_work(duty_wq, &work_enter_cool1);
		}
		break;
	case OMAP4_DUTY_COOLING_0:
		break;
	case OMAP4_DUTY_COOLING_1:
		if (freqs->new == nitro_rate)
			queue_work(duty_wq, &work_enter_heat.work);
		break;
	}

done:
	return 0;
}

static ssize_t show_nitro_interval(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	mutex_lock_interruptible(&mutex_duty);
	ret = sprintf(buf, "%u\n", nitro_interval);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_interval(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;

	strict_strtoul(buf, 0, &val);
	if (val == 0)
		return -EINVAL;

	mutex_lock_interruptible(&mutex_duty);
	nitro_interval = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_interval, S_IRUGO | S_IWUSR, show_nitro_interval,
							store_nitro_interval);

static ssize_t show_nitro_percentage(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	mutex_lock_interruptible(&mutex_duty);
	ret = sprintf(buf, "%u\n", nitro_percentage);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_percentage(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;

	strict_strtoul(buf, 0, &val);
	if (val == 0)
		return -EINVAL;

	mutex_lock_interruptible(&mutex_duty);
	nitro_percentage = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_percentage, S_IRUGO | S_IWUSR, show_nitro_percentage,
							store_nitro_percentage);

static ssize_t show_nitro_rate(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	mutex_lock_interruptible(&mutex_duty);
	ret = sprintf(buf, "%u\n", nitro_rate);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_nitro_rate(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;

	strict_strtoul(buf, 0, &val);
	if (val == 0)
		return -EINVAL;

	mutex_lock_interruptible(&mutex_duty);
	nitro_rate = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(nitro_rate, S_IRUGO | S_IWUSR, show_nitro_rate,
							store_nitro_rate);

static ssize_t show_cooling_rate(struct device *dev,
			struct device_attribute *devattr, char *buf)
{
	int ret;

	mutex_lock_interruptible(&mutex_duty);
	ret = sprintf(buf, "%u\n", cooling_rate);
	mutex_unlock(&mutex_duty);

	return ret;
}

static ssize_t store_cooling_rate(struct device *dev,
					struct device_attribute *attr,
					const char *buf,
					size_t count)
{
	unsigned long val;

	strict_strtoul(buf, 0, &val);
	if (val == 0)
		return -EINVAL;

	mutex_lock_interruptible(&mutex_duty);
	cooling_rate = val;
	mutex_unlock(&mutex_duty);

	return count;
}
static DEVICE_ATTR(cooling_rate, S_IRUGO | S_IWUSR, show_cooling_rate,
							store_cooling_rate);

static struct attribute *attrs[] = {
	&dev_attr_nitro_interval.attr,
	&dev_attr_nitro_percentage.attr,
	&dev_attr_nitro_rate.attr,
	&dev_attr_cooling_rate.attr,
	NULL,
};

static const struct attribute_group attr_group = {
	.attrs = attrs,
};

static int __init omap4_duty_probe(struct platform_device *pdev)
{
	int rval;

	rval = sysfs_create_group(&pdev->dev.kobj, &attr_group);
	if (rval < 0) {
		dev_dbg(&pdev->dev, "Could not register sysfs interface.\n");
		return rval;
	}

	return 0;
}

static int  __exit omap4_duty_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &attr_group);

	return 0;
}

static struct platform_device *omap4_duty_device;

static struct platform_driver omap4_duty_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "omap4_duty_cycle",
	},
	.remove		= __exit_p(omap4_duty_remove),
};

static struct notifier_block omap4_duty_nb = {
	.notifier_call	= omap4_duty_frequency_change,
};

/* Module Interface */
static int __init omap4_duty_module_init(void)
{
	int err = 0;

	if ((!nitro_interval) || (nitro_percentage > 100) ||
		(nitro_percentage <= 0) || (nitro_rate <= 0) ||
		(cooling_rate <= 0))
		return -EINVAL;

	/* Data initialization */
	duty_wq = create_workqueue("omap4_duty_cycle");
	INIT_DELAYED_WORK(&work_exit_cool, omap4_duty_exit_cool_wq);
	INIT_DELAYED_WORK(&work_exit_heat, omap4_duty_exit_heating_wq);
	INIT_DELAYED_WORK(&work_enter_heat, omap4_duty_enter_heat_wq);
	INIT_WORK(&work_enter_cool0, omap4_duty_enter_c0_wq);
	INIT_WORK(&work_enter_cool1, omap4_duty_enter_c1_wq);
	heating_budget = NITRO_P(nitro_percentage, nitro_interval);

	/* Register the cpufreq notification */
	if (cpufreq_register_notifier(&omap4_duty_nb,
						CPUFREQ_TRANSITION_NOTIFIER)) {
		pr_err("%s: failed to setup cpufreq_notifier\n", __func__);
		err = -EINVAL;
		goto exit;
	}

	omap4_duty_device = platform_device_register_simple("omap4_duty_cycle",
								-1, NULL, 0);
	if (IS_ERR(omap4_duty_device)) {
		err = PTR_ERR(omap4_duty_device);
		pr_err("Unable to register omap4 duty cycle device\n");
		goto exit_cpufreq_nb;
	}

	err = platform_driver_probe(&omap4_duty_driver, omap4_duty_probe);
	if (err)
		goto exit_pdevice;

	return 0;

exit_pdevice:
	platform_device_unregister(omap4_duty_device);
exit_cpufreq_nb:
	cpufreq_unregister_notifier(&omap4_duty_nb,
						CPUFREQ_TRANSITION_NOTIFIER);
exit:
	return err;
}

static void __exit omap4_duty_module_exit(void)
{
	platform_device_unregister(omap4_duty_device);
	platform_driver_unregister(&omap4_duty_driver);

	cpufreq_unregister_notifier(&omap4_duty_nb,
						CPUFREQ_TRANSITION_NOTIFIER);


	cancel_delayed_work_sync(&work_exit_cool);
	cancel_delayed_work_sync(&work_exit_heat);
	cancel_delayed_work_sync(&work_enter_heat);
	cancel_work_sync(&work_enter_cool0);
	cancel_work_sync(&work_enter_cool1);

	destroy_workqueue(duty_wq);

	pr_debug("%s Done\n", __func__);
}

module_init(omap4_duty_module_init);
module_exit(omap4_duty_module_exit);

