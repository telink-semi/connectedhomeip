#!/bin/bash
#
#    Copyright (c) 2026 Telink Semiconductor Co., Ltd.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

set -e

# Unified Package script for Telink TL3238X/TL7218X Matter firmware
# This script combines generate, extract, readme, and zip functionality
#
# Usage: package_telink_firmware.sh [--build] [--extract] [--readme] [--zip] [--all] [--lighting] [--switch] [--tl3238x] [--tl7218x] [target1 target2...]
#
# --build      : Build firmware targets
# --extract    : Extract firmware from build directories
# --readme     : Generate README.md
# --zip        : Create zip archive
# --all        : Perform all steps (build, extract, readme, zip)
#
# In addition to the steps above, a versioned snapshot of the release note
# working copy (docs/platforms/telink/releases/telink_release_notes.md) is
# written to docs/platforms/telink/releases/telink_release_notes_<tl_v*.tag>.md
# during the readme/zip steps, using the nearest `tl_v*` git tag as the version.
#
# Available Targets:
#   TL3238X:
#     Lighting App:
#       - build_tl3238x_default
#       - build_tl3238x_2m_flash_lzma_v1
#       - build_tl3238x_2m_flash_lzma_v2
#       - build_tl3238x_4m_dual_mode
#
#     Light Switch App:
#       - build_tl3238x_retention_default
#       - build_tl3238x_retention_lzma_v1
#       - build_tl3238x_retention_lzma_v2
#       - build_tl3238x_retention_dual_mode
#
#     Contact Sensor App:
#       - build_tl3238x_retention_v1
#       - build_tl3238x_retention_v2
#       - build_tl3238x_retention_lzma_v1
#       - build_tl3238x_retention_lzma_v2
#
#   TL7218X:
#     Lighting App:
#       - build_tl7218x_default
#       - build_tl7218x_2m_flash_lzma_v1
#       - build_tl7218x_2m_flash_lzma_v2
#
#     Light Switch App:
#       - build_tl7218x_retention_default
#       - build_tl7218x_retention_lzma_v1
#       - build_tl7218x_retention_lzma_v2
#
#     Contact Sensor App:
#       - build_tl7218x_retention_v1
#       - build_tl7218x_retention_v2
#       - build_tl7218x_retention_lzma_v1
#       - build_tl7218x_retention_lzma_v2
#
# If no arguments are specified, performs all steps for all targets.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONNECTEDHOME_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
ZEPHYR_BASE="$(cd "$CONNECTEDHOME_DIR/../zephyr" && pwd)"
EXAMPLES_DIR="$CONNECTEDHOME_DIR/examples"
LIGHTING_APP_DIR="$EXAMPLES_DIR/lighting-app/telink"
LIGHT_SWITCH_APP_DIR="$EXAMPLES_DIR/light-switch-app/telink"
CONTACT_SENSOR_APP_DIR="$EXAMPLES_DIR/contact-sensor-app/telink"

DATE=$(date +%Y%m%d_%H%M%S)

# Parse arguments
DO_BUILD=false
DO_EXTRACT=false
DO_README=false
DO_ZIP=false
GENERATE_LIGHTING=false
GENERATE_SWITCH=false
GENERATE_CONTACT=false
GENERATE_TL3238X=false
GENERATE_TL7218X=false
TARGETS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --build)
            DO_BUILD=true
            shift
            ;;
        --extract)
            DO_EXTRACT=true
            shift
            ;;
        --readme)
            DO_README=true
            shift
            ;;
        --zip)
            DO_ZIP=true
            shift
            ;;
        --all)
            DO_BUILD=true
            DO_EXTRACT=true
            DO_README=true
            DO_ZIP=true
            shift
            ;;
        --lighting)
            GENERATE_LIGHTING=true
            shift
            ;;
        --switch)
            GENERATE_SWITCH=true
            shift
            ;;
        --contact)
            GENERATE_CONTACT=true
            shift
            ;;
        --tl3238x)
            GENERATE_TL3238X=true
            shift
            ;;
        --tl7218x)
            GENERATE_TL7218X=true
            shift
            ;;
        build_tl3238x* | build_tl7218x*)
            TARGETS+=("$1")
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--build] [--extract] [--readme] [--zip] [--all] [--lighting] [--switch] [--contact] [--tl3238x] [--tl7218x] [target1 target2...]"
            exit 1
            ;;
    esac
done

# If no operation specified, do all
if [ "$DO_BUILD" = false ] && [ "$DO_EXTRACT" = false ] && [ "$DO_README" = false ] && [ "$DO_ZIP" = false ]; then
    DO_BUILD=true
    DO_EXTRACT=true
    DO_README=true
    DO_ZIP=true
fi

