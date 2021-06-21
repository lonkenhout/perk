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
## DEFAULTS:
i_dir="/var/scratch/$USER/"
o_dir="/var/scratch/${USER}/bm/"
count_conf=3000000
distr_conf=95
comps=(sd_sd wr_sd wr_wr wr_rd wrimm_sd)
sizes=(32 64 128 256 512 1024 2048)
offset=9

# Other stuff that can be managed through command line args
count=1000000
benchmarks=""
bm_cpu=0
bm_ops=0
bm_scale=0
bm_latency=0
scale=""
procs=""
reruns=1
all=0
OPTIONS="alocs:r:i:f:g:n:h"
LONGOPTS="all,latency,ops-per-sec,cpu-usage,scalability:,rerun:,input-conf:,output-folder:,num-requests:,input-folder,help"



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
			bm_latency=1
			scale=(1 2 4 8 16)
			procs=(1 2)
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
		-r|--rerun)
			rerun=$2
			shift 2 ;;
		-i|--input-conf)
			i_dir=$2
			shift 2 ;;
		-f|--output-folder)
			o_dir=$2
			shift 2 ;;
		-n|--num-requests)
			count=$2
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
  -c, --cpu-usage            measure cpu usage on server and client side
  -s, --scalability=<cores>  do specified benchmarks with <cores> clients
  -i, --input-conf=<conf>    use specified input file configuration, uses files as generated by
                             gen_small_workload.py, <conf> should look like:
                                 <num-requests>_<get-distribution>_<payload-size>, e.g.
                                 1000000_95_32, for 1000000 requests, of which 95% GETs, max size 32 bytes
  -n, --num-requests=<N>     the number of requests to actually do, 1,000,000 by default
  -f, --output-folder=<out>  use the specified output folder to 
  -h, --help                 display help
"
exit 0
fi

#echo "set options[bms:${benchmarks}; cpu:${bm_cpu}; scale:${bm_scale}=${scale[@]};] for user $USER"
R='\033[0;31m'
C='\033[0;36m'
NC='\033[0m'
info() { echo -e "${C}===[INFO]=== ${1}${NC}"; }

get_das5_node_ip() {
	echo "10.149.0.${1}"
}

reserve_server_node() {
	preserve -np 1 -t 28800 >/dev/null
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
	ip=$2
	rid=$3
	port=20838
	if [ "$m" == "0" ]; then
		bm_type=$4
		scale=$5
		run=$6
		prun -reserve $rid -np 1 -t 7200 -o ${o_dir}.s_${bm_type}.mcd.${in_conf}.${cores}.r$run memcached -v -d -u $USER -l $ip -p $port &
	else
		comp=$4
		cores=$5
		bm_type=$6
		run=$7
		prun -reserve $rid -np 1 -t 7200  -o ${o_dir}s_${bm_type}.${comp}.${in_conf}.${cores}.r$run ./bin/perk_server -r $comp -a $ip -p $port &
	fi
}

run_server_perf() {
	m=$1
    ip=$2
    rid=$3
    port=20838
    if [ "$m" == "0" ]; then
		run=$4
        prun -reserve $rid -np 1 -t 3600  -o ${o_dir}s_bm_cpu.mcd.${in_conf}.1_1.r$run perf stat memcached -u $USER -l $ip -p $port -t 16 &
    else
        comp=$4
        cores=$5
        bm_type=$6
		run=$7
        prun -reserve $rid -np 1 -t 3600  -o ${o_dir}s_bm_cpu.${comp}.${in_conf}.1_${cores}.r$run perf stat ./bin/perk_server -r $comp -a $ip -p $port &
    fi
}

run_clients_perf() {
	m=$1
    ip=$2
    port=20838
    num_p=$3
    num_n=$4
    count=$5
    if [ "$m" == "0" ]; then
		run=$6
        prun -$num_p -np $num_n -o ${o_dir}cl_bm_cpu.mcd.${in_conf}.${num_p}_${num_n}.r$run perf stat ./bin/client_mcd -a $ip -p $port -c $count -u -i $in_file
    else
        comp=$6
		run=$7
        prun -$num_p -np $num_n -o ${o_dir}cl_bm_cpu.${comp}.${in_conf}.${num_p}_${num_n}.r$run perf stat ./bin/perk_client -r $comp -a $ip -p $port -c $count -u -i $in_file
    fi
}

kill_server() {
	rid=$2
	if [ "$1" == "0" ]; then
        prun -reserve $rid -np 1 python3 util/kill_server.py memcached
	elif [ "$1" == "2" ]; then
        prun -reserve $rid -np 1 python3 util/kill_server.py memcachedp
	else
		prun -reserve $rid -np 1 pkill -f perk_server >/dev/null
	fi
}

