#include "pch.h"
#include "config.h"
#include "notification_area.h"

static class TabNotificationArea : public PreferencesTab {
public:
    bool m_initialised{};

    //    static ptr_list_autofree_t<char> status_items;

    static void refresh_me(HWND wnd)
    {
        SendDlgItemMessage(wnd, IDC_BALLOON, BM_SETCHECK, cfg_balloon, 0);

        SendDlgItemMessage(wnd, IDC_SHOW_SYSTRAY, BM_SETCHECK, cfg_show_systray, 0);

        SendDlgItemMessage(wnd, IDC_MINIMISE_TO_SYSTRAY, BM_SETCHECK, cfg_minimise_to_tray, 0);
        SendDlgItemMessage(wnd, IDC_USE_CUSTOM_ICON, BM_SETCHECK, cfg_custom_icon, 0);
        SendDlgItemMessage(wnd, IDC_NOWPL, BM_SETCHECK, cfg_np, 0);

        uSendDlgItemMessageText(wnd, IDC_STRING, WM_SETTEXT, NULL, main_window::config_notification_icon_script.get());
        EnableWindow(GetDlgItem(wnd, IDC_BROWSE_ICON), cfg_custom_icon);
    }

    BOOL ConfigProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
        case WM_INITDIALOG: {
            refresh_me(wnd);
            m_initialised = true;
        }

        break;
        case WM_DESTROY: {
            m_initialised = false;
        } break;
        case WM_COMMAND:
            switch (wp) {
            case (EN_CHANGE << 16) | IDC_STRING:
                main_window::config_notification_icon_script.set(uGetWindowText((HWND)lp));
                break;

            case IDC_NOWPL: {
                cfg_np = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
            } break;
            case IDC_USE_CUSTOM_ICON: {
                cfg_custom_icon = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
                EnableWindow(GetDlgItem(wnd, IDC_BROWSE_ICON), cfg_custom_icon);
                create_icon_handle();
                create_systray_icon();
            } break;
            case IDC_BROWSE_ICON: {
                pfc::string8 path = cfg_tray_icon_path;
                if (uGetOpenFileName(wnd, "Icon Files (*.ico)|*.ico|All Files (*.*)|*.*", 0, "ico", "Choose Icon",
                        nullptr, path, FALSE)) {
                    cfg_tray_icon_path = path;
                    if (cfg_custom_icon) {
                        create_icon_handle();
                        create_systray_icon();
                    }
                }
            } break;

            case IDC_MINIMISE_TO_SYSTRAY: {
                cfg_minimise_to_tray = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
            } break;
            case IDC_SHOW_SYSTRAY: {
                cfg_show_systray = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
                //                EnableWindow(GetDlgItem(wnd, IDC_MINIMISE_TO_SYSTRAY), cfg_show_systray);
                on_show_notification_area_icon_change();
            } break;
            case IDC_BALLOON: {
                cfg_balloon = Button_GetCheck(reinterpret_cast<HWND>(lp)) == BST_CHECKED;
            } break;
            }
        }
        return 0;
    }
    HWND create(HWND wnd) override
    {
        return m_helper.create(wnd, IDD_PREFS_NOTIFICATION_AREA,
            [this](auto&&... args) { return ConfigProc(std::forward<decltype(args)>(args)...); });
    }
    const char* get_name() override { return "Notification area"; }
    bool get_help_url(pfc::string_base& p_out) override
    {
        p_out = "http://yuo.be/wiki/columns_ui:config:notification_area";
        return true;
    }
    cui::prefs::PreferencesTabHelper m_helper{IDC_TITLE1};
} g_tab_sys;

PreferencesTab* g_get_tab_sys()
{
    return &g_tab_sys;
}
