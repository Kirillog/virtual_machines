#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <random>
#include <thread>

static constexpr size_t MEM_SIZE = 256 * 1024 * 1024;

static std::mt19937 rnd;

class MemcpyPool {
private:
    void Run(size_t i) {
        size_t local_run_step = 0;
        while (true) {
            ++local_run_step;
            {
                std::unique_lock lock{mutex_};
                tpool_var_.wait(lock, [&](){
                    return closed_ || run_step_ == local_run_step;
                });
            }
            if (closed_) {
                break;
            }
            size_t csize = size / (pool_size + 1);
            memcpy((char *)dst + csize * i, (char *)src + csize * i, csize);
            {
                std::unique_lock lock{mutex_};
                ++completed_;
                if (completed_ == pool_size) {
                    manager_var_.notify_one();
                }
            }
        }
    }
public:
    MemcpyPool(size_t pool_size) : pool_size(pool_size) {
        threads_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            threads_.emplace_back(&MemcpyPool::Run, this, i);
        }
    }

    void StartFor(void* dst, const void* src, size_t size) {
        std::unique_lock lock{mutex_};
        this->dst = dst;
        this->src = src;
        this->size = size;
        ++run_step_;
        tpool_var_.notify_all();
    }

    void WaitStep() {
        std::unique_lock lock{mutex_};
        manager_var_.wait(lock, [&](){
            return completed_ == pool_size;
        });
        completed_ = 0;
    }

    void* parallel_memcpy(void* dst, const void* src, size_t size) {
        StartFor(dst, src, size);
        size_t offset = (size / (pool_size + 1)) * pool_size;
        memcpy((char *)dst + offset, (char *)src + offset, size - offset);
        WaitStep();
        return dst;
    }

    ~MemcpyPool() {
        std::unique_lock lock{mutex_};
        closed_ = true;
        tpool_var_.notify_all();
    }

private:
    char pad[128];
    size_t pool_size;
    void *dst;
    const void *src;
    size_t size;
    char pad2[128];
private:
    std::vector<std::jthread> threads_;
    std::mutex mutex_;
    std::condition_variable tpool_var_;
    std::condition_variable manager_var_;
    size_t run_step_{0};
    size_t completed_{0};
    bool closed_{false};
};



void init_mem(char *ptr, size_t size) {
    for (char *ptr_end = ptr + size; ptr < ptr_end; ptr += 8) {
        *((uint64_t*)ptr) = rnd();
    }
}

int main() {
    std::vector<char> smem(MEM_SIZE);
    std::vector<char> dmem(MEM_SIZE);
    init_mem(smem.data(), smem.size());
    init_mem(dmem.data(), dmem.size());
    auto from = std::chrono::steady_clock::now();
    memcpy(dmem.data(), smem.data(), smem.size());
    auto to = std::chrono::steady_clock::now();
    auto time =  std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
    assert(memcmp(smem.data(), dmem.data(), smem.size()) == 0);
    fprintf(stderr, "Default memcpy time:\t %ld microseconds\n", time);
    for (size_t pool_size = 0; pool_size <= 8; ++pool_size) {
        init_mem(dmem.data(), dmem.size());
        MemcpyPool pool{pool_size};
        from = std::chrono::steady_clock::now();
        pool.parallel_memcpy(dmem.data(), smem.data(), smem.size());
        to = std::chrono::steady_clock::now();
        time =  std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
        fprintf(stderr, "Pool size: %lu, time:\t %ld microseconds\n", pool_size, time);
        assert(memcmp(smem.data(), dmem.data(), smem.size()) == 0);
    }
    return 0;
}