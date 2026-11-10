# Distributed Search Engine

A small, from-scratch distributed search engine: a politeness-aware web
crawler, an inverted index with BM25 ranking and phrase search, a REST
API served by a custom C++ HTTP server, Redis-backed query caching, and a
React frontend -- all containerized with Prometheus/Grafana monitoring.

Built as a portfolio project extending an original C++ HTTP server
implementation with a full search-engine stack around it.

## Features

- **Crawler**: BFS frontier, `robots.txt` (Allow/Disallow precedence,
  wildcards, `Crawl-delay`), per-host politeness throttling, shardable
  across worker processes
- **HTML parser**: title, meta description, headings, visible text, and
  resolved outbound links -- regex-based, dependency-free
- **Inverted index**: positional postings (title + body fields), thread-safe
- **Ranking**: BM25 with a title-boost weight, plus exact phrase search
  (`"quoted phrases"`) via positional intersection
- **REST API**: `/api/search`, `/api/stats`, `/health`, `/metrics`
  (Prometheus format)
- **Caching**: Redis-backed query result cache (5 min TTL), hand-rolled
  RESP client
- **Frontend**: React + Vite SPA
- **Monitoring**: Prometheus scrape config + a provisioned Grafana
  dashboard (requests/sec, cache hit rate, index size)
- **Deployment**: Dockerfiles for each service + a `docker-compose.yml`
  wiring the full stack
- **Tests**: 20 unit tests (tokenizer, robots.txt, index/BM25/phrase
  search, HTML parsing) with a small dependency-free test framework
- **CI**: GitHub Actions -- backend build+test, frontend build, Docker
  image build check

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for how the pieces fit
together, including a request-flow diagram and an honest table of what's
production-hardened vs. a documented extension point.

## Quickstart

### Docker Compose (full stack)

```bash
docker compose up --build
```

- Frontend: http://localhost:3000
- Search API: http://localhost:8080
- Prometheus: http://localhost:9090
- Grafana: http://localhost:3001 (anonymous viewer access)

The `crawler` service crawls `data/seeds.txt` on startup and writes to
`data/documents.tsv`, which `search-api` loads at boot. Edit
`data/seeds.txt` and re-run `docker compose up --build crawler` to
re-crawl, then restart `search-api` to pick up the new index.

### Local build (no Docker)

```bash
# Backend
cmake -S backend -B backend/build -DCMAKE_BUILD_TYPE=Release
cmake --build backend/build --parallel
ctest --test-dir backend/build --output-on-failure

DSE_PORT=8080 DSE_DOCUMENT_DUMP=data/documents.tsv ./backend/build/search_server

# Frontend (separate terminal)
cd frontend && npm install && npm run dev
```

### Crawling manually

```bash
DSE_SEED_FILE=data/seeds.txt DSE_OUTPUT_FILE=data/documents.tsv \
  DSE_MAX_PAGES=200 ./backend/build/crawl_worker
```

## API

| Endpoint | Description |
|---|---|
| `GET /api/search?q=<query>&k=<top_k>` | Search the index. Quote phrases: `q="exact phrase"`. `k` defaults to 10, capped at 100. |
| `GET /api/stats` | Document count, average body length, cache status |
| `GET /health` | Liveness check |
| `GET /metrics` | Prometheus text-format metrics |

Example:

```bash
curl "http://localhost:8080/api/search?q=%22distributed+search%22&k=5"
```

```json
{
  "query": "\"distributed search\"",
  "count": 1,
  "results": [
    {
      "url": "https://example.com/dse",
      "title": "Distributed Search Engines",
      "snippet": "a distributed search engine crawls the web...",
      "score": 6.522
    }
  ]
}
```

## Benchmarks

Measured with `scripts/benchmark.py` against a locally built
`search_server` (16 server threads), 2-document toy index, 300 requests
at concurrency 20, on the machine this repo was built on -- not
representative of production hardware or a realistic corpus size, but
reproducible:

```
Requests:        300
Concurrency:     20
Failures:        0
Throughput:      282.1 req/s
Latency p50:     7.88 ms
Latency p95:     767.19 ms
Latency p99:     897.86 ms
```

The p95/p99 tail at this concurrency (20 connections against 16 server
threads) shows queueing delay once the thread pool saturates -- worth
profiling further before trusting this server under real load; see
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for what's hardened vs.
not. Re-run against your own corpus with:

```bash
python3 scripts/benchmark.py --url http://localhost:8080 \
  --queries "your query here" "another query" \
  --concurrency 20 --requests 500
```

## Project layout

```
backend/          C++ core: HTTP server, parser, crawler, index, BM25, API
  include/         Public headers, mirrors src/ layout
  src/
  tests/           Unit tests + dependency-free test framework
worker/            Crawl worker entrypoint (links against backend/'s library)
frontend/          React + Vite SPA
monitoring/        Prometheus config + Grafana provisioning
scripts/           benchmark.py
docs/              ARCHITECTURE.md
data/              seeds.txt, documents.tsv (generated)
```

## License

MIT -- see [`LICENSE`](LICENSE). This repository contains no code derived
from any university coursework; the HTTP server, crawler, parser, index,
and API layers are original implementations written for this project.
