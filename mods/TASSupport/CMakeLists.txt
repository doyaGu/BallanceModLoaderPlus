# TASSupport
add_bml_mod(TASSupport TASSupport.cpp TASSupport.h)
target_link_libraries(TASSupport PRIVATE minhook)
install_bml_mod(TASSupport)