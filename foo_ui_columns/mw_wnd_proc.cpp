#include "pch.h"

#include "dark_mode.h"
#include "mw_drop_target.h"
#include "setup_dialog.h"
#include "get_msg_hook.h"
#include "main_window.h"
#include "rebar.h"
#include "status_bar.h"
#include "notification_area.h"

extern HWND g_status;
extern bool g_icon_created;

namespace statusbar_contextmenus {
enum { ID_BASE = 1 };

service_ptr_t<contextmenu_manager> g_main_nowplaying;
} // namespace statusbar_contextmenus

namespace systray_contextmenus {
enum {
    ID_ACTIVATE = 1,
    ID_NOW_PLAYING_BASE = 2,
    ID_BASE_FILE_PREFS = 0x2000,
    ID_BASE_FILE_EXIT = 0x3000,
    ID_BASE_PLAYBACK = 0x4000
};

service_ptr_t<mainmenu_manager> g_menu_file_prefs;
service_ptr_t<mainmenu_manager> g_menu_file_exit;
service_ptr_t<mainmenu_manager> g_menu_playback;
service_ptr_t<contextmenu_manager> g_main_nowplaying;
} // namespace systray_contextmenus

GetMsgHook g_get_msg_hook;
static HWND wnd_last;

LRESULT cui::MainWindow::s_on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    MainWindow* self = nullptr;

    if (msg == WM_NCCREATE) {
        const auto create_struct = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<MainWindow*>(create_struct->lpCreateParams);
        SetWindowLongPtr(wnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(self));
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtr(wnd, GWLP_USERDATA));
    }

    if (msg == WM_NCDESTROY)
        SetWindowLongPtr(wnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(nullptr));

    return self ? self->on_message(wnd, msg, wp, lp) : DefWindowProc(wnd, msg, wp, lp);
}

