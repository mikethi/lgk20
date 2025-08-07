/*
 * Copyright (C) 2016 MediaTek Inc.
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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "mtk_ppm_internal.h"


static void ppm_fbcn_limit_update_limit_cb(void);
static void ppm_fbcn_limit_status_change_cb(bool enable);

/* other members will init by ppm_main */
static struct ppm_policy_data fbcn_limit_policy = {
	.name			= __stringify(PPM_POLICY_FBCN_LIMIT),
	.lock			= __MUTEX_INITIALIZER(fbcn_limit_policy.lock),
	.policy			= PPM_POLICY_FBCN_LIMIT,
	.priority		= PPM_POLICY_PRIO_HIGHEST,
	.update_limit_cb	= ppm_fbcn_limit_update_limit_cb,
	.status_change_cb	= ppm_fbcn_limit_status_change_cb,
};

struct ppm_userlimit_data fbcn_limit_data = {
	.is_freq_limited_by_user = false,
	.is_core_limited_by_user = false,
};

struct ppm_max_status {
	int freq_max;
	int freq_min;
};

/* MUST in lock */
static bool ppm_fbcn_limit_is_policy_active(void)
{
	if (!fbcn_limit_data.is_freq_limited_by_user
		&& !fbcn_limit_data.is_core_limited_by_user)
		return false;
	else
		return true;
}

static void ppm_fbcn_limit_update_limit_cb(void)
{
	unsigned int i;
	struct ppm_policy_req *req = &fbcn_limit_policy.req;

	FUNC_ENTER(FUNC_LV_POLICY);

	if (fbcn_limit_data.is_freq_limited_by_user
		|| fbcn_limit_data.is_core_limited_by_user) {
		ppm_clear_policy_limit(&fbcn_limit_policy);

		for (i = 0; i < req->cluster_num; i++) {
			req->limit[i].max_cpu_core =
				(fbcn_limit_data.limit[i].max_core_num == -1)
				? req->limit[i].max_cpu_core
				: fbcn_limit_data.limit[i].max_core_num;
			req->limit[i].max_cpufreq_idx =
				(fbcn_limit_data.limit[i].max_freq_idx == -1)
				? req->limit[i].max_cpufreq_idx
				: fbcn_limit_data.limit[i].max_freq_idx;
		}

		/* error check */
		for (i = 0; i < req->cluster_num; i++) {
			if (req->limit[i].max_cpu_core <
					req->limit[i].min_cpu_core)
				req->limit[i].min_cpu_core =
					req->limit[i].max_cpu_core;
			if (req->limit[i].max_cpufreq_idx >
					req->limit[i].min_cpufreq_idx)
				req->limit[i].min_cpufreq_idx =
					req->limit[i].max_cpufreq_idx;
		}
	}

	FUNC_EXIT(FUNC_LV_POLICY);
}

static void ppm_fbcn_limit_status_change_cb(bool enable)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	ppm_ver("fbcn_limit policy status changed to %d\n", enable);

	FUNC_EXIT(FUNC_LV_POLICY);
}

static int ppm_fbcn_limit_proc_show(struct seq_file *m, void *v)
{
	int i;

	for (i = 0; i < fbcn_limit_policy.req.cluster_num; i++) {
		seq_printf(m, "%d: min_freq_idx = %d, max_freq_idx = %d\n",
				i, fbcn_limit_data.limit[i].min_freq_idx,
				fbcn_limit_data.limit[i].max_freq_idx);
	}

	return 0;
}

