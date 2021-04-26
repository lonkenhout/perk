#!/bin/bash
set -o errexit -o pipefail -o noclobber -o nounset

cmd=$0
! getopt --test > /dev/null
if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
    echo 'erro: getopt failed'
    exit 1
fi

OPTIONS="a:p:i:c:r:n:h"
LONGOPTS="addr:,port:,infile:,count:,rdma-comp:,node:,help"

! PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    exit 2
fi
eval set -- "$PARSED"

ip=127.0.0.1
port=20838
count=5000000
infile=input.in
exe=./bin/pears_client
comp=""
h=0
possible_comps=["w_rc",""]

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
			fi
			shift 2
			;;
		-p|--port)
			port=$2
			shift 2
			;;
		-i|--infile)
			infile=$2
                        shift 2
                        ;;
		-c|--count)
			count=$2
                        shift 2
                        ;;
		-r|--rdma-comp)
			if [[ ! "${possible_comps[@]}" =~ "$2" ]]; then
				echo "error: invalid RDMA composition: $2"
				exit 1
			fi
			comp=_$2
                        shift 2
                        ;;
		-n|--node)
			if [ $addr_set == 1]; then
				echo "address already set to $ip, redundant argument:"
                                echo "$1 $2"
                        else
				ip=10.149.0.$2
			fi
			shift 2
                        ;;
		-h|--help)
			h=1
                        shift
			break
                        ;;
		--)
			shift
			;;
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
  -a, --addr       server IP address
  -p, --port       server port
  -i, --infile     input file containing key-value pairs
  -c, --count      maximum number of operations to perform
  -r, --rdma-comp  rdma composition, can be one of:
                   w_rc    - RDMA WRITE/RECV
		   ...
  -n, --node       DAS5 node where target is expected
  -h, --help       display help
"
exit 0
fi

$exe$comp -a $ip -p $port -i $infile -c $count
