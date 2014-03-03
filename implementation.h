#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <condition_variable>
#include <functional>
#include <vector>
#include <list>

static const int DEFAULT_CORE_NUMBER = 4;

enum TFunctionState {FS_Undef, FS_Queued, FS_Running, FS_Ready};

template <typename Ret, typename ... Args>
class Implementation {

    size_t threads_num;                    // maximal threads number
    size_t open_threads = 0;               // created threads number
    std::list<boost::thread> threads_box;

    std::vector<std::pair<std::function<Ret(Args...)>, Args...>> functions; // function & arguments
    std::vector<TFunctionState> statuses;
    std::vector<Ret> results;
    size_t next_point = 0;                 // number of function which wait for calling. left of this was already called
    boost::mutex functions_mutex;

    std::condition_variable signal_to_work;
    std::mutex signal_mutex;

    size_t free_thread = 0;                // number of threads which are doing nothing and wait for new function ...
    boost::mutex free_thread_mutex;        //     ... it is nessesary just for using minimum of 'threads_num' threads

    bool die_signal = false;               // message for threads to break implementation

public:
    Implementation() {
        threads_num = boost::thread::hardware_concurrency();
        if (threads_num == 0) threads_num = DEFAULT_CORE_NUMBER;
    }
    Implementation(size_t threads_number) {
        if (threads_number > 0) threads_num = threads_number;
        else throw "bad number of threads";
    }

   ~Implementation() {
        std::unique_lock<std::mutex> locker(signal_mutex);
        die_signal = true;
        signal_to_work.notify_all();
        locker.unlock();
        for (auto i = threads_box.begin(); i != threads_box.end(); i++) i->join();
    }

    int execute(std::function<Ret(Args...)> f, Args... args) { //посыл функции на выполнение. возвращает id функции
        if (f) {
            std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
            int id = functions.size();
            functions.push_back(std::pair<std::function<Ret(Args...)>, Args...> (f, args...));
            statuses.push_back(FS_Queued);
            results.resize(results.size() + 1);
            functions_mutex_lock.unlock();

            if (open_threads < threads_num) {
                free_thread_mutex.lock();
                if (free_thread == 0) {
                    threads_box.push_back(boost::thread(long_thread, this));
                    open_threads++;
                }
                free_thread_mutex.unlock();
            }

            std::unique_lock<std::mutex> locker(signal_mutex);
            signal_to_work.notify_one();

            return id;
        }
        return -1;
    }

    TFunctionState status(size_t id) { // check, did the function execute?
        std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
        return statuses[id];
    }

    Ret result(int id) {
        std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
        if (statuses[id] == FS_Ready) return results[id];
        else throw "execution haven't been ended";
    }

private:
    static void long_thread(Implementation * self) {
        while (true) {
            std::unique_lock<std::mutex> signal_mutex_lock(self->signal_mutex, std::defer_lock);
            while (true) {
                std::unique_lock<boost::mutex> functions_mutex_lock(self->functions_mutex);
                signal_mutex_lock.lock();
                if (self->next_point == self->functions.size()) {
                    functions_mutex_lock.unlock();
                    break;
                }
                signal_mutex_lock.unlock();
                size_t f_id = self->next_point;
                std::pair<std::function<Ret(Args...)>, Args...> func_args(self->functions[f_id]);
                self->statuses[f_id] = FS_Running;
                self->next_point++;
                functions_mutex_lock.unlock();

                Ret r = func_args.first(func_args.second);

                functions_mutex_lock.lock();
                self->results[f_id] = r;
                self->statuses[f_id] = FS_Ready;
                functions_mutex_lock.unlock();
            }

            if (self->die_signal) { 
                signal_mutex_lock.unlock();
                break;
            }

            std::unique_lock<boost::mutex> free_thread_mutex_lock(self->free_thread_mutex);
            self->free_thread++;
            free_thread_mutex_lock.unlock();

            self->signal_to_work.wait(signal_mutex_lock);
            signal_mutex_lock.unlock();

            free_thread_mutex_lock.lock();
            self->free_thread--;
            free_thread_mutex_lock.unlock();
        }
    }
};


//void partial specialization
template <typename ... Args>
class Implementation<void, Args...> {

    size_t threads_num;                    // maximal threads number
    size_t open_threads = 0;               // created threads number
    std::list<boost::thread> threads_box;

    std::vector<std::pair<std::function<void(Args...)>, Args...>> functions; // function & arguments
    std::vector<TFunctionState> statuses;
    size_t next_point = 0;                 // number of function which wait for calling. left of this was already called
    boost::mutex functions_mutex;

    std::condition_variable signal_to_work;
    std::mutex signal_mutex;

    size_t free_thread = 0;                // number of threads which are doing nothing and wait for new function ...
    boost::mutex free_thread_mutex;        //     ... it is nessesary just for using minimum of 'threads_num' threads

    bool die_signal = false;               // message for threads to break implementation

public:
    Implementation() {
        threads_num = boost::thread::hardware_concurrency();
        if (threads_num == 0) threads_num = DEFAULT_CORE_NUMBER;
    }
    Implementation(size_t threads_number) {
        if (threads_number > 0) threads_num = threads_number;
        else throw "bad number of threads";
    }

   ~Implementation() {
        std::unique_lock<std::mutex> locker(signal_mutex);
        die_signal = true;
        signal_to_work.notify_all();
        locker.unlock();
        for (auto i = threads_box.begin(); i != threads_box.end(); i++) i->join();
    }

    int execute(std::function<void(Args...)> f, Args... args) { //посыл функции на выполнение. возвращает id функции
        if (f) {
            std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
            int id = functions.size();
            functions.push_back(std::pair<std::function<void(Args...)>, Args...> (f, args...));
            statuses.push_back(FS_Queued);
            functions_mutex_lock.unlock();

            if (open_threads < threads_num) {
                free_thread_mutex.lock();
                if (free_thread == 0) {
                    threads_box.push_back(boost::thread(long_thread, this));
                    open_threads++;
                }
                free_thread_mutex.unlock();
            }

            std::unique_lock<std::mutex> locker(signal_mutex);
            signal_to_work.notify_one();

            return id;
        }
        return -1;
    }

    TFunctionState status(size_t id) { // check, did the function execute?
        std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
        return statuses[id];
    }

private:
    static void long_thread(Implementation * self) {
        while (true) {
            std::unique_lock<std::mutex> signal_mutex_lock(self->signal_mutex, std::defer_lock);
            while (true) {
                std::unique_lock<boost::mutex> functions_mutex_lock(self->functions_mutex);
                signal_mutex_lock.lock();
                if (self->next_point == self->functions.size()) {
                    functions_mutex_lock.unlock();
                    break;
                }
                signal_mutex_lock.unlock();
                size_t f_id = self->next_point;
                std::pair<std::function<void(Args...)>, Args...> func_args(self->functions[f_id]);
                self->statuses[f_id] = FS_Running;
                self->next_point++;
                functions_mutex_lock.unlock();

                func_args.first(func_args.second);

                functions_mutex_lock.lock();
                self->statuses[f_id] = FS_Ready;
                functions_mutex_lock.unlock();
            }

            if (self->die_signal) { 
                signal_mutex_lock.unlock();
                break;
            }

            std::unique_lock<boost::mutex> free_thread_mutex_lock(self->free_thread_mutex);
            self->free_thread++;
            free_thread_mutex_lock.unlock();

            self->signal_to_work.wait(signal_mutex_lock);
            signal_mutex_lock.unlock();

            free_thread_mutex_lock.lock();
            self->free_thread--;
            free_thread_mutex_lock.unlock();
        }
    }
};
