#include "PluginsManagerDlg.hpp"

#include <QApplication>
#include <QFileDialog>
#include <QMenuBar>
#include <QObject>
#include <QVBoxLayout>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeDelegateModelRegistry>
#include <QtNodes/PluginsManager>

using QtNodes::ConnectionStyle;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::DataFlowGraphModel;
using QtNodes::GraphicsView;
using QtNodes::NodeDelegateModelRegistry;
using QtNodes::PluginsManager;

static void setStyle()
{
    ConnectionStyle::setConnectionStyle(
        R"(
          {
            "ConnectionStyle": {
              "UseDataDefinedColors": true
            }
          }
          )");
}

int main(int argc, char *argv[])
{
    qSetMessagePattern(
        "[%{time yyyyMMdd h:mm:ss.zzz}] [%{time process}] "
        "[%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}%{if-critical}C%{endif}"
        "%{if-fatal}F%{endif}]: %{message}\t| (%{function}) [%{file}:%{line}]");

    QApplication app(argc, argv);

    setStyle();

    QWidget mainWidget;

    // Load plugins and register models
    PluginsManagerDlg pluginsManagerDlg(&mainWidget);

    auto menuBar = new QMenuBar();
    QMenu *menu = menuBar->addMenu("Plugins");
    auto pluginsManagerAction = menu->addAction("Plugins Manager");
    auto pluginsFloderAction = menu->addAction("Open Plugins Folder");

    QObject::connect(pluginsManagerAction,
                     &QAction::triggered,
                     &pluginsManagerDlg,
                     &PluginsManagerDlg::exec);

    QObject::connect(pluginsFloderAction,
                     &QAction::triggered,
                     &pluginsManagerDlg,
                     &PluginsManagerDlg::openPluginsFolder);

    QVBoxLayout *l = new QVBoxLayout(&mainWidget);

    DataFlowGraphModel dataFlowGraphModel(PluginsManager::instance()->registry());

    l->addWidget(menuBar);
    auto scene = new DataFlowGraphicsScene(dataFlowGraphModel, &mainWidget);

    auto view = new GraphicsView(scene);
    l->addWidget(view);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    QObject::connect(scene, &DataFlowGraphicsScene::sceneLoaded, view, &GraphicsView::centerScene);

    mainWidget.setWindowTitle("Data Flow: Plugins Load");
    mainWidget.resize(800, 600);
    mainWidget.show();

    return app.exec();
}
