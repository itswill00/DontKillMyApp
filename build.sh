#!/system/bin/sh
# DontKillMyApp build - minimal
set -e
D="$(cd "$(dirname "$0")" && pwd)"; cd "$D"
O="releases"; DEPLOY=""
while [ $# -gt 0 ]; do case "$1" in
  -d) DEPLOY=1 ;;
  -c) rm -rf webui/dist webroot/index.html system/bin/libdonykillmyapp.so releases ;;
  -o) O="$2"; shift ;;
  -h) echo "usage: $0 [-d] [-c] [-o DIR]"; exit 0 ;;
  *) echo "unknown: $1"; exit 1 ;;
esac; shift; done

[ -f module.prop ] || { echo "module.prop missing"; exit 1; }
V="$(grep '^version=' module.prop | cut -d= -f2)"
VC="$(grep '^versionCode=' module.prop | cut -d= -f2)"
ZIP="DontKillMyApp-${V}-b${VC}-Universal.zip"
mkdir -p system/bin webroot "$O"
echo "build $V b$VC -> $O/$ZIP"

clang -O3 -Wall -Wextra src/main.c -o system/bin/libdonykillmyapp.so
strip system/bin/libdonykillmyapp.so 2>/dev/null || true
chmod 755 system/bin/libdonykillmyapp.so

if [ -d webui ]; then
  (cd webui && node ./node_modules/vite/bin/vite.js build)
  cp webui/dist/index.html webroot/index.html
  chmod 644 webroot/index.html
fi

# package (apps.json is [] placeholder, safe to always include)
zip -qr9 "$O/$ZIP" module.prop system.prop post-fs-data.sh service.sh action.sh customize.sh uninstall.sh webroot system apps.json -x "webui/*" "src/*" "*.git*" "releases/*" 2>/dev/null \
|| zip -qr9 "$O/$ZIP" module.prop system.prop post-fs-data.sh service.sh action.sh customize.sh uninstall.sh webroot system -x "webui/*" "src/*" "*.git*" "releases/*"

cp -f "$O/$ZIP" "$O/DontKillMyApp-latest.zip"
cp -f "$O/$ZIP" /sdcard/DontKillMyApp-v1.0.0.zip 2>/dev/null || true
echo "done $O/$ZIP ($(du -h "$O/$ZIP" | cut -f1))"

[ -z "$DEPLOY" ] && exit 0
echo "deploy to /data/adb/modules/donykillmyapp"
su -c "
  M=/data/adb/modules/donykillmyapp
  mkdir -p \$M/system/bin \$M/webroot
  for f in module.prop system.prop post-fs-data.sh service.sh action.sh uninstall.sh apps.json; do
    [ -f '$D/\$f' ] && cp -f '$D/\$f' \$M/ 2>/dev/null || true
  done
  cp -rf '$D/system' \$M/ 2>/dev/null || true
  cp -rf '$D/webroot' \$M/ 2>/dev/null || true
  chmod 755 \$M/*.sh \$M/system/bin/* 2>/dev/null || true
  chmod 644 \$M/webroot/* \$M/*.prop \$M/apps.json 2>/dev/null || true
  chown -R root:root \$M 2>/dev/null || true
  chcon -R u:object_r:system_file:s0 \$M 2>/dev/null || true
  echo deployed
"
