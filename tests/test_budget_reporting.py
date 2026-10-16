"""Exercise timing policy with synthetic timestamps, never sleeps."""
from contextlib import redirect_stderr, redirect_stdout
import io
import json
from pathlib import Path
import runpy
import subprocess
import sys
from unittest.mock import patch

wrapper = sys.argv[1]
for elapsed, status in ((.01, 0), (.46, 0), (2., 0), (.01, 7), (2., 7)):
    output = io.TextIOWrapper(io.BytesIO())
    errors = io.TextIOWrapper(io.BytesIO())
    result = subprocess.CompletedProcess(["test"], status, b"output\n", b"diagnostic\n")
    with patch.object(sys, "argv", [wrapper, ".45", "test"]), \
         patch("time.perf_counter", side_effect=[100., 100.+elapsed]), \
         patch("subprocess.run", return_value=result) as run, \
         redirect_stdout(output), redirect_stderr(errors):
        try:
            runpy.run_path(wrapper, run_name="__main__")
        except SystemExit as exit:
            assert exit.code == status, "timing must not change the functional exit status"
        else:
            raise AssertionError("wrapper did not return the test status")
    run.assert_called_once_with(["test"], capture_output=True)
    output.flush()
    errors.flush()
    assert output.buffer.getvalue() == b"output\n"
    assert b"diagnostic\n" in errors.buffer.getvalue()
    assert (b"SLO miss:" in errors.buffer.getvalue()) == (elapsed > .45)

checker = Path(wrapper).resolve().parents[1] / "tools" / "check_test_slo.py"
for duration, result, check, expected in (
        (.1, "OK", True, 0), (.5, "OK", True, 0), (.501, "OK", True, 1),
        (2., "OK", False, 0), (.1, "FAIL", True, 1), (.1, "FAIL", False, 0)):
    sample = json.dumps({"name": "synthetic", "duration": duration, "result": result})
    arguments = [str(checker), "unused.json"] + (["--check"] if check else [])
    with patch.object(sys, "argv", arguments), patch.object(Path, "read_text", return_value=sample), \
         redirect_stdout(io.StringIO()):
        try:
            runpy.run_path(str(checker), run_name="__main__")
        except SystemExit as exit:
            assert exit.code == expected
        else:
            raise AssertionError("SLO checker did not return its status")
