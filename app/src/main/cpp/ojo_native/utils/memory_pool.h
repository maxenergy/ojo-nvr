#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include "ojo_types.h"
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

namespace ojo {

/**
 * Memory pool for efficient frame buffer management
 */
class MemoryPool {
public:
    explicit MemoryPool(size_t blockSize, size_t poolSize = 32);
    ~MemoryPool();
    
    // Memory allocation
    void* allocate();
    void deallocate(void* ptr);
    
    // Pool management
    void resize(size_t newPoolSize);
    void clear();
    
    // Statistics
    size_t getBlockSize() const;
    size_t getPoolSize() const;
    size_t getUsedBlocks() const;
    size_t getFreeBlocks() const;
    
private:
    struct Block {
        void* data;
        bool inUse;
        
        Block() : data(nullptr), inUse(false) {}
    };
    
    size_t blockSize_;
    std::vector<Block> blocks_;
    mutable std::mutex poolMutex_;
    std::atomic<size_t> usedBlocks_;
    
    void initializePool(size_t poolSize);
    void cleanupPool();
};

/**
 * Smart pointer for memory pool allocated objects
 */
template<typename T>
class PoolPtr {
public:
    PoolPtr(T* ptr, MemoryPool* pool) : ptr_(ptr), pool_(pool) {}
    ~PoolPtr() {
        if (ptr_ && pool_) {
            pool_->deallocate(ptr_);
        }
    }
    
    // Move constructor
    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_), pool_(other.pool_) {
        other.ptr_ = nullptr;
        other.pool_ = nullptr;
    }
    
    // Move assignment
    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && pool_) {
                pool_->deallocate(ptr_);
            }
            ptr_ = other.ptr_;
            pool_ = other.pool_;
            other.ptr_ = nullptr;
            other.pool_ = nullptr;
        }
        return *this;
    }
    
    // Disable copy
    PoolPtr(const PoolPtr&) = delete;
    PoolPtr& operator=(const PoolPtr&) = delete;
    
    T* get() const { return ptr_; }
    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    
    bool operator==(const PoolPtr& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const PoolPtr& other) const { return ptr_ != other.ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
private:
    T* ptr_;
    MemoryPool* pool_;
};

} // namespace ojo

#endif // MEMORY_POOL_H
