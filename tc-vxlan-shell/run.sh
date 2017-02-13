#!/bin/bash

dummy="dummy_intf"

echo "cleanup.sh ..."
bash cleanup.sh
echo $?
echo

echo "vxlan_with_linux_bridge.sh ..."
bash vxlan_with_linux_bridge.sh "${dummy}"
echo $?
echo

echo "create-vm.sh ..."
bash create-vm.sh
echo $?
echo

echo "create-vxlan-tunnel.sh ..."
bash create-vxlan-tunnel.sh "${dummy}"
echo $?
