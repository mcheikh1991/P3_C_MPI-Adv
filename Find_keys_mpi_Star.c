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
char **word, **line, **all_key_array;
char *key_array;
long key_array_size, key_array_count, key_array_limit, new_key_array_size;
int filenumber;
char newword[15], hostname[256],filename[256], sent_word[20], word_to_send[20];
int *Free, *word_number , w_number; 

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
 	word_to_send[0] = '\0';

 	// This is an array that tells rank 0 which processor is free
	Free = (int *) malloc( nthreads * sizeof( int ) ); // 0 free, 1 busy
	word_number = (int *) malloc( nthreads * sizeof( int ) ); 
	for (i = 0; i < nthreads; i++)
	{
		Free[i] = 0; //Initial values (all are free)
		word_number[i] = 0;
	}
	
	if (myID == 0)
	{
		all_key_array = (char **) malloc( (nthreads-1) * sizeof( char * ) );
		for( i = 0; i < nthreads-1; i++ ) {
			all_key_array[i] = malloc( key_array_size );
			all_key_array[i][0] = '\0'; 
		}
	}
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
	count = 0;
	for( k = startPos; k < endPos; k++ ) 
		{
			if (count < 100)
			{
				if( strstr( line[k], sent_word ) != NULL ) 
				{
					if (count == 0)
					{
						//snprintf(newword,10,"%s:",sent_word);
						snprintf(newword,10,"X%d:",w_number);
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
	fclose( fd );
}


void *remove_elements(char *array, int array_length, int index)
{
   int i;
   for(i = 0; i < array_length - index; i++) 
   {
   	array[i] = array[i+index];
   }

   for(i= array_length - index; i<array_length; i++)
	{
		array[i] = '\0';
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
	w_number = 0;
	int receiver = 0;

	
	while (DONE < nthreads -1)
	{
		// Sending keywords from rank 0 to all the other:
		//-----------------------------------------------
		if (rank == 0)
		{
			for (receiver = 1; receiver < nthreads; receiver++)
			{
				if (word_number[receiver] < maxwords)
				{
					DONE = 0;
					if (Free[receiver] == 0)
					{
			 			strcat(word_to_send, word[word_number[receiver]]);
			 			word_number[receiver] += 1;
			 			Free[receiver] = 1; // Means that the reciever is now busy
			 			MPI_Isend(&Free[receiver], 1, MPI_INT, receiver, 1234, MPI_COMM_WORLD, &Request); 
						MPI_Isend(word_to_send,   20, MPI_CHAR, receiver, 5678, MPI_COMM_WORLD, &Request); 
						memset(&word_to_send[0], 0, sizeof(word_to_send));
					}
					else
					{	// Check if other ranks sent their output:
						//----------------------------------------
						MPI_Iprobe(receiver, 4321, MPI_COMM_WORLD, &flag, &Status );
						if (flag)
						{
							MPI_Irecv(&Free[receiver], 1, MPI_INT, receiver, 4321, MPI_COMM_WORLD, &Request);
							while(!flag2)
	        				{				
	        					MPI_Iprobe(receiver, 7777, MPI_COMM_WORLD, &flag2, &Status );
	        				}	
	        				MPI_Irecv(key_array, key_array_size, MPI_CHAR, receiver, 7777, MPI_COMM_WORLD, &Request);	
	        				
	        				if (strlen(key_array) > 0)
	        				{
	        					strcat(key_array,"\n");
	        					strcat(all_key_array[receiver-1],key_array);
	        				}
	        				memset(&key_array[0], 0, sizeof(key_array)*strlen(key_array) );
							flag2 = 0;
						}
						flag = 0;
					}
				}
				else
				{
					DONE+=1;
				}
			}
		}	

		// Receiving  keywords from rank 0:
		//-----------------------------------------------
		else
		{
			MPI_Iprobe(0, 1234, MPI_COMM_WORLD, &flag, &Status );
			if (flag)
			{
				MPI_Irecv(&Free[rank], 1, MPI_INT, 0, 1234, MPI_COMM_WORLD, &Request);
				if (Free[rank] == 1)
				{
					while(!flag2)
        			{
            		MPI_Iprobe( 0, 5678, MPI_COMM_WORLD, &flag2, &Status );
        			}
					MPI_Irecv(&sent_word, 20, MPI_CHAR, 0, 5678, MPI_COMM_WORLD, &Request);
					count_words(&rank); // Counting 
					flag2 = 0;
					flag = 0;
					Free[rank] = 0; // Means that the reciever is now free
					MPI_Isend(&Free[rank], 1, MPI_INT,0,4321,MPI_COMM_WORLD, &Request);  // Tell rank 0 you r free
					MPI_Isend(key_array, strlen(key_array), MPI_CHAR, 0, 7777,MPI_COMM_WORLD, &Request); 
        			memset(&key_array[0], 0, sizeof(key_array)*strlen(key_array) );
        			w_number+=1;
				}
			}
			MPI_Iprobe(0, 9999, MPI_COMM_WORLD, &flag, &Status );
			if (flag)
			{
				MPI_Irecv(&DONE, 1, MPI_INT, 0, 9999, MPI_COMM_WORLD, &Request);	
			}
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


	// Sorting the results and Printing the output:
	//-----------------------------------------------
	
	if(rank==0)
	{
		char integer[32];
		char output_word[32];
		int results;
		int number_of_times = 0;
		memset(&key_array[0], 0, sizeof(key_array)*strlen(key_array) );

		fd = fopen(filename, "a" );
		char *look_for_word;

		for (i = 0; i < maxwords ; i++)
		{
			sprintf(integer, "X%d:",i);
			strcat(output_word, word[i]);
			strcat(output_word, ":");
			for (receiver = 1; receiver < nthreads; receiver++)
			{
			
				look_for_word = strstr(all_key_array[receiver-1], integer);      // search for string 
				if (look_for_word != NULL)                     // if successful 
				{

					sscanf(look_for_word, "%[^\n]", key_array);

					if(number_of_times == 0) results = fputs(output_word, fd);
					remove_elements(key_array,strlen(key_array),strlen(integer));
					results = fputs(key_array,fd);
					number_of_times += 1;
				}                                
				look_for_word = NULL;
				memset(&key_array[0], 0, sizeof(key_array)*strlen(key_array) );
			}
			if (number_of_times > 0)
			{
				results = fputs("\n",fd);
			}
			number_of_times = 0;
	        memset(&output_word[0], 0, sizeof(char)*strlen(output_word) );
	        memset(&integer[0], 0, sizeof(char)*strlen(integer) );
		}
		fclose( fd );
	}
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

