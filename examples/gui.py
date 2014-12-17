import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button
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


''' ----- 
    MonitorData 
    ----- '''

# Represent all the data in the main screen.
class MonitorData:
	# Initialize the class.
	def __init__(self, factor, NodesPerPage):
		self.columns = ['ID', 'IP', 'PORT', 'TYPE', 'ON', 'RECEIVED','COMPLETED']  
		self.rows = []
		self.index = 0
		self.npp = NodesPerPage
		self.wid = [factor*50,factor*250,factor*100,factor*50,factor*50,factor*150,factor*150]
		self.factor = factor
		self.total_rcvd = 0
		self.total_compl = 0
		self.lastIndex= -1 
		self.lastOrder = -1
		self.ln = ""
		self.TotalTasks = 1
		self.total_compl = 0

	# Starts monitor C process.
	def runMonitorProcess(self):
		self.p = subprocess.Popen(["./spitz", "3", "127.0.0.1", "prime.so", "100"],
			     stdout=subprocess.PIPE,
			     stdin=subprocess.PIPE,
			     cwd="/home/alexandre/Codes/spitz/examples")

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
		'''	# Connect, if not connected yet, to SSH and SFTP
		if not hasattr(self, 'ssh'):
			self.ssh = paramiko.SSHClient()
			self.ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
			self.ssh.connect('143.106.16.163', username='USER', password='SENHA') 
			self.ssh.exec_command('mkdir -p ~/spitz')
			self.sftp = self.ssh.open_sftp()

		# Send spitz and libspitz.so to VM node.
		if os.path.isfile('spitz') and os.path.isfile('libspitz.so'):
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

	# Makes the header layout, with the commands.
	def makeHeaderLayout(self, layout):
		layout.clear_widgets()
		btnP = Button(text="Update", size_hint_x=None, width = self.factor*266)
		btnP.bind(on_press = buttonUpdate)

		layout.add_widget(btnP)
		btnV = Button(text="Launch VM", size_hint_x=None, width = self.factor*266)
		btnV.bind(on_press = buttonVM)
		layout.add_widget(btnV)

		btnS = Button(text="Settings", size_hint_x=None, width = self.factor*267)
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

	# Redraw list using current values of nodes.	
	def reDrawList(self):
		self.makeListLayout(self.ListLayout)
		self.makeNavigationLayout(self.NavigationLayout)



''' ----- 
    Handlers of the main screen.
    ----- '''

# Handler of the VM button, will launch an VM task manager.
def buttonVM(instance):
	ip = '191.238.16.92|11006'
	Data.makeCommandLayout(Data.CommandLayout, 'Connecting to VM Task Manager in : ' + str(ip))
	Data.launchVMnode(2, ip)

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
	Screen.layout.clear_widgets()
	Screen.buildSettingsScreen()


def buttonSettingsClose():
	Screen.layout.clear_widgets()
	Screen.buildMainScreen()

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

		self.layout = GridLayout(cols = 1, row_force_default=False, height = 600, width = 800)	
		self.sett = Settings()
		self.sett.on_close = buttonSettingsClose 
		self.settings_json = json.dumps([
		    {'type': 'title',
		     'title': 'example title'},
		    {'type': 'bool',
		     'title': 'A boolean setting',
		     'desc': 'Boolean description text',
		     'section': 'example',
		     'key': 'boolexample'},
		    {'type': 'numeric',
		     'title': 'A numeric setting',
		     'desc': 'Numeric description text',
		     'section': 'example',
		     'key': 'numericexample'},
		    {'type': 'string',
		     'title': 'A string setting',
		     'desc': 'String description text',
		     'section': 'example',
		     'key': 'stringexample'},
		    {'type': 'path',
		     'title': 'A path setting',
		     'desc': 'Path description text',
		     'section': 'example',
		     'key': 'pathexample'}])

	# Build the main screen, with header, list, navigation and command.
	def buildMainScreen(self):
		# Make header layout and add to the main.
		HeaderLayout = GridLayout(cols=3, row_default_height=Data.factor*15)
		Data.makeHeaderLayout(HeaderLayout)
		self.layout.add_widget(HeaderLayout)

		# Make list rows and add it to the main layout 
		Data.ListLayout = GridLayout(cols=len(Data.columns), row_default_height=Data.factor*30, rows=(Data.npp + 1) , size_hint_y=3)
		self.layout.add_widget(Data.ListLayout)

		# Make Navigation Commands and add it to the main layout 
		Data.NavigationLayout = GridLayout(cols=2, row_default_height=Data.factor*15)
		self.layout.add_widget(Data.NavigationLayout)


		# Make Console Layout and add it to the main layout 
		Data.CommandLayout = GridLayout(cols=1, row_default_height=Data.factor*30, row_force_default=True)
		Data.makeCommandLayout(Data.CommandLayout, 'Welcome to SPITZ Monitor')
		self.layout.add_widget(Data.CommandLayout)

		Data.reDrawList()

	# Build the Settings screen.
	def buildSettingsScreen(self):
		# Make header layout and add to the main.
		#HeaderLayout = GridLayout(cols=3, row_default_height=Data.factor*15)

		#config = ConfigParser()
		#config.read('/home/alexandre/.kivy/myconfig.ini')

		#l.add_json_panel('My custom panel', config, os.path.join(os.path.dirname(__file__), 'gui.json'))
		#settings.add_json_panel('Panel Name',self.config,data=settings_json)
		#Data.makeHeaderLayout(HeaderLayout)

		#self.sett.add_json_panel('Panel Name',MyApp.config,data=Screen.settings_json)
		self.layout.add_widget(self.sett)


class MyApp(App):
	# Responsible for building the program.
	def build(self):
		# Start the monitor process.
		#Data.runMonitorProcess()

		# Layout that will design the screen.
		Screen.buildMainScreen()

		return Screen.layout

	# Run when closing. Send a quit message and print the sucess (or not) message.
	def on_stop(self):
		Data.getStatusMessage(0)
		print Data.ln

	def build_config(self, config):
		print 'BUILD CONFIG'
		config.setdefaults('example', {
		     'boolexample' : 'True',
		     'numericexample' : '12',
		     'stringexample' : 'sssssssstring',
		     'pathexample' : '/asdsa/dsad'
		})

		jsondata = Screen.settings_json 
		Screen.sett.add_json_panel('Test application',self.config, data=jsondata)


	def build_settings(self, settings):
		print 'BUILD SETTINGS'
		
# Main part.
if __name__ == "__main__":
	Data = MonitorData(1, 10)
	Screen = ScreenBank() 	

	MyApp().run()
