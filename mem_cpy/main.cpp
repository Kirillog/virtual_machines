#include <cassert>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>

static constexpr size_t MEM_SIZE = 256 * 1024 * 1024;

typedef void *(*memcpy_func_t)(void *, const void *, size_t);

static size_t pool_size;

class MemcpyPool {
private:
    void Run(size_t i, void* dst, const void* src, size_t size) {
        size_t csize = size / (pool_size + 1);
        memcpy((char *)dst + csize * i, (char *)src + csize * i, csize);
    }
public:
    MemcpyPool(void* dst, const void* src, size_t size, size_t pool_size) {
        threads_.reserve(pool_size);
        for (size_t i = 0; i < pool_size; ++i) {
            threads_.emplace_back(&MemcpyPool::Run, this, i, dst, src, size);
        }
    }

    void WaitShutdown() {
        for (auto& thread : threads_) {
            thread.join();
        }    
    }

private:
    std::vector<std::thread> threads_;
};

void* parallel_memcpy(void* dst, const void* src, size_t size) {
    size_t offset = (size / (pool_size + 1)) * pool_size;
    memcpy((char *)dst + offset, (char *)src + offset, size - offset);
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
    for (pool_size = 0; pool_size <= 8; ++pool_size) {
        init_mem(dmem.data(), dmem.size());
        MemcpyPool pool{dmem.data(), smem.data(), smem.size(), pool_size};
        auto from = std::chrono::steady_clock::now();
        parallel_memcpy(dmem.data(), smem.data(), smem.size());
        pool.WaitShutdown();
        auto to = std::chrono::steady_clock::now();
        auto time =  std::chrono::duration_cast<std::chrono::microseconds>(to - from).count();
        printf("Pool size: %lu, time: %ld microseconds\n", pool_size, time);
        assert(memcmp(smem.data(), dmem.data(), smem.size()) == 0);
    }
    return 0;
}