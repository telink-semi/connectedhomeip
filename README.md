# Telink Matter SDK

[![Telink Website](https://img.shields.io/badge/Website-Telink-blue?style=flat-square)](https://www.telink-semi.com/)
[![Forum](https://img.shields.io/badge/Forum-Telink-green?style=flat-square)](https://forum.telink-semi.cn/)
[![License](https://img.shields.io/badge/License-Apache%202.0-red?style=flat-square)](LICENSE)
[![Matter](https://img.shields.io/badge/Matter-v1.5-green?style=flat-square)](https://github.com/project-chip/connectedhomeip)

---

> 📖 This is the README for the **Telink Matter SDK** (fork). For the upstream
> Matter (Project CHIP) README, see [MATTER_README.md](MATTER_README.md).

**A Matter protocol implementation for Telink RISC-V SoC platforms based on the
Connected Home over IP (CHIP) project**

-   For development environment setup, SDK acquisition, and quick-start
    instructions, refer to the
    [Telink Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/).
-   Specifically, check Chapter **Obtaining Matter Source Code** as the initial
    version of Getting Started in the
    [Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/#experience-the-implementation-and-function-of-matter).
-   For a detailed list of supported devices and resource usage, refer to the
    [Release Note](tl_matter_sdk_release_note.md)

---

## 📖 SDK Introduction

Telink Matter SDK is a software development platform that implements the Matter
protocol on Telink RISC-V SoC platforms. It is built on top of the Connected
Home over IP (CHIP) project and integrates with the Telink Zephyr SDK to provide
complete Matter-over-Thread support for Telink chips.

### What's Included

-   ✅ Matter 1.5.1 protocol stack
-   ✅ Telink Zephyr RTOS integration
-   ✅ Thread networking via OpenThread
-   ✅ BLE commissioning support
-   ✅ OTA firmware update support
-   ✅ Factory data provisioning
-   ✅ Power management with retention RAM
-   ✅ MCUboot bootloader integration
-   ✅ Multiple sample applications

### Supported Examples

| Example Type           | Description                                                                                                                 |
| ---------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| **Matter over Thread** | Lights, switches, locks, sensors, thermostats, bridges, Smoke/CO alarms, pumps, air quality sensors, window coverings, etc. |
| **Development Tools**  | Shell, OTA requestor, all-clusters test apps                                                                                |

---

## 🚀 Quick Reference

| Resource              | Description                                | Link                                                                                                                                                                         |
| --------------------- | ------------------------------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Release Note**      | Telink Matter SDK Changelog & New Features | [Telink Matter SDK Release Note](tl_matter_sdk_release_note.md)                                                                                                              |
| **Get Started Guide** | SDK Quick Start Guide & Developer Handbook | Chapter **Obtaining Matter Source Code** in the [Telink Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/) |
| **Examples**          | Matter Sample Applications                 | See [examples](examples/) (filter by `telink`)                                                                                                                               |
| **Dependency**        | Telink Zephyr SDK                          | [Telink Zephyr SDK](https://github.com/telink-semi/zephyr/blob/dev-tlk_v4.1/README.md)                                                                                       |

---

## 📚 Additional Resources

| Type                            | Resource                                                                                                                     |
| ------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| 🌐 **Telink Official Website**  | [Telink - Chips for a Smarter IoT](https://www.telink-semi.com/)                                                             |
| 💬 **Telink Forum**             | [Telink Technical Support](https://forum.telink-semi.cn/)                                                                    |
| 📖 **Documentation**            | [Telink Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/) |
| 📦 **Matter Community Project** | [Connected Home over IP](https://github.com/project-chip/connectedhomeip)                                                    |

---

## 🔧 Environment Setup

### Prerequisites

The Telink Matter SDK is built **on top of** the Telink Zephyr SDK. They are
tightly coupled and must be installed as a matched pair. Set up the Telink
Zephyr SDK **before** the Matter SDK.

#### Dependency Overview

| Component                                                | Role                                                                                                               | Required |
| -------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------ | :------: |
| **Telink Zephyr SDK** (`dev-tlk_v4.1` branch)            | Provides the Zephyr RTOS core, Telink HAL, BLE stack, MCUBoot, OpenThread, the WEST tool and build toolchain       |    ✅    |
| **Telink Matter SDK** (`dev-tlk_v1.5` branch, this repo) | Provides Matter protocol stack and Telink Matter examples; consumes the Telink Zephyr SDK via `TELINK_ZEPHYR_BASE` |    ✅    |

#### Setup Steps

The setup follows the standard Zephyr "Environment Setup (Manual)" flow,
tailored for Telink. A full walkthrough (with host package versions and
screenshots) is in Chapter **Environment Setup (Manual)** of the
[Telink Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/);
the high-level steps are:

1. **Set up the host (Ubuntu 20.04/22.04 LTS)** — install the OS packages
   required by Zephyr and the Matter build: `git`, `python3` + `pip` + `venv`,
   `make`, `gcc`, `g++`, `cmake`, `dfu-util`, `device-tree-compiler`,
   `libssl-dev`, `libffi-dev`, `libudev-dev`, `usbutils` and the `curl`/`wget`
   downloaders. Add the user to the `dialout`/`plugdev` groups and install the
   Telink USB udev rules so the programmer is reachable without `sudo`.

2. **Install `west`** — Zephyr's multi-repository meta-tool. It is pulled in as
   a Python package and must be available on `PATH` before any `west` command is
   used.

3. **Get the Telink Zephyr SDK** — `west init` the Zephyr manifest repository
   and then check out Telink SDK (branch `dev-tlk_v4.1`) . Run `west update` to
   fetch Zephyr itself, the Telink HAL, OpenThread, MCUBoot. Download and
   install Zephyr SDK toolchain (which provides the `riscv64-zephyr-elf` GCC
   cross-compiler). See Chapter **Install Zephyr Project Environment** in the
   [Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/)
   for the exact `west` commands.

4. **Fetch the Telink BLE SDK binary** — `west update` does **not** fetch
   `tl_ble_sdk` automatically (Zephyr CI disallows binary modules). Run
   `./hal_v2/fetch_sdk.sh` inside `modules/hal/telink/` of the Zephyr SDK to
   download the pre-built BLE stack.

5. **Get the Telink Matter SDK (this repo)** — clone the `dev-tlk_v1.5` branch
   of this repository next to the Zephyr SDK.

6. **Point the Matter SDK at the Zephyr SDK** — export `TELINK_ZEPHYR_BASE` so
   the Matter build system can find Zephyr, the Telink HAL and the toolchain.
   This variable must be set in every shell used for Matter builds.

7. **Bootstrap & activate the Matter build environment** — run the Matter
   `scripts/activate.sh` bootstrap once to install `gn`, `ninja`, `pigweed` and
   the Python dependencies, then source it in every new shell before building.

> 💡 The exact shell commands for each step (apt/pip/west/git/export) are listed
> in the **Environment Setup (Manual)** chapter of the
> [Telink Matter Developer Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/).
> Follow that guide to avoid version mismatches.

### Building Examples

Matter builds use `west build` directly from each example's `telink` directory.
Each supported board ships a dedicated `*_README.txt` under
`examples/<app>/telink/boards/` that lists the exact build commands for every
configuration of that board (default, OTA + LZMA, dual-mode Matter + Zigbee, 4
MB flash, Software Version 2 for DFU/OTA images, etc.).

Refer to the board README for the app + board combination you want to build:

#### Lighting App (TL3238X / TL7218X)

-   TL3238X:
    [`examples/lighting-app/telink/boards/tl3238x_README.txt`](examples/lighting-app/telink/boards/tl3238x_README.txt)
-   TL7218X:
    [`examples/lighting-app/telink/boards/tl7218x_README.txt`](examples/lighting-app/telink/boards/tl7218x_README.txt)

#### Light Switch App (TL3238X Retention / TL7218X Retention)

-   TL3238X Retention:
    [`examples/light-switch-app/telink/boards/tl3238x_retention_README.txt`](examples/light-switch-app/telink/boards/tl3238x_retention_README.txt)
-   TL7218X Retention:
    [`examples/light-switch-app/telink/boards/tl7218x_retention_README.txt`](examples/light-switch-app/telink/boards/tl7218x_retention_README.txt)

> **Note:** LZMA compression is **required** for 2 MB flash with OTA. Build
> Software Version 2 (via the flag documented in each `*_README.txt`) to
> generate the DFU/OTA upgrade images (`merged_dfu.lzma.bin`, `matter.ota`). The
> same `*_README.txt` files also cover dual-mode (Matter + Zigbee) and 4 MB
> flash configurations.

### Flashing Firmware

For developing and testing purposes, refer to Chapter **Firmware Burning** in
our
[Matter Develop Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/?h=bdt#experience-the-implementation-and-function-of-matter).

---

## 📱 Supported Boards

| Board          | Chip Family | Series | Status               |
| -------------- | ----------- | ------ | -------------------- |
| tlsr9518adk80d | TLSR951X    | B91    | Telink Zephyr HAL_V1 |
| tlsr9528a      | TLSR952X    | B92    | Telink Zephyr HAL_V1 |
| tlsr9118bdk40d | TLSR911X    | W91    | -                    |
| tl3218x        | TL321X      | -      | Telink Zephyr HAL_V1 |
| tl3238x        | TL323X      | -      | Telink Zephyr HAL_V2 |
| tl7218x        | TL721X      | -      | Telink Zephyr HAL_V2 |

---

## 📝 Release Information

For version history and detailed changelog, refer to the
[Release Note](tl_matter_sdk_release_note.md).

---

## 📄 License

```
Apache License, Version 2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at:

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

---

Made by Telink Semiconductor
