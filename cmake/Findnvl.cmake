if (NOT _NVL_FOUND) # If there's none, fetch and build NVL
    set(_NVL_FOUND TRUE)
    include(FetchContent)
    FetchContent_Declare(
            nvl
            DOWNLOAD_EXTRACT_TIMESTAMP OFF
            URL https://github.com/dkoeplin/nvl/archive/refs/heads/main.zip
    )
    FetchContent_GetProperties(nvl)
    if (NOT NVL_POPULATED) # Have we downloaded NVL yet?
        set(FETCHCONTENT_QUIET NO)
        FetchContent_MakeAvailable(nvl)
    endif ()
endif ()