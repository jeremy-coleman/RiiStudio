#pragma once

#include "../Node.hpp"
#include "../Reflection.hpp"

namespace kpi {

namespace detail {

struct ApplicationPluginsImpl {
  // Requires TypeIdResolvable<T>, DefaultConstructible<T>
  template <typename T>
  struct TFactory final : public ApplicationPlugins::IFactory {
    std::unique_ptr<IFactory> clone() const override {
      return std::make_unique<TFactory<T>>(*this);
    }
    std::unique_ptr<IObject> spawn() override {
      return std::unique_ptr<IObject> { static_cast<IObject*>(new T()) };
    }
    const char* getId() const override { return typeid(T).name(); }
  };
  //! Requires methods:
  //! - `T::canRead(const std::string& file, oishii::BinaryReader& reader)
  //! const`
  //! - `T::read(doc_node_t node, oishii::BinaryReader& reader) const`
  template <typename T>
  struct TBinaryDeserializer final : public IBinaryDeserializer, public T {
    std::unique_ptr<IBinaryDeserializer> clone() const override {
      return std::make_unique<TBinaryDeserializer<T>>(*this);
    }
    std::string canRead_(const std::string& file,
                         oishii::BinaryReader& reader) const override {
      return T::canRead(file, reader);
    }
    void read_(kpi::INode& node, oishii::BinaryReader& reader) const override {
      T::read(node, reader);
    }
  };
  //! Requires methods:
  //! - `T::canWrite(doc_node_t node) const`
  //! - `T::write(doc_node_t node, oishii::Writer& writer) const`
  template <typename T>
  struct TBinarySerializer final : public IBinarySerializer, public T {
    std::unique_ptr<IBinarySerializer> clone() const override {
      return std::make_unique<TBinarySerializer<T>>(*this);
    }
    bool canWrite_(kpi::INode& node) const override {
      return T::canWrite(node);
    }
    void write_(kpi::INode& node, oishii::Writer& writer) const override {
      T::write(node, writer);
    }
  };
  //! Requires: `::write(doc_node_t, oishii::Writer& writer, X*_=nullptr)`
  //! where `X` is some child that may be wrapped in a doc_node_t. No support
  //! for inheritance.
  template <typename T>
  struct TSimpleBinarySerializer final : public IBinarySerializer {
    std::unique_ptr<IBinarySerializer> clone() const override {
      return std::make_unique<TSimpleBinarySerializer<T>>(*this);
    }
    bool canWrite_(kpi::INode& node) const override {
      return dynamic_cast<T*>(&node) != nullptr;
    }
    void write_(kpi::INode& node, oishii::Writer& writer) const override {
      write(node, writer, static_cast<T*>(nullptr));
    }
  };
};

} // namespace detail

template <typename T> inline ApplicationPlugins& ApplicationPlugins::addType() {
  mFactories[typeid(T).name()] =
      std::make_unique<detail::ApplicationPluginsImpl::TFactory<T>>();
  return *this;
}

template <typename T>
inline ApplicationPlugins& ApplicationPlugins::addSerializer() {
  mWriters.push_back(
      std::make_unique<detail::ApplicationPluginsImpl::TBinarySerializer<T>>());
  return *this;
}

template <typename T>
inline ApplicationPlugins& ApplicationPlugins::addSimpleSerializer() {
  mWriters.push_back(
      std::make_unique<
          detail::ApplicationPluginsImpl::TSimpleBinarySerializer<T>>());
  return *this;
}

template <typename T>
inline ApplicationPlugins& ApplicationPlugins::addDeserializer() {
  mReaders.push_back(std::make_unique<
                     detail::ApplicationPluginsImpl::TBinaryDeserializer<T>>());
  return *this;
}

} // namespace kpi
