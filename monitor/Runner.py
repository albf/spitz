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
from kivy.app import App
import json

# Global Variables
Data = None 
Screen = None

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
		

