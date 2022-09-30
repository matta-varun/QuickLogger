#ifndef QUICK_LOGGER_H
#define QUICK_LOGGER_H

#include <stdlib.h>
#include <iostream>
#include <thread>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <filesystem>
#include <sched.h>
#include "xenium/ramalhete_queue.hpp"
#include "xenium/reclamation/generic_epoch_based.hpp"
#include "date.h"


namespace QuickLogger {

enum LOGGER_LEVEL : u_int32_t {
    ERROR = 0,
    WARN = 1,
    FAULT = 2,
    INFO = 3,
    DEBUG = 4,
    TRACE = 5
};

const int LOG_TYPES = 6;

std::string logLevelMessages[6] = {"ERROR", "WARN", "FAULT", "INFO", "DEBUG", "TRACE"};


/**
 * @brief Class for the Log Item storing the Log Value and its information.
 *
 * This Class implements the structure needed for storing the Log value and its
 * accompanying information. Please note that the value is being stored as a
 * std::string which implies the type of data you pass to the Logger to log has
 * to be convertable to string using the fmt::format utility.
 * 
 * Attributes:
 *  * logLevel
 *    Stores the level which the log is intended for.
 *  * value
 *    Stores the actual value of the Log converted to a string. Note that in case 
 *    of a string that needs formatting, the string is initially stored unformatted
 *    and later formatted using the saved args in the consumer thread.
 *  * time
 *    Stores the time of logging of the log.
 *  * parameterFlag
 *    Stores if the log has any parameters using which the value has to be formatted.
 *  * saved_op
 *    A saved method call
 * 
 * Methods:
 * 
 *  @tparam Pointer to Log
 *  @tparam Tuple of variadic arguments saved
 *  * DoOperation:
 *    Defines the formatting operation that needs to be done using the variadic arguments
 *    on the value of the Log given by the point.
 * 
 *  @tparam Parameter Pack
 *  * BuildOperation:
 *    Bundles the passed parameters into a tuple and constructs a saved operation which is
 *    stored in the saved_op attribute. This enables delayed invocation of the formatting 
 *    method while preserving the variadic arguments.
 *
 */
class Log {
    public:
    int logLevel;
    std::string value;
    std::chrono::high_resolution_clock::time_point time;
    bool parameterFlag;

    typedef std::function<void(Log*)> saved_operation;

    saved_operation saved_op;

    template<typename ...P>
    void DoOperation(Log* self, std::tuple<P...> const& tup){
        std::apply([self](auto &&... args){self->value = fmt::format(fmt::to_string(args)...);}, tup);
        return;
    }


    template<typename ...P>
    saved_operation BuildOperation(P&&... params) const {
        auto tup = std::make_tuple(std::forward<P>(params)...);
        return [tup](Log* self){
            return self->DoOperation<P...>(self, tup);
        };
    }

};


/**
 * @brief Implementation of the QuickLogger Class
 *
 * The Class implementing all required methods to use the Logger. Implemented as a Singleton.
 * Working of the Logger is as follows,
 *    
 *    The Logger is started using the START_QUICK_LOGGER function. This function initializes the
 *    singleton object of the QuickLogger and then spawns the consumer threads keeping track of 
 *    their own Fast Unbounded Lock-Free Multi-Producer Multi-Consumer queues.
 *    Please note that START_QUICK_LOGGER returns a reference to the Singleton Logger object and
 *    this is needed going forward. However you can always get the reference back via a call to 
 *    the method instance().
 *    
 *    The Logging is done by calling the class method LogItem. The level of the log, the thread ID
 *    and the log value and parameters should be passed. Please note that the thread ID indicates 
 *    which particular queue the log is to be pushed into and hence should be between 0 and 
 *    processor_count-1. Ideally you should be using as many producer threads as consumer threads.
 * 
 *    To stop the Logger, call STOP_QUICK_LOGGER which takes in the reference from earlier.
 *    This signals the threads to finish up and cleans out all the allocated memory and resets the
 *    QuickLogger.
 * 
 * Attributes:
 *  * is_stdout
 *    Stores whether the logger is enabled for logging the values to standard output
 *    in addition to file output.
 *  * processor_count
 *    Stores the number of threads to spawn as consumers. This also decides the number of
 *    queues that are constructed. When this value is not specified during the construction of
 *    QuickLogger, it is automatically set equal to the number of cores that the running
 *    machine has.
 *  * outputFiles
 *    Stores the FILE pointers to the LOG_TYPES different log level files.
 *  * initInstanceFlag
 *    Keeps track of instantiation. Once initialized, cannot do it again, unless the QuickLogger
 *    is stopped and destroyed.
 *  * start_flag
 *    Keeps track of the status of QuickLogger, whether the Logger is running or idle.
 *  * threadTerminateFlags
 *    Pointer to an array of atomic flags that are used to signal the threads when the Logger is
 *    requested to stop.
 *  * lockFreeQueues
 *    Vector of pointers to Lock-Free Unbounded MPMC Queues which are used by the threads.
 *  * threads
 *    Vector of the thread objects.
 */
class QuickLogger {

