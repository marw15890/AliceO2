# Copyright 2019-2020 CERN and copyright holders of ALICE O2.
# See https://alice-o2.web.cern.ch/copyright for details of the copyright holders.
# All rights not expressly granted are reserved.
#
# This software is distributed under the terms of the GNU General Public
# License v3 (GPL Version 3), copied verbatim in the file "COPYING".
#
# In applying this license CERN does not waive the privileges and immunities
# granted to it by virtue of its status as an Intergovernmental Organization
# or submit itself to any jurisdiction.

o2_add_library(DetectorsVertexing
               TARGETVARNAME targetName
               SOURCES src/DCAFitterN.cxx
                       src/PVertexer.cxx
                       src/PVertexerHelpers.cxx
                       src/PVertexerParams.cxx
                       src/VertexTrackMatcher.cxx
                       src/SVertexer.cxx
                       src/SVertexerParams.cxx
                       src/SVertexHypothesis.cxx
                       src/FwdDCAFitterN.cxx
               PUBLIC_LINK_LIBRARIES ROOT::Core
                                     O2::CommonUtils
                                     O2::ReconstructionDataFormats
                                     O2::DataFormatsParameters
                                     O2::DataFormatsTPC
                                     O2::DataFormatsITS
                                     O2::DetectorsBase
                                     O2::TPCBase
                                     O2::SimulationDataFormat
                                     O2::FT0Reconstruction
                                     O2::DataFormatsFT0
                                     O2::GlobalTracking
                                     Microsoft.GSL::GSL)

o2_target_root_dictionary(DetectorsVertexing
                          HEADERS include/DetectorsVertexing/HelixHelper.h
                                  include/DetectorsVertexing/PVertexerHelpers.h
                                  include/DetectorsVertexing/PVertexerParams.h
                                  include/DetectorsVertexing/DCAFitterN.h
                                  include/DetectorsVertexing/FwdDCAFitterN.h
                                  include/DetectorsVertexing/SVertexerParams.h
                                  include/DetectorsVertexing/SVertexHypothesis.h)

if (OpenMP_CXX_FOUND)
    target_compile_definitions(${targetName} PRIVATE WITH_OPENMP)
    target_link_libraries(${targetName} PRIVATE OpenMP::OpenMP_CXX)
endif()


o2_add_test(
  DCAFitterN
  SOURCES test/testDCAFitterN.cxx
  COMPONENT_NAME DetectorsVertexing
  PUBLIC_LINK_LIBRARIES O2::DetectorsVertexing ROOT::Core ROOT::Physics
  LABELS vertexing
  ENVIRONMENT O2_ROOT=${CMAKE_BINARY_DIR}/stage
  VMCWORKDIR=${CMAKE_BINARY_DIR}/stage/${CMAKE_INSTALL_DATADIR})
