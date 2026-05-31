project(wiznet C CXX)
add_compile_options(-Wall)
add_library(ETHERNET_FILES STATIC dummy.c)

# only add sources when a wiznet chip is defined
if (DEFINED WIZNET_CHIP)
	set(WIZNET_DIR ${CMAKE_CURRENT_SOURCE_DIR})

	target_include_directories(ETHERNET_FILES INTERFACE 
		${WIZNET_DIR}/port)

	target_sources(ETHERNET_FILES PUBLIC
		${WIZNET_DIR}/Ethernet/socket.c
		${WIZNET_DIR}/Ethernet/wizchip_conf.c
	)

	if(${WIZNET_CHIP} STREQUAL W5100S)
		target_include_directories(ETHERNET_FILES INTERFACE
			${WIZNET_DIR}/Ethernet
			${WIZNET_DIR}/Ethernet/W5100S
		)

		target_link_libraries(ETHERNET_FILES PUBLIC
			W5100S_FILES
				)
			elseif(${WIZNET_CHIP} STREQUAL W5500)
				target_include_directories(ETHERNET_FILES INTERFACE
			${WIZNET_DIR}/Ethernet
			${WIZNET_DIR}/Ethernet/W5500
				)

				target_link_libraries(ETHERNET_FILES PUBLIC
			W5500_FILES
		)
	endif()

	if(${WIZNET_CHIP} STREQUAL W5100S)
		add_library(W5100S_FILES STATIC)

		target_sources(W5100S_FILES PUBLIC
	${WIZNET_DIR}/Ethernet/W5100S/w5100s.c
		)

		target_include_directories(W5100S_FILES INTERFACE
	${WIZNET_DIR}/Ethernet
	${WIZNET_DIR}/Ethernet/W5100S
		)

		target_link_libraries(W5100S_FILES PRIVATE
	ETHERNET_FILES
		)
	elseif(${WIZNET_CHIP} STREQUAL W5500)
		add_library(W5500_FILES STATIC)

		target_sources(W5500_FILES PUBLIC
	${WIZNET_DIR}/Ethernet/W5500/w5500.c
		)

		target_include_directories(W5500_FILES INTERFACE
	${WIZNET_DIR}/Ethernet
	${WIZNET_DIR}/Ethernet/W5500
		)

		target_link_libraries(W5500_FILES PRIVATE
	ETHERNET_FILES
		)
	endif()

	add_subdirectory(port)
endif()
