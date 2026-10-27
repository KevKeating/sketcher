include_guard(GLOBAL)

set(SKETCHER_EXTERNAL_SOURCE_CACHE
    ""
    CACHE
      PATH
      "Directory of pristine dependency source archives (empty disables caching)"
)
set_property(GLOBAL PROPERTY SKETCHER_EXTERNAL_SOURCE_MANIFEST "{}")

if(SKETCHER_EXTERNAL_SOURCE_CACHE)
  get_filename_component(SKETCHER_EXTERNAL_SOURCE_CACHE
                         "${SKETCHER_EXTERNAL_SOURCE_CACHE}" ABSOLUTE)
  find_package(Python3 3.12 REQUIRED COMPONENTS Interpreter)
endif()

function(source_cache_json_quote OUTPUT VALUE)
  string(REPLACE "\\" "\\\\" VALUE "${VALUE}")
  string(REPLACE "\"" "\\\"" VALUE "${VALUE}")
  string(REPLACE "\n" "\\n" VALUE "${VALUE}")
  string(REPLACE "\r" "\\r" VALUE "${VALUE}")
  string(REPLACE "\t" "\\t" VALUE "${VALUE}")
  set(${OUTPUT}
      "\"${VALUE}\""
      PARENT_SCOPE)
endfunction()

function(source_cache_cmake_quote OUTPUT VALUE)
  set(EQUALS "=")
  string(FIND "${VALUE}" "]${EQUALS}]" POSITION)
  while(NOT POSITION EQUAL -1)
    string(APPEND EQUALS "=")
    string(FIND "${VALUE}" "]${EQUALS}]" POSITION)
  endwhile()
  set(${OUTPUT}
      "[${EQUALS}[${VALUE}]${EQUALS}]"
      PARENT_SCOPE)
endfunction()

