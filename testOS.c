#include "osproj.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

SystemState os_system;  // Define the system state here
static int quanta[4] = {1, 2, 4, 8};
extern GUIState gui_state;
char buffer[256];

int time_run_step = 0;
// Forward declarations
void print(const char* output);
void assign(const char* variable, const char* value);
void writeFile(const char* fileNameArg, const char* dataArg);
char* readFile(const char* fileNameArg);
void printFromTo(const char* argA, const char* argB);
void semWait(const char* resource);
void semSignal(const char* resource);
void UpdateQueueTimes(SystemState* system);

void UpdateQueueTimes(SystemState* system) {
    // Update ready queue counters
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < system->ready_queues[i].count; j++) {
            system->ready_queues[i].processes[j]->time_in_ready++;
        }
    }
    
    // Update blocked queue counters
    for (int i = 0; i < system->Blocked_queue.count; i++) {
        system->Blocked_queue.processes[i]->time_in_blocked++;
    }
}

void init_memory() {
    for (int i = 0; i < 60; i++) {
        os_system.memory[i].name = NULL;
        os_system.memory[i].data = NULL;
    }
}

char* get_variable_value(PCB* pcb, const char* var_name) {
    for (int i = pcb->memory_lower_bound; i <= pcb->memory_upper_bound; i++) {
        if (os_system.memory[i].name && strcmp(os_system.memory[i].name, var_name) == 0) {
            return os_system.memory[i].data;
        }
    }
    return NULL;
}

void set_variable(PCB* pcb, const char* var_name, const char* value) {
    for (int i = pcb->memory_lower_bound; i <= pcb->memory_upper_bound; i++) {
        if (os_system.memory[i].name && strcmp(os_system.memory[i].name, var_name) == 0) {
            free(os_system.memory[i].data);
            os_system.memory[i].data = strdup(value);
            return;
        }
    }
    // If variable doesn't exist, insert into an empty spot
    for (int i = pcb->memory_lower_bound; i <= pcb->memory_upper_bound; i++) {
        if (os_system.memory[i].name == NULL) {
            os_system.memory[i].name = strdup(var_name);
            os_system.memory[i].data = strdup(value);
            return;
        }
    }
}


void readProgramSyntax(char *instruction) {
    // Duplicate the instruction to avoid modifying the original
    char *instruction_copy = strdup(instruction);
    if (!instruction_copy) {
        printf("Memory allocation failed\n");
        return;
    }


    char *inst_line[4] = {NULL}; // Initialize array to NULL
    int i = 0;

    // Tokenize the instruction
    char *token = strtok(instruction_copy, " ");
    while (token != NULL && i < 4) {
        inst_line[i++] = token;
        token = strtok(NULL, " ");
    }

    // Determine operation and arguments
    char *operation = inst_line[0];
    char *argA = (i > 1) ? inst_line[1] : NULL;
    char *argB = NULL;

    // Handle case where instruction has 4 parts
    if (i == 4) {
        // Allocate memory for argB (third + space + fourth + null terminator)
        size_t len = strlen(inst_line[2]) + strlen(inst_line[3]) + 2; // +2 for space and \0
        argB = (char *)malloc(len);
        if (!argB) {
            printf("Memory allocation failed\n");
            free(instruction_copy);
            return;
        }
        snprintf(argB, len, "%s %s", inst_line[2], inst_line[3]);
    } else if (i > 2) {
        argB = inst_line[2]; // Use third token directly if no fourth token
    }

    // Execute the appropriate instruction
    if (operation) {
        if (strcmp(operation, "print") == 0 && argA) {
            print(argA);
        } else if (strcmp(operation, "assign") == 0 && argA && argB) {
            assign(argA, argB);
        } else if (strcmp(operation, "writeFile") == 0 && argA && argB) {
            writeFile(argA, argB);
        } else if (strcmp(operation, "readFile") == 0 && argA) {
            readFile(argA);
        } else if (strcmp(operation, "printFromTo") == 0 && argA && argB) {
            printFromTo(argA, argB);
        } else if (strcmp(operation, "semWait") == 0 && argA) {
            semWait(argA);
        } else if (strcmp(operation, "semSignal") == 0 && argA) {
            semSignal(argA);
        } else {
            printf("program syntax error\n");
        }
    } else {
        
        printf("program syntax error\n");
    }

    // Clean up
    if (i == 4) {
        free(argB); // Free allocated argB
    }
    free(instruction_copy); // Free duplicated instruction
}

void print(const char* output) {
    char* output2 = get_variable_value(os_system.running_process, output);
    if (output2) {
        printf("%s\n", output2);
    } else {
        printf("Variable not found: %s\n", output);
    }
}


void assign(const char* variable, const char* value) {
    // printf("v %s", value); 
    // printf("\n"); 
    if (strcmp(value, "input") == 0) {
        char input[100];
        printf("Please enter a value: ");
        scanf("%99s", input);
        set_variable(os_system.running_process, variable, input);
    } else if (strncmp(value, "readFile ", 9) == 0) {
        const char* fileNameVar = value + 9; // Gets "a"
        char* fileName = get_variable_value(os_system.running_process, fileNameVar); // Resolve "a" to "esra"
        if (!fileName) {
            printf("assign error: variable %s not found\n", fileNameVar);
            return;
        }
        char *value_read = readFile(fileName);
        if (value_read && value_read[0] != '\0') {
            set_variable(os_system.running_process, variable, value_read);
            free(value_read);
        } else {
            printf("assign error: readFile failed for %s\n", fileName);
            if (value_read) free(value_read);
        }
    
        // if (valFromVar) {
        //     set_variable(os_system.running_process, variable, valFromVar);
        // } 
    }
    else {
        set_variable(os_system.running_process, variable, value);
    }
}

void writeFile(const char* fileNameArg, const char* dataArg) {
    if (!fileNameArg || !dataArg) {
        printf("writeFile error: file name or data is NULL.\n");
        return;
    }
    // Resolve variable values
    char* fileName = get_variable_value(os_system.running_process, fileNameArg);
    char* data = get_variable_value(os_system.running_process, dataArg);
    if (!fileName || !data) {
        printf("writeFile error: variable not found (%s or %s)\n", fileNameArg, dataArg);
        return;
    }
    FILE* f = fopen(fileName, "w");
    if (!f) {
        printf("Could not open file %s for writing.\n", fileName);
        return;
    }
    fprintf(f, "%s", data);
    fclose(f);
    printf("File '%s' created and written successfully.\n", fileName);
}


char* readFile(const char* fileNameArg) {

    FILE* f = fopen(fileNameArg, "r");
    printf("file : %s",fileNameArg); 
    if (!f) {
        printf("Error: Could not open file %s.\n", fileNameArg);
        return NULL;
    }

    // Read file content into buffer
    char* buffer = malloc(1000); // Allocate memory for the string
    if (fgets(buffer, 1000, f)) {
        buffer[strcspn(buffer, "\n")] = '\0'; // Remove newline
    } else {
        buffer[0] = '\0'; // Empty string if read fails
    }
    printf("%s", buffer);

    fclose(f);
    return buffer; // Return the string
}



void printFromTo(const char* argA, const char* argB) {
    char* valA = get_variable_value(os_system.running_process, argA);
    char* valB = get_variable_value(os_system.running_process, argB);

    if (!valA || !valB) {
        printf("printFromTo error: variables not found\n");
        return;
    }

    int numA = atoi(valA);
    int numB = atoi(valB);

    for (int i = numA; i <= numB; i++) {
        printf("%d\n", i);
    }
}

