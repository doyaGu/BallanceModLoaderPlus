# TimerMod - v0.4+ Module Example
#
# Demonstrates:
# - Creating a repeating timer with Services().Timer()
# - Incrementing a counter on each tick
# - Publishing tick count to a custom IMC topic
# - Cancelling the timer on detach

bml_add_module(TimerMod
    SOURCES TimerMod.cpp
    ID "com.example.timermod"
    VERSION "1.0.0"
    DISPLAY_NAME "Timer Example"
    AUTHORS "BML Team"
    DESCRIPTION "Demonstrates BML timer and IMC publishing"
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/mod.toml"
)
