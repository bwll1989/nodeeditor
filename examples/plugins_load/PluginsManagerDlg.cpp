#include "PluginsManagerDlg.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableView>

#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/PluginInterface>

using QtNodes::NodeDelegateModelRegistry;
using QtNodes::PluginInterface;

PluginsManagerDlg::PluginsManagerDlg(QWidget *parent)
    : QDialog(parent)
{
    setMinimumSize(300, 250);

    _pluginsFolder.setPath(
        QDir::cleanPath(QCoreApplication::applicationDirPath() + QDir::separator() + "plugins"));

    QGridLayout *layout = new QGridLayout();
    setLayout(layout);

    QTableView *pluginTable = new QTableView();
    pluginTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pluginTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pluginTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    layout->addWidget(pluginTable, 0, 0, 1, 2);

    _model = new QStandardItemModel(pluginTable);

    _model->setColumnCount(2);
    _model->setHeaderData(0, Qt::Horizontal, "Name");
    _model->setHeaderData(1, Qt::Horizontal, "Version");
    pluginTable->setModel(_model);

    loadPluginsFromFolder();

    pluginTable->selectRow(0);

    // add button
    QPushButton *addButton = new QPushButton("+");
    layout->addWidget(addButton, 1, 0);
    connect(addButton, &QPushButton::clicked, this, [this]() {
        QString fileName
            = QFileDialog::getOpenFileName(this,
                                           tr("Load Plugin"),
                                           QCoreApplication::applicationDirPath(),
                                           tr("Node Files(*.node);;Data Files(*.data)"));

        if (!QFileInfo::exists(fileName))
            return;

        QFileInfo f(fileName);

        QFileInfo newFile(
            QDir::cleanPath(_pluginsFolder.absolutePath() + QDir::separator() + f.fileName()));
        QString const newPath = newFile.absoluteFilePath();

        if (f.absoluteFilePath() == newPath)
            return;

        // Copy to the plug-in directory
        if (!QFile::copy(f.absoluteFilePath(), newPath))
            return;

        PluginsManager *pluginsManager = PluginsManager::instance();
        auto plugin = pluginsManager->loadPluginFromPath(newPath);
        if (!plugin) {
            QFile::remove(newPath);
            return;
        }

        QStandardItem *item = new QStandardItem(plugin->name());
        item->setData(newPath);
        _model->appendRow(item);

        std::shared_ptr<NodeDelegateModelRegistry> reg = pluginsManager->registry();
        plugin->registerDataModels(reg);
    });

    // delete button
    QPushButton *deleteButton = new QPushButton("-", this);
    layout->addWidget(deleteButton, 1, 1);
    connect(deleteButton, &QPushButton::clicked, this, [this, pluginTable]() {
        QItemSelectionModel *selectionModel = pluginTable->selectionModel();

        int row = selectionModel->currentIndex().row();

        while (selectionModel->selectedRows().count() > 0) {
            auto rowIdx = selectionModel->selectedRows().first();
            row = std::min(row, rowIdx.row());

            QStandardItem *item = _model->itemFromIndex(rowIdx);

            PluginsManager *pluginsManager = PluginsManager::instance();

            // FIXME: Unload plugin successfully, but cannot delete the plugin file in windows
            if (!pluginsManager->unloadPluginFromName(item->text())
                || !QFile::remove(item->data().toString())) {
                selectionModel->select(rowIdx, QItemSelectionModel::Deselect);
                continue;
            }

            _model->removeRow(rowIdx.row());
        }

        pluginTable->selectRow(row);
    });
}

PluginsManagerDlg::~PluginsManagerDlg()
{
    //
}

void PluginsManagerDlg::openPluginsFolder()
{
    // QDesktopServices::openUrl(QUrl::fromLocalFile(_pluginsFolderPath));
    QDesktopServices::openUrl(QUrl(_pluginsFolder.absolutePath()));
}

QString PluginsManagerDlg::pluginsFolderPath() const
{
    return _pluginsFolder.absolutePath();
}

void PluginsManagerDlg::loadPluginsFromFolder()
{
    PluginsManager *pluginsManager = PluginsManager::instance();
    std::shared_ptr<NodeDelegateModelRegistry> registry = pluginsManager->registry();
    pluginsManager->loadPlugins(_pluginsFolder.absolutePath(),
                                QStringList() << "*.node"
                                              << "*.data");

    for (auto l : pluginsManager->loaders()) {
        PluginInterface *plugin = qobject_cast<PluginInterface *>(l.second->instance());
        if (!plugin)
            continue;

        QStandardItem *item = new QStandardItem(plugin->name());
        item->setData(l.second->fileName());
        _model->appendRow(item);

        plugin->registerDataModels(registry);
    }
}
