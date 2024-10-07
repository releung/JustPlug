/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Fabien Caylus
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "pluginmanager.h"

#include <algorithm> // for std::find
#include <unordered_map> // for std::unordered_map

#include "sharedlibrary.h"

#include "private/graph.h"
#include "private/fsutil.h"
#include "private/stringutil.h"
#include "private/plugin.h"

#include "version/version.h"

#include "private/pluginmanagerprivate.h"

using namespace jp;
using namespace jp_private;

/*****************************************************************************/
/* ReturnCode struct *********************************************************/
/*****************************************************************************/

ReturnCode::ReturnCode(): type(SUCCESS)
{}

ReturnCode::ReturnCode(const bool &val): type(static_cast<Type>((int)(!val)))
{}

ReturnCode::ReturnCode(const Type &codeType): type(codeType)
{}

ReturnCode::ReturnCode(const ReturnCode &code): type(code.type)
{}

ReturnCode::~ReturnCode()
{}

const ReturnCode& ReturnCode::operator=(const ReturnCode& code)
{
    type = code.type;
    return *this;
}

const char* ReturnCode::message() const
{
    return message(*this);
}

// Static
/**
 * @brief 返回错误码对应的消息
 *
 * 根据给定的错误码，返回对应的错误消息。
 *
 * @param code 错误码对象
 *
 * @return 错误消息字符串的指针
 */
const char* ReturnCode::message(const ReturnCode &code)
{
    switch(code.type)
    {
    case SUCCESS:
        return "Success";
        break;
    case UNKNOWN_ERROR:
        return "Unknown error";
        break;
    case SEARCH_NOTHING_FOUND:
        return "No plugins was found in that directory";
        break;
    case SEARCH_CANNOT_PARSE_METADATA:
        return "Plugins metadata cannot be parsed (maybe they are invalid ?)";
        break;
    case SEARCH_NAME_ALREADY_EXISTS:
        return "A plugin with the same name was already found";
        break;
    case SEARCH_LISTFILES_ERROR:
        return "An error occurs during the scan of the plugin dir";
        break;
    case LOAD_DEPENDENCY_BAD_VERSION:
        return "The plugin requires a dependency that's in an incorrect version";
        break;
    case LOAD_DEPENDENCY_NOT_FOUND:
        return "The plugin requires a dependency that wasn't found";
        break;
    case LOAD_DEPENDENCY_CYCLE:
        return "The dependencies graph contains a cycle, which makes impossible to load plugins";
        break;
    case UNLOAD_NOT_ALL:
        return "Not all plugins have been unloaded";
        break;
    }
    return "";
}

/*****************************************************************************/
/* PluginManager class *******************************************************/
/*****************************************************************************/

/**
 * @brief 插件管理器构造函数
 *
 * 初始化插件管理器，并创建一个新的插件管理器私有对象。
 */
PluginManager::PluginManager() : _p(new PlugMgrPrivate(this))
{
}

/**
 * @brief 析构函数
 *
 * 当PluginManager对象被销毁时，调用此析构函数来释放资源。
 * 如果插件映射表不为空，则卸载所有插件。
 * 最后删除_p指针所指向的对象。
 */
PluginManager::~PluginManager()
{
    if(!_p->pluginsMap.empty())
        unloadPlugins();
    delete _p;
}

// Static
/**
 * @brief 获取 PluginManager 实例
 *
 * 返回一个 PluginManager 类的单例实例。
 *
 * @return PluginManager 类的单例实例引用
 */
PluginManager& PluginManager::instance()
{
    static PluginManager inst;
    return inst;
}

/**
 * @brief 设置日志流
 *
 * 将给定的日志流设置为插件管理器的日志流。
 *
 * @param logStream 日志流引用
 */
