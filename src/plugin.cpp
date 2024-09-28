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

#include "private/plugin.h"

#include "private/stringutil.h"

#include <algorithm> // for std::copy

using namespace jp_private;

/*****************************************************************************/
/***** PluginInfoStd class ***************************************************/
/*****************************************************************************/

/**
 * @brief 将 PluginInfoStd 转换为 PluginInfo
 *
 * 将 PluginInfoStd 对象转换为 jp::PluginInfo 对象，并返回转换后的结果。
 *
 * @return 转换后的 jp::PluginInfo 对象
 */
jp::PluginInfo PluginInfoStd::toPluginInfo()
{
    jp::PluginInfo info;
    info.name = strdup(name.c_str());
    info.prettyName = strdup(prettyName.c_str());
    info.version = strdup(version.c_str());
    info.author = strdup(author.c_str());
    info.url = strdup(url.c_str());
    info.license = strdup(license.c_str());
    info.copyright = strdup(copyright.c_str());

    // Convert std::vector to C-style array used by PluginInfo
    // with jp::Dependency objects (not PluginInfoStd::Dependency)
    std::vector<jp::Dependency> depList;
    depList.reserve(dependencies.size());
    for(const Dependency& dep : dependencies)
        depList.emplace_back(jp::Dependency{strdup(dep.name.c_str()), strdup(dep.version.c_str())});

    info.dependencies = (jp::Dependency*)std::malloc(sizeof(jp::Dependency)*dependencies.size());
    std::copy(depList.begin(), depList.end(), info.dependencies);
    info.dependenciesNb = dependencies.size();

    return info;
}

/**
 * @brief 将 PluginInfoStd 对象转换为字符串
 *
 * 将 PluginInfoStd 对象转换为包含插件信息的字符串。
 *
 * @return 包含插件信息的字符串
 */
std::string PluginInfoStd::toString()
{
    if(name.empty())
        return "Invalid PluginInfo";

    std::string str = "Plugin info:\n";
    str += "Name: " + name + "\n";
    str += "Pretty name: " + prettyName + "\n";
    str += "Version: " + version + "\n";
    str += "Author: " + author + "\n";
    str += "Url: " + url + "\n";
    str += "License: " + license + "\n";
    str += "Copyright: " + copyright + "\n";
    str += "Dependencies:\n";
    for(const Dependency& dep : dependencies)
        str += " - " + dep.name + " (" + dep.version + ")\n";
    return str;
}

/*****************************************************************************/
/***** Plugin class **********************************************************/
/*****************************************************************************/

// Destructor
/**
 * @brief 析构函数
 *
 * 当 Plugin 对象被销毁时，自动调用此析构函数。
 * 用于确保插件已被正确卸载（尽管这种情况不应该发生）。
 * 如果插件尚未卸载，则先调用插件的 aboutToBeUnloaded 方法，然后重置插件指针并卸载插件库。
 */
Plugin::~Plugin()
{
    // Just in case the plugins have not been unloaded (should not happen)
    if(lib.isLoaded())
    {
        if(iplugin)
            iplugin->aboutToBeUnloaded();
        iplugin.reset();
        lib.unload();
    }
}
