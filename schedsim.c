/*
    Name: Kamron Swingle
    Course: CPSC 380 - Operating Systems
    Email: swingle@chapman.edu
    Assignment: Assignment 4 - CPU Scheduling Simulator
    File: schedsim.c
    School: Chapman University
*/

#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

// Constants
#define MAX_PROCESSES 100
#define BUFFER_SIZE 256
#define MAX_GANTT (MAX_PROCESSES * 100)

// Scheduling Algorithms
typedef enum {
    FCFS,
    SJF,
    RR,
    PRIORITY
} SchedulingAlgorithm;

// Process structure (individual process info)
typedef struct {
    // Process info (given in CSV)
    char pid[32];
    int arrival;
    int burst;
    int priority;
    
    // dyanamic info per process
    int remaining_time;
    sem_t semaphore;
    pthread_t thread;
    
    // metrics
    int start_time;
    int finish_time;
    int waiting_time;
    int response_time;
    int turnaround_time;
    
    // helper flags
    int started; 
    int finished;
    int in_ready_queue;
} Process;

// gantt chart entry
typedef struct {
    char pid[32];
    int start;
    int end;
} GanttEntry;

// process management
Process *processes = NULL;
int process_count = 0;

// scheduling state
SchedulingAlgorithm algorithm = FCFS; // default algorithm
int time_quantum = 1; // default time quantum for RR
int current_time = 0; 

// Ready queue
Process* ready_queue[MAX_PROCESSES];
int ready_count = 0;

// gantt chart
GanttEntry gantt_chart[MAX_PROCESSES * 100];
int gantt_count = 0;

// synchronization
sem_t scheduler_sem; // global semaphore for scheduler to signal processes
pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;

// global utilization
float cpu_utilization = 0.0;

// Function prototypes

// initialaization and cleanup
void initialize_scheduler(void);
void cleanup_scheduler(void);

// Parsing
void parse_file(const char* filename);

// Thread management
void spawn_threads(void);
void wait_threads(void);
void *process_thread(void *arg);

// Queue Operations
void enqueue_process(Process* process);
void dequeue_process(Process* process);

// Scheduling
void run_scheduler();
Process* select_next_process();

// Printing
void print_results();
void print_gantt_chart();
static void print_usage(const char *progname);

int main(int argc, char* argv[]) {
    char* filename = NULL;
    int algo_set = 0;
    

    static struct option long_opts[] = {
        {"fcfs", no_argument, 0, 'f'},
        {"sjf", no_argument, 0, 's'},
        {"rr", no_argument, 0, 'r'},
        {"priority", no_argument, 0, 'p'},
        {"input", required_argument, 0, 'i'},
        {"quantum", required_argument, 0, 'q'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "fsrpi:q:h", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'f': 
                algorithm = FCFS; 
                algo_set = 1; 
                break;
            case 's': 
                algorithm = SJF; 
                algo_set = 1; 
                break;
            case 'r':
                algorithm = RR; 
                algo_set = 1; 
                break;
            case 'p': 
                algorithm = PRIORITY; 
                algo_set = 1; 
                break;
            case 'i': 
                filename = optarg; 
                break;
            case 'q': 
                time_quantum = atoi(optarg); 
                break;
            case 'h': // If the user needs help, print it, but then clean up
                print_usage(argv[0]);
                free(processes);
                sem_destroy(&scheduler_sem);
                exit(0);
            default:
                print_usage(argv[0]);
                free(processes);
                sem_destroy(&scheduler_sem);
                exit(1);
        }
    }

    if (!algo_set || !filename) {
        fprintf(stderr, "Error: must specify algorithm and input file.\n\n");
        print_usage(argv[0]);
        free(processes);
        sem_destroy(&scheduler_sem);
        return 1;
    }

    // Initialize and run scheduler
    initialize_scheduler();
    parse_file(filename);
    spawn_threads();
    run_scheduler();
    wait_threads();
    print_results();
    cleanup_scheduler();

    return 0;
}

// initialaization and cleanup
void initialize_scheduler(void) {
    processes = malloc(sizeof(Process) * MAX_PROCESSES);
    if (processes == NULL) {
        perror("Failed to allocate memory for processes");
        exit(1);
    }
    sem_init(&scheduler_sem, 0, 0);
}

void cleanup_scheduler(void) {
    sem_destroy(&scheduler_sem);
    pthread_mutex_destroy(&scheduler_mutex);
    free(processes);
}

// file parsing
void parse_file(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file.\n");
    }
    char line[BUFFER_SIZE]; // buffer to store each line
    int lineNum = 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = 0;

        if (lineNum == 0) { // Skipping first line
            lineNum++;
            continue;
        }

        char* token = strtok(line, ",");
        int column = 0;
        Process* process = &processes[process_count];
        while (token != NULL) {
            
            switch (column) {
                case 0: // PID
                    strcpy(process->pid, token);
                    break;
                case 1: // Arrival Time
                    process->arrival = atoi(token);
                    break;
                case 2: // Burst Time
                    process->burst = atoi(token);
                    break;
                case 3: // Priority
                    process->priority = atoi(token);
                    break;
            }
            token = strtok(NULL, ",");
            column++;
        }
        process->remaining_time = process->burst;
        process->start_time = -1;
        process->finish_time = 0;
        process->waiting_time = 0;
        process->response_time = -1;
        process->turnaround_time = 0;
        process->in_ready_queue = 0;
        process->finished = 0;
        sem_init(&process->semaphore, 0, 0); // Initialize semaphore
        process_count++;
        lineNum++;
    }
    fclose(file);
}

