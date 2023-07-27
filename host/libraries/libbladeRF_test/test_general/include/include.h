/*
 * This file is part of the bladeRF project:
 *   http://www.github.com/nuand/bladeRF
 *
 * Copyright (C) 2023 Nuand LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Recursively searches for a file with the specified filename in the given directory path.
 *
 * This function searches for a file with the specified filename in the given directory path,
 * including all its subdirectories. If the file is found, the output parameter 'rv' is updated
 * with the path of the found file.
 *
 * @param path The directory path to search for the file.
 * @param filename The name of the file to find.
 * @param[out] rv Pointer to a 'fs::path' object to store the path of the found file. If the file
 *                is found, this parameter will be updated with the file's path; otherwise, it
 *                will remain unchanged.
 *
 * @note The function uses the current user's home directory if the 'path' starts with the '~'
 *       character. This allows using the '~' symbol as a shorthand for the user's home directory.
 *       The function does not expand environment variables other than '~'.
 * @note The 'rv' parameter must point to a valid 'fs::path' object, which will be updated if the
 *       file is found. If 'rv' is a nullptr, the function will result in undefined behavior.
 *
 * @see fs::is_regular_file
 * @see fs::is_directory
 * @see fs::directory_iterator
 * @see std::sort
 *
 * @warning The function performs recursive searches in the directory tree, and it may take time
 *          for large directory structures. Exercise caution when using it with deeply nested
 *          directories.
 */
void find_file(fs::path path, const std::string& filename, fs::path* rv) {
    if (path.string().find("~") == 0) {
        path = fs::path(std::getenv("HOME"))/path.string().substr(2);
    }

    if (fs::is_regular_file(path/filename)) {
        *rv = (path/filename);
    }

    if (fs::is_directory(path)) {
        std::vector<fs::path> entries;

        for (const auto& entry : fs::directory_iterator(path)) {
            entries.push_back(entry);
        }
        std::sort(entries.begin(), entries.end(), std::less<>());

        for (const auto& entry : entries){
            if (fs::is_directory(entry)) {
                find_file(entry, filename, rv);
            }
        }
    }
}
