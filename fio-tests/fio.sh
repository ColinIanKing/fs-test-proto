#!/bin/bash

SVG_FLAMEGRAPH_FILE=flamegraph.svg
SVG_HEATMAP_FILE=heatmap.svg
SVG_WIDTH=2048
MEM_TOTAL_KB=$(grep MemTotal /proc/meminfo | awk {'print $2}')
MEM_TOTAL_DOUBLE_KB=$((MEM_TOTAL_KB * 2))
DATE_NOW=$(date +%F)
TIME_NOW=$(date +%H%M)

TMP=/tmp

PERF_FREQ=10000
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
MOUNT_OPTS=$(mount -l | grep $DEVICE |awk '{ print substr($0, index($0,$6)) }')

#
# No filesize set, then ensure we are outside the working set
#
SIZE=${SIZE:-${MEM_TOTAL_DOUBLE_KB}K}
#
# No blocksize, default to 4K
#
BLOCKSIZE=${BLOCKSIZE:-4K}
#
# No iterations set, default to 5 so we can calc std.dev.
# on the fio-stats data on the -s stats test
#
LOOPS=${LOOPS:-5}

#
# Dump info about the system we are running the test on
#
sys_info() {
	echo "Full Date:     " $(date)
	echo "Date:          " ${DATE_NOW}
	echo "Time:          " ${TIME_NOW}
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
}

#
#  Dump info about the job we're running
#
log_job_info()
{
	JOB_INFO=${RESULTS_PATH}/sysinfo.log
	sys_info > ${JOB_INFO}
	fs_info >> ${JOB_INFO}
	echo "free:" >> ${JOB_INFO}
	free >> ${JOB_INFO}
	echo "df:" >> ${JOB_INFO}
	df >> ${JOB_INFO}
}

stats()
{
	#
	# Must remove old logs else we accumulate plot
	# data and ruin fio_generate_plots
	#
	rm -f ${RESULTS_PATH}/${JOB}_bw.log \
	      ${RESULTS_PATH}/${JOB}_clat.log \
	      ${RESULTS_PATH}/${JOB}_iops.log \
	      ${RESULTS_PATH}/${JOB}_lat.log \
	      ${RESULTS_PATH}/${JOB}_slat.log
	#
	# Run the test multiple times and accumulate fio-stats.log 
	#
	for I in $(seq ${LOOPS})
	do
		echo "fio iteration $I of ${LOOPS}"
		fio $* ${ROOT_PATH}/jobs/${JOB} --output=${RESULTS_PATH}/fio-stats-$I.log
	done
	#
	# TODO: calc std.dev. on specific fields on file-stats-*.log
	#
	echo "Generating plots.."
	fio_generate_plots ${JOB} >& /dev/null
}

heatmap()
{
	LATENCY_US=$TMP/out-lat.$$

	if [ $UID != 0 ]
	then
		echo "Need to run as root to drive perf"
		exit 1
	fi

	if [ ! -d ${HEATMAP} ]; then
		git clone https://github.com/brendangregg/HeatMap.git ${HEATMAP}
	fi

	#
	# From http://www.brendangregg.com/perf.html
	#
	perf record -e block:block_rq_issue -e block:block_rq_complete \
		fio $* ${ROOT_PATH}/jobs/${JOB}
	perf script | awk '{ gsub(/:/, "") } $5 ~ /issue/ { ts[$6, $10] = $4 }
    		$5 ~ /complete/ { if (l = ts[$6, $9]) { printf "%.f %.f\n", $4 * 1000000,
    		($4 - l) * 1000000; ts[$6, $10] = 0 } }' > ${LATENCY_US}

	${HEATMAP}/trace2heatmap.pl --boxsize=16 --unitstime=us --unitslat=us --maxlat=100000 ${LATENCY_US} > ${JOB}-${SVG_HEATMAP_FILE}
	rm ${LATENCY_US} perf.data
}

flamegraph()
{
	FOLDED=${TMP}/out.perf-folded.$$

	if [ $UID != 0 ]; then
        	echo "Need to run as root to drive perf"
        	exit 1
	fi

	perf record -F ${PERF_FREQ} -g -- fio $* ${ROOT_PATH}/jobs/${JOB}

	if [ ! -d ${ROOT_PATH}/${FLAMEGRAPH} ]; then
        	git clone https://github.com/brendangregg/FlameGraph.git ${FLAMEGRAPH}
	fi

	perf script | ${FLAMEGRAPH}/stackcollapse-perf.pl > ${FOLDED}
	${FLAMEGRAPH}/flamegraph.pl --minwidth=1 --width=${SVG_WIDTH} < ${FOLDED} > ${JOB}-${SVG_FLAMEGRAPH_FILE}
	rm ${FOLDED} perf.data
}

fg=0
hm=0
st=0

while [ ! -z $1 ]
do
	case $1 in
	-f)
		echo "FlameGraph"
		fg=1
		;;
	-h)
		echo "HeatMap"
		hm=1
		;;
	-s)
		echo "Stats"
		st=1
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

newopts="$newopts --minimal"

RESULTS_PATH=${ROOT_PATH}/${RESULTS}/${DATE_NOW}/${TIME_NOW}/${FILE_SYSTEM}/${JOB}/fs-${SIZE}-bs-${BLOCKSIZE}/

mkdir -p ${RESULTS_PATH}
cd ${RESULTS_PATH}

sys_info
fs_info

log_job_info

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

if [ $fg -eq 1 ]; then
	flamegraph $newopts
fi

if [ $hm -eq 1 ]; then
	heatmap $newopts
fi

if [ $st -eq 1 ]; then
	stats $newopts
fi

cd ${ROOT_PATH}
