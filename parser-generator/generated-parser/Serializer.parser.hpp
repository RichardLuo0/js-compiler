#pragma once

#include <fstream>
#include <list>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace GeneratedParser::Serializer {
using BinaryOType = char;
using BinaryIType = signed char;

class BinaryOfstream : public std::ofstream {
 public:
  explicit BinaryOfstream(const std::string& filename)
      : std::ofstream(filename, std::ios::binary) {}
};

class BinaryIfstream {
 public:
  [[nodiscard]] virtual BinaryIType peek() const = 0;
  virtual BinaryIType get() = 0;

  virtual const BinaryIType* current() = 0;
};

class ArrayStream : public BinaryIfstream {
 protected:
  const BinaryIType* data;

 public:
  explicit ArrayStream(const BinaryIType* data) : data(data) {}

  [[nodiscard]] BinaryIType peek() const override { return *data; }

  BinaryIType get() override { return *(data++); }

  const BinaryIType* current() override { return data; }
};

class ISerializer {
 public:
  static constexpr inline int EOS = -2;    // End of segment
  static constexpr inline int SPLIT = -3;  // Split different units

  virtual ~ISerializer() = default;

  virtual void serialize(BinaryOfstream&) const {};
  virtual void deserialize(BinaryIfstream&){};
};

template <typename ObjectType>
class Serializer : public ISerializer {
 public:
  Serializer() = delete;
};

template <>
class Serializer<size_t> : public ISerializer {
 protected:
  size_t& object;

 public:
  explicit Serializer(size_t& object) : object(object) {}
  explicit Serializer(const size_t& object)
      : Serializer(const_cast<size_t&>(object)) {}

  void serialize(BinaryOfstream& os) const override {
    os.put(static_cast<BinaryOType>(object));
    os.put(EOS);
  }

  void deserialize(BinaryIfstream& stream) override {
    object = static_cast<unsigned char>(stream.get());
    if (stream.get() != EOS) throw std::runtime_error("Incomplete size_t");
  }
};

template <class StringType>
class StringSerializer : public ISerializer {
 protected:
  StringType& object;

 public:
  explicit StringSerializer(StringType& object) : object(object) {}
  explicit StringSerializer(const StringType& object)
      : StringSerializer(const_cast<StringType&>(object)) {}

  void serialize(BinaryOfstream& os) const override {
    os << object.data();
    os.put(EOS);
  }

  void deserialize(BinaryIfstream& stream) override {
    const char* begin = reinterpret_cast<const char*>(stream.current());
    size_t count = 0;
    while (stream.peek() != EOS) {
      stream.get();
      count++;
    }
    stream.get();
    object = {begin, count};
  }
};

template <>
class Serializer<std::string> : public StringSerializer<std::string> {
 public:
  explicit Serializer(std::string& object) : StringSerializer(object) {}
  explicit Serializer(const std::string& object) : StringSerializer(object) {}
};

template <>
class Serializer<std::string_view> : public StringSerializer<std::string_view> {
 public:
  explicit Serializer(std::string_view& object) : StringSerializer(object) {}
  explicit Serializer(const std::string_view& object)
      : StringSerializer(object) {}
};

template <typename Key, typename Value, class... Args>
class Serializer<std::unordered_map<Key, Value, Args...>> : public ISerializer {
 protected:
  std::unordered_map<Key, Value, Args...>& object;

 public:
  explicit Serializer(std::unordered_map<Key, Value, Args...>& object)
      : object(object) {}
  explicit Serializer(const std::unordered_map<Key, Value, Args...>& object)
      : object(const_cast<std::unordered_map<Key, Value, Args...>&>(object)) {}

  void serialize(BinaryOfstream& os) const override {
    for (auto& [key, value] : object) {
      Serializer<Key>(key).serialize(os);
      Serializer<Value>(value).serialize(os);
    }
    os.put(EOS);
  }

  void deserialize(BinaryIfstream& stream) override {
    while (stream.peek() != EOS) {
      Key key;
      Serializer<Key>(key).deserialize(stream);
      Value value;
      Serializer<Value>(value).deserialize(stream);
      object.emplace(key, value);
    }
    stream.get();
  }
};

template <typename ItemType>
class Serializer<std::list<ItemType>> : public ISerializer {
 protected:
  std::list<ItemType>& object;

 public:
  explicit Serializer(std::list<ItemType>& object) : object(object) {}
  explicit Serializer(const std::list<ItemType>& object)
      : Serializer(const_cast<std::list<ItemType>&>(object)) {}

  void serialize(BinaryOfstream& os) const override {
    os.put(object.size());
    for (auto& item : object) {
      Serializer<ItemType>(item).serialize(os);
    }
    os.put(EOS);
  }

  void deserialize(BinaryIfstream& stream) override {
    stream.get();  // Eat size, we don't need it in std::list
    while (stream.peek() != EOS) {
      ItemType item;
      Serializer<ItemType>(item).deserialize(stream);
      object.push_back(item);
    }
    stream.get();
  }
};

template <typename ItemType>
class Serializer<std::vector<ItemType>> : public ISerializer {
 protected:
  std::vector<ItemType>& object;

 public:
  explicit Serializer(std::vector<ItemType>& object) : object(object) {}
  explicit Serializer(const std::vector<ItemType>& object)
      : Serializer(const_cast<std::vector<ItemType>&>(object)) {}

  void serialize(BinaryOfstream& os) const override {
    os.put(object.size());
    for (auto& item : object) {
      Serializer<ItemType>(item).serialize(os);
    }
    os.put(EOS);
  }

  void deserialize(BinaryIfstream& stream) override {
    object.resize(stream.get());
    size_t i = 0;
    while (stream.peek() != EOS) {
      ItemType item;
      Serializer<ItemType>(item).deserialize(stream);
      object[i++] = item;
    }
    stream.get();
  }
};

class BinarySerializer {
 protected:
  std::list<std::unique_ptr<ISerializer>> serializerList;

 public:
  template <typename ObjectType>
  void add(ObjectType& object) {
    serializerList.push_back(std::make_unique<Serializer<ObjectType>>(object));
  }
  template <typename ObjectType>
  void add(const ObjectType& object) {
    serializerList.push_back(std::make_unique<Serializer<ObjectType>>(object));
  }

  void serialize(BinaryOfstream& os) {
    for (const auto& serializer : serializerList) {
      serializer->serialize(os);
    }
  }
};

class BinaryDeserializer {
 protected:
  std::unique_ptr<BinaryIfstream> stream;

 public:
  explicit BinaryDeserializer(std::unique_ptr<BinaryIfstream>&& stream)
      : stream(std::move(stream)) {}

  template <class StreamType, class... Args>
  requires std::is_base_of<BinaryIfstream,
                           StreamType>::value static BinaryDeserializer
  create(Args&&... args) {
    return BinaryDeserializer(
        std::make_unique<StreamType>(std::forward<Args>(args)...));
  }

  template <typename ObjectType>
  void deserialize(ObjectType& object) {
    Serializer<ObjectType>(object).deserialize(*stream);
  }
};
}  // namespace GeneratedParser::Serializer
