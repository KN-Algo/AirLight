cmake_minimum_required(VERSION 3.16)
project(AudioPassthrough VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Biblioteki SDL2 i SDL2_ttf
find_package(SDL2 REQUIRED)
find_package(SDL2_ttf REQUIRED)

# Pliki zrodlowe - teraz podzielone na moduly
set(SOURCES
    main.cpp
    src/audio.cpp
    src/ui.cpp
)

set(HEADERS
    src/config.h
    src/audio.h
    src/ui.h
)

# Dodaj plik zrodlowy do projektu
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link SDL2 i SDL2_ttf
target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES} SDL2_ttf::SDL2_ttf)

# Na Windows, skopiuj pliki DLL do katalogu wyjściowego po zbudowaniu
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL2.dll to output directory"
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
        COMMENT "Copying SDL2_ttf.dll to output directory"
    )
endif()
