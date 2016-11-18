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

mkvm a 01
mkvm b 02
mkvm c 03
mkvm d 04
mkvm e 05
mkvm f 06

