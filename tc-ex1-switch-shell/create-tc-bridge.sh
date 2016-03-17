function redir(){
tc qdisc add dev "$1" handle ffff: ingress || true
tc filter add dev "$1" parent ffff: protocol all u32 match u32 0 0 action mirred egress redirect dev "$2"
}

function forward(){
tc filter add dev "$1" parent 1: protocol all u32 match \
   u16 "0x00$2" 0x00FF at -10 action mirred egress redirect dev "$3"
}

#set -x

ip link add name sp-ex1-switch1 type dummy
ip link add name sp-ex1-switch2 type dummy
ip link add name sp-ex1-switch3 type dummy

redir sp-ex1-a sp-ex1-switch1
redir sp-ex1-b sp-ex1-switch1
redir sp-ex1-c sp-ex1-switch2
redir sp-ex1-d sp-ex1-switch2
redir sp-ex1-e sp-ex1-switch3
redir sp-ex1-f sp-ex1-switch3

tc qdisc add dev sp-ex1-switch1 root handle 1: htb
tc qdisc add dev sp-ex1-switch2 root handle 1: htb
tc qdisc add dev sp-ex1-switch3 root handle 1: htb
ip link set sp-ex1-switch1 up
ip link set sp-ex1-switch2 up
ip link set sp-ex1-switch3 up

forward sp-ex1-switch1 01 sp-ex1-a
forward sp-ex1-switch1 02 sp-ex1-b
forward sp-ex1-switch1 03 sp-ex1-switch2
forward sp-ex1-switch1 04 sp-ex1-switch2
forward sp-ex1-switch1 05 sp-ex1-switch3
forward sp-ex1-switch1 06 sp-ex1-switch3

forward sp-ex1-switch2 01 sp-ex1-switch1
forward sp-ex1-switch2 02 sp-ex1-switch1
forward sp-ex1-switch2 03 sp-ex1-c
forward sp-ex1-switch2 04 sp-ex1-d
forward sp-ex1-switch2 05 sp-ex1-switch1
forward sp-ex1-switch2 06 sp-ex1-switch1

forward sp-ex1-switch3 01 sp-ex1-switch1
forward sp-ex1-switch3 02 sp-ex1-switch1
forward sp-ex1-switch3 03 sp-ex1-switch1
forward sp-ex1-switch3 04 sp-ex1-switch1
forward sp-ex1-switch3 05 sp-ex1-e
forward sp-ex1-switch3 06 sp-ex1-f


