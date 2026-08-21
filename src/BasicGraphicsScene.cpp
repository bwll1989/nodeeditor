#include "BasicGraphicsScene.hpp"

#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "AbstractNodeGeometry.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "GroupIdUtils.hpp"
#include "DefaultConnectionPainter.hpp"
#include "DefaultGroupPainter.hpp"
#include "DefaultHorizontalNodeGeometry.hpp"
#include "DefaultNodePainter.hpp"
#include "DefaultVerticalNodeGeometry.hpp"
#include "GraphicsView.hpp"
#include "NodeGraphicsObject.hpp"
#include "GroupGraphicsObject.hpp"
#include <QUndoStack>
#include <QtCore/QBuffer>
#include <QtCore/QByteArray>
#include <QtCore/QDataStream>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtCore/QtGlobal>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QGraphicsSceneMoveEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <algorithm>
#include <memory>
#include <queue>

namespace QtNodes {

BasicGraphicsScene::BasicGraphicsScene(AbstractGraphModel &graphModel, QObject *parent)
    : QGraphicsScene(parent)
    , _graphModel(graphModel)
    , _nodeGeometry(std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel))
    , _nodePainter(std::make_unique<DefaultNodePainter>())
    , _groupPainter(std::make_unique<DefaultGroupPainter>())
    , _connectionPainter(std::make_unique<DefaultConnectionPainter>())
    , _nodeDrag(false)
    , _undoStack(new QUndoStack(this))
    , _orientation(Qt::Horizontal)
{
    setItemIndexMethod(QGraphicsScene::NoIndex);

    connect(&_graphModel,
            &AbstractGraphModel::connectionCreated,
            this,
            &BasicGraphicsScene::onConnectionCreated);

    connect(&_graphModel,
            &AbstractGraphModel::connectionDeleted,
            this,
            &BasicGraphicsScene::onConnectionDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::connectionUpdated,
            this,
            &BasicGraphicsScene::onConnectionUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeCreated,
            this,
            &BasicGraphicsScene::onNodeCreated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeDeleted,
            this,
            &BasicGraphicsScene::onNodeDeleted);

    connect(&_graphModel,
            &AbstractGraphModel::nodePositionUpdated,
            this,
            &BasicGraphicsScene::onNodePositionUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeUpdated,
            this,
            &BasicGraphicsScene::onNodeUpdated);

    connect(&_graphModel,
            &AbstractGraphModel::nodeWidgetUpdated,
            this,
            &BasicGraphicsScene::onNodeWidgetUpdated);

    connect(this, &BasicGraphicsScene::nodeClicked, this, &BasicGraphicsScene::onNodeClicked);

    connect(&_graphModel, &AbstractGraphModel::modelReset, this, &BasicGraphicsScene::onModelReset);

    connect(&_graphModel,&AbstractGraphModel::groupCreated, this, &BasicGraphicsScene::onGroupCreated);

    connect(&_graphModel,&AbstractGraphModel::groupDeleted, this, &BasicGraphicsScene::onGroupDeleted);

    connect(&_graphModel,&AbstractGraphModel::groupUpdated, this, &BasicGraphicsScene::onGroupUpdate);

    traverseGraphAndPopulateGraphicsObjects();
}

BasicGraphicsScene::~BasicGraphicsScene() = default;

AbstractGraphModel const &BasicGraphicsScene::graphModel() const
{
    return _graphModel;
}

AbstractNodeGeometry const &BasicGraphicsScene::nodeGeometry() const
{
    return *_nodeGeometry;
}

AbstractGraphModel &BasicGraphicsScene::graphModel()
{
    return _graphModel;
}

AbstractNodeGeometry &BasicGraphicsScene::nodeGeometry()
{
    return *_nodeGeometry;
}

AbstractNodePainter &BasicGraphicsScene::nodePainter()
{
    return *_nodePainter;
}

AbstractConnectionPainter &BasicGraphicsScene::connectionPainter()
{
    return *_connectionPainter;
}
AbstractGroupPainter &BasicGraphicsScene::groupPainter()
{
    return *_groupPainter;
}
void BasicGraphicsScene::setNodePainter(std::unique_ptr<AbstractNodePainter> newPainter)
{
    _nodePainter = std::move(newPainter);
}

void BasicGraphicsScene::setConnectionPainter(std::unique_ptr<AbstractConnectionPainter> newPainter)
{
    _connectionPainter = std::move(newPainter);
}

void BasicGraphicsScene::setGroupPainter(std::unique_ptr<AbstractGroupPainter> newPainter)
{
    _groupPainter = std::move(newPainter);
}