void spawn_threads() {
    for (int i = 0; i < process_count; i++) {
        pthread_create(&processes[i].thread, NULL, process_thread, (void*)&processes[i]);
    }
}

void wait_threads() {
    for (int i = 0; i < process_count; i++) {
        pthread_join(processes[i].thread, NULL); // Join the thread
        sem_destroy(&processes[i].semaphore); // Also destroy the semaphore
    }
}

void* process_thread(void *arg) {
    Process *process = (Process*)arg;
    
    while (process->remaining_time > 0) {
        sem_wait(&process->semaphore);  // Wait for scheduler

        // Check if we should exit (safety check)
        if (process->finished) {
            break;
        }
        
        pthread_mutex_lock(&scheduler_mutex);
        
        // Execute one unit of work
        if (process->remaining_time > 0) {
            if (!process->started) {
                process->started = 1;
                process->start_time = current_time;
                process->response_time = current_time - process->arrival;  // Calculate response time
            }
            process->remaining_time--;
        }
        
        // Check if finished
        if (process->remaining_time == 0) {
            process->finished = 1;
            process->finish_time = current_time + 1; // +1 because we finish at end of this time unit
            process->turnaround_time = process->finish_time - process->arrival;
        }
        
        pthread_mutex_unlock(&scheduler_mutex);
        
        sem_post(&scheduler_sem);  // Signal scheduler we're done with this cycle
    }
    return NULL;
}


// queue operations
void enqueue_process(Process* process) {
    if (!process->in_ready_queue && !process->finished) {
        ready_queue[ready_count] = process;
        ready_count++;
        process->in_ready_queue = 1;
    }
}

void dequeue_process(Process* process) {
    for (int i = 0; i < ready_count; i++) {
        if (ready_queue[i] == process) {
            for (int j = i; j < ready_count - 1; j++) {
                ready_queue[j] = ready_queue[j + 1];
            }
            ready_count--;
            process->in_ready_queue = 0;
            break;
        }
    }
}

// scheduling
Process* select_next_process() {
    if (ready_count == 0) {
        return NULL;
    }

    int selected_index = 0;

    switch (algorithm) {
        case FCFS:
            // Just pick first (already in FIFO order)
            selected_index = 0;
            break;
            
        case SJF:
            // Find shortest remaining time
            for (int i = 1; i < ready_count; i++) {
                if (ready_queue[i]->remaining_time < ready_queue[selected_index]->remaining_time) {
                    selected_index = i;
                }
            }
            break;
            
        case RR:
            // Round robin - just pick first
            selected_index = 0;
            break;
            
        case PRIORITY:
            // Find highest priority (lowest number)
            for (int i = 1; i < ready_count; i++) {
                if (ready_queue[i]->priority < ready_queue[selected_index]->priority) {
                    selected_index = i;
                }
            }
            break;
    }

    return ready_queue[selected_index];
}

void run_scheduler(void) {
    int processes_finished = 0;
    Process *current_running = NULL;
    int quantum_remaining = 0;
    int cpu_busy_cycles = 0;
    int execution_start = -1;

    // Continue until all processes finish
    while (processes_finished < process_count) {
        pthread_mutex_lock(&scheduler_mutex);

        // STEP 1: Check for arrivals at current_time
        for (int i = 0; i < process_count; i++) {
            if (processes[i].arrival == current_time && 
                !processes[i].in_ready_queue && 
                !processes[i].finished) {
                enqueue_process(&processes[i]);
            }
        }

        // STEP 2: Handle preemption checks
        int should_preempt = 0;
        
        if (current_running != NULL && !current_running->finished) {
            // RR: Check quantum expiration
            if (algorithm == RR && quantum_remaining <= 0) {
                should_preempt = 1;
            }
            
            // Priority: Check for higher priority in ready queue
            if (algorithm == PRIORITY) {
                for (int i = 0; i < ready_count; i++) {
                    if (ready_queue[i]->priority < current_running->priority) {
                        should_preempt = 1;
                        break;
                    }
                }
            }
        }
        
        if (should_preempt) {
            // Record partial execution in Gantt chart
            if (execution_start != -1) {
                strcpy(gantt_chart[gantt_count].pid, current_running->pid);
                gantt_chart[gantt_count].start = execution_start;
                gantt_chart[gantt_count].end = current_time;
                gantt_count++;
            }
            
            enqueue_process(current_running);
            current_running = NULL;
            execution_start = -1;
        }

        // STEP 3: Select next process if needed
        if (current_running == NULL || current_running->finished) {
            if (current_running != NULL && current_running->finished) {
                // Record finished process in Gantt
                if (execution_start != -1) {
                    strcpy(gantt_chart[gantt_count].pid, current_running->pid);
                    gantt_chart[gantt_count].start = execution_start;
                    gantt_chart[gantt_count].end = current_time;
                    gantt_count++;
                }
                processes_finished++;
            }
            
            current_running = select_next_process();
            
            if (current_running != NULL) {
                dequeue_process(current_running);
                execution_start = current_time;
                quantum_remaining = time_quantum;
            }
        }

        // STEP 4: Execute ONE cycle
        if (current_running != NULL && processes_finished < process_count) {
            cpu_busy_cycles++;
            
            // Dispatch for ONE cycle
            pthread_mutex_unlock(&scheduler_mutex);
            sem_post(&current_running->semaphore);
            sem_wait(&scheduler_sem);
            
            quantum_remaining--;
            
        } else {
            // CPU idle this cycle
            pthread_mutex_unlock(&scheduler_mutex);
        }

        // STEP 5: Advance clock by 1 (only if we have not finished yet)
        if (processes_finished < process_count) {
            current_time++; // this fixes issue with less than 100% utilization issue
        }
    }
    
    // Calculate actual CPU utilization
    cpu_utilization = (float)cpu_busy_cycles / current_time * 100.0;
    // Store for later printing
}

