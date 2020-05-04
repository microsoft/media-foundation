// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#pragma once
#include <windows.h>

// Include ABI composition headers for interop with DCOMP surface handle from MediaEngine
#include <windows.ui.composition.h>
#include <windows.ui.composition.interop.h>

// Include prior to C++/WinRT Headers
#include <wil/cppwinrt.h> 

// C++/WinRT Headers
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>

// Direct3D
#include <d3d11.h>

// Windows Implementation Library
#include <wil/resource.h>
#include <wil/result_macros.h>

// MediaFoundation headers
#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>

// STL headers
#include <functional>
#include <memory>
