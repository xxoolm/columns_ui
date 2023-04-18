#pragma once

#define OEMRESOURCE
#define NOMINMAX
#define _SILENCE_CLANG_COROUTINE_MESSAGE

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include <algorithm>
#include <atomic>
#include <complex>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string_view>
#include <utility>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <concurrent_unordered_map.h>
#include <concurrent_vector.h>
#include <io.h>
#include <ppl.h>
#include <share.h>

#include <fmt/xchar.h>
#include <gsl/gsl>
#include <range/v3/all.hpp>

// Included before windows.h, because pfc.h includes winsock2.h
#include "../pfc/pfc.h"

#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <dwmapi.h>
#include <Shlwapi.h>
#include <WinUser.h>
#include <gdiplus.h>
#include <zmouse.h>
#include <Uxtheme.h>
#include <wincodec.h>
#include <strsafe.h>

#include <wil/cppwinrt.h>
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>

#include <winrt/windows.foundation.h>
#include <winrt/windows.ui.viewmanagement.h>

#include "../foobar2000/SDK/foobar2000.h"
#include "../foobar2000/SDK/core_api.h"
#include "../foobar2000/SDK/file_info_filter_impl.h"
#include "../foobar2000/SDK/metadb_info_container_impl.h"
#include "../foobar2000/helpers/playlist_position_reference_tracker.h"

#include "../svg-services/api/api.h"
#include "../columns_ui-sdk/ui_extension.h"
#include "../ui_helpers/stdafx.h"
#include "../mmh/stdafx.h"
#include "../fbh/stdafx.h"

using namespace mmh::literals::pcc;
using namespace uih::literals::spx;

#include "functional.h"
#include "resource.h"

#include "utf8api.h"
#include "helpers.h"
#include "config_items.h"
#include "gdiplus.h"
#include "config_vars.h"
#include "config_defaults.h"

using namespace std::literals;
using namespace fmt::literals;
