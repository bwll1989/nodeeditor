#include "ContainerDataModel.hpp"

#include "InDataModel.hpp"
#include "OutDataModel.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QDebug>
#include <QtCore/QHash>

#include <QtNodes/Definitions>
#include <QtNodes/AbstractGraphModel>

#include <algorithm>

namespace {

struct SyncGuard {
    bool &flag;
    explicit SyncGuard(bool &f)
        : flag(f)
    {
        flag = true;
    }
    ~SyncGuard() { flag = false; }
};

/// 旧版 Entrance/Export → In/Out
void migrateLegacyInterfaceTypes(QJsonObject &sceneJson)
{
    QJsonArray nodes = sceneJson.value(QStringLiteral("nodes")).toArray();
    for (int i = 0; i < nodes.size(); ++i) {
        QJsonObject node = nodes.at(i).toObject();
        QString const t = node.value(QStringLiteral("type")).toString();
        if (t == QLatin1String("Entrance"))
            node.insert(QStringLiteral("type"), QStringLiteral("In"));
        else if (t == QLatin1String("Export"))
            node.insert(QStringLiteral("type"), QStringLiteral("Out"));
        nodes.replace(i, node);
    }
    sceneJson.insert(QStringLiteral("nodes"), nodes);
}

QString firstLineRemarks(NodeDelegateModel const *model, QString const &fallback)
{
    if (!model)
        return fallback;
    QString base = model->getRemarks().trimmed();
    if (base.isEmpty())
        base = fallback;
    return base.section(QLatin1Char('\n'), 0, 0).trimmed();
}

} // namespace

std::shared_ptr<NodeDelegateModelRegistry> ContainerDataModel::s_registry;

QString ContainerDataModel::portLabelFromRemarks(NodeDelegateModel const *model,
                                                  QString const &fallback,
                                                  PortIndex localPort,
                                                  bool disambiguate,
                                                  NodeId nodeId)
{
    QString base = firstLineRemarks(model, fallback);
    if (disambiguate)
        return QStringLiteral("%1#%2.%3").arg(base).arg(nodeId).arg(localPort);
    return QStringLiteral("%1.%2").arg(base).arg(localPort);
}

ContainerDataModel::ContainerDataModel()
{
    Caption = QStringLiteral("Container");
    CaptionVisible = true;
    WidgetEmbeddable = false;
    Resizable = false;
    PortEditable = false;
    // 端口数由 syncInterfaceFromInner 按子图 In/Out 汇总；未 seed 前为 0
    InPortCount = 0;
    OutPortCount = 0;
    setRemarks(QStringLiteral("Container"));
}

ContainerDataModel::~ContainerDataModel()
{
    unbindAllExports();
    for (auto const &c : _innerModelConnections)
        QObject::disconnect(c);
    _innerModelConnections.clear();
}

void ContainerDataModel::setSharedRegistry(std::shared_ptr<NodeDelegateModelRegistry> registry)
{
    s_registry = std::move(registry);
}

std::shared_ptr<NodeDelegateModelRegistry> ContainerDataModel::sharedRegistry()
{
    return s_registry;
}

void ContainerDataModel::refreshInnerModelAlias()
{
    if (!_innerModel)
        return;

    // NodeId 尚未分配时不要写入错误路径
    if (getNodeID() == QtNodes::InvalidNodeId)
        return;

    // /dataflow/<parentAlias>/<thisNodeId>/...  — 嵌套时 parentAlias 本身已含上层 id
    QString const parent = getParentAlias().trimmed();
    QString alias;
    if (parent.isEmpty())
        alias = QString::number(getNodeID());
    else
        alias = parent + QLatin1Char('/') + QString::number(getNodeID());

    _innerModel->setModelAlias(alias);

    // 已有子节点也要跟上（尤其是再套一层 Container）
    for (NodeId id : _innerModel->allNodeIds()) {
        if (auto *m = _innerModel->delegateModel<NodeDelegateModel>(id)) {
            m->setParentAlias(alias);
            if (auto *child = dynamic_cast<ContainerDataModel *>(m))
                child->refreshInnerModelAlias();
        }
    }
}

DataFlowGraphModel &ContainerDataModel::ensureInnerModel()
{
    if (!_innerModel) {
        auto registry = s_registry;
        if (!registry)
            registry = std::make_shared<NodeDelegateModelRegistry>();
        _innerModel = std::make_unique<DataFlowGraphModel>(std::move(registry));
    }
    refreshInnerModelAlias();
    connectInnerModelSignals();
    return *_innerModel;
}

