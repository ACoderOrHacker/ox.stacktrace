package("stacktrace")
    set_kind("library", {headeronly = true})
    set_description("Stacktrace information getting library")

    add_urls("https://github.com/ACoderOrHacker/ox.stacktrace.git")
    add_versions("0.1.0", "")

    on_install(function (package)
        import("package.tools.xmake").install(package, configs)
    end)

    on_test(function (package)
        assert(package:has_cxxfuncs("ox::stacktrace::print", {includes = "ox/stacktrace.hpp"})
    end)
