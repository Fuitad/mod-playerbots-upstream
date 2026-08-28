# PLB-LOCAL FILE. Not present upstream, so it can never conflict on a merge.
# Prefer adding here over editing an upstream file. See docs/local-changes.md.

if(BUILD_TESTING)
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotFactoryTrainerPolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/RandomPlayerbotAdmissionTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotQuestShareRoutingTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotRecoveryPolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotTravelTargetTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/RandomBotMaintenancePolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotQuestPoiPointPolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotQuestPoiApproachPolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/Ai/Base/Actions/RandomBotMaintenancePolicy.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/Bot/Recovery/PlayerbotRecoveryPolicy.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/Bot/Population/RandomPlayerbotAdmission.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/src/Bot/Movement/PlayerbotTaxiFlight.cpp"
    )
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
        "${CMAKE_CURRENT_LIST_DIR}/src"
    )
endif()
