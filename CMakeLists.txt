cmake_minimum_required(VERSION 3.20)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8> $<$<CXX_COMPILER_ID:GNU>:-finput-charset=UTF-8 -fexec-charset=GBK>)
project(wxNotepad2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(wxWidgets_USE_STATIC ON)
find_package(wxWidgets CONFIG REQUIRED COMPONENTS core base stc adv net)
find_package(oniguruma CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(unofficial-tinyexpr CONFIG REQUIRED)
find_package(unofficial-mongoose CONFIG REQUIRED)
find_path(PICOJSON_INCLUDE_DIRS "picojson/picojson.h")

set(SOURCE_FILES
    App.cpp
    events.cpp
    mainFrame.cpp
    MyCardViewPanel.cpp
    MySTC.cpp
    MyTreeCtrl.cpp
    Tray.cpp
    Dialogs.cpp
    SettingDialog.cpp 
 "MongooseServer.cpp")

set(RESOURCE_FILES
    app.rc
    wx_dpi_aware_pmv2.manifest
   )
add_executable(wxNotepad2 ${SOURCE_FILES} ${RESOURCE_FILES} )
target_precompile_headers(wxNotepad2 PRIVATE  "pch.h")
target_include_directories(wxNotepad2 PRIVATE ${PICOJSON_INCLUDE_DIRS})

if(WIN32)
    set_target_properties(wxNotepad2 PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

target_link_libraries(wxNotepad2 PRIVATE
    wx::core wx::base wx::stc wx::adv wx::net
    oniguruma::onig
    unofficial::sqlite3::sqlite3
    unofficial::tinyexpr::tinyexpr
    unofficial::mongoose::mongoose
    dwrite
)
