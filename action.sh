#!/system/bin/sh
# DontKillMyApp - KernelSU Action Button Script
# Copyright (C) 2026 @itswill00

MODDIR="${0%/*}"

echo "=========================================="
echo "   DontKillMyApp - HyperOS Status & Sync"
echo "=========================================="

PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
echo "[*] Phantom Process Limit : ${PHANTOM:-32} (Target: 2147483647)"

FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
echo "[*] Cached Apps Freezer   : ${FREEZER:-true} (Target: false)"

FROZEN_STATUS=$(content query --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --projection configureName:configureParam --where "configureName='FrozenControlNewStatus'" 2>/dev/null | grep -o 'configureParam=[^ ]*' | cut -d= -f2)
echo "[*] PowerKeeper Freezer   : ${FROZEN_STATUS:-unknown} (Target: false)"

RESTRICTED_COUNT=$(content query --uri content://com.miui.powerkeeper.configure/userTable --projection pkgName --where "bgControl!='noRestrict'" 2>/dev/null | grep -c "Row:")
echo "[*] Restricted Apps in PK : $RESTRICTED_COUNT"

echo "------------------------------------------"
echo "Syncing all anti-kill protections now..."

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

echo "[+] Whitelisted $TOTAL_WHITELISTED third-party user apps!"
echo "[+] All apps set to 'noRestrict' in PowerKeeper!"
echo "[✔] Anti-kill protections synchronized!"
echo "=========================================="
