#include "vector.h"

#include <iostream>
#include <stdexcept>
#include <cassert>

using namespace std;

constexpr std::size_t SIZE = 8u;
constexpr int MAGIC = 42;
constexpr uint32_t DEFAULT_COOKIE = 0xdeadbeef;

struct ThrowObj {
    ThrowObj() {
        if (default_construction_throw_countdown > 0) {
            if (--default_construction_throw_countdown == 0) {
                throw std::runtime_error("Oops");
            }
        }
    };
    ThrowObj(const int&) {
        if (throw_on_copy_val) {
            throw std::runtime_error("Oops");
        }
    };
    ThrowObj(const ThrowObj&) {
        if (throw_on_copy) {
            throw std::runtime_error("Oops");
        }
    };
    ThrowObj& operator=(const ThrowObj&) {
        if (throw_on_copy) {
            throw std::runtime_error("Oops");
        }
        return *this;
    }
    ThrowObj(int&&) {}
    ThrowObj(ThrowObj&& other) = default;
    ThrowObj& operator=(ThrowObj&& other) = default;
    ~ThrowObj() {
        cookie = 0;
    }

    [[nodiscard]] bool IsAlive() const noexcept {
        return cookie == DEFAULT_COOKIE;
    }

    uint32_t cookie = DEFAULT_COOKIE;

    static inline size_t default_construction_throw_countdown = SIZE;
    static inline bool throw_on_copy = false;
    static inline bool throw_on_copy_val = false;
};

void TestPushBackStrongException() {
    ThrowObj::default_construction_throw_countdown = -1;
    {
        ThrowObj a;
        Vector<ThrowObj> v(1);
        ThrowObj::throw_on_copy = true;
        try {
            v.PushBack(a);
            assert(false);
        }
        catch (const std::runtime_error& /*except*/) {
            // этого и ждем
        }

        assert(v[0].IsAlive());
        assert(v.Size() == 1u);
        assert(v.Capacity() == 1u);
    } {
        ThrowObj a;
        Vector<ThrowObj> v(1);
        v.Reserve(2);
        ThrowObj::throw_on_copy = true;
        try {
            v.PushBack(a);
            assert(false);
        }
        catch (const std::runtime_error& /*except*/) {
            // этого и ждем
        }

        assert(v[0].IsAlive());
        assert(v.Size() == 1u);
        assert(v.Capacity() == 2u);
    } {
        ThrowObj a;
        Vector<ThrowObj> v(1);
        ThrowObj::throw_on_copy = true;
        try {
            v.PushBack(std::move(a));
        }
        catch (const std::exception& /*except*/) {
            assert(false);
        }

        assert(v[0].IsAlive());
        assert(v[1].IsAlive());
        assert(v.Size() == 2u);
        assert(v.Capacity() == 2u);
    } {
        ThrowObj a;
        Vector<ThrowObj> v(1);
        ThrowObj::throw_on_copy = true;
        try {
            v.PushBack(v[0]);
            assert(false);
        }
        catch (const std::runtime_error& /*except*/) {
            // этого и ждем
        }

        assert(v[0].IsAlive());
        assert(v.Size() == 1u);
        assert(v.Capacity() == 1u);
    } {
        ThrowObj a;
        Vector<ThrowObj> v(1);
        v.Reserve(2);
        ThrowObj::throw_on_copy = true;
        try {
            v.PushBack(v[0]);
            assert(false);
        }
        catch (const std::runtime_error& /*except*/) {
            // этого и ждем
        }

        assert(v[0].IsAlive());
        assert(v.Size() == 1u);
        assert(v.Capacity() == 2u);
    }
    cerr << "Test passed"s << endl;
}

int main() {
    TestPushBackStrongException();
    return 0;
}
