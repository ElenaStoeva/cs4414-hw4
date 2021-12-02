// Adapted from Sagar's word counter solution

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

namespace wc
{
    class wordCounter
    {
        // main storage structure for word frequencies
        std::map<std::string, uint64_t> freq;

        fs::path dir;
        uint32_t num_threads;
        uint32_t ngram;

        void process_file(fs::path &file, std::map<std::string, uint64_t> &local_freq);

    public:
        wordCounter(const std::string &dir, uint32_t num_threads, uint32_t ngram);
        void compute();
        void display();
    };
} // namespace wc