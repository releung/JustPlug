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

#include "private/pluginmanagerprivate.h"

#include "sharedlibrary.h"

#include "version/version.h"
#include "json/json.hpp"

#include "private/graph.h"
#include "private/tribool.h"
#include "private/fsutil.h"
#include "private/stringutil.h"

using namespace jp_private;
using namespace jp;

// Parse json metadata using json.hpp (in thirdparty/ folder)
/**
 * @brief 解析插件元数据
 *
 * 将给定的插件元数据解析为 PluginInfoStd 对象。
 *
 * @param metadata 插件元数据字符串
 *
 * @return 解析后的插件信息对象
 */
PluginInfoStd PlugMgrPrivate::parseMetadata(const char *metadata)
{
    try
    {
        using json = nlohmann::json;
        json tree = json::parse(metadata);

        // Check API version of the plugin
        if(Version(tree.at("api").get<std::string>()).compatible(JP_PLUGIN_API))
        {
            PluginInfoStd info;
            info.name = tree.at("name").get<std::string>();
            info.prettyName = tree.at("prettyName").get<std::string>();
            info.version = tree.at("version").get<std::string>();
            info.author = tree.at("author").get<std::string>();
            info.url = tree.at("url").get<std::string>();
            info.license = tree.at("license").get<std::string>();
            info.copyright = tree.at("copyright").get<std::string>();

            json jsonDep = tree.at("dependencies");
            for(json& jdep : jsonDep)
            {
                PluginInfoStd::Dependency dep;
                dep.name = jdep.at("name").get<std::string>();
                dep.version = jdep.at("version").get<std::string>();
                info.dependencies.push_back(dep);
            }

            return info;
        }
    }
    catch(const std::exception&) {}

    return PluginInfoStd();
}

// Checks if the dependencies required by the plugin exists and are compatible
// with the required version.
// If all dependencies match, mark the plugin as "compatible"
/**
 * @brief 检查插件依赖项
 *
 * 检查给定插件的依赖项是否存在且版本兼容，如果满足条件则返回成功状态码，否则返回相应的错误状态码。
 *
 * @param plugin 插件指针引用
 * @param callbackFunc 回调函数
 *
 * @return 返回状态码
 */
ReturnCode PlugMgrPrivate::checkDependencies(PluginPtr& plugin, PluginManager::callback callbackFunc)
{
    if(!plugin->dependenciesExists.indeterminate())
        return plugin->dependenciesExists == true ? ReturnCode::SUCCESS
                                                  : (pluginsMap.count(plugin->info.name) == 0 ? ReturnCode::LOAD_DEPENDENCY_NOT_FOUND
                                                                                              : ReturnCode::LOAD_DEPENDENCY_BAD_VERSION);

    for(size_t i=0; i < plugin->info.dependencies.size(); ++i)
    {
        const std::string& depName = plugin->info.dependencies[i].name;
        const std::string& depVer = plugin->info.dependencies[i].version;
        // Checks if the plugin dep is compatible
        if(pluginsMap.count(depName) == 0)
        {
            plugin->dependenciesExists = false;
            if(callbackFunc)
                callbackFunc(ReturnCode::LOAD_DEPENDENCY_NOT_FOUND, strdup(plugin->path.c_str()));
            return ReturnCode::LOAD_DEPENDENCY_NOT_FOUND;
        }

        if(!Version(pluginsMap[depName]->info.version).compatible(depVer))
        {
            plugin->dependenciesExists = false;
            if(callbackFunc)
                callbackFunc(ReturnCode::LOAD_DEPENDENCY_BAD_VERSION, strdup(plugin->path.c_str()));
            return ReturnCode::LOAD_DEPENDENCY_BAD_VERSION;
        }

        // Checks if the dependencies of the dependency exists
        ReturnCode retCode = checkDependencies(pluginsMap[depName], callbackFunc);
        if(!retCode)
            return retCode;
    }

    plugin->dependenciesExists = true;
    return ReturnCode::SUCCESS;
}

/**
 * @brief 按照加载顺序加载插件
 *
 * 遍历加载顺序列表，按照列表中的顺序依次加载插件。
 */
void PlugMgrPrivate::loadPluginsInOrder()
{
    for(const std::string& name : loadOrderList)
        loadPlugin(pluginsMap.at(name));
}


/**
 * @brief 加载插件
 *
 * 从给定的插件指针中加载插件，并设置其创建器函数和其他相关属性。
 *
 * @param plugin 插件指针的引用
 *
 * TODO: 应该有记录已经加载过成功/失败的标志, 提供查询和卸载使用
 */
void PlugMgrPrivate::loadPlugin(PluginPtr& plugin)
{
    plugin->creator = *(plugin->lib.get<Plugin::iplugin_create_t*>("jp_createPlugin"));

    // Get a list of dependencies names and handle request functions
    const int depNb = plugin->info.dependencies.size();
    IPlugin** depPlugins = (IPlugin**)malloc(sizeof(IPlugin*)*depNb);

    // Dependencies are already loaded, so it's safe to get the plugin object
    for(int i=0; i < depNb; ++i)
        depPlugins[i] = pluginsMap[plugin->info.dependencies[i].name]->iplugin.get();

    plugin->iplugin.reset(plugin->creator(PlugMgrPrivate::handleRequest,
                                          PlugMgrPrivate::getNonDepPlugin,
                                          depPlugins,
                                          depNb,
                                          plugin->isMainPlugin));
    plugin->iplugin->loaded();
}

