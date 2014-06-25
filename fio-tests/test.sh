#!/bin/bash 

DEV=/dev/sda1
MNT=/mnt
RUNTIME=600

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
			echo $job | grep rand > /dev/null
			if [ $? -eq 0 ]; then
				sz=128M
			else
				sz=8G
			fi
			echo "Job: $job, Size $sz"
			RUNTIME=${RUNTIME} SIZE=$sz DIRECTORY=$MNT ./fio.sh -$opt -j $job
			umount $MNT
		done
	done
done
