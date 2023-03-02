add_renderpass(TwoLayeredGbuffers)

target_sources(TwoLayeredGbuffers PRIVATE
    TwoLayeredGbuffers.cpp
    TwoLayeredGbuffers.h
)

target_source_group(TwoLayeredGbuffers "RenderPasses")
