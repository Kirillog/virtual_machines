#include "string_handle.h"
#include <cstring>

StringHandle::StringHandle() : str(nullptr) {
}

StringHandle::StringHandle(const char *str) {
    auto length = std::strlen(str) + 1;
    this->str = new char[length];
    std::memcpy(this->str, str, length);
    Ptr() = Ptr() | 1;
}

StringHandle::StringHandle(StringHandle& oth) {
    oth.IncCount();
    this->str = oth.str;
}

StringHandle::StringHandle(StringHandle&& oth) : str(oth.str) {
    oth.str = nullptr;
}

StringHandle& StringHandle::operator=(StringHandle&& oth) {
    if (&oth != this) {
        FreeIfRequired();
        this->str = oth.str;
        oth.str = nullptr;
    }
    return *this;
}

StringHandle& StringHandle::operator=(StringHandle& oth) {
    if (&oth != this) {
        FreeIfRequired();
        oth.IncCount();
        this->str = oth.str;
    }
    return *this;
}

StringHandle& StringHandle::operator=(const char *str) {
    FreeIfRequired();
    auto length = std::strlen(str) + 1;
    this->str = new char[length];
    std::memcpy(this->str, str, length);
    Ptr() = Ptr() | 1;
    return *this;
}

const char *StringHandle::get() const {
    return reinterpret_cast<const char *>(Ptr() & (-2ul));
}

std::ostream& operator<<(std::ostream& out, const StringHandle& a) {
    return out << "Content: " << a.get() << "\n" 
                << "Count: " << a.Count() << "\n";
}

StringHandle::~StringHandle() {
    FreeIfRequired();    
}