// printing results
void print_results() {
    char algoString[16];
    switch (algorithm) {
        case FCFS:
            strcpy(algoString, "FCFS");
            break;
        case SJF:
            strcpy(algoString, "SJF");
            break;
        case RR:
            strcpy(algoString, "RR");
            break;
        case PRIORITY:
            strcpy(algoString, "Priority");
            break;
    }

    for (int i = 0; i < process_count; i++) {
        processes[i].waiting_time = processes[i].turnaround_time - processes[i].burst;
    }

    printf("\n====================== %s Scheduling ======================\n", algoString);
    printf("------------------------------------------------------------\n");
    printf("PID\tArr\tBurst\tStart\tFinish\tWait\tResp\tTurn\n");
    printf("------------------------------------------------------------\n");

    for (int i = 0; i < process_count; i++) {
        printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
               processes[i].pid,
               processes[i].arrival,
               processes[i].burst,
               processes[i].start_time,
               processes[i].finish_time,
               processes[i].waiting_time,
               processes[i].response_time,
               processes[i].turnaround_time);
    }
    printf("------------------------------------------------------------\n");
    
    // Calculate and print averages
    float avg_wait = 0, avg_resp = 0, avg_turn = 0;
    for (int i = 0; i < process_count; i++) {
        avg_wait += processes[i].waiting_time;
        avg_resp += processes[i].response_time;
        avg_turn += processes[i].turnaround_time;
    }
    avg_wait /= process_count;
    avg_resp /= process_count;
    avg_turn /= process_count;
    
    printf("\nAvg Wait = %.2f\n", avg_wait);
    printf("Avg Resp = %.2f\n", avg_resp);
    printf("Avg Turn = %.2f\n", avg_turn);
    printf("Throughput = %.2f jobs/unit time\n", (float)process_count / current_time);
    printf("CPU Utilization = %.2f%%\n\n", cpu_utilization);  // Oops, left this hard coded as 100% earlier for testing
    print_gantt_chart();
}

void print_gantt_chart() {
    if (gantt_count == 0) return;
    
    printf("\nTimeline (Gantt Chart):\n");
    
    // Print time markers
    for (int i = 0; i < gantt_count; i++) {
        printf("%-9d", gantt_chart[i].start);
    }
    printf("%d\n", gantt_chart[gantt_count - 1].end);
    
    // Print top separator
    for (int i = 0; i < gantt_count; i++) {
        printf("|--------");
    }
    printf("|\n");
    
    // Print process names, note ChatGPT did help me with this, mentioned in README
    for (int i = 0; i < gantt_count; i++) {
        int pid_len = strlen(gantt_chart[i].pid);
        int padding_left = (8 - pid_len) / 2;
        int padding_right = 8 - pid_len - padding_left;
        
        printf("|");
        for (int j = 0; j < padding_left; j++) printf(" ");
        printf("%s", gantt_chart[i].pid);
        for (int j = 0; j < padding_right; j++) printf(" ");
    }
    printf("|\n");
    
    // Print bottom separator
    for (int i = 0; i <= gantt_count; i++) {
        printf("--------");
    }
    printf("-\n");
}

static void print_usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "-f,  --fcfs                Use FCFS scheduling\n"
        "-s,  --sjf                 Use SJF (Shortest Job First) scheduling\n"
        "-r,  --rr                  Use Round Robin scheduling\n"
        "-p,  --priority            Use Priority scheduling\n"
        "-i,  --input <file>        Input CSV filename (required)\n"
        "-q,  --quantum <N>         Time quantum for Round Robin (default 1)\n"
        "-h,  --help                Show this help message\n",
        progname);
}
