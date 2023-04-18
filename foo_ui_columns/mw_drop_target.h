#pragma once

class MainWindowDropTarget : public IDropTarget {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID FAR* ppvObject) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE DragEnter(
        IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect) override;

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect) override;

    HRESULT STDMETHODCALLTYPE DragLeave() override;

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL ptl, DWORD* pdwEffect) override;
    MainWindowDropTarget();

private:
    static bool check_window_allowed(HWND wnd);

    long drop_ref_count{0};
    POINTL last_over{};
    wil::com_ptr_t<IDropTargetHelper> m_DropTargetHelper;
    wil::com_ptr_t<IDataObject> m_DataObject;
};
