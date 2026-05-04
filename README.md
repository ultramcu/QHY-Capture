# qhy_capture

A minimal **single-frame capture demo** for [QHYCCD](https://www.qhyccd.com)
astronomy CCD / CMOS cameras, written in plain C99.

The program connects to the first QHY camera on the USB bus, configures
exposure / gain / offset, takes one full-resolution exposure, and writes
the captured frame to disk as a [PGM](https://en.wikipedia.org/wiki/Netpbm#PGM_example)
(mono) or [PPM](https://en.wikipedia.org/wiki/Netpbm#PPM_example)
(color) image. No GUI, no Qt, no OpenCV — just `libqhyccd`, `libpthread`,
`libdl`.

It's the smallest end-to-end "did my hardware work?" sanity check for a
new QHY install.

## Prerequisites

| | |
|---|---|
| **Compiler** | `gcc` or `clang` (C99) |
| **OS** | Linux or macOS |
| **Camera** | Any QHYCCD USB camera supported by the QHYCCD SDK |
| **Library** | `libqhyccd` (the QHYCCD SDK), `libpthread`, `libdl` |

### 1 — Install the QHYCCD SDK

The SDK is **not** in Homebrew or apt. Download it from QHYCCD's official
release page and run the bundled installer:

- **Linux**: <https://www.qhyccd.com/download/> → "QHYCCD SDK for Linux"
  ```sh
  tar xzf sdk_linux64_<version>.tgz
  cd sdk_linux64_<version>
  sudo ./install.sh
  ```
  The installer drops headers in `/usr/local/include/libqhy/` and
  shared objects in `/usr/local/lib/`. It also installs the udev rules
  needed to access the camera as a non-root user — re-plug the USB
  cable after `install.sh` finishes.

- **macOS**: <https://www.qhyccd.com/download/> → "QHYCCD SDK for Mac"
  ```sh
  tar xzf sdk_mac_<version>.tgz
  cd sdk_mac_<version>
  sudo ./install.sh
  ```
  Headers go to `/usr/local/include/libqhy/`, dylibs to
  `/usr/local/lib/`. On older SDK packages the dylib is named
  `libqhy.dylib` rather than `libqhyccd.dylib`; see the build notes
  below.

### 2 — Verify the install

```sh
ls /usr/local/include/libqhy/qhyccd.h        # header is in place
ls /usr/local/lib/libqhyccd.*                # libqhyccd.so / .dylib
```

If your SDK lives under a different prefix (e.g. `/opt/qhyccd`), pass
it to `make` via `QHY_PREFIX=/opt/qhyccd` (see below).

## Build

```sh
make
```

That produces a single executable, `./qhy_capture`. Override the SDK
location with:

```sh
make QHY_PREFIX=/opt/qhyccd
```

If the linker reports `library not found for -lqhyccd`, you're on an
older macOS SDK that ships the dylib as `libqhy.dylib`. Build with:

```sh
make LDLIBS="-lqhy -lpthread -ldl"
```

## Run

```sh
./qhy_capture                               # defaults: 20 ms, gain 30, frame.pgm
./qhy_capture -e 100000                     # 100 ms exposure
./qhy_capture -e 500000 -g 50 -o shot.pgm   # 0.5 s, gain 50, custom path
./qhy_capture -h                            # full help
```

Sample output on a successful run:

```
SDK initialised.
Found 1 QHY camera(s).
Using camera id: QHY5III178M-xxxxxxxxxxxxxxxx
Chip: 7.18 x 5.32 mm, pixel 2.40 x 2.40 um, max resolution 3072 x 2048 @ 14 bpp
Exposing for 20000 us...
Captured frame: 3072 x 2048, 16 bpp, 1 channel(s), 12582912 bytes
Saved "frame.pgm" (P5, 16-bit big-endian).
```

## View the captured frame

PGM / PPM is a plain raw image format — most viewers handle it:

| Tool | Command |
|---|---|
| ImageMagick | `display frame.pgm` |
| GIMP | `gimp frame.pgm` |
| `feh` | `feh frame.pgm` |
| Convert to PNG | `convert frame.pgm frame.png` |

For 16-bit data, use a viewer that actually understands 16-bit PGM —
many simple thumbnailers will only show the high byte.

## Tunable defaults

The hard-coded defaults at the top of `qhy_capture.c` cover the common
case. Override on the CLI for one-off changes; edit the source for
project-wide changes:

| Variable | Default | Purpose |
|---|---|---|
| `g_exposure_us` | `20000`  | Exposure time, microseconds (`-e`) |
| `g_gain`        | `30`     | Sensor gain (`-g`) |
| `g_offset`      | `140`    | Sensor black-level offset |
| `g_usb_traffic` | `30`     | USB transfer pacing |
| `g_bin_x/y`     | `1`      | Pixel binning |
| `g_bits`        | `16`     | Per-pixel bit depth (8 or 16) |
| `g_out_path`    | `frame.pgm` | Output path (`-o`) |

## Troubleshooting

**`No QHY camera found. Check USB cable / power.`**
Camera not enumerated. On Linux this is usually the udev rules — re-run
`sudo ./install.sh` from the SDK and re-plug the USB cable. On macOS,
make sure no other QHY-aware app (Sharpcap, EZCAP_QT, etc.) has the
camera open.

**`InitQHYCCDResource failed (-1)`**
The SDK couldn't load its USB backend. Confirm `libusb` is installed
(`brew install libusb` on macOS, `sudo apt install libusb-1.0-0` on
Debian/Ubuntu).

**`SetQHYCCDBitsMode(16) failed.`**
The connected camera doesn't support 16-bit transfer. Edit
`g_bits = 8;` at the top of `qhy_capture.c` and rebuild.

**`GetQHYCCDSingleFrame failed.`**
Most often this is exposure timing: if you set a long exposure
(`-e 5000000` for 5 s) the program will sleep `exposure_us + 100 ms`
before reading. If your camera reports `BadFrames` repeatedly, drop
`g_usb_traffic` to a smaller value (e.g. `10`) and rebuild.

## License

[MIT](LICENSE) — `SPDX-License-Identifier: MIT`.

Drop `qhy_capture.c` into any project (open or closed, commercial
or otherwise). Just keep the copyright + permission notice with
the source.

The QHYCCD SDK itself is **not** covered by this license — see
[qhyccd.com](https://www.qhyccd.com) for the SDK's own terms.
