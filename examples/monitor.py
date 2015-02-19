import socket
import time

# Message Types 
MSG_READY                   = 0
MSG_TASK                    = 1
MSG_RESULT                  = 2
MSG_KILL 	            = 3
MSG_DONE                    = 4
MSG_GET_COMMITTER           = 5
MSG_GET_PATH                = 6
MSG_GET_RUNNUM              = 7
MSG_GET_ALIVE               = 8
MSG_SET_COMMITTER           = 9
MSG_NEW_CONNECTION          = 10
MSG_CLOSE_CONNECTION        = 11
MSG_STRING                  = 12
MSG_EMPTY                   = 13
MSG_GET_BINARY              = 14
MSG_GET_HASH                = 15
MSG_GET_STATUS              = 16
MSG_GET_NUM_TASKS           = 17
MSG_SET_MONITOR             = 18
MSG_SET_JOB_MANAGER         = 19
MSG_SET_TASK_MANAGER_ID     = 20
MSG_NEW_VM_TASK_MANAGER     = 21
MSG_SEND_VM_TO_COMMITTER    = 22

# Global Variables
socket_manager = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Methods 

def COMM_cast(value):
	try:
		return int(value)
	except ValueError:
		return int(float(value))

def COMM_read_bytes (sock):
	received_char = '0'
	message_size = ''

	while(received_char != '|'):
		received_char = sock.recv(1)	
		if(len(received_char) == 0):
			return None 

		if(received_char!='|'):
			message_size += received_char

	msg_size = COMM_cast(message_size)
	offset = 0
	msg = ''	

	while(offset < msg_size):
		rcv = sock.recv((msg_size - offset))
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

def COMM_read_int(sock):
	num_str = COMM_read_bytes(sock)
	if num_str is None:
		return None
	else:
		return COMM_cast(num_str)

def COMM_send_int(sock, value):
	msg = str(value)
	return COMM_send_bytes(sock, msg)

def COMM_read_message(sock):
	msg_type = COMM_read_int(sock)

	if msg_type is None:
		return None, None

	msg = COMM_read_bytes(sock)
	return msg, msg_type

def COMM_send_message(msg, msg_type, sock):
	COMM_send_int(sock, msg_type)
	return COMM_send_bytes(sock, msg)

def COMM_connect_to_job_manager(ip, port):
	socket_manager.connect((ip, port))


COMM_connect_to_job_manager('localhost', 8898)
COMM_send_message('', MSG_GET_STATUS, socket_manager)
ret = COMM_read_message(socket_manager)

print ret
time.sleep(5)