# If no targets specified and building, generate all
if [ "$DO_BUILD" = true ] && [ ${#TARGETS[@]} -eq 0 ]; then
    GENERATE_LIGHTING=true
    GENERATE_SWITCH=true
    GENERATE_CONTACT=true
    GENERATE_TL3238X=true
    GENERATE_TL7218X=true
elif [ "$DO_BUILD" = true ]; then
    # Check which apps are needed based on targets
    for target in "${TARGETS[@]}"; do
        if [[ "$target" == build_tl3238x_* ]] || [[ "$target" == build_tl7218x_* ]]; then
            GENERATE_LIGHTING=true
        fi
        if [[ "$target" == build_tl3238x_retention* ]] || [[ "$target" == build_tl7218x_retention* ]]; then
            # Check if it's contact sensor or light switch
            if [[ "$target" == *contact* ]]; then
                GENERATE_CONTACT=true
            else
                GENERATE_SWITCH=true
            fi
        fi
        if [[ "$target" == build_tl3238x* ]]; then
            GENERATE_TL3238X=true
        fi
        if [[ "$target" == build_tl7218x* ]]; then
            GENERATE_TL7218X=true
        fi
    done
fi

echo "Activating Matter SDK environment..."
cd "$CONNECTEDHOME_DIR"
source scripts/activate.sh

# Function to check if target should be generated
should_generate() {
    local target="$1"
    if [ ${#TARGETS[@]} -eq 0 ]; then
        return 0 # No targets specified, generate all
    fi
    for arg in "${TARGETS[@]}"; do
        if [ "$arg" = "$target" ]; then
            return 0
        fi
    done
    return 1
}

# Function to check if target should be packaged
should_package() {
    local target="$1"
    shift
    if [ $# -eq 0 ]; then
        return 0
    fi
    for arg in "$@"; do
        if [ "$arg" = "$target" ]; then
            return 0
        fi
    done
    return 1
}

# Function to find the latest build directory for a given prefix
find_latest_build_dir() {
    local app_dir="$1"
    local prefix="$2"
    local latest_dir=""

    for dir in "$app_dir/$prefix"*; do
        if [ -d "$dir" ] && [ -f "$dir/zephyr/.config" ]; then
            if [ -z "$latest_dir" ] || [ "$dir" -nt "$latest_dir" ]; then
                latest_dir="$dir"
            fi
        fi
    done

    if [ -n "$latest_dir" ]; then
        echo "$latest_dir"
    else
        echo ""
    fi
}

get_build_dir() {
    local app_dir="$1"
    local target="$2"

    if [ -d "$app_dir/$target" ]; then
        echo "$app_dir/$target"
        return
    fi

    local latest_dir="$(find_latest_build_dir "$app_dir" "$target")"
    if [ -n "$latest_dir" ]; then
        echo "$latest_dir"
    else
        echo ""
    fi
}

# Function to generate Lighting App
generate_lighting_app() {
    echo ""
    echo "=== Generating Lighting App ==="
    cd "$LIGHTING_APP_DIR"

    if [ "$GENERATE_TL3238X" = true ]; then
        if should_generate build_tl3238x_default; then
            BUILD_DIR="build_tl3238x_default_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, NO OTA/MCUBoot, NO LZMA) ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x -d "$BUILD_DIR" -- \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_2m_flash_lzma_v1; then
            BUILD_DIR="build_tl3238x_2m_flash_lzma_v1_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v1 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=1 \
                -DCONF_FILE="prj.conf;boards/tl3238x_2m_flash_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_2m_flash_lzma_v2; then
            BUILD_DIR="build_tl3238x_2m_flash_lzma_v2_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v2 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=2 \
                -DCONF_FILE="prj.conf;boards/tl3238x_2m_flash_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_4m_dual_mode; then
            BUILD_DIR="build_tl3238x_4m_dual_mode_$DATE"
            echo ""
            echo "=== TL3238X: 4MB Flash (Matter + Zigbee dual mode, no LZMA) ==="
            echo "Generate directory: $BUILD_DIR"

            # Copy lighting app specific Zigbee firmware
            if [ -f "$ZEPHYR_BASE/TL323X_FW/ZB/dual_matter_sampleLight_bleAdv_tl323x.bin" ]; then
                cp "$ZEPHYR_BASE/TL323X_FW/ZB/dual_matter_sampleLight_bleAdv_tl323x.bin" "$ZEPHYR_BASE/TL323X_FW/ZB/Zigbee-SampleDemo.bin"
                echo "Copied dual_matter_sampleLight_bleAdv_tl323x.bin to Zigbee-SampleDemo.bin"
            fi

            west build -p -b tl3238x -d "$BUILD_DIR" -- \
                -DFLASH_SIZE=4m \
                -DCONF_FILE="prj.conf;boards/tl3238x_4m_flash_dual_mode_ota.conf" \
                -DDTC_OVERLAY_FILE="boards/tl3238x_for_TL3238C-EVK40D.overlay" \
                2>&1 | tee "$BUILD_DIR.log"
        fi
    fi

    if [ "$GENERATE_TL7218X" = true ]; then
        if should_generate build_tl7218x_default; then
            BUILD_DIR="build_tl7218x_default_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, NO OTA/MCUBoot, NO LZMA) ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x -d "$BUILD_DIR" -- \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl7218x_2m_flash_lzma_v1; then
            BUILD_DIR="build_tl7218x_2m_flash_lzma_v1_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v1 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=1 \
                -DCONF_FILE="prj.conf;boards/tl7218x_2m_flash_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl7218x_2m_flash_lzma_v2; then
            BUILD_DIR="build_tl7218x_2m_flash_lzma_v2_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v2 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=2 \
                -DCONF_FILE="prj.conf;boards/tl7218x_2m_flash_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi
    fi
}

# Function to generate Light Switch App
generate_light_switch() {
    echo ""
    echo "=== Generating Light Switch App ==="
    cd "$LIGHT_SWITCH_APP_DIR"

    if [ "$GENERATE_TL3238X" = true ]; then
        if should_generate build_tl3238x_retention_default; then
            BUILD_DIR="build_tl3238x_retention_default_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, NO OTA/MCUBoot, NO LZMA) ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x_retention -d "$BUILD_DIR" -- \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_retention_lzma_v1; then
            BUILD_DIR="build_tl3238x_retention_lzma_v1_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v1 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x_retention -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=1 \
                -DCONF_FILE="prj.conf;boards/tl3238x_retention_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_retention_lzma_v2; then
            BUILD_DIR="build_tl3238x_retention_lzma_v2_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v2 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl3238x_retention -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=2 \
                -DCONF_FILE="prj.conf;boards/tl3238x_retention_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl3238x_retention_dual_mode; then
            BUILD_DIR="build_tl3238x_retention_dual_mode_$DATE"
            echo ""
            echo "=== TL3238X: 2MB Flash (Matter + Zigbee dual mode, LZMA compressed, enable BT DFU) ==="
            echo "Generate directory: $BUILD_DIR"

            # Copy light-switch app specific Zigbee firmware
            if [ -f "$ZEPHYR_BASE/TL323X_FW/ZB/sampleSwitch_tl323x_log.bin" ]; then
                cp "$ZEPHYR_BASE/TL323X_FW/ZB/sampleSwitch_tl323x_log.bin" "$ZEPHYR_BASE/TL323X_FW/ZB/Zigbee-SampleDemo.bin"
                echo "Copied sampleSwitch_tl323x_log.bin to Zigbee-SampleDemo.bin"
            elif [ -f "$ZEPHYR_BASE/TL323X_FW/ZB/sampleSwitch_tl323x-bak.bin" ]; then
                cp "$ZEPHYR_BASE/TL323X_FW/ZB/sampleSwitch_tl323x-bak.bin" "$ZEPHYR_BASE/TL323X_FW/ZB/Zigbee-SampleDemo.bin"
                echo "Copied sampleSwitch_tl323x-bak.bin to Zigbee-SampleDemo.bin"
            fi

            west build -p -b tl3238x_retention -d "$BUILD_DIR" -- \
                -DCONF_FILE="prj.conf;boards/tl3238x_retention_dual_mode_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi
    fi

    if [ "$GENERATE_TL7218X" = true ]; then
        if should_generate build_tl7218x_retention_default; then
            BUILD_DIR="build_tl7218x_retention_default_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, NO OTA/MCUBoot, NO LZMA) ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x_retention -d "$BUILD_DIR" -- \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl7218x_retention_lzma_v1; then
            BUILD_DIR="build_tl7218x_retention_lzma_v1_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v1 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x_retention -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=1 \
                -DCONF_FILE="prj.conf;boards/tl7218x_retention_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi

        if should_generate build_tl7218x_retention_lzma_v2; then
            BUILD_DIR="build_tl7218x_retention_lzma_v2_$DATE"
            echo ""
            echo "=== TL7218X: 2MB Flash (Matter only, enable BT DFU, LZMA compressed) - v2 ==="
            echo "Generate directory: $BUILD_DIR"
            west build -p -b tl7218x_retention -d "$BUILD_DIR" -- \
                -DCONFIG_CHIP_DEVICE_SOFTWARE_VERSION=2 \
                -DCONF_FILE="prj.conf;boards/tl7218x_retention_ota_lzma.conf" \
                2>&1 | tee "$BUILD_DIR.log"
        fi
    fi
}

# Prepare for TL3238X dual-mode build
prepare_dual_mode() {
    echo ""
    echo "=== Preparing for TL3238X dual-mode build ==="

    # Download and extract TL323X_FW if not exists
    if [ ! -d "$ZEPHYR_BASE/TL323X_FW" ]; then
        echo "Downloading TL323X_FW..."
        wget -q https://doc.telink-semi.cn/Zephyr/binaries/public/tl323x_fw/TL323X_FW.zip -O /tmp/TL323X_FW.zip
        unzip -q /tmp/TL323X_FW.zip -d "$ZEPHYR_BASE"
        rm -f /tmp/TL323X_FW.zip
        echo "TL323X_FW downloaded and extracted successfully"
    else
        echo "TL323X_FW already exists"
    fi
}

# Cleanup temporary Zigbee-SampleDemo.bin after build
cleanup_zigbee_sample() {
    echo ""
    echo "=== Cleaning up Zigbee-SampleDemo.bin ==="
    if [ -f "$ZEPHYR_BASE/TL323X_FW/ZB/Zigbee-SampleDemo.bin" ]; then
        rm -f "$ZEPHYR_BASE/TL323X_FW/ZB/Zigbee-SampleDemo.bin"
        echo "Zigbee-SampleDemo.bin deleted"
    fi
}

# Extract firmware function
extract_firmware() {
    echo ""
    echo "=== Extracting Telink Matter Firmware ==="

    # Create package directory with date and time
    PARENT_DIR="$(cd "$CONNECTEDHOME_DIR/.." && pwd)"
    BASE_PKG_DIR="$PARENT_DIR/Matter_test_materials"
    mkdir -p "$BASE_PKG_DIR"
    PKG_DIR="$BASE_PKG_DIR/TL3238X-TL7218X-Matter-DUT-firmware-images-$DATE"
    mkdir -p "$PKG_DIR/feature-test/light-switch/tl3238x"
    mkdir -p "$PKG_DIR/feature-test/light-switch/tl7218x"
    mkdir -p "$PKG_DIR/feature-test/lighting/tl3238x"
    mkdir -p "$PKG_DIR/feature-test/lighting/tl7218x"
    mkdir -p "$PKG_DIR/feature-test/contact-sensor/tl3238x"
    mkdir -p "$PKG_DIR/feature-test/contact-sensor/tl7218x"
    echo "Package directory: $PKG_DIR"
    echo "$PKG_DIR" >"$BASE_PKG_DIR/.last_pkg_dir"

    # TL3238X Targets
    BUILD_32_DEFAULT=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl3238x_default")
    BUILD_32_2M_FLASH_LZMA_V1=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl3238x_2m_flash_lzma_v1")
    BUILD_32_2M_FLASH_LZMA_V2=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl3238x_2m_flash_lzma_v2")
    BUILD_32_4M_DUAL=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl3238x_4m_dual_mode")

    BUILD_32_RET_DEFAULT=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl3238x_retention_default")
    BUILD_32_RET_LZMA_V1=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl3238x_retention_lzma_v1")
    BUILD_32_RET_LZMA_V2=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl3238x_retention_lzma_v2")
    BUILD_32_RET_DUAL=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl3238x_retention_dual_mode")

    BUILD_32_CS_V1=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl3238x_retention_v1")
    BUILD_32_CS_V2=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl3238x_retention_v2")
    BUILD_32_CS_LZMA_V1=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl3238x_retention_lzma_v1")
    BUILD_32_CS_LZMA_V2=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl3238x_retention_lzma_v2")

    # TL7218X Targets
    BUILD_72_DEFAULT=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl7218x_default")
    BUILD_72_2M_FLASH_LZMA_V1=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl7218x_2m_flash_lzma_v1")
    BUILD_72_2M_FLASH_LZMA_V2=$(get_build_dir "$LIGHTING_APP_DIR" "build_tl7218x_2m_flash_lzma_v2")

    BUILD_72_RET_DEFAULT=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl7218x_retention_default")
    BUILD_72_RET_LZMA_V1=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl7218x_retention_lzma_v1")
    BUILD_72_RET_LZMA_V2=$(get_build_dir "$LIGHT_SWITCH_APP_DIR" "build_tl7218x_retention_lzma_v2")

    BUILD_72_CS_V1=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl7218x_retention_v1")
    BUILD_72_CS_V2=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl7218x_retention_v2")
    BUILD_72_CS_LZMA_V1=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl7218x_retention_lzma_v1")
    BUILD_72_CS_LZMA_V2=$(get_build_dir "$CONTACT_SENSOR_APP_DIR" "build_tl7218x_retention_lzma_v2")

    echo ""
    echo "=== Verifying firmware versions ==="
    for dir in \
        "$BUILD_32_DEFAULT" "$BUILD_32_2M_FLASH_LZMA_V1" "$BUILD_32_2M_FLASH_LZMA_V2" \
        "$BUILD_32_4M_DUAL" "$BUILD_32_RET_DEFAULT" "$BUILD_32_RET_LZMA_V1" \
        "$BUILD_32_RET_LZMA_V2" "$BUILD_32_RET_DUAL" "$BUILD_32_CS_V1" \
        "$BUILD_32_CS_V2" "$BUILD_32_CS_LZMA_V1" "$BUILD_32_CS_LZMA_V2" \
        "$BUILD_72_DEFAULT" "$BUILD_72_2M_FLASH_LZMA_V1" \
        "$BUILD_72_2M_FLASH_LZMA_V2" "$BUILD_72_RET_DEFAULT" "$BUILD_72_RET_LZMA_V1" \
        "$BUILD_72_RET_LZMA_V2" "$BUILD_72_CS_V1" \
        "$BUILD_72_CS_V2" "$BUILD_72_CS_LZMA_V1" "$BUILD_72_CS_LZMA_V2"; do
        if [ -d "$dir" ] && [ -f "$dir/zephyr/.config" ]; then
            version=$(grep CONFIG_CHIP_DEVICE_SOFTWARE_VERSION "$dir/zephyr/.config" 2>/dev/null | cut -d'=' -f2 || echo "unknown")
            echo "$dir: CONFIG_CHIP_DEVICE_SOFTWARE_VERSION=$version"
        fi
    done

    echo ""
    echo "=== Copying Lighting App files (TL3238X) ==="
    if [ -n "$BUILD_32_DEFAULT" ] && [ -f "$BUILD_32_DEFAULT/zephyr/merged.bin" ]; then
        cp "$BUILD_32_DEFAULT/zephyr/merged.bin" "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_default.bin"
        echo "Copied from $BUILD_32_DEFAULT: $PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_default.bin"
    fi
    if [ -n "$BUILD_32_2M_FLASH_LZMA_V1" ] && [ -f "$BUILD_32_2M_FLASH_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_32_2M_FLASH_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_32_2M_FLASH_LZMA_V1: $PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_32_2M_FLASH_LZMA_V2" ] && [ -f "$BUILD_32_2M_FLASH_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_32_2M_FLASH_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_32_2M_FLASH_LZMA_V2: $PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_32_2M_FLASH_LZMA_V2" ] && [ -f "$BUILD_32_2M_FLASH_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_32_2M_FLASH_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_32_2M_FLASH_LZMA_V2: $PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi
    if [ -n "$BUILD_32_4M_DUAL" ] && [ -f "$BUILD_32_4M_DUAL/zephyr/merged.bin" ]; then
        cp "$BUILD_32_4M_DUAL/zephyr/merged.bin" "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_4m_flash_dual_mode.bin"
        echo "Copied from $BUILD_32_4M_DUAL: $PKG_DIR/feature-test/lighting/tl3238x/lighting-app_4m_flash_dual_mode.bin"
    fi

    echo ""
    echo "=== Copying Lighting App files (TL7218X) ==="
    if [ -n "$BUILD_72_DEFAULT" ] && [ -f "$BUILD_72_DEFAULT/zephyr/merged.bin" ]; then
        cp "$BUILD_72_DEFAULT/zephyr/merged.bin" "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_default.bin"
        echo "Copied from $BUILD_72_DEFAULT: $PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_default.bin"
    fi
    if [ -n "$BUILD_72_2M_FLASH_LZMA_V1" ] && [ -f "$BUILD_72_2M_FLASH_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_72_2M_FLASH_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_72_2M_FLASH_LZMA_V1: $PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_72_2M_FLASH_LZMA_V2" ] && [ -f "$BUILD_72_2M_FLASH_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_72_2M_FLASH_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_72_2M_FLASH_LZMA_V2: $PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_72_2M_FLASH_LZMA_V2" ] && [ -f "$BUILD_72_2M_FLASH_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_72_2M_FLASH_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_72_2M_FLASH_LZMA_V2: $PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi

    echo ""
    echo "=== Copying Light Switch App files (TL3238X) ==="
    if [ -n "$BUILD_32_RET_DEFAULT" ] && [ -f "$BUILD_32_RET_DEFAULT/zephyr/merged.bin" ]; then
        cp "$BUILD_32_RET_DEFAULT/zephyr/merged.bin" "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_default.bin"
        echo "Copied from $BUILD_32_RET_DEFAULT: $PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_default.bin"
    fi
    if [ -n "$BUILD_32_RET_LZMA_V1" ] && [ -f "$BUILD_32_RET_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_32_RET_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_32_RET_LZMA_V1: $PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_32_RET_LZMA_V2" ] && [ -f "$BUILD_32_RET_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_32_RET_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_32_RET_LZMA_V2: $PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_32_RET_LZMA_V2" ] && [ -f "$BUILD_32_RET_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_32_RET_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_32_RET_LZMA_V2: $PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi
    if [ -n "$BUILD_32_RET_DUAL" ] && [ -f "$BUILD_32_RET_DUAL/zephyr/merged.bin" ]; then
        cp "$BUILD_32_RET_DUAL/zephyr/merged.bin" "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_dual_mode.bin"
        echo "Copied from $BUILD_32_RET_DUAL: $PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_dual_mode.bin"
    fi

    echo ""
    echo "=== Copying Light Switch App files (TL7218X) ==="
    if [ -n "$BUILD_72_RET_DEFAULT" ] && [ -f "$BUILD_72_RET_DEFAULT/zephyr/merged.bin" ]; then
        cp "$BUILD_72_RET_DEFAULT/zephyr/merged.bin" "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_default.bin"
        echo "Copied from $BUILD_72_RET_DEFAULT: $PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_default.bin"
    fi
    if [ -n "$BUILD_72_RET_LZMA_V1" ] && [ -f "$BUILD_72_RET_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_72_RET_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_72_RET_LZMA_V1: $PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_72_RET_LZMA_V2" ] && [ -f "$BUILD_72_RET_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_72_RET_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_72_RET_LZMA_V2: $PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_72_RET_LZMA_V2" ] && [ -f "$BUILD_72_RET_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_72_RET_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_72_RET_LZMA_V2: $PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi

    echo ""
    echo "=== Copying Contact Sensor App files (TL3238X) ==="
    if [ -n "$BUILD_32_CS_V1" ] && [ -f "$BUILD_32_CS_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_32_CS_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v1.bin"
        echo "Copied from $BUILD_32_CS_V1: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v1.bin"
    fi
    if [ -n "$BUILD_32_CS_V2" ] && [ -f "$BUILD_32_CS_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_32_CS_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v2_ota.ota"
        echo "Copied from $BUILD_32_CS_V2: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v2_ota.ota"
    fi
    if [ -n "$BUILD_32_CS_V2" ] && [ -f "$BUILD_32_CS_V2/zephyr/merged_dfu.bin" ]; then
        cp "$BUILD_32_CS_V2/zephyr/merged_dfu.bin" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v2_dfu.bin"
        echo "Copied from $BUILD_32_CS_V2: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_v2_dfu.bin"
    fi
    if [ -n "$BUILD_32_CS_LZMA_V1" ] && [ -f "$BUILD_32_CS_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_32_CS_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_32_CS_LZMA_V1: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_32_CS_LZMA_V2" ] && [ -f "$BUILD_32_CS_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_32_CS_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_32_CS_LZMA_V2: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_32_CS_LZMA_V2" ] && [ -f "$BUILD_32_CS_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_32_CS_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_32_CS_LZMA_V2: $PKG_DIR/feature-test/contact-sensor/tl3238x/contact-sensor-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi

    echo ""
    echo "=== Copying Contact Sensor App files (TL7218X) ==="
    if [ -n "$BUILD_72_CS_V1" ] && [ -f "$BUILD_72_CS_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_72_CS_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v1.bin"
        echo "Copied from $BUILD_72_CS_V1: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v1.bin"
    fi
    if [ -n "$BUILD_72_CS_V2" ] && [ -f "$BUILD_72_CS_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_72_CS_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v2_ota.ota"
        echo "Copied from $BUILD_72_CS_V2: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v2_ota.ota"
    fi
    if [ -n "$BUILD_72_CS_V2" ] && [ -f "$BUILD_72_CS_V2/zephyr/merged_dfu.bin" ]; then
        cp "$BUILD_72_CS_V2/zephyr/merged_dfu.bin" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v2_dfu.bin"
        echo "Copied from $BUILD_72_CS_V2: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_v2_dfu.bin"
    fi
    if [ -n "$BUILD_72_CS_LZMA_V1" ] && [ -f "$BUILD_72_CS_LZMA_V1/zephyr/merged.bin" ]; then
        cp "$BUILD_72_CS_LZMA_V1/zephyr/merged.bin" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v1.bin"
        echo "Copied from $BUILD_72_CS_LZMA_V1: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v1.bin"
    fi
    if [ -n "$BUILD_72_CS_LZMA_V2" ] && [ -f "$BUILD_72_CS_LZMA_V2/zephyr/matter.ota" ]; then
        cp "$BUILD_72_CS_LZMA_V2/zephyr/matter.ota" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v2_ota.ota"
        echo "Copied from $BUILD_72_CS_LZMA_V2: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v2_ota.ota"
    fi
    if [ -n "$BUILD_72_CS_LZMA_V2" ] && [ -f "$BUILD_72_CS_LZMA_V2/zephyr/merged_dfu.lzma.bin" ]; then
        cp "$BUILD_72_CS_LZMA_V2/zephyr/merged_dfu.lzma.bin" "$PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v2_dfu.lzma.bin"
        echo "Copied from $BUILD_72_CS_LZMA_V2: $PKG_DIR/feature-test/contact-sensor/tl7218x/contact-sensor-app_2m_flash_lzma_v2_dfu.lzma.bin"
    fi

    echo ""
    echo "=== Extraction complete! ==="
    echo "Package directory: $PKG_DIR"
}

# Function to add firmware entries to README
add_firmware_entry() {
    local file="$1"
    local desc="$2"
    echo "| $file | $desc |" >>"$PKG_DIR/README.md"
}

# Generate README function
generate_readme() {
    echo ""
    echo "=== Generating README.md ==="

    if [ -z "$PKG_DIR" ]; then
        # Try to find last package directory
        PARENT_DIR="$(cd "$CONNECTEDHOME_DIR/.." && pwd)"
        BASE_PKG_DIR="$PARENT_DIR/Matter_test_materials"
        if [ -f "$BASE_PKG_DIR/.last_pkg_dir" ]; then
            PKG_DIR=$(cat "$BASE_PKG_DIR/.last_pkg_dir")
        fi
    fi

    if [ -z "$PKG_DIR" ] || [ ! -d "$PKG_DIR" ]; then
        echo "Error: Package directory not specified or not found"
        return 1
    fi

    # Extract date from directory name
    DATE=$(basename "$PKG_DIR" | grep -oE '[0-9]{8}_[0-9]{6}' || date +%Y%m%d_%H%M%S)

    # Function to get git branch from directory
    get_git_branch() {
        local dir="$1"
        if [ -d "$dir/.git" ]; then
            git -C "$dir" rev-parse --abbrev-ref HEAD
        fi
    }

    # Function to get git commit (short) from directory
    get_git_commit() {
        local dir="$1"
        if [ -d "$dir/.git" ]; then
            git -C "$dir" rev-parse --short HEAD
        fi
    }

    # Get repo paths
    HAL_TELINK_DIR="$(cd "$CONNECTEDHOME_DIR/../modules/hal/telink" 2>/dev/null && pwd)"

    # Get branch and commit info
    MATTER_BRANCH="$(get_git_branch "$CONNECTEDHOME_DIR")"
    MATTER_COMMIT="$(get_git_commit "$CONNECTEDHOME_DIR")"

    ZEPHYR_BRANCH="$(get_git_branch "$ZEPHYR_BASE")"
    ZEPHYR_COMMIT="$(get_git_commit "$ZEPHYR_BASE")"

    HAL_TELINK_BRANCH="$(get_git_branch "$HAL_TELINK_DIR")"
    HAL_TELINK_COMMIT="$(get_git_commit "$HAL_TELINK_DIR")"

    # Handle detached HEAD for display
    if [ "$MATTER_BRANCH" = "HEAD" ]; then
        MATTER_BRANCH="(detached HEAD)"
    fi
    if [ "$ZEPHYR_BRANCH" = "HEAD" ]; then
        ZEPHYR_BRANCH="(detached HEAD)"
    fi
    if [ "$HAL_TELINK_BRANCH" = "HEAD" ]; then
        HAL_TELINK_BRANCH="(detached HEAD)"
    fi

    cat >"$PKG_DIR/README.md" <<EOF
# TL3238X & TL7218X Matter Firmware Package

**Generated on:** $(date +%Y-%m-%d)

## 1. Code Base Information

### Matter
- Branch: ${MATTER_BRANCH:-unknown}
- Commit: ${MATTER_COMMIT:-unknown}

### Zephyr
- Branch: ${ZEPHYR_BRANCH:-unknown}
- Commit: ${ZEPHYR_COMMIT:-unknown}

### Hal_telink
- Branch: ${HAL_TELINK_BRANCH:-unknown}
- Commit: ${HAL_TELINK_COMMIT:-unknown}

## 2. Firmware Contents

## 2.1 Light Switch App (light-switch/tl3238x)

| Firmware File | Description |
|---------------|-------------|
EOF

    # TL3238X Light Switch
    if [ -f "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_default.bin" ]; then
        add_firmware_entry "light-switch/tl3238x/light-switch-app_2m_flash_default.bin" "Basic firmware (2MB Flash, no OTA/DFU)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v1.bin" ]; then
        add_firmware_entry "light-switch/tl3238x/light-switch-app_2m_flash_lzma_v1.bin" "App software version 1 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_ota.ota" ]; then
        add_firmware_entry "light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_ota.ota" "Matter OTA image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin" ]; then
        add_firmware_entry "light-switch/tl3238x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin" "BLE DFU image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl3238x/light-switch-app_2m_flash_dual_mode.bin" ]; then
        add_firmware_entry "light-switch/tl3238x/light-switch-app_2m_flash_dual_mode.bin" "Dual-mode firmware (Matter/Zigbee, 2MB Flash)"
    fi

    # TL7218X Light Switch
    cat >>"$PKG_DIR/README.md" <<EOF

## 2.2 Light Switch App (light-switch/tl7218x)

| Firmware File | Description |
|---------------|-------------|
EOF

    if [ -f "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_default.bin" ]; then
        add_firmware_entry "light-switch/tl7218x/light-switch-app_2m_flash_default.bin" "Basic firmware (2MB Flash, no OTA/DFU)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v1.bin" ]; then
        add_firmware_entry "light-switch/tl7218x/light-switch-app_2m_flash_lzma_v1.bin" "App software version 1 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_ota.ota" ]; then
        add_firmware_entry "light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_ota.ota" "Matter OTA image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin" ]; then
        add_firmware_entry "light-switch/tl7218x/light-switch-app_2m_flash_lzma_v2_dfu.lzma.bin" "BLE DFU image v2 (LZMA, 2MB Flash)"
    fi

    # TL3238X Lighting
    cat >>"$PKG_DIR/README.md" <<EOF

## 2.3 Lighting App (lighting/tl3238x)

| Firmware File | Description |
|---------------|-------------|
EOF

    if [ -f "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_default.bin" ]; then
        add_firmware_entry "lighting/tl3238x/lighting-app_2m_flash_default.bin" "Basic firmware (2MB Flash, no OTA/DFU)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v1.bin" ]; then
        add_firmware_entry "lighting/tl3238x/lighting-app_2m_flash_lzma_v1.bin" "App software version 1 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_ota.ota" ]; then
        add_firmware_entry "lighting/tl3238x/lighting-app_2m_flash_lzma_v2_ota.ota" "Matter OTA image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin" ]; then
        add_firmware_entry "lighting/tl3238x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin" "BLE DFU image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl3238x/lighting-app_4m_flash_dual_mode.bin" ]; then
        add_firmware_entry "lighting/tl3238x/lighting-app_4m_flash_dual_mode.bin" "Dual-mode firmware (Matter/Zigbee, 4MB Flash)"
    fi

    # TL7218X Lighting
    cat >>"$PKG_DIR/README.md" <<EOF

## 2.4 Lighting App (lighting/tl7218x)

| Firmware File | Description |
|---------------|-------------|
EOF

    if [ -f "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_default.bin" ]; then
        add_firmware_entry "lighting/tl7218x/lighting-app_2m_flash_default.bin" "Basic firmware (2MB Flash, no OTA/DFU)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v1.bin" ]; then
        add_firmware_entry "lighting/tl7218x/lighting-app_2m_flash_lzma_v1.bin" "App software version 1 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_ota.ota" ]; then
        add_firmware_entry "lighting/tl7218x/lighting-app_2m_flash_lzma_v2_ota.ota" "Matter OTA image v2 (LZMA, 2MB Flash)"
    fi
    if [ -f "$PKG_DIR/feature-test/lighting/tl7218x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin" ]; then
        add_firmware_entry "lighting/tl7218x/lighting-app_2m_flash_lzma_v2_dfu.lzma.bin" "BLE DFU image v2 (LZMA, 2MB Flash)"
    fi

    cat >>"$PKG_DIR/README.md" <<EOF

## 3. Testing Recommendations

### 3.1 Functional Testing
- Matter basic functionality testing
- Commissioning testing
- Control command verification

### 3.2 OTA Upgrade Testing
- Perform OTA upgrade testing using App software version 1 and Matter OTA image v2
- Execute 100 rounds of OTA testing via Matter Test Harness

### 3.3 DFU Testing
- Verify BLE DFU functionality
- Perform upgrade testing using v2 DFU image

### 3.4 Dual-mode Testing (Optional)
- Verify dual-mode functionality (Matter/Zigbee) of lighting-app_4m_flash_dual_mode.bin
EOF

    echo "Generated README.md at: $PKG_DIR/README.md"
}

# Snapshot the Telink Matter release note working copy into a versioned archive.
#
# The hand-maintained working copy lives at:
#   docs/platforms/telink/releases/telink_release_notes.md
# This function derives the release version from the nearest `tl_v*` git tag and
# writes a frozen, versioned copy next to it, e.g.:
#   docs/platforms/telink/releases/telink_release_notes_tl_v1.0.1-beta-v1.5.1.0.md
#
# The versioned file is an exact snapshot of the working copy at release time.
# If a versioned file for the current tag already exists, it is left untouched
# (the frozen snapshot is never silently overwritten).
snapshot_release_note() {
    echo ""
    echo "=== Snapshotting Telink Matter release note ==="

    local releases_dir="$CONNECTEDHOME_DIR/docs/platforms/telink/releases"
    local working_copy="$releases_dir/telink_release_notes.md"

    if [ ! -f "$working_copy" ]; then
        echo "Warning: release note working copy not found at:"
        echo "  $working_copy"
        echo "Skipping release note snapshot."
        return 0
    fi

    # Determine the release version from the nearest `tl_v*` git tag.
    # 1. Prefer an exact tag on the current commit.
    # 2. Fall back to the most recently created `tl_v*` tag.
    local version=""
    version=$(git -C "$CONNECTEDHOME_DIR" describe --tags --exact-match 2>/dev/null |
        grep '^tl_v' | head -1 || true)
    if [ -z "$version" ]; then
        version=$(git -C "$CONNECTEDHOME_DIR" tag --list 'tl_v*' \
            --sort=-creatordate | head -1)
    fi

    if [ -z "$version" ]; then
        echo "Warning: no $(tl_v*) git tag found; skipping release note snapshot."
        echo "  Tag the release (e.g. \`git tag tl_v1.0.0\`) to enable snapshotting."
        return 0
    fi

    local snapshot="$releases_dir/telink_release_notes_$version.md"

    if [ -f "$snapshot" ]; then
        echo "Versioned snapshot already exists, leaving untouched:"
        echo "  $snapshot"
        return 0
    fi

    cp "$working_copy" "$snapshot"
    echo "Created versioned release note snapshot:"
    echo "  $snapshot"
    echo "  (version: $version)"
}

# Zip firmware function
zip_firmware() {
    echo ""
    echo "=== Creating zip archive ==="

    if [ -z "$PKG_DIR" ]; then
        # Try to find last package directory
        PARENT_DIR="$(cd "$CONNECTEDHOME_DIR/.." && pwd)"
        BASE_PKG_DIR="$PARENT_DIR/Matter_test_materials"
        if [ -f "$BASE_PKG_DIR/.last_pkg_dir" ]; then
            PKG_DIR=$(cat "$BASE_PKG_DIR/.last_pkg_dir")
        fi
    fi

    if [ -z "$PKG_DIR" ] || [ ! -d "$PKG_DIR" ]; then
        echo "Error: Package directory not specified or not found"
        return 1
    fi

    PKG_NAME=$(basename "$PKG_DIR")
    ZIP_NAME="$BASE_PKG_DIR/$PKG_NAME.zip"

    cd "$BASE_PKG_DIR"
    zip -r "$ZIP_NAME" "$PKG_NAME"
    cd "$CONNECTEDHOME_DIR"
    echo "Created: $ZIP_NAME"

    echo ""
    echo "=== Package content ==="
    ls -laR "$PKG_DIR"

    echo ""
    echo "=== Packaging complete! ==="
    echo "Package: $ZIP_NAME"
}

# Main execution
echo "Starting package process for Telink Matter firmware..."
echo "Timestamp: $DATE"

if [ "$DO_BUILD" = true ]; then
    # Prepare for dual-mode if we need to build dual-mode targets
    prepare_dual_mode

    if [ "$GENERATE_LIGHTING" = true ]; then
        generate_lighting_app
    fi

    if [ "$GENERATE_SWITCH" = true ]; then
        generate_light_switch
    fi

    echo ""
    echo "All generations complete!"
    echo "Generate timestamp: $DATE"

    # Cleanup temporary Zigbee-SampleDemo.bin
    cleanup_zigbee_sample
fi

if [ "$DO_EXTRACT" = true ]; then
    extract_firmware
fi

if [ "$DO_README" = true ]; then
    generate_readme
fi

# Snapshot the release note alongside README generation (release documentation
# step). Also runs on --all and --zip so a packaged release always gets a
# versioned release note archive.
if [ "$DO_README" = true ] || [ "$DO_ZIP" = true ]; then
    snapshot_release_note
fi

if [ "$DO_ZIP" = true ]; then
    zip_firmware
fi

echo ""
echo "All tasks complete!"
