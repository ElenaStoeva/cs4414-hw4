// Adapted from Sagar's word counter

#include <iostream>

#include "ngram_counter.hpp"

// this program computes ngram frequencies for all .txt files in the given directory and its subdirectories
int main(int argc, char *argv[])
{
  if (argc != 4)
  {
    std::cout << "Usage: " << argv[0] << " <dir> -t=<num-threads> -n=<n-gram>" << std::endl;
    return 1;
  }

  int number_threads = atoi(argv[2] + 3);
  int ngram_length = atoi(argv[3] + 3);

  nc::ngramCounter ngram_counter(argv[1], number_threads, ngram_length);
  ngram_counter.compute();
}
