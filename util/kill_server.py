import sys
import subprocess

def main(argv):
	if argv[-1] == "memcached":
		cmd = f"ps=`ps aux`; pid=`echo \"$ps\" | grep {argv[-1]} | awk 'FNR == 3 {{print $2}}'`; echo \"killing $pid\"; kill \"$pid\""
	elif argv[-1] == "memcachedp":
		cmd = f"ps=`ps aux`; pid=`echo \"$ps\" | grep memcached | awk 'FNR == 4 {{print $2}}'`; echo \"killing $pid\"; kill \"$pid\""
	else:
		cmd = f"ps=`ps aux`; pid=`echo \"$ps\" | grep {argv[-1]} | awk 'FNR == 2 {{print $2}}'`; echo \"killing $pid\"; kill \"$pid\""
	print(cmd)		

	process = subprocess.Popen(cmd, stdout=sys.stdout, shell=True)


if __name__ == "__main__":
	main(sys.argv)
