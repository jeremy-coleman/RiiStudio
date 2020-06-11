#pragma once

#include <glm/glm.hpp>
#include <map>
#include <string>
#include <tuple>

#include "MegaState.hpp"
#include "PixelOcclusion.hpp"

// TODO: Interdependency
#include <frontend/editor/renderer/UBOBuilder.hpp>

namespace riistudio::lib3d {

struct Material;

struct IObserver {
  virtual ~IObserver() = default;
  // TODO: Detach
  virtual void update(Material* mat) {}
};
struct Polygon;
struct Material {
  virtual ~Material() = default;

  virtual std::string getName() const { return "Untitled Material"; }
  virtual void setName(const std::string& name) {}
  virtual s64 getId() const { return -1; }

  virtual bool isXluPass() const { return false; }

  virtual std::pair<std::string, std::string> generateShaders() const = 0;
  // TODO: Interdependency
  virtual void generateUniforms(DelegatedUBOBuilder& builder,
                                const glm::mat4& M, const glm::mat4& V,
                                const glm::mat4& P, u32 shaderId,
                                const std::map<std::string, u32>& texIdMap,
                                const Polygon& poly) const = 0;
  virtual void
  genSamplUniforms(u32 shaderId,
                   const std::map<std::string, u32>& texIdMap) const = 0;
  virtual void onSplice(DelegatedUBOBuilder& builder, const Polygon& poly,
                        u32 id) const {}
  virtual void setMegaState(MegaState& state) const = 0;
  virtual void configure(PixelOcclusion occlusion,
                         std::vector<std::string>& textures) = 0;

  // TODO: Better system..
  void notifyObservers() {
    for (auto* it : observers) {
      it->update(this);
    }
  }
  void onUpdate() {
    // (for shader recompilation)
    notifyObservers();
  }
  mutable std::vector<IObserver*> observers;
};

} // namespace riistudio::lib3d