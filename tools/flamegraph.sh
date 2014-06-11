#!/bin/bash
#
SVG_WIDTH=2048
SVG_FILE=perf-kernel.svg
PERF_FREQ=10000
FLAMEGRAPH=FlameGraph
FS_TEST=../fs-test
TMP=/tmp
FOLDED=${TMP}/out.perf-folded.$$

if [ ! -d ${FLAMEGRAPH} ]
then
	git clone https://github.com/brendangregg/FlameGraph.git
fi

if [ $UID != 0 ]
then
	echo "Need to run as root to drive perf"
	exit 1
fi
perf record -F ${PERF_FREQ} -g -- ${FS_TEST} $* 
perf script | ./${FLAMEGRAPH}/stackcollapse-perf.pl > ${FOLDED}
./${FLAMEGRAPH}/flamegraph.pl --width=${SVG_WIDTH} < ${FOLDED} > ${SVG_FILE}
rm ${FOLDED} perf.data
echo "FlameGraph in ${SVG_FILE}"
