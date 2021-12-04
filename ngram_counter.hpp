// Adapted from Sagar's word counter

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

namespace nc
{
    class ngramCounter
    {
        fs::path dir;
        uint32_t num_threads;
        uint32_t ngram; // length of ngrams

        void process_file(fs::path &file, std::map<std::string, uint64_t> &local_freq);

    public:
        ngramCounter(const std::string &dir, uint32_t num_threads, uint32_t ngram);
        void compute();
    };
} // namespace nc
