#pragma once
#include <windows.h>
#include <unknwn.h>
#include <restrictederrorinfo.h>
#include <hstring.h>

// Undefine GetCurrentTime macro to prevent
// conflict with Storyboard::GetCurrentTime
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <wil/cppwinrt_helpers.h>


#include <d3d11_4.h>
#include <microsoft.ui.xaml.media.dxinterop.h>
#include <wil/result.h>
#include <wrl.h>
#include <windows.media.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <Mfapi.h>
#include <Mfidl.h>
#include <Mfreadwrite.h>
#include <strsafe.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <functional>

#define SAFE_RELEASE(x) { if (nullptr != (x))  {(void) (x)->Release(); (x) = nullptr;} }
#define MEDIA_FILE_NAME L"http://distribution.bbb3d.renderfarming.net/video/mp4/bbb_sunflower_1080p_30fps_normal.mp4"