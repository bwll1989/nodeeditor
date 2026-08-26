#include "UndoCommands.hpp"

#include <algorithm>
#include <typeinfo>
#include <utility>

#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "GroupIdUtils.hpp"
#include "Definitions.hpp"
#include "NodeGraphicsObject.hpp"

#include <QMap>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGraphicsObject>

#include <typeinfo>

namespace QtNodes {

static QJsonObject connectionToJson(AbstractGraphModel &graphModel, ConnectionId const &cid)
{
    QJsonObject connJson = toJson(cid);
    if (graphModel.connectionData(cid, ConnectionRole::Virtual).toBool()) {
        connJson[QStringLiteral("virtual")] = true;
        QString const label = graphModel.connectionData(cid, ConnectionRole::VirtualLabel).toString();
        if (!label.isEmpty())
            connJson[QStringLiteral("virtualLabel")] = label;
    }
    return connJson;
}

static void restoreConnectionDisplay(AbstractGraphModel &graphModel,
                                     ConnectionId const &cid,
                                     QJsonObject const &connJson)
{
    if (!connJson.value(QStringLiteral("virtual")).toBool())
        return;
    graphModel.setConnectionData(cid, ConnectionRole::Virtual, true);
    QString const label = connJson.value(QStringLiteral("virtualLabel")).toString();
    if (!label.isEmpty())
        graphModel.setConnectionData(cid, ConnectionRole::VirtualLabel, label);
}

static QJsonObject serializeSelectedItems(BasicGraphicsScene *scene)
{
    QJsonObject serializedScene;

    auto &graphModel = scene->graphModel();

    std::unordered_set<NodeId> selectedNodes;
    std::vector<GroupId> selectedGroups;

    for (QGraphicsItem *item : scene->selectedItems()) {
        if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            selectedNodes.insert(n->nodeId());
        } else if (auto g = qgraphicsitem_cast<GroupGraphicsObject *>(item)) {
            selectedGroups.push_back(g->groupId());
        }
    }

    bool const hasSelectedGroups = !selectedGroups.empty();

    if (hasSelectedGroups) {
        for (auto const &gid : selectedGroups) {
            for (auto const nodeId : gid.nodeIds) {
                selectedNodes.insert(nodeId);
            }
        }
    }

    QJsonArray nodesJsonArray;
    for (auto const nodeId : selectedNodes) {
        nodesJsonArray.append(graphModel.saveNode(nodeId));
    }

    QJsonArray connJsonArray;
    if (hasSelectedGroups) {
        std::unordered_set<ConnectionId> selectedConnections;

        for (auto const nodeId : selectedNodes) {
            for (auto const &cid : graphModel.allConnectionIds(nodeId)) {
                if (selectedNodes.count(cid.outNodeId) > 0 && selectedNodes.count(cid.inNodeId) > 0) {
                    selectedConnections.insert(cid);
                }
            }
        }

        for (auto const &cid : selectedConnections) {
            connJsonArray.append(connectionToJson(graphModel, cid));
        }
    } else {
        for (QGraphicsItem *item : scene->selectedItems()) {
            if (auto c = qgraphicsitem_cast<ConnectionGraphicsObject *>(item)) {
                auto const &cid = c->connectionId();

                if (selectedNodes.count(cid.outNodeId) > 0 && selectedNodes.count(cid.inNodeId) > 0) {
                    connJsonArray.append(connectionToJson(graphModel, cid));
                }
            }
        }
    }

    QJsonArray groupsJsonArray;
    for (auto const &gid : selectedGroups) {
        groupsJsonArray.append(groupToJson(gid));
    }

    serializedScene["nodes"] = nodesJsonArray;
    serializedScene["connections"] = connJsonArray;
    serializedScene["groups"] = groupsJsonArray;
    return serializedScene;
}

static bool findOverlappingGroup(AbstractGraphModel &graphModel,
                                 GroupId const &target,
                                 GroupId &found)
{
    for (auto const &gid : graphModel.allGroupIds()) {
        for (auto const nid : target.nodeIds) {
            if (std::find(gid.nodeIds.begin(), gid.nodeIds.end(), nid) != gid.nodeIds.end()) {
                found = gid;
                return true;
            }
        }
    }
    return false;
}

