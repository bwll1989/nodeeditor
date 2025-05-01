#pragma once

#include <utility>

#include <QtCore/QUuid>
#include <QtWidgets/QGraphicsObject>

#include "Definitions.hpp"
#include <QtWidgets/QLineEdit>
#include <QKeyEvent>
class QGraphicsSceneMouseEvent;

namespace QtNodes {

class AbstractGraphModel;
class BasicGraphicsScene;

/// Graphic Object for Group. Adds itself to scene
class GroupGraphicsObject : public QGraphicsObject
{
    Q_OBJECT
public:
    // Needed for qgraphicsitem_cast
    enum { Type = UserType + 3 };

    int type() const override { return Type; }

public:
    GroupGraphicsObject(BasicGraphicsScene &scene, GroupId const groupId);

    ~GroupGraphicsObject() = default;

public:
    AbstractGraphModel &graphModel() const;

    BasicGraphicsScene *nodeScene() const;

    QString remarks() const {
        return _groupId.groupRemarks;
    }

    void setRemarks(QString remarks) {
        _groupId.groupRemarks = std::move(remarks);
    }

    QRectF boundingRect() const override;

    QPainterPath shape() const override;

    /// Updates the position of both ends
    void move();
    /// Returns the group id
    GroupId groupId() const {
//        qDebug() << "groupId: "<< _groupId.nodeIds.size() ;
        return _groupId; }
protected:
    void paint(QPainter *painter,
               QStyleOptionGraphicsItem const *option,
               QWidget *widget = 0) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;

    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

    void keyPressEvent(QKeyEvent *event) override;

    bool eventFilter(QObject *obj, QEvent *event) override;

    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
private:
    void updatePosition();

    void addGraphicsEffect();

    void startEditingRemarks();

    void initRemarksEditor();

    void finishEditingRemarks();

    void updateGroupBounds();

    void onNodePositionUpdated(NodeId nodeId);

    void onNodeUpdated(NodeId nodeId);

    void onNodeDeleted(NodeId nodeId);

    void setLockedState();
private Q_SLOTS:
    void onLockedState(GroupId groupId);
private:
    AbstractGraphModel &_graphModel;
    GroupId _groupId;
    QRectF _rect;
    QLineEdit* _remarksEditor = nullptr;
    QString _remarks="Group";
};

} // namespace QtNodes
