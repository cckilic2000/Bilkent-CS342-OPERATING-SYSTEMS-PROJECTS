#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

//constants
#define VIRTUAL_ADDRESS_BITS 32
#define PAGE_TABLE_1_BITS 10
#define PAGE_TABLE_2_BITS 10
#define OFFSET_BITS 12

#define PAGE_TABLE_1_SIZE (1 << PAGE_TABLE_1_BITS)
#define PAGE_TABLE_2_SIZE (1 << PAGE_TABLE_2_BITS)
#define PAGE_SIZE (1 << OFFSET_BITS)

//parameters
int isRSpecified;
int m;
char *out;
int algo;

FILE *infile1;
FILE *infile2;
FILE *outfile;

//number of virtual addresses that the program will reference
//array of those addresses
int addrcount;
unsigned long *inputs;

//number of virtual adress ranges
//array of address ranges
//even indexes show the start and odd indexes show the end
//ex: rangeArr[0] is the start of first range and rangeArr[1] is the end
//for when the r parameter is specified array has length 2
//index 0 = 0 and index 1 = end address of range
int rangeValueCount;
unsigned long *rangeArr;

//struct for a page table entry
struct PageTableEntry{
    int isMapped;
    int frame_index;
};

//struct for a frame table entry
//algo_param will be used to keep track of when the frame was created or
//when was it last used depending on the algorithm the user chooses 
struct FrameTableEntry{
    unsigned long vir_addr;
    int algo_param;
    int isOccupied;
};

//page tables and frame table
unsigned long page_table_1[PAGE_TABLE_1_SIZE];
struct PageTableEntry page_table_2[PAGE_TABLE_2_SIZE];
struct FrameTableEntry *frame_table;

//two level paging function using LRU algorithm
//outfile is used to write to the output file inside the function
//j will be used to determine the age or last used time of the frame
void two_level_paging(unsigned long addr , FILE *out_file , int j){
    //checking if the value is in range if r isn't specified
    if( ! isRSpecified){
        int i;
        int isInRange = 0;
        for(i = 0; i < rangeValueCount; ++i){
            if(addr > rangeArr[i] && addr < rangeArr[i+1]){
                isInRange = 1;
            }
            i = i + 1;
        }

        //output if the value isn't in range
        if( ! isInRange){
            fprintf(out_file , "%lx e\n" , addr);
            return;
        }
    }

    //extracting the page table indexes from the virtual address
    unsigned long page_table_1_index = (addr >> OFFSET_BITS) & (PAGE_TABLE_1_SIZE - 1);
    unsigned long page_table_2_index = (addr >> (OFFSET_BITS + PAGE_TABLE_1_BITS)) & (PAGE_TABLE_2_SIZE - 1);

    struct PageTableEntry *page_table_entry = &page_table_2[page_table_1[page_table_1_index] * PAGE_TABLE_2_SIZE + page_table_2_index];

    //checking if the page is mapped to a frame
    if(!page_table_entry->isMapped){ //page isn't mapped = page fault
        // Scan the frame table to find a free frame
        int free_frame_index = -1;
        int k;
        for (k = 0; k < m; ++k) {
            if (!frame_table[k].isOccupied) {
                free_frame_index = k;
                
                frame_table[k].isOccupied = 1;
                frame_table[k].vir_addr = addr;
                frame_table[k].algo_param = j;

                page_table_entry->frame_index = k;
                page_table_entry->isMapped = 1;

                fprintf(out_file , "%lx x\n" , addr);/////////////////////////////////////////////////////////////////
                break;
            }
        }

        //if there is no free spaces
        if(free_frame_index == -1){
            if(algo == 1){//LRU ALGORITHM
                //finding the first in from frame_table
                int least_recent_index = 0;
                int least_recent_time = frame_table[least_recent_index].algo_param;
                int z;
                for(z = 1; z < m; ++z){
                    if(frame_table[z].algo_param < least_recent_time){
                        least_recent_time = frame_table[z].algo_param;
                        least_recent_index = z; 
                    }
                }

                //putting new address in the first in index
                page_table_entry->frame_index = least_recent_index;
                frame_table[least_recent_index].algo_param = j;
                frame_table[least_recent_index].vir_addr = addr;

                fprintf(out_file , "%lx e\n" , addr);/////////////////////////////////////////////////////////////////////////
            }else if(algo == 2){//FIFO ALGORITHM
                //finding the first in from frame_table
                int first_in_index = 0;
                int first_in_time = frame_table[first_in_index].algo_param;
                int z;
                for(z = 1; z < m; ++z){
                    if(frame_table[z].algo_param < first_in_time){
                        first_in_time = frame_table[z].algo_param;
                        first_in_index = z; 
                    }
                }

                //putting new address in the first in index
                page_table_entry->frame_index = first_in_index;
                frame_table[first_in_index].algo_param = j;
                frame_table[first_in_index].vir_addr = addr;

                fprintf(out_file , "%lx e\n" , addr);/////////////////////////////////////////////////////////////////////////        
            }
        }
    }else{ //page is mapped
        //updating the algo param with the most recent access
        if(algo==1){
            frame_table[page_table_entry->frame_index].algo_param = j;
        }
        fprintf(out_file , "%lx e\n" , addr);/////////////////////////////////////////////////////////////////////////
    }
}

