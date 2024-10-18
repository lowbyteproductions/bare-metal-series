# Bare Metal Series

- [Series Playlist](https://www.youtube.com/playlist?list=PLP29wDx6QmW7HaCrRydOnxcy8QmW0SNdQ)

## About the project

This series is all about building bare-metal firmware for a Cortex-M4 STM32 microcontroller, using open source tools and libraries such as GCC and libopencm3. The project will cover everything from blinking an LED, to building a bootloader for performing firmware updates over UART/USB, to building a signed firmware security mechanism that allow only authorised code to run on the device. Stretch goal content includes *breaking* the security we build, as well as exploring more peripherals, applications, and algorithms in depth.

## Prerequisites

You need to have the following installed and properly setup. Ensure that they are available in your path.

- [GNU ARM Embedded tools](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [GNU Make](https://www.gnu.org/software/make/)

### When using vscode

You'll want to install some extensions to make development smoother

- `C/C++`
- `Cortex-Debug`

## Repo setup

```bash
# Clone the repo
git clone git@github.com:lowbyteproductions/bare-metal-series.git
cd bare-metal-series

# Initialise the submodules (libopencm3)
git submodule init
git submodule update

# Build libopencm3
cd libopencm3
make
cd ..

# Build the bootloader firmware
cd bootloader
make
cd ..

# Build the main application firmware
cd app
make
```

## Debuggers

### J-Link

The YouTube series makes use of J-Link debugger hardware and JLinkGDBServer for integrating with VS Code's debug functionality.

You can use the "JLink: Debug Application" and "JLink: Attach to active" VSCode tasks to debug your built binaries.

### ST-Link

ST-Link is alternative debugger that you can use if you don't have a JLink debugger.

If you're using the same STM32F401RE Nucleo board presented in the YouTube series, ST-Link debugging hardware is already provided on the board (see the [Day 0 video's Hardware section](https://youtu.be/uQQsDWLRDuI?t=1000)).

It's recommended that you install the [ST-Link drivers](https://www.st.com/en/development-tools/stsw-link009.html).

You'll also need to install the [open source ST-Link debugging tools](https://github.com/stlink-org/stlink). The primary application you'll need from that tool-set is *stutil*. Verify that *stutil* is available in your path before attempting to use the VSCode ST-Link debugging tasks.

Once your drivers and debugging tools are installed, you can use the "ST-Link: Debug Application" and "ST-Link: Attach active" VSCode tasks to debug your firmware over ST-Link.

## Troubleshooting

### Hard Fault

During your first blinky debug-run, you may happen to find yourself in an infinite loop in the `blocking_handler()`, which is a weak implementation of the `hard_fault_handler()`. 

Hard faults usually occur if there is incorrect memory address handling, among other things. It is very likely that your linkerscript.ld has incorrect memory details. The STM32F401RET6 used in the series has `512 KiB` of flash memory and `96 KiB` of SRAM. Your board may not have the same specs. These incorrect details are then passed on to the `reset_handler()`. It is the first function that is called by the mcu at boot, and it ends up assigning unavailable memory addresses to it's local pointer variables, leading to a hard fault.

**Solution**: Simply edit the `ORIGIN` and `LENGTH` details of the `rom` (Flash) and `ram` (SRAM) in the linkerscript, as per your datasheet. Most STM32 Nucleo Boards have the same `ORIGIN`, or starting memory addresses for these memories, but you can get this information from the memory map, also available in the datasheet.
