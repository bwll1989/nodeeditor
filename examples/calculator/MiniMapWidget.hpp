#pragma once

#include <QtWidgets/QWidget>
#include <QtCore/QPointer>
#include <QGraphicsScene>
#include <QGraphicsView>
namespace QtNodes {
class GraphicsView;
}

class MiniMapWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MiniMapWidget(QtNodes::GraphicsView* view, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    QPointer<QtNodes::GraphicsView> _view;
    QPointer<QGraphicsScene> _scene;
    int _margin = 0;
    QSize _fixedSize = QSize(150, 150);

    void updateGeometryForParent();
    QRectF effectiveSceneBounds() const;
};

