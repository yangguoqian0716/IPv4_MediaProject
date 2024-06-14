/*
* Copyright (c) Huawei Technologies Co., Ltd. 2019-2022. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* Description:
* Author: huawei
* Create: 2019-10-15
*/

#include <linux/crypto.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/securec.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#include <linux/delay.h>
#include <linux/sched/mm.h>
#include <linux/kallsyms.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0)
#include <linux/namei.h>
#endif

#include "drv_whitelist.h"
#include "user_cfg_interface.h"
#include "kernel_version_adapt.h"

#ifdef CFG_FEATURE_PROCESS_WHITELIST
/*
 * get current process name and name size
 */
STATIC s32 whitelist_get_cur_process_name(u8 **p_file_name, u32 *name_size)
{
    struct mm_struct *active_mm = current->active_mm;

    if (p_file_name == NULL) {
        WHITELIST_ERR("The p_file_name parameter is NULL.\n");
        return WL_ERR_PARA;
    }

    if (name_size == NULL) {
        WHITELIST_ERR("The name_size parameter is NULL.\n");
        return WL_ERR_PARA;
    }

    if (active_mm == NULL) {
        WHITELIST_ERR("The active_mm parameter is NULL.\n");
        return WL_ERR_PARA;
    }

    if (active_mm->exe_file == NULL) {
        WHITELIST_ERR("The exe_file parameter is NULL.\n");
        return WL_ERR_PARA;
    }

    if (active_mm->exe_file->f_path.dentry == NULL) {
        WHITELIST_ERR("The exe_file f_path_dentry is NULL.\n");
        return WL_ERR_PARA;
    }

    *p_file_name = (u8 *)active_mm->exe_file->f_path.dentry->d_name.name;
    *name_size = strlen((s8 *)*p_file_name);

    return WL_OK;
}

/*
 * check if current process name is in whitelist name array
 */
STATIC s32 whitelist_check_process_name(const char **list_name, u32 name_num)
{
    s32 ret;
    u32 i = 0;
    u8 *p_file_name = NULL;
    u32 process_name_len = 0;
    u32 wl_name_len = 0;

    ret = whitelist_get_cur_process_name(&p_file_name, &process_name_len);
    if (ret) {
        WHITELIST_ERR("The whitelist_get_cur_process_name failed. (ret=%d)\n", ret);
        return WL_ERR_PARA;
    }

    if ((p_file_name == NULL) || (list_name == NULL)) {
        WHITELIST_ERR("Current process file point is NULL.\n");
        return WL_ERR_PARA;
    }

    /* check if current process is in white list */
    for (i = 0; i < name_num; i++) {
        wl_name_len = strlen(list_name[i]);
        if (wl_name_len == process_name_len) {
            ret = memcmp(list_name[i], p_file_name, process_name_len);
            if (!ret) {
                p_file_name = NULL;
                return WL_OK;
            }
        }
    }

    p_file_name = NULL;
    WHITELIST_ERR("Process name not in whitelist.\n");
    return WL_ERR_PARA;
}

typedef struct {
    const char *path;
    const char *user_name; /* minirc use only */
    uid_t default_uid; /* none minirc use, minirc uid could change */
} white_list_item_t;

STATIC const white_list_item_t g_white_list[] = {
    {"/var/dmp_daemon",          "HwDmUser",        1101},
    {"/var/tsdaemon",            "HwHiAiUser",      1000},
};

/*
 * get current exe file path
 */
STATIC char *whitelist_get_current_path(char *file_path_in, u32 path_len)
{
    char *res = NULL;
    struct mm_struct *active_mm = current->active_mm;

    if (active_mm == NULL) {
        WHITELIST_ERR("The active_mm parameter is NULL.\n");
        return NULL;
    }

    if (active_mm->exe_file == NULL) {
        WHITELIST_ERR("The exe_file parameter is NULL.\n");
        return NULL;
    }

    res = d_path(&active_mm->exe_file->f_path, file_path_in, path_len);
    if (IS_ERR_OR_NULL(res)) {
        WHITELIST_ERR("The res is err or null. (errno=%ld)\n", PTR_ERR(res));
        res = NULL;
    }

    return res;
}

#if defined(CFG_FEATURE_CUSTOM_OS) || defined(STATIC_SKIP)

