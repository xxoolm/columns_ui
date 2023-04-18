#pragma once
#include "filter_config_var.h"

namespace cui::panels::filter {

template <class Container>
void sort_tracks(Container&& tracks)
{
    if (cfg_sort) {
        service_ptr_t<titleformat_object> to;
        titleformat_compiler::get()->compile_safe(to, cfg_sort_string);
        fbh::sort_metadb_handle_list_by_format(
            std::forward<Container>(tracks), to, nullptr, false, cfg_reverse_sort_tracks);
    }
}

} // namespace cui::panels::filter
