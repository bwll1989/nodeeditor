#include "PluginsManager.hpp"

#include "NodeDelegateModelRegistry.hpp"

#include <algorithm>
#include <utility>
#include <QDebug>
#include <QDir>
#include <QPluginLoader>

namespace QtNodes {

PluginsManager *PluginsManager::_instance = nullptr;

PluginsManager::PluginsManager()
{
    if (!_register)
        _register = std::make_shared<NodeDelegateModelRegistry>();
}

PluginsManager::~PluginsManager()
{
    unloadPlugins();

    if (PluginsManager::instance()) {
        delete PluginsManager::instance();
        PluginsManager::_instance = nullptr;
    }
}

PluginsManager *PluginsManager::instance()
{
    if (_instance == nullptr)
        _instance = new PluginsManager();
    return _instance;
}

std::shared_ptr<NodeDelegateModelRegistry> PluginsManager::registry()
{
    return _register;
}

void PluginsManager::loadPlugins(const QString &folderPath)
{
    QDir pluginsDir;
    if (!pluginsDir.exists(folderPath)) {
        // Created if folderPath does not exist
        pluginsDir.mkpath(folderPath);
    }
    pluginsDir.cd(folderPath);

    QFileInfoList pluginsInfo = pluginsDir.entryInfoList(QDir::Dirs | QDir::Files
                                                         | QDir::NoDotAndDotDot | QDir::Hidden);

    for (QFileInfo fileInfo : pluginsInfo) {
        if (fileInfo.isFile()) {
            loadPluginFromPath(fileInfo.absoluteFilePath());
        } else {
            loadPlugins(fileInfo.absoluteFilePath());
        }
    }
}

void PluginsManager::unloadPlugins()
{
    for (auto l : _loaders) {
        l.second->unload();
        delete l.second;
    }
    _loaders.clear();
}

PluginInterface *PluginsManager::loadPluginFromPath(const QString &filePath)
{
    // if (!QLibrary::isLibrary(filePath))
    //     return nullptr;

    QPluginLoader *loader = new QPluginLoader(filePath);

    qDebug() << loader->metaData();

    if (loader->isLoaded()) {
        PluginInterface *plugin = qobject_cast<PluginInterface *>(loader->instance());

        QPluginLoader *l = _loaders.find(plugin->name())->second;
        plugin = qobject_cast<PluginInterface *>(l->instance());

        loader->unload();
        delete loader;

        return plugin;
    }

    PluginInterface *plugin = qobject_cast<PluginInterface *>(loader->instance());
    if (plugin) {
        _loaders[plugin->name()] = loader;

        return plugin;
    } else {
        qWarning() << loader->errorString();

        delete loader;
    }

    return nullptr;
}

std::vector<PluginInterface *> PluginsManager::loadPluginFromPaths(const QStringList filePaths)
{
    std::vector<PluginInterface *> vecPlugins;
    vecPlugins.clear();
    for (auto path : filePaths) {
        vecPlugins.push_back(loadPluginFromPath(path));
    }
    return vecPlugins;
}

bool PluginsManager::unloadPluginFromPath(const QString &filePath)
{
    for (auto l : _loaders) {
        if (l.second->fileName() == filePath) {
            if (l.second->unload() == false) {
                return false;
            }
            delete l.second;
            _loaders.erase(l.first);
            return true;
        }
    }
    return false;
}

bool PluginsManager::unloadPluginFromName(const QString &pluginName)
{
    auto loaderIter = _loaders.find(pluginName);
    if (loaderIter != _loaders.end()) {
        if (loaderIter->second->unload() == false) {
            return false;
        }
        delete loaderIter->second;
        _loaders.erase(loaderIter->first);
        return true;
    }
    return false;
}

} // namespace QtNodes
