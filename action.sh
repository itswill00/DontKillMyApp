#!/system/bin/sh
MODDIR="${0%/*}"

echo "[*] Checking system state..."

PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
echo "  Phantom limit: ${PHANTOM:-32}"
echo "  Freezer:       ${FREEZER:-true}"

echo "[*] Applying pacification..."

device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false >/dev/null 2>&1

device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
settings put global cached_apps_freezer disabled >/dev/null 2>&1

pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1
settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1

content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where "configureName='FrozenControlNewStatus'" >/dev/null 2>&1
content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where "configureName='miui_standby'" >/dev/null 2>&1
content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:0 --where "configureName='k_policy'" >/dev/null 2>&1
content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1

TOTAL_WHITELISTED=0
for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do
    [ -z "$pkg" ] && continue
    dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1
    cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1
    cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1
    TOTAL_WHITELISTED=$((TOTAL_WHITELISTED + 1))
done

echo "[+] Done ($TOTAL_WHITELISTED apps whitelisted)."

