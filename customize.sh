SKIPUNZIP=0

ui_print "- Installing DontKillMyApp..."
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/action.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755
[ -f "$MODPATH/system/bin/libdonykillmyapp.so" ] && set_perm "$MODPATH/system/bin/libdonykillmyapp.so" 0 0 0755

RESETPROP="/data/adb/ksu/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/ap/bin/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="/data/adb/magisk/resetprop"
[ ! -x "$RESETPROP" ] && RESETPROP="$(which resetprop 2>/dev/null)"

if [ -x "$RESETPROP" ]; then
    "$RESETPROP" persist.sys.miui_scout_enable false
    "$RESETPROP" persist.sys.spc.enabled false
    "$RESETPROP" persist.sys.memory_standard.enable false
    "$RESETPROP" persist.sys.miui.camera.boost.killAdj_threshold "999:999:999:999"
    "$RESETPROP" ro.lmk.psi_partial_stall_ms 250
fi

chcon -R u:object_r:system_file:s0 "$MODPATH" 2>/dev/null || true

ui_print "- Installation complete."

