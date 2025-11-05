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
#include <qtmetamacros.h>

namespace QtNodes {

/**
 * Describes whether a node configuration is usable and defines a description message
 */
struct NodeValidationState
{
    enum class State : int {
        Valid = 0,      ///< All required inputs are present and correct.
        Warning = 1,    ///< Some inputs are missing or questionable, processing may be unreliable.
        Error = 2,      ///< Inputs or settings are invalid, preventing successful computation.
    };
    bool isValid() { return _state == State::Valid; };
    QString const message() { return _stateMessage; }
    State state() { return _state; }

    State _state{State::Valid};
    QString _stateMessage{""};
};

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
    bool WidgetEmbeddable=false;
    bool Resizable=false;
    unsigned int InPortCount=1;
    unsigned int OutPortCount=1;
    bool  PortEditable=false;
    NodeWidgetType WidgetType= NodeWidgetType::InternalWidget;
    NodeDelegateModel();

    virtual ~NodeDelegateModel() = default;

    /// It is possible to hide caption in GUI
    virtual bool captionVisible() const { return CaptionVisible; }

    /// Caption is used in GUI
    virtual QString caption() const { return Caption; };

    /// It is possible to hide port caption in GUI
    virtual bool portCaptionVisible(PortType, PortIndex) const { return !WidgetEmbeddable; }

    /// Port caption is used in GUI to label individual ports
    virtual QString portCaption(PortType portType, PortIndex portIndex) const
    {
        switch(portType)
        {
            case PortType::In:
                return "INPUT "+QString::number(portIndex);
            case PortType::Out:
                return "OUTPUT "+QString::number(portIndex);
            default:
                return "";
        }
    }

    /// Name makes this model unique
    virtual QString type() const { return Caption; };
    
    /// Validation State will default to Valid, but you can manipulate it by overriding in an inherited class
    virtual NodeValidationState validationState() const { return _nodeValidationState; }
    
    virtual bool portEditable() const { return PortEditable; }

    virtual void setEmbeddWidgetType(NodeWidgetType widgetType) {
        WidgetType = widgetType;
        embeddedWidgetSizeUpdated();
    }

    virtual NodeWidgetType getWidgetType() const { return WidgetType; }

    virtual void updateNodeState(QtNodes::NodeValidationState::State state= QtNodes::NodeValidationState::State::Valid,QString message="");
public:
    QJsonObject save() const override;

    void load(QJsonObject const &) override;

 	void setValidatonState(const NodeValidationState &validationState);

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
     * 设置父模型别名
     */
    void setParentAlias(QString alias){_parentAlias=alias;};
    /**
     * 获取父模型别名
     */
    QString getParentAlias() const{return _parentAlias;};
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
    /**
     * 获取OSC地址和控件的映射
     */
    virtual std::unordered_map<QString, QWidget*> getOscMapping() const;
    /**
     * 设置备注
     */
    virtual void setRemarks(const QString& remarks);
    /**
     * 获取备注
     */
    virtual QString getRemarks() const;

    /**
     * 注册节点OSC反馈
     */
    virtual void registerOSCFeedBack(const QString& oscAddress,QWidget* feedback);

public Q_SLOTS:

    virtual void inputConnectionCreated(ConnectionId const &) {}

    virtual void inputConnectionDeleted(ConnectionId const &) {}

    virtual void outputConnectionCreated(ConnectionId const &) {}

    virtual void outputConnectionDeleted(ConnectionId const &) {}

    virtual void stateFeedBack(const QString& oscAddress,QVariant value);
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
     * 所属数据模型别名
     */
    QString _parentAlias="Dataflow";
    /**
     * 拖拽起始位置
     */
    QPoint dragStartPosition;
    /**
     * 是否正在拖拽
     */
    bool isDragging = false;

    QString _remarks;
    
    NodeValidationState _nodeValidationState;
};

} // namespace QtNodes
