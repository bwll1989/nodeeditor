#include "NodeDelegateModel.hpp"

#include "StyleCollection.hpp"

namespace QtNodes {

NodeDelegateModel::NodeDelegateModel()
    : _nodeStyle(StyleCollection::nodeStyle())
{
    // Derived classes can initialize specific style here
}

QJsonObject NodeDelegateModel::save() const
{
    QJsonObject modelJson;

    modelJson["model-name"] = name();

    return modelJson;
}

void NodeDelegateModel::load(QJsonObject const &)
{
    //
}

ConnectionPolicy NodeDelegateModel::portConnectionPolicy(PortType portType, PortIndex) const
{
    auto result = ConnectionPolicy::One;
    switch (portType) {
    case PortType::In:
        result = ConnectionPolicy::One;
        break;
    case PortType::Out:
        result = ConnectionPolicy::Many;
        break;
    case PortType::None:
        break;
    }

    return result;
}

NodeStyle const &NodeDelegateModel::nodeStyle() const
{
    return _nodeStyle;
}

void NodeDelegateModel::setNodeStyle(NodeStyle const &style)
{
    _nodeStyle = style;
}


unsigned int NodeDelegateModel::nPorts(PortType portType) const
{
    switch (portType) {
        case PortType::In:
            return InPortCount;
        case PortType::Out:
            return OutPortCount;
        default:
            return 0;
        break;
    }
}

void NodeDelegateModel::registerOSCControl(const QString& oscAddress, QWidget* control)
{
    // 如果oscAddress不以"/"开头，则不注册
    if (!oscAddress.startsWith("/")) return;
    // 构建完整的OSC地址，自动给OSC地址添加前缀，包括节点ID
    if (!control) return;
    // 如果已存在相同地址的映射，先移除旧的
    auto it = _OscMapping.find(oscAddress);
    if (it != _OscMapping.end()) {
        _OscMapping.erase(it);
    }
    
    // 添加新的映射
    _OscMapping[oscAddress] = control;
}

void NodeDelegateModel::unregisterOSCControl(const QString& oscAddress)
{
    if (!oscAddress.startsWith("/")) return;
    auto it = _OscMapping.find(oscAddress);
    if (it != _OscMapping.end()) {
        _OscMapping.erase(it);
    }
}
/**
 * 获取OSC地址对应的控件
 */
QWidget* NodeDelegateModel::getWidgetFromOSCAddress(const QString& oscAddress) const
{
    auto it = _OscMapping.find(oscAddress);
    return it != _OscMapping.end() ? it->second : nullptr;
}
/**
 * 获取OSC地址和控件的映射
 */
std::unordered_map<QString, QWidget*> NodeDelegateModel::getOscMapping() const
{
    return _OscMapping;
}

} // namespace QtNodes