# Wirepas Mesh API

This repository contains a C implementation of the Wirepas Dual MCU API.
The dual mcu api allows the exchange of data between the host and the
Wirepas Mesh sink.

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

Build the tests with

```shell
    cd test
    make
```

Execute the test suite with

```shell
    ./build/meshAPItest
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