#define USER_NAME_LEN_MAX 256
#define PREFIX_AND_SUFFIX_LEN 2
#define DECIMAL_SCALE 10
/* passwd file line example: username:x:uid:gid::/home/username:/sbin/nologin */
STATIC s32 get_uid_by_username(const char *username, const char *passwd_file, uid_t *uid)
{
    const char *index = NULL;
    const char *uid_start = NULL;
    const char *uid_end = NULL;
    char key[USER_NAME_LEN_MAX + PREFIX_AND_SUFFIX_LEN + 1]; /* add prefix and suffix and \0 */
    int ret;

    ret = sprintf_s(key, sizeof(key), "\n%s:", username);
    if (ret <= 0) {
        WHITELIST_ERR("sprintf_s fail.(ret=%d).\n", ret);
        return WL_ERR_PARA;
    }
    index = strstr(passwd_file, key);
    if (index == NULL) {
        return WL_ERR_PARA;
    }
    uid_start = strchr(index + strlen(key), ':');
    if (uid_start == NULL) {
        WHITELIST_ERR("file search uid start fail.\n");
        return WL_ERR_PARA;
    }
    uid_start += strlen(":");

    uid_end = strchr(uid_start, ':');
    if (uid_end == NULL) {
        WHITELIST_ERR("file search uid end fail.\n");
        return WL_ERR_PARA;
    }
    ret = strncpy_s(key, sizeof(key), uid_start, uid_end - uid_start);
    if (ret) {
        WHITELIST_ERR("strncpy_s fail.(ret=%d).\n", ret);
        return WL_ERR_PARA;
    }
    ret = kstrtouint(key, DECIMAL_SCALE, uid);
    if (ret) {
        WHITELIST_ERR("str to uint fail.(ret=%d).\n", ret);
        return WL_ERR_PARA;
    }

    return ret;
}

#if defined(CFG_FEATURE_SUPPORT_DEF_USER)
STATIC bool whitelist_all_users_existence_check(const char *passwd_file)
{
    int i;
    uid_t uid;
    const char *necessary_users[] = {"HwDmUser", "HwSysUser", "HwBaseUser"};
    int item_cnt = ARRAY_SIZE(necessary_users);

    for (i = 0; i < item_cnt; i++) {
        if (get_uid_by_username(necessary_users[i], passwd_file, &uid) != WL_OK) {
            return false;
        }
    }

    return true;
}
#endif

#define DEFAULT_VERIFY_USER "HwHiAiUser" /* use if customer not create new necessary users */
STATIC s32 whitelist_cmp_uid(const char *config_username, uid_t uid, const char *passwd_file)
{
    uid_t config_uid;

#if defined(CFG_FEATURE_SUPPORT_DEF_USER)
    if (!whitelist_all_users_existence_check(passwd_file)) {
        config_username = DEFAULT_VERIFY_USER;
    }
#endif

    if (get_uid_by_username(config_username, passwd_file, &config_uid) == WL_OK) {
        return config_uid == uid ? WL_OK : WL_ERR_NO_AUTHORITY;
    }

    WHITELIST_ERR("find uid by user name fail.\n");

    return WL_ERR_NO_AUTHORITY;
}

#define STR_FILE_LEN_MAX (4 * 1024 * 1024)
#define PREFIX_LF_SIZE 1 /* add prefix for line begin search */
#define WHITELIST_READ_FILE_MODE 0644
/* read text file as string, add \n prefix, caller free memory if return none NULL */
STATIC char *whitelist_read_text_file(const char *path)
{
    struct kstat stat;
    struct file *fp;
    loff_t pos = 0;
    char *buf = NULL;
    ssize_t read_len;
    int ret;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 20, 0)
    struct path kernel_path;

    ret = kern_path(path, LOOKUP_FOLLOW, &kernel_path);
    if (ret < 0) {
        return NULL;
    }
    ret = vfs_getattr(&kernel_path, &stat, STATX_BASIC_STATS, AT_NO_AUTOMOUNT);
#else
    mm_segment_t old_fs;

    old_fs = get_fs();
    set_fs(KERNEL_DS);
    ret = vfs_stat(path, &stat);
    set_fs(old_fs);
#endif
    if (ret || stat.size <= 0 || stat.size > STR_FILE_LEN_MAX) {
        WHITELIST_ERR("stat file fail. (ret=%d;size=%lld)\n", ret, stat.size);
        return NULL;
    }

    fp = filp_open(path, O_RDONLY, WHITELIST_READ_FILE_MODE);
    if (IS_ERR_OR_NULL(fp)) {
        WHITELIST_ERR("unable to open file. (errno=%ld)\n", PTR_ERR(fp));
        return NULL;
    }

    buf = vmalloc(stat.size + PREFIX_LF_SIZE + 1);
    if (buf == NULL) {
        WHITELIST_ERR("kmalloc fail. (size=%lld)\n", stat.size);
        (void)filp_close(fp, NULL);
        return NULL;
    }
    buf[0] = '\n';
    read_len = kernel_read(fp, buf + PREFIX_LF_SIZE, stat.size, &pos);
    (void)filp_close(fp, NULL);
    if (read_len <= 0) {
        WHITELIST_ERR("kernel read fail. (read len=%ld)\n", read_len);
        vfree(buf);
        return NULL;
    }
    buf[PREFIX_LF_SIZE + read_len] = '\0';
    return buf;
}