//memory 

void store_instructions(int start_index, char** instructions, int instruction_count) {
    for (int i = 0; i < instruction_count; i++) {
        char name[20];
        snprintf(name, sizeof(name), "instr_%d", i);
        os_system.memory[start_index + i].name = strdup(name);
        os_system.memory[start_index + i].data = strdup(instructions[i]);
    }
}

void init_variables(int start_index, int instruction_count, int variable_count, char** var_names) {
    for (int i = 0; i < variable_count; i++) {
        os_system.memory[start_index + instruction_count + i].name = strdup(var_names[i]);
        os_system.memory[start_index + instruction_count + i].data = NULL;
    }
}

char* trim(char* str) {
    while (*str == ' ' || *str == '\t') str++;
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n')) *end-- = '\0';
    return str;
}

//store pcb
void store_pcb(int start_index, int instruction_count, int variable_count, PCB* pcb) {
    // PCB stored after instructions and variables
    int pcb_start = start_index + instruction_count + variable_count;

    // Validate inputs
    if (!pcb || pcb_start + 6 >= 60) {
        printf("store_pcb error: invalid PCB or memory index %d for process %d\n", pcb_start, pcb->process_id);
        return;
    }

    // Store process_id
    if (os_system.memory[pcb_start].name) free(os_system.memory[pcb_start].name);
    if (os_system.memory[pcb_start].data) free(os_system.memory[pcb_start].data);
    os_system.memory[pcb_start].name = strdup("process_id");
    char pid_str[10];
    snprintf(pid_str, sizeof(pid_str), "%d", pcb->process_id);
    os_system.memory[pcb_start].data = strdup(pid_str);

    // Store state
    if (os_system.memory[pcb_start + 1].name) free(os_system.memory[pcb_start + 1].name);
    if (os_system.memory[pcb_start + 1].data) free(os_system.memory[pcb_start + 1].data);
    os_system.memory[pcb_start + 1].name = strdup("state");
    switch (pcb->state) {
        case READY: os_system.memory[pcb_start + 1].data = strdup("READY"); break;
        case RUNNING: os_system.memory[pcb_start + 1].data = strdup("RUNNING"); break;
        case BLOCKED: os_system.memory[pcb_start + 1].data = strdup("BLOCKED"); break;
        case TERMINATED: os_system.memory[pcb_start + 1].data = strdup("TERMINATED"); break;
        default: os_system.memory[pcb_start + 1].data = strdup("UNKNOWN");
    }

    // Store priority
    if (os_system.memory[pcb_start + 2].name) free(os_system.memory[pcb_start + 2].name);
    if (os_system.memory[pcb_start + 2].data) free(os_system.memory[pcb_start + 2].data);
    os_system.memory[pcb_start + 2].name = strdup("priority");
    char priority_str[10];
    snprintf(priority_str, sizeof(priority_str), "%d", pcb->priority);
    os_system.memory[pcb_start + 2].data = strdup(priority_str);

    // Store program_counter
    if (os_system.memory[pcb_start + 3].name) free(os_system.memory[pcb_start + 3].name);
    if (os_system.memory[pcb_start + 3].data) free(os_system.memory[pcb_start + 3].data);
    os_system.memory[pcb_start + 3].name = strdup("program_counter");
    char pc_str[10];
    snprintf(pc_str, sizeof(pc_str), "%d", pcb->program_counter);
    os_system.memory[pcb_start + 3].data = strdup(pc_str);

    // Store memory_lower_bound
    if (os_system.memory[pcb_start + 4].name) free(os_system.memory[pcb_start + 4].name);
    if (os_system.memory[pcb_start + 4].data) free(os_system.memory[pcb_start + 4].data);
    os_system.memory[pcb_start + 4].name = strdup("memory_lower_bound");
    char lower_str[10];
    snprintf(lower_str, sizeof(lower_str), "%d", pcb->memory_lower_bound);
    os_system.memory[pcb_start + 4].data = strdup(lower_str);

    // Store memory_upper_bound
    if (os_system.memory[pcb_start + 5].name) free(os_system.memory[pcb_start + 5].name);
    if (os_system.memory[pcb_start + 5].data) free(os_system.memory[pcb_start + 5].data);
    os_system.memory[pcb_start + 5].name = strdup("memory_upper_bound");
    char upper_str[10];
    snprintf(upper_str, sizeof(upper_str), "%d", pcb->memory_upper_bound);
    os_system.memory[pcb_start + 5].data = strdup(upper_str);

    // // Store instruction_count
    // if (os_system.memory[pcb_start + 6].name) free(os_system.memory[pcb_start + 6].name);
    // if (os_system.memory[pcb_start + 6].data) free(os_system.memory[pcb_start + 6].data);
    // os_system.memory[pcb_start + 6].name = strdup("instruction_count");
    // char ic_str[10];
    // snprintf(ic_str, sizeof(ic_str), "%d", pcb->instruction_count);
    // os_system.memory[pcb_start + 6].data = strdup(ic_str);
}

// Retrieve and populate PCB from memory
void retrieve_pcb(int start_index, int instruction_count, int variable_count, PCB* pcb) {
    // PCB stored after instructions and variables
    int pcb_start = start_index + instruction_count + variable_count;

    // Validate inputs
    if (!pcb || pcb_start + 6 >= 60) {
        printf("retrieve_pcb error: invalid PCB or memory index %d\n", pcb_start);
        return;
    }

    // Retrieve process_id
    if (os_system.memory[pcb_start].name && strcmp(os_system.memory[pcb_start].name, "process_id") == 0 && os_system.memory[pcb_start].data) {
        pcb->process_id = atoi(os_system.memory[pcb_start].data);
    } else {
        printf("retrieve_pcb error: invalid process_id at index %d\n", pcb_start);
        return;
    }

    // Retrieve state
    if (os_system.memory[pcb_start + 1].name && strcmp(os_system.memory[pcb_start + 1].name, "state") == 0 && os_system.memory[pcb_start + 1].data) {
        if (strcmp(os_system.memory[pcb_start + 1].data, "READY") == 0) pcb->state = READY;
        else if (strcmp(os_system.memory[pcb_start + 1].data, "RUNNING") == 0) pcb->state = RUNNING;
        else if (strcmp(os_system.memory[pcb_start + 1].data, "BLOCKED") == 0) pcb->state = BLOCKED;
        else if (strcmp(os_system.memory[pcb_start + 1].data, "TERMINATED") == 0) pcb->state = TERMINATED;
        else pcb->state = READY; // Default
    } else {
        printf("retrieve_pcb error: invalid state at index %d\n", pcb_start + 1);
        return;
    }

    // Retrieve priority
    if (os_system.memory[pcb_start + 2].name && strcmp(os_system.memory[pcb_start + 2].name, "priority") == 0 && os_system.memory[pcb_start + 2].data) {
        pcb->priority = atoi(os_system.memory[pcb_start + 2].data);
    } else {
        printf("retrieve_pcb error: invalid priority at index %d\n", pcb_start + 2);
        return;
    }

    // Retrieve program_counter
    if (os_system.memory[pcb_start + 3].name && strcmp(os_system.memory[pcb_start + 3].name, "program_counter") == 0 && os_system.memory[pcb_start + 3].data) {
        pcb->program_counter = atoi(os_system.memory[pcb_start + 3].data);
    } else {
        printf("retrieve_pcb error: invalid program_counter at index %d\n", pcb_start + 3);
        return;
    }

    // Retrieve memory_lower_bound
    if (os_system.memory[pcb_start + 4].name && strcmp(os_system.memory[pcb_start + 4].name, "memory_lower_bound") == 0 && os_system.memory[pcb_start + 4].data) {
        pcb->memory_lower_bound = atoi(os_system.memory[pcb_start + 4].data);
    } else {
        printf("retrieve_pcb error: invalid memory_lower_bound at index %d\n", pcb_start + 4);
        return;
    }

    // Retrieve memory_upper_bound
    if (os_system.memory[pcb_start + 5].name && strcmp(os_system.memory[pcb_start + 5].name, "memory_upper_bound") == 0 && os_system.memory[pcb_start + 5].data) {
        pcb->memory_upper_bound = atoi(os_system.memory[pcb_start + 5].data);
    } else {
        printf("retrieve_pcb error: invalid memory_upper_bound at index %d\n", pcb_start + 5);
        return;
    }

    // Retreive instruction_count
    if (os_system.memory[pcb_start + 6].name && strcmp(os_system.memory[pcb_start + 6].name, "instruction_count") == 0 && os_system.memory[pcb_start + 6].data) {
        pcb->instruction_count = atoi(os_system.memory[pcb_start + 6].data);
    } else {
        printf("retrieve_pcb error: invalid instruction_count at index %d\n", pcb_start + 6);
        return;
    }
}

