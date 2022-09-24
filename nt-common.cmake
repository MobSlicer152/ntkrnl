cmake_minimum_required(VERSION 3.22)

set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_SYSTEM_VERSION 6.9)

if (DEFINED NT_MSVC OR "${CMAKE_GENERATOR}" MATCHES "Visual Studio")
	set(CMAKE_C_COMPILER cl)
	set(CMAKE_CXX_COMPILER cl)
else()
	set(CMAKE_C_COMPILER clang-cl)
	set(CMAKE_CXX_COMPILER clang-cl)
endif()

if ("${CMAKE_C_COMPILER}" STREQUAL "cl")
	set(CMAKE_C_FLAGS_INIT "${ARCHDEFS} -Zl")
	set(CMAKE_CXX_FLAGS_INIT "${ARCHDEFS} -Zl")
else()
	set(CLANGWARNINGS "-Wno-microsoft-enum-forward-reference -Wno-incompatible-pointer-types -Wno-visibility -Wno-pragma-pack -Wno-gnu-folding-constant -Wno-inconsistent-dllimport -Wno-ignored-pragma-intrinsic -Wno-parentheses -Wno-implicit-int -Wno-pointer-sign -Wno-microsoft-anon-tag -Wno-ignored-pragma-optimize -Wno-shift-count-negative -Wno-shift-count-overflow")
	set(CMAKE_C_FLAGS_INIT "-fms-compatibility ${CLANGWARNINGS} --target=${CLANGARCH} ${ARCHDEFS} -Xclang -ffreestanding")
	set(CMAKE_CXX_FLAGS_INIT "-fms-compatibility ${CLANGWARNINGS} --target=${CLANGARCH} ${ARCHDEFS} -Xclang -ffreestanding")
endif()

set(CMAKE_C_LINK_EXECUTABLE "<CMAKE_LINKER> -nodefaultlib -errorlimit:0 <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_LINKER> -nodefaultlib -errorlimit:0 <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES>")
set(CMAKE_C_CREATE_SHARED_LIBRARY "<CMAKE_LINKER> -nodefaultlib -errorlimit:0 <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES> -dll")
set(CMAKE_CXX_CREATE_SHARED_LIBRARY "<CMAKE_LINKER> -nodefaultlib -errorlimit:0 <LINK_FLAGS> <OBJECTS> <LINK_LIBRARIES> -dll")
set(CMAKE_AR llvm-ar)

if (NOT "${CMAKE_HOST_SYSTEM_NAME}" MATCHES "Windows")
	set(NT_WINE_MASM TRUE)
endif()

set(CMAKE_C_COMPILER_WORKS TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
