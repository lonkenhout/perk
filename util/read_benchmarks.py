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
	run = re.search('(?<=\.)r\d+(?=\.)', filename)
	if cores != None:
		cores = cores.group(0)
		spl = cores.split('_')
		dat['cores'] = str(int(spl[0]) * int(spl[1]))
	else:
		cores = re.search('(?<=\.)\d+(?=\.r\d+)', filename)
		if cores != None:
			cores = cores.group(0)
			dat['cores'] = cores

	if run != None:
		run = run.group(0)
		dat['run'] = run
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
	results = {'mcd' : None, 'sd_sd' : None, 'wr_rd' : None, 'wr_sd' : None, 'wr_wr' : None, 'wrimm_sd' : None}
	szs = []
	cfgs = []

	rows = []
	for f in files:
		dat = get_fn_info(f)
		print(f)
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
			results[cfg]['size'][sz] = []
		#results[cfg]['size'][sz] += [lat_mean] #int(sz) #lat_mean
		rows.append([int(sz), cfg, lat_mean])

	print(results)
	cfgs.sort()
	
	#rows = []
	#for sz in szs:
	#	for k in cfgs:
	#		rows.append([int(sz), k, mean(results[k]['size'][sz])])
	df = pd.DataFrame(data=rows, columns=['size','type','latency'])
	sorted_df = df.sort_values(['size','type'], axis=0)	


	plot_lat(sorted_df, f'Latency of PERK for various payload sizes compared with Memcached', out_file=f'lat_{len(szs)}.svg')

def get_ops(o_f, mcd_check = False):
	ops = 0.0
	with open(o_f) as f:
		for line in f.readlines():
			if mcd_check and 'processed' in line:
				total = int(line.split(' ')[2])
				if total % 3000000 != 0:
					break
			if '== benchmark' in line:
				ops += float(line.split('[')[2].split(']')[0])
	return ops

def read_scale(path, files):
	results = {'mcd' : None, 'sd_sd' : None, 'wr_rd' : None, 'wr_sd' : None, 'wr_wr' : None, 'wrimm_sd' : None}
	nps = []
	szs = []
	cfgs = []
	runs = []
	for f in files:
		dat = get_fn_info(f)
		cfg = dat['comp']
		if cfg != 'mcd' and 's_bm_scale' in f:
			if cfg not in cfgs:
				cfgs.append(cfg)
			np = dat['cores']
			if int(np) not in nps and int(np) < 32:
				nps.append(int(np))
			sz = dat['payload_size']
			if sz not in szs:
				szs.append(sz)
			run = dat['run']
			if run not in runs:
				runs.append(run)

			if results[cfg] == None:
				results[cfg] = {'size': {}}
			if sz not in results[cfg]['size']:
				results[cfg]['size'][sz] = {'cores': {}}
			if np not in results[cfg]['size'][sz]['cores']:
				results[cfg]['size'][sz]['cores'][np] = []
			ops = get_ops(join(path, f))
			results[cfg]['size'][sz]['cores'][np] += [ops]
		elif cfg == 'mcd' and not 's_bm_scale' in f:
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
				results[cfg]['size'][sz]['cores'][np] = []
			ops = get_ops(join(path, f), True)
			if ops > 0.0:
				results[cfg]['size'][sz]['cores'][np] += [ops]
			
	cfgs.sort()
	nps.sort()
	dfs = []
	
	for sz in szs:
		rows = []
		for np in nps:
			#row = []
			for k in cfgs:
				for r in results[k]['size'][sz]['cores'][str(np)]:
					rows.append([k, np, r])
		df = pd.DataFrame(data=rows, columns=['type', 'procs', 'ops'])
		dfs.append(df)

	for i in range(len(dfs)):
		print(f'Plotting scalability for payload size {szs[i]}')
		sorted_df = dfs[i].sort_values(['type'], axis=0)	
		plot_scale(sorted_df, f'Scalability of PERK for payload size {szs[i]}', out_file=f'scale_{szs[i]}.svg')

def get_cpu(c_f):
	cpu = 0
	with open(c_f) as f:
		for line in f.readlines():
			if 'cycles' in line:
				if '32' in c_f:
					print(line, c_f)
				cols = [p for p in line.split(' ') if len(p) > 0]
				cpu = int(cols[0].replace(',',''))
	return cpu

