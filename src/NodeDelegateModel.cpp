#include "NodeDelegateModel.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>

#include "StyleCollection.hpp"

class QCheckBox;

namespace QtNodes {

// 在startDrag函数之前添加OSCMessage结构体定义
struct OSCMessage {
    QString address;
    QString host;
    int port;
    QVariant value;
};

NodeDelegateModel::NodeDelegateModel()
    : _nodeStyle(StyleCollection::nodeStyle())
{
    // Derived classes can initialize specific style here
    
}

QJsonObject NodeDelegateModel::save() const
{
    QJsonObject modelJson;

    return modelJson;
}

void NodeDelegateModel::load(QJsonObject const &)
{
    //
}

void NodeDelegateModel::setValidatonState(const NodeValidationState &validationState)
{
    _nodeValidationState = validationState;
}

void NodeDelegateModel::updateNodeState(QtNodes::NodeValidationState::State state, QString message) {
    QtNodes::NodeValidationState ste;
    switch (state) {
        case QtNodes::NodeValidationState::State::Error:
            ste._state = QtNodes::NodeValidationState::State::Error;
            ste._stateMessage = message.isEmpty() ? QStringLiteral("Error") : message;
            break;
        case QtNodes::NodeValidationState::State::Warning:
            ste._state = QtNodes::NodeValidationState::State::Warning;
            ste._stateMessage = message.isEmpty() ? QStringLiteral("Warning") : message;
            break;
        case QtNodes::NodeValidationState::State::Valid:
            ste._state = QtNodes::NodeValidationState::State::Valid;
            ste._stateMessage = message.isEmpty() ? QStringLiteral("Normal") : message;
            break;
        default:
            ste._state = QtNodes::NodeValidationState::State::Valid;
            ste._stateMessage = message;
            break;
    }
    setValidatonState(ste);
}

ConnectionPolicy NodeDelegateModel::portConnectionPolicy(PortType portType, PortIndex) const
{
    auto result = ConnectionPolicy::One;
    switch (portType) {
    case PortType::In:
        result = ConnectionPolicy::One;
        break;
    case PortType::Out:
        result = ConnectionPolicy::Many;
        break;
    case PortType::None:
        break;
    }

    return result;
}

NodeStyle const &NodeDelegateModel::nodeStyle() const
{
    return _nodeStyle;
}

void NodeDelegateModel::setNodeStyle(NodeStyle const &style)
{
    _nodeStyle = style;
}


unsigned int NodeDelegateModel::nPorts(PortType portType) const
{
    switch (portType) {
        case PortType::In:
            return InPortCount;
        case PortType::Out:
            return OutPortCount;
        default:
            return 0;
        break;
    }
}

void NodeDelegateModel::setNodeID(NodeId nodeId)
{
    _nodeId = nodeId;
}

NodeId NodeDelegateModel::getNodeID() const
{
    return _nodeId;
}

bool NodeDelegateModel::eventFilter(QObject* watched, QEvent* event)
{
    // 检查watched是否是_OscMapping中的控件
    auto it = std::find_if(_OscMapping.begin(), _OscMapping.end(),
        [watched](const auto& pair) { return pair.second == watched; });
    
    if (it != _OscMapping.end()) {

        QWidget* widget = it->second;
        switch (event->type()) {
            case QEvent::MouseButtonPress: {
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton && (mouseEvent->modifiers() & Qt::ControlModifier)) {
                    dragStartPosition = mouseEvent->pos();
                    isDragging = true;
                   
                }
                break;
            }
            case QEvent::MouseMove: {
                 
                if (!isDragging) break;
               
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                if ((mouseEvent->pos() - dragStartPosition).manhattanLength() 
                    >= QApplication::startDragDistance()) {
                    
                    startDrag(widget);
                    isDragging = false;
                    return true;
                }
                break;
            }
            case QEvent::MouseButtonRelease: {
                isDragging = false;
                break;
            }
            default:
                break;
        }
    }
    return false;
}

void NodeDelegateModel::startDrag(QWidget* widget){
    // 找到对应的OSC地址
    QString oscAddress;
    for (const auto& pair : _OscMapping) {
        if (pair.second == widget) {
            oscAddress = pair.first;
            break;
        }
    }
    
    if (oscAddress.isEmpty()) return;

    OSCMessage message;
    message.address = "/dataflow/" + _parentAlias + "/" + QString::number(_nodeId) + oscAddress;
    message.host = "127.0.0.1";
    message.port = 8991;
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(message.address);
    // 获取控件的值
    if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
        message.value = button->isChecked();
    } else if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
        message.value = slider->value();
    } else if (auto* spinBox = qobject_cast<QSpinBox*>(widget)) {
        message.value = spinBox->value();
    } else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
        message.value = lineEdit->text();
    } else if (auto* label = qobject_cast<QLabel*>(widget)) {
        message.value = label->text();
    } else {
        message.value = QVariant();
    }

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << message.host << message.port << message.address << message.value;

    QMimeData* mimeData = new QMimeData;
    mimeData->setData("application/x-osc-address", itemData);

    QDrag* drag = new QDrag(widget);
    drag->setMimeData(mimeData);
    QPixmap pixmap(200, 30);
    pixmap.fill(Qt::transparent);
    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    QColor bgColor(40, 40, 40, 200);  // 半透明深灰色
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(pixmap.rect(), 5, 5);  // 圆角矩形
    // 绘制文本
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    QRect textRect = pixmap.rect().adjusted(30, 0, -8, 0);  // 图标右侧的文本区域
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, message.address);

    // 设置拖拽预览
    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width()/2, pixmap.height()/2));  // 热点在中心

    drag->exec(Qt::CopyAction);
}

