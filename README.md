Robot Control Library
===============================
This package contains the C library and example/testing programs for the Robot Control project. This project began as a hardware interface for the Robotics Cape and later the BeagleBone Blue and was originally called Robotics_Cape_Installer. It grew to include an extensive math library for discrete time feedback control, as well as a plethora of POSIX-compliant functions for timing, threads, program flow, and lots more, all aimed at developing robot control software on embedded computers.
Full API documentation, instruction manual, and examples at <http://docs.beagle.cc/project/librobotcontrol>.
We encourage questions and discussion in the [Discord #librobotcontrol channel](https://discord.gg/PZUsdBqe).
For use with [BeagleBoard.org Robotics Cape](https://www.beagleboard.org/boards/beaglebone-robotics-cape).

---

# This fork: port to Debian 11 / Kernel 5.10 (BeagleBone Blue)

This fork adapts librobotcontrol — originally written for kernel 4.14 — to the latest official BeagleBoard.org image for the BeagleBone Blue:

- **Board:** BeagleBone Blue (AM335x Cortex-A8, 512MB RAM)
- **OS:** Debian 11 Bullseye
- **Kernel:** 5.10

The jump from kernel 4.14 to 5.10 changed device names, sysfs paths, the PRU handling subsystem, and the binding of several peripherals. This document covers what changed, why, and how to install and diagnose this fork.

---

## Installation

### 1. Prerequisites

- Official BeagleBoard.org image for BeagleBone Blue with kernel `5.10.168-ti-r71` (optionally upgradable to r83 with apt)
- TI kernel sources in `/opt/source/dtb-5.10-ti/` (needed to compile the device tree)
- PRU Software Support Package (PSSP) in `/usr/lib/ti/pru-software-support-package-v6.0/` (comes preinstalled on the factory image)
- Tools: `device-tree-compiler` (`dtc`), `cpp`, TI's PRU toolchain (`clpru`, `lnkpru`, already included on the image)

### 2. Device tree

```bash
# Copy the DTS sources into the kernel's build tree
cp device_tree/dtb-5.10-ti/am335x-boneblue.dts /opt/source/dtb-5.10-ti/src/arm/
cp device_tree/dtb-5.10-ti/am335x-bone-pins.h /opt/source/dtb-5.10-ti/src/arm/

cd /opt/source/dtb-5.10-ti/src/arm/
cpp -nostdinc \
    -I /opt/source/dtb-5.10-ti/src/arm/ \
    -I /opt/source/dtb-5.10-ti/include/ \
    -D__DTS__ -undef -P -x assembler-with-cpp \
    am335x-boneblue.dts | \
dtc -I dts -O dtb -@ -o am335x-boneblue.dtb -
```

The root `Makefile` automatically installs the compiled `.dtb` to `/boot/dtbs/5.10.168-ti-r**/` during `make install` — but the compiled binary needs to be copied back into the repository first, so the Makefile can find it:

```bash
cp am335x-boneblue.dtb /usr/local/src/librobotcontrol/device_tree/dtb-5.10-ti/
```

The directory `/usr/local/src/` is meant to store the source code of the software installed in Unix-like systems. In case the library is located in other directory it must be replaced in the command with the correct one.

### 3. Kernel driver blacklist (IMU / barometer)

The kernel tries to bind its own drivers to the I2C addresses of the MPU-9250 and BMP280 declared in the device tree, which blocks the direct `ioctl` access the library uses.

The BeagleBone Blue's IMU is a real **MPU-9250** (confirmed by the `compatible = "invensense,mpu9250"` node in the device tree). Note that Linux's IIO driver for this chip family is still named `inv_mpu6050_*` — it's a shared driver covering many chips in the same family (MPU6050, MPU9250, ICM20602, etc.), verifiable with `modinfo inv_mpu6050_i2c | grep alias`, which lists `mpu9250` among its supported chips despite the "6050" in the module name. Don't assume a "6050"-named module is irrelevant just because the chip is a 9250 — check `modinfo` first.

```bash
sudo tee /etc/modprobe.d/blacklist-mpu.conf << 'EOF'
blacklist inv_mpu6050_i2c
blacklist bmp280_i2c
EOF
```

`inv_mpu6050_i2c` is confirmed necessary (its alias list includes `mpu9250`/`i2c:mpu9250`). `bmp280_i2c` is presumed necessary by the same reasoning but wasn't independently checked with `modinfo` — run `modinfo bmp280_i2c | grep alias` if you want the same level of certainty. The SPI variants (`inv_mpu6050_spi`, `bmp280_spi`) are not needed since both chips are wired over I2C on this board, not SPI — harmless to add back if you're unsure, just not required.

### 4. udev rule for non-root SPI access

```bash
sudo tee /etc/udev/rules.d/84-spi-noroot.rules << 'EOF'
SUBSYSTEM=="spidev", GROUP="gpio", MODE="0660"
EOF
```

Without this, `/dev/spidev*` is only accessible as root. With it, any user in the `gpio` group can use SPI1 directly.

### 5. Build and install

```bash
cd /usr/local/src/librobotcontrol
make
sudo make install
sudo reboot
```

`make` (no target) compiles the PRU firmware, the library, and the examples. `sudo make install` copies everything into place — the device tree, the library, the compiled firmware, and the three systemd services (`pru_common`, `pru_servo`, `pru_encoder`) — and does not reliably rebuild stale artifacts on its own, so always run a plain `make` first.

### 6. Post-install verification

```bash
rc_test_drivers 		#Checks communication with peripherals
rc_test_motors -d 0.4 	#Activates DC motors PWM control
rc_test_encoders 		#Shows real-time-count of the encoders
rc_test_servos 			#Turns servo motors 6V power line and PWM signal
rc_spi_loopback 		#Tests SPI lines sending and reading a message (requires loopback jumper between MOSI and MISO pins)
rc_uart_loopback 		#Tests UART lines sending and reading a message (requires loopback jumper between Tx and Rx pins)
rc_test_mpu 			#Displays IMU accelerometer and gyroscope real-time-measurements
rc_altitude				#Displays some measurements and estimations from the IMU and barometer
```

---

## Changes from the original fork (kernel 4.14)

### UART

Kernel 5.10 renamed devices from `/dev/tty0` to `/dev/ttyS*`. Updated in the library code.

### PWM (`ehrpwm` / `ecap`) — device tree

TI's official DTS for the Blue (used as the base for this port) does not enable `ehrpwm0/1/2` or `ecap0/1/2` — only `eqep0/1/2`. When rebuilding the DTS on top of that base, these had to be added explicitly:

```dts
&ehrpwm0 { status = "okay"; };
&ehrpwm1 {
    pinctrl-names = "default";
    pinctrl-0 = <&ehrpwm1_pins>;
    status = "okay";
};
&ehrpwm2 {
    pinctrl-names = "default";
    pinctrl-0 = <&ehrpwm2_pins>;
    status = "okay";
};
&ecap0 { status = "okay"; };
&ecap1 { status = "okay"; };
&ecap2 { status = "okay"; };
```

Without this, `rc_test_drivers` reports `ti-pwm driver not loaded` and `rc_motor_init` fails — the motor gets no speed signal even though the direction pins (`MDIR_*`, plain GPIO) work fine.

### PWM subsystem — chip/channel discovery (`library/src/io/pwm.c`)

The original `pwm.c` discovered which `/sys/class/pwm/pwmchipN` (or, on older kernels, `/sys/class/pwm/pwm-N:M`) corresponds to each of the AM335x's three eHRPWM subsystems dynamically, via a `glob()` search — necessary because the exact chip number the kernel assigns has varied across driver/kernel revisions (the file's own comments document three different numbering schemes seen across kernel 4.9, 4.14.54, and 4.14.61).

On kernel 5.10, this was replaced with a fixed mapping, empirically confirmed on this specific kernel build:

```c
static int get_chip(int ss) {
    if(ss==0) return 3; // PWM subsystem 0
    if(ss==1) return 5; // PWM subsystem 1
    if(ss==2) return 7; // PWM subsystem 2
    return -1;
}
```

This is simpler and drops the `glob()` dependency, but is less portable than the original: if a different kernel build enumerates `pwmchip*` devices in a different order, these three numbers may need re-verifying (e.g. `ls -la /sys/class/pwm/pwmchip*/device` and matching each to its physical PWMSS address). The old mode-detection logic (`mode` / `ssindex[]`, for telling apart the `pwmchip%d/pwm0` vs `pwm-%d:0` sysfs naming styles) was removed along with the dynamic discovery; the two variables are still declared in the file but are no longer read or written anywhere — dead code, safe to remove in a future cleanup. The file's header comment describing the multi-revision discovery logic is now stale, since the code it describes no longer runs.

### eQEP encoders (channels 1-3)

Three independent problems, all in the same subsystem:

**a) Kernel module not loaded by default.** `ti-eqep` does not auto-load at boot on this image. Check with `lsmod | grep eqep`; if absent, `sudo modprobe ti-eqep`.

