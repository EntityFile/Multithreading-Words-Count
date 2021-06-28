#include <algorithm>
#include <iostream>
#include <string>
#include <boost/locale.hpp>
#include <boost/filesystem.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <boost/algorithm/string.hpp>


inline std::chrono::high_resolution_clock::time_point get_current_time_fenced()
{
    std::atomic_thread_fence(std::memory_order_seq_cst);
    auto res_time = std::chrono::high_resolution_clock::now();
    std::atomic_thread_fence(std::memory_order_seq_cst);
    return res_time;
}

template<class D>
inline long long to_us(const D& d)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

// Модифікована черга з  https://juanchopanzacpp.wordpress.com/2013/02/26/concurrent-queue-c11/

template <typename T>
class Queue
{
public:
    int maxSize;

    explicit Queue(int& maxQueueSize) {
        maxSize = maxQueueSize;
    }

    int size() {
        return queue_.size();
    }

    void push_front(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push_front(item);
        mlock.unlock();
        cond1.notify_one();
    }

    T pop()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond1.wait(mlock);
        }
        auto item = queue_.front();
        queue_.pop_front();

        mlock.unlock();
        cond2.notify_one();

        return item;
    }

    void pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond1.wait(mlock);
        }
        item = queue_.front();
        queue_.pop_front();

        mlock.unlock();
        cond2.notify_one();
    }

    void push(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);

        while (queue_.size() >= maxSize) {

            cond2.wait(mlock);
        }
        queue_.push_back(item);
        mlock.unlock();
        cond1.notify_one();
    }

    void push(T&& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.size() >= maxSize) {
            cond2.wait(mlock);
        }
        queue_.push_back(std::move(item));
        mlock.unlock();
        cond1.notify_one();
    }

private:
    std::deque<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond1;
    std::condition_variable cond2;
};

// Функція взята з https://github.com/libarchive/libarchive/wiki/Examples

std::string extract_archive(const std::string &buffer)
{
    struct archive *a;
    struct archive_entry *entry;
    int r;

    a = archive_read_new();
    archive_read_support_format_all(a);

    if ((r = archive_read_open_memory(a, buffer.c_str(), buffer.size()))) {
        exit(1);
    }
    r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF) {
        std::cerr << "Archive is empty" << std::endl;
    }
    if (r < ARCHIVE_OK) {
        std::cerr << archive_error_string(a) << std::endl;
    }
    if (r < ARCHIVE_WARN) {
        std::cerr << archive_error_string(a) << std::endl;
        exit(1);
    }

    std::string output(archive_entry_size(entry), char{});
    archive_read_data(a, &output[0], output.size());
    return output;
}

std::map<std::string, int> analyzeWords(std::pair<std::string, std::string>* currElem) {
    std::string buffer;
    std::map<std::string, int> dict;

    if (currElem->first == ".zip") {
        buffer = extract_archive(currElem->second);
        /*try{

        }
        catch (const char* err) {
            buffer = "";
        }*/
    } else {
        buffer = currElem->second;
    }

    buffer = boost::locale::to_lower(boost::locale::fold_case(boost::locale::normalize(buffer)));
    boost::locale::boundary::ssegment_index map(boost::locale::boundary::word, buffer.begin(), buffer.end());
    map.rule( boost::locale::boundary::word_letters);

    for ( boost::locale::boundary::ssegment_index::iterator it = map.begin(), e = map.end(); it != e; ++it) {
        dict[*it]++;
    }

    return dict;
}

void count_words_multi_threading(Queue<std::pair<std::string, std::string>> &queue, Queue<std::map<std::string, int>> &merge_queue) {
    std::map<std::string, int> dict;

    while (true) {
        std::pair<std::string, std::string> currElem = queue.pop();
        // Перевірка на death_pill
        if (currElem.second.empty()) {
            queue.push(currElem);
            break;
        }

        dict = analyzeWords(&currElem);

        // Пуш словників до іншої черги
        if(!dict.empty()){
            merge_queue.push_front(dict);
            dict = {};
        }
    }
}

