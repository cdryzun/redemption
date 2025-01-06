/*
SPDX-FileCopyrightText: 2023 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/attributes/spec.hpp"
#include "configs/generators/utils/write_template.hpp"
#include "configs/enumeration.hpp"

#include "configs/attributes/spec.hpp"
#include "configs/generators/utils/multi_filename_writer.hpp"
#include "configs/generators/utils/write_template.hpp"
#include "configs/generators/utils/json.hpp"
#include "configs/type_name.hpp"
#include "utils/sugar/array_view.hpp"
#include "utils/file_permissions.hpp"

#include "utils/file_permissions.hpp"
#include "utils/screen_resolution.hpp"
#include "utils/sugar/int_to_chars.hpp"
#include "utils/sugar/bounded_array_view.hpp"
#include "utils/sugar/cast.hpp"
#include "utils/sugar/split.hpp"

#include "core/RDP/rdp_performance_flags.hpp"
#include "parse_performance_flags.hpp"

#include <bitset>
#include <charconv>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cerrno>
#include <cstring>


namespace cfg_generators
{

using Names = cfg_desc::names;

using namespace std::string_view_literals;
using namespace std::string_literals;

constexpr std::string_view begin_raw_string = "R\"gen_config_ini("sv;
constexpr std::string_view end_raw_string = ")gen_config_ini\""sv;

namespace types = cfg_desc::types;

using namespace std::string_literals;
using namespace cfg_desc;

// #if __has_include(<linux/limits.h>)
// # include <linux/limits.h>
// constexpr std::size_t path_max = PATH_MAX;
// #else
// constexpr std::size_t path_max = 4096;
constexpr auto path_max_as_str = "4096"_av;
// #endif

constexpr inline std::array conn_policies {
    DestSpecFile::rdp,
    DestSpecFile::vnc,
    DestSpecFile::rdp_sogisces_1_3_2030,
};

constexpr inline auto conn_policies_acl_map_mask
  = DestSpecFile::rdp
  | DestSpecFile::vnc
;

constexpr std::string_view dest_file_to_filename(DestSpecFile dest)
{
    switch (dest) {
        case DestSpecFile::none: return ""sv;
        case DestSpecFile::ini_only: return ""sv;
        case DestSpecFile::global_spec: return ""sv;
        case DestSpecFile::rdp: return "rdp"sv;
        case DestSpecFile::rdp_sogisces_1_3_2030: return "rdp-sogisces_1.3_2030"sv;
        case DestSpecFile::vnc: return "vnc"sv;
    }
    return ""sv;
}

struct DataAsStrings
{
    struct ConnectionPolicy
    {
        DestSpecFile dest_file;
        std::string py;
        std::string json;
        bool forced;
    };

    std::string py;
    std::string ini;
    std::string json = py;
    std::string cpp = py;
    std::string cpp_comment = cpp;
    std::vector<ConnectionPolicy> connection_policies {};

    template<class Int>
    static DataAsStrings from_integer(Int value)
    {
        auto d = int_to_decimal_chars(value);
        return DataAsStrings::all(std::string(d.sv()));
    }

    static DataAsStrings all(std::string value)
    {
        return {
            .py = value,
            .ini = std::move(value),
        };
    }

    static DataAsStrings quoted(std::string value)
    {
        DataAsStrings r {
            .py = {},
            .ini = std::move(value),
        };

        r.py.reserve(value.size() * 2 + 2);
        r.py += '"';
        for (char c : r.ini) {
            if ('"' == c || '\\' == c) {
                r.py += '\\';
            }
            r.py += c;
        }
        r.py += '"';
        r.json = r.py;

        if (r.py.size() > 2) {
            r.cpp = r.py;
        }
        r.cpp_comment = r.py;

        return r;
    }

    static DataAsStrings unspaced_quote(std::string s)
    {
        return {
            .py = str_concat('"', s, '"'),
            .ini = std::move(s),
        };
    }
};

inline /*constexpr*/ DataAsStrings novalue_to_string {
    .py = "\"\"",
    .ini = {},
    .cpp = {},
    .cpp_comment = "\"\"",
};

inline DataAsStrings string_value_to_strings(types::dirpath)
{
    return novalue_to_string;
}

template<class T>
DataAsStrings string_value_to_strings(types::list<T>)
{
    return novalue_to_string;
}

template<unsigned n>
DataAsStrings string_value_to_strings(types::fixed_string<n>)
{
    return novalue_to_string;
}

inline std::string cpp_expr_to_string(cpp::expr expr)
{
    return str_concat(end_raw_string, " << ("sv, expr.value, ") << "sv, begin_raw_string);
}

inline std::string cpp_expr_to_json(cpp::expr expr)
{
    return str_concat('"', expr.value, "\", \"cppExpr\": true"sv);
}

inline DataAsStrings string_value_to_strings(cpp::expr expr)
{
    auto s = cpp_expr_to_string(expr);
    return {
        .py = str_concat('"', s, '"'),
        .ini = std::move(s),
        .json = cpp_expr_to_json(expr),
        .cpp = expr.value,
    };
}

inline DataAsStrings string_value_to_strings(std::string_view str)
{
    return DataAsStrings::quoted(std::string(str));
}

template<std::size_t N>
inline DataAsStrings binary_string_value_to_strings(sized_array_view<char, N> av)
{
    char* p;

    constexpr std::size_t py_len = N*4 + 2;
    char py[py_len];
    p = py;
    *p++ = '"';
    for (char c : av) {
        *p++ = '\\';
        *p++ = 'x';
        p = int_to_fixed_hexadecimal_upper_chars(p, static_cast<unsigned char>(c));
    }
    *p++ = '"';

    constexpr std::size_t cpp_len = N*6 + 2;
    char cpp[cpp_len];
    p = cpp;
    *p++ = '{';
    for (char c : av) {
        *p++ = '0';
        *p++ = 'x';
        p = int_to_fixed_hexadecimal_upper_chars(p, static_cast<unsigned char>(c));
        *p++ = ',';
        *p++ = ' ';
    }
    *p++ = '}';

    constexpr std::size_t ini_len = N*2;
    char ini_and_json[ini_len + 2];
    p = ini_and_json;
    *p++ = '"';
    for (char c : av) {
        p = int_to_fixed_hexadecimal_upper_chars(p, static_cast<unsigned char>(c));
    }
    *p++ = '"';

    return {
        .py = std::string(py, py_len),
        .ini = std::string(ini_and_json + 1, ini_len),
        .json = std::string(ini_and_json, ini_len + 2),
        .cpp = std::string(cpp, cpp_len),
    };
}


inline DataAsStrings integer_value_to_strings(bool) = delete;

inline DataAsStrings integer_value_to_strings(cpp::expr expr)
{
    auto s = cpp_expr_to_string(expr);
    return {
        .py = s,
        .ini = std::move(s),
        .json = cpp_expr_to_json(expr),
        .cpp = expr.value,
    };
}

template<class T>
inline DataAsStrings integer_value_to_strings(T const& value)
{
    if constexpr (std::is_base_of_v<impl::integer_base, T>) {
        return {
            .py = "0",
            .ini = "0",
        };
    }
    else {
        return DataAsStrings::from_integer(value);
    }
}

template<class T, class Ratio>
DataAsStrings integer_value_to_strings(std::chrono::duration<T, Ratio> value)
{
    if (value.count()) {
        return DataAsStrings::from_integer(value.count());
    }
    return {
        .py = "0",
        .ini = "0",
        .cpp = {},
        .cpp_comment = "0",
    };
}

template<class Int, long min, long max>
DataAsStrings integer_value_to_strings(types::range<Int, min, max>)
{
    static_assert(!min, "unspecified value but 'min' isn't 0");
    return integer_value_to_strings(Int());
}

inline DataAsStrings bool_value_to_strings(bool value)
{
    if (value) {
        return {
            .py = "True",
            .ini = "1",
            .json = "true",
            .cpp = "true",
        };
    }
    else {
        return {
            .py = "False",
            .ini = "0",
            .json = "false",
            .cpp = "false",
        };
    }
}

inline DataAsStrings bool_value_to_strings(cpp::expr expr)
{
    auto s = cpp_expr_to_string(expr);
    return {
        .py = s,
        .ini = std::move(s),
        .json = cpp_expr_to_json(expr),
        .cpp = expr.value,
    };
}

inline DataAsStrings color_value_to_strings(types::rgb) = delete;

inline DataAsStrings color_value_to_strings(uint32_t color)
{
    assert(!(color >> 24));

    char data[7];
    char* p = data;
    *p++ = '#';
    auto puthex = [&](uint32_t n){ *p++ = "0123456789ABCDEF"[n & 0xf]; };
    puthex(color >> 20);
    puthex(color >> 16);
    puthex(color >> 12);
    puthex(color >> 8);
    puthex(color >> 4);
    puthex(color >> 0);

    auto s = std::string_view(data, 7);

    return {
        .py = str_concat('"', s, '"'),
        .ini = std::string(s),
        .cpp = str_concat("0x"sv, std::string_view(data+1, 6)),
    };
}

inline DataAsStrings file_permission_value_to_strings(unsigned permissions)
{
    std::array<char, 16> d;
    d[0] = '0';
    char* p = d.data() + 1;
    auto r = std::to_chars(p, d.end(), permissions, 8);
    auto len = static_cast<std::size_t>(r.ptr - p);

    return {
        .py = str_concat('"', std::string_view(p, len), '"'),
        .ini = std::string(p, len),
        .cpp = std::string(d.data(), len + 1),
    };
}

inline DataAsStrings screen_resolution_value_to_strings(ScreenResolution resolution)
{
    if (!resolution.is_valid()) {
        return {
            .py = "\"\""s,
            .ini = {},
            .cpp = {},
        };
    }

    auto w = int_to_decimal_chars(resolution.width);
    auto h = int_to_decimal_chars(resolution.height);

    return {
        .py = str_concat('"', w.sv(), 'x', h.sv(), '"'),
        .ini = str_concat(w.sv(), 'x', h.sv()),
        .cpp = str_concat(w.sv(), ", "sv, h.sv()),
    };
}


struct ValueAsStrings
{
    std::string prefix_spec_type;
    std::string cpp_type;
    std::string_view json_type;
    std::string json_extra_type = {};
    std::string spec_type = cpp_type;
    std::string acl_diag_type = cpp_type;
    std::size_t spec_str_buffer_size;
    DataAsStrings values;
    std::string spec_note = {};
    std::string ini_note = spec_note;
    std::string acl_diag_note = spec_note;
    std::string cpp_note = {};
    std::size_t align_of = 0;
    const type_enumeration* enumeration = nullptr;
    bool string_parser_for_enum = false;
};

// "fix" -Wmissing-braces with clang
struct MemberNames : cfg_desc::names
{
    MemberNames(char const* str)
        : cfg_desc::names{str}
    {}

    MemberNames(std::string_view str)
        : cfg_desc::names{str}
    {}

    MemberNames(cfg_desc::names names)
        : cfg_desc::names{names}
    {}
};


enum class PrefixType : uint8_t
{
    Unspecified,
    NoPrefix,
    ForceDisable,
};

inline void check_policy(std::initializer_list<DestSpecFile> dest_files)
{
    auto policy_types = DestSpecFile();
    for (auto dest_file : dest_files) {
        if (!contains_conn_policy(dest_file)) {
            throw std::runtime_error("connection policy value without name");
        }
        if (bool(policy_types & dest_file)) {
            const auto mask = (policy_types & dest_file);
#define DECL(name) bool(mask & DestSpecFile::name) ? " (" #name ")"sv : ""sv
            throw std::runtime_error(
                str_concat("duplicate connection policy value"sv,
                           DECL(vnc), DECL(rdp), DECL(rdp_sogisces_1_3_2030))
            );
#undef DECL
        }
        policy_types |= dest_file;
    }
}

// for implicitly convert std::string_view to std::string
struct Description
{
    struct ConnPolicy
    {
        DestSpecFile policy_types;
        std::string desc;
    };

    std::string general_desc;
    std::string details_desc;
    std::vector<ConnPolicy> specific_descs;

    Description() = default;

    struct WithDetails
    {
        std::string general;
        std::string details;
    };

    Description(WithDetails d)
    : general_desc(std::move(d.general))
    , details_desc(std::move(d.details))
    {}

    template<class... ConnPolicy>
    Description(char const* str, ConnPolicy&&... conn_policy)
    : Description(1, str, static_cast<ConnPolicy&&>(conn_policy)...)
    {}

    template<class... ConnPolicy>
    Description(std::string_view str, ConnPolicy&&... conn_policy)
    : Description(1, str, static_cast<ConnPolicy&&>(conn_policy)...)
    {}

    template<class... ConnPolicy>
    Description(std::string const& str, ConnPolicy&&... conn_policy)
    : Description(1, str, static_cast<ConnPolicy&&>(conn_policy)...)
    {}

    template<class... ConnPolicy>
    Description(std::string&& str, ConnPolicy&&... conn_policy)
    : Description(1, std::move(str), static_cast<ConnPolicy&&>(conn_policy)...)
    {}

