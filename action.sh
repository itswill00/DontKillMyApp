#!/system/bin/sh
MODDIR="${0%/*}"
BIN="$MODDIR/system/bin/libdonykillmyapp.so"

if [ -x "$BIN" ]; then
    # Use native engine for OEM-aware status if available
    "$BIN" status 2>/dev/null | grep -E "model|oem|skin|phantom|freezer|scout|spc|lmkd|is_pacified" 2>/dev/null
    if [ $? -ne 0 ]; then
        # Fallback formatting if grep fails
        "$BIN" status 2>/dev/null
    fi
    echo ""
    echo "[*] Triggering capability-aware pacify..."
    "$BIN" pacify 2>/dev/null
    echo "[*] Result:"
    "$BIN" status 2>/dev/null | grep -E "is_pacified|available_killers|pacified_killers" 2>/dev/null
else
    echo "[*] Checking system state (fallback)..."
    SDK="$(getprop ro.build.version.sdk 2>/dev/null)"
    if [ -n "$SDK" ] && [ "$SDK" -ge 31 ] 2>/dev/null; then
        PHANTOM=$(device_config get activity_manager max_phantom_processes 2>/dev/null)
        echo "  Phantom limit: ${PHANTOM:-32}"
        device_config put activity_manager max_phantom_processes 2147483647 >/dev/null 2>&1 && echo "  -> set 2147483647"
    fi
    if [ -n "$SDK" ] && [ "$SDK" -ge 34 ] 2>/dev/null; then
        FREEZER=$(device_config get activity_manager_native_boot use_freezer 2>/dev/null)
        echo "  Freezer:       ${FREEZER:-true}"
        device_config put activity_manager_native_boot use_freezer false >/dev/null 2>&1
    fi
    if pm list packages 2>/dev/null | grep -q "com.mediatek.duraspeed"; then
        pm disable-user --user 0 com.mediatek.duraspeed >/dev/null 2>&1 && echo "  DuraSpeed disabled"
    fi
    if [ -n "$(getprop ro.mi.os.version.name 2>/dev/null)$(getprop ro.miui.ui.version.name 2>/dev/null)" ]; then
        content query --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --projection configureName >/dev/null 2>&1
        if [ $? -eq 0 ]; then
            content update --uri content://com.miui.powerkeeper.configure/GlobalFeatureTable --bind configureParam:s:false --where "configureName='FrozenControlNewStatus'" >/dev/null 2>&1
            content update --uri content://com.miui.powerkeeper.configure/userTable --bind bgControl:s:noRestrict >/dev/null 2>&1
            echo "  PowerKeeper pacified"
        fi
    fi
    TOTAL_WHITELISTED=0
    for pkg in $(pm list packages -3 2>/dev/null | cut -d: -f2); do
        [ -z "$pkg" ] && continue
        dumpsys deviceidle whitelist +$pkg >/dev/null 2>&1
        cmd appops set $pkg RUN_IN_BACKGROUND allow >/dev/null 2>&1
        TOTAL_WHITELISTED=$((TOTAL_WHITELISTED + 1))
    done
    echo "[+] Done ($TOTAL_WHITELISTED apps whitelisted)."
fi