QUndoStack &BasicGraphicsScene::undoStack()
{
    return *_undoStack;
}

std::unique_ptr<ConnectionGraphicsObject> const &BasicGraphicsScene::makeDraftConnection(
    ConnectionId const incompleteConnectionId)
{
    _draftConnection = std::make_unique<ConnectionGraphicsObject>(*this, incompleteConnectionId);

    _draftConnection->grabMouse();

    return _draftConnection;
}

void BasicGraphicsScene::resetDraftConnection()
{
    _draftConnection.reset();
}

void BasicGraphicsScene::clearScene()
{
    auto const &allNodeIds = graphModel().allNodeIds();

    for (auto nodeId : allNodeIds) {
        graphModel().deleteNode(nodeId);
    }
}

NodeGraphicsObject *BasicGraphicsScene::nodeGraphicsObject(NodeId nodeId)
{
    NodeGraphicsObject *ngo = nullptr;
    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        ngo = it->second.get();
    }

    return ngo;
}

ConnectionGraphicsObject *BasicGraphicsScene::connectionGraphicsObject(ConnectionId connectionId)
{
    ConnectionGraphicsObject *cgo = nullptr;
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        cgo = it->second.get();
    }

    return cgo;
}

GroupGraphicsObject *BasicGraphicsScene::groupGraphicsObject(GroupId groupId)
{
    GroupGraphicsObject *ggo = nullptr;
    auto it = _groupGraphicsObjects.find(groupId);
    if (it != _groupGraphicsObjects.end()) {
        ggo = it->second.get();
    }

    return ggo;
}

void BasicGraphicsScene::setOrientation(Qt::Orientation const orientation)
{
    if (_orientation != orientation) {
        _orientation = orientation;

        switch (_orientation) {
        case Qt::Horizontal:
            _nodeGeometry = std::make_unique<DefaultHorizontalNodeGeometry>(_graphModel);
            break;

        case Qt::Vertical:
            _nodeGeometry = std::make_unique<DefaultVerticalNodeGeometry>(_graphModel);
            break;
        }

        onModelReset();
    }
}

QMenu *BasicGraphicsScene::createSceneMenu(QPointF const scenePos)
{
    Q_UNUSED(scenePos);
    return nullptr;
}

void BasicGraphicsScene::appendContextMenuActions(QMenu &menu, ContextMenuKind kind)
{
    // Blank canvas: leave empty so GraphicsView can fall back to createSceneMenu
    // (examples keep right-click → create node).
    if (kind == ContextMenuKind::Scene)
        return;

    // Node / Connection / Group: dump all view actions (examples / library demos).
    if (views().isEmpty())
        return;

    menu.addSeparator();
    for (QAction *act : views().first()->actions()) {
        if (act)
            menu.addAction(act);
    }
}

void BasicGraphicsScene::traverseGraphAndPopulateGraphicsObjects()
{
    auto allNodeIds = _graphModel.allNodeIds();

    // First create all the nodes.
    for (NodeId const nodeId : allNodeIds) {
        _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);
    }

    // Then for each node check output connections and insert them.
    for (NodeId const nodeId : allNodeIds) {
        auto nOutPorts = _graphModel.nodeData<PortCount>(nodeId, NodeRole::OutPortCount);

        for (PortIndex index = 0; index < nOutPorts; ++index) {
            auto const &outConnectionIds = _graphModel.connections(nodeId, PortType::Out, index);

            for (auto cid : outConnectionIds) {
                _connectionGraphicsObjects[cid] = std::make_unique<ConnectionGraphicsObject>(*this,
                                                                                             cid);
            }
        }
    }
}

void BasicGraphicsScene::updateAttachedNodes(ConnectionId const connectionId,
                                             PortType const portType)
{
    auto node = nodeGraphicsObject(getNodeId(portType, connectionId));

    if (node) {
        node->update();
    }
}

