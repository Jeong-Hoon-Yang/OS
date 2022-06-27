//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Submission Year: 2021
// Student Name: Yang Jeong Hoon
// Student Number: B711109
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

int numProcess;
int nFrame;
int firstLevelBits;
int secondLevelBits;

int m_pow(int n, int r){
  int tmp = 1;
  while(r){
    if(r & 1)
      tmp *= n;
    r >>= 1;
    n *= n;
  }
  return tmp;
}

struct hashNode {
  int vitual_address;
  int pid;
  int fid;
  struct hashNode *next;
  struct hashNode *prev;
} *hashTable;

struct frameEntry {
  int fid;
  int pid;
  unsigned virtual_address;
  struct hashNode* mapping;
  struct frameEntry* prev;
  struct frameEntry* next;
} * frameTable, LRU;

struct pageTableEntry {
  int valid;
  int plen;
  int fid;
  struct pageTableEntry *secondLevelPageTable;
} * pageTable;

struct procEntry {
  char *traceName;           // the memory trace name
  int pid;                   // process (trace) id
  int ntraces;               // the number of memory traces
  int num2ndLevelPageTable;  // The 2nd level page created(allocated);
  int numIHTConflictAccess;  // The number of Inverted Hash Table Conflict Accesses
  int numIHTNULLAccess;      // The number of Empty Inverted Hash Table Accesses
  int numIHTNonNULLAcess;    // The number of Non Empty Inverted Hash Table Accesses
  int numPageFault;          // The number of page faults
  int numPageHit;            // The number of page hits
  struct pageTableEntry *firstLevelPageTable;
  FILE *tracefp;
} * procTable;

