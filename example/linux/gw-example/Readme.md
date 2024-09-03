# Gateway example

## Overview

This application example implements the Wirepas Gateway API (MQTT + protobuf messages) in a single monolithic application.

This example is based on Linux environment but can be used as a starting point for a different platform.
Linux is chosen here as it is the default target for c-mesh-api library and it is easy to execute from a PC.

This example mainly demonstrates the usage of the proto API of c-mesh-api and the "glue" required to convert it into a Wirepas gateway with an MQTT client.

## Installation

In order to build this application in Ubuntu (only tested environment) you need the libpaho-mqtt library:

```bash
sudo apt install libpaho-mqtt-dev
```

Application can be built directly with following command:

```bash
make
```

## Usage

Some parameters are mandatory to start the application:

```bash
Usage: gw-example -p <port name> [-b <baudrate>] -g <gateway_id> -H <mqtt hostname> [-U <mqtt user>] [-P <mqtt password>]
```

## Limitation

This example implements only the "happy case". There is no buffering implemented in case the connection to the broker is down.
Mqtt client is used in synchronous mode so may have an impact on the global performances.

