#include "stdafx.h"
#include "playlist_tabs.h"
#include "main_window.h"

cfg_guid cfg_default_playlist(create_guid(0x68527c89,0xb0f7,0xf653,0x00,0x53,0x8c,0xeb,0x47,0xe7,0xa3,0xb3), columns_ui::panels::guid_playlist_view_v2);

// {942C36A4-4E28-4cea-9644-F223C9A838EC}
const GUID g_guid_playlist_switcher_tabs_font = 
{ 0x942c36a4, 0x4e28, 0x4cea, { 0x96, 0x44, 0xf2, 0x23, 0xc9, 0xa8, 0x38, 0xec } };

void remove_playlist_helper(t_size index)
{
	static_api_ptr_t<playlist_manager> api;
	if (index == api->get_active_playlist())
	{
		if (index && index + 1 == api->get_playlist_count())
			api->set_active_playlist(index-1);
		//else
		//	api->set_active_playlist(index);
	}
	api->remove_playlist_switch(index);
}


ui_extension::window_factory<playlists_tabs_extension> blah;
ui_extension::window_host_factory<playlists_tabs_extension::window_host_impl> g_tab_host;

HFONT playlists_tabs_extension::g_font
 = nullptr;

void playlists_tabs_extension::get_supported_panels(const pfc::list_base_const_t<uie::window::ptr> & p_windows, bit_array_var & p_mask_unsupported)
{
	service_ptr_t<service_base> temp;
	g_tab_host.instance_create(temp);
	uie::window_host_ptr ptr;
	if (temp->service_query_t(ptr))
		(static_cast<playlists_tabs_extension::window_host_impl*>(ptr.get_ptr()))->set_this(this);
	t_size i, count = p_windows.get_count();
	for(i=0;i<count;i++)
		p_mask_unsupported.set(i, !p_windows[i]->is_available(ptr));
}

bool playlists_tabs_extension::is_point_ours(HWND wnd_point, const POINT & pt_screen, pfc::list_base_t<uie::window::ptr> & p_hierarchy)
{
	if (wnd_point == get_wnd() || IsChild(get_wnd(), wnd_point))
	{
		if (wnd_point == get_wnd() || wnd_point == wnd_tabs)
		{
			p_hierarchy.add_item(this);
			return true;
		}
		else
		{
			{
				uie::splitter_window_v2_ptr sptr;
				if (m_child.is_valid())
				{
					if (m_child->service_query_t(sptr))
					{
						pfc::list_t<uie::window::ptr> temp;
						temp.add_item(this);
						if (sptr->is_point_ours(wnd_point, pt_screen, temp))
						{
							p_hierarchy.add_items(temp);
							return true;
						}
					}
					else if (wnd_point == m_child_wnd || IsChild(m_child_wnd , wnd_point))
					{
						p_hierarchy.add_item(this);
						p_hierarchy.add_item(m_child);
						return true;
					}
				}
			}
		}
	}
	return false;
};

void playlists_tabs_extension::on_font_change()
{
	if (g_font!=nullptr)
	{
		unsigned n, count = list_wnd.get_count();
		for (n=0; n<count; n++)
		{
			HWND wnd = list_wnd[n]->wnd_tabs;
			if (wnd) SendMessage(wnd,WM_SETFONT,(WPARAM)0,MAKELPARAM(0,0));
		}
		DeleteObject(g_font);
	}

	g_font = static_api_ptr_t<cui::fonts::manager>()->get_font(g_guid_playlist_switcher_tabs_font);

	unsigned n, count = list_wnd.get_count();
	for (n=0; n<count; n++)
	{
		HWND wnd = list_wnd[n]->wnd_tabs;
		if (wnd) 
		{
			SendMessage(wnd,WM_SETFONT,(WPARAM)g_font,MAKELPARAM(1,0));
			list_wnd[n]->on_size();
			list_wnd[n]->on_child_position_change();
		}
	}
}



pfc::ptr_list_t<playlists_tabs_extension> playlists_tabs_extension::list_wnd;



void playlists_tabs_extension::kill_switch_timer()
{
	if (m_switch_timer)
	{
		m_switch_timer = 0;
		KillTimer(get_wnd(), SWITCH_TIMER_ID);
	}
}

void playlists_tabs_extension::switch_to_playlist_delayed2(unsigned idx)
{
	//if have a timer already and idxs re same dont bother
	if (!(m_switch_timer && idx == m_switch_playlist))
	{
		if (m_switch_timer) kill_switch_timer();
		m_switch_playlist = idx;
		m_playlist_switched = false;
		m_switch_timer = (SetTimer(get_wnd(), SWITCH_TIMER_ID, cfg_autoswitch_delay, nullptr) !=0);
	}
}


