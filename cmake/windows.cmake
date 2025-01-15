if(WIN32)
    # 设置 Eigen 相关变量
    set(EIGEN_ROOT_DIR ${CMAKE_BINARY_DIR}/_deps/eigen-src)
    set(EIGEN_INCLUDE_DIR ${EIGEN_ROOT_DIR})
    
    # 禁用一些在 Windows 上可能有问题的功能
    set(SHERPA_ONNX_ENABLE_PYTHON OFF)
    set(SHERPA_ONNX_ENABLE_TESTS OFF)
    set(SHERPA_ONNX_ENABLE_CHECK OFF)
    
    # Windows 特定的编译标志
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /MP")
    
    # 禁用一些警告
    add_compile_options(/wd4244 /wd4267 /wd4305 /wd4996)
endif() 