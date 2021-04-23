

ip=`ifconfig ib0 | grep "inet " | awk '{print $2}'`
if [ $# -eq 0 ]
then
	port=20838
	echo "Using default params: IP $ip PORT $port"
	./bin/pears_server -a $ip -p $port
elif [ $# -eq 1 ]
then
	echo "Using default params: IP 0.0.0.0 PORT $1, running with valgrind"
	./bin/pears_server -a $ip -p $1
elif [ $# -eq 2 ]
then
	echo "Using user params: IP $1 PORT $2"
	./bin/pears_server -a $1 -p $2
fi
