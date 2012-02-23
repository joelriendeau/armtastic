#pragma once

#include "types.hpp"
#include "boost/static_assert.hpp"

namespace armtastic {

namespace armtastic_internal {

// A template to generate a mask in the amount of bits specified
template <u32 Bits> struct mask
{
    typedef typename integer_type_from_bits<Bits, false>::type mask_type;
    static const u32 impl_bits = bits_from_integer_type<mask_type>::bits;
    static const mask_type bits = (mask_type)(-1) >> (impl_bits - Bits);
};

// The default template is designed to forward calls to the implementation
template <typename Impl, bool CacheEnabled>
class register_cache : public Impl
{
protected:
    typename Impl::data_type read_cache() {return this->read_impl();}
    void write_cache(typename Impl::data_type bits) {this->write_impl(bits);}
    void init_cache_only(typename Impl::data_type bits) {}
};

// While the "cached" template holds a memory cache of the register content.
template <typename Impl>
class register_cache<Impl, true> : public Impl
{
protected:
    typename Impl::data_type read_cache() {return cache;}
    void write_cache(typename Impl::data_type bits)
    {
        cache = bits;
        this->write_impl(bits);
    }
    void init_cache_only(typename Impl::data_type bits) {cache = bits;}
private:
    typename Impl::data_type cache;
};

template <u8 From, u8 To>
struct bit_range_from_to
{
    static const u8 start = To;
    static const u8 size = From - To + 1;
};

template <u8 From, u8 To>
struct bit_range
{
    typedef typename boost::mpl::if_c<(From > To), bit_range_from_to<From, To>, bit_range_from_to<To, From> >::type type;
};

} // namespace armtastic_internal

template <class Impl, bool Cached, typename Impl::data_type Readable, typename Impl::data_type Writable, typename Impl::data_type CacheInit = 0>
class base_register : public armtastic_internal::register_cache<Impl, Cached>
{
public:
    typedef base_register<Impl, Cached, Readable, Writable, CacheInit> type;
    typedef typename Impl::data_type data_type;

    // Default constructor
    base_register() {init_cache_only(CacheInit);}
    // This constructor is valid only for the 'forward_register' implementation type.
    // Do not use it with any other implementation
    template <typename InstanceType>
    base_register(InstanceType& inst) {init_cache_only(CacheInit); set_instance(&inst);}

    #ifdef __GNUC__ // don't know any version where this works
        // Patch for GCC versions (at least) 4.1.1, 4.4.0 (and no doubt other versions will probably need this patch)
        // Otherwise, compiler bugs prevent compilation of the register_manipulator
        static const bool cached_patch = Cached;
        static const typename Impl::data_type readable_patch = Readable;
        static const typename Impl::data_type writable_patch = Writable;
    #endif

private:
    // Do not allow fully unwritable _and_ unreadable registers
    BOOST_STATIC_ASSERT(!(0 == Readable && 0 == Writable));

    static const u8 bit_count = bits_from_integer_type<data_type>::bits;

    template <u8 From, u8 Size> struct access
    {
        static const bool is_readable = (((armtastic_internal::mask<Size>::bits << From) | Readable) == Readable) || Cached;
        static const bool is_writable = (((armtastic_internal::mask<Size>::bits << From) | Writable) == Writable);
    };

    type& operator=(type& bits); // remove default assignment operator so compiler does not attempt to use it instead of the official one

public:
    void set(data_type bits)
    {
        this->write(this->read() | bits);
    }
    template <u8 From, u8 Size>
    void set(data_type bits)
    {
        this->write<From, Size>(this->read<From, Size>() | bits);
    }
    void clear(data_type bits)
    {
        this->write(this->read() & ~bits);
    }
    template <u8 From, u8 Size>
    void clear(data_type bits)
    {
        this->write<From, Size>(this->read<From, Size>() & ~bits);
    }
    void toggle(data_type bits)
    {
        this->write(this->read() ^ bits);
    }
    template <u8 From, u8 Size>
    void toggle(data_type bits)
    {
        this->write<From, Size>(this->read<From, Size>() ^ bits);
    }
    type& operator=(data_type bits)
    {
        write(bits);
        return *this;
    }
    void write(data_type bits)
    {
        // Can only write to register (or cache) if the register is writable
        BOOST_STATIC_ASSERT(Writable);
        this->write_cache(bits & Writable);
    }
    operator data_type()
    {
        return read();
    }
    data_type read()
    {
        // Can only read from register (or cache) if the register has at least 1 readable bit readable or is cached
        BOOST_STATIC_ASSERT(Readable || Cached);
        return this->read_cache();
    }
    template <u8 From, u8 To>
    void write(data_type bits)
    {
        // Can only write to register if the register is writable
        const u8 start = armtastic_internal::bit_range<From, To>::type::start;
        const u8 size = armtastic_internal::bit_range<From, To>::type::size;
        BOOST_STATIC_ASSERT((access<start, size>::is_writable));
        this->write_cache((this->read_cache() & ~(armtastic_internal::mask<size>::bits << start)) | ((bits & armtastic_internal::mask<size>::bits) << start));
    }
    template <u8 From, u8 To>
    data_type read()
    {
        const u8 start = armtastic_internal::bit_range<From, To>::type::start;
        const u8 size = armtastic_internal::bit_range<From, To>::type::size;
        // Can only read from register (or cache) if the register is readable or cached
        BOOST_STATIC_ASSERT((access<start, size>::is_readable));
        return (this->read_cache() >> start) & armtastic_internal::mask<size>::bits;
    }

