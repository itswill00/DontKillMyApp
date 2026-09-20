/*
 * DontKillMyApp Core Engine - Universal OEM Edition
 * Copyright (C) 2026 @itswill00
 * Licensed under the GNU General Public License v3.0
 *
 * Supports: MIUI/HyperOS, OneUI (Samsung), XOS/HiOS (Transsion), ColorOS/OxygenOS (OPlus),
 *           stock AOSP, and any Android 8-16 device. Capability-probed, never hardfails.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/system_properties.h>

#define STATE_FILE "/data/adb/modules/donykillmyapp/state.json"
#define APPS_FILE  "/data/adb/modules/donykillmyapp/apps.json"
#define CAPS_FILE  "/data/adb/modules/donykillmyapp/caps.json"

typedef struct {
    bool phantom;
    bool freezer;
    bool duraspeed;
    bool pk;
    bool xiaomi;
    bool samsung;
    bool transsion;
    bool oplus;
    int  sdk;
    char oem[32];
    char skin[32];
} caps_t;

static void fast_prop_get(const char *key, char *out, size_t out_sz, const char *fallback) {
    out[0] = '\0';
    if (__system_property_get(key, out) > 0) return;
    snprintf(out, out_sz, "%s", fallback);
}

static int get_sdk_int(void) {
    char v[PROP_VALUE_MAX];
    fast_prop_get("ro.build.version.sdk", v, sizeof(v), "0");
    return atoi(v);
}

static void to_lower_str(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static void detect_oem_caps(caps_t *caps) {
    memset(caps, 0, sizeof(*caps));
    caps->sdk = get_sdk_int();

    char manuf[PROP_VALUE_MAX] = {0}, mi_os[PROP_VALUE_MAX] = {0}, miui[PROP_VALUE_MAX] = {0};
    char mtk_plat[PROP_VALUE_MAX] = {0}, mtk_ver[PROP_VALUE_MAX] = {0};
    char oplus_ver[PROP_VALUE_MAX] = {0}, oplus2[PROP_VALUE_MAX] = {0}, coloros[PROP_VALUE_MAX] = {0};
    char oneui[PROP_VALUE_MAX] = {0};

    fast_prop_get("ro.product.manufacturer", manuf, sizeof(manuf), "");
    fast_prop_get("ro.mi.os.version.name", mi_os, sizeof(mi_os), "");
    fast_prop_get("ro.miui.ui.version.name", miui, sizeof(miui), "");
    fast_prop_get("ro.mediatek.platform", mtk_plat, sizeof(mtk_plat), "");
    fast_prop_get("ro.mediatek.version.release", mtk_ver, sizeof(mtk_ver), "");
    fast_prop_get("ro.build.version.oplusrom", oplus_ver, sizeof(oplus_ver), "");
    fast_prop_get("ro.build.version.opporom", oplus2, sizeof(oplus2), "");
    fast_prop_get("ro.build.version.coloros", coloros, sizeof(coloros), "");
    fast_prop_get("ro.build.version.oneui", oneui, sizeof(oneui), "");

    to_lower_str(manuf);

    // Xiaomi detection
    if (mi_os[0] || miui[0]) caps->xiaomi = true;
    if (strstr(manuf, "xiaomi") || strstr(manuf, "redmi") || strstr(manuf, "poco")) caps->xiaomi = true;

    // MTK detection
    if (mtk_plat[0] || mtk_ver[0]) caps->duraspeed = true;
    // caps->duraspeed may also be true via caps.json (package probe) - handled below

    // OPlus detection (Oppo/OnePlus/Realme)
    if (oplus_ver[0] || oplus2[0] || coloros[0]) caps->oplus = true;

    // Samsung detection
    if (strstr(manuf, "samsung") || oneui[0]) caps->samsung = true;

    // Transsion detection via manufacturer
    if (strstr(manuf, "infinix") || strstr(manuf, "tecno") || strstr(manuf, "itel") || strstr(manuf, "transsion")) {
        caps->transsion = true;
    }

    // Universal capabilities by SDK
    caps->phantom = caps->sdk >= 31;
    caps->freezer = caps->sdk >= 34;

    // PK availability is Xiaomi + provider (refined by caps.json below)
    caps->pk = caps->xiaomi;

    // OEM string + skin for UI
    if (caps->xiaomi) {
        snprintf(caps->oem, sizeof(caps->oem), "xiaomi");
        if (mi_os[0]) snprintf(caps->skin, sizeof(caps->skin), "HyperOS %s", mi_os);
        else if (miui[0]) snprintf(caps->skin, sizeof(caps->skin), "MIUI %s", miui);
        else snprintf(caps->skin, sizeof(caps->skin), "HyperOS");
    } else if (caps->samsung) {
        snprintf(caps->oem, sizeof(caps->oem), "samsung");
        if (oneui[0]) snprintf(caps->skin, sizeof(caps->skin), "OneUI %s", oneui);
        else snprintf(caps->skin, sizeof(caps->skin), "OneUI");
    } else if (caps->oplus) {
        snprintf(caps->oem, sizeof(caps->oem), "oplus");
        if (coloros[0]) snprintf(caps->skin, sizeof(caps->skin), "ColorOS %s", coloros);
        else if (oplus_ver[0]) snprintf(caps->skin, sizeof(caps->skin), "OPlus %s", oplus_ver);
        else snprintf(caps->skin, sizeof(caps->skin), "ColorOS");
    } else if (caps->transsion) {
        snprintf(caps->oem, sizeof(caps->oem), "transsion");
        snprintf(caps->skin, sizeof(caps->skin), "XOS/HiOS");
    } else {
        snprintf(caps->oem, sizeof(caps->oem), "generic");
        snprintf(caps->skin, sizeof(caps->skin), "AOSP");
    }
}

static void load_caps_file(caps_t *caps) {
    FILE *f = fopen(CAPS_FILE, "r");
    if (!f) return;
    char buf[1024];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    if (n == 0) return;
    buf[n] = '\0';

    // caps.json is written by service.sh with accurate pm-based probes. Override inferred values.
    if (strstr(buf, "\"phantom\":true")) caps->phantom = true;
    else if (strstr(buf, "\"phantom\":false")) caps->phantom = false;

    if (strstr(buf, "\"freezer\":true")) caps->freezer = true;
    else if (strstr(buf, "\"freezer\":false")) caps->freezer = false;

    if (strstr(buf, "\"duraspeed\":true")) caps->duraspeed = true;
    else if (strstr(buf, "\"duraspeed\":false")) caps->duraspeed = false;

    if (strstr(buf, "\"pk\":true")) caps->pk = true;
    else if (strstr(buf, "\"pk\":false")) caps->pk = false;

    if (strstr(buf, "\"xiaomi\":true")) caps->xiaomi = true;
    if (strstr(buf, "\"samsung\":true")) caps->samsung = true;
    if (strstr(buf, "\"transsion\":true")) caps->transsion = true;
    if (strstr(buf, "\"oplus\":true")) caps->oplus = true;

    // Re-derive oem/skin if caps.json says different
    if (caps->xiaomi) snprintf(caps->oem, sizeof(caps->oem), "xiaomi");
    else if (caps->samsung) snprintf(caps->oem, sizeof(caps->oem), "samsung");
    else if (caps->oplus) snprintf(caps->oem, sizeof(caps->oem), "oplus");
    else if (caps->transsion) snprintf(caps->oem, sizeof(caps->oem), "transsion");
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
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) fwrite(buf, 1, n, stdout);
    fclose(f);
}

static void cmd_status(void) {
    caps_t caps;
    detect_oem_caps(&caps);
    load_caps_file(&caps);

    char model[PROP_VALUE_MAX], android_ver[PROP_VALUE_MAX], os_ver[PROP_VALUE_MAX];
    fast_prop_get("ro.product.model", model, sizeof(model), "Android");
    fast_prop_get("ro.build.version.release", android_ver, sizeof(android_ver), "14");
    if (caps.xiaomi) {
        char tmp[PROP_VALUE_MAX];
        fast_prop_get("ro.mi.os.version.name", tmp, sizeof(tmp), "");
        if (tmp[0]) snprintf(os_ver, sizeof(os_ver), "%s", tmp);
        else { fast_prop_get("ro.miui.ui.version.name", tmp, sizeof(tmp), "OS1.0"); snprintf(os_ver, sizeof(os_ver), "%s", tmp); }
    } else if (caps.samsung) {
        fast_prop_get("ro.build.version.oneui", os_ver, sizeof(os_ver), "6.0");
    } else {
        fast_prop_get("ro.build.version.release", os_ver, sizeof(os_ver), android_ver);
    }

    long total_ram, used_ram, free_ram, total_swap, used_swap;
    read_meminfo(&total_ram, &used_ram, &free_ram, &total_swap, &used_swap);

    char scout[PROP_VALUE_MAX], spc[PROP_VALUE_MAX], memstd[PROP_VALUE_MAX], camera_adj[PROP_VALUE_MAX];
    char lmkd_psi[PROP_VALUE_MAX], lmkd_psi_comp[PROP_VALUE_MAX];
    char adaptive[PROP_VALUE_MAX], device_care[PROP_VALUE_MAX];
    fast_prop_get("persist.sys.miui_scout_enable", scout, sizeof(scout), caps.xiaomi ? "true" : "false");
    fast_prop_get("persist.sys.spc.enabled", spc, sizeof(spc), caps.xiaomi ? "true" : "false");
    fast_prop_get("persist.sys.memory_standard.enable", memstd, sizeof(memstd), caps.xiaomi ? "true" : "false");
    fast_prop_get("persist.sys.miui.camera.boost.killAdj_threshold", camera_adj, sizeof(camera_adj), caps.xiaomi ? "400:250:100:50" : "999:999:999:999");
    fast_prop_get("ro.lmk.psi_partial_stall_ms", lmkd_psi, sizeof(lmkd_psi), "35");
    fast_prop_get("ro.lmk.psi_complete_stall_ms", lmkd_psi_comp, sizeof(lmkd_psi_comp), "70");
    fast_prop_get("persist.sys.fflag.override.settings_enable_monitor_phantom_procs", adaptive, sizeof(adaptive), "true");
    fast_prop_get("persist.sys.miui_scout_enable", device_care, sizeof(device_care), "false");

    // Samsung adaptive battery via settings (we read via prop fallback, but also check settings is out of scope for prop)
    // For status, report via caps + lmkd only; adaptive is tracked via deviceidle path
    char samsung_adaptive[PROP_VALUE_MAX];
    fast_prop_get("persist.sys.samsung_adaptive_battery", samsung_adaptive, sizeof(samsung_adaptive), caps.samsung ? "true" : "false");

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

    // Availability-aware is_pacified: only count killers that exist on this device
    int available = 0, pacified = 0;

    if (caps.phantom) {
        available++;
        if (strcmp(phantom_limit, "2147483647") == 0) pacified++;
    }
    if (caps.freezer) {
        available++;
        if (strcmp(cached_freezer, "false") == 0) pacified++;
    }
    // LMKD is universal
    available++;
    if (strcmp(lmkd_psi, "250") == 0) pacified++;

    if (caps.duraspeed) {
        available++;
        if (strcmp(duraspeed_disabled, "true") == 0) pacified++;
    }
    if (caps.xiaomi) {
        available += 3;
        if (strcmp(scout, "false") == 0) pacified++;
        if (strcmp(spc, "false") == 0) pacified++;
        if (strcmp(memstd, "false") == 0) pacified++;
        if (caps.pk) {
            available++;
            if (strcmp(pk_freezer, "false") == 0) pacified++;
        }
    }
    if (caps.samsung) {
        // Samsung adaptive battery counted via universal appops + deviceidle; treat as one slot
        available++;
        // We consider it pacified if phantom/freezer/lmkd already handled + appops whitelist done (restricted==0)
        if (restricted_apps == 0) pacified++;
    }
    if (caps.transsion || caps.oplus) {
        available++;
        if (restricted_apps == 0) pacified++;
    }
    // Fallback: at least universal LMKD counts, so available never 0

    bool is_pacified = (available > 0 && pacified == available);

    printf("{\n");
    printf("  \"device\": {\"model\":\"%s\",\"android\":\"%s\",\"os\":\"%s\",\"oem\":\"%s\",\"skin\":\"%s\",\"sdk\":%d},\n",
           model, android_ver, os_ver, caps.oem, caps.skin, caps.sdk);
    printf("  \"ram\": {\"total_mb\":%ld,\"used_mb\":%ld,\"free_mb\":%ld,\"zram_total_mb\":%ld,\"zram_used_mb\":%ld},\n",
           total_ram, used_ram, free_ram, total_swap, used_swap);
    printf("  \"caps\": {\"phantom\":%s,\"freezer\":%s,\"duraspeed\":%s,\"pk\":%s,\"xiaomi\":%s,\"samsung\":%s,\"transsion\":%s,\"oplus\":%s},\n",
           caps.phantom ? "true" : "false", caps.freezer ? "true" : "false",
           caps.duraspeed ? "true" : "false", caps.pk ? "true" : "false",
           caps.xiaomi ? "true" : "false", caps.samsung ? "true" : "false",
           caps.transsion ? "true" : "false", caps.oplus ? "true" : "false");
    printf("  \"killers\": {\n");
    printf("    \"phantom_limit\": \"%s\",\n", phantom_limit);
    printf("    \"phantom_available\": %s,\n", caps.phantom ? "true" : "false");
    printf("    \"cached_freezer\": \"%s\",\n", cached_freezer);
    printf("    \"freezer_available\": %s,\n", caps.freezer ? "true" : "false");
    printf("    \"duraspeed_disabled\": %s,\n", duraspeed_disabled);
    printf("    \"duraspeed_available\": %s,\n", caps.duraspeed ? "true" : "false");
    printf("    \"scout_enable\": %s,\n", strcmp(scout, "true") == 0 ? "true" : "false");
    printf("    \"scout_available\": %s,\n", caps.xiaomi ? "true" : "false");
    printf("    \"spc_enable\": %s,\n", strcmp(spc, "true") == 0 ? "true" : "false");
    printf("    \"spc_available\": %s,\n", caps.xiaomi ? "true" : "false");
    printf("    \"memory_standard_enable\": %s,\n", strcmp(memstd, "true") == 0 ? "true" : "false");
    printf("    \"memstd_available\": %s,\n", caps.xiaomi ? "true" : "false");
    printf("    \"camera_boost_adj\": \"%s\",\n", camera_adj);
    printf("    \"camera_available\": %s,\n", caps.xiaomi ? "true" : "false");
    printf("    \"pk_freezer\": \"%s\",\n", pk_freezer);
    printf("    \"pk_available\": %s,\n", caps.pk ? "true" : "false");
    printf("    \"lmkd_psi_partial\": \"%s\",\n", lmkd_psi);
    printf("    \"lmkd_psi_complete\": \"%s\",\n", lmkd_psi_comp);
    printf("    \"lmkd_available\": true,\n");
    printf("    \"adaptive_available\": %s\n", (caps.samsung || caps.oplus || caps.transsion) ? "true" : "false");
    printf("  },\n");
    printf("  \"stats\": {\n");
    printf("    \"total_apps\": %d,\n", total_apps);
    printf("    \"restricted_apps\": %d,\n", restricted_apps);
    printf("    \"unrestricted_apps\": %d,\n", total_apps > restricted_apps ? total_apps - restricted_apps : 0);
    printf("    \"whitelisted_apps\": %d,\n", whitelisted_apps);
    printf("    \"available_killers\": %d,\n", available);
    printf("    \"pacified_killers\": %d,\n", pacified);
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

static void caps_for_pacify(caps_t *caps) {
    detect_oem_caps(caps);
    load_caps_file(caps);
}

static void cmd_pacify(void) {
    save_state_pacified(true);

    caps_t caps;
    caps_for_pacify(&caps);

    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else if (access("/data/adb/ap/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ap/bin/resetprop");
    else if (access("/data/adb/magisk/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/magisk/resetprop");
    else strcpy(resetprop, "resetprop");

    char cmd[512];
    // Universal LMKD relax
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 250", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_complete_stall_ms 700", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s persist.sys.fflag.override.settings_enable_monitor_phantom_procs false", resetprop); system(cmd);

    // Xiaomi-only
    if (caps.xiaomi) {
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable false", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '999:999:999:999'", resetprop); system(cmd);
    }

    // Async batch: capability-gated
    char batch[4096];
    batch[0] = '\0';
    strcat(batch, "( ");

    if (caps.phantom) {
        strcat(batch, "device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1; ");
        strcat(batch, "settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1; ");
    }
    if (caps.freezer) {
        strcat(batch, "device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1; ");
        strcat(batch, "settings put global cached_apps_freezer disabled >/dev/null 2>&1; ");
    }
    if (caps.duraspeed) {
        strcat(batch, "pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1; ");
        strcat(batch, "settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1; ");
    }
    if (caps.xiaomi && caps.pk) {
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where \"configureName='miui_standby'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:0 --where \"configureName='k_policy'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1; ");
    }
    if (caps.samsung) {
        strcat(batch, "settings put global adaptive_battery_management_enabled 0 >/dev/null 2>&1; ");
        strcat(batch, "settings put global app_standby_enabled 0 >/dev/null 2>&1; ");
    }
    if (caps.oplus || caps.transsion) {
        // OPlus/Transsion often use gms standby + appops path; no extra settings needed beyond appops
        strcat(batch, "settings put global app_standby_enabled 0 >/dev/null 2>&1; ");
    }

    // Universal: deviceidle + appops (all OEMs)
    strcat(batch, "for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do ");
    strcat(batch, "  dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1; ");
    strcat(batch, "  cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1; ");
    strcat(batch, "  cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1; ");
    strcat(batch, "done ");

    strcat(batch, ") &");
    system(batch);

    printf("{\"status\":\"ok\",\"action\":\"pacify\",\"oem\":\"%s\"}\n", caps.oem);
}

static void cmd_restore_stock(void) {
    save_state_pacified(false);

    caps_t caps;
    caps_for_pacify(&caps);

    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else if (access("/data/adb/ap/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ap/bin/resetprop");
    else if (access("/data/adb/magisk/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/magisk/resetprop");
    else strcpy(resetprop, "resetprop");

    char cmd[512];
    // Universal LMKD restore
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_partial_stall_ms 35", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.psi_complete_stall_ms 70", resetprop); system(cmd);
    snprintf(cmd, sizeof(cmd), "%s ro.lmk.kill_timeout_ms 100", resetprop); system(cmd);

    if (caps.xiaomi) {
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui_scout_enable true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.spc.enabled true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.memory_standard.enable true", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.sys.miui.camera.boost.killAdj_threshold '400:250:100:50'", resetprop); system(cmd);
    }
    if (caps.duraspeed) {
        snprintf(cmd, sizeof(cmd), "%s persist.vendor.duraspeed.app.on 1", resetprop); system(cmd);
        snprintf(cmd, sizeof(cmd), "%s persist.vendor.duraspeed.support 1", resetprop); system(cmd);
    }

    char batch[4096];
    batch[0] = '\0';
    strcat(batch, "( ");

    if (caps.phantom) {
        strcat(batch, "device_config delete activity_manager max_phantom_processes >/dev/null 2>&1; ");
        strcat(batch, "settings delete global settings_enable_monitor_phantom_procs >/dev/null 2>&1; ");
    }
    if (caps.freezer) {
        strcat(batch, "device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1; ");
        strcat(batch, "settings delete global cached_apps_freezer >/dev/null 2>&1; ");
    }
    if (caps.duraspeed) {
        strcat(batch, "pm enable com.mediatek.duraspeed >/dev/null 2>&1; ");
        strcat(batch, "settings put global setting.duraspeed.enabled 1 >/dev/null 2>&1; ");
        strcat(batch, "settings put system duraspeed_enabled 1 >/dev/null 2>&1; ");
    }
    if (caps.xiaomi && caps.pk) {
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='FrozenControlNewStatus'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where \"configureName='miui_standby'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:896 --where \"configureName='k_policy'\" >/dev/null 2>&1; ");
        strcat(batch, "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:miuiAuto >/dev/null 2>&1; ");
    }
    if (caps.samsung) {
        strcat(batch, "settings put global adaptive_battery_management_enabled 1 >/dev/null 2>&1; ");
        strcat(batch, "settings put global app_standby_enabled 1 >/dev/null 2>&1; ");
    }
    if (caps.oplus || caps.transsion) {
        strcat(batch, "settings put global app_standby_enabled 1 >/dev/null 2>&1; ");
    }

    strcat(batch, ") &");
    system(batch);

    printf("{\"status\":\"ok\",\"action\":\"restore_stock\",\"oem\":\"%s\"}\n", caps.oem);
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
    caps_t caps;
    caps_for_pacify(&caps);
    // On non-Xiaomi, userTable doesn't exist - fall back to appops only
    if (caps.xiaomi && caps.pk) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:%s --where \"pkgName='%s'\" >/dev/null 2>&1 &", mode, pkg);
        system(cmd);
    } else {
        // Generic path: just ensure appops allow
        char cmd[512];
        if (strcmp(mode, "noRestrict") == 0) {
            snprintf(cmd, sizeof(cmd), "(cmd appops set %s RUN_IN_BACKGROUND allow >/dev/null 2>&1; cmd appops set %s RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1) &", pkg, pkg);
            system(cmd);
        } else {
            snprintf(cmd, sizeof(cmd), "cmd appops set %s RUN_IN_BACKGROUND ignore >/dev/null 2>&1 &", pkg);
            system(cmd);
        }
    }
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
    caps_t caps;
    caps_for_pacify(&caps);
    if (caps.xiaomi && caps.pk) {
        system("content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1 &");
    } else {
        // Generic: whitelist all via deviceidle
        system("(for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1; cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1; done) &");
    }
    printf("{\"status\":\"ok\",\"action\":\"set_all_unrestricted\"}\n");
}

static void cmd_rebuild_apps(void) {
    // Fast native generator - no per-pkg PowerKeeper queries, just pm + deviceidle
    FILE *tmp = fopen(APPS_FILE ".tmp", "w");
    if (!tmp) { printf("{\"error\":\"write_fail\"}\n"); return; }

    // Get user pkgs set via pm list packages -3
    FILE *fp = popen("pm list packages -3 2>/dev/null | cut -d: -f2", "r");
    char user_pkgs[8192] = {0};
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            size_t l = strlen(line);
            while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r')) line[--l] = '\0';
            if (l == 0) continue;
            strncat(user_pkgs, ",", sizeof(user_pkgs) - strlen(user_pkgs) - 1);
            strncat(user_pkgs, line, sizeof(user_pkgs) - strlen(user_pkgs) - 1);
            strncat(user_pkgs, ",", sizeof(user_pkgs) - strlen(user_pkgs) - 1);
        }
        pclose(fp);
    }

    // Get whitelist dump once
    char wl_out[16384] = {0};
    fp = popen("dumpsys deviceidle whitelist 2>/dev/null", "r");
    if (fp) {
        fread(wl_out, 1, sizeof(wl_out) - 1, fp);
        pclose(fp);
    }

    fprintf(tmp, "[");
    bool first = true;

    fp = popen("pm list packages 2>/dev/null | cut -d: -f2", "r");
    if (!fp) { fclose(tmp); printf("{\"error\":\"pm_fail\"}\n"); return; }

    char pkg[256];
    while (fgets(pkg, sizeof(pkg), fp)) {
        size_t l = strlen(pkg);
        while (l > 0 && (pkg[l-1] == '\n' || pkg[l-1] == '\r')) pkg[--l] = '\0';
        if (l == 0) continue;

        char needle[512];
        snprintf(needle, sizeof(needle), ",%s,", pkg);
        bool is_user = strstr(user_pkgs, needle) != NULL;
        // whitelist: check for ",pkg," in dumpsys output (format: type,pkg,uid)
        bool whitelisted = strstr(wl_out, needle) != NULL;

        if (!first) fprintf(tmp, ",");
        first = false;
        fprintf(tmp, "{\"pkg\":\"%s\",\"bgControl\":\"noRestrict\",\"whitelisted\":%s,\"isUser\":%s}",
                pkg, whitelisted ? "true" : "false", is_user ? "true" : "false");
    }
    pclose(fp);

    fprintf(tmp, "]");
    fclose(tmp);
    rename(APPS_FILE ".tmp", APPS_FILE);
    printf("{\"status\":\"ok\",\"action\":\"rebuild_apps\"}\n");
}

static void cmd_toggle(const char *key, const char *val) {
    if (!key || !val) {
        printf("{\"error\":\"missing_params\"}\n");
        return;
    }
    bool enable = (strcmp(val, "1") == 0 || strcmp(val, "true") == 0);
    caps_t caps;
    caps_for_pacify(&caps);

    char resetprop[128];
    if (access("/data/adb/ksu/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ksu/bin/resetprop");
    else if (access("/data/adb/ap/bin/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/ap/bin/resetprop");
    else if (access("/data/adb/magisk/resetprop", X_OK) == 0) strcpy(resetprop, "/data/adb/magisk/resetprop");
    else strcpy(resetprop, "resetprop");

    // Availability guard: return skipped instead of silently failing
    if (strcmp(key, "scout") == 0 && !caps.xiaomi) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "spc") == 0 && !caps.xiaomi) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "memstd") == 0 && !caps.xiaomi) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "camera") == 0 && !caps.xiaomi) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "pk_freezer") == 0 && !caps.pk) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "duraspeed") == 0 && !caps.duraspeed) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "phantom") == 0 && !caps.phantom) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "freezer") == 0 && !caps.freezer) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }
    if (strcmp(key, "adaptive") == 0 && !(caps.samsung || caps.oplus || caps.transsion)) {
        printf("{\"status\":\"skipped\",\"key\":\"%s\",\"reason\":\"not_available_on_this_device\",\"oem\":\"%s\"}\n", key, caps.oem);
        return;
    }

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
    } else if (strcmp(key, "adaptive") == 0) {
        if (enable) {
            system("(settings put global adaptive_battery_management_enabled 0 >/dev/null 2>&1; settings put global app_standby_enabled 0 >/dev/null 2>&1) &");
        } else {
            system("(settings put global adaptive_battery_management_enabled 1 >/dev/null 2>&1; settings put global app_standby_enabled 1 >/dev/null 2>&1) &");
        }
    }

    printf("{\"status\":\"ok\",\"key\":\"%s\",\"enabled\":%s,\"oem\":\"%s\"}\n", key, enable ? "true" : "false", caps.oem);
}

static void cmd_logs(void) {
    caps_t caps;
    caps_for_pacify(&caps);
    // Tailor log filter: xiaomi logs are noisy, samsung logs different tags
    const char *filter = "grep -iE '(kill|lmkd|am_kill|lowmemory|powerkeeper|scout|phantom| AdaptiveBattery|GOS|DeviceCare)' | tail -n 40";
    if (caps.samsung) filter = "grep -iE '(kill|lmkd|am_kill|lowmemory|AdaptiveBattery|GOS|DeviceCare|phantom)' | tail -n 40";
    else if (caps.xiaomi) filter = "grep -iE '(kill|lmkd|am_kill|lowmemory|powerkeeper|scout|phantom)' | tail -n 40";

    char logcmd[512];
    snprintf(logcmd, sizeof(logcmd), "logcat -d -b system -b main -b events -t 1200 2>/dev/null | %s", filter);

    FILE *fp = popen(logcmd, "r");
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
    } else if (strcmp(action, "rebuild_apps") == 0) {
        cmd_rebuild_apps();
    } else if (strcmp(action, "logs") == 0) {
        cmd_logs();
    } else {
        printf("{\"error\":\"unknown_command\"}\n");
        return 1;
    }

    return 0;
}
