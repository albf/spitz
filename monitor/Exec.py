from MonitorData import *
from Config import *

a = Config()
Data = MonitorData(1, 10)
Data.startAllNodes(a.subscription_id, a.certificate_path, a.command, a.ssh_user, a.ssh_pass, a.jm_address, a.jm_port)