void merge_v2(Queue<std::map<std::string, int>> &merge_queue, std::mutex& m, std::map<std::string, int>& finalDict) {
    std::map<std::string, int> new_dict;

    while(true) {
        std::map<std::string, int> dict1 = merge_queue.pop();
        if (dict1.empty()) {
            merge_queue.push(dict1);
            break;
        }

        for (const auto &word : dict1) {
            new_dict[word.first] += word.second;
        }
    }

    m.lock();
    for (const auto &word : new_dict) {
        finalDict[word.first] += word.second;
    }
    m.unlock();
}

void merging_dicts(Queue<std::map<std::string, int>> &merge_queue) {
    while (true) {
        std::map<std::string, int> dict1 = merge_queue.pop();
        std::map<std::string, int> dict2 = merge_queue.pop();

        // Якщо dict1 це death_pill то вона пушиться в кінець черги, а інший словник на початок
        if (dict1.empty()) {
            merge_queue.push_front(dict2);
            merge_queue.push(dict1);
            break;
        }

        // Якщо dict2 це death_pill то вона пушиться в кінець черги, а інший словник на початок
        if (dict2.empty()) {
            merge_queue.push_front(dict1);
            merge_queue.push(dict2);
            break;
        }

        std::map<std::string, int> new_dict;

        // Мердж словників
        for (const auto &word : dict1) {
            new_dict[word.first] += word.second;
        }
        for (const auto &word : dict2) {
            new_dict[word.first] += word.second;
        }

        // Пуш нового словника назад у чергу
        merge_queue.push_front(new_dict);
    }
}

void push_files_to_the_queue(std::string* directoryPath, const std::string& file_type,
                             const std::string& archive_type, Queue<std::pair<std::string, std::string>> &queue) {


    std::pair<std::string, std::string> death_pill;

    if(boost::filesystem::is_directory(*directoryPath)) {

        boost::filesystem::recursive_directory_iterator end;
        for (boost::filesystem::recursive_directory_iterator i(*directoryPath); i != end; ++i)
        {
            const boost::filesystem::path entry = (*i);
            std::string entryType = boost::filesystem::extension(entry);
            boost::algorithm::to_lower(entryType);

            if ((entryType == file_type) || (entryType == archive_type)) {
                if (boost::filesystem::file_size(entry) < 10000000) {
                    std::ifstream raw_file(entry.string(), std::ios::binary);
                    std::ostringstream buffer_ss;
                    buffer_ss << raw_file.rdbuf();
                    std::string buffer{buffer_ss.str()};

                    std::pair<std::string, std::string> pr = {entryType, buffer};
                    queue.push(std::move(pr));
                }
            }
        }

        queue.push(death_pill);
    } else {
        std::cout << "Wrong directory for parsing" << std::endl;
        exit(1);
    }
}

bool sort_by_val(const std::pair<std::string, int> &a,
                 const std::pair<std::string, int> &b)
{
    return (a.second > b.second) || ((a.second == b.second) && (a.first < b.first));
}

bool sort_by_name(const std::pair<std::string, int> &a,
                  const std::pair<std::string, int> &b)
{
    return (a.first < b.first);
}

void write_results(std::vector<std::pair<std::string, int>> *words_vector, const std::string &out_by_a,
                   const std::string &out_by_n) {
    std::sort(words_vector->begin(), words_vector->end(), sort_by_name);

    std::ofstream out_a (out_by_a);
    for(const auto& elem : *words_vector){
        out_a << elem.first << "\t" << elem.second << std::endl;
    }
    out_a.close();

    std::sort(words_vector->begin(), words_vector->end(), sort_by_val);

    std::ofstream out_n (out_by_n);
    for(const auto& elem : *words_vector){
        out_n << elem.first << "\t" << elem.second << std::endl;
    }
    out_n.close();
}