    private:
        bool         is_stdout;

        QuickLogger(){};
        ~QuickLogger() = default;

    public:

        int                 processor_count;
        std::FILE*          outputFiles[LOG_TYPES];
        bool                initInstanceFlag = true;
        bool                start_flag = true;
        std::atomic<bool>*  threadTerminateFlags;

        std::vector<xenium::ramalhete_queue<Log*,xenium::policy::reclaimer<xenium::reclamation::epoch_based<>>,xenium::policy::entries_per_node<2048>>*> lockFreeQueues;
        
        std::vector<std::thread> threads;

        QuickLogger(QuickLogger const&) = delete;
        void operator=(QuickLogger const&) = delete;


        /**
         * @brief Initializes the QuickLogger by setting its parameters.
         * 
         * This method also creates the logs folder in the current working directory if it is doesn't
         * exist and creates/opens the log files in append mode.
         * 
         * @param myLogger          Reference to the Logger
         * @param num_of_threads    Number of threads
         * @param s                 string which is the path to a target directory
         * @param enableSTDOUT      boolean indicating whether to enable output to STDOUT
         * @return                  void
         */
        void setParameters(QuickLogger &myLogger, int num_of_threads, std::string s, bool enableSTDOUT = true){
            myLogger.is_stdout = enableSTDOUT;

            if(num_of_threads > 0){
                processor_count = num_of_threads;
            }
            else{
                processor_count = std::thread::hardware_concurrency();
            }

            lockFreeQueues.resize(processor_count);
            for(int i = 0 ; i < processor_count ; i++){
                lockFreeQueues[i] = nullptr;
            }
            threadTerminateFlags = (std::atomic<bool>*)malloc(processor_count*sizeof(std::atomic<bool>));
            for(int i = 0 ; i < processor_count ; i++){
                threadTerminateFlags[i] = false;
            }
            for(int i = 0 ; i < processor_count ; i++){
                threadTerminateFlags[i] = false;
            }
            
            std::filesystem::path p = s;
            if(!std::filesystem::is_directory(p)){
                p = std::filesystem::current_path();
            }
            
            if(!std::filesystem::is_directory(p/"logs")){
                std::filesystem::create_directory((p / "logs").string());
            }
            
            for(int i = 0 ; i < LOG_TYPES ; i++){
                outputFiles[i] = std::fopen( (p / "logs" / (logLevelMessages[i] + ".log")).c_str(), "a" );
                if(outputFiles[i] == nullptr){
                    std::cerr<<"Unable to open file "<<i<<"\n";
                }
                fmt::print(outputFiles[i], "\n\n-------------Starting new Session---------------\n\n");
            }

        }

        /**
         * @brief Returns the reference of the Singleton object.
         * 
         * @param enableSTDOUT      boolean indicating whether to enable output to STDOUT
         * @return                  Reference to the QuickLogger Singleton
         */
        static QuickLogger& instance(/* bool enableSTDOUT = true*/){
            static QuickLogger newlogger;

            return newlogger;
        }


