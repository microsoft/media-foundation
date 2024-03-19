// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <tuple>
#include <array>

#include <windows.h>

#include <d3d11_4.h>
#include <Audioclient.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>
#include <mfidl.h>
#include <mfcontentdecryptionmodule.h>

// Include ABI composition headers for interop with DCOMP surface handle from MediaEngine
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>

// Include prior to WinRT Headers
#include <wil/cppwinrt.h> 

// WinRT Headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.Media.Protection.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.Headers.h>
#include <winrt/Windows.Data.Xml.Dom.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.ApplicationModel.h>

// Windows Implementation Library
#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

// GSL (C++ Guidelines Support Library)
#include <gsl/span>

////////////////////////////////////////////////////////////////////////////////

#pragma warning(disable: 4458) // declaration of ... hides class member

#include "..\..\ContentDecryptionModule01\Helper.h"
