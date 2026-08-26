#include "NodeDelegateModel.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <vector>
#include <QMetaObject>
#include <QMetaProperty>

#include "StyleCollection.hpp"

class QCheckBox;

namespace QtNodes {

// 在startDrag函数之前添加OSCMessage结构体定义
struct OSCMessage {
    QString address;
    QString host;
    QString type;
    int port;
    QVariant value;
};

NodeDelegateModel::NodeDelegateModel()
    : _nodeStyle(StyleCollection::nodeStyle())
{
    // Derived classes can initialize specific style here
    
}

/**
 * @brief 析构函数
 *
 * 该类包含用于外部控制/反馈的连接，析构时保持默认行为即可。
 * 连接会随 QObject 析构自动断开。
 */
NodeDelegateModel::~NodeDelegateModel(){
    for (auto &pair : _externalBindingMapping) {
        auto &record = pair.second;

        if (record.control)
            record.control->removeEventFilter(this);

        if (record.notifyConnection)
            QObject::disconnect(record.notifyConnection);
        if (record.destroyedControlConnection)
            QObject::disconnect(record.destroyedControlConnection);
        if (record.destroyedTargetConnection)
            QObject::disconnect(record.destroyedTargetConnection);
    }
    _externalBindingMapping.clear();
}


/**
 * @brief 外部属性 NOTIFY 触发的统一回调
 *
 * 通过 sender() 找到触发对象，对已注册且开启 feedback 的属性绑定发送反馈。
 * 1. 首先获取触发该槽函数的对象指针
 * 2. 遍历所有外部命令映射，筛选出符合条件的记录：
 *    - 目标对象有效且与触发对象一致
 *    - 绑定类型为 Property
 *    - 开启了 feedback 标志
 *    - 属性名非空
 * 3. 将符合条件的记录暂存到 pending 列表，避免遍历时修改映射表
 * 4. 遍历 pending 列表，调用 stateFeedBack 发送属性当前值
 */
void NodeDelegateModel::onExternalCommandNotified()
{
    QObject *s = sender();
    if (!s)
        return;

    int const sigIndex = this->senderSignalIndex();

    struct Pending
    {
        QString oscAddress;
        QPointer<QObject> target;
        QString propertyName;
    };

    std::vector<Pending> pending;
    pending.reserve(_externalBindingMapping.size());

    for (auto const &pair : _externalBindingMapping) {
        auto const &oscAddress = pair.first;
        auto const &record = pair.second;

        if (!record.target)
            continue;
        if (record.target.data() != s)
            continue;
        if (!record.feedback)
            continue;
        if (record.member.isEmpty())
            continue;
        if (sigIndex >= 0 && record.notifySignalIndex != sigIndex)
            continue;

        pending.push_back(Pending{oscAddress, record.target, record.member});
    }

    for (auto const &p : pending) {
        if (!p.target)
            continue;
        this->stateFeedBack(p.oscAddress, p.target->property(p.propertyName.toUtf8().constData()));
    }
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

QString NodeDelegateModel::makeFullOscAddress(QString const &relative) const
{
    QString const norm = relative.startsWith(QLatin1Char('/')) ? relative
                                                               : (QLatin1Char('/') + relative);
    QString const parent = _parentAlias.trimmed();
    if (parent.isEmpty())
        return QStringLiteral("/dataflow/%1%2").arg(_nodeId).arg(norm);
    return QStringLiteral("/dataflow/%1/%2%3").arg(parent).arg(_nodeId).arg(norm);
}

bool NodeDelegateModel::eventFilter(QObject* watched, QEvent* event)
{
    // 检查 watched 是否是外部绑定中的控件
    auto it = std::find_if(_externalBindingMapping.begin(), _externalBindingMapping.end(),
        [watched](const auto &pair) { return pair.second.control.data() == watched; });

    if (it != _externalBindingMapping.end()) {

        QWidget *widget = it->second.control.data();
        if (!widget)
            return false;
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
    for (const auto &pair : _externalBindingMapping) {
        if (pair.second.control.data() == widget) {
            oscAddress = pair.first;
            break;
        }
    }
    
    if (oscAddress.isEmpty()) return;

    OSCMessage message;
    message.address = makeFullOscAddress(oscAddress);
    message.host = "127.0.0.1";
    message.port = 8991;
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(message.address);
    // 获取控件的值
    if (auto* button = qobject_cast<QAbstractButton*>(widget)) {
        message.type = "Int";
        message.value = button->isChecked();
    } else if (auto* slider = qobject_cast<QAbstractSlider*>(widget)) {
        message.type = "Float";
        message.value = slider->value();
    } else if (auto* spinBox = qobject_cast<QSpinBox*>(widget)) {
        message.type = "Float";
        message.value = spinBox->value();
    } else if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
        message.type = "String";
        message.value = lineEdit->text();
    } else if (auto* label = qobject_cast<QLabel*>(widget)) {
        message.type = "String";
        message.value = label->text();
    } else {
        message.value = QVariant();
    }

    QByteArray itemData;
    QDataStream dataStream(&itemData, QIODevice::WriteOnly);
    dataStream << message.host << message.port << message.address << message.type << message.value;

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
 * @brief 注册外部绑定（唯一入口）
 *
 * 统一存入 _externalBindingMapping：
 * - QWidget 放入 binding.control
 * - target 与 property/method 信息放入 record.target / record.binding
 */
void NodeDelegateModel::registerExternalBinding(const QString &oscAddress,
                                               QObject *target,
                                               ExternalBinding binding)
{
    if (!oscAddress.startsWith("/"))
        return;

    auto it = _externalBindingMapping.find(oscAddress);
    if (it == _externalBindingMapping.end()) {
        _externalBindingMapping[oscAddress] = ExternalBinding{};
        it = _externalBindingMapping.find(oscAddress);
    }

    auto &record = it->second;

    /* ---------- 控件绑定（仅当传入 control 时更新；nullptr 表示不修改控件绑定） ---------- */
    if (binding.control) {
        if (record.control && record.control.data() != binding.control.data())
            record.control->removeEventFilter(this);

        if (record.destroyedControlConnection)
            QObject::disconnect(record.destroyedControlConnection);
        record.destroyedControlConnection = QMetaObject::Connection{};

        record.control = binding.control;

        QWidget *c = record.control.data();
        if (c) {
            c->installEventFilter(this);
            record.destroyedControlConnection = QObject::connect(c,
                                                                &QObject::destroyed,
                                                                this,
                                                                [this, oscAddress, destroyedObj = c]() {
                auto it2 = _externalBindingMapping.find(oscAddress);
                if (it2 == _externalBindingMapping.end())
                    return;
                auto &r = it2->second;
            if (!r.control)
                return;
            if (r.control.data() != destroyedObj)
                return;

            if (r.control)
                r.control->removeEventFilter(this);
            if (r.destroyedControlConnection)
                QObject::disconnect(r.destroyedControlConnection);

            r.control = nullptr;
            r.destroyedControlConnection = QMetaObject::Connection{};

            if (!r.target)
                _externalBindingMapping.erase(it2);
            });
        }
    }

    /* ---------- 目标/命令绑定（仅当传入 target 时更新；nullptr 表示不修改命令绑定） ---------- */
    if (target) {
        if (record.notifyConnection)
            QObject::disconnect(record.notifyConnection);
        if (record.destroyedTargetConnection)
            QObject::disconnect(record.destroyedTargetConnection);

        record.notifyConnection = QMetaObject::Connection{};
        record.destroyedTargetConnection = QMetaObject::Connection{};

        record.target = target;
        record.member = std::move(binding.member);
        record.notifySignal = std::move(binding.notifySignal);
        record.feedback = binding.feedback;

        QObject *t = record.target.data();

        if (record.feedback && !record.member.isEmpty()) {

            auto const *mo = t->metaObject();
            int propIndex = mo ? mo->indexOfProperty(record.member.toUtf8().constData()) : -1;
            if (propIndex >= 0) {
                QMetaProperty prop = mo->property(propIndex);

                QByteArray notifySig;
                if (!record.notifySignal.isEmpty()) {
                    notifySig = QMetaObject::normalizedSignature(record.notifySignal.toUtf8().constData());
                } else if (prop.hasNotifySignal()) {
                    notifySig = prop.notifySignal().methodSignature();
                }

                if (!notifySig.isEmpty()) {
                    int signalIndex = -1;
                    if (!record.notifySignal.isEmpty()) {
                        signalIndex = mo->indexOfSignal(notifySig.constData());
                    } else {
                        signalIndex = prop.notifySignalIndex();
                    }

                    record.notifySignalIndex = signalIndex;

                    int const slotIndex = this->metaObject()->indexOfSlot("onExternalCommandNotified()");

                    if (signalIndex >= 0 && slotIndex >= 0) {
                        record.notifyConnection = QMetaObject::connect(t,
                                                                      signalIndex,
                                                                      this,
                                                                      slotIndex,
                                                                      Qt::AutoConnection);
                    }

                    if (!record.notifyConnection) {
                        QByteArray sig = "2" + notifySig;
                        record.notifyConnection = QObject::connect(t,
                                                                  sig.constData(),
                                                                  this,
                                                                  SLOT(onExternalCommandNotified()));
                    }

                    if (!record.notifyConnection) {
                        qDebug() << "ExternalBinding notify connect failed" << t->metaObject()->className() << notifySig;
                    }
                }
            }
        }

        record.destroyedTargetConnection = QObject::connect(t, &QObject::destroyed, this, [this, oscAddress, destroyedObj = t]() {
            auto it2 = _externalBindingMapping.find(oscAddress);
            if (it2 == _externalBindingMapping.end())
                return;
            auto &r = it2->second;
            if (!r.target)
                return;
            if (r.target.data() != destroyedObj)
                return;

            if (r.notifyConnection)
                QObject::disconnect(r.notifyConnection);
            if (r.destroyedTargetConnection)
                QObject::disconnect(r.destroyedTargetConnection);

            r.notifyConnection = QMetaObject::Connection{};
            r.destroyedTargetConnection = QMetaObject::Connection{};
            r.target = nullptr;
            r.member.clear();
            r.notifySignal.clear();
            r.feedback = true;
            r.notifySignalIndex = -1;

            if (!r.control)
                _externalBindingMapping.erase(it2);
        });
    }

    if (!record.control && !record.target)
        _externalBindingMapping.erase(it);
}

/**
 * @brief 注销外部绑定（唯一入口）
 */
void NodeDelegateModel::unregisterExternalBinding(const QString &oscAddress)
{
    if (!oscAddress.startsWith("/"))
        return;

    auto it = _externalBindingMapping.find(oscAddress);
    if (it == _externalBindingMapping.end())
        return;

    auto &record = it->second;

    if (record.control)
        record.control->removeEventFilter(this);

    if (record.notifyConnection)
        QObject::disconnect(record.notifyConnection);
    if (record.destroyedControlConnection)
        QObject::disconnect(record.destroyedControlConnection);
    if (record.destroyedTargetConnection)
        QObject::disconnect(record.destroyedTargetConnection);

    _externalBindingMapping.erase(it);
}

/**
 * 获取OSC地址对应的控件
 */
QWidget* NodeDelegateModel::getWidgetFromAddress(const QString& oscAddress) const
{
    auto it = _externalBindingMapping.find(oscAddress);
    return it != _externalBindingMapping.end() ? it->second.control.data() : nullptr;
}

/**
 * 获取OSC地址和控件的映射
 */
std::unordered_map<QString, NodeDelegateModel::ExternalBinding> NodeDelegateModel::getExternalControlAddressMapping() const
{
    std::unordered_map<QString, NodeDelegateModel::ExternalBinding> out;
    out.reserve(_externalBindingMapping.size());

    for (auto const &pair : _externalBindingMapping) {
        // if (!pair.second.control)
        //     continue;

        NodeDelegateModel::ExternalBinding copy = pair.second;
        copy.notifyConnection = QMetaObject::Connection{};
        copy.destroyedControlConnection = QMetaObject::Connection{};
        copy.destroyedTargetConnection = QMetaObject::Connection{};
        copy.notifySignalIndex = -1;

        out[pair.first] = std::move(copy);
    }

    return out;
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
void NodeDelegateModel::stateFeedBack(const QString& oscAddress,QVariant value)
{
    qDebug() << "stateFeedBack function undefined" << oscAddress << value;
}

}
// namespace QtNodes