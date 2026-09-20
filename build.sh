#!/system/bin/sh
# DontKillMyApp Build Pipeline
# Copyright (C) 2026 @itswill00
# Licensed under the GNU General Public License v3.0

set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

DEPLOY=false
CLEAN=false
CUSTOM_OUTPUT=""

while [ $# -gt 0 ]; do
    case "$1" in
        -d|--deploy)
            DEPLOY=true
            shift
            ;;
        -o|--output)
            CUSTOM_OUTPUT="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -h|--help)
            echo "Usage: ./build.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --deploy       Deploy module directly to /data/adb/modules/donykillmyapp"
            echo "  -o, --output DIR   Specify custom output directory for zip releases"
            echo "  -c, --clean        Clean build caches before build"
            echo "  -h, --help         Show this help information"
            exit 0
            ;;
        *)
            echo "error: unrecognized option '$1' (use -h for help)"
            exit 1
            ;;
    esac
done

if [ ! -f "$PROJECT_DIR/module.prop" ]; then
    echo "error: module.prop not found in $PROJECT_DIR"
    exit 1
fi

VERSION=$(grep '^version=' "$PROJECT_DIR/module.prop" | cut -d= -f2)
VERSION_CODE=$(grep '^versionCode=' "$PROJECT_DIR/module.prop" | cut -d= -f2)

if [ -n "$CUSTOM_OUTPUT" ]; then
    OUTPUT_DIR="$CUSTOM_OUTPUT"
else
    OUTPUT_DIR="${PROJECT_DIR}/releases"
fi
mkdir -p "$OUTPUT_DIR"

ZIP_NAME="DontKillMyApp-${VERSION}-b${VERSION_CODE}-Universal.zip"

echo "=========================================="
echo "  DontKillMyApp Build Pipeline"
echo "  Version: ${VERSION} (b${VERSION_CODE})"
echo "  Target:  ${OUTPUT_DIR}/${ZIP_NAME}"
echo "=========================================="

if [ "$CLEAN" = "true" ]; then
    echo "cleaning build artifacts..."
    rm -rf webui/dist webroot/index.html system/bin/libdonykillmyapp.so releases
fi

# 1. Compile native C engine
echo "compiling native c engine..."
mkdir -p system/bin
clang -O3 -Wall -Wextra src/main.c -o system/bin/libdonykillmyapp.so
strip system/bin/libdonykillmyapp.so 2>/dev/null || true
chmod 755 system/bin/libdonykillmyapp.so

# 2. Build single-file WebUI
if [ -d "$PROJECT_DIR/webui" ]; then
    echo "building single-file webui bundle..."
    (cd "$PROJECT_DIR/webui" && node ./node_modules/vite/bin/vite.js build)
    mkdir -p "$PROJECT_DIR/webroot"
    cp -f "$PROJECT_DIR/webui/dist/index.html" "$PROJECT_DIR/webroot/index.html"
    chmod 644 "$PROJECT_DIR/webroot/index.html"
fi

# 3. Package module zip
echo "packaging module zip..."
EXTRA_FILES=""
[ -f "$PROJECT_DIR/apps.json" ] && EXTRA_FILES="apps.json"

zip -qr9 "$OUTPUT_DIR/$ZIP_NAME" \
    module.prop \
    system.prop \
    post-fs-data.sh \
    service.sh \
    action.sh \
    customize.sh \
    uninstall.sh \
    webroot \
    system \
    $EXTRA_FILES \
    -x "webui/*" -x "src/*" -x "*.git*" -x "releases/*"

# Create symlink or copy to latest
cp -f "$OUTPUT_DIR/$ZIP_NAME" "$OUTPUT_DIR/DontKillMyApp-latest.zip"
cp -f "$OUTPUT_DIR/$ZIP_NAME" /sdcard/DontKillMyApp-v1.0.0.zip 2>/dev/null || true
echo "build completed: $OUTPUT_DIR/$ZIP_NAME"

    # 4. Deploy if requested
    if [ "$DEPLOY" = "true" ]; then
        echo "deploying to /data/adb/modules/donykillmyapp..."
        su -c "
            mkdir -p /data/adb/modules/donykillmyapp
            cp -af '$PROJECT_DIR/module.prop' /data/adb/modules/donykillmyapp/
            cp -af '$PROJECT_DIR/system.prop' /data/adb/modules/donykillmyapp/
            cp -af '$PROJECT_DIR/post-fs-data.sh' /data/adb/modules/donykillmyapp/
            cp -af '$PROJECT_DIR/service.sh' /data/adb/modules/donykillmyapp/
            cp -af '$PROJECT_DIR/action.sh' /data/adb/modules/donykillmyapp/
            cp -af '$PROJECT_DIR/uninstall.sh' /data/adb/modules/donykillmyapp/
            [ -f '$PROJECT_DIR/apps.json' ] && cp -af '$PROJECT_DIR/apps.json' /data/adb/modules/donykillmyapp/
            cp -rf '$PROJECT_DIR/system' /data/adb/modules/donykillmyapp/
            cp -rf '$PROJECT_DIR/webroot' /data/adb/modules/donykillmyapp/
            chmod 755 /data/adb/modules/donykillmyapp/*.sh 2>/dev/null || true
            chmod 755 /data/adb/modules/donykillmyapp/system/bin/* 2>/dev/null || true
            chmod 755 /data/adb/modules/donykillmyapp/webroot /data/adb/modules/donykillmyapp/system /data/adb/modules/donykillmyapp/system/bin 2>/dev/null || true
            chmod 644 /data/adb/modules/donykillmyapp/webroot/* 2>/dev/null || true
            [ -f /data/adb/modules/donykillmyapp/apps.json ] && chmod 644 /data/adb/modules/donykillmyapp/apps.json
            chown -R root:root /data/adb/modules/donykillmyapp
            chcon -R u:object_r:system_file:s0 /data/adb/modules/donykillmyapp 2>/dev/null || true
        "
        echo "deployment successful!"
    fi
