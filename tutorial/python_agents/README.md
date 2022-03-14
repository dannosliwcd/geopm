# Python Agent Tutorial

This tutorial demonstrates how to implement a python agent in geopm. Specifically,
the tutorial shows how to use an agent to generate data to train a tensorflow
model for frequency control selection on a system with CPUs and GPUs.

The following steps are required to use this agent:
1. Perform frequency sweeps to generate data for model training.
2. Process the frequency sweeps to condition the data for model training.
3. Train a neural network model using the processed trace data.

## 1) Perform frequency sweeps
Use GEOPM to run a suite of workloads with varying performance and energy
responses to GPU frequency settings. Be sure to include at least the following
signals in your traces and reports:
 * GPU\_FREQUENCY\_STATUS@board\_accelerator
 * GPU\_ENERGY@board\_accelerator
 * GPU\_POWER@board\_accelerator
 * GPU\_UTILIZATION@board\_accelerator
 * GPU\_COMPUTE\_ACTIVITY@board\_accelerator
 * DCGM::DRAM\_ACTIVE@board\_accelerator

Ensure each file in the sweep contains a unique name to avoid overwriting report
or trace contents at later steps of the sweep. For example:

```
<appname>.gpufreq_<freq_in_GHz>.memratio_<mem_ratio>.precision_<precision>.trial_<trial>.report
<appname>.gpufreq_<freq_in_GHz>.memratio_<mem_ratio>.precision_<precision>.trial_<trial>.trace-<hostname>
```

The following are possible fields in the name:
 * appname: A name that identifies the application. E.g., stream or dgemm
 * freq\_in\_GHz: GPU core frequency limit applied on this report or trace
 * memratio: mem ratio for runs of the mixbench benchmark, or NaN.
 * precision: precision for runs of the mixbench benchmark, or NaN.
 * trial: number of the trial, for repeated executions.
 * hostname: name of the server associated with the trace file.

The GEOPM integration infrastructure has built-in support for frequency sweeps.
You can also use the agent in this tutorial in a shell script to generate a
frequency sweep. For example:

```bash
out_dir=${HOME}/my_sweep_$$
mkdir -p "${out_dir}"
host=$(hostname)
APPNAME=myappname
EXEC=/path/to/your/workload

# Alternatively, generate a list from `geopmread GPU_FREQUENCY_MIN_AVAIL board 0`
# and `geopmread GPU_FREQUENCY_MAX_AVAIL board 0`
supported_freqs_mhz=$(nvidia-smi -q -d SUPPORTED_CLOCKS -i 0 | awk -e '/Graphics/ { print $3 }')
max_cpu_freq=$(geopmread FREQUENCY_MAX board 0)

for trial in {1..3}
do
    for gpu_freq in $supported_freqs_mhz
    do
        agent='monitor'
        set -x
        ./base_agent.py --initialize-control GPU_FREQUENCY_CONTROL=${gpu_freq}e6 --initialize-control CPU_FREQUENCY_CONTROL=${max_cpu_freq} \
            --report="${out_dir}/agent_${agent}.${APPNAME}.gpufreq_${gpu_freq}.trial_${trial}.report" \
            --trace="${out_dir}/agent_${agent}.${APPNAME}.gpufreq_${gpu_freq}.trial_${trial}.trace-${host}" \
            --signal-list="GPU_FREQUENCY_STATUS@board_accelerator,GPU_ENERGY@board_accelerator,GPU_POWER@board_accelerator,GPU_UTILIZATION@board_accelerator,GPU_COMPUTE_ACTIVITY@board_accelerator,DCGM::DRAM_ACTIVE@board_accelerator" \
            --control-period=0.05 \
            -- \
            ${EXEC}
        set +x
        sleep 10
    done
done
```

Try generating sweeps over multiple applications, or over multiple
configurations of one application (such as different inputs to a benchmark).

## 2) Process the frequency sweeps
You now have access to one or more directories that contain GEOPM traces and
reports from your frequency sweeps.  Feed those directories to the
process\_gpu\_frequency\_sweep.py file to obtain a single file with all
results. Example:

```bash
sweep_dir=${HOME}/my_sweep_1234
./process_gpu_frequency_sweep.py $(hostname) mysweepdata ${sweep_dir}
```

## 3) Train a model
Now you have a .h5 file that contains all of your frequency sweep data, along
with some additional derived data. Use that file as an input to the
train\_gpu\_model.py script to create your model. This may take a long time,
depending on how much trace data is available for training. The script will
display time estimates for each of its iterations over the training data.

```bash
./train_gpu_model.py ./mysweepdata.h5 mygpumodel
```
