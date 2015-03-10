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
from Comm import *
import Runner

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


	# Connect to Azure and get information about all services with SPITZ in their name.
	# Check if it's on and if there is a spitz instance running.
	def connectToCloudProvider(self):
		# Get credentials and instanciate SMS
		subscription_id = str(Runner.Screen.AppInstance.config.get('example', 'subscription_id'))
		certificate_path = str(Runner.Screen.AppInstance.config.get('example', 'certificate_path'))
 		sms = ServiceManagementService(subscription_id, certificate_path)

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
			self.IsVMsListed = True	

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
		subscription_id = str(Runner.Screen.AppInstance.config.get('example', 'subscription_id'))
		certificate_path = str(Runner.Screen.AppInstance.config.get('example', 'certificate_path'))
 		sms = ServiceManagementService(subscription_id, certificate_path)

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
	def VMTryAgain(self, index):
		address = self.VMrows[index][1] 
		print address
		sshs = paramiko.SSHClient()
		sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		status = self.VMrowsInfo[3]
		isThereSpitz = "NO"

		try:
			s_action = "Try Again"
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

		self.VMrows[i][2] = status
		self.VMrows[i][4] = isThereSpitz 
		self.VMrows[i][5] = s_action

		if isThereSpitz == "YES":
			return True
		else:
			return False 

	# Send a request to the monitor and get the answer.
	def getStatusMessage(self, task):
		if COMM_connect_to_job_manager(Runner.Screen.AppInstance.config.get('example', 'jm_address'),
							Runner.Screen.AppInstance.config.get('example', 'jm_port')) >= 0:

			self.ln = COMM_get_status('')
			if self.ln != None:
				return 0
		print self.ln
		return -1 

	# Get number of tasks from the Job Manager
	def getNumberOfTasks(self, task):
		ret = COMM_connect_to_job_manager(Runner.Screen.AppInstance.config.get('example', 'jm_address'),
						Runner.Screen.AppInstance.config.get('example', 'jm_port'))
		
		if ret >= 0 :
			ret = self.TotalTasks = COMM_get_num_tasks()
		if ret >= 0 :
			print "Received number of tasks: " + str(self.TotalTasks)
		else:
			Runner.Screen.makeCommandLayout(self, "Connection problem, couldn't get the number of tasks.") 

	# Send a request to the monitor to launch the VM present in the provided dns (converted to a ip|port string).
	def launchVMnode(self, task, index):
		reach = "YES"
		address = self.VMrows[index][1] 

		# First Part : Connect to SSH and run SPITZ in the choosed VM
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		if(address != "localhost") and (address != "127.0.0.1"):
			try:
				ssh.connect(address,
					username=str(Runner.Screen.AppInstance.config.get('example', 'ssh_login')), 
					password=str(Runner.Screen.AppInstance.config.get('example', 'ssh_pass'))) 

				# Send spitz and libspitz.so to VM node.
				'''if os.path.isfile('spitz') and os.path.isfile('libspitz.so'):
					self.sftp.put('spitz', 'spitz/spitz') 
					self.ssh.exec_command('chmod 555 ~/spitz/spitz')
					self.sftp.put('libspitz.so', 'spitz/libspitz.so') 
				else:
					raise Exception('Spitz file(s) missing') '''

				stdin,stdout,stderr = ssh.exec_command('cd spitz/examples; make test4')
				self.VMrows[index][3] = "YES"
				self.VMrows[index][4] = "Stop"
			
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
		if(reach == "YES"):
			ret = COMM_connect_to_job_manager(Runner.Screen.AppInstance.config.get('example', 'jm_address'),
							Runner.Screen.AppInstance.config.get('example', 'jm_port'))
			
			if ret == 0:
				ret = COMM_send_vm_node(str(address))
				if ret == 0:
					Runner.Screen.makeCommandLayout(self, "Spitz instance running in " + str(address) + ".")
					return True
				else:
					Runner.Screen.makeCommandLayout(self, "Problem sendin vm_node to Job Manager" + str(address) + ".")
			else:
				Runner.Screen.makeCommandLayout(self, "Can't connect to Job Manager." + str(address) + ".")
	
		return False	

	# Stop SPITZ instance running in a VM node. 
	def stopVMnode(self, index):
		reach = "YES"
		address = self.VMrows[index][1] 

		# Connect to SSH and stop SPITZ in the choosed VM
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		if(address != "localhost") and (address != "127.0.0.1"):
			try:
				ssh.connect(address,
					username=str(Runner.Screen.AppInstance.config.get('example', 'ssh_login')), 
					password=str(Runner.Screen.AppInstance.config.get('example', 'ssh_pass'))) 

				# Make ssh command to stop SPITZ. 
				ssh.exec_command('pkill -f spitz')

				self.VMrows[index][3] = "NO"
				self.VMrows[index][4] = "Start"
				self.makeCommandLayout(self, "Spitz stopped in " + str(address) + ".")
				return True
			
			except socket.gaierror as e1:
				Runner.Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ". Try updating the list.")
				reach = "NO"
			except socket.error as e2:
				Runner.Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ". Try updating the list.")
				reach = "NO"
			except paramiko.AuthenticationException as e3:
				Runner.Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ". Try updating the list.")
				reach = "NO"

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
		
