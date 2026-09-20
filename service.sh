#!/system/bin/sh
MODDIR="${0%/*}"

while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 3
done

sleep 5

# Phantom process limit
device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false >/dev/null 2>&1

# Cached apps freezer
device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
settings put global cached_apps_freezer disabled >/dev/null 2>&1

# DuraSpeed
pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1
settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1
settings put system duraspeed_enabled 0 >/dev/null 2>&1

# PowerKeeper
RETRIES=0
while [ $RETRIES -lt 20 ]; do
    if content query --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --projection configureName >/dev/null 2>&1; then
        break
    fi
    sleep 2
    RETRIES=$((RETRIES + 1))
done

if [ $RETRIES -lt 20 ]; then
    content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where "configureName='FrozenControlNewStatus'" >/dev/null 2>&1
    content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where "configureName='miui_standby'" >/dev/null 2>&1
    content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:0 --where "configureName='k_policy'" >/dev/null 2>&1
    content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1
fi

pacify_user_apps() {
    for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do
        [ -z "$pkg" ] && continue
        dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1
        cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1
        cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1
    done
}
pacify_user_apps

TOTAL_COUNT=$(pm list packages 2>/dev/null | wc -l)
USER_COUNT=$(pm list packages -3 2>/dev/null | wc -l)

cat << EOF > "$MODDIR/state.json"
{
  "phantom_limit": "2147483647",
  "cached_freezer": "false",
  "duraspeed_disabled": true,
  "pk_freezer": "false",
  "total_apps": ${TOTAL_COUNT:-340},
  "restricted_apps": 0,
  "whitelisted_apps": ${USER_COUNT:-67}
}
EOF
chmod 644 "$MODDIR/state.json" 2>/dev/null || true

# Periodic guardian
(
    while true; do
        sleep 120

        CUR_PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
        if [ "$CUR_PHANTOM" != "2147483647" ]; then
            device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
            settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
        fi

        CUR_FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
        if [ "$CUR_FREEZER" != "false" ]; then
            device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
            settings put global cached_apps_freezer disabled >/dev/null 2>&1
        fi

        content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict --where "bgControl!='noRestrict'" >/dev/null 2>&1
        pacify_user_apps
    done
) &

