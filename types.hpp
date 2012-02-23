#pragma once

#include "boost/mpl/if.hpp"

typedef unsigned char      u8;
typedef signed   char      s8;
typedef unsigned short     u16;
typedef signed   short     s16;
typedef unsigned long      u32;
typedef signed   long      s32;
typedef unsigned long long u64;
typedef signed   long long s64;

typedef u32 size_t;

template <typename Type> inline Type max_t(Type a, Type b)      {return (a > b) ? a : b;}
template <typename Type> inline Type min_t(Type a, Type b)      {return (a < b) ? a : b;}

namespace armtastic {

// A template to select the smallest integer type for a given amount of bits
template <u32 Bits, bool Signed>
struct integer_type_from_bits
{
    typedef typename boost::mpl::if_c<(Bits <= 8 && Signed),   s8
          , typename boost::mpl::if_c<(Bits <= 8 && !Signed),  u8
          , typename boost::mpl::if_c<(Bits <= 16 && Signed),  s16
          , typename boost::mpl::if_c<(Bits <= 16 && !Signed), u16
          , typename boost::mpl::if_c<(Bits <= 32 && Signed),  s32
          , typename boost::mpl::if_c<(Bits <= 32 && !Signed), u32
          , typename boost::mpl::if_c<(Bits <= 64 && Signed),  s64
          , typename boost::mpl::if_c<(Bits <= 64 && !Signed), u64
          , void>::type >::type >::type >::type >::type >::type >::type >::type type;
};

// General integer type templates
template <u32 Bits>
struct u
{
    typedef typename integer_type_from_bits<Bits, false>::type t;
    static t swap(t a) {return a;}
};
template <>
struct u<16>
{
    typedef integer_type_from_bits<16, false>::type t;
    static t swap(t a) {return ((a << 8) & 0xFF00) + ((a >> 8) & 0xFF);}
};
template <>
struct u<32>
{
    typedef integer_type_from_bits<32, false>::type t;
    static t swap(t a) {return ((a << 24) & 0xFF000000) + ((a << 8) & 0x00FF0000) + ((a >> 8) & 0x0000FF00) + ((a >> 24) & 0xFF);}
};
template <u32 Bits>
struct s
{
    typedef typename integer_type_from_bits<Bits, true>::type t;
};

// A template to get the amount of bits occupied by a type
template <typename Type> struct bits_from_integer_type {static const u8 bits = 0;};
template <> struct bits_from_integer_type<s8>          {static const u8 bits = 8;};
template <> struct bits_from_integer_type<u8>          {static const u8 bits = 8;};
template <> struct bits_from_integer_type<s16>         {static const u8 bits = 16;};
template <> struct bits_from_integer_type<u16>         {static const u8 bits = 16;};
template <> struct bits_from_integer_type<s32>         {static const u8 bits = 32;};
template <> struct bits_from_integer_type<u32>         {static const u8 bits = 32;};
template <> struct bits_from_integer_type<s64>         {static const u8 bits = 64;};
template <> struct bits_from_integer_type<u64>         {static const u8 bits = 64;};

// A template to determine the highest set bit (one-based)
template <u32 Number>
struct highest_set_bit
{
    static const u8 highest = 1 + highest_set_bit<(Number >> 1)>::highest;
};
template <> struct highest_set_bit<0> {static const u32 highest = 0;};

// A template to compute the next larger integer with only one bit set (returns 4 minimum)
template <u32 Size>
struct aligned_up
{
    static const u32 next = 1 << highest_set_bit<Size - 1>::highest;
};
template <> struct aligned_up<2> {static const u32 next = 4;};
template <> struct aligned_up<1> {static const u32 next = 4;};
template <> struct aligned_up<0> {static const u32 next = 4;};

// A 2's logarithm template, ceiled
template <u32 Number>
struct log_2_ceiled
{
    static const u32 log = highest_set_bit<aligned_up<Number>::next>::highest;
};

// A template to find the next multiple of an integer from a given point
template <u32 MultipleOf, u32 Number>
struct next_multiple
{
private:
    static const u32 mod = Number%MultipleOf;
public:
    static const u32 next = Number - mod + (mod ? MultipleOf : 0);
};

}