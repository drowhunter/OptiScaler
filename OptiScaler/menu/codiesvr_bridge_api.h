#ifndef CODIESVR_OPTISCALER_API_V1_H
#define CODIESVR_OPTISCALER_API_V1_H

#include <d3d12.h>
#include <cstdint>

// Shared verbatim with the patched OptiScaler build. No ImGui or STL types
// cross the DLL boundary. Modified integration: 2026-10-19, GPL-3.0.
namespace codiesvr::opti {

constexpr uint32_t kVersion = 1;
constexpr char kExport[] = "OptiScaler_CodiesVR_GetApiV1";
constexpr char kMenuExport[] = "OptiScaler_CodiesVR_RequestMenuV1";
// Optional extension to V1. Queues visibility on OptiScaler's render thread.
using RequestMenu = HRESULT(__cdecl*)(uint32_t visible);

#pragma pack(push, 8)
struct OverlayFrame {
    uint32_t size = sizeof(OverlayFrame);
    uint32_t visible = 0;
    uint32_t shortcutKey = 0;
    uint32_t reserved = 0;
    // AddRef'd on S_OK; caller releases after its GPU copy has completed.
    ID3D12Resource* texture = nullptr;
};

struct Guides {
    uint32_t size = sizeof(Guides);
    uint32_t initFlags = 0;  // XeSS flags, not NGX flags
    uint64_t context = 0;  // identity only; never dereferenced by the consumer
    ID3D12GraphicsCommandList* commandList = nullptr;
    ID3D12Resource* color = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* motion = nullptr;
    uint32_t renderWidth = 0;
    uint32_t renderHeight = 0;
    uint32_t outputWidth = 0;
    uint32_t outputHeight = 0;
    uint32_t colorX = 0;
    uint32_t colorY = 0;
    uint32_t depthX = 0;
    uint32_t depthY = 0;
    uint32_t motionX = 0;
    uint32_t motionY = 0;
    uint32_t outputX = 0;
    uint32_t outputY = 0;
    float motionScaleX = 0;
    float motionScaleY = 0;
    float jitterX = 0;
    float jitterY = 0;
    uint32_t reset = 0;
    uint32_t reserved = 0;
};

// Resources are borrowed for this call only, before upscaler evaluation.
// The consumer must preserve resource states and record work on commandList.
using GuideCallback = void(__cdecl*)(const Guides*, void*);

struct Api {
    uint32_t size = sizeof(Api);
    uint32_t version = kVersion;
    // S_FALSE means no fresh menu texture; visibility/key are still returned.
    // A null queue disconnects VR routing and restores the desktop fallback.
    HRESULT(__cdecl* getOverlay)(ID3D12CommandQueue*, OverlayFrame*) = nullptr;
    void(__cdecl* setGuideCallback)(GuideCallback, void*) = nullptr;
};

using GetApi = HRESULT(__cdecl*)(uint32_t, uint32_t, Api*);
#pragma pack(pop)

static_assert(sizeof(OverlayFrame) == 24);
static_assert(sizeof(Guides) == 120);
static_assert(sizeof(Api) == 24);

}  // namespace codiesvr::opti

#endif
