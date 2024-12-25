#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#define MIN_GRANULARITY 10
#define SCHED_LATENCY 100
#define MAX_ALLP 2000

//time val
struct timeval tv;
double startTime;
double currentTime;

//priority number to weight convert
static const int prio_to_weight[40] = {
 /* -20 */     88761,     71755,     56483,     46273,     36291,
 /* -15 */     29154,     23254,     18705,     14949,     11916,
 /* -10 */      9548,      7620,      6100,      4904,      3906,
 /*  -5 */      3121,      2501,      1991,      1586,      1277,
 /*   0 */      1024,       820,       655,       526,       423,
 /*   5 */       335,       272,       215,       172,       137,
 /*  10 */       110,        87,        70,        56,        45,
 /*  15 */        36,        29,        23,        18,        15
};

//next burst index and length
int nextBurstIndex;
int nextBurstLength;

//common command line arguments
int inputType;
int rqlen;
int allp;
int outmode;

//parameters for F
char *inputFile;
FILE *in_file;
char *outputFile;
FILE *out_file;
int isWritingOnFile;

//parameters for C
int minPrio;
int maxPrio;
char *distPL;
int avgPL;
int minPL;
int maxPL;
char *distIAT;
int avgIAT;
int minIAT;
int maxIAT;

//stat tracker struct
struct stats{
    int tid;
    float birthTime;
    float finishTime;
    int totalLength;
    int priorityWeight;
    int cs;
};

//list for process stats
struct stats statList[MAX_ALLP];

//input file contents struct
struct inputFileContents{
    int processLength;
    int priorityValue;
    int iat;
};

//list for input from the text
struct inputFileContents content[MAX_ALLP];

//PCB struct
struct PCB{
    int index;
    char *state;
    int threadID;
    int processLength;
    int remainingLength;
    int priority;
    int vruntime;
    struct PCB *next;
};

//runqueue head
struct PCB *head;
int queueLength;

//thread function parameter struct
struct arg{
    int index;
    pthread_t tid;
    pthread_attr_t t_attr;
    pthread_cond_t t_cond;
};

//thread parameter list
struct arg t_args[MAX_ALLP];

//locks and condition variables
pthread_mutex_t lock;
pthread_cond_t cvScheduler = PTHREAD_COND_INITIALIZER;
pthread_cond_t cvProducer = PTHREAD_COND_INITIALIZER;

