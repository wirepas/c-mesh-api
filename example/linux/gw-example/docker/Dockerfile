FROM alpine:3 AS builder

RUN apk add --no-cache gcc make paho-mqtt-c-dev musl-dev linux-headers

RUN adduser --disabled-password wirepas

USER wirepas

WORKDIR /home/wirepas

COPY --chown=wirepas ./ /home/wirepas/c-mesh-api
WORKDIR /home/wirepas/c-mesh-api/example/linux/gw-example
RUN make clean all

# Build the final image
FROM alpine:3

ARG GATEWAY_BUILD_SHA1=unset

RUN adduser --disabled-password wirepas
RUN addgroup wirepas dialout

RUN apk add --no-cache paho-mqtt-c

USER wirepas

# Copy the built binary
COPY --from=builder /home/wirepas/c-mesh-api/example/linux/gw-example/build/gw-example /usr/local/bin/gw-example

ENTRYPOINT ["gw-example"]

LABEL com.wirepas.tiny_gateway.build.sha1="${GATEWAY_BUILD_SHA1}"
