

if [ $# -eq 0 ]
then
	ip=`ifconfig ib0 | grep "inet " | awk '{print $2}'`
	port=20838
	echo "Using default params: IP $ip PORT $port"
	./bin/pears_server -a $ip -p $port
elif [ $# -eq 1 ]
then
	if [ $1 == "-vg" ]
	then
		port=20838
		echo "Using default params: IP 0.0.0.0 PORT $port, running with valgrind"
		valgrind --leak-check=full --track-origins=yes ./bin/pears_server -a 0.0.0.0 -p $port
	fi
elif [ $# -eq 2 ]
then
	echo "Using user params: IP $1 PORT $2"
	./bin/pears_server -a $1 -p $2
fi
