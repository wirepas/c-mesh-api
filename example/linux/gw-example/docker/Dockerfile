FROM alpine:3.20 AS builder

RUN apk add --no-cache bash build-base gcc make paho-mqtt-c-dev

RUN adduser --disabled-password wirepas

USER wirepas

WORKDIR /home/wirepas

COPY --chown=wirepas ./ /home/wirepas/c-mesh-api
WORKDIR /home/wirepas/c-mesh-api/example/linux/gw-example
RUN make

# Build the final image
FROM alpine:3.20

RUN adduser --disabled-password wirepas
RUN addgroup wirepas dialout

RUN apk add --no-cache paho-mqtt-c

USER wirepas

# Copy the built binary
COPY --from=builder /home/wirepas/c-mesh-api/example/linux/gw-example/build/gw-example /home/wirepas/gw-example

CMD ["/home/wirepas/gw-example"]

LABEL com.wirepas.tiny_gateway.build.sha1="${GATEWAY_BUILD_SHA1}"
