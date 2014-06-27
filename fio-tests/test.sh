#!/bin/bash 

MEM_TOTAL_K=$(grep MemTotal /proc/meminfo  | awk '{print $2}')
MIN_FILE_SIZE=$((MEM_TOTAL_K * 2))
DEV=/dev/sda1
MNT=/mnt
RUNTIME=900

do_fs_new()
{
	case $1 in
	ext2)
		mkfs.ext2 -F $DEV
		mount $DEV $MNT
		;;
	ext3)
		mkfs.ext3 -F $DEV
		mount $DEV $MNT
		;;
			
	ext4)
		mkfs.ext4 -F $DEV
		mount $DEV $MNT
		;;
	xfs)
		mkfs.xfs -f $DEV
		mount $DEV $MNT
		;;
	btrfs)
		mkfs.btrfs -f $DEV
		mount $DEV $MNT
		;;
	*)
		mkfs.ext4 -F $DEV
		mount $DEV $MNT
		;;
	esac
}

ALL_JOBS=$( ls jobs | grep -v conf)

for job in ${ALL_JOBS}
do
	for fs in ext4 xfs btrfs
	do
		echo $fs
		for opt in s #f h s
		do
			umount $MNT
			do_fs_new $fs
			echo "Job: $job, Size $sz"
			RUNTIME=${RUNTIME} SIZE=${MIN_FILE_SIZE} DIRECTORY=$MNT ./fio.sh -$opt -j $job
			umount $MNT
		done
	done
done
