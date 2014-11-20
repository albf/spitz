import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button
from kivy.config import Config
import subprocess
import sys

Config.set('graphics', 'width', '950')
Config.set('graphics', 'height', '600')

class MonitorData:
	def __init__(self):
		self.columns = ['ID', 'IP', 'PORT', 'TYPE', 'ON', 'RECEIVED','COMPLETED']  
		self.rows = []

	def getStatus(self):
		self.p = subprocess.Popen(["./spitz", "3", "127.0.0.1", "prime.so", "100"],
			     stdout=subprocess.PIPE,
			     #stderr=subprocess.PIPE,
			     cwd="/home/alexandre/Codes/spitz/examples")

		while 1:
			ln = self.p.stdout.readline()
			if ln.startswith("[STATUS] "):
				
				print ln
				break

		self.total_rcvd = 0
		self.total_compl = 0
		
		ln = ln[9:]
		lnlist = ln.split(';')
		for item in lnlist:
			self.rows.append(item.split('|'))

		self.rows.pop()
		print self.rows
		

# i = 5 total_rcvd
# i = 6 completed

		percentage = (self.total_compl / float(100))*100


		


class MyApp(App):
    def build(self):
        layout = GridLayout(cols=7, row_force_default=True, row_default_height=30)
        layout.add_widget(Button(text='ID', size_hint_x=None, width=50))
        layout.add_widget(Button(text='IP', size_hint_x=None, width=250))
        layout.add_widget(Button(text='PORT', size_hint_x=None, width=100))
        layout.add_widget(Button(text='TYPE', size_hint_x=None, width=50))
        layout.add_widget(Button(text='ON', size_hint_x=None, width=50))
        layout.add_widget(Button(text='RECEIVED', size_hint_x=None, width=150))
        layout.add_widget(Button(text='COMPLETED', size_hint_x=None, width=150))
        
	wid = [50,250, 100, 50, 50, 150, 150]
	

	layout.add_widget(Button(text=value ,size_hint_x=None, width=wid[i]))



	layout.add_widget(Button(text='-'+ str(total_compl), size_hint_x=None, width=50))
        layout.add_widget(Button(text='Completed: '+ str(total_compl), size_hint_x=None, width=200))
        layout.add_widget(Button(text='Percentage: ' + str(percentage), size_hint_x=None, width=250))

	return layout

#MyApp().run()
Data = MonitorData()
Data.getStatus()
