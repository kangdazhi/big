#!/bin/bash

if [ $# -lt 2 ]; then
        echo "Usage: $0 <enable/disable> <ncpus> <kmult> <kshift>"
        exit 1
fi
op=$1
ncpus=$2
kmult=$3
kshift=$4

modprobe msr

enable()
{
        local cpus=$(($1 - 1))
        local kmult=$2
        local kshift=$3

        #for i in `seq 0 $cpus`; do taskset -c $i ./run_did hc_set_x2apic_id; done 
        for i in `seq 0 $cpus`; do taskset -c $i ./run_did hc_setup_dtid $kmult $kshift; done
        ./run_did set_apic_ipi
        ./run_did hc_disable_intercept_wrmsr_icr
}

disable()
{
        local cpus=$(($1 - 1))

        ./run_did hc_enable_intercept_wrmsr_icr
        ./run_did restore_apic_ipi
        for i in `seq 0 $cpus`; do taskset -c $i ./run_did hc_restore_dtid; done
        for i in `seq 0 $cpus`; do wrmsr -p $i 0x838 0x616d; done
        #for i in `seq 0 $cpus`; do taskset -c $i ./run_did hc_restore_x2apic_id; done 
}

if [ $op = "enable" ]; then
        enable $ncpus $kmult $kshift
elif [ $op = "disable" ]; then
        disable $ncpus
else
        echo "no such operation"
fi
