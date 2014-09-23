#!/bin/bash

MEM_TOTAL_K=$(grep MemTotal /proc/meminfo  | awk '{print $2}')
MIN_FILE_SIZE=$((MEM_TOTAL_K * 2))K
#DEV=/dev/sda1
MNT=/mnt
RUNTIME=900
HERE=$(pwd)
FIO=${HERE}/fio/fio
DATE_START=$(date +%F)
TIME_START=$(date +%H%M)
LOG_AVG_MSEC=500
#
# P or S modes
#
MODE=S

mk_fio()
{
	if [ ! -x $FIO ]; then
		gunzip < ../tools/fio-2.1.9.tar.gz | tar xvf -
		cd fio
		make clean
		make -j 4
		rc=$?
		cd ..
		if [ $rc -ne 0 ]; then
			echo "Cannot build fio"
			exit 1
		fi
	fi

	echo "fio: $FIO"
}

do_fs_new()
{
	#
	#  Zap the partition
	#
	dd if=/dev/zero of=$DEV bs=1M count=64 > /dev/null 2>&1

	case $1 in
	ext2)
		mkfs.ext2 -F $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext2 $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	ext3)
		mkfs.ext3 -F $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext3 $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
			
	ext4)
		mkfs.ext4 -F $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext4 $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	xfs)
		mkfs.xfs -f $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -f option
			#
			mkfs.xfs $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT > /dev/null 2>&1
		;;
	btrfs)
		mkfs.btrfs -f $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -f option
			#
			mkfs.btrfs $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	*)
		mkfs.ext4 -F $DEV > /dev/null 2>&1
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext4 $DEV > /dev/null 2>&1
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	esac

	return 0
}

if [ $UID -ne 0 ]; then
	echo "Need to be root to run test"
	exit 1
fi

while getopts "D:T:d:m:s:f:PSr:" opt; do
	case $opt in
	D)
		DATE_START=$OPTARG
		;;
	T)	
		TIME_START=$OPTARG
		;;
	f)	
		FS=$OPTARG
		;;
	d)
		DEV=$OPTARG
		;;
	m)
		MIN_FILE_SIZE=$OPTARG
		;;
	s)
		IOSCHED=$OPTARG
		case $IOSCHED in
		noop | cfq | deadline)
			;;
		*)
			echo "IO schediler must be one of noop, cfq or deadline"
			exit 1
			;;
		esac
		;;
	r)
		RUNTIME=$OPTARG
		;;
	P)
		echo "Using perf mode"
		MODE=-P
		;;
	S)
		echo "Using stats mode"
		MODE=-S
		;;
	esac
done

if [ -z $FS ]; then
	echo "FS not defined. Exiting"
	exit 1
fi

FS=$(echo $FS | tr ',' ' ')

if [ -z $IOSCHED ]; then
	echo "IOSCHED is not defined. Exiting"
	exit 1
fi

if [ -z $DEV ]; then
	echo "DEV is not defined. Exiting"
	exit 1
fi

if [ ! -b $DEV ]; then
	echo "Device $DEV does not exist. Exiting"
	exit 1
fi

mounts=$(mount | grep $DEV | wc -l)
if [ $mounts -ne 0 ]; then
	echo "Mounts on $DEV:"
	mount | grep $DEV
	echo "Device $DEV has file systems mounted on it. Exiting"
	exit 1
fi
BASEDEV=$(basename $DEV | sed 's/[0-9]*//g')

mk_fio

ALL_JOBS=$(ls jobs | grep -v conf | grep -v info)

for job in ${ALL_JOBS}
do
	for fs in $FS
	do
		echo $fs $MODE
		do_fs_new $fs
		if [ $? -eq 0 ]; then
			echo "Job: $job, Size $sz, IOsched $IOSCHED"
			echo ${IOSCHED} > /sys/block/$BASEDEV/queue/scheduler
			LOG_AVG_MSEC=${LOG_AVG_MSEC} DATE_START=${DATE_START} TIME_START=${TIME_START} RUNTIME=${RUNTIME} SIZE=${MIN_FILE_SIZE} DIRECTORY=$MNT ./fio.sh $MODE -j $job -F ${FIO}
			umount $MNT
		fi
	done
done
