#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <concepts>
#include <thread>
#include <unistd.h>
#include <signal.h>
#include <mutex>
#include <atomic>
#include <vector>
#include <array>

using namespace std;

static constexpr size_t PAGE_SIZE = 1 << 12;
static constexpr size_t MAX_ALLOCATORS = 20;
static constexpr unsigned long long PAGE_PTR_MASK = (-1ll) ^ ((1 << 12) - 1);

static const char str[] = "Got SIGSEGV, probably because of bad allocation at "; 

static std::array<std::atomic<const void*>, MAX_ALLOCATORS> areas{};

static char * itoa(int val, char *buf)
{
  int length = 0;
  int _val = val;
  do {
    ++length;
    _val /= 10;
  } while (_val);
  buf += length;
  *buf = 0;
  do {
    --buf;
    *buf = '0' + val % 10;
    val /= 10;
  } while (val);
  return buf;
}

static void handler(int, siginfo_t *si, void *)
{
  int id = -1;
  char * aligned_ptr = (char *)((unsigned long long)si->si_addr & PAGE_PTR_MASK);
  for (size_t i = 0; i < areas.size(); ++i) {
    if (areas[i].load(std::memory_order_relaxed) == aligned_ptr) {
      id = i;
      break;
    }
  }
  if (id == -1) {
    write(STDERR_FILENO, "Default Allocator\n", 18);
  } else {
    char buf[10];
    const char *id_str = itoa(id, buf);
    write(STDERR_FILENO, str, sizeof(str) - 1);
    write(STDERR_FILENO, "Allocator#", 10);
    write(STDERR_FILENO, id_str, std::strlen(id_str));
    write(STDERR_FILENO, "\n", 1);
  }
  exit(EXIT_FAILURE);
}

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 * https://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-runtime-using-c
 */
static size_t getCurrentRSS() {
  long rss = 0L;
  FILE* fp = NULL;
  if ((fp = fopen( "/proc/self/statm", "r" )) == NULL) {
    return (size_t)0L;
  }
  if (fscanf( fp, "%*s%ld", &rss ) != 1) {
    fclose(fp);
    return (size_t)0L;
  }
  fclose(fp);
  return (size_t)rss * 1024;
}

static void get_usage(struct rusage& usage) {
  if (getrusage(RUSAGE_SELF, &usage)) {
    perror("Cannot get usage");
    exit(EXIT_SUCCESS);
  }
}

template<typename T, typename Elem>
concept Alloc = std::derived_from<T, std::allocator<Elem>>;

struct Node {
  Node* next;
  unsigned node_id;
};

static unsigned long long hintElemCount = 0;

static unsigned long long roundToPageSize(unsigned long long size) {
  return (size + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE;
}

template <typename T>
class MemPoolAllocator : public std::allocator<T> {
private:
  char *mempool_, *ptr;
  unsigned long long size;
  int id;
  static constexpr int type = 0; 

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  MemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    id = -1;
    for (size_t i = 0; i < MAX_ALLOCATORS; ++i) {
      const void* exp = nullptr;
      if (areas[i].compare_exchange_weak(exp, mempool_, std::memory_order_relaxed)) {
        id = i;
        break;
      }
    }
    if (id == -1) {
      throw std::invalid_argument("Too many allocators at the same time");
    }
  }

  ~MemPoolAllocator() {
    areas[id].store(nullptr, std::memory_order_relaxed);
    munmap(mempool_, size);
  }

  MemPoolAllocator(MemPoolAllocator && a) = delete;
  MemPoolAllocator(const MemPoolAllocator&) = delete;
  MemPoolAllocator& operator=(const MemPoolAllocator&) = delete;

  template <class U>
  explicit MemPoolAllocator(const MemPoolAllocator<U>&) {}

  T *allocate(size_t count) {
    return (T*)(ptr -= sizeof(T) * count);
  }

  void deallocate(T*, size_t) {
    // do nothing
  }

  template<class U, class... Args>
  void construct(U* p, Args&&... args) {
      new (p) U(std::forward<Args>(args)...);
  }

  template<class U>
  void destroy(U* p) {
      (*p).~U();
  }

  template <class U>
  struct rebind {
      using other = MemPoolAllocator<U>;
  };
  using value_type = T;
};

template <typename T>
class MutexedMemPoolAllocator : public std::allocator<T> {
private:
  char *mempool_, *ptr;
  unsigned long long size;
  int id;
  static constexpr int type = 1;
  std::mutex m_;

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  MutexedMemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    id = -1;
    for (size_t i = 0; i < MAX_ALLOCATORS; ++i) {
      const void* exp = nullptr;
      if (areas[i].compare_exchange_weak(exp, mempool_, std::memory_order_relaxed)) {
        id = i;
        break;
      }
    }
    if (id == -1) {
      throw std::invalid_argument("Too many allocators at the same time");
    }
  }

  ~MutexedMemPoolAllocator() {
    areas[id].store(nullptr, std::memory_order_relaxed);
    munmap(mempool_, size);
  }

  MutexedMemPoolAllocator(MutexedMemPoolAllocator && a) = delete;
  MutexedMemPoolAllocator(const MutexedMemPoolAllocator&) = delete;
  MutexedMemPoolAllocator& operator=(const MutexedMemPoolAllocator&) = delete;

  template <class U>
  explicit MutexedMemPoolAllocator(const MutexedMemPoolAllocator<U>&) {}

  T *allocate(size_t count) {
    std::lock_guard lock{m_};
    return (T*)(ptr -= sizeof(T) * count);
  }

  void deallocate(T*, size_t) {
    // do nothing
  }

  template<class U, class... Args>
  void construct(U* p, Args&&... args) {
      new (p) U(std::forward<Args>(args)...);
  }

  template<class U>
  void destroy(U* p) {
      (*p).~U();
  }

  template <class U>
  struct rebind {
      using other = MutexedMemPoolAllocator<U>;
  };
  using value_type = T;
};

