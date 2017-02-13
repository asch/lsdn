NSPREFIX="ns-"

# Create a network namespace (simulating VM) named ${NSPREFIX}$1,
# with the host interface of the same name and guest iterface 'veth0'
function mkvm(){
    ip netns add "${NSPREFIX}$1"
    ip link add "${NSPREFIX}$1" type veth peer name "${NSPREFIX}peer-$1"
    ip link set "${NSPREFIX}$1" up
    ip link set "${NSPREFIX}peer-$1" netns "${NSPREFIX}$1"
    ip netns exec "${NSPREFIX}$1" ip link set "${NSPREFIX}peer-$1" name veth0
    ip netns exec "${NSPREFIX}$1" ip link set veth0 address "00:00:00:00:00:$3"
    ip netns exec "${NSPREFIX}$1" ip link set veth0 up
    ip netns exec "${NSPREFIX}$1" ip addr add "192.168.59.$3" dev veth0
    ip link set "${NSPREFIX}$1" master "bridge$2"
}

function default_route(){
    ip netns exec "${NSPREFIX}$1" ip route add default via 192.168.59."$2"
}

mkvm 1 a 01
mkvm 2 a 02
mkvm 3 b 01
mkvm 4 b 02

default_route 1 01
default_route 2 02
default_route 3 01
default_route 4 02
