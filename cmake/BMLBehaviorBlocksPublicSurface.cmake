# Public C++ Behavior domains and named Virtools Building Block adapters.
#
# One explicit inventory drives source-tree validation, installation, and
# independent-header compilation. Adding an adapter in only one of those
# places is therefore a configuration error rather than a broken SDK package.
set(BML_BEHAVIOR_CPP_PUBLIC_RELATIVE_HEADERS
        BML/Behavior.hpp
        BML/Behavior/Value.hpp
        BML/Behavior/Frames.hpp
        BML/Behavior/Prototype.hpp
        BML/Behavior/Pattern.hpp
        BML/Behavior/Graph.hpp
        BML/Behavior/Run.hpp
        BML/Behavior/Block.hpp
        BML/Behavior/Edit.hpp
        BML/Behavior/Script.hpp
        BML/Behavior/Session.hpp
)

set(BML_PUBLIC_BEHAVIOR_CPP_HEADERS)
foreach(BML_BEHAVIOR_CPP_PUBLIC_RELATIVE_HEADER
        IN LISTS BML_BEHAVIOR_CPP_PUBLIC_RELATIVE_HEADERS)
    list(APPEND BML_PUBLIC_BEHAVIOR_CPP_HEADERS
            "${BML_INCLUDE_DIR}/${BML_BEHAVIOR_CPP_PUBLIC_RELATIVE_HEADER}")
endforeach()

set(BML_PUBLIC_BEHAVIOR_CPP_DETAIL_HEADERS
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/BlockAccess.hpp
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/Wire.hpp
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/Hook.hpp
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/EditProgram.hpp
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/Inline.hpp
        ${BML_INCLUDE_DIR}/BML/Behavior/Detail/Blocks.hpp
)

set(BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADERS
        BML/Behavior/Blocks.hpp
        BML/Behavior/Blocks/ObjectLoad.hpp
        BML/Behavior/Blocks/Physicalize.hpp
        BML/Behavior/Blocks/PhysicsForce.hpp
        BML/Behavior/Blocks/PhysicsImpulse.hpp
        BML/Behavior/Blocks/PhysicsWakeUp.hpp
        BML/Behavior/Blocks/SendMessage.hpp
        BML/Behavior/Blocks/Text2D.hpp
)

set(BML_PUBLIC_BEHAVIOR_BLOCK_HEADERS)
foreach(BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADER
        IN LISTS BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADERS)
    set(BML_BEHAVIOR_BLOCK_PUBLIC_HEADER
            "${BML_INCLUDE_DIR}/${BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADER}")
    if(NOT EXISTS "${BML_BEHAVIOR_BLOCK_PUBLIC_HEADER}")
        message(FATAL_ERROR
                "Behavior Blocks public-surface inventory names a missing file: "
                "${BML_BEHAVIOR_BLOCK_PUBLIC_HEADER}")
    endif()
    list(APPEND BML_PUBLIC_BEHAVIOR_BLOCK_HEADERS
            "${BML_BEHAVIOR_BLOCK_PUBLIC_HEADER}")
endforeach()

file(GLOB BML_BEHAVIOR_BLOCK_DISCOVERED_HEADERS
        RELATIVE "${BML_INCLUDE_DIR}"
        "${BML_INCLUDE_DIR}/BML/Behavior/Blocks.hpp"
        "${BML_INCLUDE_DIR}/BML/Behavior/Blocks/*.hpp")
list(SORT BML_BEHAVIOR_BLOCK_DISCOVERED_HEADERS)

set(BML_BEHAVIOR_BLOCK_INVENTORIED_HEADERS
        ${BML_BEHAVIOR_BLOCK_PUBLIC_RELATIVE_HEADERS})
list(SORT BML_BEHAVIOR_BLOCK_INVENTORIED_HEADERS)
if(NOT BML_BEHAVIOR_BLOCK_DISCOVERED_HEADERS STREQUAL
       BML_BEHAVIOR_BLOCK_INVENTORIED_HEADERS)
    message(FATAL_ERROR
            "Behavior Blocks public headers and inventory differ.\n"
            "On disk: ${BML_BEHAVIOR_BLOCK_DISCOVERED_HEADERS}\n"
            "Inventory: ${BML_BEHAVIOR_BLOCK_INVENTORIED_HEADERS}")
endif()

set(BML_BEHAVIOR_BLOCK_UMBRELLA
        "${BML_INCLUDE_DIR}/BML/Behavior/Blocks.hpp")
file(READ "${BML_BEHAVIOR_BLOCK_UMBRELLA}"
        BML_BEHAVIOR_BLOCK_UMBRELLA_TEXT)
foreach(BML_BEHAVIOR_BLOCK_HEADER
        IN LISTS BML_BEHAVIOR_BLOCK_INVENTORIED_HEADERS)
    if(NOT BML_BEHAVIOR_BLOCK_HEADER MATCHES
           "^BML/Behavior/Blocks/[^/]+\\.hpp$")
        continue()
    endif()
    string(FIND "${BML_BEHAVIOR_BLOCK_UMBRELLA_TEXT}"
            "#include \"${BML_BEHAVIOR_BLOCK_HEADER}\""
            BML_BEHAVIOR_BLOCK_INCLUDE_OFFSET)
    if(BML_BEHAVIOR_BLOCK_INCLUDE_OFFSET EQUAL -1)
        message(FATAL_ERROR
                "${BML_BEHAVIOR_BLOCK_UMBRELLA} does not include "
                "${BML_BEHAVIOR_BLOCK_HEADER}")
    endif()
endforeach()