void ContainerDataModel::connectInnerModelSignals()
{
    if (!_innerModel || _innerSignalsConnected)
        return;

    auto resync = [this](NodeId) { syncInterfaceFromInner(); };

    // portsInserted/portsDeleted 不是 Q_SIGNAL；端口数变更可在返回上层时 sync
    _innerModelConnections.push_back(
        connect(_innerModel.get(), &QtNodes::AbstractGraphModel::nodeCreated, this, resync));
    _innerModelConnections.push_back(
        connect(_innerModel.get(), &QtNodes::AbstractGraphModel::nodeDeleted, this, resync));
    // 备注 / 端口数变更时刷新外壳标签（避免只能靠返回上层才更新）
    _innerModelConnections.push_back(
        connect(_innerModel.get(),
                &QtNodes::AbstractGraphModel::nodeUpdated,
                this,
                [this](NodeId id) {
                    if (!_innerModel)
                        return;
                    if (_innerModel->delegateModel<InDataModel>(id)
                        || _innerModel->delegateModel<OutDataModel>(id))
                        syncInterfaceFromInner();
                }));

    _innerSignalsConnected = true;
}

void ContainerDataModel::seedDefaultInterfaceNodes()
{
    auto &model = ensureInnerModel();
    if (!model.allNodeIds().empty()) {
        syncInterfaceFromInner();
        return;
    }

    NodeId const inId = model.addNode(QStringLiteral("In"));
    NodeId const outId = model.addNode(QStringLiteral("Out"));

    model.setNodeData(inId, QtNodes::NodeRole::Position, QPointF(50, 120));
    model.setNodeData(outId, QtNodes::NodeRole::Position, QPointF(420, 120));

    // nodeCreated 已触发 sync；再调一次保证绑定完整
    syncInterfaceFromInner();
}

std::vector<std::pair<NodeId, NodeDelegateModel *>> ContainerDataModel::sortedInNodes() const
{
    std::vector<std::pair<NodeId, NodeDelegateModel *>> result;
    if (!_innerModel)
        return result;
    for (NodeId id : _innerModel->allNodeIds()) {
        if (auto *m = _innerModel->delegateModel<InDataModel>(id))
            result.emplace_back(id, m);
    }
    std::sort(result.begin(), result.end(),
              [](auto const &a, auto const &b) { return a.first < b.first; });
    return result;
}

std::vector<std::pair<NodeId, NodeDelegateModel *>> ContainerDataModel::sortedOutNodes() const
{
    std::vector<std::pair<NodeId, NodeDelegateModel *>> result;
    if (!_innerModel)
        return result;
    for (NodeId id : _innerModel->allNodeIds()) {
        if (auto *m = _innerModel->delegateModel<OutDataModel>(id))
            result.emplace_back(id, m);
    }
    std::sort(result.begin(), result.end(),
              [](auto const &a, auto const &b) { return a.first < b.first; });
    return result;
}

void ContainerDataModel::unbindAllExports()
{
    for (auto const &c : _exportConnections)
        QObject::disconnect(c);
    _exportConnections.clear();
}

void ContainerDataModel::bindExportRelay(NodeDelegateModel *exportModel, PortIndex globalBase)
{
    if (!exportModel)
        return;

    _exportConnections.push_back(connect(
        exportModel,
        &NodeDelegateModel::dataUpdated,
        this,
        [this, exportModel, globalBase](PortIndex localPort) {
            PortIndex const globalPort = globalBase + localPort;
            _outCache[globalPort] = exportModel->outData(localPort);
            Q_EMIT dataUpdated(globalPort);
        }));
}

void ContainerDataModel::applyPortCountChange(PortType portType,
                                               unsigned int oldCount,
                                               unsigned int newCount)
{
    if (newCount == oldCount)
        return;

    if (newCount > oldCount) {
        Q_EMIT portsAboutToBeInserted(portType, oldCount, newCount - 1);
        if (portType == PortType::In)
            InPortCount = newCount;
        else
            OutPortCount = newCount;
        Q_EMIT portsInserted();
    } else {
        Q_EMIT portsAboutToBeDeleted(portType, newCount, oldCount - 1);
        if (portType == PortType::In)
            InPortCount = newCount;
        else
            OutPortCount = newCount;
        Q_EMIT portsDeleted();
    }
}

