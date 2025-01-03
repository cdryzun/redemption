/*
SPDX-FileCopyrightText: 2023 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "cxx/diagnostic.hpp"
#include <type_traits>
#include <cassert>


template<auto V>
struct nontype_t
{
    explicit nontype_t() = default;
};

template<class T>
inline constexpr bool is_nontype_t_v = false;

template<auto V>
inline constexpr bool is_nontype_t_v<nontype_t<V>> = true;


namespace detail
{
    template<class Sig> struct qual_fn_sig;

    template<class R, class... Args>
    struct qual_fn_sig<R(Args...)>
    {
        using function = R(Args...);
        static constexpr bool is_noexcept = false;
        static constexpr bool is_const = false;

        template<class T> using cv = T;
    };

    template<class R, class... Args>
    struct qual_fn_sig<R(Args...) noexcept>
    {
        using function = R(Args...);
        static constexpr bool is_noexcept = true;
        static constexpr bool is_const = false;

        template<class T> using cv = T;
    };

    template<class R, class... Args>
    struct qual_fn_sig<R(Args...) const>
    {
        using function = R(Args...);
        static constexpr bool is_noexcept = false;
        static constexpr bool is_const = true;

        template<class T> using cv = T const;
    };

    template<class R, class... Args>
    struct qual_fn_sig<R(Args...) const noexcept>
    {
        using function = R(Args...);
        static constexpr bool is_noexcept = true;
        static constexpr bool is_const = true;

        template<class T> using cv = T const;
    };


    union FunctionRefStorage
    {
        void *p = nullptr;
        void const *cp;
        void (*fp)();

        static FunctionRefStorage from_obj(void* p) noexcept
        {
            return {.p = p};
        }

        static FunctionRefStorage from_obj(void const* p) noexcept
        {
            return {.cp = p};
        }
    };

    template<class T>
    using fn_param_t = typename std::conditional_t<
        std::is_trivially_copyable_v<T> && sizeof(T) <= sizeof(long long) * 2,
        T,
        T&&
    >;


    template<class T, class Self>
    inline constexpr bool is_not_self_v = true;

    template<class T>
    inline constexpr bool is_not_self_v<T, T> = false;

    template<class T, class Self>
    inline constexpr bool is_not_self_v<T const, Self> = is_not_self_v<T, Self>;

    template<class T, class Self>
    inline constexpr bool is_not_self_v<T&, Self> = is_not_self_v<T, Self>;

    template<class T, class Self>
    inline constexpr bool is_not_self_v<T&&, Self> = is_not_self_v<T, Self>;


    template<class From, class To>
    inline constexpr bool is_convertible_without_dangle_ref_v
        = requires { static_cast<int(*)(To)>(nullptr)(static_cast<From(*)()>(nullptr)()); }
        #if __has_builtin(__reference_converts_from_temporary)
        && !__reference_converts_from_temporary(From, To)
        #endif
        ;

    template<class From>
    inline constexpr bool is_convertible_without_dangle_ref_v<From, void> = true;
} // namespace detail

template<class Sig, class = typename detail::qual_fn_sig<Sig>::function>
struct FunctionRef;

template<class Sig, class R, class... Args>
class FunctionRef<Sig, R(Args...)>
{
    using signature = detail::qual_fn_sig<Sig>;
    using storage = detail::FunctionRefStorage;

    template<class T> using cv = typename signature::template cv<T>;
    template<class T> using cvref = cv<T> &;


    static constexpr bool noex = signature::is_noexcept;

    typedef R fwd_t(storage, detail::fn_param_t<Args>...) noexcept(noex);
    fwd_t *fptr_ = nullptr;
    storage obj_;

public:
    FunctionRef(FunctionRef const&) noexcept = default;

    FunctionRef &operator=(FunctionRef const&) noexcept = default;

    template<class T>
    FunctionRef &operator=(T)
        requires(detail::is_not_self_v<T, FunctionRef> && !std::is_pointer_v<T> &&
                 !is_nontype_t_v<T>)
        = delete;

    REDEMPTION_DIAGNOSTIC_PUSH()
    REDEMPTION_DIAGNOSTIC_CLANG_IGNORE("-Wcast-function-type-strict")
    template<class F>
    FunctionRef(F * f) noexcept
        requires detail::is_convertible_without_dangle_ref_v<
            // fast invoke_result
            decltype(f(static_cast<Args(*)()>(nullptr)()...)), R
        >
        : fptr_(
            [](storage s, detail::fn_param_t<Args>... args) noexcept(noex) -> R
            {
                return reinterpret_cast<F*>(s.fp)(static_cast<decltype(args)>(args)...);
            }),
          obj_{.fp = reinterpret_cast<void(*)()>(f)}
    {
        assert(f != nullptr && "must reference a not nullptr function pointer");
    }
    REDEMPTION_DIAGNOSTIC_POP()

    template<class F>
    constexpr FunctionRef(F && f) noexcept
        requires(detail::is_not_self_v<F, FunctionRef>
            && detail::is_convertible_without_dangle_ref_v<
                // fast invoke_result for function object
                decltype(f(static_cast<Args(*)()>(nullptr)()...)), R
            >
        )
        : fptr_(
            [](storage s, detail::fn_param_t<Args>... args) noexcept(noex) -> R
            {
                using T = std::remove_reference_t<F>;
                if constexpr (signature::is_const) {
                    return (*static_cast<T const*>(s.cp))(static_cast<decltype(args)>(args)...);
                }
                else {
                    return (*static_cast<T*>(s.p))(static_cast<decltype(args)>(args)...);
                }
            }),
          obj_(storage::from_obj(__builtin_addressof(f)))
    {}

    // TODO member obj support

    template<auto f>
    constexpr FunctionRef(nontype_t<f>) noexcept
        requires detail::is_convertible_without_dangle_ref_v<
            // fast invoke_result for function object
            decltype(f(static_cast<Args(*)()>(nullptr)()...)), R
        >
        : fptr_(
              [](storage, detail::fn_param_t<Args>... args) noexcept(noex) -> R {
                  return f(static_cast<decltype(args)>(args)...);
              })
    {
        using F = decltype(f);
        if constexpr (std::is_pointer_v<F> or std::is_member_pointer_v<F>) {
            static_assert(f != nullptr, "NTTP callable must be usable");
        }
    }

    // template<auto f, class U, class T = std::remove_reference_t<U>>
    // constexpr function_ref(nontype_t<f>, U &&obj) noexcept
    // TODO add special type for temporary obj
    //
    // template<auto f, class T>
    // constexpr function_ref(nontype_t<f>, typename signature::template cv<T> *obj) noexcept

    constexpr R operator()(Args... args) const noexcept(noex)
    {
        return fptr_(obj_, static_cast<Args&&>(args)...);
    }
};