    std::string const* find_desc_for_policy(DestSpecFile policy_type) const
    {
        for (auto const& connpolicy : specific_descs) {
            if (bool(connpolicy.policy_types & policy_type)) {
                return &connpolicy.desc;
            }
        }
        return nullptr;
    }

private:
    template<class S, class... ConnPolicy>
    Description(int, S&& str, ConnPolicy&&... conn_policy) noexcept
    : general_desc(std::move(str))
    {
        check_policy({conn_policy.policy_types...});
        (..., specific_descs.push_back(conn_policy));
    }
};

struct MemberInfo
{
    MemberNames name;
    // when specified, use another section for connection policy
    std::string_view connpolicy_section {};
    ValueAsStrings value;
    SpecInfo spec;
    Tags tags {};
    PrefixType prefix_type = PrefixType::Unspecified;
    Description desc {};
};

struct Section
{
    Names names;
    std::vector<MemberInfo> members;

    MemberInfo const* find_member(std::string_view member_name) const
    {
        for (auto const& m : members) {
            if (m.name.all == member_name) {
                return &m;
            }
        }
        return nullptr;
    }
};

struct ConfigInfo
{
    std::vector<Section> sections;
    std::unordered_map<std::string_view, std::size_t> sections_pos_by_name;

    Section* find_section(std::string_view section_name)
    {
        auto it = sections_pos_by_name.find(section_name);
        if (it != sections_pos_by_name.end()) {
            return &sections[it->second];
        }
        return nullptr;
    }

    Section const* find_section(std::string_view section_name) const
    {
        return const_cast<ConfigInfo*>(this)->find_section(section_name);
    }

    void push_section(std::string_view section_name)
    {
        sections_pos_by_name.emplace(section_name, sections.size());
        sections.emplace_back().names.all = section_name;
    }
};


namespace cpp_config_writer
{

using namespace cfg_desc;

struct HtmlEntitifier
{
    struct OutputData
    {
        friend std::ostream& operator<<(std::ostream& out, OutputData const& data)
        {
            for (char c : data.str) {
                if (c == '<') {
                    out << "&lt;";
                }
                else {
                    out << c;
                }
            }
            return out;
        }

        std::string str;
    };

    template<class F>
    OutputData operator()(F&& f)
    {
        out_string.str("");
        f(out_string);
        return OutputData{out_string.str()};
    }

private:
    std::ostringstream out_string;
};


struct CppConfigWriterBase;

void write_acl_and_spec_type(std::ostream & out, CppConfigWriterBase& writer);
void write_max_str_buffer_size_hpp(std::ostream & out, CppConfigWriterBase& writer);
void write_ini_values(std::ostream & out, CppConfigWriterBase& writer);
void write_config_set_value(std::ostream & out_set_value, CppConfigWriterBase& writer);
void write_variables_configuration(std::ostream & out_varconf, CppConfigWriterBase& writer);
void write_variables_configuration_fwd(std::ostream & out_varconf, CppConfigWriterBase& writer);
void write_authid_hpp(std::ostream & out_authid, CppConfigWriterBase& writer);
void write_str_authid_hpp(std::ostream & out_authid, CppConfigWriterBase& writer);

struct CppConfigWriterBase
{
    unsigned depth = 0;
    std::ostringstream out_body_parser_;
    std::ostringstream out_member_;
    HtmlEntitifier html_entitifier;

    struct Member
    {
        std::string_view name;
        std::size_t align_of;

        friend std::ostream& operator <<(std::ostream& out, Member const& x)
        {
            return out << x.name;
        }
    };

    struct Section
    {
        std::string_view section_name;
        std::string member_struct;
        std::vector<Member> members;
    };

    struct SectionString
    {
        std::string_view section_name;
        std::string body;
    };

    std::vector<SectionString> sections_parser;
    std::vector<Section> sections;
    std::vector<Member> members;
    std::vector<std::string> variables_acl;
    std::vector<Loggable> authid_policies;
    std::size_t start_section_index = 0;
    std::vector<std::size_t> start_indexes;
    std::string cfg_values;
    std::string cfg_str_values;
    std::size_t max_str_buffer_size = 0;
    std::ostringstream out_acl_and_spec_types;

    struct Filenames
    {
        std::string authid_hpp;
        std::string str_authid_hpp;
        std::string variable_configuration_fwd;
        std::string variable_configuration_hpp;
        std::string config_set_value;
        std::string ini_values_hpp;
        std::string acl_and_spec_type;
        std::string max_str_buffer_size_hpp;
    };
    Filenames filenames;

    struct FullNames
    {
        std::vector<std::string> spec;
        std::vector<std::string> acl;

        void check()
        {
            std::string full_err;

            struct P { char const* name; std::vector<std::string> const& names; };
            for (P const& p : {
                P{"Ini", this->spec},
                P{"Sesman", this->acl},
            }) {
                std::string error;

                if (!p.names.empty()) {
                    std::vector<std::string_view> names{p.names.begin(), p.names.end()};
                    std::sort(names.begin(), names.end());

                    std::string_view previous_name;
                    for (std::string_view name : names) {
                        if (name == previous_name) {
                            str_append(error, (error.empty() ? "duplicates member: "sv : ", "sv), name);
                        }
                        previous_name = name;
                    }
                }

                if (not error.empty()) {
                    if (not full_err.empty()) {
                        full_err += " ; ";
                    }
                    str_append(full_err, p.name, ": "sv, error);
                }
            }

            if (not full_err.empty()) {
                throw std::runtime_error(full_err);
            }
        }
    };

    FullNames full_names;

    CppConfigWriterBase(Filenames filenames)
    : filenames(std::move(filenames))
    {}
};


template<class Cont, class Before, class After>
inline void join_with_comma(
    std::ostream& out, Cont const& cont,
    Before const& before, After const& after)
{
    auto first = begin(cont);
    auto last = end(cont);
    if (first == last) {
        return ;
    }
    out << before << *first << after;
    while (++first != last) {
        out << ", " << before << *first << after;
    }
}

inline void write_loggable_bitsets(
    std::ostream& out, array_view<Loggable> policies,
    std::string_view loggable_name)
{
    std::vector<std::bitset<64>> loggables;
    std::vector<std::bitset<64>> unloggable_if_value_contains_passwords;
    size_t i = 0;

    for (auto log_policy : policies)
    {
        if ((i % 64) == 0) {
            i = 0;
            loggables.push_back(0);
            unloggable_if_value_contains_passwords.push_back(0);
        }

        switch (log_policy) {
            case Loggable::Yes:
                loggables.back().set(i);
                break;
            case Loggable::No:
                break;
            case Loggable::OnlyWhenContainsPasswordString:
                unloggable_if_value_contains_passwords.back().set(i);
                break;
        }
        ++i;
    }

    out << "constexpr U64BitFlags<" << loggables.size() << "> " << loggable_name << "{ {\n  ";
    join_with_comma(out, loggables, "0b", "\n");
    out << "},\n{\n  ";
    join_with_comma(out, unloggable_if_value_contains_passwords, "0b", "\n");
    out << "} };\n";
}

inline void write_authid_hpp(std::ostream & out_authid, CppConfigWriterBase& writer)
{
    out_authid << cpp_comment(do_not_edit, 0) <<
      "\n"
      "#pragma once\n"
      "\n"
      "namespace configs\n"
      "{\n"
      "    enum class authid_t : unsigned;\n"
      "    constexpr authid_t max_authid {" << writer.full_names.acl.size() << "};\n"
      "}\n"
    ;
}

inline void write_str_authid_hpp(std::ostream & out_authid, CppConfigWriterBase& writer)
{
    out_authid << cpp_comment(do_not_edit, 0) <<
      "\n"
      "#pragma once\n"
      "\n"
      "#include \"utils/sugar/zstring_view.hpp\"\n"
      "#include \"configs/loggable.hpp\"\n"
      "#include \"configs/autogen/authid.hpp\"\n"
      "\n"
      "namespace configs\n"
      "{\n"
      "    constexpr zstring_view const authstr[] = {\n"
    ;
    for (auto & authstr : writer.full_names.acl) {
        out_authid << "        \"" << authstr << "\"_zv,\n";
    }
    out_authid <<
      "    };\n\n"
      "} // namespace configs\n"
    ;
}

inline void write_variables_configuration_fwd(std::ostream & out_varconf, CppConfigWriterBase& writer)
{
    out_varconf << cpp_comment(do_not_edit, 0) <<
        "\n"
        "#pragma once\n"
        "\n"
        "namespace cfg\n"
        "{\n"
    ;

    for (auto & section : writer.sections) {
        if (section.section_name.empty()) {
            for (auto & var : section.members) {
                out_varconf << "    struct " << var << ";\n";
            }
        }
        else if (!section.members.empty()) {
            out_varconf << "    struct " << section.section_name << " {\n";
            for (auto & var : section.members) {
                out_varconf << "        struct " << var << ";\n";
            }
            out_varconf << "    };\n\n";
        }
    }

    out_varconf << "} // namespace cfg\n";
}

inline void write_variables_configuration(std::ostream & out_varconf, CppConfigWriterBase& writer)
{
    out_varconf << cpp_comment(do_not_edit, 0) <<
        "\n"
        "#pragma once\n"
        "\n"
        "#include \"configs/autogen/authid.hpp\"\n"
        "#include \"configs/loggable.hpp\"\n"
        "#include <cstdint>\n"
        "\n"
        "namespace configs\n"
        "{\n"
        "    template<class... Ts>\n"
        "    struct Pack\n"
        "    { static const std::size_t size = sizeof...(Ts); };\n\n"
        "    namespace cfg_indexes\n"
        "    {\n"
    ;

    {
        std::size_t i = 0;
        std::size_t previous_index = 0;
        for (auto n : writer.start_indexes) {
            out_varconf << "        ";
            if (previous_index == n) {
                out_varconf << "// ";
            }
            out_varconf
                << "inline constexpr int section" << i
                << " = " << previous_index << "; "
                   "/* " << writer.sections[i].section_name << " */\n"
            ;
            previous_index = n;
            ++i;
        }
    }

    out_varconf <<
        "    } // namespace cfg_indexes\n"
        "} // namespace configs\n"
        "\n"
        "namespace cfg\n"
        "{\n"
    ;

    for (auto & section : writer.sections) {
        out_varconf << section.member_struct << "\n";
    }

    auto join = [&](auto const & cont, auto const & before, auto const & after) {
        join_with_comma(out_varconf, cont, before, after);
    };

    std::vector<std::string> section_names;

    out_varconf <<
        "} // namespace cfg\n\n"
        "namespace cfg_section {\n"
    ;
    using Member = typename CppConfigWriterBase::Member;
    auto has_values = [](CppConfigWriterBase::Section const& body) {
        return !body.section_name.empty() && !body.members.empty();
    };
    for (auto & body : writer.sections) {
        if (has_values(body)) {
            std::vector<std::reference_wrapper<Member>> v(body.members.begin(), body.members.end());
            std::stable_sort(v.begin(), v.end(), [](Member& a, Member& b){
                return a.align_of > b.align_of;
            });
            section_names.emplace_back(str_concat("cfg_section::"sv, body.section_name));
            out_varconf << "struct " << body.section_name << "\n: ";
            join(v, str_concat("cfg::"sv, body.section_name, "::"sv), "\n");
            out_varconf << "{ static constexpr bool is_section = true; };\n\n";
        }
    }
    out_varconf << "} // namespace cfg_section\n\n"
      "namespace configs {\n"
      "struct VariablesConfiguration\n"
      ": "
    ;
    join(section_names, "", "\n");
    auto it = std::find_if(begin(writer.sections), end(writer.sections),
        [&](CppConfigWriterBase::Section const& body) { return !has_values(body); });
    if (it != writer.sections.end()) {
        for (Member& mem : it->members) {
            out_varconf << ", cfg::" << mem.name << "\n";
        }
    }
    out_varconf <<
      "{};\n\n"
      "using VariablesAclPack = Pack<\n  "
    ;
    join(writer.variables_acl, "cfg::", "\n");
    out_varconf <<
      ">;\n\n\n"
    ;

    write_loggable_bitsets(out_varconf, writer.authid_policies, "loggable_field");

    out_varconf << "} // namespace configs\n";
}

inline void write_config_set_value(std::ostream & out_set_value, CppConfigWriterBase& writer)
{
    out_set_value << cpp_comment(do_not_edit, 0) <<
        "\n"
        "void Inifile::ConfigurationHolder::set_section(zstring_view section) {\n"
        "    if (0) {}\n"
    ;
    int id = 1;
    for (auto & section : writer.sections_parser) {
        out_set_value <<
            "    else if (section == \"" << section.section_name << "\"_zv) {\n"
            "        this->section_id = " << id << ";\n"
            "    }\n"
        ;
        ++id;
    }
    out_set_value <<
        "    else if (static_cast<cfg::debug::config>(this->variables).value) {\n"
        "        LOG(LOG_WARNING, \"unknown section [%s]\", section);\n"
        "        this->section_id = 0;\n"
        "    }\n"
        "\n"
        "    this->section_name = section.c_str();\n"
        "}\n"
        "\n"
        "void Inifile::ConfigurationHolder::set_value(zstring_view key, zstring_view value) {\n"
        "    if (0) {}\n"
    ;
    id = 1;
    for (auto & section : writer.sections_parser) {
        // all members are attr::external
        out_set_value <<
            "    else if (this->section_id == " << id << ") {\n"
        ;
        if (!section.body.empty()) {
            out_set_value <<
                "        if (0) {}\n" << section.body << "\n"
                "        else if (static_cast<cfg::debug::config>(this->variables).value) {\n"
                "            LOG(LOG_WARNING, \"unknown parameter %s in section [%s]\",\n"
                "                key, this->section_name);\n"
                "        }\n"
            ;
        }
        else {
            out_set_value <<
                "        // all members are external\n"
            ;
        }
        out_set_value <<
            "    }\n"
        ;
        ++id;
    }
    out_set_value <<
        "    else if (static_cast<cfg::debug::config>(this->variables).value) {\n"
        "        LOG(LOG_WARNING, \"unknown section [%s]\", this->section_name);\n"
        "    }\n"
        "}\n"
    ;
}

inline void write_ini_values(std::ostream& out, CppConfigWriterBase& writer)
{
    out << cpp_comment(do_not_edit, 0) <<
        "\n"
        "#pragma once\n"
        "\n"
        "#include \"configs/autogen/variables_configuration_fwd.hpp\"\n"
        "#include \"utils/sugar/zstring_view.hpp\"\n"
        "\n"
        "namespace configs::cfg_ini_infos {\n"
        "using IniPack = Pack<\n"
        // remove trailing comma
        << std::string_view(writer.cfg_values.data(), writer.cfg_values.size() - 2) <<
        "\n>;\n"
        "\n"
        "struct SectionAndName { zstring_view section; zstring_view name; };\n"
        "constexpr SectionAndName const ini_names[] = {\n"
        << writer.cfg_str_values <<
        "};\n"
        "} // namespace configs::cfg_ini_infos\n"
    ;
}

inline void write_acl_and_spec_type(std::ostream & out, CppConfigWriterBase& writer)
{
    out << cpp_comment(do_not_edit, 0) <<
        "\n"
        "#pragma once\n"
        "\n"
        "#include \"configs/autogen/variables_configuration_fwd.hpp\"\n"
        "\n"
        "namespace configs {\n"
        "template<class Cfg> struct acl_and_spec_type {};\n"
        "\n"
        << writer.out_acl_and_spec_types.str()
        << "\n}\n"
    ;
}

inline void write_max_str_buffer_size_hpp(std::ostream & out, CppConfigWriterBase& writer)
{
    out << cpp_comment(do_not_edit, 0) <<
        "\n"
        "#pragma once\n"
        "\n"
        "namespace configs {\n"
        "    inline constexpr std::size_t max_str_buffer_size = "
        << writer.max_str_buffer_size
        << ";\n}\n"
    ;
}

}


