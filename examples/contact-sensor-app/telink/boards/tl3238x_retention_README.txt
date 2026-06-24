TL3238X Retention Contact-Sensor App Build Guide
=================================================

1. Matter only (LZMA compressed)
---------------------------------

Config: boards/tl3238x_retention.conf
  - CONFIG_LZMA=y, CONFIG_COMPRESS_LZMA=y
  - CONFIG_DUAL_MODE=0

Overlay: src/platform/telink/tl3238x_2m_flash.overlay
  - slot0: 0x15000, size 0x120000 (1152KB)
  - slot1: 0x135000, size 0xAB000 (684KB)

Build:
  west build -p -b tl3238x_retention -d build_tl3238x_retention > build_tl3238x_retention.log

Output:
  build_tl3238x_retention/zephyr/merged.bin          -> flash this
  build_tl3238x_retention/zephyr/merged_dfu.lzma.bin -> DFU over BLE SMP
  build_tl3238x_retention/zephyr/matter.ota          -> OTA upgrade


2. Matter + Zigbee dual mode (LZMA compressed)
------------------------------------------------

Prerequisite: copy Zigbee firmware
  cp <zigbee_fw>.bin ${ZEPHYR_BASE}/TL323X_FW/ZB/Zigbee-SampleDemo.bin

Config: boards/tl3238x_retention_dual_mode.conf
  - CONFIG_LZMA=y, CONFIG_COMPRESS_LZMA=y
  - CONFIG_DUAL_MODE=1 (action switch mode)

Overlay: src/platform/telink/tl3238x_2m_flash.overlay
  - slot0: 0x15000, size 0x120000 (1152KB)
  - slot1: 0x135000, size 0xAB000 (684KB)

Build:
  west build -p -b tl3238x_retention -d build_tl3238x_retention_dual_mode -- \
    -DCONF_FILE="prj.conf boards/tl3238x_retention_dual_mode.conf" > build_tl3238x_retention_dual_mode.log

Output:
  build_tl3238x_retention_dual_mode/zephyr/merged.bin          -> flash this (sboot+mcuboot+matter+zb)
  build_tl3238x_retention_dual_mode/zephyr/merged_dfu.lzma.bin -> DFU over BLE SMP
  build_tl3238x_retention_dual_mode/zephyr/matter.ota          -> OTA upgrade
