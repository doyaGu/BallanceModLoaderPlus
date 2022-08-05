# CKZlib
add_subdirectory(CKZlib EXCLUDE_FROM_ALL)

# Xbyak
add_subdirectory(xbyak EXCLUDE_FROM_ALL)

# MinHook
add_subdirectory(minhook EXCLUDE_FROM_ALL)
set_target_properties(minhook PROPERTIES FOLDER "Dependencies")