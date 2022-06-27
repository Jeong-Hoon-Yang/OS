//
// CPU Schedule Simulator Homework
// Student Number : B711109
// Name : ¾çÁ¤ÈÆ
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent *prev;
	struct ioDoneEvent *next;
} ioDoneEventQueue, *ioDoneEvent;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime;
	int serviceTime;
	int startTime;
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process *prev;
	struct process *next;
} *procTable;

struct process idleProc;
struct process readyQueue;
struct process blockedQueue;
struct process *runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int *procIntArrTime, *procServTime, *ioReqIntArrTime, *ioServTime;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;

	for(i=0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}

// ** push Ready Queue Function
void pushRQ(int id) { // insert process into readyQueue
	if (readyQueue.next == &readyQueue) {
		readyQueue.next = &procTable[id];
		readyQueue.prev = &procTable[id];
		procTable[id].next = &readyQueue;
		procTable[id].prev = &readyQueue;
	}
	else {
		readyQueue.prev->next = &procTable[id];
		procTable[id].prev = readyQueue.prev;
		procTable[id].next = &readyQueue;
		readyQueue.prev = &procTable[id];
	}

	readyQueue.len++;

	struct process* tmp = readyQueue.next;
}

struct process* PopRQ() { // pop readyQueue member
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {
    return &idleProc;
  }

	tmp->next->prev = &readyQueue;
	readyQueue.next = tmp->next;
	tmp->next = NULL;
	tmp->prev = NULL;

	readyQueue.len--;

	return tmp;
}

void pushBQ(int id) { // push process into blockedQueue
	if (blockedQueue.next == &blockedQueue) {
		blockedQueue.next = &procTable[id];
		blockedQueue.prev = &procTable[id];
		procTable[id].next = &blockedQueue;
		procTable[id].prev = &blockedQueue;
	}
	else {
		blockedQueue.prev->next = &procTable[id];
		procTable[id].prev = blockedQueue.prev;
		procTable[id].next = &blockedQueue;
		blockedQueue.prev = &procTable[id];
	}

	blockedQueue.len++;
}

struct process* pnpBQ(int id) { // pick and pop blockedQueue member
	struct process* tmp = blockedQueue.next;

  while (tmp->id != id) {
    tmp = tmp->next;
  }

	tmp->prev->next = tmp->next;
  tmp->next->prev = tmp->prev;
  tmp->prev = NULL;
  tmp->next = NULL;

	blockedQueue.len--;

	return tmp;
}

void pushIOQueue(int id) { // push process into blockedQueue
	if (ioDoneEventQueue.next == &ioDoneEventQueue) {
		ioDoneEventQueue.next = &ioDoneEvent[id];
		ioDoneEventQueue.prev = &ioDoneEvent[id];
		ioDoneEvent[id].next = &ioDoneEventQueue;
		ioDoneEvent[id].prev = &ioDoneEventQueue;
	}
	else if (ioDoneEventQueue.next->doneTime > ioDoneEvent[id].doneTime) {
		ioDoneEvent[id].next = ioDoneEventQueue.next;
		ioDoneEventQueue.next->prev = &ioDoneEvent[id];
		ioDoneEvent[id].prev = &ioDoneEventQueue;
		ioDoneEventQueue.next = &ioDoneEvent[id];
	}
	else {
    struct ioDoneEvent* nextpos = ioDoneEventQueue.next;

    while (ioDoneEvent[id].doneTime >= nextpos->doneTime && nextpos != &ioDoneEventQueue) {
      nextpos = nextpos->next;
    }
		
		ioDoneEvent[id].next = nextpos;
    ioDoneEvent[id].prev = nextpos->prev;
		nextpos->prev->next = &ioDoneEvent[id];
    nextpos->prev = &ioDoneEvent[id];
	}

	ioDoneEventQueue.len++;
}

struct ioDoneEvent* PopIOQueue() { // pop ioDoneEventQueue member
	struct ioDoneEvent* tmp = ioDoneEventQueue.next;
	
	tmp->next->prev = &ioDoneEventQueue;
	ioDoneEventQueue.next = tmp->next;
	tmp->next = NULL;
	tmp->prev = NULL;

	ioDoneEventQueue.len--;

	return tmp;
}

