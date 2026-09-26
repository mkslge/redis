# Build from the repository root: docker build -f Server/Dockerfile .
FROM ubuntu:24.04 AS build

RUN apt-get update && apt-get install -y \
    --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY Common Common
COPY Server Server

RUN cmake -S Server -B build && \
    cmake --build build --target redisserver

FROM ubuntu:24.04

WORKDIR /app

COPY --from=build /app/build/redisserver /app/redisserver
RUN mkdir -p /app/data

EXPOSE 6380

CMD ["./redisserver"]
