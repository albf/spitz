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

		# Layout that represents the list.
		self.ListLayout = GridLayout(cols=len(self.columns), row_default_height=factor*30, row_force_default = True, rows=(self.npp + 1) , size_hint_y=10)
		self.NavigationLayout = GridLayout(cols=2, row_default_height=factor*15)

		# Layout that represents the VMlist..
		self.VMListLayout = GridLayout(cols=len(self.VMcolumns), row_default_height=factor*30, row_force_default = True, rows=(self.npp + 1) , size_hint_y=10)
		self.VMNavigationLayout = GridLayout(cols=2, row_default_height=factor*15)

		# Layout that represents the log
		self.LogWidget = TextInput(multiline=True)

	# Starts monitor C process.
	def runMonitorProcess(self):
		lib_path = str(Screen.AppInstance.config.get('example', 'lib_path'))
		jm_ip = str(Screen.AppInstance.config.get('example', 'jm_address'))
		num_tasks = str(Screen.AppInstance.config.get('example', 'num_tasks'))

		# Run a spitz instance as a subprocess.
		self.p = subprocess.Popen(["./spitz", "3", jm_ip, lib_path, num_tasks],
			     stdout=subprocess.PIPE,
			     stdin=subprocess.PIPE,
			     cwd="/home/alexandre/Codes/spitz/examples")

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
						Data.makeCommandLayout(Data.CommandLayout, "Couldn't find " + str(address) + ".")
						reach = "NO"
					except socket.error as e2:
						Data.makeCommandLayout(Data.CommandLayout, "Connection refused in " + str(address) + ".")
						reach = "NO"
					except paramiko.AuthenticationException as e3:
						Data.makeCommandLayout(Data.CommandLayout, "Wrong credentials for " + str(address) + ".")
						reach = "NO"
					except:
						Data.makeCommandLayout(Data.CommandLayout, "unexpected error connecting to " + str(address) + ".")
						reach = "NO"



					self.VMrows.append([hosted_service.service_name, address, reach, isThereSpitz, "NOTHING"])
					print self.VMrows	

			self.IsVMsListed = True	

		except WindowsAzureError as WAE:
			Data.makeCommandLayout(Data.CommandLayout, "Couldn't connect with Azure, is your credentials right?") 

	# Send a request to the monitor and get the answer.
	def getStatusMessage(self, task):
		
		self.p.stdin.write(str(task)+"\n")
		while 1:
			self.ln = self.p.stdout.readline()
			print "MESSAGE: " + str(self.ln)
			if self.ln.startswith("[STATUS"):
				return 

	# Get number of tasks from the Job Manager
	def getNumberOfTasks(self, task):
		
		self.p.stdin.write(str(task)+"\n")
		while 1:
			line = self.p.stdout.readline()
			print "MESSAGE: " + str(line)
			if line.startswith("[STATUS"):
				self.TotalTasks = int(line[13:])
				#print self.TotalTasks	
				return 


	# Send a request to the monitor to launch the VM present in the provided ip|port string.
	def launchVMnode(self, task, ip):
		# Connect, if not connected yet, to SSH and SFTP
		if not hasattr(self, 'ssh'):
			self.ssh = paramiko.SSHClient()
			self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
			self.ssh.connect(str(Screen.AppInstance.config.get('example', 'vm_ip')),
						username=str(Screen.AppInstance.config.get('example', 'ssh_login')), 
						password=str(Screen.AppInstance.config.get('example', 'ssh_pass'))) 
			#self.ssh.exec_command('mkdir -p ~/spitz')
			#self.sftp = self.ssh.open_sftp()

		#self.ssh.exec_command('cd spitz/examples')
		#command = 'LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$$PWD:$$PWD/../spitz PATH=$$PATH:$$PWD/../spitz \ '
		#command = command + './spitz 4 127.0.0.1 ' + str(Screen.AppInstance.config.get('example', 'lib_path')) + ' '
		#command = command + str(Screen.AppInstance.config.get('example', 'num_tasks'))	
		#print command

		stdin,stdout,stderr = self.ssh.exec_command('cd spitz/examples; make test4')
		#print 'LAUNCH VM NODE STDOUT:'
		#print stdout.readlines()
		#print stderr.readlines()
		#print 'END'

		# Send spitz and libspitz.so to VM node.
		'''if os.path.isfile('spitz') and os.path.isfile('libspitz.so'):
			self.sftp.put('spitz', 'spitz/spitz') 
			self.ssh.exec_command('chmod 555 ~/spitz/spitz')
			self.sftp.put('libspitz.so', 'spitz/libspitz.so') 
		else:
			raise Exception('Spitz file(s) missing') '''

		
		self.p.stdin.write(str(task)+"\n")
		self.p.stdin.write(ip+"\n")
		while 1:
			self.ln = self.p.stdout.readline()
			print "MESSAGE: " + str(self.ln)
			if self.ln.startswith("[STATUS"):
				return 


	# Using the last status received, parse the list string.
	def fillRows(self,status):
		self.total_rcvd = 0
		self.total_compl = 0
		del self.rows[:]		

		status = status[13:]
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


	# Makes the layout of the list, adding the columns name, the ordering handlers and the actual page.
	def makeListLayout(self, layout):	
		layout.clear_widgets()
		for col in range(len(self.columns)):
			btnO = Button(text=self.columns[col], size_hint_x=None, width=self.wid[col])
			btnO.bind(on_press=buttonOrder)
			layout.add_widget(btnO)
		
		upper = min(len(self.rows), (self.index + 1)*self.npp)
		#print "upper: "+str(upper)
		for i in range(self.index*self.npp, upper):
			for j in range(len(self.wid)):
				layout.add_widget(Button(text=str(self.rows[i][j]), size_hint_x=None, width=self.wid[j]))
		self.ListLayout = layout

	# Makes the buttons to navigate. Add the handler and text if there is any.
	def makeNavigationLayout(self, layout):
		layout.clear_widgets()
		if(self.index > 0):
			btnP = Button(text="Previous", size_hint_x=None, width = self.factor*400)
			btnP.bind(on_press = buttonPrev)
			layout.add_widget(btnP)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = self.factor*400))

		if(len(self.rows)>(self.index + 1)*self.npp):
			btnN = Button(text="Next", size_hint_x=None, width = self.factor*400)
			btnN.bind(on_press = buttonNext)
			layout.add_widget(btnN)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = self.factor*400))
		self.NavigationLayout = layout

	# Makes the layout of the VM list, adding the columns name, the ordering handlers and the actual page.
	def makeVMListLayout(self, layout):	
		layout.clear_widgets()
		for col in range(len(self.VMcolumns)):
			btnO = Button(text=self.VMcolumns[col], size_hint_x=None, width=self.VMwid[col])
			btnO.bind(on_press=buttonOrder)
			layout.add_widget(btnO)
		
		upper = min(len(self.VMrows), (self.index + 1)*self.npp)
		#print "upper: "+str(upper)
		for i in range(self.index*self.npp, upper):
			for j in range(len(self.VMwid)):
				layout.add_widget(Button(text=str(self.VMrows[i][j]), size_hint_x=None, width=self.VMwid[j]))
		self.VMListLayout = layout

	# Makes the buttons to navigate the VM list. Add the handler and text if there is any.
	def makeVMNavigationLayout(self, layout):
		layout.clear_widgets()
		if(self.index > 0):
			btnP = Button(text="Previous", size_hint_x=None, width = self.factor*400)
			btnP.bind(on_press = VMbuttonPrev)
			layout.add_widget(btnP)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = self.factor*400))

		if(len(self.rows)>(self.index + 1)*self.npp):
			btnN = Button(text="Next", size_hint_x=None, width = self.factor*400)
			btnN.bind(on_press = VMbuttonNext)
			layout.add_widget(btnN)
		else:
			layout.add_widget(Button(text="", size_hint_x=None, width = self.factor*400))
		self.VMNavigationLayout = layout

	# Makes the header layout, with the commands.
	def makeHeaderLayout(self, layout):
		layout.clear_widgets()

		btnP = Button(text="Update", size_hint_x=None, width = self.factor*125)
		btnP.bind(on_press = buttonUpdate)
		layout.add_widget(btnP)

		btnSep = Button(text="", size_hint_x=None, width = self.factor*50)
		layout.add_widget(btnSep)

		btnLi = ToggleButton(text="List", group='menu', size_hint_x=None, width = self.factor*125, state='down')
		btnLi.bind(on_press = buttonList)
		layout.add_widget(btnLi)

		btnV = ToggleButton(text="VM Launcher", group='menu', size_hint_x=None, width = self.factor*125)
		btnV.bind(on_press = buttonVM)
		layout.add_widget(btnV)

		btnS = ToggleButton(text="Statistics", group='menu', size_hint_x=None, width = self.factor*125)
		#btnS.bind(on_press = buttonS)
		layout.add_widget(btnS)

		btnL = ToggleButton(text="Logs", group='menu', size_hint_x=None, width = self.factor*125)
		btnL.bind(on_press = buttonLog)
		layout.add_widget(btnL)

		btnS = Button(text="Settings", size_hint_x=None, width = self.factor*125)
		btnS.bind(on_press = buttonSettings)
		layout.add_widget(btnS)

	def makeCommandLayout(self, layout, itext):
		layout.clear_widgets()
		self.commandWidget = TextInput(multiline=False)
		self.commandWidget.readonly = True	
		self.commandWidget.text = itext

		pb = ProgressBar(max=1000)	
		pb.value = (self.total_compl*1000)/(int(self.TotalTasks))

		layout.add_widget(self.commandWidget)
		layout.add_widget(pb)
	
		#Get current time and add the message to the log pile.	
		logmessage = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
		logmessage = logmessage + " >>> " + itext 
		self.log = self.log + "\n" + logmessage

	# Redraw list using current values of nodes.	
	def reDrawList(self):
		self.makeListLayout(self.ListLayout)
		self.makeNavigationLayout(self.NavigationLayout)


	def makeLogLayout(self):
		self.LogWidget.readonly = True
		self.LogWidget.text = self.log
			

