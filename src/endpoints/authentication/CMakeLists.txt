set(IRODS_HTTP_ENDPOINT authentication)

add_library(
  ${IRODS_HTTP_ENDPOINT}
  OBJECT
  impl.cpp)

target_compile_definitions(
  ${IRODS_HTTP_ENDPOINT}
  PRIVATE
  ${IRODS_COMPILE_DEFINITIONS}
  ${IRODS_COMPILE_DEFINITIONS_PRIVATE}
  JWT_DISABLE_PICOJSON)

target_include_directories(
  ${IRODS_HTTP_ENDPOINT}
  PRIVATE
  ${IRODS_INCLUDE_DIRS}
  ${IRODS_EXTERNALS_FULLPATH_BOOST}/include
  ${IRODS_EXTERNALS_FULLPATH_FMT}/include
  ${IRODS_EXTERNALS_FULLPATH_SPDLOG}/include
  ${IRODS_EXTERNALS_FULLPATH_JWTCPP}/include
  ${CMAKE_BINARY_DIR}/src # For version.hpp
  ${CMAKE_SOURCE_DIR}/src # For log.hpp
  ${CMAKE_SOURCE_DIR}/src/endpoints) # For common.hpp

target_link_libraries(
  ${IRODS_HTTP_ENDPOINT}
  PRIVATE
  CURL::libcurl
  nlohmann_json::nlohmann_json)
