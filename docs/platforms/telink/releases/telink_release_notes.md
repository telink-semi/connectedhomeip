# Telink Matter SDK Release Note

[![Version](https://img.shields.io/badge/Version-tl_v1.2.0--alpha--v1.5.1-blue?style=flat-square)](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.2.0-alpha-v1.5)
[![License](https://img.shields.io/badge/License-Apache%202.0-red?style=flat-square)](LICENSE)
[![Matter](https://img.shields.io/badge/Matter-v1.5-green?style=flat-square)](https://github.com/project-chip/connectedhomeip/commit/f4a8cf98ada4ad4f439b45e360800693cc5f1391)

---

-   **Release Type:** Pre-Release (Alpha)
-   **Branch:**
    [pre_release-v1.2-v1.5-branch](https://github.com/telink-semi/connectedhomeip/tree/pre_release-v1.2-v1.5-branch)
-   **Tag Version:**
    [tl_v1.2.0-alpha-v1.5](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.2.0-alpha-v1.5)
-   **Target Commit:**
    [50cae34](https://github.com/telink-semi/connectedhomeip/commit/50cae34e79ecbd5af5644eb60f6aa564f6c48ab4)

---

## 📖 Introduction

This release is based on the target commit of `dev-tlk_v1.5` branch, providing
Matter protocol support for Telink RISC-V SoC platforms. It integrates the
Matter SDK with the Telink Zephyr SDK to enable Matter-over-Thread devices on
Telink chips including TL323X, TL521X, and TL721X.

---

## ✨ Highlights

| Category                  | Details                                                |
| ------------------------- | ------------------------------------------------------ |
| **Matter Support**        | Matter 1.5.1 protocol stack                            |
| **Supported Chips**       | TL323X, TL521X, TL721X                                 |
| **Sample Apps**           | Lighting, Light Switch, Lock, Bridge, Thermostat, etc. |
| **OTA**                   | OTA requestor and compress-LZMA support                |
| **Factory Data**          | Factory data provisioning support                      |
| **BLE/Thread Concurrent** | BLE + Thread concurrent mode on TL323X and TL721X      |
| **Channel Sounding**      | CS RAS reflector support on TL721X                     |

---

## 🆕 New Features

-   ✅ New TL521X SoC platform support with lighting-app
-   ✅ Add BLE/Thread concurrent mode support on TL7218X and TL3238X, enabling
    BLE to run concurrently with an active Thread network
-   ✅ Introduce CHIP_CONCURRENT_MODE Kconfig option that keeps the BLE
    controller alive after Thread commissioning
-   ✅ Add BLE idle mode and tl_dual_mode_start coordination for TLX RF
    coexistence
-   ✅ Implement Channel Sounding (CS) RAS reflector (CsReflector) for TL7218X
-   ✅ Add concurrent board configs for TL3238X and TL7218X with README docs
-   ✅ Add flash overlays for TL5218X 2MB and 4MB flash variants, including LZMA
    compression and backup overlays
-   ✅ LZMA compression support for OTA images
-   ✅ Factory data provisioning support
-   ✅ Power management with retention RAM support

---

## 🔀 BLE/Thread Concurrent Mode

This release introduces support for running BLE concurrently with an active
Thread network on selected Telink SoCs.

### Supported Configurations

| SoC Family | BLE-Only | Thread-Only | BLE + Thread Concurrent | Channel Sounding |
| :--------: | :------: | :---------: | :---------------------: | :--------------: |
|   TL323X   |    ✅    |     ✅      |           ✅            |        —         |
|   TL521X   |    ✅    |     ✅      |            —            |        —         |
|   TL721X   |    ✅    |     ✅      |           ✅            |        ✅        |

> **Note:** BLE + Thread concurrent mode requires the `*_concurrent` board
> configuration. Channel Sounding on TL721X additionally requires the
> `*_concurrent_cs` configuration.

---

## 🐛 Bug Fixes

| Issue                    | Description                                                  |
| ------------------------ | ------------------------------------------------------------ |
| **Amazon Commissioning** | Fix commissioning on Amazon when Channel Sounding is enabled |

---

## 📦 Updates

-   Updated Telink BLE SDK
    ([commit:53eb98b32ea79ed7ab38f5daabde0a78a7880cd9](https://github.com/telink-semi/tl_ble_sdk_zephyr/commit/53eb98b32ea79ed7ab38f5daabde0a78a7880cd9))
-   Updated Telink HAL Zephyr
    ([commit:bdf0d7927d31809340610b5e1575667e2d862110](https://github.com/telink-semi/hal_telink/commit/bdf0d7927d31809340610b5e1575667e2d862110))
-   Updated MCUBoot
    ([commit:ce0da85c39c749df49b0ec62b33d2ecdea24c927](https://github.com/telink-semi/mcuboot/commit/ce0da85c39c749df49b0ec62b33d2ecdea24c927))
-   Updated OpenThread Telink source code
    ([commit:542aaab44e1308e1a8a24573dfbd413fade342ee](https://github.com/telink-semi/openthread/commit/542aaab44e1308e1a8a24573dfbd413fade342ee))
-   Updated OpenThread Telink library
    ([commit:308dae2f80084f87073cfd4fbd30f1be0799be7b](https://github.com/telink-semi/openthread_telink_lib/commit/308dae2f80084f87073cfd4fbd30f1be0799be7b))
-   Updated Telink Zephyr SDK to support TL521X, TL323X and TL721X
    ([commit:8e3ccc07900692fe5a9990cd517203de61b2eefc](https://github.com/telink-semi/zephyr/commit/8e3ccc07900692fe5a9990cd517203de61b2eefc))

---

## ⚠️ Important Notes

**This is an ALPHA pre-release version for demonstration and testing purposes.
Not recommended for production use.**

### Telink Matter ↔ Telink Zephyr Dependency

The Telink Matter SDK is built **on top of** the Telink Zephyr SDK, and the two
are tightly coupled — they must be used as a matched pair.

The Telink Zephyr SDK provides the foundational layers required to bring up a
Matter device on Telink silicon: the Zephyr RTOS core, the Telink hardware
abstraction layer (HAL), the BLE stack, MCUBoot bootloader, OpenThread
networking stack, the WEST tool and build toolchain.

The Telink Matter SDK (this repository) builds upon that foundation to deliver
the Matter protocol stack itself together with a set of Telink Matter example
applications. Both components are required: the Matter SDK cannot be built
without the Zephyr SDK, and the Zephyr SDK alone does not provide Matter
support.

> ⚠️ **Version pairing:** This Telink Matter release is validated against a
> specific Telink Zephyr SDK tag (see the
> [Version Information](#version-information) table below). Using a different
> Zephyr revision may cause build failures or runtime issues.

---

## 📋 Version Information

### Matter SDK &amp; Toolchain

| Component              | Version                                                                                                                                                                                                                                                                                   |
| ---------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Matter SDK Version** | Telink Matter v1.5.1.0                                                                                                                                                                                                                                                                    |
| **Matter Branch**      | master                                                                                                                                                                                                                                                                                    |
| **Commit**             | [f4a8cf9](https://github.com/project-chip/connectedhomeip/commit/f4a8cf98ada4ad4f439b45e360800693cc5f1391) (the previous one of [15a68d7](https://github.com/project-chip/connectedhomeip/commit/15a68d7026b1cd34a0d4cf35dadc7558e503d2cb) that community upgraded Matter version to 1.6) |
| **Toolchain**          | Zephyr SDK v0.17.0 riscv64-zephyr-elf                                                                                                                                                                                                                                                     |

### Telink Matter SDK

| Property          | Version                                                                                                          |
| ----------------- | ---------------------------------------------------------------------------------------------------------------- |
| **Branch**        | [pre_release-v1.2-v1.5-branch](https://github.com/telink-semi/connectedhomeip/tree/pre_release-v1.2-v1.5-branch) |
| **Target Commit** | [50cae34](https://github.com/telink-semi/connectedhomeip/commit/50cae34e79ecbd5af5644eb60f6aa564f6c48ab4)        |
| **Tag Name**      | [tl_v1.2.0-alpha-v1.5](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.2.0-alpha-v1.5)         |
| **Release Type**  | Pre-Release (Alpha)                                                                                              |

### Chip &amp; Hardware Versions

📦 **Chip Versions**

| Chip Family | Versions |
| ----------- | -------- |
| TL323X      | A0       |
| TL521X      | A0       |
| TL721X      | A2/A3    |

🔧 **Hardware EVK Versions**

| Chip   | EVK Version    |
| ------ | -------------- |
| TL323X | C1T388A20_V1.1 |
| TL521X | C1T416A20_V1.0 |
| TL721X | C1T315A20_V1.2 |

---

## 📱 Matter Community Examples

This release provides Telink platform ports for the following Matter community
example applications. Each example is located under
`examples/<app-name>/telink/` in the repository. The table below summarizes each
example and the Telink chip platforms that support it (see
[Telink Examples YAML file](.github/workflows/examples-telink.yaml)).

> ✅ = Supported and Tested &nbsp;&nbsp; 🟡 = Supported but Untested
> &nbsp;&nbsp; · = Untested

| Example          | Description                                                | TL323X | TL521X | TL721X |
| ---------------- | ---------------------------------------------------------- | :----: | :----: | :----: |
| lighting-app     | Lighting (On/Off, Level, Color Control)                    |   ✅   |   ✅   |   ✅   |
| light-switch-app | Light Switch (controller, switches a bound lighting-app)   |   ✅   |   ·    |   ✅   |
| bridge-app       | Bridge / Aggregator (bridges non-Matter devices to Matter) |   ·    |   ·    |   🟡   |
| window-app       | Window Covering (shades, blinds)                           |   ·    |   ·    |   🟡   |

### Notes on Platform Support

-   **Tested combinations (✅):** TL323X and TL721X with lighting-app and
    light-switch-app have been fully tested.
-   **Supported but untested (🟡):** TL721X bridge-app and window-app are
    compiled successfully but have not been functionally validated.
-   **Untested (·):** Combinations not listed are not built or validated.
-   TL323X / TL521X / TL721X are the latest Telink RISC-V SoC families. Lighting
    and light-switch apps are supported across TL323X and TL721X; TL521X
    supports lighting-app only.
-   The light-switch-app uses the `*_retention` board target (power management
    with retention RAM).
-   For build commands per board/app, refer to the per-board `*_README.md` files
    inside each example's `boards/` directory.

---

## 📊 Resource Usage (Code Size)

This section shows the RAM and ROM usage for various Matter examples on Telink
platforms, built with the Matter SDK and Zephyr RTOS.

### Supported Boards

| Board   | Chip Family |
| ------- | ----------- |
| tl3238x | TL323X      |
| tl5218x | TL521X      |
| tl7218x | TL721X      |

### TL323X/TL721X Matter (OTA + LZMA) Code Size

The tables below list the **Supported and Tested** Matter build targets for
TL323X and TL721X, built with OTA + BT DFU + LZMA compression enabled (2MB
Flash, Software Version 1). Memory usage data is extracted from the linker
output (`Memory region` summary) of the build logs.

> 📌 **Memory Regions:** TL323X exposes `RAM_ILM_N` (instruction RAM, 64 KB) +
> `RAM` (data RAM, 96 KB); TL7218X exposes `RAMILM` (unified, 256 KB) or
> `RAM_ILM_N` (128 KB) + `RAM_DLM` (256 KB) depending on the board target. `ROM`
> is the application slot0 partition (1152 KB with LZMA overlay).

#### TL323X (tl3238x)

📈 **Resource Usage Details**

| App                  | Build Target                        | RAM_ILM_N                 | ROM                          | RAM                       |
| -------------------- | ----------------------------------- | ------------------------- | ---------------------------- | ------------------------- |
| **lighting-app**     | `build_tl3238x_2m_flash_lzma`       | 54006 B (82.41% of 64 KB) | 960074 B (81.39% of 1152 KB) | 90464 B (92.02% of 96 KB) |
| **light-switch-app** | `build_tl3238x_retention_lzma`      | 63734 B (97.25% of 64 KB) | 904554 B (76.68% of 1152 KB) | 84496 B (85.95% of 96 KB) |
| **lighting-app**     | `build_tl3238x_concurrent_ota_lzma` | 60168 B (91.81% of 64 KB) | 960080 B (81.39% of 1152 KB) | 90448 B (92.01% of 96 KB) |

#### TL721X (tl7218x)

📈 **Resource Usage Details**

| App                  | Build Target                           | RAMILM / RAM_ILM_N          | RAM_DLM                  | ROM                           | RAM                         |
| -------------------- | -------------------------------------- | --------------------------- | ------------------------ | ----------------------------- | --------------------------- |
| **lighting-app**     | `build_tl7218x_lzma`                   | 93376 B (35.62% of 256 KB)  | —                        | 938542 B (79.56% of 1152 KB)  | 55200 B (21.06% of 256 KB)  |
| **light-switch-app** | `build_tl7218x_retention_lzma`         | 47098 B (35.93% of 128 KB)  | 8346 B (3.18% of 256 KB) | 881487 B (74.72% of 1152 KB)  | 102696 B (78.35% of 128 KB) |
| **lighting-app**     | `build_tl7218x_concurrent_ota_lzma`    | 102072 B (38.94% of 256 KB) | —                        | 950564 B (80.58% of 1152 KB)  | 55360 B (21.12% of 256 KB)  |
| **lighting-app**     | `build_tl7218x_concurrent_cs_ota_lzma` | 146248 B (55.79% of 256 KB) | —                        | 1071138 B (90.80% of 1152 KB) | 124120 B (47.35% of 256 KB) |

> 📌 **Notes:**
>
> -   **Firmware (merged.bin)** = MCUBoot (at offset 0) + gap padding + slot0
>     application image (`zephyr.signed.bin`). This is the file flashed to the
>     device.
> -   All targets fit within the slot0 partition (1152 KB) with LZMA compression
>     enabled.
> -   The LZMA-compressed DFU image (`merged_dfu.lzma.bin`) is smaller than the
>     signed image (e.g. 942630 B → 547530 B for TL7218X lighting).
> -   For a detailed RAM/ROM symbol breakdown, run `west build -t ram_report` /
>     `west build -t rom_report` in the build directory.

### TL521X (tl5218x) — Lighting App Code Size

#### TL521X (tl5218x)

📈 **Resource Usage Details**

| App              | Build Target                 | RAM_ILM_N                  | ROM                          | RAM                        |
| ---------------- | ---------------------------- | -------------------------- | ---------------------------- | -------------------------- |
| **lighting-app** | `build_tl5218x_4m_dual_mode` | 51146 B (39.02% of 128 KB) | 935554 B (47.09% of 1940 KB) | 87872 B (67.04% of 128 KB) |

> 📌 **Note:** TL521X currently only supports lighting-app.

---

### 📝 Additional Notes

-   **Memory Regions:** May vary between chip variants; check individual board
    configurations
-   **Build Config:** Matter builds use `west build` directly from each
    example's `telink` directory; CI builds use the Matter `build_examples.py`
    system with Telink targets
-   **Production Optimizations:** For production builds, disable debug logging
    and enable appropriate optimizations to reduce RAM/ROM usage
-   **Bluetooth &amp; OpenThread:** Matter uses OpenThread for Thread networking
    and BLE for commissioning
-   **OTA Images:** Signed OTA images (`zephyr.signed.bin`) are generated when
    OTA is enabled

---

---

Made by Telink Semiconductor

-   [Website](https://www.telink-semi.com/)
-   [Forum](https://forum.telink-semi.cn/)
-   [Documentation](https://doc.telink-semi.cn/)