template<class TInt>
inline constexpr std::size_t integral_buffer_size_v
  = std::numeric_limits<std::enable_if_t<std::is_integral_v<TInt>, TInt>>::digits10
    + 1 + std::numeric_limits<TInt>::is_signed;

template<class T, class Ratio>
inline constexpr std::size_t integral_buffer_size_v<std::chrono::duration<T, Ratio>> = integral_buffer_size_v<T>;

template<>
inline constexpr std::size_t integral_buffer_size_v<types::unsigned_> = integral_buffer_size_v<unsigned>;

template<>
inline constexpr std::size_t integral_buffer_size_v<types::int_> = integral_buffer_size_v<int>;

template<>
inline constexpr std::size_t integral_buffer_size_v<types::u16> = integral_buffer_size_v<uint16_t>;

template<>
inline constexpr std::size_t integral_buffer_size_v<types::u32> = integral_buffer_size_v<uint32_t>;

template<>
inline constexpr std::size_t integral_buffer_size_v<types::u64> = integral_buffer_size_v<uint64_t>;

template<class T>
struct type_
{
    using type = T;
};

template<class T>
std::string_view json_subtype(type_<T>)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return "string"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::milliseconds>) {
        return "milliseconds"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::duration<unsigned, std::ratio<1, 100>>>) {
        return "centiseconds"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::duration<unsigned, std::ratio<1, 10>>>) {
        return "deciseconds"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::seconds>) {
        return "seconds"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::minutes>) {
        return "minutes"sv;
    }
    else if constexpr (std::is_same_v<T, std::chrono::hours>) {
        return "hours"sv;
    }
    else if constexpr (std::is_same_v<T, types::int_>) {
        return "int"sv;
    }
    else if constexpr (std::is_same_v<T, types::unsigned_>) {
        return "uint"sv;
    }
    else if constexpr (std::is_same_v<T, types::u8>) {
        return "u8"sv;
    }
    else if constexpr (std::is_same_v<T, types::u16>) {
        return "u16"sv;
    }
    else if constexpr (std::is_same_v<T, types::u32>) {
        return "u32"sv;
    }
    else if constexpr (std::is_same_v<T, types::u64>) {
        return "u64"sv;
    }
    else {
        static_assert(!sizeof(T), "missing implementation");
    }
}

template<class T, class V>
ValueAsStrings compute_value_as_strings(type_<T>, V const& value)
{
    if constexpr (std::is_same_v<T, std::string>) {
        return ValueAsStrings{
            .prefix_spec_type = "string("s,
            .cpp_type = "std::string"s,
            .json_type = "str"sv,
            .spec_str_buffer_size = 0,
            .values = string_value_to_strings(value),
        };
    }
    else if constexpr (std::is_same_v<T, bool>) {
        return ValueAsStrings{
            .prefix_spec_type = "boolean("s,
            .cpp_type = "bool"s,
            .json_type = "bool"sv,
            .spec_str_buffer_size = 5,
            .values = bool_value_to_strings(value),
            .ini_note = "type: boolean (0/no/false or 1/yes/true)"s,
        };
    }
    else if constexpr (std::is_same_v<T, types::dirpath>) {
        return ValueAsStrings{
            .prefix_spec_type = str_concat("string(max="sv, path_max_as_str, ", "sv),
            .cpp_type = "::configs::spec_types::directory_path"s,
            .json_type = "dirpath"sv,
            .spec_str_buffer_size = 0,
            .values = string_value_to_strings(value),
            .ini_note = str_concat("maxlen = "sv, path_max_as_str),
        };
    }
    else if constexpr (std::is_same_v<T, types::ip_string>) {
        return ValueAsStrings{
            .prefix_spec_type = "ip_addr("s,
            .cpp_type = "std::string"s,
            .json_type = "ipaddr"sv,
            .spec_type = "::configs::spec_types::ip"s,
            .spec_str_buffer_size = 0,
            .values = string_value_to_strings(value),
        };
    }
    else if constexpr (std::is_same_v<T, types::rgb>) {
        return ValueAsStrings{
            .prefix_spec_type = "string("s,
            .cpp_type = "::configs::spec_types::rgb"s,
            .json_type = "rgb"sv,
            .spec_str_buffer_size = 7,
            .values = color_value_to_strings(value),
            .spec_note = "in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a <a href=\"https://en.wikipedia.org/wiki/Web_colors#Extended_colors\">named color</a> case insensitive (red, skyBlue, etc)"s,
            .ini_note = "in rgb format: hexadecimal (0x21AF21), #rgb (#2fa), #rrggbb (#22ffaa) or a named color case insensitive (\"https://en.wikipedia.org/wiki/Web_colors#Extended_colors\")"s,
        };
    }
    else if constexpr (std::is_same_v<T, FilePermissions>) {
        return ValueAsStrings{
            .prefix_spec_type = "string("s,
            .cpp_type = "FilePermissions"s,
            .json_type = "FilePermissions"sv,
            .spec_str_buffer_size = integral_buffer_size_v<uint16_t>,
            .values = file_permission_value_to_strings(value),
            .spec_note = "in octal or symbolic mode format (as chmod Linux command)"s,
        };
    }
    else if constexpr (std::is_same_v<T, ScreenResolution>) {
        return ValueAsStrings{
            .prefix_spec_type = "string("s,
            .cpp_type = "ScreenResolution"s,
            .json_type = "ScreenResolution"sv,
            .spec_str_buffer_size = integral_buffer_size_v<uint16_t> * 2 + 1,
            .values = screen_resolution_value_to_strings(value),
            .spec_note = "in {width}x{height} format (e.g. 800x600)"s,
        };
    }
    else if constexpr (std::is_base_of_v<impl::unsigned_base, T>) {
        return ValueAsStrings{
            .prefix_spec_type = "integer(min=0, "s,
            .cpp_type = std::string(type_name<T>()),
            .json_type = json_subtype(type_<T>()),
            .spec_str_buffer_size = integral_buffer_size_v<T>,
            // TODO hexadecimal when SpecAttributes::hex
            .values = integer_value_to_strings(value),
            .ini_note = "min = 0"s,
        };
    }
    else if constexpr (std::is_base_of_v<impl::signed_base, T>) {
        return ValueAsStrings{
            .prefix_spec_type = "integer("s,
            .cpp_type = std::string(type_name<T>()),
            .json_type = json_subtype(type_<T>()),
            .spec_str_buffer_size = integral_buffer_size_v<T>,
            // TODO hexadecimal when SpecAttributes::hex
            .values = integer_value_to_strings(value),
        };
    }
    // TODO remove that when redirection_password_or_cookie is removed
    else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
        return ValueAsStrings{
            .prefix_spec_type = {},
            .cpp_type = "std::vector<uint8_t>"s,
            .json_type = "list"sv,
            .json_extra_type = "\"subtype\": \"u8\""s,
            .spec_str_buffer_size = 0,
            .values = {
                .py = {},
                .ini = {},
                .json = "\"\""s,
                .cpp = {},
            },
        };
    }
    else {
        static_assert(!sizeof(T), "missing implementation");
        return {};
    }
}

template<class T, std::intmax_t Num, std::intmax_t Denom, class V>
ValueAsStrings compute_value_as_strings(
    type_<std::chrono::duration<T, std::ratio<Num, Denom>>>, V const& value)
{
    std::string_view note;

    if constexpr (Num == 1 && Denom == 1000) {
        note = "in milliseconds"sv;
    }
    else if constexpr (Num == 1 && Denom == 100) {
        static_assert(std::is_same_v<T, unsigned>);
        note = "in 1/100 seconds"sv;
    }
    else if constexpr (Num == 1 && Denom == 10) {
        static_assert(std::is_same_v<T, unsigned>);
        note = "in 1/10 seconds"sv;
    }
    else if constexpr (Num == 1 && Denom == 1) {
        note = "in seconds"sv;
    }
    else if constexpr (Num == 60 && Denom == 1) {
        note = "in minutes"sv;
    }
    else if constexpr (Num == 3600 && Denom == 1) {
        note = "in hours"sv;
    }
    else {
        static_assert(!Num && !Denom, "missing implementation");
    }

    using Duration = std::chrono::duration<T, std::ratio<Num, Denom>>;

    return ValueAsStrings{
        .prefix_spec_type = "integer(min=0, "s,
        .cpp_type = std::string(type_name<Duration>()),
        .json_type = json_subtype(type_<Duration>()),
        .spec_str_buffer_size = integral_buffer_size_v<T>,
        .values = integer_value_to_strings(value),
        .spec_note = std::string(note),
        .acl_diag_note = {},
    };
}

template<unsigned N, class V>
ValueAsStrings compute_value_as_strings(type_<types::fixed_string<N>>, V const& value)
{
    auto d = int_to_decimal_chars(N);
    return ValueAsStrings{
        .prefix_spec_type = str_concat("string(max="sv, d, ", "sv),
        .cpp_type = str_concat("char["sv, d, "+1]"sv),
        .json_type = "str"sv,
        .json_extra_type = str_concat("\"minlen\": "sv, d, ", \"maxlen\": "sv, d),
        .spec_type = "::configs::spec_types::fixed_string"s,
        .acl_diag_type = str_concat("std::string(maxlen="sv, d, ")"sv),
        .spec_str_buffer_size = 0,
        .values = string_value_to_strings(value),
        .ini_note = str_concat("maxlen = "sv, d),
    };
}

template<unsigned N, class V>
ValueAsStrings compute_value_as_strings(type_<types::fixed_binary<N>>, V const& value)
{
    auto d = int_to_decimal_chars(N);
    return ValueAsStrings{
        .prefix_spec_type = str_concat("string(min="sv, d, ", max="sv, d, ", "sv),
        .cpp_type = str_concat("std::array<unsigned char, "sv, d, '>'),
        .json_type = "bytes"sv,
        .json_extra_type = str_concat("\"minlen\": "sv, d, ", \"maxlen\": "sv, d),
        .spec_type = "::configs::spec_types::fixed_binary"s,
        .spec_str_buffer_size = N * 2,
        .values = binary_string_value_to_strings<N>(value),
        .spec_note = "in hexadecimal format"s,
        .ini_note = str_concat("hexadecimal string of length "sv, d),
    };
}

