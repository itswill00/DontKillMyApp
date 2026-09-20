#!/system/bin/sh
# DontKillMyApp - Background Watchdog & Synchronizer
# Copyright (C) 2026 @itswill00

MODDIR="${0%/*}"
LOGFILE="$MODDIR/dontkillmyapp.log"

log_info() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$LOGFILE"
}

# Wait for boot completion
while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 3
done

sleep 8
log_info "Boot completed. Initializing DontKillMyApp daemon..."

# 1. Tame Android 14 Phantom Process Killer
device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false >/dev/null 2>&1
log_info "Android 14 Phantom Process Killer disabled"

# 2. Tame Android Cached Apps Freezer
device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
settings put global cached_apps_freezer disabled >/dev/null 2>&1
log_info "Cached Apps Freezer disabled"

# 3. Disable MediaTek DuraSpeed
pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1
settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1
settings put system duraspeed_enabled 0 >/dev/null 2>&1
log_info "MediaTek DuraSpeed disabled"

# 4. Pacify PowerKeeper via ContentProvider
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
    log_info "PowerKeeper GlobalFeatureTable pacified"

    # Set all apps in PowerKeeper userTable to noRestrict
    content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1
    log_info "PowerKeeper userTable: all apps set to noRestrict"
fi

# 5. Whitelist 3rd-party user apps
pacify_user_apps() {
    for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do
        [ -z "$pkg" ] && continue
        dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1
        cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1
        cmd appops set $pkg RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1
    done
}
pacify_user_apps
log_info "Third-party user apps whitelisted"

# 6. Persistent Background Watcher (120s loop)
(
    while true; do
        sleep 120

        # Maintain Phantom Process limit
        CUR_PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
        if [ "$CUR_PHANTOM" != "2147483647" ]; then
            device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
            settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
        fi

        # Maintain Cached App Freezer disabled
        CUR_FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
        if [ "$CUR_FREEZER" != "false" ]; then
            device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
            settings put global cached_apps_freezer disabled >/dev/null 2>&1
        fi

        # Maintain PowerKeeper userTable noRestrict for newly installed apps
        content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict --where "bgControl!='noRestrict'" >/dev/null 2>&1

        # Check and whitelist newly installed user apps
        pacify_user_apps
    done
) &
log_info "Background sync loop active (PID: $!)"
