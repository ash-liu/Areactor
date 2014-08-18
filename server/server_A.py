''' a simple tcp server for test'''

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind(('192.168.1.109', 5566))
sock.listen(5)

while True:
	connect,address = sock.accept()
	#print address
	#print connect.recv(1000)
	connect.send('in python sa')
	connect.close()