template<class T, class V>
ValueAsStrings compute_value_as_strings(type_<types::list<T>>, V const& value)
{
    return ValueAsStrings{
        .prefix_spec_type = "string(",
        .cpp_type = "std::string",
        .json_type = "list"sv,
        .json_extra_type = str_concat("\"subtype\": \""sv, json_subtype(type_<T>()), '"'),
        .spec_type = str_concat("::configs::spec_types::list<"sv, type_name<T>(), '>'),
        .spec_str_buffer_size = 0,
        .values = string_value_to_strings(value),
        .spec_note = "values are comma-separated"s
    };
}

template<class T, class V>
ValueAsStrings compute_value_as_strings(type_<types::mebibytes<T>>, V const& value)
{
    auto res = compute_value_as_strings(type_<T>(), value);
    auto note = "in megabytes"sv;
    res.json_type = "mebibytes"sv;
    res.spec_note = std::string(note);
    res.ini_note = std::string(note);
    res.acl_diag_note = std::string(note);
    res.cpp_note = std::string(note);
    return res;
}

template<class Int, long min, long max, class V>
ValueAsStrings compute_value_as_strings(type_<types::range<Int, min, max>>, V const& value)
{
    std::string spec_note;
    chars_view sep = ""_av;

    auto smin = int_to_decimal_chars(min);
    auto smax = int_to_decimal_chars(max);

    if constexpr (!std::is_base_of_v<impl::unsigned_base, Int>) {
        spec_note = compute_value_as_strings(type_<Int>(), Int()).ini_note;
        if (!spec_note.empty()) {
            sep = " | "_av;
        }
    }

    return ValueAsStrings{
        .prefix_spec_type = str_concat("integer(min="sv, smin, ", max="sv, smax, ", "sv),
        .cpp_type = std::string(type_name<Int>()),
        .json_type = json_subtype(type_<Int>()),
        .json_extra_type = str_concat("\"min\": "sv, smin, ", \"max\": "sv, smax),
        .spec_type = str_concat("::configs::spec_types::range<"sv, type_name<Int>(), ", "sv, smin, ", "sv, smax, ">"sv),
        .spec_str_buffer_size = integral_buffer_size_v<Int>,
        // TODO hexconverter
        .values = integer_value_to_strings(value),
        .spec_note = spec_note,
        .ini_note = str_concat(spec_note, sep, "min = "sv, smin, ", max = "sv, smax),
    };
}

inline ValueAsStrings compute_value_as_strings(type_<types::performance_flags>, std::string_view value)
{
    RdpPerformanceFlags performace_flags {};
    char const* err = parse_performance_flags(performace_flags, value);
    if (err) {
        throw std::runtime_error(err);
    }

    auto force_present = int_to_hexadecimal_lower_chars(performace_flags.force_present);
    auto force_not_present = int_to_hexadecimal_lower_chars(performace_flags.force_not_present);

    return ValueAsStrings{
        .prefix_spec_type = "string("s,
        .cpp_type = "RdpPerformanceFlags"s,
        .json_type = "PerformanceFlags"sv,
        .spec_str_buffer_size = 0,
        .values = {
            .py = str_concat('"', value, '"'),
            .ini = std::string(value),
            .cpp = str_concat("0x"sv, force_present, ", 0x"sv, force_not_present),
        },
    };
}


inline std::string enum_options_to_prefix_spec_type(type_enumeration const& e)
{
    if (e.cat != type_enumeration::Category::autoincrement
        && e.cat != type_enumeration::Category::set
    ) {
        throw std::runtime_error("enum parser available only with set or autoincrement enum");
    }

    std::string ret;
    ret.reserve(127);
    ret += "option("sv;
    for (type_enumeration::value_type const & v : e.values) {
        switch (v.prop) {
        case type_enumeration::Prop::Value:
            str_append(ret, '\'', v.get_name(), "', "sv);
            break;

        case type_enumeration::Prop::NoValue:
        case type_enumeration::Prop::Reserved:
            break;
        }
    }
    return ret;
}

inline bool enum_values_has_desc(type_enumeration const& e)
{
    return std::any_of(begin(e.values), end(e.values),
        [](type_enumeration::value_type const & v) { return !v.desc.empty(); });
}

inline std::string enum_desc_with_values_in_desc(type_enumeration const& e, bool use_disable_prefix)
{
    std::string str;
    str.reserve(128);
    for (type_enumeration::value_type const & v : e.values) {
        switch (v.prop) {
        case type_enumeration::Prop::Value:
            str += "  ";
            str += v.get_name();
            str += ": ";
            if (use_disable_prefix) {
                str += "disable ";
            }
            str += v.desc;
            str += '\n';
            break;

        case type_enumeration::Prop::NoValue:
        case type_enumeration::Prop::Reserved:
            break;
        }
    }
    str += e.info;
    return str;
}

inline std::string enum_as_string_ini_desc(type_enumeration const& e)
{
    std::string str;
    str.reserve(128);
    std::string_view prefix = "values: ";
    for (type_enumeration::value_type const & v : e.values) {
        switch (v.prop) {
        case type_enumeration::Prop::Value:
            str += prefix;
            str += v.get_name();
            prefix = ", ";
            break;

        case type_enumeration::Prop::NoValue:
        case type_enumeration::Prop::Reserved:
            break;
        }
    }
    str += e.info;
    return str;
}

inline std::string_view get_enum_value_data(uint64_t value, type_enumeration const& e)
{
    for (auto& v : e.values) {
        if (v.val == value) {
            return v.get_name();
        }
    }
    throw std::runtime_error(str_concat(
        "unknown value "sv, int_to_decimal_chars(value), " for "sv, e.name
    ));
}

inline ValueAsStrings compute_string_enum_as_strings(uint64_t value, type_enumeration const& e)
{
    auto name = get_enum_value_data(value, e);
    return ValueAsStrings{
        .prefix_spec_type = enum_options_to_prefix_spec_type(e),
        .cpp_type = std::string(e.name),
        .json_type = "enumStr"sv,
        .json_extra_type = str_concat("\"subtype\": \""sv, e.name, '"'),
        .spec_type = "std::string"s,
        .acl_diag_type = "std::string"s,
        .spec_str_buffer_size = 0,
        .values = {
            .py = str_concat('"', name, '"'),
            .ini = std::string(name),
            .cpp = str_concat(e.name, "::"sv, name),
        },
        .enumeration = &e,
        .string_parser_for_enum = true,
    };
}


inline std::string enum_to_prefix_spec_type(type_enumeration const& e)
{
    switch (e.cat) {
        case type_enumeration::Category::flags:
            return str_concat("integer(min=0, max="sv, int_to_decimal_chars(e.max()), ", "sv);

        case type_enumeration::Category::autoincrement:
        case type_enumeration::Category::set:
            break;
    }

    std::string ret;
    ret.reserve(63);
    ret += "option(";
    for (type_enumeration::value_type const & v : e.values) {
        switch (v.prop) {
        case type_enumeration::Prop::Value:
            str_append(ret, int_to_decimal_chars(v.val), ", "sv);
            break;

        case type_enumeration::Prop::NoValue:
        case type_enumeration::Prop::Reserved:
            break;
        }
    }
    return ret;
}

inline std::string numeric_enum_desc(type_enumeration const& e, bool use_disable_prefix)
{
    std::string str;
    std::size_t ndigit16 = (e.cat == type_enumeration::Category::flags)
      ? static_cast<std::size_t>(64 - __builtin_clzll(e.max()) + 4 - 1) / 4
      : 0;
    bool show_name_when_description
      = (e.display_name_option == type_enumeration::DisplayNameOption::WithNameWhenDescription);

    auto show_values = [&](bool hexa)
    {
        for (type_enumeration::value_type const & v : e.values) {
            switch (v.prop) {
            case type_enumeration::Prop::Value: {
                str += "  ";
                if (hexa) {
                    auto buf = int_to_fixed_hexadecimal_lower_chars(v.val);
                    str += "0x";
                    str += chars_view(buf).last(ndigit16).as<std::string_view>();
                }
                else {
                    str += int_to_decimal_chars(v.val).sv();
                }

                // skip "disable none" text
                auto prefix = (use_disable_prefix && (!v.desc.empty() || v.val))
                    ? ": disable "sv
                    : ": "sv;

                if (v.desc.empty() || show_name_when_description) {
                    str += prefix;
                    for (char c : v.get_name()) {
                        str += ('_' == c ? ' ' : c);
                    }
                }

                if (!v.desc.empty()) {
                    str += prefix;
                    str += v.desc;
                }

                str += '\n';
                break;
            }

            case type_enumeration::Prop::NoValue:
            case type_enumeration::Prop::Reserved:
                break;
            }
        }
    };

    switch (e.cat) {
        case type_enumeration::Category::set:
        case type_enumeration::Category::autoincrement:
            show_values(false);
            break;

        case type_enumeration::Category::flags: {
            show_values(true);
            std::string note;
            uint64_t total = 0;
            for (type_enumeration::value_type const & v : e.values) {
                switch (v.prop) {
                case type_enumeration::Prop::Value:
                    if (v.val) {
                        total |= v.val;
                        note += "0x";
                        note += int_to_hexadecimal_lower_chars(v.val);
                        note += " + ";
                    }
                    break;

                case type_enumeration::Prop::NoValue:
                case type_enumeration::Prop::Reserved:
                    break;
                }
            }

            note[note.size() - 2] = '=';
            str_append(str, "\nNote: values can be added ("sv,
                (use_disable_prefix ? "disable all: "sv : "enable all: "sv),
                note, "0x"sv, int_to_hexadecimal_lower_chars(total), ')');
        }
    }

    str += e.info;
    return str;
}

inline std::string enum_to_cpp_string(
    uint64_t value, type_enumeration const& e, int_to_chars_result const& int_converted)
{
    try {
        return str_concat(e.name, "::"sv, get_enum_value_data(value, e));
    }
    catch (std::exception const&) {
        if (e.cat != type_enumeration::Category::flags) {
            throw;
        }
        // ignore error and convert to integer
    }
    return str_concat(e.name, '{', int_converted, '}');
}

inline ValueAsStrings compute_integer_enum_as_strings(uint64_t value, type_enumeration const& e)
{
    auto d = int_to_decimal_chars(value);
    return ValueAsStrings{
        .prefix_spec_type = enum_to_prefix_spec_type(e),
        .cpp_type = std::string(e.name),
        .json_type = "enum",
        .json_extra_type = str_concat("\"subtype\": \""sv, e.name, '"'),
        .spec_str_buffer_size = integral_buffer_size_v<uint64_t>,
        .values = {
            .py = std::string(d.sv()),
            .ini = std::string(d.sv()),
            .cpp = enum_to_cpp_string(value, e, d),
        },
        .enumeration = &e,
    };
}


template<class T>
struct ConnectionPolicyValue
{
    DestSpecFile policy_types;
    T value;
    bool forced;

    ConnectionPolicyValue<T> always()
    {
        return {policy_types, static_cast<T&&>(value), true};
    }
};

template<class T, class Decayed> struct minify_type { using type = Decayed; };
template<class T> struct minify_type<T, char*> { using type = std::string_view; };
template<class T> struct minify_type<T, char const*> { using type = std::string_view; };
template<class T> struct minify_type<T, std::string> { using type = std::string_view; };

template<class T> using minify_type_t = typename minify_type<T, std::decay_t<T>>::type;


template<class T>
ConnectionPolicyValue<minify_type_t<T>> rdp_general_policy_value(T&& value)
{
    return {DestSpecFile::rdp, static_cast<T&&>(value), false};
}

template<class T>
ConnectionPolicyValue<minify_type_t<T>> rdp_sogisces_1_3_2030_policy_value(T&& value)
{
    return {DestSpecFile::rdp_sogisces_1_3_2030, static_cast<T&&>(value), false};
}

template<class T>
ConnectionPolicyValue<minify_type_t<T>> rdp_all_policy_value(T&& value)
{
    return {
        DestSpecFile::rdp | DestSpecFile::rdp_sogisces_1_3_2030,
        static_cast<T&&>(value), false
    };
}

template<class T>
ConnectionPolicyValue<minify_type_t<T>> vnc_policy_value(T&& value)
{
    return {DestSpecFile::vnc, static_cast<T&&>(value), false};
}

inline DataAsStrings::ConnectionPolicy make_connpolicy_value(
    DestSpecFile dest_file, ValueAsStrings&& value_as_string, bool forced)
{
    return DataAsStrings::ConnectionPolicy{
        .dest_file = dest_file,
        .py = std::move(value_as_string.values.py),
        .json = std::move(value_as_string.values.json),
        .forced = forced,
    };
}

struct EnumAsString
{
    type_enumerations const& tenums;