void ContainerDataModel::syncInterfaceFromInner()
{
    if (!_innerModel)
        return;

    // 必须用实例标志：嵌套 Container 时 static/thread_local 会挡住内层 sync
    if (_isSyncing)
        return;
    SyncGuard guard(_isSyncing);

    std::vector<PortMap> inMaps;
    std::vector<PortMap> outMaps;

    auto const inNodes = sortedInNodes();
    auto const outNodes = sortedOutNodes();

    // 同名备注（首行）出现多次时加 #nodeId 消歧
    QHash<QString, int> inRemarkCount;
    QHash<QString, int> outRemarkCount;
    for (auto const &[id, model] : inNodes) {
        Q_UNUSED(id);
        inRemarkCount[firstLineRemarks(model, QStringLiteral("In"))]++;
    }
    for (auto const &[id, model] : outNodes) {
        Q_UNUSED(id);
        outRemarkCount[firstLineRemarks(model, QStringLiteral("Out"))]++;
    }

    for (auto const &[id, model] : inNodes) {
        QString const base = firstLineRemarks(model, QStringLiteral("In"));
        bool const clash = inRemarkCount.value(base) > 1;
        unsigned const n = model->nPorts(PortType::Out);
        for (PortIndex p = 0; p < n; ++p) {
            PortMap m;
            m.model = model;
            m.localPort = p;
            m.label = portLabelFromRemarks(model, QStringLiteral("In"), p, clash, id);
            inMaps.push_back(m);
        }
    }

    PortIndex outBase = 0;
    struct ExportBind {
        NodeDelegateModel *model = nullptr;
        PortIndex base = 0;
    };
    std::vector<ExportBind> binds;

    for (auto const &[id, model] : outNodes) {
        QString const base = firstLineRemarks(model, QStringLiteral("Out"));
        bool const clash = outRemarkCount.value(base) > 1;
        binds.push_back({model, outBase});
        unsigned const n = model->nPorts(PortType::In);
        for (PortIndex p = 0; p < n; ++p) {
            PortMap m;
            m.model = model;
            m.localPort = p;
            m.label = portLabelFromRemarks(model, QStringLiteral("Out"), p, clash, id);
            outMaps.push_back(m);
        }
        outBase = static_cast<PortIndex>(outMaps.size());
    }

    unsigned const newIn = static_cast<unsigned>(inMaps.size());
    unsigned const newOut = static_cast<unsigned>(outMaps.size());
    unsigned const oldIn = InPortCount;
    unsigned const oldOut = OutPortCount;

    _inMaps = std::move(inMaps);
    _outMaps = std::move(outMaps);

    applyPortCountChange(PortType::In, oldIn, newIn);
    applyPortCountChange(PortType::Out, oldOut, newOut);

    if (newIn == oldIn && newOut == oldOut)
        Q_EMIT embeddedWidgetSizeUpdated();

    unbindAllExports();
    for (auto const &b : binds)
        bindExportRelay(b.model, b.base);

    for (auto it = _outCache.begin(); it != _outCache.end();) {
        if (it->first >= newOut)
            it = _outCache.erase(it);
        else
            ++it;
    }
}

QString ContainerDataModel::portCaption(PortType portType, PortIndex portIndex) const
{
    if (portType == PortType::In) {
        if (portIndex < _inMaps.size())
            return _inMaps[portIndex].label;
    } else if (portType == PortType::Out) {
        if (portIndex < _outMaps.size())
            return _outMaps[portIndex].label;
    }
    return NodeDelegateModel::portCaption(portType, portIndex);
}

QJsonObject ContainerDataModel::save() const
{
    QJsonObject modelJson = NodeDelegateModel::save();
    if (_innerModel)
        modelJson[QStringLiteral("inner-scene")] = _innerModel->save();
    else
        modelJson[QStringLiteral("inner-scene")] = QJsonObject{};
    return modelJson;
}

void ContainerDataModel::load(QJsonObject const &p)
{
    NodeDelegateModel::load(p);

    QJsonObject inner = p.value(QStringLiteral("inner-scene")).toObject();
    auto &model = ensureInnerModel();
    if (!inner.isEmpty()) {
        migrateLegacyInterfaceTypes(inner);
        try {
            model.load(inner);
        } catch (std::exception const &e) {
            qWarning() << "Container inner load failed:" << e.what();
        }
    } else {
        seedDefaultInterfaceNodes();
    }
    // load 后 NodeID/parentAlias 已就绪，再刷一层 alias（含嵌套）
    refreshInnerModelAlias();
    syncInterfaceFromInner();
}

unsigned int ContainerDataModel::nPorts(PortType portType) const
{
    switch (portType) {
    case PortType::In:
        return InPortCount;
    case PortType::Out:
        return OutPortCount;
    default:
        return 0;
    }
}

NodeDataType ContainerDataModel::dataType(PortType, PortIndex) const
{
    return DecimalData().type();
}

std::shared_ptr<NodeData> ContainerDataModel::outData(PortIndex port)
{
    auto it = _outCache.find(port);
    if (it != _outCache.end())
        return it->second;
    return nullptr;
}

void ContainerDataModel::setInData(std::shared_ptr<NodeData> data, PortIndex portIndex)
{
    if (portIndex >= _inMaps.size())
        return;
    PortMap const &m = _inMaps[portIndex];
    if (m.model)
        m.model->setInData(data, m.localPort);
}

QWidget *ContainerDataModel::embeddedWidget()
{
    return nullptr;
}
