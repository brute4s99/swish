# Copyright (C) 2015, 2016  Alexander Lamaison <swish@lammy.co.uk>
#
# This program is free software: you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later
# version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.

SET(_in_FILE ${CMAKE_CURRENT_SOURCE_DIR}/Swish.idl)
SET(_out_header_name Swish.h)
SET(_out_header ${CMAKE_CURRENT_BINARY_DIR}/shell_folder.dir/${CMAKE_CFG_INTDIR}/${_out_header_name})
SET(_out_iid_name Swish_i.c)
SET(_out_iid ${CMAKE_CURRENT_BINARY_DIR}/shell_folder.dir/${CMAKE_CFG_INTDIR}/${_out_iid_name})
message("_out_header_name=${_out_header_name}, _out_header=${_out_header}, _out_iid=${_out_iid}")
ADD_CUSTOM_COMMAND(
      OUTPUT ${_out_header} ${_out_iid}
      DEPENDS ${_in_FILE}
      COMMAND midl /target NT60 /header ${_out_header} /iid ${_out_iid} ${_in_FILE}
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )


SET_SOURCE_FILES_PROPERTIES(
  ${_out_header}
  ${_out_iid}
  PROPERTIES
  GENERATED TRUE
)


set(SOURCES
  DataObject.h
  Folder.h
  IconExtractor.h
  KbdInteractiveDialog.h
  HostFolder.h
  locale_setup.hpp
  Pidl.h
  Registry.h
  RemoteFolder.h
  resource.h
  SftpDataObject.h
  SftpDirectory.h
  SnitchingDataObject.hpp
  SwishFolder.hpp
  wtl.hpp
  DataObject.cpp
  HostFolder.cpp
  IconExtractor.cpp
  KbdInteractiveDialog.cpp
  Registry.cpp
  RemoteFolder.cpp
  SftpDataObject.cpp
  SftpDirectory.cpp
  data_object/FileGroupDescriptor.hpp
  data_object/GlobalLocker.hpp
  data_object/ShellDataObject.hpp
  data_object/ShellDataObject.cpp
  data_object/StorageMedium.hpp
  Swish.idl
  ${_out_header}
  ${_out_iid}
  shell_folder.rc)

add_library(shell_folder ${SOURCES})

# This exposes the include path of the directory that CMake tells midl
# to output the header file into.  Ideally, it would be
# "${CMAKE_CURRENT_BINARY_DIR}/\$(IntDir)", but that is no good if
# used by another project because $(IntDir) expands to the
# intermediate directory of that project, not this one.
target_include_directories(shell_folder
  PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/shell_folder.dir/${CMAKE_CFG_INTDIR})

find_package(Comet)
find_package(WTL)
find_package(Washer)
find_package(Boost COMPONENTS date_time system locale filesystem)

find_package(Comet REQUIRED CONFIG)
find_package(WTL REQUIRED CONFIG)
find_package(Washer REQUIRED CONFIG)
find_package(Boost REQUIRED COMPONENTS date_time system locale filesystem)

target_link_libraries(shell_folder
  PRIVATE
  host_folder remote_folder nse drop_target urlmon
  PUBLIC
  WTL::WTL
  Washer::washer Comet::comet Boost::boost Boost::date_time Boost::system
  Boost::locale Boost::filesystem)
