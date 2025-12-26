#include "MiniMapWidget.hpp"

#include <QtNodes/GraphicsView>

#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtGui/QPainter>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtCore/QEvent>

MiniMapWidget::MiniMapWidget(QtNodes::GraphicsView* view, QWidget* parent)
    : QWidget(parent)
    , _view(view)
{
    if (_view) {
        _scene = _view->scene();
        setParent(_view);
        if (_view->viewport())
            _view->viewport()->installEventFilter(this);
    }

    setFixedSize(_fixedSize);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0, 0, 0, 230));
    setPalette(pal);

    if (_view) {
        _view->installEventFilter(this);
        updateGeometryForParent();
        raise();
        show();

        QObject::connect(_view, &QtNodes::GraphicsView::scaleChanged, this, [this](double) {
            update();
        });
        if (_scene) {
            QObject::connect(_scene, &QGraphicsScene::changed, this, [this](const QList<QRectF>&) {
                update();
            });
            QObject::connect(_scene, &QGraphicsScene::sceneRectChanged, this, [this](const QRectF&) {
                update();
            });
        }
    }
}

void MiniMapWidget::updateGeometryForParent()
{
    if (!_view) return;
    QWidget* vp = _view;
    if (!vp) return;
    int x = _margin;
    int y = vp->height() - height() - _margin;
    setGeometry(x, y, width(), height());
}

QRectF MiniMapWidget::effectiveSceneBounds() const
{
    if (!_scene) return QRectF();
    QRectF itemsRect = _scene->itemsBoundingRect();
    if (itemsRect.isEmpty())
        itemsRect = _scene->sceneRect();
    itemsRect = itemsRect.adjusted(-20, -20, 20, 20);
    return itemsRect;
}

bool MiniMapWidget::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == _view) {
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
            updateGeometryForParent();
            update();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(obj, event);
    }
    if (_view && obj == _view->viewport()) {
        switch (event->type()) {
        case QEvent::Wheel:
        case QEvent::MouseMove:
        case QEvent::Paint:
        case QEvent::UpdateRequest:
        case QEvent::Scroll:
            update();
            break;
        default:
            break;
        }
        return QWidget::eventFilter(obj, event);
    }
    return QWidget::eventFilter(obj, event);
}

void MiniMapWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    if (!_view || !_scene) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QRectF targetRect(4, 4, width() - 8, height() - 8);
    QRectF sceneRect = _view->sceneRect();
    if (sceneRect.isEmpty())
        return;

    qreal scaleX = targetRect.width() / sceneRect.width();
    qreal scaleY = targetRect.height() / sceneRect.height();
    qreal s = qMin(scaleX, scaleY);

    QSizeF scaledSize(sceneRect.size().width() * s, sceneRect.size().height() * s);
    QPointF scaledTopLeft(targetRect.x() + (targetRect.width() - scaledSize.width()) / 2.0,
                          targetRect.y() + (targetRect.height() - scaledSize.height()) / 2.0);
    QRectF scaledSceneRectMini(scaledTopLeft, scaledSize);

    _scene->render(&p, scaledSceneRectMini, sceneRect, Qt::IgnoreAspectRatio);

    p.setPen(QPen(QColor(180, 180, 180), 1));
    p.drawRect(scaledSceneRectMini);

    QPolygonF mapped = _view->mapToScene(_view->viewport()->rect());
    QRectF viewInScene = mapped.boundingRect();

    QPointF offset = scaledSceneRectMini.topLeft() - (sceneRect.topLeft() * s);
    QRectF viewRectMini(viewInScene.topLeft() * s + offset,
                        viewInScene.bottomRight() * s + offset);
    viewRectMini = viewRectMini.normalized().intersected(scaledSceneRectMini);

    QColor halo(80, 150, 255, 60);
    p.setBrush(halo);
    p.setPen(QPen(QColor(80, 150, 255), 2));
    p.drawRect(viewRectMini);
}

void MiniMapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!_view) return;
        QRectF targetRect(4, 4, width() - 8, height() - 8);
        QRectF sceneRect = _view->sceneRect();
        if (sceneRect.isEmpty()) return;

        qreal scaleX = targetRect.width() / sceneRect.width();
        qreal scaleY = targetRect.height() / sceneRect.height();
        qreal s = qMin(scaleX, scaleY);

        QSizeF scaledSize(sceneRect.size().width() * s, sceneRect.size().height() * s);
        QPointF scaledTopLeft(targetRect.x() + (targetRect.width() - scaledSize.width()) / 2.0,
                              targetRect.y() + (targetRect.height() - scaledSize.height()) / 2.0);

        QPointF pos = event->pos();
        QPointF scenePos = (pos - scaledTopLeft) / s + sceneRect.topLeft();
        
        _view->centerOn(scenePos);
        update();
    }
    QWidget::mousePressEvent(event);
}

void MiniMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (!_view) return;
        QRectF targetRect(4, 4, width() - 8, height() - 8);
        QRectF sceneRect = _view->sceneRect();
        if (sceneRect.isEmpty()) return;

        qreal scaleX = targetRect.width() / sceneRect.width();
        qreal scaleY = targetRect.height() / sceneRect.height();
        qreal s = qMin(scaleX, scaleY);

        QSizeF scaledSize(sceneRect.size().width() * s, sceneRect.size().height() * s);
        QPointF scaledTopLeft(targetRect.x() + (targetRect.width() - scaledSize.width()) / 2.0,
                              targetRect.y() + (targetRect.height() - scaledSize.height()) / 2.0);

        QPointF pos = event->pos();
        QPointF scenePos = (pos - scaledTopLeft) / s + sceneRect.topLeft();
        
        _view->centerOn(scenePos);
        update();
    }
    QWidget::mouseMoveEvent(event);
}
