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
from operator import itemgetter
import json
from functools import partial
from ScreenBank import *
from Handlers import *
import Runner
	
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
					print i
					btnA.bind(on_press=partial(buttonSpitzAction, i, btnA.text))
					layout.add_widget(btnA)

				elif j == len(Data.VMwid)-3:
					# Button responsible for VM action, as it has different index and action, calls partial functions.
					btnA = Button(text=str(Data.VMrows[i][len(Data.VMwid)-3]), size_hint_x=None, width=Data.VMwid[-3])
					print i
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

