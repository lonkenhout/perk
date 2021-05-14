#!/bin/bash
set -o errexit -o pipefail -o noclobber -o nounset

cmd=$0
! getopt --test > /dev/null
if [[ ${PIPESTATUS[0]} -ne 4 ]]; then
    echo 'erro: getopt failed'
    exit 1
fi

path=`pwd`
h=0

# internal benchmarks are stored in here
benchmarks=""
bm_cpu=0
bm_ops=0
bm_scale=0
bm_latency=0
scale=""
all=0
OPTIONS="alocs:h"
LONGOPTS="all,latency,ops-per-sec,cpu-usage,scalability:,help"


! PARSED=$(getopt --options=$OPTIONS --longoptions=$LONGOPTS --name "$0" -- "$@")
if [[ ${PIPESTATUS[0]} -ne 0 ]]; then
    exit 2
fi
eval set -- "$PARSED"

while true && [ $# -gt 1 ]; do
	case "$1" in
		-a|--all)
			bm_cpu=1
			bm_scale=1
			bm_ops=1
			bm_latency=1
			scale=(1 2 4 8 16)
			all=1
			shift ;;
		-l|--latency)
			if [[ $all == 1 ]]; then
				echo "already doing all benchmarks, -l ignored"
			else
				bm_latency=1
			fi
			shift ;;
		-o|--ops-per-sec)
			if [[ $all == 1 ]]; then
                echo "already doing all benchmarks, -o ignored"
            else
				bm_ops=1
            fi
			shift ;;
		-c|--cpu-usage)
			if [[ $all == 1 ]]; then
                echo "already doing all benchmarks, -c ignored"
            else
				bm_cpu=1
            fi
			shift ;;
		-s|--scalability)
			if [[ $all == 1 ]]; then
                echo "already doing all benchmarks, -s ignored"
            else
				bm_scale=1
				IFS=',' read -ra scale <<< "$2"
            fi
			shift 2 ;;
		-h|--help)
			h=1
			break
			shift ;;
		--)
			shift ;;
		*)
			echo "error parsing arguments"
			exit 1 ;;
	esac
done

if [ $h == 1 ]
then
	echo "Usage: $cmd [OPTIONS]..
Options:
  -a, --all                  measure everything, defaults <cores> to 1,2,4,8,16
  -l, --latency              measure latency client->server->client
  -o, --ops-per-sec          measure operations per second performed server side
  -s, --scalability=<cores>  do specified benchmarks with <cores> clients
  -h, --help                 display help
"
exit 0
fi

#echo "set options[bms:${benchmarks}; cpu:${bm_cpu}; scale:${bm_scale}=${scale[@]};] for user $USER"
R='\033[0;31m'
C='\033[0;36m'
NC='\033[0m'
info() { echo -e "${C}===[INFO]=== ${1}${NC}"; }

reserve_server_node() { 
	preserve -np 1 -t 900 >/dev/null
	rid=""
	while [ "${rid}" == "" ]; do
		rid=`preserve -llist | grep $USER | awk '{print $1}'`
		sleep 1
	done
	echo "${rid}"
}

get_node_num() {
	node=""
	while [ "${node}" == "-" ] || [ "${node}" == "" ]; do
		node=`preserve -llist | grep $USER | awk '{print $9}'`
		sleep 2
	done
	echo "${node: -2}"
}

run_server() {
	m=$1
	ip=10.149.0.$2
	rid=$3
	port=20838
	if [ "$m" == "0" ]; then
		bm_type=$4
		echo "prun -reserve $rid -np 1 memcached -d -u $USER -l $ip -p 20838 -t 16"
		prun -reserve $rid -np 1  memcached -d -u $USER -l $ip -p 20838 -t 16
	else
		comp=$4
		cores=$5
		bm_type=$6
		echo "prun -reserve $rid -np 1 ./bin/pears_server -r $comp -a $ip -p $port"
		prun -reserve $rid -np 1 -o bm/s_${bm_type}.${rid}.${comp}.${cores} ./bin/pears_server -r $comp -a $ip -p $port &
	fi
}

run_server_perf() {
	m=$1
    ip=10.149.0.$2
    rid=$3
    port=20838
    if [ "$m" == "0" ]; then
        echo "prun -reserve $rid -np 1 memcached -d -u $USER -l $ip -p 20838 -t 16"
        prun -reserve $rid -np 1 -o bm/s_cpu.${rid}.mcd.${cores} perf stat memcached -u $USER -l $ip -p $port -t 16 &
    else
        comp=$4
        cores=$5
        bm_type=$6
        echo "prun -reserve $rid -np 1 ./bin/pears_server -r $comp -a $ip -p $port"
        prun -reserve $rid -np 1 -o bm/s_cpu.${rid}.${comp}.${cores} perf stat ./bin/pears_server -r $comp -a $ip -p $port &
    fi
}

run_clients_perf() {
	m=$1
    ip=10.149.0.$2
    port=20838
    num_p=$3
    num_n=$4
    count=$5
    if [ "$m" == "0" ]; then
        echo prun -$num_p -np $num_n ./bin/mcd_client -a $ip -p $ip -c $count
        prun -$num_p -np $num_n -o bm/cl_cpu.mcd.${num_p}_${num_n} perf stat ./bin/client_mcd -a $ip -p $port -c $count
    else
        comp=$6
        echo prun -$num_p -np $num_n ./bin/pears_client -r $comp -a $ip -p $port -c $count
        prun -$num_p -np $num_n -o bm/cl_cpu.${comp}.${num_p}_${num_n} perf stat ./bin/pears_client -r $comp -a $ip -p $port -c $count
    fi
}