// Get instruction at program counter
char* get_instruction(PCB* pcb) {
    if (pcb->program_counter < pcb->instruction_count) {
        int index = pcb->memory_lower_bound + pcb->program_counter;
        if (index <= pcb->memory_upper_bound && os_system.memory[index].name) {
            return os_system.memory[index].data;
        }
    }
    return NULL;
}
// char* get_instruction(PCB* pcb) {
//     int index = pcb->memory_lower_bound + pcb->program_counter;
//     if (index <= pcb->memory_upper_bound && os_system.memory[index].name) {
//         return os_system.memory[index].data;
//     }
//     return NULL;
// }

//enqueue
void enqueue(Queue* queue, PCB* pcb) {
    if (!queue || !pcb) {
        printf("enqueue error: invalid arguments\n");
        return;
    }
    if (queue->count >= queue->capacity) {
        queue->capacity = queue->capacity ? queue->capacity * 2 : 1;
        PCB** new_processes = realloc(queue->processes, queue->capacity * sizeof(PCB*));
        if (!new_processes) {
            printf("enqueue error: memory allocation failed\n");
            return;
        }
        queue->processes = new_processes;
    }
    queue->processes[queue->count++] = pcb;
}

//dequeue with priority of mutex list

PCB* dequeue_highest_priority(Queue* queue) {
    if (!queue || queue->count == 0) {
        return NULL;
    }
    
    // Find the process with the lowest priority value (highest priority, e.g., 1)
    int min_priority = 5; // Assume priority range is 1 to 4
    int min_index = -1;
    for (int i = 0; i < queue->count; i++) {
        if (queue->processes[i] && queue->processes[i]->priority < min_priority) {
            min_priority = queue->processes[i]->priority;
            min_index = i;
        }
    }
    
    if (min_index == -1) {
        return NULL;
    }
    
    // Extract the PCB
    PCB* pcb = queue->processes[min_index];
    
    // Shift remaining processes
    for (int i = min_index; i < queue->count - 1; i++) {
        queue->processes[i] = queue->processes[i + 1];
    }
    queue->processes[queue->count - 1] = NULL;
    queue->count--;
    
    return pcb;
}

PCB* dequeue(Queue* queue) {
    if (!queue || queue->count == 0) return NULL;
    PCB* pcb = queue->processes[0];
    for (int i = 0; i < queue->count - 1; i++) {
        queue->processes[i] = queue->processes[i + 1];
    }
    queue->count--;
    return pcb;
}


// Function to create a process
int create_process(int process_id, char* program_file,int start_index) {
    // Step 1: Read program file and count instructions
    FILE* file = fopen(program_file, "r");
    if (!file) {
        printf("Error: Could not open file %s\n", program_file);
        return -1;
    }

    char** instructions = malloc(15 * sizeof(char*)); // Temporary max 15 instructions
    int instruction_count = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        char* trimmed = trim(line);
        if (strlen(trimmed) == 0) continue;
        instructions[instruction_count] = strdup(trimmed);
        instruction_count++;
    }
    fclose(file);

    // Step 2: Allocate memory (2 variables: a, b)
    int variable_count = 3;
    int pcb_feild_count=7;

    // if (start_index == -1) {
    //     printf("Error: No memory available for process %d\n", process_id);
    //     for (int i = 0; i < instruction_count; i++) free(instructions[i]);
    //     free(instructions);
    //     return -1;
    // }

    // Step 3: Store instructions
    store_instructions(start_index, instructions, instruction_count);

    // Step 4: Initialize variables a and b
    char* var_names[] = {"a", "b"," "};
    init_variables(start_index, instruction_count, variable_count, var_names);

    PCB* pcb = malloc(sizeof(PCB)); // Allocate PCB dynamically
    pcb->process_id = process_id;
    pcb->memory_lower_bound = start_index;
    pcb->memory_upper_bound = start_index + instruction_count + variable_count + pcb_feild_count - 1;
    pcb->program_counter = 0;
    pcb->state = READY;
    pcb->priority = 1; // Default priority
    pcb->blocked_priority = 1;
    pcb->instruction_count = instruction_count;
    pcb->time_in_blocked = 0;
    pcb->time_in_ready = 0;

    store_pcb(start_index, instruction_count, variable_count, pcb);

    int queue_idx = (os_system.scheduler == MLFQ) ? pcb->priority - 1 : 0;
    enqueue(&os_system.ready_queues[queue_idx], pcb);

    for (int i = 0; i < instruction_count; i++) free(instructions[i]);
    free(instructions);

    return 0;
}

// Mutex operations
static Mutex* find_mutex(const char* resource) {
    if (!resource) return NULL;
    for (int i = 0; i < 3; i++) {
        if (os_system.mutexes[i].resource_name && 
            strcmp(os_system.mutexes[i].resource_name, resource) == 0) {
            return &os_system.mutexes[i];
        }
    }
    return NULL;
}


//semwait

// void semWait(const char* resource) {
//     Mutex* mutex = find_mutex(resource);
//     if (!mutex) {
//         printf("semWait error: resource %s not found\n", resource ? resource : "NULL");
//         return;
//     }
//     PCB* pcb = os_system.running_process;
//     if (!pcb) {
//         printf("semWait error: no running process\n");
//         return;
//     }
//     printf("Debug: semWait %s by process %d, mutex is_locked=%d, holding_process=%d\n",
//            resource, pcb->process_id, mutex->is_locked, mutex->holding_process ? mutex->holding_process->process_id : -1);
//     if (mutex->is_locked == 0) {
//         mutex->is_locked = 1;
//         mutex->holding_process = pcb;
//         pcb->state = RUNNING;
//         printf("Process %d acquired %s\n", pcb->process_id, resource);
//         printf("Debug: Mutex %s now locked by process %d, is_locked=%d\n", 
//                resource, mutex->holding_process->process_id, mutex->is_locked);
//     } else {
//         printf("Process %d blocked on %s\n", pcb->process_id, resource);
//         pcb->state = BLOCKED;
//         enqueue(&mutex->blocked_queue, pcb);
//         enqueue(&os_system.Blocked_queue, pcb);
//         store_pcb(pcb->memory_lower_bound, pcb->instruction_count, 2, pcb);
//         os_system.running_process = NULL;
//     }
// }

