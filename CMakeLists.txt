cmake_minimum_required(VERSION 3.20)
project(GitWatcherBot
  VERSION "1.0.0"
  DESCRIPTION "A Telegram Bot that notifies you when a new change is made in your repositories (issues, pull requests, stars, forks, and watches)"
  LANGUAGES CXX
)

include(FetchContent)

# tgbotxx lib
FetchContent_Declare(tgbotxx
  GIT_REPOSITORY "https://github.com/baderouaich/tgbotxx"
  GIT_TAG "main"
)
FetchContent_MakeAvailable(tgbotxx)

# sqlite3 orm lib
FetchContent_Declare(sqlite_orm
  GIT_REPOSITORY "https://github.com/fnc12/sqlite_orm"
  GIT_TAG "master"
)
FetchContent_MakeAvailable(sqlite_orm)

######### Main Project ##########
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/**.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/**.hpp")
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${PROJECT_NAME} PRIVATE
  tgbotxx
  sqlite_orm
)
add_compile_definitions(RES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/res")
##################################


###### Doxygen Documentation ######
if (BUILD_DOCS)
    message(STATUS "Building Doxygen docs is enabled")
    find_package(Doxygen REQUIRED)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile)
    add_custom_target(docs_build ALL
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Generating Doxygen API documentation..."
            VERBATIM
            )
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/docs/html/ TYPE DOC)
endif ()
###################################