**b) Bug in TI's official DTS.** The `eqep0_pins` group in TI's official DTS uses `AM335X_PIN_MCASP0_AXR0` (offset `0x198`) for encoder 1's A signal — but that is the **same physical pin** as SPI1 MOSI. The comment on that same line in the official file says `mcasp0_aclkr`, and `&gpio3`'s `gpio-line-names` label that pin `EQEP_0A` — both point to the correct pin: `AM335X_PIN_MCASP0_ACLKR` (offset `0x1a0`). Fixed in this fork.

This bug was invisible on a system with no SPI1 configured (nothing else was competing for the pin), and only surfaced as a `probe` failure (`pin already requested by 481a0000.spi`) once `spi1_pins` was added.

**c) `ceiling` defaults to 0.** Kernel 5.10's `counter` subsystem (which replaces the direct eQEP driver from 4.14) leaves each channel's `ceiling` attribute at `0` by default. With `ceiling=0`, the counter never advances past a single step — it always reads back `0`. `rc_encoder_eqep_init()` now sets `ceiling` to the maximum value (`4294967295`) as part of its init sequence, on all 3 channels, every time it's called:

```
disable → ceiling = UINT32_MAX → count = 0 → enable
```

### PRU — servos and encoder channel 4

`CONFIG_STRICT_DEVMEM=y` blocks `/dev/mem`, so shared-memory PRU access (used in kernel 4.14) is no longer viable. Both firmwares were rewritten to communicate with the ARM side over **RPMsg** instead of shared memory.

