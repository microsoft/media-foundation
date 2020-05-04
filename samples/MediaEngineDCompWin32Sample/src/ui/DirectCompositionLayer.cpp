// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include "pch.h"
#include "DirectCompositionLayer.h"

using namespace Microsoft::WRL;

namespace ui
{

namespace
{
    winrt::com_ptr<IDCompositionVisual> CreateVisualFromSurfaceHandle(IDCompositionDevice* device, HANDLE surfaceHandle)
    {
        winrt::com_ptr<IDCompositionSurface> surface;
        THROW_IF_FAILED(device->CreateSurfaceFromHandle(surfaceHandle, reinterpret_cast<IUnknown**>(surface.put())));
        winrt::com_ptr<IDCompositionVisual> visual;
        THROW_IF_FAILED(device->CreateVisual(visual.put()));
        THROW_IF_FAILED(visual->SetContent(surface.get()));
        return visual;
    }
} // namespace

DirectCompositionLayer::DirectCompositionLayer(IDCompositionVisual* visual)
{
    m_visual.copy_from(visual);
}

IDCompositionVisual* DirectCompositionLayer::GetVisual() { return m_visual.get(); }

std::shared_ptr<DirectCompositionLayer> DirectCompositionLayer::CreateFromSurface(IDCompositionDevice* device, HANDLE surfaceHandle)
{
    winrt::com_ptr<IDCompositionVisual> visual = CreateVisualFromSurfaceHandle(device, surfaceHandle);
    return std::make_shared<DirectCompositionLayer>(visual.get());
}

} // namespace ui