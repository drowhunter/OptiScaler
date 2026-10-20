#include "pch.h"
#include "codiesvr_bridge.h"

#include <Logger.h>
#include <imgui/imgui_impl_dx12.h>
#include <wrl/client.h>
#include <array>
#include <atomic>
#include <mutex>

// Unofficial CodiesVR integration, modified 2026-10-19. GPL-3.0.
namespace {
using Microsoft::WRL::ComPtr;
constexpr uint32_t kSlots = 8;
std::mutex g_mutex;
ULONGLONG g_lastPoll = 0;
ULONGLONG g_lastMenuUpdate = 0;
bool g_visible = false;
uint32_t g_shortcut = VK_INSERT;
ComPtr<IUnknown> g_consumerQueue;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
std::array<ComPtr<ID3D12Resource>, kSlots> g_textures;
ComPtr<ID3D12Resource> g_published;
codiesvr::opti::GuideCallback g_guides = nullptr;
void* g_user = nullptr;
std::atomic<int> g_menuRequest{-1};

HRESULT __cdecl GetOverlay(ID3D12CommandQueue* queue, codiesvr::opti::OverlayFrame* frame) {
    if (frame == nullptr || frame->size != sizeof(*frame)) return E_INVALIDARG;
    *frame = {};
    if (queue == nullptr) {
        std::lock_guard lock(g_mutex);
        g_consumerQueue.Reset();
        g_published.Reset();
        g_lastPoll = 0;
        frame->shortcutKey = g_shortcut;
        frame->visible = g_visible && GetTickCount64() - g_lastMenuUpdate < 250;
        return S_FALSE;
    }
    ComPtr<IUnknown> identity;
    HRESULT hr = queue->QueryInterface(IID_PPV_ARGS(&identity));
    if (FAILED(hr)) return hr;
    std::lock_guard lock(g_mutex);
    if (g_consumerQueue && g_consumerQueue.Get() != identity.Get()) g_published.Reset();
    g_consumerQueue = identity;
    const auto now = GetTickCount64();
    g_lastPoll = now;
    frame->shortcutKey = g_shortcut;
    frame->visible = g_visible && now - g_lastMenuUpdate < 250;
    if (!frame->visible || !g_published) return S_FALSE;
    frame->texture = g_published.Get();
    frame->texture->AddRef();
    return S_OK;
}

void __cdecl SetGuideCallback(codiesvr::opti::GuideCallback callback, void* user) {
    std::lock_guard lock(g_mutex);
    g_guides = callback;
    g_user = user;
}
}  // namespace

extern "C" __declspec(dllexport) HRESULT __cdecl
OptiScaler_CodiesVR_RequestMenuV1(uint32_t visible) {
    if (visible > 1) return E_INVALIDARG;
    g_menuRequest.store(static_cast<int>(visible));
    return S_OK;
}

extern "C" __declspec(dllexport) HRESULT __cdecl
OptiScaler_CodiesVR_GetApiV1(uint32_t version, uint32_t size, codiesvr::opti::Api* api) {
    if (version != codiesvr::opti::kVersion || size != sizeof(*api) || api == nullptr)
        return E_INVALIDARG;
    *api = {};
    api->getOverlay = GetOverlay;
    api->setGuideCallback = SetGuideCallback;
    return S_OK;
}

