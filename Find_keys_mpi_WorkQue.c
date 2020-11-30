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
int err, *count, nthreads = 1;
double tstart, ttotal;
FILE *fd;
char **word, **line;
char *words_group;
char *key_array, *output_array;
long key_array_size, key_array_count, key_array_limit, new_key_array_size;
int filenumber;
char newword[15], hostname[256],filename[256];
int group_size, group_number; // size of the group of word to send by rank 0
int *Free; // An array that tells rank 0 which processor is free
int *Order;// An array that tells the rank 0 the order of the group of word that was sent

/* myclock: (Calculates the time)
--------------------------------------------*/
double myclock() 
{
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
	word = (char **) malloc( maxwords * sizeof( char * ) );
	count = (int *) malloc( maxwords * sizeof( int ) );
	for( i = 0; i < maxwords; i++ ) {
		word[i] = malloc( 10 );
		count[i] = 0;
	}

	line = (char **) malloc( maxlines * sizeof( char * ) );
	for( i = 0; i < maxlines; i++ ) {
		line[i] = malloc( 2001 );
	}

	// Vairables below are for storing the output
    key_array_count = 0;		 // counter
    key_array_limit =  99000000; // limit to increase array size: 99 million
    key_array_size  = 100000000; // size of key_array: 100 million ~ 95 MB
    new_key_array_size = key_array_size; // new array size (initail value is key_array_size)
 	key_array = malloc(sizeof(char)*key_array_size); 
 	key_array[0] = '\0';

 	// The array below is for printing the output in the correct order
 	output_array = malloc(sizeof(char)*key_array_size); 
 	output_array[0] = '\0';
 	// An array to group a list of keywords
 	words_group = malloc(sizeof(char)*1200);

 	// This is an array that tells rank 0 which processor is free
	Free = (int *) malloc( nthreads * sizeof( int ) ); // 0 free, 1 busy
	for (i = 0; i < nthreads; i++)
	{
		Free[i] = 0; //Initial values (all are free)
	}

 	// This array will record the order of the group of word sent to each processor
 	Order = (int *) malloc( maxwords/group_size * sizeof( int ) );
 	for( i = 0; i < maxwords/group_size; i++ ) {
		Order[i] = 0;
	}

}


