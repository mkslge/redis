FROM ubuntu:24.04 AS build



RUN apt-get update && apt-get install -y \
    --no-install-recommends \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN cmake -S . -B build && \
    cmake --build build

FROM ubuntu:24.04 

WORKDIR /app

COPY --from=BUILD /app/build/redisclient /app/redisclient


CMD ["./redisclient", "127.0.0.1", "6380"]

