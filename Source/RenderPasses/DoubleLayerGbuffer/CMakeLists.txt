add_plugin(DoubleLayerGbuffer)

target_sources(DoubleLayerGbuffer PRIVATE
    DoubleLayerGbuffer.cpp
    DoubleLayerGbuffer.h
)

target_source_group(DoubleLayerGbuffer "RenderPasses")
