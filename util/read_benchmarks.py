import sys
from os import listdir
from os.path import isfile, join
from statistics import mean
import re

import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt

import plotly.express as px

#sns.set_theme(style='ticks')

comps = ['sd_sd', 'wr_sd', 'wr_wr', 'wrimm_sd', 'wr_rd', 'mcd']
bm_types = ['']

def get_fn_info(filename):
	dat = {'comp': None, 'distr': None, 'reqs' : None, 'payload_size' : None}
	dat['comp'] = [c for c in comps if c in filename][0]
	m = re.search('(?<=\.)\d+_\d+_\d+(?=\.)', filename)
	cores = re.search('(?<=\.)\d+_\d+(?=\.)', filename)
	if cores != None:
		cores = cores.group(0)
		spl = cores.split('_')
		dat['cores'] = str(int(spl[0]) * int(spl[1]))
	info = m.group(0).split('_')
	dat['reqs'] = info[0]
	dat['payload_size'] = info[1]
	dat['distr'] = info[2]

	return dat

def get_avg_lat(l_f):
	lats = []
	with open(l_f) as f:
		for line in f.readlines():
			if '== benchmark' in line:
				lats.append(float(line.split('[')[2].split(' ')[0]))
	return mean(lats)

def read_lat(path, files):
	results = {'sd_sd': None, 'wr_sd': None, 'wr_wr': None, 'wrimm_sd': None, 'wr_rd': None, 'mcd': None}
	szs = []
	cfgs = []
	for f in files:
		dat = get_fn_info(f)
		lat_mean = get_avg_lat(join(path, f))
		#lat_mean = 0
		cfg = dat['comp']
		if cfg not in cfgs:
			cfgs.append(cfg)

		sz = dat['payload_size']
		if sz not in szs:
			szs.append(sz)

		if results[cfg] == None:
			results[cfg] = {'size' : {}}
		if sz not in results[cfg]['size']:
			results[cfg]['size'][sz] = lat_mean #int(sz) #lat_mean

	cfgs.sort()
	
	rows = []
	for sz in szs:
		row = []
		for k in cfgs:
			row.append(results[k]['size'][sz])
		rows.append(row)
	df = pd.DataFrame(data=rows, index=szs, columns=cfgs)

	plot_lat(df, f'Latency of PERK for various payload sizes compared with Memcached', out_file=f'lat_{len(szs)}.png')

def get_ops(o_f):
	ops = 0.0
	with open(o_f) as f:
		for line in f.readlines():
			if '== benchmark' in line:
				ops += float(line.split('[')[2].split(']')[0])
	return ops

def read_scale(path, files):
	results = {'sd_sd': None, 'wr_sd': None, 'wr_wr': None, 'wrimm_sd': None, 'wr_rd': None, 'mcd': None}
	nps = []
	szs = []
	cfgs = []
	for f in files:
		dat = get_fn_info(f)
		cfg = dat['comp']
		if cfg not in cfgs:
			cfgs.append(cfg)
		np = dat['cores']
		if int(np) not in nps and int(np) < 32:
			nps.append(int(np))
		sz = dat['payload_size']
		if sz not in szs:
			szs.append(sz)

		if results[cfg] == None:
			results[cfg] = {'size': {}}
		if sz not in results[cfg]['size']:
			results[cfg]['size'][sz] = {'cores': {}}
		if np not in results[cfg]['size'][sz]['cores']:
			results[cfg]['size'][sz]['cores'][np] = 0.0
		ops = get_ops(join(path, f))
		results[cfg]['size'][sz]['cores'][np] += ops

	cfgs.sort()
	nps.sort()
	dfs = []

	for sz in szs:
		rows = []
		for np in nps:
			row = []
			for k in cfgs:
				row.append(results[k]['size'][sz]['cores'][str(np)])
			rows.append(row)
		df = pd.DataFrame(data=rows, index=nps, columns=cfgs)
		dfs.append(df)

	for i in range(len(dfs)):
		print(f'Plotting scalability for payload size {szs[i]}')
		plot_scale(dfs[i], f'Scalability of PERK for payload size {szs[i]}', out_file=f'scale_{szs[i]}.png')

def get_cpu(c_f):
	cpu = 0
	with open(c_f) as f:
		for line in f.readlines():
			if 'cycles' in line:
				cols = [p for p in line.split(' ') if len(p) > 0]
				cpu = int(cols[0].replace(',',''))
	return cpu

def read_cpu(path, files):
	results = {'sd_sd': None, 'wr_sd': None, 'wr_wr': None, 'wrimm_sd': None, 'wr_rd': None, 'mcd': None}
	
	szs = []
	cfgs = []

	for f in files:
		dat = get_fn_info(f)
		cfg = dat['comp']
		if cfg not in cfgs:
			cfgs.append(cfg)
		sz = dat['payload_size']
		if sz not in szs:
			szs.append(sz)
		
		if results[cfg] == None:
			results[cfg] = {'size': {}}
		if sz not in results[cfg]['size']:
			results[cfg]['size'][sz] = {'cores': {}}
		results[cfg]['size'][sz] = get_cpu(join(path, f))
		
	cfgs.sort()

	rows = []
	for sz in szs:
		for k in cfgs:
			rows.append([k, sz, results[k]['size'][sz]])
	df = pd.DataFrame(data=rows, columns=['type','size', 'cpu'])
	print(df)
	#df = sns.load_dataset("penguins")
	#print(df.head())
	#print(df)
	plot_cpu(df, f'CPU cycles consumed by PERK for given payload size', out_file=f'cpu_{len(szs)}.png')

def plot_scale(df, title, out_file=None):
	sg = sns.lineplot(data=df, palette='Set3', linewidth=2.0, markers=True, dashes=False)

	sg.set(xlim = (0,16))
	sg.set(ylim = (0, 4500000))
	sg.set_title(title)

	plt.xlabel('Clients')
	plt.ylabel('Performance (ops/sec)')
	#store it in a file
	if out_file != None:
		sg.savefig(f'{out_file}', dpi=400)

	plt.clf()

def plot_lat(df, title, out_file=None):
	sg = sns.lineplot(data=df, palette='Set3', linewidth=1.5, markers=True, dashes=False)

	#ax.set_xlim(0, 128)
	sg.set(ylim = (0, 30))
	sg.set_title(title)

	plt.xlabel('Payload size')
	plt.ylabel('Latency (usec)')
	#store it in a file
	if out_file != None:
		sg.savefig(f'{out_file}', dpi=400)

	plt.clf()

def plot_cpu(df, title, out_file=None):
	sg = sns.catplot(data=df, kind='bar', x='size', y='cpu', hue='type', ci='sd', palette='dark', alpha=.6)

	#ax.set_title(title)
	#plt.xlabel('Configuration')
	#plt.ylabel('Cpu cycles')
	sg.set_axis_labels('','Cpu cycles')	
	sg.legend.set_title('Title')

	if out_file != None:
		sg.savefig(f'{out_file}', dpi=400)

	plt.clf()
	

def main(argv):
	path = argv[1]
	files = [f for f in listdir(path) if isfile(join(path, f))]

	bm_scale = [f for f in files if 'cl_bm_scale' in f]
	bm_ops_gen = [f for f in files if 's_bm_ops_gen' in f]
	bm_lat = [f for f in files if 'cl_bm_lat' in f]
	bm_cpu = [f for f in files if 'cpu' in f]

	#print(bm_cpu)
	read_cpu(path, bm_cpu)
	#read_lat(path, bm_lat)
	#read_scale(path, bm_scale)
	

if __name__ == "__main__":
	main(sys.argv)

