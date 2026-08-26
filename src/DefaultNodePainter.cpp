#include "DefaultNodePainter.hpp"

#include <cmath>

#include <QtCore/QJsonDocument>
#include <QtCore/QMargins>

#include "AbstractGraphModel.hpp"
#include "AbstractNodeGeometry.hpp"
#include "BasicGraphicsScene.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionIdUtils.hpp"
#include "DefaultHorizontalNodeGeometry.hpp"
#include "NodeDelegateModel.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeState.hpp"
#include "StyleCollection.hpp"
#include <QLineEdit>
#include <QPainterPath>
namespace QtNodes {

void DefaultNodePainter::paint(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeStyle const nodeStyle(
        QJsonDocument::fromVariant(model.nodeData(ngo.nodeId(), NodeRole::Style)).object());

    painter->setRenderHint(QPainter::Antialiasing, true);

    drawNodeRect(painter, ngo, nodeStyle);

    drawConnectionPoints(painter, ngo, nodeStyle);

    drawFilledConnectionPoints(painter, ngo, nodeStyle);

    drawNodeCaption(painter, ngo, nodeStyle);

    drawEntryLabels(painter, ngo, nodeStyle);

    drawResizeRect(painter, ngo);

    drawValidationIcon(painter, ngo, nodeStyle);

    drawMutedOverlay(painter, ngo, nodeStyle);
}

void DefaultNodePainter::drawNodeRect(QPainter *painter,
                                      NodeGraphicsObject &ngo,
                                      NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();

    NodeId const nodeId = ngo.nodeId();

    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    QSize size = geometry.size(nodeId);

    QVariant var = model.nodeData(nodeId, NodeRole::ValidationState);

    QColor color = ngo.isSelected() ? nodeStyle.SelectedBoundaryColor
                                    : nodeStyle.NormalBoundaryColor;

    auto validationState = NodeValidationState::State::Valid;
    if (var.canConvert<NodeValidationState>()) {
        auto state = var.value<NodeValidationState>();
        validationState = state._state;
        switch (validationState) {
        case NodeValidationState::State::Error:
            color = nodeStyle.ErrorColor;
            break;
        case NodeValidationState::State::Warning:
            color = nodeStyle.WarningColor;
            break;
        default:
            break;
        }
    }

    float penWidth = ngo.nodeState().hovered() || ngo.isSelected() ? nodeStyle.HoveredPenWidth : nodeStyle.PenWidth;
    if (validationState != NodeValidationState::State::Valid) {
        float factor = (validationState == NodeValidationState::State::Error) ? 3.0f : 2.0f;
        penWidth *= factor;
    }

    bool const muted = model.nodeFlags(nodeId).testFlag(NodeFlag::Muted);
    bool const highlighted = ngo.isSelected() || ngo.nodeState().hovered();

    QPen p(color, penWidth);
    // Mute 仅在未选中/未悬停时用虚线灰边；选中与 hover 保持原样式
    if (muted && !highlighted) {
        p.setStyle(Qt::DashLine);
        p.setColor(QColor(160, 160, 160));
    }
    painter->setPen(p);


    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, size.height()));
    gradient.setColorAt(0.0, nodeStyle.GradientColor0);
    gradient.setColorAt(0.10, nodeStyle.GradientColor1);
    gradient.setColorAt(0.90, nodeStyle.GradientColor2);
    gradient.setColorAt(1.0, nodeStyle.GradientColor3);

    painter->setBrush(gradient);

    QRectF boundary(0, 0, size.width(), size.height());

    painter->drawRoundedRect(boundary, nodeStyle.BoundaryRadius, nodeStyle.BoundaryRadius);
}

