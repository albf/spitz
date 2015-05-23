#!/usr/bin/python
import numpy as np
import matplotlib.pyplot as plt

which = 3
filename = "prime.so"

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

    w_count = -1
    for d in data_list:
        if(is_debug):
            print d
        if (len(d) == 2) and (d[1] == 'W'):
            w_count += 1
        elif (len(d) == 3): 
            left.append(float(d[1]))
            duration.append(float(d[2]) - float(d[1]))
            if(d[0] == 'P'):
                actors.append("Worker_" + str(w_count))
                ypos.append(2 + w_count)
            if(d[0] == 'R'):
                actors.append("In")
                ypos.append(0)
            if(d[0] == 'S'):
                actors.append("Out")
                ypos.append(1)

    if(is_debug):
        print actors
        print left
        print duration
        print ypos

    plt.barh(ypos, duration, left=left, align='center', alpha=0.4)
    plt.yticks(ypos, actors)
    plt.xlabel('Time(s)')
    plt.title('TaskManager ' + filename)
    plt.show()

if(which == 1):
    g_completed_tasks(filename)

elif(which == 2):
    g_task_per_node(filename)

elif(which == 3):
    g_taskmanager_journal(filename, is_debug=False)