playlists_tabs_extension::playlists_tabs_extension() : m_dragging(false), m_dragging_idx(0), m_playlist_switched(false),
m_switch_timer(false), m_switch_playlist(0), initialised(false), wnd_tabs(nullptr),
m_child_guid(/*cfg_default_playlist*/pfc::guid_null), m_child_wnd(nullptr), m_host_wnd(nullptr), m_child_top(0),
ID_CUSTOM_BASE(NULL), m_mousewheel_delta(0), tabproc(nullptr)
{
	//reset_size_limits();
	memset(&mmi, 0, sizeof(mmi));
	mmi.ptMaxTrackSize.x = MAXLONG;
	mmi.ptMaxTrackSize.y = MAXLONG;
	m_dragging_rect.left=0;
	m_dragging_rect.top=0;
	m_dragging_rect.right=0;
	m_dragging_rect.bottom=0;
};


playlists_tabs_extension::~playlists_tabs_extension()
= default;

LRESULT WINAPI playlists_tabs_extension::main_hook(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	playlists_tabs_extension * p_this;
	LRESULT rv;

	p_this = reinterpret_cast<playlists_tabs_extension*>(GetWindowLongPtr(wnd,GWLP_USERDATA));

	rv = p_this ? p_this->hook(wnd,msg,wp,lp) : DefWindowProc(wnd, msg, wp, lp);;

	return rv;
}