namespace CodiesVrBridge {
int TakeMenuRequest() { return g_menuRequest.exchange(-1); }

bool Active() {
    std::lock_guard lock(g_mutex);
    return g_consumerQueue && GetTickCount64() - g_lastPoll < 2000;
}

void UpdateMenuState(bool visible, uint32_t shortcutKey) {
    std::lock_guard lock(g_mutex);
    g_visible = visible;
    g_shortcut = shortcutKey;
    g_lastMenuUpdate = GetTickCount64();
    if (!visible) g_published.Reset();
}

void ClearOverlay() {
    std::lock_guard lock(g_mutex);
    g_published.Reset();
}

void ResetOverlay() {
    std::lock_guard lock(g_mutex);
    g_published.Reset();
    g_textures = {};
    g_rtvHeap.Reset();
    g_visible = false;
}

bool DrawOverlay(ID3D12Device* device, ID3D12CommandQueue* queue,
                 ID3D12GraphicsCommandList* list, const D3D12_RESOURCE_DESC& backbuffer,
                 uint32_t slot) {
    if (!Active() || slot >= kSlots) return false;
    ComPtr<IUnknown> identity;
    if (FAILED(queue->QueryInterface(IID_PPV_ARGS(&identity)))) {
        LOG_ERROR("CodiesVR: cannot identify the overlay queue");
        return false;
    }
    {
        std::lock_guard lock(g_mutex);
        if (identity.Get() != g_consumerQueue.Get()) {
            static bool warned = false;
            if (!warned) LOG_ERROR("CodiesVR: different command queues; VR menu disabled");
            warned = true;
            g_published.Reset();
            return false;
        }
    }
    if (backbuffer.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
        backbuffer.Format != DXGI_FORMAT_B8G8R8A8_UNORM) {
        static bool warned = false;
        if (!warned) LOG_ERROR("CodiesVR: VR menu currently requires an SDR RGBA8/BGRA8 swapchain");
        warned = true;
        return false;
    }

    const ImDrawData* draw = ImGui::GetDrawData();
    if (draw == nullptr || draw->DisplaySize.x <= 0 || draw->DisplaySize.y <= 0) return false;
    D3D12_RESOURCE_DESC overlayDesc = backbuffer;
    overlayDesc.Width = static_cast<UINT64>(draw->DisplaySize.x * draw->FramebufferScale.x);
    overlayDesc.Height = static_cast<UINT>(draw->DisplaySize.y * draw->FramebufferScale.y);
    if (overlayDesc.Width == 0 || overlayDesc.Height == 0) return false;

    if (!g_rtvHeap) {
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = kSlots;
        const HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_rtvHeap));
        if (FAILED(hr)) {
            LOG_ERROR("CodiesVR: overlay descriptor heap failed: {:X}", static_cast<unsigned long>(hr));
            return false;
        }
    }
    auto& texture = g_textures[slot];
    const auto old = texture ? texture->GetDesc() : D3D12_RESOURCE_DESC{};
    if (!texture || old.Width != overlayDesc.Width || old.Height != overlayDesc.Height ||
        old.Format != backbuffer.Format) {
        D3D12_RESOURCE_DESC desc = overlayDesc;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        desc.MipLevels = 1;
        desc.DepthOrArraySize = 1;
        desc.SampleDesc = {1, 0};
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_CLEAR_VALUE clear{};
        clear.Format = desc.Format;
        ComPtr<ID3D12Resource> replacement;
        const HRESULT hr = device->CreateCommittedResource(
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_SOURCE,
            &clear, IID_PPV_ARGS(&replacement));
        if (FAILED(hr)) {
            LOG_ERROR("CodiesVR: overlay texture failed: {:X}", static_cast<unsigned long>(hr));
            return false;
        }
        texture = replacement;
    }
    auto rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += slot * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    device->CreateRenderTargetView(texture.Get(), nullptr, rtv);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    list->ResourceBarrier(1, &barrier);
    const float clear[4]{};
    list->ClearRenderTargetView(rtv, clear, 0, nullptr);
    list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), list);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1, &barrier);
    return true;
}

void PublishOverlay(uint32_t slot) {
    std::lock_guard lock(g_mutex);
    g_published = g_textures.at(slot);
}

void PublishGuides(const codiesvr::opti::Guides& guides) {
    // Serialize registration against invocation; the callback must not reenter the API.
    std::lock_guard lock(g_mutex);
    if (g_guides) g_guides(&guides, g_user);
}
}  // namespace CodiesVrBridge
