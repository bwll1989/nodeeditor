#pragma once

#include <utility>

#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsObject>

#include "ConnectionState.hpp"
#include "Definitions.hpp"

class QGraphicsProxyWidget;
class QGraphicsSceneMouseEvent;
class QLineEdit;

namespace QtNodes {

class AbstractGraphModel;
class BasicGraphicsScene;

/// Graphic Object for connection. Adds itself to scene
class ConnectionGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 2 };

    int type() const override { return Type; }

public:
    ConnectionGraphicsObject(BasicGraphicsScene &scene, ConnectionId const connectionId);

    ~ConnectionGraphicsObject() = default;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    ConnectionId const &connectionId() const;

    QRectF boundingRect() const override;

    QPainterPath shape() const override;

    QPointF const &endPoint(PortType portType) const;

    QPointF out() const { return _out; }

    QPointF in() const { return _in; }

    std::pair<QPointF, QPointF> pointsC1C2() const;

    void setEndPoint(PortType portType, QPointF const &point);

    /// Updates the position of both ends
    void move();

    ConnectionState const &connectionState() const;

    ConnectionState &connectionState();

    /// Whether this connection is drawn as paired end tags.
    bool isVirtual() const;

    /// Shared label for virtual end tags (empty if unset).
    QString virtualLabel() const;

    /// Visible caption for a tag end (`untitled`, or `×N ▾` when folded).
    QString virtualTagCaption(PortType portType) const;

    /// Local-space polygon for one virtual tag (empty if not virtual / hidden by fold).
    QPolygonF virtualTagPolygon(PortType portType) const;

    /// Stable horizontal slot index among virtual connections on the same port.
    int virtualTagSlotIndex(PortType portType) const;

    /// Number of virtual connections on the given end port.
    int virtualTagCount(PortType portType) const;

    /// Width of this connection's tag on the given end (based on its own caption).
    qreal virtualTagWidth(PortType portType) const;

    /// True when this port end is collapsed to a single `×N` chip (count >= 2).
    bool isVirtualPortFolded(PortType portType) const;

    /// Folded group shares hover/selection highlight across sibling connections.
    bool isVirtualGroupHighlighted(PortType portType) const;

    bool isEditingLabel() const;

    /// Which end currently hosts the in-place label editor.
    PortType labelEditPort() const { return _labelEditPort; }

protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant &value) override;

private:
    void initializePosition();

    void addGraphicsEffect();

    void setLockedState();

    void initLabelEditor();
    void startEditingLabel(PortType preferPort = PortType::Out);
    void finishEditingLabel();
    void syncLabelEditorGeometry();

    /// Prefer the virtual tag under `localPos`, else the nearer one.
    PortType virtualTagPortAt(QPointF const &localPos) const;

    /// Text area inside a virtual tag (matches painter layout).
    QRectF virtualTagTextRect(PortType portType) const;

    std::vector<ConnectionId> virtualConnectionsOnPort(PortType portType) const;

    /// Horizontal offset before this tag along the port chain.
    qreal virtualTagChainOffset(PortType portType) const;

    bool isVirtualPortExpanded(PortType portType) const;
    void setVirtualPortExpanded(PortType portType, bool expanded);
    void refreshVirtualPortGraphics(PortType portType);

    /// Expand out/in ends when they have multiple virtual tags.
    void expandVirtualEndsIfNeeded();
    /// Collapse expanded ends when nothing in the group is hovered/selected.
    void maybeCollapseVirtualPorts();

    /// Repaint all siblings that share a folded port with this connection.
    void notifyVirtualGroupRepaint();

private Q_SLOTS:
    void onLockedState(NodeId nodeId);

    std::pair<QPointF, QPointF> pointsC1C2Horizontal() const;

    std::pair<QPointF, QPointF> pointsC1C2Vertical() const;

private:
    ConnectionId _connectionId;

    AbstractGraphModel &_graphModel;

    ConnectionState _connectionState;

    mutable QPointF _out;
    mutable QPointF _in;

    /// In-place virtual label editor (same interaction as node remarks).
    QGraphicsProxyWidget *_labelProxy = nullptr;
    QLineEdit *_labelEditor = nullptr;
    PortType _labelEditPort = PortType::Out;
    bool _finishingLabelEdit = false;
    bool _discardLabelEdit = false;
};

} // namespace QtNodes
