#include <cassert>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>

static constexpr size_t MEM_SIZE = 256 * 1024 * 1024;

void* parallel_memcpy(void* dst, const void* src, size_t size, size_t pool_size) {
    std::vector<std::jthread> pool_threads;
    size_t thread_count = pool_size + 1;
    size_t csize = size / thread_count;
    for (size_t i = 0; i < pool_size; ++i) {
        char * dfrom = (char *)dst + csize * i;
        char * sfrom = (char *)src + csize * i;
        pool_threads.emplace_back([dfrom, sfrom, csize](){
            memcpy(dfrom, sfrom, csize);
        });
    }
    size_t offset = csize * pool_size;
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