void playlists_tabs_extension::create_child()
{
	destroy_child();
	if (m_host.is_valid())
	{
		ui_extension::window::create_by_guid(m_child_guid, m_child);
		if (m_child.is_valid())
		{
			try {
				abort_callback_dummy abortCallback;
				m_child->set_config_from_ptr(m_child_data.get_ptr(), m_child_data.get_size(), abortCallback);
			} catch (const exception_io &) {};
			m_child_wnd = m_child->create_or_transfer_window(m_host_wnd, ui_extension::window_host_ptr(m_host.get_ptr()));
			if (m_child_wnd)
			{
				//ShowWindow(m_child_wnd, SW_SHOWNORMAL);
				SetWindowPos(m_child_wnd,HWND_TOP,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			}
		}
	}
	reset_size_limits();
	on_size();
	if (IsWindowVisible(get_wnd()))
	{
		get_host()->on_size_limit_change(get_wnd(), uie::size_limit_all);
		ShowWindow(m_child_wnd, SW_SHOWNORMAL);
	}
}

void playlists_tabs_extension::destroy_child()
{
	if (m_child.is_valid())
	{
		m_child->destroy_window();
		m_child_wnd=nullptr;
		m_child.release();
		reset_size_limits();
		if (IsWindowVisible(get_wnd()))
			get_host()->on_size_limit_change(get_wnd(), uie::size_limit_all);
	}
}

bool playlists_tabs_extension::create_tabs()
{
	bool rv = false;
	bool force_close = false;

	static_api_ptr_t<playlist_manager> playlist_api;

	if (cfg_pl_autohide)
	{
		force_close = (playlist_api->get_playlist_count() <= 1);
	}

	if (wnd_tabs && force_close) {DestroyWindow(wnd_tabs); wnd_tabs = nullptr; rv = true;}
	else if (!wnd_tabs && !force_close) 
	{
		int i, t = playlist_api->get_playlist_count();
		pfc::string8 temp;

		int x = 0;
		int y = 0;
		int cx = 0;//bah
		int cy = 0;

		wnd_tabs = CreateWindowEx(0, WC_TABCONTROL, _T("Playlist switcher"),
			WS_CHILD |  WS_TABSTOP | TCS_HOTTRACK | TCS_TABS | (t>1 ? TCS_MULTILINE : 0) | (1 ? WS_VISIBLE : 0), x, y, cx, cy,
			m_host_wnd, HMENU(5002), core_api::get_my_instance(), nullptr);

		if (wnd_tabs)
		{
			SetWindowLongPtr(wnd_tabs,GWLP_USERDATA,(LPARAM)(this));

			if (g_font)
			{
				SendMessage(wnd_tabs,WM_SETFONT,(WPARAM)g_font,MAKELPARAM(0,0));
			}
			else
				on_font_change();

			SetWindowPos(wnd_tabs,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);

			tabproc = (WNDPROC)SetWindowLongPtr(wnd_tabs,GWLP_WNDPROC,(LPARAM)main_hook);

			pfc::string8 temp2;
			for (i = 0; i < t; i++)
			{
				playlist_api->playlist_get_name(i, temp);
				uTabCtrl_InsertItemText(wnd_tabs, i, temp);
			}

			TabCtrl_SetCurSel(wnd_tabs, playlist_api->get_active_playlist());
			set_styles();

			RECT rc;
			GetWindowRect(wnd_tabs, &rc);
			adjust_rect(FALSE, &rc);
			m_child_top = rc.top;

			rv = true;
		}

	}
	if (rv) 
	{
		reset_size_limits();
		get_host()->on_size_limit_change(get_wnd(), uie::size_limit_maximum_height|uie::size_limit_maximum_width|uie::size_limit_minimum_height|uie::size_limit_minimum_width);
		on_size();
	}
	return rv;
}


const GUID & playlists_tabs_extension::get_extension_guid() const
{
	return extension_guid;
}

void playlists_tabs_extension::get_name(pfc::string_base & out)const
{
	out.set_string("Playlist tabs");
}
void playlists_tabs_extension::get_category(pfc::string_base & out)const
{
	out.set_string("Splitters");
}
bool playlists_tabs_extension::get_short_name(pfc::string_base & out)const
{
	out.set_string("Playlist tabs");
	return true;
}

// {ABB72D0D-DBF0-4bba-8C68-3357EBE07A4D}
const GUID playlists_tabs_extension::extension_guid = 
{ 0xabb72d0d, 0xdbf0, 0x4bba, { 0x8c, 0x68, 0x33, 0x57, 0xeb, 0xe0, 0x7a, 0x4d } };




LRESULT WINAPI playlists_tabs_extension::hook(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch(msg)
	{
	case WM_GETDLGCODE:
		return DLGC_WANTALLKEYS;
	case WM_KEYDOWN:
		{
			if (wp != VK_LEFT && wp != VK_RIGHT && get_host()->get_keyboard_shortcuts_enabled() && g_process_keydown_keyboard_shortcuts(wp)) return 0;
			else if (wp == VK_TAB)
			{
				ui_extension::window::g_on_tab(wnd);
				//return 0;
			}
			SendMessage(wnd, WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS), NULL);
		}
		break;
	case WM_SYSKEYDOWN:
		if (get_host()->get_keyboard_shortcuts_enabled() && g_process_keydown_keyboard_shortcuts(wp)) return 0;
		break;
	case WM_LBUTTONDOWN:
		{
			{
				TCHITTESTINFO hittest;
				hittest.pt.x = GET_X_LPARAM(lp);
				hittest.pt.y = GET_Y_LPARAM(lp);
				int idx = TabCtrl_HitTest(wnd_tabs, &hittest);
				if (idx>=0)
				{
					static_api_ptr_t<playlist_manager> playlist_api;
					//if (cfg_playlists_shift_lmb && (wp & MK_SHIFT)) remove_playlist_helper(idx);
					//else 
						if (cfg_drag_pl)
					{
						SetCapture(wnd);
						m_dragging = true;
						m_dragging_idx = idx;
						TabCtrl_GetItemRect(wnd, idx, &m_dragging_rect);
					}
				}
			}
		}
		break;
	case WM_MOUSEMOVE:
		if (m_dragging && (wp & MK_LBUTTON))
		{
			TCHITTESTINFO hittest;
			hittest.pt.x = GET_X_LPARAM(lp);
			hittest.pt.y = GET_Y_LPARAM(lp);
			int idx = TabCtrl_HitTest(wnd_tabs, &hittest);
			if (idx>=0 && !PtInRect(&m_dragging_rect, hittest.pt))
			{
				int cur_idx = m_dragging_idx;
				static_api_ptr_t<playlist_manager> playlist_api;
				int count = playlist_api->get_playlist_count();

				int n = cur_idx;
				order_helper order(count);
				if (n < idx)
				{
					while (n<idx && n < count)
					{
						order.swap(n,n+1);
						n++;
					}
				}
				else if (n > idx)
				{
					while (n>idx && n > 0)
					{
						order.swap(n,n-1);
						n--;
					}
				}
				if (n != cur_idx) 
				{
					TabCtrl_GetItemRect(wnd, n, &m_dragging_rect);
					playlist_api->reorder(order.get_ptr(),count);
					m_dragging_idx = n;
				}
			}
		}
		else m_dragging = false;

		break;
	case WM_LBUTTONUP:
		if (m_dragging)
		{
			m_dragging = false;
			ReleaseCapture();
		}
		break;
	case WM_LBUTTONDBLCLK:
	case WM_MBUTTONUP:
		{
			if (cfg_mclick || 1 || cfg_plm_rename) //in actuality we dont get messages when mouse not on tab here.
			{
				TCHITTESTINFO hittest;
				hittest.pt.x = GET_X_LPARAM(lp);
				hittest.pt.y = GET_Y_LPARAM(lp);
				int idx = TabCtrl_HitTest(wnd_tabs, &hittest);
				static_api_ptr_t<playlist_manager> playlist_api;
				if (idx >= 0) 
				{
					if (cfg_mclick && msg == WM_MBUTTONUP) {remove_playlist_helper(idx);}
					if (cfg_plm_rename && msg == WM_LBUTTONDBLCLK) {g_rename_playlist(idx, get_wnd());}
				}
				else if (1) 
				{
					unsigned new_idx = playlist_api->create_playlist(pfc::string8("Untitled"),pfc_infinite,playlist_api->get_playlist_count());
					playlist_api->set_active_playlist(new_idx);
				}
			}
		}
		break;
	case WM_MOUSEWHEEL:
		if ((GetWindowLongPtr(wnd, GWL_STYLE) & TCS_MULTILINE) == NULL)
		{
			//unsigned scroll_lines = GetNumScrollLines();

			HWND wnd_child = GetWindow(wnd, GW_CHILD);
			WCHAR str_class [129];
			memset(str_class, 0, sizeof(str_class));
			if (wnd_child && RealGetWindowClass (wnd_child, str_class, tabsize(str_class)-1) && !wcscmp(str_class, UPDOWN_CLASS) && IsWindowVisible(wnd_child))
			{

				INT min = NULL, max = NULL, index=NULL;
				BOOL err = FALSE;
				SendMessage(wnd_child, UDM_GETRANGE32, (WPARAM)&min, (LPARAM)&max);
				index = SendMessage(wnd_child, UDM_GETPOS32, (WPARAM)NULL, (LPARAM)&err);

				//if (!err)
				{
					if (max)
					{
						int zDelta = short(HIWORD(wp));

						//int delta = MulDiv(zDelta, scroll_lines, 120);
						m_mousewheel_delta += zDelta;
						int scroll_lines = 1;//GetNumScrollLines();
						//if (scroll_lines == -1)
							//scroll_lines = count;

						if (m_mousewheel_delta*scroll_lines >= WHEEL_DELTA)
						{
							if (index>min)
							{
								SendMessage(wnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, index-1), NULL);
								SendMessage(wnd, WM_HSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0), NULL);
								SendMessage(wnd_child, UDM_SETPOS32, NULL, index-1);
							}
							m_mousewheel_delta=0;
						}
						else if (m_mousewheel_delta*scroll_lines <= -WHEEL_DELTA)
						{
							if (index+1<=max)
							{
								SendMessage(wnd, WM_HSCROLL, MAKEWPARAM(SB_THUMBPOSITION, index+1), NULL);
								SendMessage(wnd, WM_HSCROLL, MAKEWPARAM(SB_ENDSCROLL, 0), NULL);
								SendMessage(wnd_child, UDM_SETPOS32, NULL, index+1);
							}
							m_mousewheel_delta=0;
						}
					}
				}

				return 0;
			}
		}
		break;
	}
	return uCallWindowProc(tabproc,wnd,msg,wp,lp);
}

