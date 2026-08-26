#include "DefaultConnectionPainter.hpp"

#include <QtGui/QIcon>
#include <QtWidgets/QGraphicsView>

#include "AbstractGraphModel.hpp"
#include "ConnectionGraphicsObject.hpp"
#include "ConnectionState.hpp"
#include "Definitions.hpp"
#include "NodeData.hpp"
#include "StyleCollection.hpp"

namespace QtNodes {

QPainterPath DefaultConnectionPainter::cubicPath(ConnectionGraphicsObject const &connection) const
{
    QPointF const &in = connection.endPoint(PortType::In);
    QPointF const &out = connection.endPoint(PortType::Out);

    auto const c1c2 = connection.pointsC1C2();

    // cubic spline
    QPainterPath cubic(out);

    cubic.cubicTo(c1c2.first, c1c2.second, in);

    return cubic;
}

void DefaultConnectionPainter::drawSketchLine(QPainter *painter, ConnectionGraphicsObject const &cgo, QPainterPath const &cubic) const
{
    ConnectionState const &state = cgo.connectionState();

    if (state.requiresPort()) {
        auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

        QPen pen;
        pen.setWidth(static_cast<int>(connectionStyle.constructionLineWidth()));
        pen.setColor(connectionStyle.constructionColor());
        pen.setStyle(Qt::DashLine);

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
//
//        auto cubic = cubicPath(cgo);

        // cubic spline
        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::drawHoveredOrSelected(QPainter *painter, ConnectionGraphicsObject const &cgo, QPainterPath const &cubic) const
{
    bool const hovered = cgo.connectionState().hovered();
    bool const selected = cgo.isSelected();

    // drawn as a fat background
    if (hovered || selected) {
        auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

        double const haloWidth = selected ? connectionStyle.lineSelectedWidth()
                                          : connectionStyle.lineHoverWidth();

        QPen pen;
        pen.setWidthF(haloWidth);
        pen.setColor(selected ? connectionStyle.selectedHaloColor()
                              : connectionStyle.hoveredColor());

        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        // cubic spline
//        auto const cubic = cubicPath(cgo);
        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::drawNormalLine(QPainter *painter, ConnectionGraphicsObject const &cgo, QPainterPath const &cubic) const
{
    ConnectionState const &state = cgo.connectionState();

    if (state.requiresPort())
        return;

    // colors

    auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

    QColor normalColorOut = connectionStyle.normalColor();
    QColor normalColorIn = connectionStyle.normalColor();
    QColor selectedColor = connectionStyle.selectedColor();

    bool useGradientColor = false;

    AbstractGraphModel const &graphModel = cgo.graphModel();

    if (connectionStyle.useDataDefinedColors()) {
        using QtNodes::PortType;

        auto const cId = cgo.connectionId();

        auto dataTypeOut = graphModel
                               .portData(cId.outNodeId,
                                         PortType::Out,
                                         cId.outPortIndex,
                                         PortRole::DataType)
                               .value<NodeDataType>();

        auto dataTypeIn
            = graphModel.portData(cId.inNodeId, PortType::In, cId.inPortIndex, PortRole::DataType)
                  .value<NodeDataType>();

        useGradientColor = (dataTypeOut.id != dataTypeIn.id);

        normalColorOut = connectionStyle.normalColor(dataTypeOut.id);
        normalColorIn = connectionStyle.normalColor(dataTypeIn.id);
        selectedColor = normalColorOut.darker(200);
    }

    // geometry

    bool const selected = cgo.isSelected();
    double const lineWidth = selected ? connectionStyle.lineSelectedWidth()
                                      : connectionStyle.lineWidth();

    // draw normal line
    QPen p;

    p.setWidthF(lineWidth);

//    auto cubic = cubicPath(cgo);
    if (useGradientColor) {
        painter->setBrush(Qt::NoBrush);

        QColor cOut = normalColorOut;
        QColor cIn = normalColorIn;
        //         if (selected)
        //             cIn = cIn.darker(200);
        // p.setColor(cOut);
        // painter->setPen(p);
        if (selected) {
            cOut = cOut.darker(200);
            cIn = cIn.darker(200);
        }

        // unsigned int constexpr segments = 60;

        QLinearGradient linear(cubic.pointAtPercent(0.0),cubic.pointAtPercent(1.0));
        linear.setColorAt(1, cIn);
        // linear.setColorAt(0.5, Qt::black);
        linear.setColorAt(0, cOut);
        p.setBrush(linear);
        // painter->setBrush(linear);
        painter->setPen(p);
        painter->drawPath(cubic);
        // for (unsigned int i = 0ul; i < segments; ++i) {
        //     double ratioPrev = double(i) / segments;
        //     double ratio = double(i + 1) / segments;
        //
        //     if (i == segments / 2) {
        //         QColor cIn = normalColorIn;
        //         if (selected)
        //             cIn = cIn.darker(200);
        //
        //         p.setColor(cIn);
        //         painter->setPen(p);
        //     }
            // painter->drawLine(cubic.pointAtPercent(ratioPrev), cubic.pointAtPercent(ratio));
        // }

        {
            QIcon icon(":convert.png");


            QPixmap pixmap = icon.pixmap(QSize(10, 10));
            painter->drawPixmap(cubic.pointAtPercent(0.50)
                                    - QPoint(5,5),
                                pixmap);
        }
    } else {
        p.setColor(normalColorOut);

        if (selected) {
            p.setColor(selectedColor);
        }

        painter->setPen(p);
        painter->setBrush(Qt::NoBrush);

        painter->drawPath(cubic);
    }
}

void DefaultConnectionPainter::paint(QPainter *painter, ConnectionGraphicsObject const &cgo) const
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (cgo.isVirtual() && !cgo.connectionState().requiresPort()) {
        drawVirtualTags(painter, cgo);
        return;
    }

    auto cubic = cubicPath(cgo);
    drawHoveredOrSelected(painter, cgo,cubic);

    drawSketchLine(painter, cgo,cubic);

    drawNormalLine(painter, cgo,cubic);

#ifdef NODE_DEBUG_DRAWING
    debugDrawing(painter, cgo,cubic);
#endif

    // draw end points
    auto const &connectionStyle = QtNodes::StyleCollection::connectionStyle();

    double const pointDiameter = connectionStyle.pointDiameter();

    painter->setPen(connectionStyle.constructionColor());
    painter->setBrush(connectionStyle.constructionColor());
    double const pointRadius = pointDiameter / 2.0;
    if(connectionStyle.outArrow()) {
        auto out = createArrowPoly(cubic,pointRadius,pointDiameter * 1.5,false);
        painter->drawPolygon(out);
    } else {
        painter->drawEllipse(cgo.out(), pointRadius, pointRadius);
    }
    if(connectionStyle.inArrow()) {
        auto in = createArrowPoly(cubic,pointRadius,pointDiameter * 1.5,true);
        painter->drawPolygon(in);
    } else {
        painter->drawEllipse(cgo.in(), pointRadius, pointRadius);
    }
}

void DefaultConnectionPainter::drawVirtualTags(QPainter *painter,
                                               ConnectionGraphicsObject const &cgo) const
{
    auto const &connectionStyle = StyleCollection::connectionStyle();
    AbstractGraphModel const &graphModel = cgo.graphModel();
    auto const cId = cgo.connectionId();

    // Body color mirrors drawNormalLine (incl. data-defined colors).
    QColor bodyColor = connectionStyle.normalColor();
    bool useDataDefined = connectionStyle.useDataDefinedColors();
    if (useDataDefined) {
        auto dataTypeOut = graphModel
                               .portData(cId.outNodeId,
                                         PortType::Out,
                                         cId.outPortIndex,
                                         PortRole::DataType)
                               .value<NodeDataType>();
        bodyColor = connectionStyle.normalColor(dataTypeOut.id);
    }

    bool const selected = cgo.isSelected();
    bool const hovered = cgo.connectionState().hovered()
                         || cgo.isVirtualGroupHighlighted(PortType::Out)
                         || cgo.isVirtualGroupHighlighted(PortType::In);

    QColor fillColor = bodyColor;
    if (selected) {
        // Match drawNormalLine: data-defined → darker; otherwise SelectedColor.
        fillColor = useDataDefined ? bodyColor.darker(200) : connectionStyle.selectedColor();
    }

    auto drawTag = [&](PortType portType) {
        QPolygonF const poly = cgo.virtualTagPolygon(portType);
        if (poly.isEmpty())
            return;

        // Halo layer — same roles as drawHoveredOrSelected on cubic wires.
        if (selected || hovered) {
            QPen halo;
            halo.setWidthF(selected ? connectionStyle.lineSelectedWidth()
                                    : connectionStyle.lineHoverWidth());
            halo.setColor(selected ? connectionStyle.selectedHaloColor()
                                   : connectionStyle.hoveredColor());
            halo.setJoinStyle(Qt::RoundJoin);
            painter->setPen(halo);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolygon(poly);
        }

        QColor fill = fillColor;
        fill.setAlpha(selected || hovered ? 230 : 200);
        QPen border(fillColor.darker(140), connectionStyle.lineWidth() > 0
                                               ? qMax(1.0, connectionStyle.lineWidth() * 0.4)
                                               : 1.2);
        border.setJoinStyle(Qt::RoundJoin);
        painter->setPen(border);
        painter->setBrush(fill);
        painter->drawPolygon(poly);

        QRectF br = poly.boundingRect();
        bool const interlock = !cgo.isVirtualPortFolded(portType)
                               && cgo.virtualTagCount(portType) >= 2;
        qreal const tipPad = 7.0;
        qreal const pad = interlock ? (tipPad + 2.0) : 9.0;
        if (portType == PortType::Out)
            br.adjust(pad, 0, interlock ? -pad : -2, 0);
        else
            br.adjust(interlock ? pad : 2, 0, -pad, 0);

        painter->setPen(connectionStyle.fontColor());
        QFont font = painter->font();
        font.setPointSize(9);
        font.setBold(true);
        painter->setFont(font);
        if (!(cgo.isEditingLabel() && cgo.labelEditPort() == portType))
            painter->drawText(br, Qt::AlignCenter, cgo.virtualTagCaption(portType));
    };

    drawTag(PortType::Out);
    drawTag(PortType::In);

    // Ghost link between tags — use the same halo colors as wire hover/select.
    if (!selected && !hovered)
        return;
    auto const outPoly = cgo.virtualTagPolygon(PortType::Out);
    auto const inPoly = cgo.virtualTagPolygon(PortType::In);
    if (outPoly.isEmpty() || inPoly.isEmpty())
        return;

    QColor ghost = selected ? connectionStyle.selectedHaloColor() : connectionStyle.hoveredColor();
    ghost.setAlpha(140);
    painter->setPen(QPen(ghost,
                         selected ? connectionStyle.lineSelectedWidth() * 0.35
                                  : connectionStyle.lineHoverWidth() * 0.35,
                         Qt::DashLine));
    painter->drawLine(QPointF(outPoly.boundingRect().right(), outPoly.boundingRect().center().y()),
                      QPointF(inPoly.boundingRect().left(), inPoly.boundingRect().center().y()));
}

QPainterPath DefaultConnectionPainter::getPainterStroke(ConnectionGraphicsObject const &connection) const
{
    if (connection.isVirtual() && !connection.connectionState().requiresPort()) {
        QPainterPath path;
        auto const outPoly = connection.virtualTagPolygon(PortType::Out);
        auto const inPoly = connection.virtualTagPolygon(PortType::In);
        if (!outPoly.isEmpty())
            path.addPolygon(outPoly);
        if (!inPoly.isEmpty())
            path.addPolygon(inPoly);
        return path;
    }

    auto cubic = cubicPath(connection);

    QPointF const &out = connection.endPoint(PortType::Out);
    QPainterPath result(out);

    // 命中用更少采样即可；原先 20 段 + stroker 在鼠标移动时对每条连线都跑一遍
    unsigned int constexpr segments = 8;

    for (auto i = 0ul; i < segments; ++i) {
        double ratio = double(i + 1) / segments;
        result.lineTo(cubic.pointAtPercent(ratio));
    }

    // Width is in item/scene coordinates. A fixed 10 becomes tiny on screen when
    // zoomed out — scale it to keep ~14px clickable thickness.
    qreal hitWidth = 10.0;
    if (auto *sc = connection.scene()) {
        auto const views = sc->views();
        if (!views.isEmpty()) {
            qreal const scale = qAbs(views.first()->transform().m11());
            if (scale > 1e-6)
                hitWidth = 14.0 / scale;
        }
    }
    hitWidth = qBound(hitWidth, 10.0, 40.0);

    QPainterPathStroker stroker;
    stroker.setWidth(hitWidth);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);

    return stroker.createStroke(result);
}

#ifdef NODE_DEBUG_DRAWING
void DefaultConnectionPainter::debugDrawing(QPainter *painter, ConnectionGraphicsObject const &cgo,QPainterPath const & cubic)
{
    Q_UNUSED(painter);

    {
        QPointF const &in = cgo.endPoint(PortType::In);
        QPointF const &out = cgo.endPoint(PortType::Out);

        auto const points = cgo.pointsC1C2();

        painter->setPen(Qt::red);
        painter->setBrush(Qt::red);

        painter->drawLine(QLineF(out, points.first));
        painter->drawLine(QLineF(points.first, points.second));
        painter->drawLine(QLineF(points.second, in));
        painter->drawEllipse(points.first, 3, 3);
        painter->drawEllipse(points.second, 3, 3);

        painter->setBrush(Qt::NoBrush);
        painter->drawPath(debugDrawing);

    }

    {
        painter->setPen(Qt::yellow);
        painter->drawRect(cgo.boundingRect());
    }
}
#endif
    QPolygonF DefaultConnectionPainter::createArrowPoly(const QPainterPath& p, double mRadius,double arrowSize,bool drawIn) {
    float arrowStartPercentage;
    float arrowEndPercentage;

    if (drawIn) {
        arrowStartPercentage = p.percentAtLength(p.length() - mRadius - arrowSize);
        arrowEndPercentage = p.percentAtLength(p.length() - mRadius);
    }
    else {
        arrowStartPercentage = p.percentAtLength(mRadius + arrowSize);
        arrowEndPercentage = p.percentAtLength(mRadius);
    }
    QPointF headStartP = p.pointAtPercent(arrowStartPercentage);
    QPointF headEndP = p.pointAtPercent(arrowEndPercentage);
    QLineF arrowMiddleLine(headStartP, headEndP);
    QPointF normHead(arrowMiddleLine.dy(), -arrowMiddleLine.dx());
    QPointF arrowP1 = headStartP + normHead * 0.4;
    QPointF arrowP2 = headStartP - normHead * 0.4;

    QPolygonF arrowHeadEnd;
    arrowHeadEnd << headEndP << arrowP1 << arrowP2 /*<< headEndP*/;
    return arrowHeadEnd;
}
} // namespace QtNodes
