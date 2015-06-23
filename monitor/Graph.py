#!/usr/bin/python
import math
import numpy as np
import matplotlib.pyplot as plt

which = 7
filename = "libcmp.so"
ending = ".cm.dia"
action = "R" 
debug = False 

# Used to determine the number of bins used.
def FDrule(l):
    l_s = sorted(l)
    IQR = l_s[int(3*len(l)/4)] - l_s[int(len(l)/4)]
    return 2*IQR*math.pow(len(l), -1/float(3))

# Graph 1: Number of Completed Tasks vs Time
def g_completed_tasks(filename):
    with open(filename + ".reg") as f:
        data_reg = f.read()
        data_reg = data_reg.split(';\n')
        data_reg = [row.split('|') for row in data_reg]
        data_reg.pop(-1)

    g1_x = sorted([float(row[5]) for row in data_reg])
    g1_y = []

    last = -1
    loop = 0

    # Accumulative sum
    for i in range(len(g1_x)):
        if(g1_x[loop] != last):
            g1_y.append(1+i)
            last = g1_x[loop]
            loop = loop+1
        else:
            g1_x.pop(-1)
            g1_y[-1] = g1_y[-1] + 1

    # Starting point
    g1_x.insert(0,float(0))
    g1_y.insert(0,int(0))

    # Now, the plotting part.
    g1_fig = plt.figure()
    gf1 = g1_fig.add_subplot(111)
    gf1.set_title('Total Completed Tasks in Time')    
    gf1.set_xlabel('Time')
    gf1.set_ylabel('Completed Tasks')
    gf1.plot(g1_x,g1_y, c='r') 
    leg = gf1.legend()
    plt.show()

# Graph 2: Completed/Send Tasks per Node
def g_task_per_node(filename):
    with open(filename + ".list") as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)

    g2_y = [int(row[0]) for row in data_list]
    g2_x1 = [int(row[5]) for row in data_list]
    g2_x2 = [int(row[6]) for row in data_list]


    plt.barh(g2_y, g2_x1, xerr=None, align='center', color='b', alpha=0.5, label = 'Sent')
    plt.barh(g2_y, g2_x2, xerr=None, align='center', color='r', alpha=0.5, label = 'Completed')
    plt.xlabel('Tasks')
    plt.title('Tasks/Node.')

    plt.show()

# Graph 3 : TaskManager Event Journal
def g_taskmanager_journal(filename, is_debug = False):
    with open(filename + ".tm.dia") as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)
        if(is_debug):
            print data_list

    actors = []
    left = []
    ypos = []
    duration = []
    leg = {}

    w_count = -1
    for d in data_list:
        if(is_debug):
            print d
        if (len(d) == 2) and (d[1] == 'W'):
            w_count += 1
        elif (len(d) >= 3): 
            left.append(float(d[1]))
            duration.append(float(d[2]) - float(d[1]))
            if(d[0] == 'P'):
                l_e = "Worker_" + str(w_count)
                actors.append("Worker_" + str(w_count))
                ypos.append(2 + w_count)
            if(d[0] == 'R'):
                l_e = "In"
                actors.append("In")
                ypos.append(0)
            if(d[0] == 'S'):
                l_e = "Out"
                actors.append("Out")
                ypos.append(1)
            if(l_e in leg):
                leg[l_e][0] += (float(d[2])-float(d[1]))
                leg[l_e][2] = float(d[2])
            else:
                leg[l_e] = [float(d[2])-float(d[1]), float(d[1]),float(d[2])]

    if(is_debug):
        print actors
        print left
        print duration
        print ypos
        print leg

    for k in leg.keys():
        perc = float(leg[k][0])/(float(leg[k][2]-leg[k][1]))*100
        perc_t = "%.2f" % perc
        label = k + " : " + perc_t + "%"
        leg[k].append(label)

    for i in range(len(actors)):
        actors[i] = leg[actors[i]][3] 

    plt.barh(ypos, duration, left=left, align='center', alpha=0.4)
    plt.yticks(ypos, actors)
    plt.xlabel('Time(s)')
    plt.title('TaskManager ' + filename)

    plt.show()

