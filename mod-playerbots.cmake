if(BUILD_TESTING)
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotFactoryTrainerPolicyTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/tests/cpp/PlayerbotQuestShareRoutingTest.cpp"
    )
    set_property(GLOBAL APPEND PROPERTY ACORE_MODULE_TEST_INCLUDES
        "${CMAKE_CURRENT_LIST_DIR}/src"
    )
endif()
