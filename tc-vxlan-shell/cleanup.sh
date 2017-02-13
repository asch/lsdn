NSPREFIX="ns-"

for dev in /sys/class/net/vxlan*; do
   dev=$(basename "${dev}")
   echo Deleting netdev "${dev}"
   ip link delete "${dev}"
done

for dev in /sys/class/net/bridge*; do
   dev=$(basename "${dev}")
   echo Deleting netdev "${dev}"
   ip link delete "${dev}"
done

for dev in /sys/class/net/dummy_*; do
   dev=$(basename "${dev}")
   echo Deleting netdev "${dev}"
   ip link delete "${dev}"
done

for ns in /var/run/netns/"${NSPREFIX}"*; do
   ns=$(basename "${ns}")
   echo Deleting namespace "${ns}"
   ip netns delete "${ns}"
done
