#pragma once

#include "armtastic/types.hpp"
#include <string.h>

template <typename data_t, u8 size>
class pool
{
public:
    pool()
    {
        clear();
    }

    void clear()
    {
        free_list_size = 255;
        for (u16 i = 0; i < 256; i++)
            free_list[i] = i;
    }

    u32 free()
    {
        return free_list_size;
    }

    u8 alloc()
    {
        if (!free())
            return 0;
        u8 next = free_list[free_list_index];
        --free_list_index;
        return next;
    }

    void release(u8 index)
    {
        if (free_list_index >= 255)
            return;
        ++free_list_index;
        free_list[free_list_index] = index;
    }

    data_t* get(u8 index)
    {
        return buf + index;
    }

private:
    u8 free_list[size];
    u8 free_list_index;
    data_t buf[size];
};