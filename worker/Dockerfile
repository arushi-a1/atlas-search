# syntax=docker/dockerfile:1
FROM ubuntu:24.04 AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY backend backend
COPY worker worker
WORKDIR /src/backend
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"$(nproc)" --target crawl_worker

FROM ubuntu:24.04
RUN useradd -m -u 10001 dse
COPY --from=build /src/backend/build/crawl_worker /usr/local/bin/crawl_worker
USER dse
ENV WORKER_INDEX=0 \
    WORKER_COUNT=1 \
    DSE_SEED_FILE=/data/seeds.txt \
    DSE_OUTPUT_FILE=/data/documents.tsv \
    DSE_MAX_PAGES=500
ENTRYPOINT ["/usr/local/bin/crawl_worker"]