/**
 * 注册OSC地址和控件
 */
void NodeDelegateModel::registerOSCControl(const QString& oscAddress, QWidget* control)
{
    // 如果oscAddress不以"/"开头，则不注册
    if (!oscAddress.startsWith("/")) return;
    // 构建完整的OSC地址，自动给OSC地址添加前缀，包括节点ID
    if (!control) return;
    // 如果已存在相同地址的映射，先移除旧的
    auto it = _OscMapping.find(oscAddress);
    if (it != _OscMapping.end()) {
        _OscMapping.erase(it);
    }
    // 添加新的映射
    control->installEventFilter(this);
    // control->setMouseTracking(true);
    _OscMapping[oscAddress] = control;
    registerOSCFeedBack(oscAddress,control);
    // 绑定控件销毁时自动注销
    QObject::connect(control, &QObject::destroyed, this, [this, oscAddress]() {
        this->unregisterOSCControl(oscAddress);
    });
}

void NodeDelegateModel::unregisterOSCControl(const QString& oscAddress)
{
    if (!oscAddress.startsWith("/")) return;
    auto it = _OscMapping.find(oscAddress);
    if (it != _OscMapping.end()) {
        _OscMapping.erase(it);
    }
}

/**
 * 获取OSC地址对应的控件
 */
QWidget* NodeDelegateModel::getWidgetFromOSCAddress(const QString& oscAddress) const
{
    auto it = _OscMapping.find(oscAddress);
    return it != _OscMapping.end() ? it->second : nullptr;
}

/**
 * 获取OSC地址和控件的映射
 */
std::unordered_map<QString, QWidget*> NodeDelegateModel::getOscMapping() const
{
    return _OscMapping;
}

void NodeDelegateModel::setRemarks(const QString& remarks){
    _remarks = remarks;
}

QString NodeDelegateModel::getRemarks() const{
    if (_remarks.isEmpty())
    {
        return type();
    }

    return _remarks;
}


void NodeDelegateModel::registerOSCFeedBack(const QString& oscAddress, QWidget* feedback)
{

    // 绑定值变化信号到 stateFeedBack
    if (auto* button = qobject_cast<QAbstractButton*>(feedback)) {
        if (button->isCheckable()){
            this->stateFeedBack(oscAddress, QVariant(button->isChecked()));
            QObject::connect(button, &QAbstractButton::toggled, this, [this, oscAddress](bool checked) {
                this->stateFeedBack(oscAddress, QVariant(checked));
            });
        }else {
            QObject::connect(button, &QAbstractButton::pressed, this, [this, oscAddress]() {
            this->stateFeedBack(oscAddress, QVariant(1));
        });
            QObject::connect(button, &QAbstractButton::released, this, [this, oscAddress]() {
            this->stateFeedBack(oscAddress, QVariant(0));
        });
        }

    } else if (auto* slider = qobject_cast<QAbstractSlider*>(feedback)) {
        this->stateFeedBack(oscAddress, QVariant(slider->value()));
        QObject::connect(slider, &QAbstractSlider::valueChanged, this, [this, oscAddress](int value) {
            this->stateFeedBack(oscAddress, QVariant(value));
        });
    } else if (auto* spinBox = qobject_cast<QSpinBox*>(feedback)) {
        this->stateFeedBack(oscAddress, QVariant(spinBox->value()));
        QObject::connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, oscAddress](int value) {
            this->stateFeedBack(oscAddress, QVariant(value));
        });
    } else if (auto* lineEdit = qobject_cast<QLineEdit*>(feedback)) {
        this->stateFeedBack(oscAddress, QVariant(lineEdit->text()));
        QObject::connect(lineEdit, &QLineEdit::textChanged, this, [this, oscAddress](const QString& text) {
            this->stateFeedBack(oscAddress, QVariant(text));
        });
    } else if (auto* comboBox = qobject_cast<QComboBox*>(feedback)) {
        this->stateFeedBack(oscAddress, QVariant(comboBox->currentIndex()));
        QObject::connect(comboBox, &QComboBox::currentIndexChanged, this, [this, oscAddress](int value) {
            this->stateFeedBack(oscAddress, QVariant(value));
        });
    } else if (auto* checkBox = qobject_cast<QCheckBox*>(feedback)) {
        this->stateFeedBack(oscAddress, QVariant(checkBox->isChecked()));
        QObject::connect(checkBox, &QCheckBox::checkStateChanged, this, [this, oscAddress](int state) {
            this->stateFeedBack(oscAddress, QVariant(state));
        });
    }
}

void NodeDelegateModel::stateFeedBack(const QString& oscAddress,QVariant value)
{
    qDebug() << "stateFeedBack function undefined" << oscAddress << value;
}
}
// namespace QtNodes