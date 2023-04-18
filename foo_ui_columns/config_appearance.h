#pragma once

#include "font_manager_data.h"
#include "colour_manager_data.h"

class ColoursClientListEntry {
public:
    pfc::string8 m_name;
    GUID m_guid{};
    cui::colours::client::ptr m_ptr;
};

class ColoursClientList : public pfc::list_t<ColoursClientListEntry> {
public:
    static void g_get_list(ColoursClientList& p_out)
    {
        service_enum_t<cui::colours::client> e;
        cui::colours::client::ptr ptr;

        while (e.next(ptr)) {
            pfc::string8 name;
            ptr->get_name(name);
            if (name.is_empty())
                name = "(unnamed item)";

            p_out.add_item({name, ptr->get_client_guid(), std::move(ptr)});
        }
        p_out.sort_t(g_compare);
    }
    static int g_compare(const ColoursClientListEntry& p1, const ColoursClientListEntry& p2)
    {
        return StrCmpLogicalW(
            pfc::stringcvt::string_os_from_utf8(p1.m_name), pfc::stringcvt::string_os_from_utf8(p2.m_name));
    }
};

class FontsClientListEntry {
public:
    pfc::string8 m_name;
    GUID m_guid{};
    cui::fonts::client::ptr m_ptr;
};

class FontsClientList : public pfc::list_t<FontsClientListEntry> {
public:
    static void g_get_list(FontsClientList& p_out)
    {
        service_enum_t<cui::fonts::client> e;
        cui::fonts::client::ptr ptr;

        while (e.next(ptr)) {
            pfc::string8 name;
            ptr->get_name(name);
            if (name.is_empty())
                name = "(unnamed item)";

            p_out.add_item({name, ptr->get_client_guid(), std::move(ptr)});
        }
        p_out.sort_t(g_compare);
    }
    static int g_compare(const FontsClientListEntry& p1, const FontsClientListEntry& p2)
    {
        return StrCmpLogicalW(
            pfc::stringcvt::string_os_from_utf8(p1.m_name), pfc::stringcvt::string_os_from_utf8(p2.m_name));
    }
};

extern cui::colours::ColourManagerData g_colour_manager_data;
extern FontManagerData g_font_manager_data;

namespace cui::colours {

enum class DarkModeStatus {
    Disabled,
    Enabled,
    UseSystemSetting,
};

extern fbh::ConfigInt32 dark_mode_status;

void handle_effective_dark_mode_status_change();
bool handle_system_dark_mode_status_change();
bool handle_system_dark_mode_availability_change();

} // namespace cui::colours
