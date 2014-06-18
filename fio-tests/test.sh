#!/bin/bash 

DEV=/dev/sda1
MNT=/mnt

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


for jobs in write-sync-seq-1 write-sync-seq-2 write-sync-seq-4
do
	for fs in ext4 xfs btrfs
	do
		echo $fs
		for opt in s #f h s
		do
			umount $MNT
			do_fs_new $fs
			SIZE=2G DIRECTORY=$MNT ./fio.sh -$opt -j $jobs
			umount $MNT
		done
	done
done