void oneLevelVMSim(int type) {
  
  int i, j, k;
  unsigned addr;
  char rw;
  int next_fnum = 0;
  FILE *file;

  for (i = 0; i < numProcess; i++) {
    procTable[i].firstLevelPageTable = (struct pageTableEntry *)malloc(sizeof(struct pageTableEntry) * m_pow(2, 20));

    for (j = 0; j < m_pow(2, 20); j++) {
      procTable[i].firstLevelPageTable[j].valid = 0;
      procTable[i].firstLevelPageTable[j].fid = -1;
      procTable[i].firstLevelPageTable[j].plen = 0;
    }
  }

  if (type == 0) {
    while (1) {
      for (i = 0; i < numProcess; i++) {
        file = procTable[i].tracefp;

        fscanf(file, "%x %c", &addr, &rw);
        if (feof(file)) break;

        procTable[i].ntraces++;

        addr >>= PAGESIZEBITS;

        if (procTable[i].firstLevelPageTable[addr].valid == 0) {
          procTable[i].firstLevelPageTable[addr].valid = 1;
          procTable[i].firstLevelPageTable[addr].fid = next_fnum;
          frameTable[next_fnum].virtual_address = addr;
          frameTable[next_fnum].pid = i;

          next_fnum = (next_fnum + 1) % nFrame;

          procTable[i].numPageFault++;

          continue;
        }

        for (k = 0; k < nFrame; k++) {
          if (frameTable[k].virtual_address == addr && frameTable[k].pid == i) {
            procTable[i].numPageHit++;
            break;
          }
          if (frameTable[k].pid == -1)
            break;
        }

        if (frameTable[k].pid == -1 || k == nFrame) {
          procTable[i].firstLevelPageTable[addr].valid = 1;
          procTable[i].firstLevelPageTable[addr].fid = next_fnum;
          frameTable[next_fnum].virtual_address = addr;
          frameTable[next_fnum].pid = i;

          next_fnum = (next_fnum + 1) % nFrame;

          procTable[i].numPageFault++;
        }
      }
      if (feof(file)) break;
    }
  } else {
    int i = 0;

    while (1) {
      for (i = 0; i < numProcess; i++) {
        file = procTable[i].tracefp;

        fscanf(file, "%x %c", &addr, &rw);
        if (feof(file)) break;

        procTable[i].ntraces++;

        addr >>= PAGESIZEBITS;

        if (procTable[i].firstLevelPageTable[addr].valid == 0) {
          procTable[i].firstLevelPageTable[addr].valid = 1;

          if (next_fnum < nFrame) {
            procTable[i].firstLevelPageTable[addr].fid = next_fnum;
            frameTable[next_fnum].virtual_address = addr;
            frameTable[next_fnum].pid = i;

            if (LRU.next == &LRU) {
              LRU.next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
              frameTable[next_fnum].prev = &LRU;
              frameTable[next_fnum].next = &LRU;
            } else {
              frameTable[next_fnum].prev = LRU.prev;
              frameTable[next_fnum].next = &LRU;
              LRU.prev->next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
            }

            next_fnum = next_fnum + 1;
          } else {
            int tmp = LRU.next->fid;

            procTable[i].firstLevelPageTable[addr].fid = tmp;
            frameTable[tmp].virtual_address = addr;
            frameTable[tmp].pid = i;

            frameTable[tmp].next->prev = frameTable[tmp].prev;
            frameTable[tmp].prev->next = frameTable[tmp].next;

            frameTable[tmp].prev = LRU.prev;
            frameTable[tmp].next = &LRU;
            LRU.prev->next = &frameTable[tmp];
            LRU.prev = &frameTable[tmp];
          }

          procTable[i].numPageFault++;

          continue;
        }

        for (k = 0; k < nFrame; k++) {
          if (frameTable[k].virtual_address == addr && frameTable[k].pid == i) {
            frameTable[k].next->prev = frameTable[k].prev;
            frameTable[k].prev->next = frameTable[k].next;

            frameTable[k].prev = LRU.prev;
            frameTable[k].next = &LRU;
            LRU.prev->next = &frameTable[k];
            LRU.prev = &frameTable[k];

            procTable[i].numPageHit++;
            break;
          }
        }

        if (k >= nFrame) {
          procTable[i].firstLevelPageTable[addr].valid = 1;

          if (next_fnum < nFrame) {
            procTable[i].firstLevelPageTable[addr].fid = next_fnum;
            frameTable[next_fnum].virtual_address = addr;
            frameTable[next_fnum].pid = i;

            if (LRU.next == &LRU) {
              LRU.next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
              frameTable[next_fnum].prev = &LRU;
              frameTable[next_fnum].next = &LRU;
            } else {
              frameTable[next_fnum].prev = LRU.prev;
              frameTable[next_fnum].next = &LRU;
              LRU.prev->next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
            }

            next_fnum = next_fnum + 1;
          } else {
            int tmp = LRU.next->fid;

            procTable[i].firstLevelPageTable[addr].fid = tmp;
            frameTable[tmp].virtual_address = addr;
            frameTable[tmp].pid = i;

            frameTable[tmp].next->prev = frameTable[tmp].prev;
            frameTable[tmp].prev->next = frameTable[tmp].next;

            frameTable[tmp].prev = LRU.prev;
            frameTable[tmp].next = &LRU;
            LRU.prev->next = &frameTable[tmp];
            LRU.prev = &frameTable[tmp];
          }

          procTable[i].numPageFault++;
        }
      }
      if (feof(file)) break;
    }
  }
  

  for (i = 0; i < numProcess; i++) {
    printf("**** %s *****\n", procTable[i].traceName);
    printf("Proc %d Num of traces %d\n", i, procTable[i].ntraces);
    printf("Proc %d Num of Page Faults %d\n", i, procTable[i].numPageFault);
    printf("Proc %d Num of Page Hit %d\n", i, procTable[i].numPageHit);
    assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
  }
}

