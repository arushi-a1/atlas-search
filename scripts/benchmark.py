#!/usr/bin/env python3
"""Benchmarks the /api/search endpoint under concurrent load.

Usage:
    python3 scripts/benchmark.py --url http://localhost:8080 \
        --queries "distributed systems" "web crawler" "bm25 ranking" \
        --concurrency 20 --requests 500

Reports p50/p95/p99 latency and throughput. No third-party dependencies --
uses urllib so it runs anywhere Python 3 does.
"""

import argparse
import concurrent.futures
import statistics
import time
import urllib.error
import urllib.parse
import urllib.request


def send_one(base_url: str, query: str, timeout: float) -> tuple[bool, float]:
    url = f"{base_url}/api/search?q={urllib.parse.quote(query)}&k=10"
    start = time.perf_counter()
    try:
        with urllib.request.urlopen(url, timeout=timeout) as resp:
            resp.read()
            ok = resp.status == 200
    except (urllib.error.URLError, TimeoutError):
        ok = False
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    return ok, elapsed_ms


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    values = sorted(values)
    idx = min(len(values) - 1, int(len(values) * pct))
    return values[idx]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--url", default="http://localhost:8080", help="Base URL of the search API")
    parser.add_argument("--queries", nargs="+", default=["search engine", "distributed systems", "bm25"])
    parser.add_argument("--concurrency", type=int, default=10)
    parser.add_argument("--requests", type=int, default=200, help="Total requests to send")
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    latencies: list[float] = []
    failures = 0

    start = time.perf_counter()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        futures = [
            pool.submit(send_one, args.url, args.queries[i % len(args.queries)], args.timeout)
            for i in range(args.requests)
        ]
        for future in concurrent.futures.as_completed(futures):
            ok, elapsed_ms = future.result()
            latencies.append(elapsed_ms)
            if not ok:
                failures += 1
    total_wall_time = time.perf_counter() - start

    print(f"Requests:        {args.requests}")
    print(f"Concurrency:     {args.concurrency}")
    print(f"Failures:        {failures}")
    print(f"Wall time:       {total_wall_time:.2f}s")
    print(f"Throughput:      {args.requests / total_wall_time:.1f} req/s")
    print(f"Latency p50:     {percentile(latencies, 0.50):.2f} ms")
    print(f"Latency p95:     {percentile(latencies, 0.95):.2f} ms")
    print(f"Latency p99:     {percentile(latencies, 0.99):.2f} ms")
    print(f"Latency mean:    {statistics.mean(latencies):.2f} ms")
    print(f"Latency max:     {max(latencies):.2f} ms")


if __name__ == "__main__":
    main()
