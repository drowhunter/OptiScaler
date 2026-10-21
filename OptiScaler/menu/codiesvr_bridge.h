#pragma once

#include "codiesvr_bridge_api.h"

namespace CodiesVrBridge {
int TakeMenuRequest();  // -1 none, 0 close, 1 open; render thread only
void RequestMenuClose();  // -1 none, 0 close, 1 open; render thread only
bool Active();
void UpdateMenuState(bool visible, uint32_t shortcutKey);
bool DrawOverlay(ID3D12Device* device, ID3D12CommandQueue* queue,
                 ID3D12GraphicsCommandList* list, const D3D12_RESOURCE_DESC& backbuffer,
                 uint32_t slot);
void PublishOverlay(uint32_t slot);
void ClearOverlay();
void ResetOverlay();
void PublishGuides(const codiesvr::opti::Guides& guides);
}  // namespace CodiesVrBridge
