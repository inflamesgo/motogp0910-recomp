#pragma once

#include <cstdint>
#include <rex/ui/d3d12/d3d12_provider.h>

// Called only by the patched D3D12 presenter, before any host UI is drawn.
void Motogp_DrawPanelBackdrop(void*& state, const rex::ui::d3d12::D3D12Provider& provider,
    ID3D12GraphicsCommandList* commands, ID3D12Resource* back_buffer,
    D3D12_CPU_DESCRIPTOR_HANDLE target, uint64_t submission, uint64_t completed);
void Motogp_DestroyPanelBackdrop(void*& state);