/* read_dict_words: (Read Dictianary words)
--------------------------------------------*/
void read_dict_words()
{
// Read in the dictionary words
	fd = fopen("/homes/dan/625/keywords.txt", "r" );
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
	fd = fopen( "/homes/dan/625/wiki_dump.txt", "r" );
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

	//Reset counter array:
	for( i = 0; i < nwords; i++ ) {
		count[i] = 0;
	}

	//Start counting and recording:
	for( i = 0; i < nwords; i++ ) 
	{
		for( k = 0; k < maxlines; k++ ) 
		{
			if (count[i] < 100) // Look up to 100 occurance
			{
				if( strstr( line[k], word[i] ) != NULL ) 
				{
					if (count[i] == 0)
					{
						snprintf(newword,10,"%s:",word[i]);
						strcat(key_array,newword);
						key_array_count += strlen(newword);
						//if (strstr( newword,"cico") != NULL) printf("found %d, %s\n",myID, newword);
					}
					count[i]++; 
					snprintf(newword,10,"%d,",k);
					strcat(key_array,newword);
					key_array_count += strlen(newword);
				}
			}

		}
		
		if (count[i] != 0)
		{
			snprintf(newword,5,"\n");
			strcat(key_array,newword);
			key_array_count += strlen(newword);
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
}


/* group_words: (Group words in to one array)
--------------------------------------------*/
void *group_words(int gnumber)
{
	words_group[0] = '\0';
	for (int i = gnumber*group_size; i < (gnumber+1)*group_size; ++i)
	{
		strcat(words_group,word[i]);
		strcat(words_group,"\n");
	}
}

/* split_words: (Split a group of words)
--------------------------------------------*/
void *split_words()
{
	nwords = -1;
	int word_size = 0;
	do {
		err = sscanf( words_group+word_size, "%[^\n]\n", word[++nwords]);
		word_size += strlen(word[nwords]) + 1;
	} while( err != EOF && nwords < group_size );

}

/* delet elements in an array
-----------------------------*/
void *remove_elements(char *array, int array_length, int index)
{
   int i;
   for(i = 0; i < array_length - index; i++) 
   {
   	array[i] = array[i+index];
   }
   /*
   //or
   for(i = 0; i < array_length ; i++) 
   {
   	if (i <array_length- index)  	array[i] = array[i+index];
   	else  	array[i] = '?';
   }*/
}

/* dump_words: (write the output on file)
--------------------------------------------*/
void *dump_words()
{
	fd = fopen(filename, "a" );
	sscanf( key_array, "%[^?]\n", output_array);
	int results =fputs(output_array,fd);
	fclose( fd );
	remove_elements(key_array,strlen(key_array),strlen(output_array)+4);
	memset(&output_array[0], 0, sizeof(output_array));
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
	filenumber = rand() %100000; // Default Value
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
	MPI_Bcast(filename, 256, MPI_CHAR, nthreads-1, MPI_COMM_WORLD);
	tstart = myclock(); // Global Clock

	// Initialization :
	//-----------------

	group_size = 100;
	group_number = 0;
	int flag = 0;
	int flag2 = 0;
	int ord = 0;	
   	tstart_init = MPI_Wtime();  // Private Clock for each core
	init_list(&rank);
	if (rank != 0)read_lines();
	else read_dict_words();
	MPI_Barrier(MPI_COMM_WORLD);

	//MPI_Bcast( array, 100, MPI_INT, root, comm);
	//MPI_Send(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
	//MPI_Isend(const void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
	//MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
	//MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source,int tag, MPI_Comm comm, MPI_Request *request)
	//MPI_IPROBE(source, tag, comm, flag, status)
	//MPI_Cancel(MPI_Request *request)
	//MPI_Abort(MPI_Comm comm, int errorcode)
	//MPI_File_open(MPI_Comm comm, char *filename, int amode, MPI_Info info, MPI_File *fh) 
	while (group_number < maxwords/group_size)
	{
		// Sending keywords from rank 0 to all the other:
		//-----------------------------------------------
		if (rank == 0)
		{
			for (int receiver = 1; receiver < nthreads; receiver++)
			{
				if (Free[receiver] == 0)
				{
		 			group_words(group_number);
		 			group_number += 1;
		 			Free[receiver] = 1; // Means that the reciever is now busy
		 			MPI_Isend(&Free[receiver], 1, MPI_INT, receiver, 1234, MPI_COMM_WORLD, &Request); 
					Order[ord] = receiver;
					ord++;
					MPI_Isend(words_group, 1200, MPI_CHAR, receiver, 5678, MPI_COMM_WORLD, &Request); 
				}
				else
				{
					MPI_Iprobe(receiver, 4321, MPI_COMM_WORLD, &flag, &Status );
					if (flag)
					{
						MPI_Irecv(&Free[receiver], 1, MPI_INT, receiver, 4321, MPI_COMM_WORLD, &Request);						
					}
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
					MPI_Irecv(words_group, 1200, MPI_CHAR, 0, 5678, MPI_COMM_WORLD, &Request);
					split_words();
					count_words(&rank); // Counting 
					strcat(key_array,"???\n");
					flag2 = 0;
					flag = 0;
					Free[rank] = 0; // Means that the reciever is now free
					MPI_Isend(&Free[rank], 1, MPI_INT,0,4321,MPI_COMM_WORLD, &Request);  // Tell rank 0 you r free
				}
			}
			MPI_Iprobe(0, 9999, MPI_COMM_WORLD, &flag, &Status );
			if (flag)
			{
				MPI_Irecv(&group_number, 1, MPI_INT, 0, 9999, MPI_COMM_WORLD, &Request);	
			}
		}
	}

	// Exiting the counting Processs:
	//-----------------------------------------------
	if(rank==0)
	{
		for (int receiver = 1; receiver < nthreads; receiver++)
		{
			MPI_Isend(&group_number, 1, MPI_INT, receiver,9999, MPI_COMM_WORLD, &Request); 
		}
	}
	printf("Done rank %d\n",rank);
	MPI_Barrier(MPI_COMM_WORLD);
	// Probing for messages from the counting Processs:
	//-------------------------------------------------
	i = 1;
	if(rank==0)
	{
		while(i == 1)
		{
			i = 0;
			for (int receiver = 1; receiver < nthreads; receiver++)
			{
				MPI_Iprobe(receiver, 4321, MPI_COMM_WORLD, &flag, &Status );
				if (flag) 
					{
						printf("Message 4321 was not recieved from rank %d \n",receiver);
						MPI_Irecv(&Free[receiver], 1, MPI_INT, receiver, 4321, MPI_COMM_WORLD, &Request);
						i = 1;
					}
			}
		}
	}
	else
	{
		MPI_Iprobe(0, 1234, MPI_COMM_WORLD, &flag, &Status );
		if (flag)  printf("Message 1234 not recieved, rank %d,1,%d\n",rank,flag);
		MPI_Iprobe(0, 9999, MPI_COMM_WORLD, &flag, &Status );
		if (flag)  printf("Message 9999 not recieved, rank %d,1,%d\n",rank,flag);
	}
	MPI_Barrier(MPI_COMM_WORLD);
	// Sending output to rank 0:
	//-----------------------------------------------

	MPI_Bcast(Order,maxwords/group_size, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Barrier(MPI_COMM_WORLD);

	// Priniting final data output to rank 0:
	//-----------------------------------------------

	for(i = 0; i < maxwords/group_size;i++)
	{
		if (rank == Order[i])
		{
		dump_words();
		}
		MPI_Barrier(MPI_COMM_WORLD);
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