LRESULT cui::MainWindow::on_message(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static UINT WM_TASKBARCREATED;
    static UINT WM_TASKBARBUTTONCREATED;
    static UINT WM_SHELLHOOKMESSAGE;
    static bool g_last_sysray_r_down;
    static bool g_last_sysray_x1_down;
    static bool g_last_sysray_x2_down;
    static HWND g_wnd_focused_before_menu;

    if (m_hook_proc) {
        LRESULT ret;
        if (m_hook_proc(wnd, msg, wp, lp, &ret)) {
            return ret;
        }
    }

    // ermm we should probably use some kind of class so we can initialise this as a const value
    if (WM_TASKBARCREATED && msg == WM_TASKBARCREATED) {
        if (g_icon_created) {
            g_icon_created = false;
            create_systray_icon();
            update_systray();
        }
        //        return 0;
    }

    if (WM_TASKBARBUTTONCREATED && msg == WM_TASKBARBUTTONCREATED) {
        m_taskbar_list = wil::CoCreateInstanceNoThrow<ITaskbarList3>(CLSID_TaskbarList);
        if (m_taskbar_list && SUCCEEDED(m_taskbar_list->HrInit())) {
            const int cx = GetSystemMetrics(SM_CXSMICON);
            const int cy = GetSystemMetrics(SM_CYSMICON);

            m_taskbar_button_images.reset(
                ImageList_Create(cx, cy, ILC_COLOR32, gsl::narrow<int>(std::size(taskbar_icon_configs)), 0));
            ImageList_SetImageCount(m_taskbar_button_images.get(), gsl::narrow<int>(std::size(taskbar_icon_configs)));

            if (update_taskbar_button_images())
                queue_taskbar_button_update(false);
            else
                m_taskbar_list.reset();
        }
    }

    if (WM_SHELLHOOKMESSAGE && msg == WM_SHELLHOOKMESSAGE && m_should_handle_multimedia_keys) {
        if (wp == HSHELL_APPCOMMAND) {
            const auto cmd = GET_APPCOMMAND_LPARAM(lp);

            switch (cmd) {
            case APPCOMMAND_MEDIA_PLAY_PAUSE:
                standard_commands::main_play_or_pause();
                return TRUE;
            case APPCOMMAND_MEDIA_PLAY:
                standard_commands::main_play();
                return TRUE;
            case APPCOMMAND_MEDIA_PAUSE:
                standard_commands::main_pause();
                return TRUE;
            case APPCOMMAND_MEDIA_STOP:
                standard_commands::main_stop();
                return TRUE;
            case APPCOMMAND_MEDIA_NEXTTRACK:
                standard_commands::main_next();
                return TRUE;
            case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                standard_commands::main_previous();
                return TRUE;
            default:
                break;
            }
        }
    }

    switch (msg) {
    case WM_CREATE: {
        WM_TASKBARCREATED = RegisterWindowMessage(L"TaskbarCreated");
        WM_TASKBARBUTTONCREATED = RegisterWindowMessage(L"TaskbarButtonCreated");
        m_wnd = wnd;

        if (m_should_handle_multimedia_keys) {
            WM_SHELLHOOKMESSAGE = RegisterWindowMessage(TEXT("SHELLHOOK"));
            m_shell_hook_registered = RegisterShellHookWindow(wnd) != 0;
        }

        on_create();

        if (!uih::are_keyboard_cues_enabled())
            SendMessage(wnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_INITIALIZE, UISF_HIDEFOCUS), NULL);

        set_title(core_version_info_v2::get()->get_name());
        if (cfg_show_systray)
            create_systray_icon();

        wil::com_ptr_t<MainWindowDropTarget> drop_handler = new MainWindowDropTarget;
        RegisterDragDrop(m_wnd, drop_handler.get());

        create_child_windows();

        g_get_msg_hook.register_hook();

        if (config_object::g_get_data_bool_simple(standard_config_objects::bool_ui_always_on_top, false))
            SendMessage(wnd, MSG_SET_AOT, TRUE, 0);
    }

        return 0;
    case WM_DESTROY: {
        g_get_msg_hook.deregister_hook();
        g_layout_window.destroy();
        if (m_shell_hook_registered)
            DeregisterShellHookWindow(wnd);

        rebar::destroy_rebar(false);
        status_bar::destroy_window();
        m_taskbar_list.reset();
        RevokeDragDrop(m_wnd);
        destroy_systray_icon();
        on_destroy();
    } break;
    case WM_NCDESTROY:
        m_taskbar_button_images.reset();
        break;
    case WM_CLOSE:
        if (config::advbool_close_to_notification_icon.get()) {
            cfg_go_to_tray = true;
            ShowWindow(wnd, SW_MINIMIZE);
        } else
            standard_commands::main_exit();
        return 0;
    case WM_COMMAND: {
        switch (wp) {
        case taskbar_buttons::ID_STOP | (THBN_CLICKED << 16):
            standard_commands::main_stop();
            break;
        case taskbar_buttons::ID_PLAY_OR_PAUSE | (THBN_CLICKED << 16):
            standard_commands::main_play_or_pause();
            break;
        case taskbar_buttons::ID_PREV | (THBN_CLICKED << 16):
            standard_commands::main_previous();
            break;
        case taskbar_buttons::ID_NEXT | (THBN_CLICKED << 16):
            standard_commands::main_next();
            break;
        case taskbar_buttons::ID_RAND | (THBN_CLICKED << 16):
            standard_commands::main_random();
            break;
        }
    } break;
    case WM_MENUSELECT: {
        if (HIWORD(wp) & MF_POPUP) {
            status_bar::clear_menu_item_description();
        } else {
            if (systray_contextmenus::g_menu_file_prefs.is_valid() || systray_contextmenus::g_menu_file_exit.is_valid()
                || systray_contextmenus::g_menu_playback.is_valid()
                || systray_contextmenus::g_main_nowplaying.is_valid()
                || statusbar_contextmenus::g_main_nowplaying.is_valid()) {
                unsigned id = LOWORD(wp);

                pfc::string8 item_description;
                if (statusbar_contextmenus::g_main_nowplaying.is_valid()) {
                    contextmenu_node* node
                        = statusbar_contextmenus::g_main_nowplaying->find_by_id(id - statusbar_contextmenus::ID_BASE);
                    if (node)
                        node->get_description(item_description);
                }

                if (systray_contextmenus::g_main_nowplaying.is_valid() && id < systray_contextmenus::ID_BASE_FILE_PREFS
                    && id >= systray_contextmenus::ID_NOW_PLAYING_BASE) {
                    contextmenu_node* node = systray_contextmenus::g_main_nowplaying->find_by_id(
                        id - systray_contextmenus::ID_NOW_PLAYING_BASE);
                    if (node)
                        node->get_description(item_description);
                } else if (systray_contextmenus::g_menu_file_prefs.is_valid()
                    && id < systray_contextmenus::ID_BASE_FILE_EXIT) {
                    systray_contextmenus::g_menu_file_prefs->get_description(
                        id - systray_contextmenus::ID_BASE_FILE_PREFS, item_description);
                } else if (systray_contextmenus::g_menu_file_exit.is_valid()
                    && id < systray_contextmenus::ID_BASE_PLAYBACK) {
                    systray_contextmenus::g_menu_file_exit->get_description(
                        id - systray_contextmenus::ID_BASE_FILE_EXIT, item_description);
                } else if (systray_contextmenus::g_menu_playback.is_valid()) {
                    systray_contextmenus::g_menu_playback->get_description(
                        id - systray_contextmenus::ID_BASE_PLAYBACK, item_description);
                }

                status_bar::set_menu_item_description(item_description.get_ptr());
            }
        }
    } break;
    case WM_ACTIVATE: {
        if ((LOWORD(wp) == WA_INACTIVE)) {
            if (!uih::are_keyboard_cues_enabled())
                SendMessage(wnd, WM_UPDATEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), NULL);
            wnd_last = GetFocus();
            if (!rebar::g_rebar_window
                || !rebar::g_rebar_window->get_previous_menu_focus_window(g_wnd_focused_before_menu))
                g_wnd_focused_before_menu = g_layout_window.get_previous_menu_focus_window();

            if (rebar::g_rebar_window)
                rebar::g_rebar_window->hide_accelerators();
            g_layout_window.hide_menu_access_keys();
        }
    } break;
    case WM_SETFOCUS: {
        HWND wnd_focus = g_wnd_focused_before_menu ? g_wnd_focused_before_menu : wnd_last;
        if (wnd_focus && IsWindow(wnd_focus))
            SetFocus(wnd_focus);
        else
            g_layout_window.set_focus();

        // wnd_last = 0; // meh minimised fuko
    } break;
    case WM_SYSCOMMAND:
        if (wp == SC_KEYMENU && !lp) {
            if (!HIBYTE(GetKeyState(VK_CONTROL))) // Workaround for obscure OS bug involving global keyboard shortcuts
            {
                if (rebar::g_rebar_window && rebar::g_rebar_window->set_menu_focus())
                    g_layout_window.hide_menu_access_keys();
                else
                    g_layout_window.set_menu_focus();
            }
            return 0;
        }
        break;
    case WM_MENUCHAR: {
        unsigned short chr = LOWORD(wp);
        bool processed = false;
        if (rebar::g_rebar_window) {
            processed = rebar::g_rebar_window->on_menu_char(chr);
        }
        if (!processed)
            g_layout_window.on_menu_char(chr);
    }
        return (MNC_CLOSE << 16);
    case WM_CONTEXTMENU:
        if (g_status && (HWND)wp == g_status) {
            POINT pt = {(short)(LOWORD(lp)), (short)(HIWORD(lp))};
            enum { ID_CUSTOM_BASE = 1 };
            HMENU menu = CreatePopupMenu();
            service_ptr_t<contextmenu_manager> p_manager;
            contextmenu_manager::g_create(p_manager);
            if (p_manager.is_valid()) {
                statusbar_contextmenus::g_main_nowplaying = p_manager;

                const keyboard_shortcut_manager::shortcut_type shortcuts[]
                    = {keyboard_shortcut_manager::TYPE_CONTEXT_NOW_PLAYING};
                p_manager->set_shortcut_preference(shortcuts, gsl::narrow<unsigned>(std::size(shortcuts)));
                if (p_manager->init_context_now_playing(
                        standard_config_objects::query_show_keyboard_shortcuts_in_menus()
                            ? contextmenu_manager::FLAG_SHOW_SHORTCUTS
                            : 0)) {
                    p_manager->win32_build_menu(menu, ID_CUSTOM_BASE, -1);
                }

                menu_helpers::win32_auto_mnemonics(menu);

                int cmd
                    = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, wnd, nullptr);
                DestroyMenu(menu);
                statusbar_contextmenus::g_main_nowplaying.release();

                if (cmd) {
                    p_manager->execute_by_id(cmd - ID_CUSTOM_BASE);
                }
            }

        } else if ((HWND)wp == rebar::g_rebar) {
            if (rebar::g_rebar_window) {
                enum { IDM_LOCK = 1, IDM_CLOSE, IDM_BASE };

                ui_extension::window_info_list_simple moo;

                service_enum_t<ui_extension::window> e;
                uie::window_ptr l;

                POINT pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};

                if (pt.x == -1 && pt.y == -1) {
                    RECT rc;
                    GetWindowRect(GetFocus(), &rc);
                    pt.x = rc.left;
                    pt.y = rc.bottom;
                }

                POINT pt_client = pt;

                ScreenToClient(rebar::g_rebar_window->wnd_rebar, &pt_client);

                unsigned IDM_EXT_BASE = IDM_BASE + 1;

                HWND child = RealChildWindowFromPoint(rebar::g_rebar_window->wnd_rebar, pt_client);

                RBHITTESTINFO rbht;
                rbht.pt = pt_client;

                int idx_hit
                    = static_cast<int>(SendMessage(rebar::g_rebar, RB_HITTEST, 0, reinterpret_cast<LPARAM>(&rbht)));

                uie::window_ptr p_ext;

                if (idx_hit >= 0 && gsl::narrow_cast<unsigned>(idx_hit) < rebar::g_rebar_window->get_bands().size())
                    p_ext = rebar::g_rebar_window->get_bands()[idx_hit].m_window;

                pfc::refcounted_object_ptr_t<ui_extension::menu_hook_impl> extension_menu_nodes
                    = new ui_extension::menu_hook_impl;

                HMENU menu = CreatePopupMenu();

                if (e.first(l))
                    do {
                        if (rebar::g_rebar_window->check_band(l->get_extension_guid())
                            || ((l->get_type() & ui_extension::type_toolbar)
                                && l->is_available(&rebar::get_rebar_host()))) {
                            ui_extension::window_info_simple info;

                            l->get_name(info.name);
                            l->get_category(info.category);
                            info.guid = l->get_extension_guid();
                            info.prefer_multiple_instances = l->get_prefer_multiple_instances();

                            moo.add_item(info);

                            l.release();
                        }
                    } while (e.next(l));

                moo.sort_by_category_and_name();

                const auto count_exts = moo.get_count();
                HMENU popup = nullptr;
                for (size_t n = 0; n < count_exts; n++) {
                    if (!n || uStringCompare(moo[n - 1].category, moo[n].category)) {
                        if (n)
                            uAppendMenu(
                                menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(popup), moo[n - 1].category);
                        popup = CreatePopupMenu();
                    }
                    uAppendMenu(popup, (MF_STRING | (rebar::g_rebar_window->check_band(moo[n].guid) ? MF_CHECKED : 0)),
                        IDM_BASE + n, moo[n].name);
                    if (n == count_exts - 1)
                        uAppendMenu(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(popup), moo[n].category);
                    IDM_EXT_BASE++;
                }

                uAppendMenu(menu, MF_SEPARATOR, 0, "");
                uAppendMenu(menu, (((cfg_lock) ? MF_CHECKED : 0) | MF_STRING), IDM_LOCK, "Lock the toolbars");
                if (idx_hit != -1)
                    uAppendMenu(menu, (MF_STRING), IDM_CLOSE, "Remove toolbar");

                if (p_ext.is_valid()) {
                    p_ext->get_menu_items(*extension_menu_nodes.get_ptr());
                    if (extension_menu_nodes->get_children_count() > 0)
                        uAppendMenu(menu, MF_SEPARATOR, 0, nullptr);

                    extension_menu_nodes->win32_build_menu(menu, IDM_EXT_BASE, pfc_infinite - IDM_EXT_BASE);
                }

                menu_helpers::win32_auto_mnemonics(menu);

                const auto cmd = static_cast<unsigned>(
                    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, wnd, nullptr));

                if (rebar::g_rebar_window) {
                    if (cmd >= IDM_EXT_BASE) {
                        extension_menu_nodes->execute_by_id(cmd);
                    }

                    //            if (p_ext.is_valid()) p_ext->menu_action(menu, IDM_EXT_BASE, cmd, user_data);

                    DestroyMenu(menu);

                    if (cmd == IDM_LOCK) {
                        cfg_lock = (cfg_lock == 0);
                        rebar::g_rebar_window->update_bands();
                    }
                    if (cmd == IDM_CLOSE) {
                        rebar::g_rebar_window->delete_band(idx_hit);
                    } else if (cmd > 0 && cmd - IDM_BASE < moo.get_count()) {
                        bool shift_down = (GetAsyncKeyState(VK_SHIFT) & (1 << 31)) != 0;
                        //                bool ctrl_down = (GetAsyncKeyState(VK_CONTROL) & (1 << 31)) != 0;

                        if (!shift_down && !moo[cmd - IDM_BASE].prefer_multiple_instances
                            && rebar::g_rebar_window->check_band(moo[cmd - IDM_BASE].guid)) {
                            rebar::g_rebar_window->delete_band(moo[cmd - IDM_BASE].guid);
                        } else {
                            if (idx_hit != -1)
                                rebar::g_rebar_window->insert_band(idx_hit + 1, moo[cmd - IDM_BASE].guid,
                                    rebar::g_rebar_window->cache.get_width(moo[cmd - IDM_BASE].guid));
                            else
                                rebar::g_rebar_window->add_band(moo[cmd - IDM_BASE].guid,
                                    rebar::g_rebar_window->cache.get_width(moo[cmd - IDM_BASE].guid));
                        }
                    }
                }
            }
        }
        break;
    case MSG_SET_AOT:
        SetWindowPos(wnd, wp ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        break;
    case MSG_UPDATE_TITLE:
        update_title();
        break;
    case MSG_RUN_INITIAL_SETUP:
        QuickSetupDialog::s_run();
        return 0;
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;
    case WM_ERASEBKGND:
        dark::draw_layout_background(wnd, reinterpret_cast<HDC>(wp));
        return TRUE;
    case WM_DRAWITEM: {
        auto lpdis = reinterpret_cast<LPDRAWITEMSTRUCT>(lp);

        if (lpdis->CtlID == ID_STATUS) {
            if (const auto result = status_bar::handle_draw_item(lpdis))
                return *result;
        }
        break;
    }
    case WM_WINDOWPOSCHANGED: {
        auto lpwp = reinterpret_cast<LPWINDOWPOS>(lp);
        if (!(lpwp->flags & SWP_NOSIZE)) {
            ULONG_PTR styles = GetWindowLongPtr(wnd, GWL_STYLE);
            if (styles & WS_MINIMIZE) {
                cfg_go_to_tray = cfg_go_to_tray || cfg_minimise_to_tray;
                if (!g_icon_created && cfg_go_to_tray)
                    create_systray_icon();
                if (g_icon_created && cfg_go_to_tray)
                    ShowWindow(wnd, SW_HIDE);
            } else {
                resize_child_windows();
            }
        }
    } break;
    case WM_SYSCOLORCHANGE:
        win32_helpers::send_message_to_direct_children(wnd, msg, wp, lp);
        break;
    case WM_TIMECHANGE:
        win32_helpers::send_message_to_direct_children(wnd, msg, wp, lp);
        break;
    case WM_SETTINGCHANGE:
        if (wp == SPI_SETNONCLIENTMETRICS) {
        } else if (wp == SPI_SETKEYBOARDCUES) {
            bool cues = uih::are_keyboard_cues_enabled();
            SendMessage(wnd, WM_CHANGEUISTATE, MAKEWPARAM(cues ? UIS_CLEAR : UIS_SET, UISF_HIDEFOCUS), NULL);
            // SendMessage(wnd, WM_UPDATEUISTATE, MAKEWPARAM(cues ? UIS_CLEAR : UIS_SET, UISF_HIDEFOCUS), NULL);
            // return 0;
        }
        win32_helpers::send_message_to_direct_children(wnd, msg, wp, lp);
        break;
    case WM_THEMECHANGED:
        if (rebar::g_rebar_window)
            rebar::g_rebar_window->on_themechanged();
        if (g_status) {
            set_part_sizes(status_bar::t_parts_none);
        }
        break;
    case WM_KEYDOWN:
        if (process_keydown(msg, lp, wp))
            return 0;
        break;
    case WM_SYSKEYUP:
        if (process_keydown(msg, lp, wp))
            return 0;
        break;
    case WM_SYSKEYDOWN:
        if (process_keydown(msg, lp, wp))
            return 0;
        break;
    case MSG_NOTIFICATION_ICON:
        if (lp == WM_LBUTTONUP) {
            // if (b_wasDown)
            standard_commands::main_activate_or_hide();
        } else if (lp == WM_RBUTTONDOWN) {
            g_last_sysray_r_down = true;
        }
#if 0
            /* There was some misbehaviour with the newer messages. So we don't use them. */
        if (lp == NIN_SELECT || lp == NIN_KEYSELECT)
        {
            standard_commands::main_activate_or_hide();
            //PostMessage(wnd, WM_NULL, 0, 0);
            return TRUE;
        }
#endif
        else if (lp == WM_XBUTTONDOWN) {
            g_last_sysray_x1_down = HIBYTE(GetKeyState(VK_XBUTTON1)) != 0;
            g_last_sysray_x2_down = HIBYTE(GetKeyState(VK_XBUTTON2)) != 0;
            return TRUE;
        } else if (lp == WM_MOUSEMOVE) {
        } else if (lp == WM_XBUTTONUP) {
            if (config::advbool_notification_icon_x_buttons.get()) {
                if (g_last_sysray_x1_down && !g_last_sysray_x2_down)
                    standard_commands::main_previous();
                if (g_last_sysray_x2_down && !g_last_sysray_x1_down)
                    standard_commands::main_next();
            }
            g_last_sysray_x1_down = false;
            g_last_sysray_x2_down = false;
            return TRUE;
        }
        // else if (lp == WM_CONTEXTMENU)
        else if (lp == WM_RBUTTONUP) {
            if (g_last_sysray_r_down) {
                SetForegroundWindow(wnd);

                POINT pt; // = {(short)LOWORD(lp),(short)HIWORD(lp)};
                GetCursorPos(&pt);

                HMENU menu = CreatePopupMenu();
                HMENU menu_now_playing = nullptr;

                service_ptr_t<contextmenu_manager> p_manager_selection;

                if (cfg_np) {
                    contextmenu_manager::g_create(p_manager_selection);
                    if (p_manager_selection.is_valid()) {
                        const keyboard_shortcut_manager::shortcut_type shortcuts[]
                            = {keyboard_shortcut_manager::TYPE_CONTEXT_NOW_PLAYING};
                        p_manager_selection->set_shortcut_preference(
                            shortcuts, gsl::narrow<unsigned>(std::size(shortcuts)));

                        if (p_manager_selection->init_context_now_playing(
                                standard_config_objects::query_show_keyboard_shortcuts_in_menus()
                                    ? contextmenu_manager::FLAG_SHOW_SHORTCUTS_GLOBAL
                                    : 0)) {
                            menu_now_playing = CreatePopupMenu();

                            p_manager_selection->win32_build_menu(menu_now_playing,
                                systray_contextmenus::ID_NOW_PLAYING_BASE,
                                systray_contextmenus::ID_BASE_FILE_PREFS - 1);

                            pfc::string8_fast_aggressive title;
                            pfc::string8_fast_aggressive name;
                            pfc::string8_fast_aggressive title2;
                            pfc::string8_fast_aggressive title3;
                            const auto play_api = play_control::get();
                            metadb_handle_ptr track;
                            if (play_api->get_now_playing(track)) {
                                metadb_info_container::ptr p_info;
                                if (track->get_async_info_ref(p_info)) {
                                    const char* ptr = p_info->info().meta_get("TITLE", 0);
                                    if (ptr)
                                        title2 = ptr;
                                }
                            }
                            if (title2.length() > 25) {
                                title2.truncate(24);
                                title2 += "\xe2\x80\xa6";
                            }

                            title.prealloc(14 + 25);
                            title << "Now Playing: " << title2;
                            //            play_control::get()->playback_format_title_ex(track, title, "$puts(title,Now
                            //            Playing:
                            //            %title%)$ifgreater($len($get(title)),25,$cut($get(title),24)..,$get(title))",0,0,true);
                            track.release();
                            uFixAmpersandChars_v2(title, name);

                            uAppendMenu(menu, MF_STRING | MF_POPUP, reinterpret_cast<UINT_PTR>(menu_now_playing), name);

                            uAppendMenu(menu, MF_SEPARATOR, 0, "");
                        }
                    }
                }

                service_ptr_t<mainmenu_manager> p_manager_context = standard_api_create_t<mainmenu_manager>();
                service_ptr_t<mainmenu_manager> p_manager_playback = standard_api_create_t<mainmenu_manager>();
                systray_contextmenus::g_menu_file_exit = standard_api_create_t<mainmenu_manager>();

                bool b_shortcuts = standard_config_objects::query_show_keyboard_shortcuts_in_menus();

                if (p_manager_playback.is_valid()) {
                    p_manager_playback->instantiate(mainmenu_groups::playback_controls);
                    p_manager_playback->generate_menu_win32(menu, systray_contextmenus::ID_BASE_PLAYBACK, pfc_infinite,
                        b_shortcuts ? mainmenu_manager::flag_show_shortcuts_global : 0);

                    AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
                }
                if (p_manager_context.is_valid()) {
                    p_manager_context->instantiate(mainmenu_groups::file_etc_preferences);
                    p_manager_context->generate_menu_win32(menu, systray_contextmenus::ID_BASE_FILE_PREFS,
                        systray_contextmenus::ID_BASE_FILE_EXIT - 1,
                        b_shortcuts ? mainmenu_manager::flag_show_shortcuts_global : 0);
                }

                AppendMenu(menu, MF_SEPARATOR, 0, nullptr);
                int insert_point = GetMenuItemCount(menu);
                if (systray_contextmenus::g_menu_file_exit.is_valid()) {
                    systray_contextmenus::g_menu_file_exit->instantiate(mainmenu_groups::file_etc_exit);
                    systray_contextmenus::g_menu_file_exit->generate_menu_win32(menu,
                        systray_contextmenus::ID_BASE_FILE_EXIT, systray_contextmenus::ID_BASE_PLAYBACK - 1,
                        b_shortcuts ? mainmenu_manager::flag_show_shortcuts_global : 0);
                }

                bool b_visible = ui_control::get()->is_visible();
                InsertMenu(menu, insert_point, MF_STRING | MF_BYPOSITION, systray_contextmenus::ID_ACTIVATE,
                    b_visible ? _T("Hide foobar2000") : _T("Show foobar2000"));

                systray_contextmenus::g_menu_file_prefs = p_manager_context;
                systray_contextmenus::g_menu_playback = p_manager_playback;
                systray_contextmenus::g_main_nowplaying = p_manager_selection;

                menu_helpers::win32_auto_mnemonics(menu);

                MENUITEMINFO mi{};
                mi.cbSize = sizeof(MENUITEMINFO);
                mi.fMask = MIIM_STATE;
                mi.fState = MFS_DEFAULT;

                SetMenuItemInfo(menu, systray_contextmenus::ID_ACTIVATE, FALSE, &mi);
                int cmd
                    = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, wnd, nullptr);

                DestroyMenu(menu);

                systray_contextmenus::g_menu_file_prefs.release();
                systray_contextmenus::g_menu_playback.release();
                systray_contextmenus::g_main_nowplaying.release();

                if (cmd) {
                    if (cmd == systray_contextmenus::ID_ACTIVATE) {
                        if (b_visible)
                            ui_control::get()->hide();
                        else
                            ui_control::get()->activate();
                    } else if (cmd < systray_contextmenus::ID_BASE_FILE_PREFS) {
                        if (p_manager_selection.is_valid()) {
                            p_manager_selection->execute_by_id(cmd - systray_contextmenus::ID_NOW_PLAYING_BASE);
                        }
                    } else if (cmd < systray_contextmenus::ID_BASE_FILE_EXIT) {
                        if (p_manager_context.is_valid()) {
                            p_manager_context->execute_command(cmd - systray_contextmenus::ID_BASE_FILE_PREFS);
                        }
                    } else if (cmd < systray_contextmenus::ID_BASE_PLAYBACK) {
                        if (systray_contextmenus::g_menu_file_exit.is_valid()) {
                            systray_contextmenus::g_menu_file_exit->execute_command(
                                cmd - systray_contextmenus::ID_BASE_FILE_EXIT);
                        }
                    } else {
                        if (p_manager_playback.is_valid()) {
                            p_manager_playback->execute_command(cmd - systray_contextmenus::ID_BASE_PLAYBACK);
                        }
                    }
                    systray_contextmenus::g_menu_file_exit.release();
                }

                PostMessage(wnd, WM_NULL, 0, 0);
            }
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        auto lpnmh = reinterpret_cast<LPNMHDR>(lp);

        switch (lpnmh->idFrom) {
        case ID_REBAR:
            switch (lpnmh->code) {
            case NM_CUSTOMDRAW: {
                if (const auto result = rebar::g_rebar_window->handle_custom_draw(reinterpret_cast<LPNMCUSTOMDRAW>(lp)))
                    return *result;
                break;
            }
            case RBN_HEIGHTCHANGE: {
                resize_child_windows();
            } break;
            case RBN_LAYOUTCHANGED: {
                if (rebar::g_rebar_window) {
                    rebar::g_rebar_window->save_bands();
                }
            } break;
            }
            break;
        case ID_STATUS:
            switch (lpnmh->code) {
            case NM_RCLICK:
            case NM_CLICK: {
                auto lpnmm = reinterpret_cast<LPNMMOUSE>(lp);
                const auto part_id = static_cast<status_bar::StatusBarPartID>(
                    SendMessage(lpnmm->hdr.hwndFrom, SB_GETTEXT, lpnmm->dwItemSpec, NULL));

                if (cfg_show_vol && part_id == status_bar::StatusBarPartID::Volume) {
                    if (!status_bar::volume_popup_window.get_wnd()) {
                        // caption vertical. alt-f4 send crazy with two??
                        RECT rc_status;
                        GetWindowRect(lpnmm->hdr.hwndFrom, &rc_status);
                        HWND wndvol = status_bar::volume_popup_window.create(wnd);
                        POINT pt = lpnmm->pt;
                        ClientToScreen(lpnmm->hdr.hwndFrom, &pt);
                        const auto border_width = 1_spx;
                        const auto left_padding = 1_spx + 2_spx;
                        const auto top_padding = 3_spx;
                        const auto bottom_padding = 3_spx;
                        const auto right_padding = 1_spx;
                        const auto trackbar_width = 22_spx;
                        int cx = volume_popup_t::g_get_caption_size() + trackbar_width + left_padding + right_padding
                            + 2 * border_width;
                        int cy = 142_spx + top_padding + bottom_padding + 2 * border_width;
                        int x = pt.x;
                        int y = pt.y;
                        HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                        if (mon) {
                            MONITORINFO mi{};
                            mi.cbSize = sizeof(MONITORINFO);
                            if (GetMonitorInfo(mon, &mi)) {
                                if (x + cx > mi.rcMonitor.right)
                                    x = x - cx > mi.rcMonitor.left ? x - cx : mi.rcMonitor.left;

                                if (x < mi.rcMonitor.left)
                                    x = mi.rcMonitor.left;

                                if (y + cy > mi.rcMonitor.bottom)
                                    y = y - cy > mi.rcMonitor.top ? y - cy : mi.rcMonitor.top;

                                if (y < mi.rcMonitor.top)
                                    y = mi.rcMonitor.top;
                            }
                        }

                        SetWindowPos(wndvol, nullptr, x, y, cx, cy, SWP_NOZORDER);
                        ShowWindow(wndvol, SW_SHOWNORMAL);
                    }
                    return TRUE;
                }
            } break;
            case NM_DBLCLK: {
                const auto lpnmm = reinterpret_cast<LPNMMOUSE>(lp);
                const auto part_id = static_cast<status_bar::StatusBarPartID>(
                    SendMessage(lpnmm->hdr.hwndFrom, SB_GETTEXT, lpnmm->dwItemSpec, NULL));

                if (part_id == status_bar::StatusBarPartID::PlaybackInformation)
                    helpers::execute_main_menu_command(cfg_statusdbl);
                else if ((cfg_show_seltime && part_id == status_bar::StatusBarPartID::TrackLength)
                    || (cfg_show_selcount && part_id == status_bar::StatusBarPartID::TrackCount)) {
                    playlist_manager::get()->activeplaylist_set_selection(bit_array_true(), bit_array_true());
                }
            } break;
            }
            break;
        }
        break;
    }
    return DefWindowProc(wnd, msg, wp, lp);
}
