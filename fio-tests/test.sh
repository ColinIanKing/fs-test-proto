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
	dd if=/dev/zero of=$DEV bs=1M count=64

	case $1 in
	ext2)
		mkfs.ext2 -F $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext2 $DEV
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	ext3)
		mkfs.ext3 -F $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext3 $DEV
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
			
	ext4)
		mkfs.ext4 -F $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext4 $DEV
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	xfs)
		mkfs.xfs -f $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -f option
			#
			mkfs.xfs $DEV
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	btrfs)
		mkfs.btrfs -f $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -f option
			#
			mkfs.btrfs $DEV
			if [ $? -ne 0 ]; then
				return 1
			fi
		fi
		mount $DEV $MNT
		;;
	*)
		mkfs.ext4 -F $DEV
		if [ $? -ne 0 ]; then
			#
			#  Older versions don't have -F option
			#
			mkfs.ext4 $DEV
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

while getopts "D:T:d:m:s:" opt; do
	case $opt in
	D)
		DATE_START=$OPTARG
		;;
	T)	TIME_START=$OPTARG
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
	esac
done

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

ALL_JOBS=$(ls jobs | grep -v conf)

for job in ${ALL_JOBS}
do
	for fs in ext4 xfs btrfs
	do
		echo $fs
		for opt in s #p
		do
			umount $MNT
			do_fs_new $fs
			if [ $? -eq 0 ]; then
				echo "Job: $job, Size $sz, IOsched $IOSCHED"
				echo ${IOSCHED} > /sys/block/$BASEDEV/queue/scheduler
				DATE_START=${DATE_START} TIME_START=${TIME_START} RUNTIME=${RUNTIME} SIZE=${MIN_FILE_SIZE} DIRECTORY=$MNT ./fio.sh -$opt -j $job -F ${FIO}
				umount $MNT
			fi
		done
	done
done
