<img src="https://geopm.github.io/images/geopm-banner.png" alt="GEOPM logo">

# GEOPM - Global Extensible Open Power Manager

[![Build Status](https://github.com/geopm/geopm/actions/workflows/build.yml/badge.svg)](https://github.com/geopm/geopm/actions)
[![version](https://img.shields.io/badge/version-3.0.1-blue)](https://github.com/geopm/geopm/releases)
[![License](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)


## Web Pages

https://geopm.github.io <br>
https://geopm.github.io/service.html <br>
https://geopm.github.io/runtime.html <br>
https://geopm.github.io/reference.html <br>
https://geopm.slack.com


## Summary

The Global Extensible Open Power Manager (GEOPM) is a collaborative
framework for exploring power and energy optimizations on
heterogeneous platforms. GEOPM is open-source (BSD licensed) and
developed to enable efficient power management and performance
optimizations. With this powerful tool, users are able to monitor
their system's energy and power consumption, and safely optimize
system hardware settings to achieve energy efficiency and/or
performance objectives.

With GEOPM you can:

- Interact with hardware settings and sensors using a
  platform-agnostic interface
- Restore changes to hardware settings when configuring process
  session terminates
- Profile applications to study their power and energy behavior
- Automatically detect MPI and OpenMP phases in an application
- Optimize applications to improve energy efficiency or reduce the
  effects of work imbalance, system jitter, and manufacturing variation
  through built-in control algorithms
- Develop your own runtime control algorithms through the extensible
  plugin architecture


The GEOPM software is separated into two major functions: Hardware
Access and Runtime Optimization.  These two services are referred to
as the "GEOPM Access Service" and the "GEOPM Runtime Service", where
the GEOPM Runtime Service builds on the GEOPM Access Service features
and provides a mechanism for user extension.  The GEOPM Access Service
is a privileged service, and the Runtime Service may run without any
privilege beyond the capabilities granted by the GEOPM Access Service.


## GEOPM Access Service

Please refer to the `docs/source/service_readme.rst` file for a high
level overview of the GEOPM Access Service.  This summary and more
detailed information about how to install and configure GEOPM Access
Service is posted here:

https://geopm.github.io/service.html


Two of the software packages provided by the GEOPM Git repository
support the GEOPM Access Service: `libgeopmd` and `geopmdpy`. The
`libgeopmd` package is written in C++ and provides bindings for the
C language.  The `geopmdpy` package is written in Python and
provides wrappers for many of the C interfaces provided by the
`libgeopmd`.


## GEOPM Runtime Service

The GEOPM Access Service provides a foundation for manipulating
hardware settings to optimize an objective defined by an unprivileged
user.  The GEOPM Runtime is a software platform built on top of the
GEOPM Service that enables users to select a runtime algorithm and
policy to meet energy efficiency or performance objectives.  More
documentation on the GEOPM Runtime is posted with our web
documentation here:

https://geopm.github.io/runtime.html


Two of the software packages provided by the GEOPM Git repository
support the GEOPM Runtime Service: `libgeopm` and `geopmpy`. The
`libgeopm` package is written in C++ and provides bindings for the
C language.  The `geopmpy` package is written in Python and
provides wrappers for some of the C interfaces provided by the
`libgeopmd`.

## Repository Directories

Each of the top level directories of the GEOPM Git repository contain
their own README file.  Please refer to these files for more detailed
information about each subdirectory.


### libgeopmd

The C++ implementation for the GEOPM Access Service.  This directory
has an autotools based build system which can be used to create a
distribution for the `libgeopmd` software package.  The library
provides the `PlatformIO` interface and support for `IOGroup`
plugins.  Additionally the Linux System-D service files and installation
is managed by the build system in this directory.


### geopmdpy

The Python implementation for the GEOPM Access Service.  This
directory has a setuptools based build system which can be used to
create a distribution for the `geopmdpy` software package for Python
3.6 and higher versions.  The `geopmdpy` Python module provides the
implementation for the geopmd daemon executed by the Linux System-D
unit and Python bindings for the PlatformIO interface.


### libgeopm

PASS


### geopmpy

PASS


### docs

PASS


### integration

PASS


## Package Dependencies

- A build of the `libgeopmd` and `geopmdpy` packages are required to
  build and run the `libgeopm` and `geopmpy` packages

- All `libgeopmd` and `geopmdpy` package features may be used
  independently of the features provided by the `libgeopm` and
  `geopmpy` packages

- All run and build requirements of the `libgeopmd` and `geopmdpy`
  packages are provided by commonly used Linux distributions

- Some of the features of the `libgeopm` and `geopmpy` packages
  require HPC specific dependencies (MPI, OpenMP, and HPC resource
  manager integration), but these features may be disabled when
  configuring the build


## Guide for Contributors

We appreciate all feedback on our project.  Please see our
contributing guide for how some guidelines on how to participate.
This guide is located in the root of the GEOPM repository in a file
called `CONTRIBUTING.rst`.  This guide can also be viewed here:

https://geopm.github.io/contrib.html


## Guide for GEOPM Developers

GEOPM is an open development project and we use Github to plan, review
and test our work.  The process we follow is documented here:

https://geopm.github.io/devel.html

this web page provides a guide for developers wishing to modify source
code anywhere in the GEOPM repository for both the `geopm-service` and
the `geopm` packages.


## Status

GEOPM version 3.0 enables the GEOPM Runtime to be used with any
application.  It shifts responsibility for managing inter-process
communication between the application and the GEOPM controller to the
GEOPM Service.  The start up of the GEOPM runtime is done by the
libgeopm library initialization rather than through interposition on
MPI initialization functions.

GEOPM version 2.0 provides a number of important changes since the
previous tagged release v1.1.0.  Some of the most significant new
features are the GEOPM Service, support for Intel and NVIDIA GPUs, and
improved consistency of signal and control names provided by
PlatformIO.  A wide range of other improvements have also been made,
including a higher performance profiling interface to support highly
parallel applications, and support for the `isst_interface` driver.

This software is production quality as of version 1.0.  We will be
enforcing [semantic versioning](https://semver.org/) for all releases
following version 1.0. Please refer to the ChangeLog for a high level
history of changes in each release.  The test coverage report from
gcov as reported by gcovr for the latest release are
[posted to our web page](http://geopm.github.io/coverage/index.html).

Some new features of GEOPM are still under development, and their
interfaces may change before they are included in official releases.
To enable these features in the GEOPM install location, configure
GEOPM with the `--enable-beta` configure flag.  The features currently
considered unfinalized are the endpoint interface, and the
`geopmendpoint` application.  The CPU-CA and GPU-CA agents are also
beta features.

The GEOPM developers are very interested in feedback from the
community.  See the [contributing guide](CONTRIBUTING.md) to learn how
to provide feedback.

## License

The GEOPM source code is distributed under the 3-clause BSD license.

SEE COPYING FILE FOR LICENSE INFORMATION.

## Last Update

2024 April 10

Christopher Cantalupo <christopher.m.cantalupo@intel.com> <br>
Brad Geltz <brad.geltz@intel.com> <br>

## ACKNOWLEDGMENTS

Development of the GEOPM software package has been partially funded
through contract B609815 with Argonne National Laboratory.
