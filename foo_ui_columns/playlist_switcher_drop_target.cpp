#include "pch.h"

#include "common.h"
#include "playlist_switcher_v2.h"

namespace cui::panels::playlist_switcher {

bool g_query_dataobj_supports_format(CLIPFORMAT cf, IDataObject* pDataObj)
{
    FORMATETC fe;

    fe.cfFormat = cf;
    fe.ptd = nullptr;
    fe.dwAspect = DVASPECT_CONTENT;
    fe.lindex = -1;
    fe.tymed = TYMED_HGLOBAL;

    return S_OK == pDataObj->QueryGetData(&fe); // Don't want S_FALSE ?
}

bool g_get_folder_name(IDataObject* pDataObj, pfc::string8& p_out)
{
    bool ret = false;
    FORMATETC fe;
    STGMEDIUM sm;
    HRESULT hr = E_FAIL;

    // memset(&sm, 0, sizeof(0));

    fe.cfFormat = CF_HDROP;
    fe.ptd = nullptr;
    fe.dwAspect = DVASPECT_CONTENT;
    fe.lindex = -1;
    fe.tymed = TYMED_HGLOBAL;

    // User has dropped on us. Get the data from drag source
    hr = pDataObj->GetData(&fe, &sm);
    if (SUCCEEDED(hr)) {
        // Display the data and release it.
        pfc::string8 temp;

        unsigned int /*n,*/ t = uDragQueryFileCount((HDROP)sm.hGlobal);
        if (t == 1) {
            {
                uDragQueryFile((HDROP)sm.hGlobal, 0, temp);
                DWORD att = uGetFileAttributes(temp);
                if (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY)) {
                    p_out.set_string(string_filename_ext(temp));
                    ret = true;
                } else {
                    p_out.set_string(string_filename(temp));
                    ret = true;
                }
            }
        }

        ReleaseStgMedium(&sm);
    }
    return ret;
}

