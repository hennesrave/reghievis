cmake_minimum_required(VERSION 3.23)

project(RegHieVis)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(RegHieVis "${PROJECT_SOURCE_DIR}/src/main.cpp")

target_sources(RegHieVis PRIVATE 
	"${PROJECT_SOURCE_DIR}/src/common_widgets.cpp"
	"${PROJECT_SOURCE_DIR}/src/ensemble.cpp"
	"${PROJECT_SOURCE_DIR}/src/hierarchical_clustering.cpp"
	"${PROJECT_SOURCE_DIR}/src/color_map.hpp"
	"${PROJECT_SOURCE_DIR}/src/common_widgets.hpp"
	"${PROJECT_SOURCE_DIR}/src/dendrogram.hpp"
	"${PROJECT_SOURCE_DIR}/src/ensemble.hpp"
	"${PROJECT_SOURCE_DIR}/src/hierarchical_clustering.hpp"
	"${PROJECT_SOURCE_DIR}/src/math.hpp"
	"${PROJECT_SOURCE_DIR}/src/parallel_coordinates.hpp"
	"${PROJECT_SOURCE_DIR}/src/region.hpp"
	"${PROJECT_SOURCE_DIR}/src/settings.hpp"
	"${PROJECT_SOURCE_DIR}/src/utility.hpp"
	"${PROJECT_SOURCE_DIR}/src/volume.hpp"
	"${PROJECT_SOURCE_DIR}/src/volume_renderer.hpp"
	"${PROJECT_SOURCE_DIR}/src/window.hpp"
	"${PROJECT_SOURCE_DIR}/resources/resources.qrc"
	"${PROJECT_SOURCE_DIR}/resources/stylesheet.qss")
	
target_include_directories(RegHieVis PRIVATE 
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_SOURCE_DIR}/external")
	
target_link_libraries(RegHieVis PRIVATE Qt5::Core)
target_link_libraries(RegHieVis PRIVATE Qt5::Gui)
target_link_libraries(RegHieVis PRIVATE Qt5::Widgets)