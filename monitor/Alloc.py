from MonitorData import *
from Config import *

a = Config()
Data = MonitorData(1, 10)

Data.allocateNodes(0, 0, a.windows_blob_url, a.image, a.location, a.linux_user, a.linux_pass, a.VMHeader)

