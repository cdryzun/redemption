/*
SPDX-FileCopyrightText: 2025 Wallix Proxies Team

SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "configs/type_name.hpp"
#include "utils/strutils.hpp"

#include <vector>
#include <algorithm>
#include <stdexcept>

#include <cassert>


struct type_enumeration
{
    enum class Category : unsigned char
    {
        autoincrement,
        flags,
        set,
    };

    enum class DisplayNameOption : bool
    {
        WithoutNameWhenDescription,
        WithNameWhenDescription,
    };

    enum class Prop : unsigned char
    {
        Value,
        NoValue,
        Reserved,
    };

    struct Descriptions
    {
        std::string_view regular;
        std::string_view disabled;
    };

    struct value_type
    {
        std::string_view name;
        Descriptions desc;
        std::string_view alias;
        uint64_t val;
        // bool is_negative;
        Prop prop = Prop::Value;

        std::string_view get_name() const { return alias.empty() ? name : alias; }
    };

    std::string_view name;
    Descriptions desc;
    std::string_view info;

    Category cat;
    DisplayNameOption display_name_option;

    std::vector<value_type> values {};

    uint64_t min() const
    {
        assert(this->values.size());

        switch (cat)
        {
            case Category::autoincrement:
            case Category::flags:
                return 0;
            case Category::set:
                return std::min_element(
                    this->values.begin(), this->values.end(),
                    [](auto& a, auto& b){ return a.val < b.val; }
                )->val;
        }

        REDEMPTION_UNREACHABLE();
    }

    uint64_t max() const
    {
        assert(this->values.size());

        switch (cat)
        {
            case Category::autoincrement:
                return this->values.size() - 1u;
            case Category::flags:
                return ~uint64_t() >> (64u - (this->values.size() - 1u));
            case Category::set:
                return std::min_element(
                    this->values.begin(), this->values.end(),
                    [](auto& a, auto& b){ return a.val > b.val; }
                )->val;
        }

        REDEMPTION_UNREACHABLE();
    }

protected:
    void _alias(std::string_view s)
    {
        if (!this->values.back().alias.empty()) {
            throw std::runtime_error("'alias' is already defined");
        }
        this->values.back().alias = s;
    }
};

// syntactic facilitator
struct EnumerationDescriptions : type_enumeration::Descriptions
{
    EnumerationDescriptions()
        : type_enumeration::Descriptions{{}, {}}
    {}

    EnumerationDescriptions(char const * regular_desc)
        : type_enumeration::Descriptions{regular_desc, {}}
    {}

    EnumerationDescriptions(std::string_view regular_desc)
        : type_enumeration::Descriptions{regular_desc, {}}
    {}

    EnumerationDescriptions(type_enumeration::Descriptions descs)
        : type_enumeration::Descriptions{descs}
    {}
};

struct type_enumeration_inc : type_enumeration
{
    using type_enumeration::type_enumeration;

    type_enumeration_inc & value(
        std::string_view name,
        EnumerationDescriptions descriptions = {})
    {
        _add_value(name, descriptions);
        return *this;
    }

    // not exposed to .spec
    type_enumeration_inc & reserved(
        std::string_view name,
        EnumerationDescriptions descriptions = {})
    {
        _add_value(name, descriptions).prop = Prop::Reserved;
        return *this;
    }

    // skip a value
    type_enumeration_inc & invalid_value()
    {
        _add_value({}, {}).prop = Prop::NoValue;
        return *this;
    }

    type_enumeration_inc & alias(std::string_view s)
    {
        this->_alias(s);
        return *this;
    }

private:
    value_type& _add_value(std::string_view name, Descriptions descriptions)
    {
        uint64_t value = this->values.size();
        if (cat == Category::flags && value) {
            value = 1ull << (value - 1u);
        }
        return this->values.emplace_back(value_type{
            .name = name,
            .desc = descriptions,
            .alias = std::string_view(),
            .val = value,
        });
    }
};

struct type_enumeration_set : type_enumeration
{
    type_enumeration_set & value(
        std::string_view name,
        unsigned long long val,
        EnumerationDescriptions descriptions = {})
    {
        this->values.emplace_back(value_type{
            .name = name,
            .desc = descriptions,
            .alias = std::string_view(),
            .val = val,
        });
        return *this;
    }

    // not exposed to .spec
    type_enumeration_set & reserved(
        std::string_view name,
        unsigned long long val,
        EnumerationDescriptions descriptions = {})
    {
        this->values.emplace_back(value_type{
            .name = name,
            .desc = descriptions,
            .alias = std::string_view(),
            .val = val,
            .prop = Prop::Reserved,
        });
        return *this;
    }

    type_enumeration_set & alias(std::string_view s)
    {
        this->_alias(s);
        return *this;
    }
};


struct type_enumerations
{
    using DisplayNameOption = type_enumeration::DisplayNameOption;
    using Category = type_enumeration::Category;

    std::vector<type_enumeration> enumerations_;

    struct DescriptionsAndInfo
    {
        EnumerationDescriptions desc;
        std::string_view info;
    };

    // syntactic facilitator
    struct DescriptionsAndInfoFacilitator : DescriptionsAndInfo
    {
        DescriptionsAndInfoFacilitator()
            : DescriptionsAndInfo{{}, {}}
        {}

        DescriptionsAndInfoFacilitator(char const * regular_desc)
            : DescriptionsAndInfo{regular_desc, {}}
        {}

        DescriptionsAndInfoFacilitator(std::string_view regular_desc)
            : DescriptionsAndInfo{regular_desc, {}}
        {}

        DescriptionsAndInfoFacilitator(EnumerationDescriptions descs)
            : DescriptionsAndInfo{descs, {}}
        {}

        DescriptionsAndInfoFacilitator(DescriptionsAndInfo descs_and_info)
            : DescriptionsAndInfo{descs_and_info}
        {}
    };

    type_enumeration_inc & enumeration_flags(
        std::string_view name,
        DisplayNameOption display_opt,
        DescriptionsAndInfoFacilitator descriptions_and_info = {})
    {
        this->enumerations_.push_back({
            .name = name,
            .desc = descriptions_and_info.desc,
            .info = descriptions_and_info.info,
            .cat = Category::flags,
            .display_name_option = display_opt,
        });
        return static_cast<type_enumeration_inc&>(this->enumerations_.back());
    }

    type_enumeration_inc & enumeration_list(
        std::string_view name,
        DisplayNameOption display_opt,
        DescriptionsAndInfoFacilitator descriptions_and_info = {})
    {
        this->enumerations_.push_back({
            .name = name,
            .desc = descriptions_and_info.desc,
            .info = descriptions_and_info.info,
            .cat = Category::autoincrement,
            .display_name_option = display_opt,
        });
        return static_cast<type_enumeration_inc&>(this->enumerations_.back());
    }

    type_enumeration_set & enumeration_set(
        std::string_view name,
        DisplayNameOption display_opt,
        DescriptionsAndInfoFacilitator descriptions_and_info = {})
    {
        this->enumerations_.push_back({
            .name = name,
            .desc = descriptions_and_info.desc,
            .info = descriptions_and_info.info,
            .cat = Category::set,
            .display_name_option = display_opt,
        });
        return static_cast<type_enumeration_set&>(this->enumerations_.back());
    }

    template<class T>
    type_enumeration const & get_enum() const
    {
        return this->_get_enum(type_name<T>());
    }

private:
    type_enumeration const & _get_enum(std::string_view str_tname) const
    {
        auto p = std::find_if(
            enumerations_.begin(),
            enumerations_.end(),
            [&str_tname](type_enumeration const& e){ return str_tname == e.name; }
        );

        if (p != enumerations_.end()) {
            return *p;
        }

        throw std::runtime_error(str_concat("unknown enum '", str_tname, "'"));
    }
};
