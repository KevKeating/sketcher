# Qt 6.12 loses multiline pastes through its HTML input proxy and creates PNG
# clipboard Blobs without a MIME type. Apply the platform fixes before building.
find_package(Git REQUIRED)
set(_patch "${CMAKE_CURRENT_LIST_DIR}/qt_wasm_clipboard.patch")
execute_process(
  COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${_patch}"
  RESULT_VARIABLE _already_applied
  OUTPUT_QUIET ERROR_QUIET)
if(_already_applied EQUAL 0)
  message(STATUS "Qt WASM clipboard fixes already applied")
  return()
endif()
execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${_patch}"
                        COMMAND_ERROR_IS_FATAL ANY)
