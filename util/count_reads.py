import sys
from os import listdir
from os.path import isfile, join
from statistics import mean

def read_count(fn):
	with open(fn, "r") as f:
		r_cts = {'GET': {'lst':[],'mean': 0.0, 'max': 0, 'min': 100000000}, 'PUT': {'lst':[],'mean':0.0, 'max': 0, 'min': 100000000}}
		for l in f.readlines():
			try:
				if "READcount" in l:
					req_type = l.split('[')[1].split(' ')[1].replace(']', '')
					r = int(l.split('[')[2].split(' ')[0])
					if r > r_cts[req_type]['max']:
						r_cts[req_type]['max'] = r
					if r < r_cts[req_type]['min']:
						r_cts[req_type]['min'] = r
					r_cts[req_type]['lst'].append(r)
			except:
				pass
		for r in r_cts:
			r_cts[r]['mean'] = mean(r_cts[r]['lst'])
		print(f'{fn}:')
		print(f'    \tGET\tPUT')
		print(f'min:\t{r_cts["GET"]["min"]}\t{r_cts["PUT"]["min"]}')
		print(f'max:\t{r_cts["GET"]["max"]}\t{r_cts["PUT"]["max"]}')
		print(f'avg:\t{round(r_cts["GET"]["mean"], 4)}\t{round(r_cts["PUT"]["mean"], 4)}')
	return r_cts

def main(argv):
	path = argv[1]
	files = [f for f in listdir(path) if isfile(join(path, f))]
	r_cts = []
	for f in files:
		r_cts.append(read_count(join(path, f)))
	overal = {'GET': {'lst':[], 'max': 0, 'min': 100000000}, 'PUT': {'lst':[], 'max': 0, 'min': 100000000}}
	for r in r_cts:
		overal['GET']['lst'].append(r['GET']['mean'])
		if r['GET']['min'] < overal['GET']['min']:
			overal['GET']['min'] = r['GET']['min']
		if r['GET']['max'] > overal['GET']['max']:
			overal['GET']['max'] = r['GET']['max']

		overal['PUT']['lst'].append(r['PUT']['mean'])
		if r['PUT']['min'] < overal['PUT']['min']:
			overal['PUT']['min'] = r['PUT']['min']
		if r['PUT']['max'] > overal['PUT']['max']:
			overal['PUT']['max'] = r['PUT']['max']
	overal['GET']['mean'] = mean(overal['GET']['lst'])
	overal['PUT']['mean'] = mean(overal['PUT']['lst'])
	print(overal)

if __name__ == "__main__":
	main(sys.argv)