//working thread function
static void *working(void *arg_ptr){
    int index = ((struct arg *)arg_ptr)->index;

    //thread finds its own PCB
    struct PCB *myPCB = head;
    if(! (myPCB->index == index) ){
        while(myPCB->next != NULL){
            myPCB = myPCB->next;
            if(myPCB->index == index){
                break;
            }
        }
    }

    while(myPCB->remainingLength > 0){

        //gets the lock and sleeps on condition variable until nextBurstIndex points to it
        pthread_mutex_lock(&lock);
        while(nextBurstIndex != index){
            pthread_cond_wait (&(t_args[index - 1].t_cond), &lock);
        }

        //update its PCB
        myPCB->state = "running";
        int runtime = fmin(nextBurstLength , myPCB->remainingLength);
        myPCB->remainingLength = myPCB->remainingLength - runtime;
        myPCB->vruntime = myPCB->vruntime + ((prio_to_weight[20]/myPCB->priority) * runtime);
        
        //current program time
        gettimeofday(&tv, NULL);
        currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;

        //write output
        if(outmode == 2){
            printf("%f %d %s %d\n" , (double) currentTime , (int) myPCB->threadID , (char*) myPCB->state , runtime);
        }else if(outmode == 3){
            printf("timestamp: %f |PROCESS RUNNING IN CPU| pid: %d , burst length: %d , state: %s/n" , currentTime , myPCB->threadID , nextBurstLength , myPCB->state);
        }

        //write output to a file
        if(isWritingOnFile){
            fprintf(out_file , "timestamp: %f |PROCESS RUNNING IN CPU| pid: %d , burst length: %d , state: %s/n" , currentTime , myPCB->threadID , nextBurstLength , myPCB->state);
        }
        
        usleep(1000 * runtime);

        if(myPCB->remainingLength <= 0){
            myPCB->state = "terminated";

            //get current time and update stats
            gettimeofday(&tv, NULL);
            currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;
            statList[myPCB->index].finishTime = currentTime;

            //output
            if(outmode == 3){
                printf("timestamp: %f |PROCESS TERMINATED| pid: %d , vruntime: %d\n , priority: %d/n" , currentTime , myPCB->threadID , myPCB->vruntime , myPCB->priority);
            }

            //output to file
            if(isWritingOnFile){
                fprintf(out_file , "timestamp: %f |PROCESS TERMINATED| pid: %d , vruntime: %d\n , priority: %d/n" , currentTime , myPCB->threadID , myPCB->vruntime , myPCB->priority);
            }

            //remove PCB from the queue
            if(myPCB->index == head->index){
                head = head->next;
            }else{
                struct PCB *iterator = head;
                while(iterator->next != NULL){
                    if(iterator->next->index == myPCB->index){
                        iterator->next = iterator->next->next;
                        break;
                    }else{
                        iterator = iterator->next;
                    }
                }
            }

            //decrement queuelength
            queueLength = queueLength - 1;

        }else{
            myPCB->state = "ready";

            //increment context switch number on its index in statList
            statList[myPCB->index].cs = statList[myPCB->index].cs + 1;
            
            //current program time
            gettimeofday(&tv, NULL);
            currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;
            
            //output
            if(outmode == 3){
                printf("timestamp: %f |PROCESS EXPIRED TIMESLICE| pid: %d , remaining length: %d , vruntime: %d\n" , currentTime , myPCB->threadID , myPCB->remainingLength , myPCB->vruntime);
            }

            //output to file
            if(isWritingOnFile){
                fprintf(out_file , "timestamp: %f |PROCESS EXPIRED TIMESLICE| pid: %d , remaining length: %d , vruntime: %d\n" , currentTime , myPCB->threadID , myPCB->remainingLength , myPCB->vruntime);
            }
        }

        //signaling producer and scheduler threads
        nextBurstIndex = -1;
        pthread_cond_signal(&cvProducer);
        pthread_cond_signal(&cvScheduler);
        pthread_mutex_unlock(&lock);
    }
    
    pthread_exit(NULL);
}

//generator thread function for input type F
static void *producerF(void *arg_ptr){
    int i;
    int res;
    for(i = 0; i < allp ; ++i){

        //gets the lock and sleeps on condition variable if rueue is full
        pthread_mutex_lock(&lock);
        while(queueLength >= rqlen){
            pthread_cond_wait (&cvProducer, &lock);
        }

        //thread arguments
        t_args[i].index = i + 1;
        pthread_attr_init(&t_args[i].t_attr);
        t_args[i].t_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;

        //create PCB
        struct PCB *tpcb = malloc(sizeof(struct PCB));
        tpcb->threadID = t_args[i].tid;
        tpcb->index = t_args[i].index;
        tpcb->state = "ready";
        tpcb->next = NULL;
        tpcb->vruntime = 0;
        tpcb->priority = content[i].priorityValue;
        tpcb->processLength = content[i].processLength;
        tpcb->remainingLength = tpcb->processLength;

        //add PCB to the queue
        if(head == NULL){
            head = (struct PCB*)malloc(sizeof(struct PCB));
            head = tpcb;
        }else{
            tpcb->next = head;
            head = tpcb;
        }

        //increments queue length after insertion
        queueLength = queueLength + 1;

        //thread creation
        res = pthread_create(&t_args[i].tid , &t_args[i].t_attr, working , (void *)&(t_args[i]));

        if(res != 0){
            exit(1);
        }

        //recording some stats for the final output
        gettimeofday(&tv, NULL);
        currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;
        statList[tpcb->index].birthTime = currentTime;
        statList[tpcb->index].tid = tpcb->threadID;
        statList[tpcb->index].totalLength = tpcb->processLength;
        statList[tpcb->index].priorityWeight = tpcb->priority;
        statList[tpcb->index].cs = 0;

        //outmode 3 print statement
        if(outmode == 3){
            printf("timestamp: %f |NEW PROCESS| pid: %d , process length: %d , priority: %d\n", currentTime , tpcb->threadID , tpcb->processLength , tpcb->priority);
        }

        //writing output on a file
        if(isWritingOnFile){
            fprintf(out_file , "timestamp: %f |NEW PROCESS| pid: %d , process length: %d , priority: %d\n", currentTime , tpcb->threadID , tpcb->processLength , tpcb->priority);
        }

        //signaling scheduler thread
        pthread_cond_signal(&cvScheduler);
        pthread_mutex_unlock(&lock);

        usleep(1000 * content[i].iat);
    }

    pthread_exit(NULL);
}

