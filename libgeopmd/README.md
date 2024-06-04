libgeopmd Directory
-------------------
C and C++ source files for libgeopmd and associated command line tools.
Directories include:

* [contrib](contrib): Third-party contributed code
* [debian](debian): Configuration files for debian packaging scripts
* [fuzz_test](fuzz_test): Corpus and test automation for fuzz tests
* [include](include): Public headers installed for use with `libgeopmd`
* [src](src): Source code, including headers that do not get installed
* [test](test): Test code for `libgeopmd`

Building
========
Steps to build `libgeopmd` are as follows:

1. Generate a build directory. E.g., the following command creates a directory
   named `builddir` with the build prefix overridden to `$HOME/build/geopm`:
   `meson setup --prefix=$HOME/build/geopm builddir`. Additional configuration
   options can either be specified now, or later by running `meson configure builddir` (If you don't add any additional options, it will print out the options that are available to you).
2. Build `libgeopmd` using the build directory you just created: `meson compile -C builddir`
3. Install `libgeopmd` using the build directory you just created: `meson install -C builddir`

Testing
-------
Run `meson test -C builddir` to build and run the test suite. Basic pass/fail
information is printed to the screen. Add the `--verbose` option to 
display more detailed test logs.

Run a subset of tests by using [gtest filters](https://google.github.io/googletest/advanced.html#running-a-subset-of-the-tests). For example, to run only ``HelperTest`` test cases, run ``GTEST_FILTER='HelperTest*' meson test -C builddir``.
