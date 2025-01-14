#include "J3dIo.hpp"
#include "Scene.hpp"
#include <core/kpi/Plugins.hpp>

namespace riistudio::j3d {

class BMD {
public:
  std::string canRead(const std::string& file,
                      oishii::BinaryReader& reader) const {
    return file.ends_with("bmd") || file.ends_with("bdl")
               ? typeid(Collection).name()
               : "";
  }
  bool canWrite(kpi::INode& node) const {
    return dynamic_cast<Collection*>(&node) != nullptr;
  }

  void write(kpi::INode& node, oishii::Writer& writer) const {
    assert(dynamic_cast<Collection*>(&node) != nullptr);
    Collection& collection = *dynamic_cast<Collection*>(&node);

    WriteBMD(collection, writer);
  }

  void read(kpi::IOTransaction& transaction) const {
    auto& node = transaction.node;
    auto& data = transaction.data;

    assert(dynamic_cast<Collection*>(&node) != nullptr);
    auto& collection = *dynamic_cast<Collection*>(&node);
    auto& mdl = collection.getModels().add();

    oishii::BinaryReader reader(std::move(data));

    ReadBMD(mdl, collection, reader, transaction);
  }
};

void InstallJ3d(kpi::ApplicationPlugins& installer) {
  installer.addType<j3d::Model, lib3d::Model>();
  installer.addType<j3d::Collection, lib3d::Scene>();

  installer.addType<j3d::Texture, libcube::Texture>();
  installer.addType<j3d::Material, libcube::IGCMaterial>();
  installer.addType<j3d::Shape, libcube::IndexedPolygon>();
  installer.addType<j3d::Joint, libcube::IBoneDelegate>();

  installer.addDeserializer<BMD>();
  installer.addSerializer<BMD>();
}

} // namespace riistudio::j3d
