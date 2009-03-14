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

# Run `bash resume_test.sh vmx' to test with VMware Workstation using the VIX
# API. Default behavior is to run with Qemu.
if [[ $1 == "vmw" ]]; then
	VMW=true
fi

MNT_PNT=/media/vmx

if [[ -n $VMW ]]; then
	VMX=/vms/TestsHDD/TestsHDD.vmx
	VMDK=/vms/TestsHDD/TestsHDD.vmdk
	trap "sudo vmware-mount -x;vmrun stop $VMX" SIGINT SIGTERM
else
	RAW=resume_testing/resume_testing.img
fi

# Check if the build is valid
make clean && make resume.c32
if [[ $? != 0 ]]; then
	exit 1
fi

if [[ -n $VMW ]]; then
	# Update the .vmdk file
	sudo vmware-mount "$VMDK" "$MNT_PNT"
	sudo cp resume.c32 "$MNT_PNT"
	sudo vmware-mount -x
else
	sudo losetup /dev/loop0 "$RAW"
	sudo mount /dev/loop0 "$MNT_PNT"
	sudo cp resume.c32 "$MNT_PNT"
	sudo umount "$MNT_PNT"
	sudo losetup -d /dev/loop0
fi

# Run a VM
if [[ -n $VMW ]]; then
	rm -f /tmp/TestsHDD3
	vmrun start /vms/TestsHDD/TestsHDD.vmx nogui
	sleep 2
	socat unix-connect:/tmp/TestsHDD3 stdout
	#socat unix-connect:/tmp/TestsHDD3 tcp4-listen:12345 
	vmrun list
else
	#qemu -nographic -M pc -hda "$RAW" -m 1500 -no-kqemu -boot c #-S -s
	qemu -M pc -hda "$RAW" -m 1500 -no-kqemu -boot c #-S -s
fi
