import kivy
from kivy.uix.gridlayout import GridLayout
from kivy.app import App
from kivy.uix.button import Button
import subprocess
import sys

class MyApp(App):
    def build(self):
        layout = GridLayout(cols=6, row_force_default=True, row_default_height=40)
        layout.add_widget(Button(text='IP', size_hint_x=None, width=250))
        layout.add_widget(Button(text='PORT', size_hint_x=None, width=100))
        layout.add_widget(Button(text='TYPE', size_hint_x=None, width=50))
        layout.add_widget(Button(text='ON', size_hint_x=None, width=50))
        layout.add_widget(Button(text='RECEIVED', size_hint_x=None, width=150))
        layout.add_widget(Button(text='COMPLETED', size_hint_x=None, width=150))
        
	wid = [250, 100, 50, 50, 150, 150]
	
	p = subprocess.Popen(["./spitz", "3", "127.0.0.1", "prime.so", "30"],
                     stdout=subprocess.PIPE,
                     #stderr=subprocess.PIPE,
                     cwd="/home/alexandre/Codes/spitz/examples")


	while 1:
		ln = p.stdout.readline()
		if ln.startswith("[STATUS] "):
			
			print ln
			break


	ln = ln[9:]
	lnlist = ln.split(';')
	fnlist = []
	for item in lnlist:
		fnlist.append(item.split('|'))

	fnlist.pop()

	for line in fnlist:
		i = 0
		for value in line:
			layout.add_widget(Button(text=value ,size_hint_x=None, width=wid[i]))
			i = i+1

	return layout

MyApp().run()
