#include "DataFlowGraphModel.hpp"
#include "ConnectionIdHash.hpp"
#include "GroupIdHash.hpp"
#include <QJsonArray>

#include <stdexcept>

namespace QtNodes {

DataFlowGraphModel::DataFlowGraphModel(std::shared_ptr<NodeDelegateModelRegistry> registry)
    : _registry(std::move(registry))
    , _nextNodeId{0}
{}

std::unordered_set<NodeId> DataFlowGraphModel::allNodeIds() const
{
    std::unordered_set<NodeId> nodeIds;
    for_each(_models.begin(), _models.end(), [&nodeIds](auto const &p) { nodeIds.insert(p.first); });

    return nodeIds;
}

std::unordered_set<ConnectionId> DataFlowGraphModel::allConnectionIds(NodeId const nodeId) const
{
    std::unordered_set<ConnectionId> result;

    std::copy_if(_connectivity.begin(),
                 _connectivity.end(),
                 std::inserter(result, std::end(result)),
                 [&nodeId](ConnectionId const &cid) {
                     return cid.inNodeId == nodeId || cid.outNodeId == nodeId;
                 });

    return result;
}

std::unordered_set<GroupId> DataFlowGraphModel::allGroupIds() const
{
    return _groups;
}

std::unordered_set<ConnectionId> DataFlowGraphModel::connections(NodeId nodeId,
                                                                 PortType portType,
                                                                 PortIndex portIndex) const
{
    std::unordered_set<ConnectionId> result;

    std::copy_if(_connectivity.begin(),
                 _connectivity.end(),
                 std::inserter(result, std::end(result)),
                 [&portType, &portIndex, &nodeId](ConnectionId const &cid) {
                     return (getNodeId(portType, cid) == nodeId
                             && getPortIndex(portType, cid) == portIndex);
                 });

    return result;
}

bool DataFlowGraphModel::connectionExists(ConnectionId const connectionId) const
{
    return (_connectivity.find(connectionId) != _connectivity.end());
}

NodeId DataFlowGraphModel::addNode(QString const nodeType)
{
    std::unique_ptr<NodeDelegateModel> model = _registry->create(nodeType);

    if (model) {
        NodeId newId = newNodeId();
        model->setNodeID(newId);
        connect(model.get(),
                &NodeDelegateModel::dataUpdated,
                [newId, this](PortIndex const portIndex) {
                    onOutPortDataUpdated(newId, portIndex);
                });

        connect(model.get(),
                &NodeDelegateModel::portsAboutToBeDeleted,
                this,
                [newId, this](PortType const portType, PortIndex const first, PortIndex const last) {
                    portsAboutToBeDeleted(newId, portType, first, last);
                });

        connect(model.get(),
                &NodeDelegateModel::portsDeleted,
                this,
                &DataFlowGraphModel::portsDeleted);

        connect(model.get(),
                &NodeDelegateModel::portsAboutToBeInserted,
                this,
                [newId, this](PortType const portType, PortIndex const first, PortIndex const last) {
                    portsAboutToBeInserted(newId, portType, first, last);
                });

        connect(model.get(),
                &NodeDelegateModel::portsInserted,
                this,
                &DataFlowGraphModel::portsInserted);

        _models[newId] = std::move(model);

        Q_EMIT nodeCreated(newId);

        return newId;
    }

    return InvalidNodeId;
}

bool DataFlowGraphModel::connectionPossible(ConnectionId const connectionId) const
{
    auto getDataType = [&](PortType const portType) {
        return portData(getNodeId(portType, connectionId),
                        portType,
                        getPortIndex(portType, connectionId),
                        PortRole::DataType)
            .value<NodeDataType>();
    };

    auto portVacant = [&](PortType const portType) {
        NodeId const nodeId = getNodeId(portType, connectionId);
        PortIndex const portIndex = getPortIndex(portType, connectionId);
        auto const connected = connections(nodeId, portType, portIndex);

        auto policy = portData(nodeId, portType, portIndex, PortRole::ConnectionPolicyRole)
                          .value<ConnectionPolicy>();

        return connected.empty() || (policy == ConnectionPolicy::Many);
    };

    return getDataType(PortType::Out).id == getDataType(PortType::In).id
           && portVacant(PortType::Out) && portVacant(PortType::In);
}

void DataFlowGraphModel::addConnection(ConnectionId const connectionId)
{
    _connectivity.insert(connectionId);

    sendConnectionCreation(connectionId);

    QVariant const portDataToPropagate = portData(connectionId.outNodeId,
                                                  PortType::Out,
                                                  connectionId.outPortIndex,
                                                  PortRole::Data);

    setPortData(connectionId.inNodeId,
                PortType::In,
                connectionId.inPortIndex,
                portDataToPropagate,
                PortRole::Data);
}

void DataFlowGraphModel::addGroup(GroupId const groupId)
{
//    // 如果组节点数量小�?，不创建�?//    if (groupId.nodeIds.size() ==0) {
        //qDebug() << "Group not added: Less than 2 nodes";
//        return;
//    }
    
    // 检查是否已经存在完全相同的�?   
    for (const auto& existingGroup : _groups) {
        // 首先比较节点数量是否相同
        if (existingGroup.nodeIds.size() == groupId.nodeIds.size()) {
            // 检查是否包含完全相同的节点（忽略顺序）
            std::vector<NodeId> sortedNewNodes = groupId.nodeIds;
            std::vector<NodeId> sortedExistingNodes = existingGroup.nodeIds;
            std::sort(sortedNewNodes.begin(), sortedNewNodes.end());
            std::sort(sortedExistingNodes.begin(), sortedExistingNodes.end());
            
            if (sortedNewNodes == sortedExistingNodes) {
                qDebug() << "Group not added: Identical group already exists";
                return;
            }
        }
    }
    
    // 检查组重叠
    for (const auto& existingGroup : _groups) {
        // 计算节点交集
        std::vector<NodeId> intersection;
        for (const auto& nodeId : groupId.nodeIds) {
            if (std::find(existingGroup.nodeIds.begin(), existingGroup.nodeIds.end(), nodeId) != existingGroup.nodeIds.end()) {
                intersection.push_back(nodeId);
            }
        }
        
        // 如果有任何重叠节点，不添加新�?        
        if (!intersection.empty()) {
            qDebug() << "Group not added: Node overlap detected";
            return;
        }
    }

    // 添加新组
//    qDebug() << "Adding group with" << groupId.nodeIds.size() << "nodes";
    _groups.insert(groupId);
    Q_EMIT groupCreated(groupId);
}

void DataFlowGraphModel::sendConnectionCreation(ConnectionId const connectionId)
{
    Q_EMIT connectionCreated(connectionId);

    auto iti = _models.find(connectionId.inNodeId);
    auto ito = _models.find(connectionId.outNodeId);
    if (iti != _models.end() && ito != _models.end()) {
        auto &modeli = iti->second;
        auto &modelo = ito->second;
        modeli->inputConnectionCreated(connectionId);
        modelo->outputConnectionCreated(connectionId);
    }
}

void DataFlowGraphModel::sendConnectionDeletion(ConnectionId const connectionId)
{
    Q_EMIT connectionDeleted(connectionId);

    auto iti = _models.find(connectionId.inNodeId);
    auto ito = _models.find(connectionId.outNodeId);
    if (iti != _models.end() && ito != _models.end()) {
        auto &modeli = iti->second;
        auto &modelo = ito->second;
        modeli->inputConnectionDeleted(connectionId);
        modelo->outputConnectionDeleted(connectionId);
    }
}

bool DataFlowGraphModel::nodeExists(NodeId const nodeId) const
{
    return (_models.find(nodeId) != _models.end());
}

QVariant DataFlowGraphModel::nodeData(NodeId nodeId, NodeRole role) const
{
    QVariant result;

    auto it = _models.find(nodeId);
    if (it == _models.end())
        return result;

    auto &model = it->second;

    switch (role) {
    case NodeRole::Type:
        result = model->type();
        break;

    case NodeRole::Position:
        result = _nodeGeometryData[nodeId].pos;
        break;

    case NodeRole::Size:
        result = _nodeGeometryData[nodeId].size;
        break;

    case NodeRole::CaptionVisible:
        result = model->widgetEmbeddable() ? model->captionVisible() : true;
        break;

    case NodeRole::Caption:
        result = model->caption();
        break;

    case NodeRole::Style: {
        auto style = StyleCollection::nodeStyle();
        result = style.toJson().toVariantMap();
    } break;

    case NodeRole::InternalData: {
        QJsonObject nodeJson;

        nodeJson["internal-data"] = _models.at(nodeId)->save();

        result = nodeJson.toVariantMap();
        break;
    }

    case NodeRole::InPortCount:
        result = model->nPorts(PortType::In);
        break;

    case NodeRole::OutPortCount:
        result = model->nPorts(PortType::Out);
        break;

    case NodeRole::WidgetEmbeddable:
        result = model->widgetEmbeddable();
        break;

    case NodeRole::Widget: {
        // switch (model->getWidgetType()) {
        //     case NodeWidgetType::InternalWidget: {
                auto w = model->embeddedWidget();
                result = QVariant::fromValue(w);
        //     }
        //         break;
        //     case  NodeWidgetType::PortEditWidget: {
        //         auto w = model->PortEditWidget;
        //         result = QVariant::fromValue(w);
        //     }
        //         break;
        //     default:
        //         break;
        // }

    } break;
    case NodeRole::NodeID:
        result = model->getNodeID();
        break;
    case NodeRole::Remarks:
        result = model->getRemarks();
        break;
    case NodeRole::PortEditable:
        result = model->PortEditable;
        break;
    case NodeRole::EmbeddWidgetType:
        result=static_cast<int>(model->getWidgetType());
        break;
    default:
        break;
    }

    return result;
}

NodeFlags DataFlowGraphModel::nodeFlags(NodeId nodeId) const
{
    auto it = _models.find(nodeId);

    if (it != _models.end() && it->second->widgetEmbeddable() && it->second->resizable())
        return NodeFlag::Resizable;

    return NodeFlag::NoFlags;
}

bool DataFlowGraphModel::setNodeData(NodeId nodeId, NodeRole role, QVariant value)
{
    Q_UNUSED(nodeId);
    Q_UNUSED(role);
    Q_UNUSED(value);

    bool result = false;

    switch (role) {
    case NodeRole::Type:
        break;
    case NodeRole::Position: {
        _nodeGeometryData[nodeId].pos = value.value<QPointF>();

        Q_EMIT nodePositionUpdated(nodeId);

        result = true;
    } break;

    case NodeRole::Size: {
        _nodeGeometryData[nodeId].size = value.value<QSize>();
        result = true;
    } break;

    case NodeRole::CaptionVisible:
        break;

    case NodeRole::Caption:
        break;

    case NodeRole::Style:
        break;

    case NodeRole::InternalData:
        break;

    case NodeRole::InPortCount: {
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->InPortCount = value.toInt();
        model->embeddedWidgetSizeUpdated();
        Q_EMIT nodeUpdated(nodeId);
    }
        break;

    case NodeRole::OutPortCount:{
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->OutPortCount = value.toInt();
        model->embeddedWidgetSizeUpdated();
        Q_EMIT nodeUpdated(nodeId);
    }
        break;
    case NodeRole::EmbeddWidgetType: {
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->setEmbeddWidgetType(static_cast<decltype(model->getWidgetType())>(value.toInt())); // 显式类型转换
        Q_EMIT nodeWidgetUpdated(nodeId);
        result=true;
    }
        break;
    case NodeRole::WidgetEmbeddable: {
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->WidgetEmbeddable=value.toBool();
        model->embeddedWidgetSizeUpdated();
        Q_EMIT nodeUpdated(nodeId);
        result = true;
    }
        break;
    case NodeRole::Widget:
        break;
    case NodeRole::NodeID:{ 
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->setNodeID(value.toInt());
        result = true;
        }
        break;
    case NodeRole::Remarks:{ 
        auto it = _models.find(nodeId);
        auto &model = it->second;
        model->setRemarks(value.toString());
        Q_EMIT nodeUpdated(nodeId);
        result = true;
    }
        break;
    default:
        break;
    }

    return result;
}

QVariant DataFlowGraphModel::portData(NodeId nodeId,
                                      PortType portType,
                                      PortIndex portIndex,
                                      PortRole role) const
{
    QVariant result;

    auto it = _models.find(nodeId);
    if (it == _models.end())
        return result;

    auto &model = it->second;

    switch (role) {
    case PortRole::Data:
        if (portType == PortType::Out)
            result = QVariant::fromValue(model->outData(portIndex));
        break;

    case PortRole::DataType:
        result = QVariant::fromValue(model->dataType(portType, portIndex));
        break;

    case PortRole::ConnectionPolicyRole:
        result = QVariant::fromValue(model->portConnectionPolicy(portType, portIndex));
        break;

    case PortRole::CaptionVisible:
        result = model->portCaptionVisible(portType, portIndex);
        break;

    case PortRole::Caption:
        result = model->portCaption(portType, portIndex);

        break;

    default:
        break;
    }

    return result;
}

bool DataFlowGraphModel::setPortData(
    NodeId nodeId, PortType portType, PortIndex portIndex, QVariant const &value, PortRole role)
{
    Q_UNUSED(nodeId);

    QVariant result;

    auto it = _models.find(nodeId);
    if (it == _models.end())
        return false;

    auto &model = it->second;

    switch (role) {
    case PortRole::Data:
        if (portType == PortType::In) {
            model->setInData(value.value<std::shared_ptr<NodeData>>(), portIndex);

            // Triggers repainting on the scene.
            Q_EMIT inPortDataWasSet(nodeId, portType, portIndex);
        }
        break;

    default:
        break;
    }

    return false;
}

bool DataFlowGraphModel::deleteConnection(ConnectionId const connectionId)
{
    bool disconnected = false;

    auto it = _connectivity.find(connectionId);

    if (it != _connectivity.end()) {
        disconnected = true;

        _connectivity.erase(it);
    }

    if (disconnected) {
        sendConnectionDeletion(connectionId);

        propagateEmptyDataTo(getNodeId(PortType::In, connectionId),
                             getPortIndex(PortType::In, connectionId));
    }

    return disconnected;
}

bool DataFlowGraphModel::deleteNode(NodeId const nodeId)
{
    // Delete connections to this node first.
    auto connectionIds = allConnectionIds(nodeId);
    for (auto &cId : connectionIds) {
        deleteConnection(cId);
    }

    _nodeGeometryData.erase(nodeId);
    _models.erase(nodeId);

    Q_EMIT nodeDeleted(nodeId);

    return true;
}
void DataFlowGraphModel::updateGroup(const GroupId oldGroupId, const GroupId newGroupId)
{
    // 删除旧组
    if (auto it = _groups.find(oldGroupId);
        it != _groups.end()) {
        _groups.erase(it);
        // 插入新组(包含更新后的节点列表)
        _groups.insert(newGroupId);
        Q_EMIT groupUpdated(oldGroupId);
    }

}
bool DataFlowGraphModel::deleteGroup(GroupId const groupId)
{

    auto it = _groups.find(groupId);
    if (it!= _groups.end()) {
        _groups.erase(it);
        Q_EMIT groupDeleted(groupId);
        return true;
    }
    return false;
}

QJsonObject DataFlowGraphModel::saveNode(NodeId const nodeId) const
{
    QJsonObject nodeJson;

    nodeJson["id"] = static_cast<qint64>(nodeId);

    nodeJson["internal-data"] = _models.at(nodeId)->save();

    nodeJson["type"] = _models.at(nodeId)->type();
    // 保存备注
    nodeJson["remarks"] = _models.at(nodeId)->getRemarks();

    nodeJson["input-count"] = nodeData(nodeId, NodeRole::InPortCount).toInt();
    nodeJson["output-count"] = nodeData(nodeId, NodeRole::OutPortCount).toInt();
    nodeJson["port-editable"] = nodeData(nodeId, NodeRole::PortEditable).toBool();
    
    {
        QPointF const pos = nodeData(nodeId, NodeRole::Position).value<QPointF>();

        QJsonObject posJson;
        posJson["x"] = pos.x();
        posJson["y"] = pos.y();
        nodeJson["position"] = posJson;
    }

    return nodeJson;
}

QJsonObject DataFlowGraphModel::save() const
{
    QJsonObject sceneJson;

    QJsonArray nodesJsonArray;
    for (auto const nodeId : allNodeIds()) {
        nodesJsonArray.append(saveNode(nodeId));
    }
    sceneJson["nodes"] = nodesJsonArray;

    QJsonArray connJsonArray;
    for (auto const &cid : _connectivity) {
        connJsonArray.append(toJson(cid));
    }
    sceneJson["connections"] = connJsonArray;
    QJsonArray groupJsonArray;
    for (auto const &gid : _groups) {
        groupJsonArray.append(groupToJson(gid));
    }
    sceneJson["groups"] = groupJsonArray;
    return sceneJson;
}

void DataFlowGraphModel::loadNode(QJsonObject const &nodeJson)
{
    // Possibility of the id clash when reading it from json and not generating a
    // new value.
    // 1. When restoring a scene from a file.
    // Conflict is not possible because the scene must be cleared by the time of
    // loading.
    // 2. When undoing the deletion command.  Conflict is not possible
    // because all the new ids were created past the removed nodes.
    NodeId restoredNodeId = nodeJson["id"].toInt();

    _nextNodeId = std::max(_nextNodeId, restoredNodeId + 1);

    QJsonObject const internalDataJson = nodeJson["internal-data"].toObject();

    QString delegateModelName =nodeJson["type"].toString();
    // 加载备注
    std::unique_ptr<NodeDelegateModel> model = _registry->create(delegateModelName);

    if (model) {
        connect(model.get(),
                &NodeDelegateModel::dataUpdated,
                [restoredNodeId, this](PortIndex const portIndex) {
                    onOutPortDataUpdated(restoredNodeId, portIndex);
                });
        model->setNodeID(restoredNodeId);
        _models[restoredNodeId] = std::move(model);

        Q_EMIT nodeCreated(restoredNodeId);

        QJsonObject posJson = nodeJson["position"].toObject();
        QPointF const pos(posJson["x"].toDouble(), posJson["y"].toDouble());

        setNodeData(restoredNodeId, NodeRole::Position, pos);
        setNodeData(restoredNodeId, NodeRole::Remarks, nodeJson["remarks"].toString());
        setNodeData(restoredNodeId, NodeRole::PortEditable, nodeJson["port-editable"].toBool());
        setNodeData(restoredNodeId, NodeRole::InPortCount, nodeJson["input-count"].toInt());
        setNodeData(restoredNodeId, NodeRole::OutPortCount, nodeJson["output-count"].toInt());
        _models[restoredNodeId]->load(internalDataJson);
        // 加载备注
       

    } else {
        throw std::logic_error(std::string("No registered model with name ")
                               + delegateModelName.toLocal8Bit().data());
    }
}

void DataFlowGraphModel::load(QJsonObject const &jsonDocument)
{
    // 先加载所有节�?   
    QJsonArray nodesJsonArray = jsonDocument["nodes"].toArray();
    for (QJsonValueRef nodeJson : nodesJsonArray) {
        loadNode(nodeJson.toObject());
    }

    // 再加载所有连�?    
    QJsonArray connectionJsonArray = jsonDocument["connections"].toArray();
    for (QJsonValueRef connection : connectionJsonArray) {
        QJsonObject connJson = connection.toObject();
        ConnectionId connId = fromJson(connJson);
        // 恢复连接
        addConnection(connId);
    }

    // 加载组之前，先收集所有组信息
    std::vector<GroupId> allGroups;
    QJsonArray groupJsonArray = jsonDocument["groups"].toArray();
    for (QJsonValueRef group : groupJsonArray) {
        QJsonObject groupJson = group.toObject();
        GroupId groupId = QtNodes::fromJsonToGroup(groupJson);
        allGroups.push_back(groupId);
    }

    // 对组进行排序，优先处理节点数量更多的�?    // 这样可以确保先添�?�?组，减少删除操作
    std::sort(allGroups.begin(), allGroups.end(), 
        [](const GroupId& a, const GroupId& b) {
            return a.nodeIds.size() > b.nodeIds.size(); // 降序排列
        });

    // 依次添加排序后的�?   
    for (const auto& groupId : allGroups) {
        addGroup(groupId);
    }
}

void DataFlowGraphModel::onOutPortDataUpdated(NodeId const nodeId, PortIndex const portIndex)
{
    std::unordered_set<ConnectionId> const &connected = connections(nodeId,
                                                                    PortType::Out,
                                                                    portIndex);

    QVariant const portDataToPropagate = portData(nodeId, PortType::Out, portIndex, PortRole::Data);

    for (auto const &cn : connected) {
        setPortData(cn.inNodeId, PortType::In, cn.inPortIndex, portDataToPropagate, PortRole::Data);
    }
}

void DataFlowGraphModel::propagateEmptyDataTo(NodeId const nodeId, PortIndex const portIndex)
{
    QVariant emptyData{};

    setPortData(nodeId, PortType::In, portIndex, emptyData, PortRole::Data);
}

} // namespace QtNodes