static void insertSerializedItems(QJsonObject const &json, BasicGraphicsScene *scene)
{
    AbstractGraphModel &graphModel = scene->graphModel();

    QJsonArray const &nodesJsonArray = json["nodes"].toArray();

    for (QJsonValue node : nodesJsonArray) {
        QJsonObject obj = node.toObject();

        graphModel.loadNode(obj);

        auto id = obj["id"].toInt();
        scene->nodeGraphicsObject(id)->setZValue(1.0);
        scene->nodeGraphicsObject(id)->setSelected(true);
    }

    QJsonArray const &groupsJsonArray = json["groups"].toArray();
    for (QJsonValue group : groupsJsonArray) {
        QJsonObject groupJson = group.toObject();
        GroupId const targetGroup = fromJsonToGroup(groupJson);

        GroupId existing;
        if (findOverlappingGroup(graphModel, targetGroup, existing)) {
            if (auto *ggo = scene->groupGraphicsObject(existing)) {
                ggo->applyGroupId(targetGroup);
            }
            graphModel.updateGroup(existing, targetGroup);
        } else {
            graphModel.addGroup(targetGroup);
        }
    }

    QJsonArray const &connJsonArray = json["connections"].toArray();

    for (QJsonValue connection : connJsonArray) {
        QJsonObject connJson = connection.toObject();

        ConnectionId connId = fromJson(connJson);

        // Restore the connection
        graphModel.addConnection(connId);
        restoreConnectionDisplay(graphModel, connId, connJson);

        if (auto* cgo = scene->connectionGraphicsObject(connId)) {
            cgo->setSelected(true);
        }
    }
}

static void deleteSerializedItems(QJsonObject &sceneJson,
                                  AbstractGraphModel &graphModel,
                                  bool deleteRestoredGroups = false)
{
    QJsonArray groupsToDeleteJsonArray = sceneJson.value(QStringLiteral("deleteGroups")).toArray();
    for (QJsonValueRef group : groupsToDeleteJsonArray) {
        QJsonObject groupJson = group.toObject();
        graphModel.deleteGroup(fromJsonToGroup(groupJson));
    }

    if (deleteRestoredGroups) {
        QJsonArray restoredGroupsJsonArray = sceneJson.value(QStringLiteral("groups")).toArray();
        for (QJsonValueRef group : restoredGroupsJsonArray) {
            QJsonObject groupJson = group.toObject();
            graphModel.deleteGroup(fromJsonToGroup(groupJson));
        }
    }
    QJsonArray connectionJsonArray = sceneJson["connections"].toArray();

    for (QJsonValueRef connection : connectionJsonArray) {
        QJsonObject connJson = connection.toObject();

        ConnectionId connId = fromJson(connJson);

        graphModel.deleteConnection(connId);
    }

    QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    for (QJsonValueRef node : nodesJsonArray) {
        QJsonObject nodeJson = node.toObject();
        graphModel.deleteNode(nodeJson["id"].toInt());
    }


}

static QPointF computeAverageNodePosition(QJsonObject const &sceneJson)
{
    QPointF averagePos(0, 0);

    QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    for (QJsonValueRef node : nodesJsonArray) {
        QJsonObject nodeJson = node.toObject();

        averagePos += QPointF(nodeJson["position"].toObject()["x"].toDouble(),
                              nodeJson["position"].toObject()["y"].toDouble());
    }

    averagePos /= static_cast<double>(nodesJsonArray.size());

    return averagePos;
}

//-------------------------------------

CreateCommand::CreateCommand(BasicGraphicsScene *scene,
                             QString const name,
                             QPointF const &mouseScenePos)
    : _scene(scene)
    , _sceneJson(QJsonObject())
{
    setText(QStringLiteral("创建节点"));
    _nodeId = _scene->graphModel().addNode(name);
    if (_nodeId != InvalidNodeId) {
        _scene->graphModel().setNodeData(_nodeId, NodeRole::Position, mouseScenePos);
    } else {
        setObsolete(true);
    }
}

