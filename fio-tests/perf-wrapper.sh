#!/bin/bash
while getopts "e:" OPTION
do
	case $OPTION in
        e)
		export $OPTARG
		;;
	esac
done

shift $((OPTIND-1))

#echo "BLOCKSIZE: "${BLOCKSIZE}
#echo "SIZE: "${SIZE}
#echo "DIR: "${DIRECTORY}
#echo "RUN: $*"
$*