void procExecSim(struct process *(*scheduler)()) {
	int pid, qTime=0, cpuUseTime = 0, nproc=0, termProc = 0, nioreq=0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;
	char run_to_ready = 0, run_to_block = 0, quantum_did = 0;
	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];

	runningProc = &idleProc;

	while (1) {
		currentTime++;
		schedule = 0; run_to_ready = 0; run_to_block = 0; quantum_did = 0;

		struct ioDoneEvent* tmp = ioDoneEventQueue.next;

		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc) {
			cpuUseTime++;
			cpuReg0 = runningProc->saveReg0;
			cpuReg1 = runningProc->saveReg1;
		}

		#ifdef DEBUG
		printf("%d processing Proc %d servT %d targetServT %d nproc %d cpuUseTime %d qTime %d ioDoneTime %d, ioDoneEvent Length %d\n"
		, currentTime, runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime, nproc, cpuUseTime, qTime, ioDoneEventQueue.next->doneTime, ioDoneEventQueue.len);
  	#endif	
		// MUST CALL compute() Inside While loop
		compute();
		runningProc->saveReg0 = cpuReg0;
		runningProc->saveReg1 = cpuReg1;
		
		
		if (currentTime == nextForkTime) { /* CASE 2 : a new process created */
			procTable[nproc].startTime = currentTime;
			procTable[nproc].state = S_READY;

			pushRQ(nproc);// **push process to Ready Queue

      if (++nproc < NPROC) {
        nextForkTime += procIntArrTime[nproc];

				#ifdef DEBUG
				printf("process %d target %d servTime %d added to Ready Queue Next Fork Time %d\n",
				procTable[nproc-1].id, procTable[nproc-1].targetServiceTime, procTable[nproc-1].serviceTime, nextForkTime);
				#endif
			}
			else {
				nextForkTime = 0x7fffffff;

				#ifdef DEBUG
				printf("process %d target %d servTime %d added to Ready Queue Next Fork Time %d\n",
				procTable[nproc-1].id, procTable[nproc-1].targetServiceTime, procTable[nproc-1].serviceTime, nextForkTime);
				#endif
			}
			run_to_ready = 1;
			
			schedule = 1;
		}
		if (qTime == QUANTUM) { /* CASE 1 : The quantum expires */
			if (runningProc != &idleProc) {
				runningProc->state = S_READY;
				// **push process to Ready Queue
				#ifdef DEBUG
				printf("process %d servT %d targetServT %d Quantum Expires\n", runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
				#endif

				runningProc->priority--;
				//printf("process %d priority is %d\n", runningProc->id, runningProc->priority);
				quantum_did = 1;

				run_to_ready = 1;

				schedule = 1;
			}
		}
		while (ioDoneEventQueue.next->doneTime == currentTime) { /* CASE 3 : IO Done Event */
      // **pop ioDoneEventQueue
      struct ioDoneEvent* iotmp = PopIOQueue();

			#ifdef DEBUG
			printf("IO Done Event for pid %d\n", iotmp->procid);
    	#endif

      // **push Blocked queue member to Ready Queue

			if (procTable[iotmp->procid].state == S_TERMINATE) {
				#ifdef DEBUG
				printf("IO Done Move proc %d Blocked to Terminate\n", iotmp->procid);
				printf("process %d servTime %d targetST %d IODone Event\n", 
				runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
    		#endif
			}
			else {
				struct process* bqtmp = pnpBQ(iotmp->procid);

				#ifdef DEBUG
				printf("**IO Done Move proc %d Blocked to Ready\n", iotmp->procid);
				printf("BlockedQueue Length %d\n", blockedQueue.len);
				printf("process %d servTime %d targetST %d IODone Event\n", 
				runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
    		#endif

				bqtmp->state = S_READY;
      	pushRQ(bqtmp->id);
			}

			if (runningProc->serviceTime != runningProc->targetServiceTime) {
				if (runningProc != &idleProc && cpuUseTime != nextIOReqTime) {
					run_to_ready = 1;
				}
				schedule = 1;
			}
		}
		if (cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
			if (quantum_did == 0) {
				runningProc->priority++;
				//printf("process %d priority is %d\n", runningProc->id, runningProc->priority);
			}

      #ifdef DEBUG
			printf("process %d servTime %d targetST %d IO Req Event\n", 
			runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
    	#endif

      // **Setting IO start function
      ioDoneEvent[nioreq].procid = runningProc->id;
      ioDoneEvent[nioreq].len = ioServTime[nioreq];
      ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq];

      // **push IO to ioDoneEventQueue
      pushIOQueue(nioreq);
			
			#ifdef DEBUG
			printf("process %d io Request : Done Event %d th added with doneTime = %d\n", 
			runningProc->id, nioreq, ioDoneEvent[nioreq].doneTime);
    	#endif

      if (++nioreq < NIOREQ) {
        nextIOReqTime += ioReqIntArrTime[nioreq];
			}
      else
        nextIOReqTime = 0x7fffffff;

			#ifdef DEBUG
				printf("%d th nextIOReqTime %d\n", 
				nioreq, nextIOReqTime);
    	#endif

      // **push process to Blocked Queue
      if (runningProc != &idleProc) {
				if (runningProc->serviceTime == runningProc->targetServiceTime) {
					runningProc->state = S_TERMINATE;
				}
				else {
        	runningProc->state = S_BLOCKED;
        	pushBQ(runningProc->id);
					#ifdef DEBUG
					printf("**Proc %d move to BlockedQueue\n", runningProc->id);
					printf("BlockedQueue Length %d \nReadyQueue Length %d \n", blockedQueue.len, readyQueue.len);
					#endif
					schedule = 1;
				}
      }
		}
		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
      if (runningProc == &idleProc) continue;
			termProc++;
      
			runningProc->state = S_TERMINATE;
			runningProc->endTime = currentTime;
			run_to_ready = 0;
			#ifdef DEBUG
			printf("process %d targetST %d servTime %d terminates\n", 
			runningProc->id, runningProc->serviceTime, runningProc->targetServiceTime);
			printf("**%d-th proc terminated at %d : Process %d\n", termProc, currentTime, runningProc->id);
			printf("ReadyQueue Length %d\nBlockedQueue Length %d \n", readyQueue.len, blockedQueue.len);
    	#endif


			schedule = 1;
			if (termProc == NPROC)
				return;	
		}
		if (run_to_ready == 1 && runningProc->state != S_BLOCKED) {
			qTime = 0;
			if (runningProc != &idleProc) {
	      pushRQ(runningProc->id);

				#ifdef DEBUG
				printf("**Proc %d move to ReadyQueue\n", runningProc->id);
				printf("ReadyQueue Length %d\nBlockedQueue Length %d \n", readyQueue.len, blockedQueue.len);
				#endif
			}
		}
		// call scheduler() if needed
		if (schedule == 1) {
			qTime = 0;
			runningProc = scheduler(); // **set running proc here
			runningProc->state = S_RUNNING;

			#ifdef DEBUG
    	printf("Scheduler selects process %d\n", runningProc->id);
			printf("ReadyQueue Length %d\n", readyQueue.len);
    	#endif
		}
	} // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {   
    return &idleProc;
  }

	tmp->next->prev = &readyQueue;
	readyQueue.next = tmp->next;
	tmp->next = NULL;
	tmp->prev = NULL;

  readyQueue.len--;

	return tmp;
}
struct process* SJFschedule() {
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {   
    return &idleProc;
  }

