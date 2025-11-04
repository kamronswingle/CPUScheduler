Name: Kamron Swingle
Course: CPSC 380 - Operating Systems
Email: swingle@chapman.edu
Assignment: Assignment 4 - CPU Scheduling Simulator
File: schedsim.c
School: Chapman University

Help Recieved from freind outside of school for basic debugging and process cleanup, more optimizing:
Name: Ian McQuerrey

References:

Structures:
https://www.geeksforgeeks.org/c/structures-c/

Enumerators:
https://www.w3schools.com/c/c_enums.php

Semaphors:
https://www.geeksforgeeks.org/c/use-posix-semaphores-c/
https://man7.org/linux/man-pages/man3/sem_init.3.html
https://man7.org/linux/man-pages/man3/sem_post.3.html

Parsing a file:
https://www.google.com/search?q=how+to+parse+a+file+in+c+function&sca_esv=9133454e1bbc4cf2&rlz=1C1JJTC_enUS1182US1182&sxsrf=AE3TifOCWFP7KWGyH8AzyJf4DysCGYmdhw%3A1761954392918&ei=WEoFaavfN5qzmtkP3N7uEA&ved=0ahUKEwir68zNz8-QAxWamSYFHVyvGwIQ4dUDCBM&uact=5&oq=how+to+parse+a+file+in+c+function&gs_lp=Egxnd3Mtd2l6LXNlcnAiIWhvdyB0byBwYXJzZSBhIGZpbGUgaW4gYyBmdW5jdGlvbjIFECEYoAEyBRAhGKABMgUQIRigATIFECEYoAFI6AZQswFYywZwAXgAkAEAmAGeAaAB6QiqAQMxLji4AQPIAQD4AQGYAgmgAqcIwgIHECMYsAMYJ8ICChAAGLADGNYEGEfCAgYQABgWGB7CAgUQABjvBZgDAIgGAZAGCZIHAzEuOKAHgTKyBwMwLji4B6UIwgcDMi43yAcL&sclient=gws-wiz-serp

Finding first occurrence of string (commas):
https://www.w3schools.com/c/ref_string_strcspn.php

Long_Opts:
https://linux.die.net/man/3/getopt_long

Struct assignment:
https://www.google.com/search?q=point+to+item+in+struct+c&rlz=1C1JJTC_enUS1182US1182&oq=point+to+item+in+struct+c&gs_lcrp=EgZjaHJvbWUyBggAEEUYOTIHCAEQIRigATIHCAIQIRigATIHCAMQIRifBTIHCAQQIRifBTIHCAUQIRifBTIHCAYQIRifBTIHCAcQIRifBTIHCAgQIRifBTIHCAkQIRifBdIBCDM2NjVqMGo0qAIAsAIA&sourceid=chrome&ie=UTF-8

Spawning threads:
https://stackoverflow.com/questions/4964142/how-to-spawn-n-threads

Waiting for threads:
https://stackoverflow.com/questions/11624545/how-to-make-main-thread-wait-for-all-child-threads-finish

Processing:
https://www.geeksforgeeks.org/c/multithreading-in-c/

Scheduling:
https://www.geeksforgeeks.org/c/multithreading-in-c/
https://www.geeksforgeeks.org/operating-systems/program-for-fcfs-cpu-scheduling-set-1/
https://www.geeksforgeeks.org/operating-systems/shortest-job-first-or-sjf-cpu-scheduling/
https://www.geeksforgeeks.org/operating-systems/program-for-round-robin-scheduling-for-the-same-arrival-time/
https://www.geeksforgeeks.org/c/c-program-to-implement-priority-queue/

strcopy:
https://www.geeksforgeeks.org/c/strcpy-in-c/

ChatGPT:
prompt used: "Help me make a gantt chart for a c multithreaded scheduler, help me clean up the output when I have apid, and variable gantt_count, that keeps track of time for the gant chart"

prompt used: "Check these outputs to make sure they are the correct implementation", pasted the outputs of the code
Response: "...your priority is not premptive, ... make sure you do this 
if (new_proc.priority < current_proc.priority)
    preempt_current_process();"


Instructions to Compile:
gcc schedsim.c -o schedsim -lpthread

Demo Run (FCFS):
./schedsim -f -i processes.csv

Demo Run (Round Robin)
./schedsim -rr -q 3 -i processes.csv