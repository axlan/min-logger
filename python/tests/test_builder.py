import logging
from pathlib import Path
import tempfile
from binascii import crc32

import pytest

from min_logger.builder import get_metric_entries, MetricEntryData


def test_successful_parsing(caplog):
    logging.basicConfig(level=logging.INFO)
    TEST_FILE = """
        MIN_LOGGER_ENTER(MIN_LOGGER_DEBUG, "TASK_LOOP");
        MIN_LOGGER_RECORD_STRING(MIN_LOGGER_INFO, "T_NAME", msg.c_str());
        MIN_LOGGER_RECORD_U64(MIN_LOGGER_INFO, "LOOP_COUNT", i);
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "task${T_NAME}: ${LOOP_COUNT}");
        MIN_LOGGER_EXIT(MIN_LOGGER_DEBUG, "TASK_LOOP");
        MIN_LOGGER_LOG_ID(MIN_LOGGER_INFO, "hello world trunc, explicit ID", 0xDEADBEEF);"""

    TEST_FILE_NAME = "test.c"

    with tempfile.TemporaryDirectory() as temp_dir:
        test_path = Path(temp_dir)
        test_file = test_path / TEST_FILE_NAME
        with open(test_file, "w") as fd:
            fd.write(TEST_FILE)

        entries = get_metric_entries([test_file], [test_path])

        with caplog.at_level(logging.WARNING):
            ID_LINE2 = crc32(b"test.c:2")
            assert ID_LINE2 in entries
            assert entries[ID_LINE2] == MetricEntryData(
                id=ID_LINE2,
                type=MetricType.ENTER,
                source_file=Path("test.c"),
                source_line=2,
                level=10,
                tags=[],
                msg=None,
                name="TASK_LOOP",
            )

            ID_LINE3 = crc32(b"test.c:3")
            assert ID_LINE3 in entries
            assert entries[ID_LINE3] == MetricEntryData(
                id=ID_LINE3,
                type=MetricType.RECORD_STRING,
                source_file=Path("test.c"),
                source_line=3,
                level=20,
                tags=[],
                msg=None,
                name="T_NAME",
            )

            ID_LINE4 = crc32(b"test.c:4")
            assert ID_LINE4 in entries
            assert entries[ID_LINE4] == MetricEntryData(
                id=ID_LINE4,
                type=MetricType.RECORD_U64,
                source_file=Path("test.c"),
                source_line=4,
                level=20,
                tags=[],
                msg=None,
                name="LOOP_COUNT",
            )

            ID_LINE5 = crc32(b"test.c:5")
            assert ID_LINE5 in entries
            assert entries[ID_LINE5] == MetricEntryData(
                id=ID_LINE5,
                type=MetricType.LOG,
                source_file=Path("test.c"),
                source_line=5,
                level=20,
                tags=[],
                msg="task${T_NAME}: ${LOOP_COUNT}",
                name=None,
            )

            ID_LINE6 = crc32(b"test.c:6")
            assert ID_LINE6 in entries
            assert entries[ID_LINE6] == MetricEntryData(
                id=ID_LINE6,
                type=MetricType.EXIT,
                source_file=Path("test.c"),
                source_line=6,
                level=10,
                tags=[],
                msg=None,
                name="TASK_LOOP",
            )

            ID_LINE7 = 0xDEADBEEF
            assert ID_LINE7 in entries
            assert entries[ID_LINE7] == MetricEntryData(
                id=ID_LINE7,
                type=MetricType.LOG,
                source_file=Path("test.c"),
                source_line=7,
                level=20,
                tags=[],
                msg="hello world trunc, explicit ID",
                name=None,
            )
    assert len(caplog.text) == 0


def test_duplicate_name_warning(caplog):
    logging.basicConfig(level=logging.INFO)
    TEST_FILE = """
        MIN_LOGGER_RECORD_STRING(MIN_LOGGER_INFO, "FOO", msg.c_str());
        MIN_LOGGER_RECORD_U64(MIN_LOGGER_INFO, "FOO", i);"""

    TEST_FILE_NAME = "test.c"

    with tempfile.TemporaryDirectory() as temp_dir:
        test_path = Path(temp_dir)
        test_file = test_path / TEST_FILE_NAME
        with open(test_file, "w") as fd:
            fd.write(TEST_FILE)

        with caplog.at_level(logging.WARNING):
            get_metric_entries([test_file], [test_path])
        assert "Duplicate metric name" in caplog.text


def test_duplicate_id():
    logging.basicConfig(level=logging.INFO)
    ID_LINE3 = crc32(b"test.c:3")
    TEST_FILE = f"""
        MIN_LOGGER_LOG_ID(MIN_LOGGER_INFO, "hello1", {ID_LINE3});
        MIN_LOGGER_LOG(MIN_LOGGER_INFO, "hello2");"""

    TEST_FILE_NAME = "test.c"

    with tempfile.TemporaryDirectory() as temp_dir:
        test_path = Path(temp_dir)
        test_file = test_path / TEST_FILE_NAME
        with open(test_file, "w") as fd:
            fd.write(TEST_FILE)

        with pytest.raises(ValueError) as excinfo:
            get_metric_entries([test_file], [test_path])
        assert "Duplicate ID" in str(excinfo.value)


def test_string_literal():
    logging.basicConfig(level=logging.INFO)
    TEST_FILE = "MIN_LOGGER_LOG(MIN_LOGGER_INFO, hello2);"

    TEST_FILE_NAME = "test.c"

    with tempfile.TemporaryDirectory() as temp_dir:
        test_path = Path(temp_dir)
        test_file = test_path / TEST_FILE_NAME
        with open(test_file, "w") as fd:
            fd.write(TEST_FILE)

        with pytest.raises(ValueError) as excinfo:
            get_metric_entries([test_file], [test_path])
        assert "not string literal" in str(excinfo.value)


def test_malformed_id():
    logging.basicConfig(level=logging.INFO)
    TEST_FILE = 'MIN_LOGGER_LOG_ID(MIN_LOGGER_INFO, "hello1", a123);'

    TEST_FILE_NAME = "test.c"

    with tempfile.TemporaryDirectory() as temp_dir:
        test_path = Path(temp_dir)
        test_file = test_path / TEST_FILE_NAME
        with open(test_file, "w") as fd:
            fd.write(TEST_FILE)

        with pytest.raises(ValueError) as excinfo:
            get_metric_entries([test_file], [test_path])
        assert "Could not parse ID" in str(excinfo.value)
