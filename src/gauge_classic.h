#pragma once

#include <filesystem>
#include <memory>

struct ImDrawList;
struct ImVec2;
namespace rex::ui {class ImmediateDrawer;}
namespace motogp::gauge {
struct Telemetry;
class ClassicRenderer {
 public:
  virtual ~ClassicRenderer()=default;
  virtual void Draw(ImDrawList* draw,const Telemetry& sample,ImVec2 viewport) const=0;
};
std::unique_ptr<ClassicRenderer> LoadClassicRenderer(rex::ui::ImmediateDrawer* drawer,
    const std::filesystem::path& directory,const char* style);
}