    #ifdef __GNUC__ // those methods replace templated write and read methods since GCC cannot call them from the register_manipulator
        void write_unchecked_gcc(data_type bits)
        {
            this->write_cache(bits);
        }
        data_type read_unchecked_gcc()
        {
            return this->read_cache();
        }
    #endif
};

template <typename RegisterType, u32 From, u8 To = From>
class register_manipulator
{
private:
    RegisterType& reg;

    static const u8 start = armtastic_internal::bit_range<From, To>::type::start;
    static const u8 size = armtastic_internal::bit_range<From, To>::type::size;

public:
    typedef typename RegisterType::data_type data_type;

    register_manipulator(RegisterType& reg) : reg(reg) {}

    void set(data_type bits)
    {
        reg.set<From, To>(bits);
    }
    void clear(data_type bits)
    {
        reg.clear<From, To>(bits);
    }
    void toggle(data_type bits)
    {
        reg.toggle<From, To>(bits);
    }
    register_manipulator& operator=(data_type bits)
    {
        write(bits);
        return *this;
    }
    register_manipulator& operator=(register_manipulator& manip)
    {
        write(manip.read());
        return *this;
    }
    void write(data_type bits)
    {
        #ifdef __GNUC__ // don't know any version where this works
            // using m_Reg.read<From, To>(); triggers a compiler bug on GCC versions (at least) 4.1.1, 4.4.0
            BOOST_STATIC_ASSERT((((armtastic_internal::mask<size>::bits << start) | RegisterType::writable_patch) == RegisterType::writable_patch));
            reg.write_unchecked_gcc((reg.read_unchecked_gcc() & ~(armtastic_internal::mask<size>::bits << start)) | ((bits & armtastic_internal::mask<size>::bits) << start));
        #else
            reg.write<From, To>(bits);
        #endif
    }
    operator data_type()
    {
        return read();
    }
    data_type read()
    {
        #ifdef __GNUC__ // don't know any version where this works
            // using m_Reg.read<From, To>(); triggers a compiler bug on GCC versions (at least) 4.1.1, 4.4.0
            BOOST_STATIC_ASSERT(((((armtastic_internal::mask<size>::bits << start) | RegisterType::readable_patch) == RegisterType::readable_patch) || RegisterType::cached_patch));
            return (reg.read_unchecked_gcc() >> start) & armtastic_internal::mask<size>::bits;
        #else
            return reg.read<From, To>();
        #endif
    }
};

// Implementation : registers with fixed addresses on the memory bus
template <u32 Address, u8 Width = 32>
class static_memory_register
{
public:
    typedef typename integer_type_from_bits<Width, false>::type data_type;

protected:
    data_type read_impl()
    {
        return *((volatile data_type*)Address);
    }

    void write_impl(data_type bits)
    {
        *((volatile data_type*)Address) = bits;
    }
};

// Implementation : forward to instance
template <typename InstanceType>
class forward_register
{
public:
    typedef typename InstanceType::data_type data_type;
    static const u8 bit_count = bits_from_integer_type<data_type>::bits;

protected:
    data_type read_impl()
    {
        return instance->read_inst();
    }

    void write_impl(data_type bits)
    {
        instance->write_inst(bits);
    }

    void set_instance(InstanceType* instance) {instance = instance;}

private:
    InstanceType* instance;
};

// Instance : registers with fixed addresses on the memory bus, but the address is not static (allows having a single type for several identical registers at different addresses)
template <u8 Width = 32>
class dynamic_memory_register
{
public:
    typedef typename integer_type_from_bits<Width, false>::type data_type;

    dynamic_memory_register(u32 address) : ptr( (volatile data_type*) address ) {}

    data_type read_inst()
    {
        return *ptr;
    }

    void write_inst(data_type bits)
    {
        *ptr = bits;
    }

private:
    volatile data_type* ptr;
};

}
