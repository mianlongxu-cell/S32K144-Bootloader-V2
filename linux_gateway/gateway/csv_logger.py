"""CSV logging for decoded vehicle snapshots."""

from __future__ import annotations

import csv
from datetime import datetime
from pathlib import Path
from typing import IO, Optional

from .vehicle import VehicleState


CSV_FIELDS = (
    "timestamp",
    "speed_kmh",
    "rpm",
    "coolant_c",
    "gear",
    "powertrain_status",
    "light",
    "door",
    "wiper",
    "turn_signal",
    "lock",
    "fault_active",
    "dtc",
    "fault_level",
    "occurrence",
    "dtc_status",
)


class CsvStateLogger:
    def __init__(self, path: str) -> None:
        output_path = Path(path).expanduser()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        existed = output_path.exists() and output_path.stat().st_size > 0
        self._file: IO[str] = output_path.open("a", newline="", encoding="utf-8")
        self._writer = csv.DictWriter(self._file, fieldnames=CSV_FIELDS)
        if not existed:
            self._writer.writeheader()
            self._file.flush()

    def write(
        self, state: VehicleState, timestamp: Optional[datetime] = None
    ) -> None:
        self._writer.writerow(state.csv_row(timestamp))
        self._file.flush()

    def close(self) -> None:
        self._file.close()

    def __enter__(self) -> "CsvStateLogger":
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()
