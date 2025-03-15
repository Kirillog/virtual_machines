#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <memory>
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

using namespace std;

static constexpr size_t PAGE_SIZE = 4096;

static const char str[] = "Got SIGSEGV, probably because of bad allocation at "; 

struct AllocArea {
  char *begin;
  size_t size;
};

static AllocArea areas[3] = {};

static const char *allocName[] = {
  "Default",
  "MemPoolAllocator",
  "MutexedMemPoolAllocator",
  "LockFreeMemPoolAllocator"
};

static void handler(int, siginfo_t *si, void *)
{
  int id = -1;
  char * ptr = (char *)si->si_addr;
  for (int i = 0; i < 3; ++i) {
    if (areas[i].begin < ptr && ptr < areas[i].begin + areas[i].size) {
      id = i;
      break;
    }
  }
  const char * allocator_name = allocName[id + 1];
  write(STDERR_FILENO, str, sizeof(str) - 1);
  write(STDERR_FILENO, allocator_name, std::strlen(allocator_name));
  write(STDERR_FILENO, "\n", 1);
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
  static constexpr int id = 0; 

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  MemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    areas[id] = {mempool_, size};
  }

  ~MemPoolAllocator() {
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
  static constexpr int id = 1; 
  std::mutex m_;

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  MutexedMemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    areas[id] = {mempool_, size};
  }

  ~MutexedMemPoolAllocator() {
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
  static constexpr int id = 2;

public:
  static constexpr unsigned long long defaultSize = 128ll * 1024 * PAGE_SIZE; 

  LockFreeMemPoolAllocator() {
    size = hintElemCount == 0 ? defaultSize : (roundToPageSize(hintElemCount * sizeof(T) + PAGE_SIZE));
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(mempool_, PAGE_SIZE, PROT_NONE);
    ptr = mempool_ + size;
    areas[id] = {mempool_, size};
  }

  ~LockFreeMemPoolAllocator() {
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
      threads.emplace_back([=](){
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

  auto mem_required = threadsNum * n * sizeof(Node);

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
