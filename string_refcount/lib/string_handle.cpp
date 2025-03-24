#include "string_handle.h"

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
    ConstructFromCString(str);
    return *this;
}

std::ostream& operator<<(std::ostream& out, const StringHandle& a) {
    return out << "Content: " << a.get() << "\n" 
                << "Count: " << a.Count() << "\n";
}
