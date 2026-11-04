#include "nobk_backdrop_gpu.h"
#include "nobk_backdrop_shader.h"

#include <rex/logging.h>
#include <rex/ui/d3d12/d3d12_util.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>

namespace {
using Microsoft::WRL::ComPtr;
struct Settings {float strength=0,begin=0,end=1,radius=0;};
std::mutex settings_mutex;
Settings settings;

void Transition(ID3D12GraphicsCommandList* commands, ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
  D3D12_RESOURCE_BARRIER barrier{};
  barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};
  commands->ResourceBarrier(1,&barrier);
}

struct Images {
  ComPtr<ID3D12Resource> scene,horizontal;
  ComPtr<ID3D12DescriptorHeap> views,targets;
  uint32_t width=0,height=0;
  uint64_t last_submission=0;
};
struct Backdrop {
  ComPtr<ID3D12RootSignature> root;
  ComPtr<ID3D12PipelineState> horizontal_pipeline,composite_pipeline;
  std::unique_ptr<Images> images;
  std::deque<std::unique_ptr<Images>> retired;
  bool attempted=false,failed=false;

  bool Initialize(const rex::ui::d3d12::D3D12Provider& provider) {
    if(attempted)return !failed;
    attempted=true;failed=true;
    // Compile once, on first opening. Load only the Windows system compiler;
    // no new redistributable DLL or shader file is searched beside the game.
    HMODULE compiler=LoadLibraryExW(L"d3dcompiler_47.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!compiler)return false;
    struct Library {HMODULE value;~Library(){FreeLibrary(value);}} library{compiler};
    const auto compile=reinterpret_cast<pD3DCompile>(GetProcAddress(compiler,"D3DCompile"));
    if(!compile)return false;
    ComPtr<ID3DBlob> vertex,pixel,errors;
    if(FAILED(compile(motogp::nobk::backdrop::shader,std::strlen(motogp::nobk::backdrop::shader),
        "F1 backdrop",nullptr,nullptr,"Vertex","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vertex,&errors)))return false;
    errors.Reset();
    if(FAILED(compile(motogp::nobk::backdrop::shader,std::strlen(motogp::nobk::backdrop::shader),
        "F1 backdrop",nullptr,nullptr,"Pixel","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pixel,&errors)))return false;

    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[0].DescriptorTable={1,&range};
    parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[1].Constants={0,0,8};
    parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxAnisotropy=1;
    sampler.MaxLOD=D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{2,parameters,1,&sampler,D3D12_ROOT_SIGNATURE_FLAG_NONE};
    root.Attach(rex::ui::d3d12::util::CreateRootSignature(provider,signature));
    if(!root)return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature=root.Get();
    pipeline.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};
    pipeline.PS={pixel->GetBufferPointer(),pixel->GetBufferSize()};
    pipeline.SampleMask=UINT_MAX;
    pipeline.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    pipeline.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pipeline.RasterizerState.DepthClipEnable=TRUE;
    pipeline.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    auto& stencil=pipeline.DepthStencilState.FrontFace;
    stencil.StencilFailOp=stencil.StencilDepthFailOp=stencil.StencilPassOp=D3D12_STENCIL_OP_KEEP;
    stencil.StencilFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    pipeline.DepthStencilState.BackFace=stencil;
    pipeline.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pipeline.NumRenderTargets=1;
    pipeline.RTVFormats[0]=DXGI_FORMAT_B8G8R8A8_UNORM;
    pipeline.SampleDesc.Count=1;
    auto& blend=pipeline.BlendState.RenderTarget[0];
    blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;
    blend.BlendOp=blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_ZERO;
    blend.LogicOp=D3D12_LOGIC_OP_NOOP;
    blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
    auto* device=provider.GetDevice();
    if(FAILED(device->CreateGraphicsPipelineState(&pipeline,IID_PPV_ARGS(&horizontal_pipeline))))return false;
    blend.BlendEnable=TRUE;
    blend.SrcBlend=D3D12_BLEND_SRC_ALPHA;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
    blend.SrcBlendAlpha=D3D12_BLEND_ZERO;blend.DestBlendAlpha=D3D12_BLEND_ONE;
    if(FAILED(device->CreateGraphicsPipelineState(&pipeline,IID_PPV_ARGS(&composite_pipeline))))return false;
    failed=false;
    return true;
  }

  bool Resize(ID3D12Device* device,const D3D12_RESOURCE_DESC& buffer) {
    const auto width=uint32_t(buffer.Width),height=buffer.Height;
    if(images&&images->width==width&&images->height==height)return true;
    auto next=std::make_unique<Images>();next->width=width;next->height=height;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    auto description=buffer;description.Flags=D3D12_RESOURCE_FLAG_NONE;
    description.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&next->scene))))return false;
    description.Width=std::max(1u,(width+1)/2);description.Height=std::max(1u,(height+1)/2);
    description.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&next->horizontal))))return false;
    D3D12_DESCRIPTOR_HEAP_DESC views{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,0};
    D3D12_DESCRIPTOR_HEAP_DESC targets{D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,D3D12_DESCRIPTOR_HEAP_FLAG_NONE,0};
    if(FAILED(device->CreateDescriptorHeap(&views,IID_PPV_ARGS(&next->views)))||
        FAILED(device->CreateDescriptorHeap(&targets,IID_PPV_ARGS(&next->targets))))return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format=buffer.Format;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    view.Texture2D.MipLevels=1;
    auto handle=next->views->GetCPUDescriptorHandleForHeapStart();
    device->CreateShaderResourceView(next->scene.Get(),&view,handle);
    handle.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    device->CreateShaderResourceView(next->horizontal.Get(),&view,handle);
    device->CreateRenderTargetView(next->horizontal.Get(),nullptr,next->targets->GetCPUDescriptorHandleForHeapStart());
    // Descriptor heaps and images may still be referenced by an earlier paint.
    // Retire whole allocations together, without a GPU wait on the UI thread.
    if(images)retired.push_back(std::move(images));
    images=std::move(next);
    return true;
  }
};
}

