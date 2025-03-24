#pragma once
#include <ostream>

class StringHandle {
public:
    StringHandle();
    StringHandle(const char *str);
    StringHandle(StringHandle& oth);
    StringHandle(StringHandle&& oth);

    StringHandle& operator=(StringHandle&& oth);
    StringHandle& operator=(StringHandle& oth);
    StringHandle& operator=(const char *str);

    const char *get() const;

    ~StringHandle();

    friend std::ostream& operator<<(std::ostream& out, const StringHandle& a);

    inline int Count() const {
        return !(reinterpret_cast<std::uint64_t>(str) & 1) + 1;
    }

    #ifdef DEBUG
    inline int DeallocCount() const {
        return dealloc_count;
    }
    #endif
private:
    inline void FreeIfRequired() {
        if (OneOwner()) {
            #ifdef DEBUG
            fprintf(stderr, "Free str: %s, count: %i\n", get(), Count());
            ++dealloc_count;
            #endif
            delete[] get();
        }
    }

    inline void IncCount() {
        str = reinterpret_cast<char *>(reinterpret_cast<std::uint64_t>(str) & (-2ull));
    }

    inline bool OneOwner() const {
        return reinterpret_cast<std::uint64_t>(str) & 1;
    }
private:
    char *str{nullptr};
    #ifdef DEBUG
    int dealloc_count{0};
    #endif
};

