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

import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button
from kivy.uix.togglebutton import ToggleButton
from kivy.uix.textinput import TextInput
from kivy.uix.progressbar import ProgressBar
from kivy.config import Config
from kivy.uix.label import Label
from kivy.uix.settings import Settings
from kivy.config import ConfigParser
from kivy.uix.scrollview import ScrollView 
from operator import itemgetter
import json
import subprocess
import sys
import os
import paramiko
import socket
from datetime import datetime
from azure import *
from azure.servicemanagement import *
from functools import partial
import atexit
from comm import *

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
		self.VMcolumns = ['Name', 'Location', 'Reachable', 'Spitz', 'Action'] 
		self.VMwid = [factor*200, factor*200, factor*100, factor*100, factor*200] 
		self.VMrows = []
		self.VMlastIndex= -1 
		self.VMlastOrder = -1


	# Connect to Azure and get information about all services with SPITZ in their name.
	# Check if it's on and if there is a spitz instance running.
	def connectToCloudProvider(self):
		subscription_id = str(Screen.AppInstance.config.get('example', 'subscription_id'))
		certificate_path = str(Screen.AppInstance.config.get('example', 'certificate_path'))
 		sms = ServiceManagementService(subscription_id, certificate_path)

		try:
			result = sms.list_hosted_services()
			for hosted_service in result:
				print hosted_service.service_name
				if "spitz" in str(hosted_service.service_name).lower() :
					address = str(hosted_service.service_name) + ".cloudapp.net"
					sshs = paramiko.SSHClient()
					sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
					reach = "YES"					
					isThereSpitz = "NO"
	
					try:
						sshs.connect(address,
							 username=str(Screen.AppInstance.config.get('example', 'ssh_login')),
							 password=str(Screen.AppInstance.config.get('example', 'ssh_pass'))) 

						stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
						pid = stdouts.readlines()
						if len(pid) != 0:
							isThereSpitz = "YES"
					except socket.gaierror as e1:
						Screen.makeCommandLayout(Data, "Couldn't find " + str(address) + ".")
						reach = "NO"
					except socket.error as e2:
						Screen.makeCommandLayout(Data, "Connection refused in " + str(address) + ".")
						reach = "NO"
					except paramiko.AuthenticationException as e3:
						Screen.makeCommandLayout(Data, "Wrong credentials for " + str(address) + ".")
						reach = "NO"
					except:
						Screen.makeCommandLayout(Data, "unexpected error connecting to " + str(address) + ".")
						reach = "NO"

					action = "Try Again"
					if (reach == "YES") and (isThereSpitz=="YES"):
						action = "Stop"
					elif (reach == "YES") and (isThereSpitz=="NO"):
						action = "Start"

					self.VMrows.append([hosted_service.service_name, address, reach, isThereSpitz, action])
					print self.VMrows	

			# Debug for running a VM TM locally.
			self.VMrows.append(["Debug", "localhost", 1, 1, "Start"])
			self.IsVMsListed = True	

		except WindowsAzureError as WAE:
			Screen.makeCommandLayout(self, "Couldn't connect with Azure, is your credentials right?") 
		except socket.gaierror as SGE:
			Screen.makeCommandLayout(self, "Problem connecting to Azure, are your internet ok?")
		except ssl.SSLError as SLE:
			Screen.makeCommandLayout(self, "Problem connecting to Azure, are your certificates ok?")

	# Test if an unreachable VM is, now, reachable. Also checks for SPITZ intance if it's on. 
	def VMTryAgain(self, index):
		address = self.VMrows[index][1] 
		print address
		sshs = paramiko.SSHClient()
		sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
		reach = "YES"					
		isThereSpitz = "NO"

		try:
			sshs.connect(address,
				 username=str(Screen.AppInstance.config.get('example', 'ssh_login')),
				 password=str(Screen.AppInstance.config.get('example', 'ssh_pass'))) 

			stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
			pid = stdouts.readlines()
			if len(pid) != 0:
				isThereSpitz = "YES"
		except socket.gaierror as e1:
			Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ".")
			reach = "NO"
		except socket.error as e2:
			Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ".")
			reach = "NO"
		except paramiko.AuthenticationException as e3:
			Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ".")
			reach = "NO"
		except:
			Screen.makeCommandLayout(self, "unexpected error connecting to " + str(address) + ".")
			reach = "NO"

		if(reach=="YES"):
			self.VMrows[index][2] = reach
			self.VMrows[index][3] = isThereSpitz
			if(isThereSpitz == "YES"):
				self.VMrows[index][4] = "Stop"
			else:
				self.VMrows[index][4] = "Start"

		return reach

	# Send a request to the monitor and get the answer.
	def getStatusMessage(self, task):
		COMM_connect_to_job_manager(Screen.AppInstance.config.get('example', 'jm_address'),
						Screen.AppInstance.config.get('example', 'jm_port'))

		self.ln = COMM_get_status('')
		print self.ln

	# Get number of tasks from the Job Manager
	def getNumberOfTasks(self, task):
		COMM_connect_to_job_manager(Screen.AppInstance.config.get('example', 'jm_address'),
						Screen.AppInstance.config.get('example', 'jm_port'))
		
		self.TotalTasks = COMM_get_num_tasks()
		print "Received number of tasks: " + str(self.TotalTasks)

	# Send a request to the monitor to launch the VM present in the provided dns (converted to a ip|port string).
	def launchVMnode(self, task, index):
		reach = "YES"
		address = self.VMrows[index][1] 

		# First Part : Connect to SSH and run SPITZ in the choosed VM
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		if(address != "localhost"):
			try:
				ssh.connect(address,
					username=str(Screen.AppInstance.config.get('example', 'ssh_login')), 
					password=str(Screen.AppInstance.config.get('example', 'ssh_pass'))) 

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
				Screen.makeCommandLayout(self, "Couldn't find " + str(address) + ".")
				reach = "NO"
			except socket.error as e2:
				Screen.makeCommandLayout(self, "Connection refused in " + str(address) + ".")
				reach = "NO"
			except paramiko.AuthenticationException as e3:
				Screen.makeCommandLayout(self, "Wrong credentials for " + str(address) + ".")
				reach = "NO"

		# Second part, send info to the monitor. 
		if(reach == "YES"):
			ret = COMM_connect_to_job_manager(Screen.AppInstance.config.get('example', 'jm_address'),
							Screen.AppInstance.config.get('example', 'jm_port'))
			
			if ret == 0:
				ret = COMM_send_vm_node(str(address))
				if ret == 0:
					Screen.makeCommandLayout(self, "Spitz instance running in " + str(address) + ".")
					return True
				else:
					Screen.makeCommandLayout(self, "Problem sendin vm_node to Job Manager" + str(address) + ".")
			else:
				Screen.makeCommandLayout(self, "Can't connect to Job Manager." + str(address) + ".")
	
		return False	

	# Stop SPITZ instance running in a VM node. 
	def stopVMnode(self, index):
		reach = "YES"
		address = self.VMrows[index][1] 

		# Connect to SSH and stop SPITZ in the choosed VM
		ssh = paramiko.SSHClient()
		ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())

		if(address != "localhost"):
			try:
				ssh.connect(address,
					username=str(Screen.AppInstance.config.get('example', 'ssh_login')), 
					password=str(Screen.AppInstance.config.get('example', 'ssh_pass'))) 

				# Make ssh command to stop SPITZ. 
				ssh.exec_command('pkill -f spitz')

				self.VMrows[index][3] = "NO"
				self.VMrows[index][4] = "Start"
				self.makeCommandLayout(Data.CommandLayout, "Spitz stopped in " + str(address) + ".")
				return True
			
			except socket.gaierror as e1:
				Data.makeCommandLayout(Data.CommandLayout, "Couldn't find " + str(address) + ". Try updating the list.")
				reach = "NO"
			except socket.error as e2:
				Data.makeCommandLayout(Data.CommandLayout, "Connection refused in " + str(address) + ". Try updating the list.")
				reach = "NO"
			except paramiko.AuthenticationException as e3:
				Data.makeCommandLayout(Data.CommandLayout, "Wrong credentials for " + str(address) + ". Try updating the list.")
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
			

