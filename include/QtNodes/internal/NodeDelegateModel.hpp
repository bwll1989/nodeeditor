#pragma once

#include <memory>

#include <QtWidgets/QWidget>

#include "Definitions.hpp"
#include "Export.hpp"
#include "NodeData.hpp"
#include "NodeStyle.hpp"
#include "Serializable.hpp"
#include <QDrag>
#include <QMimeData>
#include <QPixmap>
#include <QPainter>
#include <QApplication>
#include <QMouseEvent>
#include <QEvent>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
#include <QLabel>
namespace QtNodes {

class StyleCollection;

/**
 * The class wraps Node-specific data operations and propagates it to
 * the nesting DataFlowGraphModel which is a subclass of
 * AbstractGraphModel.
 * This class is the same what has been called NodeDataModel before v3.
 */
class NODE_EDITOR_PUBLIC NodeDelegateModel : public QObject, public Serializable
{
    Q_OBJECT

public:
    bool CaptionVisible=true;
    QString Caption="Default Node";
    bool WidgetEmbeddable=true;
    bool Resizable=false;
    unsigned int InPortCount=1;
    unsigned int OutPortCount=1;
    bool  PortEditable=false;
    NodeDelegateModel();

    virtual ~NodeDelegateModel() = default;

    /// It is possible to hide caption in GUI
    virtual bool captionVisible() const { return CaptionVisible; }

    /// Caption is used in GUI
    virtual QString caption() const { return Caption; };

    /// It is possible to hide port caption in GUI
    virtual bool portCaptionVisible(PortType, PortIndex) const { return !WidgetEmbeddable; }

    /// Port caption is used in GUI to label individual ports
    virtual QString portCaption(PortType portType, PortIndex portIndex) const { return dataType(portType, portIndex).name; }

    /// Name makes this model unique
    virtual QString name() const { return Caption; };

    virtual bool portEditable() const { return PortEditable; }

public:
    QJsonObject save() const override;

    void load(QJsonObject const &) override;

public:
    virtual unsigned int nPorts(PortType portType) const;

    virtual NodeDataType dataType(PortType portType, PortIndex portIndex) const = 0;

public:
    virtual ConnectionPolicy portConnectionPolicy(PortType, PortIndex) const;

    NodeStyle const &nodeStyle() const;

    void setNodeStyle(NodeStyle const &style);

public:
    virtual void setInData(std::shared_ptr<NodeData> nodeData, PortIndex const portIndex) = 0;

    virtual std::shared_ptr<NodeData> outData(PortIndex const port) = 0;

    /**
   * It is recommented to preform a lazy initialization for the
   * embedded widget and create it inside this function, not in the
   * constructor of the current model.
   *
   * Our Model Registry is able to shortly instantiate models in order
   * to call the non-static `Model::name()`. If the embedded widget is
   * allocated in the constructor but not actually embedded into some
   * QGraphicsProxyWidget, we'll gonna have a dangling pointer.
   */
    virtual QWidget *embeddedWidget() = 0;

    virtual bool widgetEmbeddable() const { return WidgetEmbeddable; }

    virtual bool resizable() const { return Resizable; }
    /**
     * 设置节点ID
     */
    void setNodeID(NodeId nodeId);
    /**
     * 获取节点ID
     */
    NodeId getNodeID() const;
    /**
     * 注册控件OSC地址和Widget指针
     */
    virtual void registerOSCControl(const QString& oscAddress, QWidget* control);
    /**
     * 注销控件OSC地址和Widget指针
     */
    virtual void unregisterOSCControl(const QString& oscAddress);
    /**
     * 获取控件OSC地址和Widget指针
     */
    virtual QWidget* getWidgetFromOSCAddress(const QString& oscAddress) const;
    
    virtual std::unordered_map<QString, QWidget*> getOscMapping() const;
    
public Q_SLOTS:

    virtual void inputConnectionCreated(ConnectionId const &) {}

    virtual void inputConnectionDeleted(ConnectionId const &) {}

    virtual void outputConnectionCreated(ConnectionId const &) {}

    virtual void outputConnectionDeleted(ConnectionId const &) {}

Q_SIGNALS:

    /// Triggers the updates in the nodes downstream.
    void dataUpdated(PortIndex const index);

    /// Triggers the propagation of the empty data downstream.
    void dataInvalidated(PortIndex const index);

    void computingStarted();

    void computingFinished();

    void embeddedWidgetSizeUpdated();

    /// Call this function before deleting the data associated with ports.
    /**
   * The function notifies the Graph Model and makes it remove and recompute the
   * affected connection addresses.
   */
    void portsAboutToBeDeleted(PortType const portType, PortIndex const first, PortIndex const last);

    /// Call this function when data and port moditications are finished.
    void portsDeleted();

    /// Call this function before inserting the data associated with ports.
    /**
   * The function notifies the Graph Model and makes it recompute the affected
   * connection addresses.
   */
    void portsAboutToBeInserted(PortType const portType,
                                PortIndex const first,
                                PortIndex const last);

    /// Call this function when data and port moditications are finished.
    void portsInserted();

protected:
    virtual bool eventFilter(QObject* watched, QEvent* event) override;
private:
    void startDrag(QWidget* widget);
    NodeStyle _nodeStyle;
    /**
     * 存储OSC地址和控件的映射
     */
    std::unordered_map<QString, QWidget*> _OscMapping;
    /**
     * 节点ID
     */
    NodeId _nodeId;
    /**
     * 拖拽起始位置
     */
    QPoint dragStartPosition;
    /**
     * 是否正在拖拽
     */
    bool isDragging = false;
};

} // namespace QtNodes
