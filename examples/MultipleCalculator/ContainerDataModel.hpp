#pragma once

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QtCore/QObject>
#include <memory>
#include <unordered_map>
#include <vector>

#include "DecimalData.hpp"

using QtNodes::DataFlowGraphModel;
using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::NodeDelegateModelRegistry;
using QtNodes::NodeId;
using QtNodes::PortIndex;
using QtNodes::PortType;

/**
 * Container：外壳端口按子图全部 In/Out 节点汇总暴露。
 * 多个 In：各自 Out 端口依次映射到 Container In；
 * 多个 Out：各自 In 端口依次映射到 Container Out。
 */
class ContainerDataModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    ContainerDataModel();
    ~ContainerDataModel() override;

    static void setSharedRegistry(std::shared_ptr<NodeDelegateModelRegistry> registry);
    static std::shared_ptr<NodeDelegateModelRegistry> sharedRegistry();

    QString caption() const override { return QStringLiteral("Container"); }
    QString type() const override { return QStringLiteral("Container"); }
    bool captionVisible() const override { return true; }

    QString portCaption(PortType portType, PortIndex portIndex) const override;
    bool portCaptionVisible(PortType, PortIndex) const override { return true; }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    unsigned int nPorts(PortType portType) const override;
    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;
    QWidget *embeddedWidget() override;

    DataFlowGraphModel &ensureInnerModel();
    DataFlowGraphModel *innerModel() const { return _innerModel.get(); }

    void seedDefaultInterfaceNodes();

    /// 按当前 parentAlias + NodeId 刷新子图 modelAlias（嵌套 Container 用）
    void refreshInnerModelAlias();

    /// 按子图全部 In/Out 重新汇总外壳端口并接线
    void syncInterfaceFromInner();

private:
    struct PortMap {
        NodeDelegateModel *model = nullptr;
        PortIndex localPort = 0;
        QString label;
    };

    void connectInnerModelSignals();
    void unbindAllExports();
    void bindExportRelay(NodeDelegateModel *exportModel, PortIndex globalBase);
    void applyPortCountChange(PortType portType, unsigned int oldCount, unsigned int newCount);
    static QString portLabelFromRemarks(NodeDelegateModel const *model,
                                        QString const &fallback,
                                        PortIndex localPort,
                                        bool disambiguate,
                                        NodeId nodeId);

    std::vector<std::pair<NodeId, NodeDelegateModel *>> sortedInNodes() const;
    std::vector<std::pair<NodeId, NodeDelegateModel *>> sortedOutNodes() const;

    static std::shared_ptr<NodeDelegateModelRegistry> s_registry;

    std::unique_ptr<DataFlowGraphModel> _innerModel;
    bool _innerSignalsConnected = false;
    bool _isSyncing = false;

    std::vector<PortMap> _inMaps;
    std::vector<PortMap> _outMaps;
    std::unordered_map<PortIndex, std::shared_ptr<NodeData>> _outCache;
    std::vector<QMetaObject::Connection> _exportConnections;
    std::vector<QMetaObject::Connection> _innerModelConnections;
};
