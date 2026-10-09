import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class TemperatureCheckTests(unittest.TestCase):
    def run_sample(self, text, *options):
        with tempfile.TemporaryDirectory() as directory:
            sample = Path(directory) / "temp"
            if text is not None:
                sample.write_text(text)
            return subprocess.run([sys.executable, str(Path(__file__).with_name("temperature_check.py")),
                                   "--temperature-file", str(sample), *options],
                                  capture_output=True, text=True, check=False)

    def test_normal(self):
        result = self.run_sample("65000\n")
        self.assertEqual(result.returncode, 0)
        self.assertEqual(json.loads(result.stdout)["temperature_c"], 65.0)

    def test_warning_boundary(self):
        for sample, code in [("74999", 0), ("75000", 1), ("78000", 1)]:
            with self.subTest(sample=sample):
                result = self.run_sample(sample)
                self.assertEqual(result.returncode, code)
                self.assertEqual(json.loads(result.stdout)["status"], "warning" if code else "normal")

    def test_custom_warning(self):
        self.assertEqual(self.run_sample("65000", "--warning", "65").returncode, 1)

    def test_read_errors(self):
        for sample in [None, "", "not-a-number", "nan", "inf", "-inf"]:
            with self.subTest(sample=sample):
                result = self.run_sample(sample)
                self.assertEqual(result.returncode, 2)
                self.assertFalse(result.stdout)
                self.assertEqual(json.loads(result.stderr)["status"], "error")

    def test_invalid_threshold(self):
        for threshold in ["nan", "inf", "not-a-number"]:
            with self.subTest(threshold=threshold):
                self.assertEqual(self.run_sample("65000", "--warning", threshold).returncode, 2)


if __name__ == "__main__":
    unittest.main()