int main(int argc, char *argv[]){
    //taking parameters
    if(argc == 6){ // r not specified
        isRSpecified = 0;
        char *in1 = argv[1];
        char *in2 = argv[2];
        m = atoi(argv[3]);
        out = argv[4];
        algo = atoi(argv[6]);

        //counting the number of addresses in file in2
        addrcount = 0;
        char word[50];
        infile2 = fopen(in2 , "r");

        if(infile2 == NULL){
            printf("FILE 2 CANT BE OPENED!");
            return -1;
        }else{
            while(fscanf(infile2, "%s", word) != EOF){
                addrcount = addrcount + 1;
            }
        }

        //resetting the pointer to point to the begining for the second iteration
        fseek(infile2,0,SEEK_SET);

        //dynamically allocate space for the array
        inputs = (unsigned long*) malloc(addrcount * sizeof(unsigned long));

        //saving contents of in2 to inputs
        int i = 0;
        while(fscanf(infile2, "%s", word) != EOF){
            //inputs[i] = strtol(word, NULL , 0);
            inputs[i] = (unsigned long) word;
            i = i + 1;
        }

        //close file
        fclose(infile2);

        //counting the number of addresses in file in1
        rangeValueCount = 0;
        char word1[50];
        infile1 = fopen( in1, "r");

        if(infile1 == NULL){
            printf("FILE 1 CANT BE OPENED!");
            return -1;
        }else{
            while(fscanf(infile1, "%s", word1) != EOF){
                rangeValueCount = rangeValueCount + 1;
            }
        }

        //resetting the pointer to point to the begining for the second iteration
        fseek(infile1,0,SEEK_SET);

        //dynamically allocate space for the rangeArr
        rangeArr = (unsigned long*) malloc(rangeValueCount * sizeof(unsigned long));

        //saving contents of in2 to rangeArr
        i = 0;
        while(fscanf(infile1, "%s", word1) != EOF){
            //rangeArr[i] = strtol(word1, NULL , 0);
            rangeArr[i] = (unsigned long) word1;
            i = i + 1;
        }

        //close file
        fclose(infile1);

    }else{ // r specified
        isRSpecified = 1;
        m = atoi(argv[1]);
        out = argv[2];
        algo = atoi(argv[4]);
        //long int vmssize = strtol(argv[6], NULL , 0);
        unsigned long vmssize = (unsigned long) argv[6]; 
        addrcount = atoi(argv[8]);

        //dynamically allocate space for the array
        //inputs = (long int*) malloc(addrcount * sizeof(long int));
        inputs = (unsigned long*) malloc(addrcount * sizeof(unsigned long));

        //generating 32 bit addresses to put in inputs array
        int i;
        srand(time(NULL));
        for(i = 0; i < addrcount; ++i){
            inputs[i] = (unsigned long) rand();
        }

        //dynamically allocate space for the range array
        rangeValueCount = 2;
        //rangeArr = (long int*) malloc(rangeValueCount * sizeof(long int));
        rangeArr = (unsigned long*) malloc(rangeValueCount * sizeof(unsigned long));

        //put values in the rangeArr
        rangeArr[0] = 0;
        rangeArr[1] = vmssize;
    }
    //opening the output file
    outfile = fopen(out , "w");

    if(outfile == NULL){
        printf("outfile cant be opened!");
        return -1;
    }

    //dynamically allocating space for the frame_table
    frame_table = (struct FrameTableEntry*) malloc( m * sizeof(struct FrameTableEntry));

    //processing all input addresses
    int j;
    for(j = 0; j < addrcount; ++j){
        two_level_paging(inputs[j] , outfile , j);
    }

    fclose(outfile);
    free(rangeArr);
    free(inputs);
    free(frame_table);
    return 0;
}