#!/bin/bash

./create_vm.sh m1 10.1.1.1/24
./create_vm.sh m2 10.1.1.2/24

./create_vm.sh vm1 192.168.1.1/24
./create_vm.sh vm2 192.168.1.2/24
./create_vm.sh vm3 192.168.1.3/24
./create_vm.sh vm4 192.168.1.4/24

./insert_vm_with_vxlan.sh vm1 m1 10 172.16.50.1 10.1.1.2
./insert_vm_with_vxlan.sh vm2 m2 10 172.16.50.2 10.1.1.1
./insert_vm_with_vxlan.sh vm3 m1 20 172.16.50.3 10.1.1.2
./insert_vm_with_vxlan.sh vm4 m2 20 172.16.50.4 10.1.1.1

ip link add br0 type bridge
ip link set m1 master br0
ip link set m2 master br0
ip link set br0 up