''' ----- 
    Handlers of the main screen.
    ----- '''

def checkToggleButton(button):
	if(button.state == 'normal'):
		button.state = 'down' 

# Handler of the VM button, will launch an VM task manager.
def buttonVM(instance):
	checkToggleButton(Screen.btnV)
	Data.index = 0				# Reset the current index.
	if(Data.IsVMsListed == False):
		Data.connectToCloudProvider()
		Screen.makeVMListLayout(Data)
		Screen.makeVMNavigationLayout(Data)

	Screen.buildVMListScreen()

# Handler of the Prev button, return to the previous page.		
def buttonPrev(instance):
	Data.index = Data.index - 1
	Screen.makeListLayout(Data)
	Screen.makeNavigationLayout(Data)	

# Handler of the Next button, go to the next page.		
def buttonNext(instance):
	Data.index = Data.index + 1
	Screen.makeListLayout(Data)
	Screen.makeNavigationLayout(Data)	

# Handler responsible for ordering the list (using one of the columns).
def buttonOrder(instance):
	index = Data.columns.index(instance.text) 
	if(Data.lastIndex == index):
		if(Data.lastOrder == 1):
			Data.lastOrder = -1
			Data.rows = sorted(Data.rows, key=itemgetter(index), reverse = True)
		else:
			Data.rows = sorted(Data.rows, key=itemgetter(index)) 
			Data.lastOrder = 1
	else:
		Data.lastIndex = index	
		Data.lastOrder = 1
		Data.rows = sorted(Data.rows, key=itemgetter(index))

	Screen.makeListLayout(Data)

