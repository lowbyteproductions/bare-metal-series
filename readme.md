# Bare Metal Series

- [Series Playlist](https://www.youtube.com/playlist?list=PLP29wDx6QmW7HaCrRydOnxcy8QmW0SNdQ)

## About the project

This series is all about building bare-metal firmware for a Cortex-M4 STM32 microcontroller, using open source tools and libraries such as GCC and libopencm3. The project will cover everything from blinking an LED, to building a bootloader for performing firmware updates over UART/USB, to building a signed firmware security mechanism that allow only authorised code to run on the device. Stretch goal content includes *breaking* the security we build, as well as exploring more peripherals, applications, and algorithms in depth.

## Prerequisites

You need to have the following installed and properly setup. Ensure that they are available in your path.

- [GNU ARM Embedded tools](https://developer.arm.com/downloads/-/gnu-rm)
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

# Build the main application firmware
cd app
make
```

## Note on ST-Link

The repository does not yet include instructions or tools for working with the ST-Link. If you get the ST-Link up and running, please submit a PR with a *separate* .vscode directory (`.vscode-stlink`) for tasks and launch configurations, along with usage instructions for the readme. Your help will be very much appreciated!
