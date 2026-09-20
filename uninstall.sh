#!/system/bin/sh
# DontKillMyApp - universal uninstall (capability-aware restore)
# Copyright (C) 2026 @itswill00

BIN="/data/adb/modules/donykillmyapp/system/bin/libdonykillmyapp.so"
if [ -x "$BIN" ]; then
    "$BIN" restore_stock >/dev/null 2>&1
else
    # Fallback manual restore (only where available)
    SDK="$(getprop ro.build.version.sdk 2>/dev/null)"
    [ -n "$SDK" ] && [ "$SDK" -ge 31 ] 2>/dev/null && device_config delete activity_manager max_phantom_processes >/dev/null 2>&1
    [ -n "$SDK" ] && [ "$SDK" -ge 34 ] 2>/dev/null && device_config delete activity_manager_native_boot use_freezer >/dev/null 2>&1
    pm enable com.mediatek.duraspeed >/dev/null 2>&1
    if [ -n "$(getprop ro.mi.os.version.name 2>/dev/null)$(getprop ro.miui.ui.version.name 2>/dev/null)" ]; then
        content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:true --where "configureName='FrozenControlNewStatus'" >/dev/null 2>&1
        content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:miuiAuto >/dev/null 2>&1
    fi
    if pm list packages 2>/dev/null | grep -q "com.samsung.android.lool"; then
        settings put global adaptive_battery_management_enabled 1 >/dev/null 2>&1
        settings put global app_standby_enabled 1 >/dev/null 2>&1
    fi
fi
rm -rf /data/adb/modules/donykillmyapp
