#include "NodeDelegateModelRegistry.hpp"

#include <QtCore/QFile>
#include <QtWidgets/QMessageBox>

using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeDelegateModelRegistry;

std::unique_ptr<NodeDelegateModel> NodeDelegateModelRegistry::create(QString const &modelName)
{
    // 首先检查工厂函数
    auto factoryIt = _registeredFactories.find(modelName);
    if (factoryIt != _registeredFactories.end()) {
        return factoryIt->second();
    }
    
    // 然后检查传统的创建器
    auto it = _registeredItemCreators.find(modelName);
    if (it != _registeredItemCreators.end()) {
        return it->second();
    }

    return nullptr;
}

void NodeDelegateModelRegistry::registerFactory(QString const& modelName, 
                                               FactoryFunction factory, 
                                               QString const& category)
{
    _registeredFactories[modelName] = std::move(factory);
    _registeredModelsCategory[modelName] = category;
    _categories.insert(category);
}

bool NodeDelegateModelRegistry::hasFactory(QString const& modelName) const
{
    return _registeredFactories.find(modelName) != _registeredFactories.end();
}

NodeDelegateModelRegistry::FactoryFunctionMap const& 
NodeDelegateModelRegistry::registeredFactories() const
{
    return _registeredFactories;
}

bool NodeDelegateModelRegistry::unregisterFactory(QString const& modelName)
{
    auto it = _registeredFactories.find(modelName);
    if (it != _registeredFactories.end()) {
        _registeredFactories.erase(it);
        
        // 同时移除类别信息（如果没有其他模型使用该类别）
        auto categoryIt = _registeredModelsCategory.find(modelName);
        if (categoryIt != _registeredModelsCategory.end()) {
            QString category = categoryIt->second;
            _registeredModelsCategory.erase(categoryIt);
            
            // 检查是否还有其他模型使用这个类别
            bool categoryStillUsed = false;
            for (const auto& pair : _registeredModelsCategory) {
                if (pair.second == category) {
                    categoryStillUsed = true;
                    break;
                }
            }
            
            if (!categoryStillUsed) {
                _categories.erase(category);
            }
        }
        
        return true;
    }
    return false;
}

NodeDelegateModelRegistry::RegisteredModelCreatorsMap const &
NodeDelegateModelRegistry::registeredModelCreators() const
{
    return _registeredItemCreators;
}

NodeDelegateModelRegistry::RegisteredModelsCategoryMap const &
NodeDelegateModelRegistry::registeredModelsCategoryAssociation() const
{
    return _registeredModelsCategory;
}

NodeDelegateModelRegistry::CategoriesSet const &NodeDelegateModelRegistry::categories() const
{
    return _categories;
}
