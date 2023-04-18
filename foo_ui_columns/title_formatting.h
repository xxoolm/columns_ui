#pragma once

#include "../foobar2000/SDK/foobar2000.h"

namespace cui::tf {

namespace internal {

struct CaseInsensitiveUtf8Comparator {
    bool operator()(const std::string_view& left, const std::string_view& right) const
    {
        return stricmp_utf8_ex(left.data(), left.size(), right.data(), right.size()) < 0;
    }
};

} // namespace internal

class FieldProviderTitleformatHook : public titleformat_hook {
public:
    using FieldValue = std::variant<std::string, std::string_view, pfc::string8, bool, std::function<std::string()>>;
    using FieldMap = std::map<std::string_view, FieldValue, internal::CaseInsensitiveUtf8Comparator>;

    explicit FieldProviderTitleformatHook(FieldMap field_map) : m_field_map(std::move(field_map)) {}

    bool process_field(
        titleformat_text_out* p_out, const char* p_name, size_t p_name_length, bool& p_found_flag) override;

    bool process_function(titleformat_text_out* p_out, const char* p_name, size_t p_name_length,
        titleformat_hook_function_params* p_params, bool& p_found_flag) override
    {
        return false;
    }

    FieldMap m_field_map;
};

} // namespace cui::tf
