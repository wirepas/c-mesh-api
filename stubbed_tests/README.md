# Introduction

These tests use a stub implementation for the serial interface and can be run
without a real serial device. See README.md under serialstub for more detils.

There are two types of tests included; regular unit tests and fuzz tests. The
tests use [FuzzTest](https://github.com/google/fuzztest) and
[GTest](https://github.com/google/googletest).

# Requirements
[Clang](https://clang.llvm.org/), since FuzzTest requires it.

# Building
FuzzTest tests can be run as unit tests or in fuzzing mode. For more
information refer to FuzzTest documentation
[here](https://github.com/google/fuzztest/blob/main/doc/quickstart-cmake.md).

Example command to build for unit testing:

```shell
mkdir build && cd build
CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=RelWithDebug ..
cmake --build . -j $(nproc)
```

# Running the tests

To run unit tests together with fuzz tests in unit testing mode:
```shell
./stubbed_tests
```

Refer to FuzzTest documentation on how to run fuzz tests in fuzzing mode.

## Naming conventions
Test suites that support fuzzing should start with "Fuzz". This way you can
filter tests when running, for example to run only fuzz tests:

```shell
./stubbed_tests --gtest_filter="Fuzz*"
```

And to run non-fuzz tests:

```shell
./stubbed_tests --gtest_filter="-Fuzz*"
```

