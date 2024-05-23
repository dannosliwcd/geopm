#  Copyright (c) 2015 - 2024 Intel Corporation
#  SPDX-License-Identifier: BSD-3-Clause
#


import sys

PYTHON_PATHS = [
        "/usr/lib/python3.6/site-packages",
        "/usr/lib64/python3.6/site-packages"]

for p in PYTHON_PATHS:
    if p not in sys.path:
        sys.path.insert(0, p)

import os
import json
import copy

import pbs

from geopmdpy import pio
from geopmdpy import system_files


_SAVED_CONTROLS_PATH = "/run/geopm/pbs-hooks/SAVE_FILES"
_SAVED_CONTROLS_FILE = _SAVED_CONTROLS_PATH + "/power-limit-save-control.json"
_POWER_LIMIT_RESOURCE = "geopm-node-power-limit"
_MAX_POWER_LIMIT_RESOURCE = "geopm-max-node-power-limit"
_MIN_POWER_LIMIT_RESOURCE = "geopm-min-node-power-limit"
_JOB_TYPE_RESOURCE = "geopm-job-type"

_power_limit_control = {
        "name": "MSR::PLATFORM_POWER_LIMIT:PL1_POWER_LIMIT",
        "domain_type": "board",
        "domain_idx": 0,
        "setting": None
    }
_controls = [
        {"name": "MSR::PLATFORM_POWER_LIMIT:PL1_TIME_WINDOW",
         "domain_type": "board",
         "domain_idx": 0,
         "setting": 0.013}, # SDM Vol. 4. Table 2.39 - Recommends 0xD = 13
        {"name": "MSR::PLATFORM_POWER_LIMIT:PL1_CLAMP_ENABLE",
         "domain_type": "board",
         "domain_idx": 0,
         "setting": 1},
        _power_limit_control,
        {"name": "MSR::PLATFORM_POWER_LIMIT:PL1_LIMIT_ENABLE",
         "domain_type": "board",
         "domain_idx": 0,
         "setting": 1}
    ]

def read_controls(controls):
    try:
        for c in controls:
            c["setting"] = pio.read_signal(c["name"], c["domain_type"],
                                           c["domain_idx"])
    except RuntimeError as e:
        reject_event(f"Unable to read signal {c['name']}: {e}")


def write_controls(controls):
    try:
        for c in controls:
            pio.write_control(c["name"], c["domain_type"], c["domain_idx"],
                              c["setting"])
    except RuntimeError as e:
        reject_event(f"Unable to write control {c['name']}: {e}")


def resource_to_float(resource_name, resource_str):
    try:
        value = float(resource_str)
        return value
    except ValueError:
        reject_event(f"Invalid value provided for: {resource_name}")


def save_controls_to_file(file_name, controls):
    try:
        with open(file_name, "w") as f:
            f.write(json.dumps(controls))
    except (OSError, ValueError) as e:
        reject_event(f"Unable to write to saved controls file: {e}")


def reject_event(msg):
    e = pbs.event()
    if e.type in [pbs.EXECJOB_PROLOGUE, pbs.EXECJOB_EPILOGUE]:
        e.job.delete()
    e.reject(f"{e.hook_name}: {msg}")


def restore_controls_from_file(file_name):
    controls_json = None
    try:
        with open(file_name) as f:
            controls_json = f.read()
    except (OSError, ValueError) as e:
        reject_event(f"Unable to read saved controls file: {e}")

    try:
        controls = json.loads(controls_json)
        if not controls:
            reject_event("Encountered empty saved controls file")
        write_controls(controls)
    except (json.decoder.JSONDecodeError, KeyError, TypeError) as e:
        reject_event(f"Malformed saved controls file: {e}")
    os.unlink(file_name)


def do_power_limit_prologue():
    e = pbs.event()
    job_id = e.job.id
    server_job = pbs.server().job(job_id)

    node_power_limit_requested = False
    try:
        node_power_limit_str = server_job.Resource_List[_POWER_LIMIT_RESOURCE]
        node_power_limit_requested = bool(node_power_limit_str)
    except KeyError:
        pass

    if not node_power_limit_requested:
        if os.path.exists(_SAVED_CONTROLS_FILE):
            restore_controls_from_file(_SAVED_CONTROLS_FILE)
        e.accept()
        return

    if node_power_limit_requested:
        # The user requested a specific node power limit. Do not modify it.
        power_limit = resource_to_float(_POWER_LIMIT_RESOURCE, node_power_limit_str)

    pbs.logmsg(pbs.LOG_DEBUG, f"{e.hook_name}: Requested power limit: {power_limit}")
    current_settings = copy.deepcopy(_controls)
    read_controls(current_settings)
    system_files.secure_make_dirs(_SAVED_CONTROLS_PATH)
    save_controls_to_file(_SAVED_CONTROLS_FILE, current_settings)
    _power_limit_control["setting"] = power_limit
    write_controls(_controls)
    e.accept()


def do_power_limit_epilogue():
    e = pbs.event()
    if os.path.exists(_SAVED_CONTROLS_FILE):
        restore_controls_from_file(_SAVED_CONTROLS_FILE)
    e.accept()

def do_power_limit_queuejob():
    """GEOPM handler for queuejob PBS events. This handler clamps the
    user's requested power cap to the administrator's allowed range.
    """
    server = pbs.server()

    requested_resources = pbs.event().job.Resource_List
    submitted_node_limit = requested_resources[_POWER_LIMIT_RESOURCE]
    if (submitted_node_limit is None):
		# No limit has been specified by the user, so we have nothing to do.
        return

    min_power_per_node = server.resources_available[_MIN_POWER_LIMIT_RESOURCE]
    if min_power_per_node is None:
        pbs.event().reject(f'{_MIN_POWER_LIMIT_RESOURCE} must be configured by an administrator.')

    max_power_per_node = server.resources_available[_MAX_POWER_LIMIT_RESOURCE]
    if max_power_per_node is None:
        pbs.event().reject(f'{_MAX_POWER_LIMIT_RESOURCE} must be configured by an administrator.')

    min_power_per_node = float(min_power_per_node)
    max_power_per_node = float(max_power_per_node)

    requested_resources[_POWER_LIMIT_RESOURCE] = max(min_power_per_node, min(submitted_node_limit, max_power_per_node))


def hook_main():
    try:
        event_type = pbs.event().type
        if event_type == pbs.EXECJOB_PROLOGUE:
            do_power_limit_prologue()
        elif event_type == pbs.EXECJOB_EPILOGUE:
            do_power_limit_epilogue()
        elif event_type == pbs.QUEUEJOB:
            do_power_limit_queuejob()
        else:
            reject_event("Power limit hook incorrectly configured!")
    except SystemExit:
        pass
    except:
        _, e, _ = sys.exc_info()
        reject_event(f"Unexpected error: {str(e)}")


# Begin hook...
hook_main()
