import matplotlib.pyplot as plt
import numpy as np
import re
import os
from os import listdir, remove
from os.path import isfile, join
from matplotlib.font_manager import FontProperties
from matplotlib import rcParams
rcParams['xtick.direction'] = 'in'
rcParams['ytick.direction'] = 'in'
rcParams['figure.figsize'] = 7.5, 7
rcParams['font.size'] =  12
#rcParams['font.family'] = 'serif'


Loc = os.getcwd() 

def sort(main,other):
	main , other = zip(*sorted(zip(main,other)))
	main = np.array(main)
	other = np.array(other)
	return main, other

#---------------------------------------------------------
# Reading the Data  
#---------------------------------------------------------

Col = ['r','b','g','m','r','c','y','#92C6FF','#E24A33', '#001C7F', '.15', '0.40']
Mar = ["d","s","o",".","P","v","^","<",">","1","2","3","4","8","+","p","P","*"]
Lin = ['-','-.',':']


Part_A = np.genfromtxt('results-A',skip_header=1,delimiter=',')
Part_B = np.genfromtxt('results-B',skip_header=1,delimiter=',')
Part_C = np.genfromtxt('results-C',skip_header=1,delimiter=',')

Name = ['Star/centralized','Ring','Work queue']

# Plot for single machine Lines Vs Time

Time = []
Lines = []

c = 0
for i in [2,4,8,16]:
	m = 0
	for Part in [Part_A,Part_B,Part_C]:
		Time = []
		Lines = []
		for l in Part:
			if l[2] == i and l[-1] == 1: 
				Time.append(l[3]/3600.0)
				Lines.append(l[5])
		plt.plot(Lines,Time,color=Col[c],marker=Mar[m],linestyle=Lin[m],label='%s-%d'%(Name[int(l[1])-1],i))
		m +=1
		
	c +=1

plt.xlabel('Number of Lines')
plt.ylabel('Global Time ($hr$)')
plt.xticks(np.arange(200000, 1200000, 200000))
leg = plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.15), ncol=4, fancybox=False, shadow=False, prop={'size': 9})
leg.get_frame().set_linewidth(0.0)

plt.savefig('Num_Lines_Vs_Time.png')
plt.close()

# Plot for single machine Lines Vs Memory

Lines = []
Memory = []

c = 0
for i in [2,4,8,16]:
	m = 0
	for Part in [Part_A,Part_B,Part_C]:
		Lines = []
		Memory = []
		for l in Part:
			if l[2] == i and l[-1] == 1:
				Lines.append(l[5])
				Memory.append(l[-2]/1024.0)
		plt.plot(Lines,Memory,color=Col[c],marker=Mar[m],linestyle=Lin[m],label='%s-%d'%(Name[int(l[1])-1],i))
		m +=1
		
	c +=1

plt.xlabel('Number of Lines')
plt.ylabel('Memory per Core (MB)')
plt.xticks(np.arange(200000, 1200000, 200000))
leg = plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.15), ncol=4, fancybox=False, shadow=False, prop={'size': 9})
leg.get_frame().set_linewidth(0.0)

plt.savefig('Num_Lines_Vs_Memory.png')
plt.close()


# Plot for single machine Lines Vs Time

Time  = []
Cores = []

Time2  = []
Cores2 = []
c = 0
for Part in [Part_A,Part_B,Part_C]:
	Time  = []
	Cores = []
	Time2  = []
	Cores2 = []
	for l in Part:
		if l[-3] == 1000000:
			if l[-1]==1:
				Time.append(l[3]/3600.0)
				Cores.append(l[2])
			if l[-1]==2:
				Time2.append(l[3]/3600.0)
				Cores2.append(l[2])
	Cores,Time = sort(Cores,Time)
	Cores2,Time2 = sort(Cores2,Time2)
	plt.plot(Cores,Time,color=Col[c],marker=Mar[c],label='%s'%(Name[int(l[1])-1]))
	#plt.plot(Cores2,Time2,color=Col[c],marker=Mar[c], linestyle=Lin[1],label='%s (Multiple)'%(Name[int(l[1])-1]))
	c +=1

plt.xlabel('Number of Cores')
plt.ylabel('Global Time ($hr$)')
plt.xticks(np.arange(2, 17, 2))
plt.xlim()
leg = plt.legend(loc='upper center', bbox_to_anchor=(0.5, 1.1), ncol=4, fancybox=False, shadow=False, prop={'size': 12})
leg.get_frame().set_linewidth(0.0)

plt.savefig('Time_Vs_Cores.png')
plt.close()