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

    struct TFunctions {
        std::function<Ret(void)> function;
        TFunctionState           status;
        Ret                      result;
    };

    std::vector<TFunctions> functions;
    boost::mutex functions_mutex;
    size_t next_point = 0;                 // number of function which wait for calling. left of this was already called

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

    int execute(std::function<Ret(Args...)> f, Args... args) {
        //check if "f" is a valid target
        if (f) {
            //insert new function to queue
            std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
            int id = functions.size();
            functions.push_back(TFunctions {std::bind(f, args...), FS_Queued, });
            functions_mutex_lock.unlock();

            //create new thread if it's possible(can create) and necessary(all threads execute smth)
            if (open_threads < threads_num) {
                free_thread_mutex.lock();
                if (free_thread == 0) {
                    threads_box.push_back(boost::thread(long_thread, this));
                    open_threads++;
                }
                free_thread_mutex.unlock();
            }

            //signal, that there is one more function for execution
            std::unique_lock<std::mutex> locker(signal_mutex);
            signal_to_work.notify_one();

            return id;
        }
        return -1;
    }

    TFunctionState status(size_t id) { // check, did the function execute?
        std::lock_guard<boost::mutex> functions_mutex_lock(functions_mutex);
        return functions[id].status;
    }

    Ret result(size_t id) {
        std::lock_guard<boost::mutex> functions_mutex_lock(functions_mutex);
        if (functions[id].status == FS_Ready) return functions[id].result;
        else throw "execution haven't been ended";
    }

private:
    static void long_thread(Implementation * self) {
        while (true) {
            //creating of smart locker for signal mutex
            std::unique_lock<std::mutex> signal_mutex_lock(self->signal_mutex, std::defer_lock);
            while (true) {
                //lock functions mutex for searching function to execute
                std::unique_lock<boost::mutex> functions_mutex_lock(self->functions_mutex);
                
                //it's necessary to forbid sending of signals
                //because while thread search function to execute, signal sending can cause unexpected results
                signal_mutex_lock.lock();

                //check if function to execute is exist
                if (self->next_point == self->functions.size()) {
                    //if there are no functions to execute, searching has ended
                    functions_mutex_lock.unlock();
                    break;
                }
                
                //Now signal sending can't hurt damage
                signal_mutex_lock.unlock();

                //Function getting, its status changing
                size_t f_id = self->next_point;
                auto func = self->functions[f_id].function;
                self->functions[f_id].status = FS_Running;
                self->next_point++;

                //Functions are not necessary at executing time
                functions_mutex_lock.unlock();

                //Executing
                Ret r = func();

                //Results writing, status changing
                functions_mutex_lock.lock();
                self->functions[f_id].result = r;
                self->functions[f_id].status = FS_Ready;
                functions_mutex_lock.unlock();
            }

            //Check for destructorization
            if (self->die_signal) {
                signal_mutex_lock.unlock();
                break;
            }

            //While thread waits signal, thread do nothing. Increasing number of "free" threads
            std::unique_lock<boost::mutex> free_thread_mutex_lock(self->free_thread_mutex);
            self->free_thread++;
            free_thread_mutex_lock.unlock();

            //Waiting for new function to execute it.
            //It's important, that signal mutex was locked before this.
            //Otherwise, situation, when function "execute" send signal but all threads don't catch it and go to "sleep mode", possible
            self->signal_to_work.wait(signal_mutex_lock);
            signal_mutex_lock.unlock();

            //Thread has got signal. Decreasing number of "free" threads
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

    struct TFunctions {
        std::function<void(void)> function;
        TFunctionState           status;
    };

    std::vector<TFunctions> functions;
    boost::mutex functions_mutex;
    size_t next_point = 0;                 // number of function which wait for calling. left of this was already called

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

    int execute(std::function<void(Args...)> f, Args... args) {
        //check if "f" is a valid target
        if (f) {
            //insert new function to queue
            std::unique_lock<boost::mutex> functions_mutex_lock(functions_mutex);
            int id = functions.size();
            functions.push_back(TFunctions {std::bind(f, args...), FS_Queued, });
            functions_mutex_lock.unlock();

            //create new thread if it's possible(can create) and necessary(all threads execute smth)
            if (open_threads < threads_num) {
                free_thread_mutex.lock();
                if (free_thread == 0) {
                    threads_box.push_back(boost::thread(long_thread, this));
                    open_threads++;
                }
                free_thread_mutex.unlock();
            }

            //signal, that there is one more function for execution
            std::unique_lock<std::mutex> locker(signal_mutex);
            signal_to_work.notify_one();

            return id;
        }
        return -1;
    }

    TFunctionState status(size_t id) { // check, did the function execute?
        std::lock_guard<boost::mutex> functions_mutex_lock(functions_mutex);
        return functions[id].status;
    }

private:
    static void long_thread(Implementation * self) {
        while (true) {
            //creating of smart locker for signal mutex
            std::unique_lock<std::mutex> signal_mutex_lock(self->signal_mutex, std::defer_lock);
            while (true) {
                //lock functions mutex for searching function to execute
                std::unique_lock<boost::mutex> functions_mutex_lock(self->functions_mutex);
                
                //it's necessary to forbid sending of signals
                //because while thread search function to execute, signal sending can cause unexpected results
                signal_mutex_lock.lock();

                //check if function to execute is exist
                if (self->next_point == self->functions.size()) {
                    //if there are no functions to execute, searching has ended
                    functions_mutex_lock.unlock();
                    break;
                }
                
                //Now signal sending can't hurt damage
                signal_mutex_lock.unlock();

                //Function getting, its status changing
                size_t f_id = self->next_point;
                auto func = self->functions[f_id].function;
                self->functions[f_id].status = FS_Running;
                self->next_point++;

                //Functions are not necessary at executing time
                functions_mutex_lock.unlock();

                //Executing
                func();

                //Status changing
                functions_mutex_lock.lock();
                self->functions[f_id].status = FS_Ready;
                functions_mutex_lock.unlock();
            }

            //Check for destructorization
            if (self->die_signal) {
                signal_mutex_lock.unlock();
                break;
            }

            //While thread waits signal, thread do nothing. Increasing number of "free" threads
            std::unique_lock<boost::mutex> free_thread_mutex_lock(self->free_thread_mutex);
            self->free_thread++;
            free_thread_mutex_lock.unlock();

            //Waiting for new function to execute it.
            //It's important, that signal mutex was locked before this.
            //Otherwise, situation, when function "execute" send signal but all threads don't catch it and go to "sleep mode", possible
            self->signal_to_work.wait(signal_mutex_lock);
            signal_mutex_lock.unlock();

            //Thread has got signal. Decreasing number of "free" threads
            free_thread_mutex_lock.lock();
            self->free_thread--;
            free_thread_mutex_lock.unlock();
        }
    }
};
