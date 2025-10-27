#include "ModelDataBridge.hpp"
#include <QtNodes/NodeDelegateModel>
#include <QtNodes/Definitions>
#include <QtCore/QVariant>
#include <QtCore/QDebug>
#include <QtWidgets/QLineEdit>

using namespace QtNodes;

ModelDataBridge::ModelDataBridge(QObject* parent)
    : QObject(parent){}

/// 单例访问器实现
/// 作用：返回全局唯一的桥接器实例
ModelDataBridge& ModelDataBridge::instance()
{
    static ModelDataBridge s_instance;
    return s_instance;
}
/// Entrance 委托注册
/// 作用：入口模型在创建时调用，桥接器基于备注建立 dataUpdated 监听与地址映射
void ModelDataBridge::registerEntranceDelegate(NodeDelegateModel* delegate)
{
    if (!delegate) return;

    const QString remarks = getRemarks(delegate);
    if (!remarks.isEmpty()) {
        _entrances.insert(remarks, QPointer<NodeDelegateModel>(delegate));
    }


}

/// Export 委托注册
/// 作用：导出模型在创建时调用，桥接器基于备注建立输入端索引与地址监听
void ModelDataBridge::registerExportDelegate(NodeDelegateModel* delegate)
{
    if (!delegate) return;

    const QString remarks = getRemarks(delegate);

    if (!remarks.isEmpty()) {
        _exports.insert(remarks, QPointer<NodeDelegateModel>(delegate));
    }
    // 监听入口节点的输出变化，转发给同备注的所有出口
    QObject::connect(delegate,
                     &NodeDelegateModel::dataUpdated,
                     this,
                     [this, delegate](PortIndex outIdx) {
                         // 拉取当前输出数据
                         std::shared_ptr<NodeData> data = delegate->outData(outIdx);
                         const QString remarks = getRemarks(delegate);
                         if (!remarks.isEmpty() && data) {
                             // 推送到所有匹配备注的导出节点
                             pushToExports(remarks, data, outIdx);
                         }
                     });
}

/// 更新委托备注
/// 作用：当备注变化时刷新索引（先移除旧挂载，再按新备注挂载）
void ModelDataBridge::updateRemarksForDelegate(NodeDelegateModel* delegate, bool isEntrance, const QString& newRemarks)
{
    if (!delegate) return;

    if (isEntrance) {
        removeDelegateFromMap(_entrances, delegate);
        if (!newRemarks.trimmed().isEmpty()) {
            _entrances.insert(newRemarks.trimmed(), QPointer<NodeDelegateModel>(delegate));
        }
    } else {
        removeDelegateFromMap(_exports, delegate);
        if (!newRemarks.trimmed().isEmpty()) {
            _exports.insert(newRemarks.trimmed(), QPointer<NodeDelegateModel>(delegate));
        }
    }
}

/// 工具：获取委托备注文本（去除首尾空白）
QString ModelDataBridge::getRemarks(NodeDelegateModel* delegate) const
{
    return delegate ? delegate->getRemarks().trimmed() : QString();
}

/// 工具：从映射中移除某个委托（遍历所有键）
/// 作用：备注变更或委托销毁时清理映射，避免悬挂
void ModelDataBridge::removeDelegateFromMap(QMultiMap<QString, QPointer<NodeDelegateModel>>& map,
                                            NodeDelegateModel* delegate)
{
    for (auto it = map.begin(); it != map.end();) {
        if (it.value() == delegate) {
            it = map.erase(it);
        } else {
            ++it;
        }
    }
}

/// 数据转发：将入口输出数据推送到同备注的所有出口
/// 作用：在入口 dataUpdated 后调用；包含基本类型匹配与端口检查
void ModelDataBridge::pushToExports(const QString& remarks,
                                    std::shared_ptr<NodeData> data,
                                    PortIndex inPortIndex)
{
    const auto targets = _entrances.values(remarks);
    for (auto const& targetPtr : targets) {
        NodeDelegateModel* target = targetPtr.data();
        if (!target) continue;
        target->setInData(data, inPortIndex);
    }
}

QStringList ModelDataBridge::getAllEntranceRemarks() {
    QStringList remarks;
    for (auto const& pair : _entrances) {
        remarks << pair->getRemarks();
    }
    return remarks;

}

QStringList ModelDataBridge::getAllExportRemarks() {
    QStringList remarks;
    for (auto const& pair : _exports) {
        remarks << pair->getRemarks();
    }
    return remarks;
}

void ModelDataBridge::unregisterEntranceDelegate(NodeDelegateModel* delegate) {
    removeDelegateFromMap(_entrances, delegate);

}

void ModelDataBridge::unregisterExportDelegate(NodeDelegateModel* delegate) {
    removeDelegateFromMap(_exports, delegate);
}
