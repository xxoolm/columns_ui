#include "pch.h"
#include "ng_playlist.h"

namespace cui::panels::playlist_view {
void PlaylistView::on_items_added(/*unsigned p_playlist, */ size_t start,
    const pfc::list_base_const_t<metadb_handle_ptr>& p_data, const bit_array& p_selection)
{
    /*(if (p_playlist == 0)*/
    {
        clear_sort_column();
        InsertItemsContainer items;
        get_insert_items(start, p_data.get_count(), items);
        insert_items(start, items.get_count(), items.get_ptr());
        refresh_all_items_text();
    }
}
void PlaylistView::on_items_reordered(/*size_t p_playlist, */ const size_t* p_order, size_t p_count)
{
    /*(if (p_playlist ==0)*/
    {
        clear_sort_column();
        for (size_t i = 0; i < p_count; i++) {
            size_t start = i;
            while (i < p_count && p_order[i] != i) {
                i++;
            }
            if (i > start) {
                InsertItemsContainer items;
                get_insert_items(start, i - start, items);
                replace_items(start, items);
            }
        }
    } // namespace cui::panels::playlist_view
}
// changes selection too; doesnt actually change set of items that are selected or item having focus, just changes
// their order

void PlaylistView::on_items_removed(const bit_array& p_mask, size_t p_old_count, size_t p_new_count)
{
    clear_sort_column();

    if (p_new_count == 0) {
        remove_all_items();
        return;
    }

    remove_items(p_mask);
    refresh_all_items_text();
}

void PlaylistView::on_items_selection_change(
    /*size_t p_playlist, */ const bit_array& p_affected, const bit_array& p_state)
{
    /*(if (p_playlist == 0)*/
    if (!m_ignore_callback) {
        RedrawWindow(get_wnd(), nullptr, nullptr, RDW_INVALIDATE);
    }
}
void PlaylistView::on_item_focus_change(/*size_t p_playlist, */ size_t p_from, size_t p_to)
{
    // if (p_playlist==0)
    if (!m_ignore_callback) {
        on_focus_change(p_from, p_to);
    }
} // focus may be -1 when no item has focus; reminder: focus may also change on other callbacks

void PlaylistView::on_items_modified(/*size_t p_playlist, */ const bit_array& p_mask)
{ // if (p_playlist==0)
    {
        clear_sort_column();
        size_t count = m_playlist_api->activeplaylist_get_item_count();

        for (size_t i = 0; i < count; i++) {
            size_t start = i;
            while (i < count && p_mask[i]) {
                i++;
            }
            if (i > start) {
                InsertItemsContainer items;
                get_insert_items(start, i - start, items);
                replace_items(start, items);
            }
        }
    }
}

void PlaylistView::on_items_modified_fromplayback(
    /*size_t p_playlist, */ const bit_array& p_mask, play_control::t_display_level p_level)
{
    if (!core_api::is_shutting_down()) {
        size_t count = m_playlist_api->activeplaylist_get_item_count();

        for (size_t i = 0; i < count; i++) {
            size_t start = i;
            while (i < count && p_mask[i]) {
                i++;
            }
            if (i > start) {
                update_items(start, i - start);
            }
        }
    }
}

void PlaylistView::on_items_replaced(
    /*size_t p_playlist, */ const bit_array& p_mask, const pfc::list_base_const_t<t_on_items_replaced_entry>& p_data)
{
    on_items_modified(p_mask);
}

void PlaylistView::on_item_ensure_visible(/*size_t p_playlist, */ size_t p_idx)
{ // if (p_playlist==0)
    {
        ensure_visible(p_idx);
    }
}

void PlaylistView::on_playlist_switch()
{
    clear_sort_column();
    clear_all_items();
    size_t playlist_index = m_playlist_api->get_active_playlist();
    bool b_scrolled = false;
    if (playlist_index != pfc_infinite && m_playlist_cache[playlist_index].m_initialised) {
        b_scrolled = true;
        _set_scroll_position(m_playlist_cache[playlist_index].m_scroll_position);
    } else
        _set_scroll_position(0);
    size_t focus = m_playlist_api->activeplaylist_get_focus_item();
    /*
    {
        int pos = 0;
        if (focus != pfc_infinite)
        {
            RECT rc;
            get_items_rect(&rc);
            size_t offset = RECT_CY(rc);
            if (get_item_height()>offset)
                offset -= get_item_height();
            else
                offset=0;
            offset /= 2;
            pos = get_item_position(focus) + offset;
        }
        _set_scroll_position(pos);
    }*/
    refresh_groups();
    refresh_columns();
    populate_list();
    if (!b_scrolled && focus != pfc_infinite)
        ensure_visible(focus);
}
void PlaylistView::on_playlist_renamed(const char* p_new_name, size_t p_new_name_len)
{
    clear_sort_column();
    clear_all_items();
    refresh_groups();
    refresh_columns();
    populate_list();
}
} // namespace cui::panels::playlist_view