extern "C" __declspec(dllexport) uint32_t Motogp_SetPanelBackdrop(uint32_t version,
    float strength,float begin,float end,float radius) {
  if(version!=1||!std::isfinite(strength)||!std::isfinite(begin)||!std::isfinite(end)||!std::isfinite(radius))return 0;
  std::lock_guard lock(settings_mutex);
  settings={std::clamp(strength,0.f,1.f),std::clamp(begin,0.f,1.f),
      std::clamp(end,0.f,1.f),std::clamp(radius,0.f,.02f)};
  if(settings.end<=settings.begin)settings.strength=0;
  return 1;
}

void Motogp_DrawPanelBackdrop(void*& opaque,const rex::ui::d3d12::D3D12Provider& provider,
    ID3D12GraphicsCommandList* commands,ID3D12Resource* back_buffer,
    D3D12_CPU_DESCRIPTOR_HANDLE target,uint64_t submission,uint64_t completed) {
  Settings value;{std::lock_guard lock(settings_mutex);value=settings;}
  auto* state=static_cast<Backdrop*>(opaque);
  if(state)while(!state->retired.empty()&&state->retired.front()->last_submission<=completed)state->retired.pop_front();
  if(value.strength<=0)return;
  if(!state){state=new Backdrop;opaque=state;}
  const bool attempted=state->attempted;
  if(!state->Initialize(provider)){
    if(!attempted)REXLOG_WARN("F1 backdrop blur unavailable; retaining the soft contrast scrim");
    return;
  }
  const auto description=back_buffer->GetDesc();
  if(description.Format!=DXGI_FORMAT_B8G8R8A8_UNORM||description.SampleDesc.Count!=1)return;
  if(!state->Resize(provider.GetDevice(),description)){
    if(!state->failed)REXLOG_WARN("F1 backdrop image allocation failed; retaining the soft contrast scrim");
    state->failed=true;return;
  }
  auto& images=*state->images;images.last_submission=submission;
  Transition(commands,back_buffer,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
  Transition(commands,images.scene.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);
  commands->CopyResource(images.scene.Get(),back_buffer);
  Transition(commands,back_buffer,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  Transition(commands,images.scene.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  Transition(commands,images.horizontal.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
  auto* heap=images.views.Get();commands->SetDescriptorHeaps(1,&heap);
  commands->SetGraphicsRootSignature(state->root.Get());
  commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  const auto source=images.views->GetGPUDescriptorHandleForHeapStart();
  const auto destination=images.targets->GetCPUDescriptorHandleForHeapStart();
  const uint32_t half_width=(images.width+1)/2,half_height=(images.height+1)/2;
  D3D12_VIEWPORT viewport{0,0,float(half_width),float(half_height),0,1};
  D3D12_RECT scissor{0,0,LONG(half_width),LONG(half_height)};
  commands->RSSetViewports(1,&viewport);commands->RSSetScissorRects(1,&scissor);
  commands->OMSetRenderTargets(1,&destination,TRUE,nullptr);
  commands->SetPipelineState(state->horizontal_pipeline.Get());
  commands->SetGraphicsRootDescriptorTable(0,source);
  // Five bilinear samples implement a normalized nine-tap Gaussian kernel.
  // The intermediate is half resolution; the source remains on the GPU.
  float constants[8]{1.f/half_width,1.f/half_height,
      value.radius*images.height/images.width,0,value.begin,value.end,value.strength,0};
  commands->SetGraphicsRoot32BitConstants(1,8,constants,0);commands->DrawInstanced(3,1,0,0);
  Transition(commands,images.horizontal.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  viewport.Width=float(images.width);viewport.Height=float(images.height);
  scissor.right=LONG(std::ceil(images.width*value.end));scissor.bottom=LONG(images.height);
  commands->RSSetViewports(1,&viewport);commands->RSSetScissorRects(1,&scissor);
  commands->OMSetRenderTargets(1,&target,TRUE,nullptr);
  commands->SetPipelineState(state->composite_pipeline.Get());
  auto intermediate=source;intermediate.ptr+=provider.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  commands->SetGraphicsRootDescriptorTable(0,intermediate);
  constants[0]=1.f/images.width;constants[1]=1.f/images.height;
  constants[2]=0;constants[3]=value.radius;constants[7]=1;
  commands->SetGraphicsRoot32BitConstants(1,8,constants,0);commands->DrawInstanced(3,1,0,0);
  // The SDK immediate drawer rebinds root signature, heaps, pipeline, vertex
  // buffers, viewport and scissor before drawing any sharp UI foreground.
}

void Motogp_DestroyPanelBackdrop(void*& state) {
  // Presenter destruction has already waited for all paint submissions.
  delete static_cast<Backdrop*>(state);state=nullptr;
}