// void PluginManager::setLogStream(std::ostream& logStream)
// {
//     _p->log = std::ref(logStream);
// }
void PluginManager::setLogger(std::shared_ptr<spdlog::logger> logger)
{
    if (logger) {
        _p->log = logger;
    } else {
        // 处理空指针情况，恢复默认 logger
        _p->log = spdlog::stdout_color_mt("console");
  		// 设置日志格式. 参数含义: [日志标识符] [日期] [日志级别] [线程号] [文件名 函数名:行号] [数据]
  		_p->log->set_pattern("[%n] [%Y-%m-%d %H:%M:%S.%e] [%l] [%t] [%s %!:%#]  %v");
  		_p->log->set_level(spdlog::level::debug);
    }
}

/**
 * @brief 启用或禁用日志输出
 *
 * 根据传入的布尔值启用或禁用日志输出功能。
 *
 * @param enable 是否启用日志输出
 */
void PluginManager::enableLogOutput(const bool &enable)
{
    if(!_p->useLog && enable)
        JP_LOG_INFO(_p->log, "Enable log output");
    _p->useLog = enable;
}

/**
 * @brief 禁用日志输出
 *
 * 禁用插件管理器的日志输出功能。
 */
void PluginManager::disableLogOutput()
{
    enableLogOutput(false);
}

/**
 * @brief 搜索插件
 *
 * 在指定的插件目录中搜索插件，并根据递归选项进行递归搜索。
 *
 * @param pluginDir 插件目录路径
 * @param recursive 是否递归搜索子目录
 * @param callbackFunc 回调函数，用于处理搜索过程中的事件
 *
 * @return 返回搜索结果的状态码
 */
ReturnCode PluginManager::searchForPlugins(const std::string &pluginDir, bool recursive, callback callbackFunc)
{
    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Search for plugins in {}", pluginDir);

    bool atLeastOneFound = false;
    fsutil::PathList libList;
    if(!fsutil::listLibrariesInDir(pluginDir, &libList, recursive))
    {
        // An error occured
        if(callbackFunc)
            callbackFunc(ReturnCode::SEARCH_LISTFILES_ERROR, strerror(errno));
        // Only return if no files was found
        if(libList.empty())
            return ReturnCode::SEARCH_LISTFILES_ERROR;
    }

    for(const std::string& path : libList)
    {
        PluginPtr plugin(new Plugin());
        plugin->lib.load(path);

        if(plugin->lib.isLoaded()
           && plugin->lib.hasSymbol("jp_name")
           && plugin->lib.hasSymbol("jp_metadata")
           && plugin->lib.hasSymbol("jp_createPlugin"))
        {
            // This is a JustPlug library
            if(_p->useLog)
                JP_LOG_INFO(_p->log, "Found library at: {}", path);
            plugin->path = path;
            std::string name = plugin->lib.get<const char*>("jp_name");;

            // name must be unique for each plugin
            if(_p->pluginsMap.count(name) == 1)
            {
                if(callbackFunc)
                    callbackFunc(ReturnCode::SEARCH_NAME_ALREADY_EXISTS, strdup(path.c_str()));
                plugin.reset();
                continue;
            }

            if(_p->useLog)
                JP_LOG_INFO(_p->log, "Library name: {}", name);

            PluginInfoStd info = _p->parseMetadata(plugin->lib.get<const char[]>("jp_metadata"));
            if(info.name.empty())
            {
                if(callbackFunc)
                    callbackFunc(ReturnCode::SEARCH_CANNOT_PARSE_METADATA, strdup(path.c_str()));
                plugin.reset();
                continue;
            }

            plugin->info = info;
            // Print plugin's info
            if(_p->useLog)
                JP_LOG_INFO(_p->log, "{}", info.toString());

            _p->pluginsMap[name] = plugin;
            atLeastOneFound = true;
        }
        else
        {
            plugin.reset();
        }
    }

    if(atLeastOneFound)
    {
        // Only add the location if it's not already in the list
        if(std::find(_p->locations.begin(), _p->locations.end(), pluginDir) == _p->locations.end())
            _p->locations.push_back(pluginDir);
        return ReturnCode::SUCCESS;
    }
    return ReturnCode::SEARCH_NOTHING_FOUND;
}

/**
 * @brief 搜索插件
 *
 * 在指定的插件目录下搜索插件，并调用回调函数处理每个找到的插件。
 *
 * @param pluginDir 插件目录路径
 * @param callbackFunc 回调函数，用于处理找到的插件
 *
 * @return 返回操作结果，成功返回 ReturnCode::Success，失败返回其他错误码
 */