**PRU1 — Servos (8 channels).** `pru_firmware/src/main_pru1.c`. Receives per-channel duty cycles over RPMsg and generates the PWM signals in software using `PRU1_CTRL.CYCLE` for timing.

**PRU0 — Encoder channel 4.** `pru_firmware/src/main_pru0.c`. A port of the original `pru0-encoder.asm` (quadrature decoding in PRU assembly) to C — channel 4 has no hardware counter available, so counting is done in software inside the firmware.

- **Real pins:** channel A = R31 bit 14 = `P8_16`/`GPMC_AD14`; channel B = bit 15 = `P8_15`/`GPMC_AD15`. The `PRU0_r31_16` comment in the kernel 4.14 DTS (inherited from an earlier version of this same pin) was incorrect — confirmed against the original `pru0-encoder.asm` itself (`.asg 14,A` / `.asg 15,B`) and against `am335x-bone-common-univ.dtsi`.
- **RPMsg interrupts:** PRU0 uses sysevt 16 (to ARM) / 17 (from ARM), confirmed both in the compiled DTB (`&pru0` → `interrupts = <0x10 0x02 0x02>`) and in the comment of TI's own official AM335x example (`PRU_RPMsg_Echo_Interrupt0/main.c`).
- **Firmware:** `am335x-pru0-rc-encoder-fw`. The name must match exactly between `&pru0 { firmware-name = ...}` in the DTS and `PRU0_FW` in `pru_firmware/Makefile` — a mismatch means `remoteproc` never finds the file.

**RPMsg protocol (both, via `/dev/rpmsg_pru30` for PRU0 / `/dev/rpmsg_pru31` for PRU1):**

| Message | Length | Meaning |
|---|---|---|
| `'r'` | 1 byte | Read request → firmware responds with a 4-byte `int32_t` of the current count |
| `'w'` + `int32_t` | 5 bytes | Write/reset the count to the given value (little-endian). No response. |

`rc_encoder_pru_init()` now automatically sends a reset to `0` on every call, matching what `rc_encoder_eqep_init()` already does for channels 1-3 — without this, the PRU's count keeps accumulating continuously from system boot, no matter how many times the reading program restarts.

**PSSP v6.0 — signature change.** The version of the PRU Software Support Package shipped on the factory image (`v6.0`, in `/usr/lib/ti/pru-software-support-package-v6.0/`) changed `pru_rpmsg_channel()`'s signature, adding a `char* desc` parameter between `name` and `port` compared to older versions. Both firmwares (`main_pru0.c`, `main_pru1.c`) were updated to include `CHAN_DESC`.

### systemd services

```
services/
├── pru_common/     # infrastructure shared by both PRUs
│   ├── Makefile
│   └── 00_pru_rpmsg_modules.conf   (virtio_rpmsg_bus, rpmsg_pru, pru_rproc)
├── pru_servo/      # PRU1-specific
│   ├── Makefile
│   └── pru_servo.service           (remoteproc2 / pruss-core1)
└── pru_encoder/    # PRU0-specific
    ├── Makefile
    └── pru_encoder.service         (remoteproc1 / pruss-core0)
```

Each service stops and restarts its corresponding PRU after kernel modules finish loading (`After=systemd-modules-load.service`), avoiding a race condition where the PRU tries to start before `rpmsg_pru` is ready.

Installing the compiled `.dtb` lives in the root `Makefile` (not in any `services/*/Makefile`), since it affects the whole system, not just the PRUs.

