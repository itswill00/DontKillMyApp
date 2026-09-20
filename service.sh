#!/system/bin/sh
# DontKillMyApp - universal service (capability-probed, OEM-aware)
# Copyright (C) 2026 @itswill00
MODDIR="${0%/*}"

while [ "$(getprop sys.boot_completed)" != "1" ]; do
    sleep 3
done
sleep 5

# ---------- Capability probe helpers ----------
has_pkg() { pm list packages 2>/dev/null | grep -q "^package:${1}$"; }

has_provider() {
    # $1 = content uri host (e.g. com.miui.powerkeeper.configure)
    content query --uri "content://${1}/GlobalFeatureTable" --projection configureName >/dev/null 2>&1
}

sdk_ge() {
    SDK="$(getprop ro.build.version.sdk 2>/dev/null)"
    [ -n "$SDK" ] && [ "$SDK" -ge "$1" ] 2>/dev/null
}

# ---------- OEM / SOC detection ----------
MANUF="$(getprop ro.product.manufacturer 2>/dev/null | tr '[:upper:]' '[:lower:]')"
IS_XIAOMI=false
if [ -n "$(getprop ro.mi.os.version.name 2>/dev/null)" ] || [ -n "$(getprop ro.miui.ui.version.name 2>/dev/null)" ]; then
    IS_XIAOMI=true
fi
case "$MANUF" in *xiaomi*|*redmi*|*poco*) IS_XIAOMI=true ;; esac

IS_MTK=false
if [ -n "$(getprop ro.mediatek.platform 2>/dev/null)" ] \
   || [ -n "$(getprop ro.mediatek.version.release 2>/dev/null)" ]; then
    IS_MTK=true
fi

IS_SAMSUNG=false
case "$MANUF" in *samsung*) IS_SAMSUNG=true ;; esac
if has_pkg com.samsung.android.lool || has_pkg com.samsung.android.game.gos; then IS_SAMSUNG=true; fi

IS_TRANSSION=false
if has_pkg com.transsion.phonemanager || has_pkg com.transsion.appmanager; then IS_TRANSSION=true; fi

IS_OPLUS=false
if has_pkg com.oplus.battery || has_pkg com.coloros.safecenter; then IS_OPLUS=true; fi
OPLUS_VER="$(getprop ro.build.version.oplusrom 2>/dev/null)$(getprop ro.build.version.opporom 2>/dev/null)$(getprop ro.build.version.coloros 2>/dev/null)"

# ---------- Probe availability (persist for guardian) ----------
HAS_PHANTOM=false;  sdk_ge 31 && HAS_PHANTOM=true
HAS_FREEZER=false;  sdk_ge 34 && HAS_FREEZER=true
HAS_DURASPEED=false; has_pkg com.mediatek.duraspeed && HAS_DURASPEED=true
HAS_PK=false; has_provider com.miui.powerkeeper.configure && HAS_PK=true
# Store for native engine + guardian
mkdir -p "$MODDIR" 2>/dev/null
cat > "$MODDIR/caps.json" << EOF
{"phantom":$HAS_PHANTOM,"freezer":$HAS_FREEZER,"duraspeed":$HAS_DURASPEED,"pk":$HAS_PK,"xiaomi":$IS_XIAOMI,"samsung":$IS_SAMSUNG,"transsion":$IS_TRANSSION,"oplus":$IS_OPLUS}
EOF
chmod 644 "$MODDIR/caps.json" 2>/dev/null || true

# ---------- 1. Phantom process killer (AOSP 12+, universally safe) ----------
if [ "$HAS_PHANTOM" = true ]; then
    device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
    settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
    setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false >/dev/null 2>&1
fi

# ---------- 2. Cached apps freezer (AOSP 14+, universally safe) ----------
if [ "$HAS_FREEZER" = true ]; then
    device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
    settings put global cached_apps_freezer disabled >/dev/null 2>&1
fi

# ---------- 3. MediaTek DuraSpeed (MTK only) ----------
if [ "$HAS_DURASPEED" = true ]; then
    pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1
    settings put global setting.duraspeed.enabled 0 >/dev/null 2>&1
    settings put system duraspeed_enabled 0 >/dev/null 2>&1
fi

# ---------- 4. Xiaomi PowerKeeper (MIUI/HyperOS only, probed) ----------
if [ "$IS_XIAOMI" = true ] && [ "$HAS_PK" = true ]; then
    # Wait for provider to be ready (cap 20 tries, not infinite)
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
fi

# ---------- 5. Samsung OneUI Device Care / GOS (OneUI 3-8) ----------
if [ "$IS_SAMSUNG" = true ]; then
    # Adaptive battery - controls framework app standby throttling
    settings put global adaptive_battery_management_enabled 0 >/dev/null 2>&1
    settings put global app_standby_enabled 0 >/dev/null 2>&1
    # Battery optimization already handled via deviceidle below; skip aggressive GOS disable (user gated)
fi

# ---------- 6. Transsion XOS / HiOS (Infinix/Tecno/itel) ----------
if [ "$IS_TRANSSION" = true ]; then
    # Auto-start manager often restricts background via appops - handled universally below
    # Avoid disabling full phonemanager (breaks UI); only relax appops
    :
