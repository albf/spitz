'''
 * Copyright 2014 Alexandre Luiz Brisighello Filho <albf.unicamp@gmail.com>
 *
 * This file is part of spitz.
 *
 * spitz is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * spitz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with spitz.  If not, see <http://www.gnu.org/licenses/>.
'''

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
COMM_my_rank = None


# Communication Methods 

# Try to convert string to int.
def COMM_cast(value):
	try:
		return int(value)
	except ValueError:
		return int(float(value))

# Read unknown number of bytes from socket provided.
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
		
# Send msg to socket provided.
def COMM_send_bytes(sock, msg):
	sock.sendall((str(len(msg)) + '|'))
	if(len(msg)>0):
		sock.sendall(msg)
	return 0

# Read int from socket. Convert before returning.
def COMM_read_int(sock):
	num_str = COMM_read_bytes(sock)
	if num_str is None:
		return None
	else:
		return COMM_cast(num_str)

# Sent int, convert to string and use send_bytes.
def COMM_send_int(sock, value):
	msg = str(value)
	return COMM_send_bytes(sock, msg)

# Read mesage from socket. Return tuple with message and its kind.
def COMM_read_message(sock):
	msg_type = COMM_read_int(sock)

	if msg_type is None:
		return None, None

	msg = COMM_read_bytes(sock)
	return msg, msg_type

# Send message using socket provided. Both send and read are compatible with c-code.
def COMM_send_message(msg, msg_type, sock):
	COMM_send_int(sock, msg_type)
	return COMM_send_bytes(sock, msg)

# Connect to job_manager and received rank. Register as monitor.
def COMM_connect_to_job_manager(ip, port):
	socket_manager.connect((ip, port))			# Connect to Job Manager
	COMM_my_rank = COMM_read_int(socket_manager)		# Receive rank
	COMM_send_message('', MSG_SET_MONITOR, socket_manager)	# Set as a Monitor
	return 0

# Send VM identification to Job Manager.
def COMM_send_vm_node(ip, port):
	return COMM_send_message(ip+'|'+port, MSG_NEW_VM_TASK_MANAGER, socket_manager)

# Get status string. Used to fill the table of status.
def COMM_get_status(request):
	COMM_send_message(str(request), MSG_GET_STATUS, socket_manager)
	msg, msg_type = COMM_read_message(socket_manager)
	return msg[:-1]

# Return number of tasks in current run.
def COMM_get_num_tasks():
	COMM_send_message('', MSG_GET_NUM_TASKS, socket_manager)
	msg, msg_type = COMM_read_message(socket_manager)
	#return COMM_cast(msg)
	return msg


# Examples of use.

#COMM_connect_to_job_manager('localhost', 8898)
#print COMM_get_status('')
#print COMM_get_num_tasks() 
