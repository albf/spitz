from MonitorData import *
from Config import *

a = Config()
Data = MonitorData(1, 10)

Data.allocateNodes(8, 4, a.subscription_id, a.certificate_path, a.windows_blob_url, a.image, a.location, a.linux_user, a.linux_pass, a.VMHeader)