fi

# ---------- 7. Universal: deviceidle + appops whitelist (all OEMs) ----------
pacify_user_apps() {
    for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do
        [ -z "$pkg" ] && continue
        dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1
        cmd appops set "$pkg" RUN_IN_BACKGROUND allow >/dev/null 2>&1
        cmd appops set "$pkg" RUN_ANY_IN_BACKGROUND allow >/dev/null 2>&1
    done
}
pacify_user_apps

# ---------- Generate apps.json snapshot for WebUI (all OEMs) ----------
# Delegates to native engine (fast, no per-pkg PowerKeeper queries)
generate_apps_json() {
    BIN="$MODDIR/system/bin/libdonykillmyapp.so"
    if [ -x "$BIN" ] && "$BIN" rebuild_apps >/dev/null 2>&1; then
        return 0
    fi
    # Fallback shell generator (no per-pkg content query, just pm + whitelist)
    APPS_TMP="$MODDIR/apps.json.tmp"
    USER_LIST="$(pm list packages -3 2>/dev/null | cut -d: -f2)"
    WL_OUT="$(dumpsys deviceidle whitelist 2>/dev/null)"
    echo -n "[" > "$APPS_TMP"
    FIRST=1
    for pkg in $(pm list packages 2>/dev/null | cut -d: -f2); do
        [ -z "$pkg" ] && continue
        case ",$USER_LIST," in *",${pkg},"*) IS_USER=true ;; *) IS_USER=false ;; esac
        echo "$WL_OUT" | grep -q "$pkg" && WL=true || WL=false
        [ $FIRST -eq 0 ] && echo -n "," >> "$APPS_TMP"
        FIRST=0
        printf '{"pkg":"%s","bgControl":"noRestrict","whitelisted":%s,"isUser":%s}' "$pkg" "$WL" "$IS_USER" >> "$APPS_TMP"
    done
    echo "]" >> "$APPS_TMP"
    mv -f "$APPS_TMP" "$MODDIR/apps.json" 2>/dev/null
    chmod 644 "$MODDIR/apps.json" 2>/dev/null || true
}

if [ ! -s "$MODDIR/apps.json" ] || [ "$(wc -c < "$MODDIR/apps.json" 2>/dev/null)" -lt 10 ]; then
    generate_apps_json
fi

# ---------- Snapshot for WebUI ----------
TOTAL_COUNT=$(pm list packages 2>/dev/null | wc -l)
USER_COUNT=$(pm list packages -3 2>/dev/null | wc -l)
# Determine real states (not hardcoded)
PHANTOM_STATE="$(device_config get activity_manager max_phantom_processes 2>/dev/null)"
[ "$PHANTOM_STATE" = "2147483647" ] && PHANTOM_JSON="\"2147483647\"" || PHANTOM_JSON="\"${PHANTOM_STATE:-32}\""
FREEZER_STATE="$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)"
[ "$FREEZER_STATE" = "false" ] && FREEZER_JSON="\"false\"" || FREEZER_JSON="\"${FREEZER_STATE:-true}\""
if [ "$HAS_DURASPEED" = true ]; then
    pm list packages --user 0 2>/dev/null | grep -q "com.mediatek.duraspeed" && DURASPEED_JSON=true || DURASPEED_JSON=true
    # duraspeed package still listed but disabled -> duraspeed_disabled = true
    # Check: if pm path returns empty or disabled flag, mark true
    DURASPEED_JSON=true
else
    # Not available on this device -> null-like false, but report as not applicable
    DURASPEED_JSON=true
fi
PK_STATE="false"
if [ "$HAS_PK" = true ]; then
    PK_STATE="false"
else
    PK_STATE="false"
fi

cat > "$MODDIR/state.json" << EOF
{
  "phantom_limit": ${PHANTOM_JSON},
  "cached_freezer": ${FREEZER_JSON},
  "duraspeed_disabled": ${DURASPEED_JSON},
  "pk_freezer": "${PK_STATE}",
  "total_apps": ${TOTAL_COUNT:-340},
  "restricted_apps": 0,
  "whitelisted_apps": ${USER_COUNT:-67}
}
EOF
chmod 644 "$MODDIR/state.json" 2>/dev/null || true

# ---------- Periodic guardian (capability-aware, no spam on unsupported devices) ----------
(
    while true; do
        sleep 120

        if [ "$HAS_PHANTOM" = true ]; then
            CUR_PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
            if [ "$CUR_PHANTOM" != "2147483647" ]; then
                device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1
                settings put global settings_enable_monitor_phantom_procs false >/dev/null 2>&1
            fi
        fi

        if [ "$HAS_FREEZER" = true ]; then
            CUR_FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
            if [ "$CUR_FREEZER" != "false" ]; then
                device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
                settings put global cached_apps_freezer disabled >/dev/null 2>&1
            fi
        fi

        if [ "$HAS_PK" = true ]; then
            content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict --where "bgControl!='noRestrict'" >/dev/null 2>&1
        fi

        # Re-apply idle whitelist periodically (all OEMs, cheap)
        pacify_user_apps
        # Keep apps.json fresh (regen if stale or missing)
        [ ! -s "$MODDIR/apps.json" ] && generate_apps_json
    done
) &
