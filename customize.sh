SKIPUNZIP=0

ui_print "- Installing DontKillMyApp (Universal)..."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/action.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755
[ -f "$MODPATH/system/bin/libdonykillmyapp.so" ] && set_perm "$MODPATH/system/bin/libdonykillmyapp.so" 0 0 0755

# Detect chipset for log only (service.sh does the real gating)
MODEL="$(getprop ro.product.model 2>/dev/null)"
MANUF="$(getprop ro.product.manufacturer 2>/dev/null)"
ANDROID="$(getprop ro.build.version.release 2>/dev/null)"
SDK="$(getprop ro.build.version.sdk 2>/dev/null)"
ui_print "  Device: $MANUF $MODEL · Android $ANDROID (SDK $SDK)"

RESETPROP="/data/adb/ksu/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/ap/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/magisk/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="$(which resetprop 2>/dev/null)"

# Only universal props here; OEM-specific is in post-fs-data.sh (conditional) + service.sh
if [ -x "$RESETPROP" ]; then
    "$RESETPROP" ro.lmk.psi_partial_stall_ms 250 2>/dev/null || true
    # Scout/SPC only if Xiaomi - check live
    if [ -n "$(getprop ro.mi.os.version.name 2>/dev/null)$(getprop ro.miui.ui.version.name 2>/dev/null)" ]; then
        "$RESETPROP" persist.sys.miui_scout_enable false 2>/dev/null || true
        "$RESETPROP" persist.sys.spc.enabled false 2>/dev/null || true
        ui_print "  HyperOS/MIUI detected - Scout/SPC queued"
    fi
fi

# Install-time pacify for phantom/freezer (AOSP 12+/14+) if available
if [ "$SDK" -ge 31 ] 2>/dev/null; then
    device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1 && ui_print "  Phantom killer disabled"
fi
if [ "$SDK" -ge 34 ] 2>/dev/null; then
    device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1 && ui_print "  Cached freezer disabled"
fi

chcon -R u:object_r:system_file:s0 "$MODPATH" 2>/dev/null || true

ui_print "- Installation complete. Reboot recommended."
