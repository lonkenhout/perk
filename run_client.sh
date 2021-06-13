#!/bin/bash
set -o errexit -o pipefail -o noclobber -o nounset

cmd=$0
! getopt --test > /dev/null
if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
    echo 'erro: getopt failed'
    exit 1
fi

OPTIONS="a:p:ui:c:r:n:bh"
LONGOPTS="addr:,port:,use-id,infile:,count:,rdma-comp:,node:,benchmark,help"

! PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    exit 2
fi
eval set -- "$PARSED"

ip=127.0.0.1
use_id=""
port=20838
count=5000000
infile="none"
exe=./bin/pears_client
comp="wr_wr"
h=0
possible_comps=["wr_sd","wrimm_sd","sd_sd","mcd","wr_rd","wr_wr"]

addr_set=0
while true && [ $# -gt 1 ]; do
	#echo $1
	case "$1" in
		-a|--addr)
			if [ $addr_set == 1]; then
				echo "address already set to $ip, redundant argument:"
				echo "$1 $2"
			else
				ip=$2
				addr_set=1
			fi
			shift 2 ;;
		-p|--port)
			port=$2
			shift 2 ;;
		-u|--use-id)
			use_id=-u
			shift ;;
		-i|--infile)
			infile=$2
			shift 2 ;;
		-c|--count)
			count=$2
			echo "$count"
			shift 2 ;;
		-r|--rdma-comp)
			if [[ ! "${possible_comps[@]}" =~ "$2" ]]; then
				echo "error: invalid RDMA composition: $2"
				exit 1
			fi
			if [ "$2" == "mcd" ]; then
				exe=./bin/client_mcd
				port=11211
			fi
			comp=$2
			shift 2 ;;
		-n|--node)
			if [ $addr_set == 1 ]; then
				echo "address already set to $ip, redundant argument:"
				echo "$1 $2"
			else
				ip=10.149.0.$2
				addr_set=1
			fi
			shift 2 ;;
		-b|--benchmark)
			exe=./bin/pears_client_bm
			shift ;;
		-h|--help)
			h=1
			shift
			break ;;
		--)
			shift ;;
		*)
			echo "error parsing arguments"
			exit 1
			;;
	esac
done

if [ $h == 1 ]
then
	echo "Usage: $cmd [OPTIONS]..

Options:
  -a, --addr=<IP>         server IP address (default: 0.0.0.0)
  -p, --port=<PORT>       server port (default: 20838)
  -i, --infile=<IN>       input file containing key-value pairs
  -c, --count=<MAX>       maximum number of operations to perform (default: 5000000)
  -r, --rdma-comp=<COMP>  rdma composition, can be one of: (default: wr_wr)
                          wr_sd    - send request using WRITE, send response using SEND
                          sd_sd    - send request using SEND, send response using SEND
                          wimm_sd  - send request using WRITE with immediate data, send response using SEND
                          wr_wr    - send request using WRITE, send back response using WRITE
                          wr_rd    - send request using WRITE, READ response from server
  -n, --node=<NODENUM>    DAS5 node where target is expected, will replace default IP with 10.149.0.NODENUM
  -h, --help              display help
"
exit 0
fi

if [ "$comp" != "mcd" ]; then
	$exe -r $comp -a $ip -p $port $use_id -i $infile -c $count
else
	$exe -a $ip -p $port $use_id -i $infile -c $count
fi
