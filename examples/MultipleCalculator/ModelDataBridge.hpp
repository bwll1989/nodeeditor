#pragma once

#include <QObject>
#include <QHash>
#include <QMultiMap>
#include <QPointer>
#include <QLineEdit>
#include <memory>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/Definitions>
#include "EntranceDataModel.hpp"
#include "ExportDataModel.hpp"

class ModelDataBridge : public QObject {
    Q_OBJECT
public:
    /// 构造函数
    /// 作用：初始化桥接器对象
    explicit ModelDataBridge(QObject* parent = nullptr);

    /// 单例访问器
    /// 作用：获取全局唯一的桥接器实例
    static ModelDataBridge& instance();

    /// 委托注册：Entrance（基于备注进行映射）
    /// 作用：入口模型在创建时直接调用，桥接器建立 dataUpdated 监听与地址映射
    /// 参数：
    /// - delegate: Entrance 的 NodeDelegateModel 指针（基类）
    void registerEntranceDelegate(QtNodes::NodeDelegateModel* delegate);

    /// 委托注册：Export（基于备注进行映射）
    /// 作用：导出模型在创建时直接调用，桥接器建立地址映射
    /// 参数：
    /// - delegate: Export 的 NodeDelegateModel 指针（基类）
    void registerExportDelegate(QtNodes::NodeDelegateModel* delegate);

    /// 更新委托备注
    /// 作用：当备注文本变化时，由 Entrance/Export 通知桥接器更新映射
    /// 参数：
    /// - delegate: 对应的 NodeDelegateModel 指针
    /// - isEntrance: 是否为入口类型
    /// - newRemarks: 新备注文本
    void updateRemarksForDelegate(QtNodes::NodeDelegateModel* delegate, bool isEntrance, const QString& newRemarks);

    QStringList getAllEntranceRemarks();

    QStringList getAllExportRemarks();

    void unregisterEntranceDelegate(NodeDelegateModel* delegate);

    void unregisterExportDelegate(NodeDelegateModel* delegate);
private:
    /// 工具：从委托模型获取备注文本（去除首尾空白）
    /// 作用：用于作为映射键
    QString getRemarks(QtNodes::NodeDelegateModel* delegate) const;

    /// 工具：从映射中移除某个委托（无论当前挂在哪个键上）
    /// 作用：备注变更或销毁时清理映射
    void removeDelegateFromMap(QMultiMap<QString, QPointer<QtNodes::NodeDelegateModel>>& map,
                               QtNodes::NodeDelegateModel* delegate);

    /// 数据转发：将入口输出数据推送到同备注的所有出口
    /// 作用：在入口 dataUpdated 后调用
    void pushToExports(const QString& remarks,
                       std::shared_ptr<QtNodes::NodeData> data,
                       QtNodes::PortIndex inPortIndex = 0);

private:
    /// 入口委托的映射：备注 -> 委托（弱指针）
    QMultiMap<QString, QPointer<QtNodes::NodeDelegateModel>> _entrances;

    /// 出口委托的映射：备注 -> 委托（弱指针）
    QMultiMap<QString, QPointer<QtNodes::NodeDelegateModel>> _exports;
};