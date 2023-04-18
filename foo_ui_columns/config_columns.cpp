#include "pch.h"

#include "config.h"
#include "playlist_view_tfhooks.h"
#include "ng_playlist/ng_playlist_style.h"

cfg_int g_cur_tab(GUID{0x1f7903e5, 0x9523, 0xac7e, {0xd4, 0xea, 0x13, 0xdd, 0xe5, 0xac, 0xc8, 0x66}}, 0);

cfg_uint g_last_colour(GUID{0xd352a60a, 0x4d87, 0x07b9, {0x09, 0x07, 0x03, 0xa1, 0xe0, 0x08, 0x03, 0x2f}}, 0);

EditorFontNotify g_editor_font_notify;

struct ColumnTimes {
    service_ptr_t<titleformat_object> to_display;
    service_ptr_t<titleformat_object> to_colour;
    double time_display_compile{};
    double time_colour_compile{};
    double time_display{};
    double time_colour{};
};

void double_to_string(double blah, pfc::string_base& p_out, int points = 10, bool ms = true)
{
    int decimal;
    int sign;
    pfc::array_t<char> buffer;
    buffer.set_size(_CVTBUFSIZE);
    buffer.fill_null();
    _fcvt_s(buffer.get_ptr(), buffer.get_size(), blah * (ms ? 1000.0 : 1.0), points, &decimal, &sign);
    const char* ptr = buffer.get_ptr();
    if (decimal <= 0) {
        p_out.add_string("0.", 2);
        while (decimal) {
            p_out.add_byte('0');
            decimal++;
        }
        p_out.add_string(ptr, pfc_infinite);
    } else {
        p_out.add_string(ptr, decimal);
        p_out.add_string(".", 1);
        ptr += decimal;
        p_out.add_string(ptr, pfc_infinite);
    }
}

