TL3238X Lighting App Build Guide
=================================

1. 2MB Flash (Matter only, LZMA compressed)
-------------------------------------------

Config: boards/tl3238x.conf
  - CONFIG_LZMA=y, CONFIG_COMPRESS_LZMA=y
  - CONFIG_DUAL_MODE=0

Overlay: src/platform/telink/tl3238x_2m_lzma_flash.overlay
  - slot0: 0x15000, size 0x120000 (1152KB)
  - slot1: 0x135000, size 0xAB000 (684KB)

Build:
  west build -p -b tl3238x -d build_tl3238x_2m > build_tl3238x_2m.log

Output:
  build_tl3238x_2m/zephyr/merged.bin          -> flash this
  build_tl3238x_2m/zephyr/merged_dfu.lzma.bin -> DFU over BLE SMP
  build_tl3238x_2m/zephyr/matter.ota          -> OTA upgrade


2. 4MB Flash (Matter + Zigbee dual mode, no LZMA)
---------------------------------------------------

Prerequisite: copy Zigbee firmware
  cp <zigbee_fw>.bin ${ZEPHYR_BASE}/TL323X_FW/ZB/Zigbee-SampleDemo.bin

Config: boards/tl3238x_4m_flash_dual_mode.conf
  - CONFIG_LZMA=n, CONFIG_COMPRESS_LZMA=n
  - CONFIG_DUAL_MODE=2 (auto-switch mode)

Overlay: src/platform/telink/tl3238x_4m_flash.overlay
  - slot0: 0x16000, size 0x1E5000 (1940KB)
  - slot1: 0x1FB000, size 0x1E5000 (1940KB)

Board overlay: boards/tl3238x_for_TL3238C-EVK40D.overlay
  - LED/Key definitions for TL3238C-EVK40D board

Build:
  west build -p -b tl3238x -d build_tl3238x_4m_dual_mode -- \
    -DFLASH_SIZE=4m \
    -DCONF_FILE="prj.conf boards/tl3238x_4m_flash_dual_mode.conf" \
    -DDTC_OVERLAY_FILE="boards/tl3238x_for_TL3238C-EVK40D.overlay" > build_tl3238x_4m_dual_mode.log 

Output:
  build_tl3238x_4m_dual_mode/zephyr/merged.bin      -> flash this (sboot+mcuboot+matter+zb)
  build_tl3238x_4m_dual_mode/zephyr/merged_dfu.bin  -> DFU over BLE SMP
  build_tl3238x_4m_dual_mode/zephyr/matter.ota      -> OTA upgrade
