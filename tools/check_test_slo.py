#!/usr/bin/env python3
"""Report every test over the SLO; --check is the explicit pre-push gate."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("log", type=Path, help="Meson's meson-logs/testlog.json")
parser.add_argument("--limit", type=float, default=.5)
parser.add_argument("--check", action="store_true", help="fail this separate SLO check on misses")
args = parser.parse_args()
tests = [json.loads(line) for line in args.log.read_text().splitlines() if line.strip()]
if not tests:
    parser.error("no test results found")
misses = sorted((test for test in tests if test["duration"] > args.limit),
                key=lambda test: test["duration"], reverse=True)
failed = [test for test in tests if test["result"] != "OK"]
for test in misses:
    print(f"SLO MISS {test['duration']*1000:.1f} ms: {test['name']}")
print(f"SLO: {len(tests)-len(misses)}/{len(tests)} within {args.limit*1000:.0f} ms; "
      f"slowest {max(test['duration'] for test in tests)*1000:.1f} ms; "
      f"functional non-passes {len(failed)}")
raise SystemExit(1 if args.check and (misses or failed) else 0)
