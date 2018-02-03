add_library(PataterSynth
    oscillator.c
)

target_include_directories(PataterSynth
    PUBLIC ../include
)

target_link_libraries(PataterSynth m)

target_compile_features(PataterSynth PUBLIC c_std_90)
