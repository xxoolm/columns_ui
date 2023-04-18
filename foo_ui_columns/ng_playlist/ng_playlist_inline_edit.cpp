#include "pch.h"
#include "ng_playlist.h"

namespace cui::panels::playlist_view {

namespace {

std::string_view trim_string(std::string_view value)
{
    const auto start = value.find_first_not_of(' ');
    const auto end = value.find_last_not_of(' ');

    if (start > end || start == std::string_view::npos)
        return ""sv;

    return value.substr(start, end - start + 1);
}

auto get_info_field_values(const file_info& info, std::string_view field)
{
    std::vector<std::string_view> values;

    for (size_t i{}; i < info.meta_get_count_by_name(field.data()); ++i) {
        values.emplace_back(info.meta_get(field.data(), i));
    }

    return values;
}

class InlineEditFileInfoFilter : public file_info_filter {
public:
    InlineEditFileInfoFilter(std::string field, std::vector<std::string> new_values)
        : m_field(std::move(field))
        , m_new_values(std::move(new_values))
    {
    }

    bool apply_filter(metadb_handle_ptr p_location, t_filestats p_stats, file_info& p_info) override
    {
        auto old_values = get_info_field_values(p_info, m_field);
        std::vector<std::string_view> new_values;
        ranges::push_back(new_values, m_new_values);

        if (old_values == new_values)
            return false;

        p_info.meta_remove_field(m_field.data());

        for (auto&& value : m_new_values)
            p_info.meta_add_ex(m_field.data(), m_field.length(), value.data(), value.length());

        return true;
    }

    std::string m_field;
    std::vector<std::string> m_new_values;
};

} // namespace

bool PlaylistView::notify_before_create_inline_edit(
    const pfc::list_base_const_t<size_t>& indices, size_t column, bool b_source_mouse)
{
    return (!b_source_mouse || main_window::config_get_inline_metafield_edit_mode() != main_window::mode_disabled)
        && column < m_edit_fields.get_count() && strlen(m_edit_fields[column]);
}

bool PlaylistView::notify_create_inline_edit(const pfc::list_base_const_t<size_t>& indices, size_t column,
    pfc::string_base& p_text, size_t& p_flags, mmh::ComPtr<IUnknown>& pAutocompleteEntries)
{
    const size_t indices_count = indices.get_count();
    m_edit_handles.remove_all();
    m_edit_handles.set_count(indices_count);

    std::vector<std::string_view> values;

    m_edit_field = m_edit_fields[column];

    for (size_t i = 0; i < indices_count; i++) {
        if (!m_playlist_api->activeplaylist_get_item_handle(m_edit_handles[i], indices[i]))
            return false;
    }

    bool matching = true;

    for (size_t i = 0; i < indices_count; i++) {
        metadb_info_container::ptr info_container = m_edit_handles[i]->get_info_ref();

        auto& info = info_container->info();
        auto item_values = get_info_field_values(info, m_edit_field.get_ptr());

        if (i == 0) {
            values = item_values;
        } else if (item_values != values) {
            matching = false;
            break;
        }
    }

    if (matching) {
        p_text = mmh::join<decltype(values)&, std::string_view, std::string>(values, "; "sv).c_str();
    } else {
        p_text = "<multiple values>";
    }

    if (m_library_autocomplete_v2.is_empty() && m_library_autocomplete_v1.is_empty()) {
        if (!library_meta_autocomplete_v2::tryGet(m_library_autocomplete_v2)) {
            m_library_autocomplete_v1 = library_meta_autocomplete::get();
        }
    }

    p_flags |= inline_edit_autocomplete;
    pfc::com_ptr_t<IUnknown> autocomplete_entries;

    if (m_library_autocomplete_v2.is_valid())
        m_library_autocomplete_v2->get_value_list_async(m_edit_field, autocomplete_entries);
    else
        m_library_autocomplete_v1->get_value_list(m_edit_field, autocomplete_entries);

    pAutocompleteEntries = autocomplete_entries.get_ptr();

    return true;
}

void PlaylistView::notify_save_inline_edit(const char* value)
{
    const std::string_view value_view(value);
    std::vector<std::string> values;

    size_t offset{};
    for (;;) {
        const size_t index = value_view.find(";"sv, offset);
        const auto substr = value_view.substr(offset, index - offset);
        const auto trimmed_substr = trim_string(substr);

        if (trimmed_substr.length() > 0)
            values.emplace_back(trimmed_substr);

        if (index == std::string_view::npos)
            break;

        offset = index + 1;
    }

    const auto tagger_api = metadb_io_v2::get();

    const auto filter = fb2k::service_new<InlineEditFileInfoFilter>(m_edit_field.get_ptr(), values);
    tagger_api->update_info_async(m_edit_handles, filter, GetAncestor(get_wnd(), GA_ROOT),
        metadb_io_v2::op_flag_no_errors | metadb_io_v2::op_flag_background | metadb_io_v2::op_flag_delay_ui, nullptr);
}

void PlaylistView::notify_exit_inline_edit()
{
    m_edit_field.reset();
    m_edit_handles.remove_all();
}

} // namespace cui::panels::playlist_view
