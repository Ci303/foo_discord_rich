import shutil
import subprocess
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
HELPER = ROOT / "scripts" / "python_command.ps1"


def quote_powershell(value: str) -> str:
    return "'" + value.replace("'", "''") + "'"


class PythonCommandTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.shell = shutil.which("pwsh") or shutil.which("powershell")
        if not cls.shell:
            raise unittest.SkipTest("PowerShell is not available")

    def run_powershell(self, command: str):
        return subprocess.run(
            [
                self.shell,
                "-NoLogo",
                "-NoProfile",
                "-NonInteractive",
                "-Command",
                command,
            ],
            cwd=ROOT,
            capture_output=True,
            text=True,
            check=False,
        )

    def test_explicit_python_is_validated_and_resolved(self):
        command = (
            f". {quote_powershell(str(HELPER))}; "
            f"$result = Resolve-PythonCommand -RequestedExecutable {quote_powershell(sys.executable)}; "
            "Write-Output ($result.Version + '|' + $result.ReportedPath)"
        )
        result = self.run_powershell(command)

        self.assertEqual(result.returncode, 0, result.stderr)
        version, reported_path = result.stdout.strip().split("|", 1)
        self.assertEqual(version, ".".join(map(str, sys.version_info[:3])))
        self.assertEqual(Path(reported_path).resolve(), Path(sys.executable).resolve())

    def test_default_discovery_finds_a_supported_python(self):
        command = (
            f". {quote_powershell(str(HELPER))}; "
            "$result = Resolve-PythonCommand; "
            "Write-Output ($result.Version + '|' + $result.ReportedPath)"
        )
        result = self.run_powershell(command)

        self.assertEqual(result.returncode, 0, result.stderr)
        version, reported_path = result.stdout.strip().split("|", 1)
        self.assertGreaterEqual(tuple(map(int, version.split("."))), (3, 10, 0))
        self.assertTrue(Path(reported_path).is_file())

    def test_invalid_explicit_python_is_rejected(self):
        missing = ROOT / "_result" / "definitely-missing-python.exe"
        command = (
            "$ErrorActionPreference = 'Stop'; "
            f". {quote_powershell(str(HELPER))}; "
            f"Resolve-PythonCommand -RequestedExecutable {quote_powershell(str(missing))}"
        )
        result = self.run_powershell(command)

        self.assertNotEqual(result.returncode, 0)
        normalised_error = " ".join(result.stderr.split())
        self.assertIn("not Python 3.10 or newer", normalised_error)


if __name__ == "__main__":
    unittest.main()
