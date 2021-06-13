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

def check_entry_par(key, val, kvs):
	ret = False
	if key in kvs and val in kvs[key]:
		ret = True
	else:
		print(f'key with value "{val}" should have one of following values {kvs[key]}')
	return ret

def check_single_core():
	f = open("out.txt", "r")
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
					print("failed")
					sys.exit()
				else:
					kvs[spl[1]] = spl[2]
					correct += 1
			else:
				continue
			total += 1
	print(f"{total} entries checked, {correct} correct")		
	f.close()

def check_multi_core():
	f = open("out.txt", "r")
	# first read all the puts and add them to potential values at a given time
	if f:
		correct = 0
		total = 0
		lines = f.read()
		lines = lines.split('\n')
		kvs = {}
		# value can be empty at first, if a get is done before a put, add EMPTY as an option for the returned value
		# otherwise the value should simply be one of the put values
		for line in lines:
			spl = line.split(':')
			if len(spl) == 4:
				if spl[3] != 'INSERTED':
					print("failed")
					sys.exit()
				else:
					if spl[1] in kvs:
						kvs[spl[1]].append(spl[2])
					else:
						kvs[spl[1]] = [spl[2]]
					correct += 1
					total += 1
			elif len(spl) == 3:
				if spl[1] not in kvs and spl[2] == 'EMPTY':
					kvs[spl[1]] = [spl[2]]

		for line in lines:
			spl = line.split(':')
			if len(spl) == 3:
				# get request
				if not check_entry_par(spl[1], spl[2], kvs):
					print("incorrect: ", spl)
				else: 
					correct += 1
			else:
				continue
			total += 1
	print(f"{total} entries checked, {correct} correct")		
	f.close()

if __name__ == '__main__':
	check_multi_core()
	#check_single_core()