void semWait(const char* resource) {
    Mutex* mutex = find_mutex(resource);
    if (!mutex) {
        printf("semWait error: resource %s not found\n", resource ? resource : "NULL");

    snprintf(buffer, sizeof(buffer), "semWait error: resource %s not found\n", resource ? resource : "NULL");
    AddLogMessage(&gui_state, buffer);
        return;
    }
    PCB* pcb = os_system.running_process;
    if (!pcb) {
        printf("semWait error: no running process\n");
        return;
    }
    printf("Debug: semWait %s by process %d, mutex is_locked=%d, holding_process=%d\n",
           resource, pcb->process_id, mutex->is_locked,
           mutex->holding_process ? mutex->holding_process->process_id : -1);

    if (mutex->is_locked == 0) {
        mutex->is_locked = 1;
        mutex->holding_process = pcb;
        printf("Process %d acquired %s\n", pcb->process_id, resource);

        snprintf(buffer, sizeof(buffer), "Process %d acquired %s\n", pcb->process_id, resource);
        AddLogMessage(&gui_state, buffer);

    } else {
        printf("Process %d blocked on %s\n", pcb->process_id, resource);

        snprintf(buffer, sizeof(buffer), "Process %d blocked on %s\n", pcb->process_id, resource);
        AddLogMessage(&gui_state, buffer);

        pcb->state = BLOCKED;
        if (os_system.scheduler == MLFQ ) {
            static int quanta[4] = {1, 2, 4, 8};
            int level = pcb->priority - 1;
            if (level >= 0 && level < 3 ) {
                pcb->priority++; // Demote to next queue
            }
        }
        pcb->blocked_priority = pcb->priority; // Save priority when blocked
        enqueue(&mutex->blocked_queue, pcb);
        enqueue(&os_system.Blocked_queue, pcb);
        store_pcb(pcb->memory_lower_bound, pcb->instruction_count, 3, pcb);
        os_system.running_process = NULL;
    }
}

// void semWait(const char* resource) {
//     Mutex* mutex = find_mutex(resource);
//     if (!mutex) {
//         printf("semWait error: resource %s not found\n", resource ? resource : "NULL");
//         return;
//     }
//     PCB* pcb = os_system.running_process;
//     if (!pcb) {
//         printf("semWait error: no running process\n");
//         return;
//     }
//     printf("Debug: semWait %s by process %d, mutex is_locked=%d, holding_process=%d\n",
//            resource, pcb->process_id, mutex->is_locked,
//            mutex->holding_process ? mutex->holding_process->process_id : -1);

//     if (mutex->is_locked == 0) {
//         mutex->is_locked = 1;
//         mutex->holding_process = pcb;
//         printf("Process %d acquired %s\n", pcb->process_id, resource);
//     } else {
//         printf("Process %d blocked on %s\n", pcb->process_id, resource);
//         pcb->state = BLOCKED;
//         pcb->blocked_priority = pcb->priority; // Save priority when blocked
//         enqueue(&mutex->blocked_queue, pcb);
//         enqueue(&os_system.Blocked_queue, pcb);
//         store_pcb(pcb->memory_lower_bound, pcb->instruction_count, 2, pcb);
//         os_system.running_process = NULL;
//     }
// }

//sem signal
// void semSignal(const char* resource) {
//     Mutex* mutex = find_mutex(resource);
//     if (!mutex) {
//         printf("semSignal error: resource %s not found\n", resource ? resource : "NULL");
//         return;
//     }
//     PCB* current_process = os_system.running_process;
//     if (!current_process) {
//         printf("semSignal error: no running process\n");
//         return;
//     }
//     if (mutex->is_locked == 0 || mutex->holding_process != current_process) {
//         printf("semSignal error: resource %s not held by process %d\n",
//                resource, current_process->process_id);
//         return;
//     }
//     mutex->is_locked = 0;
//     mutex->holding_process = NULL;
//     printf("Process %d released %s\n", current_process->process_id, resource);
    
//     PCB* next_pcb = dequeue_highest_priority(&mutex->blocked_queue);
//     if (next_pcb) {
//         mutex->is_locked = 1;
//         mutex->holding_process = next_pcb;
//         next_pcb->state = READY;
//         int queue_idx = (os_system.scheduler == MLFQ) ? next_pcb->priority - 1 : 0;
//         if (queue_idx < 0 || queue_idx >= 4) queue_idx = 0;
//         enqueue(&os_system.ready_queues[queue_idx], next_pcb);
//         // Remove from Blocked_queue
//         for (int i = 0; i < os_system.Blocked_queue.count; i++) {
//             if (os_system.Blocked_queue.processes[i] == next_pcb) {
//                 for (int j = i; j < os_system.Blocked_queue.count - 1; j++) {
//                     os_system.Blocked_queue.processes[j] = os_system.Blocked_queue.processes[j + 1];
//                 }
//                 os_system.Blocked_queue.count--;
//                 break;
//             }
//         }
//         printf("Process %d unblocked for %s\n", next_pcb->process_id, resource);
//     }
// }

// void semSignal(const char* resource) {
//     Mutex* mutex = find_mutex(resource);
//     if (!mutex) {
//         printf("semSignal error: resource %s not found\n", resource ? resource : "NULL");
//         return;
//     }
//     PCB* pcb = os_system.running_process;
//     if (!pcb) {
//         printf("semSignal error: no running process\n");
//         return;
//     }
//     printf("Debug: semSignal %s by process %d\n", resource, pcb->process_id);

//     if (mutex->is_locked && mutex->holding_process == pcb) {
//         mutex->is_locked = 0;
//         mutex->holding_process = NULL;
//         printf("Process %d released %s\n", pcb->process_id, resource);

//         // Unblock one process if available
//         if (mutex->blocked_queue.count > 0) {
//             PCB* unblocked_pcb = dequeue_highest_priority(&mutex->blocked_queue);
//             if (unblocked_pcb) {
//                 unblocked_pcb->state = READY;
//                 unblocked_pcb->priority = unblocked_pcb->blocked_priority; // Restore priority
//                 unblocked_pcb->program_counter++; // Skip semWait
//                 int queue_idx = unblocked_pcb->priority - 1;
//                 if (queue_idx < 0 || queue_idx >= 4) queue_idx = 0;
//                 enqueue(&os_system.ready_queues[queue_idx], unblocked_pcb);
//                 printf("Process %d unblocked for %s, returned to queue %d, PC=%d\n",
//                        unblocked_pcb->process_id, resource, queue_idx, unblocked_pcb->program_counter);
//                 // Remove from global Blocked_queue
//                 for (int i = 0; i < os_system.Blocked_queue.count; i++) {
//                     if (os_system.Blocked_queue.processes[i] == unblocked_pcb) {
//                         for (int j = i; j < os_system.Blocked_queue.count - 1; j++) {
//                             os_system.Blocked_queue.processes[j] = os_system.Blocked_queue.processes[j + 1];
//                         }
//                         os_system.Blocked_queue.count--;
//                         break;
//                     }
//                 }
//                 store_pcb(unblocked_pcb->memory_lower_bound, unblocked_pcb->instruction_count, 2, unblocked_pcb);
//                 // Acquire the mutex for the unblocked process
//                 mutex->is_locked = 1;
//                 mutex->holding_process = unblocked_pcb;
//                 printf("Process %d acquired %s upon unblocking\n", unblocked_pcb->process_id, resource);
//             }
//         }
//     } else {
//         printf("semSignal error: resource %s not held by process %d\n", resource, pcb->process_id);
//     }
// }


