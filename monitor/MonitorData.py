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

from operator import itemgetter
import subprocess
import os
import paramiko
import socket
from azure import *
from azure.servicemanagement import *
import ssl
import time
from Comm import *

''' ----- 
    MonitorData 
    ----- '''

# Represent all the data in the main screen.
class MonitorData:
	# Initialize the class.
	def __init__(self, factor, NodesPerPage):
		self.columns = ['ID', 'IP', 'PORT', 'TYPE', 'ON', 'RECEIVED','COMPLETED']  
		self.rows = []			# Rows representing the status values.
		self.index = 0			# Index of the current page 
		self.npp = NodesPerPage		# Nodes displayed in each status page.
		self.wid = [factor*50,factor*250,factor*100,factor*50,factor*50,factor*150,factor*150]
		self.factor = factor		# Factor of screen size.
		self.total_rcvd = 0
		self.total_compl = 0		# Total completed tasks.
		self.lastIndex= -1 
		self.lastOrder = -1
		self.ln = ""
		self.TotalTasks = 1		# Number of tasks, initiate with 1 to avoid division issues.
		self.total_compl = 0		# Total completed tasks.
		self.log = ""			# Stores all log information

		# VM Variables.
		self.IsVMsListed = False	# Indicate if VMs are listed alredy.
		self.VMcolumns = ['Name', 'Location', 'Status','Action', 'Spitz', 'Action'] 
		self.VMwid = [factor*75, factor*175, factor*175,factor*125, factor*125, factor*125] 
		self.VMrows = []
		self.VMrowsInfo =[]		# stores service names: [service_name, deployment_name, instance_name, azure status]
		self.VMlastIndex= -1 
		self.VMlastOrder = -1
		self.subscription_id = ''
		self.certificate_path = ''

	# Connect to Azure and get information about all services with SPITZ in their name.
	# Check if it's on and if there is a spitz instance running.
	def connectToCloudProvider(self):
		# Get credentials and instanciate SMS
		#subscription_id = str(.Screen.AppInstance.config.get('example', 'subscription_id'))
		#certificate_path = str(Runner.Screen.AppInstance.config.get('example', 'certificate_path'))
 		sms = ServiceManagementService(self.subscription_id, self.certificate_path)

		# May be an update, clean before doing anything.
		if len(self.VMrows) > 0:
			self.VMrows = []
			self.VMrowsInfo = []

		try:
			# Get service list
			result = sms.list_hosted_services()
			for hosted_service in result:
				print hosted_service.service_name
				# Get only services with SPITZ in its name.
				if "spitz" in str(hosted_service.service_name).lower() :
					address = str(hosted_service.service_name) + ".cloudapp.net"
					sshs = paramiko.SSHClient()
					sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
					status = "None"					
					original_status = status
					isThereSpitz = "-"
					isThereInstance = 0
					a_action = "-"
					s_action = "-"
					instance_name = ""

					# Get deployment information.
					d_result = sms.get_deployment_by_slot(hosted_service.service_name,'Production')
					deployment_name = d_result.name

					# Get instance information.
					for instance in d_result.role_instance_list:
						instance_name = instance.instance_name
						isThereInstance = 1
						status = instance.instance_status
						original_status = status

						break

					print "Status: "+status

					# If there is no instance, something is wrong.
					if isThereInstance == 0:
						Runner.Screen.makeCommandLayout(self, "No instance in following service: " + address)
						status = "No Instance"

					# If the VM is ok and running, try to establish a SSH connection.
					elif (status != "StoppedVM") and (status!= "StoppedDeallocated"):
						a_action = "Stop"
						s_action = "Try Again"
						try:
							sshs.connect(address,
								 username=str(Runner.Screen.AppInstance.config.get('example', 'ssh_login')),
								 password=str(Runner.Screen.AppInstance.config.get('example', 'ssh_pass'))) 

							stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
							pid = stdouts.readlines()
							if len(pid) != 0:
								isThereSpitz = "YES"
								s_action = "Restart"
							else:
								isThereSpitz = "NO"
								s_action = "Start"
						except socket.gaierror as e1:
							Runner.Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ".")
							status = "No SSH access."
						except socket.error as e2:
							Runner.Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ".")
							status = "SSH refused."
						except paramiko.AuthenticationException as e3:
							Runner.Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ".")
							status = "SSH denied."
						except:
							Runner.Screen.makeCommandLayout(self, "unexpected error connecting to " + str(address) + ".")
							status = "SSH issue."
					# If it's ok, but down, offer the possibility to start it.
					else:
						a_action = "Start"

					# Append service.
					self.VMrows.append([hosted_service.service_name, address, status, a_action, isThereSpitz, s_action])
					self.VMrowsInfo.append([hosted_service.service_name, deployment_name, instance_name, original_status])

			# Debug for running a VM TM locally.
			self.VMrows.append(["Debug", "127.0.0.1", "Local", "-", "Yes", "Start"])
			self.VMrowsInfo.append(["Debug", "Debug", "Debug", "Debug"])

		except WindowsAzureError as WAE:
			Runner.Screen.makeCommandLayout(self, "Couldn't connect with Azure, is your credentials right?") 
		except socket.gaierror as SGE:
			Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your internet ok?")
		except ssl.SSLError as SLE:
			Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your certificates ok?")

	# Update one or more VMs. Ids must be passed in a list by indexes [index1, index2, index3 ... ].
	# Valid action strings : Start, Stop
	def updateVMs(self, indexes, action):
		services_names = []

		# Mount list of names
		for index in indexes:
			services_names.append(self.VMrows[index][0])

		print "Services_names"
		print services_names

		# Get credentials and instanciate SMS
		#subscription_id = str(Runner.Screen.AppInstance.config.get('example', 'subscription_id'))
		#certificate_path = str(Runner.Screen.AppInstance.config.get('example', 'certificate_path'))
 		sms = ServiceManagementService(self.subscription_id, self.certificate_path)

		# May be an update, clean before doing anything.
		if len(self.VMrows) > 0:
			self.VMrows = []
			self.VMrowsInfo = []

		try:
			# Get service list
			result = sms.list_hosted_services()
			for hosted_service in result:
				print hosted_service.service_name
				if(hosted_service.service_name not in services_names):
					continue

				print "Passed filter"

				address = str(hosted_service.service_name) + ".cloudapp.net"
				instance_name = ""
				isThereInstance = 0

				# Get deployment information.
				d_result = sms.get_deployment_by_slot(hosted_service.service_name,'Production')
				deployment_name = d_result.name

				# Get instance information.
				for instance in d_result.role_instance_list:
					instance_name = instance.instance_name
					isThereInstance = 1
					status = instance.instance_status
					original_status = status
					break

				# If there is no instance, something is wrong.
				if isThereInstance == 0:
					Runner.Screen.makeCommandLayout(self, "No instance in following service: " + address)
				# Start the VM 
				elif action == "Start":
					sms.start_role(hosted_service.service_name, d_result.name, instance_name)
				# Or stop the VM 
				elif action == "Stop":
					sms.shutdown_role(hosted_service.service_name, d_result.name, instance_name, post_shutdown_action='StoppedDeallocated')

		except WindowsAzureError as WAE:
			Runner.Screen.makeCommandLayout(self, "Couldn't connect with Azure, is your credentials right?") 
			return False
		except socket.gaierror as SGE:
			Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your internet ok?")
			return False
		except ssl.SSLError as SLE:
			Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your certificates ok?")
			return False

	# Test if an unreachable VM is, now, reachable (using SSH). Also checks for SPITZ intance if it's on. 
	# OLD:str(Runner.Screen.AppInstance.config.get('example', 'ssh_login')),
	# OLD:str(Runner.Screen.AppInstance.config.get('example', 'ssh_pass'))) 
	def VMTryAgain(self, index, ssh_login, ssh_pass):
		address = self.VMrows[index][1] 
		print address
		sshs = paramiko.SSHClient()
		sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		status = self.VMrowsInfo[3]
		isThereSpitz = "NO"

		try:
			s_action = "Try Again"
			sshs.connect(address,
				 username=ssh_login,
				 password=ssh_pass)

			stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
			pid = stdouts.readlines()
			if len(pid) != 0:
				isThereSpitz = "YES"
				s_action = "Restart"
			else:
				isThereSpitz = "NO"
				s_action = "Start"

		except socket.gaierror as e1:
			Runner.Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ".")
			status = "No SSH access."
		except socket.error as e2:
			Runner.Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ".")
			status = "SSH refused."
		except paramiko.AuthenticationException as e3:
			Runner.Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ".")
			status = "SSH denied."
		except:
			Runner.Screen.makeCommandLayout(self, "unexpected error connecting to " + str(address) + ".")
			status = "SSH issue."

		self.VMrows[i][2] = status
		self.VMrows[i][4] = isThereSpitz 
		self.VMrows[i][5] = s_action

		if isThereSpitz == "YES":
			return True
		else:
			return False 

	# Send a request to the monitor and get the answer.
	# OLD: Runner.Screen.AppInstance.config.get('example', 'jm_address'),
	# OLD: Runner.Screen.AppInstance.config.get('example', 'jm_port')) >= 0:
	def getStatusMessage(self, task, jm_address, jm_port):
		if COMM_connect_to_job_manager(jm_addres, jm_port) >= 0:

			self.ln = COMM_get_status('')
			if self.ln != None:
				return 0
		print self.ln
		return -1 

	# Get number of tasks from the Job Manager
	# OLD:Runner.Screen.AppInstance.config.get('example', 'jm_address'),
	# OLD:Runner.Screen.AppInstance.config.get('example', 'jm_port'))
	def getNumberOfTasks(self, task, jm_address, jm_port):
		ret = COMM_connect_to_job_manager(jm_address, jm_port)
		
		if ret >= 0 :
			self.TotalTasks = COMM_get_num_tasks()
			print "Received number of tasks: " + str(self.TotalTasks)
			return 0
		else:
			print "Connection problem, couldn't get the number of tasks." 
			return -1

	# Send a request to the monitor to launch the VM present in the provided dns (converted to a ip|port string).
	# Kill : Kill existing if true ; Start : start new process if true ; Send : send data to JM if true.
	# Note, invalid combination : true, false, true.
	# OLD: str(Runner.Screen.AppInstance.config.get('example', 'ssh_login')), 
	# OLD: str(Runner.Screen.AppInstance.config.get('example', 'ssh_pass'))) 
	def launchVMnode(self, index, kill, update, start, send, username, password):
		if (kill == True) and (start == False) and (send == True):
			Runner.Screen.makeCommandLayout(self, "Invalid combination in MonitorData.launchVMnode.")
			return False

		reach = "YES"
		address = self.VMrows[index][1] 

		# First Part : Connect to SSH and run SPITZ in the choosed VM
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		if(address != "localhost") and (address != "127.0.0.1"):
			try:
				ssh.connect(address, username=username,	password=password)

				# Send spitz and libspitz.so to VM node.
				'''if os.path.isfile('spitz') and os.path.isfile('libspitz.so'):
					self.sftp.put('spitz', 'spitz/spitz') 
					self.ssh.exec_command('chmod 555 ~/spitz/spitz')
					self.sftp.put('libspitz.so', 'spitz/libspitz.so') 
				else:
					raise Exception('Spitz file(s) missing') '''

				if kill == True:
					stdin,stdout,stderr = ssh.exec_command('killall spitz')

				if update == True:
					stdin,stdout,stderr = ssh.exec_command('cd spitz; git push')

				if start == True:
					stdin,stdout,stderr = ssh.exec_command('cd spitz/examples; make test4')
					self.VMrows[index][4] = "YES"
					self.VMrows[index][5] = "Restart"
			
			except socket.gaierror as e1:
				Runner.Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ".")
				reach = "NO"
			except socket.error as e2:
				Runner.Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ".")
				reach = "NO"
			except paramiko.AuthenticationException as e3:
				Runner.Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ".")
				reach = "NO"

		# Second part, send info to the monitor. 
		if (reach == "YES") and (send == True):
			ret = COMM_connect_to_job_manager(Runner.Screen.AppInstance.config.get('example', 'jm_address'),
							Runner.Screen.AppInstance.config.get('example', 'jm_port'))
			
			if ret == 0:
				ret = COMM_send_vm_node(str(address), 22)
				if ret == 0:
					Runner.Screen.makeCommandLayout(self, "Spitz instance running in " + str(address) + ".")
					return True
				else:
					Runner.Screen.makeCommandLayout(self, "Problem sendin vm_node to Job Manager" + str(address) + ".")
			else:
				Runner.Screen.makeCommandLayout(self, "Can't connect to Job Manager." + str(address) + ".")
	
		return False	

	# Using the last status received, parse the list string.
	def fillRows(self,status):
		self.total_rcvd = 0
		self.total_compl = 0
		del self.rows[:]		

		#status = status[13:]
		lnlist = status.split(';')
		for item in lnlist:
			self.rows.append(item.split('|'))

			if (len(self.rows[-1]) >= 6):
				self.total_rcvd += int(self.rows[-1][5])
				self.total_compl += int(self.rows[-1][6])
				for indexT in range(len(self.columns)):
					if(indexT!=1):
						self.rows[-1][indexT] = int(self.rows[-1][indexT])
					
			else:
				self.rows.pop()

		print self.rows

		# i = 5 total_rcvd
		# i = 6 completed
		#percentage = (self.total_compl / float(100))*100


	# Check if it's on and if there is a spitz instance running.
	def startAllNodes(self, subscription_id, certificate_path, command, ssh_user, ssh_pass, jm_address, jm_port):
		# Get credentials and instanciate SMS
		sms = ServiceManagementService(subscription_id, certificate_path)

		try:
			# Get service list
			result = sms.list_hosted_services()
			for hosted_service in result:
				print hosted_service.service_name
				# Get only services with SPITZ in its name.
				if "spitz" in str(hosted_service.service_name).lower() :
					address = str(hosted_service.service_name) + ".cloudapp.net"
					status = "None"
					instance_name = ""

					# Get deployment information.
					d_result = sms.get_deployment_by_slot(hosted_service.service_name,'Production')
					deployment_name = d_result.name

					# Get instance information.
					for instance in d_result.role_instance_list:
						instance_name = instance.instance_name
						status = instance.instance_status
						offset = int(instance_name[5:])
						port = offset+22

						# If the VM is ok and running, try to establish a SSH connection.
						if (status != "StoppedVM") and (status!= "StoppedDeallocated"):
							sshs = paramiko.SSHClient()
							sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
							try:
								print "name: " + str(instance_name)
								print "address: " + str(address)
								print "port: " + str(port)
								print "ip: " + str(socket.gethostbyname(str(address)))
								print "port: " + str(PORT_VM+offset)
								sshs.connect(address,
									username=ssh_user,
									password=ssh_pass, 
									port = port) 

							 	
								sshs.exec_command('pkill spitz')
								sshs.exec_command('rm -r spitz')
								sshs.exec_command('mkdir spitz')
								sftp = sshs.open_sftp()
								#sftp.get('spitz/spitz/spitz', 'spitz_vm')
								#sftp.get('spitz/spitz/libspitz.so', 'libspitz_vm.so')
								#sftp.get('spitz/examples/prime.so', 'prime_vm.so')
								sftp.put('spitz_vm', 'spitz/spitz') 
								sshs.exec_command('chmod 555 ~/spitz/spitz')
								sftp.put('libspitz_vm.so', 'spitz/libspitz.so') 
								sftp.put('prime_vm.so', 'spitz/prime.so') 
								#sshs.exec_command('screen')
								sshs.exec_command('export LD_LIBRARY_PATH=$PWD/spitz && ' + str(command))
								#print 'stdout: ' + str(stdout.readlines())
								#print 'stderr: ' + str(stderr.readlines())
								print command
								#sstdin, stdout, stderr = sshs.exec_command(command)
								#print 'stdout: ' + str(stdout.readlines())
								#print 'stderr: ' + str(stderr.readlines())
								#print 'sleeping'
								#time.sleep(5)
								
								ret = COMM_connect_to_job_manager(jm_address, jm_port)

								#ret = COMM_send_vm_node(socket.gethostbyname(str(address)), PORT_VM+offset)
								if ret == 0:
									ret = COMM_send_vm_node(socket.gethostbyname(str(address)), PORT_VM+offset)
									#ret = COMM_send_vm_node('127.0.0.1', PORT_VM+offset)

									if ret == 0:
										print("Spitz instance running in " + str(address) + "|" + str(PORT_VM+offset) + ".")
									else:
										print("Problem sendin vm_node to Job Manager" + str(address) + "|" + str(port) + ".")
								else:
									print("Can't connect to Job Manager.")
						

							except socket.gaierror as e1:
								print ("Couldn't find " + str(address) + ".")
								status = "No SSH access."
							except socket.error as e2:
								print("Connection refused in " + str(address) + ".")
								status = "SSH refused."
							except paramiko.AuthenticationException as e3:
								print("Wrong credentials for " + str(address) + ".")
								status = "SSH denied."
							#except Exception as e3:
							#	print("unexpected error (" + str(e3) + ") connecting to " + str(address) + ".")
							#	status = "SSH issue."

		except WindowsAzureError as WAE:
			#Runner.Screen.makeCommandLayout(self, "Couldn't connect with Azure, is your credentials right?") 
			print "Couldn't connect with Azure, is your credentials right?"
		except socket.gaierror as SGE:
			#Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your internet ok?")
			print "Problem connecting to Azure, are your internet ok?"
		except ssl.SSLError as SLE:
			#Runner.Screen.makeCommandLayout(self, "Problem connecting to Azure, are your certificates ok?")
			print "Problem connecting to Azure, are your certificates ok?"



	def createNode(self, sms, service_name, vm_name, blob_url, image, offset, linux_user, linux_pass, is_first, wait=10):
		# Create linux config
		print 'Linux user: ' + str(linux_user)
		print 'Linux pass: ' + str(linux_pass)
		linux_config = LinuxConfigurationSet(vm_name, linux_user, linux_pass, True)
		linux_config.disable_ssh_password_authentication = False

		# Create linux endpoints
		endpoint_config = ConfigurationSet()
		endpoint_config.configuration_set_type = 'NetworkConfiguration'
		#endpoint1 = ConfigurationSetInputEndpoint(name='JM', protocol='tcp', port=str(8898+offset), local_port='8898', 
		#											load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
		#endpoint2 = ConfigurationSetInputEndpoint(name='CM', protocol='tcp', port=str(10007+offset), local_port='10007', 
		#											load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
		#endpoint1 = ConfigurationSetInputEndpoint(name='SPITZ', protocol='tcp', port=str(PORT_VM+offset), local_port='10007', 
		#											load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
		print "port: " + str(PORT_VM+offset)
		print "local_port: " + str(PORT_VM)
		endpoint1 = ConfigurationSetInputEndpoint(name='SPITZ', protocol='tcp', port=str(PORT_VM+offset), local_port=str(PORT_VM),
													load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
		endpoint2 = ConfigurationSetInputEndpoint(name='SSH', protocol='tcp', port=str(22+offset), local_port='22', 
													load_balanced_endpoint_set_name=None, enable_direct_server_return=False)

		endpoint_config.input_endpoints.input_endpoints.append(endpoint1)
		endpoint_config.input_endpoints.input_endpoints.append(endpoint2)
		#endpoint_config.input_endpoints.input_endpoints.append(endpoint3)

		# Create HD
		label = service_name + '-' + vm_name
		media_link = blob_url + label + '.vhd'
		os_hd = OSVirtualHardDisk(image, media_link, disk_label= label +'.vhd', disk_name = label +'.name' )
		print vars(os_hd)

		if(is_first == True):
			done = False
			try_t = 0
			while (done == False):
				try:
					print "Try: " + str(try_t)
					sms.create_virtual_machine_deployment(service_name=service_name, deployment_name=service_name,
											  deployment_slot='production', label=vm_name, role_name=vm_name,
											  system_config=linux_config, network_config=endpoint_config,
											  os_virtual_hard_disk=os_hd, role_size='Small')
					done=True
				except Exception as exc:
					try_t += 1
					time.sleep(wait)
					print 'error, exc: ' + str(exc)
		else:
			done = False
			try_t = 0
			while (done == False):
				try:
					print "Try: " + str(try_t)
					sms.add_role(service_name=service_name, deployment_name=service_name, role_name = vm_name, 
									system_config=linux_config, os_virtual_hard_disk = os_hd, 
									network_config=endpoint_config, role_size='Small')
					done = True
				except Exception as exc:
					try_t += 1
					time.sleep(wait)
					print 'error, exc: ' + str(exc)

	def allocateNodes(self, total_cloud_services, vms_per_cloud, subscription_id, 
				certificate_path, windows_blob_url, image, location, 
				linux_user, linux_pass, VMHeader = "spitz"):

		sms = ServiceManagementService(subscription_id, certificate_path)

		# Stores names of services.
		ServiceList = []
		VMList = []

		# Get current services.
		try:
			result = sms.list_hosted_services()
		except WindowsAzureError as x:
			print 'Except: Could not list hosted services'
			return


		# Check current services and create deployments when needed.
		service_counter = 0
		for hosted_service in result:
			print('Service name: ' + hosted_service.service_name)

			if "spitz" in str(hosted_service.service_name).lower():
				if(service_counter < total_cloud_services):
					service_counter += 1
					ServiceList.append(str(hosted_service.service_name))
					remove_list = []
					try:
						d_result = sms.get_deployment_by_slot(hosted_service.service_name, 'Production')
						vm_counter = 0
						print ( 'Deployment Name:'  + d_result.name )
						for instance in d_result.role_instance_list :
							print ( 'Instance name:'  + instance.instance_name )
							if(vm_counter >= vms_per_cloud):
								print 'Not necessary role detected'
								remove_list.append(instance.instance_name)
								# Don't need it, remove.
							else:
								vm_counter += 1

						if(len(remove_list)>0):
							print 'Deleting extra roles'
							sms.delete_role_instances(hosted_service.service_name, d_result.name, remove_list)

						VMList.append(vm_counter)
					# Deployment doesn't exist
					except WindowsAzureMissingResourceError:
						print 'Creating Deployment'
						self.createNode(sms, str(hosted_service.service_name), VMHeader + str(0), 
							windows_blob_url, image, 0, linux_user, linux_pass, True)
						VMList.append(1)

				# Service not necessary, destroy it.
				else:
					print 'Deleting extra service'
					sms.delete_hosted_service(hosted_service.service_name)

			print('')
		# Verify if any service should be created and create them.
		while(service_counter < total_cloud_services):
			nextName = VMHeader + str(service_counter)
			print 'Creating service and deployment: ' + str(nextName)
			sms.create_hosted_service(service_name=nextName, label=nextName, location=location)
			self.createNode(sms, nextName, VMHeader + str(0), windows_blob_url, image, 
				0, linux_user, linux_pass, True)

			ServiceList.append(nextName)
			VMList.append(1)
			service_counter += 1

		# Create the remaining VMs
		for vm_num in range(vms_per_cloud):
			for sv_num in range(len(ServiceList)):
				if (VMList[sv_num] <= vm_num):
					print 'Creating Node: ' + str(ServiceList[sv_num]) + ' - VMNUM: ' + str(vm_num)
					self.createNode(sms, ServiceList[sv_num], VMHeader + str(vm_num), windows_blob_url, 
						image, vm_num, linux_user, linux_pass, False)

