add_renderpass(MyGbuffer)

target_sources(MyGbuffer PRIVATE
    MyGbuffer.cpp
    MyGbuffer.h
)

target_source_group(MyGbuffer "RenderPasses")
