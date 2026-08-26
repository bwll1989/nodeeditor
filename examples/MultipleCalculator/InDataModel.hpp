#pragma once

#include <QtNodes/NodeDelegateModel>

#include <QtCore/QObject>
#include <unordered_map>
#include <memory>

using QtNodes::NodeData;
using QtNodes::NodeDataType;
using QtNodes::NodeDelegateModel;
using QtNodes::PortIndex;
using QtNodes::PortType;

class DecimalData;

/// 子图输入接口（TD 风格 In）：由 Container 外壳注入，再输出给内部节点。
class InDataModel : public NodeDelegateModel
{
    Q_OBJECT
public:
    InDataModel();
    ~InDataModel() override = default;

    QString caption() const override { return QStringLiteral("In"); }
    bool captionVisible() const override { return true; }
    QString type() const override { return QStringLiteral("In"); }

    QJsonObject save() const override;
    void load(QJsonObject const &p) override;

    NodeDataType dataType(PortType portType, PortIndex portIndex) const override;
    std::shared_ptr<NodeData> outData(PortIndex port) override;
    void setInData(std::shared_ptr<NodeData> data, PortIndex portIndex) override;
    QWidget *embeddedWidget() override;

private:
    std::unordered_map<PortIndex, std::shared_ptr<NodeData>> _dataMap;
};
