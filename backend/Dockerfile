# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY backend backend
COPY worker worker
WORKDIR /src/backend
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)" --target search_server

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd -m -u 10001 dse
COPY --from=build /src/backend/build/search_server /usr/local/bin/search_server
USER dse
ENV DSE_BIND_ADDRESS=0.0.0.0 \
    DSE_PORT=8080 \
    DSE_THREADS=16 \
    DSE_DOCUMENT_DUMP=/data/documents.tsv \
    DSE_REDIS_HOST=redis \
    DSE_REDIS_PORT=6379
EXPOSE 8080
HEALTHCHECK --interval=10s --timeout=3s CMD curl -f http://localhost:8080/health || exit 1
ENTRYPOINT ["/usr/local/bin/search_server"]
