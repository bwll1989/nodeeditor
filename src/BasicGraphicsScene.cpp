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
#include "NodeSearchSession.hpp"
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
#include <QtWidgets/QFrame>
#include <QtWidgets/QGraphicsSceneMoveEvent>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QToolButton>
#include <QtGui/QKeyEvent>
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
    setItemIndexMethod(QGraphicsScene::BspTreeIndex);

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

void BasicGraphicsScene::requestSendOscBindingToWebPanel(QJsonObject const &binding)
{
    Q_EMIT sendOscBindingToWebPanel(binding);
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
    if (views().isEmpty()) {
        return;
    }

    auto *view = views().first();

    // 虚拟标签：按两端标签定位。跨度过大（当前视口装不下）时优先聚焦 Out 端标签，
    // 避免落到两标签连线中点的空白区域。
    if (cgo->isVirtual() && !cgo->connectionState().requiresPort()) {
        auto tagSceneRect = [cgo](PortType port) -> QRectF {
            QPolygonF const poly = cgo->virtualTagPolygon(port);
            if (poly.isEmpty()) {
                return {};
            }
            return cgo->mapToScene(poly.boundingRect()).boundingRect();
        };

        QRectF const outTag = tagSceneRect(PortType::Out);
        QRectF const inTag = tagSceneRect(PortType::In);
        QRectF tags;
        if (outTag.isValid()) {
            tags = outTag;
        }
        if (inTag.isValid()) {
            tags = tags.isValid() ? tags.united(inTag) : inTag;
        }

        if (tags.isValid() && !tags.isEmpty()) {
            QRectF const viewRect = view->mapToScene(view->viewport()->rect()).boundingRect();
            bool const bothFit = tags.width() <= viewRect.width() * 0.9
                                 && tags.height() <= viewRect.height() * 0.9;
            if (bothFit) {
                view->centerOn(tags.center());
            } else if (outTag.isValid()) {
                view->centerOn(outTag.center());
            } else {
                view->centerOn(inTag.center());
            }
            return;
        }
    }

    view->centerOn(cgo);
}

