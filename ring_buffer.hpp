#pragma once

#include "armtastic/types.hpp"
#include "assert.h"
#include <string.h>

template <typename data_t, typename pointer_t = data_t*>
class ring_buffer_base
{
public:
    ring_buffer_base(data_t* buffer = 0, u32 buffer_size = 0) : buf(buffer), size(buffer_size)
    {
        clear();
    }

    void init(data_t* buffer = 0, u32 buffer_size = 0)
    {
        assert(buffer);
        assert(buffer_size);
        buf = buffer;
        size = buffer_size;
        clear();
    }

    void clear()
    {
        write_pos = read_pos = buf;
        if (buf)
            memset(buf, 0, size * sizeof(data_t));
    }

    u32 awaiting()
    {
        return ring_diff(write_pos, read_pos);
    }

    u32 free()
    {
        return size - 1 - awaiting(); // don't allow writing a full buffer, it will look like an empty buffer with the pointer arithmetic
    }

    bool write(const data_t* data) // returns true if there is more space left
    {
        u32 free_slots = free();

        if (free_slots)
        {
            if (data)
            {
                *write_pos = *data;
                write_pos = wrap(write_pos + 1);
                if (free_slots > 1)
                    return true;
            }
            else
                return true;
        }

        return false;
    }

    bool write_test(const data_t* data) // returns true if space was available for writing
    {
        u32 free_slots = free();

        if (free_slots)
        {
            if (data)
            {
                *write_pos = *data;
                write_pos = wrap(write_pos + 1);
            }
            return true;
        }

        return false;
    }

    // mainly intended for filling the buffer for software interrupts
    void fast_write(const data_t& data)
    {
        *write_pos = data;
        write_pos = wrap(write_pos + 1);
    }

    bool write_buffer(const data_t* buffer, u32 count, bool all_or_nothing = true, u32* written = 0)
    {
        u32 free_slots = free();

        if (count > free_slots)
        {
            if (all_or_nothing || !written)
                return false;
            *written = free_slots;
            count = free_slots;
        }
        else if (written)
            *written = count;

        u32 size_left_until_wrap = buf + size - write_pos;

        if (size_left_until_wrap >= count)
        {
            memcpy((void*)write_pos, buffer, count * sizeof(data_t));
            write_pos = wrap(write_pos + count);
        }
        else
        {
            memcpy((void*)write_pos, buffer, size_left_until_wrap * sizeof(data_t));
            memcpy(buf, buffer + size_left_until_wrap, (count - size_left_until_wrap) * sizeof(data_t));
            write_pos = buf + count - size_left_until_wrap;
        }

        return true;
    }

    data_t* get_write_pointer()
    {
        return const_cast<data_t*>(write_pos);
    }

    bool advance_write_pointer(u32 count)
    {
        if (count <= free())
        {
            write_pos = wrap(write_pos + count);
            return true;
        }
        return false;
    }

    bool read(data_t* data, bool peek = false) // returns true if there is more data left
    {
        u32 awaiting_slots = awaiting();
        
        if (awaiting_slots)
        {
            if (data)
            {
                *data = *read_pos;
                if (!peek)
                    read_pos = wrap(read_pos + 1);
                if (awaiting_slots > 1 || (peek && awaiting_slots))
                    return true;
            }
            else
                return true;
        }

        return false;
    }

    bool read_test(data_t* data, bool peek = false) // returns true if data was available
    {
        u32 awaiting_slots = awaiting();
        
        if (awaiting_slots)
        {
            if (data)
            {
                *data = *read_pos;
                if (!peek)
                    read_pos = wrap(read_pos + 1);
            }
            return true;
        }

        return false;
    }

    // mainly intended for reading the buffer for software interrupts
    bool fast_read(u8& data)
    {
        // don't check to see if a byte is awaiting, the caller has another way of knowing (otherwise, this call is not safe to use)
        data = *read_pos;
        read_pos = wrap(read_pos + 1);
        return awaiting();
    }

    bool read_buffer(data_t* buffer, u32 count, bool peek = false)
    {
        if (count > awaiting())
            return false;

        u32 size_left_until_wrap = buf + size - read_pos;

        if (size_left_until_wrap >= count)
        {
            memcpy(buffer, reinterpret_cast<u8*>(const_cast<data_t*>(read_pos)), count * sizeof(data_t));
            if (!peek)
                read_pos = wrap(read_pos + count);
        }
        else
        {
            memcpy(buffer, reinterpret_cast<u8*>(const_cast<data_t*>(read_pos)), size_left_until_wrap);
            memcpy(buffer + size_left_until_wrap, buf, (count - size_left_until_wrap) * sizeof(data_t));
            if (!peek)
                read_pos = buf + count - size_left_until_wrap;
        }

        return true;
    }

    data_t* get_read_pointer()
    {
        return const_cast<data_t*>(read_pos);
    }

    bool advance_read_pointer(u32 count)
    {
        if (count <= awaiting())
        {
            read_pos = wrap(read_pos + count);
            return true;
        }
        return false;
    }

private:
    u32 ring_diff(pointer_t a, pointer_t b)
    {
        // if a == b, this function acts as if the buffer is empty
        return (a >= b) ? a - b : size + a - b;
    }
    pointer_t wrap(pointer_t pos)
    {
        if (pos >= buf + size)
            return pos - size;
        return pos;
    }

    pointer_t write_pos;
    pointer_t read_pos;
    data_t* buf;
    u32 size;
};

template <typename data_t, u32 size, typename pointer_t = data_t*>
class ring_buffer : public ring_buffer_base<data_t, pointer_t>
{
public:
    ring_buffer() : ring_buffer_base<data_t, pointer_t>(buffer, size) {}

private:
    data_t buffer[size];
};
