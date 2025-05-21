# 🖥️ OS Project - Milestone 2 (Spring 2025)

## 📌 Project Overview

This project simulates a simplified **Operating System Scheduler** in C, incorporating **process management**, **memory allocation**, **scheduling algorithms**, and **mutex-based synchronization**. It includes a real-time **Graphical User Interface** built using Raylib to visualize memory, processes, queues, and resource locks.

Milestone 2 focuses on:

- Creating and scheduling processes from program files.
- Implementing three scheduling strategies.
- Managing shared resource access through mutexes.
- Visualizing OS behavior through an interactive GUI.

---

## 🛠️ Features

### 🧠 Process Simulation
- Parses and runs 3 custom programs:
  - `Program_1.txt`: Print numbers between two integers.
  - `Program_2.txt`: Write user input to a file.
  - `Program_3.txt`: Read and print file contents.
- Each program simulates a process with its own PCB and memory space.

### 🧮 Scheduler Algorithms
- **FCFS** (First-Come First-Serve)
- **Round Robin** (with adjustable quantum)
- **Multilevel Feedback Queue (MLFQ)**
  - 4 priority levels
  - Dynamic time quantums: {1, 2, 4, 8}

### 💾 Memory Management
- Fixed memory: 60 words
- Each process is allocated space for:
  - Instructions
  - 3 variables
  - PCB metadata

### 🔒 Mutual Exclusion (Mutexes)
- Three critical resources:
  - `userInput`
  - `userOutput`
  - `file`
- Managed using `semWait` and `semSignal`
- Blocked queue implemented with priority unblocking

### 🖼️ GUI Simulation (Raylib)
- Live visualization of:
  - System clock, current instruction
  - Ready & Blocked Queues
  - Memory status
  - Mutex holders & waitlists
  - Process states & priority

---

## 📷 Check Out the GUI

Check out our custom-built Raylib GUI that visualizes process scheduling, memory usage, and mutexes in real-time:


Checkout the GUI below
![image](https://github.com/user-attachments/assets/76f11d67-4499-4961-9d6c-e3b554a19137)
![image](https://github.com/user-attachments/assets/89428273-e825-4044-adaa-a16336fd51a3)

---

## 🚀 How to Run

### 🔧 Requirements
- C compiler (e.g. `gcc`)
- [Raylib](https://www.raylib.com/) installed