static ssize_t ppm_fbcn_limit_proc_write(struct file *file,
	const char __user *buffer,	size_t count, loff_t *pos)
{
	int id, max_freq, idx, i;
	bool freq_limit = false;

	char *buf = ppm_copy_from_user_for_proc(buffer, count);

	if (!buf)
		return -EINVAL;

	if (sscanf(buf, "%d %d", &id, &max_freq) == 2) {
		if (id < 0 || id >= ppm_main_info.cluster_num) {
			ppm_err("Invalid cluster id: %d\n", id);
			goto out;
		}

		ppm_lock(&fbcn_limit_policy.lock);

		if (!fbcn_limit_policy.is_enabled) {
			ppm_warn("fbcn_limit policy is not enabled!\n");
			ppm_unlock(&fbcn_limit_policy.lock);
			goto out;
		}

		idx = (max_freq == -1) ? -1
			: ppm_main_freq_to_idx(id, max_freq,
					CPUFREQ_RELATION_H);

		/* error check, sync to max idx if max freq < min freq */
		if (fbcn_limit_data.limit[id].min_freq_idx != -1
				&& idx != -1
				&& idx > fbcn_limit_data.limit[id].min_freq_idx)
			fbcn_limit_data.limit[id].min_freq_idx = idx;

		if (idx != fbcn_limit_data.limit[id].max_freq_idx) {
			fbcn_limit_data.limit[id].max_freq_idx = idx;
			ppm_dbg(USER_LIMIT,
					"%d:fbcn limit max_freq = %d KHz(idx = %d)\n",
					id, max_freq, idx);
		}

		/* check is freq limited or not */
		for (i = 0; i < fbcn_limit_policy.req.cluster_num; i++) {
			if (fbcn_limit_data.limit[i].min_freq_idx != -1
					|| fbcn_limit_data.limit[i].max_freq_idx != -1) {
				freq_limit = true;
				break;
			}
		}
		fbcn_limit_data.is_freq_limited_by_user = freq_limit;

		fbcn_limit_policy.is_activated =
			ppm_fbcn_limit_is_policy_active();

		ppm_unlock(&fbcn_limit_policy.lock);
		mt_ppm_main();
	} else
		ppm_err("@%s: Invalid input!\n", __func__);

out:
	free_page((unsigned long)buf);
	return count;
}
PROC_FOPS_RW(fbcn_limit);

static int __init ppm_fbcn_limit_policy_init(void)
{
	int i, ret = 0;

	struct pentry {
		const char *name;
		const struct file_operations *fops;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(fbcn_limit),
	};

	FUNC_ENTER(FUNC_LV_POLICY);

	/* create procfs */
	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create(entries[i].name, 0644,
			policy_dir, entries[i].fops)) {
			ppm_err("%s(), create /proc/ppm/policy/%s failed\n",
				__func__, entries[i].name);
			ret = -EINVAL;
			goto out;
		}
	}

	fbcn_limit_data.limit = kcalloc(ppm_main_info.cluster_num,
		sizeof(*fbcn_limit_data.limit), GFP_KERNEL);
	if (!fbcn_limit_data.limit) {
		ret = -ENOMEM;
		goto out;
	}

	/* init fbcn_limit_data */
	for_each_ppm_clusters(i) {
		fbcn_limit_data.limit[i].min_freq_idx = -1;
		fbcn_limit_data.limit[i].max_freq_idx = -1;
		fbcn_limit_data.limit[i].min_core_num = -1;
		fbcn_limit_data.limit[i].max_core_num = -1;
	}

	if (ppm_main_register_policy(&fbcn_limit_policy)) {
		ppm_err("@%s: fbcn_limit policy register failed\n", __func__);
		kfree(fbcn_limit_data.limit);
		ret = -EINVAL;
		goto out;
	}

	ppm_info("@%s: register %s done!\n", __func__, fbcn_limit_policy.name);

out:
	FUNC_EXIT(FUNC_LV_POLICY);

	return ret;
}

static void __exit ppm_fbcn_limit_policy_exit(void)
{
	FUNC_ENTER(FUNC_LV_POLICY);

	kfree(fbcn_limit_data.limit);

	ppm_main_unregister_policy(&fbcn_limit_policy);

	FUNC_EXIT(FUNC_LV_POLICY);
}

module_init(ppm_fbcn_limit_policy_init);
module_exit(ppm_fbcn_limit_policy_exit);

