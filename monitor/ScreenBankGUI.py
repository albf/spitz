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
from kivy.uix.button import Button
from kivy.uix.togglebutton import ToggleButton
from kivy.uix.textinput import TextInput
from kivy.uix.progressbar import ProgressBar
from kivy.config import Config
from kivy.uix.label import Label
from kivy.uix.settings import Settings
from kivy.config import ConfigParser
from kivy.uix.scrollview import ScrollView 
from kivy.app import App
from kivy.clock import Clock
from operator import itemgetter
import json
from functools import partial
from ScreenBank import *
from Handlers import *
import Runner

# Handlers
from operator import itemgetter
from datetime import datetime

# For worker/threading part.
import threading
import time

class FIFO:
	def __init__(self):
		# Semaphore
		self.pendent_tasks = threading.Semaphore(0)
		self.tasks_in_FIFO = 0
		# Mutex
		self.task_FIFO_mutex = threading.Lock()
		self.task_FIFO = []
		self.task_end = False

	# Methods to manage the FIFO 

	# Add task to Worker FIFO
	def addTask(self,task):
		self.task_FIFO_mutex.acquire()	# Take mutex of FIFO
		print task
		self.task_FIFO.append(task)	# Append task
		self.tasks_in_FIFO += 1		# Update task counter
		self.task_FIFO_mutex.release()	# Release mutex
		print 'task appended.'
		self.pendent_tasks.release()	# Warn worker

	def popTask(self):
		self.task_FIFO_mutex.acquire()
		task = self.task_FIFO.pop(0)		# Pop first task available
		self.tasks_in_FIFO -=1			# Update task counter
		self.task_FIFO_mutex.release()
		return task

	# Worker : Run task passed by FIFO
	def runOneTask(self,task):
		for i in range(task[0]):	# Run the specified number
			if(self.task_end == True):
				return
			task[1](*task[2+i])	# Call the function passed with the arguments


''' ----- 
    ScreenBankGUI
    ----- '''

# Class that stores the main layout and build the different screens of the program.
class ScreenBankGUI(ScreenBank):
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

		# Name of current screen
		self.ScreenName = "List"

		# Task FIFOs
		self.workerFIFO = FIFO()
		self.mainFIFO = FIFO()

		#Extra Thread
		self.WorkerThread = threading.Thread(target=self.worker)
		self.WorkerThread.start()

		# Set consume as callback function
		Clock.schedule_interval(self.consume, 0.1)

	# Worker function.
	def worker(self):
		print 'worker'
		while(self.workerFIFO.task_end == False):		# Check if it's over.
			self.workerFIFO.pendent_tasks.acquire()		# Wait for tasks
			task = self.workerFIFO.popTask()		# Pop task
			self.workerFIFO.runOneTask(task)		# Run the task

		print 'Worker exited.'
		return

	# Function to kill/remove worker from tasks FIFO and exit.
	def quitWorker(self):
		self.workerFIFO.task_end = True				# Mark as ended.
		self.workerFIFO.task_FIFO_mutex.acquire()		# Add empty task
		self.workerFIFO.task_FIFO.append([1, None, []])
		self.workerFIFO.task_FIFO_mutex.release()
		self.workerFIFO.pendent_tasks.release()			# Release, it may be stopped

	def consume(self, *args):
		print 'consume'
		while (self.mainFIFO.tasks_in_FIFO > 0): 
			task = self.mainFIFO.popTask()
			self.mainFIFO.runOneTask(task)

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
		Runner.MyApp.open_settings(self.AppInstance)
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
			# Create buttons. Actions buttons has actions.
			for j in range(len(Data.VMwid)):
				if j == len(Data.VMwid)-1:
					# Button responsible for VM action, as it has different index and action, calls partial functions.
					btnA = Button(text=str(Data.VMrows[i][len(Data.VMwid)-1]), size_hint_x=None, width=Data.VMwid[-1])
					btnA.bind(on_press=partial(buttonSpitzAction, i, btnA.text))
					layout.add_widget(btnA)

				elif j == len(Data.VMwid)-3:
					# Button responsible for VM action, as it has different index and action, calls partial functions.
					btnA = Button(text=str(Data.VMrows[i][len(Data.VMwid)-3]), size_hint_x=None, width=Data.VMwid[-3])
					btnA.bind(on_press=partial(buttonAzureAction, i, btnA.text))
					layout.add_widget(btnA)

				else:
					layout.add_widget(Button(text=str(Data.VMrows[i][j]), size_hint_x=None, width=Data.VMwid[j]))


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

		self.btnL = ToggleButton(text="Log", group='menu', size_hint_x=None, width = self.factor*125)
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

	def screenChange(self, value, name):
		self.ScreenName = name
		if(value.state == 'normal'):
			value.state = 'down' 


''' ----- 
    Handlers of the main screen.
    ----- '''

# Handler of the VM button, will launch an VM task manager.
def buttonVM(instance):
        Runner.Screen.screenChange(Runner.Screen.btnV, "VMLauncher")
        Runner.Data.index = 0                           # Reset the current index.
        if(Runner.Data.IsVMsListed == False):
                Runner.Data.connectToCloudProvider()
                Runner.Screen.makeVMListLayout(Runner.Data)
                if(Runner.Data.IsVMsListed == True):
                        Runner.Screen.makeVMNavigationLayout(Runner.Data)

        Runner.Screen.buildVMListScreen()

# Handler of the Prev button, return to the previous page.              
def buttonPrev(instance):
        Runner.Data.index = Runner.Data.index - 1 
        Runner.Screen.makeListLayout(Runner.Data)
        Runner.Screen.makeNavigationLayout(Runner.Data)

