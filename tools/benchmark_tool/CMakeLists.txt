# Copyright (C) 2018-2023 Intel Corporation
# SPDX-License-Identifier: Apache-2.0
#

cmake_minimum_required (VERSION 3.13)

project(OpenVINOBenchmarkTool)

#
# Packages & settings
#

if(NOT DEFINED OpenVINO_SOURCE_DIR)
    get_filename_component(OpenVINO_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." REALPATH)
endif()

if(NOT IEDevScripts_FOUND)
    find_package(IEDevScripts REQUIRED
                 PATHS "${OpenVINO_SOURCE_DIR}/cmake/developer_package"
                 NO_CMAKE_FIND_ROOT_PATH
                 NO_DEFAULT_PATH)
endif()

#
# Installation rules
#

ov_get_pyversion(pyversion)
ov_cpack_add_component(${OV_CPACK_COMP_PYTHON_OPENVINO}_${pyversion}
                        HIDDEN)

install(DIRECTORY ${OpenVINOBenchmarkTool_SOURCE_DIR}/openvino
        DESTINATION ${OV_CPACK_PYTHONDIR}
        COMPONENT ${OV_CPACK_COMP_PYTHON_OPENVINO}_${pyversion}
        ${OV_CPACK_COMP_PYTHON_OPENVINO_EXCLUDE_ALL}
        USE_SOURCE_PERMISSIONS)
