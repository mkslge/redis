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

COPY --from=build /app/build/redisserver /app/redisserver
RUN mkdir -p /app/Persistance

EXPOSE 6380

CMD ["./redisserver"]
