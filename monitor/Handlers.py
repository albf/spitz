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
import sys
import socket
from datetime import datetime
import atexit
import Runner

''' ----- 
    Handlers of the main screen.
    ----- '''

# Handler of the VM button, will launch an VM task manager.
def buttonVM(instance):
	Runner.Screen.screenChange(Runner.Screen.btnV, "VMLauncher")
	Runner.Data.index = 0				# Reset the current index.
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
	if Runner.Screen.ScreenName == "List":
		if (Runner.Data.getStatusMessage(1) >= 0) and (Runner.Data.getNumberOfTasks(3) >= 0):
			Runner.Data.fillRows(Runner.Data.ln)
			Runner.Screen.makeCommandLayout(Runner.Data, Runner.Data.ln) 
			Runner.Screen.reDrawList(Runner.Data)
		else:
			Runner.Screen.makeCommandLayout(Runner.Data, "Problem getting status information from Job Manager. Is the address and port correct?")
	elif Runner.Screen.ScreenName == "VMLauncher":
		Runner.Data.IsVMsListed = False
		buttonVM(None) 

def buttonSettings(instance):
	#Screen.layout.clear_widgets()
	Runner.Screen.buildSettingsScreen()

def buttonLog(instane):
	Runner.Screen.screenChange(Screen.btnL, "Log")
	Runner.Screen.makeLogLayout(Runner.Data)
	Runner.Screen.buildLogScreen()

def buttonList(instance):
	Runner.Screen.screenChange(Screen.btnLi, "List")
	Runner.Data.index = 0				# Reset the current index.
	Runner.Screen.buildListScreen()
	buttonUpdate(None)

def buttonStatistics(instance):
	Runner.Screen.screenChange(Screen.btnSta, "Statistics")

def buttonVMAction(*args, **kwargs):
	index = args[0]
	action = args[1]
	print index
	print action
	
	if(action == "Try Again"):
		if(Runner.Data.VMTryAgain(index) == "YES"):
			Runner.Screen.makeVMListLayout(Runner.Data)	
	elif(action == "Start"):
		if(Runner.Data.launchVMnode(2, index) == True):
			Runner.Screen.makeVMListLayout(Runner.Data)	
	elif(action == "Stop"):
		if(Runner.Data.stopVMnode(index) == True):
			Runner.Screen.makeVMListLayout(Runner.Data)	
	else:
		Runner.Screen.makeCommandLayout(Runner.Data, "Error: Don't know what this comand is, check python code")
	
