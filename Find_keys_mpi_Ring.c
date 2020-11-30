#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <mpi.h>
#include <sys/resource.h>

# define version 1

int maxwords = 50000;
int maxlines;
int nwords;
int nlines;
int err, count, nthreads = 1;
double tstart, ttotal;
FILE *fd;
char **word, **line;
char *key_array;
long key_array_size, key_array_count, key_array_limit, new_key_array_size;
int filenumber;
char newword[15], hostname[256],filename[256], sent_word[20];
int  word_number;

/* myclock: (Calculates the time)
--------------------------------------------*/

double myclock() {
	static time_t t_start = 0;  // Save and subtract off each time

	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if( t_start == 0 ) t_start = ts.tv_sec;

	return (double) (ts.tv_sec - t_start) + ts.tv_nsec * 1.0e-9;
}

/* init_list: (Initaite word list and lines)
--------------------------------------------*/
void init_list(void *rank)
{
	// Malloc space for the word list and lines
	int i;
	int myID =  *((int*) rank);
	count = 0;
	word = (char **) malloc( maxwords * sizeof( char * ) );
	for( i = 0; i < maxwords; i++ ) {
		word[i] = malloc( 10 );
	}

	line = (char **) malloc( maxlines * sizeof( char * ) );
	for( i = 0; i < maxlines; i++ ) {
		line[i] = malloc( 2001 );
	}

    key_array_count = 0;		   // counter
    key_array_limit =  99000000; // limit to increase array size: 99 million
    key_array_size  = 100000000; // size of key_array: 100 million ~ 95 MB
    new_key_array_size = key_array_size; // new array size (initail value is key_array_size)
 	key_array = malloc(sizeof(char)*key_array_size); 
 	key_array[0] = '\0';
 	sent_word[0] = '\0';
}


/* read_dict_words: (Read Dictianary words)
--------------------------------------------*/
void read_dict_words()
{
// Read in the dictionary words
	fd = fopen("625/keywords.txt", "r" );
	nwords = -1;
	do {
		err = fscanf( fd, "%[^\n]\n", word[++nwords] );
	} while( err != EOF && nwords < maxwords );
	fclose( fd );

	//printf( "Read in %d words\n", nwords);
}


/* read_lines: (Read wiki lines)
--------------------------------------------*/
void read_lines()
{
// Read in the lines from the data file
	double nchars = 0;
	fd = fopen( "625/wiki_dump.txt", "r" );
	nlines = -1;
	do {
		err = fscanf( fd, "%[^\n]\n", line[++nlines] );
		if( line[nlines] != NULL ) nchars += (double) strlen( line[nlines] );
	} while( err != EOF && nlines < maxlines);
	fclose( fd );

	//printf( "Read in %d lines averaging %.0lf chars/line\n", nlines, nchars / nlines);
}

/* reset_file: (create an output file)
--------------------------------------------*/
void *reset_file()
{
	snprintf(filename,250,"wiki-mpi-%d.out", filenumber);
	fd = fopen(filename, "w" );
	fclose( fd );
}

/* count_words: (count the words)
--------------------------------------------*/
void *count_words(void *rank)
{
	int i,k;
	int myID =  *((int*) rank);
	int startPos = ((long) myID-1) * (nlines / (nthreads-1));
	int endPos = startPos + (nlines / (nthreads-1));
	key_array_count = 0;
	for( k = startPos; k < endPos; k++ ) 
		{
			if (count < 100)
			{
				if( strstr( line[k], sent_word ) != NULL ) 
				{
					if (count == 0)
					{
						snprintf(newword,10,"%s:",sent_word);
						strcat(key_array,newword);
						key_array_count += strlen(newword);
					}

					count++; 
					snprintf(newword,10,"%d,",k);
					strcat(key_array,newword);
					key_array_count += strlen(newword);
				}
			}

		}

  		// Checks if the size of the key_array has reached the limit
		if (key_array_count > key_array_limit)
		{
			new_key_array_size += key_array_size; // incrase size
			char* myrealloced_array = realloc(key_array, new_key_array_size * sizeof(char));
			if (myrealloced_array) key_array = myrealloced_array;
			key_array_count = 0; // reset counter
		}
}


/* dump_words: (write the output on file)
--------------------------------------------*/
void *dump_words()
{
	fd = fopen(filename, "a" );
	int results =fputs(key_array,fd);
	results =fputs("\n",fd);
	fclose( fd );
}


void *remove_elements(char *array, int array_length, int index)
{
   int i;
   for(i = 0; i < array_length - index; i++) 
   {
   	array[i] = array[i+index];
   }
}

/*--------------------------------------------------
						Main 
--------------------------------------------------*/

