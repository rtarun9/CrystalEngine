workspace("nether-engine")
configurations({ "Debug", "Release" })

project("nether-engine")
kind("WindowedApp")
language("C++")
cppdialect("C++20")
targetdir("bin/%{cfg.buildcfg}")

files({ "src/**.hpp", "src/**.cpp" })

filter("configurations:Debug")
defines({ "NETHER_DEBUG" })
symbols("On")

filter("configurations:Release")
optimize("On")
