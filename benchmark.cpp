#include <bits/stdc++.h>
#include "QuickLogger.hpp"
#include <sched.h>

inline void SetCpuAffinity(int cpu)
{
    if (cpu >= 0) {
    #ifdef __linux__
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        sched_setaffinity(0, sizeof(mask), &mask);
    #endif
    }
}

void benchmark(QuickLogger::QuickLogger &myLogger, int threadID, int cpu, int threads){
    SetCpuAffinity(cpu);
    int const iters =  1e6;
    const char* text = "BENCHMARK";
    uint64_t begin = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
    for(int i = 0 ; i < iters ; i++){
        if(!myLogger.LogItem(i%QuickLogger::LOG_TYPES, threadID,  "LOGGING {} {} {}", text, i+1, 42.42)){
            printf("Unable to log %d!\n", i);
        }
    }
    uint64_t end = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
    long int avg_latency = (end-begin)/iters;
    long long int tot_time = end-begin;
    printf("\tAverage Latency = %ld nanoseconds\n", avg_latency);
    printf("\tTotal Time Taken for Logging was %lld nanoseconds\n", tot_time);
}

template<typename Function>
void run_benchmark(Function && f, int thread_count, int total_cores){
    printf("\nThread Count : %d\n", thread_count);
    std::vector<std::thread> threads;

    uint64_t begin = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);

    QuickLogger::QuickLogger &myLogger = QuickLogger::START_QUICK_LOGGER("", thread_count, false);

    for(int i = 0 ; i < thread_count ; i++){
        threads.push_back(std::thread(f, std::ref(myLogger), i, total_cores -(i%total_cores), thread_count ) );
    }
    for(int i = 0 ; i < thread_count ; i++){
        threads[i].join();
    }
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    QuickLogger::STOP_QUICK_LOGGER(myLogger);
    uint64_t end = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);
    long long int time_taken = end-begin;
    printf("\nTotal Time Taken from start to end is %lld nanoseconds.\n", time_taken);
    return;
}

int main(){

    // cpu_set_t mask;
    // CPU_ZERO(&mask);
    // CPU_SET(4, &mask);
    // sched_setaffinity(0, sizeof(mask), &mask);

    // int number_of_threads = 8;

    // QuickLogger::QuickLogger &myLogger = QuickLogger::START_QUICK_LOGGER("", number_of_threads, 1024, false);

    // const int max_value = 1e6;

    for(auto threads : {1, 2, 4, 8}){
        run_benchmark(benchmark, threads, 8);
    }

    // uint64_t begin = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);

    // for(int i = 0 ; i < max_value ; i++){
    //     // printf("Working till here!");
    //     // if(!QuickLogger::LOG_ERROR<int>(myLogger, i+1)){
    //     //     printf("Unable to log %d!\n", i+1);
    //     // }
    //     // std::this_thread::sleep_for(std::chrono::seconds(1));
    // }

//    std::this_thread::sleep_for(std::chrono::microseconds(1000));

    // u_int64_t end = std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::nanoseconds(1);

    // long int avg_latency = (end-begin)/max_value;
    // long long int tot_time = end-begin;
    // printf("\tAverage Latency = %ld nanoseconds\n", avg_latency);
    // printf("\tTotal Time Taken was %lld nanoseconds\n", tot_time);

    // std::this_thread::sleep_for(std::chrono::seconds(10));

    // QuickLogger::STOP_QUICK_LOGGER(myLogger);

    return 0;
}