void semSignal(const char* resource) {
    Mutex* mutex = find_mutex(resource);
    if (!mutex) {
        printf("semSignal error: resource %s not found\n", resource ? resource : "NULL");
        return;
    }
    PCB* pcb = os_system.running_process;
    if (!pcb) {
        printf("semSignal error: no running process\n");
        return;
    }
    printf("Debug: semSignal %s by process %d\n", resource, pcb->process_id);

    if (mutex->is_locked && mutex->holding_process == pcb) {
        mutex->is_locked = 0;
        mutex->holding_process = NULL;
        printf("Process %d released %s\n", pcb->process_id, resource);

        snprintf(buffer, sizeof(buffer), "Process %d released %s\n", pcb->process_id, resource);
        AddLogMessage(&gui_state, buffer);

        // Unblock one process if available
        if (mutex->blocked_queue.count > 0) {
            PCB* unblocked_pcb = dequeue_highest_priority(&mutex->blocked_queue);
            if (unblocked_pcb) {
                unblocked_pcb->state = READY;
                unblocked_pcb->priority = unblocked_pcb->blocked_priority; // Restore priority
                unblocked_pcb->program_counter++; // Skip semWait

                unblocked_pcb->time_in_blocked = 0;

                int queue_idx = unblocked_pcb->priority - 1;
                if (queue_idx < 0 || queue_idx >= 4) queue_idx = 0;
                enqueue(&os_system.ready_queues[queue_idx], unblocked_pcb);
                printf("Process %d unblocked for %s, returned to queue %d, PC=%d\n",
                       unblocked_pcb->process_id, resource, queue_idx, unblocked_pcb->program_counter);

                snprintf(buffer, sizeof(buffer), "Process %d unblocked for %s, returned to queue %d, PC=%d\n",
                       unblocked_pcb->process_id, resource, queue_idx, unblocked_pcb->program_counter);
                AddLogMessage(&gui_state, buffer);

                // Remove from global Blocked_queue
                for (int i = 0; i < os_system.Blocked_queue.count; i++) {
                    if (os_system.Blocked_queue.processes[i] == unblocked_pcb) {
                        for (int j = i; j < os_system.Blocked_queue.count - 1; j++) {
                            os_system.Blocked_queue.processes[j] = os_system.Blocked_queue.processes[j + 1];
                        }
                        os_system.Blocked_queue.count--;
                        break;
                    }
                }
                store_pcb(unblocked_pcb->memory_lower_bound, unblocked_pcb->instruction_count, 3, unblocked_pcb);
                // Acquire the mutex for the unblocked process
                mutex->is_locked = 1;
                mutex->holding_process = unblocked_pcb;
                printf("Process %d acquired %s upon unblocking\n", unblocked_pcb->process_id, resource);

                snprintf(buffer, sizeof(buffer), "Process %d acquired %s upon unblocking\n", unblocked_pcb->process_id, resource);
                AddLogMessage(&gui_state, buffer);
                
            }
        }
    } else {
        printf("semSignal error: resource %s not held by process %d\n", resource, pcb->process_id);
    }
}

//END OF MUTEX AND INTERPRETER////////////////////////////

//start of schedulers code///////////////////////////////

// FCFS Scheduler: Select the next process from the ready queue (queue 0)
// FCFS Scheduler
PCB* schedule_fcfs() {
    if (os_system.running_process && os_system.running_process->state == RUNNING) {
        printf("Debug: FCFS continuing process %d\n", os_system.running_process->process_id);

        // snprintf(buffer, sizeof(buffer), "FCFS continuing process %d\n", os_system.running_process->process_id);
        // AddLogMessage(&gui_state, buffer);

        return os_system.running_process;
    }
    PCB* next = dequeue(&os_system.ready_queues[0]);
    if (next) {
        next->state = RUNNING;
        printf("Debug: FCFS scheduled process %d\n", next->process_id);

        snprintf(buffer, sizeof(buffer), "FCFS scheduled process %d\n", next->process_id);
        AddLogMessage(&gui_state, buffer);

    }
    return next;
}



// Round Robin Scheduler
// PCB* schedule_round_robin(int* time_run) {

//     if (os_system.running_process && os_system.running_process->state == RUNNING) {
//         printf("Debug: RR process %d, time_run=%d, quantum=%d\n", 
//                os_system.running_process->process_id, *time_run, os_system.quantum);
//         if (*time_run >= os_system.quantum) {
//             os_system.running_process->state = READY;
//             enqueue(&os_system.ready_queues[0], os_system.running_process);
//             store_pcb(os_system.running_process->memory_lower_bound, 
//                      os_system.running_process->instruction_count, 2, 
//                      os_system.running_process);
//             os_system.running_process = NULL;
//             *time_run = 0;
//             printf("Debug: RR preempted process, time_run reset\n");
//         }
//     }
//     if (!os_system.running_process) {
//         PCB* next = dequeue(&os_system.ready_queues[0]);
//         if (next) {
//             next->state = RUNNING;
//             *time_run = 0;
//             os_system.running_process = next;
//             printf("Debug: RR scheduled process %d\n", next->process_id);
//         }
//     }
//     return os_system.running_process;
// }

// PCB* schedule_round_robin(int* time_run) {
//     if (os_system.running_process && os_system.running_process->state == RUNNING) {
//         printf("Debug: RR process %d, time_run=%d, quantum=%d\n", 
//                os_system.running_process->process_id, *time_run, os_system.quantum);
//         if (*time_run >= os_system.quantum) {
//             os_system.running_process->state = READY;
//             enqueue(&os_system.ready_queues[0], os_system.running_process);
//             store_pcb(os_system.running_process->memory_lower_bound, 
//                      os_system.running_process->instruction_count, 2, 
//                      os_system.running_process);
//             os_system.running_process = NULL;
//             *time_run = 0;
//             printf("Debug: RR preempted process, time_run reset\n");
//         }
//     }
//     if (!os_system.running_process) {
//         PCB* next = dequeue(&os_system.ready_queues[0]);
//         if (next) {
//             next->state = RUNNING;
//             // Only reset time_run if selecting a new process
//             if (next != os_system.running_process) {
//                 *time_run = 0;
//             }
//             os_system.running_process = next;
//             printf("Debug: RR scheduled process %d\n", next->process_id);
//         }
//     }
//     return os_system.running_process;
// }

// // MLFQ Scheduler: Manage four priority queues

// PCB* schedule_mlfq(int* time_run) {
//     static int quanta[4] = {1, 2, 4, 8};

//     printf("Debug: schedule_mlfq at cycle %d, time_run=%d, running_process=%d\n",
//            os_system.clock_cycle, *time_run,
//            os_system.running_process ? os_system.running_process->process_id : -1);

