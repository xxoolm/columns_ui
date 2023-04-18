#include "pch.h"
#include "filter.h"
#include "filter_config_var.h"

namespace cui::panels::filter {
bool FilterPanel::notify_on_contextmenu_header(const POINT& pt, const HDHITTESTINFO& ht)
{
    HMENU menu = CreatePopupMenu();
    size_t count = g_field_data.get_count();
    for (size_t i = 0; i < count; i++) {
        pfc::stringcvt::string_wide_from_utf8 wide(g_field_data[i].m_name);
        {
            MENUITEMINFO mii{};
            mii.cbSize = sizeof(mii);
            mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
            mii.fType = MFT_STRING;
            mii.fState = MFS_ENABLED;
            if (!stricmp_utf8(g_field_data[i].m_name, m_field_data.m_name)) {
                mii.fState |= MFS_CHECKED;
                mii.fType |= MFT_RADIOCHECK;
            }
            mii.dwTypeData = (LPWSTR)wide.get_ptr();
            mii.cch = gsl::narrow<UINT>(wide.length());
            mii.wID = gsl::narrow<UINT>(i + 1);
            InsertMenuItem(menu, gsl::narrow<UINT>(i), TRUE, &mii);
            // console::formatter() << (uint32_t)GetLastError();
        }
    }
    int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, get_wnd(), nullptr);
    DestroyMenu(menu);
    if (cmd) {
        set_field(g_field_data[cmd - 1]);
    }
    return true;
}
void FilterPanel::notify_on_menu_select(WPARAM wp, LPARAM lp)
{
    if (HIWORD(wp) & MF_POPUP) {
        m_status_text_override.release();
    } else {
        if (m_contextmenu_manager.is_valid()) {
            unsigned id = LOWORD(wp);

            bool set = false;

            pfc::string8 desc;

            if (m_contextmenu_manager.is_valid() && id >= m_contextmenu_manager_base) {
                contextmenu_node* node = m_contextmenu_manager->find_by_id(id - m_contextmenu_manager_base);
                if (node)
                    set = node->get_description(desc);
            }

            ui_status_text_override::ptr p_status_override;

            if (set) {
                get_host()->override_status_text_create(p_status_override);

                if (p_status_override.is_valid()) {
                    p_status_override->override_text(desc);
                }
            }
            m_status_text_override = p_status_override;
        }
    }
}

bool FilterPanel::notify_on_contextmenu(const POINT& pt, bool from_keyboard)
{
    uie::window_ptr p_this_temp = this;
    enum { ID_BASE = action_add_to_active + 2 };
    metadb_handle_list_t<pfc::alloc_fast_aggressive> handles;
    handles.prealloc(m_nodes.get_count());
    get_selection_handles(handles, true, true);
    HMENU menu = CreatePopupMenu();
    {
        const wchar_t* p_asend = L"Send to autosend playlist";
        const wchar_t* p_asend_play = L"Send to autosend playlist and play";
        const wchar_t* p_send = L"Send to playlist";
        const wchar_t* p_send_play = L"Send to playlist and play";
        const wchar_t* p_add = L"Add to active playlist";
        MENUITEMINFO mii{};
        mii.cbSize = sizeof(mii);
        mii.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_ID | MIIM_STATE;
        mii.fType = MFT_STRING;

        mii.dwTypeData = const_cast<wchar_t*>(p_asend);
        mii.cch = gsl::narrow<UINT>(wcslen(p_asend));
        mii.wID = action_send_to_autosend + 1;
        mii.fState = MFS_ENABLED;
        if (cfg_doubleclickaction + 1 == mii.wID)
            mii.fState |= MFS_DEFAULT;
        InsertMenuItem(menu, 2, TRUE, &mii);

        mii.dwTypeData = const_cast<wchar_t*>(p_asend_play);
        mii.cch = gsl::narrow<UINT>(wcslen(p_asend_play));
        mii.wID = action_send_to_autosend_play + 1;
        mii.fState = MFS_ENABLED;
        if (cfg_doubleclickaction + 1 == mii.wID)
            mii.fState |= MFS_DEFAULT;
        InsertMenuItem(menu, 3, TRUE, &mii);

        mii.dwTypeData = const_cast<wchar_t*>(p_send);
        mii.cch = gsl::narrow<UINT>(wcslen(p_send));
        mii.wID = action_send_to_new + 1;
        mii.fState = MFS_ENABLED;
        if (cfg_doubleclickaction + 1 == mii.wID)
            mii.fState |= MFS_DEFAULT;
        InsertMenuItem(menu, 4, TRUE, &mii);

        mii.dwTypeData = const_cast<wchar_t*>(p_send_play);
        mii.cch = gsl::narrow<UINT>(wcslen(p_send_play));
        mii.wID = action_send_to_new_play + 1;
        mii.fState = MFS_ENABLED;
        if (cfg_doubleclickaction + 1 == mii.wID)
            mii.fState |= MFS_DEFAULT;
        InsertMenuItem(menu, 5, TRUE, &mii);

        mii.dwTypeData = const_cast<wchar_t*>(p_add);
        mii.cch = gsl::narrow<UINT>(wcslen(p_add));
        mii.wID = action_add_to_active + 1;
        mii.fState = MFS_ENABLED;
        if (cfg_doubleclickaction + 1 == mii.wID)
            mii.fState |= MFS_DEFAULT;
        InsertMenuItem(menu, 6, TRUE, &mii);
    }
    AppendMenu(menu, MF_SEPARATOR, NULL, nullptr);
    service_ptr_t<contextmenu_manager> manager;
    contextmenu_manager::g_create(manager);
    manager->init_context(handles, 0);
    manager->win32_build_menu(menu, ID_BASE, -1);
    menu_helpers::win32_auto_mnemonics(menu);
    m_contextmenu_manager = manager;
    m_contextmenu_manager_base = ID_BASE;
    int cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, get_wnd(), nullptr);
    DestroyMenu(menu);
    m_contextmenu_manager.release();
    m_contextmenu_manager_base = NULL;
    m_status_text_override.release();

    if (cmd >= ID_BASE)
        manager->execute_by_id(cmd - ID_BASE);
    else if (cmd > 0)
        do_selection_action((Action)(cmd - 1));

    return true;
}

} // namespace cui::panels::filter
