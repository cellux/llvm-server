#!/usr/bin/env python

import socket, os, re

src_path = "hello.ll"

if not os.path.exists(src_path):
    os.system("make {}".format(src_path))
src_size = os.stat(src_path).st_size
with open(src_path, 'rb') as f:
    src = f.read()
if len(src) != src_size:
    raise RuntimeError("read size mismatch")

def send_line(s, line):
    print("send_line: {}".format(line))
    s.sendall(bytes(line+"\n", 'utf-8'))

def send_payload(s, data):
    print("send_payload: {} bytes".format(len(data)))
    s.sendall(data)

def read_ok(s):
    response = s.recv(4096)
    delimiter_index = response.index(b'\n')
    first_line = response[:delimiter_index].decode()
    m = re.match(r'^OK ([0-9]+)', first_line)
    if not m:
        raise RuntimeError("invalid first_line in response: {}".format(first_line))
    payload_size = int(m[1])
    if payload_size:
        payload = response[(delimiter_index+1):]
        while len(payload) < payload_size:
            payload += s.recv(payload_size - len(payload))
        if len(payload) != payload_size:
            raise RuntimeError("payload size mismatch: received={} expected={}".format(len(payload), payload_size))

s = socket.create_connection(("127.0.0.1", 4000))

send_line(s, "PARSE {}".format(src_size))
send_payload(s, src)
read_ok(s)

send_line(s, "COMMIT")
read_ok(s)

send_line(s, "CALL hello 4096")
read_ok(s)

send_line(s, "QUIT")
read_ok(s)

s.close()
