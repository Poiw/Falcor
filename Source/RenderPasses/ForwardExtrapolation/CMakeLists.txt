add_renderpass(ForwardExtrapolation)

target_sources(ForwardExtrapolation PRIVATE
    ForwardExtrapolation.cpp
    ForwardExtrapolation.h
)

target_source_group(ForwardExtrapolation "RenderPasses")
