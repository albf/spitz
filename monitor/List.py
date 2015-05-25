from MonitorData import *
from Config import *

a = Config()
Data = MonitorData(1, 10)

Data.listSpitzVMs(a.subscription_id, a.certificate_path, a.linux_user, a.linux_pass, verbose=False)

