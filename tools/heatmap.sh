#!/bin/bash
#
SVG_WIDTH=2048
SVG_FILE=heatmap.svg
PERF_FREQ=10000
HEATMAP=HeatMap
FS_TEST=../fs-test
TMP=/tmp
LATENCY_US=$TMP/out-lat.$$

if [ ! -d ${HEATMAP} ]
then
	git clone https://github.com/brendangregg/HeatMap.git
fi

if [ $UID != 0 ]
then
	echo "Need to run as root to drive perf"
	exit 1
fi
#
# From http://www.brendangregg.com/perf.html
#
perf record -e block:block_rq_issue -e block:block_rq_complete ${FS_TEST} $*
perf script | awk '{ gsub(/:/, "") } $5 ~ /issue/ { ts[$6, $10] = $4 }
    $5 ~ /complete/ { if (l = ts[$6, $9]) { printf "%.f %.f\n", $4 * 1000000,
    ($4 - l) * 1000000; ts[$6, $10] = 0 } }' > ${LATENCY_US}

./${HEATMAP}/trace2heatmap.pl --boxsize=16 --unitstime=us --unitslat=us --maxlat=100000 ${LATENCY_US} > ${SVG_FILE}
echo "HeatMap in ${SVG_FILE}"
rm ${LATENCY_US} perf.data
