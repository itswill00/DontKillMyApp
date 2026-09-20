/*
 * DontKillMyApp Core Engine
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
#include <sys/stat.h>
#include <sys/system_properties.h>

#define STATE_FILE "/data/adb/modules/donykillmyapp/state.json"
#define APPS_FILE  "/data/adb/modules/donykillmyapp/apps.json"

static void fast_prop_get(const char *key, char *out, size_t out_sz, const char *fallback) {
    out[0] = '\0';
    if (__system_property_get(key, out) > 0) {
        return;
    }
    snprintf(out, out_sz, "%s", fallback);
}

static void read_meminfo(long *total_ram, long *used_ram, long *free_ram, long *total_swap, long *used_swap) {
    *total_ram = 0; *used_ram = 0; *free_ram = 0;
    *total_swap = 0; *used_swap = 0;
    long avail_ram = 0, free_swap = 0;

    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", total_ram);
        else if (strncmp(line, "MemFree:", 8) == 0) sscanf(line + 8, "%ld", free_ram);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail_ram);
        else if (strncmp(line, "SwapTotal:", 10) == 0) sscanf(line + 10, "%ld", total_swap);
        else if (strncmp(line, "SwapFree:", 9) == 0) sscanf(line + 9, "%ld", &free_swap);
    }
    fclose(f);

    *total_ram /= 1024;
    avail_ram /= 1024;
    *free_ram = avail_ram;
    *total_swap /= 1024;
    free_swap /= 1024;
    *used_ram = *total_ram - avail_ram;
    *used_swap = *total_swap - free_swap;
}

static void dump_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        fwrite(buf, 1, n, stdout);
    }
    fclose(f);
}

static void cmd_status(void) {
    char model[PROP_VALUE_MAX], android_ver[PROP_VALUE_MAX], os_ver[PROP_VALUE_MAX];
    fast_prop_get("ro.product.model", model, sizeof(model), "Android");
    fast_prop_get("ro.build.version.release", android_ver, sizeof(android_ver), "14");
    fast_prop_get("ro.mi.os.version.name", os_ver, sizeof(os_ver), "OS1.0");

    long total_ram, used_ram, free_ram, total_swap, used_swap;
    read_meminfo(&total_ram, &used_ram, &free_ram, &total_swap, &used_swap);

    char scout[PROP_VALUE_MAX], spc[PROP_VALUE_MAX], memstd[PROP_VALUE_MAX], camera_adj[PROP_VALUE_MAX];
    char lmkd_psi[PROP_VALUE_MAX], lmkd_psi_comp[PROP_VALUE_MAX], phantom_flag[PROP_VALUE_MAX];
    fast_prop_get("persist.sys.miui_scout_enable", scout, sizeof(scout), "true");
    fast_prop_get("persist.sys.spc.enabled", spc, sizeof(spc), "true");
    fast_prop_get("persist.sys.memory_standard.enable", memstd, sizeof(memstd), "true");
    fast_prop_get("persist.sys.miui.camera.boost.killAdj_threshold", camera_adj, sizeof(camera_adj), "400:250:100:50");
    fast_prop_get("ro.lmk.psi_partial_stall_ms", lmkd_psi, sizeof(lmkd_psi), "35");
    fast_prop_get("ro.lmk.psi_complete_stall_ms", lmkd_psi_comp, sizeof(lmkd_psi_comp), "70");
    fast_prop_get("persist.sys.fflag.override.settings_enable_monitor_phantom_procs", phantom_flag, sizeof(phantom_flag), "true");

    int total_apps = 340, restricted_apps = 0, whitelisted_apps = 67;
    char phantom_limit[32] = "32";
    char cached_freezer[32] = "true";
    char duraspeed_disabled[32] = "false";
    char pk_freezer[32] = "true";

    FILE *fstate = fopen(STATE_FILE, "r");
    if (fstate) {
        char line[256];
        while (fgets(line, sizeof(line), fstate)) {
            if (strstr(line, "\"total_apps\":")) sscanf(line, " \"total_apps\": %d", &total_apps);
            else if (strstr(line, "\"restricted_apps\":")) sscanf(line, " \"restricted_apps\": %d", &restricted_apps);
            else if (strstr(line, "\"whitelisted_apps\":")) sscanf(line, " \"whitelisted_apps\": %d", &whitelisted_apps);
            else if (strstr(line, "\"phantom_limit\":")) sscanf(line, " \"phantom_limit\": \"%31[^\"]\"", phantom_limit);
            else if (strstr(line, "\"cached_freezer\":")) sscanf(line, " \"cached_freezer\": \"%31[^\"]\"", cached_freezer);
            else if (strstr(line, "\"duraspeed_disabled\":")) sscanf(line, " \"duraspeed_disabled\": %31[^,\n]", duraspeed_disabled);
            else if (strstr(line, "\"pk_freezer\":")) sscanf(line, " \"pk_freezer\": \"%31[^\"]\"", pk_freezer);
        }
        fclose(fstate);
    }

    bool is_pacified = (strcmp(scout, "false") == 0 &&
                        strcmp(spc, "false") == 0 &&
                        strcmp(memstd, "false") == 0 &&
                        restricted_apps == 0 &&
                        strcmp(phantom_limit, "2147483647") == 0);

    printf("{\n");
    printf("  \"device\": {\"model\":\"%s\",\"android\":\"%s\",\"os\":\"%s\"},\n", model, android_ver, os_ver);
    printf("  \"ram\": {\"total_mb\":%ld,\"used_mb\":%ld,\"free_mb\":%ld,\"zram_total_mb\":%ld,\"zram_used_mb\":%ld},\n", total_ram, used_ram, free_ram, total_swap, used_swap);
    printf("  \"killers\": {\n");
    printf("    \"phantom_limit\": \"%s\",\n", phantom_limit);
    printf("    \"cached_freezer\": \"%s\",\n", cached_freezer);
    printf("    \"duraspeed_disabled\": %s,\n", duraspeed_disabled);
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
    printf("    \"restricted_apps\": %d,\n", restricted_apps);
    printf("    \"unrestricted_apps\": %d,\n", total_apps > restricted_apps ? total_apps - restricted_apps : 0);
    printf("    \"whitelisted_apps\": %d,\n", whitelisted_apps);
    printf("    \"is_pacified\": %s\n", is_pacified ? "true" : "false");
    printf("  }\n");
    printf("}\n");
}

static void save_state_pacified(bool pacified) {
    int total = 0, wl = 0;
    FILE *fr = fopen(STATE_FILE, "r");
    if (fr) {
        char line[256];
        while (fgets(line, sizeof(line), fr)) {
            if (strstr(line, "\"total_apps\":")) sscanf(line, " \"total_apps\": %d", &total);
            else if (strstr(line, "\"whitelisted_apps\":")) sscanf(line, " \"whitelisted_apps\": %d", &wl);
        }
        fclose(fr);
    }
    if (total <= 0) total = 340;
    if (wl <= 0) wl = 67;

    FILE *f = fopen(STATE_FILE, "w");
    if (!f) return;
    if (pacified) {
        fprintf(f, "{\n"
                   "  \"phantom_limit\": \"2147483647\",\n"
                   "  \"cached_freezer\": \"false\",\n"
                   "  \"duraspeed_disabled\": true,\n"
                   "  \"pk_freezer\": \"false\",\n"
                   "  \"total_apps\": %d,\n"
                   "  \"restricted_apps\": 0,\n"
                   "  \"whitelisted_apps\": %d\n"
                   "}\n", total, wl);
    } else {
        fprintf(f, "{\n"
                   "  \"phantom_limit\": \"32\",\n"
                   "  \"cached_freezer\": \"true\",\n"
                   "  \"duraspeed_disabled\": false,\n"
                   "  \"pk_freezer\": \"true\",\n"
                   "  \"total_apps\": %d,\n"
                   "  \"restricted_apps\": %d,\n"
                   "  \"whitelisted_apps\": 0\n"
                   "}\n", total, total);
    }
    fclose(f);
}

static void cmd_pacify(void) {
    save_state_pacified(true);

    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else strcpy(resetprop, "resetprop");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable false", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled false", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable false", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '999:999:999:999'", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 250", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.fflag.override.settings_enable_monitor_phantom_procs false", resetprop); system(cmd);

    system("( "
           "device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1; "
           "settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1; "
           "device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1; "
           "settings put global cached_apps_freezer disabled >/dev/null 2>&1; "
           "pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='miui_standby'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:0 --where \"configureName='k_policy'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1; "
           "for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do "
           "  dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1; "
           "  cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1; "
           "  cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1; "
           "done "
           ") &");

    printf("{\"status\":\"ok\",\"action\":\"pacify\"}\n");
}

static void cmd_restore_stock(void) {
    save_state_pacified(false);

    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else strcpy(resetprop, "resetprop");

    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable true", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled true", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable true", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '400:250:100:50'", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 35", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_complete_stall_ms 70", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.kill_timeout_ms 100", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.vendor.duraspeed.app.on 1", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.vendor.duraspeed.support 1", resetprop); system(cmd);

    system("( "
           "device_config delete activity_manager max_phantom_processes >/dev/null 2>&1; "
           "settings delete global settings_enable_monitor_phantom_procs >/dev/null 2>&1; "
           "device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1; "
           "settings delete global cached_apps_freezer >/dev/null 2>&1; "
           "pm enable com.mediatek.duraspeed >/dev/null 2>&1; "
           "settings put global setting.duraspeed.enabled 1 >/dev/null 2>&1; "
           "settings put system duraspeed_enabled 1 >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='miui_standby'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:896 --where \"configureName='k_policy'\" >/dev/null 2>&1; "
           "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:miuiAuto >/dev/null 2>&1 "
           ") &");

    printf("{\"status\":\"ok\",\"action\":\"restore_stock\"}\n");
}

static void cmd_apps(void) {
    if (access(APPS_FILE, R_OK) == 0) {
        dump_file(APPS_FILE);
        printf("\n");
        return;
    }
    printf("[]\n");
}

static void cmd_set_app(const char *pkg, const char *mode) {
    if (!pkg || !mode) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:%s --where \"pkgName='%s'\" >/dev/null 2>&1 &", mode, pkg);
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
        snprintf(cmd, sizeof(cmd), "(dumpsys deviceidle whitelist +%s >/dev/null 2>&1; cmd appops set %s RUN_IN_BACKGROUND allow >/dev/null 2>&1) &", pkg, pkg);
    } else {
        snprintf(cmd, sizeof(cmd), "dumpsys deviceidle whitelist -%s >/dev/null 2>&1 &", pkg);
    }
    system(cmd);
    printf("{\"status\":\"ok\",\"pkg\":\"%s\",\"whitelisted\":%s}\n", pkg, (enable && strcmp(enable, "1") == 0) ? "true" : "false");
}

static void cmd_set_all_unrestricted(void) {
    system("content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1 &");
    printf("{\"status\":\"ok\",\"action\":\"set_all_unrestricted\"}\n");
}

static void cmd_toggle(const char *key, const char *val) {
    if (!key || !val) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    bool enable = (strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else strcpy(resetprop, "resetprop");

    if (strcmp(key, "phantom") == 0) {
        if (enable) {
            system("(device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1; settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1) &");
        } else {
            system("(device_config delete activity_manager max_phantom_processes >/dev/null 2>&1; settings delete global settings_enable_monitor_phantom_procs >/dev/null 2>&1) &");
        }
    } else if (strcmp(key, "freezer") == 0) {
        if (enable) {
            system("(device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1; settings put global cached_apps_freezer disabled >/dev/null 2>&1) &");
        } else {
            system("(device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1) &");
        }
    } else if (strcmp(key, "duraspeed") == 0) {
        if (enable) {
            system("pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1 &");
        } else {
            system("pm enable com.mediatek.duraspeed >/dev/null 2>&1 &");
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
        snprintf(cmd, sizeof(cmd), "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:%s --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1 &", enable ? "false" : "true");
        system(cmd);
    }

    printf("{\"status\":\"ok\",\"key\":\"%s\",\"enabled\":%s}\n", key, enable ? "true" : "false");
}

static void cmd_logs(void) {
    FILE *fp = popen("logcat -d -b system -b main -b events -t 1200 2>/dev/null | grep -iE '(kill|lmkd|am_kill|lowmemory|powerkeeper|scout|phantom)' | tail -n 40", "r");
    if (!fp) {
        printf("[]\n");
        return;
    }
    printf("[\n");
    char line[512];
    bool first = true;
    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) line[--len] = '\0';
        if (len == 0) continue;

        char escaped[1024];
        size_t j = 0;
        for (size_t i = 0; line[i] && j + 2 < sizeof(escaped); i++) {
            if (line[i] == '"' || line[i] == '\\') {
                escaped[j++] = '\\';
                escaped[j++] = line[i];
            } else {
                escaped[j++] = line[i];
            }
        }
        escaped[j] = '\0';

        if (!first) printf(",\n");
        first = false;
        printf("  \"%s\"", escaped);
    }
    pclose(fp);
    printf("\n]\n");
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