# Handler of the Next button, go to the next page.              
def buttonNext(instance):
        Runner.Data.index = Runner.Data.index + 1
        Runner.Screen.makeListLayout(Runner.Data)
        Runner.Screen.makeNavigationLayout(Runner.Data)

# Handler responsible for ordering the list (using one of the columns).
def buttonOrder(instance):
        index = Runner.Data.columns.index(instance.text)
        if(Runner.Data.lastIndex == index):
                if(Runner.Data.lastOrder == 1):
                        Runner.Data.lastOrder = -1
                        Runner.Data.rows = sorted(Runner.Data.rows, key=itemgetter(index), reverse = True)
                else:
                        Runner.Data.rows = sorted(Runner.Data.rows, key=itemgetter(index))
                        Runner.Data.lastOrder = 1
        else:
                Runner.Data.lastIndex = index
                Runner.Data.lastOrder = 1
                Runner.Data.rows = sorted(Runner.Data.rows, key=itemgetter(index))

        Runner.Screen.makeListLayout(Runner.Data)

# Handler responsible for ordering the list (using one of the columns).
def buttonVMOrder(instance):
        index = Runner.Data.VMcolumns.index(instance.text)
        if(Runner.Data.VMlastIndex == index):
                if(Runner.Data.VMlastOrder == 1):
                        Runner.Data.VMlastOrder = -1
                        Runner.Data.VMrows = sorted(Runner.Data.VMrows, key=itemgetter(index), reverse = True)
                else:
                        Runner.Data.VMrows = sorted(Runner.Data.VMrows, key=itemgetter(index))
                        Runner.Data.VMlastOrder = 1
        else:
                Runner.Data.VMlastIndex = index
                Runner.Data.VMlastOrder = 1
                Runner.Data.VMrows = sorted(Runner.Data.VMrows, key=itemgetter(index))

        Runner.Screen.makeVMListLayout(Runner.Data)

# Request the current status and update the list. Redraw after that.
def buttonUpdate(instance):
	Runner.Screen.workerFIFO.addTask([1, WorkerUpdate, [None]])

def WorkerUpdate(instance):
	print 'Update Function'
        if Runner.Screen.ScreenName == "List":
                if (Runner.Data.getStatusMessage(1) >= 0) and (Runner.Data.getNumberOfTasks(3) >= 0):
                        Runner.Data.fillRows(Runner.Data.ln)
			Runner.Screen.mainFIFO.addTask([1, getattr(Runner.Screen, 'makeCommandLayout'), [Runner.Data, Runner.Data.ln]])
			Runner.Screen.mainFIFO.addTask([1, getattr(Runner.Screen, 'reDrawList'), [Runner.Data]])
                else:
                        Runner.Screen.makeCommandLayout(Runner.Data, "Problem getting status information from Job Manager. Is the address and port correct?")
        elif Runner.Screen.ScreenName == "VMLauncher":
                Runner.Data.IsVMsListed = False
                buttonVM(None)

def Update(instance):
	print 'Update Function'
        if Runner.Screen.ScreenName == "List":
                if (Runner.Data.getStatusMessage(1) >= 0) and (Runner.Data.getNumberOfTasks(3) >= 0):
                        Runner.Data.fillRows(Runner.Data.ln)
                        #Runner.Screen.makeCommandLayout(Runner.Data, Runner.Data.ln)
                        #Runner.Screen.reDrawList(Runner.Data)
			Clock.schedule_once(Runner.Screen.makeCommandLayout, 0)
                else:
                        Runner.Screen.makeCommandLayout(Runner.Data, "Problem getting status information from Job Manager. Is the address and port correct?")
        elif Runner.Screen.ScreenName == "VMLauncher":
                Runner.Data.IsVMsListed = False
                buttonVM(None)



def buttonSettings(instance):
        #Screen.layout.clear_widgets()
        Runner.Screen.buildSettingsScreen()

def buttonLog(instane):
        Runner.Screen.screenChange(Runner.Screen.btnL, "Log")
        Runner.Screen.makeLogLayout(Runner.Data)
        Runner.Screen.buildLogScreen()

def buttonList(instance):
        Runner.Screen.screenChange(Runner.Screen.btnLi, "List")
        Runner.Data.index = 0                           # Reset the current index.
        Runner.Screen.buildListScreen()
        buttonUpdate(None)

def buttonStatistics(instance):
        Runner.Screen.screenChange(Screen.btnSta, "Statistics")

def buttonSpitzAction(*args, **kwargs):
        # Debug
        index = args[0]
        action = args[1]
        print index
        print action

        if(action == "Restart"):
                if(Runner.Data.launchVMnode(index, True, False, True, True) == True):
                        Runner.Screen.makeVMListLayout(Runner.Data)
        elif(action == "Start"):
                if(Runner.Data.launchVMnode(index, False, False, True, True) == True):
                        Runner.Screen.makeVMListLayout(Runner.Data)
        else:
                Runner.Screen.makeCommandLayout(Runner.Data, "Error: Don't know what this comand is, check python code")

def buttonAzureAction(*args, **kwargs):
        # Debug
        index = args[0]
        action = args[1]
        print index
        print action

        if(action == "Start"):
                if(Runner.Data.updateVMs([index], "Start") == True):
                        # If could start the node, update the list and the screen.
                        Runner.Data.connectToCloudProvider()
                        Runner.Screen.makeVMListLayout(Runner.Data)
        elif(action == "Stop"):
                if(Runner.Data.updateVMs([index], "Stop") == True):
                        Runner.Data.connectToCloudProvider()
                        Runner.Screen.makeVMListLayout(Runner.Data)
        else:
                Runner.Screen.makeCommandLayout(Runner.Data, "Error: Don't know what this comand is, check python code")

