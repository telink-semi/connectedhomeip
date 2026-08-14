# Matter Telink Lock with Aliro NFC Example Application

The Telink Aliro Minimal Lock Example demonstrates a Matter door lock with
Aliro credential provisioning and NFC access on `tl3238x`. The application is
commissioned over Matter BLE, operates on a Thread network, and uses a CLRC663
NFC frontend for Aliro standard transactions.

Matter owns the device lifecycle, BLE commissioning, Thread networking, and
Matter persistence. The Aliro SDK is linked as a library and is used only for
the NFC access flow in this version.

## Supported devices

| Board/SoC | Build target | NFC frontend | Zephyr Board Info |
| :-------- | :----------- | :----------- | :---------------- |
| TL3238X | `tl3238x` | CLRC663 | [TL3238X](https://github.com/telink-semi/zephyr/tree/telink_aliro_baza_zephyr_4.1.0/boards/telink/tl323x) |

This application has been tested with Telink Zephyr revision
`69f4e4ebf0f607c1808e6f5ff7e91c6f6c531a29`, Telink HAL revision
`ce77c8f74d7e99a75d755dd4c3b43c859cb00b1b`, and Zephyr SDK 0.17.0.

## Implemented functionality

- Matter commissioning over BLE and normal operation over Thread.
- Two Matter fabrics with four access-control entries per fabric.
- A Door Lock endpoint with PIN, COTA, User, and Aliro Provisioning features.
- Six users, two PIN credentials, three Aliro issuer keys, and six combined
  evictable or non-evictable Aliro endpoint keys.
- CLRC663 low-power card detection using a GPIO interrupt, followed by an Aliro
  standard NFC transaction.
- Authorization of the authenticated Aliro endpoint key against an occupied
  Matter lock user before accepting the requested lock action.
- A simulated two-second lock actuator controlled by Matter, NFC, or a button.

Apple Home has been used to commission the device, provision a Home Key in Apple Wallet, and
unlock the simulated Matter lock through the CLRC663 reader.

## Apple Home commissioning and NFC unlock flow

The following sequence is intended to document the tested Apple Home flow from
initial Matter commissioning through an Aliro NFC unlock.

### 1. Add the Matter accessory

Open the Apple Home accessory setup flow and scan the Matter QR code, or enter
the manual setup code. At this stage the lock is discovered through Matter BLE
commissioning.

|  |  |
| :---: | :---: |
| ![Left](./images/photo_1.jpg) | ![Right](./images/photo_2.jpg) |

### 2. Commission the lock, finish accessory and Home Key setup

Apple Home establishes the commissioning session, provisions the Thread
operational credentials, and adds the lock to the selected home.

|  |  |  |
| :---: | :---: | :---: |
| ![Left](./images/photo_3.jpg) | ![Center](./images/photo_4.jpg) | ![Right](./images/photo_5.jpg) |

Complete the accessory name and room selection. Apple Home then provisions the
Aliro reader configuration, issuer credentials, endpoint credentials, and lock
users required for Home Key operation.
|  |  |
| :---: | :---: |
| ![Left](./images/photo_6.jpg) | ![Right](./images/photo_7.jpg) |

### 3. Confirm the commissioned lock

Confirm that the lock appears in Apple Home and is reachable on the network.
|  |  |
| :---: | :---: |
| ![Left](./images/photo_8.jpg) | ![Right](./images/photo_9.jpg) |

### 4. Unlock with the Home Key in Apple Wallet over NFC

Present the iPhone containing the Home Key to the CLRC663 NFC reader. This is an Aliro NFC transaction; Matter BLE is not used. The application authenticates the endpoint key, verifies that it belongs to an occupied Matter lock user, performs the requested unlock action, and reports the updated Door Lock state over Thread. Apple Wallet confirms the transaction, and Apple Home displays the lock as unlocked.

|  |  |
| :---: | :---: |
| ![Left](./images/photo_10.jpg) | ![Right](./images/photo_11.jpg) |

## Build and flash

Build flow of this application doesn't change much from any other Telink example apps.
Initial Matter/Zephyr environment setup is mostly the same as in [Telink Developer's Guide](https://doc.telink-semi.cn/doc/en/software/res/sdk/matter/telink_matter_developer_guide_en/).
You just need to checkout specific branch of Zephyr using current Matter revision.

1. Prepare the connectedhomeip and Telink Zephyr build environment. The Zephyr
   workspace must include the `tl3238x` board and the Telink HAL revisions noted
   above.

2. Activate the Matter build environment from the connectedhomeip root:

    ```bash
    source scripts/activate.sh -p all,telink
    ```

3. Build the application from `examples/lock-app/telink-aliro-minimal`:

    ```bash
    west build -p always -b tl3238x
    ```

   CMake downloads the Aliro SDK archive configured by
   `TELINK_ALIRO_SDK_URL`, extracts it under `build/_deps`, and links the
   `Telink::Aliro` target. The resulting image is `build/zephyr/merged.bin`.

   In case you want to use another published SDK archive:

    ```bash
    west build -p always -b tl3238x -- \
      -DTELINK_ALIRO_SDK_URL=https://server/path/telink-aliro-sdk.tar.gz
    ```

   To build against a local Aliro source or SDK checkout without downloading an
   archive:

    ```bash
    west build -p always -b tl3238x -- \
      -DFETCHCONTENT_SOURCE_DIR_TELINK_ALIRO=/absolute/path/to/aliro
    ```

4. Flash the generated `merged.bin` using the TL3238X flashing procedure. The
   current TL3238X Zephyr board documentation does not enable a `west flash`
   runner.

## Hardware connections

The application devicetree overlay configures the CLRC663 as follows:

| CLRC663 signal | TL3238X pin |
| :------------- | :---------- |
| SPI chip select | PE4 |
| SPI clock | PE5 |
| SPI MISO | PE6 |
| SPI MOSI | PE7 |
| IRQ | PA5 |
| Reset | PA6 |

Power and ground must match the CLRC663 board being used.

## Usage

### UART

The Zephyr console uses UART0 at 115200 baud, 8 data bits, no parity, and one
stop bit.

| Signal | TL3238X pin |
| :----- | :---------- |
| TX | PB2 |
| RX | PB0 |
| GND | GND |

### Buttons

| Name | Function | Description |
| :--- | :------- | :---------- |
| User KEY1 | Factory reset | Press three times within three seconds to erase the commissioned Matter state. |
| User KEY2 | Lock control | Toggle the simulated lock between locked and unlocked. |

### LEDs

| LED | Function | Description |
| :-- | :------- | :---------- |
| White | Matter network status | Short pulse while uncommissioned, fast blink while joining, and long pulse while attached to Thread. |
| Green | Lock state | On when locked, off when unlocked, and fast blink while the actuator is moving. |

### Commission with CHIP Tool

Build the [CHIP Tool](../../chip-tool/README.md), then commission the device over
BLE with a Thread operational dataset:

```bash
${CHIP_TOOL_DIR}/chip-tool pairing ble-thread \
  ${NODE_ID} hex:${THREAD_DATASET} ${PIN_CODE} ${DISCRIMINATOR}
```

Lock, unlock, or read the lock state on endpoint 1:

```bash
${CHIP_TOOL_DIR}/chip-tool doorlock lock-door ${NODE_ID} 1
${CHIP_TOOL_DIR}/chip-tool doorlock unlock-door ${NODE_ID} 1
${CHIP_TOOL_DIR}/chip-tool doorlock read lock-state ${NODE_ID} 1
```

### Aliro NFC access

An ecosystem must first commission the Matter device and provision the Aliro
reader configuration, issuer key, endpoint key, and corresponding lock user.
Until the reader configuration is received, NFC transactions are rejected.

After provisioning, present the matching NFC credential to the CLRC663 reader.
The application accepts the requested lock action only when the transaction is
authenticated and its endpoint key belongs to an occupied Matter user.

## Current limitations

- Only Aliro NFC standard transactions are enabled. Aliro BLE/UWB, expedited
  transactions, and keyslot credentials are not included.
- Aliro issuer keys, endpoint keys, and reader configuration are stored in RAM
  only and are lost on reboot. Matter settings remain persistent.
- Aliro transaction-control persistence is disabled because the standalone SDK
  NVS backend targets the same flash storage used by Matter. A Matter-owned
  storage adapter is still required.
- Power management and Matter OTA Requestor support are disabled.
- The lock actuator is simulated; no physical bolt or door-position sensor is
  controlled by this application.
- Factory data is disabled and development commissioning credentials are used.
  This is not a certification or production configuration.

The TL3238X application overlay exposes 128 KiB of retained RAM and 32 KiB of
non-retained instruction RAM as separate linker regions. The Aliro NFC thread
currently uses a provisional 4 KiB stack; runtime stack and heap high-water
measurements are still required.

The default SDK URL currently identifies a test archive and is not accompanied
by a content hash. A public release should use an immutable, versioned archive
name and pin its contents before this build is treated as reproducible.