void CreateCommand::undo()
{
    QJsonArray nodesJsonArray;
    nodesJsonArray.append(_scene->graphModel().saveNode(_nodeId));
    _sceneJson["nodes"] = nodesJsonArray;

    _scene->graphModel().deleteNode(_nodeId);
}

void CreateCommand::redo()
{
    if (_sceneJson.empty() || _sceneJson["nodes"].toArray().empty())
        return;

    insertSerializedItems(_sceneJson, _scene);
}

//-------------------------------------

DeleteCommand::DeleteCommand(BasicGraphicsScene *scene)
    : _scene(scene)
{
    setText(QStringLiteral("删除"));

    auto &graphModel = _scene->graphModel();

    std::unordered_set<NodeId> selectedNodes;
    std::vector<GroupId> selectedGroups;
    std::unordered_set<ConnectionId> selectedConnections;

    for (QGraphicsItem *item : _scene->selectedItems()) {
        if (auto c = qgraphicsitem_cast<ConnectionGraphicsObject *>(item)) {
            selectedConnections.insert(c->connectionId());
        } else if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            selectedNodes.insert(n->nodeId());
        } else if (auto g = qgraphicsitem_cast<GroupGraphicsObject *>(item)) {
            selectedGroups.push_back(g->groupId());
        }
    }

    if (!selectedGroups.empty()) {
        for (auto const &gid : selectedGroups) {
            for (auto const nodeId : gid.nodeIds) {
                selectedNodes.insert(nodeId);
            }
        }
    }

    for (auto const nodeId : selectedNodes) {
        auto const conns = graphModel.allConnectionIds(nodeId);
        selectedConnections.insert(conns.begin(), conns.end());
    }

    QJsonArray connJsonArray;
    for (auto const &cid : selectedConnections) {
        connJsonArray.append(connectionToJson(graphModel, cid));
    }

    QJsonArray groupsForUndoJsonArray;
    if (!selectedNodes.empty()) {
        auto const allGroups = graphModel.allGroupIds();
        for (auto const &gid : allGroups) {
            bool affected = false;
            for (auto const nodeId : gid.nodeIds) {
                if (selectedNodes.count(nodeId) > 0) {
                    affected = true;
                    break;
                }
            }
            if (affected)
                groupsForUndoJsonArray.append(groupToJson(gid));
        }
    }

    QJsonArray groupsToDeleteJsonArray;
    for (auto const &gid : selectedGroups) {
        groupsToDeleteJsonArray.append(groupToJson(gid));
    }

    QJsonArray nodesJsonArray;
    for (auto const nodeId : selectedNodes) {
        nodesJsonArray.append(graphModel.saveNode(nodeId));
    }

    // If nothing is deleted, cancel this operation
    if (connJsonArray.isEmpty() && nodesJsonArray.isEmpty() && groupsToDeleteJsonArray.isEmpty())
    {
        setObsolete(true);
        return;
    }

    _sceneJson["nodes"] = nodesJsonArray;
    _sceneJson["connections"] = connJsonArray;
    _sceneJson["groups"] = groupsForUndoJsonArray;
    _sceneJson["deleteGroups"] = groupsToDeleteJsonArray;

}

void DeleteCommand::undo()
{
    insertSerializedItems(_sceneJson, _scene);
}

void DeleteCommand::redo()
{

    deleteSerializedItems(_sceneJson, _scene->graphModel());
}

//-------------------------------------

void offsetNodeGroup(QJsonObject &sceneJson, QPointF const &diff)
{
    QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();

    QJsonArray newNodesJsonArray;
    for (QJsonValueRef node : nodesJsonArray) {
        QJsonObject obj = node.toObject();

        QPointF oldPos(obj["position"].toObject()["x"].toDouble(),
                       obj["position"].toObject()["y"].toDouble());

        oldPos += diff;

        QJsonObject posJson;
        posJson["x"] = oldPos.x();
        posJson["y"] = oldPos.y();
        obj["position"] = posJson;

        newNodesJsonArray.append(obj);
    }

    sceneJson["nodes"] = newNodesJsonArray;
}

