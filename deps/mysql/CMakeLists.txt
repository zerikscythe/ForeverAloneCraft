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

if (NOT MYSQL_FOUND)
  message(FATAL_ERROR "MySQL wasn't found on your system but it's required to build the servers!")
endif()

add_library(mysql STATIC IMPORTED GLOBAL)

set_target_properties(mysql
  PROPERTIES
    IMPORTED_LOCATION
      "${MYSQL_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES
      "${MYSQL_INCLUDE_DIR}")

if(MSVC)
  set_target_properties(mysql
    PROPERTIES
      IMPORTED_CONFIGURATIONS "RELEASE;RELWITHDEBINFO;MINSIZEREL;DEBUG"
      IMPORTED_LOCATION_RELEASE "${MYSQL_LIBRARY}"
      IMPORTED_LOCATION_RELWITHDEBINFO "${MYSQL_LIBRARY}"
      IMPORTED_LOCATION_MINSIZEREL "${MYSQL_LIBRARY}"
      IMPORTED_LOCATION_DEBUG "${MYSQL_LIBRARY}"
      MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
      MAP_IMPORTED_CONFIG_MINSIZEREL Release)
endif()