        /**
         * @brief the consumer thread function
         * 
         * Consumer threads are spawned as this function which keeps checking the queue for new
         * logs until the Logger is stopped.
         * 
         * @param threadID          The ID uniquely identifying the thread in the Logger.
         * @param cpu               This value is used to SET the affinity mask of this thread.
         *                          This is ignored when the number of cores in the system are less
         *                          than the number of threads we want to spawn.
         * @return                  void
         */
        void consumerThread( int threadID, int cpu){
            
            xenium::ramalhete_queue<Log*,xenium::policy::reclaimer<xenium::reclamation::epoch_based<>>,xenium::policy::entries_per_node<2048>>* myqueue = new xenium::ramalhete_queue<Log*,xenium::policy::reclaimer<xenium::reclamation::epoch_based<>>,xenium::policy::entries_per_node<2048>>();

            lockFreeQueues[threadID] = myqueue;
            
            std::string id = fmt::to_string(threadID);

            // if(cpu >= 0 && processor_count <= std::thread::hardware_concurrency()){
            //     cpu_set_t mask;
            //     CPU_ZERO(&mask);
            //     CPU_SET(cpu, &mask);
            //     sched_setaffinity(0, sizeof(mask), &mask);
            // }

            Log* newlog =  NULL;

            bool pop_status = false;

            while( (pop_status = myqueue->try_pop(std::ref(newlog))) || !threadTerminateFlags[threadID] ){

                if(!pop_status)
                continue;

                if(newlog->parameterFlag){
                    newlog->saved_op(newlog);
                }

                using namespace date;
                using namespace std::chrono;

                auto sd = floor<days>(newlog->time);
                // Create time_of_day
                auto tod = date::make_time(newlog->time - sd);
                // Create year_month_day
                year_month_day ymd = sd;

                // Extract field types as int
                int y = int{ymd.year()}; // Note 1
                int m = unsigned{ymd.month()};
                int d = unsigned{ymd.day()};
                int h = tod.hours().count();
                int M = tod.minutes().count();
                int s = tod.seconds().count();
                int ns = duration_cast<nanoseconds>(tod.subseconds()).count();
                
                std::string time = fmt::format("{}-{}-{} {}:{}:{}.{}\t", y, m, d, h, M, s, ns);

                std::string logMessage =  time + "\tThread ID : " + id + "\t" + newlog->value + "\n"; 
                
                fmt::print(outputFiles[newlog->logLevel], logMessage);

                if(is_stdout){
                    switch (newlog->logLevel)
                    {
                    case ERROR:
                        fmt::print(fmt::fg(fmt::color::red) | fmt::bg(fmt::color::yellow), logMessage);
                        break;
                    case WARN:
                        fmt::print(fmt::fg(fmt::color::yellow), logMessage);
                        break;
                    case FAULT:
                        fmt::print(fmt::fg(fmt::color::orange), logMessage);
                        break;
                    case INFO:
                        fmt::print(fmt::fg(fmt::color::aqua), logMessage);
                        break;
                    case DEBUG:
                        fmt::print(fmt::fg(fmt::color::green), logMessage);
                        break;
                    case TRACE:
                        fmt::print(fmt::fg(fmt::color::hot_pink), logMessage);
                        break;
                    
                    default:
                        fmt::print(fmt::fg(fmt::color::antique_white), logMessage);
                        break;
                    }
                }

                if(pop_status){
                    free(newlog);
                    newlog = NULL;
                }
            }

            lockFreeQueues[threadID] = nullptr;
            free(myqueue);
            return;
        }

        
        /**
         * @brief Starts the Logger
         * 
         * Spawns the consumer threads, and only returns when all the threads have been spawned
         * and their queues have been initialized.
         */
        void StartLogger(){
            if(threads.size() == processor_count){
                std::cerr<<"ERROR\t:\tMax Threads already created and running\n";
            }
            int TOT_TRDS = processor_count == 1 ? 1 : processor_count/2;
            int copy = processor_count;
            for(int i = 0 ; i < copy ; i++){
                int temp = (i%TOT_TRDS)+1;
                threads.push_back(std::thread(&QuickLogger::consumerThread, this, i, temp));
            }

            while(true){
                bool checkflag = true;
                for(int i = 0 ; i < copy ; i++){
                    checkflag &= (lockFreeQueues[i] != nullptr);
                }
                if(checkflag)
                break;
            }
        }

        /**
         * @brief Starts the Logger once.
         * 
         * Later calls are ignored unless the Logger is stopped and started again.
         */
        void start(){
            if(start_flag){
                StartLogger();
                start_flag = false;
            }
            return;
        }

        /**
         * @brief Initializes the Logger once
         * 
         * Later calls are ignored unless the Logger is stopped and started again.
         * 
         * @param myLogger              Reference to Logger object
         * @param number_of_threads     Number of threads
         * @param s                     String representing path to the target directory
         * @param enableSTDOUT          boolean indicating whether to enable output for STDOUT
         * @return                      The number of threads the Logger will be spawning as consumers
         */
        int Initialize(QuickLogger &myLogger, int number_of_threads, std::string s, bool enableSTDOUT = true){
            if(initInstanceFlag){
                setParameters(myLogger, number_of_threads, s, enableSTDOUT);
                initInstanceFlag = false;
            }
            return myLogger.processor_count;
        }

