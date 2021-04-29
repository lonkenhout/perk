#!/bin/bash
set -o errexit -o pipefail -o noclobber -o nounset

cmd=$0
! getopt --test > /dev/null
if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
    echo 'erro: getopt failed'
    exit 1
fi

OPTIONS="a:p:r:nh"
LONGOPTS="addr:,port:,rdma-comp:,node,help"

! PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    exit 2
fi
eval set -- "$PARSED"

ip=0.0.0.0
port=20838
exe=./bin/pears_server
comp=""
h=0
addr_set=0
possible_comps=["w_sd","wimm_sd","sd_sd",""]

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
                -r|--rdma-comp)
                        if [[ ! "${possible_comps[@]}" =~ "$2" ]]; then
                                echo "error: invalid RDMA composition: $2"
                                exit 1
                        fi
                        comp=_$2
                        shift 2
                        ;;
                -n|--node)
                        if [ $addr_set == 1 ]; then
                                echo "address already set to $ip, redundant argument:"
                                echo "$1 $2"
                        else
				# extract ip from local node configuration (expects it on ib0)
				ip=`ifconfig ib0 | grep "inet " | awk '{print $2}'`
                        fi
                        shift
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
  -r, --rdma-comp  rdma composition, can be one of:
                   w_rc    - RDMA WRITE/RECV
                   wimm_rc - RDMA WRITE with IMM/RECV
                   ...
  -n, --node       bind to local node (no argument)
  -h, --help       display help
"
exit 0
fi

$exe$comp -a $ip -p $port

