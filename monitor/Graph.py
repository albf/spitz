#!/usr/bin/python
import numpy as np
import matplotlib.pyplot as plt

with open("prime.so.list") as f:
    data_list = f.read()
    data_list = data_list.split(';\n')
    data_list = [row.split('|') for row in data_list]
    data_list.pop(-1)

with open("prime.so.reg") as f:
    data_reg = f.read()
    data_reg = data_reg.split(';\n')
    data_reg = [row.split('|') for row in data_reg]
    data_reg.pop(-1)

# Graph 1: Number of Completed Tasks vs Time
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
