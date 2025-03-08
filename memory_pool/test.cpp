#include <cerrno>
#include <iostream>
#include <iomanip>
#include <new>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <concepts>
#include <unistd.h>

using namespace std;

static constexpr size_t PAGE_SIZE = 1024;

/**
 * Returns the current resident set size (physical memory use) measured
 * in bytes, or zero if the value cannot be determined on this OS.
 * https://stackoverflow.com/questions/669438/how-to-get-memory-usage-at-runtime-using-c
 */
 size_t getCurrentRSS( )
 {
  long rss = 0L;
  FILE* fp = NULL;
  if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL ) {
    return (size_t)0L;
  }
  if ( fscanf( fp, "%*s%ld", &rss ) != 1 ) {
    fclose( fp );
    return (size_t)0L;
  }
  fclose( fp );
  return (size_t)rss * PAGE_SIZE;
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

template <typename T>
class MemPoolAllocator : public std::allocator<T> {
private:
  char *mempool_, *ptr;

public:
  static constexpr unsigned long long size = 40ll * 1024 * 4096; 

  MemPoolAllocator() {
    mempool_ = (char *) mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ptr = mempool_ + size;
  }

  ~MemPoolAllocator() {
    munmap(mempool_, size);
  }

  MemPoolAllocator(const MemPoolAllocator&) = default;
  MemPoolAllocator& operator=(const MemPoolAllocator&) = default;

  template <class U>
  explicit MemPoolAllocator(const MemPoolAllocator<U>& other) {}

  T *allocate(size_t count) {
    if (ptr - sizeof(T) * count < mempool_) {
      throw std::bad_alloc();
    }
    return (T*)(ptr -= sizeof(T) * count);
  }

  void deallocate(T* ptr, size_t) {
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

template <typename T = Node, Alloc<T> Allocator = std::allocator<T>> 
class List {
  using Traits = std::allocator_traits<Allocator>;
  Node * head;
  Allocator alloc;

  List(unsigned n) : head(nullptr), alloc(Allocator()) {
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

public:
  static inline void test(unsigned n) {
    struct rusage start, finish;
    auto start_mem = getCurrentRSS();
    get_usage(start);
    {
      auto list = List(n);
    }
    get_usage(finish);

    struct timeval diff;
    timersub(&finish.ru_utime, &start.ru_utime, &diff);
    uint64_t time_used = diff.tv_sec * 1000000 + diff.tv_usec;
    cout << "Time used: " << time_used << " usec\n";

    uint64_t mem_used = finish.ru_maxrss * 1024 - start_mem;
    cout << "Memory used: " << mem_used << " bytes\n";

    auto mem_required = n * sizeof(Node);

    cout << "Mem required: " << mem_required << " bytes\n";
    auto overhead = (mem_used - mem_required) * double(100) / mem_used;
    cout << "Overhead: " << std::fixed << std::setw(4) << std::setprecision(1)
        << overhead << "%\n";
  }

};

int main(const int argc, const char* argv[]) {
  std::string arg = argv[1];
  cout << arg << ":\n";
  if (arg == "Default") {
    List<>::test(10000000);
  } else if (arg == "MemPool") {
    List<Node, MemPoolAllocator<Node>>::test(10000000);
  }
  return EXIT_SUCCESS;
}
