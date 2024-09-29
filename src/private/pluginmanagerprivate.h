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

#ifndef PLUGINMANAGERPRIVATE_H
#define PLUGINMANAGERPRIVATE_H

/*
 * This file is an internal header. It's not part of the public API,
 * and may change at any moment.
 */

#include <iostream> // for std::cout
#include <unordered_map> // for std::unordered_map
#include <vector> // for std::vector

#include "plugin.h"

#include "pluginmanager.h"

namespace jp_private
{

// Private implementation class for PluginManager class
// (used to ensure ABI compatibility if implementation changes)
struct PlugMgrPrivate
{
    PlugMgrPrivate(jp::PluginManager* plugMgr): pluginManager(plugMgr) {

        // 默认使用 console logger
        log = spdlog::stdout_color_mt("console");
        log->set_level(spdlog::level::info);
        // log->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%@,%!][%l] : %v"); // %@ 是绝对路径
        log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] [%s:%#] [%l] : %v");

    }
    ~PlugMgrPrivate() {}

    jp::PluginManager* pluginManager;

    std::unordered_map<std::string, PluginPtr> pluginsMap;

    // Contains the last load order used
    std::vector<std::string> loadOrderList;

    // List all locations to load plugins
    std::vector<std::string> locations;

    // Stream used to print log outputs.
    // (default set to std::cout)
    // std::reference_wrapper<std::ostream> log = std::ref(std::cout);
    std::shared_ptr<spdlog::logger> log;

    bool useLog = true; // log output is enable by default

    std::string mainPluginName;

    //
    // Functions

    PluginInfoStd parseMetadata(const char* metadata);
    jp::ReturnCode checkDependencies(PluginPtr& plugin, jp::PluginManager::callback callbackFunc);

    // Simply load all plugins in the order specified by loadOrderList
    // Called by PluginManager::loadPlugins()
    void loadPluginsInOrder();
    // No checks is performed for the dependencies, they MUST be loaded
    void loadPlugin(PluginPtr& plugin);

    // Like loadPluginsInOrder, but for the unload step
    bool unloadPluginsInOrder();
    bool unloadPlugin(PluginPtr &plugin);

    // Function called by plugins throught IPlugin::sendRequest()
    static uint16_t handleRequest(const char* sender, uint16_t code, void** data, uint32_t *dataSize);
    // Return nullptr if sender is not the main plugin or if pluginName is not loaded
    static jp::IPlugin* getNonDepPlugin(const char* sender, const char* pluginName);
};

} // namespace jp_private

#endif // PLUGINMANAGERPRIVATE_H
