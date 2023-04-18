#pragma once

#include "filter.h"

namespace cui::panels::filter {

class ConfigFields
    : public cfg_var
    , public pfc::list_t<Field> {
public:
    enum { stream_version_current = 0 };

    enum { sub_stream_version_current = 0 };

    void set_data_raw(stream_reader* p_stream, size_t p_sizehint, abort_callback& p_abort) override;
    void get_data_raw(stream_writer* p_stream, abort_callback& p_abort) override;
    void reset();

    bool find_by_name(const char* p_name, size_t& p_index);
    bool have_name(const char* p_name);
    void fix_name(const char* p_name, pfc::string8& p_out);
    void fix_name(pfc::string8& p_name);

    explicit ConfigFields(const GUID& p_guid) : cfg_var(p_guid) { reset(); }
};

class ConfigFavourites
    : public cfg_var
    , public pfc::list_t<pfc::string8> {
public:
    void get_data_raw(stream_writer* p_stream, abort_callback& p_abort) override;
    void set_data_raw(stream_reader* p_stream, size_t p_sizehint, abort_callback& p_abort) override;

    bool have_item(const char* p_item);
    bool find_item(const char* p_item, size_t& index);

    explicit ConfigFavourites(const GUID& p_guid) : cfg_var(p_guid) {}
};

extern ConfigFavourites cfg_favourites;
extern cfg_string cfg_sort_string;
extern cfg_bool cfg_sort, cfg_autosend, cfg_orderedbysplitters, cfg_showemptyitems, cfg_showsearchclearbutton;
extern cfg_int cfg_doubleclickaction, cfg_middleclickaction, cfg_edgestyle;
extern ConfigFields cfg_field_list;
extern fbh::ConfigInt32DpiAware cfg_vertical_item_padding;
extern fbh::ConfigBool cfg_show_column_titles, cfg_allow_sorting, cfg_show_sort_indicators, cfg_reverse_sort_tracks;

} // namespace cui::panels::filter