kill_server() {
	rid=$2
	if [ "$1" == "0" ]; then
		echo prun -reserve $rid -np 1 python3 util/kill_server.py memcached
		prun -reserve $rid -np 1 python3 util/kill_server.py memcached
	elif [ "$1" == "2" ]; then
		echo prun -reserve $rid -np 1 python3 util/kill_server.py memcached
        prun -reserve $rid -np 1 python3 util/kill_server.py memcachedp
	else
		echo prun -reserve $rid -np 1 python3 util/kill_server.py pears_server
		prun -reserve $rid -np 1 python3 util/kill_server.py pears_server
	fi
}

run_clients() {
	m=$1
	ip=10.149.0.$2
	port=20838
	num_p=$3
	num_n=$4
	count=$5
	if [ "$m" == "0" ]; then
		bm_type=$6
        echo prun -$num_p -np $num_n ./bin/mcd_client -a $ip -p $ip -c $count
        prun -$num_p -np $num_n -o bm/cl_${bm_type}.mcd.${num_p}_${num_n} ./bin/client_mcd -a $ip -p $port -c $count
    else
        comp=$6
		bm_type=$7
        echo prun -$num_p -np $num_n ./bin/pears_client -r $comp -a $ip -p $port -c $count
        prun -$num_p -np $num_n -o bm/cl_${bm_type}.${comp}.${num_p}_${num_n} ./bin/pears_client -r $comp -a $ip -p $port -c $count
    fi
}

# reserve a node for the server
info "RESERVING NODE FOR SERVER"
rid="$(reserve_server_node)"
node="$(get_node_num)"
comps=(sd_sd wr_sd wr_wr wr_rd) #wrimm_sd)
count=1000000
echo "acquired node, reservation id: ${rid}, node ${node}"

if [ "$bm_scale" == "1" ]; then
	info "PREPARING BENCHMARKS"
	PERK_BM_SERVER_EXIT=1 PERK_BM_OPS_PER_SEC=1 cmake .
	make
	
	info "BENCHMARKING PERK OPS/SEC"
	for comp in "${comps[@]}"
	do
		for core in "${scale[@]}"
		do
			info "RUNNING PERK SERVER"
			run_server 1 $node $rid $comp $core bm_scale
			info "Running ops/sec benchmark with $core clients"
			run_clients 1 $node $core 1 $count $comp bm_scale
		done
	done
	info "BENCHMARKING MCD OPS/SEC"
	info "RUNNING MCD SERVER"
	run_server 0 $node $rid $comp $core
	for core in "${scale[@]}"
	do
		info "Running ops/sec benchmark with $core clients"
		run_clients 0 $node $core 1 $count bm_scale
	done
	info "KILLING MCD SERVER"
	kill_server 0 $rid
fi
if [ "$bm_cpu" == "1" ]; then
	info "PREPARING CPU BENCHMARK"
	PERK_BM_SERVER_EXIT=1 cmake .
	make
	info "BENCHMARKING CPU USAGE PERK"
	for comp in "${comps[@]}"
    do
		info "RUNNING PERK SERVER"
		run_server_perf 1 $node $rid $comp 1 bm_cpu
		info "RUNNING PERK CLIENT"
        run_clients_perf 1 $node 1 1 $count $comp bm_cpu
	done
	info "RUNNING MCD SERVER"
	run_server_perf 0 $node $rid bm_cpu
	info "RUNNING MCD CLIENT"
	run_clients_perf 0 $node 1 1 $count bm_cpu
	kill_server 2 $rid
fi
if [ "$bm_latency" == "1" ]; then
	info "PREPARING LATENCY BENCHMARK"
	PERK_BM_SERVER_EXIT=1 PERK_BM_LATENCY=1 cmake .
	make
	info "BENCHMARKING PERK LATENCY"
	for comp in "${comps[@]}"
	do
		info "RUNNING PERK SERVER"
		run_server 1 $node $rid $comp 1 bm_lat
		info "RUNNING PERK CLIENT"
		run_clients 1 $node 1 1 $count $comp bm_lat
	done
	info "RUNNING MCD SERVER"
	run_server 0 $node $rid bm_lat
	info "RUNNING MCD CLIENT"
	run_clients 0 $node 1 1 $count bm_lat
	kill_server 0 $rid
fi
if [ "$bm_ops" == "1" ]; then
	info "PREPARING GENERIC OPS/SEC BENCHMARK"
	PERK_BM_SERVER_EXIT=1 PERK_BM_OPS_PER_SEC=1 cmake .
	make
	info "BENCHMARKING PERK LATENCY"
    for comp in "${comps[@]}"
    do
        info "RUNNING PERK SERVER"
        run_server 1 $node $rid $comp 1 bm_ops_gen
        info "RUNNING PERK CLIENT"
        run_clients 1 $node 1 1 $count $comp bm_ops_gen
    done
    info "RUNNING MCD SERVER"
    run_server 0 $node $rid bm_ops_gen
    info "RUNNING MCD CLIENT"
    run_clients 0 $node 1 1 $count bm_ops_gen
    kill_server 0 $rid
fi

info "Cancelling server node reservation"
echo preserve -c ${rid}