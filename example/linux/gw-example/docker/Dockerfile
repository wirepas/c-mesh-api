FROM alpine:3 AS builder

RUN apk add --no-cache gcc cmake make paho-mqtt-c-dev musl-dev linux-headers

RUN adduser --disabled-password wirepas

USER wirepas

WORKDIR /home/wirepas

COPY --chown=wirepas ./ /home/wirepas/c-mesh-api
RUN cmake -S /home/wirepas/c-mesh-api/example/linux/gw-example -B build
RUN cmake --build build

# Build the final image
FROM alpine:3

ARG GATEWAY_BUILD_SHA1=unset

RUN adduser --disabled-password wirepas
RUN addgroup wirepas dialout

RUN apk add --no-cache paho-mqtt-c

USER wirepas

# Copy the built binary
COPY --from=builder /home/wirepas/build/gw-example /usr/local/bin/gw-example

ENTRYPOINT ["gw-example"]

LABEL com.wirepas.tiny_gateway.build.sha1="${GATEWAY_BUILD_SHA1}"