void speedtest(ColumnListCRef columns, bool b_global)
{
    const auto playlist_api = playlist_manager::get();
    const auto titleformat_api = titleformat_compiler::get();
    service_ptr_t<genrand_service> p_genrand = genrand_service::g_create();

    const auto activeplaylist_item_count = playlist_api->activeplaylist_get_item_count();

    bool b_global_colour_used = false;
    bool b_column_times_valid = false;

    const auto column_count = columns.get_count();
    pfc::array_t<ColumnTimes> times_columns;
    times_columns.set_size(column_count);

    {
        for (size_t i = 0; i < column_count; i++)
            if (!columns[i]->use_custom_colour) {
                b_global_colour_used = true;
                break;
            }
    }

    GlobalVariableList p_vars;

    double time_compile_global = 0;
    double time_compile_global_colour = 0;
    double time_global = 0;
    double time_colour = 0;

    service_ptr_t<titleformat_object> to_global;
    service_ptr_t<titleformat_object> to_global_colour;

    {
        double time_temp = 0;
        if (b_global && column_count)
            for (unsigned i = 0; i < 10; i++) {
                to_global.release();
                pfc::hires_timer timer;
                timer.start();
                titleformat_api->compile_safe(to_global, cfg_globalstring);
                time_temp += timer.query();
            }
        time_compile_global = time_temp / 10;
    }

    {
        double time_temp = 0;
        if (b_global_colour_used)
            for (unsigned i = 0; i < 10; i++) {
                to_global_colour.release();
                pfc::hires_timer timer;
                timer.start();
                titleformat_api->compile_safe(to_global_colour, cfg_colour);
                time_temp += timer.query();
            }
        time_compile_global_colour = time_temp / 10;
    }

    pfc::array_t<unsigned> tracks;
    tracks.set_size(16);

    if (activeplaylist_item_count >= 1) {
        b_column_times_valid = true;

        {
            for (auto i = 0; i < 16; i++)
                tracks[i] = p_genrand->genrand(
                    gsl::narrow<unsigned>(std::min(activeplaylist_item_count, static_cast<size_t>(UINT32_MAX))));
        }

        SYSTEMTIME st;
        GetLocalTime(&st);

        {
            pfc::string8_fast_aggressive str_temp;
            str_temp.prealloc(512);

            {
                double time_temp = 0;
                if (b_global && column_count)
                    for (unsigned i = 0; i < 10; i++) {
                        for (unsigned j = 0; j < 16; j++) {
                            p_vars.delete_all();
                            pfc::hires_timer timer;
                            timer.start();
                            {
                                SetGlobalTitleformatHook<true, false> tf_hook_set_global(p_vars);
                                DateTitleformatHook tf_hook_date(&st);
                                titleformat_hook_impl_splitter tf_hook(&tf_hook_set_global, &tf_hook_date);
                                playlist_api->activeplaylist_item_format_title(
                                    tracks[j], &tf_hook, str_temp, to_global, nullptr, play_control::display_level_all);

                                //    if (map_codes) extra_formatted.replace_char(3, 6);
                            }
                            time_temp += timer.query();
                        }
                    }
                time_global = time_temp / (10 * 16);
            }

            {
                double time_temp = 0;
                if (b_global_colour_used)
                    for (unsigned i = 0; i < 10; i++) {
                        for (unsigned j = 0; j < 16; j++) {
                            auto style_info = cui::panels::playlist_view::CellStyleData::g_create_default();
                            pfc::hires_timer timer;
                            timer.start();
                            cui::panels::playlist_view::StyleTitleformatHook tf_hook_style(style_info, 0);
                            SetGlobalTitleformatHook<false, true> tf_hook_set_global(p_vars);
                            DateTitleformatHook tf_hook_date(&st);
                            SplitterTitleformatHook tf_hook(
                                &tf_hook_style, b_global ? &tf_hook_set_global : nullptr, &tf_hook_date);

                            playlist_api->activeplaylist_item_format_title(tracks[j], &tf_hook, str_temp,
                                to_global_colour, nullptr, play_control::display_level_all);
                            time_temp += timer.query();
                        }
                    }
                time_colour = time_temp / (10 * 16);
            }

            for (unsigned n = 0; n < column_count; n++) {
                {
                    double time_temp = 0;
                    unsigned i;
                    for (i = 0; i < 10; i++) {
                        times_columns[n].to_display.release();
                        pfc::hires_timer timer;
                        timer.start();
                        titleformat_api->compile_safe(times_columns[n].to_display, columns[n]->spec);
                        time_temp += timer.query();
                    }
                    times_columns[n].time_display_compile = time_temp / 10;
                    time_temp = 0;
                    for (i = 0; i < 10; i++) {
                        for (unsigned j = 0; j < 16; j++) {
                            pfc::hires_timer timer;
                            timer.start();
                            SetGlobalTitleformatHook<false, true> tf_hook_set_global(p_vars);
                            DateTitleformatHook tf_hook_date(&st);
                            titleformat_hook_impl_splitter tf_hook(
                                b_global ? &tf_hook_set_global : nullptr, &tf_hook_date);

                            playlist_api->activeplaylist_item_format_title(tracks[j], &tf_hook, str_temp,
                                times_columns[n].to_display, nullptr, play_control::display_level_all);
                            time_temp += timer.query();
                        }
                    }
                    times_columns[n].time_display = time_temp / (10 * 16);
                }
                {
                    times_columns[n].time_colour_compile = 0;
                    times_columns[n].time_colour = 0;

                    if (columns[n]->use_custom_colour) {
                        double time_temp = 0;
                        unsigned i;
                        for (i = 0; i < 10; i++) {
                            times_columns[n].to_colour.release();
                            pfc::hires_timer timer;
                            timer.start();
                            titleformat_api->compile_safe(times_columns[n].to_colour, columns[n]->colour_spec);
                            time_temp += timer.query();
                        }
                        times_columns[n].time_colour_compile = time_temp / 10;
                        time_temp = 0;
                        for (i = 0; i < 10; i++) {
                            for (unsigned j = 0; j < 16; j++) {
                                auto style_info = cui::panels::playlist_view::CellStyleData::g_create_default();
                                pfc::hires_timer timer;
                                timer.start();
                                cui::panels::playlist_view::StyleTitleformatHook tf_hook_style(style_info, 0);
                                SetGlobalTitleformatHook<false, true> tf_hook_set_global(p_vars);
                                DateTitleformatHook tf_hook_date(&st);
                                SplitterTitleformatHook tf_hook(
                                    &tf_hook_style, b_global ? &tf_hook_set_global : nullptr, &tf_hook_date);

                                playlist_api->activeplaylist_item_format_title(tracks[j], &tf_hook, str_temp,
                                    times_columns[n].to_colour, nullptr, play_control::display_level_all);
                                time_temp += timer.query();
                            }
                        }
                        times_columns[n].time_colour = time_temp / (10 * 16);
                    }
                }
            }
        }
    }

    {
        pfc::string8_fast_aggressive buffer;
        buffer.prealloc(512);
        buffer += "script compile tests\n========\n";
        buffer += "(track)\x09\x09"
                  "variable\x09"
                  "style";
        buffer += "\n\x9\x9";
        double_to_string(time_compile_global, buffer);
        buffer += "\x9";
        double_to_string(time_compile_global_colour, buffer);

        if (b_column_times_valid) {
            unsigned n;

            buffer += "\n\n(column)\x09"
                      "display\x09\x09"
                      "style";

            for (n = 0; n < column_count; n++) {
                buffer += "\ncolumn ";
                buffer.add_string(pfc::format_int(n + 1));
                buffer.add_byte('\x9');
                double_to_string(times_columns[n].time_display_compile, buffer);
                buffer.add_byte('\x9');
                double_to_string(times_columns[n].time_colour_compile, buffer);
            }

            buffer += "\n\nper-track script execution tests\n========\n";
            buffer += "tracks chosen to test\n";

            for (n = 0; n < 16; n++) {
                if (n)
                    buffer.add_string(", ");
                buffer.add_string(pfc::format_int(tracks[n]));
            }

            buffer += "\n\n(track)\x09\x09"
                      "variable\x09"
                      "style";
            buffer += "\n\x9\x9";
            double_to_string(time_global, buffer);
            buffer += "\x9";
            double_to_string(time_colour, buffer);

            buffer += "\n\n(column)\x9"
                      "display\x9\x9"
                      "style";

            for (n = 0; n < column_count; n++) {
                buffer += "\ncolumn ";
                buffer.add_string(pfc::format_int((n + 1)));
                buffer.add_byte('\x9');
                double_to_string(times_columns[n].time_display, buffer);
                buffer.add_byte('\x9');
                double_to_string(times_columns[n].time_colour, buffer);
            }

            buffer += "\n\nnotes\n========\nUnits measured in seconds.\nScipts that are never executed during use will "
                      "show 0 seconds for its compilation and execution time.";
        }

        popup_message::g_show(buffer, "speedtest results");
    }
}
