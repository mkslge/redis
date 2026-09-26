# Build from the repository root: docker build -f Client/Dockerfile .
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y \
    --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Common Common
COPY Client Client

RUN cmake -S Client -B build && \
    cmake --build build --target redisclient

FROM ubuntu:24.04

WORKDIR /app

COPY --from=build /app/build/redisclient /app/redisclient

CMD ["./redisclient", "127.0.0.1", "6380"]
