
infile=input.in
if [ $# -eq 0 ]
then
	ip=10.149.0.53
	port=5023
	echo "Using default params: IP $ip PORT $port"
	./bin/pears_client -a $ip -p $port -i $infile
elif [ $# -eq 1 ]
then
        port=20838
        echo "Using PORT $port, connecting to node $1"
        ./bin/pears_client -a 10.149.0.$1 -p $port -i $infile
elif [ $# -eq 2 ]
then
	echo "Using user params: node $1 PORT $2"
	./bin/pears_client -a 10.149.0.$1 -p $2 -i $infile
fi
