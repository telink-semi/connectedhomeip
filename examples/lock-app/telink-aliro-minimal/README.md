# Telink Matter Lock + Aliro Minimal Integration

This application is an NFC-first integration prototype for a functional Matter
lock on `tl3238x`. Matter remains responsible for Zephyr startup, BLE
commissioning, Thread operation, persistent Matter state, power management, and
the lock application lifecycle. The Aliro SDK is included as a library and uses
the CLRC663 NFC frontend.

## Current Surface

- Matter commissioning over BLE and normal operation over Thread.
- Two Matter fabrics and four ACL entries per fabric.
- Root endpoint clusters needed for commissioning, access control, diagnostics,
  and Thread operation.
- Door endpoint with Identify, Descriptor, and Door Lock.
- Door Lock base, PIN/COTA, User, and Aliro Provisioning features.
- Six users, two PIN credentials, three Aliro issuer keys, and six Aliro
  endpoint keys. Apple Home was observed provisioning three issuer-key users.
- Aliro credential presentation over NFC using CLRC663.
- Simulated lock actuator, lock button, status LEDs, and factory reset.

Aliro BLE/UWB, Matter OTA Requestor, Diagnostic Logs, Software Diagnostics,
User Label, ICD Management, RFID credentials, schedules, door-position
sensing, and unbolting are excluded. Matter BLE commissioning is unchanged;
the Aliro BLE implementation is not linked in this release.

## Local Build

The Aliro source checkout can be used directly during development. The same
consumer target also supports the packaged SDK layout:

```bash
export CHIP_ROOT=/path/to/connectedhomeip
export ZEPHYR_WORKSPACE=/path/to/zephyrproject
source "$CHIP_ROOT/scripts/activate.sh" -p all,telink
cd "$ZEPHYR_WORKSPACE"
west build -b tl3238x \
  "$CHIP_ROOT/examples/lock-app/telink-aliro-minimal" -- \
  -DFETCHCONTENT_SOURCE_DIR_TELINK_ALIRO=/path/to/aliro
```

`FETCHCONTENT_SOURCE_DIR_TELINK_ALIRO` is a standard CMake FetchContent override.
It makes the build use that checkout and skips all network access.

## Published SDK Build

For public builds, publish an immutable source SDK archive containing
`cmake/zephyr`, the public headers and glue sources, NFC sources, and the
prebuilt proprietary libraries. Then build with its URL and SHA-256:

```bash
west build -b tl3238x \
  "$CHIP_ROOT/examples/lock-app/telink-aliro-minimal" -- \
  -DTELINK_ALIRO_SDK_URL=ftp://downloads.example.com/aliro/telink-aliro-sdk-<version>.tar.gz \
  -DTELINK_ALIRO_SDK_SHA256=<archive-sha256>
```

CMake downloads and extracts the SDK under the build directory. Both FTP and
HTTPS archive URLs are supported. The hash pins the exact contents; HTTPS is
preferred when the server supports it because it also authenticates the
transport. The archive layout should match the repository root, so
`cmake/zephyr/CMakeLists.txt` exists after extraction.

## Prototype Limits

- The NFC runtime has been validated on `tl3238x` with CLRC663: Apple Home
  provisioned a Wallet key, Aliro completed a standard NFC transaction, and
  the authenticated endpoint unlocked the Matter lock.
- Matter-provisioned Aliro reader keys and identifiers are applied to the live
  Aliro SDK reader configuration. NFC transactions are rejected until that
  configuration has been received.
- Aliro issuer and endpoint credentials are RAM-only in the Matter delegate.
  Evictable and non-evictable endpoint keys share one six-entry pool, matching
  the combined Matter limit without keeping duplicate key arrays.
- After AUTH1 signature verification, the Aliro protocol core passes the
  authenticated endpoint key to the parent-app authorization callback. Matter
  checks it against both the endpoint-key pool and an occupied Matter user
  before the actuator request can be accepted.
- Aliro transaction-control persistence is intentionally disabled for the
  standard-transaction bring-up. The SDK's standalone NVS backend is not used
  because it targets the same flash partition as Matter settings. Persistent
  and expedited transactions require a Matter-owned storage adapter.
- Power management is disabled while the combined runtime is validated.
- The Aliro NFC task uses a provisional 4 KiB stack and the application reserves
  a provisional 20,716-byte libc arena. Hardware stack and heap high-water
  measurements are required.
- Aliro protocol debug logging is disabled by default because it exposes
  ephemeral and derived transaction keys. It must remain a lab-only option.
- Factory data is disabled and test commissioning credentials are enabled, so
  this profile is for development only.

The `tl3238x` overlay uses the full 160 KiB SRAM as 128 KiB retained RAM and
32 KiB non-retained instruction RAM. These are separate linker regions and
cannot be treated as one interchangeable pool.

Current NFC-first source-build usage on `tl3238x` is 874,340 B of 1 MiB ROM,
130,016 B of 128 KiB retained RAM, and 29,020 B of 32 KiB non-retained
instruction RAM.

The same application built from the packaged Aliro SDK uses 875,072 B of ROM
with identical retained and non-retained RAM usage.