### I2C / IMU-Barometer

See [installation section](#3-kernel-driver-blacklist-imu--barometer). The kernel binds its own drivers to the I2C addresses declared in the device tree (`0x68` MPU-9250, `0x76` BMP280), blocking the direct `ioctl` access the library uses (`rc_i2c_init` fails with `ioctl slave address change failed`).

### SPI access without root

See [installation section](#4-udev-rule-for-non-root-spi-access). Without the udev rule, `/dev/spidev*` is root-only.

---

## Known limitations / pending items

- **DSM and GPS:** not tested due to lack of available hardware. The code was not touched during this port; it should work, but this is not confirmed on this hardware.
- **`GPIO3_17` shares physical pin with `SPI1_CS`** — it always boots into `spi_cs`. To change the state of the pin to gpio edit `/sys/devices/platform/ocp/ocp:gp0_pin6_pinmux/state` replacing `default` with `gpio`. If the project ends up depending on this pin as GPIO more than on SPI1_CS0, consider flipping the default state of the pin in dts file or automating it with a service, once it's confirmed which chip-select each connected SPI peripheral actually uses.
- **`pinmux.c` partially functional.** Only the `GP0_PIN_6`/`GPIO3_17` pin has its `bone-pinmux-helper` mechanism rebuilt in the kernel 5.10 DTS, exposed directly via sysfs (see the GPIO3_17/SPI1_CS0 section). The C-level `rc_pinmux_set()` / `rc_pinmux_set_default()` functions in `library/src/pinmux.c` themselves have **no functional changes** in this port — they still `return 0` immediately and do nothing, exactly as they did before this port started. The other pins covered by `rc_pinmux_set()`'s `switch` statement (`P9_22`, `P9_21`, `P9_26`, `P9_24`, `P9_30`, `P9_29`, `P9_31`, `H18`, `C18`, `U16`, `J15`, `H17`) still lack their corresponding nodes in the 5.10 DTS. Reactivating them would require declaring each node in the DTS following the same pattern as `gp0_pin6_pinmux`, then removing the `return 0` lines in `pinmux.c`.
- **`pwm.c`'s chip-number mapping is hardcoded** (`get_chip()`: subsystems 0/1/2 → pwmchip 3/5/7), replacing the original's dynamic `glob()`-based discovery. Confirmed working on this exact kernel build; may need re-verification if the kernel image changes. The now-unused `mode`/`ssindex[]` variables and the stale multi-revision-discovery comment in the file are leftover cleanup items.
- **`AM335X_PIN_GPMC_CLK`** in `device_tree/dtb-5.10-ti/am335x-bone-pins.h` is defined but unused — a leftover from an earlier version of `pru_encoder_pins` that pointed at the wrong pin (before confirming encoder 4 uses `GPMC_AD14`/`AD15`, not `GPMC_CLK`). Safe to remove.
- **`bmp280`-related blacklist entries** were not independently verified with `modinfo` the way the MPU ones were — see the installation section.

---

## Files modified / added in this port

```
device_tree/dtb-5.10-ti/
├── am335x-boneblue.dts 		# full DTS, based on TI's official one
└── am335x-bone-pins.h 			# pin offsets missing from the official header

library/src/
├── io/uart.c 					# renamed devices from /dev/tty0 to /dev/ttyS
├── io/encoder_eqep.c 			# automatic ceiling, disable→ceiling→zero→enable
├── io/pwm.c 					# hardcoded pwmchip mapping (get_chip()) replacing the original's dynamic glob()-based discovery
├── pru/pru.					# return 0 immediately and do nothing 
├── pru/encoder_pru.c 			# real RPMsg protocol (read + write), auto-reset
├── pru/servo.c 				# real RPMsg protocol (read + write)
└── pinmux.c 					# return 0 immediately and do nothing

pru_firmware/
├── src/main_pru0.c 			# rewritten in C + RPMsg protocol
├── src/intc_map_0.h 			# new, PRU0 interrupt map
├── src/main_pru1.c 			# rewritten in C + RPMsg protocol
└── Makefile 					# changed compiled firmware from asm to c and updated PSSP path

services/
├── pru_common/
│   ├── Makefile 						# new
│   └── 00_pru_rpmsg_modules.conf 		# new
├── pru_servo/
│   ├── Makefile 						# new
│   └── pru_servo.service 				# new
└── pru_encoder/
    ├── Makefile 						# new
    └── pru_encoder.service 			# new

Makefile                    # invokes pru_common/pru_encoder; installs the DTB

# Outside the repository:
/etc/modprobe.d/blacklist-mpu.conf       # new
/etc/udev/rules.d/84-spi-noroot.rules    # new
```
