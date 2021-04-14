import string
import random

def key_gen(ext_len = 3):
    return 'key'+''.join(random.choices(string.digits, k=ext_len))

def val_gen(val_len = 30, chars=string.ascii_lowercase + string.digits):
    return ''.join(random.choice(chars) for _ in range(val_len))

def get_gen(key_rg = 3):
    return 'G' + ':' + key_gen(key_rg)

def put_gen(key_rg = 3):
    return 'P' + ':' + key_gen(key_rg) + ':' + val_gen()

def request_gen(req_amt=1000, distr=0.95):
    res = []
    GET_num = int(distr *req_amt)
    for i in range(0, GET_num):
        res.append(get_gen(3))
    for i in range(0, req_amt - GET_num):
        res.append(put_gen(3))
    random.shuffle(res)
    return res

res = request_gen(100000)

f = open("input.in", "w")
if f:
    for r in res:
        f.write(f'{r}\n')

f.close()
