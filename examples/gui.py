import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button
from kivy.config import Config
from operator import itemgetter
import subprocess
import sys


class MonitorData:
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

	def runMonitorProcess(self):
		self.p = subprocess.Popen(["./spitz", "3", "127.0.0.1", "prime.so", "100"],
			     stdout=subprocess.PIPE,
			     stdin=subprocess.PIPE,
			     cwd="/home/alexandre/Codes/spitz/examples")

	def getStatusMessage(self, task):
		
		self.p.stdin.write(str(task)+"\n")
		while 1:
			self.ln = self.p.stdout.readline()
			print "MESSAGE: "
			if self.ln.startswith("[STATUS"):
				return 

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

		percentage = (self.total_compl / float(100))*100


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

	def makeHeaderLayout(self, layout):
		layout.clear_widgets()
		btnP = Button(text="Update", size_hint_x=None, width = self.factor*400)
		btnP.bind(on_press = buttonUpdate)
		layout.add_widget(btnP)

	def test(self,layout,index):
		layout.add_widget(Button(text='Hello 1'))
		layout.add_widget(Button(text='World 1'))
		
def buttonPrev(instance):
	Data.index = Data.index - 1
	Data.makeListLayout(Data.ListLayout)
	Data.makeNavigationLayout(Data.NavigationLayout)	

def buttonNext(instance):
	Data.index = Data.index + 1
	Data.makeListLayout(Data.ListLayout)
	Data.makeNavigationLayout(Data.NavigationLayout)	

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

def buttonUpdate(instance):
	Data.getStatusMessage(1)
	Data.fillRows(Data.ln)
	reDrawList()

# Redraw list using current values of nodes.	
def reDrawList():
	Data.makeListLayout(Data.ListLayout)
	Data.makeNavigationLayout(Data.NavigationLayout)

class MyApp(App):
	def build(self):
		Data.runMonitorProcess()

		#Define window size.
		Config.set('graphics', 'width', Data.factor*800) 
		Config.set('graphics', 'height', Data.factor*600)

		# Layout that will design the screen.
		layout = GridLayout(cols = 1, row_force_default=False, height = 600, width = 800)	

		# Make header layout and add to the main.
		HeaderLayout = GridLayout(cols=1, row_default_height=Data.factor*15)
		Data.makeHeaderLayout(HeaderLayout)
		layout.add_widget(HeaderLayout)

		# Make list rows and add it to the main layout 
		Data.ListLayout = GridLayout(cols=len(Data.columns), row_default_height=Data.factor*30, rows=(Data.npp + 1) , size_hint_y=3)
		layout.add_widget(Data.ListLayout)

		# Make Navigation Commands and add it to the main layout 
		Data.NavigationLayout = GridLayout(cols=2, row_default_height=Data.factor*15)
		layout.add_widget(Data.NavigationLayout)

		reDrawList()

		# Make Navigation Commands and add it to the main layout 
		#NavigationLayout2 = GridLayout(cols=2, row_default_height=Data.factor*15)
		#Data.makeNavigationLayout(NavigationLayout2)
		#layout.add_widget(NavigationLayout2)

		return layout

	def on_stop(self):
		Data.getStatusMessage(0)
		print Data.ln

Data = MonitorData(1, 10)
MyApp().run()
