#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
// one thread per process and coordinates scheduling among them with semaphores.
// needs to surpport fcfs, sjf, rr, priority scheduling algorithms.

// Part 1 - File processing (done)
// Part 2 - Queueing
// Part 3 - Threading
// Part 4 - Scheduling
// Part 5 - Scheduling algorithms
// Part 6 - Metrics

#define MAX_PROCESSES 100
#define BUFFER_SIZE 256

typedef enum {
    FCFS,
    SJF,
    RR,
    PRIORITY
} SchedulingAlgorithm;

typedef struct {
    char pid[32]; // random max size for PID
    int arrival;
    int burst;
    int priority;
    
    // Runtime state
    int remaining_time;
    sem_t semaphore;
    pthread_t thread;
    
    // Metrics
    int start_time;
    int finish_time;
    int waiting_time;
    int response_time;
    int turnaround_time;
    
    int started;  // Flag: has this process run yet?
    int finished; // Flag: is this process done?
    int in_ready_queue;
} Process;

// Add after ready_count declaration
typedef struct {
    char pid[32];
    int start;
    int end;
} GanttEntry;

GanttEntry gantt_chart[MAX_PROCESSES * 100];
int gantt_count = 0;

sem_t scheduler_sem; // global semaphore for scheduler to signal processes

pthread_mutex_t scheduler_mutex = PTHREAD_MUTEX_INITIALIZER;

Process *processes = NULL;
int process_count = 0;
int current_time = 0;
SchedulingAlgorithm algorithm = FCFS; // default algorithm
int time_quantum = 1;


// Ready queue
Process* ready_queue[MAX_PROCESSES];
int ready_count = 0;

// Function prototypes

// Parsing
void parse_file(const char* filename);
// TODO: long opts

// Queue Operations
void enqueue_process(Process* process);
void dequeue_process(Process* process);

// Spawning threads
void spawn_threads();

// Threading
void* process_thread(void* arg);

// Thread cleanup
void wait_threads();

// Scheduling
void run_scheduler();
Process* select_next_process();

// Printing
void print_results();
void print_gantt_chart();

// Long opts
static void print_usage(const char *progname);

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

void run_scheduler() {
    int processes_finished = 0;
    int time_slice = 0;
    Process* current_running = NULL;
    int execution_start = -1;

    while (processes_finished < process_count) {
        pthread_mutex_lock(&scheduler_mutex);

        // 1. Check for new arrivals
        for (int i = 0; i < process_count; i++) {
            if (processes[i].arrival == current_time && !processes[i].in_ready_queue && !processes[i].finished) {
                enqueue_process(&processes[i]);
            }
        }

        // 2. Handle preemption for RR (time quantum expired)
        if (algorithm == RR && current_running != NULL && !current_running->finished && time_slice >= time_quantum) {
            // Record Gantt entry
            if (execution_start != -1) {
                strcpy(gantt_chart[gantt_count].pid, current_running->pid);
                gantt_chart[gantt_count].start = execution_start;
                gantt_chart[gantt_count].end = current_time;
                gantt_count++;
            }
            
            // Re-queue the preempted process
            enqueue_process(current_running);
            current_running = NULL;
            time_slice = 0;
            execution_start = -1;
        }
        // 2b. Handle preemption for Priority
        else if (algorithm == PRIORITY && current_running != NULL && !current_running->finished) {
            // Check if a higher priority process is available
            for (int i = 0; i < ready_count; i++) {
                if (ready_queue[i]->priority < current_running->priority) {
                    // Record Gantt entry
                    if (execution_start != -1) {
                        strcpy(gantt_chart[gantt_count].pid, current_running->pid);
                        gantt_chart[gantt_count].start = execution_start;
                        gantt_chart[gantt_count].end = current_time;
                        gantt_count++;
                    }
                    
                    // Preempt current process
                    enqueue_process(current_running);
                    current_running = NULL;
                    time_slice = 0;
                    execution_start = -1;
                    break;
                }
            }
        }

        // 3. Select next process if none is running
        if (current_running == NULL || current_running->finished) {
            if (current_running != NULL && current_running->finished) {
                // Record Gantt entry for finished process
                if (execution_start != -1) {
                    strcpy(gantt_chart[gantt_count].pid, current_running->pid);
                    gantt_chart[gantt_count].start = execution_start;
                    gantt_chart[gantt_count].end = current_time;
                    gantt_count++;
                    execution_start = -1;
                }
                processes_finished++;
            }
            
            current_running = select_next_process();
            time_slice = 0;
            
            if (current_running != NULL) {
                dequeue_process(current_running);  // Remove from ready queue
                execution_start = current_time;    // Mark when this process starts
            }
        }

        pthread_mutex_unlock(&scheduler_mutex);

        // 4. Execute one cycle if we have a process
        if (current_running != NULL) {
            sem_post(&current_running->semaphore);  // Signal process to run
            sem_wait(&scheduler_sem);               // Wait for it to finish cycle
            
            time_slice++;
        }

        // 5. Advance time
        current_time++;
    }
}

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

        //processes[i].waiting_time = processes[i].turnaround_time - processes[i].burst;
    }
}

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
    printf("CPU Utilization = 100%%\n\n");  // Assuming no idle time for now

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
    
    // Print process names
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



int main(int argc, char* argv[]) {
    processes = malloc(sizeof(Process) * MAX_PROCESSES);
    sem_init(&scheduler_sem, 0, 0);

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

    parse_file(filename);

    spawn_threads();

    run_scheduler();

    wait_threads();

    sem_destroy(&scheduler_sem);
    pthread_mutex_destroy(&scheduler_mutex);

    print_results();

    free(processes);

    return 0;
}