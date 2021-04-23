#!/bin/bash

infile=input.in
if [ $# -eq 1 ]
then
	ip=$1
	port=20838
	echo "Using params: IP $ip PORT $port"
	prun -np 1 -t 900 -v ./bin/pears_client -a $ip -p $port -i $infile
elif [ $# -eq 2 ]
then
	ip=$1
	port=20838
	echo "Using default params: IP $ip PORT $port"
	prun -$2 -np 1 -t 900 -v ./bin/pears_client -a $ip -p $port -i $infile
fi
