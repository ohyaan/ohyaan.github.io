#!/usr/bin/env python3
"""Read one Linux thermal-zone sample; report a local warning, not a remote alert."""
import argparse
import json
import math
from pathlib import Path
import sys


def finite_number(text):
    value = float(text)
    if not math.isfinite(value):
        raise ValueError("value must be finite")
    return value


def read_temperature(path):
    return finite_number(Path(path).read_text().strip()) / 1000


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--temperature-file", default="/sys/class/thermal/thermal_zone0/temp",
                        help="Select the SoC thermal zone after checking its type")
    parser.add_argument("--warning", type=finite_number, default=75.0,
                        help="Local warning threshold in Celsius, not a firmware limit")
    args = parser.parse_args(argv)
    try:
        temperature = read_temperature(args.temperature_file)
    except (OSError, ValueError) as error:
        print(json.dumps({"status": "error", "error": str(error)}), file=sys.stderr)
        return 2
    warning = temperature >= args.warning
    print(json.dumps({"temperature_c": temperature, "warning_c": args.warning,
                      "status": "warning" if warning else "normal"}, sort_keys=True))
    return 1 if warning else 0


if __name__ == "__main__":
    raise SystemExit(main())
