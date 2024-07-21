#include <stdint.h>
#include <vector>
#include <numeric>
#include <assert.h>
#include <iostream>

template <typename T>
struct ResourcePool {

    void Initialize(uint32_t size, std::string name = "") {
        _size = size;
        _name = name;

        _freeLists.resize(size);
        _resources.resize(size);
        std::iota(_freeLists.rbegin(), _freeLists.rend(), 0);
    }

    uint64_t Obtain() {
        assert(_freeLists.size() > 0);
        uint64_t index = _freeLists.back();
        _freeLists.pop_back();
        return index;
    }

    T *Access(uint64_t index) {
        assert(index < _size);
        return &_resources[index];
    }

    void Release(uint64_t index) {
        assert(index < _size);
        _freeLists.push_back(index);
    }

    void Shutdown() {
        if (_freeLists.size() < _size) {
            std::string message = _name + "[ResourcePool] has unfreed resources";
            LOG(message)
        }
        _resources.clear();
        _freeLists.clear();
    }

    std::string _name;
    uint32_t _size;
    std::vector<uint64_t> _freeLists;
    std::vector<T> _resources;
};