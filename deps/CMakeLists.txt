# CKZlib
add_subdirectory(CKZlib EXCLUDE_FROM_ALL)

# AsmJit
set(ASMJIT_STATIC ON CACHE BOOL "")
add_subdirectory(asmjit EXCLUDE_FROM_ALL)
set_target_properties(asmjit PROPERTIES FOLDER "Dependencies")

# MinHook
add_subdirectory(minhook EXCLUDE_FROM_ALL)
set_target_properties(MinHook PROPERTIES FOLDER "Dependencies")