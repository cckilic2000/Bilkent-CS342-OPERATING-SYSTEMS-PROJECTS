#include <stdlib.h>
#include <mqueue.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#define MQNAME "/mqname"

//data structure for message format

struct Map{
    char *word;
    int count;
};

struct MapArr{
    struct Map **map;
    int length;
};

char** nameOfFiles = NULL;
char *bufferpReceive;
int bufferlenReceive;
struct MapArr *receiveMapArr;
struct MapArr *finalMapArr;

int main(int argc, char *argv[]){

    if(argc > 12 || atoi(argv[3]) > 8){
        printf("Maximum 11 arguements are accepted and n can be at most 8!");
        return -1;
    }else if((int) atoi(argv[1]) != 128 &&
            (int) atoi(argv[1]) != 256 &&
            (int) atoi(argv[1]) != 512 &&
            (int) atoi(argv[1]) != 1024 &&
            (int) atoi(argv[1]) != 2048 &&
            (int) atoi(argv[1]) != 4096 ){
        printf("Invalid msg size input!");
        return -1;
    }

    //parameters
    int numberOfFiles = (int) atoi(argv[3]);
    nameOfFiles = calloc(numberOfFiles , sizeof(char*));
    //int msgSize = (int) atoi(argv[1]);
    char* outFile = argv[2];

    //saving the filenames
    int i = 0;
    for(i = 0; i < numberOfFiles; i++){
        nameOfFiles[i] = argv[4+i];
    }
    
    //create message queue
    mqd_t mq;
    struct mq_attr mq_attr;
    int a;

    mq = mq_open(MQNAME, O_RDWR | O_CREAT, 0666, NULL);
    if (mq == -1) {
        perror("can not create msg queue\n");
        exit(1);
    }
    
    mq_getattr(mq, &mq_attr);
    printf("mq maximum msgsize = %d\n", (int) mq_attr.mq_msgsize); //////msgsize
    
    bufferlenReceive = mq_attr.mq_msgsize; ///////////////////msgsize
    bufferpReceive = (char *) malloc(bufferlenReceive);



    //n child processes
    pid_t apid[numberOfFiles];

    for(i = 0; i < numberOfFiles; i++){
        apid[i] = fork();
        if(apid[i] < 0){
            printf ("fork() failed\n");
            exit (1);
        }

        if(apid[i] == 0){
            //child program
            FILE *textfile;
            char txtword[64];
            struct MapArr *sendMapArr;

            char *bufferpSend;
            int   bufferlenSend; /////////////////////msgsize

            textfile = fopen(nameOfFiles[i] , "r");

            if(textfile == NULL){
                printf("Can't read file: %d" , i);
                exit(0);
            }

            while(fscanf(textfile , "%s" , txtword) != EOF){
                if(sendMapArr == NULL){
                    sendMapArr->map[0]->word = txtword;
                    sendMapArr->map[0]->count = 1;
                    sendMapArr->length = 1;
                }else{
                    int y;
                    int booleanVal = 0;
                    for(y = 0; y < sendMapArr->length; y++){
                        if(!strcmp(txtword , sendMapArr->map[y]->word)){
                            sendMapArr->map[y]->count++;
                            booleanVal = 1;
                        }
                    }

                    if(booleanVal == 0){
                        sendMapArr->map[sendMapArr->length]->word = txtword;
                        sendMapArr->map[sendMapArr->length]->count = 1;
                        sendMapArr->length++;
                    }
                }
            }

            //send message
            mqd_t mq1;
            struct mq_attr mq_attr1;

            mq1 = mq_open(MQNAME, O_RDWR);
            if(mq1 == -1){
                perror("can not open msg queue\n");
                exit(1);
            }

            mq_getattr(mq1, &mq_attr1);
            bufferlenSend = mq_attr1.mq_msgsize; ///////////////////msgsize
            bufferpSend = (char *) malloc(bufferlenSend);

            int h;
            while(1){
                sendMapArr = (struct MapArr *) bufferpSend;
                h = mq_send(mq1, bufferpSend, sizeof(struct MapArr), 0); ////////msgsize
                if(h == -1){
                    perror("mq_send failed\n");
                    exit(1);
                }
            }
            mq_close(mq1);

            exit(0);
        }
        waitpid(apid[i], NULL, 0);
    }

    //read message
    while (1) {
        //close?open
        a = mq_receive(mq, bufferpReceive, bufferlenReceive, NULL);
        if (a == -1) {
            perror("mq_receive failed\n");
            exit(1);
        }
        
        printf("mq_receive success, message size = %d\n", a);

        if(finalMapArr == NULL){
            finalMapArr = (struct MapArr*) bufferpReceive; 
        }else{
            receiveMapArr = (struct MapArr*) bufferpReceive;

            int k;
            for(k = 0; k < receiveMapArr->length; k++){
                int t;
                int boolVal1 = 0;
                for(t = 0; t < finalMapArr->length; t++){
                    if(!strcmp(finalMapArr->map[t]->word , receiveMapArr->map[k]->word)){
                        finalMapArr->map[t]->count = finalMapArr->map[t]->count + receiveMapArr->map[k]->count;
                        boolVal1 = 1;
                    }
                }
                if(boolVal1 == 0){
                    finalMapArr->map[finalMapArr->length] = receiveMapArr->map[k];
                    finalMapArr->length++;
                }
            }
        }
    }

    //sort ascending

    //write to output
    FILE *outputFile;
    outputFile = fopen(outFile , "w");
    if(outputFile == NULL){
        printf("Error opening file!\n");
        exit(1);
    }

    int q;
    for(q = 0; q < finalMapArr->length; q++){
        fprintf(outputFile, "%s %d\n" , finalMapArr->map[q]->word , finalMapArr->map[q]->count);
    }

    fclose(outputFile);
    free(bufferpReceive);
    mq_close(mq);
    return 0;
}