HRESULT STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::QueryInterface(REFIID riid, LPVOID FAR* ppvObject)
{
    if (ppvObject == nullptr)
        return E_INVALIDARG;
    *ppvObject = nullptr;
    if (riid == IID_IUnknown) {
        AddRef();
        *ppvObject = (IUnknown*)this;
        return S_OK;
    }
    if (riid == IID_IDropTarget) {
        AddRef();
        *ppvObject = (IDropTarget*)this;
        return S_OK;
    }
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::AddRef()
{
    return InterlockedIncrement(&drop_ref_count);
}

ULONG STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::Release()
{
    LONG rv = InterlockedDecrement(&drop_ref_count);
    if (!rv) {
        delete this;
    }
    return rv;
}

HRESULT STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::DragEnter(
    IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragEnter(m_window->get_wnd(), pDataObj, &pt, *pdwEffect);

    if (!m_ole_api.is_valid())
        return S_FALSE;

    m_DataObject = pDataObj;
    m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

    *pdwEffect = m_window->m_dragging && m_window->m_DataObject == pDataObj && (0 == (grfKeyState & MK_CONTROL))
        ? DROPEFFECT_MOVE
        : DROPEFFECT_COPY;

    DWORD dummy;
    bool native;

    m_is_playlists = false;

    m_is_playlists = g_query_dataobj_supports_format(
        m_ole_api->get_clipboard_format(m_ole_api->KClipboardFormatMultiFPL), pDataObj);

    m_is_accepted_type = ui_drop_item_callback::g_is_accepted_type(pDataObj, pdwEffect)
        || S_OK == m_ole_api->check_dataobject(pDataObj, dummy, native);
    if (!m_is_accepted_type) {
        *pdwEffect = DROPEFFECT_NONE;
        uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::DragOver(DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    bool isAltDown = (grfKeyState & MK_ALT) != 0;
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragOver(&pt, *pdwEffect);

    if (!m_ole_api.is_valid())
        return S_FALSE;

    if (!m_is_accepted_type) {
        *pdwEffect = DROPEFFECT_NONE;
        uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");
        return S_OK;
    }

    if (ui_drop_item_callback::g_is_accepted_type(m_DataObject.get(), pdwEffect))
        return S_OK;

    *pdwEffect = m_window->m_dragging && m_window->m_DataObject == m_DataObject && (0 == (grfKeyState & MK_CONTROL))
        ? DROPEFFECT_MOVE
        : DROPEFFECT_COPY;
    m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

    POINT pti;
    pti.y = ptl.y;
    pti.x = ptl.x;
    if (m_window->get_wnd()) {
        HitTestResult hi;

        {
            POINT ptt = pti;
            ScreenToClient(m_window->get_wnd(), &ptt);

            RECT rc_items = m_window->get_items_rect();

            rc_items.top += m_window->get_item_height();
            rc_items.bottom -= m_window->get_item_height();

            bool b_scrolling = false;

            if (ptt.y < rc_items.top && ptt.y < rc_items.bottom) {
                m_window->create_timer_scroll_up();
                b_scrolling = true;
            } else
                m_window->destroy_timer_scroll_up();

            if (ptt.y >= rc_items.top && ptt.y >= rc_items.bottom) {
                m_window->create_timer_scroll_down();
                b_scrolling = true;
            } else
                m_window->destroy_timer_scroll_down();

            /*if (*pdwEffect == DROPEFFECT_COPY && cfg_drop_at_end)
            {
                m_window->set_insert_mark(m_window->get_item_count());
                m_window->destroy_switch_timer();
            }
            else*/
            {
                m_window->hit_test_ex(ptt, hi);
                if (hi.category == HitTestCategory::OnUnobscuredItem || hi.category == HitTestCategory::OnGroupHeader
                    || hi.category == HitTestCategory::OnItemObscuredBelow) {
                    if (m_is_playlists) {
                        m_window->set_insert_mark(hi.insertion_index);
                        m_window->destroy_switch_timer();
                        uih::ole::set_drop_description(m_DataObject.get(),
                            *pdwEffect == DROPEFFECT_MOVE ? DROPIMAGE_MOVE : DROPIMAGE_COPY,
                            *pdwEffect == DROPEFFECT_MOVE ? "Move here" : "Copy here", "");
                    } else {
                        if (isAltDown) {
                            m_window->set_insert_mark(hi.insertion_index);
                            m_window->destroy_switch_timer();
                            m_window->remove_highlight_item();
                            uih::ole::set_drop_description(
                                m_DataObject.get(), DROPIMAGE_COPY, "Add to new playlist", "");
                        } else {
                            m_window->remove_insert_mark();
                            m_window->set_highlight_item(hi.index);
                            if (cfg_drag_autoswitch)
                                m_window->set_switch_timer(hi.index);
                            pfc::string8 name;
                            playlist_manager::get()->playlist_get_name(hi.index, name);
                            uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_COPY, "Add to %1", name);
                        }
                    }
                } else if (hi.category == HitTestCategory::NotOnItem) {
                    m_window->remove_highlight_item();
                    m_window->set_insert_mark(hi.insertion_index);
                    m_window->destroy_switch_timer();
                    pfc::string8 message;
                    if (*pdwEffect == DROPEFFECT_MOVE)
                        message = "Move here";
                    else if (m_is_playlists)
                        message = "Copy here";
                    else
                        message = "Add to new playlist";

                    uih::ole::set_drop_description(m_DataObject.get(),
                        *pdwEffect == DROPEFFECT_MOVE ? DROPIMAGE_MOVE : DROPIMAGE_COPY, message, "");
                } else {
                    m_window->remove_insert_mark();
                    m_window->remove_highlight_item();
                    m_window->destroy_switch_timer();
                    uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_COPY, "Add to new playlist", "");
                }
            }
        }
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::DragLeave()
{
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragLeave();

    if (!m_ole_api.is_valid())
        return S_FALSE;

    m_window->remove_insert_mark();
    m_window->remove_highlight_item();
    m_window->destroy_timer_scroll_up();
    m_window->destroy_timer_scroll_down();
    m_window->destroy_switch_timer();
    m_is_playlists = false;
    m_is_accepted_type = false;

    uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");

    m_DataObject.reset();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistSwitcher::DropTarget::Drop(
    IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    bool isAltDown = (grfKeyState & MK_ALT) != 0;
    if (m_DropTargetHelper)
        m_DropTargetHelper->Drop(pDataObj, &pt, *pdwEffect);

    if (!m_ole_api.is_valid())
        return S_FALSE;

    *pdwEffect = m_window->m_dragging && m_window->m_DataObject == pDataObj && (0 == (grfKeyState & MK_CONTROL))
        ? DROPEFFECT_MOVE
        : DROPEFFECT_COPY;
    m_DataObject.reset();
    m_window->destroy_timer_scroll_up();
    m_window->destroy_timer_scroll_down();
    m_window->destroy_switch_timer();

    if (!m_is_accepted_type) {
        *pdwEffect = DROPEFFECT_NONE;
        m_window->remove_insert_mark();
        m_window->remove_highlight_item();
        m_is_playlists = false;
        return S_OK;
    }

    POINT pti;
    pti.y = ptl.y;
    pti.x = ptl.x;
    if (m_window->get_wnd()) {
        bool process = !ui_drop_item_callback::g_on_drop(pDataObj);

        if (process) {
            HitTestResult hi;
            {
                POINT ptt = pti;
                ScreenToClient(m_window->get_wnd(), &ptt);
                m_window->hit_test_ex(ptt, hi);
            }

            playlist_dataobject_desc_impl data;

            bool b_internal_move = false;

            if (m_is_playlists
                && ((b_internal_move
                        = m_window->m_dragging && m_window->m_DataObject == pDataObj && *pdwEffect == DROPEFFECT_MOVE)
                    || SUCCEEDED(m_ole_api->parse_dataobject_playlists(pDataObj, data)))) {
                if (b_internal_move) {
                    if (hi.category == HitTestCategory::OnUnobscuredItem
                        || hi.category == HitTestCategory::OnGroupHeader
                        || hi.category == HitTestCategory::OnItemObscuredBelow
                        || hi.category == HitTestCategory::OnItemObscuredAbove
                        || hi.category == HitTestCategory::NotOnItem) {
                        size_t count = m_playlist_api->get_playlist_count();
                        order_helper order(count);
                        {
                            size_t from = m_window->get_selected_item_single();
                            if (from != pfc_infinite) {
                                size_t to = hi.insertion_index;
                                if (to > from)
                                    to--;
                                if (from != to && to < count) {
                                    if (from < to)
                                        while (from < to && from < count) {
                                            order.swap(from, from + 1);
                                            from++;
                                        }
                                    else if (from > to)
                                        while (from > to && from > 0) {
                                            order.swap(from, from - 1);
                                            from--;
                                        }
                                    {
                                        m_playlist_api->reorder(order.get_ptr(), count);
                                    }
                                }
                            }
                        }
                    }
                } else {
                    {
                        size_t index_insert = m_playlist_api->get_playlist_count();

                        if (hi.category == HitTestCategory::OnUnobscuredItem
                            || hi.category == HitTestCategory::OnGroupHeader
                            || hi.category == HitTestCategory::OnItemObscuredBelow
                            || hi.category == HitTestCategory::OnItemObscuredAbove
                            || hi.category == HitTestCategory::NotOnItem)
                            index_insert = hi.insertion_index;

                        size_t index_activate = NULL;

                        size_t count = data.get_entry_count();
                        for (size_t i = 0; i < count; i++) {
                            pfc::string8 name;
                            metadb_handle_list handles;
                            mem_block_container_impl sidedata;

                            data.get_entry_name(i, name);
                            data.get_entry_content(i, handles);
                            data.get_side_data(i, sidedata);
                            stream_reader_memblock_ref side_data_reader(
                                static_cast<uint8_t*>(sidedata.get_ptr()), sidedata.get_size());
                            abort_callback_dummy p_abort;
                            size_t index = m_playlist_api->create_playlist_ex(
                                name, pfc_infinite, index_insert + i, handles, &side_data_reader, p_abort);
                            if (i == 0)
                                index_activate = index;
                        }
                        if (count)
                            m_playlist_api->set_active_playlist(index_activate);
                    }
                }
            } else {
                pfc::string8 playlist_name;

                metadb_handle_list data;

                dropped_files_data_impl dropped_data;
                m_ole_api->parse_dataobject(pDataObj, dropped_data);

                bool create_new = true;
                size_t idx = pfc_infinite;

                if (hi.category == HitTestCategory::OnUnobscuredItem || hi.category == HitTestCategory::OnGroupHeader
                    || hi.category == HitTestCategory::OnItemObscuredBelow
                    || hi.category == HitTestCategory::OnItemObscuredAbove) {
                    create_new = isAltDown;
                    idx = create_new ? hi.insertion_index : hi.index;
                } else if (hi.category == HitTestCategory::NotOnItem) {
                    create_new = true;
                }

                if (create_new)
                    if (g_get_folder_name(pDataObj, playlist_name) && cfg_replace_drop_underscores)
                        playlist_name.replace_char('_', ' ');

                {
                    class DelayedDropTargetProcesser : public process_locations_notify {
                    public:
                        playlist_position_reference_tracker m_insertIndexTracker;
                        service_ptr_t<PlaylistSwitcher> m_window;
                        bool m_new_playlist{false};
                        pfc::string8 m_playlist_name;

                        void on_completion(const pfc::list_base_const_t<metadb_handle_ptr>& p_items) override
                        {
                            const auto playlist_api = playlist_manager_v4::get();
                            if ((m_new_playlist || m_insertIndexTracker.m_playlist != pfc_infinite)
                                && p_items.get_count()) {
                                if (m_new_playlist) {
                                    if (!m_playlist_name.length()) {
                                        if (cfg_pgen_tf)
                                            m_playlist_name = StringFormatCommonTrackTitle(p_items, cfg_pgenstring);
                                        else
                                            m_playlist_name = "Untitled";
                                    }

                                    m_insertIndexTracker.m_playlist = playlist_api->create_playlist(
                                        m_playlist_name, pfc_infinite, m_insertIndexTracker.m_playlist);
                                } else {
                                    playlist_api->playlist_undo_backup(m_insertIndexTracker.m_playlist);
                                    playlist_api->playlist_clear_selection(m_insertIndexTracker.m_playlist);
                                }

                                playlist_api->playlist_add_items(
                                    m_insertIndexTracker.m_playlist, p_items, bit_array_true());

                                if (main_window::config_get_activate_target_playlist_on_dropped_items())
                                    playlist_api->set_active_playlist(m_insertIndexTracker.m_playlist);
                            }
                        }
                        void on_aborted() override {}

                        DelayedDropTargetProcesser() : m_insertIndexTracker(false) {}
                    };

                    service_ptr_t<DelayedDropTargetProcesser> ptr = new service_impl_t<DelayedDropTargetProcesser>;
                    ptr->m_window = m_window;
                    ptr->m_insertIndexTracker.m_playlist = idx;
                    ptr->m_new_playlist = create_new;
                    ptr->m_playlist_name = playlist_name;
                    dropped_data.to_handles_async(true, m_window->get_wnd(), ptr);
                    SetFocus(m_window->get_wnd());
                }
            }
        }
    }

    m_window->remove_insert_mark();
    m_window->remove_highlight_item();
    uih::ole::set_drop_description(pDataObj, DROPIMAGE_INVALID, "", "");

    m_is_playlists = false;
    m_is_accepted_type = false;

    return S_OK;
}

PlaylistSwitcher::DropTarget::DropTarget(PlaylistSwitcher* p_window)
    : drop_ref_count(0)
    , m_last_rmb(false)
    , m_is_playlists(false)
    , m_is_accepted_type(false)
    , m_window(p_window)
{
    m_ole_api = standard_api_create_t<ole_interaction_v2>();
    m_playlist_api = standard_api_create_t<playlist_manager_v4>();
    m_DropTargetHelper = wil::CoCreateInstanceNoThrow<IDropTargetHelper>(CLSID_DragDropHelper);
}

} // namespace cui::panels::playlist_switcher