# Forward the ordinary ExternalProject arguments. Only source acquisition is
# replaced when caching is enabled; patches and build settings stay local.
# SKETCHER_SOURCE_CACHE_GIT_SUBMODULES overrides GIT_SUBMODULES for cached
# snapshots only.
function(sketcher_external_project_add NAME)
  set(SOURCE_SINGLE GIT_REPOSITORY GIT_TAG GIT_SHALLOW GIT_SUBMODULES_RECURSE
                    URL_HASH)
  set(SOURCE_MULTI GIT_SUBMODULES GIT_CONFIG URL)
  set(PROJECT_SINGLE ${SOURCE_SINGLE} SOURCE_SUBDIR)
  set(PROJECT_MULTI
      ${SOURCE_MULTI}
      CMAKE_ARGS
      PATCH_COMMAND
      CONFIGURE_COMMAND
      BUILD_COMMAND
      INSTALL_COMMAND
      DEPENDS)
  cmake_parse_arguments(PARSE_ARGV 1 EP "" "${PROJECT_SINGLE}"
                        "${PROJECT_MULTI};SKETCHER_SOURCE_CACHE_GIT_SUBMODULES")
  if(EP_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "Unsupported external project arguments: ${EP_UNPARSED_ARGUMENTS}")
  endif()

  # A snapshot may contain more modules than one particular build needs. Qt's
  # host and target builds use the same archive, but different build options.
  if(DEFINED EP_SKETCHER_SOURCE_CACHE_GIT_SUBMODULES
     OR "SKETCHER_SOURCE_CACHE_GIT_SUBMODULES" IN_LIST
        EP_KEYWORDS_MISSING_VALUES)
    set(EP_GIT_SUBMODULES "${EP_SKETCHER_SOURCE_CACHE_GIT_SUBMODULES}")
  endif()

  set(SOURCE_ARGS)
  set(OPTIONS_JSON "[]")
  set(FETCH_OPTIONS "")
  foreach(KEYWORD IN LISTS SOURCE_SINGLE SOURCE_MULTI)
    if(DEFINED EP_${KEYWORD} OR KEYWORD IN_LIST EP_KEYWORDS_MISSING_VALUES)
      list(APPEND SOURCE_ARGS "${KEYWORD}")
      if(DEFINED EP_${KEYWORD} AND NOT "${EP_${KEYWORD}}" STREQUAL "")
        list(APPEND SOURCE_ARGS ${EP_${KEYWORD}})
      else()
        list(APPEND SOURCE_ARGS "")
      endif()
    endif()
  endforeach()
  foreach(ARG IN LISTS SOURCE_ARGS)
    source_cache_json_quote(QUOTED "${ARG}")
    string(JSON OPTION_COUNT LENGTH "${OPTIONS_JSON}")
    string(JSON OPTIONS_JSON SET "${OPTIONS_JSON}" ${OPTION_COUNT} "${QUOTED}")
    source_cache_cmake_quote(QUOTED "${ARG}")
    string(APPEND FETCH_OPTIONS "  ${QUOTED}\n")
  endforeach()
  string(SHA256 ID "${OPTIONS_JSON}")
  set(KEY "external-source-v1-${NAME}-${ID}")
  set(FETCH_PROJECT "${CMAKE_CURRENT_BINARY_DIR}/source-fetch/${KEY}")
  file(MAKE_DIRECTORY "${FETCH_PROJECT}")
  file(
    WRITE "${FETCH_PROJECT}/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.25)\n"
    "project(source_fetch NONE)\ninclude(ExternalProject)\n"
    "ExternalProject_Add(source\n${FETCH_OPTIONS}"
    "  SOURCE_DIR \"\${CMAKE_CURRENT_BINARY_DIR}/source\"\n"
    "  DOWNLOAD_EXTRACT_TIMESTAMP TRUE\n"
    "  CONFIGURE_COMMAND \"\" BUILD_COMMAND \"\" INSTALL_COMMAND \"\"\n"
    "  STEP_TARGETS download)\n")

  set(ENTRY "{}")
  foreach(FIELD key fetch_project)
    string(TOUPPER "${FIELD}" VARIABLE)
    source_cache_json_quote(QUOTED "${${VARIABLE}}")
    string(JSON ENTRY SET "${ENTRY}" "${FIELD}" "${QUOTED}")
  endforeach()
  string(JSON ENTRY SET "${ENTRY}" options "${OPTIONS_JSON}")
  get_property(MANIFEST GLOBAL PROPERTY SKETCHER_EXTERNAL_SOURCE_MANIFEST)
  string(JSON MANIFEST SET "${MANIFEST}" "${NAME}" "${ENTRY}")
  set_property(GLOBAL PROPERTY SKETCHER_EXTERNAL_SOURCE_MANIFEST "${MANIFEST}")
  set(MANIFEST_PATH "${CMAKE_CURRENT_BINARY_DIR}/source-manifest.json")
  file(WRITE "${MANIFEST_PATH}" "${MANIFEST}\n")

  # Expanding ARGN directly loses empty arguments and splits quoted semicolons.
  # Serialize individual arguments so even CONFIGURE_COMMAND "" is forwarded.
  set(FORWARDED "")
  set(FORWARD_ARGUMENT TRUE)
  math(EXPR LAST "${ARGC} - 1")
  foreach(INDEX RANGE 0 ${LAST})
    set(ARG "${ARGV${INDEX}}")
    # Consume the wrapper-only option up to the next ExternalProject keyword.
    if(INDEX GREATER 0 AND ARG STREQUAL "SKETCHER_SOURCE_CACHE_GIT_SUBMODULES")
      set(FORWARD_ARGUMENT FALSE)
    elseif(ARG IN_LIST PROJECT_SINGLE OR ARG IN_LIST PROJECT_MULTI)
      set(FORWARD_ARGUMENT TRUE)
    endif()
    if(FORWARD_ARGUMENT)
      source_cache_cmake_quote(QUOTED "${ARG}")
      string(APPEND FORWARDED "  ${QUOTED}\n")
    endif()
  endforeach()
  if(SKETCHER_EXTERNAL_SOURCE_CACHE)
    set(CACHE_ARGS
        DOWNLOAD_COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/source_cache.py"
        materialize
        --manifest
        "${MANIFEST_PATH}"
        --library
        "${NAME}"
        --key
        "${KEY}"
        --cache-dir
        "${SKETCHER_EXTERNAL_SOURCE_CACHE}"
        --cmake
        "${CMAKE_COMMAND}"
        --destination
        <SOURCE_DIR>
        UPDATE_COMMAND
        "")
    foreach(ARG IN LISTS CACHE_ARGS)
      source_cache_cmake_quote(QUOTED "${ARG}")
      string(APPEND FORWARDED "  ${QUOTED}\n")
    endforeach()
  endif()
  cmake_language(EVAL CODE "ExternalProject_Add(\n${FORWARDED})")
endfunction()