def read_cpu(path, files):
	results = {'sd_sd': None, 'wr_sd': None, 'wr_wr': None, 'wrimm_sd': None, 'wr_rd': None, 'mcd': None}
	
	szs = []
	cfgs = []

	for f in files:
		sd = 's'
		if 'cl' in f:
			sd = 'cl'
		dat = get_fn_info(f)
		cfg = dat['comp']
		if cfg not in cfgs:
			cfgs.append(cfg)
		sz = dat['payload_size']
		if sz not in szs:
			szs.append(sz)
		
		if results[cfg] == None:
			results[cfg] = {'side': {}}
		if sd not in results[cfg]['side']:
			results[cfg]['side'][sd] = {'size': {}}
		if sz not in results[cfg]['side'][sd]['size']:
			results[cfg]['side'][sd]['size'][sz] = []
		cpu = get_cpu(join(path, f))
		if cpu > 0:
			results[cfg]['side'][sd]['size'][sz] += [cpu]
		
	cfgs.sort()

	rows = []
	rows_cl = []
	rows_sr = []
	for sz in szs:
		for sd in ['cl', 's']:
			for k in cfgs:
				if sd == 'cl':
					for r in results[k]['side'][sd]['size'][sz]:
						rows_cl.append([k, sd, int(sz), r])
				else:
					for r in results[k]['side'][sd]['size'][sz]:
						rows_sr.append([k, sd, int(sz), r])
	print(pd.DataFrame(data=rows_cl, columns=['type', 'side','size', 'cpu']))
	df_cl = pd.DataFrame(data=rows_cl, columns=['type', 'side','size', 'cpu']).sort_values(['size','type'], axis=0)
	df_sr = pd.DataFrame(data=rows_sr, columns=['type', 'side','size', 'cpu']).sort_values(['size','type'], axis=0)
	print('Plotting CPU usage for following payload sizes:', szs)
	plot_cpu(df_cl, f'CPU cycles consumed by PERK for given payload size client-side', out_file=f'c_cpu_{len(szs)}.svg')
	plot_cpu(df_sr, f'CPU cycles consumed by PERK for given payload size server-side', out_file=f's_cpu_{len(szs)}.svg')

def plot_scale(df, title, out_file=None):
	sg = sns.lineplot(data=df, x='procs', y='ops', hue='type', ci='sd', style='type', linewidth=2.0, markers=True, dashes=False)

	sg.set(xlim = (0,16))
	sg.set(ylim = (0, 4000000))

	plt.xlabel('Clients')
	plt.ylabel('Performance (ops/sec)')

	if out_file != None:
		f = sg.get_figure()
		f.savefig(f'{out_file}', dpi=400, bbox_inches="tight")

	plt.clf()

def plot_lat(df, title, out_file=None):
	sg = sns.lineplot(data=df, x='size', y='latency', hue='type', style='type', linewidth=1.5, markers=True, dashes=False)

	sg.set(xlim = (0, None))
	sg.set(ylim = (0, 30))
	#sg.set_title(title)

	plt.xlabel('Payload size (bytes)')
	plt.ylabel('Latency (usec)')

	if out_file != None:
		f = sg.get_figure()
		f.savefig(f'{out_file}', dpi=400, bbox_inches="tight")

	plt.clf()

def plot_cpu(df, title, out_file=None):
	sg = sns.catplot(data=df, x='size', y='cpu', hue='type', kind='bar', ci='sd', alpha=.6, capsize=.1, errwidth=0.7)

	sg.set_axis_labels('Payload size (bytes)','Cpu cycles')	
	#sg.legend.set_title('Title')

	if out_file != None:
		sg.savefig(f'{out_file}', dpi=400, bbox_inches="tight")
	plt.clf()
	

def main(argv):
	path = argv[1]
	files = [f for f in listdir(path) if isfile(join(path, f))]

	bm_scale = [f for f in files if 'bm_scale' in f]
	bm_ops_gen = [f for f in files if 's_bm_ops_gen' in f]
	bm_lat = [f for f in files if 'cl_bm_lat' in f]
	bm_cpu = [f for f in files if 'cpu' in f]

	read_cpu(path, bm_cpu)
	read_lat(path, bm_lat)
	read_scale(path, bm_scale)
	

if __name__ == "__main__":
	main(sys.argv)

