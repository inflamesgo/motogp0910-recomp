#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>

namespace rex::ui {class ImGuiDialog;class ImGuiDrawer;class ImmediateDrawer;}
namespace motogp::gauge {
namespace state {
inline std::atomic<int> selected_style{0};
inline std::atomic<unsigned> available_styles{0};
inline std::atomic<bool> tyre_panel_available{false};
}
// Native option callbacks publish only these atomics. Texture creation,
// preferences and all host rendering stay on the UI thread.
inline int SelectedStyle(){return state::selected_style.load(std::memory_order_acquire);}
inline constexpr int kStyleCount=5;
inline const char* StyleKey(int value){return value==1?"motogp17":value==2?"motogp06":value==3?"motogpurt2":value==4?"motogpurt3":"original";}
inline void SelectStyle(int value){if(value>=0&&value<kStyleCount)state::selected_style.store(value,std::memory_order_release);}
inline bool Available(){return state::available_styles.load(std::memory_order_acquire)!=0;}
inline bool StyleAvailable(int value){return value>0&&value<kStyleCount&&(state::available_styles.load(std::memory_order_acquire)&(1u<<value));}
inline bool SelectedAvailable(){return StyleAvailable(SelectedStyle());}
inline bool TyrePanelAvailable(){return state::tyre_panel_available.load(std::memory_order_acquire);}
inline void SetTyrePanelAvailable(bool value){state::tyre_panel_available.store(value,std::memory_order_release);}
inline void SetStyleAvailable(int value,bool available){
  if(value<=0||value>=kStyleCount)return;
  if(available)state::available_styles.fetch_or(1u<<value,std::memory_order_release);
  else state::available_styles.fetch_and(~(1u<<value),std::memory_order_release);
}
// Compatibility for the original MotoGP 17 fixture; false resets all styles.
inline void SetAvailable(bool value){if(value)SetStyleAvailable(1,true);else{state::available_styles.store(0,std::memory_order_release);SetTyrePanelAvailable(false);}}

std::unique_ptr<rex::ui::ImGuiDialog> CreateOverlay(rex::ui::ImGuiDrawer* drawer,
    rex::ui::ImmediateDrawer* immediate,const std::filesystem::path& assets,
    const std::filesystem::path& user_root,std::function<bool()> blocked);
}