//-------------------------------------

CopyCommand::CopyCommand(BasicGraphicsScene *scene)
{
    QJsonObject sceneJson = serializeSelectedItems(scene);

    if (sceneJson.empty() || sceneJson["nodes"].toArray().empty()) {
        setObsolete(true);
        return;
    }

    QClipboard *clipboard = QApplication::clipboard();

    QByteArray const data = QJsonDocument(sceneJson).toJson();

    QMimeData *mimeData = new QMimeData();
    mimeData->setData("application/qt-nodes-graph", data);
    mimeData->setText(data);

    clipboard->setMimeData(mimeData);

    // Copy command does not have any effective redo/undo operations.
    // It copies the data to the clipboard and could be immediately removed
    // from the stack.
    setObsolete(true);
}

//-------------------------------------

PasteCommand::PasteCommand(BasicGraphicsScene *scene, QPointF const &mouseScenePos)
    : _scene(scene)
    , _mouseScenePos(mouseScenePos)
{
    setText(QStringLiteral("粘贴"));
    _newSceneJson = takeSceneJsonFromClipboard();

    if (_newSceneJson.empty() || _newSceneJson["nodes"].toArray().empty()) {
        setObsolete(true);
        return;
    }

    _newSceneJson = makeNewNodeIdsInScene(_newSceneJson);

    QPointF averagePos = computeAverageNodePosition(_newSceneJson);

    offsetNodeGroup(_newSceneJson, _mouseScenePos - averagePos);
}

void PasteCommand::undo()
{
    deleteSerializedItems(_newSceneJson, _scene->graphModel(), true);
}

void PasteCommand::redo()
{
    _scene->clearSelection();

    // Ignore if pasted in content does not generate nodes.
    try {
        insertSerializedItems(_newSceneJson, _scene);
    } catch (...) {
        // If the paste does not work, delete all selected nodes and connections
        // `deleteNode(...)` implicitly removed connections
        auto &graphModel = _scene->graphModel();

        QJsonArray nodesJsonArray;
        for (QGraphicsItem *item : _scene->selectedItems()) {
            if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
                graphModel.deleteNode(n->nodeId());
            }
        }

        setObsolete(true);
    }
}

QJsonObject PasteCommand::takeSceneJsonFromClipboard()
{
    QClipboard const *clipboard = QApplication::clipboard();
    QMimeData const *mimeData = clipboard->mimeData();

    QJsonDocument json;
    if (mimeData->hasFormat("application/qt-nodes-graph")) {
        json = QJsonDocument::fromJson(mimeData->data("application/qt-nodes-graph"));
    } else if (mimeData->hasText()) {
        json = QJsonDocument::fromJson(mimeData->text().toUtf8());
    }

    return json.object();
}