/**
 * @brief 按顺序卸载插件
 *
 * 按逆序卸载插件，并返回是否全部卸载成功。
 *
 * @return 如果全部卸载成功返回 true，否则返回 false
 */
bool PlugMgrPrivate::unloadPluginsInOrder()
{
    // Unload plugins in reverse order
    bool allUnloaded = true;
    for(auto it = loadOrderList.rbegin();
        it != loadOrderList.rend(); ++it)
    {
        if(!unloadPlugin(pluginsMap[*it]))
            allUnloaded = false;
        pluginsMap.erase(*it);
    }

    // Remove remaining plugins (if they are not in the loading list)
    while(!pluginsMap.empty())
    {
        if(!unloadPlugin(pluginsMap.begin()->second))
            allUnloaded = false;
        pluginsMap.erase(pluginsMap.begin());
    }

    // Clear the locations list
    locations.clear();

    return allUnloaded;
}

// Return true if the plugin is successfully unloaded
/**
 * @brief 卸载插件
 *
 * 从插件管理器中卸载指定的插件。
 *
 * @param plugin 插件指针的引用
 *
 * @return 如果插件成功卸载则返回 true，否则返回 false
 */
bool PlugMgrPrivate::unloadPlugin(PluginPtr& plugin)
{
    if(plugin->iplugin)
    {
        plugin->iplugin->aboutToBeUnloaded();
        plugin->iplugin.reset();
    }
    plugin->lib.unload();
    const bool isLoaded = plugin->lib.isLoaded();
    plugin.reset();

    return !isLoaded;
}

// Static
/**
 * @brief 处理请求
 *
 * 根据发送者、请求代码以及数据参数，处理对应的请求，并返回处理结果。
 *
 * @param sender 发送者
 * @param code 请求代码
 * @param data 数据指针
 * @param dataSize 数据大小指针
 *
 * @return 处理结果
 */
uint16_t PlugMgrPrivate::handleRequest(const char *sender,
                                       uint16_t code,
                                       void **data,
                                       uint32_t *dataSize)
{
    PlugMgrPrivate *_p = PluginManager::instance()._p;

    if(_p->useLog)
        JP_LOG_INFO(_p->log, "Request from {}", sender);
    // All requests to the manager sent or receive data, so check here if dataSize is null
    if(!dataSize)
        return IPlugin::DATASIZE_NULL;

    switch(code)
    {
    case IPlugin::GET_APPDIRECTORY:
    {
        *data = (void*)strdup(PluginManager::appDirectory().c_str());
        *dataSize = strlen((char*)*data);
        break;
    }
    case IPlugin::GET_PLUGINAPI:
    {
        *data = (void*)strdup(PluginManager::pluginApi().c_str());
        *dataSize = strlen((char*)*data);
        break;
    }
    case IPlugin::GET_PLUGINSCOUNT:
    {
        *data = (void*)(new size_t(PluginManager::instance().pluginsCount()));
        *dataSize = 1;
        break;
    }
    case IPlugin::GET_PLUGININFO:
    {
        PluginInfo info = PluginManager::instance().pluginInfo(*data ? (const char*)*data : sender);
        if(!info.name)
            return IPlugin::NOT_FOUND;

        *data = (void*)(new PluginInfo(info));
        *dataSize = 1;
        break;
    }
    case IPlugin::GET_PLUGINVERSION:
    {
        PluginInfo info = PluginManager::instance().pluginInfo(*data ? (const char*)*data : sender);
        if(!info.name)
            return IPlugin::NOT_FOUND;

        *data = (void*)strdup(info.version);
        *dataSize = strlen((char*)*data);
        info.free();
        break;
    }
    case IPlugin::CHECK_PLUGIN:
    {
        if(PluginManager::instance().hasPlugin((const char*)*data))
            return IPlugin::RESULT_TRUE;
        return IPlugin::RESULT_FALSE;
        break;
    }
    case IPlugin::CHECK_PLUGINLOADED:
    {
        if(PluginManager::instance().isPluginLoaded((const char*)*data))
            return IPlugin::RESULT_TRUE;
        return IPlugin::RESULT_FALSE;
        break;
    }
    default:
        return IPlugin::UNKNOWN_REQUEST;
        break;
    }

    return IPlugin::SUCCESS;
}

// Static
/**
 * @brief 获取非依赖插件对象
 *
 * 根据发送者和插件名称，获取非依赖插件的插件对象指针。
 *
 * @param sender 发送者
 * @param pluginName 插件名称
 *
 * @return 插件对象指针，若未找到则返回 nullptr
 */
IPlugin* PlugMgrPrivate::getNonDepPlugin(const char* sender, const char* pluginName)
{
    PlugMgrPrivate *_p = PluginManager::instance()._p;

    if(_p->pluginsMap[std::string(sender)]->isMainPlugin)
    {
        if(_p->useLog)
            JP_LOG_INFO(_p->log, "Get plugin object of {} plugin (request from the main plugin)", pluginName);

        if(_p->pluginManager->isPluginLoaded(pluginName))
            return _p->pluginsMap[std::string(pluginName)]->iplugin.get();
    }

    return nullptr;
}
