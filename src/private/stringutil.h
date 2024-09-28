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

#ifndef STRINGUTIL_H
#define STRINGUTIL_H

/*
 * This file is an internal header. It's not part of the public API,
 * and may change at any moment.
 */

#include <cstring>
#include <cstdlib>

namespace jp_private
{
namespace StringUtil
{

// strdup is not part of the C standard but part of POSIX
// So, define the function here
/**
 * @brief 自定义字符串复制函数
 *
 * 使用 malloc 分配内存，并复制给定的字符串内容到新的内存空间中。
 *
 * @param s 指向待复制的字符串的指针
 *
 * @return 指向新分配内存的指针，复制了原始字符串的内容；如果分配失败则返回 nullptr
 */
inline char* strdup_custom(const char* s)
{
    size_t size = strlen(s) + 1;
    char *p = (char*)malloc(size);
    if(p)
        memcpy(p, s, size);
    return p;
}

// Set the correct version of strdup depending on the system
#if defined(CONFINFO_PLATFORM_LINUX) || defined(CONFINFO_PLATFORM_MACOS) || defined(CONFINFO_PLATFORM_CYGWIN) || defined(CONFINFO_COMPILER_GCC)
// It's a POSIX env, strdup is already defined
#  define strdup(x) strdup(x)
#elif defined(CONFINFO_COMPILER_MSVC)
#  define strdup(x) _strdup(x)
#else
// Use the custom strdup
//#  define strdup(x) jp_private::StringUtil::strdup_custom(x)
#endif

} // namespace StringUtil
} // namespace jp_private

#endif // STRINGUTIL_H
