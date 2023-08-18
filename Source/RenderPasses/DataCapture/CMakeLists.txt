add_renderpass(DataCapture)

target_sources(DataCapture PRIVATE
    DataCapture.cpp
    DataCapture.h
)

target_source_group(DataCapture "RenderPasses")
