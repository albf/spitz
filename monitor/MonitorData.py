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

from operator import itemgetter
import subprocess
import os
import paramiko
import socket
from azure import *
from azure.servicemanagement import *
import ssl
import time
from Comm import *
import sys

''' ----- 
    MonitorData 
    ----- '''

# Represent all the data in the main screen.
class MonitorData:
    # Initialize the class.
    def __init__(self, factor, NodesPerPage):
        self.columns = ['ID', 'IP', 'PORT', 'TYPE', 'ON', 'RECEIVED','COMPLETED']  
        self.rows = []          # Rows representing the status values.
        self.index = 0          # Index of the current page 
        self.npp = NodesPerPage     # Nodes displayed in each status page.
        self.factor = factor        # Factor of screen size.
        self.total_rcvd = 0
        self.total_compl = 0        # Total completed tasks.
        self.lastIndex= -1 
        self.lastOrder = -1
        self.ln = ""
        self.TotalTasks = 1     # Number of tasks, initiate with 1 to avoid division issues.
        self.total_compl = 0        # Total completed tasks.
        self.log = ""           # Stores all log information

        # VM Variables.
        self.IsVMsListed = False    # Indicate if VMs are listed alredy.
        self.VMcolumns = ['Service', 'Location','SSH Port', 'Status', 'Spitz', 'SPITZ Port']
        self.VMrows = []
        self.VMrowsInfo =[]     # stores service names: [service_name, deployment_name, instance_name, azure status]
        self.VMlastIndex= -1 
        self.VMlastOrder = -1
        self.subscription_id = ''
        self.certificate_path = ''

    # Connect to Azure and get information about all services with SPITZ in their name.
    # Check if it's on and if there is a spitz instance running.
    def listSpitzVMs(self, subscription_id, certificate_path, ssh_login, ssh_pass, verbose=False):
        # Get credentials and instanciate SMS
        #subscription_id = str(.Screen.AppInstance.config.get('example', 'subscription_id'))
        #certificate_path = str(Runner.Screen.AppInstance.config.get('example', 'certificate_path'))
        sms = ServiceManagementService(subscription_id, certificate_path)

        # May be an update, clean before doing anything.
        if len(self.VMrows) > 0:
            self.VMrows = []
            self.VMrowsInfo = []

        try:
            # Get service list
            result = sms.list_hosted_services()
            for hosted_service in result:
                if(verbose):
                    print hosted_service.service_name
                # Get only services with SPITZ in its name.
                if "spitz" in str(hosted_service.service_name).lower() :
                    address = str(hosted_service.service_name) + ".cloudapp.net"
                    sshs = paramiko.SSHClient()
                    sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
                    status = "None"                 
                    original_status = status
                    isThereSpitz = "-"
                    isThereInstance = 0

                    # Get deployment information.
                    d_result = sms.get_deployment_by_slot(hosted_service.service_name,'Production')
                    deployment_name = d_result.name

                    # Get instance information.
                    for instance in d_result.role_instance_list:
                        instance_name = instance.instance_name
                        isThereInstance = 1
                        status = instance.instance_status
                        original_status = status

                        offset = int(instance_name[5:])
                        port = 22+offset
                        spitz_port = PORT_VM + offset

                        if(verbose):
                            print "Status: "+status

                        # If the VM is ok and running, try to establish a SSH connection.
                        if (status != "StoppedVM") and (status!= "StoppedDeallocated"):
                            try:
                                sshs.connect(address, username=str(ssh_login), password=str(ssh_pass), port=port) 
                                stdins, stdouts, stderrs = sshs.exec_command('if test -f "spitz/spitz";then echo "s" ;fi')
                                if len(stdouts.readlines()) > 0:
                                    stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
                                    if len(stdouts.readlines()) > 0:
                                        isThereSpitz = "Running"
                                    else:
                                        isThereSpitz = "Stopped"
                                else:
                                    isThereSpitz = "No"
                            except socket.gaierror as e1:
                                if(verbose):
                                    print "Couldn't find " + str(address) + "."
                                status = "No SSH access."
                            except socket.error as e2:
                                if(verbose):
                                    print "Connection refused in " + str(address) + "."
                                status = "SSH refused."
                            except paramiko.AuthenticationException as e3:
                                if(verbose):
                                    print "Wrong credentials for " + str(address) + "."
                                status = "SSH denied."
                            except Exception as exc:
                                if(verbose):
                                    print "unexpected error (" + str(exc) + ") connecting to " + str(address) + "."
                                status = "SSH issue."

                        # Append service.
                        self.VMrows.append([hosted_service.service_name, address, port, status, isThereSpitz, spitz_port])
                        self.VMrowsInfo.append([hosted_service.service_name, deployment_name, instance_name, original_status])

            self.printTable(self.VMcolumns, self.VMrows, 5);

        except WindowsAzureError as WAE:
            print("Couldn't connect with Azure, is your credentials right?") 
        except socket.gaierror as SGE:
            print("Problem connecting to Azure, are your internet ok?")
        except ssl.SSLError as SLE:
            print("Problem connecting to Azure, are your certificates ok?")
        except Exception as exc:
            if(verbose):
                print "unexpected error (" + str(exc) + ")" 

    def printTable(self, names, table, spacing):
        maxsize = []
        
        for title in names:
            maxsize.append(len(title)+spacing)
            
        for i in range(len(names)):
            for line in table:
                aux = (len(str(line[i]))+spacing)
                if(  aux > maxsize[i] ):
                    maxsize[i] = aux
                    
        for index in range(len(names)):
            sys.stdout.write(str(names[index]).ljust(maxsize[index]))
        for line in table:
            sys.stdout.write('\n')
            for index in range(len(names)):
                sys.stdout.write(str(line[index]).ljust(maxsize[index]))
        sys.stdout.write('\n')

    # Test if an unreachable VM is, now, reachable (using SSH). Also checks for SPITZ intance if it's on. 
    def VMTryAgain(self, index, ssh_login, ssh_pass):
        address = self.VMrows[index][1] 
        print address
        sshs = paramiko.SSHClient()
        sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        status = self.VMrowsInfo[3]
        isThereSpitz = "NO"

        try:
            s_action = "Try Again"
            sshs.connect(address,
                 username=ssh_login,
                 password=ssh_pass)

            stdins, stdouts, stderrs = sshs.exec_command('pgrep spitz')
            pid = stdouts.readlines()
            if len(pid) != 0:
                isThereSpitz = "YES"
                s_action = "Restart"
            else:
                isThereSpitz = "NO"
                s_action = "Start"

        except socket.gaierror as e1:
            print( "Couldn't find " + str(address) + ".")
            status = "No SSH access."
        except socket.error as e2:
            print("Connection refused in " + str(address) + ".")
            status = "SSH refused."
        except paramiko.AuthenticationException as e3:
            print("Wrong credentials for " + str(address) + ".")
            status = "SSH denied."
        except:
            print( "unexpected error connecting to " + str(address) + ".")
            status = "SSH issue."

        self.VMrows[i][2] = status
        self.VMrows[i][4] = isThereSpitz 
        self.VMrows[i][5] = s_action

        if isThereSpitz == "YES":
            return True
        else:
            return False 

    # Send a request to the monitor and get the answer.
    def getStatusMessage(self, task, jm_address, jm_port):
        if COMM_connect_to_job_manager(jm_addres, jm_port) >= 0:

            self.ln = COMM_get_status('')
            if self.ln != None:
                return 0
        print self.ln
        return -1 

    # Get number of tasks from the Job Manager
    def getNumberOfTasks(self, task, jm_address, jm_port):
        ret = COMM_connect_to_job_manager(jm_address, jm_port)
        
        if ret >= 0 :
            self.TotalTasks = COMM_get_num_tasks()
            print "Received number of tasks: " + str(self.TotalTasks)
            return 0
        else:
            print "Connection problem, couldn't get the number of tasks." 
            return -1

    # Using the last status received, parse the list string.
    def fillRows(self,status):
        self.total_rcvd = 0
        self.total_compl = 0
        del self.rows[:]        

        #status = status[13:]
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
        #percentage = (self.total_compl / float(100))*100


    # Check if it's on and if there is a spitz instance running.
    def startAllNodes(self, subscription_id, certificate_path, lib_file, script, ssh_user, ssh_pass, jm_address, jm_port, upgrade=False):
        #ret = COMM_connect_to_job_manager(jm_address, jm_port)
        #ret = COMM_send_vm_node(socket.gethostbyname('spitz0.cloudapp.net'), VM_PORT)
        #ret = COMM_send_vm_node('127.0.0.1', PORT_VM)
        #return

        # Get credentials and instanciate SMS
        self.listSpitzVMs(subscription_id, certificate_path, ssh_user, ssh_pass, verbose=True)

        if(len(self.VMrows) == 0):
            print "Couldn't find any VM with a SPITZ name."
            return

        # Iterate through list and upload sptiz
        for service in self.VMrows:
            address = service[1]

            if(service[3] != "ReadyRole"):
                continue

            try:
                sshs = paramiko.SSHClient()
                sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())

                sshs.connect(service[1], username=ssh_user,
                    password=ssh_pass, port = service[2],
                    timeout = float(1))

                if(service[4] == "Running"):
                    sshs.exec_command('pkill spitz')
                    service[4] = "Stopped"

                if (upgrade) or (service[4] == "No"):
                    sshs.exec_command('rm -r spitz')
                    sshs.exec_command('mkdir spitz')
                    sftp = sshs.open_sftp()
                    sftp.put('spitz', 'spitz/spitz') 
                    sshs.exec_command('chmod 555 ~/spitz/spitz')
                    sftp.put('libspitz.so', 'spitz/libspitz.so') 
                    sftp.put(lib_file, 'spitz/' + lib_file) 
                    sftp.put(script, 'spitz/' + script) 
                    sshs.exec_command('chmod 555 ~/spitz/' + script)
                    service[4] = "Stopped"
            except socket.gaierror as e1:
                print ("Couldn't find " + str(address) + ".")
            except socket.error as e2:
                print("Connection refused in " + str(address) + ".")
            except paramiko.AuthenticationException as e3:
                print("Wrong credentials for " + str(address) + ".")
            except Exception as e3:
                print("unexpected error (" + str(e3) + ") connecting to " + str(address) + ".")

        # Everyone is set up, run everyone!
        for service in self.VMrows:
            address = service[1]
            print service[1] 
            print service[2]
            print service[3]
            print service[4]

            if(service[4] != "Stopped"):
                continue
                       
            try:
                sshs = paramiko.SSHClient()
                sshs.set_missing_host_key_policy(paramiko.AutoAddPolicy())

                sshs.connect(service[1], username=ssh_user,
                    password=ssh_pass, port = service[2],
                    timeout = float(1))

                sshs.exec_command('./spitz/' + script) 
                service[4] = "Running"
                                    
                ret = COMM_connect_to_job_manager(jm_address, jm_port)

                #ret = COMM_send_vm_node(socket.gethostbyname(str(address)), PORT_VM+offset)
                if ret == 0:
                    ret = COMM_send_vm_node(socket.gethostbyname(str(address)), service[5])

                    if ret == 0:
                        print("Spitz instance running in " + str(address) + "|" + str(service[2]) + ".")
                    else:
                        print("Problem sendin vm_node to Job Manager" + str(address) + "|" + str(service[2]) + ".")
                else:
                    print("Can't connect to Job Manager.")
                        
            except socket.gaierror as e1:
                print ("Couldn't find " + str(address) + ".")
            except socket.error as e2:
                print("Connection refused in " + str(address) + ".")
            except paramiko.AuthenticationException as e3:
                print("Wrong credentials for " + str(address) + ".")
            except Exception as e3:
                print("unexpected error (" + str(e3) + ") connecting to " + str(address) + ".")

        self.printTable(self.VMcolumns, self.VMrows, 5)

    def createNode(self, sms, service_name, vm_name, blob_url, image, offset, linux_user, linux_pass, is_first, wait=10):
        # Create linux config
        linux_config = LinuxConfigurationSet(vm_name, linux_user, linux_pass, True)
        linux_config.disable_ssh_password_authentication = False

        # Create linux endpoints
        endpoint_config = ConfigurationSet()
        endpoint_config.configuration_set_type = 'NetworkConfiguration'
        #endpoint1 = ConfigurationSetInputEndpoint(name='JM', protocol='tcp', port=str(8898+offset), local_port='8898', 
        #                                           load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
        #endpoint2 = ConfigurationSetInputEndpoint(name='CM', protocol='tcp', port=str(10007+offset), local_port='10007', 
        #                                           load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
        #endpoint1 = ConfigurationSetInputEndpoint(name='SPITZ', protocol='tcp', port=str(PORT_VM+offset), local_port='10007', 
        #                                           load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
        print "port: " + str(PORT_VM+offset)
        print "local_port: " + str(PORT_VM)
        endpoint1 = ConfigurationSetInputEndpoint(name='SPITZ', protocol='tcp', port=str(PORT_VM+offset), local_port=str(PORT_VM),
                                                    load_balanced_endpoint_set_name=None, enable_direct_server_return=False)
        endpoint2 = ConfigurationSetInputEndpoint(name='SSH', protocol='tcp', port=str(22+offset), local_port='22', 
                                                    load_balanced_endpoint_set_name=None, enable_direct_server_return=False)

        endpoint_config.input_endpoints.input_endpoints.append(endpoint1)
        endpoint_config.input_endpoints.input_endpoints.append(endpoint2)
        #endpoint_config.input_endpoints.input_endpoints.append(endpoint3)

        # Create HD
        label = service_name + '-' + vm_name
        media_link = blob_url + label + '.vhd'
        os_hd = OSVirtualHardDisk(image, media_link, disk_label= label +'.vhd', disk_name = label +'.name' )
        print vars(os_hd)

        if(is_first == True):
            done = False
            try_t = 0
            while (done == False):
                try:
                    print "Try: " + str(try_t)
                    sms.create_virtual_machine_deployment(service_name=service_name, deployment_name=service_name,
                                              deployment_slot='production', label=vm_name, role_name=vm_name,
                                              system_config=linux_config, network_config=endpoint_config,
                                              os_virtual_hard_disk=os_hd, role_size='Small')
                    done=True
                except Exception as exc:
                    try_t += 1
                    time.sleep(wait)
                    print 'error, exc: ' + str(exc)
        else:
            done = False
            try_t = 0
            while (done == False):
                try:
                    print "Try: " + str(try_t)
                    sms.add_role(service_name=service_name, deployment_name=service_name, role_name = vm_name, 
                                    system_config=linux_config, os_virtual_hard_disk = os_hd, 
                                    network_config=endpoint_config, role_size='Small')
                    done = True
                except Exception as exc:
                    try_t += 1
                    time.sleep(wait)
                    print 'error, exc: ' + str(exc)

    def allocateNodes(self, total_cloud_services, vms_per_cloud, subscription_id, 
                certificate_path, windows_blob_url, image, location, 
                linux_user, linux_pass, VMHeader = "spitz"):

        sms = ServiceManagementService(subscription_id, certificate_path)

        # Stores names of services.
        ServiceList = []
        VMList = []

        # Get current services.
        try:
            result = sms.list_hosted_services()
        except WindowsAzureError as x:
            print 'Except: Could not list hosted services'
            return


        # Check current services and create deployments when needed.
        service_counter = 0
        for hosted_service in result:
            print('Service name: ' + hosted_service.service_name)

            if "spitz" in str(hosted_service.service_name).lower():
                if(service_counter < total_cloud_services):
                    service_counter += 1
                    ServiceList.append(str(hosted_service.service_name))
                    remove_list = []
                    try:
                        d_result = sms.get_deployment_by_slot(hosted_service.service_name, 'Production')
                        vm_counter = 0
                        print ( 'Deployment Name:'  + d_result.name )
                        for instance in d_result.role_instance_list :
                            print ( 'Instance name:'  + instance.instance_name )
                            if(vm_counter >= vms_per_cloud):
                                print 'Not necessary role detected'
                                remove_list.append(instance.instance_name)
                                # Don't need it, remove.
                            else:
                                vm_counter += 1

                        if(len(remove_list)>0):
                            print 'Deleting extra roles'
                            sms.delete_role_instances(hosted_service.service_name, d_result.name, remove_list)

                        VMList.append(vm_counter)
                    # Deployment doesn't exist
                    except WindowsAzureMissingResourceError:
                        print 'Creating Deployment'
                        self.createNode(sms, str(hosted_service.service_name), VMHeader + str(0), 
                            windows_blob_url, image, 0, linux_user, linux_pass, True)
                        VMList.append(1)

                # Service not necessary, destroy it.
                else:
                    print 'Deleting extra service'
                    sms.delete_hosted_service(hosted_service.service_name)

            print('')
        # Verify if any service should be created and create them.
        while(service_counter < total_cloud_services):
            nextName = VMHeader + str(service_counter)
            print 'Creating service and deployment: ' + str(nextName)
            sms.create_hosted_service(service_name=nextName, label=nextName, location=location)
            self.createNode(sms, nextName, VMHeader + str(0), windows_blob_url, image, 
                0, linux_user, linux_pass, True)

            ServiceList.append(nextName)
            VMList.append(1)
            service_counter += 1

        # Create the remaining VMs
        for vm_num in range(vms_per_cloud):
            for sv_num in range(len(ServiceList)):
                if (VMList[sv_num] <= vm_num):
                    print 'Creating Node: ' + str(ServiceList[sv_num]) + ' - VMNUM: ' + str(vm_num)
                    self.createNode(sms, ServiceList[sv_num], VMHeader + str(vm_num), windows_blob_url, 
                        image, vm_num, linux_user, linux_pass, False)

