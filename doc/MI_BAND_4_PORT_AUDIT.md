# Mi Band 4 Port Audit (2026-10-10)

This document records a source-level audit of the current `main` branch. It distinguishes source/configuration evidence from behavior that still requires a physical device. No firmware was flashed as part of this audit.

## Current build evidence

- GitHub Actions workflow: [Mi Band 4 display smoke-test build](https://github.com/herotux/ZSband/actions/workflows/mi-band-4-build.yml)
- Last reviewed successful run: [38060893065](https://github.com/herotux/ZSband/actions/runs/38060893065)
- The run compiled the `mi_band_4/da14697` display smoke-test application and uploaded build outputs.
- This proves that the current sample and driver compile in that CI environment. It does **not** prove that the display, touch, BLE, or sensors work on a physical band.

## Hardware and source status

| Area | Evidence in source | Status / limitation |
|---|---|---|
| SoC / board | DA14697 board definition, clocks, GPIO, UART, I2C and QSPI definitions | Board description exists; electrical behavior is not validated by CI |
| Display | `chosen.zephyr,display = &rm69310`; RM69310 node is 120×240 RGB565; `drivers/display/display_rm69xxx.c` is included by the driver's CMake/Kconfig | Compile-tested by the display smoke-test. Panel initialization, offsets, colors and stability still need hardware validation |
| Bluetooth controller | `chosen.zephyr,bt-hci = &bt_hci_da1469x`; controller node enabled | Controller is described in DTS; no end-to-end phone pairing or GATT application is demonstrated by this sample |
| Touch | Board documentation identifies a Zinitix I2C touch controller | No Zinitix driver or active touch-controller node was found in the current source tree |
| IMU | Documentation identifies a Bosch BMI260; datasheet folder is named BMI270 | No BMI260 driver or active IMU node was found. Confirm exact silicon identity before choosing a driver |
| Heart-rate AFE | AFE4900 source, Kconfig, CMake and binding files exist | Driver source exists, but there is no active AFE4900 node in the Mi Band 4 board DTS and no sensor test in CI |
| Proximity / wear | IQS620A source, Kconfig, CMake and binding files exist | Driver source exists, but there is no active IQS620A node in the Mi Band 4 board DTS and no sensor test in CI |
| Power / charging | Board documentation lists power management and charging as unsupported | No validated battery/charging path is demonstrated |
| User interface / Persian | No product UI or text rendering application is present in the current sample | Not implemented yet |

## Important risks and gaps

1. **The existing sample is a compile smoke-test, not a product firmware.** It attempts to initialize the display and submit six RGB565 color bands, but those operations have not been observed on hardware.
2. **The driver reports RGB888 as supported as well as RGB565.** The current test only uses RGB565; hardware validation should start with that format.
3. **The board DTS sets an X offset of 4.** Confirm the panel's visible area and addressing behavior on the actual device before changing offsets.
4. **The manifest tracks Zephyr `main`.** That is moving and can make later builds change without a repository commit. After the initial bring-up is stable, pin Zephyr to a known commit/tag and update it deliberately.
5. **Do not flash an artifact based only on a green build.** First arrange a recoverable SWD/J-Link procedure and verify the exact image format, partitioning and boot assumptions.

## Recommended implementation order

1. Keep the current build as a non-flashing CI check and preserve its artifacts.
2. Add stronger compile-time checks and log build metadata/configuration so failures are diagnosable.
3. Prepare a hardware validation checklist for the display: safe recovery path, display-ready log, RGB565 color bands, orientation/offset, and repeatable cold boots.
4. Only after display validation, investigate BLE services and phone communication.
5. Add missing touch and IMU support after confirming exact chip identity and board wiring.
6. Enable/test the existing AFE4900 and IQS620A drivers only after confirming their bus connections, addresses, interrupt pins and power requirements.
7. Build the UI and Persian text/notification support on top of verified display and input primitives.

## Scope of this audit

This is a source/configuration audit based on the repository tree, board DTS, board documentation, driver build files and the successful CI run. Items marked missing mean they were not found in the current repository source/configuration reviewed here; they are not claims that the hardware lacks those components.
