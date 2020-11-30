# Project 3: C-Language Advance Methadolgies using MPI
The current repository presents three codes that use MPI to read a moderately large (wiki_dump.txt, 1.7GB) file containing approximately 1M Wikipedia entries, 1 entry per line. The codes will look for a keyword in a list of 50,000 words (keywords.txt, extracted from a common cracking dictionary) in the wikipedia entry files. 
The files are in /625 folder. 

The task of each code is as follow:
1. Read the Wiki files into memory
2. Check for key word matches
3. Print out an alphabetized list of words which appeared in the text strings, with their indices (by line number).


The three codes are:
1. Star/centralized: Designate rank 0 as the source of keywords. Divide the set of lines among the worker processes. Each worker process will request the next individual keyword from rank 0, search for matches in its Wikipedia entries, and return an array listing any line numbers the keyword appears in (up to 100). Rank 0 should print out all the results.
2. Ring – the same idea, but pass the keyword from process n to process rank n+1, updating a count of the total times the word appears. When it gets back to rank 0, print out the word and the total, if the total is greater than 0.
3. Work queue – Batch up the keywords into groups of 100 on rank 0 as you read them in from the file. Put the batched keywords on a list/queue of tasks to be completed. Each worker spins, requesting the next batch of keywords from Rank 0, completing the search from that batch, and at the end of the program run returns the results (up to 100 appearances per keyword) to rank 0 for printing out.

The results are shown in "Report.pdf"