''' ----- 
    Handlers of the main screen.
    ----- '''

# Handler of the VM button, will launch an VM task manager.
def buttonVM(instance):
	Data.index = 0				# Reset the current index.
	if(Data.IsVMsListed == False):
		Data.connectToCloudProvider()
		Data.makeVMListLayout(Data.VMListLayout)
		Data.makeVMNavigationLayout(Data.VMNavigationLayout)

	Screen.buildVMListScreen()

# Handler of the Prev button, return to the previous page.		
def buttonPrev(instance):
	Data.index = Data.index - 1
	Data.makeListLayout(Data.ListLayout)
	Data.makeNavigationLayout(Data.NavigationLayout)	

# Handler of the Next button, go to the next page.		
def buttonNext(instance):
	Data.index = Data.index + 1
	Data.makeListLayout(Data.ListLayout)
	Data.makeNavigationLayout(Data.NavigationLayout)	

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

	Data.makeListLayout(Data.ListLayout)

# Request the current status and update the list. Redraw after that.
def buttonUpdate(instance):
	Data.getStatusMessage(1)
	Data.getNumberOfTasks(3)
	Data.fillRows(Data.ln)
	Data.makeCommandLayout(Data.CommandLayout, Data.ln) 
	Data.reDrawList()

def buttonSettings(instance):
	#Screen.layout.clear_widgets()
	Screen.buildSettingsScreen()