//generator thread function for input type C
static void *producerC(void *arg_ptr){
    int i;
    int res;
    for(i = 0; i < allp ; ++i){

        //gets the lock and sleeps on condition variable cvProducer when queue is full
        pthread_mutex_lock(&lock);
        while(queueLength >= rqlen){
            pthread_cond_wait (&cvProducer, &lock);
        }

        //thread arguments
        t_args[i].index = i + 1;
        pthread_attr_init(&t_args[i].t_attr);
        t_args[i].t_cond = (pthread_cond_t) PTHREAD_COND_INITIALIZER;

        //generate uniform dist random priority value
        int prio = (int) (rand() % (maxPrio - minPrio + 1)) + minPrio;

        //create PCB
        struct PCB *tpcb = malloc(sizeof(struct PCB));
        tpcb->threadID = t_args[i].tid;
        tpcb->index = t_args[i].index;
        tpcb->state = "ready";
        tpcb->next = NULL;
        tpcb->vruntime = 0;
        tpcb->priority = prio_to_weight[prio];

        //generating process length according to input distPL
        if(!strcmp(distPL , "fixed")){
            tpcb->processLength = avgPL;
        }else if(!strcmp(distPL , "exponential")){
            double u_min = exp((-1) * minPL * (1/avgPL));
            double u_max = exp((-1) * maxPL * (1/avgPL));
            double u = u_min + (1.0 - rand() / (RAND_MAX + 1.0)) * (u_max - u_min);
            tpcb->processLength = (int) -log(u) / (1/avgPL);
        }else if(!strcmp(distPL , "uniform")){
            tpcb->processLength = (int) (rand() % (maxPL - minPL + 1)) + minPL;
        }

        tpcb->remainingLength = tpcb->processLength;

        //add PCB to the queue
        if(head == NULL){
            head = (struct PCB*)malloc(sizeof(struct PCB));
            head = tpcb;
        }else{
            tpcb->next = head;
            head = tpcb;
        }

        //increment queue length after insertion
        queueLength = queueLength + 1;

        //create thread
        res = pthread_create(&t_args[i].tid , &t_args[i].t_attr, working , (void *)&(t_args[i]));

        if(res != 0){
            exit(1);
        }

        //recording stats for the final output of the program
        gettimeofday(&tv, NULL);
        currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;
        statList[tpcb->index].birthTime = currentTime;
        statList[tpcb->index].tid = tpcb->threadID;
        statList[tpcb->index].totalLength = tpcb->processLength;
        statList[tpcb->index].priorityWeight = tpcb->priority;
        statList[tpcb->index].cs = 0;

        //output mode 3
        if(outmode == 3){            
            printf("timestamp: %f |NEW PROCESS| pid: %d , process length: %d , priority: %d\n", currentTime , tpcb->threadID , tpcb->processLength , tpcb->priority);
        }

        //writing output on the output file
        if(isWritingOnFile){
            fprintf(out_file , "timestamp: %f |NEW PROCESS| pid: %d , process length: %d , priority: %d\n", currentTime , tpcb->threadID , tpcb->processLength , tpcb->priority);
        }

        int sleepTime = 0;

        //generating sleep time according to the input distIAT
        if(!strcmp(distIAT , "fixed")){
            sleepTime = avgIAT;
        }else if(!strcmp(distIAT , "exponential")){
            double u_min = exp((-1) * minIAT * (1/avgIAT));
            double u_max = exp((-1) * maxIAT * (1/avgIAT));
            double u = u_min + (1.0 - rand() / (RAND_MAX + 1.0)) * (u_max - u_min);
            sleepTime = (int) -log(u) / (1/avgIAT);
        }else if(!strcmp(distIAT , "uniform")){
            sleepTime = (int) (rand() % (maxIAT - minIAT + 1)) + minIAT;
        }

        //signals scheduler thread
        pthread_cond_signal(&cvScheduler);
        pthread_mutex_unlock(&lock);

        usleep(1000 * sleepTime);
    }

    pthread_exit(NULL);
}

