for dev in /sys/class/net/sp-*; do
   dev=$(basename "${dev}")
   echo Deleting netdev "${dev}"
   ip link delete "${dev}"
done

for ns in /var/run/netns/sp-*; do
   ns=$(basename "${ns}")
   echo Deleting namespace "${ns}"
   ip netns delete "${ns}"
done
