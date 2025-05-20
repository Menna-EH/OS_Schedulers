#ifndef OS_STRUCTURES_H
#define OS_STRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Process states
typedef enum { READY, RUNNING, BLOCKED, TERMINATED } ProcessState;

// Scheduler types
typedef enum { FCFS, ROUND_ROBIN, MLFQ } SchedulerType;

// Process Control Block (PCB)
typedef struct {
    int process_id;              // Unique process identifier
    ProcessState state;          // READY, RUNNING, BLOCKED, TERMINATED
    int priority;               // 1 (highest) to 4 (lowest) for MLFQ
    int blocked_priority; 
    int program_counter;         // Index of next instruction to execute
    int memory_lower_bound;      // Start index in memory
    int memory_upper_bound;      // End index in memory
    int instruction_count;       // Number of instructions in the program
        // Simple queue time counters
        int time_in_ready;     // Tracks cycles spent in ready state
        int time_in_blocked;   // Tracks cycles spent in blocked state
} PCB;

typedef struct {
    PCB** processes;
    int count;
    int capacity;
} Queue;

typedef struct {
    char* resource_name;    // Name of resource (userInput, userOutput, file)
    int is_locked;          // 0 (free) or 1 (locked)
    PCB* holding_process;   // PCB of process holding the mutex
    Queue blocked_queue;    // Queue of blocked processes
} Mutex;

// // Queue for ready and blocked processes
// typedef struct {
//     PCB** processes;             // Array of PCB pointers
//     int count;                   // Number of processes in queue
//     int capacity;                // Maximum capacity of the queue
// } Queue;

// // Mutex for shared resources
// typedef struct {
//     char* resource_name;         // Name of resource (userInput, userOutput, file)
//     int is_locked;               // 0 (free) or 1 (locked)
//     PCB* holding_process;        // PCB of process holding the mutex
//     PCB** blocked_queue;         // Processes blocked on this mutex
//     int blocked_count;           // Number of blocked processes
//     int blocked_capacity;        // Capacity of blocked queue
// } Mutex;

// Memory word (instruction or variable)
typedef struct {
    char* name;                  // Identifier for the memory word (e.g., instruction index or variable name)
    char* data;                  // Content (e.g., instruction string or variable value)
} MemoryWord;

// MLFQ-specific state
typedef struct {
    PCB* process;                // Current running process
    int time_run;                // Cycles run in current quantum
} MLFQState;

// System state
typedef struct {
    int clock_cycle;             // Current clock cycle
    SchedulerType scheduler;     // FCFS, ROUND_ROBIN, or MLFQ
    int quantum;                 // User-defined quantum (for RR or MLFQ priority 4)
    PCB* running_process;        // Current running process
    Queue ready_queues[4];           // ready queue
    Queue Blocked_queue;         // General blocked queue
    Queue terminated_queue;
    Mutex mutexes[3];            // Mutexes for userInput, userOutput, file
    MemoryWord memory[60];       // 60-word memory
} SystemState;
extern SystemState os_system;

// Process creation info (for arrival times)
typedef struct {
    int process_id;
    char* program_file;
    int arrival_time;
    int start_index;
} ProcessInfo;
// GUI State
typedef struct {
    bool running;
    bool step_mode;
    bool show_memory;
    int selected_scheduler;
    int quantum;
    char log_text[1024][256];
    int log_count;
    int scroll_index;
    
    // Add these new members for process management
    ProcessInfo processes[10];      // Array to store processes
    int process_count;             // Current number of processes
    int next_process_id;           // ID for the next process to be added
    char process_file_path[256];   // Buffer for file path input
    int new_process_arrival;       // Arrival time for new process
    int new_process_start;         // Start index for new process
    float log_scroll_offset; // Add this

} GUIState;
// Function declarations
void init_memory();
char* get_variable_value(PCB* pcb, const char* var_name);
void set_variable(PCB* pcb, const char* var_name, const char* value);
void readProgramSyntax(char* instruction);
void print(const char* output);
void assign(const char* variable, const char* value);
void writeFile(const char* fileNameArg, const char* dataArg);
char* readFile(const char* fileNameArg);
void printFromTo(const char* argA, const char* argB);
void semWait(const char* resource);
void semSignal(const char* resource);
void store_instructions(int start_index, char** instructions, int instruction_count);
void init_variables(int start_index, int instruction_count, int variable_count, char** var_names);
char* trim(char* str);
void store_pcb(int start_index, int instruction_count, int variable_count, PCB* pcb);
void retrieve_pcb(int start_index, int instruction_count, int variable_count, PCB* pcb);
char* get_instruction(PCB* pcb);
void enqueue(Queue* queue, PCB* pcb);
PCB* dequeue_highest_priority(Queue* queue);
int create_process(int process_id, char* program_file, int start_index);
void init_system(SchedulerType scheduler, int quantum);
PCB* dequeue(Queue* queue);
PCB* schedule_fcfs();
PCB* schedule_round_robin(int* time_run);
PCB* schedule_mlfq(int* time_run);
void run_system(ProcessInfo* processes, int process_count);
void run_system_step(ProcessInfo* processes, int process_count);
void AddLogMessage(GUIState *gui_state, const char *message);



#endif