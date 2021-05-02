#!/bin/bash
set -o errexit -o pipefail -o noclobber -o nounset

cmd=$0
! getopt --test > /dev/null
if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
    echo 'erro: getopt failed'
    exit 1
fi

OPTIONS="a:p:r:T:nh"
LONGOPTS="addr:,port:,rdma-comp:,timeout,node,help"

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
possible_comps=["w_sd","wimm_sd","sd_sd","mcd",""]
T=0
max_threads=4

while true && [ $# -gt 1 ]; do
        #echo $1
        case "$1" in
                -a|--addr)
                        if [ "$addr_set" == "1"]; then
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
			if [ "$2" == "mcd" ]; then
				echo "switching to memcached"
				ip=`ifconfig ib0 | grep "inet " | awk '{print $2}'`
				port=11211
			fi
                        comp=_$2
                        shift 2
                        ;;
		-t|--timeout)
			T=$2
			shift 2
			;;
                -n|--node)
                        if [ "$addr_set" == "1" ]; then
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
                   w_sd    - RDMA WRITE/SEND
                   wimm_sd - RDMA WRITE with IMM/SEND
                   sd_sd   - SEND/SEND
                   mcd     - use memcached
                   ...
  -t, --timeout    timeout after which the server is automatically killed
  -n, --node       bind to local node (no argument)
  -h, --help       display help
"
exit 0
fi
if [ "$comp" != "_mcd" ]; then
	$exe$comp -a $ip -p $port
else
	echo "Starting memcached on $ip:$port on behalf of $USER with $max_threads threads"
	memcached -u $USER -l $ip -p $port -t $max_threads

	# find process id and make sure memcached is terminated after timeout T
	#ps_list=`ps aux`
	#ps_mcd=`echo "$ps_list" | grep "memcached" | awk 'FNR == 1 {print $2}'`
	#if [ $T > 0 ]; then
	#	(sleep $T; kill $ps_mcd) &
	#fi
fi





