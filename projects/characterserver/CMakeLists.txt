# MIT License

# Copyright (c) 2018-2019 NovusCore

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

project(characterserver VERSION 1.0.0 DESCRIPTION "characterserver for NovusCore")

file(GLOB_RECURSE CHARACTERSERVER_FILES "*.cpp" "*.h")
set(CHARACTERSERVER_DEPENDENCIES
    "../common/Dependencies/amy"
    "../common/Dependencies/json"
    "${ZLIB_INCLUDE_DIRS}"
)

add_executable(characterserver ${CHARACTERSERVER_FILES})
find_assign_files(${CHARACTERSERVER_FILES})

# Set VERSION & FOLDER Property
set_target_properties(characterserver PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(characterserver PROPERTIES FOLDER "server")

target_link_libraries(characterserver common)
target_include_directories(characterserver PRIVATE ${CHARACTERSERVER_DEPENDENCIES})
include_directories(CHARACTERSERVER "${common_SOURCE_DIR}")
install(TARGETS characterserver DESTINATION bin)