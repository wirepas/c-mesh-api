# Configuration
Confugiration parameters can be passed to the container as command line
parameters. See --help for the available parameters.

```bash
docker run --rm local/tiny_gateway --help
```

## Command line example

```bash
docker run --device /dev/ttyACM0:/dev/sink local/tiny_gateway \
-p /dev/sink -b 125000 -g my-gateway-id -H mqtt.broker.example:8883 -U "username" -P "password"
```

## Docker compose example

```yaml
  gateway:
    image: local/tiny_gateway:latest
    container_name: gateway
    command: >
      -p /dev/sink
      -b 125000
      -g my-gateway-id
      -H mqtt.broker.example:8883
      -U "username"
      -P "password"
    devices:
      - /dev/ttyACM0:/dev/sink
```

