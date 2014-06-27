#!/bin/bash 

MEM_TOTAL_K=$(grep MemTotal /proc/meminfo  | awk '{print $2}')
MIN_FILE_SIZE=$((MEM_TOTAL_K * 2))
#DEV=/dev/sda1
MNT=/mnt
RUNTIME=900
HERE=$(pwd)
FIO=${HERE}/fio/fio

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

mk_fio
exit

while getopts "d:m:" opt; do
	case $opt in
	d)
		DEV=$OPTARG
		;;
	m)
		MIN_FILE_SIZE=$OPTARG
		;;
	esac
done

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

ALL_JOBS=$(ls jobs | grep -v conf)

for job in ${ALL_JOBS}
do
	for fs in ext4 xfs btrfs
	do
		echo $fs
		for opt in s #f h s
		do
			umount $MNT
			do_fs_new $fs
			if [ $? -eq 0 ]; then
				echo "Job: $job, Size $sz"
				RUNTIME=${RUNTIME} SIZE=${MIN_FILE_SIZE}K DIRECTORY=$MNT ./fio.sh -$opt -j $job -F ${FIO}
				umount $MNT
			fi
		done
	done
done
