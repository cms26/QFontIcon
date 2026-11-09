# Compiled-in icons for QFontIcon: no font file at run time.
#
#   qfonticon_embed_icons(<target>
#       [ICONS <entry>...]            # icons to compile in
#       [ICONS_FILE <file>]           # the same, from a file (one entry per line, # comments)
#       [ALL]                         # every icon (e.g. while developing)
#       [DATA <icon data file>])      # default: icons/fontawesome5.icons (Font Awesome 5)
#
# Entries pick icons from the icon data file, by code or by name, from any
# set or from one set ("solid", "regular", "brands" for Font Awesome 5):
#
#   f015            the first set that has it (Solid, then Regular, then Brands)
#   home            the same, by name
#   regular:f15b    from that set only
#   brands:github
#   regular:*       the whole set
#
# The selected icons are written into a generated .cpp that is added to
# <target> (use the executable, not a static library, so its registration
# runs). QFontIcon::icon(0xf015, ...) then draws them without any font;
# fonts added with QFontIcon::addFont() still work for the rest.
#
# Unknown entries stop the build with a message. The .cpp is regenerated
# when the list, the data file or this script change.
#
# The same script also runs on its own (e.g. from qmake or a shell), writing
# the .cpp directly:
#   cmake -DQFONTICON_DATA=icons/fontawesome5.icons -DQFONTICON_SELECTION=icons.txt
#         -DQFONTICON_OUTPUT=qfonticon_icons.cpp -P cmake/QFontIconEmbed.cmake
# (QFONTICON_SELECTION: a file of entries, or the word ALL.)

