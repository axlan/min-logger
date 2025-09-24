from dataclasses import dataclass, field
import logging
import re
from typing import Any, TextIO

from min_logger.builder import MetricEntryData, MetricType, THREAD_NAME_MSG_ID, SEVERITY_LEVELS


_logger = logging.getLogger("min_logger.parser")


@dataclass
class ParsingState:
    last_values: dict[str, str] = field(default_factory=dict)
    unknown_ids: set[int] = field(default_factory=set)
    thread_names: dict[int, str] = field(default_factory=dict)


def _substitute_vars(text, values):
    pattern = r"\$\{(.+?)\}"

    def replacer(match):
        key = match.group(1)
        return str(values.get(key, match.group(0)))

    return re.sub(pattern, replacer, text)


def _severity_string(severity):
    if severity <= SEVERITY_LEVELS["DEBUG"]:
        return "DEBUG"
    elif severity <= SEVERITY_LEVELS["INFO"]:
        return "INFO"
    elif severity <= SEVERITY_LEVELS["WARN"]:
        return "WARN"
    elif severity <= SEVERITY_LEVELS["ERROR"]:
        return "ERROR"
    else:
        return "CRITICAL"


def print_msg(
    timestamp: float,
    metric_id: int,
    thread_id: int,
    value: str,
    meta: dict[int, MetricEntryData],
    parsing_state: ParsingState,
):
    if metric_id == THREAD_NAME_MSG_ID:
        parsing_state.thread_names[thread_id] = value
        return

    if metric_id not in meta:
        if metric_id not in parsing_state.unknown_ids:
            _logger.warning("Metric with unknown ID: 0x%08X", metric_id)
            parsing_state.unknown_ids.add(metric_id)
        return

    metric = meta[metric_id]

    if metric.type in (MetricType.RECORD_STRING, MetricType.RECORD_U64) and metric.name is not None:
        parsing_state.last_values[metric.name] = value
        return

    thread_name = (
        f"thread_id_{thread_id}"
        if thread_id not in parsing_state.thread_names
        else parsing_state.thread_names[thread_id]
    )

    if metric.type == MetricType.LOG and metric.msg is not None:
        print(
            f"{timestamp:.6f} {_severity_string(metric.level):5} {metric.source_file}:{metric.source_line} {thread_name}] {_substitute_vars(metric.msg, parsing_state.last_values)}"
        )


def read_text(fd: TextIO, meta: dict[int, MetricEntryData]):
    parsing_state = ParsingState()
    for line in fd:
        if line.startswith("$"):
            values = line.strip().split(",")
            if len(values) < 3:
                continue
            try:
                timestamp = float(values[0][1:])
                metric_id = int(values[1], 16)
                thread_id = int(values[2], 16)
                value = ",".join(values[3:])
                print_msg(timestamp, metric_id, thread_id, value, meta, parsing_state)
                continue
            except ValueError:
                _logger.warning("Error parsing line: ", exc_info=True)
        print(line, end="")

    if len(parsing_state.unknown_ids) > 0:
        _logger.warning("Log contained unknown IDs: %s", str(parsing_state.unknown_ids))