QJsonObject PasteCommand::makeNewNodeIdsInScene(QJsonObject const &sceneJson)
{
    AbstractGraphModel &graphModel = _scene->graphModel();
    std::unordered_map<NodeId, NodeId> mapNodeIds;

    // 1. 处理节点
    QJsonArray nodesJsonArray = sceneJson["nodes"].toArray();
    QJsonArray newNodesJsonArray;
    for (QJsonValueRef node : nodesJsonArray) {
        QJsonObject nodeJson = node.toObject();
        NodeId oldNodeId = nodeJson["id"].toInt();
        NodeId newNodeId = graphModel.newNodeId();
        mapNodeIds[oldNodeId] = newNodeId;
        
        // 替换节点ID
        nodeJson["id"] = static_cast<qint64>(newNodeId);
        newNodesJsonArray.append(nodeJson);
    }

    // 2. 处理连接
    QJsonArray connectionJsonArray = sceneJson["connections"].toArray();
    QJsonArray newConnJsonArray;
    for (QJsonValueRef connection : connectionJsonArray) {
        QJsonObject connJson = connection.toObject();
        ConnectionId connId = fromJson(connJson);
        
        // 使用新的节点ID创建连接
        ConnectionId newConnId{
            mapNodeIds[connId.outNodeId],
            connId.outPortIndex,
            mapNodeIds[connId.inNodeId],
            connId.inPortIndex
        };
        QJsonObject newConnJson = toJson(newConnId);
        if (connJson.value(QStringLiteral("virtual")).toBool()) {
            newConnJson[QStringLiteral("virtual")] = true;
            QString const label = connJson.value(QStringLiteral("virtualLabel")).toString();
            if (!label.isEmpty())
                newConnJson[QStringLiteral("virtualLabel")] = label;
        }
        newConnJsonArray.append(newConnJson);
    }

    // 3. 处理组
    QJsonArray groupsJsonArray = sceneJson["groups"].toArray();
    QJsonArray newGroupsJsonArray;
    for (QJsonValueRef group : groupsJsonArray) {
        QJsonObject groupJson = group.toObject();
        GroupId oldGroupId = fromJsonToGroup(groupJson);

        GroupId newGroupId = oldGroupId;
        newGroupId.nodeIds.clear();

        for (const NodeId& oldNodeId : oldGroupId.nodeIds) {
            auto it = mapNodeIds.find(oldNodeId);
            if (it != mapNodeIds.end()) {
                newGroupId.nodeIds.push_back(it->second);
            }
        }

        // 只有当组内有节点时才添加组
        if (!newGroupId.nodeIds.empty()) {
            newGroupsJsonArray.append(groupToJson(newGroupId));
        }
    }

    // 4. 创建新的场景JSON
    QJsonObject newSceneJson;
    newSceneJson["nodes"] = newNodesJsonArray;
    newSceneJson["connections"] = newConnJsonArray;
    newSceneJson["groups"] = newGroupsJsonArray;

    return newSceneJson;
}

//-------------------------------------

DisconnectCommand::DisconnectCommand(BasicGraphicsScene *scene, ConnectionId const connId)
    : _scene(scene)
    , _connId(connId)
{
    setText(QStringLiteral("断开连线"));
    auto &model = _scene->graphModel();
    _wasVirtual = model.connectionData(connId, ConnectionRole::Virtual).toBool();
    _virtualLabel = model.connectionData(connId, ConnectionRole::VirtualLabel).toString();
}

void DisconnectCommand::undo()
{
    auto &model = _scene->graphModel();
    model.addConnection(_connId);
    if (_wasVirtual) {
        model.setConnectionData(_connId, ConnectionRole::Virtual, true);
        if (!_virtualLabel.isEmpty())
            model.setConnectionData(_connId, ConnectionRole::VirtualLabel, _virtualLabel);
    }
}

void DisconnectCommand::redo()
{
    _scene->graphModel().deleteConnection(_connId);
}

//------

ConnectCommand::ConnectCommand(BasicGraphicsScene *scene, ConnectionId const connId)
    : _scene(scene)
    , _connId(connId)
{
    setText(QStringLiteral("连接"));
}

void ConnectCommand::undo()
{
    _scene->graphModel().deleteConnection(_connId);
}

void ConnectCommand::redo()
{
    _scene->graphModel().addConnection(_connId);
}

//------

MoveNodeCommand::MoveNodeCommand(BasicGraphicsScene *scene, QPointF const &diff)
    : _scene(scene)
    , _diff(diff)
{
    setText(QStringLiteral("移动节点"));
    _selectedNodes.clear();
    for (QGraphicsItem *item : _scene->selectedItems()) {
        if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            _selectedNodes.insert(n->nodeId());
        }
    }
}

MoveNodeCommand::MoveNodeCommand(BasicGraphicsScene *scene,
                                 QPointF const &diff,
                                 std::unordered_set<NodeId> selectedNodes)
    : _scene(scene)
    , _selectedNodes(std::move(selectedNodes))
    , _diff(diff)
{
    setText(QStringLiteral("移动节点"));
}

