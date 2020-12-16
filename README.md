# Project 3: C-Language Advance Methadolgies using MPI
The current repository presents three codes that use the MPI module to read a moderately large (wiki_dump.txt, 1.7GB) file containing approximately 1M Wikipedia entries. The codes will search for a keyword from a list of 50,000 words (keywords.txt) in the wikipedia entry files. The "wiki_dump.txt" and "keyword.txt" files are in the /625 folder. 

The task of each code is as follow:
1. Read the Wiki files 
2. Check the number of times the keyword has occured
3. Save the Line in which the keyword has occured
3. Print out an alphabetized list of keyword which appeared in the text strings, with their indices (by line number).

Each of the three codes utilize a different search algorithm to look for the keywords. The three search algorithms are:
1. Star/centralized approach: In in this alogirtim, the code will designate processor 0 as the source of keywords, and divide the set of lines among the other processors (which will be called worker) . Each worker processor will request the next individual keyword from rank 0, search for matches in its Wikipedia entries, and return an array listing any line numbers the keyword appears in (up to 100). Finally processor 0 prints out all the results.
2. Ring approach: In this approach, the Wikipedia entries are divided equally over all the processors. Each processor will take a keyword from the list search for it in its Wikipedia entry and then when done will send it to the next rank. Thus all keywords will be passed from process n to process n+1 while having an update on the total count of the times the word appears. When the keyword returns back to processor 0, the processor will print out the word and the total time it appeared (if the total is greater than 0).
3. Work queue approach: In this approach, processor 0 will batch up the keywords into groups of 100 and then send the batched keywords on a list/queue of tasks to be completed to a free processor. Each worker spins, requesting the next batch of keywords from processor 0, completing the search from that batch, and at the end of the program run returns the results (up to 100 appearances per keyword) to processor 0 for printing out.

The results are shown in "Report.pdf"
