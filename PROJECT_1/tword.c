#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#define MAXTHREADS  8

struct Map{
    char *word;
    int count;
};

struct MapArr{
    struct Map **map;
    int length;
};

struct arg {
	int t_index;
};

struct MapArr *finalMapArr;
char** nameOfFiles = NULL;
int numberOfFiles;
char* outFile;

static void *do_task(void *arg_ptr){
    char *retreason;

    FILE *textfile;
    char txtword[64];
    textfile = fopen(nameOfFiles[((struct arg *) arg_ptr)->t_index] , "r");
    
    if(textfile == NULL){
        printf("Can't read file");
        exit(0);
    }

    while(fscanf(textfile , "%s" , txtword) != EOF){
        if(finalMapArr == NULL){
            finalMapArr->map[0]->word = txtword;
            finalMapArr->map[0]->count = 1;
            finalMapArr->length = 1;
        }else{
            int j;
            int booleanVal = 0;
            for(j = 0; j < finalMapArr->length; j++){
                if(!strcmp(txtword , finalMapArr->map[j]->word)){
                    finalMapArr->map[j]->count++;
                    booleanVal = 1;
                }
            }

            if(booleanVal == 0){
                finalMapArr->map[finalMapArr->length]->word = txtword;
                finalMapArr->map[finalMapArr->length]->count = 1;
                finalMapArr->length++;
            }
        }
    }

    retreason = (char *) malloc (200);
	strcpy (retreason, "normal termination of thread"); 
	pthread_exit(retreason);
}

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
    numberOfFiles = (int) atoi(argv[3]);
    nameOfFiles = calloc(numberOfFiles , sizeof(char*));
    //int msgSize = (int) atoi(argv[1]);
    outFile = argv[2];

    //saving the filenames
    int i = 0;
    for(i = 0; i < numberOfFiles; i++){
        nameOfFiles[i] = argv[4+i];
    }

    //creating threads
    pthread_t tids[MAXTHREADS];
    struct arg t_args[MAXTHREADS];

    int ret;
	char *retmsg; 

    int z;
    for(z = 0; z < numberOfFiles; z++){
        t_args[z].t_index = z;

        ret = pthread_create(&(tids[z]) , NULL , do_task, (void *) &(t_args[z]));
    
        if (ret != 0) {
			printf("thread create failed \n");
			exit(1);
		}
    }

    for(z = 0; z < numberOfFiles; z++){
        ret = pthread_join(tids[z], (void **)&retmsg);
        if (ret != 0) {
			printf("thread join failed \n");
			exit(1);
		}
        free (retmsg);
    }

    return 0;
}