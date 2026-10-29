# Only inspect the archive; ExternalProject handles removal and downloading.
if(NOT EXISTS "${ARCHIVE_PATH}")
  return()
endif()

string(REPLACE "=" ";" HASH_PARTS "${ARCHIVE_HASH}")
list(GET HASH_PARTS 0 HASH_ALGORITHM)
list(GET HASH_PARTS 1 EXPECTED_HASH)
file(${HASH_ALGORITHM} "${ARCHIVE_PATH}" ACTUAL_HASH)
if(ACTUAL_HASH STREQUAL EXPECTED_HASH)
  return()
endif()

set(WARNING_TEXT
    "Cached source archive '${ARCHIVE_PATH}' failed ${HASH_ALGORITHM} verification (expected ${EXPECTED_HASH}, actual ${ACTUAL_HASH}). CMake will download it again."
)
if("$ENV{GITHUB_ACTIONS}" STREQUAL "true")
  string(
    APPEND
    WARNING_TEXT
    " Remove GitHub Actions cache entry '${CACHE_KEY}' to prevent repeated mismatches."
  )
  # Escape workflow-command data so paths cannot create additional annotations.
  string(REPLACE "%" "%25" WARNING_TEXT "${WARNING_TEXT}")
  string(REPLACE "\r" "%0D" WARNING_TEXT "${WARNING_TEXT}")
  string(REPLACE "\n" "%0A" WARNING_TEXT "${WARNING_TEXT}")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E echo
            "::warning title=Dependency source cache mismatch::${WARNING_TEXT}")
else()
  message(WARNING "${WARNING_TEXT}")
endif()
