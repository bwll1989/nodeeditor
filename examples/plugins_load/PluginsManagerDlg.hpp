#pragma once

#include <QDialog>
#include <QDir>
#include <QStandardItemModel>
#include <QtNodes/PluginsManager>

using QtNodes::PluginsManager;

class PluginsManagerDlg : public QDialog
{
public:
    PluginsManagerDlg(QWidget *parent = nullptr);

    virtual ~PluginsManagerDlg();

public:
    void openPluginsFolder();

    QString pluginsFolderPath() const;

private:
    void loadPluginsFromFolder();

private:
    QDir _pluginsFolder;

    QStandardItemModel *_model = nullptr;
};
