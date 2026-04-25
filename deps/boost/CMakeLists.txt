#
# This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#

if(WIN32)
  if(DEFINED ENV{Boost_ROOT})
    set(Boost_ROOT $ENV{Boost_ROOT})
  endif()

  set(Boost_USE_STATIC_LIBS        ON)
  set(Boost_USE_MULTITHREADED      ON)
  set(Boost_USE_STATIC_RUNTIME     OFF)
endif()

set(Boost_NO_WARN_NEW_VERSIONS ON)

include (CheckCXXSourceCompiles)

if (WIN32)
  # On windows the requirements are higher according to the wiki.
  set(BOOST_REQUIRED_VERSION 1.78)
else()
  set(BOOST_REQUIRED_VERSION 1.74)
endif()

# Boost.System is header-only since 1.69; do not require it explicitly.
find_package(Boost ${BOOST_REQUIRED_VERSION} REQUIRED COMPONENTS filesystem program_options iostreams regex)
find_package(boost_throw_exception CONFIG QUIET)

if(NOT Boost_FOUND)
  if(NOT DEFINED ENV{Boost_ROOT} AND NOT DEFINED Boost_DIR AND NOT DEFINED BOOST_ROOT AND NOT DEFINED BOOSTROOT)
    message(FATAL_ERROR "No Boost_ROOT environment variable could be found! Please make sure it is set and the points to your Boost installation.")
  endif()
endif()

add_library(boost INTERFACE)

set(AC_BOOST_LIBRARIES ${Boost_LIBRARIES})

if(MSVC)
  set(AC_BOOST_LIBRARIES)

  function(ac_append_boost_import_lib target_name)
    if(NOT TARGET ${target_name})
      return()
    endif()

    get_target_property(_boost_implib_release ${target_name} IMPORTED_IMPLIB_RELEASE)
    get_target_property(_boost_implib_debug ${target_name} IMPORTED_IMPLIB_DEBUG)
    get_target_property(_boost_location_release ${target_name} IMPORTED_LOCATION_RELEASE)
    get_target_property(_boost_location_debug ${target_name} IMPORTED_LOCATION_DEBUG)

    if(NOT _boost_implib_release OR _boost_implib_release STREQUAL "_boost_implib_release-NOTFOUND")
      set(_boost_implib_release "${_boost_location_release}")
    endif()

    if(NOT _boost_implib_debug OR _boost_implib_debug STREQUAL "_boost_implib_debug-NOTFOUND")
      set(_boost_implib_debug "${_boost_location_debug}")
    endif()

    if(_boost_implib_release AND NOT _boost_implib_release STREQUAL "_boost_implib_release-NOTFOUND")
      list(APPEND AC_BOOST_LIBRARIES
        "$<$<CONFIG:Debug>:${_boost_implib_debug}>"
        "$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:${_boost_implib_release}>")
      set(AC_BOOST_LIBRARIES "${AC_BOOST_LIBRARIES}" PARENT_SCOPE)
    endif()
  endfunction()

  foreach(BOOST_COMPONENT filesystem program_options iostreams regex)
    ac_append_boost_import_lib(Boost::${BOOST_COMPONENT})
  endforeach()
else()
  foreach(BOOST_COMPONENT filesystem program_options iostreams regex)
    if(TARGET Boost::${BOOST_COMPONENT})
      list(APPEND AC_BOOST_LIBRARIES Boost::${BOOST_COMPONENT})
    endif()
  endforeach()

  if(TARGET Boost::throw_exception)
    list(APPEND AC_BOOST_LIBRARIES Boost::throw_exception)
  endif()
endif()

list(REMOVE_DUPLICATES AC_BOOST_LIBRARIES)

target_link_libraries(boost
  INTERFACE
    ${AC_BOOST_LIBRARIES})

set(AC_BOOST_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})

if(NOT AC_BOOST_INCLUDE_DIRS AND DEFINED Boost_INCLUDE_DIR)
  list(APPEND AC_BOOST_INCLUDE_DIRS ${Boost_INCLUDE_DIR})
endif()

target_include_directories(boost
  INTERFACE
    ${AC_BOOST_INCLUDE_DIRS})

target_compile_definitions(boost
  INTERFACE
    -DBOOST_DATE_TIME_NO_LIB
    -DBOOST_REGEX_NO_LIB
    -DBOOST_CHRONO_NO_LIB
    -DBOOST_SERIALIZATION_NO_LIB
    -DBOOST_CONFIG_SUPPRESS_OUTDATED_MESSAGE
    -DBOOST_ASIO_NO_DEPRECATED
    -DBOOST_SYSTEM_USE_UTF8
    -DBOOST_BIND_NO_PLACEHOLDERS)
