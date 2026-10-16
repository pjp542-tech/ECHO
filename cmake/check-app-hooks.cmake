# Stop with an explanation if a CubeMX regeneration loses the application hooks.
set(GENERATED_MAIN "${CMAKE_CURRENT_LIST_DIR}/../ECHO/Core/Src/main.c")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GENERATED_MAIN}")
file(READ "${GENERATED_MAIN}" GENERATED_MAIN_CONTENT)
foreach(HOOK IN ITEMS "#include \"app.h\"" "setup();" "loop();")
    string(FIND "${GENERATED_MAIN_CONTENT}" "${HOOK}" HOOK_POSITION)
    if(HOOK_POSITION EQUAL -1)
        message(FATAL_ERROR
            "CubeMX application connection missing: ${HOOK}\n"
            "Restore the USER CODE blocks described under Regeneration in README.md.")
    endif()
endforeach()
