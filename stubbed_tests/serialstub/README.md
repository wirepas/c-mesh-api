# Introduction

This is a stub serial implementation for the c-mesh-api platform interface
(lib/platform/serial.h). Intended use is to build on top of the linux platform
implementation. Since only serial.h is implemented, the resulting object from
this stub should be linked before the default platform objects so that the
default serial implementation is not taken into use, while rest of the platform
implementation is.

The stub has a wrapper for Dual MCU frames. A handler is assigned for each
frame which is normally sent to the serial device. The handler should return
responses for every call (these are called "confirm" or "indication" in the
DualMCU API.)

For more information, see the Wirepas DualMCU API documentation (for example
[here](https://github.com/wirepas/wm-sdk-5g/blob/rel_1.2.0_5G/libraries/dualmcu/api/DualMcuAPI.md))

