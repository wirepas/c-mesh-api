# Wirepas Mesh API

This repository contains a C implementation of the Wirepas Dual MCU API.
The Dual MCU API allows the exchange of data between a host and a Wirepas node via UART.
The C-Mesh library is used by the Wirepas Gateway software but can also be used in case of a Dual MCU architecture.

## C Library

The C implementation is available from [lib](./lib) and is structured in the
following folders:

-   **[api](./lib/api)**: header files for the WM dual mcu api
-   **[platform](./lib/platform)**: host platform functions for interface handling
-   **[wpc](./lib/wpc)**: implementation of the WM dual mcu api

An example on how to use and extend the library is available from:

-   [C-mesh-api example](./example/main.c)

## Tests

To run the tests you will to connect a device running WM's dual mcu api on
your host machine.

Tests require Google Test. On Debian and Ununtu, you can install the package
libgtest-dev.

Build the tests with

```shell
    cd test
    make
```

When executing the tests, environment variables WPC_SERIAL_PORT and
WPC_WPC_BAUD_RATE should be set. For example:

```shell
    WPC_SERIAL_PORT=/dev/ttyACM0 WPC_BAUD_RATE=125000 ./build/meshAPItest
```

## Contributing

We welcome your contributions!

Please read the [instructions on how to do it][here_contribution]
and please review our [code of conduct][here_code_of_conduct].

## License

Wirepas Oy licensed under Apache License, Version 2.0 See file LICENSE for
full license details.

[here_contribution]: https://github.com/wirepas/c-mesh-api/blob/master/CONTRIBUTING.md
[here_code_of_conduct]: https://github.com/wirepas/c-mesh-api/blob/master/CODE_OF_CONDUCT.md
