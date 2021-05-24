import string
import random
import sys
import getpass

def key_gen(ext_len = 3):
    return 'key'+''.join(random.choices(string.digits, k=ext_len))

def val_gen(val_len, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(val_len))

def get_gen(key_rg = 3):
    return 'G' + ':' + key_gen(key_rg)

def put_gen(key_rg = 3, val_len = 30):
    return 'P' + ':' + key_gen(key_rg) + ':' + val_gen(val_len)

def request_gen(req_amt=1000, distr=0.95, val_len=30):
	res = []
	ext_len = 3
	GET_num = int(distr * req_amt)
	get_count = 0
	put_count = 0
	top_bound = int(distr * 100)
	max_put = (req_amt - GET_num)
	for i in range(0, req_amt):
		if put_count == max_put or random.randint(0, 100) < top_bound and not get_count > GET_num:
			r = get_gen(ext_len)
			get_count += 1
		else:
			r = put_gen(ext_len, val_len)
			put_count += 1
		yield r

def main(argv):
	user = getpass.getuser()
	reqs = int(argv[0])
	distr = float(argv[1])
	payload_len = int(argv[2])
	# val len = total len - len(key) - len(type) - 1 (null byte)
	val_len = payload_len - 8 - 1 - 1

	f = open(f"/var/scratch/{user}/input_{reqs}_{payload_len}_{int(distr*100)}.in", "w")
	if f:
		for r in request_gen(reqs, distr, val_len):
			f.write(f'{r}\n')
	f.close()

if __name__ == '__main__':
	main(sys.argv[1:])
