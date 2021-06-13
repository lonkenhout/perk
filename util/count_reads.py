import sys
import statistics

def read_count_32():
	with open("read_count.txt", "r") as f:
		r_max = 0
		r_min = 100000000
		r_lst = []
		for l in f.readlines():
			if "read" in l:
				r = int(l.strip("\n").split(' ')[2])
				if r > r_max:
					r_max = r
				if r < r_min:
					r_min = r
				r_lst.append(r)
		print(r_max, r_min, sum(r_lst) / len(r_lst))

def read_count_2048():
	with open("read_count2048.txt", "r") as f:
		r_max = 0
		r_min = 100000000
		r_lst = []
		for l in f.readlines():
			if "read" in l:
				r = int(l.strip("\n").split(' ')[2])
				if r > r_max:
					r_max = r
				if r < r_min:
					r_min = r
				r_lst.append(r)
		print(r_max, r_min, sum(r_lst) / len(r_lst))

def read_cl():
	with open("/var/scratch/lot230/copy.txt", "r") as f:
		rl = []
		wl = []
		copy = []
		for l in f.readlines():
			try:
				if 'benchmark' in l:
					ls = l.split('[')
					tm = ls[2].split(' ')[0]
					if 'copying' in ls[1]:
						copy.append(float(tm))
					elif 'rdlock' in ls[1]:
						rl.append(float(tm))

					elif 'wrlock' in ls[1]:
						wl.append(float(tm))
			except:
				print(l)

		print(f'copy {sum(copy) / len(copy)} nanosec avg')
		print(f'rl {sum(rl) / len(rl)} nanosec avg')
		print(f'wl {sum(wl) / len(wl)} nanosec avg')

read_cl()

