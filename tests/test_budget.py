"""Enforce the fast-test budget without adding waits to the test itself."""
import subprocess
import sys
import time

budget = float(sys.argv[1])
started = time.perf_counter()
result = subprocess.run(sys.argv[2:], timeout=budget)
elapsed = time.perf_counter()-started
assert elapsed <= budget, f"test took {elapsed:.3f}s (budget {budget:.3f}s)"
raise SystemExit(result.returncode)
