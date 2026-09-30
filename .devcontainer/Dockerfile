FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        g++ clang libclang-rt-18-dev cmake ninja-build gdb git ca-certificates netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*
