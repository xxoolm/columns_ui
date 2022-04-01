#pragma once

#include "layout.h"

class QuickSetupDialog {
    pfc::list_t<ConfigLayout::Preset> m_presets;
    uie::splitter_item_ptr m_previous_layout;
    cui::colours::colour_mode_t m_previous_light_colour_mode{columns_ui::colours::colour_mode_themed};
    cui::colours::colour_mode_t m_previous_dark_colour_mode{columns_ui::colours::colour_mode_themed};
    bool m_previous_show_artwork{};
    bool m_previous_show_grouping{};
    INT_PTR SetupDialogProc(HWND wnd, UINT msg, WPARAM wp, LPARAM lp);

public:
    static void g_run();
};