void readConfig(std::string *configFilePath, std::string& filesPath,
                std::string& out_by_a, std::string& out_by_n, int& counting_thrds,
                int& merging_thrds, int& maxQueueSize) {
    std::ifstream myFile (*configFilePath);

    if (myFile.is_open())
    {
        myFile >> filesPath >> out_by_a >> out_by_n >> counting_thrds >> merging_thrds >> maxQueueSize;
        myFile.close();
    }

    else {
        std::cout << "Unable to open config file" << std::endl;
        exit(1);
    }

    if (filesPath.empty()) {
        std::cout << "Wrong index path" << std::endl;
        exit(1);
    }

    if (out_by_a.empty()) {
        out_by_a = "./out_by_a.txt";
    }

    if (out_by_n.empty()) {
        out_by_n = "./out_by_n.txt";
    }

    if ((counting_thrds <= 0) || (merging_thrds <= 0)) {
        std::cout << "Wrong threads number" << std::endl;
        exit(1);
    }

    if (maxQueueSize <= 0) {
        std::cout << "Wrong queue size" << std::endl;
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    std::string filesPath;
    std::string out_by_a;
    std::string out_by_n;
    int counting_thrds;
    int merging_thrds;
    int maxQueueSize;
    int maxMergeSize = 100000;

    std::string file_type = ".txt";
    std::string archive_type = ".zip";

    std::string config;

    if (argc == 2) {
        config = argv[1];
    } else {
        if (boost::filesystem::exists("./config.dat")) {
            config = "./config.dat";
        } else {
            std::cout << "Unable to find config file" << std::endl;
            exit(1);
        }
    }

    readConfig(&config, filesPath, out_by_a, out_by_n, counting_thrds, merging_thrds, maxQueueSize);


    boost::locale::generator gen;
    std::locale::global(gen("en_us.UTF-8"));

    Queue<std::pair<std::string, std::string>> queue(maxQueueSize);
    Queue<std::map<std::string, int>> merge_queue(maxMergeSize);

    std::vector<std::thread> v;

    // Читання усіх директорій та додавання усіх .txt або .zip файлів до черги
    v.emplace_back(push_files_to_the_queue, &filesPath, file_type, archive_type, std::ref(queue));

    // Паралельний підрахунок слів у файлах, що знаходяться у черзі та пуш вже готових словників до іншої черги
    auto start2 = get_current_time_fenced();

    for (int i = 0; i < counting_thrds; ++i) {
        v.emplace_back(count_words_multi_threading, std::ref(queue), std::ref(merge_queue));
    }

    for (int j = 0; j <= counting_thrds; j++) {
        v[j].join();
    }

    // Пуш стоп-словника до другої черги
    std::map<std::string, int> death_pill2{};
    merge_queue.push(death_pill2);
    // Мердж усіх словників в один

    std::mutex m;
    std::map<std::string, int> final_dict;

    for (int ind = 0; ind < merging_thrds; ++ind) {
        v.emplace_back(merge_v2, std::ref(merge_queue), std::ref(m), std::ref(final_dict));
    }

    for (int k = counting_thrds+1; k <= merging_thrds + counting_thrds; k++) {
        v[k].join();
    }

    auto stop2 = get_current_time_fenced();
    auto duration2 = stop2 - start2;
    // Запис рузультату у вектор
    auto start3 = get_current_time_fenced();

    std::vector<std::pair<std::string, int>> words_vector;
    for (const auto& element : final_dict) {
        words_vector.emplace_back(element);
    }

    // Запис результуту у відповідні файли
    write_results(&words_vector, std::ref(out_by_a), std::ref(out_by_n));

    auto stop3 = get_current_time_fenced();
    auto duration3 = stop3 - start3;

    std::cout << "Indexing: " << to_us(duration2)/1000000 << " sec" << std::endl;
    std::cout << "Total: " << to_us(duration2 + duration3)/1000000 << " sec" << std::endl;
}