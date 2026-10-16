# A project that uses newton-framework as NCX does: FetchContent, pointed at a
# local checkout with -DFETCHCONTENT_SOURCE_DIR_NEWTON=<path> (nothing is
# downloaded then). Test/cmake/test_subproject.py configures and builds it.
#   -DTEST_TOOLS=ON   the parent asks for the tools (a normal variable)

cmake_minimum_required (VERSION 3.15)
project (NewtonSubprojectTest LANGUAGES C CXX)

if (DEFINED TEST_TOOLS)
  set (NEWTON_BUILD_TOOLS ${TEST_TOOLS})
endif ()

include (FetchContent)
FetchContent_Declare (newton
  GIT_REPOSITORY https://github.com/MatthiasWM/newton-framework.git
  GIT_TAG dap_to_toolbox
)
FetchContent_MakeAvailable (newton)