    template<class Enum, class... TConnPolicy>
    ValueAsStrings operator()(Enum value, TConnPolicy&&... conn_policy_value)
    {
        static_assert(std::is_enum_v<Enum>);

        check_policy({conn_policy_value.policy_types...});

        auto& e = tenums.get_enum<Enum>();
        auto ret = compute_string_enum_as_strings(safe_int(underlying_cast(value)), e);

        (..., (ret.values.connection_policies.push_back(make_connpolicy_value(
            conn_policy_value.policy_types,
            compute_string_enum_as_strings(
                safe_int(underlying_cast(Enum{conn_policy_value.value})),
                e
            ),
            conn_policy_value.forced
        ))));

        ret.align_of = alignof(Enum);
        return ret;
    }
};

struct ValueFromEnum
{
    type_enumerations const& tenums;

    template<class Enum, class... TConnPolicy>
    ValueAsStrings operator()(Enum value, TConnPolicy&&... conn_policy_value)
    {
        static_assert(std::is_enum_v<Enum>);

        check_policy({conn_policy_value.policy_types...});

        auto& e = tenums.get_enum<Enum>();
        auto ret = compute_integer_enum_as_strings(safe_int(underlying_cast(value)), e);

        (..., (ret.values.connection_policies.push_back(make_connpolicy_value(
            conn_policy_value.policy_types,
            compute_integer_enum_as_strings(
                safe_int(underlying_cast(Enum{conn_policy_value.value})),
                e
            ),
            conn_policy_value.forced
        ))));

        ret.align_of = alignof(Enum);
        return ret;
    }
};

template<class T = void, class U = T, class... TConnPolicy>
ValueAsStrings value(U const& value = T(), TConnPolicy const&... conn_policy_value)
{
    static_assert(!std::is_enum_v<T>, "uses EnumAsString or ValueFromEnum");
    TYPE_REQUIEREMENT(T);

    using ty = std::conditional_t<std::is_void_v<T> && std::is_same_v<U, bool>, bool, T>;
    static_assert(!std::is_void_v<ty>, "T not specified");

    check_policy({conn_policy_value.policy_types...});

    auto ret = compute_value_as_strings(type_<ty>(), static_cast<minify_type_t<U> const&>(value));

    (..., (ret.values.connection_policies.push_back(make_connpolicy_value(
        conn_policy_value.policy_types,
        compute_value_as_strings(type_<ty>(), conn_policy_value.value),
        conn_policy_value.forced
    ))));

    ret.align_of = alignof(ty);
    return ret;
}

inline void inplace_upper(char& c)
{
    c = ('a' <= c && c <= 'z') ? c + 'A' - 'a' : c;
}

inline constexpr std::string_view workaround_message = "⚠ The use of this feature is not recommended!\n\n";

inline void add_attr_spec(std::string& out, std::string_view image_path, SpecAttributes attr)
{
    if (bool(attr & SpecAttributes::iptables)) out += "#_iptables\n"sv;
    if (bool(attr & SpecAttributes::advanced)) out += "#_advanced\n"sv;
    if (bool(attr & SpecAttributes::hex))      out += "#_hex\n"sv;
    if (bool(attr & SpecAttributes::password)) out += "#_password\n"sv;
    if (bool(attr & SpecAttributes::image))    str_append(out, "#_image="sv, image_path, '\n');
    if (bool(attr & SpecAttributes::adminkit)) out += "#_adminkit\n"sv;
    if (bool(attr & SpecAttributes::logged))   out += "#_logged\n"sv;
}

inline void add_prefered_display_name(std::string& out, Names const& names)
{
    if (!names.display.empty()) {
        out += "#_display_name="sv;
        out += names.display;
        out += '\n';
    }
}

inline void add_comment(std::string& out, std::string_view str, std::string_view comment)
{
    if (str.empty()) {
        return ;
    }

    out += comment;
    for (char c : str) {
        out += c;
        if (c == '\n') {
            out += comment;
        }
    }

    if (str.back() == '\n') {
        out.resize(out.size() - comment.size());
    }
    else {
        out += '\n';
    }
}

static std::string htmlize(std::string str)
{
    std::string html;
    html.reserve(str.capacity());

    // trim new lines
    while (!str.empty() && str.back() == '\n') {
        str.pop_back();
    }
    zstring_view zstr = str;
    while (zstr[0] == '\n') {
        zstr.pop_front();
    }

    // replace '&' with "&amp;" and
    // replace '<' with "&lt;" when not followed by / or a tag
    for (char const& c : zstr) {
        if (c == '&') {
            html += "&amp;";
        }
        // TODO should be replaced with a system that describes richtext
        else if (c == '<' && zstr.end() - 1 != &c && *(&c + 1) != '/' && *(&c + 1) != 'a') {
            html += "&lt;";
        }
        else {
            html += c;
        }
    }

    str.clear();
    html.swap(str);

    // replace "\n\n" with "<br/>\n"
    html.reserve(str.size());
    for (char const& c : str) {
        if (c == '\n' && (&c)[1] == '\n') {
            html += "<br/>";
        }
        else {
            html += c;
        }
    }

    str.clear();
    html.swap(str);

    // replace "  " at start line with "&nbsp; ;&nbsp; "
    if (str[0] == ' ' && str[1] == ' ') {
        html += "&nbsp; &nbsp; ";
    }
    for (char const& c : str) {
        if (c == '\n' && (&c)[1] == ' ' && (&c)[2] == ' ') {
            html += "\n&nbsp; &nbsp; ";
        }
        else {
            html += c;
        }
    }

    return html;
}

static void check_names(
    cfg_desc::names const& names,
    bool has_ini, bool has_acl, bool has_connpolicy)
{
    if (!has_ini && !names.ini.empty())
        throw std::runtime_error(str_concat("names.ini without ini for "sv, names.all));

    if (!has_acl && !names.acl.empty())
        throw std::runtime_error(str_concat("names.acl without acl for "sv, names.all));

    // if (has_acl && !names.connpolicy.empty())
    //     throw std::runtime_error(str_concat("names.connpolicy with acl for "sv, names.all));

    if (!has_connpolicy && !names.connpolicy.empty())
        throw std::runtime_error(str_concat("names.connpolicy without connpolicy for "sv, names.all));

    if (has_connpolicy && !names.acl.empty())
        throw std::runtime_error(str_concat("names.acl with connpolicy for "sv, names.all));

    if (!(has_ini || has_connpolicy) && !names.display.empty())
        throw std::runtime_error(str_concat("names.display without ini or connpolicy for "sv, names.all));
}

struct Appender
{
    std::string& str;

    template<class... T>
    void operator()(T const&... frags) const
    {
        (..., (str += frags));
    }

    void add_paragraph(std::string_view para)
    {
        if (!para.empty()) {
            str += para;
            str += '\n';
        }
    }
};

struct Marker
{
    std::string* str;
    std::size_t position;

    Marker(std::string& str)
    : str(&str)
    , position(str.size())
    {}

    std::string cut()
    {
        std::string ret(str->data() + position, str->size() - position);
        str->resize(position);
        return ret;
    }
};

class GeneratorConfig
{
    struct File
    {
        std::string filename;
        std::ofstream out {filename};
    };

    struct SesmanMap
    {
        std::string filename;
        std::ofstream out {filename};
        std::vector<std::string> values_sent {};
        std::vector<std::string> values_reinit {};
    };

    struct SesmanDiag
    {
        std::string filename;
        std::ofstream out {filename};
        std::string buffer {};
    };

    struct Policy
    {
        using data_by_section_t = std::unordered_map<std::string_view, std::string>;
        struct SesmanInfo
        {
            data_by_section_t sections;
            std::string hidden_values;
        };

        std::string directory_spec;
        std::string remap_filename;
        std::unordered_map<DestSpecFile, data_by_section_t> spec_file_map {};
        std::unordered_map<DestSpecFile, SesmanInfo> acl_map {};
        std::vector<std::string> ordered_section {};
        std::unordered_set<std::string> section_names {};
        std::map<std::string, std::string> hidden_values {};
        std::unordered_set<std::string> acl_mems {};
    };

    struct SectionData
    {
        Names names;
        std::string global_spec {};
        std::string ini {};
    };

    File spec;
    File ini;
    File json;
    SesmanMap acl_map;
    SesmanDiag acl_diag;
    Policy policy;
    cpp_config_writer::CppConfigWriterBase cpp;
    std::string json_values;

    std::unordered_map<std::string_view, SectionData> sections;

    std::vector<std::string_view> ordered_section_names;

public:
    GeneratorConfig(
        std::string ini_filename,
        std::string ini_spec_filename,
        std::string json_filename,
        std::string acl_map_filename,
        std::string acl_diag_filename,
        std::string directory_spec,
        std::string acl_remap_filename,
        cpp_config_writer::CppConfigWriterBase::Filenames filenames
    )
    : spec{std::move(ini_spec_filename)}
    , ini{std::move(ini_filename)}
    , json{std::move(json_filename)}
    , acl_map{std::move(acl_map_filename)}
    , acl_diag{std::move(acl_diag_filename)}
    , policy{std::move(directory_spec), std::move(acl_remap_filename)}
    , cpp(std::move(filenames))
    {
        json_values.reserve(1024 * 64);

        acl_map.out << python_comment(do_not_edit, 0) << "\n";

        acl_diag.buffer += do_not_edit;
        acl_diag.buffer += "\n       cpp name       |       acl / passthrough name\n\n"sv;
    }

    SectionData& get_section(Names const& section_names)
    {
        auto it = sections.find(section_names.all);
        if (it != sections.end()) {
            return it->second;
        }
        ordered_section_names.emplace_back(section_names.all);
        return sections.insert({section_names.all, SectionData{section_names}}).first->second;
    }

    int do_finish()
    {
        int err = 0;
        auto check_file = [&](std::string_view filename, std::ostream& f){
            f.flush();
            if (f) {
                return ;
            }

            int errnum = errno;
            std::cerr << filename << ": " << strerror(errnum) << "\n";
            ++err;
        };

        spec.out
            << "#include \"config_variant.hpp\"\n\n"
            << begin_raw_string
            << "## Python spec file for RDP proxy.\n\n\n"
        ;

        ini.out
            << "#include \"config_variant.hpp\"\n\n"
            << begin_raw_string
            << "## Config file for RDP proxy.\n\n\n"
        ;

        for (File* f : {&spec, &ini}) {
            for (std::string_view section_name : ordered_section_names) {
                auto& section = sections.find(section_name)->second;
                std::string_view str = (f == &spec) ? section.global_spec : section.ini;
                if (!str.empty()) {
                    f->out << "[" << section.names.ini_name() << "]\n\n" << str;
                }
            }

            f->out << end_raw_string << "\n";
            check_file(f->filename, f->out);
        }


        json.out << "{\n\"tags\": [";
        for (std::size_t i = 0; i < Tags::max; ++i) {
            json.out << '"' << tag_to_sv(Tag(i)) << "\","sv;
        }
        json.out << "\"SOG-IS\"],\n\"sections\": [\n";
        std::string_view json_sep = {};
        for (std::string_view section_name : ordered_section_names) {
            auto& section = sections.find(section_name)->second;
            json.out << json_sep << "  {\"name\": \"" << section.names.all << "\"";
            if (!section.names.connpolicy.empty()) {
                json.out << ", \"connpolicyName\": \"" << section.names.connpolicy << "\"";
            }
            json.out << "}";
            json_sep = ",\n";
        }
        json.out << "\n],\n  \"values\": [\n" << json_values << "\n]\n}\n";
        check_file(json.filename, json.out);


        std::sort(acl_map.values_sent.begin(), acl_map.values_sent.end());
        std::sort(acl_map.values_reinit.begin(), acl_map.values_reinit.end());
        auto write = [&](char const* name, std::vector<std::string> const& values){
            acl_map.out << name << " = {\n";
            for (auto&& x : values)
            {
                acl_map.out << x;
            }
            acl_map.out << "}\n";
        };
        write("back_to_selector_default_sent", acl_map.values_sent);
        write("back_to_selector_default_reinit", acl_map.values_reinit);
        check_file(acl_map.filename, acl_map.out);


        acl_diag.out << acl_diag.buffer;
        check_file(acl_diag.filename, acl_diag.out);


        std::ofstream out(policy.remap_filename);

        out << python_comment(do_not_edit, 0) << "\n"
            << "cp_spec = {\n"
        ;

        for (auto dest_file : conn_policies) {
            if (!bool(dest_file & conn_policies_acl_map_mask)) {
                continue;
            }
            auto const& sections_by_file = *policy.acl_map.find(dest_file);
            auto const& sections = sections_by_file.second.sections;
            out << "'" << dest_file_to_filename(sections_by_file.first) << "': ({\n";
            for (auto const& section_name : policy.ordered_section) {
                auto section_it = sections.find(section_name);
                if (section_it != sections.end()) {
                    out
                        << "    '" << section_name << "': {\n"
                        << section_it->second << "    },\n"
                    ;
                }
            }
            out << "}, {\n" << sections_by_file.second.hidden_values << "}),\n";
        }

        out << "}\n";

        if (!out.flush()) {
            std::cerr << "ConnectionPolicyWriterBase: " << policy.remap_filename << ": " << strerror(errno) << "\n";
            return 1;
        }

        for (auto const& [cat, section_map] : policy.spec_file_map) {
            auto const spec_filename = str_concat(
                policy.directory_spec, '/', dest_file_to_filename(cat), ".spec"sv);

            std::ofstream out_spec(spec_filename);

            for (auto& section_name : policy.ordered_section) {
                auto section_it = section_map.find(section_name);
                if (section_it != section_map.end()) {
                    out_spec
                        << "[" << section_name << "]\n\n"
                        << section_it->second
                    ;
                }
            }

            if (!out_spec.flush()) {
                std::cerr << "ConnectionPolicyWriterBase: " << spec_filename << ": " << strerror(errno) << "\n";
                return 2;
            }
        }


        cpp.full_names.check();

        MultiFilenameWriter<cpp_config_writer::CppConfigWriterBase> sw(cpp);
        sw.then(cpp.filenames.authid_hpp, &cpp_config_writer::write_authid_hpp)
          .then(cpp.filenames.str_authid_hpp, &cpp_config_writer::write_str_authid_hpp)
          .then(cpp.filenames.variable_configuration_fwd, &cpp_config_writer::write_variables_configuration_fwd)
          .then(cpp.filenames.variable_configuration_hpp, &cpp_config_writer::write_variables_configuration)
          .then(cpp.filenames.config_set_value, &cpp_config_writer::write_config_set_value)
          .then(cpp.filenames.ini_values_hpp, &cpp_config_writer::write_ini_values)
          .then(cpp.filenames.acl_and_spec_type, &cpp_config_writer::write_acl_and_spec_type)
          .then(cpp.filenames.max_str_buffer_size_hpp, &cpp_config_writer::write_max_str_buffer_size_hpp)
        ;
        if (sw.err) {
            std::cerr << "CppConfigWriterBase: " << sw.filename << ": " << strerror(sw.errnum) << "\n";
            return sw.errnum;
        }

        return err;
    }

