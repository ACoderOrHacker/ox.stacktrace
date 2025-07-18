set_xmakever("2.2.2")
set_project("stacktrace")
set_version("0.1.0")
set_languages("c++11")

includes("@builtin/xpack")
add_includedirs("include")

add_rules("mode.debug", "mode.release", "mode.releasedbg")

target("stacktrace")
    set_kind("headeronly")

    add_headerfiles("include/**")
    add_rules("utils.install.cmake_importfiles")
    add_rules("utils.install.pkgconfig_importfiles")
target_end()

option("enable-tests")
    set_default(false)

    -- WARN: switch to releasedbg mode!!!
    target("test")
        set_kind("binary")

        if is_os("linux") then
            add_links("dl")
        end

        add_files("tests/test.cpp")

        on_install(function (target)
        
        end)
    target_end()
option_end()

xpack("stacktrace")
    set_description("Stacktrace information getting library")

    add_targets("stacktrace")
xpack_end()