ReturnCode PluginManager::searchForPlugins(const std::string &pluginDir, callback callbackFunc)
{
    return searchForPlugins(pluginDir, false, callbackFunc);
}

/**
 * @brief 注册主插件
 *
 * 将指定的插件名注册为主插件。如果当前没有主插件，并且插件存在，则将其注册为主插件。
 *
 * @param pluginName 插件名
 *
 * @return 返回操作结果，成功返回 ReturnCode::SUCCESS，否则返回 ReturnCode::UNKNOWN_ERROR
 */
ReturnCode PluginManager::registerMainPlugin(const std::string &pluginName)
{
    if(_p->mainPluginName.empty() && hasPlugin(pluginName))
    {
        _p->mainPluginName = pluginName;
        _p->pluginsMap[pluginName]->isMainPlugin = true;
        return ReturnCode::SUCCESS;
    }
    return ReturnCode::UNKNOWN_ERROR;
}

/**
 * @brief 加载插件
 *
 * 遍历所有插件，检查其依赖项是否已找到，并创建一个用于图排序的节点列表。
 * 注意：即使已经调用了loadPlugins()，图也会重新创建。
 *
 * @param tryToContinue 是否尝试继续加载其他插件（如果某个插件加载失败）
 * @param callbackFunc 回调函数指针
 *
 * @return 返回操作结果，成功返回`ReturnCode::SUCCESS`，失败返回对应的错误码
 */
ReturnCode PluginManager::loadPlugins(bool tryToContinue, callback callbackFunc)
{
    // First step: For each plugins, check if it's dependencies have been found
    // Also creates a node list used by the graph to sort the dependencies
    // NOTE: The graph is re-created even if loadPlugins() was already called.

    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Load plugins ...");

    Graph::NodeList nodeList;
    nodeList.reserve(_p->pluginsMap.size());

    for(auto& val : _p->pluginsMap)
    {
        // Init the ID to the default value (in case loadPlugins is called several times)
        val.second->graphId = -1;

        ReturnCode retCode = _p->checkDependencies(val.second, callbackFunc);
        if(!tryToContinue && !retCode)
        {
            // An error occured on one plugin, stop everything
            return retCode;
        }

        if(val.second->dependenciesExists == true)
        {
            Graph::Node node;
            node.name = &(val.first);
            nodeList.push_back(node);
            val.second->graphId = nodeList.size() - 1;
        }
    }

    // Fill parentNodes list for each node
    for(auto& val : _p->pluginsMap)
    {
        const int nodeId = val.second->graphId;
        if(nodeId != -1)
        {
            for(size_t i=0; i<val.second->info.dependencies.size(); ++i)
                nodeList[nodeId].parentNodes.push_back(_p->pluginsMap[val.second->info.dependencies[i].name]->graphId);
        }
    }


    // Second step: create a graph of all dependencies
    Graph graph(nodeList);

    // Third step: find the correct loading order using the topological Sort
    bool error = false;
    _p->loadOrderList = graph.topologicalSort(error);
    if(error)
    {
        // There is a cycle inside the graph
        if(callbackFunc)
            callbackFunc(ReturnCode::LOAD_DEPENDENCY_CYCLE, nullptr);
        return ReturnCode::LOAD_DEPENDENCY_CYCLE;
    }

    if(_p->useLog)
    {
        JP_LOG_INFO(_p->log, "Load order:");
        for(auto const& name : _p->loadOrderList)
            JP_LOG_INFO(_p->log, " - {}", name);
    }

    // Fourth step: load plugins
    _p->loadPluginsInOrder();

    // Call the main plugin function
    if(!_p->mainPluginName.empty())
        _p->pluginsMap.at(_p->mainPluginName)->iplugin->mainPluginExec();

    // Here, all plugins are loaded, the function can return
    return ReturnCode::SUCCESS;
}

/**
 * @brief 加载插件
 *
 * 加载插件，并调用回调函数处理加载结果。
 *
 * @param callbackFunc 回调函数指针，用于处理插件加载结果
 *
 * @return 返回加载插件的结果
 */