void MoveNodeCommand::undo()
{
    for (auto nodeId : _selectedNodes) {
        auto oldPos = _scene->graphModel().nodeData(nodeId, NodeRole::Position).value<QPointF>();

        oldPos -= _diff;

        _scene->graphModel().setNodeData(nodeId, NodeRole::Position, oldPos);
    }
}

void MoveNodeCommand::redo()
{
    for (auto nodeId : _selectedNodes) {
        auto oldPos = _scene->graphModel().nodeData(nodeId, NodeRole::Position).value<QPointF>();

        oldPos += _diff;

        _scene->graphModel().setNodeData(nodeId, NodeRole::Position, oldPos);
    }
}

int MoveNodeCommand::id() const
{
    return static_cast<int>(typeid(MoveNodeCommand).hash_code());
}

bool MoveNodeCommand::mergeWith(QUndoCommand const *c)
{
    auto mc = static_cast<MoveNodeCommand const *>(c);

    if (_selectedNodes == mc->_selectedNodes) {
        _diff += mc->_diff;
        return true;
    }
    return false;
}

CreateGroupCommand::CreateGroupCommand(BasicGraphicsScene *scene)
    : _scene(scene)
    , _firstRun(true)  // 添加标志，用于跟踪是否是首次运行
{
    setText(QStringLiteral("创建分组"));
    // 收集选中的节点
    for (QGraphicsItem *item : _scene->selectedItems()) {
        if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            _groupId.nodeIds.push_back(n->nodeId());
        }
    }

    // 判断选中的节点数
    if (_groupId.nodeIds.size()== 0) {

        setObsolete(true);  // 如果节点不足，标记命令为过时
        
    }
}

RemoveFromGroupCommand::RemoveFromGroupCommand(BasicGraphicsScene *scene)
    : _scene(scene)
{
    setText(QStringLiteral("移除分组"));
    if (!_scene) {
        setObsolete(true);
        return;
    }

    auto &graphModel = _scene->graphModel();

    std::vector<GroupId> selectedGroups;
    std::unordered_set<NodeId> selectedNodes;

    for (QGraphicsItem *item : _scene->selectedItems()) {
        if (auto g = qgraphicsitem_cast<GroupGraphicsObject *>(item)) {
            selectedGroups.push_back(g->groupId());
        } else if (auto n = qgraphicsitem_cast<NodeGraphicsObject *>(item)) {
            selectedNodes.insert(n->nodeId());
        }
    }

    auto const allGroups = graphModel.allGroupIds();

    if (!selectedGroups.empty()) {
        for (auto const &gid : selectedGroups) {
            GroupId after = gid;
            after.nodeIds.clear();
            _changes.push_back({gid, after, true});
        }
    } else if (!selectedNodes.empty()) {
        for (auto const &gid : allGroups) {
            GroupId after = gid;
            auto &ids = after.nodeIds;

            auto beforeSize = ids.size();
            ids.erase(std::remove_if(ids.begin(),
                                     ids.end(),
                                     [&selectedNodes](NodeId nid) {
                                         return selectedNodes.count(nid) > 0;
                                     }),
                      ids.end());

            if (ids.size() != beforeSize) {
                bool const deleted = ids.empty();
                _changes.push_back({gid, after, deleted});
            }
        }
    }

    if (_changes.empty()) {
        setObsolete(true);
    }
}

void RemoveFromGroupCommand::redo()
{
    if (!_scene)
        return;

    auto &graphModel = _scene->graphModel();

    for (auto const &ch : _changes) {
        if (ch.deleted) {
            graphModel.deleteGroup(ch.before);
        } else {
            if (auto *ggo = _scene->groupGraphicsObject(ch.before)) {
                ggo->applyGroupId(ch.after);
            }
            graphModel.updateGroup(ch.before, ch.after);
        }
    }
}

void RemoveFromGroupCommand::undo()
{
    if (!_scene)
        return;

    auto &graphModel = _scene->graphModel();

    for (auto it = _changes.rbegin(); it != _changes.rend(); ++it) {
        auto const &ch = *it;
        if (ch.deleted) {
            graphModel.addGroup(ch.before);
        } else {
            if (auto *ggo = _scene->groupGraphicsObject(ch.after)) {
                ggo->applyGroupId(ch.before);
            }
            graphModel.updateGroup(ch.after, ch.before);
        }
    }
}