    void do_start_section(Names const& section_names)
    {
        get_section(section_names);

        if (!section_names.all.empty()) {
            ++cpp.depth;
        }
        cpp.start_section_index = cpp.authid_policies.size();
    }

    void do_stop_section(Names const& section_names)
    {
        acl_diag.buffer += '\n';

        if (!section_names.all.empty()) {
            --cpp.depth;
        }
        cpp.sections.emplace_back(cpp_config_writer::CppConfigWriterBase::Section{section_names.all, cpp.out_member_.str(), std::move(cpp.members)});
        cpp.out_member_.str("");
        std::string str = cpp.out_body_parser_.str();
        if (!str.empty()) {
            // all members are attr::external
            if (cpp.sections.back().members.empty()) {
                str.clear();
            }
            cpp.sections_parser.emplace_back(cpp_config_writer::CppConfigWriterBase::SectionString{section_names.all, str});
            cpp.out_body_parser_.str("");
        }
        cpp.start_indexes.emplace_back(cpp.authid_policies.size());
    }

    // check whether value or desc policy is specified, but not in spec info
    void check_connpolicy_integrity(MemberInfo const& mem_info)
    {
        DestSpecFile policy_types {};
        for (auto const& connpolicy : mem_info.value.values.connection_policies) {
            policy_types |= connpolicy.dest_file;
        }

        for (auto dest_file : conn_policies) {
            // not in connpolicy
            if (!bool(dest_file & mem_info.spec.dest)) {
                // but value specified
                if (bool(dest_file & policy_types)) {
                    throw std::runtime_error(str_concat(
                        "Connpolicy value is specified, but not in spec (for '",
                        mem_info.name.all,
                        "')"
                    ));
                }
                // but desc specified
                if (mem_info.desc.find_desc_for_policy(dest_file)) {
                    throw std::runtime_error(str_concat(
                        "Connpolicy description is specified, but not in spec (for '",
                        mem_info.name.all,
                        "')"
                    ));
                }
            }
        }
    }

