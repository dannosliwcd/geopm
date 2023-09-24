#  Copyright (c) 2015 - 2023, Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#

EXTRA_DIST += geopmdpy_test/__init__.py \
              geopmdpy_test/__main__.py \
	      geopmdpy_test/run_geopmdpy_tests.py \
              geopmdpy_test/geopmdpy.test \
              geopmdpy_test/geopmdpy_test.sh \
              geopmdpy_test/TestAccess.py \
              geopmdpy_test/TestConfigPath.py \
              geopmdpy_test/TestPlatformService.py \
              geopmdpy_test/TestDBusXML.py \
              geopmdpy_test/TestError.py \
              geopmdpy_test/TestPIO.py \
              geopmdpy_test/TestTopo.py \
              geopmdpy_test/TestRequestQueue.py \
              geopmdpy_test/TestRestorableFileWriter.py \
              geopmdpy_test/TestSession.py \
              geopmdpy_test/TestActiveSessions.py \
              geopmdpy_test/TestSecureFiles.py \
              geopmdpy_test/TestAccessLists.py \
              geopmdpy_test/TestWriteLock.py \
              geopmdpy_test/TestMSRDataFiles.py \
              geopmdpy_test/TestTimedLoop.py \
              # end

TESTS += geopmdpy_test/geopmdpy.test