void playlists_tabs_extension::on_child_position_change()
{
	reset_size_limits();
	get_host()->on_size_limit_change(get_wnd(), uie::size_limit_maximum_height | uie::size_limit_maximum_width | uie::size_limit_minimum_height | uie::size_limit_minimum_width);
	//on_size();
}

bool playlists_tabs_extension::show_config_popup(unsigned p_index, HWND wnd_parent)
{
	if (p_index == 0 && m_child_guid != pfc::guid_null)
	{
		abort_callback_dummy abortCallback;
		uie::window_ptr p_window = m_child;
		if (!p_window.is_valid())
		{
			if (uie::window::create_by_guid(m_child_guid, p_window))
			{
				p_window->set_config_from_ptr(m_child_data.get_ptr(), m_child_data.get_size(), abortCallback);
			}
		}
		if (p_window.is_valid())
		{
			bool rv = p_window->show_config_popup(wnd_parent);
			if (rv)
			{
				p_window->get_config_to_array(m_child_data, abortCallback, true);
			}
		}
	}
	return false;
}

bool playlists_tabs_extension::have_config_popup(unsigned p_index) const
{
	if (p_index == 0 && m_child_guid != pfc::guid_null)
	{
		uie::window_ptr p_window = m_child;
		if (!p_window.is_valid())
			uie::window::create_by_guid(m_child_guid, p_window);
		if (p_window.is_valid())
			return p_window->have_config_popup();
	}
	return false;
}

void playlists_tabs_extension::get_config(stream_writer * out, abort_callback & p_abort) const
{
	out->write_lendian_t(m_child_guid, p_abort);
	if (m_child.is_valid())
	{
		pfc::array_t<t_uint8> data;
		stream_writer_memblock_ref w(data);
		abort_callback_dummy abortCallback;
		m_child->get_config(&w, abortCallback);
		out->write_lendian_t(data.get_size(), p_abort);
		out->write(data.get_ptr(), data.get_size(), p_abort);
	}
	else
	{
		out->write_lendian_t(m_child_data.get_size(), p_abort);
		out->write(m_child_data.get_ptr(), m_child_data.get_size(), p_abort);
	}
}