void BasicGraphicsScene::onConnectionDeleted(ConnectionId const connectionId)
{
    auto it = _connectionGraphicsObjects.find(connectionId);
    if (it != _connectionGraphicsObjects.end()) {
        _connectionGraphicsObjects.erase(it);
    }

    // TODO: do we need it?
    if (_draftConnection && _draftConnection->connectionId() == connectionId) {
        _draftConnection.reset();
    }

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onConnectionCreated(ConnectionId const connectionId)
{
    _connectionGraphicsObjects[connectionId]
        = std::make_unique<ConnectionGraphicsObject>(*this, connectionId);

    updateAttachedNodes(connectionId, PortType::Out);
    updateAttachedNodes(connectionId, PortType::In);

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onConnectionUpdated(ConnectionId const connectionId)
{
    if (auto *cgo = connectionGraphicsObject(connectionId)) {
        cgo->move();
    }
    Q_EMIT modified(this);
}

void BasicGraphicsScene::onGroupCreated(const QtNodes::GroupId groupId)
{
    _groupGraphicsObjects[groupId] = std::make_unique<GroupGraphicsObject>(*this, groupId);
    Q_EMIT modified(this);
}

void BasicGraphicsScene::onGroupUpdate(const QtNodes::GroupId groupId)
{
    auto it = _groupGraphicsObjects.find(groupId);
    if (it!= _groupGraphicsObjects.end()) {
        auto groupObj = std::move(it->second);
        _groupGraphicsObjects.erase(it);  // 删除旧键
        auto newGroupId=groupObj->groupId();
          // 3. 更新图形对象的内部ID
        _groupGraphicsObjects[newGroupId] = std::move(groupObj);

        Q_EMIT modified(this);
    }

}

void BasicGraphicsScene::onGroupDeleted(const QtNodes::GroupId groupId)
{
    // 在组图形对象映射中查找指定 groupId
    auto it = _groupGraphicsObjects.find(groupId);
    if (it != _groupGraphicsObjects.end()) {
        // 获取该组对应的 GroupId（包含组内所有节点 ID）
        GroupId const gid = it->first;

        // 将被删除组内的所有节点设为可见
        for (auto const nodeId : gid.nodeIds) {
            if (auto* nodeItem = nodeGraphicsObject(nodeId)) {
                nodeItem->setVisible(true);
            }
        }

        // 收集该组内所有节点相关的连接 ID，使用无序集合去重
        std::unordered_set<ConnectionId> affectedConnections;
        for (auto const nodeId : gid.nodeIds) {
            auto const conns = _graphModel.allConnectionIds(nodeId);
            affectedConnections.insert(conns.begin(), conns.end());
        }

        // 通知所有受影响的连接重新计算位置
        for (auto const &cid : affectedConnections) {
            if (auto* cgo = connectionGraphicsObject(cid)) {
                cgo->move();
            }
        }

        // 从映射中移除该组图形对象
        _groupGraphicsObjects.erase(it);
    }
    // 发送场景被修改的信号
    Q_EMIT modified(this);
}

void BasicGraphicsScene::onNodeDeleted(NodeId const nodeId)
{
    auto it = _nodeGraphicsObjects.find(nodeId);
    if (it != _nodeGraphicsObjects.end()) {
        _nodeGraphicsObjects.erase(it);

        Q_EMIT modified(this);
    }
}

void BasicGraphicsScene::onNodeCreated(NodeId const nodeId)
{
    _nodeGraphicsObjects[nodeId] = std::make_unique<NodeGraphicsObject>(*this, nodeId);

    Q_EMIT modified(this);
}

void BasicGraphicsScene::onNodePositionUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);
    if (node) {
        node->setPos(_graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
        node->update();
        _nodeDrag = true;
    }
}

void BasicGraphicsScene::onNodeUpdated(NodeId const nodeId)
{
    auto node = nodeGraphicsObject(nodeId);

    if (node) {
        node->setGeometryChanged();

        _nodeGeometry->recomputeSize(nodeId);

        node->updateQWidgetEmbedPos();
        node->update();
        node->moveConnections();
    }
}

void BasicGraphicsScene::onNodeClicked(NodeId const nodeId)
{
    if (_nodeDrag) {
        Q_EMIT nodeMoved(nodeId, _graphModel.nodeData(nodeId, NodeRole::Position).value<QPointF>());
        Q_EMIT modified(this);
    }
    _nodeDrag = false;
}

void BasicGraphicsScene::onModelReset()
{
    _connectionGraphicsObjects.clear();
    _nodeGraphicsObjects.clear();
    _groupGraphicsObjects.clear();
    clear();

    traverseGraphAndPopulateGraphicsObjects();
}

void BasicGraphicsScene::centerOnNode(NodeId nodeId) {
    auto nodeItem = nodeGraphicsObject(nodeId);
    if (nodeItem) {
        views().first()->centerOn(nodeItem);
    }
}

void BasicGraphicsScene::selectAndCenterNode(NodeId nodeId)
{
    if (!_graphModel.nodeExists(nodeId)) {
        return;
    }

    clearSelection();
    if (auto *nodeObj = nodeGraphicsObject(nodeId)) {
        nodeObj->setSelected(true);
    }
    if (!views().isEmpty()) {
        centerOnNode(nodeId);
    }
}

void BasicGraphicsScene::selectAndCenterConnection(ConnectionId connectionId)
{
    clearSelection();
    auto *cgo = connectionGraphicsObject(connectionId);
    if (!cgo) {
        return;
    }
    cgo->setSelected(true);
    if (!views().isEmpty()) {
        views().first()->centerOn(cgo);
    }
}

void BasicGraphicsScene::showSearchNodeBar()
{
    // 延迟激活并聚焦输入框，避免快捷键事件抢焦点失败
    const auto focusSearchEdit = [](QDialog *dialog) {
        if (!dialog) {
            return;
        }
        QPointer<QDialog> dialogPtr(dialog);
        QTimer::singleShot(0, dialog, [dialogPtr]() {
            if (!dialogPtr) {
                return;
            }
            dialogPtr->raise();
            dialogPtr->activateWindow();
            if (auto *edit = dialogPtr->findChild<QLineEdit *>(QStringLiteral("searchNodeEdit"))) {
                edit->setFocus(Qt::ShortcutFocusReason);
                edit->selectAll();
            }
        });
    };

    // 本场景已有搜索条则复用
    if (_searchNodeBar) {
        _searchNodeBar->show();
        focusSearchEdit(_searchNodeBar);
        return;
    }

    QWidget *parentWidget = views().isEmpty() ? nullptr : views().first();

    auto *dialog = new QDialog(parentWidget);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    // Dialog（非 Tool）才能在 Windows 上稳定抢到键盘焦点；无边框隐藏标题栏
    dialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    dialog->setFixedHeight(40);
    dialog->resize(520, 40);
    _searchNodeBar = dialog;

    auto *layout = new QHBoxLayout(dialog);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    auto *searchEdit = new QLineEdit(dialog);
    searchEdit->setObjectName(QStringLiteral("searchNodeEdit"));
    searchEdit->setFocusPolicy(Qt::StrongFocus);
    searchEdit->setPlaceholderText(tr("Search by name, type, id or virtual tag..."));
    dialog->setFocusProxy(searchEdit);

    auto *prevBtn = new QToolButton(dialog);
    prevBtn->setArrowType(Qt::LeftArrow);
    prevBtn->setToolTip(tr("Previous match"));
    prevBtn->setAutoRaise(true);
    prevBtn->setEnabled(false);

    auto *nextBtn = new QToolButton(dialog);
    nextBtn->setArrowType(Qt::RightArrow);
    nextBtn->setToolTip(tr("Next match"));
    nextBtn->setAutoRaise(true);
    nextBtn->setEnabled(false);

    auto *countLabel = new QLabel(tr("0/0"), dialog);
    countLabel->setMinimumWidth(72);
    countLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addWidget(searchEdit, 1);
    layout->addWidget(prevBtn);
    layout->addWidget(nextBtn);
    layout->addWidget(countLabel);

    enum class MatchKind { Node, Connection };
    struct Entry {
        MatchKind kind = MatchKind::Node;
        NodeId nodeId = InvalidNodeId;
        ConnectionId connectionId{};
        QString haystack;
    };

    auto entries = std::make_shared<QVector<Entry>>();
    auto matches = std::make_shared<QVector<Entry>>();
    auto matchIndex = std::make_shared<int>(-1);

    entries->reserve(static_cast<int>(_graphModel.allNodeIds().size()));
    for (NodeId id : _graphModel.allNodeIds()) {
        const QString remarks = _graphModel.nodeData(id, NodeRole::Remarks).toString();
        const QString type = _graphModel.nodeData(id, NodeRole::Type).toString();
        const QString caption = _graphModel.nodeData(id, NodeRole::Caption).toString();
        const QString idText = QString::number(id);
        Entry entry;
        entry.kind = MatchKind::Node;
        entry.nodeId = id;
        entry.haystack = (idText + QLatin1Char(' ') + remarks + QLatin1Char(' ') + caption
                          + QLatin1Char(' ') + type)
                             .toLower();
        entries->push_back(std::move(entry));
    }

    {
        std::unordered_set<ConnectionId> seenConnections;
        for (NodeId id : _graphModel.allNodeIds()) {
            for (ConnectionId const &cid : _graphModel.allConnectionIds(id)) {
                if (!seenConnections.insert(cid).second) {
                    continue;
                }
                if (!_graphModel.connectionData(cid, ConnectionRole::Virtual).toBool()) {
                    continue;
                }
                QString label = _graphModel.connectionData(cid, ConnectionRole::VirtualLabel)
                                    .toString();
                if (label.isEmpty()) {
                    label = QStringLiteral("untitled");
                }
                Entry entry;
                entry.kind = MatchKind::Connection;
                entry.connectionId = cid;
                entry.haystack = label.toLower();
                entries->push_back(std::move(entry));
            }
        }
    }

    std::sort(entries->begin(), entries->end(), [](Entry const &a, Entry const &b) {
        if (a.kind != b.kind) {
            return a.kind == MatchKind::Node;
        }
        if (a.kind == MatchKind::Node) {
            return a.nodeId < b.nodeId;
        }
        if (a.connectionId.outNodeId != b.connectionId.outNodeId) {
            return a.connectionId.outNodeId < b.connectionId.outNodeId;
        }
        if (a.connectionId.outPortIndex != b.connectionId.outPortIndex) {
            return a.connectionId.outPortIndex < b.connectionId.outPortIndex;
        }
        if (a.connectionId.inNodeId != b.connectionId.inNodeId) {
            return a.connectionId.inNodeId < b.connectionId.inNodeId;
        }
        return a.connectionId.inPortIndex < b.connectionId.inPortIndex;
    });

    const auto updateStatus = [prevBtn, nextBtn, countLabel, matches, matchIndex]() {
        bool const hasMatches = !matches->isEmpty();
        prevBtn->setEnabled(hasMatches);
        nextBtn->setEnabled(hasMatches);
        if (!hasMatches || *matchIndex < 0) {
            countLabel->setText(QObject::tr("0/%1").arg(matches->size()));
            return;
        }
        countLabel->setText(QObject::tr("%1/%2").arg(*matchIndex + 1).arg(matches->size()));
    };

    const auto focusMatchAt = [this, dialog, searchEdit, matches, matchIndex, updateStatus](int index) {
        if (matches->isEmpty()) {
            *matchIndex = -1;
            updateStatus();
            return;
        }
        int const count = matches->size();
        *matchIndex = ((index % count) + count) % count;
        Entry const &match = matches->at(*matchIndex);
        if (match.kind == MatchKind::Node) {
            selectAndCenterNode(match.nodeId);
        } else {
            selectAndCenterConnection(match.connectionId);
        }
        updateStatus();
        dialog->raise();
        searchEdit->setFocus(Qt::OtherFocusReason);
    };

    const auto refill = [entries, matches, matchIndex, focusMatchAt, updateStatus](QString const &filter) {
        matches->clear();
        *matchIndex = -1;
        QString const needle = filter.trimmed().toLower();
        if (!needle.isEmpty()) {
            for (Entry const &entry : *entries) {
                if (entry.haystack.contains(needle)) {
                    matches->push_back(entry);
                }
            }
        }
        updateStatus();
        if (!matches->isEmpty()) {
            focusMatchAt(0);
        }
    };

    // 点击搜索条外空白区域时自动关闭
    class OutsideCloseFilter : public QObject
    {
    public:
        explicit OutsideCloseFilter(QDialog *dlg)
            : QObject(dlg)
            , _dialog(dlg)
        {
        }

        bool eventFilter(QObject *, QEvent *event) override
        {
            if (!_dialog || !_dialog->isVisible()) {
                return false;
            }
            if (event->type() != QEvent::MouseButtonPress) {
                return false;
            }
            QPoint const globalPos = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
            if (!_dialog->frameGeometry().contains(globalPos)) {
                _dialog->reject();
            }
            return false;
        }

    private:
        QPointer<QDialog> _dialog;
    };

    qApp->installEventFilter(new OutsideCloseFilter(dialog));

    QObject::connect(searchEdit, &QLineEdit::textChanged, dialog, refill);
    QObject::connect(prevBtn, &QToolButton::clicked, dialog, [focusMatchAt, matchIndex]() {
        focusMatchAt(*matchIndex < 0 ? 0 : *matchIndex - 1);
    });
    QObject::connect(nextBtn, &QToolButton::clicked, dialog, [focusMatchAt, matchIndex]() {
        focusMatchAt(*matchIndex < 0 ? 0 : *matchIndex + 1);
    });
    QObject::connect(searchEdit, &QLineEdit::returnPressed, dialog, [focusMatchAt, matchIndex]() {
        focusMatchAt(*matchIndex < 0 ? 0 : *matchIndex + 1);
    });

    if (parentWidget) {
        QPoint const topCenter = parentWidget->mapToGlobal(QPoint(parentWidget->width() / 2, 12));
        dialog->move(topCenter.x() - dialog->width() / 2, topCenter.y());
    }

    dialog->show();
    focusSearchEdit(dialog);
}

void BasicGraphicsScene::onNodeWidgetUpdated(NodeId const nodeId) {
    auto node = nodeGraphicsObject(nodeId);
    if (node) {
        node->onEmbedWidgetChanged();
        // node->updateQWidgetEmbedPos();
        node->update();
    }
}

} // namespace QtNodes