void CreateGroupCommand::undo()
{
    if (!_groupId.nodeIds.empty()) {
        _scene->graphModel().deleteGroup(_groupId);
    }
}

void CreateGroupCommand::redo()
{
    if (!_groupId.nodeIds.empty()) {
        auto& graphModel = _scene->graphModel();
        // 只在首次运行时才直接创建组
        // 这确保了快捷键操作能立即看到效果
        if (_firstRun) {
            _firstRun = false;
 
            graphModel.addGroup(_groupId);
        } else {

            graphModel.addGroup(_groupId);
        }
    }
}

//------

AlignNodesCommand::AlignNodesCommand(BasicGraphicsScene *scene, std::vector<NodeMove> const &moves)
    : _scene(scene)
    , _moves(moves)
{
    setText(QStringLiteral("对齐节点"));
}

void AlignNodesCommand::undo()
{
    for (auto const &move : _moves) {
        _scene->graphModel().setNodeData(move.nodeId, NodeRole::Position, move.oldPos);
    }
}

void AlignNodesCommand::redo()
{
    for (auto const &move : _moves) {
        _scene->graphModel().setNodeData(move.nodeId, NodeRole::Position, move.newPos);
    }
}

//------

SetNodeDataCommand::SetNodeDataCommand(BasicGraphicsScene *scene,
                                       NodeRole role,
                                       std::vector<Change> changes,
                                       QString const &text)
    : _scene(scene)
    , _role(role)
    , _changes(std::move(changes))
{
    setText(text.isEmpty() ? QStringLiteral("更改节点属性") : text);
}

void SetNodeDataCommand::undo()
{
    for (auto const &c : _changes) {
        _scene->graphModel().setNodeData(c.nodeId, _role, c.oldValue);
    }
}

void SetNodeDataCommand::redo()
{
    for (auto const &c : _changes) {
        _scene->graphModel().setNodeData(c.nodeId, _role, c.newValue);
    }
}

//------

UpdateGroupCommand::UpdateGroupCommand(BasicGraphicsScene *scene,
                                       std::vector<Change> changes,
                                       QString const &text)
    : _scene(scene)
    , _changes(std::move(changes))
{
    setText(text.isEmpty() ? QStringLiteral("更改分组属性") : text);
}

UpdateGroupCommand::UpdateGroupCommand(BasicGraphicsScene *scene,
                                       GroupId const &oldGroup,
                                       GroupId const &newGroup,
                                       QString const &text)
    : UpdateGroupCommand(scene, std::vector<Change>{{oldGroup, newGroup}}, text)
{
}

void UpdateGroupCommand::apply(GroupId const &from, GroupId const &to)
{
    if (auto *ggo = _scene->groupGraphicsObject(from)) {
        ggo->applyGroupId(to);
    }
    _scene->graphModel().updateGroup(from, to);
}

void UpdateGroupCommand::undo()
{
    for (auto const &c : _changes) {
        apply(c.newGroup, c.oldGroup);
    }
}

void UpdateGroupCommand::redo()
{
    for (auto const &c : _changes) {
        apply(c.oldGroup, c.newGroup);
    }
}

//------

SetConnectionDataCommand::SetConnectionDataCommand(BasicGraphicsScene *scene,
                                                   std::vector<Change> changes,
                                                   QString const &text)
    : _scene(scene)
    , _changes(std::move(changes))
{
    setText(text.isEmpty() ? QStringLiteral("更改连线属性") : text);
}

void SetConnectionDataCommand::undo()
{
    for (auto it = _changes.rbegin(); it != _changes.rend(); ++it) {
        _scene->graphModel().setConnectionData(it->connectionId, it->role, it->oldValue);
    }
}

void SetConnectionDataCommand::redo()
{
    for (auto const &c : _changes) {
        _scene->graphModel().setConnectionData(c.connectionId, c.role, c.newValue);
    }
}

} // namespace QtNodes

