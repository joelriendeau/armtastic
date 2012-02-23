#pragma once

#include "armtastic/types.hpp"
#include <string.h>

template <typename data_t, u32 size, typename pointer_t = data_t*>
class linear_buffer
{
public:
    linear_buffer()
    {
        clear();
    }

    void clear()
    {
        write_pos = read_pos = buf;
        #if INIT_BUFFER_TO_0
            memset(buf, 0, size * sizeof(data_t));
        #endif
    }

    pointer_t get(u32 offset = 0)
    {
        return buf + offset;
    }

    u32 awaiting()
    {
        return write_pos - read_pos;
    }

    u32 free()
    {
        return size - awaiting();
    }

    bool write(data_t* data)
    {
        u32 free_slots = free();

        if (free_slots)
        {
            if (data)
            {
                *write_pos = *data;
                write_pos += 1;
                if (free_slots > 1)
                    return true;
            }
            else
                return true;
        }

        return false;
    }

    // mainly intended for filling the buffer while interrupts disabled, must be fast
    void fast_write(data_t& data)
    {
        *write_pos = data;
        write_pos += 1;
    }

    bool write_buffer(const data_t* buffer, u32 count)
    {
        if (count > free())
            return false;

        memcpy((void*)write_pos, buffer, count * sizeof(data_t));
        write_pos += count;
        
        return true;
    }

    bool read(data_t* data)
    {
        u32 awaiting_slots = awaiting();
        
        if (awaiting_slots)
        {
            if (data)
            {
                *data = *read_pos;
                read_pos += 1;
                if (awaiting_slots > 1)
                    return true;
            }
            else
                return true;
        }

        return false;
    }

    // mainly intended for interrogating the buffer while interrupts disabled, must be fast
    bool fast_read(data_t& data)
    {
        // don't check to see if a byte is awaiting, the caller knows through alternate ways (otherwise, this call is not safe to use)
        data = *read_pos;
        read_pos += 1;
        return awaiting();
    }

    bool read_buffer(data_t* buffer, u32 count, bool peek = false)
    {
        if (count > awaiting())
            return false;

        memcpy(buffer, read_pos, count * sizeof(data_t));
        if (!peek)
            read_pos += count;

        return true;
    }

private:
    pointer_t write_pos;
    pointer_t read_pos;
    data_t buf[size];
};