#include "pch.h"
#include "playlist_view_tfhooks.h"
#include "tab_colours.h"
#include "prefs_utils.h"

void preview_to_console(const char* spec, bool extra)
{
    const auto playlist_api = playlist_manager::get();

    const auto count = playlist_api->activeplaylist_get_item_count();

    if (!count)
        popup_message::g_show("Activate a non-empty playlist and try again", "No track to preview");
    else {
        auto idx = playlist_api->activeplaylist_get_focus_item();
        if (idx >= count)
            idx = 0;

        pfc::string8 temp;

        SYSTEMTIME st{};
        GetLocalTime(&st);

        GlobalVariableList extra_items;
        if (extra) {
            pfc::string8_fast_aggressive str_dummy;
            service_ptr_t<titleformat_object> to_global;
            titleformat_compiler::get()->compile_safe(to_global, cfg_globalstring);

            PlaylistNameTitleformatHook tf_hook_playlist_name;
            DateTitleformatHook tf_hook_date(&st);
            SetGlobalTitleformatHook<true, false> tf_hook_set_global(extra_items);
            SplitterTitleformatHook tf_hook(&tf_hook_set_global, &tf_hook_date, &tf_hook_playlist_name);
            playlist_api->activeplaylist_item_format_title(
                idx, &tf_hook, str_dummy, to_global, nullptr, play_control::display_level_all);
        }

        service_ptr_t<titleformat_object> to_temp;
        titleformat_compiler::get()->compile_safe(to_temp, spec);

        SetGlobalTitleformatHook<false, true> tf_hook_set_global(extra_items);
        DateTitleformatHook tf_hook_date(&st);

        titleformat_hook_impl_splitter tf_hook(&tf_hook_set_global, &tf_hook_date);

        // 0.9 fallout
        playlist_api->activeplaylist_item_format_title(
            idx, &tf_hook, temp, to_temp, nullptr, play_control::display_level_all);
        //    if (map) temp.replace_char(6, 3);
        pfc::string_formatter formatter;
        popup_message::g_show(temp, formatter << "Preview of track " << (idx + 1));
    }
    // console::popup();
}

void colour_code_gen(HWND parent, UINT edit, bool markers, bool init)
{
    COLORREF COLOR = g_last_colour;
    COLORREF COLORS[16] = {g_last_colour, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (init || uChooseColor(&COLOR, parent, &COLORS[0])) {
        g_last_colour = COLOR;

        pfc::string_formatter text;
        text << "$rgb(" << unsigned(COLOR & 0xff) << "," << unsigned(COLOR >> 8 & 0xff) << ","
             << unsigned(COLOR >> 16 & 0xff) << ")";

        uSendDlgItemMessageText(parent, edit, WM_SETTEXT, 0, text);
    }
}

bool colour_picker(HWND wnd, COLORREF& out, COLORREF custom)
{
    bool rv = false;
    COLORREF COLOR = out;
    COLORREF COLORS[16] = {custom, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    if (uChooseColor(&COLOR, wnd, &COLORS[0])) {
        out = COLOR;
        rv = true;
    }
    return rv;
}

void populate_menu_combo(HWND wnd, unsigned ID, unsigned ID_DESC, const MenuItemIdentifier& p_item,
    const std::vector<MenuItemInfo>& p_cache, bool insert_none)
{
    HWND wnd_combo = GetDlgItem(wnd, ID);

    const auto count = p_cache.size();
    pfc::string8_fast_aggressive temp;
    int idx_none = 0;
    if (insert_none) {
        idx_none = ComboBox_AddString(wnd_combo, L"(None)");
        SendMessage(wnd_combo, CB_SETITEMDATA, idx_none, -1);
    }

    int sel = -1;
    pfc::string8 desc;

    for (size_t n = 0; n < count; n++) {
        const auto idx
            = ComboBox_AddString(wnd_combo, pfc::stringcvt::string_wide_from_utf8(p_cache[n].m_name).get_ptr());
        SendMessage(wnd_combo, CB_SETITEMDATA, idx, n);

        if (sel == -1 && p_cache[n] == p_item) {
            sel = idx;
            desc = p_cache[n].m_desc;
        } else if (sel != -1 && idx <= sel)
            sel++;

        if (insert_none && idx <= idx_none)
            idx_none++;
    }

    ComboBox_SetCurSel(wnd_combo, sel == -1 && insert_none ? idx_none : sel);

    // menu_helpers::get_description(menu_item::TYPE_MAIN, item, desc);
    uSendDlgItemMessageText(wnd, ID_DESC, WM_SETTEXT, 0, desc);
}

void on_menu_combo_change(
    HWND wnd, LPARAM lp, ConfigMenuItem& cfg_menu_store, const std::vector<MenuItemInfo>& p_cache, unsigned ID_DESC)
{
    auto wnd_combo = (HWND)lp;

    pfc::string8 temp;
    size_t cache_idx = ComboBox_GetItemData(wnd_combo, ComboBox_GetCurSel(wnd_combo));

    if (cache_idx == static_cast<size_t>(CB_ERR)) {
        cfg_menu_store = MenuItemIdentifier();
    } else if (cache_idx < p_cache.size()) {
        cfg_menu_store = p_cache[cache_idx];
    }

    pfc::string8 desc;
    // if (cfg_menu_store != pfc::guid_null) menu_helpers::get_description(menu_item::TYPE_MAIN, cfg_menu_store, desc);
    uSendDlgItemMessageText(
        wnd, ID_DESC, WM_SETTEXT, 0, cache_idx < p_cache.size() ? p_cache[cache_idx].m_desc.get_ptr() : "");
}

namespace cui::prefs {

HFONT create_default_ui_font(unsigned point_size)
{
    LOGFONT lf{};
    wcsncpy_s(lf.lfFaceName, L"Segoe UI", _TRUNCATE);
    lf.lfHeight = -MulDiv(point_size, uih::get_system_dpi_cached().cy, 72);
    return CreateFontIndirect(&lf);
}

HFONT create_default_title_font()
{
    return create_default_ui_font(12);
}

} // namespace cui::prefs
