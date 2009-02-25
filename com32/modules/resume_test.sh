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

if [[ -n $VMW ]]; then
	VMX=/vms/TestsHDD/TestsHDD.vmx
	VMDK=/vms/TestsHDD/TestsHDD.vmdk
	trap "sudo vmware-mount -x;vmrun stop $VMX" SIGINT SIGTERM
else
	VMDK=/vms/TestsHDDQemu/TestsHDDQemu.vmdk
fi

# Check if the build is valid
make clean && make resume.c32
if [[ $? != 0 ]]; then
	exit 1
fi

# Update the .vmdk file
sudo vmware-mount "$VMDK" /media/vmx
sudo cp resume.c32 /media/vmx
sudo vmware-mount -x

# Run a VM
if [[ -n $VMW ]]; then
	rm -f /tmp/TestsHDD3
	vmrun start /vms/TestsHDD/TestsHDD.vmx nogui
	sleep 2
	socat unix-connect:/tmp/TestsHDD3 stdout
	#socat unix-connect:/tmp/TestsHDD3 tcp4-listen:12345 
	vmrun list
else
	qemu -nographic -M pc -hda "$VMDK" -m 1000 -no-kqemu -boot c #-S -s
	#qemu -M pc -hda "$VMDK" -m 1000 -no-kqemu -boot c #-S -s
fi