void DefaultNodePainter::drawConnectionPoints(QPainter *painter,
                                              NodeGraphicsObject &ngo,
                                              NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    auto const &connectionStyle = StyleCollection::connectionStyle();

    float diameter = nodeStyle.ConnectionPointDiameter;
    auto reducedDiameter = diameter * 0.6;

    for (PortType portType : {PortType::Out, PortType::In}) {
        size_t const n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            auto const &dataType = model.portData(nodeId, portType, portIndex, PortRole::DataType)
                                       .value<NodeDataType>();

            double r = 1.0;

            NodeState const &state = ngo.nodeState();

            // 鼠标悬停的端口略微放大，便于辨认
            if (state.hoveredPortType() == portType
                && state.hoveredPortIndex() == portIndex) {
                r = 1.35;
            }

            if (auto const *cgo = state.connectionForReaction()) {
                PortType requiredPort = cgo->connectionState().requiredPort();

                if (requiredPort == portType) {
                    ConnectionId possibleConnectionId = makeCompleteConnectionId(cgo->connectionId(),
                                                                                 nodeId,
                                                                                 portIndex);

                    bool const possible = model.connectionPossible(possibleConnectionId);

                    auto cp = cgo->sceneTransform().map(cgo->endPoint(requiredPort));
                    cp = ngo.sceneTransform().inverted().map(cp);

                    auto diff = cp - p;
                    double dist = std::sqrt(QPointF::dotProduct(diff, diff));

                    if (possible) {
                        double const thres = 40.0;
                        r = (dist < thres) ? (2.0 - dist / thres) : 1.15;
                    } else {
                        double const thres = 40.0;
                        r = (dist < thres) ? (dist / thres) : 1.0;
                    }
                }
            }

            if (connectionStyle.useDataDefinedColors()) {
                painter->setBrush(connectionStyle.normalColor(dataType.id));
            } else {
                painter->setBrush(nodeStyle.ConnectionPointColor);
            }
            painter->drawEllipse(p, reducedDiameter * r, reducedDiameter * r);
        }
    }

    if (ngo.nodeState().connectionForReaction()) {
        ngo.nodeState().resetConnectionForReaction();
    }
}

void DefaultNodePainter::drawFilledConnectionPoints(QPainter *painter,
                                                    NodeGraphicsObject &ngo,
                                                    NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    auto diameter = nodeStyle.ConnectionPointDiameter;

    for (PortType portType : {PortType::Out, PortType::In}) {
        size_t const n = model
                             .nodeData(nodeId,
                                       (portType == PortType::Out) ? NodeRole::OutPortCount
                                                                   : NodeRole::InPortCount)
                             .toUInt();

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            QPointF p = geometry.portPosition(nodeId, portType, portIndex);

            auto const &connected = model.connections(nodeId, portType, portIndex);

            if (!connected.empty()) {
                auto const &dataType = model
                                           .portData(nodeId, portType, portIndex, PortRole::DataType)
                                           .value<NodeDataType>();

                auto const &connectionStyle = StyleCollection::connectionStyle();
                if (connectionStyle.useDataDefinedColors()) {
                    QColor const c = connectionStyle.normalColor(dataType.id);
                    painter->setPen(c);
                    painter->setBrush(c);
                } else {
                    painter->setPen(nodeStyle.FilledConnectionPointColor);
                    painter->setBrush(nodeStyle.FilledConnectionPointColor);
                }
                painter->drawEllipse(p, diameter * 0.4, diameter * 0.4);
            }
        }
    }
}

void DefaultNodePainter::drawNodeCaption(QPainter *painter,
                                         NodeGraphicsObject &ngo,
                                         NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (!model.nodeData(nodeId, NodeRole::CaptionVisible).toBool())
        return;

    QString const name = model.nodeData(nodeId, NodeRole::Remarks).toString();
    QFont f = painter->font();
    f.setBold(true);

    QPointF position = geometry.captionPosition(nodeId);

    auto offset=ngo.nodeState().hovered() || ngo.isSelected() ? nodeStyle.HoveredPenWidth : nodeStyle.PenWidth;
    // draw caption color
    painter->setPen(Qt::NoPen);
    painter->setBrush(nodeStyle.TitleColor);
    painter->drawRoundedRect(offset,
        offset,
        geometry.size(nodeId).width()-2*offset,
        geometry.captionPosition(nodeId).y()*2-geometry.captionRect(nodeId).height()-offset,
        nodeStyle.BoundaryRadius-offset,
        nodeStyle.BoundaryRadius-offset);

    // 编辑中由场景内 editor 显示文字，避免叠字
    if (!ngo.isEditingRemarks()) {
        painter->setFont(f);
        painter->setPen(nodeStyle.FontColor);
        painter->drawText(position, name);
    }

    f.setBold(false);
    painter->setFont(f);
}

