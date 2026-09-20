#!/system/bin/sh
RESETPROP="/data/adb/ksu/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/ap/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/magisk/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="$(which resetprop 2>/dev/null)"

if [ -x "$RESETPROP" ]; then
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

    "$RESETPROP" persist.vendor.duraspeed.app.on 0
    "$RESETPROP" persist.vendor.duraspeed.support 0

    "$RESETPROP" ro.lmk.psi_partial_stall_ms 250
    "$RESETPROP" ro.lmk.psi_complete_stall_ms 700
    "$RESETPROP" ro.lmk.thrashing_limit 100
    "$RESETPROP" ro.lmk.thrashing_limit_decay 10
    "$RESETPROP" ro.lmk.kill_timeout_ms 250
    "$RESETPROP" persist.device_config.lmkd_native.thrashing_limit_critical 600
fi

