add_subdirectory( binaryen )

# support for cmake time plugins
foreach(external_plugin_path ${EXTERNAL_PLUGINS})
    get_filename_component(plugin_name "${external_plugin_path}" NAME)
    add_subdirectory(${external_plugin_path} ${plugin_name})
endforeach(external_plugin_path)