	struct process* min_process = readyQueue.next;
	int min = 0x7fffffff;

	while(tmp != &readyQueue){
		if (tmp->targetServiceTime < min){
			min_process = tmp;
			min = tmp->targetServiceTime;
		}
		tmp = tmp->next;
	}

	min_process->next->prev = min_process->prev;
	min_process->prev->next = min_process->next;
	min_process->next = NULL;
	min_process->prev = NULL;
	
  readyQueue.len--;

	return min_process;
}
struct process* SRTNschedule() {
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {   
    return &idleProc;
  }

	struct process* min_process = readyQueue.next;
	int min = 0x7fffffff;

	while(tmp != &readyQueue){
		if ((tmp->targetServiceTime - tmp->serviceTime) < min){
			min_process = tmp;
			min = (tmp->targetServiceTime - tmp->serviceTime);
		}
		tmp = tmp->next;
	}

	min_process->next->prev = min_process->prev;
	min_process->prev->next = min_process->next;
	min_process->next = NULL;
	min_process->prev = NULL;
	
  readyQueue.len--;

	return min_process;
}
struct process* GSschedule() {
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {   
    return &idleProc;
  }

	struct process* min_process = readyQueue.next;
	float min = 100;

	while(tmp != &readyQueue) {
		if (((float)tmp->serviceTime / (float)tmp->targetServiceTime) < min) {
			min_process = tmp;
			min = ((float)tmp->serviceTime / (float)tmp->targetServiceTime);
		}
		tmp = tmp->next;
	}