void twoLevelVMSim() {
  int i, j, k;
  unsigned addr;
  unsigned first_addr, second_addr;
  char rw;
  int next_fnum = 0;
  FILE *file;

  for (i = 0; i < numProcess; i++) {
    procTable[i].firstLevelPageTable = (struct pageTableEntry *)malloc(sizeof(struct pageTableEntry) * m_pow(2, firstLevelBits));

    for (j = 0; j < m_pow(2, firstLevelBits); j++) {
      procTable[i].firstLevelPageTable[j].valid = 0;
      procTable[i].firstLevelPageTable[j].fid = -1;
      procTable[i].firstLevelPageTable[j].plen = 0;
    }
  }

  while (1) {
    for (i = 0; i < numProcess; i++) {
      file = procTable[i].tracefp;

      fscanf(file, "%x %c", &addr, &rw);

      if (feof(file)) break;

      procTable[i].ntraces++;

      first_addr = addr >> (VIRTUALADDRBITS - firstLevelBits);
      second_addr = addr << firstLevelBits;
      second_addr = second_addr >> (VIRTUALADDRBITS - secondLevelBits);

      if (procTable[i].firstLevelPageTable[first_addr].valid == 0) {
        procTable[i].firstLevelPageTable[first_addr].valid = 1;

        procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable = (struct pageTableEntry*)malloc(sizeof(struct pageTableEntry) * m_pow(2,secondLevelBits));

        for (j = 0; j < m_pow(2, secondLevelBits); j++) {
          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[j].valid = 0;
          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[j].fid = -1;
          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[j].plen = 0;
        }

        procTable[i].num2ndLevelPageTable++;
      }

      if (procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].valid == 0) {
        procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].valid = 1;

        if (next_fnum < nFrame) {
          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].fid = next_fnum;
          frameTable[next_fnum].virtual_address = addr >> 12;
          frameTable[next_fnum].pid = i;

          if (LRU.next == &LRU) {
            LRU.next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
            frameTable[next_fnum].prev = &LRU;
            frameTable[next_fnum].next = &LRU;
          } else {
            frameTable[next_fnum].prev = LRU.prev;
            frameTable[next_fnum].next = &LRU;
            LRU.prev->next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
          }

          next_fnum = next_fnum + 1;
        } else {
          int tmp = LRU.next->fid;

          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].fid = tmp;
          frameTable[tmp].virtual_address = addr >> 12;
          frameTable[tmp].pid = i;

          frameTable[tmp].next->prev = frameTable[tmp].prev;
          frameTable[tmp].prev->next = frameTable[tmp].next;

          frameTable[tmp].prev = LRU.prev;
          frameTable[tmp].next = &LRU;
          LRU.prev->next = &frameTable[tmp];
          LRU.prev = &frameTable[tmp];
        }

        procTable[i].numPageFault++;

        continue;
      }

      for (k = 0; k < nFrame; k++) {
        if (frameTable[k].virtual_address == addr >> 12 && frameTable[k].pid == i) {
          frameTable[k].next->prev = frameTable[k].prev;
          frameTable[k].prev->next = frameTable[k].next;

          frameTable[k].prev = LRU.prev;
          frameTable[k].next = &LRU;
          LRU.prev->next = &frameTable[k];
          LRU.prev = &frameTable[k];

          procTable[i].numPageHit++;
          break;
        }
      }

      if (k >= nFrame) {
        procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].valid = 1;

        if (next_fnum < nFrame) {
          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].fid = next_fnum;
          frameTable[next_fnum].virtual_address = addr >> 12;
          frameTable[next_fnum].pid = i;

          if (LRU.next == &LRU) {
            LRU.next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
            frameTable[next_fnum].prev = &LRU;
            frameTable[next_fnum].next = &LRU;
          } else {
            frameTable[next_fnum].prev = LRU.prev;
            frameTable[next_fnum].next = &LRU;
            LRU.prev->next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
          }

          next_fnum = next_fnum + 1;
        } else {
          int tmp = LRU.next->fid;

          procTable[i].firstLevelPageTable[first_addr].secondLevelPageTable[second_addr].fid = tmp;
          frameTable[tmp].virtual_address = addr >> 12;
          frameTable[tmp].pid = i;

          frameTable[tmp].next->prev = frameTable[tmp].prev;
          frameTable[tmp].prev->next = frameTable[tmp].next;

          frameTable[tmp].prev = LRU.prev;
          frameTable[tmp].next = &LRU;
          LRU.prev->next = &frameTable[tmp];
          LRU.prev = &frameTable[tmp];
        }

        procTable[i].numPageFault++;
      }
    }
    if (feof(file)) break;
  }
  
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim() {
  int i, j;
  int hit = 0;
  unsigned addr;
  char rw;
  int next_fnum = 0;
  FILE *file;

  while (1) {
    for (i = 0; i < numProcess; i++) {
      hit = 0;


      file = procTable[i].tracefp;

      fscanf(file, "%x %c", &addr, &rw);

      if (feof(file)) break;

      procTable[i].ntraces++;

      addr >>= PAGESIZEBITS;

      int hash_index;
      hash_index = (addr + i) % nFrame;

      struct hashNode* tmp;
      tmp = &hashTable[hash_index];

      if (tmp->next == NULL) {
        procTable[i].numIHTNULLAccess++;

        struct hashNode* new_node;
        new_node = (struct hashNode *)malloc(sizeof(struct hashNode));
        new_node->pid = i;
        new_node->vitual_address = addr;

        new_node->next = NULL;
        tmp->next = new_node;
        new_node->prev = tmp;

        if (next_fnum < nFrame) {
          new_node->fid = next_fnum;
          frameTable[next_fnum].virtual_address = addr;
          frameTable[next_fnum].pid = i;
          frameTable[next_fnum].mapping = new_node;

          if (LRU.next == &LRU) {
            LRU.next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
            frameTable[next_fnum].prev = &LRU;
            frameTable[next_fnum].next = &LRU;
          } else {
            frameTable[next_fnum].prev = LRU.prev;
            frameTable[next_fnum].next = &LRU;
            LRU.prev->next = &frameTable[next_fnum];
            LRU.prev = &frameTable[next_fnum];
          }

          next_fnum = next_fnum + 1;
        } else {
          int f_tmp = LRU.next->fid;

          frameTable[f_tmp].mapping->prev->next = frameTable[f_tmp].mapping->next;
          if (frameTable[f_tmp].mapping->next != NULL)
            frameTable[f_tmp].mapping->next->prev = frameTable[f_tmp].mapping->prev;

          frameTable[f_tmp].mapping = new_node;

          new_node->fid = f_tmp;
          frameTable[f_tmp].virtual_address = addr;
          frameTable[f_tmp].pid = i;

          frameTable[f_tmp].next->prev = frameTable[f_tmp].prev;
          frameTable[f_tmp].prev->next = frameTable[f_tmp].next;

          frameTable[f_tmp].prev = LRU.prev;
          frameTable[f_tmp].next = &LRU;
          LRU.prev->next = &frameTable[f_tmp];
          LRU.prev = &frameTable[f_tmp];
        }
        
        procTable[i].numPageFault++;
        
        continue;
      } else {
        procTable[i].numIHTNonNULLAcess++;

        while (tmp->next != NULL) {
          tmp = tmp->next;
          procTable[i].numIHTConflictAccess++;

          if (tmp->pid == i && tmp->vitual_address == addr) {
            hit = 1;
            
            frameTable[tmp->fid].next->prev = frameTable[tmp->fid].prev;
            frameTable[tmp->fid].prev->next = frameTable[tmp->fid].next;

            frameTable[tmp->fid].prev = LRU.prev;
            frameTable[tmp->fid].next = &LRU;
            LRU.prev->next = &frameTable[tmp->fid];
            LRU.prev = &frameTable[tmp->fid];

            procTable[i].numPageHit++;
            break;
          }
        }

        if (tmp->next == NULL && hit == 0) {
          tmp = &hashTable[hash_index];

          struct hashNode *new_node;
          new_node = (struct hashNode *)malloc(sizeof(struct hashNode));
          new_node->pid = i;
          new_node->vitual_address = addr;

          if (next_fnum < nFrame) {
            new_node->fid = next_fnum;
            frameTable[next_fnum].virtual_address = addr;
            frameTable[next_fnum].pid = i;
            frameTable[next_fnum].mapping = new_node;

            if (LRU.next == &LRU) {
              LRU.next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
              frameTable[next_fnum].prev = &LRU;
              frameTable[next_fnum].next = &LRU;
            } else {
              frameTable[next_fnum].prev = LRU.prev;
              frameTable[next_fnum].next = &LRU;
              LRU.prev->next = &frameTable[next_fnum];
              LRU.prev = &frameTable[next_fnum];
            }

            new_node->next = tmp->next;

            if (tmp->next != NULL) {
              tmp->next->prev = new_node;
            }

            tmp->next = new_node;
            new_node->prev = tmp;

            next_fnum = next_fnum + 1;
          } else {
            int f_tmp = LRU.next->fid;

            frameTable[f_tmp].mapping->prev->next = frameTable[f_tmp].mapping->next;
            if (frameTable[f_tmp].mapping->next != NULL)
              frameTable[f_tmp].mapping->next->prev = frameTable[f_tmp].mapping->prev;

            frameTable[f_tmp].mapping = new_node;

            new_node->fid = f_tmp;
            frameTable[f_tmp].virtual_address = addr;
            frameTable[f_tmp].pid = i;

            frameTable[f_tmp].next->prev = frameTable[f_tmp].prev;
            frameTable[f_tmp].prev->next = frameTable[f_tmp].next;

            frameTable[f_tmp].prev = LRU.prev;
            frameTable[f_tmp].next = &LRU;
            LRU.prev->next = &frameTable[f_tmp];
            LRU.prev = &frameTable[f_tmp];

            new_node->next = tmp->next;

            if (tmp->next != NULL) {
              tmp->next->prev = new_node;
            }

            tmp->next = new_node;
            new_node->prev = tmp;
          }
          procTable[i].numPageFault++;
        }
      }
    }
    if (feof(file)) break;
  }

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int i,c, simType;
  int optind = 1;
  int phyMemSizeBits;
	
  numProcess = argc - 4;
  simType = atoi(argv[1]);
  firstLevelBits = atoi(argv[2]);
  secondLevelBits = VIRTUALADDRBITS - 12 - firstLevelBits;
  phyMemSizeBits = atoi(argv[3]);
  if(phyMemSizeBits < 12) {
    printf("ERROR: too small phyMemSizeBits...");
    exit(1);
  }
  nFrame = m_pow(2, (phyMemSizeBits - 12));

  procTable = (struct procEntry *) malloc(sizeof(struct procEntry) * numProcess);
  frameTable = (struct frameEntry *) malloc(sizeof(struct frameEntry) * nFrame);
  hashTable = (struct hashNode *) malloc(sizeof(struct hashNode) * nFrame);

  for (i = 0; i < numProcess; i++) {
    procTable[i].pid = i;      // process (trace) id
    procTable[i].ntraces = 0;  // the number of memory traces
    procTable[i].num2ndLevelPageTable = 0;	// The 2nd level page created(allocated);
    procTable[i].numIHTConflictAccess = 0; 	// The number of Inverted Hash Table Conflict Accesses
    procTable[i].numIHTNULLAccess = 0;		// The number of Empty Inverted Hash Table Accesses
    procTable[i].numIHTNonNULLAcess = 0;		// The number of Non Empty Inverted Hash Table Accesses
    procTable[i].numPageFault = 0;      // The number of page faults
    procTable[i].numPageHit = 0;        // The number of page hits
  }

  for (i = 0; i < nFrame; i++) {
    frameTable[i].fid = i;
    frameTable[i].pid = -1;
    frameTable[i].virtual_address = -1;
    frameTable[i].mapping = NULL;
    frameTable[i].prev = NULL;
    frameTable[i].next = NULL;

    hashTable[i].vitual_address = -1;
    hashTable[i].pid = -1;
    hashTable[i].fid = -1;
    hashTable[i].next = NULL;
    hashTable[i].prev = NULL;
  }

  // initialize procTable for Memory Simulations
	for(i = 0; i < numProcess; i++) {
		// opening a tracefile for the process
		printf("process %d opening %s\n",i,argv[i + optind + 3]);
		procTable[i].tracefp = fopen(argv[i + optind + 3],"r");
    procTable[i].traceName = argv[i+optind+3];
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...",argv[i+optind+3]);
			exit(1);
		}
	}

  LRU.next = &LRU;
  LRU.prev = &LRU;

	printf("Num of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
	
	if (simType == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(0);
	}
	
	if (simType == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(1);
	}
	
	if (simType == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim();
	}
	
	if (simType == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim();
	}

	return(0);
}