#define PASSWD_PATH "/etc/passwd"
STATIC s32 whitelist_match_uid_path_cos(const char *process_name, const char *current_path, uid_t uid)
{
    int item_cnt = ARRAY_SIZE(g_white_list);
    int i;
    const char *config_process_name = NULL;
    char *passwd_file = NULL;

    passwd_file = whitelist_read_text_file(PASSWD_PATH);
    if (passwd_file == NULL) {
        return WL_ERR_PARA;
    }
    for (i = 0; i < item_cnt; i++) {
        config_process_name = kbasename(g_white_list[i].path);
        if (config_process_name == NULL) {
            continue;
        }
        if (strcmp(config_process_name, process_name) == 0 &&
            strcmp(g_white_list[i].path, current_path) == 0 &&
            whitelist_cmp_uid(g_white_list[i].user_name, uid, passwd_file) == WL_OK) {
            vfree(passwd_file);
            return WL_OK;
        }
    }
    vfree(passwd_file);
    WHITELIST_ERR("match fail. (proc name=\"%s\";path=\"%s\";uid=%u)\n", process_name, current_path, uid);
    return WL_ERR_NO_AUTHORITY;
}
#endif
#if !defined(CFG_FEATURE_CUSTOM_OS) || defined(STATIC_SKIP)
/* fixed username and uid */
STATIC s32 whitelist_match_uid_path(const char *process_name, const char *current_path, uid_t uid)
{
    int item_cnt = ARRAY_SIZE(g_white_list);
    int i;
    const char *config_process_name = NULL;

    for (i = 0; i < item_cnt; i++) {
        if (g_white_list[i].default_uid != uid) {
            continue;
        }
        config_process_name = kbasename(g_white_list[i].path);
        if (config_process_name == NULL) {
            continue;
        }
        if (strcmp(config_process_name, process_name) == 0 &&
                (strcmp(g_white_list[i].path, current_path) == 0)) {
            return WL_OK;
        }
    }
    WHITELIST_ERR("match fail. (proc name=\"%s\";path=\"%s\";uid=%u)\n", process_name, current_path, uid);
    return WL_ERR_NO_AUTHORITY;
}
#endif

/*
 * check process info
 */
STATIC s32 whitelist_check_process_info(void)
{
    s32 ret;
    u8 *process_name = NULL;
    u32 process_len = 0;
    uid_t uid;
    char *path_str = NULL;
    char *current_path = NULL;

    ret = whitelist_get_cur_process_name(&process_name, &process_len);
    if (ret) {
        WHITELIST_ERR("The whitelist_get_cur_process_name failed. (ret=%d)\n", ret);
        return ret;
    }
    path_str = (char *)vzalloc(PATH_MAX);
    if (path_str == NULL) {
        WHITELIST_ERR("File path vmalloc failed.\n");
        return WL_ERR_PARA;
    }
    current_path = whitelist_get_current_path(path_str, PATH_MAX);
    if (current_path == NULL) {
        vfree(path_str);
        return WL_ERR_PARA;
    }
    uid = __kuid_val(current_uid());

#ifdef CFG_FEATURE_CUSTOM_OS
    ret = whitelist_match_uid_path_cos(process_name, current_path, uid);
#else
    ret = whitelist_match_uid_path(process_name, current_path, uid);
#endif
    vfree(path_str);
    return ret;
}

#ifdef CFG_FEATURE_CONTAINER
typedef int (*FUNC_IS_IN_CONTAINER)(void);
STATIC s32 is_in_container(void)
{
    static FUNC_IS_IN_CONTAINER pfun = NULL;

    if (!pfun) {
        pfun = (FUNC_IS_IN_CONTAINER)(uintptr_t)__kallsyms_lookup_name("devdrv_manager_container_is_in_container");
        if (pfun == NULL) {
            WHITELIST_ERR("kallsyms_lookup_name fail\n");
            return 1;
        }
    }
    return pfun();
}
#endif

/*
 * ioctl whitelist process
 */
s32 whitelist_process_handler(const char **list_name, u32 name_num)
{
    s32 ret;
    u32 i = 0;

    if ((list_name == NULL) || (name_num > WL_NAME_NUM_MAX)) {
        WHITELIST_ERR("List name para is incorrect. (name_num=%u)\n", name_num);
        return WL_ERR_PARA;
    }

    for (i = 0; i < name_num; i++) {
        if (list_name[i] == NULL) {
            WHITELIST_ERR("Input process name is NULL. (i=%u)\n", i);
            return WL_ERR_PARA;
        }
    }

#ifdef CFG_FEATURE_CONTAINER
    if (is_in_container()) {
        WHITELIST_ERR("check fail because in container\n");
        return WL_ERR_NO_AUTHORITY;
    }
#endif

    /* check cur process name */
    ret = whitelist_check_process_name(list_name, name_num);
    if (ret != WL_OK) {
        WHITELIST_ERR("The whitelist_check_process_name failed. (ret=%d)\n", ret);
        return ret;
    }

    /* check uid and path */
    ret = whitelist_check_process_info();
    if (ret != WL_OK) {
        WHITELIST_ERR("The whitelist_check_process_info failed. (ret=%d)\n", ret);
        return ret;
    }

    return ret;
}
// EXPORT_SYMBOL(whitelist_process_handler);

#else

s32 whitelist_process_handler(const char **list_name, u32 name_num)
{
    return WL_OK;
}
// EXPORT_SYMBOL(whitelist_process_handler);
#endif