ReturnCode PluginManager::loadPlugins(callback callbackFunc)
{
    return loadPlugins(true, callbackFunc);
}

/**
 * @brief 卸载插件
 *
 * 卸载已加载的插件，并在卸载过程中调用回调函数。
 *
 * @param callbackFunc 回调函数指针
 *
 * @return 返回操作结果码
 *   - ReturnCode::SUCCESS：卸载成功
 *   - ReturnCode::UNLOAD_NOT_ALL：未卸载所有插件
 */
ReturnCode PluginManager::unloadPlugins(callback callbackFunc)
{
    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Unload plugins ...");

    if(!_p->unloadPluginsInOrder())
    {
        if(callbackFunc)
            callbackFunc(ReturnCode::UNLOAD_NOT_ALL, nullptr);

        return ReturnCode::UNLOAD_NOT_ALL;
    }
    if(_p->useLog)
        JP_LOG_INFO(_p->log, "All plugins have been unloaded");

    return ReturnCode::SUCCESS;
}

//
// Getters
//

// Static
/**
 * @brief 返回应用程序目录
 *
 * 返回当前应用程序所在的目录路径。
 *
 * @return 应用程序目录路径的字符串表示
 */
std::string PluginManager::appDirectory()
{
    return fsutil::appDir();
}

// Static
/**
 * @brief 插件API
 *
 * 返回插件API的字符串。
 *
 * @return 插件API的字符串
 */
std::string PluginManager::pluginApi()
{
    return JP_PLUGIN_API;
}

/**
 * @brief 获取插件数量
 *
 * 返回当前插件管理器中已注册的插件数量。
 *
 * @return 插件数量
 */
size_t PluginManager::pluginsCount() const
{
    return _p->pluginsMap.size();
}

/**
 * @brief 获取插件列表
 *
 * 返回插件管理器中所有插件的名称列表。
 *
 * @return 插件名称列表
 */
std::vector<std::string> PluginManager::pluginsList() const
{
    std::vector<std::string> nameList;
    nameList.reserve(_p->pluginsMap.size());
    for(auto const& x : _p->pluginsMap)
        nameList.push_back(x.first);
    return nameList;
}

/**
 * @brief 获取插件位置
 *
 * 返回插件的存储位置列表。
 *
 * @return 插件位置列表
 */
std::vector<std::string> PluginManager::pluginsLocation() const
{
    return _p->locations;
}

/**
 * @brief 判断插件管理器中是否存在指定名称的插件
 *
 * 在插件管理器中查找指定名称的插件，判断是否存在。
 *
 * @param name 插件名称
 *
 * @return 如果存在指定名称的插件，则返回 true；否则返回 false
 */
bool PluginManager::hasPlugin(const std::string &name) const
{
    return _p->pluginsMap.count(name) == 1;
}

/**
 * @brief 判断插件是否存在并且版本兼容
 *
 * 根据插件名称和最小版本号，判断插件是否存在并且其版本与最小版本号兼容。
 *
 * @param name 插件名称
 * @param minVersion 最小版本号
 *
 * @return 如果插件存在且版本兼容，则返回 true；否则返回 false
 */
bool PluginManager::hasPlugin(const std::string &name, const std::string &minVersion) const
{
    return hasPlugin(name) && Version(_p->pluginsMap[name]->info.version).compatible(minVersion);
}

/**
 * @brief 判断插件是否已加载
 *
 * 根据插件名称判断插件是否已加载。
 *
 * @param name 插件名称
 *
 * @return 如果插件已加载，则返回 true；否则返回 false
 */
bool PluginManager::isPluginLoaded(const std::string &name) const
{
    return hasPlugin(name) && _p->pluginsMap[name]->lib.isLoaded() && _p->pluginsMap[name]->iplugin;
}

/**
 * @brief 获取插件对象
 *
 * 根据插件名称获取对应的插件对象。
 *
 * @param name 插件名称
 *
 * @return 插件对象的共享指针，如果插件不存在则返回空指针
 */
