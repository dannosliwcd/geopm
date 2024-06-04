gtest_project = subproject('gtest')
gtest_dependency = gtest_project.get_variable('gtest_dep')
gmock_dependency = gtest_project.get_variable('gmock_dep')

test_sources = files(
  'AggTest.cpp',
  'BatchClientTest.cpp',
  'BatchServerTest.cpp',
  'BatchStatusTest.cpp',
  'CNLIOGroupTest.cpp',
  'CircularBufferTest.cpp',
  'CombinedSignalTest.cpp',
  'ConstConfigIOGroupTest.cpp',
  'CpufreqSysfsDriverTest.cpp',
  'CpuinfoIOGroupTest.cpp',
  'DCGMIOGroupTest.cpp',
  'DerivativeSignalTest.cpp',
  'DifferenceSignalTest.cpp',
  'DomainControlTest.cpp',
  'ExceptionTest.cpp',
  'GEOPMHintTest.cpp',
  'GPUTopoNullTest.cpp',
  'HelperTest.cpp',
  'IOGroupTest.cpp',
  'IOUringTest.cpp',
  'LevelZeroDevicePoolTest.cpp',
  'LevelZeroGPUTopoTest.cpp',
  'LevelZeroIOGroupTest.cpp',
  'MSRFieldControlTest.cpp',
  'MSRFieldSignalTest.cpp',
  'MSRIOGroupTest.cpp',
  'MSRIOTest.cpp',
  'NVMLGPUTopoTest.cpp',
  'NVMLIOGroupTest.cpp',
  'POSIXSignalTest.cpp',
  'PlatformIOTest.cpp',
  'PlatformTopoTest.cpp',
  'RatioSignalTest.cpp',
  'RawMSRSignalTest.cpp',
  'SSTControlTest.cpp',
  'SSTIOGroupTest.cpp',
  'SSTIOTest.cpp',
  'SSTSignalTest.cpp',
  'SaveControlTest.cpp',
  'SecurePathTest.cpp',
  'ServiceIOGroupTest.cpp',
  'ServiceProxyTest.cpp',
  'SharedMemoryTest.cpp',
  'SysfsIOGroupTest.cpp',
  'TimeIOGroupTest.cpp',
  'UniqueFdTest.cpp',
)

foreach test_source : test_sources
  test_name = fs_module.stem(test_source)
  test_executable = executable(
      test_name,
      'geopm_test.cpp',
      'geopm_test_helper.cpp',
      'MockPlatformTopo.cpp',
      test_source,
      include_directories: [configuration_include, private_include, public_include],
      link_with: libgeopmd,
      dependencies: [gtest_dependency, gmock_dependency],
  )
  test(test_name, test_executable,
    #args: ['--tap-out-path', '-'],
    protocol: 'gtest')
endforeach