if(CMAKE_SCRIPT_MODE_FILE)
    # --- Script mode: write the .cpp --------------------------------------------------
    cmake_minimum_required(VERSION 3.16)
    foreach(_var QFONTICON_DATA QFONTICON_SELECTION QFONTICON_OUTPUT)
        if(NOT DEFINED ${_var})
            message(FATAL_ERROR "QFontIconEmbed: ${_var} is not set")
        endif()
    endforeach()

    # The entries wanted
    set(_all FALSE)
    set(_entries "")
    if(QFONTICON_SELECTION STREQUAL "ALL")
        set(_all TRUE)
    else()
        file(STRINGS "${QFONTICON_SELECTION}" _lines)
        foreach(_line IN LISTS _lines)
            string(REGEX REPLACE "#.*$" "" _line "${_line}")
            string(STRIP "${_line}" _line)
            if(_line STREQUAL "ALL")
                set(_all TRUE)
            elseif(NOT _line STREQUAL "")
                string(TOLOWER "${_line}" _line)
                list(APPEND _entries "${_line}")
            endif()
        endforeach()
    endif()

    # The data file: fonts (sets) and glyphs
    file(STRINGS "${QFONTICON_DATA}" _data)
    set(_sets "")
    set(_fonts_cpp "")
    set(_glyph_lines "")
    foreach(_line IN LISTS _data)
        if(_line MATCHES "^font\\|")
            string(REPLACE "|" ";" _f "${_line}")
            list(GET _f 1 _set)
            list(GET _f 2 _family)
            list(GET _f 3 _style)
            list(GET _f 4 _upem)
            list(GET _f 5 _ascent)
            list(GET _f 6 _descent)
            list(APPEND _sets "${_set}")
            string(APPEND _fonts_cpp "    {\"${_set}\", \"${_family}\", \"${_style}\", ${_upem}, ${_ascent}, ${_descent}},\n")
        elseif(_line MATCHES "^glyph\\|")
            list(APPEND _glyph_lines "${_line}")
        endif()
    endforeach()
    if(_sets STREQUAL "")
        message(FATAL_ERROR "QFontIconEmbed: no icon sets in ${QFONTICON_DATA}")
    endif()

    # Which glyphs: "set|code" keys
    set(_chosen "")
    set(_unknown "")
    if(NOT _all)
        foreach(_entry IN LISTS _entries)
            set(_want_set "")
            set(_want "${_entry}")
            if(_entry MATCHES "^([^:]+):(.+)$")
                set(_want_set "${CMAKE_MATCH_1}")
                set(_want "${CMAKE_MATCH_2}")
                if(NOT _want_set IN_LIST _sets)
                    list(JOIN _sets ", " _sets_text)
                    list(APPEND _unknown "${_entry} (no set \"${_want_set}\"; the sets are ${_sets_text})")
                    continue()
                endif()
            endif()
            string(REGEX REPLACE "^(0x|u\\+)" "" _want "${_want}")
            set(_found FALSE)
            foreach(_set IN LISTS _sets)
                if(NOT _want_set STREQUAL "" AND NOT _set STREQUAL _want_set)
                    continue()
                endif()
                foreach(_line IN LISTS _glyph_lines)
                    if(_line MATCHES "^glyph\\|${_set}\\|([0-9a-f]+)\\|([^|]+)\\|")
                        set(_code "${CMAKE_MATCH_1}")
                        set(_name "${CMAKE_MATCH_2}")
                        if(_want STREQUAL "*" OR _want STREQUAL _code OR _want STREQUAL _name)
                            list(APPEND _chosen "${_set}|${_code}")
                            set(_found TRUE)
                            if(NOT _want STREQUAL "*")
                                break()
                            endif()
                        endif()
                    endif()
                endforeach()
                if(_found)
                    break()   # the first set that has it
                endif()
            endforeach()
            if(NOT _found)
                list(APPEND _unknown "${_entry}")
            endif()
        endforeach()
        if(_unknown)
            list(JOIN _unknown "\n  " _unknown_text)
            message(FATAL_ERROR "QFontIconEmbed: not in ${QFONTICON_DATA}:\n  ${_unknown_text}\n"
                                "(entries are codes like f015, names like home, optionally with a set: regular:f15b)")
        endif()
        list(REMOVE_DUPLICATES _chosen)
    endif()

    # The glyph table
    set(_glyphs_cpp "")
    set(_count 0)
    foreach(_line IN LISTS _glyph_lines)
        string(REPLACE "|" ";" _g "${_line}")
        list(GET _g 1 _set)
        list(GET _g 2 _code)
        if(NOT _all AND NOT "${_set}|${_code}" IN_LIST _chosen)
            continue()
        endif()
        list(GET _g 3 _name)
        list(GET _g 4 _advance)
        list(GET _g 5 _path)
        list(FIND _sets "${_set}" _font)
        # Long outlines as several string pieces (some compilers limit one piece)
        string(LENGTH "${_path}" _len)
        set(_pieces "")
        set(_pos 0)
        while(_pos LESS _len)
            string(SUBSTRING "${_path}" ${_pos} 2000 _piece)
            string(APPEND _pieces "\n        \"${_piece}\"")
            math(EXPR _pos "${_pos} + 2000")
        endwhile()
        string(APPEND _glyphs_cpp "    {${_font}, 0x${_code}, \"${_name}\", ${_advance},${_pieces}},\n")
        math(EXPR _count "${_count} + 1")
    endforeach()
    if(_count EQUAL 0)
        message(FATAL_ERROR "QFontIconEmbed: no icons selected (list them, or use ALL)")
    endif()

    get_filename_component(_data_name "${QFONTICON_DATA}" NAME)
    file(WRITE "${QFONTICON_OUTPUT}.tmp"
"// Compiled-in QFontIcon icons: ${_count} from ${_data_name}.
// Generated by QFontIconEmbed.cmake (qfonticon_embed_icons()); do not edit.

#include \"qfonticon.h\"

namespace {
const QFontIconEmbeddedFont kFonts[] = {
${_fonts_cpp}};

const QFontIconEmbeddedGlyph kGlyphs[] = {
${_glyphs_cpp}};

// Registers the icons before main() runs
const bool kRegistered = QFontIcon::addEmbeddedIcons(kFonts, int(sizeof(kFonts) / sizeof(kFonts[0])),
                                                     kGlyphs, int(sizeof(kGlyphs) / sizeof(kGlyphs[0])));
} // namespace
")
    # Only touch the .cpp when it changed (no needless rebuilds)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QFONTICON_OUTPUT}.tmp" "${QFONTICON_OUTPUT}")
    file(REMOVE "${QFONTICON_OUTPUT}.tmp")
    message(STATUS "QFontIcon: ${_count} icons compiled in")
    return()
endif()

# --- Configure mode: the qfonticon_embed_icons() function -----------------------------
# Kept as global properties: the function may be called from any directory
# (e.g. the application's, when QFontIcon is added with add_subdirectory())
set_property(GLOBAL PROPERTY QFONTICON_EMBED_SCRIPT "${CMAKE_CURRENT_LIST_FILE}")
get_filename_component(_qfonticon_data "${CMAKE_CURRENT_LIST_DIR}/../icons/fontawesome5.icons" ABSOLUTE)
set_property(GLOBAL PROPERTY QFONTICON_DEFAULT_DATA "${_qfonticon_data}")

function(qfonticon_embed_icons target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "ALL" "ICONS_FILE;DATA" "ICONS")
    if(NOT TARGET ${target})
        message(FATAL_ERROR "qfonticon_embed_icons: no target ${target}")
    endif()
    get_property(_script GLOBAL PROPERTY QFONTICON_EMBED_SCRIPT)
    if(NOT arg_DATA)
        get_property(arg_DATA GLOBAL PROPERTY QFONTICON_DEFAULT_DATA)
    endif()
    get_filename_component(arg_DATA "${arg_DATA}" ABSOLUTE)

    set(_dir "${CMAKE_CURRENT_BINARY_DIR}/qfonticon_${target}")
    set(_output "${_dir}/qfonticon_icons.cpp")
    set(_selection "${_dir}/icons.txt")
    set(_depends "${arg_DATA}" "${_script}" "${_selection}")

    # The list of entries: written at configure time (only when it changed)
    set(_content "")
    if(arg_ALL)
        set(_content "ALL\n")
    endif()
    foreach(_entry IN LISTS arg_ICONS)
        string(APPEND _content "${_entry}\n")
    endforeach()
    if(arg_ICONS_FILE)
        get_filename_component(arg_ICONS_FILE "${arg_ICONS_FILE}" ABSOLUTE)
        file(READ "${arg_ICONS_FILE}" _file_content)
        string(APPEND _content "${_file_content}\n")
        # Editing the file re-runs CMake, which rewrites the list
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${arg_ICONS_FILE}")
    endif()
    if(_content STREQUAL "")
        message(FATAL_ERROR "qfonticon_embed_icons(${target}): give ICONS, ICONS_FILE or ALL")
    endif()
    file(CONFIGURE OUTPUT "${_selection}" CONTENT "${_content}" @ONLY)

    add_custom_command(
        OUTPUT "${_output}"
        COMMAND ${CMAKE_COMMAND}
                "-DQFONTICON_DATA=${arg_DATA}"
                "-DQFONTICON_SELECTION=${_selection}"
                "-DQFONTICON_OUTPUT=${_output}"
                -P "${_script}"
        DEPENDS ${_depends}
        COMMENT "Compiling in QFontIcon icons for ${target}"
        VERBATIM)
    target_sources(${target} PRIVATE "${_output}")
endfunction()