# Handler responsible for ordering the list (using one of the columns).
def buttonVMOrder(instance):
	index = Data.VMcolumns.index(instance.text) 
	if(Data.VMlastIndex == index):
		if(Data.VMlastOrder == 1):
			Data.VMlastOrder = -1
			Data.VMrows = sorted(Data.VMrows, key=itemgetter(index), reverse = True)
		else:
			Data.VMrows = sorted(Data.VMrows, key=itemgetter(index)) 
			Data.VMlastOrder = 1
	else:
		Data.VMlastIndex = index	
		Data.VMlastOrder = 1
		Data.VMrows = sorted(Data.VMrows, key=itemgetter(index))

	Screen.makeVMListLayout(Data)

# Request the current status and update the list. Redraw after that.
def buttonUpdate(instance):
	Data.getStatusMessage(1)
	Data.getNumberOfTasks(3)
	Data.fillRows(Data.ln)
	Screen.makeCommandLayout(Data, Data.ln) 
	Screen.reDrawList(Data)

def buttonSettings(instance):
	#Screen.layout.clear_widgets()
	Screen.buildSettingsScreen()

def buttonLog(instane):
	checkToggleButton(Screen.btnL)
	Screen.makeLogLayout(Data)
	Screen.buildLogScreen()

def buttonList(instance):
	checkToggleButton(Screen.btnLi)
	Data.index = 0				# Reset the current index.
	Screen.buildListScreen()

def buttonStatistics(instance):
	checkToggleButton(Screen.btnSta)

def buttonVMAction(*args, **kwargs):
	index = args[0]
	action = args[1]
	print index
	print action
	
	if(action == "Try Again"):
		if(Data.VMTryAgain(index) == "YES"):
			Screen.makeVMListLayout(Data)	
	elif(action == "Start"):
		if(Data.launchVMnode(2, index) == True):
			Screen.makeVMListLayout(Data)	
	elif(action == "Stop"):
		if(Data.stopVMnode(index) == True):
			Screen.makeVMListLayout(Data)	
	else:
		Screen.makeCommandLayout(Data.CommandLayout, "Error: Don't know what this comand is, check python code")
	
''' ----- 
    ScreenBank
    ----- '''

