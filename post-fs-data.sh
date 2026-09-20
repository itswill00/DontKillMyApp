#!/system/bin/sh
# DontKillMyApp - universal post-fs-data (OEM-aware resetprop)
# Copyright (C) 2026 @itswill00

RESETPROP="/data/adb/ksu/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/ap/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/magisk/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="$(which resetprop 2>/dev/null)"

[ ! -x "$RESETPROP" ] && exit 0

# ---------- OEM detection (post-fs-data: pm not ready yet, use props only) ----------
MANUF="$(getprop ro.product.manufacturer 2>/dev/null | tr '[:upper:]' '[:lower:]')"
IS_XIAOMI=false
if [ -n "$(getprop ro.mi.os.version.name 2>/dev/null)" ] || [ -n "$(getprop ro.miui.ui.version.name 2>/dev/null)" ]; then
    IS_XIAOMI=true
fi
case "$MANUF" in *xiaomi*|*redmi*|*poco*) IS_XIAOMI=true ;; esac

IS_MTK=false
if [ -n "$(getprop ro.mediatek.platform 2>/dev/null)" ] \
   || [ -n "$(getprop ro.mediatek.version.release 2>/dev/null)" ] \
   || [ -n "$(getprop ro.soc.manufacturer 2>/dev/null | grep -i mediatek 2>/dev/null)" ]; then
    IS_MTK=true
fi

# ---------- Universal LMKD relax (all OEMs, all Android versions) ----------
# Lower PSI sensitivity prevents premature kills under brief spikes. Harmless on every device.
"$RESETPROP" ro.lmk.psi_partial_stall_ms 250
"$RESETPROP" ro.lmk.psi_complete_stall_ms 700
"$RESETPROP" ro.lmk.thrashing_limit 100
"$RESETPROP" ro.lmk.thrashing_limit_decay 10
"$RESETPROP" ro.lmk.kill_timeout_ms 250
"$RESETPROP" persist.device_config.lmkd_native.thrashing_limit_critical 600

# Phantom monitor disable flag - AOSP 12+ (no-op on older, safe everywhere)
"$RESETPROP" persist.sys.fflag.override.settings_enable_monitor_phantom_procs false

# ---------- Xiaomi-only (MIUI/HyperOS) ----------
if [ "$IS_XIAOMI" = true ]; then
    "$RESETPROP" persist.sys.miui_scout_enable false
    "$RESETPROP" persist.sys.miui_scout_binder_full_kill_process false
    "$RESETPROP" persist.sys.debug.enable_scout_memory_monitor false
    "$RESETPROP" persist.sys.debug.enable_scout_memory_resume false
    "$RESETPROP" persist.sys.scout_binder_gki false

    "$RESETPROP" persist.sys.spc.enabled false
    "$RESETPROP" persist.sys.spc.proc_restart_enable false
    "$RESETPROP" persist.sys.spc.protect.critical.count 100

    "$RESETPROP" persist.sys.memory_standard.enable false
    "$RESETPROP" persist.sys.memory_standard.appheap.enable false

    "$RESETPROP" persist.sys.miui.camera.boost.killAdj_threshold "999:999:999:999"
fi

# ---------- MediaTek-only ----------
if [ "$IS_MTK" = true ]; then
    "$RESETPROP" persist.vendor.duraspeed.app.on 0
    "$RESETPROP" persist.vendor.duraspeed.support 0
fi
