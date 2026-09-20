/*
 * DontKillMyApp Core Engine
 * HyperOS & Android 14 Background Killer Pacifier
 * Copyright (C) 2026 @itswill00
 * Licensed under the GNU General Public License v3.0
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

static void run_cmd_output(const char *cmd, char *out, size_t out_sz) {
    out[0] = '\0';
    FILE *fp = popen(cmd, "r");
    if (!fp) return;
    if (fgets(out, out_sz, fp)) {
        size_t len = strlen(out);
        while (len > 0 && (out[len - 1] == '\r' || out[len - 1] == '\n' || out[len - 1] == ' ')) {
            out[--len] = '\0';
        }
    }
    pclose(fp);
}

static void get_prop(const char *key, char *out, size_t out_sz, const char *fallback) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "getprop %s", key);
    run_cmd_output(cmd, out, out_sz);
    if (out[0] == '\0') {
        snprintf(out, out_sz, "%s", fallback);
    }
}

static void escape_json_str(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 2 < out_sz; i++) {
        if (in[i] == '"' || in[i] == '\\') {
            out[j++] = '\\';
            out[j++] = in[i];
        } else if (in[i] == '\n') {
            out[j++] = '\\';
            out[j++] = 'n';
        } else if (in[i] == '\r') {
            continue;
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

static void cmd_status(void) {
    char model[64], android_ver[32], os_ver[32], kernel[128];
    get_prop("ro.product.model", model, sizeof(model), "Android Device");
    get_prop("ro.build.version.release", android_ver, sizeof(android_ver), "14");
    get_prop("ro.mi.os.version.name", os_ver, sizeof(os_ver), "OS1.0");
    run_cmd_output("uname -r", kernel, sizeof(kernel));

    // Memory info
    long total_ram = 0, free_ram = 0, avail_ram = 0, total_swap = 0, free_swap = 0;
    FILE *fmem = fopen("/proc/meminfo", "r");
    if (fmem) {
        char line[128];
        while (fgets(line, sizeof(line), fmem)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total_ram);
            else if (strncmp(line, "MemFree:", 8) == 0) sscanf(line + 8, "%ld", &free_ram);
            else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail_ram);
            else if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line + 10, "%ld", &total_swap);
            else if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line + 9, "%ld", &free_swap);
        }
        fclose(fmem);
    }
    total_ram /= 1024;
    free_ram /= 1024;
    avail_ram /= 1024;
    total_swap /= 1024;
    free_swap /= 1024;
    long used_ram = total_ram - avail_ram;
    long used_swap = total_swap - free_swap;

    // Killer statuses
    char phantom_limit[64], freezer[64];
    run_cmd_output("device_config get activity_manager max_phantom_processes 2>/dev/null", phantom_limit, sizeof(phantom_limit));
    if (phantom_limit[0] == '\0' || strcmp(phantom_limit, "null") == 0) strcpy(phantom_limit, "32");

    run_cmd_output("device_config get activity_manager_native_boot use_freezer 2>/dev/null", freezer, sizeof(freezer));
    if (freezer[0] == '\0' || strcmp(freezer, "null") == 0) strcpy(freezer, "true");

    char scout[32], spc[32], memstd[32], camera_adj[64], lmkd_psi[32], lmkd_psi_comp[32];
    get_prop("persist.sys.miui_scout_enable", scout, sizeof(scout), "true");
    get_prop("persist.sys.spc.enabled", spc, sizeof(spc), "true");
    get_prop("persist.sys.memory_standard.enable", memstd, sizeof(memstd), "true");
    get_prop("persist.sys.miui.camera.boost.killAdj_threshold", camera_adj, sizeof(camera_adj), "400:250:100:50");
    get_prop("ro.lmk.psi_partial_stall_ms", lmkd_psi, sizeof(lmkd_psi), "35");
    get_prop("ro.lmk.psi_complete_stall_ms", lmkd_psi_comp, sizeof(lmkd_psi_comp), "70");

    char dura_state[64];
    run_cmd_output("dumpsys package com.mediatek.duraspeed 2>/dev/null | grep -i 'enabled=' | head -n 1", dura_state, sizeof(dura_state));
    bool duraspeed_disabled = (strstr(dura_state, "enabled=2") != NULL || strstr(dura_state, "enabled=3") != NULL);

    char pk_freezer[64];
    run_cmd_output("content query --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --projection configureName:configureParam --where \"configureName='FrozenControlNewStatus'\" 2>/dev/null | grep -o 'configureParam=[^ ]*' | cut -d= -f2", pk_freezer, sizeof(pk_freezer));
    if (pk_freezer[0] == '\0') strcpy(pk_freezer, "true");

    char pk_restricted_str[64];
    run_cmd_output("content query --uri content://com.miui.powerkeeper.configure/userTable --projection pkgName --where \"bgControl!='noRestrict'\" 2>/dev/null | grep -c 'Row:'", pk_restricted_str, sizeof(pk_restricted_str));
    int pk_restricted = atoi(pk_restricted_str);

    char total_apps_str[64];
    run_cmd_output("content query --uri content://com.miui.powerkeeper.configure/userTable --projection pkgName 2>/dev/null | grep -c 'Row:'", total_apps_str, sizeof(total_apps_str));
    int total_apps = atoi(total_apps_str);

    char whitelisted_str[64];
    run_cmd_output("dumpsys deviceidle whitelist 2>/dev/null | grep '^user,' | grep -c ','", whitelisted_str, sizeof(whitelisted_str));
    int whitelisted_count = atoi(whitelisted_str);

    bool is_pacified = (strcmp(scout, "false") == 0 &&
                        strcmp(spc, "false") == 0 &&
                        strcmp(memstd, "false") == 0 &&
                        pk_restricted == 0 &&
                        strcmp(phantom_limit, "2147483647") == 0);

    printf("{\n");
    printf("  \"device\": {\"model\":\"%s\",\"android\":\"%s\",\"os\":\"%s\",\"kernel\":\"%s\"},\n", model, android_ver, os_ver, kernel);
    printf("  \"ram\": {\"total_mb\":%ld,\"used_mb\":%ld,\"free_mb\":%ld,\"zram_total_mb\":%ld,\"zram_used_mb\":%ld},\n", total_ram, used_ram, free_ram, total_swap, used_swap);
    printf("  \"killers\": {\n");
    printf("    \"phantom_limit\": \"%s\",\n", phantom_limit);
    printf("    \"cached_freezer\": \"%s\",\n", freezer);
    printf("    \"duraspeed_disabled\": %s,\n", duraspeed_disabled ? "true" : "false");
    printf("    \"scout_enable\": %s,\n", strcmp(scout, "true") == 0 ? "true" : "false");
    printf("    \"spc_enable\": %s,\n", strcmp(spc, "true") == 0 ? "true" : "false");
    printf("    \"memory_standard_enable\": %s,\n", strcmp(memstd, "true") == 0 ? "true" : "false");
    printf("    \"camera_boost_adj\": \"%s\",\n", camera_adj);
    printf("    \"pk_freezer\": \"%s\",\n", pk_freezer);
    printf("    \"lmkd_psi_partial\": \"%s\",\n", lmkd_psi);
    printf("    \"lmkd_psi_complete\": \"%s\"\n", lmkd_psi_comp);
    printf("  },\n");
    printf("  \"stats\": {\n");
    printf("    \"total_apps\": %d,\n", total_apps);
    printf("    \"restricted_apps\": %d,\n", pk_restricted);
    printf("    \"unrestricted_apps\": %d,\n", total_apps > pk_restricted ? total_apps - pk_restricted : 0);
    printf("    \"whitelisted_apps\": %d,\n", whitelisted_count);
    printf("    \"is_pacified\": %s\n", is_pacified ? "true" : "false");
    printf("  }\n");
    printf("}\n");
}

static void cmd_pacify(void) {
    // 1. Android 14 Phantom Process Limit
    system("device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1");
    system("settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1");
    system("setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false >/dev/null 2>&1");

    // 2. Android 14 Cached Apps Freezer
    system("device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1");
    system("settings put global cached_apps_freezer disabled >/dev/null 2>&1");

    // 3. MediaTek DuraSpeed
    system("pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1");
    system("settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1");

    // 4. PowerKeeper GlobalFeatureTable
    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1");
    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='miui_standby'\" >/dev/null 2>&1");
    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:0 --where \"configureName='k_policy'\" >/dev/null 2>&1");

    // 5. PowerKeeper userTable (all apps -> noRestrict)
    system("content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1");

    // 6. Whitelist all user apps from battery optimization & AppOps
    system("for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do "
           "  dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1; "
           "  cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1; "
           "  cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1; "
           "done");

    // 7. Resetprops
    char resetprop[128];
    run_cmd_output("which resetprop 2>/dev/null || echo /data/adb/ksu/bin/resetprop", resetprop, sizeof(resetprop));
    if (access(resetprop, X_OK) == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '999:999:999:999'", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 250", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_complete_stall_ms 700", resetprop); system(cmd);
    }

    printf("{\"status\":\"ok\",\"action\":\"pacify\"}\n");
}

static void cmd_restore_stock(void) {
    system("device_config delete activity_manager max_phantom_processes >/dev/null 2>&1");
    system("settings delete global settings_enable_monitor_phantom_procs >/dev/null 2>&1");
    system("device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1");
    system("settings put global cached_apps_freezer disabled >/dev/null 2>&1");
    system("pm enable com.mediatek.duraspeed >/dev/null 2>&1");

    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1");
    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='miui_standby'\" >/dev/null 2>&1");
    system("content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:896 --where \"configureName='k_policy'\" >/dev/null 2>&1");

    char resetprop[128];
    run_cmd_output("which resetprop 2>/dev/null || echo /data/adb/ksu/bin/resetprop", resetprop, sizeof(resetprop));
    if (access(resetprop, X_OK) == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '400:250:100:50'", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 35", resetprop); system(cmd);
    }

    printf("{\"status\":\"ok\",\"action\":\"restore_stock\"}\n");
}

static void cmd_apps(void) {
    // Collect user apps list into temporary map
    FILE *fp = popen("content query --uri content://com.miui.powerkeeper.configure/userTable --projection pkgName:bgControl 2>/dev/null", "r");
    if (!fp) {
        printf("[]\n");
        return;
    }

    // Read whitelist cache
    char wl_output[16384];
    run_cmd_output("dumpsys deviceidle whitelist 2>/dev/null | grep '^user,'", wl_output, sizeof(wl_output));

    // Read 3rd-party user packages list
    char u_output[16384];
    run_cmd_output("pm list packages -3 2>/dev/null | cut -d: -f2", u_output, sizeof(u_output));

    printf("[\n");
    char line[512];
    bool first = true;
    while (fgets(line, sizeof(line), fp)) {
        // Line format: Row: 0 pkgName=xxx, bgControl=yyy
        char *pkg_ptr = strstr(line, "pkgName=");
        char *bg_ptr = strstr(line, "bgControl=");
        if (!pkg_ptr || !bg_ptr) continue;

        pkg_ptr += 8;
        char *pkg_end = strchr(pkg_ptr, ',');
        if (!pkg_end) continue;
        *pkg_end = '\0';

        bg_ptr += 10;
        char *bg_end = strchr(bg_ptr, '\r');
        if (!bg_end) bg_end = strchr(bg_ptr, '\n');
        if (bg_end) *bg_end = '\0';

        bool is_whitelisted = (strstr(wl_output, pkg_ptr) != NULL);
        bool is_user = (strstr(u_output, pkg_ptr) != NULL);

        if (!first) printf(",\n");
        first = false;

        printf("  {\"pkg\":\"%s\",\"bgControl\":\"%s\",\"whitelisted\":%s,\"isUser\":%s}",
               pkg_ptr, bg_ptr, is_whitelisted ? "true" : "false", is_user ? "true" : "false");
    }
    pclose(fp);
    printf("\n]\n");
}

static void cmd_set_app(const char *pkg, const char *mode) {
    if (!pkg || !mode) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:%s --where \"pkgName='%s'\" >/dev/null 2>&1", mode, pkg);
    system(cmd);
    printf("{\"status\":\"ok\",\"pkg\":\"%s\",\"bgControl\":\"%s\"}\n", pkg, mode);
}

static void cmd_whitelist_app(const char *pkg, const char *enable) {
    if (!pkg) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    char cmd[512];
    if (enable && strcmp(enable, "1") == 0) {
        snprintf(cmd, sizeof(cmd), "dumpsys deviceidle whitelist +%s >/dev/null 2>&1; cmd appops set %s RUN_IN_BACKGROUND allow >/dev/null 2>&1", pkg, pkg);
    } else {
        snprintf(cmd, sizeof(cmd), "dumpsys deviceidle whitelist -%s >/dev/null 2>&1", pkg);
    }
    system(cmd);
    printf("{\"status\":\"ok\",\"pkg\":\"%s\",\"whitelisted\":%s}\n", pkg, (enable && strcmp(enable, "1") == 0) ? "true" : "false");
}

static void cmd_logs(void) {
    FILE *fp = popen("logcat -d -b events -b system -b main 2>/dev/null | grep -iE '(kill|lmkd|am_kill|lowmemory|powerkeeper|scout|phantom)' | tail -n 40", "r");
    if (!fp) {
        printf("[]\n");
        return;
    }
    printf("[\n");
    char line[1024];
    char escaped[2048];
    bool first = true;
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = '\0';
        if (len == 0) continue;
        escape_json_str(line, escaped, sizeof(escaped));
        if (!first) printf(",\n");
        first = false;
        printf("  \"%s\"", escaped);
    }
    pclose(fp);
    printf("\n]\n");
}

static void cmd_set_all_unrestricted(void) {
    system("content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1");
    printf("{\"status\":\"ok\",\"action\":\"set_all_unrestricted\"}\n");
}

static void cmd_toggle(const char *key, const char *val) {
    if (!key || !val) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    bool enable = (strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
    char resetprop[128];
    run_cmd_output("which resetprop 2>/dev/null || echo /data/adb/ksu/bin/resetprop", resetprop, sizeof(resetprop));

    if (strcmp(key, "phantom") == 0) {
        if (enable) {
            system("device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1");
            system("settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1");
        } else {
            system("device_config delete activity_manager max_phantom_processes >/dev/null 2>&1");
            system("settings delete global settings_enable_monitor_phantom_procs >/dev/null 2>&1");
        }
    } else if (strcmp(key, "freezer") == 0) {
        if (enable) {
            system("device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1");
            system("settings put global cached_apps_freezer disabled >/dev/null 2>&1");
        } else {
            system("device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1");
            system("settings put global cached_apps_freezer disabled >/dev/null 2>&1");
        }
    } else if (strcmp(key, "duraspeed") == 0) {
        if (enable) {
            system("pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1");
            system("settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1");
        } else {
            system("pm enable com.mediatek.duraspeed >/dev/null 2>&1");
            system("settings delete global setting.duraspeed.enabled >/dev/null 2>&1");
        }
    } else if (strcmp(key, "scout") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable %s", resetprop, enable ? "false" : "true");
        system(cmd);
    } else if (strcmp(key, "spc") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled %s", resetprop, enable ? "false" : "true");
        system(cmd);
    } else if (strcmp(key, "memstd") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable %s", resetprop, enable ? "false" : "true");
        system(cmd);
    } else if (strcmp(key, "camera") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '%s'", resetprop, enable ? "999:999:999:999" : "400:250:100:50");
        system(cmd);
    } else if (strcmp(key, "lmkd") == 0) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms %s", resetprop, enable ? "250" : "35");
        system(cmd);
    } else if (strcmp(key, "pk_freezer") == 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:%s --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1", enable ? "false" : "true");
        system(cmd);
    }

    printf("{\"status\":\"ok\",\"key\":\"%s\",\"enabled\":%s}\n", key, enable ? "true" : "false");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cmd_status();
        return 0;
    }

    const char *action = argv[1];
    if (strcmp(action, "status") == 0) {
        cmd_status();
    } else if (strcmp(action, "pacify") == 0) {
        cmd_pacify();
    } else if (strcmp(action, "restore_stock") == 0) {
        cmd_restore_stock();
    } else if (strcmp(action, "apps") == 0) {
        cmd_apps();
    } else if (strcmp(action, "set_all_unrestricted") == 0) {
        cmd_set_all_unrestricted();
    } else if (strcmp(action, "set_app") == 0 && argc >= 4) {
        cmd_set_app(argv[2], argv[3]);
    } else if (strcmp(action, "whitelist_app") == 0 && argc >= 4) {
        cmd_whitelist_app(argv[2], argv[3]);
    } else if (strcmp(action, "toggle") == 0 && argc >= 4) {
        cmd_toggle(argv[2], argv[3]);
    } else if (strcmp(action, "logs") == 0) {
        cmd_logs();
    } else {
        printf("{\"error\":\"unknown_command\"}\n");
        return 1;
    }

    return 0;
}