//     // Continue running process if quantum hasn't expired
//     if (os_system.running_process && os_system.running_process->state == RUNNING) {
//         int level = os_system.running_process->priority - 1;
//         if (*time_run < quanta[level]) {
//             printf("Debug: Continuing process %d (priority %d, time_run=%d, quantum=%d)\n",
//                    os_system.running_process->process_id,
//                    os_system.running_process->priority, *time_run, quanta[level]);
//             return os_system.running_process;
//         }
//         // Quantum expired, demote the process
//         printf("Debug: Quantum expired for process %d (priority %d)\n",
//                os_system.running_process->process_id,
//                os_system.running_process->priority);
//         if (level < 3) {
//             os_system.running_process->priority++; // Demote
//             printf("Debug: Demoted process %d to priority %d\n",
//                    os_system.running_process->process_id,
//                    os_system.running_process->priority);
//         }
//         os_system.running_process->state = READY;
//         int new_queue_idx = os_system.running_process->priority - 1;
//         enqueue(&os_system.ready_queues[new_queue_idx], os_system.running_process);
//         store_pcb(os_system.running_process->memory_lower_bound,
//                   os_system.running_process->instruction_count, 2,
//                   os_system.running_process);
//         os_system.running_process = NULL;
//         *time_run = 0;
//     }

//     // Select from highest-priority queue
//     for (int i = 0; i < 4; i++) {
//         if (os_system.ready_queues[i].count > 0) {
//             printf("Debug: Queue %d has %d processes: ", i, os_system.ready_queues[i].count);
//             for (int j = 0; j < os_system.ready_queues[i].count; j++) {
//                 printf("%d (state=%d) ", os_system.ready_queues[i].processes[j]->process_id,
//                        os_system.ready_queues[i].processes[j]->state);
//             }
//             printf("\n");

//             PCB* next_process = dequeue(&os_system.ready_queues[i]);
//             if (next_process) {
//                 next_process->state = RUNNING;
//                 os_system.running_process = next_process;
//                 *time_run = 0;
//                 store_pcb(next_process->memory_lower_bound, next_process->instruction_count, 2,
//                           next_process);
//                 printf("Debug: Scheduled process %d from queue %d\n",
//                        next_process->process_id, i);
//                 return next_process;
//             }
//         }
//     }

//     // Clear blocked running process
//     if (os_system.running_process && os_system.running_process->state == BLOCKED) {
//         printf("Debug: Clearing blocked running process %d\n",
//                os_system.running_process->process_id);
//         os_system.running_process = NULL;
//         *time_run = 0;
//     }

//     return os_system.running_process;
// }


PCB* schedule_round_robin(int* time_run) {
    if (os_system.running_process && os_system.running_process->state == RUNNING) {
        printf("Debug: RR process %d, time_run=%d, quantum=%d\n", 
               os_system.running_process->process_id, *time_run, os_system.quantum);

      

        if (*time_run >= os_system.quantum) {
            os_system.running_process->state = READY;
            enqueue(&os_system.ready_queues[0], os_system.running_process);
            store_pcb(os_system.running_process->memory_lower_bound, 
                     os_system.running_process->instruction_count, 3, 
                     os_system.running_process);
            os_system.running_process = NULL;
            *time_run = 0;
            printf("Debug: RR preempted process, time_run reset\n");
        }
    }
    if (!os_system.running_process) {
        PCB* next = dequeue(&os_system.ready_queues[0]);
        if (next) {
            next->state = RUNNING;
            // Only reset time_run if selecting a new process
            if (next != os_system.running_process) {
                *time_run = 0;
            }
            os_system.running_process = next;
            printf("Debug: RR scheduled process %d\n", next->process_id);

            snprintf(buffer, sizeof(buffer), "RR scheduled process %d\n", next->process_id);
            AddLogMessage(&gui_state, buffer);

        }
    }
    return os_system.running_process;
}

// MLFQ Scheduler: Manage four priority queues

PCB* schedule_mlfq(int* time_run) {
    static int quanta[4] = {1, 2, 4, 8};

    // printf("Debug: schedule_mlfq at cycle %d, time_run=%d, running_process=%d\n",
    //        os_system.clock_cycle, *time_run,
    //        os_system.running_process ? os_system.running_process->process_id : -1);

    // Continue running process if quantum hasn't expired
    if (os_system.running_process && os_system.running_process->state == RUNNING) {
        int level = os_system.running_process->priority - 1;
        if (*time_run < quanta[level]) {
            // Check if a higher-priority queue has processes
            int current_queue = os_system.running_process->priority - 1;
            bool higher_priority_exists = false;
            for (int i = 0; i < current_queue; i++) {
                if (os_system.ready_queues[i].count > 0) {
                    higher_priority_exists = true;
                    break;
                }
            }
            if (!higher_priority_exists) {
                printf("Debug: Continuing process %d (priority %d, time_run=%d, quantum=%d)\n",
                       os_system.running_process->process_id,
                       os_system.running_process->priority, *time_run, quanta[level]);

                    //    snprintf(buffer, sizeof(buffer), "Continuing process %d (priority %d, time_run=%d, quantum=%d)",
                    //    os_system.running_process->process_id,
                    //    os_system.running_process->priority, *time_run, quanta[level]);
                    //    AddLogMessage(&gui_state, buffer);

                return os_system.running_process;
            }
        }
        if (*time_run < quanta[level]) {
            // printf("Debug: Continuing process %d (priority %d, time_run=%d, quantum=%d)\n",
            //        os_system.running_process->process_id,
            //        os_system.running_process->priority, *time_run, quanta[level]);
            
            return os_system.running_process;
        }
        // Quantum expired, demote the process
        // printf("Debug: Quantum expired for process %d (priority %d)\n",
        //        os_system.running_process->process_id,
        //        os_system.running_process->priority);
        if (level < 3) {
            os_system.running_process->priority++; // Demote
            // printf("Debug: Demoted process %d to priority %d\n",
            //        os_system.running_process->process_id,
            //        os_system.running_process->priority);
        }
        os_system.running_process->state = READY;
        int new_queue_idx = os_system.running_process->priority - 1;
        enqueue(&os_system.ready_queues[new_queue_idx], os_system.running_process);
        store_pcb(os_system.running_process->memory_lower_bound,
                  os_system.running_process->instruction_count, 3,
                  os_system.running_process);
        os_system.running_process = NULL;
        *time_run = 0;
    }

    // Select from highest-priority queue
    for (int i = 0; i < 4; i++) {
        if (os_system.ready_queues[i].count > 0) {
            //printf("Debug: Queue %d has %d processes: ", i, os_system.ready_queues[i].count);
            for (int j = 0; j < os_system.ready_queues[i].count; j++) {
                // printf("%d (state=%d) ", os_system.ready_queues[i].processes[j]->process_id,
                //        os_system.ready_queues[i].processes[j]->state);
            }
            printf("\n");

            PCB* next_process = dequeue(&os_system.ready_queues[i]);
            if (next_process) {
                next_process->state = RUNNING;
                os_system.running_process = next_process;
                *time_run = 0;
                store_pcb(next_process->memory_lower_bound, next_process->instruction_count, 3,
                          next_process);
                // printf("Debug: Scheduled process %d from queue %d\n",
                //        next_process->process_id, i);
                return next_process;
            }
        }
    }

    // Clear blocked running process
    if (os_system.running_process && os_system.running_process->state == BLOCKED) {
        // printf("Debug: Clearing blocked running process %d\n",
        //        os_system.running_process->process_id);
        os_system.running_process = NULL;
        *time_run = 0;
    }

    return os_system.running_process;
}

