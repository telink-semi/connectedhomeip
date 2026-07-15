# Telink Matter SDK Release Note

[![Version](https://img.shields.io/badge/Version-tl_v1.0.1--rc1--v1.5.1.0-blue?style=flat-square)](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.0.1-rc1-v1.5.1.0)
[![License](https://img.shields.io/badge/License-Apache%202.0-red?style=flat-square)](LICENSE)
[![Matter](https://img.shields.io/badge/Matter-v1.5-green?style=flat-square)](https://github.com/project-chip/connectedhomeip/commit/f4a8cf98ada4ad4f439b45e360800693cc5f1391)

---

-   **Release Type:** Pre-Release (rc1)
-   **Branch:**
    [release-v1.0-v1.5-branch](https://github.com/telink-semi/connectedhomeip/tree/release-v1.0-v1.5-branch)
-   **Tag Version:**
    [tl_v1.0.1-rc1-v1.5.1.0](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.0.1-rc1-v1.5.1.0)
-   **Target Commit:**
    [b4c04e3](https://github.com/telink-semi/connectedhomeip/commit/b4c04e3c1816fc242a100e305047ac1350457d17)

---

## 📖 Introduction

This release is based on the latest commit of `release-v1.0-v1.5-branch` branch, providing
Matter protocol support for Telink RISC-V SoC platforms. It integrates the
Matter SDK with the Telink Zephyr SDK to enable Matter-over-Thread devices on
Telink chips including TLSR9 Series, TL321X, TL323X, and TL721X.

---

## ✨ Highlights

| Category            | Details                                                |
| ------------------- | ------------------------------------------------------ |
| **Matter Support**  | Matter 1.5.1 protocol stack                            |
| **Supported Chips** | TLSR951X, TLSR952X, TLSR911X, TL321X, TL323X, TL721X   |
| **Sample Apps**     | Lighting, Light Switch, Lock, Bridge, Thermostat, etc. |
| **OTA**             | OTA requestor and compress-LZMA support                |
| **Factory Data**    | Factory data provisioning support                      |

---

## 🆕 New Features

-   ✅ Matter 1.5.1 protocol support for Telink platforms
-   ✅ Full support for TL323X series chips in Matter
-   ✅ Support for TL721X series chips in Matter
-   ✅ LZMA compression support for OTA images
-   ✅ Factory data provisioning support
-   ✅ Dual-mode configuration support for TL3238X
-   ✅ Power management with retention RAM support
-   ✅ NFC payload support for all-clusters-minimal-app
-   ✅ DFU over SMP support for lock-app

---

## 🐛 Bug Fixes

| Issue             | Description                                                                   |
| ----------------- | ----------------------------------------------------------------------------- |
| **Network State** | Check Matter network state when power-on to ensure correct commissioning flow |
| **TL721X HAL V2** | Update hal_v1 to hal_v2 for TL721X                                            |
| **PM Stability**  | Improve power management stability on retention RAM configurations            |
| **OTA Recovery**  | Fix OTA recovery flow on TL323X series                                        |

---

## 📦 Updates

-   Updated Telink BLE SDK for improved RF performance
    ([commit:46322e5b570e2a68373b18d4f08811acadd1266c](https://github.com/telink-semi/tl_ble_sdk_zephyr/commit/46322e5b570e2a68373b18d4f08811acadd1266c))
-   Updated Telink HAL Zephyr to support TL721X
    hal_v2([commit:14c6149f6cc466c49d81e3b2f7f1e4d8ff6fbbb5](https://github.com/telink-semi/hal_telink/commit/14c6149f6cc466c49d81e3b2f7f1e4d8ff6fbbb5))
-   Updated MCUBoot with Telink-specific flash operation
    ([commit:ce0da85c39c749df49b0ec62b33d2ecdea24c927](https://github.com/telink-semi/mcuboot/commit/ce0da85c39c749df49b0ec62b33d2ecdea24c927))
-   Updated OpenThread Telink source code
    ([commit:542aaab44e1308e1a8a24573dfbd413fade342ee](https://github.com/telink-semi/openthread/commit/542aaab44e1308e1a8a24573dfbd413fade342ee))
-   Updated OpenThread Telink library
    ([commit:308dae2f80084f87073cfd4fbd30f1be0799be7b](https://github.com/telink-semi/openthread_telink_lib/commit/308dae2f80084f87073cfd4fbd30f1be0799be7b))
-   Updated Telink Zephyr SDK to support TL323X and TL721X hal_v2
    ([commit:e08fc42546e58d808bfd39f35c8df296f5617a44](https://github.com/telink-semi/zephyr/commit/e08fc42546e58d808bfd39f35c8df296f5617a44))

---

## ⚠️ Important Notes

**This is a pre-release version for demonstration, development and testing purposes. Not
recommended for production use.**

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

| Property          | Version                                                                                                        |
| ----------------- | -------------------------------------------------------------------------------------------------------------- |
| **Branch**        | [release-v1.0-v1.5-branch](https://github.com/telink-semi/connectedhomeip/tree/release-v1.0-v1.5-branch)                               |
| **Target Commit** | [b4c04e3](https://github.com/telink-semi/connectedhomeip/commit/b4c04e3c1816fc242a100e305047ac1350457d17)      |
| **Tag Name**      | [tl_v1.0.1-rc1-v1.5.1.0](https://github.com/telink-semi/connectedhomeip/releases/tag/tl_v1.0.1-rc1-v1.5.1.0) |
| **Release Type**  | Pre-Release (rc1)                                                                                             |

### Chip &amp; Hardware Versions

📦 **Chip Versions**

| Chip Family            | Versions |
| ---------------------- | -------- |
| TLSR921X/TLSR951X(B91) | A2       |
| TLSR922X/TLSR952X(B92) | A3/A4    |
| TLSR911X(W91)          | A2       |
| TL721X                 | A2/A3    |
| TL321X                 | A1/A2/A3 |
| TL323X                 | A0       |

🔧 **Hardware EVK Versions**

| Chip     | EVK Version                   |
| -------- | ----------------------------- |
| TLSR921X | C1T213A20_V1.3                |
| TLSR952X | C1T266A20_V1.3                |
| TLSR911X | C1T301A20_V2.1                |
| TL721X   | C1T315A20_V1.2                |
| TL321X   | C1T331A20_V1.0/C1T335A20_V1.3 |
| TL323X   | C1T388A20_V1.1                |

---

## 📱 Matter Community Examples

This release provides Telink platform ports for the following Matter community
example applications. Each example is located under
`examples/<app-name>/telink/` in the repository. The table below summarizes each
example and the Telink chip platforms that support it (see
[Telink Examples YAML file](.github/workflows/examples-telink.yaml)).

> ✅ = Supported and Tested &nbsp;&nbsp; 🟡 = Supported but Untested
> &nbsp;&nbsp; · = Untested

| Example                  | Description                                                | B91 (TLSR951X) | B92 (TLSR952X) | W91 (TLSR911X) | TL321X | TL323X | TL721X |
| ------------------------ | ---------------------------------------------------------- | :------------: | :------------: | :------------: | :----: | :----: | :----: |
| lighting-app             | Lighting (On/Off, Level, Color Control)                    |       🟡       |       ·        |       🟡       |   🟡   |   ✅   |   ✅   |
| light-switch-app         | Light Switch (controller, switches a bound lighting-app)   |       ·        |       🟡       |       ·        |   🟡   |   ✅   |   ✅   |
| bridge-app               | Bridge / Aggregator (bridges non-Matter devices to Matter) |       ·        |       ·        |       ·        |   ·    |   ·    |   🟡   |
| window-app               | Window Covering (shades, blinds)                           |       ·        |       ·        |       ·        |   ·    |   ·    |   🟡   |
| air-quality-sensor-app   | Air Quality Sensor                                         |       ·        |       🟡       |       ·        |   ·    |   ·    |   ·    |
| all-clusters-app         | All Clusters (full-featured test app)                      |       ·        |       ·        |       🟡       |   ·    |   ·    |   ·    |
| all-clusters-minimal-app | All Clusters Minimal (lightweight test app, NFC payload)   |       ·        |       🟡       |       ·        |   ·    |   ·    |   ·    |
| contact-sensor-app       | Contact Sensor (stateless contact sensing)                 |       ·        |       🟡       |       ·        |   ·    |   ·    |   ·    |
| lock-app                 | Door Lock (supports DFU over BLE SMP)                      |       ·        |       🟡       |       ·        |   ·    |   ·    |   ·    |
| smoke-co-alarm-app       | Smoke &amp; CO Alarm                                       |       ·        |       🟡       |       ·        |   ·    |   ·    |   ·    |
| pump-controller-app      | Pump Controller                                            |       🟡       |       ·        |       ·        |   ·    |   ·    |   ·    |
| shell                    | Shell (debug console)                                      |       🟡       |       ·        |       ·        |   ·    |   ·    |   ·    |
| thermostat               | Thermostat                                                 |       ·        |       ·        |       🟡       |   ·    |   ·    |   ·    |
| ota-requestor-app        | OTA Requestor (handles OTA image download)                 |       ·        |       ·        |       ·        |   🟡   |   ·    |   ·    |

### Notes on Platform Support

-   **Tested combinations (✅):** Only TL323X and TL721X with lighting-app and
    light-switch-app have been fully tested in this release.
-   **Supported but untested (🟡):** All other build targets listed in the table
    are compiled successfully but have not been functionally validated in this
    release. Use with caution.
-   **Untested (·):** Combinations not listed are not built or validated in this
    release.
-   **B91 (TLSR951X)** and **W91 (TLSR911X)** are legacy platforms with broad
    sample coverage (lighting, pump-controller, shell, thermostat,
    all-clusters).
-   **B92 (TLSR952X)** targets sensors and small appliances (air-quality,
    contact-sensor, smoke-co-alarm, lock, light-switch, all-clusters-minimal).
-   **TL321X / TL323X / TL721X** are the latest Telink RISC-V SoC families.
    Lighting and light-switch apps are supported across all three; TL721X
    additionally supports bridge-app and window-app.
-   The light-switch-app on TL321X/TL323X/TL721X uses the `*_retention` board
    target (power management with retention RAM).
-   For build commands per board/app, refer to the per-board `*_README.md` files
    inside each example's `boards/` directory.

---

## 📊 Resource Usage (Code Size)

This section shows the RAM and ROM usage for various Matter examples on Telink
platforms, built with the Matter SDK and Zephyr RTOS.

### Supported Boards

| Board          | Chip Family  |
| -------------- | ------------ |
| tlsr9518adk80d | TLSR951X/B91 |
| tlsr9528a      | TLSR952X/B92 |
| tl3218x        | TL321X       |
| tl3238x        | TL323X       |
| tl7218x        | TL721X       |
| tlsr9118bdk40d | TLSR911X/W91 |

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

| App                  | Build Target                      | RAM_ILM_N                 | ROM                          | RAM                       | Firmware (merged.bin) |
| -------------------- | --------------------------------- | ------------------------- | ---------------------------- | ------------------------- | --------------------- |
| **lighting-app**     | `build_tl3238x_2m_flash_lzma_v1`  | 51788 B (79.02% of 64 KB) | 960663 B (81.44% of 1152 KB) | 92552 B (94.15% of 96 KB) | 1047015 B             |
| **light-switch-app** | `build_tl3238x_retention_lzma_v1` | 61558 B (93.93% of 64 KB) | 905054 B (76.72% of 1152 KB) | 86608 B (88.10% of 96 KB) | 991406 B              |

#### TL721X (tl7218x)

📈 **Resource Usage Details**

| App                  | Build Target                      | RAMILM / RAM_ILM_N         | RAM_DLM                   | ROM                          | RAM                         | Firmware (merged.bin) |
| -------------------- | --------------------------------- | -------------------------- | ------------------------- | ---------------------------- | --------------------------- | --------------------- |
| **lighting-app**     | `build_tl7218x_2m_flash_lzma_v1`  | 96432 B (36.79% of 256 KB) | —                         | 942294 B (79.88% of 1152 KB) | 55936 B (21.34% of 256 KB)  | 1028646 B             |
| **light-switch-app** | `build_tl7218x_retention_lzma_v1` | 46834 B (35.73% of 128 KB) | 10026 B (3.82% of 256 KB) | 886204 B (75.12% of 1152 KB) | 104616 B (79.82% of 128 KB) | 972556 B              |

> 📌 **Notes:**
>
> -   **Firmware (merged.bin)** = MCUBoot (at offset 0) + gap padding + slot0
>     application image (`zephyr.signed.bin`). This is the file flashed to the
>     device.
> -   All four targets fit within the slot0 partition (1152 KB) with LZMA
>     compression enabled.
> -   The LZMA-compressed DFU image (`merged_dfu.lzma.bin`) is smaller than the
>     signed image (e.g. 942630 B → 547530 B for TL7218X lighting).
> -   For a detailed RAM/ROM symbol breakdown, run `west build -t ram_report` /
>     `west build -t rom_report` in the build directory.

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
