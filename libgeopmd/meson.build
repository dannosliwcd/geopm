project('libgeopmd', 'c', 'cpp',
        default_options : ['c_std=c99', 'cpp_std=c++17'],
        version: '2.0.0')

fs_module = import('fs')

geopm_abi_version = meson.project_version().replace('.', ':')

cc = meson.get_compiler('c')
cxx = meson.get_compiler('cpp')
prog_sed = find_program('sed')

conf = configuration_data()
conf.set_quoted('GEOPM_ABI_VERSION', geopm_abi_version)

git_version_command = run_command('sh', '-c', 'python3 -m setuptools_scm --root "${MESON_SOURCE_ROOT}/.."', capture: true, check: true)
git_version = git_version_command.stdout().strip()
conf.set_quoted('VERSION', git_version)

systemd_dep = dependency('libsystemd', version: '>= 234', required: get_option('enable-systemd'))
if systemd_dep.found()
  conf.set('GEOPM_ENABLE_SYSTEMD', 1)
endif

libcap_dep = dependency('libcap', version: '>= 2.26', required: get_option('enable-libcap'))
if libcap_dep.found()
  conf.set('GEOPM_ENABLE_LIBCAP', 1)
endif

if not get_option('enable-rawmsr').disabled()
  conf.set('GEOPM_ENABLE_RAWMSR', 1)
endif

if cc.check_header('cpuid.h', required: get_option('enable-cpuid'))
  conf.set('GEOPM_ENABLE_CPUID', 1)
endif

levelzero_dep = dependency('level-zero', version: '>= 1.13.1', required: get_option('enable-levelzero'))
if levelzero_dep.found()
  conf.set('GEOPM_ENABLE_LEVELZERO', 1)
endif

# TODO: This package doesn't come with a pkg-config file. May just need to make it manual:
nvml_dep = dependency('nvidia-ml', version: '>= 11.5.50', required: get_option('enable-nvml'))
if nvml_dep.found()
  conf.set('GEOPM_ENABLE_NVML', 1)
endif

# TODO: This package doesn't come with a pkg-config file. May just need to make it manual:
dcgm_dep = dependency('datacenter-gpu-manager', version: '>= 3.3.5', required: get_option('enable-dcgm'))
if dcgm_dep.found()
  conf.set('GEOPM_ENABLE_DCGM', 1)
endif

if not get_option('enable-cnl-iogroup').disabled()
  conf.set('GEOPM_CNL_IOGROUP', 1)
endif

# liburing 2.2 doesn't support everything we need, but it at least does support 
# the feature interrogation interface we use to detect what we can use.
uring_dep = dependency('liburing', version: '>= 2.2', required: get_option('enable-io-uring'))
if uring_dep.found()
  conf.set('GEOPM_HAS_IO_URING', 1)
  if cc.has_function('io_uring_free_probe', prefix: '#include <liburing.h>')
    # Newer versions of liburing provide an function to release probe
    # resources, but with older versions the user must call free(3) to
    # release the resources.  This check sets a preprocessor define
    # to identify which is required.
    conf.set('GEOPM_IO_URING_HAS_FREE', 1)
  endif
endif

conf.set_quoted('GEOPM_CONFIG_PATH', get_option('geopm-config-path'))
conf.set_quoted('GEOPM_DEFAULT_PLUGIN_PATH', get_option('prefix') / get_option('libdir') / 'geopm')

# TODO: disable fast math (for nan checks)

# TODO: CFI sanitizer has a variable in configure.ac. Is it used?

thread_dep = dependency('threads')

# TODO: check for cpuid header?




zlib_dep = dependency('zlib', not_found_message: 'Required for crc32 interface')

# Once we drop support for Python 3.6 we can upgrade to meson 0.62 or later,
# which automatically handle 'dl' as a dependency type. Marked non-required
# here because some glibc versions include it but others need to be linked to
# it separately.
dl_dep = cc.find_library('dl', required: false) # Needed for IOGroup plugins

# TODO: Assert that double is 64 bits. Or do in whichever source file actually
#       requires that (commtree?)

# TODO: setup warning flags, PIC, PIE for compiler
# TODO: setup relro, noexecstat, pie for linker
# TODO: setup LTO for non-debug linker

# TODO: set GEOPM_SOURCE_DIR macro. Always? Debug-only?

# TODO: register tests

configure_file(output: 'config.h',
               configuration: conf)

private_include = include_directories('src')
public_include = include_directories('include')
configuration_include = include_directories('.')



subdir('contrib')
subdir('include')
subdir('src')
subdir('test')

pkg_module = import('pkgconfig')
pkg_module.generate(libraries: libgeopmd,
                    name: 'libgeopmd',
                    filebase: 'libgeopmd',
                    description: 'Library providing implementations used by the GEOPM service and by clients of that service.')
