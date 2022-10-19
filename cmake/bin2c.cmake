# https://stackoverflow.com/a/10692876/13287102
function(bin2c TARGET input_file output_file_name)
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/${output_file_name}.s "
  .global ${output_file_name}
  .global ${output_file_name}_size
  .section .rodata
${output_file_name}:
  .incbin \"${input_file}\"
1:
${output_file_name}_size:
  .int 1b - ${output_file_name}
  ")
  set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${output_file_name}.s PROPERTY LANGUAGE CXX)
  target_sources(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${output_file_name}.s)
  add_custom_target(INPUT_FILE DEPENDS ${input_file})
  add_dependencies(${TARGET} INPUT_FILE)
endfunction()
