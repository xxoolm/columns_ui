#include "pch.h"

#include "common.h"
#include "playlist_tabs.h"

namespace cui::panels::playlist_tabs {

HRESULT STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::QueryInterface(REFIID riid, LPVOID FAR* ppvObject)
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
ULONG STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::AddRef()
{
    return InterlockedIncrement(&drop_ref_count);
}
ULONG STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::Release()
{
    LONG rv = InterlockedDecrement(&drop_ref_count);
    if (!rv) {
#ifdef _DEBUG
        OutputDebugString(_T("deleting playlists_tabs_extension"));
#endif
        delete this;
    }
    return rv;
}

HRESULT STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::DragEnter(
    IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragEnter(p_list->get_wnd(), pDataObj, &pt, *pdwEffect);
    m_DataObject = pDataObj;

    last_over.x = 0;
    last_over.y = 0;
    m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

    if (ui_drop_item_callback::g_is_accepted_type(pDataObj, pdwEffect)) {
        m_is_accepted_type = true;
    } else if (playlist_incoming_item_filter::get()->process_dropped_files_check(pDataObj)) {
        *pdwEffect = DROPEFFECT_COPY;
        m_is_accepted_type = true;
    } else
        m_is_accepted_type = false;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::DragOver(
    DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    bool isAltDown = (grfKeyState & MK_ALT) != 0;
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragOver(&pt, *pdwEffect);

    if (ui_drop_item_callback::g_is_accepted_type(m_DataObject.get(), pdwEffect))
        return S_OK;

    *pdwEffect = DROPEFFECT_COPY;
    m_last_rmb = ((grfKeyState & MK_RBUTTON) != 0);

    if (last_over.x != pt.x || last_over.y != pt.y) {
        last_over = ptl;

        if (!m_is_accepted_type) {
            *pdwEffect = DROPEFFECT_NONE;
            uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");
            return S_OK;
        }

        const auto playlist_api = playlist_manager::get();
        POINT pti;
        pti.y = pt.y;
        pti.x = pt.x;

        POINT ptm = pti;
        ScreenToClient(p_list->get_wnd(), &ptm);

        HWND wnd = ChildWindowFromPointEx(p_list->get_wnd(), ptm, CWP_SKIPINVISIBLE);

        //    RECT plist;

        //    GetWindowRect(g_plist, &plist);
        //    RECT tabs;

        //    GetWindowRect(g_tab, &tabs);

        if (p_list->wnd_tabs) {
            POINT pttab = pti;

            if (wnd == p_list->wnd_tabs && ScreenToClient(p_list->wnd_tabs, &pttab)) {
                TCHITTESTINFO hittest;
                hittest.pt.x = pttab.x;
                hittest.pt.y = pttab.y;
                int idx = TabCtrl_HitTest(p_list->wnd_tabs, &hittest);
                const auto old = playlist_api->get_active_playlist();
                if (cfg_drag_autoswitch && idx >= 0 && old != gsl::narrow<size_t>(idx) && !isAltDown)
                    p_list->switch_to_playlist_delayed2(idx); // playlist_switcher::get()->set_active_playlist(idx);
                else
                    p_list->kill_switch_timer();

                if (idx != -1 && !isAltDown) {
                    pfc::string8 name;
                    playlist_manager::get()->playlist_get_name(idx, name);
                    uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_COPY, "Add to %1", name);
                } else
                    uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_COPY, "Add to new playlist", "");
            } else
                uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_COPY, "Add to new playlist", "");
        }
        if ((!p_list->wnd_tabs || wnd != p_list->wnd_tabs))
            p_list->kill_switch_timer();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::DragLeave()
{
    if (m_DropTargetHelper)
        m_DropTargetHelper->DragLeave();
    last_over.x = 0;
    last_over.y = 0;
    p_list->kill_switch_timer();
    uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");
    m_DataObject.reset();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE PlaylistTabs::PlaylistTabsDropTarget::Drop(
    IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect)
{
    POINT pt = {ptl.x, ptl.y};
    bool isAltDown = (grfKeyState & MK_ALT) != 0;
    if (m_DropTargetHelper)
        m_DropTargetHelper->Drop(pDataObj, &pt, *pdwEffect);

    p_list->kill_switch_timer();
    m_DataObject.reset();
    last_over.x = 0;
    last_over.y = 0;

    if (!m_is_accepted_type) {
        uih::ole::set_drop_description(m_DataObject.get(), DROPIMAGE_INVALID, "", "");
        return S_OK;
    }

    const auto playlist_api = playlist_manager::get();

    POINT pti;
    pti.y = pt.y;
    pti.x = pt.x;

    POINT ptm = pti;
    ScreenToClient(p_list->get_wnd(), &ptm);

    HWND wnd = ChildWindowFromPointEx(p_list->get_wnd(), ptm, CWP_SKIPINVISIBLE);

    if (wnd) {
        bool process = !ui_drop_item_callback::g_on_drop(pDataObj);

        bool send_new_playlist = false;

        if (process && m_last_rmb) {
            process = false;
            enum { ID_DROP = 1, ID_NEW_PLAYLIST, ID_CANCEL };

            HMENU menu = CreatePopupMenu();

            uAppendMenu(menu, (MF_STRING), ID_DROP, "&Add files here");
            uAppendMenu(menu, (MF_STRING), ID_NEW_PLAYLIST, "&Add files to new playlist");
            uAppendMenu(menu, MF_SEPARATOR, 0, nullptr);
            uAppendMenu(menu, MF_STRING, ID_CANCEL, "&Cancel");

            int cmd = TrackPopupMenu(
                menu, TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD, pt.x, pt.y, 0, p_list->get_wnd(), nullptr);
            DestroyMenu(menu);

            if (cmd) {
                switch (cmd) {
                case ID_DROP:
                    process = true;
                    break;
                case ID_NEW_PLAYLIST:
                    process = true;
                    send_new_playlist = true;
                    break;
                }
            }
        }

        if (process) {
            metadb_handle_list data;
            const auto incoming_api = playlist_incoming_item_filter::get();
            incoming_api->process_dropped_files(pDataObj, data, true, p_list->get_wnd());

            POINT pttab = pti;

            size_t newPlaylistIndex = pfc_infinite;
            size_t target_index = playlist_api->get_active_playlist();

            if (p_list->wnd_tabs && wnd == p_list->wnd_tabs) {
                RECT tabs;

                GetWindowRect(p_list->wnd_tabs, &tabs);
                if (ScreenToClient(p_list->wnd_tabs, &pttab)) {
                    TCHITTESTINFO hittest;
                    hittest.pt.x = pttab.x;
                    hittest.pt.y = pttab.y;
                    int idx = TabCtrl_HitTest(p_list->wnd_tabs, &hittest);

                    if (send_new_playlist || idx < 0 || isAltDown) {
                        send_new_playlist = true;
                        if (idx >= 0)
                            newPlaylistIndex = idx;
                    } else
                        target_index = idx;
                }
            }

            if (send_new_playlist) {
                pfc::string8 playlist_name("Untitled");

                bool named = false;

                if (true || true) {
                    FORMATETC fe;
                    STGMEDIUM sm;
                    HRESULT hr = E_FAIL;

                    //                    memset(&sm, 0, sizeof(0));

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
                                if (uGetFileAttributes(temp) & FILE_ATTRIBUTE_DIRECTORY) {
                                    playlist_name.set_string(string_filename_ext(temp));
                                    named = true;
                                } else {
                                    playlist_name.set_string(string_filename(temp));
                                    named = true;
#if 0
                                    pfc::string_extension ext(temp);

                                    service_enum_t<playlist_loader> e;
                                    service_ptr_t<playlist_loader> l;
                                    if (e.first(l))
                                        do
                                        {
                                            if (!strcmp(l->get_extension(), ext))
                                            {
                                                playlist_name.set_string(pfc::string_filename(temp));
                                                named = true;
                                                l.release();
                                                break;
                                            }
                                            l.release();
                                        } while (e.next(l));
#endif
                                }
                            }
                        }

                        ReleaseStgMedium(&sm);
                    }
                }

                size_t new_idx;
                if (newPlaylistIndex == pfc_infinite)
                    newPlaylistIndex = playlist_api->get_playlist_count();

                if (named && cfg_replace_drop_underscores)
                    playlist_name.replace_char('_', ' ');
                if (!named && cfg_pgen_tf)
                    new_idx = playlist_api->create_playlist(
                        StringFormatCommonTrackTitle(data, cfg_pgenstring), pfc_infinite, newPlaylistIndex);

                else
                    new_idx = playlist_api->create_playlist(playlist_name, pfc_infinite, newPlaylistIndex);

                playlist_api->playlist_add_items(new_idx, data, bit_array_false());
                if (main_window::config_get_activate_target_playlist_on_dropped_items())
                    playlist_api->set_active_playlist(new_idx);

            } else {
                playlist_api->playlist_clear_selection(target_index);
                playlist_api->playlist_add_items(target_index, data, bit_array_true());
                if (main_window::config_get_activate_target_playlist_on_dropped_items())
                    playlist_api->set_active_playlist(target_index);
            }

            data.remove_all();
        }
    }

    return S_OK;
}
PlaylistTabs::PlaylistTabsDropTarget::PlaylistTabsDropTarget(PlaylistTabs* p_wnd) : p_list(p_wnd)
{
    m_DropTargetHelper = wil::CoCreateInstanceNoThrow<IDropTargetHelper>(CLSID_DragDropHelper);
}

} // namespace cui::panels::playlist_tabs
