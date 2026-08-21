#pragma once

#include "Definitions.hpp"
#include "Export.hpp"

#include <QtCore/QJsonObject>
#include <QtCore/QPointF>
#include <QtCore/QVariant>
#include <QUndoCommand>

#include <unordered_set>
#include <vector>

namespace QtNodes {

class BasicGraphicsScene;

class NODE_EDITOR_PUBLIC CreateCommand : public QUndoCommand
{
public:
    CreateCommand(BasicGraphicsScene *scene, QString const name, QPointF const &mouseScenePos);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    NodeId _nodeId;
    QJsonObject _sceneJson;
};

/**
 * Selected scene objects are serialized and then removed from the scene.
 * The deleted elements could be restored in `undo`.
 */
class NODE_EDITOR_PUBLIC DeleteCommand : public QUndoCommand
{
public:
    DeleteCommand(BasicGraphicsScene *scene);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    QJsonObject _sceneJson;
};

class NODE_EDITOR_PUBLIC CopyCommand : public QUndoCommand
{
public:
    CopyCommand(BasicGraphicsScene *scene);
};

class NODE_EDITOR_PUBLIC PasteCommand : public QUndoCommand
{
public:
    PasteCommand(BasicGraphicsScene *scene, QPointF const &mouseScenePos);

    void undo() override;
    void redo() override;

private:
    QJsonObject takeSceneJsonFromClipboard();
    QJsonObject makeNewNodeIdsInScene(QJsonObject const &sceneJson);

private:
    BasicGraphicsScene *_scene;
    QPointF const &_mouseScenePos;
    QJsonObject _newSceneJson;
};

class NODE_EDITOR_PUBLIC DisconnectCommand : public QUndoCommand
{
public:
    DisconnectCommand(BasicGraphicsScene *scene, ConnectionId const);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;

    ConnectionId _connId;
    bool _wasVirtual = false;
    QString _virtualLabel;
};

class NODE_EDITOR_PUBLIC ConnectCommand : public QUndoCommand
{
public:
    ConnectCommand(BasicGraphicsScene *scene, ConnectionId const);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;

    ConnectionId _connId;
};

class NODE_EDITOR_PUBLIC MoveNodeCommand : public QUndoCommand
{
public:
    MoveNodeCommand(BasicGraphicsScene *scene, QPointF const &diff);

    MoveNodeCommand(BasicGraphicsScene *scene,
                    QPointF const &diff,
                    std::unordered_set<NodeId> selectedNodes);

    void undo() override;
    void redo() override;

    /**
   * A command ID is used in command compression. It must be an integer unique to
   * this command's class, or -1 if the command doesn't support compression.
   */
    int id() const override;

    /**
   * Several sequential movements could be merged into one command.
   */
    bool mergeWith(QUndoCommand const *c) override;

private:
    BasicGraphicsScene *_scene;
    std::unordered_set<NodeId> _selectedNodes;
    QPointF _diff;
};

class NODE_EDITOR_PUBLIC CreateGroupCommand : public QUndoCommand
{
public:
    CreateGroupCommand(BasicGraphicsScene *scene);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    GroupId _groupId;
    bool _firstRun;  // 添加跟踪标志
};

class NODE_EDITOR_PUBLIC RemoveFromGroupCommand : public QUndoCommand
{
public:
    explicit RemoveFromGroupCommand(BasicGraphicsScene *scene);

    void undo() override;
    void redo() override;

private:
    struct GroupChange {
        GroupId before;
        GroupId after;
        bool deleted;
    };

    BasicGraphicsScene *_scene;
    std::vector<GroupChange> _changes;
};

class NODE_EDITOR_PUBLIC AlignNodesCommand : public QUndoCommand
{
public:
    struct NodeMove {
        NodeId nodeId;
        QPointF oldPos;
        QPointF newPos;
    };

    AlignNodesCommand(BasicGraphicsScene *scene, std::vector<NodeMove> const &moves);

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    std::vector<NodeMove> _moves;
};

/**
 * Undoable node property change (remarks, style/color, …).
 * Multiple nodes can be batched into one undo step.
 */
class NODE_EDITOR_PUBLIC SetNodeDataCommand : public QUndoCommand
{
public:
    struct Change {
        NodeId nodeId;
        QVariant oldValue;
        QVariant newValue;
    };

    SetNodeDataCommand(BasicGraphicsScene *scene,
                       NodeRole role,
                       std::vector<Change> changes,
                       QString const &text = {});

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    NodeRole _role;
    std::vector<Change> _changes;
};

/**
 * Undoable group metadata change (remarks, title color, …).
 * Multiple groups can be batched into one undo step.
 */
class NODE_EDITOR_PUBLIC UpdateGroupCommand : public QUndoCommand
{
public:
    struct Change {
        GroupId oldGroup;
        GroupId newGroup;
    };

    UpdateGroupCommand(BasicGraphicsScene *scene,
                       std::vector<Change> changes,
                       QString const &text = {});

    UpdateGroupCommand(BasicGraphicsScene *scene,
                       GroupId const &oldGroup,
                       GroupId const &newGroup,
                       QString const &text = {});

    void undo() override;
    void redo() override;

private:
    void apply(GroupId const &from, GroupId const &to);

    BasicGraphicsScene *_scene;
    std::vector<Change> _changes;
};

/**
 * Undoable connection display change (virtual flag / label).
 * Multiple role changes can be batched into one undo step.
 */
class NODE_EDITOR_PUBLIC SetConnectionDataCommand : public QUndoCommand
{
public:
    struct Change {
        ConnectionId connectionId;
        ConnectionRole role;
        QVariant oldValue;
        QVariant newValue;
    };

    SetConnectionDataCommand(BasicGraphicsScene *scene,
                             std::vector<Change> changes,
                             QString const &text = {});

    void undo() override;
    void redo() override;

private:
    BasicGraphicsScene *_scene;
    std::vector<Change> _changes;
};

} // namespace QtNodes