template <typename T>
class LockFreeMemPoolAllocator : public std::allocator<T> {
private:
  char *mempool_;
  std::atomic<char*> ptr;
  unsigned long long size;
  int id;
  static constexpr int type = 2;

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  LockFreeMemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    id = -1;
    for (size_t i = 0; i < MAX_ALLOCATORS; ++i) {
      const void* exp = nullptr;
      if (areas[i].compare_exchange_weak(exp, mempool_, std::memory_order_relaxed)) {
        id = i;
        break;
      }
    }
    if (id == -1) {
      throw std::invalid_argument("Too many allocators at the same time");
    }
  }

  ~LockFreeMemPoolAllocator() {
    areas[id].store(nullptr, std::memory_order_relaxed);
    munmap(mempool_, size);
  }

  LockFreeMemPoolAllocator(LockFreeMemPoolAllocator && a) = delete;
  LockFreeMemPoolAllocator(const LockFreeMemPoolAllocator&) = delete;
  LockFreeMemPoolAllocator& operator=(const LockFreeMemPoolAllocator&) = delete;

  template <class U>
  explicit LockFreeMemPoolAllocator(const LockFreeMemPoolAllocator<U>&) {}

  T *allocate(size_t count) {
    return (T*)(ptr.fetch_sub(sizeof(T) * count, std::memory_order_relaxed) - sizeof(T) * count);
  }

  void deallocate(T*, size_t) {
    // do nothing
  }

  template<class U, class... Args>
  void construct(U* p, Args&&... args) {
      new (p) U(std::forward<Args>(args)...);
  }

  template<class U>
  void destroy(U* p) {
      (*p).~U();
  }

  template <class U>
  struct rebind {
      using other = LockFreeMemPoolAllocator<U>;
  };
  using value_type = T;
};


template <Alloc<Node> Allocator> 
class List {
  using Traits = std::allocator_traits<Allocator>;
  Node * head;
  Allocator& alloc;

  public:

  List(unsigned n, Allocator& alloc) : head(nullptr), alloc(alloc) {
    for (unsigned i = 0; i < n; i++) {
      Node* node = Traits::allocate(alloc, 1);
      Traits::construct(alloc, node, head, i);
      head = node;
    }
  }

  ~List() {
    while (head) {
      Node* node = head;
      head = head->next;
      Traits::destroy(alloc, node);
    }
  }
};

template <template <typename> typename Allocator>
requires Alloc<Allocator<Node>, Node>
static inline void test(unsigned n, bool local = false) {
  constexpr int threadsNum = 16;
  struct rusage start, finish;
  get_usage(start);
  auto start_mem = getCurrentRSS();
  if (local) {
    hintElemCount = n;
    std::vector<std::jthread> threads;
    threads.reserve(threadsNum);
    for (int i = 0; i < threadsNum; ++i) {
      threads.emplace_back([n](){
        Allocator<Node> allocator;
        auto list = List<Allocator<Node>>(n, allocator);
      });
    }
    for (int i = 0; i < threadsNum; ++i) {
      threads[i].join();
    }
  } else {
    hintElemCount = threadsNum * n;
    Allocator<Node> alloc;
    std::vector<std::jthread> threads;
    threads.reserve(threadsNum);
    for (int i = 0; i < threadsNum; ++i) {
      threads.emplace_back([=, &alloc](){
        auto list = List<Allocator<Node>>(n, alloc);
      });
    }
    for (int i = 0; i < threadsNum; ++i) {
      threads[i].join();
    }
  }
  get_usage(finish);

  struct timeval diff;
  timersub(&finish.ru_utime, &start.ru_utime, &diff);
  uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
  cout << "Time used: " << time_used << " usec\n";

  uint64_t mem_used = finish.ru_maxrss * 1024 - start_mem;
  cout << "Memory used: " << mem_used << " bytes\n";

  auto mem_required = std::min(threadsNum * n * sizeof(Node), mem_used);

  cout << "Mem required: " << mem_required << " bytes\n";
  auto overhead = (mem_used - mem_required) * double(100) / mem_used;
  cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
      << overhead << "%\n";
}


int main(const int, const char* argv[]) {
  struct sigaction sa;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset(&sa.sa_mask);
  sa.sa_sigaction = handler;
  sigaction(SIGSEGV, &sa, NULL);

  constexpr int n = 1'000'000;

  std::string allocator_name = argv[1];
  cout << allocator_name << ":\n";
  if (allocator_name == "Default") {
    test<std::allocator>(n);
  } else if (allocator_name == "MutexedMemPool") {
    test<MutexedMemPoolAllocator>(n);
  } else if (allocator_name == "LockFreeMemPool") {
    test<LockFreeMemPoolAllocator>(n);
  } else if (allocator_name == "LocalMemPool") {
    test<MemPoolAllocator>(n, true);
  }
  return EXIT_SUCCESS;
}