void BasicGraphicsScene::showSearchNodeBar()
{
    if (views().isEmpty()) {
        return;
    }
    auto *view = qobject_cast<QGraphicsView *>(views().first());
    if (!view) {
        return;
    }

    const auto focusEditOf = [](QWidget *bar) {
        if (!bar) {
            return;
        }
        QPointer<QWidget> barPtr(bar);
        QTimer::singleShot(0, bar, [barPtr]() {
            if (!barPtr) {
                return;
            }
            barPtr->raise();
            if (auto *edit = barPtr->findChild<QLineEdit *>(QStringLiteral("searchNodeEdit"))) {
                edit->setFocus(Qt::ShortcutFocusReason);
                edit->selectAll();
            }
        });
    };

    if (_searchNodeBar && _searchNodeBar->parentWidget() != view) {
        delete _searchNodeBar;
        _searchNodeBar = nullptr;
    }

    if (!_searchNodeBar) {
        auto *bar = new QFrame(view);
        bar->setObjectName(QStringLiteral("nodeSearchBar"));
        bar->setAttribute(Qt::WA_StyledBackground, true);
        bar->setFixedHeight(40);
        bar->setFocusPolicy(Qt::StrongFocus);
        _searchNodeBar = bar;

        auto *layout = new QHBoxLayout(bar);
        layout->setContentsMargins(10, 4, 10, 4);
        layout->setSpacing(6);

        auto *searchEdit = new QLineEdit(bar);
        searchEdit->setObjectName(QStringLiteral("searchNodeEdit"));
        searchEdit->setFocusPolicy(Qt::StrongFocus);
        searchEdit->setPlaceholderText(tr("Search by name, type, id or virtual tag..."));
        searchEdit->setClearButtonEnabled(true);
        bar->setFocusProxy(searchEdit);

        auto *prevBtn = new QToolButton(bar);
        prevBtn->setObjectName(QStringLiteral("searchNodePrev"));
        prevBtn->setArrowType(Qt::LeftArrow);
        prevBtn->setToolTip(tr("Previous match"));
        prevBtn->setAutoRaise(true);
        prevBtn->setEnabled(false);

        auto *nextBtn = new QToolButton(bar);
        nextBtn->setObjectName(QStringLiteral("searchNodeNext"));
        nextBtn->setArrowType(Qt::RightArrow);
        nextBtn->setToolTip(tr("Next match"));
        nextBtn->setAutoRaise(true);
        nextBtn->setEnabled(false);

        auto *countLabel = new QLabel(tr("0/0"), bar);
        countLabel->setObjectName(QStringLiteral("searchNodeCount"));
        countLabel->setMinimumWidth(56);
        countLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        auto *closeBtn = new QToolButton(bar);
        closeBtn->setObjectName(QStringLiteral("searchNodeClose"));
        closeBtn->setText(QStringLiteral("×"));
        closeBtn->setToolTip(tr("Close (Esc)"));
        closeBtn->setAutoRaise(true);
        closeBtn->setFixedWidth(22);

        layout->addWidget(searchEdit, 1);
        layout->addWidget(prevBtn);
        layout->addWidget(nextBtn);
        layout->addWidget(countLabel);
        layout->addWidget(closeBtn);

        auto *session = new NodeSearchSession(bar);

        session->reposition = [this, bar]() {
            if (!bar || views().isEmpty()) {
                return;
            }
            QWidget *host = views().first();
            int const margin = 12;
            int const w = qBound(320, host->width() - 2 * margin, 560);
            bar->setFixedWidth(w);
            bar->move((host->width() - w) / 2, margin);
            bar->raise();
        };

        session->focusEdit = [bar, focusEditOf]() { focusEditOf(bar); };

        session->rebuildIndex = [this, session]() {
            session->entries.clear();
            session->entries.reserve(static_cast<int>(_graphModel.allNodeIds().size()));
            for (NodeId id : _graphModel.allNodeIds()) {
                QString const remarks = _graphModel.nodeData(id, NodeRole::Remarks).toString();
                QString const type = _graphModel.nodeData(id, NodeRole::Type).toString();
                QString const caption = _graphModel.nodeData(id, NodeRole::Caption).toString();
                QString const idText = QString::number(id);
                SearchEntry entry;
                entry.kind = SearchMatchKind::Node;
                entry.nodeId = id;
                entry.haystack = (idText + QLatin1Char(' ') + remarks + QLatin1Char(' ') + caption
                                  + QLatin1Char(' ') + type)
                                     .toLower();
                session->entries.push_back(std::move(entry));
            }

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
                    SearchEntry entry;
                    entry.kind = SearchMatchKind::Connection;
                    entry.connectionId = cid;
                    entry.haystack = label.toLower();
                    session->entries.push_back(std::move(entry));
                }
            }

            std::sort(session->entries.begin(),
                      session->entries.end(),
                      [](SearchEntry const &a, SearchEntry const &b) {
                          if (a.kind != b.kind) {
                              return a.kind == SearchMatchKind::Node;
                          }
                          if (a.kind == SearchMatchKind::Node) {
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
        };

        auto updateStatus = [prevBtn, nextBtn, countLabel, session]() {
            bool const hasMatches = !session->matches.isEmpty();
            prevBtn->setEnabled(hasMatches);
            nextBtn->setEnabled(hasMatches);
            if (!hasMatches || session->matchIndex < 0) {
                countLabel->setText(QObject::tr("0/%1").arg(session->matches.size()));
                return;
            }
            countLabel->setText(
                QObject::tr("%1/%2").arg(session->matchIndex + 1).arg(session->matches.size()));
        };

        auto focusMatchAt = [this, bar, searchEdit, session, updateStatus](int index) {
            if (session->matches.isEmpty()) {
                session->matchIndex = -1;
                updateStatus();
                return;
            }
            int const count = session->matches.size();
            session->matchIndex = ((index % count) + count) % count;
            SearchEntry const &match = session->matches.at(session->matchIndex);
            if (match.kind == SearchMatchKind::Node) {
                selectAndCenterNode(match.nodeId);
            } else {
                selectAndCenterConnection(match.connectionId);
            }
            updateStatus();
            bar->raise();
            searchEdit->setFocus(Qt::OtherFocusReason);
        };

        session->refill = [session, focusMatchAt, updateStatus](QString const &filter) {
            session->matches.clear();
            session->matchIndex = -1;
            QString const needle = filter.trimmed().toLower();
            if (!needle.isEmpty()) {
                for (SearchEntry const &entry : session->entries) {
                    if (entry.haystack.contains(needle)) {
                        session->matches.push_back(entry);
                    }
                }
            }
            updateStatus();
            if (!session->matches.isEmpty()) {
                focusMatchAt(0);
            }
        };

        class OverlayEventFilter : public QObject
        {
        public:
            OverlayEventFilter(QWidget *bar, QGraphicsView *host, NodeSearchSession *session)
                : QObject(bar)
                , _bar(bar)
                , _host(host)
                , _session(session)
            {}

            bool eventFilter(QObject *watched, QEvent *event) override
            {
                if (!_bar) {
                    return false;
                }

                if (watched == _host && event->type() == QEvent::Resize) {
                    if (_session && _session->reposition) {
                        _session->reposition();
                    }
                    return false;
                }

                // Esc 只在搜索条及其子控件上拦截，避免抢占全局 Esc
                if (event->type() == QEvent::KeyPress && _bar->isVisible()) {
                    auto *ke = static_cast<QKeyEvent *>(event);
                    if (ke->key() == Qt::Key_Escape) {
                        auto *w = qobject_cast<QWidget *>(watched);
                        if (w && (_bar == w || _bar->isAncestorOf(w))) {
                            _bar->hide();
                            if (_host) {
                                _host->setFocus(Qt::OtherFocusReason);
                            }
                            return true;
                        }
                    }
                }

                if (event->type() == QEvent::MouseButtonPress && _bar->isVisible()) {
                    auto *me = static_cast<QMouseEvent *>(event);
                    QPoint const globalPos = me->globalPosition().toPoint();
                    if (!_bar->rect().contains(_bar->mapFromGlobal(globalPos))) {
                        // 点在搜索条外：关闭；不拦截，让画布继续处理拖拽/点选
                        _bar->hide();
                    }
                }
                return false;
            }

        private:
            QPointer<QWidget> _bar;
            QPointer<QGraphicsView> _host;
            NodeSearchSession *_session = nullptr;
        };

        auto *filter = new OverlayEventFilter(bar, view, session);
        view->installEventFilter(filter);
        bar->installEventFilter(filter);
        searchEdit->installEventFilter(filter);
        qApp->installEventFilter(filter);

        QObject::connect(searchEdit, &QLineEdit::textChanged, bar, [session](QString const &text) {
            if (session->refill) {
                session->refill(text);
            }
        });
        QObject::connect(prevBtn, &QToolButton::clicked, bar, [session, focusMatchAt]() {
            focusMatchAt(session->matchIndex < 0 ? 0 : session->matchIndex - 1);
        });
        QObject::connect(nextBtn, &QToolButton::clicked, bar, [session, focusMatchAt]() {
            focusMatchAt(session->matchIndex < 0 ? 0 : session->matchIndex + 1);
        });
        QObject::connect(searchEdit, &QLineEdit::returnPressed, bar, [session, focusMatchAt]() {
            focusMatchAt(session->matchIndex < 0 ? 0 : session->matchIndex + 1);
        });
        QObject::connect(closeBtn, &QToolButton::clicked, bar, [bar, view]() {
            bar->hide();
            view->setFocus(Qt::OtherFocusReason);
        });
    }

    auto *session = searchSessionOf(_searchNodeBar);
    if (!session || !session->rebuildIndex || !session->reposition || !session->refill
        || !session->focusEdit) {
        return;
    }

    session->rebuildIndex();
    session->reposition();
    _searchNodeBar->show();
    _searchNodeBar->raise();

    if (auto *edit = _searchNodeBar->findChild<QLineEdit *>(QStringLiteral("searchNodeEdit"))) {
        session->refill(edit->text());
    }
    session->focusEdit();
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
