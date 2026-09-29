#
# A macOS icon (.icns) from one picture (a PNG, square): every size the icon
# needs, the picture at 80% of it and centred (as macOS draws app icons),
# with sips and iconutil (macOS). Run in script mode by CMakeLists.txt (the
# NewtPlay target):
#
#   cmake -DIN=<picture.png> -DOUT=<file.icns> -DWORK=<folder> -P MakeIcns.cmake
#

set(iconset "${WORK}/icon.iconset")
file(REMOVE_RECURSE "${iconset}")
file(MAKE_DIRECTORY "${iconset}")
foreach(base 16 32 128 256 512)
  foreach(scale 1 2)
    math(EXPR size "${base} * ${scale}")
    math(EXPR inner "${size} * 8 / 10")
    if(scale EQUAL 1)
      set(name "icon_${base}x${base}.png")
    else()
      set(name "icon_${base}x${base}@2x.png")
    endif()
    execute_process(
      COMMAND sips -z ${inner} ${inner} "${IN}" --out "${WORK}/inner.png"
      OUTPUT_QUIET RESULT_VARIABLE failed)
    if(NOT failed)
      execute_process(
        COMMAND sips --padToHeightWidth ${size} ${size} "${WORK}/inner.png" --out "${iconset}/${name}"
        OUTPUT_QUIET RESULT_VARIABLE failed)
    endif()
    if(failed)
      message(FATAL_ERROR "MakeIcns: sips failed for ${name}")
    endif()
  endforeach()
endforeach()
execute_process(COMMAND iconutil -c icns "${iconset}" -o "${OUT}" RESULT_VARIABLE failed)
if(failed)
  message(FATAL_ERROR "MakeIcns: iconutil failed")
endif()
