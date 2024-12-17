/*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, write to the Free Software
*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*
*   Product name: redemption, a FLOSS RDP proxy
*   Copyright (C) Wallix 2010-2014
*   Author(s): Christophe Grosjean, Raphael Zhou, Jonathan Poelen, Meng Tan
*/

#pragma once

#include "utils/sugar/array_view.hpp"
#include "utils/sugar/cast.hpp"
#include "cxx/diagnostic.hpp"

#include <type_traits>
#include <cstring>


namespace detail
{
    // template<class SplitterView>
    class split_view_end_iterator
    {};

    template<class SplitterView>
    struct split_view_iterator
    {
    public:
        using value_type = typename SplitterView::value_type;

        explicit split_view_iterator(SplitterView & splitter)
        : splitter(&splitter)
        , has_value(splitter.next())
        {
            if (has_value) {
                av = splitter.value();
            }
        }

        explicit split_view_iterator(split_view_end_iterator /*end*/)
        : splitter(nullptr)
        {}

        split_view_iterator& operator++()
        {
            has_value = this->splitter->next();
            av = this->splitter->value();
            return *this;
        }

        const value_type& operator*() const
        {
            return av;
        }

        const value_type* operator->() const
        {
            return &av;
        }

        // should be operator==(split_view_end_iterator), but does not work with gcc-8.0
        // bool operator==(split_view_end_iterator<SplitterView> const & /*other*/) const
        // {
        //     return splitter->empty();
        // }

        bool operator==([[maybe_unused]] split_view_iterator const & other) const
        {
            assert(!other.splitter && splitter);
            return !has_value;
        }

        bool operator!=(split_view_iterator const & other) const
        {
            return !operator==(other);
        }

    private:
        SplitterView* splitter;
        bool has_value;
        value_type av;
    };

    inline char* strchr_from_cstr(char* s, int c) noexcept
    {
        return std::strchr(s, c);
    }

    inline char const* strchr_from_cstr(char const* s, int c) noexcept
    {
        return std::strchr(s, c);
    }

    inline unsigned char* strchr_from_cstr(unsigned char* s, int c) noexcept
    {
        return byte_ptr_cast(std::strchr(char_ptr_cast(s), c));
    }

    inline unsigned char const* strchr_from_cstr(unsigned char const* s, int c) noexcept
    {
        return byte_ptr_cast(std::strchr(char_ptr_cast(s), c));
    }

    inline char* strchr_from_view(char* s, std::size_t n, int c) noexcept
    {
        return static_cast<char*>(std::memchr(s, c, n));
    }

    inline char const* strchr_from_view(char const* s, std::size_t n, int c) noexcept
    {
        return static_cast<char const*>(std::memchr(s, c, n));
    }

    inline unsigned char* strchr_from_view(unsigned char* s, std::size_t n, int c) noexcept
    {
        return static_cast<unsigned char*>(std::memchr(s, c, n));
    }

    inline unsigned char const* strchr_from_view(unsigned char const* s, std::size_t n, int c) noexcept
    {
        return static_cast<unsigned char const*>(std::memchr(s, c, n));
    }

    template<class Data, class AV>
    struct SplitterChView
    {
        using value_type = AV;

        SplitterChView(Data data, int sep) noexcept
        : data_(data)
        , has_value_(data.str && *data.str)
        , sep_(sep)
        {}

        bool next() noexcept
        {
            if (REDEMPTION_UNLIKELY(!has_value_)) {
                return false;
            }

            data_.str = cur_;
            cur_ = data_.find(sep_);
            if (cur_) {
                end_av_ = cur_;
                ++cur_;
            }
            else {
                has_value_ = false;
                cur_ = data_.end_ptr();
                end_av_ = cur_;
            }
            return true;
        }

        value_type value() const
        {
            return AV{data_.str, end_av_};
        }

        split_view_iterator<SplitterChView> begin()
        {
            return split_view_iterator<SplitterChView>(*this);
        }

        // should be detail::split_view_end_iterator<SplitterView> end(), but does not work with gcc-8.0
        split_view_iterator<SplitterChView> end()
        {
            return split_view_iterator<SplitterChView>{detail::split_view_end_iterator()};
        }

    private:
        Data data_;
        typename Data::pointer cur_ = data_.str;
        typename Data::pointer end_av_;
        bool has_value_;
        int sep_;
    };

    template<class Ptr>
    struct SplitterChViewDataCStr
    {
        using pointer = Ptr;

        Ptr end_ptr() const
        {
            return str + strlen(str);
        }

        pointer find(int c)
        {
            return strchr_from_cstr(str, c);
        }

        Ptr str;
    };

    template<class Ptr>
    struct SplitterChViewDataView
    {
        using pointer = Ptr;

        Ptr end_ptr() const
        {
            return end;
        }

