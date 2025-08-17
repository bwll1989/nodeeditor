#pragma once

#include "Export.hpp"
#include "NodeData.hpp"
#include "NodeDelegateModel.hpp"
#include "QStringStdHash.hpp"

#include <QtCore/QString>

#include <functional>
#include <memory>
#include <set>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace QtNodes {

/// Class uses map for storing models (name, model)
class NODE_EDITOR_PUBLIC NodeDelegateModelRegistry
{
public:
    using RegistryItemPtr = std::unique_ptr<NodeDelegateModel>;
    using RegistryItemCreator = std::function<RegistryItemPtr()>;
    using RegisteredModelCreatorsMap = std::unordered_map<QString, RegistryItemCreator>;
    using RegisteredModelsCategoryMap = std::unordered_map<QString, QString>;
    using CategoriesSet = std::set<QString>;

    //using RegisteredTypeConvertersMap = std::map<TypeConverterId, TypeConverter>;

    NodeDelegateModelRegistry() = default;
    ~NodeDelegateModelRegistry() = default;

    NodeDelegateModelRegistry(NodeDelegateModelRegistry const &) = delete;
    NodeDelegateModelRegistry(NodeDelegateModelRegistry &&) = default;

    NodeDelegateModelRegistry &operator=(NodeDelegateModelRegistry const &) = delete;

    NodeDelegateModelRegistry &operator=(NodeDelegateModelRegistry &&) = default;

public:

    /**
     * @brief 注册模型类型，不会实例化对象
     * 
     * 这个方法使用模板参数中的类型名称，避免实例化对象
     * 
     * @tparam ModelType 要注册的模型类型
     * @param category 模型类别
     */
    template<typename ModelType>
    void registerModel(QString const& name = "Nodes",QString const& category = "Nodes")
    {
        static_assert(std::is_base_of<NodeDelegateModel, ModelType>::value,
                    "Model must inherit from NodeDelegateModel");

        QString const m_name = name;

        RegistryItemCreator creator = [](){ return std::make_unique<ModelType>(); };

        _registeredItemCreators[m_name] = std::move(creator);
        _registeredModelsCategory[m_name] = category;
        _categories.insert(category);
    }

#if 0
  template<typename ModelType>
  void
  registerModel(RegistryItemCreator creator,
                QString const&      category = "Nodes")
  {
    registerModel<ModelType>(std::move(creator), category);
  }


  template <typename ModelCreator>
  void
  registerModel(ModelCreator&& creator, QString const& category = "Nodes")
  {
    using ModelType = compute_model_type_t<decltype(creator())>;
    registerModel<ModelType>(std::forward<ModelCreator>(creator), category);
  }


  template <typename ModelCreator>
  void
  registerModel(QString const& category, ModelCreator&& creator)
  {
    registerModel(std::forward<ModelCreator>(creator), category);
  }


  void
  registerTypeConverter(TypeConverterId const& id,
                        TypeConverter          typeConverter)
  {
    _registeredTypeConverters[id] = std::move(typeConverter);
  }

#endif

    std::unique_ptr<NodeDelegateModel> create(QString const &modelName);

    RegisteredModelCreatorsMap const &registeredModelCreators() const;

    RegisteredModelsCategoryMap const &registeredModelsCategoryAssociation() const;

    CategoriesSet const &categories() const;

#if 0
  TypeConverter
  getTypeConverter(NodeDataType const& d1,
                   NodeDataType const& d2) const;
#endif

private:
    RegisteredModelsCategoryMap _registeredModelsCategory;

    CategoriesSet _categories;

    RegisteredModelCreatorsMap _registeredItemCreators;

#if 0
  RegisteredTypeConvertersMap _registeredTypeConverters;
#endif

private:
    // If the registered ModelType class has the static member method
    // `static QString Name();`, use it. Otherwise use the non-static
    // method: `virtual QString name() const;`
    template<typename T, typename = void>
    struct HasStaticMethodName : std::false_type
    {};

    template<typename T>
    struct HasStaticMethodName<
        T,
        typename std::enable_if<std::is_same<decltype(T::Name()), QString>::value>::type>
        : std::true_type
    {};

    template<typename ModelType>
    static QString computeName(std::true_type, RegistryItemCreator const &)
    {
        return ModelType::Name();
    }

    template<typename ModelType>
    static QString computeName(std::false_type, RegistryItemCreator const &creator)
    {
        return creator()->type();
    }

    template<typename T>
    struct UnwrapUniquePtr
    {
        // Assert always fires, but the compiler doesn't know this:
        static_assert(!std::is_same<T, T>::value,
                      "The ModelCreator must return a std::unique_ptr<T>, where T "
                      "inherits from NodeDelegateModel");
    };

    template<typename T>
    struct UnwrapUniquePtr<std::unique_ptr<T>>
    {
        static_assert(std::is_base_of<NodeDelegateModel, T>::value,
                      "The ModelCreator must return a std::unique_ptr<T>, where T "
                      "inherits from NodeDelegateModel");
        using type = T;
    };

    template<typename CreatorResult>
    using compute_model_type_t = typename UnwrapUniquePtr<CreatorResult>::type;



public:
    // 工厂函数类型定义
    using FactoryFunction = std::function<std::unique_ptr<NodeDelegateModel>()>;
    using FactoryFunctionMap = std::unordered_map<QString, FactoryFunction>;
    
    /**
     * @brief 注册工厂函数用于创建节点模型
     * 
     * 这个方法允许注册自定义的工厂函数来创建节点模型实例，
     * 提供比模板注册更灵活的创建方式
     * 
     * @param modelName 模型名称
     * @param factory 工厂函数，返回NodeDelegateModel的unique_ptr
     * @param category 模型类别，默认为"Nodes"
     */
    void registerFactory(QString const& modelName, 
                        FactoryFunction factory, 
                        QString const& category = "Nodes");
    
    /**
     * @brief 注册带参数的工厂函数
     * 
     * 允许注册需要特定参数的工厂函数
     * 
     * @tparam Args 工厂函数参数类型
     * @param modelName 模型名称
     * @param factory 工厂函数
     * @param category 模型类别
     * @param args 工厂函数参数
     */
    template<typename... Args>
    void registerFactoryWithArgs(QString const& modelName,
                                std::function<std::unique_ptr<NodeDelegateModel>(Args...)> factory,
                                QString const& category,
                                Args... args)
    {
        auto boundFactory = [factory, args...]() -> std::unique_ptr<NodeDelegateModel> {
            return factory(args...);
        };
        
        registerFactory(modelName, boundFactory, category);
    }
    
    /**
     * @brief 检查是否存在指定名称的工厂函数
     * 
     * @param modelName 模型名称
     * @return true 如果存在工厂函数
     * @return false 如果不存在工厂函数
     */
    bool hasFactory(QString const& modelName) const;
    
    /**
     * @brief 获取所有已注册的工厂函数
     * 
     * @return const FactoryFunctionMap& 工厂函数映射表的常量引用
     */
    FactoryFunctionMap const& registeredFactories() const;
    
    /**
     * @brief 移除指定的工厂函数
     * 
     * @param modelName 要移除的模型名称
     * @return true 如果成功移除
     * @return false 如果模型不存在
     */
    bool unregisterFactory(QString const& modelName);

private:
    // 工厂函数存储
    FactoryFunctionMap _registeredFactories;
    
    // If the registered ModelType class has the static member method
    // `static QString Name();`, use it. Otherwise use the non-static
    // method: `virtual QString name() const;`

};
} // namespace QtNodes

