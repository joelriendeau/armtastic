#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include "support/types.hpp"
#include "support/singletons.hpp"
#include "modules/platf_abs_layer/debug.hpp"
#include "modules/platf_abs_layer/system.hpp"

namespace memory_pool {

// this global info is needed in order to have the memory allocator alive even before static objects get constructed.
extern u8*          frag_buffer;
extern volatile u8* next;
extern bool         released;
extern u32*         block_counts;
extern u32          pool_count;
extern bool         debug;

// canonical fixed-sized block allocator
class block_pool
{
public:
    block_pool() : free_count(0), block_count(0), next_block(0), buffer(0) {}
    ~block_pool() {if (buffer) delete [] buffer;}

    void init(u32 block_size, u32 block_count)
    {
        this->block_count = block_count;
        buffer = reinterpret_cast<u32*>(new (sys::pool_type::fixed) u8[block_size * block_count]);
        end_buffer = reinterpret_cast<u32*>(reinterpret_cast<u8*>(buffer) + (block_size * block_count));

        for (u32 block = 0; block < (block_count - 1); ++block) // initialize each block to the next block's address
            buffer[block * (block_size >> 2)] = reinterpret_cast<u32>(&buffer[(block + 1) * (block_size >> 2)]);
        next_block = buffer;
        free_count = block_count;
    }

    void release()
    {
        delete [] buffer;
    }

    void* alloc()
    {
        if (free_count <= 0)
            return 0;
        void* mem = next_block;
        next_block = reinterpret_cast<u32*>(*next_block);
        --free_count;
        return mem;
    }

    void dealloc(void* p)
    {
        assertion(free_count < block_count);
        *reinterpret_cast<u32*>(p) = reinterpret_cast<u32>(next_block);
        next_block = reinterpret_cast<u32*>(p);
        ++free_count;
    }

    bool in_range(void* p)
    {
        return (p >= buffer && p < end_buffer);
    }

private:
    u32  block_count;
    u32  free_count;
    u32* next_block;
    u32* buffer;
    u32* end_buffer;
};

// a set of memory pools with logarithmically increasing block size
class pool_set : public static_singleton<pool_set>
{
public:
    pool_set() : pool_count(0), pools(0) {}
    ~pool_set() {if (pools) delete [] pools;}

    void init(u32* block_counts, u32 pool_count)
    {
        u32 block_size = 4;
        this->pool_count = pool_count;
        pools = new (sys::pool_type::fixed) block_pool[pool_count];
        for (u32 p = 0; p < pool_count; p++)
        {
            pools[p].init(block_size, block_counts[p]);
            block_size <<= 1;
        }
    }

    void release()
    {
        for (u32 p = 0; p < pool_count; p++)
            pools[p].release();
        delete [] pools;
    }

    void* alloc(std::size_t size)
    {
        void* p = 0;
        size = (size + 3) & ~3;
        u32 pool_index = integer_log(size) - 2;
        if (pool_index < pool_count)
            p = pools[pool_index].alloc();
        return p;
    }

    bool dealloc(void* p)
    {
        for (u32 pool = 0; pool < pool_count; pool++)
        {
            if (pools[pool].in_range(p))
            {
                pools[pool].dealloc(p);
                return true;
            }
        }
        return false;
    }

private:
    u32 integer_log(u32 x)
    {
        s32 l = (x & (x - 1));

        l |= -l;
        l >>= 31;
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);

        return (bits_set(x >> 1) - l);
    }

    u32 bits_set(u32 x)
    {
        x -= ((x >> 1) & 0x55555555);
        x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
        x = (((x >> 4) + x) & 0x0f0f0f0f);
        x += (x >> 8);
        x += (x >> 16);

        return (x & 0x3f);
    }

    u32 pool_count;
    block_pool* pools;
};

// a pool allocating inside a global buffer and also using the pool set and the heap, if requested to
template <std::size_t PoolSize>
class fixed_pool : public static_singleton<fixed_pool<PoolSize> >
{
public:
    void init()
    {
        pool_set::get().init(block_counts, pool_count);
    }

    void release()
    {
        released = true;
        pool_set::get().release();
    }

    void* alloc(std::size_t bytes, sys::pool_type::en type)
    {
        bool heap_fallback = false;
        void* p = 0;
        std::size_t alloc_bytes = (bytes + 3) & ~3;

        if (sys::pool_type::logarithmic == type)
            p = pool_set::get().alloc(alloc_bytes);
        else if (sys::pool_type::fixed == type && alloc_bytes <= PoolSize)
        {
            volatile u8* cur_next, * next_next;
            do // atomic compare-and-swap makes this operation thread safe
            {  // when paired with a retry do-while
                cur_next = next;
                next_next = cur_next + alloc_bytes;
                if (next_next > frag_buffer + PoolSize)
                    break;
                if (sys::cas(reinterpret_cast<s32*>(&next),
                             reinterpret_cast<s32>(cur_next),
                             reinterpret_cast<s32>(next_next)))
                    p = const_cast<void*>(reinterpret_cast<volatile void*>(cur_next));                         
            } while (0 == p);
        }

        if (0 == p)
            heap_fallback = true;

        if (sys::pool_type::heap == type || heap_fallback)
            p = malloc(alloc_bytes);

        if (debug)
        {
            sys::log(sys::log_type::trace,
                     sys::log_origin::memory,
                     "Alloc type \"%s\" size %5db  0x%08x %s %s",
                     (sys::pool_type::logarithmic == type) ? " Log " : (sys::pool_type::fixed == type) ? "Fixed" : "Heap ",
                     bytes,
                     p,
                     (p != 0) ? "succeeded" : "failed",
                     (heap_fallback) ? "(fallback on heap)" : "");
        }

        return p;
    }

    void dealloc(void* p)
    {
        if (p >= frag_buffer && p < frag_buffer + PoolSize)
        {
            // memory is inside the boundaries of the fixed pool. it may be inside the log pools as well.
            if (!released) // only look into the logarithmic pools if they are not released
                if (pool_set::get().dealloc(p))
                    return;
        }
        else
            free(p);
    }
};

} // namespace memory_pool

#endif // MEMORY_POOL_HPP