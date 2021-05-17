import sys
from os import listdir
from os.path import isfile, join
from statistics import mean
import re

comps = ['sd_sd', 'wr_sd', 'wr_wr', 'wrimm_sd', 'wr_rd', 'mcd']
bm_types = ['']

def get_fn_info(filename):
	dat = {'comp': None, 'distr': None, 'reqs' : None, 'payload_size' : None}
	dat['comp'] = [c for c in comps if c in filename][0]
	m = re.search('(?<=\.)\d+_\d+_\d+(?=\.)', filename)
	cores = re.search('(?<=\.)\d+_\d+(?=\.)', filename).group(0)
	info = m.group(0).split('_')
	dat['reqs'] = info[0]
	dat['payload_size'] = info[1]
	dat['distr'] = info[2]
	if len(cores) > 0:
		spl = cores.split('_')
		dat['cores'] = str(int(spl[0]) * int(spl[1]))

	return dat

def get_avg_lat(l_f):
	lats = []
	with open(l_f) as f:
		for line in f.readlines():
			if '== benchmark' in line:
				lats.append(float(line.split('[')[2].split(' ')[0]))
	return mean(lats)

def read_lat(path, files):
	for f in files:
		dat = get_fn_info(f)
		lat_mean = get_avg_lat(join(path, f))
		print(f'Mean latency for {dat["comp"]}: {lat_mean} usec with {dat["reqs"]} iter, {dat["distr"]}%% GETs, and payload size {dat["payload_size"]}')
	
def get_ops(o_f):
	ops = 0.0
	with open(o_f) as f:
		for line in f.readlines():
			if '== benchmark' in line:
				ops += float(line.split('[')[2].split(']')[0])
	return ops

def read_scale(path, files):
	results = {'sd_sd': None, 'wr_sd': None, 'wr_wr': None, 'wrimm_sd': None, 'wr_rd': None, 'mcd': None}
	for f in files:
		dat = get_fn_info(f)
		if results[dat['comp']] == None:
			results[dat['comp']] = {'cores': {}}
		if dat['cores'] not in results[dat['comp']]['cores']:
			results[dat['comp']]['cores'][dat['cores']] = 0.0
		ops = get_ops(join(path, f))
		results[dat['comp']]['cores'][dat['cores']] += ops
	
	print(results)

	

def main(argv):
	path = argv[1]
	files = [f for f in listdir(path) if isfile(join(path, f))]

	bm_scale = [f for f in files if 'cl_bm_scale' in f]
	bm_ops_gen = [f for f in files if 's_bm_ops_gen' in f]
	bm_lat = [f for f in files if 'cl_bm_lat' in f]

	#read_lat(path, bm_lat)
	read_scale(path, bm_scale)
	

if __name__ == "__main__":
	main(sys.argv)

