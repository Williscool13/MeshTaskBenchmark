add_library(core-lib STATIC
        time.cpp
        time.h
        transform.cpp
        transform.h
)

target_link_libraries(core-lib PUBLIC
        engine-common
)