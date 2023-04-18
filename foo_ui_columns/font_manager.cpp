#include "pch.h"

#include "config_appearance.h"
#include "system_appearance_manager.h"

namespace cui {

namespace {

class FontManager : public fonts::manager {
public:
    void get_font(const GUID& p_guid, LOGFONT& p_out) const override
    {
        system_appearance_manager::initialise();
        FontManagerData::entry_ptr_t p_entry;
        g_font_manager_data.find_by_guid(p_guid, p_entry);
        if (p_entry->font_mode == fonts::font_mode_common_items)
            get_font(fonts::font_type_items, p_out);
        else if (p_entry->font_mode == fonts::font_mode_common_labels)
            get_font(fonts::font_type_labels, p_out);
        else {
            p_out = p_entry->get_normalised_font();
        }
    }

    void get_font(const fonts::font_type_t p_type, LOGFONT& p_out) const override
    {
        FontManagerData::entry_ptr_t p_entry;
        if (p_type == fonts::font_type_items)
            p_entry = g_font_manager_data.m_common_items_entry;
        else
            p_entry = g_font_manager_data.m_common_labels_entry;

        if (p_entry->font_mode == fonts::font_mode_system) {
            if (p_type == fonts::font_type_items)
                uGetIconFont(&p_out);
            else
                uGetMenuFont(&p_out);
        } else {
            p_out = p_entry->get_normalised_font();
        }
    }

    void set_font(const GUID& p_guid, const LOGFONT& p_font) override
    {
        FontManagerData::entry_ptr_t p_entry;
        g_font_manager_data.find_by_guid(p_guid, p_entry);
        p_entry->font_mode = fonts::font_mode_custom;
        p_entry->font_description.log_font = p_font;
        p_entry->font_description.estimate_point_size();
        fonts::client::ptr ptr;
        if (fonts::client::create_by_guid(p_guid, ptr))
            ptr->on_font_changed();
    }

    void register_common_callback(fonts::common_callback* p_callback) override
    {
        g_font_manager_data.register_common_callback(p_callback);
    }

    void deregister_common_callback(fonts::common_callback* p_callback) override
    {
        g_font_manager_data.deregister_common_callback(p_callback);
    }
};

class FontManager2 : public fonts::manager_v2 {
public:
    [[nodiscard]] LOGFONT get_client_font(GUID p_guid, unsigned dpi) const override
    {
        system_appearance_manager::initialise();
        FontManagerData::entry_ptr_t p_entry;
        g_font_manager_data.find_by_guid(p_guid, p_entry);

        if (p_entry->font_mode == fonts::font_mode_common_items)
            return get_common_font(fonts::font_type_items, dpi);

        if (p_entry->font_mode == fonts::font_mode_common_labels)
            return get_common_font(fonts::font_type_labels, dpi);

        return p_entry->get_normalised_font(dpi);
    }

    [[nodiscard]] LOGFONT get_common_font(const fonts::font_type_t p_type, unsigned dpi) const override
    {
        FontManagerData::entry_ptr_t entry;
        if (p_type == fonts::font_type_items)
            entry = g_font_manager_data.m_common_items_entry;
        else
            entry = g_font_manager_data.m_common_labels_entry;

        if (entry->font_mode == fonts::font_mode_system) {
            if (p_type == fonts::font_type_items) {
                LOGFONT lf{};
                uih::dpi::system_parameters_info_for_dpi(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &lf, dpi);
                return lf;
            }

            NONCLIENTMETRICS ncm{};
            ncm.cbSize = sizeof(NONCLIENTMETRICS);
            uih::dpi::system_parameters_info_for_dpi(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, dpi);
            return ncm.lfMenuFont;
        }

        return entry->get_normalised_font(dpi);
    }

    void set_client_font(GUID guid, const LOGFONT& p_font, int point_size_tenths) override
    {
        FontManagerData::entry_ptr_t p_entry;
        g_font_manager_data.find_by_guid(guid, p_entry);
        p_entry->font_mode = fonts::font_mode_custom;
        p_entry->font_description.log_font = p_font;
        p_entry->font_description.point_size_tenths = point_size_tenths;

        fonts::client::ptr ptr;
        if (fonts::client::create_by_guid(guid, ptr))
            ptr->on_font_changed();
    }

    void register_common_callback(fonts::common_callback* p_callback) override
    {
        g_font_manager_data.register_common_callback(p_callback);
    }

    void deregister_common_callback(fonts::common_callback* p_callback) override
    {
        g_font_manager_data.deregister_common_callback(p_callback);
    }
};

service_factory_t<FontManager> _font_manager;
service_factory_t<FontManager2> _font_manager_v2;

} // namespace

} // namespace cui