void DefaultNodePainter::drawEntryLabels(QPainter *painter,
                                         NodeGraphicsObject &ngo,
                                         NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    for (PortType portType : {PortType::Out, PortType::In}) {
        unsigned int n = model.nodeData<unsigned int>(nodeId,
                                                      (portType == PortType::Out)
                                                          ? NodeRole::OutPortCount
                                                          : NodeRole::InPortCount);

        for (PortIndex portIndex = 0; portIndex < n; ++portIndex) {
            auto const &connected = model.connections(nodeId, portType, portIndex);

            QPointF p = geometry.portTextPosition(nodeId, portType, portIndex);

            if (connected.empty())
                painter->setPen(nodeStyle.FontColorFaded);
            else
                painter->setPen(nodeStyle.FontColor);

            QString s;

            if (model.portData<bool>(nodeId, portType, portIndex, PortRole::CaptionVisible)) {
                s = model.portData<QString>(nodeId, portType, portIndex, PortRole::Caption);
            }
            // CaptionVisible=false（收起）时不画常驻端口名，改由悬停/拖线时的 QToolTip 显示

            painter->drawText(p, s);
        }
    }
}

void DefaultNodePainter::drawResizeRect(QPainter *painter, NodeGraphicsObject &ngo) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    if (model.nodeFlags(nodeId) & NodeFlag::Resizable) {
        painter->setBrush(Qt::gray);
        painter->setPen(Qt::NoPen);
        // 获取调整手柄的矩形区域
        QRect handleRect = geometry.resizeHandleRect(nodeId);

        // 创建三角形路径（右下角三角形）
        QPolygonF triangle;
        triangle << QPointF(handleRect.left(), handleRect.bottom())
                 << QPointF(handleRect.right(), handleRect.bottom())
                 << QPointF(handleRect.right(), handleRect.top());
        
        // 绘制三角形
        painter->drawPolygon(triangle);
    }
}

void DefaultNodePainter::drawValidationIcon(QPainter *painter,
                                            NodeGraphicsObject &ngo,
                                            NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();

    QVariant var = model.nodeData(nodeId, NodeRole::ValidationState);
    if (!var.canConvert<NodeValidationState>())
        return;

    auto state = var.value<NodeValidationState>();
    if (state._state == NodeValidationState::State::Valid)
        return;

    QSize size = geometry.size(nodeId);

    QIcon icon(":/info-tooltip.svg");
    QSize iconSize(16, 16);
    QPixmap pixmap = icon.pixmap(iconSize);

    QColor color = (state._state == NodeValidationState::State::Error) ? nodeStyle.ErrorColor
                                                                       : nodeStyle.WarningColor;

    QPointF center(size.width(), 0.0);
    center += QPointF(iconSize.width() / 2.0, -iconSize.height() / 2.0);

    painter->save();

    // Draw a colored circle behind the icon to highlight validation issues
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawEllipse(center, iconSize.width() / 2.0 + 2.0, iconSize.height() / 2.0 + 2.0);


    QPainter imgPainter(&pixmap);
    imgPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    imgPainter.fillRect(pixmap.rect(), nodeStyle.FontColor);
    imgPainter.end();


    painter->drawPixmap(center.toPoint() - QPoint(iconSize.width() / 2, iconSize.height() / 2),
                        pixmap);

    painter->restore();
}

void DefaultNodePainter::drawMutedOverlay(QPainter *painter,
                                          NodeGraphicsObject &ngo,
                                          NodeStyle const &nodeStyle) const
{
    AbstractGraphModel &model = ngo.graphModel();
    NodeId const nodeId = ngo.nodeId();
    if (!model.nodeFlags(nodeId).testFlag(NodeFlag::Muted))
        return;

    AbstractNodeGeometry &geometry = ngo.nodeScene()->nodeGeometry();
    QSize size = geometry.size(nodeId);
    QRectF boundary(0, 0, size.width(), size.height());

    painter->save();
    painter->setClipPath([&] {
        QPainterPath path;
        path.addRoundedRect(boundary, nodeStyle.BoundaryRadius, nodeStyle.BoundaryRadius);
        return path;
    }());

    // Light hatch is enough; no badge (would cover remarks).
    QPen hatchPen(QColor(180, 180, 180, 90), 1.0);
    painter->setPen(hatchPen);
    qreal const step = 8.0;
    for (qreal x = -size.height(); x < size.width(); x += step) {
        painter->drawLine(QPointF(x, 0), QPointF(x + size.height(), size.height()));
    }

    painter->restore();
}

} // namespace QtNodes