void playlists_tabs_extension::set_config(stream_reader * config, t_size p_size, abort_callback & p_abort)
{
	if (p_size)
	{
		config->read_lendian_t(m_child_guid, p_abort);
		unsigned size = 0;
		config->read_lendian_t(size, p_abort);
		m_child_data.set_size(0);
		m_child_data.set_size(size);
		config->read(m_child_data.get_ptr(), size, p_abort);
	}
}

void playlists_tabs_extension::export_config(stream_writer * p_writer, abort_callback & p_abort) const
{
	abort_callback_dummy abortCallback;
	p_writer->write_lendian_t(m_child_guid, p_abort);
	uie::window_ptr ptr = m_child;
	if (!ptr.is_valid() && m_child_guid != pfc::guid_null)
	{
		if (uie::window::create_by_guid(m_child_guid, ptr))
		{
			try {
				ptr->set_config_from_ptr(m_child_data.get_ptr(), m_child_data.get_size(), abortCallback);
			}
			catch (const exception_io &) {};
		}
		else throw cui::fcl::exception_missing_panel();
	}
	pfc::array_t<t_uint8> data;
	stream_writer_memblock_ref w(data);
	if (ptr.is_valid())
		ptr->export_config(&w, abortCallback);
	p_writer->write_lendian_t(data.get_size(), p_abort);
	p_writer->write(data.get_ptr(), data.get_size(), p_abort);
}

void playlists_tabs_extension::import_config(stream_reader * p_reader, t_size p_size, abort_callback & p_abort)
{
	if (p_size)
	{
		p_reader->read_lendian_t(m_child_guid, p_abort);
		unsigned size = 0;
		p_reader->read_lendian_t(size, p_abort);
		m_child_data.set_size(0);
		//m_child_data.set_size(size);
		pfc::array_t<t_uint8> data;
		data.set_size(size);
		p_reader->read(data.get_ptr(), size, p_abort);
		uie::window_ptr ptr;
		if (uie::window::create_by_guid(m_child_guid, ptr))
		{
			try {
				ptr->import_config_from_ptr(data.get_ptr(), data.get_size(), p_abort);
			}
			catch (const exception_io &) {};
			ptr->get_config_to_array(m_child_data, p_abort, true);
		}
		//p_reader->read(m_child_data.get_ptr(), size, p_abort);
	}
}

uie::splitter_item_t * playlists_tabs_extension::get_panel(unsigned index) const
{
	auto  ptr = new uie::splitter_item_simple_t;
	ptr->set_panel_guid(m_child_guid);
	ptr->set_panel_config_from_ptr(m_child_data.get_ptr(), m_child_data.get_size());
	if (index == 0 && m_child_guid != pfc::guid_null)
	{
		if (m_child_wnd && m_child.is_valid())
			ptr->set_window_ptr(m_child);
		//else
		//	p_out.set_window_ptr(uie::window_ptr(NULL));
	}
	return ptr;
}

unsigned playlists_tabs_extension::get_maximum_panel_count() const
{
	return 1;
}

unsigned playlists_tabs_extension::get_panel_count() const
{
	return m_child_guid != pfc::guid_null ? 1 : 0;
}

void playlists_tabs_extension::replace_panel(unsigned index, const uie::splitter_item_t * p_item)
{
	if (index == 0 && m_child_guid != pfc::guid_null)
	{
		if (initialised)
			destroy_child();

		m_child_data.set_size(0);
		stream_writer_memblock_ref w(m_child_data);
		p_item->get_panel_config(&w);
		m_child_guid = p_item->get_panel_guid();

		if (initialised)
			create_child();
	}
}

void playlists_tabs_extension::remove_panel(unsigned index)
{
	if (index == 0 && m_child_guid != pfc::guid_null)
	{
		if (initialised)
			destroy_child();

		m_child_guid = pfc::guid_null;
		m_child_data.set_size(0);

		if (initialised)
			create_child();
	}
}

void playlists_tabs_extension::insert_panel(unsigned index, const uie::splitter_item_t * p_item)
{
	if (index == 0 && m_child_guid == pfc::guid_null)
	{
		if (initialised)
			destroy_child();

		m_child_data.set_size(0);
		stream_writer_memblock_ref w(m_child_data);
		p_item->get_panel_config(&w);
		m_child_guid = p_item->get_panel_guid();

		if (initialised)
			create_child();
	}
}

unsigned playlists_tabs_extension::get_type() const
{
	return ui_extension::type_layout | uie::type_splitter;
}

