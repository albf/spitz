import abc

class ScreenBank(object):
	__metaclass__ = abc.ABCMeta	

	@abc.abstractmethod
	def __init__(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildMainScreen(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildListScreen(self):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildVMListScreen(self):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildLogScreen(self):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def buildSettingsScreen(self):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def makeListLayout(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def makeNavigationLayout(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def makeVMListLayout(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def makeVMNavigationLayout(self, Data):
		"""Init, will create the screen bank"""
		return


	@abc.abstractmethod
	def makeHeaderLayout(self):
		"""Init, will create the screen bank"""
		return


	@abc.abstractmethod
	def makeCommandLayout(self, Data, itext):
		"""Init, will create the screen bank"""
		return


	@abc.abstractmethod
	def reDrawList(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def makeLogLayout(self, Data):
		"""Init, will create the screen bank"""
		return

	@abc.abstractmethod
	def screenChange(self, Data):
		"""Init, will create the screen bank"""
		return

