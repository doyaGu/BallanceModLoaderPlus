set(IVP_PLAYER_PROBE_SOURCES
        PlayerProbe.cpp
        PlayerProbe.h
        PlayerProbeApi.h)

function(ballance_ivp_add_player_test target scenario)
    bml_add_mod(${target}
            ${target}.cpp
            Ball${scenario}Scenario.cpp
            Ball${scenario}Scenario.h
            BallScenarioProbe.h
            GameplayInputDriver.cpp
            GameplayInputDriver.h
            GameplayPilot.cpp
            GameplayPilot.h
            PlayerBallLocator.cpp
            PlayerBallLocator.h
            ${IVP_PLAYER_PROBE_SOURCES})
    target_link_libraries(${target} PRIVATE IVP::API)
    set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
            FOLDER "Tests/Player")
endfunction()

# Keep the real-game cases runnable from the independent IVP tree. The driver
# owns only Ballance's menu/tutorial flow and collects verdicts from whichever
# scenario Mod the runner installs.
bml_add_mod(PlayerFlowDriver
        PlayerFlowDriver.cpp
        PlayerFrameCapture.cpp
        PlayerFrameCapture.h
        PlayerNavigator.cpp
        PlayerNavigator.h
        PlayerProbeApi.h)
set_target_properties(PlayerFlowDriver PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin"
        FOLDER "Tests/Player")
add_custom_command(TARGET PlayerFlowDriver POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:BML::BML>"
                "$<TARGET_FILE_DIR:PlayerFlowDriver>/BMLPlus.dll"
        VERBATIM)

ballance_ivp_add_player_test(IvpBallSpeedGovernorTest SpeedGovernor)
ballance_ivp_add_player_test(IvpBallStateRoundTripTest StateRoundTrip)
ballance_ivp_add_player_test(IvpBallSurfaceRaycastTest SurfaceRaycast)
ballance_ivp_add_player_test(IvpBallConstraintTest Constraint)
