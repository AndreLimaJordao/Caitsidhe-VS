"""Do not let a random crash be mistaken for a successful negative test."""
import subprocess
import sys

r = subprocess.run(sys.argv[1:], capture_output=True, text=True)
output = r.stdout + r.stderr
verdict = r.stderr.rstrip().splitlines()[-1:]
if r.returncode == 1 and verdict == ["RESULT: FAIL (tohost=3)"]:
    print("Expected tohost failure correctly detected")
    sys.exit(0)
print("Negative test unexpectedly behaved differently:\n" + output)
sys.exit(1)
