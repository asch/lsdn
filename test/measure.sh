#/bin/bash
# Usage: measure.sh to-ns from-ns to-ipaddr
# Example for the qos.sh test: ./measure.sh lsdn-a-1 lsdn-a-2 192.168.99.1
ns1="ip netns exec $1"
ns2="ip netns exec $2"

$ns1 netserver -D&
$ns1 tcpdump -i out-1 -w measure.pcap &
sleep 1
$ns2 netperf -H $3 -t UDP_STREAM -l 20
sleep 1

jobs -p | xargs kill
wait
