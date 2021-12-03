CC = g++
CFLAGS = -O3 -std=c++17

hw4:
	$(CC) $(CFLAGS) ngram.cpp ngram_counter.cpp utils.cpp -lpthread -lstdc++fs -o hw4

clean:
	rm -f hw4