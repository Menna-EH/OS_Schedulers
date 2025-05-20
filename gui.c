#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "osproj.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// GUI Constants
#define SCREEN_WIDTH 1600
#define SCREEN_HEIGHT 920
#define FONT_SIZE 20
#define PANEL_PADDING 10
#define COLOR_BACKGROUND (Color){ 40, 40, 40, 255 }
#define COLOR_PANEL (Color){ 60, 60, 60, 255 }
#define COLOR_TEXT (Color){ 230, 230, 230, 255 }
#define COLOR_HIGHLIGHT (Color){ 100, 149, 237, 255 } // Cornflower blue
#define COLOR_RUNNING (Color){ 50, 205, 50, 255 }    // Lime green
#define COLOR_READY (Color){ 255, 215, 0, 255 }      // Gold
#define COLOR_BLOCKED (Color){ 220, 20, 60, 255 }    // Crimson
#define COLOR_TERMINATED (Color){ 169, 169, 169, 255 } // Dark gray



GUIState gui_state = {0};

// Function prototypes
void InitGUI(GUIState *gui_state);
void UpdateGUI(GUIState *gui_state, SystemState *system);
void DrawGUI(GUIState *gui_state, SystemState *system);
void AddLogMessage(GUIState *gui_state, const char *message);
void DrawProcessList(Rectangle bounds, SystemState *system);
void DrawQueues(Rectangle bounds, SystemState *system);
void DrawMutexStatus(Rectangle bounds, SystemState *system);
void DrawMemoryViewer(Rectangle bounds, SystemState *system);
void DrawLogPanel(Rectangle bounds, GUIState *gui_state);
void DrawControlPanel(Rectangle bounds, GUIState *gui_state, SystemState *system);
Color GetStateColor(ProcessState state);
void AddInstToLog(char * instructions);
bool flag_run = true;

int getProgramNumber(const char *fileName) {
    int number = 0;
    // Move pointer past "Program_" (8 characters)
    const char *numStart = fileName + 8;
    // Parse the number until '.' or end of string
    while (*numStart != '.' && *numStart != '\0') {
        if (*numStart >= '0' && *numStart <= '9') {
            number = number * 10 + (*numStart - '0');
        }
        numStart++;
    }
    return number;
}

void AddInstToLog(char *filename) {
    FILE *file = fopen(filename, "r");  // Open the file for reading
    if (file == NULL) {
        AddLogMessage(&gui_state, TextFormat("Error: Could not open file %s", filename));
        return;
    }

    char line[256];  // Buffer to hold each line
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline if present
        line[strcspn(line, "\n")] = '\0';
        
        // Add the instruction to your log
        AddLogMessage(&gui_state, TextFormat("Instruction: %s", line));
    }

    fclose(file);  // Close the file
    AddLogMessage(&gui_state, TextFormat("\n"));
}

bool all_processes_terminated(SystemState *system) {
    for (int i = 0; i < 4; i++) {
        if (system->ready_queues[i].count > 0) {
            return false;
        }
    }
    if (system->Blocked_queue.count > 0) return false;
    if (system->running_process && system->running_process->state != TERMINATED) return false;
    return true;
}
void DrawProcessCreationPanel(Rectangle bounds, GUIState *gui_state) {
    DrawRectangleRec(bounds, COLOR_PANEL);
    DrawRectangleLinesEx(bounds, 2, COLOR_HIGHLIGHT);
    DrawText("Process Creation", bounds.x + 10, bounds.y + 10, FONT_SIZE, COLOR_TEXT);
    
    // File path input
    DrawText("Program File:", bounds.x + 20, bounds.y + 40, FONT_SIZE, COLOR_TEXT);
    GuiTextBox((Rectangle){bounds.x + 160, bounds.y + 40, bounds.width - 1200, 25}, 
               gui_state->process_file_path, 256, !gui_state->running);
    
    // Arrival time
    DrawText("Arrival Time:", bounds.x + 20, bounds.y + 75, FONT_SIZE, COLOR_TEXT);
    GuiSpinner((Rectangle){bounds.x + 160, bounds.y + 75, 100, 25}, NULL, 
               &gui_state->new_process_arrival, 0, 100, !gui_state->running);
    
    // // Start index
    // DrawText("Start Index:", bounds.x + 20, bounds.y + 110, FONT_SIZE, COLOR_TEXT);
    // GuiSpinner((Rectangle){bounds.x + 160, bounds.y + 110, 100, 25}, NULL, 
    //            &gui_state->new_process_start, 0, 50, !gui_state->running);
    
    // Add button
    if (GuiButton((Rectangle){bounds.x + 20, bounds.y + 145, bounds.width - 1300, 40}, "Add Process") && 
        !gui_state->running) {
        if (strlen(gui_state->process_file_path) > 0 && gui_state->process_count < 10) {
            int proc_id = getProgramNumber(gui_state->process_file_path);
            gui_state->next_process_id++;
            gui_state->processes[gui_state->process_count] = (ProcessInfo){
               proc_id,
                strdup(gui_state->process_file_path),
                gui_state->new_process_arrival,
                gui_state->new_process_start
            };
            gui_state->process_count++;
            gui_state->new_process_start = gui_state->new_process_start + 20;

            AddLogMessage(gui_state, TextFormat("Added process : %s", 
                 gui_state->process_file_path));
            AddInstToLog(gui_state->process_file_path);

            memset(gui_state->process_file_path, 0, sizeof(gui_state->process_file_path));
        }
    }
}
int main() {
    // Initialize the system
    init_system(ROUND_ROBIN, 2);
    ProcessInfo processes[] = {
        {1, "Program_1.txt", 0, 0},
        {2, "Program_2.txt", 1, 20},
        {3, "Program_3.txt", 2, 40}
    };
    int process_count = 3;

    // Initialize GUI
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OS Scheduler Simulation");
    SetTargetFPS(60);
    
    InitGUI(&gui_state);
    gui_state.quantum = os_system.quantum;
    
    gui_state.selected_scheduler = os_system.scheduler;
    
    // Main game loop
    // Main game loop
    while (!WindowShouldClose()) {

        if (IsKeyPressed(KEY_SPACE)) {
            gui_state.step_mode = !gui_state.step_mode;
            gui_state.running = false;
        }
        
        // Add this line to handle Q key for memory viewer
        if (IsKeyPressed(KEY_Q)) {
            gui_state.show_memory = !gui_state.show_memory;
        }

        // Handle input
        if (IsKeyPressed(KEY_SPACE)) {
            gui_state.step_mode = !gui_state.step_mode;
            gui_state.running = false;
        }
        
        // Update simulation if running and not completed
        if (flag_run && gui_state.running ) {
            run_system(gui_state.processes, gui_state.process_count);
            flag_run = false;
            
            // if (!all_processes_terminated(&os_system)) {
            //     printf("noo/n");

            //     // Run the system with the current processes
            //     run_system(gui_state.processes, gui_state.process_count);
                
            //     // For step mode, only run one cycle
            //     if (gui_state.step_mode) {
            //         gui_state.running = false;
            //     }
            // } else {
            //     gui_state.running = false;
            //     AddLogMessage(&gui_state, "All processes completed!");
            // }
        }
        if ((gui_state.step_mode && IsKeyPressed(KEY_ENTER))){
            run_system_step(gui_state.processes, gui_state.process_count);
        }
        
        // Update GUI
        UpdateGUI(&gui_state, &os_system);
        
        // Draw
        BeginDrawing();
            ClearBackground(COLOR_BACKGROUND);
            DrawGUI(&gui_state, &os_system);
        EndDrawing();
    }
    
    CloseWindow();
    return 0;
}