# Class that stores the main layout and build the different screens of the program.
class ScreenBank:
	# Initialize the class.
	def __init__(self, Data):
		#Define window size.
		self.factor = Data.factor
		Config.set('graphics', 'width', Data.factor*800) 
		Config.set('graphics', 'height', Data.factor*600)

		# Create main and middle layout
		self.layout = GridLayout(cols = 1, row_force_default=False, height = 600, width = 800)	
		self.MiddleLayout = GridLayout(cols=1, size_hint_y=11)
		self.MiddleScroll = ScrollView(size_hint=(1, None), size=(Data.factor*400, Data.factor*500), bar_width=4)

		self.settings_json = json.dumps([
		    {'type': 'string',
		     'title': 'Job Manager IP',
		     'desc': 'Address of the JM.',
		     'section': 'example',
		     'key': 'jm_address'},
		    {'type': 'numeric',
		     'title': 'Job Manager Port',
		     'desc': 'Port of the JM.',
		     'section': 'example',
		     'key': 'jm_port'},
		    {'type': 'path',
		     'title': 'Library Path',
		     'desc': 'File with user functions.',
		     'section': 'example',
		     'key': 'lib_path'},
		    {'type': 'numeric',
		     'title': 'Number of Tasks',
		     'desc': 'Amount of work to be done.',
		     'section': 'example',
		     'key': 'num_tasks'}])

		self.settings_json2 = json.dumps([
		   {'type': 'string',
		     'title': 'Virtual Machine IP',
		     'desc': 'Address of the Virtual Machine TM.',
		     'section': 'example',
		     'key': 'vm_ip'},
		   {'type': 'string',
		     'title': 'Virtual Machine Port',
		     'desc': 'Port of the Virtual Machine TM.',
		     'section': 'example',
		     'key': 'vm_prt'}, 
		   {'type': 'string',
		     'title': 'SSH Login',
		     'desc': 'Used to log in Virtual Machine TM.',
		     'section': 'example',
		     'key': 'ssh_login'},
		   {'type': 'string',
		     'title': 'SSH Password',
		     'desc': 'Look behind you before typing.',
		     'section': 'example',
		     'key': 'ssh_pass'}])

		self.settings_json3 = json.dumps([
		   {'type': 'bool',
		     'title': 'SSH Port Fowarding',
		     'desc': 'Activate/Deactivate for connecting with Job Manager',
		     'section': 'example',
		     'key': 'ssh_pf_bool'},
		   {'type': 'string',
		     'title': 'SSH Login',
		     'desc': 'Username used in SSH connection.',
		     'section': 'example',
		     'key': 'ssh_pf_login'}, 
		   {'type': 'string',
		     'title': 'SSH Password',
		     'desc': 'Look behind you before typing.',
		     'section': 'example',
		     'key': 'ssh_pf_pass'}])

		self.settings_json4 = json.dumps([
		   {'type': 'string',
		     'title': 'Subscription ID',
		     'desc': 'ID obtained in Azure.',
		     'section': 'example',
		     'key': 'subscription_id'}, 
		   {'type': 'path',
		     'title': 'Certificate Path',
		     'desc': 'Location of the path sent to Azure.',
		     'section': 'example',
		     'key': 'certificate_path'}])

		# Layout that represents the list.
		self.ListLayout = GridLayout(cols=len(Data.columns), row_default_height=Data.factor*30, row_force_default = True, rows=(Data.npp + 1) , size_hint_y=10)
		self.NavigationLayout = GridLayout(cols=2, row_default_height=Data.factor*15)

		# Layout that represents the VMlist..
		self.VMListLayout = GridLayout(cols=len(Data.VMcolumns), row_default_height=Data.factor*30, row_force_default = True, rows=(Data.npp + 1) , size_hint_y=10)
		self.VMNavigationLayout = GridLayout(cols=2, row_default_height=Data.factor*15)

		# Layout that represents the log
		self.LogWidget = TextInput(multiline=True)

		# Layout that represents the main Header
		self.HeaderLayout = GridLayout(cols=7, row_default_height=Data.factor*15)

	# Build the main screen, with header, list, navigation and command.
	def buildMainScreen(self, Data):
		# Make header layout and add to the main.
		self.makeHeaderLayout()
		self.layout.add_widget(self.HeaderLayout)

		# Make list rows and add it to the middle layout 
		self.MiddleLayout.add_widget(self.ListLayout)

		# Build list screen and add Middle Layout to the main layout.
		self.buildListScreen()
		self.layout.add_widget(self.MiddleLayout)

		# Make Console Layout and add it to the main layout 
		self.CommandLayout = GridLayout(cols=1, row_default_height=Data.factor*30, row_force_default=True)
		self.makeCommandLayout(Data, 'Welcome to SPITZ Monitor')
		self.layout.add_widget(self.CommandLayout)

		self.reDrawList(Data)

	# Build the list screen, just updating the middle layout.
	def buildListScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleLayout.add_widget(self.ListLayout)
		self.MiddleLayout.add_widget(self.NavigationLayout)

	# Build the list screen, just updating the middle layout.
	def buildVMListScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleLayout.add_widget(self.VMListLayout)
		self.MiddleLayout.add_widget(self.VMNavigationLayout)

	# Build the log screen, just updating the middle layout.
	def buildLogScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleScroll.clear_widgets()
		self.LogWidget.size_hint_y = None
		self.LogWidget.height = max( (len(self.LogWidget._lines)+1) * self.LogWidget.line_height, self.MiddleScroll.size[1])
		self.MiddleScroll.do_scroll_x = False
		self.MiddleScroll.add_widget(self.LogWidget)
		self.MiddleLayout.add_widget(self.MiddleScroll)

	# Build the Settings screen.
	def buildSettingsScreen(self):
		MyApp.open_settings(self.AppInstance)
		#self.layout.add_widget(self.sett)

	# Makes the layout of the list, adding the columns name, the ordering handlers and the actual page.
	def makeListLayout(self, Data):	
		layout = self.ListLayout
		layout.clear_widgets()
		for col in range(len(Data.columns)):
			btnO = Button(text=Data.columns[col], size_hint_x=None, width=Data.wid[col])
			btnO.bind(on_press=buttonOrder)
			layout.add_widget(btnO)
		
		upper = min(len(Data.rows), (Data.index + 1)*Data.npp)
		#print "upper: "+str(upper)
		for i in range(Data.index*Data.npp, upper):
			for j in range(len(Data.wid)):
				layout.add_widget(Button(text=str(Data.rows[i][j]), size_hint_x=None, width=Data.wid[j]))
		self.ListLayout = layout

	# Makes the buttons to navigate. Add the handler and text if there is any.
	def makeNavigationLayout(self, Data):
		layout = self.NavigationLayout
		layout.clear_widgets()
		if(Data.index > 0):
			btnP = Button(text="Previous", size_hint_x=None, width = Data.factor*400)
			btnP.bind(on_press = buttonPrev)
			layout.add_widget(btnP)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = Data.factor*400))

		if(len(Data.rows)>(Data.index + 1)*Data.npp):
			btnN = Button(text="Next", size_hint_x=None, width = Data.factor*400)
			btnN.bind(on_press = buttonNext)
			layout.add_widget(btnN)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = Data.factor*400))
		self.NavigationLayout = layout

	# Makes the layout of the VM list, adding the columns name, the ordering handlers and the actual page.
	def makeVMListLayout(self, Data):	
		layout = self.VMListLayout
		layout.clear_widgets()
		for col in range(len(Data.VMcolumns)):
			btnO = Button(text=Data.VMcolumns[col], size_hint_x=None, width=Data.VMwid[col])
			btnO.bind(on_press=buttonVMOrder)
			layout.add_widget(btnO)
		
		upper = min(len(Data.VMrows), (Data.index + 1)*Data.npp)
		#print "upper: "+str(upper)
		for i in range(Data.index*Data.npp, upper):
			for j in range(len(Data.VMwid)-1):
				layout.add_widget(Button(text=str(Data.VMrows[i][j]), size_hint_x=None, width=Data.VMwid[j]))

			# Button responsible for VM action, as it has different index and action, calls partial functions.
			btnA = Button(text=str(Data.VMrows[i][len(Data.VMwid)-1]), size_hint_x=None, width=Data.VMwid[-1])
			print i
			btnA.bind(on_press=partial(buttonVMAction, i, btnA.text))
			layout.add_widget(btnA)
		self.VMListLayout = layout

	# Makes the buttons to navigate the VM list. Add the handler and text if there is any.
	def makeVMNavigationLayout(self, Data):
		layout = self.VMNavigationLayout
		layout.clear_widgets()
		if(Data.index > 0):
			btnP = Button(text="Previous", size_hint_x=None, width = Data.factor*400)
			btnP.bind(on_press = VMbuttonPrev)
			layout.add_widget(btnP)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = Data.factor*400))

		if(len(Data.rows)>(Data.index + 1)*Data.npp):
			btnN = Button(text="Next", size_hint_x=None, width = Data.factor*400)
			btnN.bind(on_press = VMbuttonNext)
			layout.add_widget(btnN)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = Data.factor*400))
		self.VMNavigationLayout = layout

	# Makes the header layout, with the commands.
	def makeHeaderLayout(self):
		layout = self.HeaderLayout
		layout.clear_widgets()

		self.btnP = Button(text="Update", size_hint_x=None, width = self.factor*125)
		self.btnP.bind(on_press = buttonUpdate)
		layout.add_widget(self.btnP)

		btnSep = Button(text="", size_hint_x=None, width = self.factor*50)
		layout.add_widget(btnSep)

		self.btnLi = ToggleButton(text="List", group='menu', size_hint_x=None, width = self.factor*125, state='down')
		self.btnLi.bind(on_press = buttonList)
		layout.add_widget(self.btnLi)

		self.btnV = ToggleButton(text="VM Launcher", group='menu', size_hint_x=None, width = self.factor*125)
		self.btnV.bind(on_press = buttonVM)
		layout.add_widget(self.btnV)

		self.btnSta = ToggleButton(text="Statistics", group='menu', size_hint_x=None, width = self.factor*125)
		self.btnSta.bind(on_press = buttonStatistics)
		layout.add_widget(self.btnSta)

		self.btnL = ToggleButton(text="Logs", group='menu', size_hint_x=None, width = self.factor*125)
		self.btnL.bind(on_press = buttonLog)
		layout.add_widget(self.btnL)

		self.btnS = Button(text="Settings", size_hint_x=None, width = self.factor*125)
		self.btnS.bind(on_press = buttonSettings)
		layout.add_widget(self.btnS)

	def makeCommandLayout(self, Data, itext):
		layout = self.CommandLayout
		layout.clear_widgets()
		self.commandWidget = TextInput(multiline=False)
		self.commandWidget.readonly = True	
		self.commandWidget.text = itext

		pb = ProgressBar(max=1000)	
		pb.value = (Data.total_compl*1000)/(int(Data.TotalTasks))

		layout.add_widget(self.commandWidget)
		layout.add_widget(pb)
	
		#Get current time and add the message to the log pile.	
		logmessage = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
		logmessage = logmessage + " >>> " + itext 
		Data.log = Data.log + "\n" + logmessage

	# Redraw list using current values of nodes.	
	def reDrawList(self, Data):
		self.makeListLayout(Data)
		self.makeNavigationLayout(Data)

	def makeLogLayout(self, Data):
		self.LogWidget.readonly = True
		self.LogWidget.text = Data.log