	min_process->next->prev = min_process->prev;
	min_process->prev->next = min_process->next;
	min_process->next = NULL;
	min_process->prev = NULL;
	
  readyQueue.len--;

	return min_process;
}
struct process* SFSschedule() {
	struct process* tmp = readyQueue.next;
	
	if (tmp == &readyQueue) {   
    return &idleProc;
  }

	struct process* max_process;
	int max = -2147483648;

	while(tmp != &readyQueue){
		if ((tmp->priority) > max){
			max_process = tmp;
			max = tmp->priority;
		}
		tmp = tmp->next;
	}

	max_process->next->prev = max_process->prev;
	max_process->prev->next = max_process->next;
	max_process->next = NULL;
	max_process->prev = NULL;
	
  readyQueue.len--;

	return max_process;
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime=0,totalProcServTime=0,totalIOReqIntArrTime=0,totalIOServTime=0;
	long totalWallTime=0, totalRegValue=0;
	for(i=0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0+procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for(i = 0; i < NPROC; i++ ) { 
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for(i = 0; i < NIOREQ; i++ ) { 
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}
	
	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float) totalProcIntArrTime/NPROC, (float) totalProcServTime/NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float) totalIOReqIntArrTime/NIOREQ, (float) totalIOServTime/NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC,NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float) totalWallTime/NPROC, (float) totalRegValue/NPROC);
	
}

void init() {
	idleProc.id = -1;
	idleProc.len = 0;
	idleProc.targetServiceTime = 0x7fffffff;

	ioDoneEventQueue.doneTime = 0x7fffffff;
}

int main(int argc, char *argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;
	
	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n",argv[0]);
		exit(1);
	}
	
	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);
	
	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);
	
	srandom(SEED);
	
	// allocate array structures
	procTable = (struct process *) malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent *) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int *) malloc(sizeof(int) * NPROC);
	procServTime = (int *) malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int *) malloc(sizeof(int) * NIOREQ);
	ioServTime = (int *) malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.next = readyQueue.prev = &readyQueue;
	blockedQueue.next = blockedQueue.prev = &blockedQueue;
	ioDoneEventQueue.next = ioDoneEventQueue.prev = &ioDoneEventQueue;
	ioDoneEventQueue.doneTime = INT_MAX;
	ioDoneEventQueue.procid = -1;
	ioDoneEventQueue.len  = readyQueue.len = blockedQueue.len = 0;
	
	// generate process interarrival times
	for(i = 0; i < NPROC; i++ ) {
		procIntArrTime[i] = random()%(MAX_INT_ARRTIME - MIN_INT_ARRTIME+1) + MIN_INT_ARRTIME;
	}
	
	// assign service time for each process
	for(i=0; i < NPROC; i++) {
		procServTime[i] = random()% (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
    procTable[i].targetServiceTime = procServTime[i];
		totalProcServTime += procServTime[i];	
	}
	
	ioReqAvgIntArrTime = totalProcServTime/(NIOREQ+1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);
	
	// generate io request interarrival time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioReqIntArrTime[i] = random()%((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+1) + MIN_IOREQ_INT_ARRTIME;
	}
	
	// generate io request service time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioServTime[i] = random()%(MAX_IO_SERVTIME - MIN_IO_SERVTIME+1) + MIN_IO_SERVTIME;
	}
	
#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procTable[i].targetServiceTime);
	}
	printf("\n");
#endif
	
	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n",MIN_IOREQ_INT_ARRTIME,
			(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+ MIN_IOREQ_INT_ARRTIME);
			
#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioServTime[i]);
	}
	printf("\n");
#endif
	
	struct process* (*schFunc)();
	switch(SCHEDULING_METHOD) {
		case 1 : schFunc = RRschedule; break;
		case 2 : schFunc = SJFschedule; break;
		case 3 : schFunc = SRTNschedule; break;
		case 4 : schFunc = GSschedule; break;
		case 5 : schFunc = SFSschedule; break;
		default : printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	init();
	initProcTable();
	procExecSim(schFunc);
	printResult();
}
	