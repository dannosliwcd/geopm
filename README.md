# GEOPM Tools
This project contains CLI tools build on top of GEOPM.

## Installation
You may need to install geopmdpy locally if it is not already
available. To do so, run: `python3 -m pip install -e ~/geopm/service`.
Then install this package. For example:

    pip install -e 'git+ssh://git@github.com/geopm/geopm.git#egg=geopmtools&subdirectory=geopmtools'

## Usage

Run `geopmtrace -h` to see usage of the tracer.

## Additional configuration
Persistent configuration options can be written to
 `~/.config/geopmtools/configuration.ini`. The file follows the python
[configparser](https://docs.python.org/3/library/configparser.html)
file format composed of `[SECTION_NAME]` and `key = value` lines.

`editor` section:
 * `vi_mode`: (`yes`/`no`, `on`/`off`, `true`/`false` and `1`/`0`, default:
   `off`). When enabled, interactive text-editors use vi-style key bindings.