void playlists_tabs_extension::on_size(unsigned cx, unsigned cy)
{
	if (wnd_tabs)
	{
		SetWindowPos(wnd_tabs, nullptr, 0, 0, cx, cy, SWP_NOZORDER);
		RECT rc = { 0, 0, cx, cy };
		adjust_rect(FALSE, &rc);
		if (m_child_wnd)
			SetWindowPos(m_child_wnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER);
		unsigned old_top = m_child_top; //prevent braindead (partial) recursion
		m_child_top = rc.top;
		if (rc.top != old_top)
		{
			//PostMessage(get_wnd(), WM_USER+3,0,0);
			on_child_position_change();
			//console::formatter() << "old top: " << old_top << " new top: " << rc.top;
		}
	}
	else if (m_child_wnd)
	{
		SetWindowPos(m_child_wnd, nullptr, 0, 0, cx, cy, SWP_NOZORDER);
	}
}

void playlists_tabs_extension::on_size()
{
	RECT rc;
	GetWindowRect(get_wnd(), &rc);
	on_size(rc.right - rc.left, rc.bottom - rc.top);
}

void playlists_tabs_extension::adjust_rect(bool b_larger, RECT * rc)
{
	if (b_larger)
	{
		RECT rc_child = *rc;
		TabCtrl_AdjustRect(wnd_tabs, FALSE, &rc_child);
		rc_child.top = rc->top + 2;
		TabCtrl_AdjustRect(wnd_tabs, TRUE, &rc_child);
		*rc = rc_child;
	}
	else
	{
		RECT rc_tabs;
		rc_tabs = *rc;
		TabCtrl_AdjustRect(wnd_tabs, FALSE, &rc_tabs);
		rc->top = rc_tabs.top - 2;
	}
}

void playlists_tabs_extension::reset_size_limits()
{
	memset(&mmi, 0, sizeof(mmi));
	if (m_child_wnd)
	{
		mmi.ptMinTrackSize.x = 0;
		mmi.ptMinTrackSize.y = 0;
		mmi.ptMaxTrackSize.x = MAXLONG;
		mmi.ptMaxTrackSize.y = MAXLONG;
		SendMessage(m_child_wnd, WM_GETMINMAXINFO, 0, (LPARAM)&mmi);
		if (mmi.ptMinTrackSize.x != 0 || mmi.ptMinTrackSize.y != 0 || mmi.ptMaxTrackSize.x != MAXLONG || mmi.ptMaxTrackSize.y != MAXLONG)
		{
			RECT rc_min = { 0, 0, mmi.ptMinTrackSize.x, mmi.ptMinTrackSize.y };
			RECT rc_max = { 0, 0, mmi.ptMaxTrackSize.x, mmi.ptMaxTrackSize.y };
			if (wnd_tabs)
			{
				adjust_rect(TRUE, &rc_min);
				adjust_rect(TRUE, &rc_max);
			}
			mmi.ptMinTrackSize.x = rc_min.right - rc_min.left;
			mmi.ptMinTrackSize.y = rc_min.bottom - rc_min.top;
			mmi.ptMaxTrackSize.x = rc_max.right - rc_max.left;
			mmi.ptMaxTrackSize.y = rc_max.bottom - rc_max.top;
		}
		else
		{
			if (wnd_tabs)
			{
				RECT rc, rc_child;
				GetWindowRect(wnd_tabs, &rc);
				MapWindowPoints(HWND_DESKTOP, get_wnd(), (LPPOINT)&rc, 2);
				rc_child = rc;
				adjust_rect(FALSE, &rc_child);
				mmi.ptMinTrackSize.x = rc_child.left - rc.left + rc.right - rc_child.right;
				mmi.ptMinTrackSize.y = rc_child.top - rc.top + rc.bottom - rc_child.bottom;
			}
			mmi.ptMaxTrackSize.x = MAXLONG;
			mmi.ptMaxTrackSize.y = MAXLONG;
		}
	}
	else
	{
		if (wnd_tabs)
		{
			RECT rc_tabs, rc_child;
			GetWindowRect(wnd_tabs, &rc_tabs);
			MapWindowPoints(HWND_DESKTOP, get_wnd(), (LPPOINT)&rc_tabs, 2);
			rc_child = rc_tabs;
			adjust_rect(FALSE, &rc_child);
			mmi.ptMinTrackSize.x = rc_child.left - rc_tabs.left + rc_tabs.right - rc_child.right;
			mmi.ptMinTrackSize.y = rc_child.top - rc_tabs.top + rc_tabs.bottom - rc_child.bottom;
			mmi.ptMaxTrackSize.y = mmi.ptMinTrackSize.y;
			mmi.ptMaxTrackSize.x = MAXLONG;
		}
		else
		{
			mmi.ptMaxTrackSize.x = MAXLONG;
			mmi.ptMaxTrackSize.y = MAXLONG;
		}
	}
}

