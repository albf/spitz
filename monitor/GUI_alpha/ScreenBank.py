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

import abc

class ScreenBank(object):
	__metaclass__ = abc.ABCMeta	

	@abc.abstractmethod
	def __init__(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildMainScreen(self, Data):
		"""Build Main Screen of monitor"""
		return

	@abc.abstractmethod
	def buildListScreen(self):
		"""Build List Screen of monitor"""
		return

	@abc.abstractmethod
	def buildVMListScreen(self):
		"""Build VM List Screen of monitor"""
		return

	@abc.abstractmethod
	def buildLogScreen(self):
		"""Build Log Screen of monitor"""
		return

	@abc.abstractmethod
	def buildSettingsScreen(self):
		"""Build Settings Screen of monitor"""
		return

	@abc.abstractmethod
	def makeListLayout(self, Data):
		"""Build List Layout (internal parts) of monitor"""
		return

	@abc.abstractmethod
	def makeNavigationLayout(self, Data):
		"""Build Navigation Layout (internal parts) of monitor"""
		return

	@abc.abstractmethod
	def makeVMListLayout(self, Data):
		"""Build VM List Layout (internal parts) of monitor"""
		return

	@abc.abstractmethod
	def makeVMNavigationLayout(self, Data):
		"""Build VM Navigation Layout (internal parts) of monitor"""
		return


	@abc.abstractmethod
	def makeHeaderLayout(self):
		"""Build VM Header Layout (internal parts) of monitor"""
		return


	@abc.abstractmethod
	def makeCommandLayout(self, Data, itext):
		"""Build VM Command Layout (internal parts) of monitor"""
		return


	@abc.abstractmethod
	def reDrawList(self, Data):
		"""Redraw list, restart index and try to fill it again"""
		return

	@abc.abstractmethod
	def makeLogLayout(self, Data):
		"""Build Log Layout (internal parts) of monitor"""
		return

	@abc.abstractmethod
	def screenChange(self, Data):
		"""Transition function, called everytime the screen is changed"""
		return

