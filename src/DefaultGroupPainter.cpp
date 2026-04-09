#include "DefaultGroupPainter.hpp"

#include <algorithm>

#include <QtGui/QIcon>
#include "GroupGraphicsObject.hpp"
#include "Definitions.hpp"
#include "StyleCollection.hpp"
#include "BasicGraphicsScene.hpp"
namespace QtNodes {

//void DefaultGroupPainter::drawHoveredOrSelected(QPainter *painter,
//                                                GroupGraphicsObject const &cgo) const
//{
//    // 绘制选中/悬停状态的特殊效果
//    bool const selected = cgo.isSelected();
//
//    QPen p;
//    p.setWidth(2);
//    p.setColor(selected ? Qt::yellow : Qt::white);
//    painter->setPen(p);
//    // 绘制外边框
//    painter->drawRoundedRect(cgo.boundingRect().adjusted(2, 2, -2, -2), 5, 5);
//}

void DefaultGroupPainter::paint(QPainter *painter, GroupGraphicsObject const &ggo) const
{
    drawGroupRect(painter, ggo);
    drawGroupCaption(painter, ggo);
    drawGroupPorts(painter, ggo);
}
void DefaultGroupPainter::drawGroupRect(QPainter *painter, GroupGraphicsObject const &ggo) const
{
    auto const rect = ggo.contentRect();

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();

     auto color = ggo.isSelected() ? groupStyle.SelectedColor : groupStyle.NormalColor;

     if (ggo.isUnderMouse()) {
         QPen p(color, groupStyle.HoveredPenWidth);
         painter->setPen(p);
     } else {
         QPen p(color, groupStyle.PenWidth);
         painter->setPen(p);
     }

    QRectF boundary(0, 0, rect.width(), rect.height());

    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, rect.height()));

    gradient.setColorAt(0.0, groupStyle.GradientColor0);
    gradient.setColorAt(0.10, groupStyle.GradientColor1);
    gradient.setColorAt(0.90, groupStyle.GradientColor2);
    gradient.setColorAt(1.0, groupStyle.GradientColor3);

    painter->setBrush(gradient);
    painter->drawRoundedRect(boundary, groupStyle.BoundaryRadius, groupStyle.BoundaryRadius);

}

void DefaultGroupPainter::drawGroupCaption(QPainter *painter, GroupGraphicsObject const &ggo) const
{

    QFont f = painter->font();
    f.setBold(true);
    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();
    QRectF captionRect = QRectF(0, 0, ggo.contentRect().width(), groupStyle.CaptionHeight);
    painter->setBrush(groupStyle.CaptionColor);

    painter->drawRoundedRect(captionRect,
                             2.0,
                             2.0);
    painter->setFont(f);
    painter->setPen(groupStyle.FontColor);
    QRectF textRect = captionRect.adjusted(6, 5, -6, -5);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, ggo.remarks());
    f.setBold(false);
    painter->setFont(f);


}

void DefaultGroupPainter::drawGroupPorts(QPainter *painter, GroupGraphicsObject const &ggo) const
{
    if (!ggo.isCollapsed())
        return;

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();
    auto const &nodeStyle = QtNodes::StyleCollection::nodeStyle();

    qreal const w = ggo.contentRect().width();
    qreal const collapsedHeight = (groupStyle.CollapsedHeight > 0 ? groupStyle.CollapsedHeight : groupStyle.CaptionHeight);

    bool hasIn = false;
    bool hasOut = false;

    auto const &gid = ggo.groupId();
    auto &gm = ggo.graphModel();

    auto inGroup = [&gid](NodeId nid) {
        return std::find(gid.nodeIds.begin(), gid.nodeIds.end(), nid) != gid.nodeIds.end();
    };

    for (auto nid : gid.nodeIds) {
        for (auto const &cid : gm.allConnectionIds(nid)) {
            bool aIn = (cid.inNodeId == nid) && !inGroup(cid.outNodeId);
            bool aOut = (cid.outNodeId == nid) && !inGroup(cid.inNodeId);
            hasIn = hasIn || aIn;
            hasOut = hasOut || aOut;
            if (hasIn && hasOut) break;
        }
        if (hasIn && hasOut) break;
    }

    painter->save();

    QPen dividerPen(groupStyle.FontColor, 1.0);
    dividerPen.setCosmetic(true);
    QColor dividerColor = groupStyle.FontColor;
    dividerColor.setAlphaF(0.35);
    dividerPen.setColor(dividerColor);
    painter->setPen(dividerPen);
    painter->drawLine(QPointF(0.0, groupStyle.CaptionHeight), QPointF(w, groupStyle.CaptionHeight));

    qreal const bodyHeight = std::max<qreal>(0.0, ggo.contentRect().height() - groupStyle.CaptionHeight);

    float ratio = groupStyle.PortHeight;
    if (ratio <= 0.0f) ratio = 0.6f;
    if (ratio > 1.0f) ratio = 1.0f;

    float barWidth = groupStyle.PortWidth;
    if (barWidth <= 0.0f) barWidth = nodeStyle.ConnectionPointDiameter;

    qreal const barHeight = bodyHeight * ratio;
    qreal const barY = groupStyle.CaptionHeight + (bodyHeight - barHeight) * 0.5;

    qreal const halfW = barWidth * 0.5;

    QRectF inBar(-halfW, barY, barWidth, barHeight);
    QRectF outBar(w - halfW, barY, barWidth, barHeight);

    auto const borderColor = ggo.isSelected() ? groupStyle.SelectedColor : groupStyle.NormalColor;
    qreal const borderWidth = ggo.isUnderMouse() ? groupStyle.HoveredPenWidth : groupStyle.PenWidth;

    QPen portPen(borderColor, borderWidth);
    portPen.setJoinStyle(Qt::MiterJoin);
    portPen.setCapStyle(Qt::SquareCap);

    painter->setPen(portPen);
    painter->setBrush(groupStyle.PortColor);

    if (hasIn) {
        painter->drawRoundedRect(inBar, 2.0, 2.0);
    }
    if (hasOut) {
        painter->drawRoundedRect(outBar, 2.0, 2.0);
    }

    painter->restore();
}
}