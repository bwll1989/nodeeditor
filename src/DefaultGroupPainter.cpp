#include "DefaultGroupPainter.hpp"

#include <QtGui/QIcon>
#include "GroupGraphicsObject.hpp"
#include "Definitions.hpp"
#include "StyleCollection.hpp"

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
}
void DefaultGroupPainter::drawGroupRect(QPainter *painter, GroupGraphicsObject const &ggo) const
{
    ggo.boundingRect();

    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();

    auto color = ggo.isSelected() ? groupStyle.SelectedColor : groupStyle.NormalColor;

    auto linwidth=ggo.isSelected()? groupStyle.LineWidth : groupStyle.ConstructionLineWidth;
    QPen p(color, linwidth);
    painter->setPen(p);
    QRectF boundary(0, 0, ggo.boundingRect().width(), ggo.boundingRect().height());
    double const radius = 2.0;
    QLinearGradient gradient(QPointF(0.0, 0.0), QPointF(2.0, ggo.boundingRect().height()));

    gradient.setColorAt(0.0, groupStyle.GradientColor0);
    gradient.setColorAt(0.10, groupStyle.GradientColor1);
    gradient.setColorAt(0.90, groupStyle.GradientColor2);
    gradient.setColorAt(1.0, groupStyle.GradientColor3);

    painter->setBrush(gradient);
    painter->drawRoundedRect(boundary, radius, radius);

}

void DefaultGroupPainter::drawGroupCaption(QPainter *painter, GroupGraphicsObject const &ggo) const
{

    QFont f = painter->font();
    f.setBold(true);
    auto const &groupStyle = QtNodes::StyleCollection::groupStyle();
    QRectF captionRect = QRectF(0, 0, ggo.boundingRect().width(), groupStyle.CaptionHeight);
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
}