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

#include "private/fsutil.h"

#include <cerrno> // for errno
#include <cstdlib> // for malloc and free

#include "tinydir/tinydir.h"
#include "whereami/src/whereami.h"

#include "confinfo.h"

namespace jp_private
{
namespace fsutil
{

/**
 * @brief 获取库文件扩展名
 *
 * 根据不同的平台，返回对应的库文件扩展名。
 *
 * @return 返回库文件扩展名的字符串
 */
std::string libraryExtension()
{
#if defined(CONFINFO_PLATFORM_WIN32) || defined(CONFINFO_PLATFORM_CYGWIN)
    return "dll";
#elif defined(CONFINFO_PLATFORM_MACOS)
    return "dylib";
#else
    return "so";
#endif
}

/**
 * @brief 获取库文件后缀
 *
 * 返回库文件的扩展名后缀，格式为"." + 库文件扩展名。
 *
 * @return 库文件后缀的字符串
 */
std::string librarySuffix()
{
    return "." + libraryExtension();
}

/**
 * @brief 列出指定目录下的文件
 *
 * 遍历指定目录，将符合条件的文件路径添加到文件列表中。
 *
 * @param rootDir 根目录路径
 * @param filesList 文件列表指针
 * @param extFilter 文件扩展名过滤器，为空则不过滤
 * @param recursive 是否递归遍历子目录
 *
 * @return 是否成功遍历目录并添加文件路径到列表中
 */
bool listFilesInDir(const std::string& rootDir,
                    PathList* filesList,
                    const std::string& extFilter,
                    bool recursive)
{
    bool success = true;

    if(!filesList)
    {
        errno = EINVAL;
        return false;
    }

    tinydir_dir dir;
    if(tinydir_open(&dir, rootDir.c_str()) == -1)
        return false;

    while(dir.has_next)
    {
        tinydir_file file;
        if(tinydir_readfile(&dir, &file) == -1)
        {
            success = false;
            continue;
        }

        // Regular file
        if(file.is_reg && (extFilter.empty() || extFilter == file.extension))
            filesList->push_back(file.path);
        // Directory
        else if(recursive
                && file.is_dir
                && strcmp(file.name, ".") != 0
                && strcmp(file.name, "..") != 0)
        {
            if(!listFilesInDir(file.path, filesList, extFilter, true))
                success = false;
        }

        if(tinydir_next(&dir) == -1)
        {
            success = false;
            break;
        }
    }
    tinydir_close(&dir);

    return success;
}

/**
 * @brief 列出指定目录下的库文件
 *
 * 在指定的根目录下，列出所有库文件，并将结果存储在给定的文件列表中。
 * 如果需要递归搜索子目录，则设置递归参数为 true。
 *
 * @param rootDir 根目录路径
 * @param filesList 存储文件列表的指针
 * @param recursive 是否递归搜索子目录
 *
 * @return 如果成功列出文件，则返回 true；否则返回 false
 */
bool listLibrariesInDir(const std::string& rootDir,
                        PathList* filesList,
                        bool recursive)
{
    return listFilesInDir(rootDir, filesList, libraryExtension(), recursive);
}

/**
 * @brief 获取应用程序目录
 *
 * 通过调用 wai_getExecutablePath 函数获取应用程序的可执行文件路径，并返回其所在的目录路径。
 *
 * @return 应用程序目录路径的字符串表示，如果获取失败则返回空字符串。
 */
std::string appDir()
{
    const int length = wai_getExecutablePath(nullptr, 0, nullptr);
    if(length == -1)
        return std::string();
    char* path = (char*)malloc(length+1);
    int dirnameLength = length;
    if(wai_getExecutablePath(path, length, &dirnameLength) != length)
        return std::string();

    // Only get the dirname
    path[dirnameLength] = '\0';

    std::string str(path);
    free(path);
    return str;
}

} // namespace fsutil
} // namespace jp_private
