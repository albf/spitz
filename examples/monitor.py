#client example
import socket
import time
import ctypes

client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect(('localhost', 8898))

#def COMM_send_message(string, int_type, dest_socket):
	


time.sleep(5)
client_socket.close()