        pointer find(int c)
        {
            return strchr_from_view(str, static_cast<std::size_t>(end - str), c);
        }

        Ptr str;
        Ptr end;
    };
} // namespace detail

template<class AV, class Sep = typename AV::value_type>
struct SplitterView
{
    using value_type = AV;

    template<class TSep>
    SplitterView(AV av, TSep&& sep)
    : cur_(av.begin())
    , last_(av.end())
    , has_value_(cur_ != last_)
    , sep_(static_cast<TSep&&>(sep))
    {}

    bool next()
    {
        if (REDEMPTION_UNLIKELY(!has_value_)) {
            return false;
        }

        auto first = cur_;
        while (cur_ != last_ && !bool(sep_ == *cur_)) {
            ++cur_;
        }

        av_ = AV{first, cur_};

        if (cur_ != last_) {
            ++cur_;
        }
        else {
            has_value_ = false;
        }

        return true;
    }

    value_type value() const
    {
        return av_;
    }

    detail::split_view_iterator<SplitterView> begin()
    {
        return detail::split_view_iterator<SplitterView>(*this);
    }

    // should be detail::split_view_end_iterator<SplitterView> end(), but does not work with gcc-8.0
    detail::split_view_iterator<SplitterView> end()
    {
        return detail::split_view_iterator<SplitterView>{detail::split_view_end_iterator()};
    }

private:
    typename AV::iterator cur_;
    typename AV::iterator last_;
    AV av_;
    bool has_value_;
    Sep sep_;
};


namespace detail
{
    template<class> struct split_with_char_sep : std::false_type {};
    template<> struct split_with_char_sep<char> : std::true_type {};
    template<> struct split_with_char_sep<unsigned char> : std::true_type {};

    template<class> struct split_with_chars_ptr : std::false_type {};
    template<> struct split_with_chars_ptr<char*> : std::true_type
    {
        using view = writable_chars_view;
    };
    template<> struct split_with_chars_ptr<char const*> : std::true_type
    {
        using view = chars_view;
    };
    template<> struct split_with_chars_ptr<unsigned char*> : std::true_type
    {
        using view = writable_array_view<unsigned char*>;
    };
    template<> struct split_with_chars_ptr<unsigned char const*> : std::true_type
    {
        using view = array_view<unsigned char*>;
    };

    inline int normalize_split_with_sep_char(char c) noexcept
    {
        return static_cast<unsigned char>(c);
    }

    inline int normalize_split_with_sep_char(unsigned char c) noexcept
    {
        return c;
    }
} // namespace detail

/// \brief Returns a view which allows to loop on strings separated by a character.
/// \note There is no result when the separator is on the last character
/// \code
/// split_with(",a,b,", ',') -> "", "a", "b"
/// \endcode
REDEMPTION_DIAGNOSTIC_PUSH()
#if REDEMPTION_WORKAROUND(REDEMPTION_COMP_GCC, >= 1400) \
    && REDEMPTION_WORKAROUND(REDEMPTION_COMP_GCC, < 1500)
REDEMPTION_DIAGNOSTIC_GCC_ONLY_IGNORE("-Wdangling-reference")
#endif
template<class Chars, class Sep>
auto split_with(Chars&& chars, Sep&& sep)
{
    using Chars2 = std::decay_t<Chars>;
    using Sep2 = std::decay_t<Sep>;
    using IsCharPtr = detail::split_with_chars_ptr<Chars2>;

    if constexpr (IsCharPtr::value && std::is_integral_v<Sep2>) {
        using CharsView = typename IsCharPtr::view;
        using Data = detail::SplitterChViewDataCStr<Chars2>;
        return detail::SplitterChView<Data, CharsView>(
            Data{chars}, detail::normalize_split_with_sep_char(sep)
        );
    }
    else {
        using cv_elem_t = std::remove_pointer_t<decltype(utils::data(std::declval<Chars&&>()))>;
        using elem_t = std::remove_cv_t<cv_elem_t>;
        using View = std::conditional_t<
            std::is_const_v<cv_elem_t>,
            array_view<elem_t>,
            writable_array_view<elem_t>
        >;
        View av(chars);
        if constexpr (detail::split_with_chars_ptr<elem_t*>::value && std::is_integral_v<Sep2>) {
            using Data = detail::SplitterChViewDataView<typename View::pointer>;
            return detail::SplitterChView<Data, View>(
                Data{av.data(), av.data() + av.size()},
                detail::normalize_split_with_sep_char(sep)
            );
        }
        else {
            return SplitterView<View, Sep2>(av, static_cast<Sep&&>(sep));
        }
    }
}

template<class Chars, class Sep = char>
auto get_lines(Chars&& chars, Sep&& sep = '\n') /*NOLINT*/
{
    return split_with(static_cast<Chars&&>(chars), static_cast<Sep&&>(sep));
}
REDEMPTION_DIAGNOSTIC_POP()