def buttonLog(instane):
	Data.makeLogLayout()
	Screen.buildLogScreen()

def buttonList(instance):
	Data.index = 0				# Reset the current index.
	Screen.buildListScreen()


''' ----- 
    ScreenBank
    ----- '''

# Class that stores the main layout and build the different screens of the program.
class ScreenBank:
	# Initialize the class.
	def __init__(self):
		#Define window size.
		Config.set('graphics', 'width', Data.factor*800) 
		Config.set('graphics', 'height', Data.factor*600)

		# Create main and middle layout
		self.layout = GridLayout(cols = 1, row_force_default=False, height = 600, width = 800)	
		self.MiddleLayout = GridLayout(cols=1, size_hint_y=11)

		self.settings_json = json.dumps([
		    {'type': 'string',
		     'title': 'Job Manager IP',
		     'desc': 'Address of the JM.',
		     'section': 'example',
		     'key': 'jm_address'},
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

	# Build the main screen, with header, list, navigation and command.
	def buildMainScreen(self):
		# Make header layout and add to the main.
		HeaderLayout = GridLayout(cols=7, row_default_height=Data.factor*15)
		Data.makeHeaderLayout(HeaderLayout)
		self.layout.add_widget(HeaderLayout)

		# Make list rows and add it to the middle layout 
		self.MiddleLayout.add_widget(Data.ListLayout)

		# Build list screen and add Middle Layout to the main layout.
		self.buildListScreen()
		self.layout.add_widget(self.MiddleLayout)

		# Make Console Layout and add it to the main layout 
		Data.CommandLayout = GridLayout(cols=1, row_default_height=Data.factor*30, row_force_default=True)
		Data.makeCommandLayout(Data.CommandLayout, 'Welcome to SPITZ Monitor')
		self.layout.add_widget(Data.CommandLayout)

		Data.reDrawList()

	# Build the list screen, just updating the middle layout.
	def buildListScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleLayout.add_widget(Data.ListLayout)
		self.MiddleLayout.add_widget(Data.NavigationLayout)

	# Build the list screen, just updating the middle layout.
	def buildVMListScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleLayout.add_widget(Data.VMListLayout)
		self.MiddleLayout.add_widget(Data.VMNavigationLayout)

	# Build the log screen, justu pdating the middle layout.
	def buildLogScreen(self):
		self.MiddleLayout.clear_widgets()
		self.MiddleLayout.add_widget(Data.LogWidget)

	# Build the Settings screen.
	def buildSettingsScreen(self):
		MyApp.open_settings(self.AppInstance)
		#self.layout.add_widget(self.sett)


class MyApp(App):
	use_kivy_settings = False

	# Responsible for building the program.
	def build(self):
		# Start the monitor process.
		Data.runMonitorProcess()
		#Data.connectToCloudProvider()

		# Layout that will design the screen.
		Screen.buildMainScreen()

		return Screen.layout

	# Run when closing. Send a quit message and print the sucess (or not) message.
	def on_stop(self):
		Data.getStatusMessage(0)
		print Data.ln

	def build_config(self, config):
		print 'BUILD CONFIG'
		self.config.setdefaults('example', {
		     'jm_address' : '127.0.0.1',
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
	Screen = ScreenBank() 	

	MyApp().run()