std::shared_ptr<IPlugin> PluginManager::pluginObject(const std::string& name) const
{
    if(!hasPlugin(name))
        return std::shared_ptr<IPlugin>();

    return _p->pluginsMap[name]->iplugin;
}

/**
 * @brief 获取插件信息
 *
 * 根据插件名称获取插件的信息。
 *
 * @param name 插件名称
 *
 * @return 插件信息
 */
PluginInfo PluginManager::pluginInfo(const std::string &name) const
{
    if(!hasPlugin(name))
        return PluginInfo();
    return _p->pluginsMap[name]->info.toPluginInfo();
}


bool PluginManager::loadPlugin(const std::string& pluginName)
{
    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Loading plugin: {}", pluginName);

    auto it = _p->pluginsMap.find(pluginName);
    if (it == _p->pluginsMap.end())
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Plugin {} not found", pluginName);
        return false;
    }

    PluginPtr& plugin = it->second;
    if (plugin->lib.isLoaded())
    {
        if (_p->useLog)
            JP_LOG_INFO(_p->log, "Plugin {} is already loaded", pluginName);
        return true;
    }

    ReturnCode retCode = _p->checkDependencies(plugin, nullptr);
    if (!retCode)
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Failed to load plugin {} due to unmet dependencies", pluginName);
        return false;
    }

    _p->loadPlugin(plugin);
    if (_p->useLog)
        JP_LOG_INFO(_p->log, "Successfully loaded plugin {}", pluginName);

    return true;
}

bool PluginManager::loadPluginFromPath(const std::string& pluginPath)
{
    PluginPtr plugin(new Plugin());
    plugin->lib.load(pluginPath);

    if (!plugin->lib.isLoaded())
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Failed to load plugin from path: {}", pluginPath);
        return false;
    }

    std::string pluginName = plugin->lib.get<const char*>("jp_name");
    auto it = _p->pluginsMap.find(pluginName);
    if (it != _p->pluginsMap.end() && it->second->lib.isLoaded())
    {
        if (_p->useLog)
            JP_LOG_INFO(_p->log, "Plugin {} is already loaded", pluginName);
        return true;
    }

    _p->pluginsMap[pluginName] = plugin;
    ReturnCode retCode = _p->checkDependencies(plugin, nullptr);
    if (!retCode)
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Failed to load plugin {} due to unmet dependencies", pluginName);
        return false;
    }

    _p->loadPlugin(plugin);
    if (_p->useLog)
        JP_LOG_INFO(_p->log, "Successfully loaded plugin {} from path: {}", pluginName, pluginPath);

    return true;
}

bool PluginManager::unloadPlugin(const std::string& pluginName)
{
    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Unloading plugin: {}", pluginName);

    auto it = _p->pluginsMap.find(pluginName);
    if (it == _p->pluginsMap.end())
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Plugin {} not found", pluginName);
        return false;
    }

    PluginPtr& plugin = it->second;
    if (!plugin->lib.isLoaded())
    {
        if (_p->useLog)
            JP_LOG_INFO(_p->log, "Plugin {} is not loaded", pluginName);
        return false;
    }

    // Unload dependent plugins first
    std::vector<std::string> dependentPlugins;
    for (const auto& [name, p] : _p->pluginsMap)
    {
        for (const auto& dep : p->info.dependencies)
        {
            if (dep.name == pluginName && p->lib.isLoaded())
            {
                dependentPlugins.push_back(name);
            }
        }
    }

    for (const auto& depName : dependentPlugins)
    {
        if (!unloadPlugin(depName))
        {
            if (_p->useLog)
                JP_LOG_ERROR(_p->log, "Failed to unload dependent plugin {}", depName);
            return false;
        }
    }

    if (!_p->unloadPlugin(plugin))
    {
        if (_p->useLog)
            JP_LOG_ERROR(_p->log, "Failed to unload plugin {}", pluginName);
        return false;
    }
    _p->pluginsMap.erase(pluginName);

    if (_p->useLog)
        JP_LOG_INFO(_p->log, "Successfully unloaded plugin {}", pluginName);

    return true;
}