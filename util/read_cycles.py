import sys
from os import listdir
from os.path import isfile, join

from statistics import mean
import re

get_set = ['recv_get','handle_get','send_get','prep_get']
put_set = ['recv_put','handle_put','send_put','prep_put']

cl_get_set = ['prep_get', 'send_get', 'recv_get', 'prep_next_get']
cl_put_set = ['prep_put', 'send_put', 'recv_put', 'prep_next_put']

def get_fn_info(fn):
	m = re.search('(?<=cycle)\d+(?=\.)', fn)
	return int(m.group(0))

def read_cycles(fn):
	res = {}
	size = get_fn_info(fn)
	if not '32' in fn and not '2048' in fn:
		return
	f = open(fn, 'r')
	for line in f.readlines():
		try:
			if 'benchmark' in line:
				tp = line.split('[')[1].split(':')[1].replace(']','')
				tm = line.split('[')[2].split(' ')[0]
				if tp not in res:
					res[tp] = {'vals': [float(tm)], 'mean': 0.0}
				else:
					res[tp]['vals'].append(float(tm))
		except:
			pass
	get_total = 0.0
	if 'cl_' in fn:
		sets = [cl_get_set, cl_put_set]
	else:
		sets = [get_set, put_set]
	for k in sets[0]:
		res[k]['mean'] = mean(res[k]['vals'])
		get_total += res[k]['mean']
	put_total = 0.0
	for k in sets[1]:
		if res[k]['mean'] == 0.0:
			res[k]['mean'] = mean(res[k]['vals'])
		put_total += res[k]['mean']
		
	print(f'=== Results for {size} byte payloads')
	print(f'# Get requests {round(get_total, 1)} nsec')
	for k in sets[0]:
		print(f'\t{k}:\t{round(res[k]["mean"], 1)} nsec\t({round(res[k]["mean"] / get_total * 100, 1)}%)')
	print(f'# Put requests {round(put_total, 1)} nsec')
	for k in sets[1]:
		print(f'\t{k}:\t{round(res[k]["mean"], 1)} nsec\t({round(res[k]["mean"] / put_total * 100, 1)}%)')
		
	f.close()

def main(argv):
	path = argv[1]
	files = [f for f in listdir(path) if isfile(join(path, f))]
	
	for f in files:
		read_cycles(join(path, f))

if __name__ == "__main__":
	main(sys.argv)