void playlists_tabs_extension::set_styles(bool visible /*= true*/)
{
	if (wnd_tabs)
	{
		long flags = WS_CHILD | TCS_HOTTRACK | TCS_TABS | ((cfg_tabs_multiline && (TabCtrl_GetItemCount(wnd_tabs) > 1)) ? TCS_MULTILINE | TCS_RIGHTJUSTIFY : TCS_SINGLELINE) | (visible ? WS_VISIBLE : 0) | WS_CLIPSIBLINGS | WS_TABSTOP | 0;

		if (GetWindowLongPtr(wnd_tabs, GWL_STYLE) != flags)
			SetWindowLongPtr(wnd_tabs, GWL_STYLE, flags);
	}
}

void playlists_tabs_extension::on_playlist_locked(unsigned int, bool)
{

}

void playlists_tabs_extension::on_playback_order_changed(unsigned int)
{

}

void playlists_tabs_extension::on_default_format_changed(void)
{

}

void playlists_tabs_extension::on_playlists_removing(const bit_array &, unsigned int, unsigned int)
{

}

void playlists_tabs_extension::on_item_ensure_visible(unsigned int, unsigned int)
{

}

void playlists_tabs_extension::on_items_replaced(unsigned int, const bit_array &, const pfc::list_base_const_t<t_on_items_replaced_entry> &)
{

}

void playlists_tabs_extension::on_items_modified_fromplayback(unsigned int, const bit_array &, play_control::t_display_level)
{

}

void playlists_tabs_extension::on_items_modified(unsigned int, const bit_array &)
{

}

void playlists_tabs_extension::on_item_focus_change(unsigned int, unsigned int, unsigned int)
{

}

void playlists_tabs_extension::on_items_selection_change(unsigned int, const bit_array &, const bit_array &)
{

}

void playlists_tabs_extension::on_items_reordered(unsigned int, const unsigned int *, unsigned int)
{

}

void playlists_tabs_extension::on_items_added(unsigned int, unsigned int, const pfc::list_base_const_t<metadb_handle_ptr> &, const bit_array &)
{

}

void playlists_tabs_extension::on_playlist_renamed(unsigned p_index, const char * p_new_name, unsigned p_new_name_len)
{
	if (wnd_tabs)
	{
		uTabCtrl_InsertItemText(wnd_tabs, p_index, pfc::string8(p_new_name, p_new_name_len), false);
		if (cfg_tabs_multiline) on_size();
	}
}

void playlists_tabs_extension::on_playlists_removed(const bit_array & p_mask, unsigned p_old_count, unsigned p_new_count)
{
	bool need_move = false;

	if (create_tabs()) need_move = true;
	else
		if (wnd_tabs)
		{
			unsigned n = p_old_count;
			for (; n > 0; n--)
			{
				if (p_mask[n - 1])
					TabCtrl_DeleteItem(wnd_tabs, n - 1);
			}
			set_styles();
			if (cfg_tabs_multiline) on_size();
			//RedrawWindow(wnd_tabs, NULL, NULL, RDW_ERASE|RDW_INVALIDATE);
		}

	if (need_move) on_size();
}

void playlists_tabs_extension::on_playlist_created(unsigned p_index, const char * p_name, unsigned p_name_len)
{
	if (wnd_tabs)
	{
		uTabCtrl_InsertItemText(wnd_tabs, p_index, pfc::string8(p_name, p_name_len));
		set_styles();
		if (cfg_tabs_multiline) on_size();
	}
	else if (create_tabs()) on_size();
}

void playlists_tabs_extension::on_playlists_reorder(const unsigned * p_order, unsigned p_count)
{
	if (wnd_tabs)
	{
		static_api_ptr_t<playlist_manager> playlist_api;
		unsigned n;
		int sel = playlist_api->get_active_playlist();

		for (n = 0; n < p_count; n++)
		{
			if (n != (unsigned)p_order[n])
			{
				pfc::string8 temp, temp2;
				playlist_api->playlist_get_name(n, temp);

				uTabCtrl_InsertItemText(wnd_tabs, n, temp, false);
			}
		}
		TabCtrl_SetCurSel(wnd_tabs, sel);
	}
}

void playlists_tabs_extension::on_playlist_activate(unsigned p_old, unsigned p_new)
{
	if (wnd_tabs)
	{
		TabCtrl_SetCurSel(wnd_tabs, p_new);
	}
}

void FB2KAPI playlists_tabs_extension::on_items_removed(unsigned p_playlist, const bit_array & p_mask, unsigned p_old_count, unsigned p_new_count)
{

}

void FB2KAPI playlists_tabs_extension::on_items_removing(unsigned p_playlist, const bit_array & p_mask, unsigned p_old_count, unsigned p_new_count)
{

}

