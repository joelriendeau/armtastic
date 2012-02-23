#pragma once

#include "linear_buffer.hpp"
#include <string.h>

template <typename data_t, u32 max_size>
class list
{
public:
    list() : current_size(0)
    {
        for (u32 p = 0; p < max_size; p++)
            free_list[p] = p;
    }

    u32 size()
    {
        return current_size;
    }

    bool full()
    {
        return current_size == max_size;
    }

    data_t* get(u32 pos)
    {
        if (pos < current_size)
        {
            return content.get(order[pos]);
        }
        return 0;
    }

    data_t* insert(u32 pos)
    {
        if (current_size == max_size)
            return 0;
		if (pos > current_size)
			return 0;
        u32 next = free_list[current_size];
        for (u32 p = current_size; p > pos; --p)
            order[p] = order[p - 1];
        order[pos] = next;
        current_size++;
        return content.get(next);
    }

    data_t* append()
    {
        return insert(current_size);
    }
    
    void free(u32 pos)
    {
        if (pos >= current_size) return;        
        --current_size;
        free_list[current_size] = pos;
        for (u32 p = pos; p < current_size; ++p)
            order[p] = order[p + 1];
    }

    void free(data_t* data)
    {
        if (!data) return;
        u32 pos = data - content.get(0);
        free(pos);
    }

private:
    u32 order[max_size]; // order to visit the objects in
    u32 free_list[max_size]; // maintains the list of free offsets
    u32 current_size;
    linear_buffer<data_t, max_size> content;
};