#!/bin/bash
# -----------------------------------------------------------------------
#
#   Copyright (C) 2008, VMware, Inc.
#   Author : Pierre-Alexandre Meyer <pierre@vmware.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
#   Boston MA 02110-1301, USA; either version 2 of the License, or
#   (at your option) any later version; incorporated herein by reference.
#
# -----------------------------------------------------------------------

set -e

# Set VMW to test with VMware Workstation using the VIX
# API. Default behavior is to run with Qemu.
#VMW=true

# Global parameters
VM_ROOT=resume_testing/linux_vm
MNT_PNT=resume_testing/mnt
SERIAL=$VM_ROOT/serial.pipe

# Testing parameters
KERNEL="$VM_ROOT"/bzImage
RAM_SIZE=1024
TOI_FILE_OFFSET=0x8960

if [[ -n $VMW ]]; then
	VMX=/vms/TestsHDD/TestsHDD.vmx
	VMDK=/vms/TestsHDD/TestsHDD.vmdk
	trap "sudo vmware-mount -x;vmrun stop $VMX" SIGINT SIGTERM
else
	RAW=$VM_ROOT/debian_lenny_i386_small.img
	OFFSET=$((63*512))
	QEMU="qemu -smp 1 -no-kqemu -m $RAM_SIZE -hda $RAW -serial file:$SERIAL"
	trap "exit 1" SIGINT SIGTERM
fi

function test_resume_c32
{
	make resume.c32
	update_resume_c32

	if [[ -n $VMW ]]; then
		rm -f /tmp/TestsHDD3
		echo "  VMW     $RAW"
		vmrun start "$VMX" nogui
		sleep 2
		socat unix-connect:/tmp/TestsHDD3 stdout
		#socat unix-connect:/tmp/TestsHDD3 tcp4-listen:12345
		vmrun list
	else
		echo "  QEMU    $RAW"
		$QEMU
	fi
}

function create_hibernate_image
{
	echo "  QEMU    $RAW"
	$QEMU	-append "noresume resume=file:/dev/sda1:$TOI_FILE_OFFSET console=tty0 console=ttyS0 root=/dev/sda1" \
		-kernel "$KERNEL"

	# Create the hibernate image by... hibernating the VM!
	# e.g. echo /image > /sys/power/tuxonice/file/target
	#      echo 1 > /sys/power/tuxonice/do_hibernate

	update_system_map
}

function mount_vm
{
if [[ -n $VMW ]]; then
	echo "  MOUNT   $MNT_PNT"
	sudo vmware-mount "$VMDK" "$MNT_PNT"
else
	echo "  LOSETUP /dev/loop0"
	sudo losetup -o "$OFFSET" /dev/loop0 "$RAW"
	echo "  MOUNT   $MNT_PNT"
	sudo mount /dev/loop0 "$MNT_PNT"
fi
}

function umount_vm
{
if [[ -n $VMW ]]; then
	echo "  UMOUNT  $MNT_PNT"
	sudo vmware-mount -x
else
	echo "  UMOUNT  $MNT_PNT"
	sudo umount "$MNT_PNT"
	echo "  LOSETUP /dev/loop0"
	sudo losetup -d /dev/loop0
fi
}

function update_resume_c32
{
	mount_vm
	echo "  CP      $MNT_PNT/resume.c32"
	sudo cp resume.c32 "$MNT_PNT"
	umount_vm
}

function update_system_map
{
	mount_vm
	echo "  CP      $MNT_PNT/System.map"
	sudo cp "$VM_ROOT"/System.map "$MNT_PNT"
	umount_vm
}

function update_extlinux
{
	mount_vm
	echo "  EXTLINUX $MNT_PNT"
	sudo ../../extlinux/extlinux -U "$MNT_PNT"
	umount_vm
}

if [[ $1 == 'create' ]]; then
	create_hibernate_image
elif [[ $1 == 'extlinux' ]]; then
	update_extlinux
else
	test_resume_c32
fi