# Graph 4 : Committer Event Journal
def g_committer_journal(filename, is_debug = False):
    with open(filename + ".cm.dia") as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)
        if(is_debug):
            print data_list

    actors = []
    left = []
    ypos = []
    duration = []
    leg = {}
    w_count = -1

    for d in data_list:
        if(is_debug):
            print d
        if (len(d) == 2) and (d[1] == 'R'):
            w_count += 1
        if (len(d) >= 3): 
            left.append(float(d[1]))
            duration.append(float(d[2]) - float(d[1]))
            if(d[0] == 'P'):
                l_e = "Commit_Pit"
                actors.append("Commit_Pit")
                ypos.append(1)
            if(d[0] == 'R'):
                if(w_count < 0):
                    w_count = 0
                l_e = "In " + str(w_count)
                actors.append("In " + str(w_count))
                ypos.append(2 + w_count)
            if(d[0] == 'J'):
                l_e = "Commit Job"
                actors.append("Commit Job")
                ypos.append(0)
            if(l_e in leg):
                print "XXXXXXXXXX"
                print (float(d[2])-float(d[1]))
                leg[l_e][0] += (float(d[2])-float(d[1]))
                leg[l_e][2] = float(d[2])
            else:
                leg[l_e] = [float(d[2])-float(d[1]), float(d[1]),float(d[2])]

    if(is_debug):
        print actors
        print left
        print duration
        print ypos
        print leg

    for k in leg.keys():
        perc = float(leg[k][0])/(float(leg[k][2]-leg[k][1]))*100
        perc_t = "%.2f" % perc
        label = k + " : " + perc_t + "%"
        leg[k].append(label)

    for i in range(len(actors)):
        actors[i] = leg[actors[i]][3] 

    plt.barh(ypos, duration, left=left, align='center', alpha=0.4)
    plt.yticks(ypos, actors)
    plt.xlabel('Time(s)')
    plt.title('Committer ' + filename)
    plt.show()

# Graph 5 : JobManager Event Journal
def g_jobmanager_journal(filename, is_debug = False):
    with open(filename + ".jm.dia") as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)
        if(is_debug):
            print data_list

    actors = []
    left = []
    ypos = []
    duration = []
    leg = {}

    s_count = -1
    for d in data_list:
        if(is_debug):
            print d
        if (len(d) == 2) and (d[1] == 'E'):
            s_count += 1
        elif (len(d) >= 3): 
            left.append(float(d[1]))
            duration.append(float(d[2]) - float(d[1]))
            if(d[0] == 'S'):
                l_e = "Send_" + str(s_count)
                actors.append("Send_" + str(s_count))
                ypos.append(3 + s_count)
            if(d[0] == 'G'):
                l_e = "Gen"
                actors.append("Gen")
                ypos.append(2)
            if(d[0] == 'R'):
                l_e = "Request"
                actors.append("Request")
                ypos.append(1)
            if(d[0] == 'V'):
                l_e = "VMRestore"
                actors.append("VMRestore")
                ypos.append(0)
            if(l_e in leg):
                leg[l_e][0] += (float(d[2])-float(d[1]))
                leg[l_e][2] = float(d[2])
            else:
                leg[l_e] = [float(d[2])-float(d[1]), float(d[1]),float(d[2])]

    if(is_debug):
        print actors
        print left
        print duration
        print ypos

    for k in leg.keys():
        perc = float(leg[k][0])/(float(leg[k][2]-leg[k][1]))*100
        perc_t = "%.2f" % perc
        label = k + " : " + perc_t + "%"
        leg[k].append(label)

    for i in range(len(actors)):
        actors[i] = leg[actors[i]][3] 

    plt.barh(ypos, duration, left=left, align='center', alpha=0.4)
    plt.yticks(ypos, actors)
    plt.xlabel('Time(s)')
    plt.title('JobManager ' + filename)
    plt.show()

# Graph 6 : Histogram 
def g_hist(filename, ending, action, is_debug = False):
    with open(filename + ending) as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)
        if(is_debug):
            print data_list

    data = []

    for d in data_list:
        if(is_debug):
            print d
        if (len(d) >= 3) and (d[0] == action): 
            data.append(float(d[2]) - float(d[1]))

    if(is_debug):
        print data 

    binwidth = FDrule(data)
    if(is_debug):
        print "binwidth: " + str(binwidth)
    plt.hist(data, bins=np.arange(min(data), max(data) + binwidth, binwidth)) 
    plt.xlabel('Time(s)')
    plt.title(action + " - " + filename)
    plt.show()

# Graph 7 : Histogram with size
def g_hist_ratio(filename, ending, action, is_debug = False):
    with open(filename + ending) as f:
        data_list = f.read()
        data_list = data_list.split(';\n')
        data_list = [row.split('|') for row in data_list]
        data_list.pop(-1)
        if(is_debug):
            print data_list

    data = []

    for d in data_list:
        if(is_debug):
            print d
        if (len(d) >= 4) and (d[0] == action): 
            data.append(float(d[3])/(float(d[2]) - float(d[1])))

    if(is_debug):
        print data 

    binwidth = FDrule(data)
    if(is_debug):
        print "binwidth: " + str(binwidth)
    plt.hist(data, bins=np.arange(min(data), max(data) + binwidth, binwidth)) 
    plt.xlabel('bandwidth(B/s)')
    plt.title(action + " - " + filename)
    plt.show()



if(which == 1):
    g_completed_tasks(filename)

elif(which == 2):
    g_task_per_node(filename)

elif(which == 3):
    g_taskmanager_journal(filename, is_debug=debug)

elif(which == 4):
    g_committer_journal(filename, is_debug=False)

elif(which == 5):
    g_jobmanager_journal(filename, is_debug=debug)

elif(which == 6):
    g_hist(filename, ending, action, is_debug=debug)

elif(which == 7):
    g_hist_ratio(filename, ending, action, is_debug=debug)
