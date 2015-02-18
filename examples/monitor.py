#client example
import socket
import time
import ctypes

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect(('localhost', 8898))

#def COMM_send_message(string, int_type, dest_socket):


def COMM_cast(value):
	try:
		return int(value)
	except ValueError:
		return int(float(value))

def COMM_read_bytes (sock):
	received_char = '0'
	message_size = ''

	while(recived_char != '|'):
		received_char = sock.recv(1)	
		if(len(received_char) == 0):
			return None 

		if(received_char!='|'):
			message_size += received_char

	msg_size = COMM_cast(message_size)
	offset = 0
	msg = ''	

	while(offset < msg_size):
		rcv = sock.recv((msg_size - ofsset))
		if(len(rcv) <= 0):
			return None
		msg+=rcv
		offset+=len(rcv)

	return msg
		
def COMM_send_bytes(sock, msg):
	sock.sendall((str(len(msg)) + '|'))
	if(len(msg)>0):
		sock.sendall(msg)
	return 0


time.sleep(5)
client_socket.close()
