set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/test/SimpleZonePopulationPlannerTest.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test/SimplePartyRosterPlannerTest.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test/PartyBotServiceTest.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test/AccountAltRuntimeServiceTest.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test/LivingWorldCommandGrammarTest.cpp)

set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
  ${CMAKE_CURRENT_LIST_DIR}/src)
