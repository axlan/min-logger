from min_logger.builder import _parse_args


def test_parser():
    assert _parse_args("") == [""]
    assert _parse_args(" asdasd ,dss") == ["asdasd", "dss"]
    assert _parse_args(' as"asdasd,asdasd"dasd ,dss') == ['as"asdasd,asdasd"dasd', "dss"]
    assert _parse_args(' as\\"asdasd,asdasd"dasd ,dss') == ['as\\"asdasd', 'asdasd"dasd ,dss']
    assert _parse_args('INFO, "complicated, \\"msg\\", test", value') == [
        "INFO",
        '"complicated, \\"msg\\", test"',
        "value",
    ]
