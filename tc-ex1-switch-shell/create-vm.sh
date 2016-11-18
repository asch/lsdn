NSPREFIX='sp-ex1-'

# Create a network namespace (simulating VM) named ${NSPREFIX}$1, 
# with the host interface o the same name and guest iterface 'veth0'
function mkvm(){
   ip netns add "${NSPREFIX}$1"
   ip link add "${NSPREFIX}$1" type veth peer name "${NSPREFIX}peer-$1"
   ip link set "${NSPREFIX}$1" up
   ip link set "${NSPREFIX}peer-$1" netns "${NSPREFIX}$1"
   ip netns exec "${NSPREFIX}$1" ip link set "${NSPREFIX}peer-$1" name veth0
   ip netns exec "${NSPREFIX}$1" ip link set veth0 address "00:00:00:00:00:$2"
   ip netns exec "${NSPREFIX}$1" ip link set veth0 up
   ip netns exec "${NSPREFIX}$1" ip addr add "192.168.59.$2" dev veth0
}

function default_route(){
ip netns exec "${NSPREFIX}$1" ip route add default via 192.168.59."$2"
}

function arp(){
ip netns exec "${NSPREFIX}$1" ip neigh add 192.168.59."$2" lladdr "00:00:00:00:00:$3" dev veth0
}

mkvm a 01
mkvm b 02
mkvm c 03
mkvm d 04
mkvm e 05
mkvm f 06

default_route a 1
default_route b 2
default_route c 3
default_route d 4
default_route e 5
default_route f 6

arp a 2 02
arp a 3 03
arp a 4 04
arp a 5 05
arp a 6 06

arp b 1 01
arp b 3 03
arp b 4 04
arp b 5 05
arp b 6 06

arp c 1 01
arp c 2 02
arp c 4 04
arp c 5 05
arp c 6 06

arp d 1 01
arp d 2 02
arp d 3 03
arp d 5 05
arp d 6 06

arp e 1 01
arp e 2 02
arp e 3 03
arp e 4 04
arp e 6 06

arp f 1 01
arp f 2 02
arp f 3 03
arp f 4 04
arp f 5 05