    void evaluate_member(Section const& section_info, MemberInfo const& mem_info, ConfigInfo const& conf)
    {
        check_connpolicy_integrity(mem_info);

        std::string spec_desc {};
        std::string ini_desc {};

        Names const& section_names = section_info.names;
        auto& section = get_section(section_names);

        Names const& names = mem_info.name;

        desc_ref_replacer.init(mem_info.desc.general_desc, section_info, mem_info, conf);

        bool const has_spec = mem_info.spec.has_spec();
        bool const has_ini = mem_info.spec.has_ini();
        bool const has_global_spec = mem_info.spec.has_global_spec();
        bool const has_connpolicy = mem_info.spec.has_connpolicy();

        auto const acl_io = mem_info.spec.acl_io;
        bool const has_acl = mem_info.spec.has_acl();

        check_names(mem_info.name, has_ini, has_acl, has_connpolicy);

        std::string acl_network_name_tmp;
        std::string_view const acl_network_name
            = (has_connpolicy
               || (bool(mem_info.spec.dest & DestSpecFile::ini_only) && names.acl.empty()))
            ? (acl_network_name_tmp = str_concat(section_names.all, ':', names.all))
            : names.acl_name();

        if (has_spec
         || mem_info.spec.reset_back_to_selector == ResetBackToSelector::Yes
         || !bool(mem_info.spec.attributes & SpecAttributes::external)
        ) {
            auto prefix_type = mem_info.prefix_type;
            auto* enumeration = mem_info.value.enumeration;
            if (enumeration) {
                desc_ref_replacer.set_desc_if_empty(enumeration->desc);

                bool use_disable_prefix_for_enumeration = false;

                switch (prefix_type) {
                    case PrefixType::Unspecified:
                        use_disable_prefix_for_enumeration
                          = utils::starts_with(names.cpp_name(), "disable"_av);
                        break;
                    case PrefixType::ForceDisable:
                        use_disable_prefix_for_enumeration = true;
                        break;
                    case PrefixType::NoPrefix:
                        break;
                }

                if (mem_info.value.string_parser_for_enum) {
                    if (enum_values_has_desc(*enumeration)) {
                        spec_desc = enum_desc_with_values_in_desc(
                            *enumeration, use_disable_prefix_for_enumeration);
                        ini_desc = spec_desc;
                    }
                    else {
                        ini_desc = enum_as_string_ini_desc(*enumeration);
                    }
                }
                else {
                    spec_desc = numeric_enum_desc(
                        *enumeration, use_disable_prefix_for_enumeration);
                    ini_desc = spec_desc;
                }
            }
            else if (prefix_type == PrefixType::ForceDisable) {
                throw std::runtime_error("PrefixType::ForceDisable only with enums type");
            }
        }

        auto connpolicy_section_name = [](MemberInfo const& mem, Names const& section_names){
            return mem.connpolicy_section.empty()
                ? section_names.connpolicy_name()
                : mem.connpolicy_section;
        };

        auto ini_desc_replacer = [&](
            std::string& s, Section const& section, MemberInfo const& mem, bool no_suffix
        ){
            (void)no_suffix;
            str_append(s, '[', section.names.ini_name(), ']', mem.name.ini_name());
        };

        auto ini_mem_desc = desc_ref_replacer.replace_refs(has_ini || has_acl, ini_desc_replacer);

        auto spec_desc_replacer = [&](
            std::string& s, Section const& section, MemberInfo const& mem, bool no_suffix
        ){
            if (!mem.name.display.empty()) {
                str_append(s, '"', mem.name.display, '"');
            }
            else {
                auto name = mem.spec.has_connpolicy()
                    ? mem.name.connpolicy_name()
                    : mem.name.ini_name();
                auto first = name.begin();
                auto last = name.end();
                s.push_back('"');
                s.push_back(*first);
                inplace_upper(s.back());
                while (++first != last) {
                    s.push_back(*first == '_' ? ' ' : *first);
                }
                s.push_back('"');
            }
            if (!no_suffix) {
                s += " option"sv;
            }

            const bool refer_to_same_spec =
                (mem.spec.has_connpolicy() == mem_info.spec.has_connpolicy());
            auto section_name = connpolicy_section_name(mem_info, section_names);
            auto section_name_ref = connpolicy_section_name(mem, section.names);
            bool same_section = (section_name == section_name_ref);
            if (!refer_to_same_spec || !same_section) {
                s += " (in "sv;
                if (!refer_to_same_spec || !same_section) {
                    str_append(s, '"', section_name_ref, "\" section"sv,
                        refer_to_same_spec ? ")"sv : " of ");
                }
                if (!refer_to_same_spec) {
                    s += mem.spec.has_connpolicy()
                        ? "\"Connection Policy\" configuration)"sv
                        : "\"RDP Proxy\" configuration option)"sv;
                }
            }
        };

        auto spec_mem_desc = desc_ref_replacer.replace_refs(has_global_spec || has_connpolicy, spec_desc_replacer);

        auto const acl_direction
                = (acl_io == SesmanIO::acl_to_proxy)
                ? " ⇐ "sv
                : (acl_io == SesmanIO::proxy_to_acl)
                ? " ⇒ "sv
                : (acl_io == SesmanIO::rw)
                ? " ⇔ "sv
                : ""sv;

        //
        // ini and spec
        //
        auto push_ini_or_spec_desc = [&](Appender appender, bool is_spec, DescRefReplacer::String* mem_desc = nullptr){
            auto marker = Marker{appender.str};

            if (mem_info.tags.test(Tag::Workaround)) {
                appender(workaround_message);
            }

            appender.add_paragraph(
                mem_desc ? mem_desc->sv()
              : is_spec ? spec_mem_desc.sv()
              : ini_mem_desc.sv()
            );

            appender(is_spec ? spec_desc : ini_desc);
            auto const& note = is_spec ? mem_info.value.spec_note : mem_info.value.ini_note;
            if (!note.empty()) {
                appender(is_spec ? "\n("sv : "("sv, note, ")\n"sv);
            }

            auto comment = is_spec ? htmlize(marker.cut()) : marker.cut();
            add_comment(appender.str, comment, "# "sv);

            if (!mem_info.desc.details_desc.empty()) {
                add_comment(appender.str, mem_info.desc.details_desc, "# "sv);
            }

            auto spec_attr = mem_info.spec.attributes;
            if (is_spec) {
                spec_attr |= (mem_info.value.enumeration && type_enumeration::Category::flags == mem_info.value.enumeration->cat)
                    ? SpecAttributes::hex
                    : SpecAttributes();
            }
            else {
                spec_attr &= SpecAttributes::advanced;
            }

            add_attr_spec(appender.str, mem_info.spec.image_path, spec_attr);

            add_prefered_display_name(appender.str, names);
        };

        //
        // ini
        //
        if (has_ini) {
            auto appender = Appender{section.ini};
            push_ini_or_spec_desc(appender, false);
            if (has_acl) {
                appender("# (acl config: proxy"sv, acl_direction, acl_network_name, ")\n"sv);
            }
            appender('#', names.ini_name(), " = "sv,
                     mem_info.value.values.ini,  "\n\n"sv);
        }

        //
        // global spec
        //
        if (has_global_spec) {
            auto appender = Appender{section.global_spec};
            push_ini_or_spec_desc(appender, true);
            appender(names.ini_name(), " = "sv,
                     mem_info.value.prefix_spec_type,
                     "default="sv, mem_info.value.values.py, ")\n\n"sv);
        }

        //
        // acl default value
        //
        if (mem_info.spec.reset_back_to_selector == ResetBackToSelector::Yes) {
            auto& values = (SesmanIO::proxy_to_acl == acl_io)
                ? acl_map.values_reinit
                : acl_map.values_sent;

            values.emplace_back(str_concat(
                "    '"sv, acl_network_name, "': "sv,
                mem_info.value.values.py, ",\n"sv
            ));
        }

        //
        // acl dialog
        //
        if (has_acl) {
            auto add_to_acl_diag = Appender{acl_diag.buffer};

            add_to_acl_diag("cfg::"sv, section_names.acl_name(), "::"sv, names.all);

            add_to_acl_diag(acl_direction, acl_network_name, "   ["sv);

            if (mem_info.value.string_parser_for_enum) {
                add_to_acl_diag(mem_info.value.cpp_type, acl_direction);
            }
            add_to_acl_diag(mem_info.value.acl_diag_type, "]\n"sv);

            auto marker = Marker(acl_diag.buffer);

            add_to_acl_diag.add_paragraph(ini_mem_desc.sv());
            add_to_acl_diag(spec_desc);
            add_to_acl_diag(mem_info.value.acl_diag_note);
            add_comment(acl_diag.buffer, marker.cut(), "    "sv);
        }


        //
        // json
        //
        {
            Appender{json_values}(
                json_values.empty() ? ""sv : ","sv,
                "\n    {"
                "\n      \"section\": \""sv, section.names.all, "\","
                "\n      \"name\": \""sv, mem_info.name.all, "\","
                "\n      \"type\": \""sv, mem_info.value.json_type, "\","
                "\n      \"value\": "sv, mem_info.value.values.json, ","
                "\n      \"description\": \""sv
            );
            auto json_replacer = [&](std::string& s, Section const& section, MemberInfo const& mem, bool no_suffix){
                s += "<code>"sv;
                if (has_global_spec || has_connpolicy) {
                    spec_desc_replacer(s, section, mem, no_suffix);
                }
                else {
                    ini_desc_replacer(s, section, mem, no_suffix);
                }
                s += "</code>"sv;
            };
            json_quoted(json_values, desc_ref_replacer.replace_refs(true, json_replacer).sv());
            json_values += '"';

            if (!mem_info.desc.details_desc.empty()) {
                json_values += ",\n      \"details\": \"";
                json_quoted(json_values, mem_info.desc.details_desc);
                json_values += '"';
            }

            std::string const* rdp_value = &mem_info.value.values.json;
            std::string const* rdp_sogis_value = nullptr;

            if (!mem_info.value.values.connection_policies.empty()) {
                json_values += ",\n      \"connpolicyValues\": "sv;
                char c = '{';
                for (auto const& connpolicy : mem_info.value.values.connection_policies) {
                    for (auto dest_file : conn_policies) {
                        if (!bool(dest_file & connpolicy.dest_file)) {
                            continue;
                        }
                        if (bool(dest_file & DestSpecFile::rdp)) {
                            rdp_value = &connpolicy.json;
                        }
                        if (bool(dest_file & DestSpecFile::rdp_sogisces_1_3_2030)) {
                            rdp_sogis_value = &connpolicy.json;
                        }
                        Appender{json_values}(
                            c, "\n        \""sv,
                            dest_file_to_filename(dest_file),
                            "\": {\"value\": "sv, connpolicy.json, ", \"forced\": "sv,
                            connpolicy.forced ? "true"sv : "false"sv
                        );
                        if (auto* specific_desc = mem_info.desc.find_desc_for_policy(dest_file)) {
                            DescRefReplacer desc_policy;
                            desc_policy.init(*specific_desc, section_info, mem_info, conf);
                            json_values += ", \"description\": \""sv;
                            json_quoted(
                                json_values,
                                desc_policy.replace_refs(true, json_replacer).sv()
                            );
                            json_values += '"';
                        }
                        json_values += "}"sv;
                        c = ',';
                    }
                }
                json_values += "\n      }"sv;
            }

            const bool has_rdp_sogis = rdp_sogis_value && *rdp_sogis_value != *rdp_value;

            if (bool(mem_info.tags) || has_rdp_sogis) {
                json_values += ",\n      \"tags\": ["sv;
                for (std::size_t i = 0; i < mem_info.tags.max; ++i) {
                    if (mem_info.tags.test(Tag(i))) {
                        json_values += '"';
                        json_values += tag_to_sv(Tag(i));
                        json_values += '"';
                        json_values += ',';
                    }
                }
                if (has_rdp_sogis) {
                    json_values += "\"SOG-IS\","sv;
                }
                json_values.back() = ']';
            }

            auto append_if_not_empty = [&](std::string_view json_key, std::string_view str) {
                if (str.empty()) {
                    return;
                }
                str_append(json_values, ",\n      \""sv, json_key, "\": \""sv, str, '"');
            };
            append_if_not_empty("connpolicySection"sv, mem_info.connpolicy_section);
            append_if_not_empty("iniName"sv, names.ini);
            append_if_not_empty("aclName"sv, names.acl);
            append_if_not_empty("connpolicyName"sv, names.connpolicy);
            append_if_not_empty("displayName"sv, names.display);
            append_if_not_empty("imagePath"sv, mem_info.spec.image_path);
            auto append_if_true = [&](std::string_view json_key, bool value) {
                if (value) {
                    str_append(json_values, ",\n      \""sv, json_key, "\": true"sv);
                }
            };
            append_if_true("globalSpec", bool(mem_info.spec.dest & DestSpecFile::global_spec));
            append_if_true("iniOnly", bool(mem_info.spec.dest & DestSpecFile::ini_only));
            append_if_true("rdp", bool(mem_info.spec.dest & DestSpecFile::rdp));
            append_if_true("rdp_sogisces_1_3_2030", bool(mem_info.spec.dest & DestSpecFile::rdp_sogisces_1_3_2030));
            append_if_true("vnc", bool(mem_info.spec.dest & DestSpecFile::vnc));
            append_if_true("aclToProxy", bool(mem_info.spec.acl_io & SesmanIO::acl_to_proxy));
            append_if_true("ProxyToAcl", bool(mem_info.spec.acl_io & SesmanIO::proxy_to_acl));
            append_if_true("logged", bool(mem_info.spec.attributes & SpecAttributes::logged));
            append_if_true("hex", bool(mem_info.spec.attributes & SpecAttributes::hex));
            append_if_true("advanced", bool(mem_info.spec.attributes & SpecAttributes::advanced));
            append_if_true("iptables", bool(mem_info.spec.attributes & SpecAttributes::iptables));
            append_if_true("password", bool(mem_info.spec.attributes & SpecAttributes::password));
            append_if_true("image", bool(mem_info.spec.attributes & SpecAttributes::image));
            append_if_true("external", bool(mem_info.spec.attributes & SpecAttributes::external));
            append_if_true("adminkit", bool(mem_info.spec.attributes & SpecAttributes::adminkit));
            append_if_true("resetBackToSelector", bool(mem_info.spec.reset_back_to_selector));
            append_if_not_empty("logStategy", mem_info.spec.loggable == Loggable::No
                ? ""sv : mem_info.spec.loggable == Loggable::Yes ? "1"sv : "2"sv);
            if (!mem_info.value.json_extra_type.empty()) {
                str_append(json_values, ",\n      "sv, mem_info.value.json_extra_type);
            }
            json_values += "\n    }"sv;
        }


        //
        // connection policy
        //
        if (has_connpolicy) {
            std::string_view member_name = names.connpolicy_name();

            std::string spec_str;
            spec_str.reserve(128);
            push_ini_or_spec_desc(Appender{spec_str}, true);

            std::string_view section_name = connpolicy_section_name(mem_info, section_names);

            if (policy.section_names.emplace(section_name).second) {
                policy.ordered_section.emplace_back(section_name);
            }

            auto acl_name = acl_network_name;

            auto acl_mem_key = str_concat(section_name, '/', acl_name, '/', member_name);
            if (!policy.acl_mems.emplace(acl_mem_key).second) {
                throw std::runtime_error(str_concat("duplicate "sv, section_name, ' ', member_name));
            }

            for (auto dest_file : conn_policies) {
                if (!bool(mem_info.spec.dest & dest_file)) {
                    continue;
                }

                bool forced = false;

                // acl <-> proxy mapping (forced value)
                if (!bool(mem_info.spec.attributes & SpecAttributes::external)) {
                    for (auto& connpolicy : mem_info.value.values.connection_policies) {
                        auto const dest_map = (conn_policies_acl_map_mask & connpolicy.dest_file);
                        if (bool(dest_file & connpolicy.dest_file) && connpolicy.forced) {
                            str_append(policy.acl_map[dest_map].hidden_values,
                                "    '"sv, acl_name, "': "sv, connpolicy.py, ",\n"sv);
                            forced = true;
                            break;
                        }
                    }
                }

                // forced values arre hidden (not in spec)
                if (forced) {
                    continue;
                }

                auto& content = policy.spec_file_map[dest_file][section_name];
                if (auto* specific_desc = mem_info.desc.find_desc_for_policy(dest_file)) {
                    DescRefReplacer desc_policy;
                    desc_policy.init(*specific_desc, section_info, mem_info, conf);
                    if (auto* enumeration = mem_info.value.enumeration) {
                        desc_policy.set_desc_if_empty(enumeration->desc);
                    }
                    auto mem_desc = desc_policy.replace_refs(true, spec_desc_replacer);
                    std::string spec_str;
                    spec_str.reserve(128);
                    push_ini_or_spec_desc(Appender{spec_str}, true, &mem_desc);
                    content += spec_str;
                }
                else {
                    content += spec_str;
                }

                std::string const* py_value = nullptr;
                std::string const* all_value = &mem_info.value.values.py;
                if (!mem_info.value.values.connection_policies.empty()) {
                    for (auto& connpolicy : mem_info.value.values.connection_policies) {
                        if (!bool(connpolicy.dest_file)) {
                            all_value = &connpolicy.py;
                        }
                        else if (!connpolicy.forced && bool(connpolicy.dest_file & dest_file)) {
                            py_value = &connpolicy.py;
                            break;
                        }
                    }
                }
                if (!py_value) {
                    py_value = all_value;
                }

                Appender{content}(
                    member_name, " = "sv,
                    mem_info.value.prefix_spec_type,
                    "default="sv, *py_value, ")\n\n"sv
                );

                if (!bool(mem_info.spec.attributes & SpecAttributes::external)) {
                    str_append(policy.acl_map[dest_file].sections[section_name],
                        "        ('"sv, acl_name, "', '"sv, member_name, "', "sv, *py_value, "),\n"sv
                    );
                }
            }
        }

        if (bool(mem_info.spec.attributes & SpecAttributes::external)) {
            if (!has_connpolicy) {
                std::string_view ini_name = names.ini_name();
                cpp.out_body_parser_ << "        else if (key == \"" << ini_name << "\"_zv) {\n"
                "            // ignored\n"
                "        }\n";
            }
        }
        else {
            std::string_view varname = names.all;

            cpp.members.push_back({varname, mem_info.value.align_of});

            std::string_view varname_with_section = varname;
            std::string tmp_string1;
            if (!section_names.all.empty()) {
                str_assign(tmp_string1, section_names.all, "::", varname);
                varname_with_section = tmp_string1;
            }

            if (has_acl) {
                cpp.variables_acl.emplace_back(varname_with_section);
            }

            if (!desc_ref_replacer.empty()) {
                cpp.out_member_ << cpp_doxygen_comment(ini_mem_desc.sv(), 4);
            }
            if (!mem_info.value.cpp_note.empty()) {
                cpp.out_member_ << "    /// (" << mem_info.value.cpp_note << ") <br/>\n";
            }

            std::string acl_name;
            if (has_acl) {
                acl_name = acl_network_name;
                cpp.full_names.acl.emplace_back(acl_name);
            }
            cpp.out_member_ << "    /// type: " << cpp.html_entitifier([&](std::ostream& out){
                out << mem_info.value.cpp_type;
            }) << " <br/>\n";

            if (has_acl) {
                if (acl_io == SesmanIO::acl_to_proxy) {
                    if (!has_connpolicy) {
                        cpp.out_member_ << "    /// acl ⇒ proxy <br/>\n";
                    }
                }
                else if (acl_io == SesmanIO::proxy_to_acl) {
                    cpp.out_member_ << "    /// acl ⇐ proxy <br/>\n";
                }
                else if (acl_io == SesmanIO::rw) {
                    cpp.out_member_ << "    /// acl ⇔ proxy <br/>\n";
                }
            }

            if (!names.acl.empty()) {
                cpp.out_member_ << "    /// acl::name: " << acl_name << " <br/>\n";
            }
            else if (has_connpolicy) {
                cpp.out_member_ << "    /// connpolicy -> proxy";
                if (!names.connpolicy.empty() || !mem_info.connpolicy_section.empty()) {
                    cpp.out_member_ << "    [name: "
                        << (mem_info.connpolicy_section.empty()
                            ? section_names.all
                            : mem_info.connpolicy_section)
                        << "::" << names.connpolicy_name() << "]"
                    ;
                }
                cpp.out_member_ << " <br/>\n";

                cpp.out_member_ << "    /// aclName: " << acl_name << " <br/>\n";
            }

            if (!names.display.empty()) {
                cpp.out_member_ << "    /// displayName: " << names.display << " <br/>\n";
            }

            cpp.out_member_ << "    /// default: ";
            cpp.out_member_ << mem_info.value.values.cpp_comment;
            cpp.out_member_ << " <br/>\n";
            cpp.out_member_ << "    struct " << varname_with_section << " {\n";
            cpp.out_member_ << "        static constexpr unsigned acl_proxy_communication_flags = 0b";
            cpp.out_member_ << (bool(acl_io & SesmanIO::acl_to_proxy) ? "1" : "0");
            cpp.out_member_ << (bool(acl_io & SesmanIO::proxy_to_acl) ? "1" : "0");
            cpp.out_member_ << ";\n";

            if (has_acl) {
                cpp.out_member_ << "        // for old cppcheck\n";
                cpp.out_member_ << "        // cppcheck-suppress obsoleteFunctionsindex\n";
                cpp.out_member_ << "        static constexpr ::configs::authid_t index {"
                    " ::configs::cfg_indexes::section" << cpp.sections.size() << " + "
                    << (cpp.authid_policies.size() - cpp.start_section_index) << "};\n";
                cpp.authid_policies.emplace_back(mem_info.spec.loggable);
            }

            cpp.out_member_ << "        using type = ";
            cpp.out_member_ << mem_info.value.cpp_type;
            cpp.out_member_ << ";\n";


            // write type
            if (has_acl || has_spec) {
                cpp.out_member_ << "        using mapped_type = ";
                cpp.out_member_ << mem_info.value.spec_type;
                if (bool(/*PropertyFieldFlags::read & */acl_io)) {
                    cpp.max_str_buffer_size = std::max(cpp.max_str_buffer_size, mem_info.value.spec_str_buffer_size);
                }
                cpp.out_member_ << ";\n";

                cpp.out_acl_and_spec_types <<
                    "template<> struct acl_and_spec_type<cfg::" << varname_with_section << "> { using type = "
                ;
                cpp.out_acl_and_spec_types << mem_info.value.spec_type;
                cpp.out_acl_and_spec_types << "; };\n";
            }
            else {
                cpp.out_member_ << "        using mapped_type = type;\n";
            }


            // write value
            cpp.out_member_ << "        type value { ";
            cpp.out_member_ << mem_info.value.values.cpp;
            cpp.out_member_ << " };\n";

            cpp.out_member_ << "    };\n";

            if (has_spec) {
                std::string_view ini_name = names.ini_name();
                cpp.full_names.spec.emplace_back(str_concat(section_names.all, ':', ini_name));
                cpp.out_body_parser_ << "        else if (key == \"" << ini_name << "\"_zv) {\n"
                "            ::config_parse_and_log(\n"
                "                this->section_name, key.c_str(),\n"
                "                static_cast<cfg::" << varname_with_section << "&>(this->variables).value,\n"
                "                ::configs::spec_type<";
                cpp.out_body_parser_ << mem_info.value.spec_type;
                cpp.out_body_parser_ << ">{},\n"
                "                value\n"
                "            );\n"
                "        }\n";
                str_append(cpp.cfg_values, "cfg::"sv, varname_with_section, ",\n"sv);
                str_append(cpp.cfg_str_values,
                    "{\""sv, section_names.ini_name(), "\"_zv, \""sv,
                    names.ini_name(), "\"_zv},\n"sv);
            }
        }
    }

private:
    struct DescRefReplacer
    {
        // format:
        //  :REF:[section_name]:member_name
        //  :REF::member_name
        //  :REF:SELF:
        static constexpr std::string_view sref = ":REF:"sv;
        static constexpr std::string_view self_param = "SELF:"sv;
        static constexpr std::string_view no_option_suffix = "NOSUFFIX:"sv;