// Initialize system state
void init_system(SchedulerType scheduler, int quantum) {
    os_system.clock_cycle = 0;
    os_system.scheduler = scheduler;
    os_system.quantum = quantum;
    os_system.running_process = NULL;
    time_run_step = 0;
    // Initialize memory
    init_memory();

    // Initialize ready queues
    for (int i = 0; i < 4; i++) {
        os_system.ready_queues[i].processes = NULL;
        os_system.ready_queues[i].count = 0;
        os_system.ready_queues[i].capacity = 0;
    }

    // Initialize blocked queue
    os_system.Blocked_queue.processes = NULL;
    os_system.Blocked_queue.count = 0;
    os_system.Blocked_queue.capacity = 0;

  // Initialize terminated queue
  os_system.terminated_queue.processes = NULL;
  os_system.terminated_queue.count = 0;
  os_system.terminated_queue.capacity = 0;

    // Initialize mutexes
    os_system.mutexes[0].resource_name = strdup("userInput");
    os_system.mutexes[0].is_locked = 0;
    os_system.mutexes[0].holding_process = NULL;
    os_system.mutexes[0].blocked_queue.processes = NULL;
    os_system.mutexes[0].blocked_queue.count = 0;
    os_system.mutexes[0].blocked_queue.capacity = 0;

    os_system.mutexes[1].resource_name = strdup("userOutput");
    os_system.mutexes[1].is_locked = 0;
    os_system.mutexes[1].holding_process = NULL;
    os_system.mutexes[1].blocked_queue.processes = NULL;
    os_system.mutexes[1].blocked_queue.count = 0;
    os_system.mutexes[1].blocked_queue.capacity = 0;

    os_system.mutexes[2].resource_name = strdup("file");
    os_system.mutexes[2].is_locked = 0;
    os_system.mutexes[2].holding_process = NULL;
    os_system.mutexes[2].blocked_queue.processes = NULL;
    os_system.mutexes[2].blocked_queue.count = 0;
    os_system.mutexes[2].blocked_queue.capacity = 0;

}


void run_system(ProcessInfo* processes, int process_count) {
    int time_run = 0;
    while (1) {
        // Create processes that have arrived
        for (int i = 0; i < process_count; i++) {
            if (processes[i].arrival_time == os_system.clock_cycle) {
                create_process(processes[i].process_id, processes[i].program_file, 
                              processes[i].start_index);
                printf("Process %d created at clock cycle %d\n", 
                       processes[i].process_id, os_system.clock_cycle);

                snprintf(buffer, sizeof(buffer), "Process %d created at clock cycle %d\n", 
                       processes[i].process_id, os_system.clock_cycle);
                    AddLogMessage(&gui_state, buffer);
            }
        }

        // Schedule a process
        PCB* next_process = NULL;
        if (os_system.scheduler == FCFS) {
            next_process = schedule_fcfs();
        } else if (os_system.scheduler == ROUND_ROBIN) {
            next_process = schedule_round_robin(&time_run);
        } else if (os_system.scheduler == MLFQ) {
            next_process = schedule_mlfq(&time_run);
        }
        os_system.running_process = next_process;

        if (next_process && next_process->state == RUNNING) {
            char* instruction = get_instruction(next_process);
            if (instruction) {
                printf("Clock %d: Process %d executing: %s\n", 
                       os_system.clock_cycle, next_process->process_id, instruction);

                       snprintf(buffer, sizeof(buffer), "Clock %d: Process %d executing: %s\n", 
                       os_system.clock_cycle, next_process->process_id, instruction);
                       AddLogMessage(&gui_state, buffer);

                readProgramSyntax(instruction);
                if (next_process->state != BLOCKED) {
                    next_process->program_counter++;
                    // Check if process has completed all instructions
                    if (next_process->program_counter >= next_process->instruction_count) {
                        next_process->state = TERMINATED;
                        enqueue(&os_system.terminated_queue, next_process);

                        store_pcb(next_process->memory_lower_bound,next_process->instruction_count, 3, next_process);
                        printf("Process %d terminated at clock cycle %d\n", 
                               next_process->process_id, os_system.clock_cycle);

                               snprintf(buffer, sizeof(buffer), "Process %d terminated at clock cycle %d\n", 
                               next_process->process_id, os_system.clock_cycle);
                               AddLogMessage(&gui_state, buffer);

                        os_system.running_process = NULL;
                        time_run = 0;
                    } else {
                      time_run++;
                    }
                } else {
                    time_run++; // Count the cycle for semWait
                }
                store_pcb(next_process->memory_lower_bound, 
                         next_process->instruction_count, 3, next_process);
            } else {
                // Handle case where instruction is NULL (should not occur if PC is valid)
                next_process->state = TERMINATED;
                store_pcb(next_process->memory_lower_bound, 
                         next_process->instruction_count, 3, next_process);
                printf("Process %d terminated at clock cycle %d\n", 
                       next_process->process_id, os_system.clock_cycle);
                os_system.running_process = NULL;
                time_run = 0;
            }
        } else {
            printf("Clock %d: No ready processes\n", os_system.clock_cycle);
        }

        // Debug: Log queue states
        // for (int i = 0; i < 4; i++) {
        //     if (os_system.ready_queues[i].count > 0) {
        //         printf("Debug: Ready queue %d has %d processes: ", 
        //                i, os_system.ready_queues[i].count);
        //         for (int j = 0; j < os_system.ready_queues[i].count; j++) {
        //             printf("%d (state=%d) ", os_system.ready_queues[i].processes[j]->process_id,
        //                    os_system.ready_queues[i].processes[j]->state);
        //         }
        //         printf("\n");
        //     }
        // }
        if (os_system.Blocked_queue.count > 0) {
          //  printf("Debug: Blocked queue has %d processes: ", os_system.Blocked_queue.count);
            for (int i = 0; i < os_system.Blocked_queue.count; i++) {
                if (os_system.Blocked_queue.processes[i]) {
                   // printf("%d ", os_system.Blocked_queue.processes[i]->process_id);
                }
            }
            printf("\n");
        }

        // Check if all processes are terminated
        int all_terminated = 1;
        for (int i = 0; i < 4; i++) {
            if (os_system.ready_queues[i].count > 0) {
                all_terminated = 0;
                break;
            }
        }
        if (os_system.Blocked_queue.count > 0) all_terminated = 0;
        if (os_system.running_process && os_system.running_process->state != TERMINATED) all_terminated = 0;
        if (all_terminated) {
            printf("Simulation completed at clock cycle %d\n", os_system.clock_cycle);
            break;
        }
        UpdateQueueTimes(&os_system);
        os_system.clock_cycle++;
    }
}

// void run_system(ProcessInfo* processes, int process_count) {
//     int time_run = 0;
//     bool all_terminated = false;
    
//     while (!all_terminated) {
//         // Create processes that have arrived
//         for (int i = 0; i < process_count; i++) {
//             if (processes[i].arrival_time == os_system.clock_cycle) {
//                 create_process(processes[i].process_id, processes[i].program_file, 
//                               processes[i].start_index);
//                 printf("Process %d created at clock cycle %d\n", 
//                        processes[i].process_id, os_system.clock_cycle);
//             }
//         }

