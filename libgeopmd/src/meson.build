#json_gen = generator(prog_sed,
#                    output: '@BASENAME@.cpp',
#                    capture: true,
#                    arguments: [
#                      '-e', '/@JSON_CONTENTS@/ {',
#                      '-e', 'r @INPUT@',
#                      '-e', 'd', '-e', '}',
#                      '-e', 's/@JSON_IDENTIFIER@/$(lastword $(subst _, ,$*))_@EXTRA_ARGS@_json/g',
#                      'json_data.cpp.in'])
#msr_gen_src = json_gen.process('json_data'/'msr_data_arch.json',
#                              'json_data'/'msr_data_hsx.json',
#                              'json_data'/'msr_data_knl.json',
#                              'json_data'/'msr_data_skx.json',
#                              'json_data'/'msr_data_snb.json',
#                              'json_data'/'msr_data_spr.json',
#                              extra_args: 'msr')
#sysfs_gen_src = json_gen.process('json_data'/'sysfs_attributes_cpufreq.json',
#                                 extra_args: 'sysfs')

msr_json = files('json_data'/'msr_data_arch.json',
                 'json_data'/'msr_data_hsx.json',
                 'json_data'/'msr_data_knl.json',
                 'json_data'/'msr_data_skx.json',
                 'json_data'/'msr_data_snb.json',
                 'json_data'/'msr_data_spr.json')
sysfs_json = files('json_data'/'sysfs_attributes_cpufreq.json')

json_gen_files = []
foreach f : msr_json
  target = custom_target(fs_module.replace_suffix(f, '.cpp'),
                         input: files('json_data.cpp.in') + [f],
                         output: fs_module.stem(f) + '.cpp',
                         capture: true,
                         command: [prog_sed,
                                   '-e', '/@JSON_CONTENTS@/ {',
                                   '-e', 'r @INPUT1@',
                                   '-e', 'd', '-e', '}',
                                   '-e', 's/@JSON_IDENTIFIER@/' + fs_module.stem(f).split('_')[-1] + '_msr_json/g',
                                   '@INPUT0@'])
  json_gen_files += target
endforeach

foreach f : sysfs_json
  target = custom_target(fs_module.replace_suffix(f, '.cpp'),
                         input: files('json_data.cpp.in') + [f],
                         output: fs_module.stem(f) + '.cpp',
                         capture: true,
                         command: [prog_sed,
                                   '-e', '/@JSON_CONTENTS@/ {',
                                   '-e', 'r @INPUT1@',
                                   '-e', 'd', '-e', '}',
                                   '-e', 's/@JSON_IDENTIFIER@/' + fs_module.stem(f).split('_')[-1] + '_sysfs_json/g',
                                   '@INPUT0@'])
  json_gen_files += target
endforeach




libgeopmd_sources = files(
  'Agg.cpp',
  'BatchClient.cpp',
  'BatchServer.cpp',
  'BatchStatus.cpp',
  'CNLIOGroup.cpp',
  'CombinedControl.cpp',
  'CombinedSignal.cpp',
  'ConstConfigIOGroup.cpp',
  'CpufreqSysfsDriver.cpp',
  'Cpuid.cpp',
  'CpuinfoIOGroup.cpp',
  'DCGMIOGroup.cpp',
  'DerivativeSignal.cpp',
  'DifferenceSignal.cpp',
  'DomainControl.cpp',
  'Exception.cpp',
  'GEOPMHint.cpp',
  'GPUTopo.cpp',
  'GPUTopoNull.cpp',
  'Helper.cpp',
  'IOGroup.cpp',
  'IOUring.cpp',
  'IOUringFallback.cpp',
  'LevelZeroDevicePool.cpp',
  'LevelZeroGPUTopo.cpp',
  'LevelZeroIOGroup.cpp',
  'LevelZeroSignal.cpp',
  'MSR.cpp',
  'MSRFieldControl.cpp',
  'MSRFieldSignal.cpp',
  'MSRIO.cpp',
  'MSRIOGroup.cpp',
  'MSRPath.cpp',
  'MultiplicationSignal.cpp',
  'NVMLGPUTopo.cpp',
  'NVMLIOGroup.cpp',
  'POSIXSignal.cpp',
  'PlatformIO.cpp',
  'PlatformTopo.cpp',
  'RatioSignal.cpp',
  'RawMSRSignal.cpp',
  'SSTControl.cpp',
  'SSTIO.cpp',
  'SSTIOGroup.cpp',
  'SSTIoctl.cpp',
  'SSTSignal.cpp',
  'SaveControl.cpp',
  'SecurePath.cpp',
  'ServiceIOGroup.cpp',
  'ServiceProxy.cpp',
  'SharedMemory.cpp',
  'SharedMemoryScopedLock.cpp',
  'SysfsDriver.cpp',
  'SysfsIOGroup.cpp',
  'TimeIOGroup.cpp',
  'TimeSignal.cpp',
  'TimeZero.cpp',
  'UniqueFd.cpp',
  'geopm_hash.cpp',
  'geopm_plugin.cpp',
  'geopm_sched.c',
  'geopm_shmem.cpp',
  'geopm_version.c',
)

if systemd_dep.found()
  libgeopmd_sources += files('SDBus.cpp', 'SDBusMessage.cpp')
else
  libgeopmd_sources += files('SDBusNull.cpp')
endif

if levelzero_dep.found()
  libgeopmd_sources += files('LevelZero.cpp')
else
  libgeopmd_sources += files('LevelZeroThrow.cpp')
endif

if nvml_dep.found()
  libgeopmd_sources += files('NVMLDevicePool.cpp')
else
  libgeopmd_sources += files('NVMLDevicePoolThrow.cpp')
endif

if dcgm_dep.found()
  libgeopmd_sources += files('DCGMDevicePool.cpp')
else
  libgeopmd_sources += files('DCGMDevicePoolThrow.cpp')
endif

if uring_dep.found()
  libgeopmd_sources += files('IOUringImp.cpp')
endif

libgeopmd = shared_library(
    'geopmd',
    libgeopmd_sources,
    json11_sources,
    json_gen_files,
    include_directories: [configuration_include, public_include],
    dependencies: [systemd_dep, libcap_dep, levelzero_dep, nvml_dep, dcgm_dep, uring_dep, thread_dep, zlib_dep, dl_dep],
    install: true)