class MyApp(App):
	use_kivy_settings = False

	# Responsible for building the program.
	def build(self):
		# Just build start screen. 
			
		# Layout that will design the screen.
		Screen.buildMainScreen(Data)

		return Screen.layout

	# Run when closing. Send a quit message and print the sucess (or not) message.
	def on_stop(self):
		Data.getStatusMessage(0)
		print Data.ln

	def build_config(self, config):
		print 'BUILD CONFIG'
		self.config.setdefaults('example', {
		     'jm_address' : '127.0.0.1',
		     'jm_port' : '8898',
		     'lib_path' : 'prime.so',
		     'num_tasks' : '100',
		     'vm_ip' : '127.0.0.1',
		     'vm_prt' : '11006',
		     'ssh_login' : 'user',
		     'ssh_pass' : 'pass',
		     'ssh_pf_bool' : True,
		     'ssh_pf_login' : 'user',
		     'ssh_pf_pass' : 'pass',
		     'subscription_id' : 'id',
		     'certificate_path' : '/',
		})

		Screen.AppInstance = self

		#Screen.sett = self.settings
		#Screen.sett.on_close = buttonSettingsClose
		#jsondata = Screen.settings_json 
		#Screen.sett.add_json_panel('SPITZ Virtual Machine',self.config, data=jsondata)
		#Data.config = self.config


	def build_settings(self, settings):
		print 'BUILD SETTINGS'

		jsondata = Screen.settings_json 
		settings.add_json_panel('Spitz Parameters',self.config, data=jsondata)

		jsondata = Screen.settings_json2 
		settings.add_json_panel('Virtual Machine',self.config, data=jsondata)
	
		jsondata = Screen.settings_json3 
		settings.add_json_panel('Port Fowarding',self.config, data=jsondata)

		jsondata = Screen.settings_json4 
		settings.add_json_panel('Cloud Provider',self.config, data=jsondata)
		#self.open_settings()	
		
# Main part.
if __name__ == "__main__":
	Data = MonitorData(1, 10)
	Screen = ScreenBank(Data) 	

	MyApp().run()
