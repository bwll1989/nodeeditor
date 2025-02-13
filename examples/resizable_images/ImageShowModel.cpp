#include "ImageShowModel.hpp"

#include "PixmapData.hpp"

#include <QtNodes/NodeDelegateModelRegistry>

#include <QtCore/QDir>
#include <QtCore/QEvent>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtWidgets/QFileDialog>

ImageShowModel::ImageShowModel()
    : _label(new QLabel("Image will appear here"))
{
    _label->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    QFont f = _label->font();
    f.setBold(true);
    f.setItalic(true);

    _label->setFont(f);

    _label->setMinimumSize(200, 200);

    _label->installEventFilter(this);
}

ImageShowModel::~ImageShowModel()
{
    delete _label;
}

unsigned int ImageShowModel::nPorts(PortType portType) const
{
    unsigned int result = 1;

    switch (portType) {
    case PortType::In:
        result = 1;
        break;

    case PortType::Out:
        result = 1;

    default:
        break;
    }

    return result;
}

bool ImageShowModel::eventFilter(QObject *object, QEvent *event)
{
    if (object == _label) {
        int w = _label->width();
        int h = _label->height();

        if (event->type() == QEvent::Resize) {
            auto d = std::dynamic_pointer_cast<PixmapData>(_nodeData);
            if (d) {
                _label->setPixmap(d->pixmap().scaled(w, h, Qt::KeepAspectRatio));
            }
        }
    }

    return false;
}

NodeDataType ImageShowModel::dataType(PortType const, PortIndex const) const
{
    return PixmapData().type();
}

std::shared_ptr<NodeData> ImageShowModel::outData(PortIndex)
{
    return _nodeData;
}

void ImageShowModel::setInData(std::shared_ptr<NodeData> nodeData, PortIndex const)
{
    _nodeData = nodeData;

    if (_nodeData) {
        auto d = std::dynamic_pointer_cast<PixmapData>(_nodeData);

        int w = _label->width();
        int h = _label->height();

        _label->setPixmap(d->pixmap().scaled(w, h, Qt::KeepAspectRatio));
    } else {
        _label->setPixmap(QPixmap());
    }

    Q_EMIT dataUpdated(0);
}