int main(int argc, char* argv[]) 
{

	int i, rc;
	int numtasks, rank;
	double tstart_init, tend_int, tstart_count, tend_count, tend_reduce;
  	struct rusage ru;

	MPI_Status Status;
    MPI_Request Request;
	maxlines = 100000; // Default Value
	filenumber = rand() %100000;
	if (argc >= 2){
		maxlines = atol(argv[1]);
		filenumber = atol(argv[2]);
	}

	rc = MPI_Init(&argc,&argv);
	if (rc != MPI_SUCCESS){
		printf ("Error starting MPI program. Terminating.\n");
		MPI_Abort(MPI_COMM_WORLD, rc);
	}

    MPI_Comm_size(MPI_COMM_WORLD,&numtasks); // Number of cores
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);	 // rank of each core
	nthreads = numtasks;

	gethostname(hostname,255);
	if(rank == nthreads-1) reset_file(); // The last core will reset the output file 
	MPI_Bcast(filename, 256, MPI_CHAR, nthreads-1, MPI_COMM_WORLD); // send the name of the output file to all cores
	tstart = myclock(); // Global Clock

	// Initialization 
	init_list(&rank);
	if (rank ==0) read_dict_words();
	else           read_lines();
	int flag = 0;
	int flag2 = 0;
	int DONE = 0;
	MPI_Barrier(MPI_COMM_WORLD);

	word_number = 0;
	int words_written =0;

	int receiver,sender;

	if(rank==nthreads-1) receiver = 0; // reciever is the rank that will recieve the message
	else 		receiver = rank + 1;

	if(rank==0) sender = nthreads-1; // sender is the rank that will send the message
	else 		sender = rank - 1;

	int Waiting = 0;
	// Task to be done by rank 0 before starting the loop:
	if (rank==0)
	{
		strcat(sent_word, word[word_number]);
		word_number +=1;
		count = 0;
		//Free[receiver]=1;
		//MPI_Isend(&Free[receiver], 1, MPI_INT, receiver, 1234, MPI_COMM_WORLD, &Request);
		MPI_Isend(&count, 1, MPI_INT, receiver, 2233, MPI_COMM_WORLD, &Request);
		MPI_Isend(sent_word,   20, MPI_CHAR, receiver, 5678, MPI_COMM_WORLD, &Request); 
		MPI_Isend(key_array, strlen(key_array), MPI_CHAR, receiver, 7777, MPI_COMM_WORLD, &Request);	
	}

	// Entering the while loop:
	while(DONE != 1)
	{
		// Check fo int count 
		MPI_Iprobe(sender, 2233, MPI_COMM_WORLD, &flag2, &Status );
		if(flag2)
		{
			MPI_Irecv(&count, 1, MPI_INT, sender, 2233, MPI_COMM_WORLD, &Request);
			flag2 = 0;
			// Recieve char sent_word 
			while(!flag2)
			{
			MPI_Iprobe( sender, 5678, MPI_COMM_WORLD, &flag2, &Status );
			}
			MPI_Irecv(&sent_word, 20, MPI_CHAR, sender, 5678, MPI_COMM_WORLD, &Request);
			flag2 = 0;

			// Recieve char key_array
			while(!flag2)
			{
			MPI_Iprobe( sender, 7777, MPI_COMM_WORLD, &flag2, &Status );
			}
			MPI_Irecv(key_array, key_array_size, MPI_CHAR, sender, 7777, MPI_COMM_WORLD, &Request);	
			flag2 = 0;

			// Tasks to do
			if(rank == 0)
			{
				if(strlen(key_array)>0) dump_words();
				// Reset
				Waiting = 0;
				words_written +=1;
				if(words_written==maxwords) DONE = 1;
			}
			else
			{	
				count_words(&rank); // CHECK THIS WORK
			}	

			// Send again	
			if (rank != 0)
			{
				MPI_Isend(&count, 1, MPI_INT, receiver, 2233, MPI_COMM_WORLD, &Request);
				MPI_Isend(sent_word,   20, MPI_CHAR, receiver, 5678, MPI_COMM_WORLD, &Request); 
				MPI_Isend(key_array, strlen(key_array), MPI_CHAR, receiver, 7777, MPI_COMM_WORLD, &Request);
				memset(&key_array[0], 0, sizeof(key_array[0])*strlen(key_array));	
				key_array[0] = '\0';	
			}

		}
		MPI_Iprobe(0, 9999, MPI_COMM_WORLD, &flag, &Status );
		if (flag)
		{
			MPI_Irecv(&DONE, 1, MPI_INT, 0, 9999, MPI_COMM_WORLD, &Request);	
		}

		if (rank ==0 && Waiting <1 && word_number<maxwords)
		{
			// Reset
			memset(&key_array[0], 0, sizeof(key_array[0])*strlen(key_array));
			memset(&sent_word[0], 0, sizeof(sent_word[0])*strlen(sent_word));	
			count = 0;
			// New word

			strcat(sent_word, word[word_number]);
			MPI_Isend(&count, 1, MPI_INT, receiver, 2233, MPI_COMM_WORLD, &Request);
			MPI_Isend(sent_word,   20, MPI_CHAR, receiver, 5678, MPI_COMM_WORLD, &Request); 
			MPI_Isend(key_array, strlen(key_array), MPI_CHAR, receiver, 7777, MPI_COMM_WORLD, &Request);
			memset(&key_array[0], 0, sizeof(key_array[0])*strlen(key_array));	
			key_array[0] = '\0';

			word_number +=1;
			Waiting +=1;
		}
	}

	// Exiting the counting Processs:
	//-----------------------------------------------
	if(rank==0)
	{
		for (receiver = 1; receiver < nthreads; receiver++)
		{
			MPI_Isend(&DONE, 1, MPI_INT, receiver,9999, MPI_COMM_WORLD, &Request); 
		}
	}
	printf("Done rank %d\n",rank);

	// Print results:
	//-----------------------------------------------

	getrusage(RUSAGE_SELF, &ru);
    long MEMORY_USAGE = ru.ru_maxrss;   // Memory usage in Kb
	printf(" rank %d, hostaname %s, size %d, memory %ld Kb\n",
	            rank, hostname, nthreads,  MEMORY_USAGE); 
	fflush(stdout);

	if ( rank == 0 ) {
	ttotal = myclock() - tstart;
	printf("version %d, cores %d, total time %lf seconds, words %d, lines %d\n",
	 		version, nthreads, ttotal, nwords, maxlines);	
	}

	MPI_Finalize();
	return 0;
}

