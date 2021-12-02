// Adapted from Sagar's word counter solution

#include "ngram_counter.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <regex>
#include <thread>
#include <vector>
#include <functional>
#include <future>

#include "utils.hpp"

wc::wordCounter::wordCounter(const std::string &dir, uint32_t num_threads, uint32_t ngram)
    : dir(dir),
      num_threads(num_threads),
      ngram(ngram)
{
}

void wc::wordCounter::compute()
{
    // this tracks which files have already been processed or are being processed
    std::mutex wc_mtx;

    std::vector<fs::path> files_to_sweep = utils::find_all_files(dir, [](const std::string &extension)
                                                                 { return extension == ".txt" || extension == ".hex"; });

    // threads use this atomic as fetch and add to decide on which files to process
    std::atomic<uint64_t> global_index = 0;

    std::vector<std::vector<std::promise<std::vector<std::pair<std::string, uint64_t>>>>> shuffle_matrix;
    // // add promises

    for (int i = 0; i < num_threads; i++)
    {
        std::vector<std::promise<std::vector<std::pair<std::string, uint64_t>>>> vec;
        for (int j = 0; j < num_threads; j++)
        {
            std::promise<std::vector<std::pair<std::string, uint64_t>>> promised_assignment;
            vec.push_back(std::move(promised_assignment));
        }
        shuffle_matrix.push_back(std::move(vec));
    }

    std::vector<std::vector<std::future<std::vector<std::pair<std::string, uint64_t>>>>> shuffle_matrix_fut;
    // // add futures
    for (int i = 0; i < num_threads; i++)
    {
        std::vector<std::future<std::vector<std::pair<std::string, uint64_t>>>> vec;
        for (int prom_id = 0; prom_id < num_threads; prom_id++)
        {
            std::future<std::vector<std::pair<std::string, uint64_t>>> future_assignment = (shuffle_matrix.at(i).at(prom_id)).get_future();
            vec.push_back(std::move(future_assignment));
        }
        shuffle_matrix_fut.push_back(std::move(vec));
    }

    // ngram table that will store the ngram strings at indexes corresponding to their hash values
    // std::vector<std::string> ngram_table;
    // 64 bit unsigned integers
    // no need to do it

    auto sweep = [this, &files_to_sweep, &global_index, &wc_mtx, &shuffle_matrix, &shuffle_matrix_fut](int thread_id)
    {
        std::map<std::string, uint64_t> local_freq;

        uint64_t file_index;
        while ((file_index = global_index++) < files_to_sweep.size())
        {
            process_file(files_to_sweep[file_index], local_freq);
        }

        // Assign pairs to other threads
        std::hash<std::string> str_hash;
        std::vector<std::vector<std::pair<std::string, uint64_t>>> assignment_matrix(num_threads);
        for (auto pair : local_freq)
        {
            // find which thread to shuffle to
            size_t assigned_thread = str_hash(pair.first) % num_threads;
            assignment_matrix.at(assigned_thread).push_back(pair);
        }
        // Each assigned vector is the value for a promise in the shuffle matrix
        for (int i = 0; i < num_threads; i++)
        {
            // std::cout << thread_id << ": " << i << std::endl;
            // std::promise<std::vector<std::pair<std::string, uint64_t>>> *pr = shuffle_matrix.at(i).at(thread_id);
            (shuffle_matrix.at(i).at(thread_id)).set_value(assignment_matrix.at(i));
        }

        // Process pairs that have been assigned to this thread
        std::map<std::string, uint64_t> reduced_freq;
        for (int t = 0; t < num_threads; t++)
        {
            std::vector<std::pair<std::string, uint64_t>> updates = (shuffle_matrix_fut.at(thread_id).at(t)).get();
            for (auto [ngram, cnt] : updates)
            {
                reduced_freq[ngram] += cnt;
            }
        }

        std::vector<std::pair<std::string, uint64_t>> top_five(5);
        std::partial_sort_copy(reduced_freq.begin(),
                               reduced_freq.end(),
                               top_five.begin(),
                               top_five.end(),
                               [](std::pair<const std::string, uint64_t> const &l,
                                  std::pair<const std::string, uint64_t> const &r)
                               {
                                   return l.second > r.second;
                               });

        std::lock_guard<std::mutex> lock(wc_mtx);
        std::cout << "Thread " << thread_id << ":" << std::endl;
        for (auto [ngram, cnt] : top_five)
        {
            if (cnt == 0)
            {
                std::cout << "       ..." << std::endl;
            }
            else
            {
                std::cout << "       " << ngram << ": " << cnt << std::endl;
            }
        }
    };

    // start all threads and wait for them to finish
    std::vector<std::thread> workers;
    for (uint32_t i = 0; i < num_threads; ++i)
    {
        workers.push_back(std::thread(sweep, i));
    }
    for (auto &worker : workers)
    {
        worker.join();
    }
}

char toLowerCase(char c)
{
    return (c >= 65 && c <= 90) ? c + 32 : c;
}

char removeNewLine(char c)
{
    return (c == 10) ? 32 : c;
}

// TODO: add more punctuation marks
char removePunctuationAndNumbers(char c)
{
    return (c >= 48 && c <= 57 || c == 33 || c == 44 || c == 46 || c == 63) ? 124 : c;
}

void wc::wordCounter::process_file(fs::path &file, std::map<std::string, uint64_t> &local_freq)
{
    // read the entire file and update local_freq
    std::ifstream fin(file);
    std::stringstream buffer;
    buffer << fin.rdbuf();
    std::string contents = buffer.str();
    std::transform(contents.begin(), contents.end(), contents.begin(), toLowerCase);
    std::transform(contents.begin(), contents.end(), contents.begin(), removeNewLine);
    std::transform(contents.begin(), contents.end(), contents.begin(), removePunctuationAndNumbers);

    // break the word into sequences of alphanumeric characters, ignoring other characters
    std::regex rgx("[|]");
    std::sregex_token_iterator iter(contents.begin(), contents.end(), rgx, -1);
    std::sregex_token_iterator end;

    std::vector<std::string> ngrams;

    for (; iter != end; ++iter)
    {
        if (*iter != "")
        {
            // find ngrams
            std::string sequence = *iter;
            // std::cout << sequence << std::endl;
            std::regex rgx("[a-z]+");
            std::sregex_token_iterator iter_seq(sequence.begin(), sequence.end(), rgx);
            std::sregex_token_iterator end_seq;
            for (; iter_seq != end_seq; ++iter_seq)
            {
                // start an ngram
                int n = 1;
                std::string current_ngram = *iter_seq;
                std::sregex_token_iterator iter_seq_inner = iter_seq;
                ++iter_seq_inner;
                while (n < ngram && iter_seq_inner != end_seq)
                {
                    std::string word = *iter_seq_inner;
                    current_ngram = current_ngram + " " + word;
                    n++;
                    ++iter_seq_inner;
                }
                if (n == ngram)
                {
                    local_freq[current_ngram]++;
                }
            }
        }
    }
}