//scheduler thread function
static void *scheduler(void *arg_ptr){
    while(head != NULL){
        pthread_mutex_lock(&lock);
        while(nextBurstIndex != -1){
            pthread_cond_wait (&cvScheduler, &lock);
        }

        //temp PCBs to store information
        struct PCB *nextToRun = head;
        struct PCB *iterator = head;
        int totalWeight = iterator->priority;

        //choosing the process that will run next
        while(iterator->next != NULL){
            if(nextToRun->vruntime > iterator->next->vruntime){
                nextToRun = iterator->next;
            }else if(nextToRun->vruntime == iterator->next->vruntime && nextToRun->priority > iterator->next->priority){
                nextToRun = iterator->next;
            }
            iterator = iterator->next;
            totalWeight = totalWeight + iterator->priority;
        }

        //calculates the timeslice
        int timeSlice = (nextToRun->priority / totalWeight) * SCHED_LATENCY;

        //uses nextBurstLength and nextBurstIndex to pass information to the threads 
        if(timeSlice < MIN_GRANULARITY){
            nextBurstLength = MIN_GRANULARITY;
        }else{
            nextBurstLength = timeSlice;
        }

        nextBurstIndex = nextToRun->index;

        //calculate current program time
        gettimeofday(&tv, NULL);
        currentTime = ((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000) - startTime;

        //writing output on console
        if(outmode == 3){
            printf("timestamp: %f |PROCESS SCHEDULED| pid: %d , remaining length of chosen process: %d , timeslice: %d\n" , currentTime , nextToRun->threadID , nextToRun->remainingLength , nextBurstLength);
        }

        //writing output to a file
        if(isWritingOnFile){
            fprintf(out_file , "timestamp: %f |PROCESS SCHEDULED| pid: %d , remaining length of chosen process: %d , timeslice: %d\n" , currentTime , nextToRun->threadID , nextToRun->remainingLength , nextBurstLength);
        }

        //signaling the targeted working thread
        pthread_cond_signal (&(t_args[nextBurstIndex - 1].t_cond));
        pthread_mutex_unlock(&lock);
    }

    pthread_exit(NULL);
}

//main
int main(int argc, char *argv[]){
    //time in the start
    gettimeofday(&tv, NULL);
    startTime = (tv.tv_sec) * 1000 + (tv.tv_usec) / 1000;

    queueLength = 0;

    isWritingOnFile = 0;

    //taking command line arguments
    if(! strcmp(argv[1] , "F")){
        inputType = 0; //F
        rqlen = atoi(argv[2]);
        
        if(rqlen > 100){
            return -1;
        }

        allp = atoi(argv[3]);

        if(allp < 1 || allp > 2000){
            return -1;
        }

        outmode = atoi(argv[4]);
        inputFile = argv[5];

        if(outmode != 1 || outmode != 2 || outmode != 3){
            printf("Wrong parameters for outmode!!");
            return -1;    
        }

        //reading from the file
        char word[50];

        in_file = fopen(inputFile , "r");

        int nextIn = 0;
        int inputIndex = 0;

        if(in_file == NULL){
            return -1;
        }else{
            while(fscanf(in_file, "%s", word) != EOF){
                if(nextIn == 1){
                    content[inputIndex].processLength = atoi(word);
                    nextIn = 2;
                }else if(nextIn == 2){
                    content[inputIndex].priorityValue = atoi(word);
                    nextIn = 0;
                }else if(nextIn == 3){
                    content[inputIndex].iat = atoi(word);
                    nextIn = 0;
                    inputIndex = inputIndex + 1;
                }
                
                if(!strcmp(word , "PL")){
                    nextIn = 1;
                }else if(!strcmp(word , "IAT")){
                    nextIn = 3;
                }
            }
            fclose(in_file);
        }

        //connection with output file
        if(argc == 7){
            isWritingOnFile = 1;
            outputFile = argv[6];
            out_file = fopen(outputFile , "w");

            if(out_file == NULL){
                printf("Output file can't be opened");
                return -1;
            }
        }

    }else if(! strcmp(argv[1] , "C")){
        inputType = 1; //C
        minPrio = atoi(argv[2]);
        maxPrio = atoi(argv[3]);

        if(minPrio > maxPrio || minPrio < -20 || maxPrio > 19){
            printf("Wrong parameters for min/max prio!!");
            return -1;    
        }

        distPL = argv[4];

        if(!strcmp(distPL , "fixed") || !strcmp(distPL , "uniform") || !strcmp(distPL , "exponential")){
            printf("Wrong parameters for distPL!!");
            return -1;
        }

        avgPL = atoi(argv[5]);
        minPL = atoi(argv[6]);
        maxPL = atoi(argv[7]);

        if(minPL < 10 || maxPL > 100000){
            return -1;
        }

        if(minPL > maxPL){
            printf("Wrong parameters for minPL and maxPL!!");
            return -1;
        }

        distIAT = argv[8];

        if(!strcmp(distIAT , "fixed") || !strcmp(distIAT , "uniform") || !strcmp(distIAT , "exponential")){
            printf("Wrong parameters for distIAT!!");
            return -1;
        }

        avgIAT = atoi(argv[9]);
        minIAT = atoi(argv[10]);
        maxIAT = atoi(argv[11]);

        if(minIAT > maxIAT){
            printf("Wrong parameters for minIAT and maxIAT!!");
            return -1;
        }

        rqlen = atoi(argv[12]);

        if(rqlen > 100){
            return -1;
        }

        allp = atoi(argv[13]);

        if(allp < 1 || allp > 2000){
            return -1;
        }

        outmode = atoi(argv[14]);

        if(outmode != 1 || outmode != 2 || outmode != 3){
            printf("Wrong parameters for outmode!!");
            return -1;    
        }

        //connection to output file
        if(argc == 16){
            isWritingOnFile = 1;
            outputFile = argv[15];
            out_file = fopen(outputFile , "w");

            if(out_file == NULL){
                printf("Output file can't be opened");
                return -1;
            }
        }        

    }else{
        printf("Wrong parameters for input type!!");
        return -1;
    }

    //initialize srand
    srand(time(NULL));

    //creating produceer and scheduler threads
    pthread_t producerID; 
    pthread_t schedulerID;

    int a;
    if(inputType == 1){
        a = pthread_create(& producerID, NULL, producerC , NULL);
    }else if(inputType == 0){
        a = pthread_create(& producerID, NULL, producerF , NULL);
    }

    int b = pthread_create(& schedulerID, NULL, scheduler , NULL);

    if(a != 0 || b != 0){
        printf("Problem occured while creating producer and scheduler threads");
    }

    a = pthread_join(producerID, NULL);
    b = pthread_join(schedulerID, NULL);

    if(a != 0 || b != 0){
        printf("Problem occured while joining producer and scheduler threads");
    }

    //final output of stats
    printf("PROGRAM EXECUTED HERE IS THE ENDING STATS FOR PROCESSES\n");
    printf("pid arv dept prio cpu waitr turna cs");

    int j;
    for(j = 0; j < allp; ++j){
        printf("%d %f %f %d %d %f %f %d\n" , statList[j].tid , statList[j].birthTime , 
         statList[j].finishTime , statList[j].priorityWeight , statList[j].totalLength ,
         ((statList[j].finishTime - statList[j].birthTime) - statList[j].totalLength) ,
         (statList[j].finishTime - statList[j].birthTime) , statList[j].cs);
    }

    //final output of stats to the output
    if(isWritingOnFile){
        fprintf(out_file , "\n");
        
        fprintf(out_file , "PROGRAM EXECUTED HERE IS THE ENDING STATS FOR PROCESSES\n");
        fprintf(out_file , "pid arv dept prio cpu waitr turna cs");

        for(j = 0; j < allp; ++j){
        fprintf(out_file , "%d %f %f %d %d %f %f %d\n" , statList[j].tid , statList[j].birthTime , 
         statList[j].finishTime , statList[j].priorityWeight , statList[j].totalLength ,
         ((statList[j].finishTime - statList[j].birthTime) - statList[j].totalLength) ,
         (statList[j].finishTime - statList[j].birthTime) , statList[j].cs);
        }
    }

    fclose(out_file);

    return 0;
}