        /**
         * @brief currently not implemented as anything. 
         */
        void StopLogger();

        
        /**
         * @brief Logs the Item passed to it
         * 
         * Logs the item into the queue belonging to the thread given by threadID.
         * Before logging, the formatting method call is saved with the arguments in a function
         * wrapper which will be invoked in the consumer thread. This is done to reduce the 
         * logging latency as string formatting during logging can slow the performance a lot.
         * 
         * @param level             Log Level
         * @param threadID          Uniquely identifying thread ID
         * @param value             an object of type T which is to be logged. 
         *                          (must be convertable to string using fmt::to_string)
         * @param parameters        the parameter pack using which the value is to be formatted.
         * @return                  `true` if the operation was successful, otherwise `false`
         */
        template<typename T, typename ...P>
        bool LogItem(int level, int threadID, T &&value, P&&... parameters){

            Log *l = new Log();
            
            l->value = std::string(value);
            int paramlength = 0;

            ([&]
            {

                paramlength++;
                while(parameters)break;

            } (), ...);

            l->logLevel = level;
            l->time = std::chrono::system_clock::now();


            if(paramlength == 0){
                l->parameterFlag = false;
            }
            else{
                l->parameterFlag = true;               
                l->saved_op = l->BuildOperation(std::move(l->value), std::move(parameters)...);
            }
            
            bool flag = true;

            if(threadID < 0 || threadID >= processor_count){
                return false;
            }
            else{
                if(lockFreeQueues[threadID] != nullptr){
                    lockFreeQueues[threadID]->push(l);
                    flag = false;
                }
                
                return !flag;
            }

        }
};

/**
 * @brief Starts the Quick Logger
 * 
 * Gets an instance of the QuickLogger singleton, initializes the logger and starts it.
 * 
 * @param num_of_threads        the number of threads, passed as an integer by reference.
 *                              If the number given cannot be accomodated, the Logger sets it
 *                              automatically to the number of cores in the system.
 * @param enableSTDOUT          boolean indicating whether output to STDOUT should be enabled.
 * @return                      Reference to the QuickLogger singleton.
 */
QuickLogger &START_QUICK_LOGGER(int &num_of_threads, bool enableSTDOUT = true){
    QuickLogger &myLogger = QuickLogger::instance();
    num_of_threads = myLogger.Initialize(myLogger, num_of_threads, "", enableSTDOUT);
    myLogger.start();
    return myLogger;
}

/**
 * @brief An overload of the above function
 * 
 * @param s                     String indicating the path of the target directory.
 * @param num_of_threads        the number of threads, passed as an integer by reference.
 *                              If the number given cannot be accomodated, the Logger sets it
 *                              automatically to the number of cores in the system.
 * @param enableSTDOUT          boolean indicating whether output to STDOUT should be enabled.
 * @return                      Reference to the QuickLogger singleton.
 */
QuickLogger &START_QUICK_LOGGER(std::string s, int &num_of_threads, bool enableSTDOUT = true){
    printf("Starting Logger...\n");
    QuickLogger &myLogger = QuickLogger::instance();
    num_of_threads = myLogger.Initialize(myLogger, num_of_threads, s, enableSTDOUT);
    myLogger.start();
    printf("Done!\n");
    return myLogger;
}

/**
 * @brief An overload of the above function. enableSTDOUT is false by default.
 * 
 * @param num_of_threads        the number of threads, passed as an integer by reference.
 *                              If the number given cannot be accomodated, the Logger sets it
 *                              automatically to the number of cores in the system.
 * @return                      Reference to the QuickLogger singleton.
 */
QuickLogger &START_QUICK_LOGGER(int &num_of_threads){
    QuickLogger &myLogger = QuickLogger::instance();
    num_of_threads = myLogger.Initialize(myLogger, num_of_threads, "", false);
    myLogger.start();
    return myLogger;
}

/**
 * @brief Stops the QuickLogger
 * 
 * Signals the threads to stop and clean up. Frees all the resources held by the Logger and
 * resets its attributes. As good as new!
 * 
 * @param myLogger              Reference to the QuickLogger singleton.
 * @return                      void
 */
void STOP_QUICK_LOGGER(QuickLogger& myLogger){
    printf("Stopping Logger\n");
    for(int i = 0 ; i < myLogger.processor_count ; i++){
        myLogger.threadTerminateFlags[i] = true;
        myLogger.threads[i].join();
    }
    myLogger.threads.clear();

    for(int i = 0 ; i < LOG_TYPES ; i++){
        if(myLogger.outputFiles[i] != nullptr){
            fclose(myLogger.outputFiles[i]);
        }
    }

    myLogger.start_flag = true;
    myLogger.initInstanceFlag = true;
    myLogger.lockFreeQueues.clear();
    free(myLogger.threadTerminateFlags);

    return;
}
}


#endif
