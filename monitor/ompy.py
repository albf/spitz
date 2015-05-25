# For worker/threading part.
import threading
#import time

# Task Format = [NumberOfExecution, Initial i, For-Function, [nonSharedArguments]]
class FIFO:
    def __init__(self):
        # Semaphore
        self.pendent_tasks = threading.Semaphore(0)
        self.tasks_in_FIFO = 0
        # Mutex
        self.task_FIFO_mutex = threading.Lock()
        self.task_FIFO = []

    # Methods to manage the FIFO 

    # Add task to Worker FIFO
    def addTask(self,task):
        self.task_FIFO_mutex.acquire()  # Take mutex of FIFO
        #print task
        self.task_FIFO.append(task)     # Append task
        self.tasks_in_FIFO += 1         # Update task counter
        self.task_FIFO_mutex.release()  # Release mutex
        self.pendent_tasks.release()    # Warn worker

    def popTask(self):
        self.task_FIFO_mutex.acquire()
        task = self.task_FIFO.pop(0)    # Pop first task available
        self.tasks_in_FIFO -=1          # Update task counter
        self.task_FIFO_mutex.release()
        return task

    # Worker : Run task passed by FIFO
    def runOneTask(self,task):
        for i in range(task[0]):        # Run the specified number
            loop = i+task[1]
            args = [loop] + task[3]
            task[2](*args)              # Call the function passed with the arguments
    


class OMPY_Execution:
    def __init__(self, NumThreads=2):
        # Task FIFOs
        self.workerFIFO = FIFO()
        
        #Extra Thread
        self.NumThreads = NumThreads
        self.WorkerThreads = []
        for i in range(NumThreads):
            self.WorkerThreads.append(threading.Thread(target=self.worker))
        
        # Counter Lock
        self.CounterMutex = threading.Lock()
        self.Counter = 0
        
        # Number of Tasks
        self.TotalTasks = 0
        
    # Worker function.
    def worker(self):
        #print 'Worker Started'
        while(self.getCounter() < self.TotalTasks):     # Check if it's over.
            self.workerFIFO.pendent_tasks.acquire()     # Wait for tasks
            task = self.workerFIFO.popTask()            # Pop task
            self.workerFIFO.runOneTask(task)            # Run the task

        #print 'Worker exited.'
        return
        
    def startThreads(self):
        for t in self.WorkerThreads:
            t.start()
            
    def joinThreads(self):
        for t in self.WorkerThreads:
            t.join()
        
    def getCounter(self):
        self.CounterMutex.acquire()
        ret = self.Counter
        self.Counter = self.Counter+1
        self.CounterMutex.release()
        return ret
        
    def forExecution(self, func, i_min, i_max, nonSharedArg):
        self.TotalTasks = self.NumThreads
        iPerThread = (int)(i_max - i_min) / self.NumThreads
        i_min_t = 0
        # Add tasks for all but the last.
        for t in range(self.TotalTasks-1):
            # Task Format = [NumberOfExecution, Initial i, For-Function, [nonSharedArguments]]
            self.workerFIFO.addTask([iPerThread, i_min_t, func, nonSharedArg])
            i_min_t = i_min_t + iPerThread
        # Add the last task, avoid problem with the division.
        # Task Format = [NumberOfExecution, Initial i, For-Function, [nonSharedArguments]]
        self.workerFIFO.addTask([(i_max-i_min_t), i_min_t, func, nonSharedArg])
        self.startThreads()
        self.joinThreads()
        
        
        
# Main Example
'''
listinha = range(10000)
def ForFunction(i):
        global listinha
        global delay
        listinha[i] = listinha[i]+delay
OMPY.forExecution(ForFunction, 0, len(listinha), [])

OMPY = OMPY_Execution(1)
start = time.time()
OMPY.forExecution(ForFunction, 0, len(listinha), [])
end = time.time()
elapsed = end - start
print 'Elapsed time: ' + str(elapsed)

OMPY = OMPY_Execution(2)
start = time.time()
OMPY.forExecution(ForFunction, 0, len(listinha), [])
end = time.time()
elapsed = end - start
print 'Elapsed time with 2 Threads: ' + str(elapsed)

OMPY = OMPY_Execution(4)
start = time.time()
OMPY.forExecution(ForFunction, 0, len(listinha), [])
end = time.time()
elapsed = end - start
print 'Elapsed time with 4 Threads: ' + str(elapsed)
'''
