FROM ubuntu:24.04 AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends g++ cmake ninja-build ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04

RUN useradd --system --no-create-home server
COPY --from=build /src/build/server_app /usr/local/bin/server_app
COPY --from=build /src/build/bench_app /usr/local/bin/bench_app

USER server
EXPOSE 8080

ENTRYPOINT ["server_app"]
CMD ["--host", "0.0.0.0", "--port", "8080"]
