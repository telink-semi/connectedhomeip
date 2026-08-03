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
- Door Lock base, User, and Aliro Provisioning features.
- Two users, two Aliro issuer keys, and six Aliro endpoint keys.
- Aliro credential presentation over NFC using CLRC663.
- Simulated lock actuator, lock button, status LEDs, and factory reset.

Aliro BLE/UWB, Matter OTA Requestor, Diagnostic Logs, Software Diagnostics,
User Label, ICD Management, PIN/RFID credentials, schedules, door-position
sensing, and unbolting are excluded. Matter BLE commissioning is unchanged;
the Aliro BLE implementation is not linked in this release.

## Local Build

The current SDK checkout can be used directly while the public release archive
is being prepared:

```bash
export CHIP_ROOT=/path/to/connectedhomeip
export ZEPHYR_WORKSPACE=/path/to/zephyrproject
source "$CHIP_ROOT/scripts/activate.sh" -p all,telink
cd "$ZEPHYR_WORKSPACE"
west build -b tl3238x \
  "$CHIP_ROOT/examples/lock-app/telink-aliro-minimal" -- \
  -DFETCHCONTENT_SOURCE_DIR_TELINK_ALIRO=/path/to/telink_aliro
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

- The NFC runtime is wired to the Matter lock action path, but still needs
  validation on `tl3238x` hardware with CLRC663.
- Matter accepts Aliro provisioning commands through its Door Lock delegate,
  but those keys are currently RAM-only and are not yet transferred into the
  Aliro SDK reader/key store. This is the main functional integration gap.
- The Aliro SDK currently starts with its built-in reader defaults. The public
  SDK interface still needs explicit reader configuration and credential-store
  callbacks before ecosystem-provisioned keys can drive NFC access.
- Power management is disabled while the combined runtime is validated.
- The Aliro NFC task uses a provisional 4 KiB stack and the application reserves
  a provisional 16 KiB libc arena. Hardware stack and heap high-water
  measurements are required.
- Factory data is disabled and test commissioning credentials are enabled, so
  this profile is for development only.

The `tl3238x` overlay uses the full 160 KiB SRAM as 128 KiB retained RAM and
32 KiB non-retained instruction RAM. These are separate linker regions and
cannot be treated as one interchangeable pool.

Current NFC-first link usage on `tl3238x` is 906,658 B of 1 MiB ROM, 119,648 B
of 128 KiB retained RAM, and 29,134 B of 32 KiB non-retained instruction RAM.
