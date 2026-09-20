#!/system/bin/sh
# DontKillMyApp - Uninstall script
# Copyright (C) 2026 @itswill00

pm enable com.mediatek.duraspeed >/dev/null 2>&1
rm -rf /data/adb/modules/donykillmyapp
