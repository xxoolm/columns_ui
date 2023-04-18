#include "pch.h"
#include "playlist_manager_utils.h"
#include "rename_dialog.h"

namespace playlist_manager_utils {

void rename_playlist(size_t index, HWND wnd_parent)
{
    const auto playlist_api = playlist_manager::get();
    pfc::string8 current_name;

    if (!playlist_api->playlist_get_name(index, current_name))
        return;

    pfc::string8 title;
    title << "Rename playlist: " << current_name;

    // Note: The playlist could be moved etc. while the dialog is displayed
    playlist_position_reference_tracker playlist_position_tracker(false);
    playlist_position_tracker.m_playlist = index;

    const auto new_name = cui::helpers::show_rename_dialog_box(wnd_parent, title, current_name);

    if (new_name && playlist_position_tracker.m_playlist != (std::numeric_limits<size_t>::max)())
        playlist_api->playlist_rename(playlist_position_tracker.m_playlist, *new_name, new_name->length());
}

bool check_clipboard()
{
    const auto api = ole_interaction::get();
    wil::com_ptr_t<IDataObject> pDO;

    HRESULT hr = OleGetClipboard(&pDO);
    if (FAILED(hr))
        return false;

    hr = api->check_dataobject_playlists(pDO.get());
    return SUCCEEDED(hr);
}
bool cut(const bit_array& mask)
{
    const auto m_playlist_api = playlist_manager::get();
    const auto api = ole_interaction::get();
    playlist_dataobject_desc_impl data;

    data.set_from_playlist_manager(mask);
    pfc::com_ptr_t<IDataObject> pDO = api->create_dataobject(data);

    HRESULT hr = OleSetClipboard(pDO.get_ptr());
    if (FAILED(hr))
        return false;

    m_playlist_api->remove_playlists(mask);
    if (m_playlist_api->get_active_playlist() == pfc_infinite && m_playlist_api->get_playlist_count())
        m_playlist_api->set_active_playlist(0);

    return true;
}
bool cut(const pfc::list_base_const_t<size_t>& indices)
{
    const auto m_playlist_api = playlist_manager::get();
    size_t count = indices.get_count();
    size_t playlist_count = m_playlist_api->get_playlist_count();
    bit_array_bittable mask(playlist_count);
    for (size_t i = 0; i < count; i++) {
        if (indices[i] < playlist_count)
            mask.set(indices[i], true);
    }
    return cut(mask);
}
bool copy(const bit_array& mask)
{
    const auto m_playlist_api = playlist_manager::get();
    const auto api = ole_interaction::get();
    playlist_dataobject_desc_impl data;

    data.set_from_playlist_manager(mask);
    pfc::com_ptr_t<IDataObject> pDO = api->create_dataobject(data);

    HRESULT hr = OleSetClipboard(pDO.get_ptr());
    return SUCCEEDED(hr);
}
bool copy(const pfc::list_base_const_t<size_t>& indices)
{
    const auto m_playlist_api = playlist_manager::get();
    size_t count = indices.get_count();
    size_t playlist_count = m_playlist_api->get_playlist_count();
    bit_array_bittable mask(playlist_count);
    for (size_t i = 0; i < count; i++) {
        if (indices[i] < playlist_count)
            mask.set(indices[i], true);
    }
    return copy(mask);
}
bool paste(HWND wnd, size_t index_insert)
{
    const auto m_playlist_api = playlist_manager::get();
    const auto api = ole_interaction::get();
    wil::com_ptr_t<IDataObject> pDO;

    HRESULT hr = OleGetClipboard(&pDO);
    if (FAILED(hr))
        return false;

    playlist_dataobject_desc_impl data;
    hr = api->check_dataobject_playlists(pDO.get());
    if (FAILED(hr))
        return false;

    hr = api->parse_dataobject_playlists(pDO.get(), data);
    if (FAILED(hr))
        return false;

    size_t plcount = m_playlist_api->get_playlist_count();
    if (index_insert > plcount)
        index_insert = plcount;

    size_t count = data.get_entry_count();
    for (size_t i = 0; i < count; i++) {
        pfc::string8 name;
        metadb_handle_list handles;
        data.get_entry_name(i, name);
        data.get_entry_content(i, handles);
        index_insert = m_playlist_api->create_playlist(name, pfc_infinite, index_insert);
        m_playlist_api->playlist_insert_items(index_insert, 0, handles, bit_array_false());
        index_insert++;
    }
    return true;
}
} // namespace playlist_manager_utils
