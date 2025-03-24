#pragma once
#include <ostream>
#include <cstring>

class StringHandle {
public:
    StringHandle() : str(nullptr) {}

    StringHandle(const char *str) {
        ConstructFromCString(str);
    }

    StringHandle(StringHandle& oth) {
        oth.IncCount();
        this->str = oth.str;
    }
    
    StringHandle(StringHandle&& oth) : str(oth.str) {
        oth.str = nullptr;
    }

    StringHandle& operator=(StringHandle&& oth);
    StringHandle& operator=(StringHandle& oth);
    StringHandle& operator=(const char *str);

    const char *get() const {
        return reinterpret_cast<const char *>(Ptr() & ALIGNED_PTR_MASK);
    }

    ~StringHandle() {
        FreeIfRequired();    
    }

    friend std::ostream& operator<<(std::ostream& out, const StringHandle& a);

    int Count() const {
        return !(Ptr() & 1) + 1;
    }

    #ifdef DEBUG
    int DeallocCount() const {
        return dealloc_count;
    }
    #endif
private:
    void ConstructFromCString(const char *str) {
        if (str == nullptr) {
            Ptr() = 0;
            return;
        }
        auto length = std::strlen(str) + 1;
        this->str = new char[length];
        std::memcpy(this->str, str, length);
        Ptr() = Ptr() | 1;
    }

    void FreeIfRequired() {
        if (OneOwner()) {
            #ifdef DEBUG
            fprintf(stderr, "Free str: %s, count: %i\n", get(), Count());
            ++dealloc_count;
            #endif
            delete[] get();
        }
    }

    std::uintptr_t& Ptr() {
        return reinterpret_cast<std::uintptr_t&>(str);
    }

    std::uintptr_t Ptr() const {
        return reinterpret_cast<std::uintptr_t>(str);
    }

    void IncCount() {
        Ptr() &= ALIGNED_PTR_MASK;
    }

    bool OneOwner() const {
        return Ptr() & 1;
    }

    static constexpr uintptr_t ALIGNED_PTR_MASK = -2ul;

private:
    char *str{nullptr};
    #ifdef DEBUG
    int dealloc_count{0};
    #endif
};

