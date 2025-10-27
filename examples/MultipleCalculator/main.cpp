#include <QFileDialog>
#include <qtabwidget.h>
#include <QtNodes/ConnectionStyle>
#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/DataFlowGraphicsScene>
#include <QtNodes/GraphicsView>
#include <QtNodes/NodeData>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QtGui/QScreen>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QVBoxLayout>
#include <utility>
#include <QtGui/QScreen>

#include "AdditionModel.hpp"
#include "DivisionModel.hpp"
#include "MultiplicationModel.hpp"
#include "NumberDisplayDataModel.hpp"
#include "NumberSourceDataModel.hpp"
#include "SubtractionModel.hpp"
#include "ExportDataModel.hpp"
#include  "EntranceDataModel.hpp"
#include "ViewsTabWidget.hpp"
using QtNodes::ConnectionStyle;
using QtNodes::DataFlowGraphicsScene;
using QtNodes::DataFlowGraphModel;
using QtNodes::GraphicsView;
using QtNodes::NodeDelegateModelRegistry;

static std::shared_ptr<NodeDelegateModelRegistry> registerDataModels()
{
    auto ret = std::make_shared<NodeDelegateModelRegistry>();
    ret->registerModel<NumberSourceDataModel>("Number","Sources");

    ret->registerModel<NumberDisplayDataModel>("Displays","Displays");

    ret->registerModel<AdditionModel>("Addition","Operators");

    ret->registerModel<SubtractionModel>("Subtraction","Operators");

    ret->registerModel<MultiplicationModel>("Multiplication","Operators");

    ret->registerModel<DivisionModel>("Division","Operators");

    ret->registerModel<ExportDataModel>("Export","Extend");

    ret->registerModel<EntranceDataModel>("Entrance","Extend");

    return ret;
}

static void setStyle()
{
    ConnectionStyle::setConnectionStyle(
        R"(
  {
    "ConnectionStyle": {
      "ConstructionColor": "gray",
      "NormalColor": "black",
      "SelectedColor": "gray",
      "SelectedHaloColor": "deepskyblue",
      "HoveredColor": "deepskyblue",

      "LineWidth": 3.0,
      "ConstructionLineWidth": 2.0,
      "PointDiameter": 10.0,

      "UseDataDefinedColors": true
    }
  }
  )");
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    setStyle();

    QWidget mainWidget;

    auto menuBar = new QMenuBar();
    QMenu *menu = menuBar->addMenu("File");

    auto saveAction = menu->addAction("Save Scene");
    saveAction->setShortcut(QKeySequence::Save);

    auto loadAction = menu->addAction("Load Scene");
    loadAction->setShortcut(QKeySequence::Open);

    auto newAction = menu->addAction("New Scene");
    newAction->setShortcut(QKeySequence::New);

    QVBoxLayout *l = new QVBoxLayout(&mainWidget);
    l->addWidget(menuBar);

    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);

    std::shared_ptr<NodeDelegateModelRegistry> registry = registerDataModels();
    auto tabs = new DataflowViewsManger();
    tabs->setDefaultRegistry(registry);
    l->addWidget(tabs);

    // 创建初始页（master）
    auto scene = tabs->addNewScene("master");

    // 保存动作：作用于当前活动页
    QObject::connect(saveAction, &QAction::triggered, tabs, [tabs, &mainWidget]() {
        QString fileName = QFileDialog::getSaveFileName(nullptr,
                                                   "Open Flow Scene",
                                                   QDir::homePath(),
                                                   "Flow Scene Files (*.flow)");

           if (!fileName.isEmpty()) {
               if (!fileName.endsWith("flow", Qt::CaseInsensitive))
                   fileName += ".flow";

               QFile file(fileName);
               if (file.open(QIODevice::WriteOnly)) {
                   file.write(QJsonDocument(tabs->save()).toJson());
               }
           }
    });

    // 新建页：按需创建独立的注册器（与原示例一致）
    QObject::connect(newAction, &QAction::triggered, tabs, [tabs, &mainWidget]() {
        auto registry1 = registerDataModels();
        auto* scene1 = tabs->addNewScene(registry1, QString("view%1").arg(tabs->count() - 1));
        if (scene1) {
            QObject::connect(scene1, &DataFlowGraphicsScene::modified, &mainWidget, [&mainWidget]() {
                mainWidget.setWindowModified(true);
            });
        }
    });

    // 加载动作：作用于当前活动页
    QObject::connect(loadAction, &QAction::triggered, tabs, [tabs]() {
        QString fileName = QFileDialog::getOpenFileName(nullptr,
                                                    "Open Flow Scene",
                                                    QDir::homePath(),
                                                    "Flow Scene Files (*.flow)");

    if (!QFileInfo::exists(fileName))
        return ;

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
        return ;



    QByteArray const wholeFile = file.readAll();

    tabs->load(QJsonDocument::fromJson(wholeFile).object());

    });

    // 初始页修改状态联动
    if (scene) {
        QObject::connect(scene, &DataFlowGraphicsScene::modified, &mainWidget, [&mainWidget]() {
            mainWidget.setWindowModified(true);
        });
    }

    mainWidget.setWindowTitle("[*]Data Flow: simplest calculator");
    mainWidget.resize(800, 600);
    // Center window.
    mainWidget.move(QApplication::primaryScreen()->availableGeometry().center()
                    - mainWidget.rect().center());
    mainWidget.showNormal();

    return app.exec();
}
