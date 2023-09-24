#!/usr/bin/env python3
#  Copyright (c) 2015 - 2023, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#
import unittest
import os
import sys

from tap import TAPTestRunner

if __name__ == "__main__":
    tests_dir = os.path.dirname(os.path.abspath(__file__))
    top_dir = os.path.join(tests_dir, '..')
    loader = unittest.TestLoader()
    tests = loader.discover(start_dir=tests_dir, pattern='Test*', top_level_dir=top_dir)
    runner = TAPTestRunner()
    runner.set_stream(True)
    runner.set_format("{method_name} - {short_description}")
    result = runner.run(tests)