void InitGUI(GUIState *gui_state) {
    gui_state->running = false;
    gui_state->step_mode = false;
    gui_state->show_memory = false;
    gui_state->log_count = 0;
    gui_state->scroll_index = 0;
    gui_state->log_scroll_offset = 0;

    memset(gui_state->log_text, 0, sizeof(gui_state->log_text));
    
    // Initialize the new members
    gui_state->process_count = 0;
    gui_state->next_process_id = 1;
    memset(gui_state->process_file_path, 0, sizeof(gui_state->process_file_path));
    gui_state->new_process_arrival = 0;
    gui_state->new_process_start = 0;
}

void UpdateGUI(GUIState *gui_state, SystemState *system) {
    // Update quantum if changed
    system->quantum = gui_state->quantum;
    
    // Update scheduler if changed
    if (gui_state->selected_scheduler != system->scheduler) {
        system->scheduler = gui_state->selected_scheduler;
        char message[256];
        snprintf(message, sizeof(message), "Scheduler changed to %s", 
                system->scheduler == FCFS ? "FCFS" : 
                system->scheduler == ROUND_ROBIN ? "Round Robin" : "MLFQ");
        AddLogMessage(gui_state, message);
    }
}

void DrawGUI(GUIState *gui_state, SystemState *system) {
    // Draw main panels
    Rectangle dashboard = { PANEL_PADDING, PANEL_PADDING, SCREEN_WIDTH * 0.7f, SCREEN_HEIGHT * 0.4f };
    Rectangle control_panel = { SCREEN_WIDTH * 0.72f, PANEL_PADDING, SCREEN_WIDTH * 0.27f, SCREEN_HEIGHT * 0.3f };
    Rectangle queues_panel = { PANEL_PADDING, dashboard.y + dashboard.height + PANEL_PADDING, 
                             SCREEN_WIDTH * 0.35f, SCREEN_HEIGHT * 0.35f };
    Rectangle mutex_panel = { queues_panel.x + queues_panel.width + PANEL_PADDING, queues_panel.y,
                            SCREEN_WIDTH * 0.35f, queues_panel.height };
    Rectangle log_panel = { PANEL_PADDING + 1150, 
                    queues_panel.y + queues_panel.height + PANEL_PADDING - 420,
                    SCREEN_WIDTH - 2*PANEL_PADDING - 1150, 
                    SCREEN_HEIGHT - (queues_panel.y + queues_panel.height + 2*PANEL_PADDING) + 220 };
    
    // Draw panels
    DrawRectangleRec(dashboard, COLOR_PANEL);
    DrawRectangleRec(control_panel, COLOR_PANEL);
    DrawRectangleRec(queues_panel, COLOR_PANEL);
    DrawRectangleRec(mutex_panel, COLOR_PANEL);
    DrawRectangleRec(log_panel, COLOR_PANEL);
    
    // Draw panel borders
    DrawRectangleLinesEx(dashboard, 2, COLOR_HIGHLIGHT);
    DrawRectangleLinesEx(control_panel, 2, COLOR_HIGHLIGHT);
    DrawRectangleLinesEx(queues_panel, 2, COLOR_HIGHLIGHT);
    DrawRectangleLinesEx(mutex_panel, 2, COLOR_HIGHLIGHT);
    DrawRectangleLinesEx(log_panel, 2, COLOR_HIGHLIGHT);
    
    // Draw panel titles
    DrawText("System Dashboard", dashboard.x + 10, dashboard.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("Control Panel", control_panel.x + 10, control_panel.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("Queues", queues_panel.x + 10, queues_panel.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("Resource Status", mutex_panel.x + 10, mutex_panel.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("Execution Log", log_panel.x + 10, log_panel.y + 10, FONT_SIZE, COLOR_TEXT);
    
    // Draw panel contents
    DrawProcessList((Rectangle){dashboard.x + 10, dashboard.y + 40, dashboard.width - 20, dashboard.height - 50}, system);
    DrawControlPanel(control_panel, gui_state, system);
    DrawQueues(queues_panel, system);
    DrawMutexStatus(mutex_panel, system);
    DrawLogPanel(log_panel, gui_state);

    Rectangle process_panel = { PANEL_PADDING, SCREEN_HEIGHT - 200, 
        SCREEN_WIDTH - 2*PANEL_PADDING, 190 };
    DrawProcessCreationPanel(process_panel, gui_state);
    
    // Draw memory viewer if enabled
    if (gui_state->show_memory) {
        Rectangle memory_viewer = { SCREEN_WIDTH * 0.1f, SCREEN_HEIGHT * 0.1f, 
                                  SCREEN_WIDTH * 0.8f, SCREEN_HEIGHT * 0.8f };
        DrawRectangleRec(memory_viewer, COLOR_PANEL);
        DrawRectangleLinesEx(memory_viewer, 2, COLOR_HIGHLIGHT);
        DrawText("Memory Viewer (Press Q to close)", memory_viewer.x + 10, memory_viewer.y + 10, FONT_SIZE, COLOR_TEXT);
        DrawMemoryViewer((Rectangle){memory_viewer.x + 10, memory_viewer.y + 40, 
                                    memory_viewer.width - 20, memory_viewer.height - 50}, system);
    }
    
}



void AddLogMessage(GUIState *gui_state, const char *message) {
    if (gui_state->log_count < 1024) {
        strncpy(gui_state->log_text[gui_state->log_count], message, 255);
        gui_state->log_text[gui_state->log_count][255] = '\0';
        gui_state->log_count++;
        gui_state->scroll_index = gui_state->log_count - 1;
    } else {
        // Shift all messages up (FIFO behavior)
        for (int i = 0; i < 1023; i++) {
            strcpy(gui_state->log_text[i], gui_state->log_text[i+1]);
        }
        strncpy(gui_state->log_text[1023], message, 255);
        gui_state->log_text[1023][255] = '\0';
        gui_state->scroll_index = 1023;
    }
}

// void AddLogMessage(GUIState *gui_state, const char *message) {
//     if (gui_state->log_count < 1024) {
//         strncpy(gui_state->log_text[gui_state->log_count], message, 255);
//         gui_state->log_text[gui_state->log_count][255] = '\0';
//         gui_state->log_count++;
//         gui_state->scroll_index = gui_state->log_count - 1;
//     } else {
//         // Shift all messages up
//         for (int i = 0; i < 1023; i++) {
//             strcpy(gui_state->log_text[i], gui_state->log_text[i+1]);
//         }
//         strncpy(gui_state->log_text[1023], message, 255);
//         gui_state->log_text[1023][255] = '\0';
//         gui_state->scroll_index = 1023;
//     }
// }

void DrawProcessList(Rectangle bounds, SystemState *system) {
    // Draw column headers
    DrawText("PID", bounds.x + 10, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    DrawText("State", bounds.x + 80, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    DrawText("Priority", bounds.x + 180, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    DrawText("PC", bounds.x + 280, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    DrawText("Memory", bounds.x + 350, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    // DrawText("Instructions", bounds.x + 480, bounds.y + 10, FONT_SIZE, COLOR_HIGHLIGHT);
    
    // Draw system info
    char info_text[256];
    snprintf(info_text, sizeof(info_text), "Clock Cycle: %d | Scheduler: %s | Quantum: %d | Num of Processes: %d", 
            system->clock_cycle,
            system->scheduler == FCFS ? "FCFS" : 
            system->scheduler == ROUND_ROBIN ? "Round Robin" : "MLFQ",
            system->quantum, 
            gui_state.process_count);
    DrawText(info_text, bounds.x + 10, bounds.y + bounds.height - 30, FONT_SIZE, COLOR_TEXT);
    
    // Draw all processes (simplified - in a real app you'd need to track all processes)
    int y_pos = bounds.y + 40;
    
    // Draw running process
    if (system->running_process) {
        PCB *pcb = system->running_process;
        char state_text[20];
        switch (pcb->state) {
            case READY: strcpy(state_text, "READY"); break;
            case RUNNING: strcpy(state_text, "RUNNING"); break;
            case BLOCKED: strcpy(state_text, "BLOCKED"); break;
            case TERMINATED: strcpy(state_text, "TERMINATED"); break;
        }
        
        char mem_text[50];
        snprintf(mem_text, sizeof(mem_text), "%d-%d", pcb->memory_lower_bound, pcb->memory_upper_bound);
        
        DrawText(TextFormat("%d", pcb->process_id), bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText(state_text, bounds.x + 60, y_pos, FONT_SIZE, GetStateColor(pcb->state));
        DrawText(TextFormat("%d", pcb->priority), bounds.x + 180, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText(TextFormat("%d", pcb->program_counter), bounds.x + 280, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText(mem_text, bounds.x + 350, y_pos, FONT_SIZE, COLOR_TEXT);
        // DrawText(TextFormat("%d", pcb->instruction_count), bounds.x + 480, y_pos, FONT_SIZE, COLOR_TEXT);
        
        y_pos += 30;
    }
    
    // Draw processes in ready queues
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < system->ready_queues[i].count; j++) {
            PCB *pcb = system->ready_queues[i].processes[j];
            
            char mem_text[50];
            snprintf(mem_text, sizeof(mem_text), "%d-%d", pcb->memory_lower_bound, pcb->memory_upper_bound);
            
            DrawText(TextFormat("%d", pcb->process_id), bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText("READY", bounds.x + 60, y_pos, FONT_SIZE, COLOR_READY);
            DrawText(TextFormat("%d", pcb->priority), bounds.x + 180, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText(TextFormat("%d", pcb->program_counter), bounds.x + 280, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText(mem_text, bounds.x + 350, y_pos, FONT_SIZE, COLOR_TEXT);
            // DrawText(TextFormat("%d", pcb->instruction_count), bounds.x + 480, y_pos, FONT_SIZE, COLOR_TEXT);
            
            y_pos += 30;
            if (y_pos > bounds.y + bounds.height - 40) break;
        }
    }
    
    // Draw processes in blocked queue
    for (int i = 0; i < system->Blocked_queue.count; i++) {
        PCB *pcb = system->Blocked_queue.processes[i];
        
        char mem_text[50];
        snprintf(mem_text, sizeof(mem_text), "%d-%d", pcb->memory_lower_bound, pcb->memory_upper_bound);
        
        DrawText(TextFormat("%d", pcb->process_id), bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText("BLOCKED", bounds.x + 60, y_pos, FONT_SIZE, COLOR_BLOCKED);
        DrawText(TextFormat("%d", pcb->priority), bounds.x + 180, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText(TextFormat("%d", pcb->program_counter), bounds.x + 280, y_pos, FONT_SIZE, COLOR_TEXT);
        DrawText(mem_text, bounds.x + 350, y_pos, FONT_SIZE, COLOR_TEXT);
        // DrawText(TextFormat("%d", pcb->instruction_count), bounds.x + 480, y_pos, FONT_SIZE, COLOR_TEXT);
        
        y_pos += 30;
        if (y_pos > bounds.y + bounds.height - 40) break;
    }
        // Draw processes in terminated queue
        for (int i = 0; i < system->terminated_queue.count; i++) {
            PCB *pcb = system->terminated_queue.processes[i];
            
            char mem_text[50];
            snprintf(mem_text, sizeof(mem_text), "%d-%d", pcb->memory_lower_bound, pcb->memory_upper_bound);
            
            DrawText(TextFormat("%d", pcb->process_id), bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText("Terminated", bounds.x + 60, y_pos, FONT_SIZE, COLOR_BLOCKED);
            DrawText(TextFormat("%d", pcb->priority), bounds.x + 180, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText(TextFormat("%d", pcb->program_counter), bounds.x + 280, y_pos, FONT_SIZE, COLOR_TEXT);
            DrawText(mem_text, bounds.x + 350, y_pos, FONT_SIZE, COLOR_TEXT);
            // DrawText(TextFormat("%d", pcb->instruction_count), bounds.x + 480, y_pos, FONT_SIZE, COLOR_TEXT);
            
            y_pos += 30;
            if (y_pos > bounds.y + bounds.height - 40) break;
        }
}

void DrawControlPanel(Rectangle bounds, GUIState *gui_state, SystemState *system) {
    int y_pos = bounds.y + 40;
    
    // Scheduler selection
    DrawText("Scheduler:", bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
    
    

    Rectangle fcfs_btn = { bounds.x + 120, y_pos, 100, 30 };
    Rectangle rr_btn = { bounds.x + 200, y_pos, 100, 30 };
    Rectangle mlfq_btn = { bounds.x + 310, y_pos, 100, 30 };
    
    DrawRectangleRec(fcfs_btn, system->scheduler == FCFS ? COLOR_HIGHLIGHT : COLOR_PANEL);
    DrawRectangleRec(rr_btn, system->scheduler == ROUND_ROBIN ? COLOR_HIGHLIGHT : COLOR_PANEL);
    DrawRectangleRec(mlfq_btn, system->scheduler == MLFQ ? COLOR_HIGHLIGHT : COLOR_PANEL);
    
    DrawText("FCFS", fcfs_btn.x + 10, fcfs_btn.y + 5, FONT_SIZE, COLOR_TEXT);
    DrawText("RR", rr_btn.x + 20, rr_btn.y + 5, FONT_SIZE, COLOR_TEXT);
    DrawText("MLFQ", mlfq_btn.x + 10, mlfq_btn.y + 5, FONT_SIZE, COLOR_TEXT);
    
    // Handle scheduler selection
    if (CheckCollisionPointRec(GetMousePosition(), fcfs_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->selected_scheduler = FCFS;
    }
    if (CheckCollisionPointRec(GetMousePosition(), rr_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->selected_scheduler = ROUND_ROBIN;
    }
    if (CheckCollisionPointRec(GetMousePosition(), mlfq_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->selected_scheduler = MLFQ;
    }
    
    y_pos += 40;
    
    // Quantum control (only for RR)
    if (system->scheduler == ROUND_ROBIN ) {
        DrawText("Quantum:", bounds.x + 10, y_pos, FONT_SIZE, COLOR_TEXT);
        
        Rectangle dec_btn = { bounds.x + 120, y_pos, 30, 30 };
        Rectangle inc_btn = { bounds.x + 220, y_pos, 30, 30 };
        
        DrawRectangleRec(dec_btn, COLOR_HIGHLIGHT);
        DrawRectangleRec(inc_btn, COLOR_HIGHLIGHT);
        
        DrawText("-", dec_btn.x + 10, dec_btn.y + 5, FONT_SIZE, COLOR_TEXT);
        DrawText("+", inc_btn.x + 10, inc_btn.y + 5, FONT_SIZE, COLOR_TEXT);
        
        DrawText(TextFormat("%d", gui_state->quantum), bounds.x + 160, y_pos, FONT_SIZE, COLOR_TEXT);
        
        if (CheckCollisionPointRec(GetMousePosition(), dec_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (gui_state->quantum > 1) gui_state->quantum--;
        }
        if (CheckCollisionPointRec(GetMousePosition(), inc_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (gui_state->quantum < 10) gui_state->quantum++;
        }
        
        y_pos += 40;
        if (GuiButton((Rectangle){bounds.x + 10, bounds.y + bounds.height - 40, 120, 30}, "Add Process")) {
            if (gui_state->process_count < 10) {
                // Create a default process (modify as needed)
                gui_state->processes[gui_state->process_count] = (ProcessInfo){
                    gui_state->next_process_id++,
                    "Program_1.txt",  // Default program file
                    gui_state->process_count * 5,  // Staggered arrival times
                    gui_state->process_count * 20  // Staggered memory locations
                };
                gui_state->process_count++;
                AddLogMessage(gui_state, TextFormat("Added process %d", gui_state->next_process_id - 1));
            }
        }
    }
    
    // Control buttons
    Rectangle run_btn = { bounds.x + 10, y_pos, 120, 40 };
    Rectangle step_btn = { bounds.x + 140, y_pos, 140, 40 };
    Rectangle reset_btn = { bounds.x + 295, y_pos, 120, 40 };
    Rectangle mem_btn = { bounds.x + 10, y_pos + 50, 120, 40 };
    
    DrawRectangleRec(run_btn, gui_state->running ? COLOR_RUNNING : COLOR_HIGHLIGHT);
    DrawRectangleRec(step_btn, gui_state->step_mode ? COLOR_HIGHLIGHT : COLOR_PANEL);
    DrawRectangleRec(reset_btn, COLOR_HIGHLIGHT);
    DrawRectangleRec(mem_btn, COLOR_HIGHLIGHT);
    
    DrawText(gui_state->running ? "STOP" : "START", run_btn.x + 30, run_btn.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("STEP MODE", step_btn.x + 10, step_btn.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("RESET", reset_btn.x + 30, reset_btn.y + 10, FONT_SIZE, COLOR_TEXT);
    DrawText("MEMORY", mem_btn.x + 20, mem_btn.y + 10, FONT_SIZE, COLOR_TEXT);
    
    // Handle button clicks
    if (CheckCollisionPointRec(GetMousePosition(), run_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->running = !gui_state->running;
        gui_state->step_mode = false;
    }
    if (CheckCollisionPointRec(GetMousePosition(), step_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->step_mode = !gui_state->step_mode;
        gui_state->running = false;
    }
    if (CheckCollisionPointRec(GetMousePosition(), reset_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // Reset system
        init_system(gui_state->selected_scheduler, gui_state->quantum);
        gui_state->running = false;
        gui_state->step_mode = false;
        flag_run = true;
        InitGUI(gui_state);

        AddLogMessage(gui_state, "System reset");
    }
    if (CheckCollisionPointRec(GetMousePosition(), mem_btn) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        gui_state->show_memory = !gui_state->show_memory;
    }
}

void DrawQueues(Rectangle bounds, SystemState *system) {
    int y_pos = bounds.y + 40;
    
    // Running process
    DrawText("Running Process:", bounds.x + 10, y_pos, FONT_SIZE-2, COLOR_HIGHLIGHT);
    y_pos += 30;
    
    if (system->running_process && system->running_process->state == RUNNING) {
        PCB *pcb = system->running_process;
        char *instruction = get_instruction(pcb);
        
        // Updated to show ready wait time before running
        DrawText(TextFormat("PID: %d ", 
                pcb->process_id),
                bounds.x + 20, y_pos, FONT_SIZE-4, COLOR_TEXT);
        y_pos += 25;
        
        if (instruction) {
            char instr_text[100];
            snprintf(instr_text, sizeof(instr_text), "Instruction: %s", instruction);
            DrawText(instr_text, bounds.x + 20, y_pos, FONT_SIZE-4, COLOR_TEXT);
            y_pos += 25;
        }
    } else {
        DrawText("None", bounds.x + 20, y_pos, FONT_SIZE-4, COLOR_TEXT);
        y_pos += 25;
    }
    
    y_pos += 15;
    
    // Ready queues - updated with wait times
    DrawText("Ready Queues:", bounds.x + 10, y_pos-15, FONT_SIZE-2, COLOR_HIGHLIGHT);
    y_pos += 30;
    
    for (int i = 0; i < 4; i++) {
        if (system->ready_queues[i].count > 0) {
            DrawText(TextFormat("Queue %d (%d processes):", i+1, system->ready_queues[i].count), 
                    bounds.x + 20, y_pos-10, FONT_SIZE-4, COLOR_TEXT);
            y_pos += 25;
            
            for (int j = 0; j < system->ready_queues[i].count; j++) {
                if (j < 3) { // Show first 3 processes
                    PCB* pcb = system->ready_queues[i].processes[j];
                    // Added wait time display
                    DrawText(TextFormat("PID %d (PC: %d, Wait: %d cycles)", 
                            pcb->process_id,
                            pcb->program_counter,
                            pcb->time_in_ready), 
                            bounds.x + 40, y_pos-10, FONT_SIZE-5, COLOR_TEXT);
                    y_pos += 25;
                }
            }
            
            if (system->ready_queues[i].count > 3) {
                DrawText(TextFormat("+%d more...", system->ready_queues[i].count - 3), 
                        bounds.x + 40, y_pos, FONT_SIZE-4, COLOR_TEXT);
                y_pos += 25;
            }
        }
    }
    
    // Blocked queue - updated with wait times
    y_pos += 15;
    DrawText("Blocked Queue:", bounds.x + 10, y_pos-10, FONT_SIZE-2, COLOR_HIGHLIGHT);
    y_pos += 20;
    
    if (system->Blocked_queue.count > 0) {
        DrawText(TextFormat("%d processes blocked:", system->Blocked_queue.count), 
                bounds.x + 20, y_pos-10, FONT_SIZE-4, COLOR_TEXT);
        y_pos += 20;
        
        for (int i = 0; i < system->Blocked_queue.count; i++) {
            if (i < 3) { // Show first 3 processes
                PCB* pcb = system->Blocked_queue.processes[i];
                // Added wait time display
                DrawText(TextFormat("PID %d (Priority: %d, Wait: %d cycles)", 
                        pcb->process_id,
                        pcb->priority,
                        pcb->time_in_blocked), 
                        bounds.x + 40, y_pos-10, FONT_SIZE-5, COLOR_TEXT);
                y_pos += 25;
            }
        }
        
        if (system->Blocked_queue.count > 3) {
            DrawText(TextFormat("+%d more...", system->Blocked_queue.count - 3), 
                    bounds.x + 40, y_pos, FONT_SIZE-4, COLOR_TEXT);
            y_pos += 25;
        }
    } else {
        DrawText("No processes blocked", bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
        y_pos += 25;
    }
}

// void DrawQueues(Rectangle bounds, SystemState *system) {
//     int y_pos = bounds.y + 40;
    
//     // Running process
//     DrawText("Running Process:", bounds.x + 10, y_pos, FONT_SIZE, COLOR_HIGHLIGHT);
//     y_pos += 30;
    
//     if (system->running_process && system->running_process->state == RUNNING) {
//         PCB *pcb = system->running_process;
//         char *instruction = get_instruction(pcb);
        
//         DrawText(TextFormat("PID: %d", pcb->process_id), bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//         y_pos += 25;
//         // DrawText(TextFormat("PC: %d", pcb->program_counter), bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//         // y_pos += 25;
//         // DrawText(TextFormat("Priority: %d", pcb->priority), bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//         // y_pos += 25;
        
//         if (instruction) {
//             char instr_text[100];
//             snprintf(instr_text, sizeof(instr_text), "Instruction: %s", instruction);
//             DrawText(instr_text, bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//             y_pos += 25;
//         }
//     } else {
//         DrawText("None", bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//         y_pos += 25;
//     }
    
//     y_pos += 20;
    
//     // Ready queues
//     DrawText("Ready Queues:", bounds.x + 10, y_pos, FONT_SIZE, COLOR_HIGHLIGHT);
//     y_pos += 30;
    
//     for (int i = 0; i < 4; i++) {
//         if (system->ready_queues[i].count > 0) {
//             DrawText(TextFormat("Queue %d (%d processes):", i+1, system->ready_queues[i].count), 
//                     bounds.x + 20, y_pos, FONT_SIZE-2, COLOR_TEXT);
//             y_pos += 25;
            
//             for (int j = 0; j < system->ready_queues[i].count; j++) {
//                 if (j < 3) { // Show first 3 processes
//                     DrawText(TextFormat("PID %d (PC: %d)", 
//                             system->ready_queues[i].processes[j]->process_id,
//                             system->ready_queues[i].processes[j]->program_counter), 
//                             bounds.x + 40, y_pos, FONT_SIZE-3, COLOR_TEXT);
//                     y_pos += 25;
//                 }
//             }
            
//             if (system->ready_queues[i].count > 3) {
//                 DrawText(TextFormat("+%d more...", system->ready_queues[i].count - 3), 
//                         bounds.x + 40, y_pos, FONT_SIZE-3, COLOR_TEXT);
//                 y_pos += 25;
//             }
//         }
//     }
    
//     // Blocked queue
//     y_pos += 20;
//     DrawText("Blocked Queue:", bounds.x + 10, y_pos, FONT_SIZE, COLOR_HIGHLIGHT);
//     y_pos += 30;
    
//     if (system->Blocked_queue.count > 0) {
//         DrawText(TextFormat("%d processes blocked:", system->Blocked_queue.count), 
//                 bounds.x + 20, y_pos, FONT_SIZE-2, COLOR_TEXT);
//         y_pos += 25;
        
//         for (int i = 0; i < system->Blocked_queue.count; i++) {
//             if (i < 3) { // Show first 3 processes
//                 DrawText(TextFormat("PID %d (Priority: %d)", 
//                         system->Blocked_queue.processes[i]->process_id,
//                         system->Blocked_queue.processes[i]->priority), 
//                         bounds.x + 40, y_pos, FONT_SIZE-3, COLOR_TEXT);
//                 y_pos += 25;
//             }
//         }
        
//         if (system->Blocked_queue.count > 3) {
//             DrawText(TextFormat("+%d more...", system->Blocked_queue.count - 3), 
//                     bounds.x + 40, y_pos, FONT_SIZE-3, COLOR_TEXT);
//             y_pos += 25;
//         }
//     } else {
//         DrawText("No processes blocked", bounds.x + 20, y_pos, FONT_SIZE-3, COLOR_TEXT);
//         y_pos += 25;
//     }
// }

void DrawMutexStatus(Rectangle bounds, SystemState *system) {
    int y_pos = bounds.y + 40;
    
    for (int i = 0; i < 3; i++) {
        Mutex *mutex = &system->mutexes[i];
        
        DrawText(mutex->resource_name, bounds.x + 10, y_pos, FONT_SIZE, COLOR_HIGHLIGHT);
        y_pos += 25;
        
        if (mutex->is_locked && mutex->holding_process) {
            DrawText(TextFormat("Locked by PID %d", mutex->holding_process->process_id), 
                    bounds.x + 30, y_pos, FONT_SIZE-3, COLOR_TEXT);
        } else {
            DrawText("Available", bounds.x + 30, y_pos, FONT_SIZE-3, COLOR_TEXT);
        }
        y_pos += 25;
        
        if (mutex->blocked_queue.count > 0) {
            DrawText(TextFormat("%d waiting:", mutex->blocked_queue.count), 
                    bounds.x + 30, y_pos, FONT_SIZE-3, COLOR_TEXT);
            y_pos += 25;
            
            for (int j = 0; j < mutex->blocked_queue.count; j++) {
                if (j < 2) { // Show first 2 waiting processes
                    DrawText(TextFormat("PID %d (Priority: %d)", 
                            mutex->blocked_queue.processes[j]->process_id,
                            mutex->blocked_queue.processes[j]->priority), 
                            bounds.x + 50, y_pos, FONT_SIZE-3, COLOR_TEXT);
                    y_pos += 25;
                }
            }
            
            if (mutex->blocked_queue.count > 2) {
                DrawText(TextFormat("+%d more...", mutex->blocked_queue.count - 2), 
                        bounds.x + 50, y_pos, FONT_SIZE-3, COLOR_TEXT);
                y_pos += 25;
            }
        } else {
            DrawText("No processes waiting", bounds.x + 30, y_pos, FONT_SIZE-3, COLOR_TEXT);
            y_pos += 25;
        }
        
        y_pos += 15;
    }
}

void DrawMemoryViewer(Rectangle bounds, SystemState *system) {
    int cols = 7;
    int rows = 8;
    float cell_width = bounds.width / cols;
    float cell_height = (bounds.height / rows)-20;
    
    for (int i = 0; i < 60; i++) {
        int row = i / cols;
        int col = i % cols;
        float x = bounds.x + col * cell_width;
        float y = bounds.y + row * cell_height;
        
        // Draw cell
        Color cell_color = COLOR_PANEL;
        if (system->memory[i].name != NULL) {
            if (strstr(system->memory[i].name, "instr_") != NULL) {
                cell_color = (Color){ 70, 70, 120, 255 }; // Blue for instructions
            } else if (strcmp(system->memory[i].name, "a") == 0 || strcmp(system->memory[i].name, "b") == 0 || strcmp(system->memory[i].name, " ") == 0 ) {
                cell_color = (Color){ 120, 70, 70, 255 }; // Red for variables
            } else if (strstr(system->memory[i].name, "process_id") != NULL ||
                      strstr(system->memory[i].name, "state") != NULL ||
                      strstr(system->memory[i].name, "priority") != NULL ||
                      strstr(system->memory[i].name, "program_counter") != NULL ||
                      strstr(system->memory[i].name, "memory_lower_bound") != NULL ||
                      strstr(system->memory[i].name, "memory_upper_bound") != NULL) {
                cell_color = (Color){ 70, 120, 70, 255 }; // Green for PCB fields
            }
        }
        
        DrawRectangle(x, y, cell_width - 2, cell_height - 2, cell_color);
        DrawRectangleLines(x, y, cell_width - 2, cell_height - 2, COLOR_HIGHLIGHT);
        
        // Draw memory content
        if (system->memory[i].name != NULL) {
            char content[50];
            if (system->memory[i].data != NULL) {
                snprintf(content, sizeof(content), "%s: %s", system->memory[i].name, system->memory[i].data);
            } else {
                snprintf(content, sizeof(content), "%s", system->memory[i].name);
            }
            
            // // Truncate if too long
            // if (strlen(content) > 15) {
            //     content[12] = '.';
            //     content[13] = '.';
            //     content[14] = '.';
            //     content[15] = '\0';
            // }
            
            DrawText(content, x + 5, y + 5, 11, COLOR_TEXT);
            

        } else {
            DrawText(TextFormat("%d", i), x + 5, y + 5, 12, COLOR_TEXT);
        }
    }
}



// void DrawLogPanel(Rectangle bounds, GUIState *gui_state) {
//     // Draw the panel background and border
//     DrawRectangleRec(bounds, (Color){50, 50, 50, 255}); // Darker background for contrast
//     DrawRectangleLinesEx(bounds, 2, COLOR_HIGHLIGHT);
    
//     // Draw title
//     DrawText("Execution Log", bounds.x + 10, bounds.y + 10, FONT_SIZE, COLOR_TEXT);
    
//     // Calculate how many log lines we can display
//     int line_height = 20;
//     int max_lines = (bounds.height - 40) / line_height;
//     int start_line = (gui_state->log_count > max_lines) ? gui_state->log_count - max_lines : 0;
    
//     // Draw log messages with a scrollable view
//     BeginScissorMode(bounds.x, bounds.y + 30, bounds.width, bounds.height - 30);
//     {
//         int y_pos = bounds.y + 30;
//         for (int i = start_line; i < gui_state->log_count; i++) {
//             DrawText(gui_state->log_text[i], bounds.x + 10, y_pos, 16, COLOR_TEXT);
//             y_pos += line_height;
            
//             // Stop drawing if we've filled the available space
//             if (y_pos > bounds.y + bounds.height) break;
//         }
//     }
//     EndScissorMode();
    
//     // Draw scrollbar if needed
//     if (gui_state->log_count > max_lines) {
//         float scrollbar_width = 10;
//         float scrollbar_height = bounds.height - 30;
//         float thumb_height = scrollbar_height * (max_lines / (float)gui_state->log_count);
//         float thumb_position = (scrollbar_height - thumb_height) * (start_line / (float)(gui_state->log_count - max_lines));
        
//         // Scrollbar track
//         DrawRectangle(bounds.x + bounds.width - scrollbar_width - 2, bounds.y + 30, 
//                      scrollbar_width, scrollbar_height, (Color){80, 80, 80, 255});
        
//         // Scrollbar thumb
//         DrawRectangle(bounds.x + bounds.width - scrollbar_width - 2, bounds.y + 30 + thumb_position, 
//                      scrollbar_width, thumb_height, COLOR_HIGHLIGHT);
//     }
// }

void DrawLogPanel(Rectangle bounds, GUIState *gui_state) {
    // Draw the panel background and border
    DrawRectangleRec(bounds, (Color){50, 50, 50, 255});
    DrawRectangleLinesEx(bounds, 2, COLOR_HIGHLIGHT);
    
    // Draw title
    DrawText("Execution Log", bounds.x + 10, bounds.y + 10, FONT_SIZE, COLOR_TEXT);
    
    // Calculate how many log lines we can display
    int line_height = 20;
    int max_lines = (bounds.height - 40) / line_height;
    int start_line = 0;
    
    // Calculate total content height and visible area
    float content_height = gui_state->log_count * line_height;
    float visible_height = bounds.height - 40;
    
    // Handle scrolling with mouse wheel
    if (CheckCollisionPointRec(GetMousePosition(), bounds)) {
        float wheel = GetMouseWheelMove();
        gui_state->log_scroll_offset -= wheel * line_height * 3; // Scroll 3 lines at a time
        
        // Clamp scroll offset
        if (content_height > visible_height) {
            // Replace Clamp() with manual clamping
            if (gui_state->log_scroll_offset < 0) gui_state->log_scroll_offset = 0;
            if (gui_state->log_scroll_offset > content_height - visible_height)
                gui_state->log_scroll_offset = content_height - visible_height;
        } else {
            gui_state->log_scroll_offset = 0;
        }
    }
    
    // Calculate which lines to display based on scroll offset
    start_line = gui_state->log_scroll_offset / line_height;
    
    // Calculate y position without using modulo on float
    int y_pos = bounds.y + 30 - ((int)gui_state->log_scroll_offset % line_height);
    
    // Draw log messages with clipping
    BeginScissorMode(bounds.x, bounds.y + 30, bounds.width - 15, bounds.height - 30);
    {
        for (int i = start_line; i < gui_state->log_count && i < start_line + max_lines + 1; i++) {
            DrawText(gui_state->log_text[i], bounds.x + 10, y_pos, 16, COLOR_TEXT);
            y_pos += line_height;
        }
    }
    EndScissorMode();
    
    // Draw scrollbar if needed
    if (content_height > visible_height) {
        float scrollbar_width = 10;
        float scrollbar_x = bounds.x + bounds.width - scrollbar_width - 2;
        
        // Scrollbar track
        DrawRectangle(scrollbar_x, bounds.y + 30, scrollbar_width, visible_height, (Color){80, 80, 80, 255});
        
        // Calculate thumb size and position
        float thumb_height = visible_height * (visible_height / content_height);
        if (thumb_height < 20) thumb_height = 20;
        if (thumb_height > visible_height) thumb_height = visible_height;
        
        float thumb_position = (visible_height - thumb_height) * 
                             (gui_state->log_scroll_offset / (content_height - visible_height));
        
        // Scrollbar thumb
        DrawRectangle(scrollbar_x, bounds.y + 30 + thumb_position, 
                     scrollbar_width, thumb_height, COLOR_HIGHLIGHT);
        
        // Handle scrollbar dragging
        Rectangle scrollbar_rect = {scrollbar_x, bounds.y + 30, scrollbar_width, visible_height};
        if (CheckCollisionPointRec(GetMousePosition(), scrollbar_rect)) {
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                float mouse_y = GetMousePosition().y - (bounds.y + 30);
                float normalized = mouse_y / visible_height;
                gui_state->log_scroll_offset = normalized * (content_height - visible_height);
                // Manual clamping
                if (gui_state->log_scroll_offset < 0) gui_state->log_scroll_offset = 0;
                if (gui_state->log_scroll_offset > content_height - visible_height)
                    gui_state->log_scroll_offset = content_height - visible_height;
            }
        }
    }
}

Color GetStateColor(ProcessState state) {
    switch (state) {
        case READY: return COLOR_READY;
        case RUNNING: return COLOR_RUNNING;
        case BLOCKED: return COLOR_BLOCKED;
        case TERMINATED: return COLOR_TERMINATED;
        default: return COLOR_TEXT;
    }
}


// #include "osproj.h"
// #include <raylib.h>
// #define RAYGUI_IMPLEMENTATION
// #include <raygui.h>
// #include <string.h>
// #include <stdio.h>
// #include <stdlib.h>

// extern char input_variable[100]; // Declared in testOS.c

// // Global variables
// extern SystemState os_system;
// OSGuiState gui_state = {0};
// ProcessInfo processes[10];
// int process_count = 0;
// int time_run = 0;

// // Add log message
// void add_log(const char* message) {
//     if (gui_state.log_count < 1000) {
//         strncpy(gui_state.log[gui_state.log_count], message, 255);
//         gui_state.log[gui_state.log_count][255] = '\0';
//         gui_state.log_count++;
//         gui_state.log_scroll.y = gui_state.log_count * 20.0f;
//     }
// }

// // Initialize processes
// void init_processes() {
//     // Free existing process strings to prevent memory leaks
//     for (int i = 0; i < process_count; i++) {
//         free(processes[i].program_file);
//     }
//     process_count = 3;
//     processes[0] = (ProcessInfo){1, strdup("Program_1.txt"), 0, 0};
//     processes[1] = (ProcessInfo){2, strdup("Program_2.txt"), 1, 20};
//     processes[2] = (ProcessInfo){3, strdup("Program_3.txt"), 2, 40};
//     gui_state.new_process_id = process_count + 1;
// }

// // Execute one clock cycle
// void step_simulation() {
//     // Create arriving processes
//     for (int i = 0; i < process_count; i++) {
//         if (processes[i].arrival_time == os_system.clock_cycle) {
//             create_process(processes[i].process_id, processes[i].program_file, processes[i].start_index);
//             add_log(TextFormat("Process %d created at clock %d", processes[i].process_id, os_system.clock_cycle));
//         }
//     }

//     // Schedule and execute
//     PCB* next_process = NULL;
//     if (os_system.scheduler == FCFS) {
//         next_process = schedule_fcfs();
//     } else if (os_system.scheduler == ROUND_ROBIN) {
//         next_process = schedule_round_robin(&time_run);
//     } else if (os_system.scheduler == MLFQ) {
//         next_process = schedule_mlfq(&time_run);
//     }
//     os_system.running_process = next_process;

//     if (next_process && next_process->state == RUNNING) {
//         char* instruction = get_instruction(next_process);
//         if (instruction) {
//             add_log(TextFormat("Clock %d: PID %d executing: %s", os_system.clock_cycle, next_process->process_id, instruction));
//             readProgramSyntax(instruction);
//             if (next_process->state != BLOCKED) {
//                 next_process->program_counter++;
//                 if (next_process->program_counter >= next_process->instruction_count) {
//                     next_process->state = TERMINATED;
//                     store_pcb(next_process->memory_lower_bound, next_process->instruction_count, 2, next_process);
//                     add_log(TextFormat("PID %d terminated", next_process->process_id));
//                     os_system.running_process = NULL;
//                     time_run = 0;
//                 } else {
//                     time_run++;
//                 }
//             } else {
//                 time_run = 0;
//             }
//             store_pcb(next_process->memory_lower_bound, next_process->instruction_count, 2, next_process);
//         } else {
//             next_process->state = TERMINATED;
//             store_pcb(next_process->memory_lower_bound, next_process->instruction_count, 2, next_process);
//             add_log(TextFormat("PID %d terminated", next_process->process_id));
//             os_system.running_process = NULL;
//             time_run = 0;
//         }
//     } else {
//         add_log(TextFormat("Clock %d: No ready processes", os_system.clock_cycle));
//     }

//     os_system.clock_cycle++;
// }

// // Check if simulation is complete
// bool is_simulation_complete() {
//     for (int i = 0; i < 4; i++) {
//         if (os_system.ready_queues[i].count > 0) return false;
//     }
//     if (os_system.Blocked_queue.count > 0) return false;
//     if (os_system.running_process && os_system.running_process->state != TERMINATED) return false;
//     return true;
// }

// int main(void) {
//     // Initialize Raylib
//     InitWindow(1280, 720, "OS Simulator");
//     SetTargetFPS(60);

//     // Initialize GUI state
//     gui_state.simulation_running = false;
//     gui_state.step_mode = false;
//     gui_state.selected_scheduler = 0;
//     gui_state.quantum = 2;
//     gui_state.input_active = false;
//     gui_state.new_process_id = process_count + 1;
//     gui_state.new_arrival_time = 0;
//     gui_state.new_start_index = 0;
//     gui_state.log_count = 0;
//     gui_state.log_scroll = (Vector2){0.0f, 0.0f};
//     memset(gui_state.input_buffer, 0, sizeof(gui_state.input_buffer));
//     memset(gui_state.process_file_buffer, 0, sizeof(gui_state.process_file_buffer));

//     // Initialize system
//     init_processes();
//     init_system(ROUND_ROBIN, gui_state.quantum);

//     // Main loop
//     while (!WindowShouldClose()) {
//         // Handle input for text fields
//         if (gui_state.input_active) {
//             int key = GetCharPressed();
//             while (key > 0) {
//                 if (key >= 32 && key <= 125 && strlen(gui_state.input_buffer) < 99) {
//                     gui_state.input_buffer[strlen(gui_state.input_buffer)] = (char)key;
//                 }
//                 key = GetCharPressed();
//             }
//             if (IsKeyPressed(KEY_BACKSPACE) && strlen(gui_state.input_buffer) > 0) {
//                 gui_state.input_buffer[strlen(gui_state.input_buffer) - 1] = '\0';
//             }
//             if (IsKeyPressed(KEY_ENTER) && strlen(gui_state.input_buffer) > 0) {
//                 // Handle input for assign instruction
//                 if (os_system.running_process) {
//                     set_variable(os_system.running_process, "input_var", gui_state.input_buffer);
//                     add_log(TextFormat("Input assigned: %s", gui_state.input_buffer));
//                     gui_state.input_active = false;
//                     memset(gui_state.input_buffer, 0, sizeof(gui_state.input_buffer));
//                 }
//             }
//         }

//         // Run simulation
//         if (gui_state.simulation_running && !gui_state.step_mode && !is_simulation_complete()) {
//             step_simulation();
//         }

//         // Begin drawing
//         BeginDrawing();
//         ClearBackground(RAYWHITE);

//         // Main Dashboard
//         DrawText("OS Simulator Dashboard", 20, 20, 20, BLACK);
//         DrawText(TextFormat("Clock Cycle: %d", os_system.clock_cycle), 20, 50, 16, BLACK);
//         DrawText(TextFormat("Total Processes: %d", process_count), 20, 70, 16, BLACK);
//         DrawText(TextFormat("Scheduler: %s", gui_state.selected_scheduler == 0 ? "FCFS" : 
//                            (gui_state.selected_scheduler == 1 ? "Round Robin" : "MLFQ")), 20, 90, 16, BLACK);

//         // Process List
//         DrawText("Process List", 20, 120, 18, BLACK);
//         DrawRectangleLines(20, 140, 300, 200, BLACK);
//         int y = 150;
//         for (int i = 0; i < process_count; i++) {
//             PCB temp_pcb;
//             retrieve_pcb(processes[i].start_index, 15, 2, &temp_pcb); // Assume max 15 instructions
//             DrawText(TextFormat("PID %d: State=%s, Pri=%d, PC=%d, Mem=%d-%d", 
//                         temp_pcb.process_id, 
//                         temp_pcb.state == READY ? "READY" : 
//                         (temp_pcb.state == RUNNING ? "RUNNING" : 
//                          (temp_pcb.state == BLOCKED ? "BLOCKED" : "TERMINATED")),
//                         temp_pcb.priority, temp_pcb.program_counter,
//                         temp_pcb.memory_lower_bound, temp_pcb.memory_upper_bound), 
//                      30, y, 14, BLACK);
//             y += 20;
//         }

//         // Queue Section
//         DrawText("Queues", 340, 120, 18, BLACK);
//         DrawRectangleLines(340, 140, 300, 200, BLACK);
//         y = 150;
//         for (int i = 0; i < 4; i++) {
//             if (os_system.ready_queues[i].count > 0) {
//                 char queue_str[256] = "";
//                 strcat(queue_str, TextFormat("Ready Q%d: [", i));
//                 for (int j = 0; j < os_system.ready_queues[i].count; j++) {
//                     strcat(queue_str, TextFormat("%d", os_system.ready_queues[i].processes[j]->process_id));
//                     if (j < os_system.ready_queues[i].count - 1) strcat(queue_str, ", ");
//                 }
//                 strcat(queue_str, "]");
//                 DrawText(queue_str, 350, y, 14, BLACK);
//                 y += 20;
//             }
//         }
//         if (os_system.Blocked_queue.count > 0) {
//             char queue_str[256] = "Blocked Q: [";
//             for (int i = 0; i < os_system.Blocked_queue.count; i++) {
//                 strcat(queue_str, TextFormat("%d", os_system.Blocked_queue.processes[i]->process_id));
//                 if (i < os_system.Blocked_queue.count - 1) strcat(queue_str, ", ");
//             }
//             strcat(queue_str, "]");
//             DrawText(queue_str, 350, y, 14, BLACK);
//         }
//         if (os_system.running_process) {
//             DrawText(TextFormat("Running: PID %d, Instr: %s", 
//                         os_system.running_process->process_id, 
//                         get_instruction(os_system.running_process) ? get_instruction(os_system.running_process) : "None"), 
//                      350, y + 20, 14, BLACK);
//         }

//         // Scheduler Control Panel
//         DrawText("Scheduler Control", 20, 360, 18, BLACK);
//         DrawRectangleLines(20, 380, 300, 150, BLACK);
//         if (GuiDropdownBox((Rectangle){30, 390, 280, 30}, "FCFS;Round Robin;MLFQ", 
//                            &gui_state.selected_scheduler, !gui_state.simulation_running)) {
//             SchedulerType scheduler = gui_state.selected_scheduler == 0 ? FCFS : 
//                                      (gui_state.selected_scheduler == 1 ? ROUND_ROBIN : MLFQ);
//             init_system(scheduler, gui_state.quantum);
//             init_processes();
//             add_log("Scheduler changed and system reset");
//         }
//         GuiLabel((Rectangle){30, 430, 100, 20}, "Quantum:");
//         GuiSpinner((Rectangle){130, 430, 180, 20}, NULL, 
//                    &gui_state.quantum, 1, 10, !gui_state.simulation_running);
//         if (GuiButton((Rectangle){30, 460, 90, 30}, gui_state.simulation_running ? "Stop" : "Start")) {
//             gui_state.simulation_running = !gui_state.simulation_running;
//             gui_state.step_mode = false;
//             if (!gui_state.simulation_running) add_log("Simulation stopped");
//             else add_log("Simulation started");
//         }
//         if (GuiButton((Rectangle){130, 460, 90, 30}, "Step")) {
//             if (!gui_state.simulation_running) {
//                 gui_state.step_mode = true;
//                 step_simulation();
//                 add_log("Simulation stepped");
//             }
//         }
//         if (GuiButton((Rectangle){230, 460, 90, 30}, "Reset")) {
//             init_system(gui_state.selected_scheduler == 0 ? FCFS : 
//                        (gui_state.selected_scheduler == 1 ? ROUND_ROBIN : MLFQ), gui_state.quantum);
//             init_processes();
//             gui_state.simulation_running = false;
//             gui_state.step_mode = false;
//             gui_state.log_count = 0;
//             gui_state.log_scroll = (Vector2){0.0f, 0.0f};
//             time_run = 0;
//             add_log("System reset");
//         }

//         // Resource Management Panel
//         DrawText("Resources", 340, 360, 18, BLACK);
//         DrawRectangleLines(340, 380, 300, 150, BLACK);
//         y = 390;
//         for (int i = 0; i < 3; i++) {
//             char status[256];
//             snprintf(status, sizeof(status), "%s: %s%s", os_system.mutexes[i].resource_name,
//                      os_system.mutexes[i].is_locked ? "Locked by PID " : "Free",
//                      os_system.mutexes[i].is_locked && os_system.mutexes[i].holding_process ? 
//                      TextFormat("%d", os_system.mutexes[i].holding_process->process_id) : "");
//             DrawText(status, 350, y, 14, BLACK);
//             if (os_system.mutexes[i].blocked_queue.count > 0) {
//                 char blocked[256] = "Blocked: [";
//                 for (int j = 0; j < os_system.mutexes[i].blocked_queue.count; j++) {
//                     strcat(blocked, TextFormat("%d", os_system.mutexes[i].blocked_queue.processes[j]->process_id));
//                     if (j < os_system.mutexes[i].blocked_queue.count - 1) strcat(blocked, ", ");
//                 }
//                 strcat(blocked, "]");
//                 DrawText(blocked, 350, y + 15, 14, BLACK);
//                 y += 15;
//             }
//             y += 30;
//         }

//         // Memory Viewer
//         DrawText("Memory (60 Words)", 660, 20, 18, BLACK);
//         DrawRectangleLines(660, 40, 600, 300, BLACK);
//         for (int i = 0; i < 60; i++) {
//             int row = i / 10;
//             int col = i % 10;
//             Color color = (os_system.memory[i].name && os_system.memory[i].data) ? LIGHTGRAY : WHITE;
//             DrawRectangle(670 + col * 60, 50 + row * 50, 58, 48, color);
//             DrawRectangleLines(670 + col * 60, 50 + row * 50, 58, 48, BLACK);
//             if (os_system.memory[i].name && os_system.memory[i].data) {
//                 DrawText(TextFormat("%s: %s", os_system.memory[i].name, os_system.memory[i].data), 
//                          675 + col * 60, 55 + row * 50, 10, BLACK);
//             } else {
//                 DrawText("Empty", 675 + col * 60, 55 + row * 50, 10, BLACK);
//             }
//         }

//         // Log & Console Panel
//         DrawText("Execution Log", 660, 360, 18, BLACK);
//         DrawRectangleLines(660, 380, 600, 300, BLACK);
//         Rectangle log_area = {670, 390, 580, 280};
//         Rectangle view = {0};
//         GuiScrollPanel(log_area, NULL, 
//                        (Rectangle){0, 0, 580, gui_state.log_count * 20}, 
//                        &gui_state.log_scroll, &view);
//         BeginScissorMode(log_area.x, log_area.y, log_area.width, log_area.height);
//         y = 390 - gui_state.log_scroll.y;
//         for (int i = 0; i < gui_state.log_count; i++) {
//             DrawText(gui_state.log[i], 670, y + i * 20, 14, BLACK);
//         }
//         EndScissorMode();

//         // Process Creation
//         DrawText("Add Process", 20, 550, 18, BLACK);
//         DrawRectangleLines(20, 570, 620, 130, BLACK);
//         GuiLabel((Rectangle){30, 580, 100, 20}, "File Path:");
//         GuiTextBox((Rectangle){130, 580, 500, 20}, gui_state.process_file_buffer, 
//                    99, !gui_state.simulation_running);
//         GuiLabel((Rectangle){30, 610, 100, 20}, "Arrival Time:");
//         GuiSpinner((Rectangle){130, 610, 180, 20}, NULL, 
//                    &gui_state.new_arrival_time, 0, 100, !gui_state.simulation_running);
//         GuiLabel((Rectangle){30, 640, 100, 20}, "Start Index:");
//         GuiSpinner((Rectangle){130, 640, 180, 20}, NULL, 
//                    &gui_state.new_start_index, 0, 50, !gui_state.simulation_running);
//         if (GuiButton((Rectangle){30, 670, 100, 30}, "Add Process") && !gui_state.simulation_running) {
//             if (strlen(gui_state.process_file_buffer) > 0 && process_count < 10) {
//                 processes[process_count] = (ProcessInfo){
//                     gui_state.new_process_id++,
//                     strdup(gui_state.process_file_buffer),
//                     gui_state.new_arrival_time,
//                     gui_state.new_start_index
//                 };
//                 process_count++;
//                 add_log(TextFormat("Added process %d: %s", processes[process_count-1].process_id, 
//                                   gui_state.process_file_buffer));
//                 memset(gui_state.process_file_buffer, 0, sizeof(gui_state.process_file_buffer));
//                 gui_state.new_start_index += 20;
//             }
//         }

//         // Input Box for assign instruction
//         if (gui_state.input_active) {
//             DrawRectangle(400, 300, 480, 120, Fade(BLACK, 0.8f));
//             DrawText("Enter Input Value", 420, 320, 20, WHITE);
//             GuiTextBox((Rectangle){420, 350, 440, 30}, gui_state.input_buffer, 99, true);
//         }

//         EndDrawing();
//     }

//     // Cleanup
//     for (int i = 0; i < process_count; i++) {
//         free(processes[i].program_file);
//     }
//     CloseWindow();
//     return 0;
// }