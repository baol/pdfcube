"""Report the timing SLO without turning runner contention into a test failure."""
import subprocess
import sys
import time

target = float(sys.argv[1])
started = time.perf_counter()
result = subprocess.run(sys.argv[2:], capture_output=True)
elapsed = time.perf_counter()-started
# Piped communicate() waits for readiness, not wait(timeout)'s exponential
# polling, which can add 50 ms after exit. Meson owns the hang watchdog.
sys.stdout.buffer.write(result.stdout)
sys.stderr.buffer.write(result.stderr)
if elapsed > target:
    print(f"SLO miss: test took {elapsed:.3f}s (target {target:.3f}s)", file=sys.stderr)
raise SystemExit(result.returncode)
