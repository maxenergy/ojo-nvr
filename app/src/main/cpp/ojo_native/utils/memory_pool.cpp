#include "memory_pool.h"
#include <android/log.h>
#include <cstdlib>
#include <cstring>
#include <stdlib.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "OjoMemoryPool"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace ojo {

MemoryPool::MemoryPool(size_t blockSize, size_t poolSize)
    : blockSize_(blockSize), usedBlocks_(0) {
    
    LOGI("Creating memory pool: blockSize=%zu, poolSize=%zu", blockSize, poolSize);
    
    initializePool(poolSize);
    
    LOGI("Memory pool created successfully");
}

MemoryPool::~MemoryPool() {
    LOGI("Destroying memory pool");
    cleanupPool();
}

void* MemoryPool::allocate() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Find a free block
    for (auto& block : blocks_) {
        if (!block.inUse) {
            block.inUse = true;
            usedBlocks_++;
            return block.data;
        }
    }
    
    // No free blocks available
    LOGW("Memory pool exhausted, allocating from heap");
    return std::malloc(blockSize_);
}

void MemoryPool::deallocate(void* ptr) {
    if (!ptr) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    // Check if pointer belongs to pool
    for (auto& block : blocks_) {
        if (block.data == ptr) {
            if (block.inUse) {
                block.inUse = false;
                usedBlocks_--;
                return;
            } else {
                LOGW("Double free detected in memory pool");
                return;
            }
        }
    }
    
    // Pointer not from pool, free normally
    std::free(ptr);
}

void MemoryPool::resize(size_t newPoolSize) {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    LOGI("Resizing memory pool from %zu to %zu blocks", blocks_.size(), newPoolSize);
    
    if (newPoolSize > blocks_.size()) {
        // Expand pool
        size_t oldSize = blocks_.size();
        blocks_.resize(newPoolSize);
        
        for (size_t i = oldSize; i < newPoolSize; ++i) {
            void* ptr = nullptr;
            if (posix_memalign(&ptr, 32, blockSize_) != 0) {
                ptr = nullptr;
            }
            blocks_[i].data = ptr;
            blocks_[i].inUse = false;
            
            if (!blocks_[i].data) {
                LOGE("Failed to allocate memory for pool block %zu", i);
                blocks_.resize(i);
                break;
            }
        }
    } else if (newPoolSize < blocks_.size()) {
        // Shrink pool - only remove unused blocks
        for (size_t i = blocks_.size() - 1; i >= newPoolSize; --i) {
            if (!blocks_[i].inUse) {
                std::free(blocks_[i].data);
                blocks_.pop_back();
            } else {
                LOGW("Cannot shrink pool - block %zu is in use", i);
                break;
            }
        }
    }
    
    LOGI("Memory pool resized to %zu blocks", blocks_.size());
}

void MemoryPool::clear() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    LOGI("Clearing memory pool");
    
    // Mark all blocks as free
    for (auto& block : blocks_) {
        block.inUse = false;
    }
    
    usedBlocks_ = 0;
    
    LOGI("Memory pool cleared");
}

size_t MemoryPool::getBlockSize() const {
    return blockSize_;
}

size_t MemoryPool::getPoolSize() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return blocks_.size();
}

size_t MemoryPool::getUsedBlocks() const {
    return usedBlocks_;
}

size_t MemoryPool::getFreeBlocks() const {
    std::lock_guard<std::mutex> lock(poolMutex_);
    return blocks_.size() - usedBlocks_;
}

void MemoryPool::initializePool(size_t poolSize) {
    blocks_.reserve(poolSize);
    
    for (size_t i = 0; i < poolSize; ++i) {
        Block block;
        void* ptr = nullptr;
        if (posix_memalign(&ptr, 32, blockSize_) != 0) {
            ptr = nullptr;
        }
        block.data = ptr;
        block.inUse = false;
        
        if (!block.data) {
            LOGE("Failed to allocate memory for pool block %zu", i);
            break;
        }
        
        blocks_.push_back(block);
    }
    
    LOGI("Initialized memory pool with %zu blocks", blocks_.size());
}

void MemoryPool::cleanupPool() {
    std::lock_guard<std::mutex> lock(poolMutex_);
    
    for (auto& block : blocks_) {
        if (block.data) {
            std::free(block.data);
            block.data = nullptr;
        }
    }
    
    blocks_.clear();
    usedBlocks_ = 0;
    
    LOGI("Memory pool cleanup completed");
}

} // namespace ojo
