#!/bin/bash

MEM_TOTAL_KB=$(grep MemTotal /proc/meminfo | awk {'print $2}')
MEM_TOTAL_DOUBLE_KB=$((MEM_TOTAL_KB * 2))
DATE_NOW=$(date +%F)
TIME_NOW=$(date +%H%M)
KERNEL=$(uname -r)
ID=$(cat /dev/urandom | tr -dc 'a-f0-9' | fold -w 12 | head -n 1)
FIO=fio

TMP=/tmp

PERF_FREQ=1000
RESULTS=results
ROOT_PATH=$(pwd)

FLAMEGRAPH=${ROOT_PATH}/FlameGraph
HEATMAP=${ROOT_PATH}/HeatMap

#
# No directory? Abort!
#
if [ -z $DIRECTORY ]; then
	echo "Must set DIRECTORY to the target test directory"
	exit 1
fi
#
# Setting pertaining to the directory we are running the test on
#
FILE_SYSTEM=$(df -T "${DIRECTORY}" | awk '{print $2}' | tail -n1)
DEVICE=$(df -T "${DIRECTORY}" | awk '{print $1}' | tail -n1)
BASEDEV=$(basename $DEVICE | sed 's/[0-9]*//g')
IOSCHED=$(cat /sys/block/$BASEDEV/queue/scheduler | tr ' ' '\n'| grep "\[" | sed 's/\[//' | sed 's/\]//')

MOUNT_OPTS=$(mount -l | grep $DEVICE |awk '{ print substr($0, index($0,$6)) }')

#
# No start date or time, then use date and time now
#
DATE_START=${DATE_START:-${DATE_NOW}}
TIME_START=${TIME_START:-${TIME_NOW}}
#
# No filesize set, then ensure we are outside the working set
#
SIZE=${SIZE:-${MEM_TOTAL_DOUBLE_KB}K}
#
# No blocksize, default to 4K
#
BLOCKSIZE=${BLOCKSIZE:-4K}

#
# Dump info about the system we are running the test on
#
sys_info() {
	echo "Full Date:     " $(date)
	echo "Date:          " ${DATE_START}
	echo "Time:          " ${TIME_START}
	echo "Date-run:      " ${DATE_NOW}
	echo "Time-run:      " ${TIME_NOW}
	echo "Job:           " ${JOB}
	echo "Scenario:      " ${SCENARIO}
	echo "Nodename:      " $(uname -n)
	echo "Kernel-release:" $(uname -r)
	echo "Kernel-version:" $(uname -v)
	echo "Architecture:  " $(uname -m)
	echo "Processor:     " $(uname -p)
	echo "Hardware:      " $(uname -i)
	lsb_release -a >& /dev/null
}

#
# Dump info about the file system we are testing
#
fs_info() {
	echo "Filesize:       $SIZE"
	echo "Blocksize:      $BLOCKSIZE"
	echo "Directory:      $DIRECTORY"
	echo "Filesystem:     $FILE_SYSTEM"
	echo "Device:         $DEVICE"
	echo "Mount Opts:     $MOUNT_OPTS"
	echo "IO Scheduler:   $IOSCHED"
}

#
#  Dump info about the job we're running
#
log_job_info()
{
	JOB_INFO=${RESULTS_PATH}/sysinfo.log
	sys_info > ${JOB_INFO}
	fs_info >> ${JOB_INFO}

	sys_info
	fs_info

	gzip --best ${JOB_INFO}
}

stats()
{
	if [ ${SCENARIO} == "perf" ]; then
		perf record -F ${PERF_FREQ} -g -- ${ROOT_PATH}/perf-wrapper.sh -e BLOCKSIZE=${BLOCKSIZE} -e SIZE=${SIZE} -e DIRECTORY=${DIRECTORY} $FIO $* ${ROOT_PATH}/jobs/${JOB} --output-format=json --output=${RESULTS_PATH}/fio-stats.json
		perf report -i ${RESULTS_PATH}/perf.data > ${RESULTS_PATH}/perf.report
		perf script -i ${RESULTS_PATH}/perf.data > ${RESULTS_PATH}/perf.script
		gzip --best ${RESULTS_PATH}/perf.data ${RESULTS_PATH}/perf.report ${RESULTS_PATH}/perf.script
	else
		$FIO $* ${ROOT_PATH}/jobs/${JOB} --output-format=json --output=${RESULTS_PATH}/fio-stats.json
	fi
	gzip --best ${RESULTS_PATH}/*.log
}


while [ ! -z $1 ]
do
	case $1 in
	-F)
		FIO=$OPTARG
		;;
	-p)
		echo "Perf"
		SCENARIO=perf
		if [ $UID != 0 ]; then
			echo "Need to run as root to drive perf"
			exit 1
		fi
		;;
	-s)
		echo "Stats"
		SCENARIO=stats
		;;
	-j)
		shift
		JOB=$1
		;;
	*)
		newopts="$newopts $1"
	;;
	esac
	shift
done

newopts="$newopts"
if [ -z $JOB ]; then
	echo "No job specified"
	exit 1
fi

#
# Need to pass these down to fio
#
export BLOCKSIZE=${BLOCKSIZE}
export SIZE=${SIZE}
export DIRECTORY=${DIRECTORY}

RESULTS_PATH=${ROOT_PATH}/${RESULTS}/${DATE_NOW}/job-${JOB}-kv-${KERNEL}-size-${SIZE}-bs-${BLOCKSIZE}-fs-${FILE_SYSTEM}-sched-${IOSCHED}-${ID}/${SCENARIO}
mkdir -p ${RESULTS_PATH}
log_job_info
cd ${RESULTS_PATH}
stats $newopts
cd ${ROOT_PATH}
