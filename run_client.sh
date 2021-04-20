
#infile=/home/rdmabox/Documents/bsc-project-rdma/testin
infile=input.in
if [ $# -eq 0 ]
then
	ip=10.149.0.57
	port=20838
	echo "Using default params: IP $ip PORT $port"
	./bin/pears_client -a $ip -p $port -i $infile
elif [ $# -eq 1 ]
then
    if [ $1 == "-vg" ]
    then
        port=20838
        echo "Using default params: IP 0.0.0.0 PORT $port, running with valgrind"
        valgrind --leak-check=full --track-origins=yes ./bin/pears_client -a 127.0.0.1 -p $port -i $infile
    fi
elif [ $# -eq 2 ]
then
	echo "Using user params: IP $1 PORT $2"
	./bin/pears_client -a $1 -p $2 -i $infile
fi