//         // Check if all processes are terminated
//         all_terminated = true;
//         for (int i = 0; i < 4; i++) {
//             if (os_system.ready_queues[i].count > 0) {
//                 all_terminated = false;
//                 break;
//             }
//         }
//         if (os_system.Blocked_queue.count > 0) all_terminated = false;
//         if (os_system.running_process && os_system.running_process->state != TERMINATED) {
//             all_terminated = false;
//         }

//         if (all_terminated) {
//             printf("Simulation completed at clock cycle %d\n", os_system.clock_cycle);
//             break;
//         }

//         int instruction_executed = 0;
//         while (!instruction_executed && !all_terminated) {
//             // Schedule a process
//             PCB* next_process = NULL;
//             if (os_system.scheduler == FCFS) {
//                 next_process = schedule_fcfs();
//             } else if (os_system.scheduler == ROUND_ROBIN) {
//                 next_process = schedule_round_robin(&time_run);
//             } else if (os_system.scheduler == MLFQ) {
//                 next_process = schedule_mlfq(&time_run);
//             }
//             os_system.running_process = next_process;

//             if (next_process && next_process->state == RUNNING) {
//                 char* instruction = get_instruction(next_process);
//                 if (instruction) {
//                     printf("Clock %d: Process %d executing: %s\n", 
//                            os_system.clock_cycle, next_process->process_id, instruction);
//                     readProgramSyntax(instruction);
//                     if (next_process->state != BLOCKED) {
//                         next_process->program_counter++;
//                         // Check if process has completed all instructions
//                         if (next_process->program_counter >= next_process->instruction_count) {
//                             next_process->state = TERMINATED;
//                             enqueue(&os_system.terminated_queue, next_process);
//                             store_pcb(next_process->memory_lower_bound, 
//                                      next_process->instruction_count, 2, next_process);
//                             printf("Process %d terminated at clock cycle %d\n", 
//                                    next_process->process_id, os_system.clock_cycle);
//                             os_system.running_process = NULL;
//                             time_run = 0;
//                             instruction_executed = 1;
//                         } else {
//                             time_run++;
//                             instruction_executed = 1;
//                         }
//                     } else {
//                         time_run = 0;
//                     }
//                     store_pcb(next_process->memory_lower_bound, 
//                              next_process->instruction_count, 2, next_process);
//                 } else {
//                     // Handle case where instruction is NULL (should not occur if PC is valid)
//                     next_process->state = TERMINATED;
//                     store_pcb(next_process->memory_lower_bound, 
//                              next_process->instruction_count, 2, next_process);
//                     printf("Process %d terminated at clock cycle %d\n", 
//                            next_process->process_id, os_system.clock_cycle);
//                     os_system.running_process = NULL;
//                     time_run = 0;
//                     instruction_executed = 1;
//                 }
//             } else {
//                 printf("Clock %d: No ready processes\n", os_system.clock_cycle);
//                 instruction_executed = 1;
//                 break;
//             }
//         }

//         os_system.clock_cycle++;
//     }

// }

void run_system_step(ProcessInfo* processes, int process_count) {
        // Create processes that have arrived
        for (int i = 0; i < process_count; i++) {
            if (processes[i].arrival_time == os_system.clock_cycle) {
                
                create_process(processes[i].process_id, processes[i].program_file, 
                              processes[i].start_index);

                printf("Process %d created at clock cycle %d\n", 
                       processes[i].process_id, os_system.clock_cycle);
            }
        }

        

        // Schedule a process
        PCB* next_process = NULL;
        if (os_system.scheduler == FCFS) {
            next_process = schedule_fcfs();
        } else if (os_system.scheduler == ROUND_ROBIN) {
            next_process = schedule_round_robin(&time_run_step);
        } else if (os_system.scheduler == MLFQ) {
            next_process = schedule_mlfq(&time_run_step);
        }
        os_system.running_process = next_process;

        if (next_process && next_process->state == RUNNING) {
            char* instruction = get_instruction(next_process);
            if (instruction) {
                printf("Clock %d: Process %d executing: %s\n", 
                       os_system.clock_cycle, next_process->process_id, instruction);

                       snprintf(buffer, sizeof(buffer), "Clock %d: Process %d executing: %s\n", 
                       os_system.clock_cycle, next_process->process_id, instruction);
                       AddLogMessage(&gui_state, buffer);
                       
                readProgramSyntax(instruction);
                if (next_process->state != BLOCKED) {
                    next_process->program_counter++;
                    // Check if process has completed all instructions
                    if (next_process->program_counter >= next_process->instruction_count) {
                        next_process->state = TERMINATED;
                        enqueue(&os_system.terminated_queue, next_process);
                        store_pcb(next_process->memory_lower_bound,next_process->instruction_count, 3, next_process);
                        printf("Process %d terminated at clock cycle %d\n", 
                               next_process->process_id, os_system.clock_cycle);
                        os_system.running_process = NULL;
                        time_run_step = 0;
                    } else {
                      time_run_step++;
                    }
                } else {
                    time_run_step++; // Count the cycle for semWait
                }
                store_pcb(next_process->memory_lower_bound, 
                         next_process->instruction_count, 3, next_process);
            } else {
                // Handle case where instruction is NULL (should not occur if PC is valid)
                next_process->state = TERMINATED;
                store_pcb(next_process->memory_lower_bound, 
                         next_process->instruction_count, 3, next_process);
                printf("Process %d terminated at clock cycle %d\n", 
                       next_process->process_id, os_system.clock_cycle);
                os_system.running_process = NULL;
                time_run_step = 0;
            }
        } else {
            printf("Clock %d: No ready processes\n", os_system.clock_cycle);
        }

        // Debug: Log queue states
        // for (int i = 0; i < 4; i++) {
        //     if (os_system.ready_queues[i].count > 0) {
        //         printf("Debug: Ready queue %d has %d processes: ", 
        //                i, os_system.ready_queues[i].count);
        //         for (int j = 0; j < os_system.ready_queues[i].count; j++) {
        //             printf("%d (state=%d) ", os_system.ready_queues[i].processes[j]->process_id,
        //                    os_system.ready_queues[i].processes[j]->state);
        //         }
        //         printf("\n");
        //     }
        // }
        if (os_system.Blocked_queue.count > 0) {
          //  printf("Debug: Blocked queue has %d processes: ", os_system.Blocked_queue.count);
            for (int i = 0; i < os_system.Blocked_queue.count; i++) {
                if (os_system.Blocked_queue.processes[i]) {
                   // printf("%d ", os_system.Blocked_queue.processes[i]->process_id);
                }
            }
            printf("\n");
        }

        // Check if all processes are terminated
        int all_terminated = 1;
        for (int i = 0; i < 4; i++) {
            if (os_system.ready_queues[i].count > 0) {
                all_terminated = 0;
                break;
            }
        }
        if (os_system.Blocked_queue.count > 0) all_terminated = 0;
        if (os_system.running_process && os_system.running_process->state != TERMINATED) all_terminated = 0;
        if (all_terminated) {
            printf("Simulation completed at clock cycle %d\n", os_system.clock_cycle);
            return;
        }

        UpdateQueueTimes(&os_system);
        os_system.clock_cycle++;
}

// int main() {


// init_system(ROUND_ROBIN, 2);
//     ProcessInfo processes[] = {
//         {1, "Program_1.txt", 0, 0},
//         {2, "Program_2.txt", 1, 20},
//         {3, "Program_3.txt", 2, 40}
//     };
//     int process_count = 3;
//     run_system(processes, process_count);
//     return 0;
// }