run_clients() {
	m=$1
	ip=$2
	port=20838
	num_p=$3
	num_n=$4
	count=$5
	if [ "$m" == "0" ]; then
		bm_type=$6
		run=$7
        prun -$num_p -np $num_n -t 1800 -o ${o_dir}cl_${bm_type}.mcd.${in_conf}.${num_p}_${num_n}.r$run ./bin/client_mcd -a $ip -p $port -c $count -u -i $in_file
    else
        comp=$6
		bm_type=$7
		run=$8
        prun -$num_p -np $num_n -t 1800 -o ${o_dir}cl_${bm_type}.${comp}.${in_conf}.${num_p}_${num_n}.r$run ./bin/perk_client -r $comp -a $ip -p $port -c $count -u -i $in_file
    fi
}


for csz in "${sizes[@]}"; do
	vsz=`expr ${csz} - ${offset}`
    in_file="${i_dir}input_${count}_${csz}_${distr_conf}_"
	in_conf="${count_conf}_${csz}_${distr_conf}"
	echo $in_file
	for i in $(seq 1 $rerun); do
		# reserve a node for the server
		info "RESERVING NODE FOR SERVER, RUN $i SIZE $csz"
		rid="$(reserve_server_node)"
		node="$(get_node_num)"
		sr_ip="$(get_das5_node_ip ${node})"
		echo "acquired node, reservation id: ${rid}, node ${node}"
		if [ "$bm_scale" == "1" ]; then
			info "PREPARING BENCHMARKS"
			PERK_OVERRIDE_VALSIZE=${vsz} PERK_BM_SERVER_EXIT=1 PERK_BM_OPS_PER_SEC=1 cmake .
			make
			
			info "BENCHMARKING PERK OPS/SEC"
			for comp in "${comps[@]}"
			do
				for core in "${scale[@]}"
				do
					if [ "$core" == "32" ]; then
						for proc in "${procs[@]}"
						do
							info "RUNNING PERK SERVER"
							run_server 1 $sr_ip $rid $comp $core bm_scale $i
							info "Running ops/sec benchmark with ${core}*${proc} clients"
							run_clients 1 $sr_ip 2 16 $count $comp bm_scale $i
						done
					else
						info "RUNNING PERK SERVER"
						run_server 1 $sr_ip $rid $comp $core bm_scale $i
						info "Running ops/sec benchmark with $core clients"
						run_clients 1 $sr_ip $core 1 $count $comp bm_scale $i
					fi
				done
			done

			comp=mcd
			info "BENCHMARKING MCD OPS/SEC"
			for core in "${scale[@]}"
			do
				info "RUNNING MCD SERVER"
				run_server 0 $sr_ip $rid bm_scale $scale $i
				sleep 2
				info "Running ops/sec benchmark with $core clients"
				run_clients 0 $sr_ip $core 1 $count bm_scale $i
				info "KILLING MCD SERVER"
				kill_server 0 $rid
				sleep 2
			done

		fi
		if [ "$bm_cpu" == "1" ]; then
			info "PREPARING CPU BENCHMARK"
			PERK_OVERRIDE_VALSIZE=${vsz} PERK_BM_OPS_PER_SEC=1 PERK_BM_SERVER_EXIT=1 cmake .
			make
			info "BENCHMARKING CPU USAGE PERK"
			for comp in "${comps[@]}"
			do
				info "RUNNING PERK SERVER"
				run_server_perf 1 $sr_ip $rid $comp 1 bm_cpu $i
				info "RUNNING PERK CLIENT"
				run_clients_perf 1 $sr_ip 1 1 $count $comp $i
			done
			info "RUNNING MCD SERVER"
			run_server_perf 0 $sr_ip $rid $i
			info "RUNNING MCD CLIENT"
			run_clients_perf 0 $sr_ip 1 1 $count $i
			kill_server 2 $rid
		fi
		if [ "$bm_latency" == "1" ]; then
			info "PREPARING LATENCY BENCHMARK"
			echo PERK_OVERRIDE_VALSIZE=${vsz} PERK_BM_SERVER_EXIT=1 PERK_BM_LATENCY=1 cmake .
			PERK_OVERRIDE_VALSIZE=${vsz} PERK_BM_SERVER_EXIT=1 PERK_BM_LATENCY=1 cmake .
			make
			info "BENCHMARKING PERK LATENCY"
			for comp in "${comps[@]}"
			do
				info "RUNNING PERK SERVER"
				run_server 1 $sr_ip $rid $comp 1 bm_lat $i
				info "RUNNING PERK CLIENT"
				run_clients 1 $sr_ip 1 1 $count $comp bm_lat $i
			done
			info "RUNNING MCD SERVER"
			run_server 0 $sr_ip $rid bm_lat 1 $i
			info "RUNNING MCD CLIENT"
			run_clients 0 $sr_ip 1 1 $count bm_lat $i
			kill_server 0 $rid
		fi
		info "Cancelling server node reservation"
		preserve -c ${rid}
		rid=""
		node=""
		sleep 2
	done
done