playlists_tabs_extension::class_data & playlists_tabs_extension::get_class_data() const
{
	__implement_get_class_data_ex(_T("{ABB72D0D-DBF0-4bba-8C68-3357EBE07A4D}"), _T(""), false, 0, WS_CHILD | WS_CLIPCHILDREN, WS_EX_CONTROLPARENT, CS_DBLCLKS);
}


void g_on_autohide_tabs_change()
{
	unsigned n, count = playlists_tabs_extension::list_wnd.get_count();
	for (n=0; n<count; n++)
	{
		playlists_tabs_extension::list_wnd[n]->create_tabs();
	}
}

void g_on_multiline_tabs_change()
{
	unsigned n, count = playlists_tabs_extension::list_wnd.get_count();
	for (n=0; n<count; n++)
	{
		playlists_tabs_extension * p_tabs = playlists_tabs_extension::list_wnd[n];
		p_tabs->set_styles();
		p_tabs->on_size();
	}
}

void g_on_tabs_font_change()
{
	playlists_tabs_extension::on_font_change();
}

class font_client_switcher_tabs : public cui::fonts::client
{
public:
	const GUID & get_client_guid() const override
	{
		return g_guid_playlist_switcher_tabs_font;
	}
	void get_name (pfc::string_base & p_out) const override
	{
		p_out = "Playlist Tabs";
	}

	cui::fonts::font_type_t get_default_font_type() const override
	{
		return cui::fonts::font_type_labels;
	}

	void on_font_changed() const override 
	{
		playlists_tabs_extension::on_font_change();
	}
};

font_client_switcher_tabs::factory<font_client_switcher_tabs> g_font_client_switcher_tabs;

void playlists_tabs_extension::window_host_impl::set_this(playlists_tabs_extension * ptr)
{
	m_this = ptr;
}

void playlists_tabs_extension::window_host_impl::relinquish_ownership(HWND wnd)
{
	m_this->m_child_wnd = nullptr;
	m_this->m_host.release();
	m_this->m_child.release();
	m_this->reset_size_limits();
}

bool playlists_tabs_extension::window_host_impl::override_status_text_create(service_ptr_t<ui_status_text_override> & p_out)
{
	static_api_ptr_t<ui_control> api;
	return m_this->get_host()->override_status_text_create(p_out);
}

const GUID & playlists_tabs_extension::window_host_impl::get_host_guid() const
{
	// {20789B52-4998-43ae-9B20-CCFD3BFBEEBD}
	static const GUID guid =
	{ 0x20789b52, 0x4998, 0x43ae, { 0x9b, 0x20, 0xcc, 0xfd, 0x3b, 0xfb, 0xee, 0xbd } };
	return guid;
}

void playlists_tabs_extension::window_host_impl::on_size_limit_change(HWND wnd, unsigned flags)
{
	m_this->on_child_position_change();
}

bool playlists_tabs_extension::window_host_impl::get_show_shortcuts() const
{
	return m_this->get_host()->get_keyboard_shortcuts_enabled();
}

bool playlists_tabs_extension::window_host_impl::get_keyboard_shortcuts_enabled() const
{
	return m_this->get_host()->get_keyboard_shortcuts_enabled();
}

bool playlists_tabs_extension::window_host_impl::set_window_visibility(HWND wnd, bool visibility)
{
	return false;
}

bool playlists_tabs_extension::window_host_impl::is_visibility_modifiable(HWND wnd, bool desired_visibility) const
{
	return false;
}

bool playlists_tabs_extension::window_host_impl::is_visible(HWND wnd) const
{
	return true;
}

bool playlists_tabs_extension::window_host_impl::request_resize(HWND wnd, unsigned flags, unsigned width, unsigned height)
{
	if (flags == ui_extension::size_height && is_resize_supported(wnd))
	{
		if (m_this->wnd_tabs)
		{
			RECT rc;
			GetWindowRect(m_this->m_child_wnd, &rc);
			MapWindowPoints(HWND_DESKTOP, m_this->get_wnd(), (LPPOINT)&rc, 2);
			rc.bottom = rc.top + height;
			m_this->adjust_rect(TRUE, &rc);
			//We would expect rc.top and rc.left to be 0.
			return m_this->get_host()->request_resize(m_this->get_wnd(), flags, rc.right, rc.bottom);
		}
		else
			return m_this->get_host()->request_resize(m_this->get_wnd(), flags, width, height);
	}
	return false;
}

unsigned playlists_tabs_extension::window_host_impl::is_resize_supported(HWND wnd) const
{
	//We won't support ui_extension::size_width since we can't reliably detect multiline tab shit
	return (m_this->get_host()->is_resize_supported(m_this->get_wnd()) & ui_extension::size_height);
}

unsigned playlists_tabs_extension::window_host_impl::get_supported_types() const
{
	return ui_extension::type_panel;
}
