#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <functional>
#include <mutex>
#include <random>
#include <thread>

static constexpr size_t MEM_SIZE = 256 * 1024 * 1024;

typedef std::function<void()> task_t;

class ThreadPool {
private:
    void Run() {
        while (true) {
            std::unique_lock lock{task_mutex_};
            while (!closed_ && tasks_.empty()) {
                not_empty_var_.wait(lock);
            }
 
            if (closed_) {
                break;
            }
            auto task = std::move(tasks_.front());
    
            tasks_.pop_front();
            lock.unlock();
            task();
            std::this_thread::yield();
        }
    
    }
public:
    ThreadPool(size_t pool_size) {
        threads_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            threads_.emplace_back(&ThreadPool::Run, this);
        }
    }

    void WaitShutdown() {
        {
            std::lock_guard lock{task_mutex_};
            if (closed_) {
                return;
            }
            closed_ = true;
        }

        for (size_t i = 0; i < threads_.size(); ++i) {
            not_empty_var_.notify_one();
        }    
    
        for (auto& thread : threads_) {
            thread.join();
        }    
    }

    void Submit(task_t&& task) {
        std::unique_lock lock{task_mutex_};
        if (closed_) {
            return;
        }
        tasks_.emplace_back(std::move(task));
        not_empty_var_.notify_one();
    }

private:
    std::mutex task_mutex_;
    bool closed_{false};
    std::vector<std::thread> threads_;
    std::deque<task_t> tasks_;
    std::condition_variable not_empty_var_;
};

void* parallel_memcpy(void* dst, const void* src, size_t size, size_t pool_size) {
    ThreadPool pool{pool_size};
    size_t thread_count = pool_size + 1;
    size_t csize = size / thread_count;
    for (size_t i = 0; i < pool_size; ++i) {
        char * dfrom = (char *)dst + csize * i;
        char * sfrom = (char *)src + csize * i;
        pool.Submit([dfrom, sfrom, csize](){
            memcpy(dfrom, sfrom, csize);
        });
    }
    size_t offset = csize * pool_size;
    memcpy((char *)dst + offset, (char *)src + offset, size - offset);
    pool.WaitShutdown();
    return dst;
}

static std::mt19937 rnd;

void init_mem(char *ptr, size_t size) {
    for (char *ptr_end = ptr + size; ptr < ptr_end; ptr += 8) {
        *((uint64_t*)ptr) = rnd();
    }
}

int main() {
    std::vector<char> smem(MEM_SIZE);
    std::vector<char> dmem(MEM_SIZE);
    init_mem(smem.data(), smem.size());
    for (size_t pool_size = 0; pool_size <= 8; ++pool_size) {
        init_mem(dmem.data(), dmem.size());
        auto from = std::chrono::steady_clock::now();
        parallel_memcpy(dmem.data(), smem.data(), smem.size(), pool_size);
        auto to = std::chrono::steady_clock::now();
        auto time =  std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
        printf("Pool size: %lu, time: %ld microseconds\n", pool_size, time);
        assert(memcmp(smem.data(), dmem.data(), smem.size()) == 0);
    }
    return 0;
}