        struct String
        {
            DescRefReplacer const* self;
            std::size_t start;
            std::size_t end;
            std::string_view s;

            std::string_view sv() const
            {
                return start != end
                    ? std::string_view(self->buf.data() + start, end - start)
                    : s;
            }
        };

        DescRefReplacer()
        {
            rep_buffer.reserve(240);
            buf.reserve(8);
        }

        void init(std::string_view str, Section const& section_info, MemberInfo const& mem_info, ConfigInfo const& conf)
        {
            buf.clear();
            replacements.clear();

            // trim right
            while (!str.empty() && (str.back() == ' ' || str.back() == '\n')) {
                str.remove_suffix(1);
            }

            std::string_view::size_type pos = 0;
            while ((pos = str.find(sref, pos)) != std::string_view::npos) {
                auto len = add_replacement(str, pos, section_info, mem_info, conf);
                if (!len) {
                    throw std::runtime_error(str_concat(
                        "invalid :REF: in ["sv, section_info.names.all, ']', mem_info.name.all
                    ));
                }
                pos += len;
            }

            desc = str;
        }

        void set_desc_if_empty(std::string_view str)
        {
            if (desc.empty()) {
                desc = str;
            }
        }

        bool empty() const
        {
            return desc.empty();
        }

        bool has_replacement() const
        {
            return !replacements.empty();
        }

        template<class Fn>
        String replace_refs(bool cond, Fn&& fn)
        {
            if (!cond || !has_replacement()) {
                return {this, 0, 0, desc};
            }

            const auto n = buf.size();

            std::size_t pos = 0;
            for (auto const& rep : replacements) {
                buf.insert(buf.end(), desc.data() + pos, desc.data() + rep.pattern_start);
                rep_buffer.clear();
                fn(rep_buffer, *rep.section, *rep.member, rep.no_suffix);
                buf.insert(buf.end(), rep_buffer.begin(), rep_buffer.end());
                pos = rep.pattern_start + rep.pattern_len;
            }

            buf.insert(buf.end(), desc.data() + pos, desc.data() + desc.size());
            return {this, n, buf.size(), {}};
        }

    private:
        struct ReplacementInfo
        {
            Section const* section;
            MemberInfo const* member;
            std::size_t pattern_start;
            std::size_t pattern_len;
            bool no_suffix;
        };

        std::vector<ReplacementInfo> replacements;
        std::string_view desc;
        std::vector<char> buf;
        std::string rep_buffer;

        std::size_t add_replacement(std::string_view str, std::string_view::size_type pos, Section const& section_info, MemberInfo const& mem_info, ConfigInfo const& conf)
        {
            str = str.substr(pos + sref.size());

            const bool no_suffix_option = utils::starts_with(str, no_option_suffix);
            const std::size_t option_len = no_suffix_option ? no_option_suffix.size() : 0;

            str = str.substr(option_len);

            if (str.empty()) {
                return 0;
            }


            Section const* section_ref = &section_info;

            std::string_view::size_type start_mem_pos;
            if (str[0] == '[') {
                std::string_view::size_type end_sec_pos = str.find(']', 1);
                if (end_sec_pos == std::string_view::npos || str.size() == end_sec_pos || str[end_sec_pos+1] != ':') {
                    return 0;
                }
                const auto section_ref_name = str.substr(1, end_sec_pos - 1);
                section_ref = conf.find_section(section_ref_name);
                if (!section_ref) {
                    return 0;
                }
                start_mem_pos = end_sec_pos + 2;
            }
            else if (str[0] == ':') {
                start_mem_pos = 1;
            }
            else {
                if (!utils::starts_with(str, self_param)) {
                    return 0;
                }
                auto patt_len = sref.size() + self_param.size() + option_len;
                replacements.push_back(ReplacementInfo{
                    .section = &section_info,
                    .member = &mem_info,
                    .pattern_start = pos,
                    .pattern_len = patt_len,
                    .no_suffix = true,
                });
                return patt_len;
            }

            std::string_view::size_type end_mem_pos = start_mem_pos;
            for (char c : str.substr(end_mem_pos)) {
                auto is_identifier_char = ('a' <= c && c <= 'z')
                                       || ('A' <= c && c <= 'Z')
                                       || ('0' <= c && c <= '9')
                                       || (c == '_');
                if (!is_identifier_char) {
                    break;
                }
                ++end_mem_pos;
            }

            const auto member_ref_name = str.substr(start_mem_pos, end_mem_pos - start_mem_pos);
            MemberInfo const* member_ref = section_ref->find_member(member_ref_name);

            if (!member_ref || member_ref == &mem_info) {
                return 0;
            }

            auto patt_len = end_mem_pos + sref.size() + option_len;
            replacements.push_back(ReplacementInfo{
                .section = section_ref,
                .member = member_ref,
                .pattern_start = pos,
                .pattern_len = patt_len,
                .no_suffix = no_suffix_option,
            });
            return patt_len;
        }
    };

    DescRefReplacer desc_ref_replacer;

    struct DescRefReplacers
    {
        DescRefReplacer general;
        DescRefReplacer vnc;
        DescRefReplacer rdp;
        DescRefReplacer rdp_sogisces_1_3_2030;
        DestSpecFile policy_types;

        void init(Description const& desc, Section const& section_info, MemberInfo const& mem_info, ConfigInfo const& conf)
        {
            general.init(desc.general_desc, section_info, mem_info, conf);

            policy_types = DestSpecFile();

            struct D { DestSpecFile type; DescRefReplacer& ref; };
            for (auto const& connpolicy : desc.specific_descs) {
                apply_policies([&](DestSpecFile type, DescRefReplacer& ref){
                    policy_types |= connpolicy.policy_types;
                    auto s = bool(connpolicy.policy_types & type) ? connpolicy.desc : ""sv;
                    ref.init(s, section_info, mem_info, conf);
                });
            }
        }

        void set_desc_if_empty(std::string_view str)
        {
            general.set_desc_if_empty(str);
            apply_on_activated_policies([&](DescRefReplacer& ref){
                ref.set_desc_if_empty(str);
            });
        }

    private:
        template<class F>
        void apply_policies(F f)
        {
            struct D { DestSpecFile type; DescRefReplacer& ref; };
            for (D d : {
                D{DestSpecFile::vnc, vnc},
                D{DestSpecFile::rdp, rdp},
                D{DestSpecFile::rdp_sogisces_1_3_2030, rdp_sogisces_1_3_2030},
            }) {
                f(d.type, d.ref);
            }
        }

        template<class F>
        void apply_on_activated_policies(F f)
        {
            apply_policies([&](DestSpecFile type, DescRefReplacer& ref){
                if (bool(policy_types & type)) {
                    f(ref);
                }
            });
        }
    };
};


struct GeneratorConfigWrapper
{
    struct WordReplacement
    {
        std::string_view word;
        std::string_view replacement;
    };

    GeneratorConfig writer;
    std::vector<WordReplacement> display_name_word_replacement_table {};

    void set_sections(std::initializer_list<std::string_view> l)
    {
        for (std::string_view section_name : l) {
            if (conf.find_section(section_name)) {
                throw std::runtime_error(str_concat(
                    "set_sections(): duplicated section name: "sv, section_name
                ));
            }
            conf.push_section(section_name);
        }
    }

    template<class Fn>
    void section(MemberNames names, Fn fn)
    {
        auto& section = find_section(names.all);
        section.names = std::move(names);
        current_members = &section.members;
        fn();
    }

    void member(MemberInfo&& mem_info)
    {
        assert(current_members);

        // compute display name there is a word replacement
        if (mem_info.name.display.empty() && (mem_info.spec.has_ini() || mem_info.spec.has_connpolicy())) {
            compute_display_name_with_replacement(mem_info.name);
        }

        current_members->emplace_back(std::move(mem_info));
    }

    void build()
    {
        for (auto& section : conf.sections) {
            writer.do_start_section(section.names);
            for (auto const& member : section.members) {
                writer.evaluate_member(section, member, conf);
            }
            writer.do_stop_section(section.names);
        }
    }

private:
    Section& find_section(MemberNames&& names)
    {
        assert(names.acl.empty());

        std::string_view const section_name = names.all;

        auto* section = conf.find_section(section_name);
        if (!section) {
            throw std::runtime_error(str_concat(
                "Unknown section '"sv, section_name, "'. Please add it in set_sections()"sv
            ));
        }

        if (!section->members.empty()) {
            throw std::runtime_error(str_concat('\'', section_name, "' is already set"sv));
        }

        return *section;
    }

    void compute_display_name_with_replacement(cfg_generators::MemberNames& names)
    {
        auto const& replacements = display_name_word_replacement_table;

        auto const& name = names.all;

        auto find_replacement_at_start = [&](std::string_view str) -> WordReplacement const* {
            for (auto& rep : replacements) {
                if (utils::starts_with(str, rep.word)
                && (str.size() == rep.word.size() || str[rep.word.size()] == '_')
                ) {
                    return &rep;
                }
            }
            return nullptr;
        };

        auto replace = [&](auto f, auto repf){
            char const* p = name.data();
            while (p < name.end()) {
                auto str_size = static_cast<std::size_t>(name.end() - p);
                auto str = std::string_view{p, str_size};
                if (auto* rep = find_replacement_at_start(str)) {
                    repf(*rep);
                    str = rep->replacement;
                    p += rep->word.size();
                    if (str_size != rep->word.size()) {
                        ++p;
                    }
                }
                else {
                    p = static_cast<char const*>(memchr(p, '_', str_size));
                    if (p) {
                        str_size = static_cast<std::size_t>(p - str.data());
                        ++p;
                    }
                    else {
                        p = name.end();
                    }
                    str = {str.data(), str_size};
                }

                f(str);
            }
        };

        std::size_t replacement_size = 0;
        // init rep_size
        replace(
            [](std::string_view /*str*/){},
            [&](WordReplacement const& word_rep) {
                replacement_size += word_rep.replacement.size();
            }
        );

        if (!replacement_size) {
            return;
        }

        char* buf = display_names.emplace_back(
            std::make_unique<char[]>(replacement_size + name.size() + 1)
        ).get();
        char* p = buf;
        int first_word_is_replaced = 2;

        replace(
            [&](std::string_view str){
                memcpy(p, str.data(), str.size());
                p += str.size();
                *p++ = ' ';
                if (first_word_is_replaced != 1) {
                    first_word_is_replaced = 0;
                }
            },
            [&](WordReplacement const& /*word_rep*/) {
                if (first_word_is_replaced == 2) {
                    first_word_is_replaced = 1;
                }
            }
        );

        // remove last space
        --p;
        // upper for first letter
        if (first_word_is_replaced != 1) {
            inplace_upper(*buf);
        }
        names.display = {buf, static_cast<std::size_t>(p-buf)};
    }

public:
    std::vector<MemberInfo>* current_members = nullptr;
    // buffer of std::string_view for cfg_desc::names::display (used in menber())
    std::vector<std::unique_ptr<char[]>> display_names {};
    ConfigInfo conf {};
};

}
