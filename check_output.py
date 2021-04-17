import sys

def check_entry(key, val, kvs):
	ret = False
	if val == 'EMPTY' and key not in kvs:
		ret = True
	elif val == 'EMPTY' and key in kvs:
		print("key is empty, but should have value")
	else:
		if key not in kvs:
			print("key should not have value")
		elif val == kvs[key]:
			ret = True
	return ret

if __name__ == '__main__':
	f = open("test.out", "r")
	if f:
		correct = 0
		total = 0
		lines = f.read()
		lines = lines.split('\n')
		kvs = {}
		for line in lines:
			spl = line.split(':')
			if len(spl) == 3:
				# get request
				if not check_entry(spl[1], spl[2], kvs):
					print("incorrect: ", spl)
				else: 
					correct += 1
			elif len(spl) == 4:
				# put request
				if spl[3] != 'INSERTED':
					print("failed: ", spl)
				else:
					kvs[spl[1]] = spl[2]
					correct += 1
			else:
				continue
			total += 1
	print(f"{total} entries checked, {correct} correct")		
	f.close()